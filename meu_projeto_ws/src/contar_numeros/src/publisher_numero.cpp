#include <rclcpp/rclcpp.hpp>

#include <example_interfaces/msg/int64.hpp> //como eu sei a sintaxe exata?

class PublisherNumero() : public rclcpp::Node {
public: 
    PublisherNumero() : Node("publisher_numero"){
        publisher_ = this->create_publisher<example_interfaces::msg::int64>("numero", 10);
    }

private:
    rclcpp::Publisher<example_interfaces::msg::int64>::SharedPtr publisher_;

    //criar o timer (wall timer) e a funcao que será chamada, a que cria o dado p/ publicar
};

int main(){
    rclcpp::init();
    rclcpp::spin();
    rclcpp::shutdown();

    return 0;
}