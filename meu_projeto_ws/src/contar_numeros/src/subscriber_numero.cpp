#include <rclcpp/rclcpp.hpp>
#include <example_interfaces/msg/int64.hpp>

using std::placeholders::_1;

class SubscriberNum : public rclcpp::Node {
public:
    SubscriberNum() : Node("subscriber_numero"){
        subscriber_ = this->create_subscription<example_interfaces::msg::Int64>("numero", 10, std::bind(&SubscriberNum::callback, this, _1));
        publisher_  = this->create_publisher<example_interfaces::msg::Int64>("contador", 10); 

        RCLCPP_INFO(this->get_logger(), "Subscriber e Publisher inicializados...");
    }
private:
    rclcpp::Subscription<example_interfaces::msg::Int64>::SharedPtr subscriber_;
    rclcpp::Publisher<example_interfaces::msg::Int64>::SharedPtr publisher_;
    int64_t contador_ {0};

    void callback(const example_interfaces::msg::Int64::SharedPtr msg) { //eu poderia criar essa variavel dentro da função? qual seria a diferença
        contador_ = msg->data;
        RCLCPP_INFO(this->get_logger(), "Contador: %ld", contador_);

        auto out_msg = example_interfaces::msg::Int64();
        out_msg.data = contador_;
        publisher_->publish(out_msg);
    }
};

int main(int argc, char * argv[]){
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<SubscriberNum>());
    rclcpp::shutdown();
}

/* Quando um node já é um subscriber, ele já é ativado automaticamente pelo topico,
   Se precisar de um publisher nesse node, não é necessário timer, use o mesmo tempo do subscriber
   ROS2 é event-driven
*/