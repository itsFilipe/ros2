/**
 * wasd_teleop_node.cpp
 *
 * Controle estilo videogame (WASD) para o robô 2WD via /cmd_vel!
 * - W: Frente (força total nas 2 rodas)
 * - S: Ré (força total nas 2 rodas)
 * - A: Gira para a Esquerda
 * - D: Gira para a Direita
 * - Q: Curva suave p/ Esquerda (só roda direita gira)
 * - E: Curva suave p/ Direita (só roda esquerda gira)
 * - 1 / 2: Teste individual da Roda Esquerda / Roda Direita
 * - Espaço: Freio imediato
 * - Soltou a tecla: Para sozinho automaticamente!
 */

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <cstdio>
#include <algorithm>
#include <chrono>

using namespace std::chrono_literals;

class WasdTeleopNode : public rclcpp::Node
{
public:
  WasdTeleopNode()
  : Node("wasd_teleop_node"),
    power_(1.0), // Começa em 100% de potência (PWM 255) para vencer qualquer atrito!
    target_linear_(0.0),
    target_angular_(0.0),
    moving_(false)
  {
    publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);

    // Configura o terminal para ler teclas instantaneamente sem precisar apertar Enter
    tcgetattr(STDIN_FILENO, &orig_termios_);
    struct termios raw = orig_termios_;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    last_key_time_ = this->now();

    // Timer a 20 Hz (a cada 50ms) para ler o teclado e gerenciar a parada automática
    timer_ = this->create_wall_timer(
      50ms,
      std::bind(&WasdTeleopNode::loop_callback, this));

    print_interface();
  }

  ~WasdTeleopNode()
  {
    // Restaura o terminal ao normal e para o robô ao sair
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios_);
    publish_cmd(0.0, 0.0);
    printf("\n\nRobô parado. Até a próxima!\n");
  }

private:
  void print_interface()
  {
    printf("\033[2J\033[H"); // Limpa a tela
    printf("=========================================================\n");
    printf("   🎮 CONTROLE WASD DO ROBÔ 2WD (ESTILO VIDEOGAME) 🎮   \n");
    printf("=========================================================\n\n");
    printf("          [ Q ]       [ W ]       [ E ]                  \n");
    printf("         Curva ↖     FRENTE ↑    Curva ↗                 \n\n");
    printf("          [ A ]       [ S ]       [ D ]                  \n");
    printf("         Gira ↺       RÉ ↓       Gira ↻                  \n\n");
    printf("---------------------------------------------------------\n");
    printf("  [ESPAÇO] : Freio imediato (ou apenas solte a tecla!)   \n");
    printf("  [+ / -]  : Aumentar / Diminuir potência (Atual: %d%%)  \n", (int)(power_ * 100));
    printf("  [ 1 ]    : Testar SÓ Roda Esquerda (M3)                \n");
    printf("  [ 2 ]    : Testar SÓ Roda Direita  (M4)                \n");
    printf("  [Ctrl+C] : Sair                                        \n");
    printf("=========================================================\n");
    printf("Status: ⏹️  PARADO\r");
    fflush(stdout);
  }

  void loop_callback()
  {
    char c = 0;
    bool key_pressed = false;

    // Lê todas as teclas no buffer para pegar a mais recente sem atraso
    while (read(STDIN_FILENO, &c, 1) > 0) {
      key_pressed = true;
    }

    if (key_pressed) {
      last_key_time_ = this->now();
      handle_key(c);
    } else {
      // Se passou mais de 450ms sem nenhuma tecla pressionada, para o robô sozinho!
      // (450ms cobre o pequeno atraso inicial de repetição do teclado quando você segura a tecla)
      double elapsed = (this->now() - last_key_time_).seconds();
      if (moving_ && elapsed > 0.45) {
        moving_ = false;
        target_linear_ = 0.0;
        target_angular_ = 0.0;
        publish_cmd(0.0, 0.0);
        printf("\r\033[KStatus: ⏹️  PARADO (soltou a tecla) | Potência: %d%%", (int)(power_ * 100));
        fflush(stdout);
      }
    }
  }

  void handle_key(char c)
  {
    std::string action = "";
    bool send = true;

    switch (c) {
      case 'w':
      case 'W':
        // Frente reto: linear = +power, angular = 0 -> Esq = +power, Dir = +power
        target_linear_ = power_;
        target_angular_ = 0.0;
        moving_ = true;
        action = "⬆️  FRENTE";
        break;

      case 's':
      case 'S':
        // Ré reto: linear = -power, angular = 0 -> Esq = -power, Dir = -power
        target_linear_ = -power_;
        target_angular_ = 0.0;
        moving_ = true;
        action = "⬇️  RÉ";
        break;

      case 'a':
      case 'A':
        // Gira p/ Esquerda no eixo: Esq = -power, Dir = +power
        target_linear_ = 0.0;
        target_angular_ = power_;
        moving_ = true;
        action = "↺  GIRANDO ESQUERDA";
        break;

      case 'd':
      case 'D':
        // Gira p/ Direita no eixo: Esq = +power, Dir = -power
        target_linear_ = 0.0;
        target_angular_ = -power_;
        moving_ = true;
        action = "↻  GIRANDO DIREITA";
        break;

      case 'q':
      case 'Q':
      case '2':
        // Curva p/ Esquerda / Testar SÓ Roda Direita:
        // linear = +0.5*power, angular = +0.5*power -> Esq = 0, Dir = +power
        target_linear_ = 0.5 * power_;
        target_angular_ = 0.5 * power_;
        moving_ = true;
        action = (c == '2') ? "🔧 TESTE: SÓ RODA DIREITA (M4)" : "↖️  CURVA ESQUERDA";
        break;

      case 'e':
      case 'E':
      case '1':
        // Curva p/ Direita / Testar SÓ Roda Esquerda:
        // linear = +0.5*power, angular = -0.5*power -> Esq = +power, Dir = 0
        target_linear_ = 0.5 * power_;
        target_angular_ = -0.5 * power_;
        moving_ = true;
        action = (c == '1') ? "🔧 TESTE: SÓ RODA ESQUERDA (M3)" : "↗️  CURVA DIREITA";
        break;

      case ' ':
      case 'k':
      case 'K':
        target_linear_ = 0.0;
        target_angular_ = 0.0;
        moving_ = false;
        action = "🛑 FREIO!";
        break;

      case '+':
      case '=':
        power_ = std::min(1.0, power_ + 0.1);
        send = false;
        printf("\r\033[K⚡ Potência aumentada para: %d%%", (int)(power_ * 100));
        fflush(stdout);
        break;

      case '-':
      case '_':
        power_ = std::max(0.2, power_ - 0.1);
        send = false;
        printf("\r\033[K⚡ Potência reduzida para: %d%%", (int)(power_ * 100));
        fflush(stdout);
        break;

      default:
        send = false;
        break;
    }

    if (send) {
      publish_cmd(target_linear_, target_angular_);
      printf("\r\033[KStatus: %s | Potência: %d%%", action.c_str(), (int)(power_ * 100));
      fflush(stdout);
    }
  }

  void publish_cmd(double linear, double angular)
  {
    auto msg = geometry_msgs::msg::Twist();
    msg.linear.x = linear;
    msg.angular.z = angular;
    publisher_->publish(msg);
  }

  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  struct termios orig_termios_;
  rclcpp::Time last_key_time_;
  double power_;
  double target_linear_;
  double target_angular_;
  bool moving_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<WasdTeleopNode>());
  rclcpp::shutdown();
  return 0;
}
