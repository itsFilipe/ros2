#include <rclcpp/rclcpp.hpp>
#include "example_interfaces/msg/int64.hpp"
#include "std_srvs/srv/set_bool.hpp"

using std::placeholders::_1;

class Escutador : public rclcpp::Node {
public: 
  Escutador() : Node("escutador") {
    subscriber_ = this->create_subscription<example_interfaces::msg::Int64>(
        "contador", 10, std::bind(&Escutador::callback, this, _1));
    client_ = this->create_client<std_srvs::srv::SetBool>("resetar_contador");
  }

private:
  
  void callback(const example_interfaces::msg::Int64 msg) const {
    if (msg.data % 2 == 0) {
        RCLCPP_INFO(this->get_logger(), "O numero %ld eh par\n", msg.data);
    }

    if (msg.data == 100) {
        auto request = std::make_shared<std_srvs::srv::SetBool::Request>();
        request->data = true;
        client_->async_send_request(request);
    }
  }

  rclcpp::Subscription<example_interfaces::msg::Int64>::SharedPtr subscriber_;
  rclcpp::Client<std_srvs::srv::SetBool>::SharedPtr client_;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Escutador>());
  rclcpp::shutdown();
  return 0;
}