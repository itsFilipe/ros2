#include "geometry_msgs/msg/twist.hpp"
#include "std_msgs/msg/empty.hpp"
#include "turtlesim/msg/pose.hpp"
#include "turtlesim/srv/kill.hpp"
#include <random>
#include <rclcpp/rclcpp.hpp>

using namespace std::placeholders;
using namespace std::chrono_literals;

class TurtleVitima : public rclcpp::Node {
public:
  TurtleVitima() : Node("turtle_vitima"), rng_(std::random_device{}()) {
    sub_pegador_ = this->create_subscription<turtlesim::msg::Pose>(
        "/pegador/pose", 10,
        std::bind(&TurtleVitima::callback_pegador, this, _1));

    sub_vitima_ = this->create_subscription<turtlesim::msg::Pose>(
        "/vitima/pose", 10,
        std::bind(&TurtleVitima::callback_vitima, this, _1));

    // Escuta o sinal de respawn para resetar o estado interno quando uma nova vitima nascer
    sub_respawn_ = this->create_subscription<std_msgs::msg::Empty>(
        "/respawn_vitima", 10,
        std::bind(&TurtleVitima::callback_respawn, this, _1));

    pub_cmd_vel_ = this->create_publisher<geometry_msgs::msg::Twist>(
        "/vitima/cmd_vel", 10);

    pub_respawn_ = this->create_publisher<std_msgs::msg::Empty>(
        "/respawn_vitima", 10);

    kill_client_ = this->create_client<turtlesim::srv::Kill>("/kill");

    // Timer para movimentação autônoma aleatória (muda de direção a cada 1.5s)
    move_timer_ = this->create_wall_timer(
        1500ms, std::bind(&TurtleVitima::atualizar_movimento, this));

    // Timer de controle de velocidade constante (20 Hz)
    control_timer_ = this->create_wall_timer(
        50ms, std::bind(&TurtleVitima::publicar_velocidade, this));
  }

private:
  void callback_pegador(const turtlesim::msg::Pose::SharedPtr msg) {
    posicao_pegador_ = *msg;
    pegador_pose_recebida_ = true;
    verificar_captura();
  }

  void callback_vitima(const turtlesim::msg::Pose::SharedPtr msg) {
    posicao_vitima_ = *msg;
    // Se estava aguardando nova vitima, o recebimento da pose confirma que ela nasceu
    if (aguardando_respawn_) {
      aguardando_respawn_ = false;
      vitima_viva_ = true;
      RCLCPP_INFO(this->get_logger(), "Nova vitima detectada! Captura reativada.");
    }
    vitima_pose_recebida_ = true;
    verificar_captura();
  }

  // Chamado quando game_manager notifica que uma nova vitima foi (ou sera) spawnada
  void callback_respawn(const std_msgs::msg::Empty::SharedPtr) {
    // Nao reseta aqui ainda: aguarda a proxima pose de /vitima/pose chegar
    // para evitar captura dupla com poses antigas em buffer
    aguardando_respawn_ = true;
    vitima_pose_recebida_ = false;
    RCLCPP_INFO(this->get_logger(), "Respawn requisitado. Aguardando nova vitima...");
  }

  void atualizar_movimento() {
    if (!vitima_viva_) {
      return;
    }

    std::uniform_real_distribution<float> dist_linear(1.0f, 2.0f);
    std::uniform_real_distribution<float> dist_angular(-2.0f, 2.0f);

    cmd_atual_.linear.x = dist_linear(rng_);
    cmd_atual_.angular.z = dist_angular(rng_);
  }

  void publicar_velocidade() {
    if (!vitima_viva_ || !vitima_pose_recebida_) {
      return;
    }

    // Rebater nas paredes se estiver próximo das bordas do turtlesim (0 a 11)
    if (posicao_vitima_.x < 1.5f || posicao_vitima_.x > 9.5f ||
        posicao_vitima_.y < 1.5f || posicao_vitima_.y > 9.5f) {
      cmd_atual_.linear.x = 0.8f;
      cmd_atual_.angular.z = 2.5f; // Virar rápido para longe da parede
    }

    pub_cmd_vel_->publish(cmd_atual_);
  }

  void verificar_captura() {
    if (!vitima_viva_ || !vitima_pose_recebida_ || !pegador_pose_recebida_) {
      return;
    }

    float dx = posicao_pegador_.x - posicao_vitima_.x;
    float dy = posicao_pegador_.y - posicao_vitima_.y;
    float distancia_sq = dx * dx + dy * dy;

    // Raio de captura: 0.8 unidades (distancia_sq < 0.64)
    if (distancia_sq < 0.64f) {
      vitima_viva_ = false;
      RCLCPP_WARN(this->get_logger(),
                  "Vitima pego pelo pegador! Notificando respawn e eliminando vitima...");

      // Notificar o game_manager para criar uma nova vitima em posicao aleatoria
      std_msgs::msg::Empty msg_respawn;
      pub_respawn_->publish(msg_respawn);

      // Eliminar a vitima atual
      matar("vitima");
    }
  }

  void matar(const std::string &nome) {
    if (!kill_client_->service_is_ready()) {
      RCLCPP_WARN(this->get_logger(), "Servico /kill nao disponivel no turtlesim.");
      return;
    }

    auto request = std::make_shared<turtlesim::srv::Kill::Request>();
    request->name = nome;

    kill_client_->async_send_request(
        request,
        [this](rclcpp::Client<turtlesim::srv::Kill>::SharedFuture) {
          RCLCPP_INFO(this->get_logger(), "Vitima eliminada do turtlesim!");
        });
  }

  rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr sub_pegador_;
  rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr sub_vitima_;
  rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr sub_respawn_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_vel_;
  rclcpp::Publisher<std_msgs::msg::Empty>::SharedPtr pub_respawn_;
  rclcpp::Client<turtlesim::srv::Kill>::SharedPtr kill_client_;

  rclcpp::TimerBase::SharedPtr move_timer_;
  rclcpp::TimerBase::SharedPtr control_timer_;

  turtlesim::msg::Pose posicao_pegador_;
  turtlesim::msg::Pose posicao_vitima_;
  geometry_msgs::msg::Twist cmd_atual_;

  bool pegador_pose_recebida_{false};
  bool vitima_pose_recebida_{false};
  bool vitima_viva_{true};
  bool aguardando_respawn_{false};

  std::mt19937 rng_;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<TurtleVitima>());
  rclcpp::shutdown();
  return 0;
}