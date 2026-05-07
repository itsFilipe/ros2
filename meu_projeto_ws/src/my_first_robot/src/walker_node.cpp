#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include <chrono>

using namespace std::chrono_literals;

// Criamos uma classe que herda de Node. Isso é POO pura!
class WalkerNode : public rclcpp::Node {
public:
    WalkerNode() : Node("walker_node") {
        // Criamos un "Publisher" que envia mensagens do tipo Twist no tópico da tartaruga
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/turtle1/cmd_vel", 10);
        
        // Criamos um Timer que executa a função 'move_robot' a cada 500ms
        timer_ = this->create_wall_timer(500ms, std::bind(&WalkerNode::move_robot, this));
    }

private:
    void move_robot() {
        auto message = geometry_msgs::msg::Twist();
        message.linear.x = 1.0;  // Vai para frente
        message.angular.z = 0.5; // Gira um pouco
        
        RCLCPP_INFO(this->get_logger(), "Enviando comando de movimento...");
        publisher_->publish(message);
    }

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<WalkerNode>());
    rclcpp::shutdown();
    return 0;
}