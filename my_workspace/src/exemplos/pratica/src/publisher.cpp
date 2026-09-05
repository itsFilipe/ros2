#include <rclcpp/rclcpp.hpp>
#include "example_interfaces/msg/int64.hpp"
#include "std_srvs/srv/set_bool.hpp"

using namespace std::chrono_literals;

class Publicador : public rclcpp::Node {
public: // publisher, timer, subscription are always on constructor?
  Publicador() : Node("publicador") {
    publisher_ = this->create_publisher<example_interfaces::msg::Int64>("contador", 10);
    timer_ = this->create_wall_timer(500ms, std::bind(&Publicador::publish, this));

    service_ = this->create_service<std_srvs::srv::SetBool>(
      "resetar_contador",
      std::bind(
        &Publicador::resetar_contador_callback, this,
        std::placeholders::_1, std::placeholders::_2)); 
  }

private:

  void publish(){
    contador_++;
    auto message = example_interfaces::msg::Int64();
    message.data = contador_;

    publisher_->publish(message);
    RCLCPP_INFO(this->get_logger(), "Publishing: '%d'", contador_);
  }

    void resetar_contador_callback(
        const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
        std::shared_ptr<std_srvs::srv::SetBool::Response> response)
    {
        if (request->data) {
        contador_ = 0;
        response->success = true;
        response->message = "Contador resetado para zero.";
        } else {
        response->success = false;
        response->message = "Nao foi possivel resetar o contador.";
        }
    }

  size_t contador_ {0};
  rclcpp::Publisher<example_interfaces::msg::Int64>::SharedPtr publisher_;
  rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr service_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Publicador>());
  rclcpp::shutdown();
  return 0;
}