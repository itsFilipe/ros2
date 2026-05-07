#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "turtlesim/msg/pose.hpp"
#include <chrono>
#include <memory>
#include <sys/ioctl.h> 

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
        memset(&tty, 0, sizeof(tty)); // Zera a struct antes de tudo

        if (tcgetattr(file_desc_, &tty) != 0) { // 1º: LÊ as configs atuais
            RCLCPP_ERROR(this->get_logger(), "Erro ao ler as configs da porta serial!");
        }

        // 2º: MODIFICA o que precisa
        cfsetospeed(&tty, B9600);
        cfsetispeed(&tty, B9600);

        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        tty.c_cflag |= CREAD | CLOCAL;
        tty.c_cflag &= ~HUPCL;
        tty.c_lflag |= ICANON;
        tty.c_lflag &= ~(ECHO | ECHOE | ECHONL | ISIG);
        tty.c_iflag |= IGNCR; 

        // 3º: APLICA as configs corretas
        tcsetattr(file_desc_, TCSANOW, &tty);

        // 4º: Limpa o buffer DEPOIS de configurar
        tcflush(file_desc_, TCIOFLUSH);

        int flags;
        ioctl(file_desc_, TIOCMGET, &flags);
        flags &= ~TIOCM_DTR;
        ioctl(file_desc_, TIOCMSET, &flags);

        // Espera o Arduino terminar de bootar (caso já tenha resetado)
        RCLCPP_INFO(this->get_logger(), "Aguardando Arduino inicializar...");
        sleep(2);
        tcflush(file_desc_, TCIOFLUSH); // Limpa lixo do boot DEPOIS de esperar
            
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
        char buffer[64]; // 64 bytes: suficiente para um número inteiro como string + '\0'
        int bytes_lidos = read(file_desc_, buffer, sizeof(buffer));
        if (bytes_lidos <= 0) return;

        buffer[bytes_lidos] = '\0';
        int luz = std::atoi(buffer);
        RCLCPP_INFO(this->get_logger(), "VALOR PURO -> Número: %d | Texto: %s", luz, buffer);

        auto message = geometry_msgs::msg::Twist();

        if (luz > 100) { // Claro!

            // GATILHO: só ativa a fuga se NÃO estiver já fugindo
            // Isso evita o re-trigger enquanto a manobra ainda está em curso!
            if (tempo_de_fuga_ == 0) {
                if (pos_x > 10.0 || pos_x < 1.0 || pos_y > 10.0 || pos_y < 1.0) {
                    tempo_de_fuga_ = 20; // 20 ciclos de 100ms = 2 segundos de manobra
                    RCLCPP_WARN(this->get_logger(), "Borda detectada! Iniciando fuga.");
                }
            }

            if (tempo_de_fuga_ > 0) {
                // FASE 1 (ciclos 20→11): Recua para ganhar espaço
                if (tempo_de_fuga_ > 10) {
                    message.linear.x  = -1.0; // Ré!
                    message.angular.z =  0.0;
                }
                // FASE 2 (ciclos 10→1): Gira no lugar (sem andar pra frente)
                else {
                    message.linear.x  =  0.0;
                    message.angular.z =  2.5; // Gira no próprio eixo, sem avançar em círculo
                }

                write(file_desc_, "R", 1); // Sirene ligada
                tempo_de_fuga_--;

            } else {
                // PASSEIO TRANQUILO
                message.linear.x  = 1.5;
                message.angular.z = 0.0;
                write(file_desc_, "V", 1);
            }

        } else { // Escuro!
            message.linear.x  = 0.0;
            message.angular.z = 0.0;
            RCLCPP_INFO(this->get_logger(), "Está escuro, pare de andar!");
        }

        publisher_->publish(message);
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