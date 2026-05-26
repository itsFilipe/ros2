#include <rclcpp/rclcpp.hpp>
#include <example_interfaces/srv/add_two_ints.hpp>

using namespace std::placeholders;

class AddTwoIntServerNode : public rclcpp::Node {
public:
    AddTwoIntServerNode() : Node("add_two_ints_server") {
        server_ = this->create_service<example_interfaces::srv::AddTwoInts>(
            "add_two_ints",
            std::bind(&AddTwoIntServerNode::callbackAddTwoInts, this, _1, _2));
        RCLCPP_INFO(this->get_logger(), "Add Two Ints Service foi iniciado.");
    }
private:
    void callbackAddTwoInts(const example_interfaces::srv::AddTwoInts::Request::SharedPtr request,
                            const example_interfaces::srv::AddTwoInts::Response::SharedPtr response)
    {
        //action, turn led, motor, etc
        response->sum = request->a + request->b;
        RCLCPP_INFO(this->get_logger(), "%d + %d = %d",
                    (int)request->a, (int)request->b, (int)response->sum);
    }


    rclcpp::Service<example_interfaces::srv::AddTwoInts>::SharedPtr server_;
};

int main(int argc, char* argv[]){
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<AddTwoIntServerNode>());
    rclcpp::shutdown();
    return 0;
}