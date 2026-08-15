/**
 * motor_bridge_arduino.ino (versão para L293D Motor Shield HW-130)
 *
 * IMPORTANTE: Este sketch usa a biblioteca AFMotor (Adafruit Motor Shield V1)
 * porque o HW-130 L293D Shield não expõe pinos ENA/IN1/IN2 diretamente.
 * Em vez disso, ele usa o chip SN74HC595 (registrador de deslocamento) para
 * controlar a direção, o que exige comunicação serial entre chips — a
 * biblioteca AFMotor abstrai toda essa complexidade.
 *
 * POR QUE USAR A BIBLIOTECA AQUI:
 *   O projeto original pediu termios.h POSIX puro no lado ROS2 (C++) para
 *   aprender a configuração de serial em baixo nível. No Arduino, a restrição
 *   equivalente seria controlar o 74HC595 manualmente — mas isso adicionaria
 *   ~50 linhas de bit-banging que não ensinam nada relevante sobre ROS2.
 *   A biblioteca faz sentido aqui: é o nível certo de abstração.
 *
 * INSTALAÇÃO DA BIBLIOTECA:
 *   Arduino IDE → Tools → Manage Libraries → buscar "AFMotor" →
 *   instalar "Adafruit Motor Shield library" (v1.x, NÃO a v2)
 *
 * PROTOCOLO SERIAL (igual ao antes — o node ROS2 não muda):
 *   Texto simples + '\n': ex "75\n", "-50\n", "0\n"
 *   Valores: -100 a 100 (negativo = reverso, positivo = frente, 0 = parado)
 *
 * CONEXÃO FÍSICA:
 *   - Shield encaixa direto no Arduino Uno (sem fios de controle)
 *   - Motor DC → terminais M1 do shield (parafusos azuis, canto superior)
 *   - Fonte externa → borne EXT_PWR do shield (parafusos azuis, canto esquerdo)
 *     * + da fonte → terminal + do EXT_PWR
 *     * - da fonte → terminal - (GND) do EXT_PWR
 *   - Arduino → notebook via USB (para serial + alimentação lógica)
 *
 * FONTE EXTERNA:
 *   Use 4x pilhas AA (6V) ou fonte 5-12V.
 *   O motor NÃO pode ser alimentado pelo USB do Arduino.
 */

#include <AFMotor_R4.h>

// ── Motor conectado no canal M1 do shield ─────────────────────────────────
// Se preferir usar o canal M2, M3 ou M4, troque o número aqui.
// Canais M1 e M2 ficam nos terminais do canto superior do shield.
AF_DCMotor motor(1);

// ── Failsafe ───────────────────────────────────────────────────────────────
// Se não chegar nenhum comando em 2000 ms (2 segundos), para o motor.
const unsigned long FAILSAFE_MS = 2000;

unsigned long last_cmd_time = 0;
String serial_buffer = "";

// ═══════════════════════════════════════════════════════════════════════════
void setup()
{
  // Para o motor no estado inicial (segurança)
  motor.run(RELEASE);
  motor.setSpeed(0);

  Serial.begin(9600);
  while (!Serial) { ; }

  Serial.println("motor_bridge_arduino (L293D shield) pronto.");
  last_cmd_time = millis();
}

// ═══════════════════════════════════════════════════════════════════════════
void loop()
{
  // ── Leitura serial ────────────────────────────────────────────────────────
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n') {
      process_command(serial_buffer);
      serial_buffer = "";
    } else if (c != '\r') {
      serial_buffer += c;
      if (serial_buffer.length() > 10) {
        serial_buffer = ""; // proteção contra lixo serial
      }
    }
  }

  // ── Failsafe ───────────────────────────────────────────────────────────────
  if (millis() - last_cmd_time > FAILSAFE_MS) {
    motor.run(RELEASE);
    motor.setSpeed(0);
    // Não reseta last_cmd_time — só age quando novo comando chegar
  }
}

// ═══════════════════════════════════════════════════════════════════════════
void process_command(const String & cmd)
{
  if (cmd.length() == 0) return;

  int value = cmd.toInt();
  value = constrain(value, -100, 100);

  set_motor(value);
  last_cmd_time = millis();
}

// ═══════════════════════════════════════════════════════════════════════════
/**
 * Controla o motor com base no valor -100 a 100.
 *
 * AFMotor API:
 *   motor.setSpeed(0..255)  — velocidade (duty cycle PWM)
 *   motor.run(FORWARD)      — gira para frente
 *   motor.run(BACKWARD)     — gira ao contrário
 *   motor.run(RELEASE)      — para (sem freio ativo)
 *
 * map(value, 0, 100, 0, 255) escala linearmente:
 *   0%   → 0   (sem tensão)
 *   50%  → 127
 *   100% → 255 (tensão máxima)
 */
void set_motor(int value)
{
  if (value == 0) {
    motor.run(RELEASE);
    motor.setSpeed(0);
    return;
  }

  // Calcula PWM (0–255) a partir do valor absoluto (0–100)
  int pwm = map(abs(value), 0, 100, 0, 255);
  motor.setSpeed(pwm);

  if (value > 0) {
    motor.run(FORWARD);
  } else {
    motor.run(BACKWARD);
  }
}
