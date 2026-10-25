# 🤖 robo_2wd_control — Robô Móvel 2WD (Real + Simulação Gazebo)

Pacote completo para controle de um **Robô Móvel de Tração Diferencial (2WD)**:
1. **Robô Físico Sem Fio:** Controlado via Wi-Fi usando **micro-ROS no ESP32** (cérebro) + **Arduino Uno com Motor Shield HW-130** (músculo) + 2 motores DC com redução (chassi `ZK-2WD`) e 2 baterias Li-ion `3.7V` (18650).
2. **Gêmeo Digital (Simulação Gazebo):** Modelo **URDF** construído do zero com física `DiffDrive` no **Gazebo Harmonic**, escutando o mesmo tópico `/cmd_vel` do robô real!
3. **Controle `WASD` Estilo Videogame:** Nó C++ customizado (`wasd_teleop_node`) com parada automática ao soltar a tecla e diagnóstico individual de rodas.

**Stack:** ROS 2 Jazzy · C++ (`rclcpp`) · micro-ROS (ESP32 Wi-Fi UDP) · Arduino (`AFMotor_R4`) · URDF & Gazebo Harmonic · Ubuntu 24.04

---

## 🏗️ Arquitetura do Sistema (Real vs. Simulação)

```text
                       ┌───► [Gazebo Harmonic (Simulação 3D)]
                       │       └── robo_2wd.urdf (Plugin DiffDrive)
                       │
[Teclado WASD no PC] ──┼───► Tópico ROS 2: /cmd_vel (geometry_msgs/msg/Twist)
 (wasd_teleop_node)    │
                       └───► [micro_ros_agent (UDP :8888)] ───(( Wi-Fi ))───► [ESP32 (esp32_robot_brain.ino)]
                                                                                     │
                                                                          Serial2 TX2 (GPIO 17)
                                                                          "pwm_esq,pwm_dir\n"
                                                                                     │
                                                                                     ▼
                                                                       [Arduino Uno + Shield HW-130]
                                                                        ├── Pino 10 (RX SoftwareSerial)
                                                                        ├── Borne M3 ──► Motor Esquerdo
                                                                        └── Borne M4 ──► Motor Direito
```

---

## 📂 Estrutura do Pacote

```text
robo_2wd_control/
├── arduino/
│   ├── esp32_robot_brain/
│   │   └── esp32_robot_brain.ino       ← Firmware do ESP32 (Wi-Fi + micro-ROS + Cinemática + PWM 0..255)
│   ├── motor_bridge_arduino/
│   │   └── motor_bridge_arduino.ino    ← Firmware do Arduino Uno + Shield HW-130 (M3 e M4 + Failsafe)
│   └── esp32_microros_hello/
│       └── esp32_microros_hello.ino    ← Teste básico de conexão micro-ROS
├── launch/
│   ├── sim_gazebo.launch.py            ← Abre o Gazebo + Spawna o robô URDF + Ponte /cmd_vel
│   └── motor_bridge.launch.py          ← Launch para conexão direta via cabo USB (modo legado)
├── src/
│   ├── wasd_teleop_node.cpp            ← Controle pelo teclado estilo videogame (W, A, S, D, Q, E, 1, 2)
│   ├── motor_demo_node.cpp             ← Demonstração autônoma (aceleração senoidal frente/ré)
│   └── motor_bridge_node.cpp           ← Ponte serial USB direta (modo legado)
├── urdf/
│   └── robo_2wd.urdf                   ← Modelo 3D e física do carrinho 2WD construído do zero
├── CMakeLists.txt
├── package.xml
└── README.md
```

---

## 🔌 Ligações Físicas do Robô Real (Hardware)

### 1. ESP32 $\leftrightarrow$ Motor Shield HW-130 (Conector `SER1` no canto superior esquerdo)
| Fio | Pino no ESP32 | Pino no Shield HW-130 | Função |
| :--- | :--- | :--- | :--- |
| 🔴 **Vermelho** | `VIN` (5V) | Pino **`+`** do `SER1` (ou barra `5V`) | Alimentação 5V para o ESP32 |
| ⚫ **Preto** | `GND` | Pino **`-`** do `SER1` (ou barra `GND`) | Terra comum (Obrigatório) |
| ⚪ **Branco** | `TX2` (`GPIO 17`) | Pino **`S`** do `SER1` (**Pino 10** do Arduino) | Envio de comandos Serial (`9600` baud) |

### 2. Motores e Bateria no Shield HW-130
* **Motor Esquerdo:** Ligado nos 2 parafusos do borne **`M3`**.
* **Motor Direito:** Ligado nos 2 parafusos do borne **`M4`**.
* **Bateria (2x Li-ion `3.7V` 18650 em série = `7.4V` a `8.4V`):**
  * Fio Vermelho (`+`) $\rightarrow$ Borne **`+M`** do conector `EXT_PWR`.
  * Fio Preto (`-`) $\rightarrow$ Borne **`GND`** do conector `EXT_PWR`.
  * **Jumper Amarelo (`PWR`) fechado:** Alimenta os motores, o Arduino e o ESP32 simultaneamente.

---

## 🛠️ Como Compilar (Build)

```bash
cd ~/Desktop/ros2/my_workspace
source /opt/ros/jazzy/setup.bash
colcon build --packages-select robo_2wd_control
source install/local_setup.bash
```

---

## 🎮 Como Rodar o Controle `WASD` (`wasd_teleop_node`)

Funciona tanto para o **Robô Real** quanto para a **Simulação no Gazebo** (ou os dois juntos ao mesmo tempo!):

```bash
source /opt/ros/jazzy/setup.bash
source ~/Desktop/ros2/my_workspace/install/local_setup.bash
ros2 run robo_2wd_control wasd_teleop_node
```

### Mapa de Teclas:
* **`W`** : Frente (força nas 2 rodas)
* **`S`** : Ré (força nas 2 rodas)
* **`A`** : Gira para a Esquerda no próprio eixo
* **`D`** : Gira para a Direita no próprio eixo
* **`Q`** : Curva suave para a Esquerda (apenas roda direita gira)
* **`E`** : Curva suave para a Direita (apenas roda esquerda gira)
* **`1`** : Diagnóstico — Gira **SÓ a Roda Esquerda (`M3`)**
* **`2`** : Diagnóstico — Gira **SÓ a Roda Direita (`M4`)**
* **`+` / `-`** : Aumenta / Diminui a potência (PWM)
* **`Espaço`** (ou soltar a tecla) : Freio imediato!

---

## 🌐 Modo 1: Rodando na Simulação 3D (Gazebo)

**Terminal 1 — Abrir o Gazebo com o carrinho `robo_2wd`:**
```bash
source /opt/ros/jazzy/setup.bash
source ~/Desktop/ros2/my_workspace/install/local_setup.bash
ros2 launch robo_2wd_control sim_gazebo.launch.py
```
*(Dica no Gazebo: Na lista `Entities` à direita, clique com o botão direito em **`robo_2wd` $\rightarrow$ `Follow`** ou **`Move To`** para focar a câmera de perto no carrinho!)*

**Terminal 2 — Dirigir com `WASD`:**
```bash
source /opt/ros/jazzy/setup.bash
source ~/Desktop/ros2/my_workspace/install/local_setup.bash
ros2 run robo_2wd_control wasd_teleop_node
```

---

## 📡 Modo 2: Rodando no Robô Real Sem Fio (micro-ROS + ESP32)

**Terminal 1 — Iniciar o Agente micro-ROS (Wi-Fi UDP):**
```bash
source /opt/ros/jazzy/setup.bash
source ~/Desktop/ros2/microros_ws/install/local_setup.bash
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888
```
*(Ligue o carrinho ou aperte o botão `EN` no ESP32. O LED azul do ESP32 ficará aceso fixo e o terminal mostrará `session established`).*

**Terminal 2 — Dirigir com `WASD`:**
```bash
source /opt/ros/jazzy/setup.bash
source ~/Desktop/ros2/my_workspace/install/local_setup.bash
ros2 run robo_2wd_control wasd_teleop_node
```
