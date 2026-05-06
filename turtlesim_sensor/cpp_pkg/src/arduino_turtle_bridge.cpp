#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "turtlesim/msg/pose.hpp"
#include <chrono>
#include <memory>

#include <fcntl.h>   // Contém a função open() e flags como O_RDWR
#include <unistd.h>  // Contém as funções read() e close()
#include <termios.h> 

using namespace std::chrono_literals;
using std::placeholders::_1; //oq é isso?

class ArduinoTurtleBridge : public rclcpp::Node {
public:
    ArduinoTurtleBridge() : Node("arduino_turtle_bridge"){
        file_desc_ = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);
        if (file_desc_ == -1){
            RCLCPP_ERROR(this->get_logger(), "Falha ao abrir a porta serial! O cabo está conectado?");
            throw std::runtime_error("Erro fatal: Porta serial inativa.");
        }

        struct termios tty;

        // Limpa qualquer lixo que tenha ficado preso no tubo do cabo USB antes de começar
        tcflush(file_desc_, TCIOFLUSH);

        // Salva as configurações
        tcsetattr(file_desc_, TCSANOW, &tty);


        if(tcgetattr(file_desc_, &tty) != 0) {
            RCLCPP_ERROR(this->get_logger(), "Erro ao ler as configs da porta serial!");
        }

        cfsetospeed(&tty, B9600);
        cfsetispeed(&tty, B9600);

        tty.c_cflag &= ~PARENB; // Sem paridade
        tty.c_cflag &= ~CSTOPB; // 1 stop bit
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;     // 8 bits de dados
        tty.c_lflag |= ICANON; // Mantém a leitura por linha do enter (\n)
        tty.c_lflag &= ~(ECHO | ECHOE | ECHONL | ISIG); // DESLIGA O ECO E A FOFOCA DO LINUX!

        tcsetattr(file_desc_, TCSANOW, &tty);
            
        // Criamos un "Publisher" que envia mensagens do tipo Twist no tópico da tartaruga
        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/turtle1/cmd_vel", 10);

        subscription_ = this->create_subscription<turtlesim::msg::Pose>(
        "/turtle1/pose", 10, std::bind(&ArduinoTurtleBridge::topic_callback_pose, this, _1)); 
        
        // Criamos um Timer que executa a função 'move_turtle' a cada 100ms
        timer_ = this->create_wall_timer(100ms, std::bind(&ArduinoTurtleBridge::move_turtle, this));

        pos_x = 5.54;
        pos_y = 5.54;
    }

    ~ArduinoTurtleBridge() {
        if (file_desc_ != -1){
            close(file_desc_);
            RCLCPP_INFO(this->get_logger(), "Porta serial fechada com sucesso...");
        }
    }

private:
    void move_turtle() {
        char buffer[64]; //porque64?

        int bytes_lidos = read(file_desc_, buffer, sizeof(buffer));

        if (bytes_lidos > 0) {
        buffer[bytes_lidos] = '\0'; 
        
        int luz = std::atoi(buffer);
        RCLCPP_INFO(this->get_logger(), "VALOR PURO -> Número: %d | Texto: %s", luz, buffer);

        auto message = geometry_msgs::msg::Twist();

        if (luz > 100) {  // Claro!
            // 1. O GATILHO: Se encostar na parede, enche o tanque de fuga!
            if (pos_x > 10.0 || pos_x < 1.0 || pos_y > 10.0 || pos_y < 1.0) { 
                tempo_de_fuga_ = 10; // 10 ciclos de 100ms = 1 Segundo de manobra forçada!
            }

            // 2. A EXECUÇÃO DA FUGA: Enquanto o tanque tiver "combustível"...
            if (tempo_de_fuga_ > 0) {
                message.linear.x = 1.0;  // Anda pra frente
                message.angular.z = 4.0; // Gira muuuito rápido (Curva em "U" perfeita)
                
                write(file_desc_, "R", 1); // SIRENE LIGADA!
                tempo_de_fuga_--;          // Gasta um ciclo do tanque
            } 
            // 3. O PASSEIO TRANQUILO (Só roda se não estiver fugindo)
            else {
                message.linear.x = 1.5;  
                message.angular.z = 0.0; 

                write(file_desc_, "V", 1); // VERDE, PAZ!
            }
        } 
        else {          // Escuro!
            message.linear.x = 0.0; 
            message.angular.z = 0.0; 
            RCLCPP_INFO(this->get_logger(), "Está escuro, pare de andar!");
        }

        publisher_->publish(message);
        }
    }

    void topic_callback_pose(const turtlesim::msg::Pose::SharedPtr msg) {
        //RCLCPP_INFO(this->get_logger(), "X:'%f' Y:'%f'", msg->x, msg->y);
        pos_x = msg->x;
        pos_y = msg->y;
    }

    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
    rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr subscription_;
    rclcpp::TimerBase::SharedPtr timer_;
    int file_desc_;
    float pos_x;
    float pos_y;
    int tempo_de_fuga_ = 0;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ArduinoTurtleBridge>());
    rclcpp::shutdown();
    return 0;
}