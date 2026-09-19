#include "geometry_msgs/msg/twist_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp" // <-- New include for LiDAR!

class ObstacleAvoidanceNode : public rclcpp::Node {
public:
  ObstacleAvoidanceNode() : Node("obstacle_avoidance") {
    // 1. Publisher for velocity
    publisher_ = this->create_publisher<geometry_msgs::msg::TwistStamped>(
        "/cmd_vel", 10);

    // 2. Subscriber for LiDAR.
    // Whenever a new LaserScan message arrives, it triggers the 'scan_callback'
    // function
    subscriber_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
        "/scan", 10,
        std::bind(&ObstacleAvoidanceNode::scan_callback, this,
                  std::placeholders::_1));

    RCLCPP_INFO(this->get_logger(),
                "Obstacle Avoidance Node has been started.");
  }

private:
  void scan_callback(const sensor_msgs::msg::LaserScan::SharedPtr msg) {
    auto cmd = geometry_msgs::msg::TwistStamped();
    cmd.header.stamp = this->get_clock()->now();
    cmd.header.frame_id = "base_link";

    // 3. The LiDAR sends an array of distances called 'ranges'.
    // In Gazebo for the TurtleBot3 Burger, index 0 is directly in front of the
    // robot! Let's check how far away the wall is directly in front of us.
    float front_distance = msg->ranges[0];

    RCLCPP_INFO(this->get_logger(), "Distance straight ahead: %f meters",
                front_distance);

    // 4. Simple Brain: If an obstacle is closer than 1.0 meter, turn!
    // Otherwise, go forward.
    if (front_distance < 1.0) {
      // Turn left in place
      cmd.twist.linear.x = 0.0;
      cmd.twist.angular.z = 0.5;
    } else {
      // Drive forward safely
      cmd.twist.linear.x = 0.2;
      cmd.twist.angular.z = 0.0;
    }

    // 5. Publish the decision
    publisher_->publish(cmd);
  }

  rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr publisher_;
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr subscriber_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<ObstacleAvoidanceNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
