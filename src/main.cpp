#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <esp_system.h>
#include <ArduinoJson.h>
#include "../common/ControlProtocol.h"
#include "LoRaBoards.h"

// ========== Verificación de plataforma ==========
#if !defined(ESP32)
#error "This TX build targets the LilyGO T-Beam (ESP32)."
#endif

// ========== Configuración LoRa ==========
#ifndef CONFIG_RADIO_FREQ
#define CONFIG_RADIO_FREQ           920.0
#endif
#ifndef CONFIG_RADIO_OUTPUT_POWER
#define CONFIG_RADIO_OUTPUT_POWER   17
#endif
#ifndef CONFIG_RADIO_BW
#define CONFIG_RADIO_BW             125.0
#endif

// ========================================
// MODO DE OPERACIÓN
// 1 = AP + Web UI
// 2 = Cliente WiFi + GET a servidor
// ========================================
const uint8_t MODE = 2;

// ========== Configuración WiFi (MODO 2) ==========
const char* kStaSsid     = "UPBWiFi";
const char* kStaPassword = "";
const char* kServerUrl   = "http://52.4.205.146/api/tank_commands";

// ========== Variables Globales ==========
uint8_t sequenceCounter = 0;
uint8_t currentLeftSpeed = 255;
uint8_t currentRightSpeed = 255;
String lastState = "STOP";

unsigned long lastGetTime = 0;
const long getInterval = 500;
unsigned long lastWifiCheck = 0;
const long wifiCheckInterval = 5000;

bool wasConnected = false;
bool lastCommandWasStop = true;

WebServer server(80);

// ========== Interfaz Web HTML ==========
const char indexPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Tank Commander</title>
<style>
body {
  font-family: 'Segoe UI', sans-serif;
  margin: 0;
  padding: 2rem;
  background: radial-gradient(circle at top, #0a192f 0%, #020c1b 100%);
  color: #e6f1ff;
  text-align: center;
}
h1 { color: #64ffda; font-weight: 500; letter-spacing: 1px; }
p { color: #8892b0; margin-top: -0.5rem; }
button {
  border: none; border-radius: 50%;
  width: 5rem; height: 5rem; font-size: 0.95rem;
  cursor: pointer; background: #233554; color: #64ffda;
  transition: all 0.2s ease; box-shadow: 0 0 10px #0a192f inset, 0 0 8px #64ffda44;
}
button:hover {
  transform: scale(1.08);
  background: #112240;
  box-shadow: 0 0 12px #64ffda99;
}
button.stop {
  background: #ff3b30; color: #fff;
  box-shadow: 0 0 12px #ff3b30aa;
  border-radius: 1rem;
  width: 5rem; height: 5rem;
}
.pad {
  display: grid;
  grid-template-columns: repeat(3, 6rem);
  grid-template-rows: repeat(3, 6rem);
  justify-content: center;
  gap: 0.7rem;
  margin: 3rem auto;
}
.speeds {
  margin-top: 2rem;
  display: flex;
  flex-wrap: wrap;
  gap: 2rem;
  justify-content: center;
  align-items: center;
}
.speeds label {
  display: flex;
  flex-direction: column;
  align-items: center;
  font-size: 0.9rem;
  color: #a8b2d1;
}
input[type=range] {
  width: 180px;
  accent-color: #64ffda;
}
#speedBtn {
  background: #64ffda;
  color: #0a192f;
  border-radius: 1rem;
  width: 8rem;
  height: 2.5rem;
  font-weight: bold;
  box-shadow: 0 0 5px #64ffdaaa;
}
#speedBtn:hover {
  transform: scale(1.05);
  box-shadow: 0 0 10px #64ffda;
}
#status {
  margin-top: 1.5rem;
  font-size: 1.1rem;
  color: #ccd6f6;
  font-weight: bold;
}
footer {
  margin-top: 3rem;
  font-size: 0.8rem;
  color: #8892b0;
}
</style>
</head>
<body>
<h1>Tank Commander Interface</h1>
<p>Controla tu robot de forma inalámbrica mediante Wi-Fi Bridge</p>
<div class="pad">
  <div></div>
  <button data-cmd="forward">▲</button>
  <div></div>
  <button data-cmd="left">◀</button>
  <button class="stop" data-cmd="stop">■</button>
  <button data-cmd="right">▶</button>
  <div></div>
  <button data-cmd="backward">▼</button>
  <div></div>
</div>
<div class="speeds">
  <label>Velocidad Izquierda
    <input id="leftSpeed" type="range" min="0" max="255" value="255">
    <span id="leftValue">255</span>
  </label>
  <label>Velocidad Derecha
    <input id="rightSpeed" type="range" min="0" max="255" value="255">
    <span id="rightValue">255</span>
  </label>
  <button data-cmd="speed" id="speedBtn">AJUSTAR</button>
</div>
<div id="status">Estado: IDLE</div>
<footer>Conéctate a <strong>TankController Wi-Fi</strong> (contraseña: tank12345) para operar.</footer>

<script>
const statusEl = document.getElementById('status');
const left = document.getElementById('leftSpeed');
const right = document.getElementById('rightSpeed');
const leftValue = document.getElementById('leftValue');
const rightValue = document.getElementById('rightValue');

function updateLabels() {
  leftValue.textContent = left.value;
  rightValue.textContent = right.value;
}
left.addEventListener('input', updateLabels);
right.addEventListener('input', updateLabels);
updateLabels();

async function sendCommand(cmd) {
  statusEl.textContent = 'Estado: enviando...';
  const params = new URLSearchParams({ action: cmd });
  if (cmd === 'speed') {
    params.set('left', left.value);
    params.set('right', right.value);
  }
  try {
    const res = await fetch('/cmd', { method: 'POST', body: params });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const data = await res.json();
    statusEl.textContent = `Estado: ${data.state}`;
  } catch (err) {
    statusEl.textContent = 'Estado: ERROR - ' + err.message;
  }
}

document.querySelectorAll('button[data-cmd]').forEach(btn => {
  btn.addEventListener('click', () => sendCommand(btn.dataset.cmd));
});
</script>
</body>
</html>
)rawliteral";

// ========== Funciones LoRa ==========

bool beginLoRa() {
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);
  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);

#ifdef RADIO_TCXO_ENABLE
  pinMode(RADIO_TCXO_ENABLE, OUTPUT);
  digitalWrite(RADIO_TCXO_ENABLE, HIGH);
#endif

  if (!LoRa.begin(CONFIG_RADIO_FREQ * 1000000)) {
    Serial.println("LoRa init failed. Check wiring.");
    return false;
  }

  LoRa.setTxPower(CONFIG_RADIO_OUTPUT_POWER);
  LoRa.setSignalBandwidth(CONFIG_RADIO_BW * 1000);
  LoRa.setSpreadingFactor(7);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();
  LoRa.receive();

  Serial.println("LoRa radio ready (TX mode).");
  return true;
}

bool sendLoRaFrame(TankControl::Command cmd, uint8_t leftSpeed, uint8_t rightSpeed) {
  TankControl::ControlFrame frame;
  TankControl::initFrame(frame, cmd, leftSpeed, rightSpeed, sequenceCounter++);

  uint8_t encrypted[TankControl::kFrameSize];
  if (!TankControl::encryptFrame(frame, encrypted, sizeof(encrypted))) {
    Serial.println("Encrypt failed");
    return false;
  }

  LoRa.idle();
  LoRa.beginPacket();
  LoRa.write(encrypted, sizeof(encrypted));
  bool ok = (LoRa.endPacket() == 1);
  LoRa.receive();

  if (ok) {
    Serial.print("TX -> cmd=");
    Serial.print(static_cast<int>(frame.command));
    Serial.print(" seq=");
    Serial.print(frame.sequence);
    Serial.print(" left=");
    Serial.print(frame.leftSpeed);
    Serial.print(" right=");
    Serial.println(frame.rightSpeed);
  } else {
    Serial.println("LoRa TX failed");
  }
  return ok;
}

void sendStopCommand() {
  if (lastCommandWasStop && currentLeftSpeed == 0 && currentRightSpeed == 0) {
    return;
  }

  currentLeftSpeed = 0;
  currentRightSpeed = 0;
  lastState = "STOP";
  lastCommandWasStop = true;

  if (sendLoRaFrame(TankControl::Command::Stop, 0, 0)) {
    Serial.println("SAFETY STOP sent");
  }
}

void sendSpectrumTestBurst() {
  static constexpr size_t kBurstSize = 192;
  uint8_t payload[kBurstSize];
  for (size_t i = 0; i < kBurstSize; ++i) {
    payload[i] = static_cast<uint8_t>(random(0, 256));
  }

  Serial.println("Sending LoRa spectrum test burst...");
  LoRa.idle();
  LoRa.beginPacket();
  LoRa.write(payload, sizeof(payload));
  if (LoRa.endPacket() == 1) {
    Serial.print("Burst sent. Length: ");
    Serial.println(sizeof(payload));
  } else {
    Serial.println("Spectrum test burst failed");
  }
  LoRa.receive();
}

// ========== Parseo de Comandos (MODO 1: Web UI) ==========

TankControl::Command parseCommand(const String &action) {
  if (action == "forward") return TankControl::Command::Forward;
  if (action == "backward") return TankControl::Command::Backward;
  if (action == "left") return TankControl::Command::Left;
  if (action == "right") return TankControl::Command::Right;
  if (action == "speed") return TankControl::Command::SetSpeed;
  return TankControl::Command::Stop;
}

// ========== Handlers Web Server (MODO 1) ==========

void handleWebRoot() {
  server.send_P(200, "text/html", indexPage);
}

void handleWebCommand() {
  if (!server.hasArg("action")) {
    server.send(400, "application/json", "{\"error\":\"missing action\"}");
    return;
  }

  String action = server.arg("action");
  action.toLowerCase();
  TankControl::Command cmd = parseCommand(action);
  bool ok = true;

  if (cmd == TankControl::Command::SetSpeed) {
    int left = server.hasArg("left") ? server.arg("left").toInt() : currentLeftSpeed;
    int right = server.hasArg("right") ? server.arg("right").toInt() : currentRightSpeed;
    left = constrain(left, 0, 255);
    right = constrain(right, 0, 255);
    currentLeftSpeed = static_cast<uint8_t>(left);
    currentRightSpeed = static_cast<uint8_t>(right);
    ok = sendLoRaFrame(cmd, currentLeftSpeed, currentRightSpeed);
    if (ok) lastState = "SPEED";
    lastCommandWasStop = false;
  } else {
    ok = sendLoRaFrame(cmd, currentLeftSpeed, currentRightSpeed);
    if (ok) {
      switch (cmd) {
        case TankControl::Command::Forward:  lastState = "FORWARD";  break;
        case TankControl::Command::Backward: lastState = "BACKWARD"; break;
        case TankControl::Command::Left:     lastState = "LEFT";     break;
        case TankControl::Command::Right:    lastState = "RIGHT";    break;
        case TankControl::Command::Stop:     lastState = "STOP";     break;
        default: break;
      }
      lastCommandWasStop = (cmd == TankControl::Command::Stop);
    }
  }

  if (!ok) {
    server.send(500, "application/json", "{\"error\":\"lora tx failed\"}");
    return;
  }

  String body = "{\"state\":\"";
  body += lastState;
  body += "\"}";
  server.send(200, "application/json", body);
}

// ========== Comunicación con Servidor (MODO 2) ==========
// ========== Comunicación con Servidor (MODO 2) ==========
void performHttpGet() {
  if (WiFi.status() != WL_CONNECTED) {
    sendStopCommand();
    return;
  }

  HTTPClient http;
  http.setTimeout(2000);
  http.begin(kServerUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.println("JSON parse failed: " + String(error.c_str()));
      sendStopCommand();
      http.end();
      return;
    }

    const char* cmdStr = doc["command"] | "STOP";
    int speedness = doc["speedness"] | 0;

    speedness = constrain(speedness, 0, 100);
    uint8_t speed = map(speedness, 0, 100, 0, 255);

    // ========== MAPEOS AJUSTADOS ==========
    // Según observación real:
    //  - LEFT hace forward → debería ser giro izquierda
    //  - FORWARD hace left → debería avanzar
    //  - RIGHT está bien
    //  - BACKWARD hace +1,-1 → debería ser ambos -1
    //
    // Se aplica lógica inversa en FORWARD y LEFT, y se ajusta BACKWARD.

    TankControl::Command cmd;
    uint8_t leftSpeed = speed;
    uint8_t rightSpeed = speed;
    String debugMsg = "";

    if (strcmp(cmdStr, "FORWARD") == 0) {
      // Antes hacía izquierda → ahora corregido a FORWARD real
      cmd = TankControl::Command::Right;
      debugMsg = "FORWARD → RIGHT (corregido para avance)";

    } else if (strcmp(cmdStr, "BACKWARD") == 0) {
      // Antes hacía +1,-1 → invertimos
      cmd = TankControl::Command::Left;
      debugMsg = "BACKWARD → LEFT (corregido para reversa)";

    } else if (strcmp(cmdStr, "LEFT") == 0) {
      // Antes hacía forward → debe girar a la izquierda
      cmd = TankControl::Command::Forward;
      debugMsg = "LEFT → FORWARD (corregido giro izquierda)";

    } else if (strcmp(cmdStr, "RIGHT") == 0) {
      // Este ya funcionaba bien
      cmd = TankControl::Command::Backward;
      debugMsg = "RIGHT → BACKWARD (mantiene correcto)";

    } else {
      cmd = TankControl::Command::Stop;
      leftSpeed = 0;
      rightSpeed = 0;
      debugMsg = "STOP";
    }

    currentLeftSpeed = leftSpeed;
    currentRightSpeed = rightSpeed;
    lastState = cmdStr;
    lastCommandWasStop = (cmd == TankControl::Command::Stop);

    if (sendLoRaFrame(cmd, leftSpeed, rightSpeed)) {
      Serial.printf(" User: %s @ %d%% → TX: cmd=%d | %s\n", 
                    cmdStr, speedness, (int)cmd, debugMsg.c_str());
    }
  } else {
    Serial.printf("❌ HTTP failed: %d\n", httpCode);
    sendStopCommand();
  }
  http.end();
}




// ========== SETUP ==========

void setup() {
  setupBoards(/*disable_u8g2=*/true);
  delay(1500);
  
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("\n===========================================");
  Serial.println("T-Beam TX | LoRa Tank Commander");
  Serial.println("===========================================");

  // Inicializar LoRa
  bool radioReady = beginLoRa();
  if (!radioReady) {
    Serial.println("FATAL: LoRa setup failed. Check radio module.");
    while (true) {
      delay(1000);
    }
  }

  // Test burst inicial
  randomSeed(esp_random());
  sendSpectrumTestBurst();

  // Comando inicial de seguridad
  sendStopCommand();

  // Configurar según modo
  if (MODE == 1) {
    Serial.println("Starting in AP + Web UI mode (MODE 1)");
    WiFi.mode(WIFI_AP);
    if (WiFi.softAP("TankController", "tank12345")) {
      Serial.print("SoftAP ready. SSID: TankController, IP: ");
      Serial.println(WiFi.softAPIP());
    } else {
      Serial.println("Failed to start SoftAP.");
    }

    server.on("/", HTTP_GET, handleWebRoot);
    server.on("/cmd", HTTP_POST, handleWebCommand);
    server.onNotFound([]() {
      server.send(404, "application/json", "{\"error\":\"not found\"}");
    });
    server.begin();
    Serial.println("Web UI ready at http://" + WiFi.softAPIP().toString());
  } else if (MODE == 2) {
    Serial.println("Starting in WiFi Client + Server GET mode (MODE 2)");
    WiFi.mode(WIFI_STA);
    WiFi.begin(kStaSsid, kStaPassword);

    Serial.print("Connecting to ");
    Serial.println(kStaSsid);

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("Server: ");
      Serial.println(kServerUrl);
      wasConnected = true;
    } else {
      Serial.println("\nFailed to connect to WiFi. Will retry in loop.");
      sendStopCommand();
    }
  } else {
    Serial.println("Invalid MODE. Must be 1 or 2.");
    while (true) delay(1000);
  }

  Serial.println("System ready.");
}

// ========== LOOP ==========

void loop() {
  if (MODE == 1) {
    server.handleClient();
  } else if (MODE == 2) {
    unsigned long now = millis();

    if (now - lastWifiCheck >= wifiCheckInterval) {
      lastWifiCheck = now;
      bool currentlyConnected = (WiFi.status() == WL_CONNECTED);

      if (wasConnected && !currentlyConnected) {
        Serial.println("WiFi LOST -> Sending STOP");
        sendStopCommand();
      } else if (!wasConnected && currentlyConnected) {
        Serial.println("WiFi RECONNECTED");
      }
      wasConnected = currentlyConnected;

      if (!currentlyConnected) {
        Serial.println("Attempting WiFi reconnect...");
        WiFi.reconnect();
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      if (now - lastGetTime >= getInterval) {
        lastGetTime = now;
        performHttpGet();
      }
    } else {
      static unsigned long lastSafetyStop = 0;
      if (now - lastSafetyStop >= 1000) {
        lastSafetyStop = now;
        sendStopCommand();
      }
    }
  }

  delay(10);
}