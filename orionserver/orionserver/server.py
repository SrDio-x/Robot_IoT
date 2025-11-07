import requests
from flask import Flask, request, jsonify, render_template_string
from datetime import datetime
import logging

# ============================================
# CONFIGURACIÓN INICIAL
# ============================================
logging.basicConfig(level=logging.INFO)
app = Flask(__name__)

# Dirección IP pública del servidor AWS
AWS_SERVER_IP = "http://52.4.205.146"

# ============================================
# ESTADO GLOBAL
# ============================================
current_state = {
    "command": "STOP",
    "speedness": 0
}
command_history = [current_state]
MAX_HISTORY = 100

# ============================================
# INTERFAZ WEB (Panel de control)
# ============================================
TANK_COMMANDER_HTML = f"""
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Tank Commander - AWS Control</title>
<style>
body {{
  font-family: 'Segoe UI', sans-serif;
  background: radial-gradient(circle at top, #0a192f 0%, #020c1b 100%);
  color: #e6f1ff; text-align: center; padding: 2rem;
}}
button {{
  border: none; border-radius: 50%;
  width: 4.5rem; height: 4.5rem;
  font-size: 1.5rem;
  background: #233554; color: #64ffda;
  box-shadow: 0 0 8px #64ffda44;
  transition: all 0.2s ease;
}}
button:hover {{ transform: scale(1.1); background: #112240; }}
button.stop {{
  background: #ff3b30; color: #fff;
  border-radius: 1rem; width: 5rem; height: 5rem;
}}
.pad {{
  display: grid;
  grid-template-columns: 5rem 5rem 5rem;
  grid-template-rows: repeat(3, 5rem);
  gap: 0.5rem; justify-content: center; margin: 2rem auto;
}}
.pad > button[data-cmd="left"] {{ grid-column: 1; grid-row: 2; }}
.pad > button[data-cmd="right"] {{ grid-column: 3; grid-row: 2; }}
.pad > button[data-cmd="forward"] {{ grid-column: 2; grid-row: 1; }}
.pad > button[data-cmd="backward"] {{ grid-column: 2; grid-row: 3; }}
.pad > button.stop {{ grid-column: 2; grid-row: 2; }}
</style>
</head>
<body>
<h1>Tank Commander (AWS)</h1>
<p>Controla tu robot remoto desde la nube.</p>
<div class="pad">
  <button data-cmd="forward">▲</button>
  <button data-cmd="left">◀</button>
  <button class="stop" data-cmd="stop">■</button>
  <button data-cmd="right">▶</button>
  <button data-cmd="backward">▼</button>
</div>
<div>
  <label>Velocidad
    <input id="speed" type="range" min="0" max="255" value="150">
    <span id="speedValue">150</span>
  </label>
</div>
<div id="status">Estado: Inactivo</div>
<script>
const API_URL = "{AWS_SERVER_IP}/api/tank_commands";
const statusEl = document.getElementById('status');
const speedSlider = document.getElementById('speed');
const speedValue = document.getElementById('speedValue');

speedSlider.addEventListener('input', (e) => {{
  speedValue.textContent = e.target.value;
}});

async function sendCommand(cmd) {{
  let speed = parseInt(speedSlider.value);
  let speedness = Math.round(speed * 100 / 255);
  
  let payload = {{
    command: cmd.toUpperCase(),
    speedness: speedness
  }};

  try {{
    const res = await fetch(API_URL, {{
      method: 'POST',
      headers: {{ 'Content-Type': 'application/json' }},
      body: JSON.stringify(payload)
    }});
    const data = await res.json();
    if (!res.ok) throw new Error(data.message || 'Error HTTP');
    statusEl.textContent = 'Comando: ' + cmd.toUpperCase() + ' (' + speedness + '%)';
  }} catch (err) {{
    statusEl.textContent = 'Error: ' + err.message;
  }}
}}

document.querySelectorAll('button[data-cmd]').forEach(btn => {{
  btn.addEventListener('click', () => sendCommand(btn.dataset.cmd));
}});
</script>
</body>
</html>
"""

# ============================================
# RUTAS DE LA API
# ============================================

@app.route('/', methods=['GET'])
def serve_control_page():
    return render_template_string(TANK_COMMANDER_HTML)


@app.route('/api/tank_commands', methods=['POST', 'GET'])
def handle_command():
    global current_state

    if request.method == 'POST':
        try:
            data = request.get_json(force=True)
            command = data.get('command', 'STOP').upper()
            speedness = int(data.get('speedness', 0))

            current_state["command"] = command
            current_state["speedness"] = speedness

            command_record = {
                "timestamp": datetime.now().isoformat(),
                "command": command,
                "speedness": speedness
            }
            command_history.append(command_record)
            if len(command_history) > MAX_HISTORY:
                command_history.pop(0)

            app.logger.info(f"[AWS] Comando recibido: {command} @ {speedness}%")
            return jsonify({"status": "OK", "message": "Comando recibido", "data": command_record}), 200

        except Exception as e:
            app.logger.error(f"[AWS] Error en POST: {e}")
            return jsonify({"status": "error", "message": str(e)}), 500

    # GET: El T-Beam consulta este endpoint
    command = current_state.get("command", "STOP")
    speedness = current_state.get("speedness", 0)

    app.logger.info(f"[AWS->TX] Enviando: {command} @ {speedness}%")

    return jsonify({
        "command": command,
        "speedness": speedness
    }), 200


@app.route('/api/commands/history', methods=['GET'])
def get_history():
    limit = request.args.get('limit', 10, type=int)
    return jsonify({
        "status": "OK",
        "count": len(command_history),
        "commands": command_history[-limit:]
    }), 200


@app.route('/health', methods=['GET'])
def health_check():
    return jsonify({
        "status": "OK",
        "message": "Servidor AWS operativo",
        "commands_received": len(command_history)
    }), 200


@app.route('/api/commands/latest', methods=['GET'])
def get_latest():
    return handle_command()


if __name__ == '__main__':
    current_state = {
        "command": "STOP",
        "speedness": 0
    }
    app.run(host='0.0.0.0', port=80)
