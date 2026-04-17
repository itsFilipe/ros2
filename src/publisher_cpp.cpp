#include "rclcpp/rclcpp.hpp"

#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/float32.hpp"

#include <chrono>

#include <string>

#include <memory>

using namespace std::chrono_literals;

auto topic1 = "float_topic";
auto topic2 = "string_topic";

class MinimalPublisher : public rclcpp::Node {
public:
    MinimalPublisher() : Node("minimal_publisher") {
        publisher1_ = this->create_publisher<std_msgs::msg::Float32>(topic1, 10);
        publisher2_ = this->create_publisher<std_msgs::msg::String>(topic2, 10);
        timer1_ = this->create_wall_timer(
            1s, std::bind(&MinimalPublisher::timer_callback1, this));
        timer2_ = this->create_wall_timer(
            1s, std::bind(&MinimalPublisher::timer_callback2, this));
    }

private:
    void timer_callback1() {
        auto message = std_msgs::msg::Float32();
        message.data = 1.0;
        publisher1_->publish(message);
    }

    void timer_callback2() {
        auto message = std_msgs::msg::String();
        message.data = "Hello World";
        publisher2_->publish(message);
    }

    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisher1_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher2_;
    rclcpp::TimerBase::SharedPtr timer1_;
    rclcpp::TimerBase::SharedPtr timer2_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<MinimalPublisher>());
    rclcpp::shutdown();
    return 0;
}   