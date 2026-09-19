/**
 * esp32_robot_brain.ino
 *
 * Cérebro do robô móvel usando micro-ROS.
 * Ele recebe as velocidades pelo tópico /cmd_vel (via Wi-Fi),
 * faz a conta da cinemática diferencial e envia pela porta 
 * Serial2 (Pino TX2 = GPIO 17) para o Arduino.
 *
 * O Arduino (que já tem o motor_bridge_arduino.ino gravado) 
 * recebe esse texto e comanda a ponte H.
 */

#include <Arduino.h>
#include <WiFi.h>

#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <geometry_msgs/msg/twist.h>
#include <stdio.h>

// ═══════════════════════════════════════════════════════════════════════════
//  ⚙️  CONFIGURAÇÕES DO WI-FI E ROS 2
// ═══════════════════════════════════════════════════════════════════════════
#define WIFI_SSID "Gustavo"             // Seu Wi-Fi
#define WIFI_PASS "29101998"            // Sua Senha
#define AGENT_IP  "192.168.18.9"        // IP do Ubuntu
#define AGENT_PORT 8888                 // Porta padrão

// Pino TX2 (GPIO 17) no ESP32 DOIT DevKit V1
#define TX2_PIN 17
#define RX2_PIN 16 // Não usaremos para receber, mas é obrigatório declarar

// ═══════════════════════════════════════════════════════════════════════════

rcl_subscription_t subscriber;
geometry_msgs__msg__Twist msg;
rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){;}}

void error_loop() {
  while (1) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
  }
}

// ── Callback do Tópico /cmd_vel ───────────────────────────────────────────
// Esta função roda sempre que você pressiona uma tecla no teclado do Ubuntu
void cmd_vel_callback(const void * msgin)
{
  // Converte o ponteiro genérico para a mensagem Twist
  const geometry_msgs__msg__Twist * twist_msg = (const geometry_msgs__msg__Twist *)msgin;

  // 1. Cinemática Diferencial (Mesma conta que fazíamos no PC)
  const float speed_scale = 100.0f;
  
  // v_esq = linear.x - angular.z
  // v_dir = linear.x + angular.z
  int left  = (int)((twist_msg->linear.x - twist_msg->angular.z) * speed_scale);
  int right = (int)((twist_msg->linear.x + twist_msg->angular.z) * speed_scale);

  // 2. Limita para não passar de 100%
  left  = constrain(left, -100, 100);
  right = constrain(right, -100, 100);

  // 3. Envia para o Arduino via Serial2!
  // O Arduino espera o formato: "75,75\n"
  Serial2.printf("%d,%d\n", left, right);
  
  // Feedback visual e log pro computador (Serial 1)
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  Serial.printf("Recebido! Enviando p/ Arduino: %d,%d\n", left, right);
}

// ═══════════════════════════════════════════════════════════════════════════
void setup()
{
  // Serial Padrão (Cabo USB - apenas para debug e log no Serial Monitor)
  Serial.begin(115200);
  
  // Serial2 (Para falar com o Arduino)
  // O Arduino roda a 9600 baud.
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // ── Conecta no Wi-Fi ────────────────────────────────────────────────────
  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWi-Fi conectado! IP: %s\n", WiFi.localIP().toString().c_str());

  // ── Configura micro-ROS ─────────────────────────────────────────────────
  set_microros_wifi_transports(WIFI_SSID, WIFI_PASS, AGENT_IP, AGENT_PORT);
  delay(2000);

  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "esp32_robot_brain", "", &support));

  // Cria o SUBSCRIBER no tópico /cmd_vel
  RCCHECK(rclc_subscription_init_default(
    &subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
    "/cmd_vel"
  ));

  // Cria o executor (1 handle = 1 subscriber)
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &msg, &cmd_vel_callback, ON_NEW_DATA));

  Serial.println("Cérebro pronto! Escutando /cmd_vel via Wi-Fi...");
  digitalWrite(LED_BUILTIN, HIGH); 
}

// ═══════════════════════════════════════════════════════════════════════════
void loop()
{
  RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)));
  delay(10);
}
