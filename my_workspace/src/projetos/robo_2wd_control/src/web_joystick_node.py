#!/usr/bin/env python3
"""
web_joystick_node.py

Servidor Web + Nó ROS 2 que cria um Joystick Analógico Touch (estilo jogo mobile)
para controlar o robô pelo celular via Wi-Fi sem atraso e sem entupir a Serial do Arduino!

Basta rodar este nó no Ubuntu e abrir no navegador do celular:
  http://192.168.18.9:8080  (ou o IP do seu PC)
"""

import json
import socket
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist

HTML_PAGE = """<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>🕹️ Controle Robô 2WD</title>
  <style>
    * { box-sizing: border-box; user-select: none; -webkit-user-select: none; -webkit-touch-callout: none; }
    body {
      margin: 0; padding: 16px;
      background: #0f172a; color: #f8fafc;
      font-family: system-ui, -apple-system, sans-serif;
      display: flex; flex-direction: column; align-items: center;
      justify-content: space-between; height: 100dvh; overflow: hidden;
      touch-action: none;
    }
    .header { text-align: center; width: 100%; }
    .header h1 { margin: 4px 0; font-size: 1.3rem; color: #38bdf8; }
    .status-bar {
      display: flex; justify-content: center; gap: 16px;
      background: #1e293b; padding: 8px 16px; border-radius: 12px;
      font-size: 0.9rem; font-family: monospace; margin-top: 6px;
    }
    .joystick-zone {
      position: relative;
      width: 260px; height: 260px;
      border-radius: 50%;
      background: radial-gradient(circle, #1e293b 0%, #0f172a 80%);
      border: 3px solid #334155;
      box-shadow: 0 0 30px rgba(56, 189, 248, 0.15);
      display: flex; align-items: center; justify-content: center;
    }
    .crosshair-h, .crosshair-v {
      position: absolute; background: rgba(148, 163, 184, 0.15); pointer-events: none;
    }
    .crosshair-h { width: 100%; height: 2px; }
    .crosshair-v { width: 2px; height: 100%; }
    .stick {
      position: absolute;
      width: 90px; height: 90px;
      border-radius: 50%;
      background: radial-gradient(circle at 30% 30%, #38bdf8, #0284c7);
      box-shadow: 0 8px 20px rgba(2, 132, 199, 0.5);
      border: 3px solid #bae6fd;
      transform: translate(0px, 0px);
      pointer-events: none;
    }
    .controls {
      width: 100%; max-width: 360px;
      display: flex; flex-direction: column; gap: 10px;
    }
    .btn-row { display: flex; gap: 10px; width: 100%; }
    button {
      flex: 1; padding: 14px; border: none; border-radius: 12px;
      font-size: 1rem; font-weight: 700; color: white;
      background: #334155;
    }
    button:active { filter: brightness(1.3); }
    .btn-stop { background: #ef4444; font-size: 1.1rem; }
    .btn-wheel { background: #0369a1; font-size: 0.9rem; }
  </style>
</head>
<body>
  <div class="header">
    <h1>🤖 Controle Robô 2WD</h1>
    <div class="status-bar">
      <span id="lin">Lin: 0.00</span>
      <span id="ang">Ang: 0.00</span>
      <span id="state">⏹️ PARADO</span>
    </div>
  </div>

  <div class="joystick-zone" id="zone">
    <div class="crosshair-h"></div>
    <div class="crosshair-v"></div>
    <div class="stick" id="stick"></div>
  </div>

  <div class="controls">
    <div class="btn-row">
      <button class="btn-wheel" id="btnLeft">🔧 Só Roda Esq (M3)</button>
      <button class="btn-wheel" id="btnRight">🔧 Só Roda Dir (M4)</button>
    </div>
    <button class="btn-stop" id="btnStop">🛑 PARAR IMEDIATAMENTE</button>
  </div>

  <script>
    const zone = document.getElementById('zone');
    const stick = document.getElementById('stick');
    const linEl = document.getElementById('lin');
    const angEl = document.getElementById('ang');
    const stateEl = document.getElementById('state');

    const maxRadius = 85;
    let active = false;
    let targetLinear = 0.0;
    let targetAngular = 0.0;
    let seq = 0; // Número de sequência para ignorar pacotes Wi-Fi atrasados!
    let inFlight = false;

    function updateUI(lin, ang) {
      linEl.textContent = `Lin: ${lin.toFixed(2)}`;
      angEl.textContent = `Ang: ${ang.toFixed(2)}`;
      stateEl.textContent = (Math.abs(lin) < 0.05 && Math.abs(ang) < 0.05) ? '⏹️ PARADO' : '🟢 ANDANDO';
    }

    function sendPost(lin, ang, forceStop = false) {
      if (inFlight && !forceStop) return; // Evita fila de requisições no celular
      inFlight = true;
      seq++;
      fetch('/cmd', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({linear: lin, angular: ang, seq: seq})
      })
      .catch(() => {})
      .finally(() => { inFlight = false; });
    }

    function computeStick(clientX, clientY) {
      const rect = zone.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;

      let dx = clientX - centerX;
      let dy = clientY - centerY;
      const dist = Math.hypot(dx, dy);

      if (dist > maxRadius) {
        dx = (dx / dist) * maxRadius;
        dy = (dy / dist) * maxRadius;
      }

      stick.style.transition = 'none';
      stick.style.transform = `translate(${dx}px, ${dy}px)`;

      // Zona morta no centro (15%) para não ligar sem querer
      let normLin = -(dy / maxRadius);
      let normAng = -(dx / maxRadius);
      if (Math.abs(normLin) < 0.15) normLin = 0.0;
      if (Math.abs(normAng) < 0.15) normAng = 0.0;

      targetLinear = normLin;
      targetAngular = normAng;
      updateUI(targetLinear, targetAngular);
    }

    function stopNow() {
      active = false;
      targetLinear = 0.0;
      targetAngular = 0.0;
      stick.style.transition = 'transform 0.15s ease-out';
      stick.style.transform = 'translate(0px, 0px)';
      updateUI(0.0, 0.0);

      // Envia parada imediata com prioridade máxima
      sendPost(0.0, 0.0, true);
      setTimeout(() => { if (!active) sendPost(0.0, 0.0, true); }, 80);
    }

    zone.addEventListener('touchstart', (e) => {
      e.preventDefault();
      active = true;
      computeStick(e.touches[0].clientX, e.touches[0].clientY);
      sendPost(targetLinear, targetAngular);
    }, {passive: false});

    zone.addEventListener('touchmove', (e) => {
      e.preventDefault();
      if (!active) return;
      // Apenas atualiza as variáveis; o loop de 120ms envia de forma limpa!
      computeStick(e.touches[0].clientX, e.touches[0].clientY);
    }, {passive: false});

    zone.addEventListener('touchend', (e) => { e.preventDefault(); stopNow(); }, {passive: false});
    zone.addEventListener('touchcancel', (e) => { e.preventDefault(); stopNow(); }, {passive: false});

    // Se o usuário trocar de aba ou bloquear a tela do celular, para o robô na hora!
    window.addEventListener('blur', stopNow);
    document.addEventListener('visibilitychange', () => { if (document.hidden) stopNow(); });

    // Suporte a Mouse (PC)
    zone.addEventListener('mousedown', (e) => {
      active = true;
      computeStick(e.clientX, e.clientY);
      sendPost(targetLinear, targetAngular);
    });
    window.addEventListener('mousemove', (e) => { if (active) computeStick(e.clientX, e.clientY); });
    window.addEventListener('mouseup', () => { if (active) stopNow(); });

    // Botões de diagnóstico individual das rodas
    const bindHoldButton = (id, lin, ang) => {
      const btn = document.getElementById(id);
      const start = (e) => {
        e.preventDefault();
        active = true;
        targetLinear = lin;
        targetAngular = ang;
        updateUI(lin, ang);
        sendPost(lin, ang);
      };
      const end = (e) => { e.preventDefault(); stopNow(); };
      btn.addEventListener('touchstart', start, {passive: false});
      btn.addEventListener('touchend', end, {passive: false});
      btn.addEventListener('touchcancel', end, {passive: false});
      btn.addEventListener('mousedown', start);
      btn.addEventListener('mouseup', end);
    };

    bindHoldButton('btnLeft', 0.5, -0.5);  // Só roda esquerda
    bindHoldButton('btnRight', 0.5, 0.5);  // Só roda direita

    document.getElementById('btnStop').addEventListener('touchstart', (e) => { e.preventDefault(); stopNow(); }, {passive: false});
    document.getElementById('btnStop').addEventListener('click', stopNow);

    // Loop controlado a ~8 Hz (a cada 125ms):
    // Envia comandos na medida exata para o Arduino (9600 baud) nunca acumular fila!
    setInterval(() => {
      if (active) {
        sendPost(targetLinear, targetAngular);
      }
    }, 125);
  </script>
</body>
</html>
"""


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True


class WebJoystickNode(Node):
    def __init__(self):
        super().__init__('web_joystick_node')
        self.publisher_ = self.create_publisher(Twist, '/cmd_vel', 10)
        self.target_linear = 0.0
        self.target_angular = 0.0
        self.last_seq = -1
        self.is_stopped = True
        self.last_cmd_time = self.get_clock().now()

        # Timer a 8 Hz (125ms) — suave para o robô e nunca sobrecarrega a Serial de 9600 baud do Arduino!
        self.timer = self.create_timer(0.125, self.timer_callback)

    def update_cmd(self, linear: float, angular: float, seq: int):
        # Ignora pacotes Wi-Fi antigos que chegaram fora de ordem!
        if seq < self.last_seq and seq > 5:
            return
        self.last_seq = seq

        self.target_linear = float(linear)
        self.target_angular = float(angular)
        self.last_cmd_time = self.get_clock().now()

        # Se for comando de parada (0,0), publica na hora!
        if abs(self.target_linear) < 0.01 and abs(self.target_angular) < 0.01:
            self.target_linear = 0.0
            self.target_angular = 0.0
            self.publish_now()
            self.is_stopped = True
        else:
            self.is_stopped = False

    def publish_now(self):
        msg = Twist()
        msg.linear.x = self.target_linear
        msg.angular.z = self.target_angular
        self.publisher_.publish(msg)

    def timer_callback(self):
        elapsed = (self.get_clock().now() - self.last_cmd_time).nanoseconds / 1e9

        if not self.is_stopped:
            if elapsed < 0.40:
                # Mantém o movimento contínuo enquanto o dedo está na tela
                self.publish_now()
            else:
                # Passou de 400ms sem sinal do celular -> Freia imediatamente!
                self.target_linear = 0.0
                self.target_angular = 0.0
                self.publish_now()
                self.is_stopped = True


def get_local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return '192.168.18.9'


def main(args=None):
    rclpy.init(args=args)
    ros_node = WebJoystickNode()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, format, *args):
            pass

        def do_GET(self):
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0')
            self.end_headers()
            self.wfile.write(HTML_PAGE.encode('utf-8'))

        def do_POST(self):
            if self.path == '/cmd':
                length = int(self.headers.get('Content-Length', 0))
                body = self.rfile.read(length)
                try:
                    data = json.loads(body.decode('utf-8'))
                    ros_node.update_cmd(
                        data.get('linear', 0.0),
                        data.get('angular', 0.0),
                        data.get('seq', 0),
                    )
                except Exception:
                    pass
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b'OK')

    port = 8080
    ip = get_local_ip()
    server = ThreadedHTTPServer(('0.0.0.0', port), Handler)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()

    print("\n============================================================")
    print(" 📱 JOYSTICK WEB DO ROBÔ 2WD NO AR! (VERSÃO ANTI-LAG) ")
    print("============================================================")
    print(" 👉 Abra (ou recarregue a página) no navegador do seu CELULAR:")
    print(f"\n      http://{ip}:{port}\n")
    print("============================================================")
    print(" Pressione Ctrl+C para encerrar.\n")

    try:
        rclpy.spin(ros_node)
    except KeyboardInterrupt:
        pass
    finally:
        ros_node.update_cmd(0.0, 0.0, 999999)
        server.shutdown()
        ros_node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
