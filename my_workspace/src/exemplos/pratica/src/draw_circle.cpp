#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp" // <-- Notice the new include!

class DrawCircleNode : public rclcpp::Node
{
public:
    DrawCircleNode() : Node("draw_circle")
    {
        // 1. Create a Publisher. We publish TwistStamped messages to "/cmd_vel".
        publisher_ = this->create_publisher<geometry_msgs::msg::TwistStamped>("/cmd_vel", 10);

        // 2. Create a timer that calls the send_velocity_command function every 500 milliseconds (2 Hz)
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(500),
            std::bind(&DrawCircleNode::send_velocity_command, this));
            
        RCLCPP_INFO(this->get_logger(), "Draw Circle Node (TwistStamped) has been started.");
    }

private:
    void send_velocity_command()
    {
        // 3. Create a TwistStamped message
        auto msg = geometry_msgs::msg::TwistStamped();
        
        // 4. Fill in the header information!
        msg.header.stamp = this->get_clock()->now(); // Current time
        msg.header.frame_id = "base_link";           // Relative to the robot

        // 5. Fill in the velocity values
        msg.twist.linear.x = 0.2;  // Move forward
        msg.twist.angular.z = 0.5; // Rotate

        // 6. Publish the message!
        publisher_->publish(msg);
    }

    rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<DrawCircleNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
