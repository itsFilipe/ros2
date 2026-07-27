#include "rcl_interfaces/msg/parameter.hpp"
#include "rcl_interfaces/msg/parameter_type.hpp"
#include "rcl_interfaces/srv/set_parameters.hpp"
#include "std_msgs/msg/empty.hpp"
#include "turtlesim/srv/kill.hpp"
#include "turtlesim/srv/spawn.hpp"
#include <random>
#include <rclcpp/rclcpp.hpp>

using namespace std::chrono_literals;

class GameManager : public rclcpp::Node {
public:
  GameManager() : Node("game_manager"), rng_(std::random_device{}()) {
    spawn_client_ = this->create_client<turtlesim::srv::Spawn>("/spawn");
    kill_client_  = this->create_client<turtlesim::srv::Kill>("/kill");
    bg_client_    = this->create_client<rcl_interfaces::srv::SetParameters>("/turtlesim/set_parameters");

    respawn_sub_ = this->create_subscription<std_msgs::msg::Empty>(
        "/respawn_vitima", 10,
        std::bind(&GameManager::respawn_callback, this, std::placeholders::_1));

    // Timer para iniciar o jogo apos conexao com os servicos do turtlesim
    timer_iniciar_ = this->create_wall_timer(
        500ms, std::bind(&GameManager::iniciar_jogo, this));
  }

private:
  void iniciar_jogo() {
    timer_iniciar_->cancel(); // Executa apenas uma vez

    if (!spawn_client_->wait_for_service(3s)) {
      RCLCPP_ERROR(this->get_logger(), "Servico /spawn nao disponivel!");
      return;
    }

    // Remover a turtle1 padrao se ela existir
    if (kill_client_->service_is_ready() || kill_client_->wait_for_service(1s)) {
      auto req_kill = std::make_shared<turtlesim::srv::Kill::Request>();
      req_kill->name = "turtle1";
      kill_client_->async_send_request(req_kill);
    }

    // Spawnar pegador no centro
    auto req_pegador = std::make_shared<turtlesim::srv::Spawn::Request>();
    req_pegador->name = "pegador";
    req_pegador->x = 5.5;
    req_pegador->y = 5.5;
    req_pegador->theta = 0.0;
    spawn_client_->async_send_request(req_pegador);

    // Spawnar a primeira vitima em posicao aleatoria
    spawn_vitima_aleatoria();

    RCLCPP_INFO(this->get_logger(), "Jogo iniciado! Pegador no centro e Vitima pronta.");
  }

  void respawn_callback(const std_msgs::msg::Empty::SharedPtr) {
    RCLCPP_INFO(this->get_logger(), "Solicitacao de respawn recebida. Spawnando nova vitima...");

    // Muda a cor de fundo ao capturar a vitima
    mudar_cor_background();

    // Pequeno timer para dar tempo do /kill ser finalizado
    respawn_timer_ = this->create_wall_timer(
        500ms, [this]() {
          respawn_timer_->cancel();
          spawn_vitima_aleatoria();
        });
  }

  void mudar_cor_background() {
    if (!bg_client_->service_is_ready()) {
      RCLCPP_WARN(this->get_logger(), "Servico /turtlesim/set_parameters nao disponivel.");
      return;
    }

    std::uniform_int_distribution<int64_t> dist_cor(0, 255);
    int64_t r = dist_cor(rng_);
    int64_t g = dist_cor(rng_);
    int64_t b = dist_cor(rng_);

    // Monta os tres parametros de cor (background_r, background_g, background_b)
    auto fazer_param = [](const std::string &nome, int64_t valor) {
      rcl_interfaces::msg::Parameter param;
      param.name = nome;
      param.value.type = rcl_interfaces::msg::ParameterType::PARAMETER_INTEGER;
      param.value.integer_value = valor;
      return param;
    };

    auto request = std::make_shared<rcl_interfaces::srv::SetParameters::Request>();
    request->parameters = {
        fazer_param("background_r", r),
        fazer_param("background_g", g),
        fazer_param("background_b", b),
    };

    bg_client_->async_send_request(
        request,
        [this, r, g, b](rclcpp::Client<rcl_interfaces::srv::SetParameters>::SharedFuture) {
          RCLCPP_INFO(this->get_logger(),
                      "Background alterado para RGB(%ld, %ld, %ld)!", r, g, b);
        });
  }

  void spawn_vitima_aleatoria() {
    if (!spawn_client_->service_is_ready()) {
      RCLCPP_WARN(this->get_logger(), "Servico /spawn nao pronto para respawn.");
      return;
    }

    std::uniform_real_distribution<float> dist_pos(1.5f, 9.5f);
    std::uniform_real_distribution<float> dist_theta(0.0f, 6.28f);

    float rand_x = dist_pos(rng_);
    float rand_y = dist_pos(rng_);
    float rand_theta = dist_theta(rng_);

    auto req_vitima = std::make_shared<turtlesim::srv::Spawn::Request>();
    req_vitima->name = "vitima";
    req_vitima->x = rand_x;
    req_vitima->y = rand_y;
    req_vitima->theta = rand_theta;

    spawn_client_->async_send_request(
        req_vitima,
        [this, rand_x, rand_y](rclcpp::Client<turtlesim::srv::Spawn>::SharedFuture) {
          RCLCPP_INFO(this->get_logger(), "Nova vitima spawnada em (%.2f, %.2f)!", rand_x, rand_y);
        });
  }

  rclcpp::Client<turtlesim::srv::Spawn>::SharedPtr spawn_client_;
  rclcpp::Client<turtlesim::srv::Kill>::SharedPtr kill_client_;
  rclcpp::Client<rcl_interfaces::srv::SetParameters>::SharedPtr bg_client_;
  rclcpp::Subscription<std_msgs::msg::Empty>::SharedPtr respawn_sub_;
  rclcpp::TimerBase::SharedPtr timer_iniciar_;
  rclcpp::TimerBase::SharedPtr respawn_timer_;

  std::mt19937 rng_;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<GameManager>());
  rclcpp::shutdown();
  return 0;
}