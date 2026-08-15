# motor_bridge

Projeto de aprendizado: ponte ROS2 ↔ Arduino para controle de motor DC via serial USB.

**Stack**: ROS2 Jazzy · C++ (rclcpp) · Arduino Uno · Driver HW-130 · Ubuntu 24.04

---

## Arquitetura

```
[ROS2 node C++]  --serial USB-->  [Arduino Uno]  --pinos-->  [HW-130]  -->  [Motor DC]
  /motor_cmd                        .ino firmware              ponte H
  Int32 (-100..100)                 parse + PWM
```

**Protocolo serial**: texto simples + `\n`, ex: `"75\n"`, `"-50\n"`, `"0\n"`  
Isso permite debugar com o Serial Monitor da Arduino IDE em paralelo.

---

## Estrutura do pacote

```
motor_bridge/
├── arduino/
│   └── motor_bridge_arduino.ino   ← grave no Arduino com a Arduino IDE
├── launch/
│   └── motor_bridge.launch.py
├── src/
│   └── motor_bridge_node.cpp
├── CMakeLists.txt
├── package.xml
└── README.md
```

---

## Pré-requisitos

### 1. Permissão de porta serial

Por padrão no Ubuntu, a porta serial exige `sudo`. Para usar sem `sudo`:

```bash
sudo usermod -aG dialout $USER
```

**Faça logout e login** após este comando (ou reinicie) para o grupo ser aplicado.

Verifique se funcionou:
```bash
groups | grep dialout
```

### 2. Descobrir a porta do Arduino

Antes de conectar o Arduino:
```bash
ls /dev/tty{USB,ACM}*
```

Conecte o Arduino e rode de novo — a nova entrada é a porta do seu Arduino.  
Arduino Uno no Ubuntu normalmente aparece como `/dev/ttyACM0`.

---

## Como gravar o firmware no Arduino

1. Abra a **Arduino IDE**
2. Abra o arquivo `arduino/motor_bridge_arduino.ino`
3. Selecione **Tools → Board → Arduino Uno**
4. Selecione **Tools → Port → /dev/ttyACM0** (ou a porta que você descobriu)
5. Clique em **Upload** (→)

Após o upload, abra o **Serial Monitor** (baud: 9600) e você deve ver:
```
motor_bridge_arduino pronto.
```

---

## Build do pacote ROS2

```bash
# Na raiz do workspace
cd ~/Desktop/ros2/my_workspace

# Build apenas este pacote (mais rápido durante desenvolvimento)
colcon build --packages-select motor_bridge

# Carrega os novos executáveis no ambiente
source install/setup.bash
```

---

## Como rodar

### Opção A — `ros2 run` (direto)

```bash
# Com porta padrão (/dev/ttyACM0, 9600 bps)
ros2 run motor_bridge motor_bridge_node

# Especificando porta e baud rate
ros2 run motor_bridge motor_bridge_node --ros-args \
  -p serial_port:=/dev/ttyUSB0 \
  -p baud_rate:=9600
```

### Opção B — `ros2 launch` (recomendado)

```bash
# Com valores padrão
ros2 launch motor_bridge motor_bridge.launch.py

# Sobrescrevendo a porta
ros2 launch motor_bridge motor_bridge.launch.py serial_port:=/dev/ttyUSB0
```

---

## Como testar

### Teste básico — sem Arduino físico

Rode o node em um terminal:
```bash
ros2 run motor_bridge motor_bridge_node
```

Em outro terminal, publique um comando:
```bash
ros2 topic pub --once /motor_cmd std_msgs/msg/Int32 "{data: 75}"
```

O node vai logar um erro de porta serial (esperado sem Arduino), mas vai processar a mensagem sem crashar.

### Teste com Arduino conectado

```bash
# Terminal 1: node ROS2
ros2 run motor_bridge motor_bridge_node

# Terminal 2: publica comandos
ros2 topic pub --once /motor_cmd std_msgs/msg/Int32 "{data: 90}"   # frente, 90%
ros2 topic pub --once /motor_cmd std_msgs/msg/Int32 "{data: -50}"  # reverso, 50%
ros2 topic pub --once /motor_cmd std_msgs/msg/Int32 "{data: 0}"    # parado
```

### Monitorar o tópico

```bash
# Ver mensagens publicadas em tempo real
ros2 topic echo /motor_cmd

# Ver informações do tópico
ros2 topic info /motor_cmd
```

### Debug com Serial Monitor da Arduino IDE

Você pode abrir o Serial Monitor enquanto o node ROS2 está rodando para ver os comandos chegando. **Atenção**: apenas um processo pode ter a porta serial aberta por vez. Para debugar com o Serial Monitor:

1. Pare o node ROS2 (`Ctrl+C`)
2. Abra o Serial Monitor (9600 baud)
3. Digite manualmente: `75` + Enter → motor deve girar
4. Feche o Serial Monitor antes de rodar o node novamente

---

## Ajuste de pinagem

Se o motor girar no sentido errado, troque `IN1` e `IN2` no sketch:

```cpp
// Em motor_bridge_arduino.ino, troque:
const int PIN_IN1 = 7;
const int PIN_IN2 = 8;
// por:
const int PIN_IN1 = 8;
const int PIN_IN2 = 7;
```

Ou simplesmente inverta o sinal no tópico (publique `-90` em vez de `90`).

---

## Failsafe

O firmware para o motor automaticamente se **nenhum comando chegar em 2 segundos**. Isso protege contra:
- Cabo USB desconectado acidentalmente
- Node ROS2 travado ou morto
- Perda de comunicação serial

Para ajustar o timeout, altere `FAILSAFE_MS` no sketch `.ino`.

---

## Próximos passos (contexto)

1. Adicionar segundo motor (canal B do HW-130)
2. Migrar para `ros2_control` com hardware interface customizado
3. Escalar para robô quadrúpede com múltiplos servos (ESP32)
