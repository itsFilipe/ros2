/**
 * motor_demo_node.cpp
 *
 * Node de demonstração que publica automaticamente em /motor_cmd,
 * fazendo o motor executar uma sequência animada em loop:
 *
 *   1. Acelera de 0 → 100 (frente)
 *   2. Desacelera de 100 → 0
 *   3. Pausa 1 segundo
 *   4. Acelera de 0 → -100 (reverso)
 *   5. Desacelera de -100 → 0
 *   6. Pausa 1 segundo
 *   ... repete
 *
 * COMO USAR:
 *   Terminal 1: ros2 run motor_bridge motor_bridge_node --ros-args -p serial_port:=/dev/ttyUSB0
 *   Terminal 2: ros2 run motor_bridge motor_demo_node
 *
 * COMO PARAR O MOTOR COM SEGURANÇA:
 *   Ctrl+C no motor_demo_node → o motor para automaticamente (envia velocidades 0)
 *   O failsafe do Arduino também para tudo em 2 segundos.
 */

#include <chrono>
#include <cmath>   // std::sin, M_PI
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"

using namespace std::chrono_literals;

class MotorDemoNode : public rclcpp::Node
{
public:
  MotorDemoNode()
  : Node("motor_demo_node"),
    step_(0),
    phase_(Phase::RAMP_UP_FORWARD)
  {
    publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);

    // Timer a 20 Hz — publica um novo valor a cada 50ms
    // Suficientemente rápido para uma aceleração suave e visível
    timer_ = this->create_wall_timer(
      50ms,
      std::bind(&MotorDemoNode::timer_callback, this));

    RCLCPP_INFO(this->get_logger(),
      "motor_demo_node iniciado! Publicando em /cmd_vel a 20 Hz.");
    RCLCPP_INFO(this->get_logger(),
      "Ctrl+C para parar (motor para automaticamente).");
  }

  // Destrutor: envia 0 antes de fechar para parar o motor
  ~MotorDemoNode()
  {
    auto msg = geometry_msgs::msg::Twist();
    msg.linear.x = 0.0;
    msg.angular.z = 0.0;
    publisher_->publish(msg);
    RCLCPP_INFO(this->get_logger(), "Motor parado. Até logo!");
  }

private:
  enum class Phase {
    RAMP_UP_FORWARD,    // acelera 0 → 100
    RAMP_DOWN_FORWARD,  // desacelera 100 → 0
    PAUSE_AFTER_FWD,    // pausa parado
    RAMP_UP_REVERSE,    // acelera 0 → -100
    RAMP_DOWN_REVERSE,  // desacelera -100 → 0
    PAUSE_AFTER_REV,    // pausa parado
  };

  void timer_callback()
  {
    auto msg = geometry_msgs::msg::Twist();
    msg.angular.z = 0.0; // O demo só move para frente e para trás

    // Cada fase tem um número de steps a 20Hz:
    // RAMP: 60 steps × 50ms = 3 segundos para ir de 0 a 1.0
    // PAUSE: 20 steps × 50ms = 1 segundo parado
    const int RAMP_STEPS  = 60;
    const int PAUSE_STEPS = 20;

    switch (phase_) {
      case Phase::RAMP_UP_FORWARD:
        // Usa seno para uma aceleração mais suave (curva S)
        // sin(0 → π/2) vai de 0 a 1 suavemente
        msg.linear.x = 1.0 * std::sin((M_PI / 2.0) * step_ / RAMP_STEPS);
        if (++step_ > RAMP_STEPS) { step_ = 0; phase_ = Phase::RAMP_DOWN_FORWARD; }
        break;

      case Phase::RAMP_DOWN_FORWARD:
        msg.linear.x = 1.0 * std::cos((M_PI / 2.0) * step_ / RAMP_STEPS);
        if (++step_ > RAMP_STEPS) { step_ = 0; phase_ = Phase::PAUSE_AFTER_FWD; }
        break;

      case Phase::PAUSE_AFTER_FWD:
        msg.linear.x = 0.0;
        if (++step_ > PAUSE_STEPS) { step_ = 0; phase_ = Phase::RAMP_UP_REVERSE; }
        break;

      case Phase::RAMP_UP_REVERSE:
        msg.linear.x = -1.0 * std::sin((M_PI / 2.0) * step_ / RAMP_STEPS);
        if (++step_ > RAMP_STEPS) { step_ = 0; phase_ = Phase::RAMP_DOWN_REVERSE; }
        break;

      case Phase::RAMP_DOWN_REVERSE:
        msg.linear.x = -1.0 * std::cos((M_PI / 2.0) * step_ / RAMP_STEPS);
        if (++step_ > RAMP_STEPS) { step_ = 0; phase_ = Phase::PAUSE_AFTER_REV; }
        break;

      case Phase::PAUSE_AFTER_REV:
        msg.linear.x = 0.0;
        if (++step_ > PAUSE_STEPS) { step_ = 0; phase_ = Phase::RAMP_UP_FORWARD; }
        break;
    }

    publisher_->publish(msg);

    RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 500,
      "Publicando linear.x: %.2f", msg.linear.x);
  }

  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  int step_;
  Phase phase_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MotorDemoNode>());
  rclcpp::shutdown();
  return 0;
}
