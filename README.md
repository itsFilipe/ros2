# ROS2 — Referência Rápida em C++

Este repositório contém exemplos e projetos desenvolvidos durante o aprendizado de ROS2 (Humble).
Todo o código é escrito em C++ (exceto `velocity_monitor`, que é Python).

---

## Estrutura do Repositório

```
ros2/
├── README.md                    ← este arquivo
├── my_workspace/
│   ├── template.cpp             ← esqueleto de nó para copiar
│   └── src/
│       ├── exemplos/            ← exercícios e código introdutório
│       │   ├── pkg_curso/       publisher, subscriber, service server/client
│       │   ├── contar_numeros/  pub+sub contando inteiros
│       │   └── lidar_node/      publisher de sensor lidar simulado
│       ├── projetos/            ← aplicações mais completas
│       │   ├── battery_led/     bateria + painel LED via service
│       │   ├── my_first_robot/  walker autônomo com lidar
│       │   ├── turtlesim_sensor/ bridge Arduino → turtlesim
│       │   └── velocity_monitor/ monitor de velocidade (Python)
│       └── interfaces/          ← mensagens e serviços customizados
│           └── meu_projeto_interfaces/
└── turtlebot3_ws/               ← simulações TurtleBot3
```

---

## Conceitos Fundamentais

| Mecanismo | Para quê | Analogia |
|---|---|---|
| **Topic** | Transmissão contínua de dados (1:N) | Rádio — qualquer um sintoniza |
| **Service** | Chamada síncrona request/response (1:1) | Função remota |
| **Action** | Tarefa longa com feedback e cancelamento | Tarefa com barra de progresso |
| **Launch file** | Iniciar vários nós de uma vez | Script de startup |

---

## Boilerplate — Todo Nó C++ Começa Assim

```cpp
#include "rclcpp/rclcpp.hpp"

class MeuNo : public rclcpp::Node {
public:
    MeuNo() : Node("meu_no") {
        // crie publishers, subscribers, timers, services aqui
    }
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<MeuNo>());
    rclcpp::shutdown();
    return 0;
}
```

---

## Publisher

Publica uma mensagem `Float32` a cada 1 segundo.

```cpp
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32.hpp"
#include <chrono>
using namespace std::chrono_literals;

class MinimalPublisher : public rclcpp::Node {
public:
    MinimalPublisher() : Node("minimal_publisher") {
        publisher_ = this->create_publisher<std_msgs::msg::Float32>("meu_topico", 10);
        timer_ = this->create_wall_timer(
            1s, std::bind(&MinimalPublisher::publicar, this));
    }

private:
    void publicar() {
        auto msg = std_msgs::msg::Float32();
        msg.data = 42.0f;
        publisher_->publish(msg);
        RCLCPP_INFO(this->get_logger(), "Publicado: %.1f", msg.data);
    }

    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
};
```

> Ver código completo: [`exemplos/pkg_curso/src/publisher_cpp.cpp`](my_workspace/src/exemplos/pkg_curso/src/publisher_cpp.cpp)

---

## Subscriber

Recebe mensagens do mesmo tópico publicado acima.

```cpp
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/float32.hpp"
using std::placeholders::_1;

class MinimalSubscriber : public rclcpp::Node {
public:
    MinimalSubscriber() : Node("minimal_subscriber") {
        subscription_ = this->create_subscription<std_msgs::msg::Float32>(
            "meu_topico", 10,
            std::bind(&MinimalSubscriber::callback, this, _1));
    }

private:
    void callback(const std_msgs::msg::Float32::SharedPtr msg) {
        RCLCPP_INFO(this->get_logger(), "Recebi: %.1f", msg->data);
    }

    rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr subscription_;
};
```

> Ver código completo: [`exemplos/pkg_curso/src/subscriber_cpp.cpp`](my_workspace/src/exemplos/pkg_curso/src/subscriber_cpp.cpp)

---

## Service — Server

Recebe uma requisição e retorna uma resposta. Exemplo: soma dois inteiros.

```cpp
#include <rclcpp/rclcpp.hpp>
#include <example_interfaces/srv/add_two_ints.hpp>
using namespace std::placeholders;

class AddTwoIntsServer : public rclcpp::Node {
public:
    AddTwoIntsServer() : Node("add_two_ints_server") {
        server_ = this->create_service<example_interfaces::srv::AddTwoInts>(
            "add_two_ints",
            std::bind(&AddTwoIntsServer::callback, this, _1, _2));
    }

private:
    void callback(
        const example_interfaces::srv::AddTwoInts::Request::SharedPtr request,
        const example_interfaces::srv::AddTwoInts::Response::SharedPtr response)
    {
        response->sum = request->a + request->b;
        RCLCPP_INFO(this->get_logger(), "%ld + %ld = %ld",
                    request->a, request->b, response->sum);
    }

    rclcpp::Service<example_interfaces::srv::AddTwoInts>::SharedPtr server_;
};
```

> Ver código completo: [`exemplos/pkg_curso/src/add_two_ints_server.cpp`](my_workspace/src/exemplos/pkg_curso/src/add_two_ints_server.cpp)

---

## Service — Client

Chama o service acima de forma assíncrona.

```cpp
#include <rclcpp/rclcpp.hpp>
#include <example_interfaces/srv/add_two_ints.hpp>
using namespace std::chrono_literals;
using namespace std::placeholders;

class AddTwoIntsClient : public rclcpp::Node {
public:
    AddTwoIntsClient() : Node("add_two_ints_client") {
        client_ = this->create_client<example_interfaces::srv::AddTwoInts>("add_two_ints");
    }

    void chamar(int a, int b) {
        while (!client_->wait_for_service(1s)) {
            RCLCPP_WARN(this->get_logger(), "Aguardando o server...");
        }
        auto request = std::make_shared<example_interfaces::srv::AddTwoInts::Request>();
        request->a = a;
        request->b = b;
        client_->async_send_request(
            request, std::bind(&AddTwoIntsClient::resposta, this, _1));
    }

private:
    void resposta(rclcpp::Client<example_interfaces::srv::AddTwoInts>::SharedFuture future) {
        RCLCPP_INFO(this->get_logger(), "Resultado: %ld", future.get()->sum);
    }

    rclcpp::Client<example_interfaces::srv::AddTwoInts>::SharedPtr client_;
};
```

> Ver código completo: [`exemplos/pkg_curso/src/add_two_ints_client.cpp`](my_workspace/src/exemplos/pkg_curso/src/add_two_ints_client.cpp)

---

## Custom Messages e Services

Defina suas próprias mensagens em um pacote separado de interfaces.

### Mensagem (`.msg`)

```
# interfaces/meu_projeto_interfaces/msg/LedStatus.msg
int64[] numeros
```

Use no C++:
```cpp
#include "meu_projeto_interfaces/msg/led_status.hpp"
using LedStatus = meu_projeto_interfaces::msg::LedStatus;

// Publicar:
LedStatus msg;
msg.numeros = {1, 0, 1};
publisher_->publish(msg);
```

### Service (`.srv`)

```
# interfaces/meu_projeto_interfaces/srv/SetLed.srv
int64 led_number
bool state
---
bool success
```

Use no C++:
```cpp
#include "meu_projeto_interfaces/srv/set_led.hpp"
using SetLed = meu_projeto_interfaces::srv::SetLed;

// No server — acessa request e preenche response:
response->success = true;

// No client — cria e envia request:
auto request = std::make_shared<SetLed::Request>();
request->led_number = 1;
request->state = true;
client_->async_send_request(request, callback);
```

> Exemplo completo em uso: [`projetos/battery_led/`](my_workspace/src/projetos/battery_led/)
> Definições: [`interfaces/meu_projeto_interfaces/`](my_workspace/src/interfaces/meu_projeto_interfaces/)

---

## Build & Run

```bash
# 1. Source ROS2 (todo terminal novo)
source /opt/ros/jazzy/setup.bash

# 2. Build (da raiz do workspace)
cd ~/Desktop/ros2/my_workspace
colcon build --packages-select <nome_do_pacote>
source install/setup.bash

# 3. Rodar
ros2 run <pacote> <executavel>
```

---

## CLI — Comandos Úteis

```bash
ros2 node list                              # nós ativos
ros2 topic list                             # tópicos ativos
ros2 topic echo /meu_topico                 # inspecionar mensagens em tempo real
ros2 topic info /meu_topico                 # quem publica/subscreve
ros2 service list                           # services ativos
ros2 service call /add_two_ints \
  example_interfaces/srv/AddTwoInts "{a: 3, b: 5}"  # chamar service pela CLI
ros2 interface show std_msgs/msg/Float32    # ver campos de um tipo de mensagem
```

---

## Cheat Sheet

| O que fazer | C++ |
|---|---|
| Criar nó | `class MyNode : public rclcpp::Node` |
| Publisher | `create_publisher<MsgType>("topico", 10)` |
| Subscriber | `create_subscription<MsgType>("topico", 10, callback)` |
| Timer | `create_wall_timer(500ms, callback)` |
| Service server | `create_service<SrvType>("nome", callback)` |
| Service client | `create_client<SrvType>("nome")` |
| Log info | `RCLCPP_INFO(get_logger(), "msg %d", val)` |
| Log warn | `RCLCPP_WARN(get_logger(), "msg")` |
| Log error | `RCLCPP_ERROR(get_logger(), "msg")` |
| Iniciar ROS2 | `rclcpp::init(argc, argv)` |
| Rodar nó | `rclcpp::spin(std::make_shared<MyNode>())` |
| Encerrar | `rclcpp::shutdown()` |

### Tipos de mensagem mais comuns

| Tipo | Include | Campo |
|---|---|---|
| `std_msgs::msg::String` | `std_msgs/msg/string.hpp` | `.data` (std::string) |
| `std_msgs::msg::Float32` | `std_msgs/msg/float32.hpp` | `.data` (float) |
| `std_msgs::msg::Float64` | `std_msgs/msg/float64.hpp` | `.data` (double) |
| `std_msgs::msg::Int32` | `std_msgs/msg/int32.hpp` | `.data` (int32) |
| `std_msgs::msg::Bool` | `std_msgs/msg/bool.hpp` | `.data` (bool) |
| `geometry_msgs::msg::Twist` | `geometry_msgs/msg/twist.hpp` | `.linear.x`, `.angular.z` |
| `sensor_msgs::msg::LaserScan` | `sensor_msgs/msg/laser_scan.hpp` | `.ranges[]` |
