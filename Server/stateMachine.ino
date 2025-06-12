// BOŞTA OLAN ARACI SEÇME
std::vector<String> getIdleClients() {
  std::vector<String> idleClients;

  for (const auto& pair : clients) {
    const String& clientId = pair.first;
    const ClientData& data = pair.second;

    if (data.state == IDLE) {
      idleClients.push_back(clientId);
    }
  }

  return idleClients;
}

// O ARAÇLARDAN RANDOM BİR ADET ALDIK
String getRandomIdleClient() {
  std::vector<String> idleClients = getIdleClients();
  
  if (idleClients.empty()) {
    return ""; // Hiç boşta client yoksa boş string döner
  }

  int index = random(0, idleClients.size());
  return idleClients[index];
}

int uniqueIdGenerate(){
  int ID = 10000;
  ID = ID + 1000;
  return ID;
}


void assignmentTask(int pickupX, int pickupY, int dropoffX, int dropoffY) {
  String selectedClient = getRandomIdleClient();
  
  if (selectedClient == "") {
    String msg = "Boşta araç yok, görev atanamadı.";
    sendLogMessage(msg);
    return;
  }
  else {
    long processID = uniqueIdGenerate();
    clients[selectedClient].processID = processID;
    clients[selectedClient].state = GO_TO_PICKUP;
    
    // Alım noktası hedefi
    clients[selectedClient].targetX = pickupX;
    clients[selectedClient].targetY = pickupY;
    
    // Bırakma noktasını kaydet (DÜZELTME: Bu satırlar eklendi)
    clients[selectedClient].dropoffX = dropoffX;
    clients[selectedClient].dropoffY = dropoffY;
    
    clients[selectedClient].haveTask = true;

    sendLogMessage("Process ID atandı: " + selectedClient + " => ProcessID=" + String(processID));
    sendLogMessage(selectedClient + " alım noktasına yönlendiriliyor: (" + String(clients[selectedClient].targetX) + ", " + String(clients[selectedClient].targetY) + ")");

    // Hedefi istemciye gönder
    for (auto& c : clientIDs) {
      if (c.second == selectedClient) {
        ws.text(c.first, "TARGET:" + String(clients[selectedClient].targetX) + "," + String(clients[selectedClient].targetY));
        break;
      }
    }
  }
}

void updateClientState(String clientId) {
  if (clients.find(clientId) == clients.end()) return;

  ClientData &client = clients[clientId];
  
  switch (client.state) {
    case GO_TO_PICKUP:
      if (client.x == client.targetX && client.y == client.targetY) {
        client.state = PICKING;
        client.targetX = client.dropoffX;  // Bırakım noktasını hedef yap
        client.targetY = client.dropoffY;
        sendLogMessage(clientId + " alım noktasına ulaştı, yükleme bekleniyor...");
      }
      break;
      
    case GO_TO_DROPOFF:
      if (client.x == client.targetX && client.y == client.targetY) {
        client.state = DROPPING;
        sendLogMessage(clientId + " bırakma noktasına ulaştı, yük indirme bekleniyor...");
      }
      break;
      
    case RETURN_HOME:
      if (client.x == client.targetX && client.y == client.targetY) {
        client.state = IDLE;
        client.haveTask = false;
        client.processID = -1;
        sendLogMessage(clientId + " ana üsse döndü, görev tamamlandı!");
      }
      break;
      
    // PICKING ve DROPPING durumları artık kullanıcı onayıyla ilerleyecek
    default:
      break;
  }
}