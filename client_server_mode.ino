// ==================== Smart WiFi Config ====================
extern int selectedClientIndex;

bool saveConfiguredStationCredentials(const String &ssid, const String &password) {
  if (!validateWiFiSsidValue(ssid) || !validateWiFiPasswordValue(password, false)) {
    return false;
  }

  return copyStringToBuffer(ssid, client_wifi_ssid, sizeof(client_wifi_ssid)) &&
         copyStringToBuffer(password, client_wifi_password, sizeof(client_wifi_password));
}

bool isServerAccessPointHealthy() {
  wifi_mode_t wifiMode = WiFi.getMode();
  IPAddress apAddress = WiFi.softAPIP();
  bool apModeActive = wifiMode == WIFI_AP || wifiMode == WIFI_AP_STA;
  return apModeActive && apAddress != IPAddress(0, 0, 0, 0);
}

String getWiFiStatusName(wl_status_t status) {
  switch (status) {
    case (wl_status_t)254:
      return "Stopped";
    case WL_CONNECTED:
      return "Connected";
    case WL_NO_SSID_AVAIL:
      return "No SSID";
    case WL_CONNECT_FAILED:
      return "Connect Failed";
    case WL_CONNECTION_LOST:
      return "Connection Lost";
    case WL_DISCONNECTED:
      return "Disconnected";
    case WL_IDLE_STATUS:
      return "Idle";
    case WL_SCAN_COMPLETED:
      return "Scan Completed";
    default:
      return "Unknown(" + String((int)status) + ")";
  }
}

String getWiFiModeName(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_OFF:
      return "OFF";
    case WIFI_STA:
      return "STA";
    case WIFI_AP:
      return "AP";
    case WIFI_AP_STA:
      return "AP+STA";
    default:
      return "UNKNOWN";
  }
}

bool hasClientSlotData(int clientIndex) {
  if (clientIndex < 0 || clientIndex >= MAX_CLIENTS) {
    return false;
  }

  return clientSerialData[clientIndex].length() > 0 ||
         connectedClientIds[clientIndex].length() > 0 ||
         clientLastSeenMillis[clientIndex] > 0;
}

bool isServerControlPayload(const String &payload) {
  return payload == "SERVER:WELCOME" ||
         payload == "SERVER:WELCOME_BACK" ||
         payload == "SERVER:BUSY";
}

void startConfigMode() {
  Serial.println("\nEntering WiFi config mode...");
  Serial.println("Temporary config WiFi is active");
  Serial.println("Portal SSID: " + maskSensitiveValue(wifimanager_ssid));
  Serial.println("Then visit: http://192.168.4.1");
  Serial.println("You can continue using AT commands during config");
  Serial.println("Type AT+EXITCONFIG to exit config mode");

  wm.resetSettings();
  wm.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  wm.setConfigPortalTimeout(300);

  wm.setConfigPortalBlocking(false);
  wm.startConfigPortal(wifimanager_ssid, wifimanager_password);

  inConfigMode = true;
  configModeStartTime = millis();

  Serial.println("OK Config mode started (non-blocking)");
}

void handleConfigMode() {
  if (!inConfigMode) return;

  wm.process();

  if (WiFi.status() == WL_CONNECTED) {
    String configuredSsid = WiFi.SSID();
    String configuredPassword = WiFi.psk();
    if (!saveConfiguredStationCredentials(configuredSsid, configuredPassword)) {
      Serial.println("\nX Config rejected by security policy");
      WiFi.disconnect(true, false);
      wm.stopConfigPortal();
      inConfigMode = false;
      return;
    }

    saveConfigToEEPROM();
    Serial.println("\nOK WiFi config saved securely");
    inConfigMode = false;
    wm.stopConfigPortal();

    Serial.println("System will restart...");
    delay(1000);
    ESP.restart();
  }

  if (millis() - configModeStartTime > configModeTimeout) {
    Serial.println("\nX Config timeout, restarting...");
    inConfigMode = false;
    wm.stopConfigPortal();
    ESP.restart();
  }
}

void exitConfigMode() {
  if (inConfigMode) {
    Serial.println("Exiting config mode...");
    inConfigMode = false;
    wm.stopConfigPortal();
    Serial.println("OK Exited config mode");
  } else {
    Serial.println("Not in config mode");
  }
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  setLED(CRGB(255, 255, 0));
}

void handleValidatedClientPayload(const String &payload) {
  if (isServerControlPayload(payload)) {
    if (debugMode) {
      Serial.println("[TCP control] " + payload);
    }

    if (payload == "SERVER:BUSY") {
      tcpClient.stop();
      clientTcpFrameBuffer = "";
      clearSecurityState(clientTcpSecurityState);
      tcpConnected = false;
    }
    return;
  }

  if (!sendValidatedPayloadToUART(UART_NUM_2, payload)) {
    recordSecurityFailure(clientTcpSecurityState);
    return;
  }

  appendToSerialBuffer((char *)payload.c_str(), payload.length());

  if (logToSD && sdCardReady) {
    enqueueSDLog(payload, client_id, false);
  }
}

bool processClientIngress(const uint8_t *buf, size_t readBytes) {
  String errorReason;
  if (!appendIngressChunk(SECURITY_SOURCE_TCP_CLIENT, -1, buf, readBytes, errorReason)) {
    return false;
  }

  String payload;
  while (getValidatedPayload(SECURITY_SOURCE_TCP_CLIENT, -1, payload, errorReason)) {
    handleValidatedClientPayload(payload);
  }

  return true;
}

void handleValidatedServerPayload(int clientIndex, const String &payload) {
  clientLastSeenMillis[clientIndex] = millis();

  String parsedClientId = parseClientId(payload);
  if (parsedClientId != "UNKNOWN") {
    if (connectedClientIds[clientIndex] != parsedClientId) {
      connectedClientIds[clientIndex] = parsedClientId;
      clientSerialData[clientIndex] += "// Linked: " + parsedClientId + "\n";
    }
    return;
  }

  appendToSerialBuffer((char *)payload.c_str(), payload.length());

  clientSerialData[clientIndex] += payload;
  clientSerialData[clientIndex] += '\n';

  if (selectedClientIndex >= 0 && selectedClientIndex == clientIndex) {
    Serial.write((const uint8_t *)payload.c_str(), payload.length());
    sendValidatedPayloadToUART(UART_NUM_2, payload);
  }

  if (logToSD && sdCardReady) {
    String clientId = "client_" + String(clientIndex);
    enqueueSDLog(payload, clientId, true);
  }

  if (clientSerialData[clientIndex].length() > 2000) {
    clientSerialData[clientIndex] = clientSerialData[clientIndex].substring(clientSerialData[clientIndex].length() - 1500);
  }
}

bool processServerIngress(int clientIndex, const uint8_t *buf, size_t readBytes) {
  String errorReason;
  if (!appendIngressChunk(SECURITY_SOURCE_TCP_SERVER, clientIndex, buf, readBytes, errorReason)) {
    return false;
  }

  String payload;
  while (getValidatedPayload(SECURITY_SOURCE_TCP_SERVER, clientIndex, payload, errorReason)) {
    handleValidatedServerPayload(clientIndex, payload);
  }

  return true;
}

// ==================== Client Mode ====================
void initClientMode() {
  if (debugMode) {
    Serial.println("Initializing client mode...");
  }

  pinMode(CONFIG_MODE_PIN, INPUT_PULLUP);
  if (digitalRead(CONFIG_MODE_PIN) == LOW) {
    if (debugMode) {
      Serial.println("Config mode triggered, entering WiFi config...");
    }
    startConfigMode();
    return;
  }

  wifiConnected = false;
  tcpConnected = false;
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);

  if (!hasConfiguredClientWiFi()) {
    startConfigMode();
  }
}

void connectToServer() {
  if (debugMode) {
    Serial.print("Connecting to protected server channel...");
  }

  if (tcpClient.connect(server_ip, server_port)) {
    tcpConnected = true;
    if (debugMode) {
      Serial.println(" Success");
    }

    String clientInfo = "CLIENT_ID:" + client_id + "|TIMESTAMP:" + String(millis());
    sendFramedPayloadToClient(tcpClient, clientInfo);
  } else {
    tcpConnected = false;
    if (debugMode) {
      Serial.println(" Failed");
    }
  }
}

void runClientMode() {
  static unsigned long lastConnectAttempt = 0;
  static unsigned long lastWiFiAttempt = 0;
  static unsigned long wifiConnectStart = 0;
  static bool wifiConnecting = false;
  static uint8_t wifiFailureCount = 0;
  static uint8_t tcpFailureCount = 0;
  const unsigned long reconnectionInterval = 5000;
  const unsigned long wifiConnectTimeout = 15000;

  if (!hasConfiguredClientWiFi()) {
    wifiConnected = false;
    tcpConnected = false;
    if (!inConfigMode) {
      startConfigMode();
    }
    return;
  }

  wl_status_t wifiStatus = WiFi.status();
  wifi_mode_t wifiMode = WiFi.getMode();
  bool wasWifiConnected = wifiConnected;

  if (wifiMode == WIFI_OFF) {
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    wifiStatus = WiFi.status();
    wifiMode = WiFi.getMode();
    wifiConnecting = false;
    lastWiFiAttempt = 0;
    if (debugMode) {
      Serial.println("WiFi radio restarted for client mode");
    }
  }

  if (!wifiConnected || wifiStatus != WL_CONNECTED) {
    wifiConnected = false;
    if (tcpConnected) {
      tcpClient.stop();
      clientTcpFrameBuffer = "";
      clearSecurityState(clientTcpSecurityState);
      tcpConnected = false;
    }

    if (!wifiConnecting && millis() - lastWiFiAttempt >= reconnectionInterval &&
        (wifiMode == WIFI_OFF || (int)wifiStatus == 254 || wifiStatus == WL_IDLE_STATUS || wifiStatus == WL_DISCONNECTED || wifiStatus == WL_NO_SSID_AVAIL || wifiStatus == WL_CONNECT_FAILED)) {
      wifiConnecting = true;
      wifiConnectStart = millis();
      lastWiFiAttempt = millis();
      WiFi.disconnect(false, false);
      WiFi.mode(WIFI_STA);
      WiFi.persistent(false);
      WiFi.begin(client_wifi_ssid, client_wifi_password);
    }

    if (wifiStatus == WL_CONNECTED) {
      wifiConnected = true;
      wifiConnecting = false;
      wifiFailureCount = 0;
      tcpFailureCount = 0;
      if (debugMode) {
        Serial.println("WiFi connected");
      }
    } else if (millis() - wifiConnectStart > wifiConnectTimeout) {
      wifiConnecting = false;
      if (wifiFailureCount < SECURITY_MAX_INVALID_ATTEMPTS) {
        wifiFailureCount++;
      }

      if (wifiFailureCount >= SECURITY_MAX_INVALID_ATTEMPTS) {
        WiFi.disconnect(false, false);
        WiFi.mode(WIFI_STA);
        WiFi.persistent(false);
        wifiFailureCount = 0;
        lastWiFiAttempt = millis();
      }
      return;
    }
  }

  if (wifiConnected && !wasWifiConnected) {
    lastConnectAttempt = 0;
  }

  if (!tcpConnected && wifiConnected) {
    if (millis() - lastConnectAttempt > reconnectionInterval) {
      connectToServer();
      lastConnectAttempt = millis();
      if (tcpConnected) {
        tcpFailureCount = 0;
      } else {
        if (tcpFailureCount < SECURITY_MAX_INVALID_ATTEMPTS) {
          tcpFailureCount++;
        }

        if (tcpFailureCount >= SECURITY_MAX_INVALID_ATTEMPTS) {
          WiFi.disconnect(false, false);
          wifiConnected = false;
          tcpFailureCount = 0;
        }
      }
    }
  }

  if (tcpConnected && tcpClient.available()) {
    size_t available = tcpClient.available();
    size_t toRead = min(available, (size_t)256);
    uint8_t buf[256];
    size_t readBytes = tcpClient.read(buf, toRead);

    if (!processClientIngress(buf, readBytes) && isSecuritySourceBlocked(SECURITY_SOURCE_TCP_CLIENT, -1)) {
      tcpClient.stop();
      tcpConnected = false;
    }
  }

  if (tcpConnected && !tcpClient.connected()) {
    if (debugMode) {
      Serial.println("TCP disconnected");
    }
    tcpClient.stop();
    clientTcpFrameBuffer = "";
    clearSecurityState(clientTcpSecurityState);
    tcpConnected = false;
  }
}

// ==================== Server Mode ====================
void initServerMode() {
  if (debugMode) {
    Serial.println("Initializing server mode...");
  }

  delay(200);

  WiFi.mode(WIFI_AP);

  esp_wifi_set_max_tx_power(40);

  IPAddress localIP(192, 168, 1, 1);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(localIP, gateway, subnet);

  if (WiFi.softAP(ap_ssid, ap_password)) {
    wifiConnected = true;
    if (debugMode) {
      Serial.println("WiFi AP started");
    }
    if (logToSD && sdCardReady) {
      saveServerSystemLog("WiFi AP started - protected profile active");
    }
  } else {
    wifiConnected = false;
    if (debugMode) {
      Serial.println("WiFi AP failed to start");
    }
    if (logToSD && sdCardReady) {
      saveServerSystemLog("WiFi AP failed to start");
    }
  }

  tcpServer.begin();
  if (debugMode) {
    Serial.println("TCP server started");
    Serial.println("  Listen port: " + String(server_listen_port));
  }
  if (logToSD && sdCardReady) {
    saveServerSystemLog("TCP server started on port " + String(server_listen_port));
  }

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (serverClients[i]) {
      serverClients[i].stop();
    }
    clientSerialData[i] = "";
    clientLineBuffer[i] = "";
    connectedClientIds[i] = "";
    clientLastSeenMillis[i] = 0;
    clientSerialData[i].reserve(2200);
    clientLineBuffer[i].reserve(512);
  }
}

void runServerMode() {
  static unsigned long lastServerRecoveryAttempt = 0;
  static uint8_t serverRecoveryFailures = 0;

  if (!isServerAccessPointHealthy()) {
    wifiConnected = false;
    if (millis() - lastServerRecoveryAttempt >= 10000UL) {
      lastServerRecoveryAttempt = millis();
      if (serverRecoveryFailures < SECURITY_MAX_INVALID_ATTEMPTS) {
        serverRecoveryFailures++;
      }

      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_OFF);
      delay(100);
      initServerMode();
    }
    return;
  }

  wifiConnected = true;
  serverRecoveryFailures = 0;

  WiFiClient newClient = tcpServer.available();
  if (newClient) {
    IPAddress newIP = newClient.remoteIP();
    int replaceSlot = -1;
    int freeSlot = -1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (serverClients[i] && serverClients[i].connected()) {
        if (serverClients[i].remoteIP() == newIP) {
          replaceSlot = i;
          break;
        }
      } else if (freeSlot < 0) {
        freeSlot = i;
      }
    }

    if (replaceSlot >= 0) {
      serverClients[replaceSlot].stop();
      serverClients[replaceSlot] = newClient;
      clientSerialData[replaceSlot] = "// Reconnected\n";
      connectedClientIds[replaceSlot] = "";
      clientLastSeenMillis[replaceSlot] = millis();
      clientSerialData[replaceSlot].reserve(2200);
      clientLineBuffer[replaceSlot].reserve(512);
      if (logToSD && sdCardReady) {
        saveServerSystemLog("Client reconnected: " + maskIpAddress(newIP));
      }
    } else if (freeSlot >= 0) {
      serverClients[freeSlot] = newClient;
      clientSerialData[freeSlot] = "// Serial data log\n";
      connectedClientIds[freeSlot] = "";
      clientLastSeenMillis[freeSlot] = millis();
      clientSerialData[freeSlot].reserve(2200);
      clientLineBuffer[freeSlot].reserve(512);
      if (logToSD && sdCardReady) {
        saveServerSystemLog("New client connected: " + maskIpAddress(newIP));
      }
    } else {
      sendFramedPayloadToClient(newClient, "SERVER:BUSY");
      newClient.stop();
      if (logToSD && sdCardReady) {
        saveServerSystemLog("Client connection rejected: server limit reached - " + maskIpAddress(newIP));
      }
    }
  }

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (serverClients[i] && serverClients[i].connected()) {
      size_t available = serverClients[i].available();
      if (available > 0) {
        size_t toRead = min(available, (size_t)256);
        uint8_t buf[256];
        size_t readBytes = serverClients[i].read(buf, toRead);

        if (!processServerIngress(i, buf, readBytes) && isSecuritySourceBlocked(SECURITY_SOURCE_TCP_SERVER, i)) {
          IPAddress blockedIp = serverClients[i].remoteIP();
          serverClients[i].stop();
          clientLineBuffer[i] = "";
          clientLastSeenMillis[i] = millis();
          clientSerialData[i] = "// Blocked by security policy\n";
          if (logToSD && sdCardReady) {
            saveServerSystemLog("Client blocked due to invalid frames: " + maskIpAddress(blockedIp));
          }
        }
      }
    } else if (serverClients[i]) {
      IPAddress clientIP = serverClients[i].remoteIP();
      serverClients[i].stop();
      clientLineBuffer[i] = "";
      clientLastSeenMillis[i] = millis();
      if (logToSD && sdCardReady) {
        saveServerSystemLog("Client disconnected: " + maskIpAddress(clientIP));
      }
    }
  }

  yield();
}

String parseClientId(String data) {
  int idStart = data.indexOf("CLIENT_ID:");
  if (idStart >= 0) {
    int idEnd = data.indexOf("|", idStart);
    if (idEnd >= 0) {
      return data.substring(idStart + 10, idEnd);
    }
  }
  return "UNKNOWN";
}

int getConnectedClientCount() {
  int count = 0;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (serverClients[i] && serverClients[i].connected()) {
      count++;
    }
  }
  return count;
}
