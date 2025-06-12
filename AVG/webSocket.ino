void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      Serial.println("WebSocket bağlı.");
      
      // ID gönder
      String idMessage = "ID:" + String(deviceID);
      webSocket.sendTXT(idMessage);
      Serial.println("Gönderilen ID: " + idMessage);
      break;
    }
      
    case WStype_DISCONNECTED:
      Serial.println("WebSocket bağlantısı koptu.");
      break;
      
    case WStype_TEXT: {
      String message = (char*)payload;
      Serial.printf("Sunucu: %s\n", message.c_str());
      
      // Kimlik onayı kontrolü
      if (message.indexOf("Kimlik kaydedildi") != -1) {
        Serial.println(">> Sunucu ID'yi tanıdı");
      }
      
      // Hedef mesajlarını işleme
      if (message.startsWith("TARGET:")) {
        int commaIndex = message.indexOf(',', 7);
        if (commaIndex != -1) {
          target_x = message.substring(7, commaIndex).toInt();
          target_y = message.substring(commaIndex + 1).toInt();
          Serial.printf("Hedef güncellendi: X=%d, Y=%d\n", target_x, target_y);
          
          if (target_x == x && target_y == y) {
            hedefeUlasildi = true;
          } else {
            hedefeUlasildi = false;
            hareketPlanla();
            break;
          }
        }
      }
      break;
    }
    
    default:
      break;
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("WiFi bağlanıyor...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi Bağlandı: %s\n", WiFi.localIP().toString().c_str());
}

void setupWebSocket() {
  webSocket.begin("192.168.4.1", 80, "/ws");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}