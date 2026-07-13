#include "rcl_interfaces/msg/parameter.hpp"
#include "rcl_interfaces/srv/set_parameters.hpp"
#include <rclcpp/rclcpp.hpp>

using SetParameters = rcl_interfaces::srv::SetParameters;
using Parameter = rcl_interfaces::msg::Parameter;
using namespace std::chrono_literals;

class AlterarBackground : public rclcpp::Node {
public:
  AlterarBackground() : Node("alterar_background") {
    this->declare_parameter("r", 0);
    this->declare_parameter("g", 0);
    this->declare_parameter("b", 255);

    client_ = this->create_client<SetParameters>("/turtlesim/set_parameters");

    alterar_bg();
  }

  void alterar_bg() {
    if (!client_->wait_for_service(2s)) {
      RCLCPP_WARN(
          this->get_logger(),
          "set_parameters service not available. Is turtlesim running?");
      return;
    }

    int r = this->get_parameter("r").as_int();
    int g = this->get_parameter("g").as_int();
    int b = this->get_parameter("b").as_int();

    auto request = std::make_shared<SetParameters::Request>();

    Parameter param_r;
    param_r.name = "background_r";
    param_r.value.type = 2;
    param_r.value.integer_value = r;
    request->parameters.push_back(param_r);

    Parameter param_g;
    param_g.name = "background_g";
    param_g.value.type = 2;
    param_g.value.integer_value = g;
    request->parameters.push_back(param_g);

    Parameter param_b;
    param_b.name = "background_b";
    param_b.value.type = 2;
    param_b.value.integer_value = b;
    request->parameters.push_back(param_b);

    client_->async_send_request(
        request, [this](rclcpp::Client<SetParameters>::SharedFuture future) {
          auto response = future.get();
          bool all_ok = true;
          for (auto &result : response->results) {
            if (!result.successful) {
              all_ok = false;
            }
          }
          if (all_ok) {
            RCLCPP_INFO(this->get_logger(),
                        "Background color changed successfully!");
          } else {
            RCLCPP_ERROR(this->get_logger(),
                         "Failed to change background color.");
          }
        });
  }

private:
  rclcpp::Client<SetParameters>::SharedPtr client_;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<AlterarBackground>());
  rclcpp::shutdown();
  return 0;
}