// ============================================================
// ArduinoTurtleBridge.cpp
// Lê um sensor de luz via serial (Arduino) e move a tartaruga
// ============================================================

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "turtlesim/msg/pose.hpp"
#include <chrono>
#include <memory>
#include <fcntl.h>      // open()
#include <unistd.h>     // read(), write(), close()
#include <termios.h>    // configuração da porta serial
#include <sys/ioctl.h>  // ioctl() → controle de sinais elétricos (DTR)

using namespace std::chrono_literals;
using std::placeholders::_1; // atalho para bind: significa "1º argumento vem de fora"

// ============================================================
// CLASSE PRINCIPAL
// Herda de rclcpp::Node → ela mesma É um nó ROS2
// ============================================================
class ArduinoTurtleBridge : public rclcpp::Node {
public:

    ArduinoTurtleBridge() : Node("arduino_turtle_bridge") {
        abrir_serial();
        configurar_serial();

        // Publisher: envia comandos de velocidade para a tartaruga
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/turtle1/cmd_vel", 10);

        // Subscriber: recebe a posição atual da tartaruga (atualiza pos_x / pos_y)
        subscription_ = this->create_subscription<turtlesim::msg::Pose>(
            "/turtle1/pose", 10,
            std::bind(&ArduinoTurtleBridge::atualizar_posicao, this, _1)
        );

        // Timer: chama mover_tartaruga() a cada 100ms
        timer_ = this->create_wall_timer(
            100ms, std::bind(&ArduinoTurtleBridge::mover_tartaruga, this)
        );

        pos_x = 5.54;
        pos_y = 5.54;
    }

    ~ArduinoTurtleBridge() {
        if (file_desc_ != -1) {
            close(file_desc_);
            RCLCPP_INFO(this->get_logger(), "Porta serial fechada.");
        }
    }

private:

    // ============================================================
    // SERIAL — Abertura
    // O_RDWR    → abre para leitura e escrita (receber sensor, enviar LED)
    // O_NOCTTY  → não deixa o Arduino "tomar controle" do terminal
    // ============================================================
    void abrir_serial() {
        file_desc_ = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);
        if (file_desc_ == -1) {
            RCLCPP_ERROR(this->get_logger(), "Porta serial não encontrada!");
            throw std::runtime_error("Erro fatal: cabo desconectado?");
        }
    }

    // ============================================================
    // SERIAL — Configuração
    // A porta serial é como um contrato entre dois dispositivos:
    // ambos precisam combinar velocidade, bits, paridade, etc.
    // ============================================================
    void configurar_serial() {
        struct termios tty;
        memset(&tty, 0, sizeof(tty)); // zera para não ter lixo de memória

        tcgetattr(file_desc_, &tty);  // lê as configs atuais como ponto de partida

        // Velocidade: tem que ser igual ao Serial.begin(9600) do Arduino
        cfsetospeed(&tty, B9600);
        cfsetispeed(&tty, B9600);

        // Formato dos dados: 8 bits, sem paridade, 1 stop bit (padrão "8N1")
        tty.c_cflag &= ~PARENB; // sem paridade
        tty.c_cflag &= ~CSTOPB; // 1 stop bit
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |=  CS8;    // 8 bits por mensagem

        // Habilita recepção + ignora controle de linha
        tty.c_cflag |=  CREAD | CLOCAL;

        // Desliga o DTR: sem isso, abrir a porta reseta o Arduino!
        tty.c_cflag &= ~HUPCL;

        // Modo canônico: o read() só entrega dados quando chega um '\n'
        // (funciona porque o Arduino usa Serial.println() que termina com \n)
        tty.c_lflag |=  ICANON;
        tty.c_lflag &= ~(ECHO | ECHOE | ECHONL | ISIG);

        // Descarta o '\r' do println() — sem isso cada envio virava 2 leituras:
        // leitura 1: "31"  → leitura 2: "" (linha vazia do \r)
        tty.c_iflag |=  IGNCR;

        tcsetattr(file_desc_, TCSANOW, &tty); // aplica tudo

        // Baixa o sinal DTR manualmente para garantir que o Arduino não resete
        int sinais;
        ioctl(file_desc_, TIOCMGET, &sinais);
        sinais &= ~TIOCM_DTR;
        ioctl(file_desc_, TIOCMSET, &sinais);

        RCLCPP_INFO(this->get_logger(), "Aguardando Arduino inicializar...");
        sleep(2);

        tcflush(file_desc_, TCIOFLUSH); // descarta qualquer lixo do boot
    }

    // ============================================================
    // LÓGICA PRINCIPAL — chamada a cada 100ms pelo timer
    // ============================================================
    void mover_tartaruga() {
        // --- Lê o sensor ---
        char buffer[64];
        int bytes_lidos = read(file_desc_, buffer, sizeof(buffer));
        if (bytes_lidos <= 0) return;

        buffer[bytes_lidos] = '\0';
        int luz = std::atoi(buffer);
        RCLCPP_INFO(this->get_logger(), "Sensor: %d", luz);

        auto cmd = geometry_msgs::msg::Twist();

        if (luz > 100) {
            cmd = calcular_movimento_claro();
        } else {
            // Escuro → para tudo
            cmd.linear.x  = 0.0;
            cmd.angular.z = 0.0;
        }

        publisher_->publish(cmd);
    }

    // ============================================================
    // MOVIMENTO — só chamado quando está claro
    //
    // Máquina de estados com 2 fases:
    //
    //  tempo_de_fuga_ == 0  → passeio normal
    //  tempo_de_fuga_ > 0   → manobra de fuga ativa
    //
    // A fuga só é ATIVADA se não estiver já fugindo (== 0),
    // evitando que a borda resete o contador infinitamente.
    // ============================================================
    geometry_msgs::msg::Twist calcular_movimento_claro() {
        geometry_msgs::msg::Twist cmd;

        // Detecta borda apenas quando não está em fuga
        if (tempo_de_fuga_ == 0) {
            bool na_borda = (pos_x > 10.0 || pos_x < 1.0 ||
                             pos_y > 10.0 || pos_y < 1.0);
            if (na_borda) {
                tempo_de_fuga_ = 20; // 20 × 100ms = 2 segundos de manobra
                RCLCPP_WARN(this->get_logger(), "Borda! Iniciando fuga.");
            }
        }

        if (tempo_de_fuga_ > 10) {
            // Fase 1 (ciclos 20→11): recua para longe da parede
            cmd.linear.x  = -1.0;
            cmd.angular.z =  0.0;
            write(file_desc_, "R", 1); // LED vermelho piscando
            tempo_de_fuga_--;

        } else if (tempo_de_fuga_ > 0) {
            // Fase 2 (ciclos 10→1): gira no próprio eixo para mudar direção
            cmd.linear.x  =  0.0;
            cmd.angular.z =  2.5;
            write(file_desc_, "R", 1); // LED vermelho ainda piscando
            tempo_de_fuga_--;

        } else {
            // Passeio tranquilo: anda em frente
            cmd.linear.x  = 1.5;
            cmd.angular.z = 0.0;
            write(file_desc_, "V", 1); // LED verde
        }

        return cmd;
    }

    // ============================================================
    // SUBSCRIBER CALLBACK — atualiza posição quando turtlesim publica
    // ============================================================
    void atualizar_posicao(const turtlesim::msg::Pose::SharedPtr msg) {
        pos_x = msg->x;
        pos_y = msg->y;
    }

    // --- Membros da classe ---
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr subscription_;
    rclcpp::TimerBase::SharedPtr timer_;

    int file_desc_ = -1;
    float pos_x;
    float pos_y;
    int tempo_de_fuga_ = 0;
};

// ============================================================
// MAIN — inicializa o ROS2, sobe o nó, fica rodando até Ctrl+C
// ============================================================
int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ArduinoTurtleBridge>());
    rclcpp::shutdown();
    return 0;
}