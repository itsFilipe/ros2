#include "meu_projeto_interfaces/msg/led_status.hpp"
#include "meu_projeto_interfaces/srv/set_led.hpp"
#include <memory>
#include <rclcpp/rclcpp.hpp>

using namespace std::chrono_literals;
using LedStatus = meu_projeto_interfaces::msg::LedStatus;
using SetLed = meu_projeto_interfaces::srv::SetLed;

class LedPanel : public rclcpp::Node {
public:
  LedPanel() : Node("led_panel_node") {
    // Internal LED panel state: 3 LEDs, all off
    led_panel_ = {0, 0, 0};

    // Publisher: broadcasts current panel state every 4 seconds
    publisher_ = this->create_publisher<LedStatus>("led_panel_state", 10);
    timer_ = this->create_wall_timer(
        4s, std::bind(&LedPanel::publish_led_state, this));

    // Service server: allows other nodes to set individual LEDs
    server_ = this->create_service<SetLed>(
        "set_led", std::bind(&LedPanel::callback_set_led, this,
                             std::placeholders::_1, std::placeholders::_2));

    RCLCPP_INFO(this->get_logger(), "LED Panel node started. LEDs: [0, 0, 0]");
  }

private:
  // Publish current LED panel state
  void publish_led_state() {
    LedStatus msg;
    msg.numeros = led_panel_;
    publisher_->publish(msg);
    RCLCPP_INFO(this->get_logger(), "LED panel state: [%ld, %ld, %ld]",
                led_panel_[0], led_panel_[1], led_panel_[2]);
  }

  // Service callback: turn a specific LED on or off
  void callback_set_led(const std::shared_ptr<SetLed::Request> request,
                        std::shared_ptr<SetLed::Response> response) {

    int64_t idx = request->led_number - 1; // convert 1-indexed to 0-indexed
    if (idx < 0 || idx >= static_cast<int64_t>(led_panel_.size())) {
      RCLCPP_WARN(this->get_logger(),
                  "Invalid LED number: %ld. Valid range: 1-%zu",
                  request->led_number, led_panel_.size());
      response->success = false;
      return;
    }

    led_panel_[idx] = request->state ? 1 : 0;
    response->success = true;

    RCLCPP_INFO(this->get_logger(),
                "LED %ld set to %s → panel: [%ld, %ld, %ld]",
                request->led_number, request->state ? "ON" : "OFF",
                led_panel_[0], led_panel_[1], led_panel_[2]);
  }

  // Attributes
  std::vector<int64_t> led_panel_;
  rclcpp::Publisher<LedStatus>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Service<SetLed>::SharedPtr server_;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LedPanel>());
  rclcpp::shutdown();
  return 0;
}