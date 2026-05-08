#include <rclcpp/rclcpp.hpp>

#include <example_interfaces/msg/int64.hpp> //como eu sei a sintaxe exata?

using namespace std::chrono_literals;

class PublisherNumero : public rclcpp::Node {
public: 
    PublisherNumero() : Node("publisher_numero"){
        publisher_ = this->create_publisher<example_interfaces::msg::Int64>("numero", 10);
        timer_ = this->create_wall_timer(0.5s, std::bind(&PublisherNumero::publica_numero, this));
        RCLCPP_INFO(this->get_logger(), "Publisher iniciado!");
    }

private:
    rclcpp::Publisher<example_interfaces::msg::Int64>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
    int contador_ {0};

    void publica_numero() {
        auto numero = example_interfaces::msg::Int64();
        contador_++;
        numero.data = contador_;

        //publicar string mostrando o numero que foi postado, ele fez no video um igual.
        RCLCPP_INFO(this->get_logger(), "Publicando o numero %ld", numero.data);
        publisher_->publish(numero);
    }
};

int main(int argc, char * argv[]){
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<PublisherNumero>());
    rclcpp::shutdown();

    return 0;
}