#include "geometry_msgs/msg/twist.hpp"
#include "turtlesim/msg/pose.hpp"
#include <cmath>
#include <rclcpp/rclcpp.hpp>

using namespace std::placeholders;
using namespace std::chrono_literals;

class TurtlePegador : public rclcpp::Node {
public:
  TurtlePegador() : Node("turtle_pegador") {
    sub_pegador_pose_ = this->create_subscription<turtlesim::msg::Pose>(
        "/pegador/pose", 10,
        std::bind(&TurtlePegador::pegador_pose_callback, this, _1));

    sub_vitima_pose_ = this->create_subscription<turtlesim::msg::Pose>(
        "/vitima/pose", 10,
        std::bind(&TurtlePegador::vitima_pose_callback, this, _1));

    pub_cmd_vel_ = this->create_publisher<geometry_msgs::msg::Twist>(
        "/pegador/cmd_vel", 10);

    // Timer de controle de movimento continuo (20 Hz -> 50ms)
    control_timer_ = this->create_wall_timer(
        50ms, std::bind(&TurtlePegador::control_loop, this));
  }

private:
  void pegador_pose_callback(const turtlesim::msg::Pose::SharedPtr msg) {
    pegador_pose_ = *msg;
    has_pegador_pose_ = true;
  }

  void vitima_pose_callback(const turtlesim::msg::Pose::SharedPtr msg) {
    vitima_pose_ = *msg;
    has_vitima_pose_ = true;
    last_vitima_time_ = this->now();
  }

  void control_loop() {
    // Se não recebeu ambas as poses ou se a vítima foi morta (sem atualização há mais de 1.5s)
    if (!has_pegador_pose_ || !has_vitima_pose_) {
      return;
    }

    if ((this->now() - last_vitima_time_).seconds() > 1.5) {
      // Parar o pegador se a vítima não existir
      geometry_msgs::msg::Twist stop_msg;
      pub_cmd_vel_->publish(stop_msg);
      return;
    }

    float dx = vitima_pose_.x - pegador_pose_.x;
    float dy = vitima_pose_.y - pegador_pose_.y;
    float distance = std::sqrt(dx * dx + dy * dy);

    float target_theta = std::atan2(dy, dx);
    float angle_error = std::atan2(std::sin(target_theta - pegador_pose_.theta),
                                  std::cos(target_theta - pegador_pose_.theta));

    geometry_msgs::msg::Twist cmd;

    // Se estiver alinhando o ângulo, reduz a velocidade linear para curvas suaves
    if (std::abs(angle_error) > 0.8f) {
      cmd.linear.x = 0.5;
    } else {
      cmd.linear.x = std::min(2.0f, 1.2f * distance);
    }

    cmd.angular.z = 4.0f * angle_error;

    pub_cmd_vel_->publish(cmd);
  }

  rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr sub_pegador_pose_;
  rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr sub_vitima_pose_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_vel_;
  rclcpp::TimerBase::SharedPtr control_timer_;

  turtlesim::msg::Pose pegador_pose_;
  turtlesim::msg::Pose vitima_pose_;

  bool has_pegador_pose_{false};
  bool has_vitima_pose_{false};
  rclcpp::Time last_vitima_time_;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<TurtlePegador>());
  rclcpp::shutdown();
  return 0;
}