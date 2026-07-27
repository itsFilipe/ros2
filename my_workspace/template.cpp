#include <rclcpp/rclcpp.hpp>
// estrutura de mensagem

class ClassName : public rclcpp::Node {
public: // publisher, timer, subscription are always on constructor?
  ClassName() : Node("node_name") {}

private:
  // methods
  // attributes
  /*
  rclcpp::Subscription<example_interfaces::msg::Int64>::SharedPtr subscriber_;
  rclcpp::Publisher<example_interfaces::msg::Int64>::SharedPtr publisher_;
  rclcpp::Service<example_interfaces::srv::SetBool>::SharedPtr service_;
    rclcpp::Client<SetLed>::SharedPtr client_;

  */
};

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ClassName>());
  rclcpp::shutdown();
  return 0;
}