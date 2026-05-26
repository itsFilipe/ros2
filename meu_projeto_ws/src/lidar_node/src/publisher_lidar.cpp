#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32_multi_array.hpp"
#include <chrono>
#include <memory>
#include <random>
#include <vector>

using namespace std::chrono_literals;

auto topic1 = "lidar_topic";

class lidarPublisher : public rclcpp::Node {
public:
    lidarPublisher() : Node("lidar_publisher") {
        pub1_ = this->create_publisher<std_msgs::msg::Int32MultiArray>(topic1, 10);
        timer1_ = this->create_wall_timer(
            5s, std::bind(&lidarPublisher::create_data, this));
    }

private:
        void create_data() {
        resolution = 360;
        maxDistance = 115;

        std::uniform_int_distribution<> distr(1, maxDistance);

        auto message = std_msgs::msg::Int32MultiArray();
        message.data.resize(resolution);

        for (size_t i = 0; i < resolution; i++) {
            message.data[i] = distr(gen);
        }

        pub1_->publish(message);  
    }

    rclcpp::Publisher<std_msgs::msg::Int32MultiArray>::SharedPtr pub1_;
    rclcpp::TimerBase::SharedPtr timer1_;
    std::mt19937 gen{std::random_device{}()};
    size_t maxDistance;
    size_t resolution;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<lidarPublisher>());
    rclcpp::shutdown();
    return 0;
}