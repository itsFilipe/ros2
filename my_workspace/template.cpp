#include <rclcpp/rclcpp.hpp>
//estrutura de mensagem

class ClassName : public rclcpp::Node {
public: //publisher, timer, subscription are always on constructor?
    ClassName() : Node("node_name") {

    }
private:
    //methods
    //attributes
};

int main(int argc, char* argv[]){
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ClassName>());
    rclcpp::shutdown();
    return 0;
}