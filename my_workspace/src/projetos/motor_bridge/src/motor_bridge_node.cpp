/**
 * motor_bridge_node.cpp
 *
 * Node ROS2 (rclcpp) que faz a ponte entre o tópico /motor_cmd e um
 * Arduino conectado via USB serial.
 *
 * FLUXO:
 *   /motor_cmd (Int32) → callback → formata string → write() na porta serial
 *
 * PROTOCOLO SERIAL:
 *   Texto simples terminado em '\n', ex: "75\n" ou "-50\n" ou "0\n"
 *   Isso permite debugar manualmente com o Serial Monitor da Arduino IDE.
 *
 * SERIAL (termios.h):
 *   Usamos a API POSIX pura (termios.h) em vez de uma biblioteca como
 *   libserial ou boost::asio. Isso significa mais código boilerplate, mas
 *   você vê exatamente o que está acontecendo — é o mesmo mecanismo que
 *   qualquer programa Unix usa para falar com portas seriais.
 */

// ── Includes padrão C/Linux ────────────────────────────────────────────────
#include <fcntl.h>      // open(), O_RDWR, O_NOCTTY, O_NONBLOCK
#include <termios.h>    // struct termios, tcgetattr(), tcsetattr(), cfsetspeed()
#include <unistd.h>     // write(), close()
#include <cerrno>       // errno
#include <cstring>      // strerror()
#include <string>
#include <algorithm>    // std::clamp

// ── Includes ROS2 ─────────────────────────────────────────────────────────
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"

// ═══════════════════════════════════════════════════════════════════════════
//  Helpers de porta serial
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Abre e configura a porta serial para comunicação com o Arduino.
 *
 * Retorna o file descriptor (>= 0) em caso de sucesso, ou -1 em erro.
 *
 * Flags relevantes explicadas:
 *   O_RDWR      — abre para leitura e escrita (mesmo que só vamos escrever,
 *                 algumas implementações exigem RDWR para configurar a porta)
 *   O_NOCTTY    — não deixa o processo se tornar "controlling terminal"
 *                 do dispositivo. Sem isso, sinais como Ctrl+C poderiam
 *                 interferir no processo ROS2.
 *   O_NONBLOCK  — abertura não bloqueia mesmo que o dispositivo não esteja
 *                 pronto. Importante para não travar o node na inicialização.
 */
static int open_serial_port(const std::string & port, int baud_rate)
{
  int fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    return -1;
  }

  // ── Lê configuração atual da porta ──────────────────────────────────────
  struct termios tty{};
  if (tcgetattr(fd, &tty) != 0) {
    close(fd);
    return -1;
  }

  // ── Seleciona baud rate ──────────────────────────────────────────────────
  // cfsetspeed() configura input E output speed de uma vez.
  // As constantes Bxxx (B9600, B115200...) são definidas em termios.h.
  speed_t speed = B9600;
  if (baud_rate == 115200) {
    speed = B115200;
  } else if (baud_rate == 57600) {
    speed = B57600;
  } else if (baud_rate == 38400) {
    speed = B38400;
  } else if (baud_rate == 19200) {
    speed = B19200;
  }
  // (default: B9600 para valores não mapeados)
  cfsetspeed(&tty, speed);

  // ── Modo RAW (sem processamento especial de caracteres) ──────────────────
  // cfmakeraw() é um atalho que configura o conjunto de flags necessárias
  // para comunicação binária/texto simples sem interpretação de caracteres
  // de controle (eco, sinal, processamento de linha, etc.).
  cfmakeraw(&tty);

  // ── Configurações adicionais explícitas ──────────────────────────────────
  // CREAD  — habilita o receptor (necessário para receber dados)
  // CLOCAL — ignora sinais de modem (não queremos que DCD/DTR afetem o fd)
  tty.c_cflag |= (CREAD | CLOCAL);

  // 8N1: 8 bits de dados, sem paridade, 1 stop bit
  // (cfmakeraw() já garante isso, mas deixamos explícito para documentação)
  tty.c_cflag &= ~PARENB;   // sem paridade
  tty.c_cflag &= ~CSTOPB;   // 1 stop bit
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;        // 8 bits de dados

  // VMIN=0, VTIME=0 → leitura não-bloqueante (não vamos ler, só escrever,
  // mas é boa prática configurar de forma consistente)
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 0;

  // ── Aplica a configuração ────────────────────────────────────────────────
  // TCSANOW — aplica imediatamente (sem esperar buffer esvaziar)
  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    close(fd);
    return -1;
  }

  return fd;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Node principal
// ═══════════════════════════════════════════════════════════════════════════

class MotorBridgeNode : public rclcpp::Node
{
public:
  /**
   * ARQUITETURA — Por que herdar de rclcpp::Node?
   *
   * Em ROS2, um "node" é a unidade básica de computação. Cada node tem
   * seu próprio namespace, parâmetros, publishers/subscribers e lifecycle.
   * Herdar de rclcpp::Node é a forma mais direta de criar um node C++.
   *
   * A alternativa seria usar composição (ter um rclcpp::Node como membro),
   * mas a herança é mais comum em nodes simples como este.
   */
  MotorBridgeNode()
  : Node("motor_bridge_node"), serial_fd_(-1)
  {
    // ── Parâmetros ─────────────────────────────────────────────────────────
    // Declarar parâmetros permite alterá-los em runtime via:
    //   ros2 run motor_bridge motor_bridge_node --ros-args -p serial_port:=/dev/ttyUSB0
    // ou no launch file, sem precisar recompilar.
    this->declare_parameter<std::string>("serial_port", "/dev/ttyACM0");
    this->declare_parameter<int>("baud_rate", 9600);

    const std::string port = this->get_parameter("serial_port").as_string();
    const int baud         = this->get_parameter("baud_rate").as_int();

    // ── Abre a porta serial ────────────────────────────────────────────────
    serial_fd_ = open_serial_port(port, baud);
    if (serial_fd_ < 0) {
      // RCLCPP_ERROR: mensagem de erro que aparece em vermelho nos logs.
      // strerror(errno) traduz o código de erro para texto legível.
      // Optamos por NÃO jogar exceção aqui — o node fica vivo e tenta
      // escrever na porta a cada mensagem (vai logar warning até reconectar).
      RCLCPP_ERROR(
        this->get_logger(),
        "Não foi possível abrir a porta serial '%s': %s\n"
        "  → Verifique se o Arduino está conectado e tente:\n"
        "      ls /dev/tty{USB,ACM}*\n"
        "  → Se aparecer mas der 'Permission denied', execute:\n"
        "      sudo usermod -aG dialout $USER  (e faça logout/login)",
        port.c_str(), strerror(errno));
    } else {
      RCLCPP_INFO(
        this->get_logger(),
        "Porta serial '%s' aberta com sucesso a %d bps.",
        port.c_str(), baud);
    }

    // ── Subscription ────────────────────────────────────────────────────────
    // ARQUITETURA — Por que subscription e não um timer?
    //
    // Este node é puramente REATIVO: só age quando alguém publica em
    // /motor_cmd. Não há estado interno que precise ser publicado
    // periodicamente. Por isso, uma subscription é a abstração certa.
    //
    // O QoS padrão (rmw_qos_profile_default) é suficiente aqui:
    //   - reliability: RELIABLE (garante entrega)
    //   - history: KEEP_LAST com depth=10
    // Para controle de motor em tempo real, considere no futuro:
    //   rclcpp::SensorDataQoS() → BEST_EFFORT + KEEP_LAST(1)
    // que descarta mensagens antigas em favor das mais recentes.
    subscription_ = this->create_subscription<std_msgs::msg::Int32>(
      "/motor_cmd",
      10,   // queue depth: quantas mensagens ficam em buffer esperando o callback
      std::bind(&MotorBridgeNode::motor_cmd_callback, this, std::placeholders::_1)
    );

    RCLCPP_INFO(this->get_logger(), "motor_bridge_node iniciado. Aguardando /motor_cmd...");

    // Armazena parâmetros para uso no callback (ex: reabrir porta)
    serial_port_ = port;
    baud_rate_   = baud;
  }

  // Destrutor: garante que o fd seja fechado quando o node for destruído
  ~MotorBridgeNode()
  {
    if (serial_fd_ >= 0) {
      close(serial_fd_);
      RCLCPP_INFO(this->get_logger(), "Porta serial fechada.");
    }
  }

private:
  /**
   * Callback chamado cada vez que uma mensagem chega em /motor_cmd.
   *
   * O ROS2 passa a mensagem como shared_ptr const& para evitar cópia.
   * Usamos ConstSharedPtr (alias para shared_ptr<const T>) por convenção.
   */
  void motor_cmd_callback(const std_msgs::msg::Int32::ConstSharedPtr msg)
  {
    // ── Clampeia o valor recebido ────────────────────────────────────────
    // Garante que só enviamos valores válidos para o Arduino,
    // mesmo que alguém publique algo fora do range -100..100.
    const int value = std::clamp(msg->data, -100, 100);

    // ── Formata o comando serial ─────────────────────────────────────────
    // Protocolo: inteiro em ASCII + '\n'
    // Ex: valor 75  → "75\n"
    //     valor -50 → "-50\n"
    //     valor 0   → "0\n"
    const std::string cmd = std::to_string(value) + "\n";

    RCLCPP_DEBUG(this->get_logger(), "Enviando: '%s'", cmd.c_str());

    // ── Tenta reabrir a porta se necessário ─────────────────────────────
    if (serial_fd_ < 0) {
      serial_fd_ = open_serial_port(serial_port_, baud_rate_);
      if (serial_fd_ < 0) {
        RCLCPP_WARN_THROTTLE(
          this->get_logger(), *this->get_clock(), 5000,  // no máximo a cada 5s
          "Porta serial '%s' ainda não disponível. Reconectando...",
          serial_port_.c_str());
        return;
      }
      RCLCPP_INFO(this->get_logger(), "Porta serial reconectada.");
    }

    // ── Escreve na porta serial ──────────────────────────────────────────
    // write() retorna o número de bytes escritos, ou -1 em erro.
    // Para strings curtas como "75\n", write() geralmente escreve tudo
    // de uma vez, mas em geral devemos verificar bytes escritos.
    const ssize_t bytes_written = write(serial_fd_, cmd.c_str(), cmd.size());

    if (bytes_written < 0) {
      RCLCPP_WARN(
        this->get_logger(),
        "Erro ao escrever na porta serial: %s. Tentará reabrir na próxima mensagem.",
        strerror(errno));
      // Fecha o fd corrompido; o próximo callback tentará reabrir
      close(serial_fd_);
      serial_fd_ = -1;
    } else if (static_cast<size_t>(bytes_written) < cmd.size()) {
      // Escrita parcial (raro para strings curtas, mas possível)
      RCLCPP_WARN(
        this->get_logger(),
        "Escrita parcial na serial: %zd de %zu bytes.",
        bytes_written, cmd.size());
    }
  }

  // ── Membros ──────────────────────────────────────────────────────────────
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr subscription_;
  int serial_fd_;           // file descriptor da porta serial (-1 = fechada)
  std::string serial_port_; // ex: "/dev/ttyACM0"
  int baud_rate_;           // ex: 9600
};

// ═══════════════════════════════════════════════════════════════════════════
//  main
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char * argv[])
{
  // rclcpp::init() inicializa a comunicação ROS2 (DDS middleware).
  // Deve ser chamado UMA VEZ antes de criar qualquer node.
  rclcpp::init(argc, argv);

  // make_shared cria o node no heap; rclcpp::spin() mantém o processo vivo
  // e processa callbacks (subscription, timers, etc.) até Ctrl+C.
  // spin() é bloqueante — só retorna quando rclcpp::shutdown() for chamado.
  rclcpp::spin(std::make_shared<MotorBridgeNode>());

  // rclcpp::shutdown() libera recursos do middleware antes de sair.
  rclcpp::shutdown();
  return 0;
}
