/**
 * esp32_microros_hello.ino
 *
 * "Hello World" com micro-ROS no ESP32 via Wi-Fi.
 *
 * O ESP32 se conecta na rede Wi-Fi e publica mensagens
 * no tópico /esp32_hello como um node ROS 2 de verdade.
 *
 * DEPENDÊNCIAS:
 *   - Biblioteca: micro_ros_arduino (instale pelo Gerenciador de Bibliotecas)
 *   - Placa: ESP32 Dev Module (selecione em Tools > Board)
 *
 * COMO USAR:
 *   1. Preencha WIFI_SSID, WIFI_PASS e AGENT_IP abaixo.
 *   2. Faça o Upload pelo cabo USB-C.
 *   3. No Ubuntu: rode o Agent e o echo (instruções no final do arquivo).
 */

#include <Arduino.h>
#include <WiFi.h>

#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <std_msgs/msg/string.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════
//  ⚙️  CONFIGURAÇÕES — PREENCHA AQUI!
// ═══════════════════════════════════════════════════════════════════════════
#define WIFI_SSID "Gustavo"     // ← Troque pelo nome da sua rede
#define WIFI_PASS "29101998"    // ← Troque pela sua senha
#define AGENT_IP "192.168.18.9" // ← IP do seu notebook Ubuntu
#define AGENT_PORT 8888         // Porta padrão do micro-ROS Agent

// ═══════════════════════════════════════════════════════════════════════════

rcl_publisher_t publisher;
std_msgs__msg__String msg;
rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;

int contador = 0;

// Macro de verificação de erro — pisca o LED embutido 3x se algo falhar
#define RCCHECK(fn)                                                            \
  {                                                                            \
    rcl_ret_t temp_rc = fn;                                                    \
    if ((temp_rc != RCL_RET_OK)) {                                             \
      error_loop();                                                            \
    }                                                                          \
  }
#define RCSOFTCHECK(fn)                                                        \
  {                                                                            \
    rcl_ret_t temp_rc = fn;                                                    \
    if ((temp_rc != RCL_RET_OK)) {                                             \
      ;                                                                        \
    }                                                                          \
  }

void error_loop() {
  // Se algo no micro-ROS falhar, pisca o LED infinitamente
  while (1) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
  }
}

// ── Callback do Timer ──────────────────────────────────────────────────────
// É chamado a cada 1 segundo para publicar uma mensagem
void timer_callback(rcl_timer_t *timer, int64_t last_call_time) {
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    contador++;
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "Ola do ESP32! Contagem: %d", contador);
    msg.data.data = buffer;
    msg.data.size = strlen(buffer);
    RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));

    // Pisca o LED a cada publicação (feedback visual)
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    Serial.printf("[%d] Publicando: %s\n", contador, buffer);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // ── Conecta no Wi-Fi ────────────────────────────────────────────────────
  Serial.printf("\nConectando ao Wi-Fi: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWi-Fi conectado! IP do ESP32: %s\n",
                WiFi.localIP().toString().c_str());
  Serial.printf("Conectando ao Agent em %s:%d...\n", AGENT_IP, AGENT_PORT);

  // ── Configura transporte micro-ROS via Wi-Fi (UDP) ──────────────────────
  set_microros_wifi_transports(WIFI_SSID, WIFI_PASS, AGENT_IP, AGENT_PORT);

  delay(2000); // Aguarda o Agent ficar pronto

  // ── Inicializa micro-ROS ─────────────────────────────────────────────────
  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

  // Cria o node "esp32_hello_node" no namespace vazio
  RCCHECK(rclc_node_init_default(&node, "esp32_hello_node", "", &support));

  // Cria o publisher no tópico /esp32_hello com mensagem tipo String
  RCCHECK(rclc_publisher_init_default(
      &publisher, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
      "/esp32_hello"));

  // Cria um timer que dispara a cada 1000ms (1 segundo)
  RCCHECK(rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(1000),
                                  timer_callback));

  // Cria o executor e adiciona o timer
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));

  // Inicializa a string da mensagem
  char string_memory[100];
  msg.data.data = string_memory;
  msg.data.capacity = sizeof(string_memory);
  msg.data.size = 0;

  Serial.println("micro-ROS inicializado! Publicando em /esp32_hello...");
  digitalWrite(LED_BUILTIN, HIGH); // LED aceso = tudo OK
}

// ═══════════════════════════════════════════════════════════════════════════
void loop() {
  // rclc_executor_spin_some processa callbacks pendentes (timers,
  // subscriptions) O delay de 100ms define o tempo de resposta máximo
  RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)));
  delay(10);
}

/*
 * ══════════════════════════════════════════════════════════════════════════
 * COMO TESTAR NO UBUNTU:
 *
 * Terminal 1 — Rode o Agent (o "servidor" que escuta o ESP32):
 *   cd ~/microros_ws
 *   source install/local_setup.bash
 *   ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888
 *
 * Terminal 2 — Leia as mensagens publicadas pelo ESP32:
 *   source /opt/ros/jazzy/setup.bash
 *   ros2 topic echo /esp32_hello
 *
 * Se tudo funcionar, você verá no Terminal 2:
 *   data: 'Ola do ESP32! Contagem: 1'
 *   ---
 *   data: 'Ola do ESP32! Contagem: 2'
 *   ---
 *   ...
 * ══════════════════════════════════════════════════════════════════════════
 */
