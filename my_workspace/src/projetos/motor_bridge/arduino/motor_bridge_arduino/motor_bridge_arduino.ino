// ═══════════════════════════════════════════════════════════════════════════
// motor_bridge_arduino.ino (ATUALIZADO PARA O ROBÔ HÍBRIDO ESP32)
// Agora usamos a biblioteca SoftwareSerial no Pino 10 para não dar 
// conflito com a porta USB e contornar os furos ruins do Shield!
// ═══════════════════════════════════════════════════════════════════════════

#include <AFMotor_R4.h>
#include <SoftwareSerial.h>

// ── Motores conectados nos terminais M3 e M4 do shield ────────
AF_DCMotor motor_left(3);
AF_DCMotor motor_right(4);

// ── Nova Porta Serial (Ouvindo o ESP32) ───────────────────────
// Usaremos o Pino 10 como RX (onde vamos espetar o fio roxo do ESP32)
// e o Pino 11 como TX (não vamos usar, mas é obrigatório declarar)
SoftwareSerial esp32Serial(10, 11);

// ── Timeout de Segurança (Failsafe) ───────────────────────────
const unsigned long FAILSAFE_MS = 2000;
unsigned long last_cmd_time = 0;
String serial_buffer = "";

void setup()
{
  Serial.begin(9600);      // Serial original (USB) só para debug
  esp32Serial.begin(9600); // Serial Nova (ESP32) recebendo comandos

  motor_left.run(RELEASE);
  motor_right.run(RELEASE);
  
  Serial.println("Arduino pronto! Aguardando comandos do ESP32 no pino 10...");
}

void loop()
{
  // 1. LER DADOS DO ESP32
  while (esp32Serial.available() > 0)
  {
    char c = esp32Serial.read();
    
    if (c == '\n') 
    {
      processarComando(serial_buffer);
      serial_buffer = ""; 
    }
    else 
    {
      serial_buffer += c;
    }
  }

  // 2. FAILSAFE (Se o teclado parar, pare o robô)
  if (millis() - last_cmd_time > FAILSAFE_MS)
  {
    motor_left.run(RELEASE);
    motor_right.run(RELEASE);
  }
}

// ── Função de Processamento do Comando ────────────────────────
void processarComando(String comando)
{
  comando.trim();
  if (comando.length() == 0) return;

  // DEBUG: Imprime no Serial Monitor do PC o que chegou do fio roxo!
  Serial.println("RECEBI DO ESP32: " + comando);

  // Procura a vírgula: "75,75"
  int comma_index = comando.indexOf(',');
  if (comma_index == -1) return;

  String str_esq = comando.substring(0, comma_index);
  String str_dir = comando.substring(comma_index + 1);

  int vel_esq = str_esq.toInt();
  int vel_dir = str_dir.toInt();

  // Motor Esquerdo (M3)
  if (vel_esq > 0) {
    motor_left.setSpeed(vel_esq);
    motor_left.run(FORWARD);
  } else if (vel_esq < 0) {
    motor_left.setSpeed(-vel_esq);
    motor_left.run(BACKWARD);
  } else {
    motor_left.run(RELEASE);
  }

  // Motor Direito (M4)
  if (vel_dir > 0) {
    motor_right.setSpeed(vel_dir);
    motor_right.run(FORWARD);
  } else if (vel_dir < 0) {
    motor_right.setSpeed(-vel_dir);
    motor_right.run(BACKWARD);
  } else {
    motor_right.run(RELEASE);
  }

  last_cmd_time = millis();
}
