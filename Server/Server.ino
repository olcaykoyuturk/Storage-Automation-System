#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <map>
#include <ArduinoJson.h>

const char* ssid = "ESP32-AP";
const char* password = "12345678";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

enum ClientState {
  IDLE,          // Beklemede
  GO_TO_PICKUP,  // Alım noktasına gidiyor
  PICKING,       // Nesneyi alıyor
  GO_TO_DROPOFF, // Bırakma noktasına gidiyor
  DROPPING,      // Nesneyi bırakıyor
  RETURN_HOME    // Ana üsse geri dönüyor
};

struct ClientData {
  int8_t x = 0;
  int8_t y = 0;
  int8_t targetX = 0;
  int8_t targetY = 0;
  int8_t dropoffX = 0;
  int8_t dropoffY = 0;
  String yon = "BILINMEYEN";
  bool haveTask = false;
  ClientState state = IDLE;
  int processID = -1;
};

std::map<String, ClientData> clients;
std::map<uint32_t, String> clientIDs;

unsigned long lastBroadcast = 0;

const char html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
  <meta charset="UTF-8" />
  <title>ESP32 WebSocket Kontrol Paneli</title>
  <style>
    body { 
      font-family: Arial, sans-serif; 
      margin: 20px;
      display: flex;
      flex-direction: column;
      align-items: center;
      background-color: #f5f5f5;
    }

    h2 {
      text-align: center;
      color: #2c3e50;
    }

    .main-container {
      display: flex;
      flex-direction: row;
      gap: 20px;
      width: 100%;
      max-width: 1200px;
      margin-top: 20px;
    }

    .left-panel, .right-panel {
      flex: 1;
      display: flex;
      flex-direction: column;
    }

    .log-panel, .task-panel {
      border: 2px solid #3498db;
      border-radius: 10px;
      padding: 15px;
      background-color: #ffffff;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
      min-height: 200px;
    }

    #log {
      height: 200px;
      overflow-y: scroll;
      padding: 10px;
      background-color: white;
      border: 1px solid #ddd;
      margin-top: 10px;
      border-radius: 5px;
      font-family: monospace;
    }

    #clients {
      display: flex;
      flex-wrap: wrap;
      gap: 15px;
      margin-top: 20px;
    }

    .client-box {
      border: 1px solid #e0e0e0;
      border-radius: 8px;
      padding: 15px;
      min-width: 200px;
      background: #ffffff;
      box-shadow: 0 2px 4px rgba(0,0,0,0.05);
    }

    .client-header {
      font-weight: bold;
      color: #2c3e50;
      margin-bottom: 10px;
      font-size: 16px;
      border-bottom: 1px solid #eee;
      padding-bottom: 5px;
    }

    .client-info {
      margin: 6px 0;
      font-size: 14px;
    }

    .info-label {
      color: #7f8c8d;
      display: inline-block;
      width: 100px;
    }

    .info-value {
      color: #2c3e50;
      font-weight: 500;
    }

    .status-idle {
      color: #27ae60;
      font-weight: bold;
    }

    .status-busy {
      color: #e74c3c;
      font-weight: bold;
    }

    .no-pid {
      color: #95a5a6;
      font-style: italic;
    }

    .has-pid {
      color: #3498db;
      font-weight: bold;
    }

    .target-form {
      margin-top: 12px;
      padding-top: 8px;
      border-top: 1px dashed #ddd;
    }

    .target-form input {
      width: 60px;
      padding: 4px;
      margin-right: 5px;
      border: 1px solid #ddd;
      border-radius: 4px;
    }

    button {
      background-color: #3498db;
      color: white;
      border: none;
      border-radius: 5px;
      padding: 6px 12px;
      cursor: pointer;
      font-size: 14px;
      transition: background-color 0.3s;
    }

    button:hover {
      background-color: #2980b9;
    }

    .task-panel h3, .log-panel h3 {
      margin-top: 0;
      color: #2c3e50;
      border-bottom: 1px solid #eee;
      padding-bottom: 8px;
    }

    .task-panel select {
      margin-top: 10px;
      padding: 6px;
      font-size: 14px;
      border: 1px solid #ddd;
      border-radius: 4px;
      width: 100%;
    }

    .task-panel button {
      width: 100%;
      margin-top: 10px;
      padding: 8px;
    }

    button:disabled {
      background-color: #95a5a6 !important;
      cursor: not-allowed;
    }

    .task-button {
      margin-top: 5px;
      padding: 4px 8px;
      font-size: 13px;
    }

    .coordinate-input {
      margin-top: 10px;
    }

    .coordinate-input label {
      display: block;
      margin-bottom: 5px;
      color: #2c3e50;
      font-size: 14px;
    }

    .coordinate-input input {
      width: 60px;
      padding: 4px;
      margin-right: 5px;
      border: 1px solid #ddd;
      border-radius: 4px;
    }
  </style>
</head>
<body>
  <h2>ESP32 WebSocket Kontrol Paneli</h2>

  <div class="main-container">
    <div class="left-panel">
      <div class="log-panel">
        <h3>Gelen Mesajlar</h3>
        <div id="log"></div>
      </div>

      <h3>Client Bilgileri</h3>
      <div id="clients"></div>
    </div>

    <div class="right-panel">
      <div class="task-panel">
        <h3>Görev Atama</h3>
        <label for="colorSelect">Renk Seçin:</label>
        <select id="colorSelect">
          <option value="KIRMIZI">Kırmızı</option>
          <option value="MAVI">Mavi</option>
          <option value="SARI">Sarı</option>
        </select>

        <!-- Yeni Eklenen Alanlar -->
        <div class="coordinate-input">
          <label for="pickupX">Yük Alım Noktası (X,Y):</label>
          <div>
            <input type="number" id="pickupX" placeholder="X" min="-128" max="127" value="5">
            <input type="number" id="pickupY" placeholder="Y" min="-128" max="127" value="5">
          </div>
        </div>

        <div class="coordinate-input">
          <label for="dropoffX">Yük Boşaltım Noktası (X,Y):</label>
          <div>
            <input type="number" id="dropoffX" placeholder="X" min="-128" max="127" value="10">
            <input type="number" id="dropoffY" placeholder="Y" min="-128" max="127" value="10">
          </div>
        </div>

        <button onclick="startTask()">Görevi Başlat</button>
      </div>
    </div>
  </div>

  <script>
    let ws;
    const log = document.getElementById('log');
    const clientsDiv = document.getElementById('clients');

    function logMessage(message) {
      const p = document.createElement('p');
      p.textContent = message;
      log.appendChild(p);
      log.scrollTop = log.scrollHeight;
    }

    function connect() {
      ws = new WebSocket('ws://' + location.hostname + '/ws');

      ws.onopen = () => logMessage('WebSocket bağlantısı açıldı.');

      ws.onmessage = function(e) {
        try {
          let data = JSON.parse(e.data);
          if (data.type === "log") {
            logMessage(data.message);
          } else {
            updateClients(data);
          }
        } catch {
          logMessage(e.data);
        }
      };

      ws.onclose = () => {
        logMessage('Bağlantı kapandı. Yeniden bağlanıyor...');
        setTimeout(connect, 3000);
      };

      ws.onerror = e => {
        logMessage('Hata: ' + e.message);
        ws.close();
      };
    }

    function updateClients(data) {
      clientsDiv.innerHTML = '';
      for (const id in data) {
        const clientBox = document.createElement('div');
        clientBox.className = 'client-box';
        
        const stateText = {
          0: "BOŞTA",
          1: "ALIMA GİDİYOR",
          2: "ALIYOR",
          3: "BIRAKMAYA GİDİYOR",
          4: "BIRAKIYOR",
          5: "EVE DÖNÜYOR"
        };
        
        const statusClass = data[id].state === 0 ? 'status-idle' : 'status-busy';
        const pidClass = data[id].processID === -1 ? 'no-pid' : 'has-pid';
        const pidText = data[id].processID === -1 ? 'YOK' : data[id].processID;
        
        clientBox.innerHTML = `
          <div class="client-header">${id}</div>
          <div class="client-info">
            <span class="info-label">Pozisyon:</span>
            <span class="info-value">X=${data[id].x}, Y=${data[id].y}</span>
          </div>
          <div class="client-info">
            <span class="info-label">Yön:</span>
            <span class="info-value">${data[id].yon}</span>
          </div>
          <div class="client-info">
            <span class="info-label">Hedef:</span>
            <span class="info-value">X=${data[id].targetX}, Y=${data[id].targetY}</span>
          </div>
          <div class="client-info">
            <span class="info-label">Görev:</span>
            <span class="info-value">${data[id].haveTask ? 'VAR' : 'YOK'}</span>
          </div>
          <div class="client-info">
            <span class="info-label">Durum:</span>
            <span class="info-value ${statusClass}">${stateText[data[id].state] || "BİLİNMİYOR"}</span>
          </div>
          <div class="client-info">
            <span class="info-label">Process ID:</span>
            <span class="info-value ${pidClass}">${pidText}</span>
          </div>
          <div class="target-form">
            <input type="number" id="targetX-${id}" placeholder="X" min="-128" max="127">
            <input type="number" id="targetY-${id}" placeholder="Y" min="-128" max="127">
            <button onclick="setTarget('${id}')">Hedef Gönder</button>
            <button onclick="sendCommand('${id}', 'LOAD_COMPLETE')" 
                    ${data[id].state !== 2 ? 'disabled style="background-color: #95a5a6;"' : ''}>
              Yükleme Tamamlandı
            </button>
            <button onclick="sendCommand('${id}', 'UNLOAD_COMPLETE')" 
                    ${data[id].state !== 4 ? 'disabled style="background-color: #95a5a6;"' : ''}>
              Yük İndirildi
            </button>
          </div>
        `;
        clientsDiv.appendChild(clientBox);
      }
    }

    function setTarget(id) {
      const targetX = document.getElementById(`targetX-${id}`).value;
      const targetY = document.getElementById(`targetY-${id}`).value;
      if (targetX && targetY) {
        ws.send(`TARGET:${id},${targetX},${targetY}`);
      }
    }

    function startTask() {
      const selectedColor = document.getElementById('colorSelect').value;
      const pickupX = document.getElementById('pickupX').value;
      const pickupY = document.getElementById('pickupY').value;
      const dropoffX = document.getElementById('dropoffX').value;
      const dropoffY = document.getElementById('dropoffY').value;

      if (!pickupX || !pickupY || !dropoffX || !dropoffY) {
        logMessage("Hata: Lütfen tüm koordinatları girin!");
        return;
      }

      const taskData = `TASK:${selectedColor},${pickupX},${pickupY},${dropoffX},${dropoffY}`;
      ws.send(taskData);
      logMessage(`Görev başlatıldı: ${selectedColor} | Alım: (${pickupX},${pickupY}) → Bırakım: (${dropoffX},${dropoffY})`);
    }

    function sendCommand(id, command) {
      ws.send(`COMMAND:${id},${command}`);
      logMessage(`Komut gönderildi: ${id} -> ${command}`);
    }

    connect();
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);
  Serial.print("ESP32 Access Point IP adresi: ");
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", html);
  });

  server.begin();
  Serial.println("Sunucu başladı.");

  randomSeed(analogRead(0));
}

void loop() {
  ws.cleanupClients();

  for (auto& client : clients) {
    updateClientState(client.first);
  }

  if (millis() - lastBroadcast > 3000) {
    broadcastClientsData();
    lastBroadcast = millis();
  }
}