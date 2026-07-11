#include "meu_projeto_interfaces/srv/set_led.hpp"
#include <chrono>
#include <memory>
#include <rclcpp/rclcpp.hpp>

using namespace std::chrono_literals;
using SetLed = meu_projeto_interfaces::srv::SetLed;

class Battery : public rclcpp::Node {
public:
  Battery() : Node("battery_node") {
    // Start with full battery, all LEDs off
    battery_state_ = 1; // 1 = full, 0 = empty
    start_time_ = this->now();

    // Service client to control the LED panel
    client_ = this->create_client<SetLed>("set_led");

    // Timer: checks battery state every 100ms
    timer_ = this->create_wall_timer(100ms,
                                     std::bind(&Battery::check_battery, this));

    RCLCPP_INFO(this->get_logger(),
                "Battery node started. Battery FULL. Draining in 4s...");
  }

private:
  void check_battery() {
    double elapsed = (this->now() - start_time_).seconds();

    // After 4s: battery empty → turn LED 1 ON
    if (battery_state_ == 1 && elapsed >= 4.0) {
      battery_state_ = 0;
      start_time_ = this->now(); // reset timer
      RCLCPP_INFO(this->get_logger(),
                  "Battery EMPTY! Turning LED ON. Charging for 6s...");
      call_set_led(1, true);
    }
    // After 6s more: battery full → turn LED 1 OFF
    else if (battery_state_ == 0 && elapsed >= 6.0) {
      battery_state_ = 1;
      start_time_ = this->now(); // reset timer
      RCLCPP_INFO(this->get_logger(),
                  "Battery FULL! Turning LED OFF. Draining in 4s...");
      call_set_led(1, false);
    }
  }

  void call_set_led(int64_t led_number, bool state) {
    // Wait for the service to be available
    if (!client_->wait_for_service(1s)) {
      RCLCPP_WARN(this->get_logger(),
                  "set_led service not available. Is led_panel_node running?");
      return;
    }

    auto request = std::make_shared<SetLed::Request>();
    request->led_number = led_number;
    request->state = state;

    // Send async request with a callback for the response
    client_->async_send_request(
        request,
        [this, led_number, state](rclcpp::Client<SetLed>::SharedFuture future) {
          auto response = future.get();
          if (response->success) {
            RCLCPP_INFO(this->get_logger(), "LED %ld successfully set to %s.",
                        led_number, state ? "ON" : "OFF");
          } else {
            RCLCPP_ERROR(this->get_logger(), "Failed to set LED %ld!",
                         led_number);
          }
        });
  }

  // Attributes
  int battery_state_;
  rclcpp::Time start_time_;
  rclcpp::Client<SetLed>::SharedPtr client_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Battery>());
  rclcpp::shutdown();
  return 0;
}
