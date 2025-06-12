void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client connected: %u\n", client->id());
      sendLogMessage("Client bağlandı: #" + String(client->id()));
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client disconnected: %u\n", client->id());
      if (clientIDs.count(client->id())) {
        String id = clientIDs[client->id()];
        clientIDs.erase(client->id());
        clients.erase(id);
        sendLogMessage("Client ayrıldı: " + id);
      }
      break;

    case WS_EVT_DATA: {
      String msg = "";
      for (size_t i = 0; i < len; i++) {
        msg += (char)data[i];
      }

      Serial.printf("Mesaj alındı: %s\n", msg.c_str());
      sendLogMessage("Server: " + msg);

      if (msg.startsWith("ID:") && msg.indexOf(">>") == -1 && msg.indexOf("TARGET:") == -1) {
        String id = msg.substring(3);
        id.trim();
        
        if (clientIDs.count(client->id())) {
          String oldId = clientIDs[client->id()];
          if (oldId == id) break;
          clients.erase(oldId);
        }
        
        clientIDs[client->id()] = id;
        if (clients.find(id) == clients.end()) {
          clients[id] = ClientData();
        }
        sendLogMessage("ID kaydedildi: " + id);
      }

      else if (msg.indexOf(">>") != -1 && msg.indexOf("X=") != -1 && msg.indexOf("Y=") != -1) {
        if (!clientIDs.count(client->id())) break;

        String id = clientIDs[client->id()];
        
        int coordStart = msg.indexOf(">>") + 2;
        String coordPart = msg.substring(coordStart);
        coordPart.trim();

        int xIndex = coordPart.indexOf("X=");
        int yIndex = coordPart.indexOf("Y=");
        int yonIndex = coordPart.indexOf("Yön=");
        
        if (xIndex == -1 || yIndex == -1) break;

        String xs = coordPart.substring(xIndex + 2, coordPart.indexOf(',', xIndex));
        String ys;
        
        if (yonIndex != -1) {
          ys = coordPart.substring(yIndex + 2, yonIndex);
        } else {
          ys = coordPart.substring(yIndex + 2);
        }
        
        xs.trim();
        ys.trim();

        int xVal = xs.toInt();
        int yVal = ys.toInt();

        if (xVal < -128) xVal = -128;
        else if (xVal > 127) xVal = 127;
        if (yVal < -128) yVal = -128;
        else if (yVal > 127) yVal = 127;

        clients[id].x = xVal;
        clients[id].y = yVal;
        
        if (yonIndex != -1) {
          String yonStr = coordPart.substring(yonIndex + 4);
          yonStr.trim();
          clients[id].yon = yonStr;
        } else {
          clients[id].yon = "BILINMEYEN";
        }
      }

      else if (msg.startsWith("TARGET:")) {
        String content = msg.substring(7);
        int firstComma = content.indexOf(',');
        int secondComma = content.indexOf(',', firstComma + 1);
        
        if (firstComma == -1 || secondComma == -1) break;
        
        String targetID = content.substring(0, firstComma);
        String targetXStr = content.substring(firstComma + 1, secondComma);
        String targetYStr = content.substring(secondComma + 1);
        
        if (clients.find(targetID) != clients.end()) {
          int targetX = targetXStr.toInt();
          int targetY = targetYStr.toInt();
          
          if (targetX < -128) targetX = -128;
          else if (targetX > 127) targetX = 127;
          if (targetY < -128) targetY = -128;
          else if (targetY > 127) targetY = 127;
          
          clients[targetID].targetX = targetX;
          clients[targetID].targetY = targetY;
          
          for (auto& c : clientIDs) {
            if (c.second == targetID) {
              ws.text(c.first, "TARGET:" + String(targetX) + "," + String(targetY));
              break;
            }
          }
        }
      }

      else if (msg.startsWith("TASK:")) {
        // FORMAT: "TASK:RENK,PICKUP_X,PICKUP_Y,DROPOFF_X,DROPOFF_Y"
        String content = msg.substring(5);
        int firstComma = content.indexOf(',');
        int secondComma = content.indexOf(',', firstComma + 1);
        int thirdComma = content.indexOf(',', secondComma + 1);
        int fourthComma = content.indexOf(',', thirdComma + 1);

        if (firstComma == -1 || secondComma == -1 || thirdComma == -1 || fourthComma == -1) {
          sendLogMessage("Hata: Geçersiz görev formatı!");
          break;
        }

        String renk = content.substring(0, firstComma);
        int pickupX = content.substring(firstComma + 1, secondComma).toInt();
        int pickupY = content.substring(secondComma + 1, thirdComma).toInt();
        int dropoffX = content.substring(thirdComma + 1, fourthComma).toInt();
        int dropoffY = content.substring(fourthComma + 1).toInt();

        sendLogMessage("Görev başlatılıyor: " + renk + 
                      " | Alım: (" + String(pickupX) + "," + String(pickupY) + 
                      ") → Bırakım: (" + String(dropoffX) + "," + String(dropoffY) + ")");

        assignmentTask(pickupX, pickupY, dropoffX, dropoffY);
}

      else if (msg.startsWith("COMMAND:")) {
        String content = msg.substring(8);
        int commaPos = content.indexOf(',');
        
        if (commaPos == -1) break;
        
        String clientId = content.substring(0, commaPos);
        String command = content.substring(commaPos + 1);
        
        if (clients.find(clientId) == clients.end()) break;
        
        ClientData &client = clients[clientId];
        
        if (command == "LOAD_COMPLETE" && client.state == PICKING) {
          client.state = GO_TO_DROPOFF;
          client.targetX = client.dropoffX;
          client.targetY = client.dropoffY;
          sendLogMessage(clientId + " yükleme tamamlandı, bırakma noktasına gidiyor...");
          
          for (auto& c : clientIDs) {
            if (c.second == clientId) {
              ws.text(c.first, "TARGET:" + String(client.targetX) + "," + String(client.targetY));
              break;
            }
          }
        }
        else if (command == "UNLOAD_COMPLETE" && client.state == DROPPING) {
          client.state = RETURN_HOME;
          client.targetX = 0;
          client.targetY = 0;
          sendLogMessage(clientId + " yük indirildi, ana üsse dönüyor...");
          
          for (auto& c : clientIDs) {
            if (c.second == clientId) {
              ws.text(c.first, "TARGET:" + String(client.targetX) + "," + String(client.targetY));
              break;
            }
          }
        }
      }
      
      break;
    }

    default:
      break;
  }
}

// LOG MESSAGE
void sendLogMessage(const String& message) {
  DynamicJsonDocument doc(256);
  doc["type"] = "log";
  doc["message"] = "ESP32SERVER: " + message;

  String output;
  serializeJson(doc, output);
  ws.textAll(output);
}

// BROADCAST
void broadcastClientsData() {
  DynamicJsonDocument doc(1024);
  JsonObject data = doc.to<JsonObject>();

  for (auto &item : clients) {
    JsonObject clientObj = data[item.first].to<JsonObject>();
    clientObj["x"] = item.second.x;
    clientObj["y"] = item.second.y;
    clientObj["targetX"] = item.second.targetX;
    clientObj["targetY"] = item.second.targetY;
    clientObj["yon"] = item.second.yon;
    clientObj["haveTask"] = item.second.haveTask;
    clientObj["state"] = item.second.state;
    clientObj["processID"] = item.second.processID;
  }

  String jsonStr;
  serializeJson(data, jsonStr);
  ws.textAll(jsonStr);
}