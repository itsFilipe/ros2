#include <rclcpp/rclcpp.hpp>
#include <example_interfaces/msg/int64.hpp>
#include <example_interfaces/srv/set_bool.hpp>

using namespace std::placeholders;

class SubscriberNum : public rclcpp::Node {
public:
    SubscriberNum() : Node("subscriber_numero"){
        subscriber_ = this->create_subscription<example_interfaces::msg::Int64>("numero", 10, std::bind(&SubscriberNum::callback, this, _1));
        publisher_  = this->create_publisher<example_interfaces::msg::Int64>("contador", 10); 
        service_    = this->create_service<example_interfaces::srv::SetBool>("reset_counter", std::bind(&SubscriberNum::callback_server, this, _1, _2));

        RCLCPP_INFO(this->get_logger(), "Subscriber e Publisher inicializados...");
    }
private:
    rclcpp::Subscription<example_interfaces::msg::Int64>::SharedPtr subscriber_;
    rclcpp::Publisher<example_interfaces::msg::Int64>::SharedPtr publisher_;
    rclcpp::Service<example_interfaces::srv::SetBool>::SharedPtr service_;
    int64_t contador_ {0};

    void callback(const example_interfaces::msg::Int64::SharedPtr msg) { 
        //contador_ = msg->data;
        RCLCPP_UNUSED(msg); 
        contador_++;
        RCLCPP_INFO(this->get_logger(), "Contador: %ld", contador_);

        auto out_msg = example_interfaces::msg::Int64();
        out_msg.data = contador_;
        publisher_->publish(out_msg);
    }

    void callback_server(const std::shared_ptr<example_interfaces::srv::SetBool::Request> request,
                         std::shared_ptr<example_interfaces::srv::SetBool::Response> response) {
        if (request->data == true){
            contador_ = 0;
            response->success = true;
            response->message = "Contador reiniciado";
        } else {
            response->success = false;
            response->message = "Contador se mantem";
        }
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