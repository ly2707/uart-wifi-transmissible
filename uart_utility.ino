// ==================== UART Transparent Mode ====================

#define UART_BUFFER_SIZE 1024

// ==================== LED Multi-Flash (非阻塞) ====================
bool multiFlashActive = false;
CRGB multiFlashColor = CRGB(0, 0, 0);
int multiFlashTimes = 0;           // 总闪烁次数
int multiFlashCount = 0;           // 已完成闪烁次数
unsigned long multiFlashInterval = 0; // on/off 切换间隔(ms)
unsigned long multiFlashLastToggle = 0;
bool multiFlashOnState = false;

void requestMultiFlash(CRGB color, int times, unsigned long interval) {
  if (times <= 0) return;
  previousLEDState = currentLEDState;
  currentLEDState = LED_MULTI_FLASH;
  multiFlashActive = true;
  multiFlashColor = color;
  multiFlashTimes = times;
  multiFlashCount = 0;
  multiFlashInterval = interval;
  multiFlashLastToggle = millis();
  multiFlashOnState = true;
  setLED(color);
}

bool isATPlusCandidate(const String &buffer) {
  if (buffer.length() > 3) {
    return false;
  }

  if (buffer.length() >= 1 && buffer[0] != 'A' && buffer[0] != 'a') {
    return false;
  }

  if (buffer.length() >= 2 && buffer[1] != 'T' && buffer[1] != 't') {
    return false;
  }

  if (buffer.length() == 3 && buffer[2] != '+') {
    return false;
  }

  return true;
}

bool hasATPlusPrefix(const String &buffer) {
  return buffer.length() >= 3 &&
         (buffer[0] == 'A' || buffer[0] == 'a') &&
         (buffer[1] == 'T' || buffer[1] == 't') &&
         buffer[2] == '+';
}

void handleUART2ToDebug() {
  // Handle UART2 receive from ring buffer, send to debug serial
  // This uses the interrupt-driven ring buffer from uart_interrupt.ino
  handleHighSpeedUART();
}

void forwardValidatedUSBPayload(const String &payload) {
  if (!sendValidatedPayloadToUART(UART_NUM_2, payload)) {
    recordSecurityFailure(usbSecurityState);
    return;
  }

  if (currentMode == MODE_SERVER && selectedClientIndex >= 0) {
    for (unsigned int i = 0; i < payload.length(); i++) {
      queueTCPWrite((uint8_t)payload[i]);
    }
  }

  if (currentMode == MODE_CLIENT && logToSD && sdCardReady) {
    enqueueSDLog(payload, client_id, false);
  }
}

void processUSBTransparentBytes(const uint8_t *data, size_t len) {
  String errorReason;
  if (!appendIngressChunk(SECURITY_SOURCE_USB, -1, data, len, errorReason)) {
    return;
  }

  String payload;
  while (getValidatedPayload(SECURITY_SOURCE_USB, -1, payload, errorReason)) {
    forwardValidatedUSBPayload(payload);
  }
}

void handleUSBSerial() {
  static String commandBuffer = "";
  
  while (Serial.available()) {
    char incoming = Serial.read();

    bool collectingAT = commandBuffer.length() > 0 && isATPlusCandidate(commandBuffer);

    if (commandBuffer.length() == 0 && (incoming == 'A' || incoming == 'a')) {
      commandBuffer += incoming;
      continue;
    }

    if (collectingAT && incoming != '\r' && incoming != '\n') {
      commandBuffer += incoming;
      if (!isATPlusCandidate(commandBuffer) && !hasATPlusPrefix(commandBuffer)) {
        processUSBTransparentBytes((const uint8_t *)commandBuffer.c_str(), commandBuffer.length());
        commandBuffer = "";
      }
      continue;
    }

    if (hasATPlusPrefix(commandBuffer)) {
      if (incoming == '\n' || incoming == '\r') {
        String command = commandBuffer;
        command.trim();
        handleCommand(command);
        commandBuffer = "";
        continue;
      }

      commandBuffer += incoming;
      if (commandBuffer.length() > 64) {
        recordSecurityFailure(usbSecurityState);
        commandBuffer = "";
      }
      continue;
    }

    if (commandBuffer.length() > 0) {
      processUSBTransparentBytes((const uint8_t *)commandBuffer.c_str(), commandBuffer.length());
      commandBuffer = "";
    }

    if (commandBuffer.length() > UART_BUFFER_SIZE) {
      recordSecurityFailure(usbSecurityState);
      commandBuffer = "";
    }

    processUSBTransparentBytes((const uint8_t *)&incoming, 1);
  }
}

String formatClientData(String data) {
  // Format client data, add timestamp etc.
  unsigned long timestamp = millis();
  return "[" + String(timestamp) + "] " + data;
}

// ==================== Command Parser ====================
void handleCommand(String command) {
  command.trim();
  
  if (command.length() == 0) return;
  
  if (command == "AT+HELP" || command == "AT+?") {
    printHelp();
  } else if (command == "AT+SWITCH") {
    switchMode();
  } else if (command == "AT+RESET") {
    resetToDefault();
  } else if (command == "AT+STATUS") {
    wl_status_t wifiStatus = WiFi.status();
    wifi_mode_t wifiMode = WiFi.getMode();
    String tcpStatusText = currentMode == MODE_CLIENT
      ? String(tcpConnected ? "Connected" : "Disconnected")
      : String(getConnectedClientCount() > 0 ? "Clients Connected" : "Listening");

    Serial.println("\nSystem Status:");
    Serial.println("  Mode: " + String(currentMode == MODE_CLIENT ? "Client" : "Server"));
    Serial.println("  Config Mode: " + String(inConfigMode ? "Yes" : "No"));
    Serial.println("  RAW Pass-through: " + String(rawTransmitMode ? "Enabled" : "Disabled"));
    Serial.println("  WiFi: " + String(wifiConnected ? "Connected" : "Disconnected"));
    Serial.println("  WiFi Mode: " + getWiFiModeName(wifiMode));
    if (currentMode == MODE_CLIENT) {
      Serial.println("  WiFi Link: " + getWiFiStatusName(wifiStatus) + " (" + String((int)wifiStatus) + ")");
      Serial.println("  Target SSID: " + maskSensitiveValue(client_wifi_ssid));
      Serial.println("  Target Server: " + String(server_ip) + ":" + String(server_port));
      if (wifiStatus == WL_CONNECTED) {
        Serial.println("  Local IP: " + WiFi.localIP().toString());
        Serial.println("  Gateway: " + WiFi.gatewayIP().toString());
        Serial.println("  RSSI: " + String(WiFi.RSSI()) + " dBm");
      }
    } else {
      Serial.println("  AP Health: " + String(isServerAccessPointHealthy() ? "Healthy" : "Fault"));
      Serial.println("  AP SSID: " + maskSensitiveValue(ap_ssid));
      Serial.println("  AP IP: " + WiFi.softAPIP().toString());
      Serial.println("  Stations: " + String(WiFi.softAPgetStationNum()));
      Serial.println("  TCP Listen: " + String(server_listen_port));
      Serial.println("  TCP Clients: " + String(getConnectedClientCount()));
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (serverClients[i] && serverClients[i].connected()) {
          String clientName = connectedClientIds[i].length() > 0 ? connectedClientIds[i] : ("client_" + String(i));
          Serial.println("    Slot " + String(i) + ": " + clientName + " @ " + serverClients[i].remoteIP().toString());
        }
      }
    }
    Serial.println("  TCP: " + tcpStatusText);
    Serial.println("  SD Card: " + String(sdCardReady ? "Ready" : "Not Ready"));
    Serial.println("  Log: " + logFileName + " (" + String(logCount) + " entries)");
    Serial.println("  Log Timestamp: " + String(logWithTimestamp ? "ON" : "OFF"));
    Serial.println("  Battery: " + String(batteryVoltage) + "V" + (lowBattery ? " (Low)" : ""));
    Serial.println("  UART2 Baud: " + String(uart2BaudRate));
    Serial.println("  UART1 Baud: " + String(uart1BaudRate));
  } else if (command.startsWith("AT+LOGNAME ")) {
    String newName = command.substring(10);
    logFileName = newName;
    saveConfigToEEPROM();
    Serial.println("OK Log filename set to: " + logFileName);
  } else if (command == "AT+LOGTIME=ON") {
    logWithTimestamp = true;
    saveConfigToEEPROM();
    Serial.println("OK Log timestamp enabled");
  } else if (command == "AT+LOGTIME=OFF") {
    logWithTimestamp = false;
    saveConfigToEEPROM();
    Serial.println("OK Log timestamp disabled");
  } else if (command == "AT+LOGTIME?") {
    Serial.println("Log timestamp: " + String(logWithTimestamp ? "ON" : "OFF"));
  } else if (command.startsWith("AT+BAUD ")) {
    String baudStr = command.substring(8);
    unsigned long newBaud = baudStr.toInt();
    if (newBaud >= 9600 && newBaud <= 921600) {
      uart2BaudRate = newBaud;
      initUARTInterrupt(true);
      saveConfigToEEPROM();
      Serial.println("OK UART2 baud rate set to: " + String(uart2BaudRate));
    } else {
      Serial.println("X Invalid baud rate");
    }
  } else if (command.startsWith("AT+BAUD1 ")) {
    String baudStr = command.substring(9);
    unsigned long newBaud = baudStr.toInt();
    if (newBaud >= 9600 && newBaud <= 921600) {
      uart1BaudRate = newBaud;
      initUART1Interrupt(true);
      saveConfigToEEPROM();
      Serial.println("OK UART1 baud rate set to: " + String(uart1BaudRate));
    } else {
      Serial.println("X Invalid baud rate");
    }
  } else if (command == "AT+DEBUG=ON") {
    debugMode = true;
    saveConfigToEEPROM();
    Serial.println("OK Debug mode enabled");
  } else if (command == "AT+DEBUG=OFF") {
    debugMode = false;
    saveConfigToEEPROM();
    Serial.println("OK Debug mode disabled");
  } else if (command == "AT+RESTART") {
    Serial.println("System will restart...");
    ESP.restart();
  } else if (command == "AT+RAW") {
    rawTransmitMode = true;
    Serial.println("OK RAW mode enabled - transparent TCP/UART payload filtering disabled");
  } else if (command == "AT+EXIT" || command == "+++") {
    if (rawTransmitMode) {
      rawTransmitMode = false;
      Serial.println("OK RAW mode disabled - secure filtering restored");
    } else {
      Serial.println("OK Not in RAW mode");
    }
  } else if (command == "AT+POWER=ON") {
    powerOn();
  } else if (command == "AT+POWER=OFF") {
    powerOff();
  } else if (command == "AT+POWER=TRIGGER") {
    triggerShutdown();
  } else if (command == "AT+RESET=CPU") {
    resetCPU();
  } else if (command == "AT+CONFIG") {
    // Enter config mode
    startConfigMode();
  } else if (command == "AT+EXITCONFIG") {
    // Exit config mode
    exitConfigMode();
  } else if (command == "AT+SELECT") {
    // Display selectable client list
    listSelectableClients();
  } else if (command.startsWith("AT+SELECT ")) {
    // Select client for transparent mode
    String indexStr = command.substring(10);
    int index = indexStr.toInt();
    selectClient(index);
  } else if (command == "AT+CPU") {
    // Display CPU and memory usage
    Serial.println("\n=== CPU & Memory Status ===");
    Serial.println("  CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
    Serial.println("  CPU Cores: " + String(ESP.getChipCores()));
    Serial.println("  Chip Model: " + String(ESP.getChipModel()));
    Serial.println("  Chip Revision: " + String(ESP.getChipRevision()));
    Serial.println("  Flash Size: " + String(ESP.getFlashChipSize() / (1024 * 1024)) + " MB");
    Serial.println("  Flash Speed: " + String(ESP.getFlashChipSpeed() / 1000000) + " MHz");
    
    size_t freeHeap = ESP.getFreeHeap();
    size_t totalHeap = ESP.getHeapSize();
    Serial.println("  Heap Memory: " + String(freeHeap) + " / " + String(totalHeap) + " bytes");
    Serial.println("  Heap Usage: " + String((totalHeap - freeHeap) * 100 / totalHeap) + "%");
    
    Serial.println("  Free Sketch Space: " + String(ESP.getFreeSketchSpace() / 1024) + " KB");
    
    #ifdef ARDUINO_ESP32S3_DEV
    Serial.println("  PSRAM Size: " + String(ESP.getPsramSize() / (1024 * 1024)) + " MB");
    Serial.println("  Free PSRAM: " + String(ESP.getFreePsram() / 1024) + " KB");
    #endif
    
    Serial.println("  Uptime: " + String(millis() / 1000) + " seconds");
    Serial.println("  WiFi Mode: " + String(WiFi.getMode() == WIFI_AP ? "AP" : (WiFi.getMode() == WIFI_STA ? "STA" : "AP+STA")));
  } else if (command == "AT+CLIENTS") {
    if (currentMode == MODE_SERVER) {
      int count = getConnectedClientCount();
      Serial.println("\nConnected clients: " + String(count));
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (serverClients[i] && serverClients[i].connected()) {
          String selected = (selectedClientIndex == i) ? " [Selected]" : "";
          Serial.println("  Client " + String(i) + ": " + maskIpAddress(serverClients[i].remoteIP()) + selected);
        }
      }
      if (selectedClientIndex >= 0) {
        Serial.println("\nCurrent transparent: Client " + String(selectedClientIndex));
      }
    } else {
      Serial.println("X This command is only available in server mode");
    }
  } else {
    Serial.println("X Unsupported command");
  }
}

// ==================== Utility Functions ====================
void setLED(CRGB color) {
  leds[0] = color;
  FastLED.show();
}

void updateLEDStatus() {
  // 如果有多次闪烁请求优先处理（非阻塞）
  if (multiFlashActive && currentLEDState == LED_MULTI_FLASH) {
    if (millis() - multiFlashLastToggle >= multiFlashInterval) {
      if (multiFlashOnState) {
        setLED(CRGB(0, 0, 0));
        multiFlashOnState = false;
        multiFlashCount++;
      } else {
        setLED(multiFlashColor);
        multiFlashOnState = true;
      }
      multiFlashLastToggle = millis();
    }
    if (multiFlashCount >= multiFlashTimes) {
      multiFlashActive = false;
      currentLEDState = previousLEDState;
      lastLEDToggle = millis();
    }
    return;
  }

  // Handle one-shot single flash (非阻塞)，优先于常规状态
  if (currentLEDState == LED_SINGLE_FLASH) {
    if (millis() - lastLEDToggle < 100) {
      setLED(CRGB(255, 255, 255));
      return;
    }
    currentLEDState = previousLEDState;
    lastLEDToggle = millis();
  }

  // Update LED based on system status
  LEDState newState = LED_OFF;

  // Config mode takes priority
  if (inConfigMode) {
    newState = LED_SOLID_YELLOW;
  } else if (lowBattery) {
    newState = LED_LOW_BATTERY;
  } else if (sdCardError) {
    newState = LED_SD_ERROR;
  } else if (currentMode == MODE_CLIENT) {
    if (tcpConnected) {
      newState = LED_SOLID_GREEN;
    } else if (wifiConnected) {
      newState = LED_SOLID_YELLOW;
    } else {
      newState = LED_FAST_BLINK;
    }
  } else { // Server mode
    if (getConnectedClientCount() > 0) {
      newState = LED_SOLID_GREEN;
    } else {
      newState = LED_SLOW_BLINK;
    }
  }

  if (newState != currentLEDState) {
    currentLEDState = newState;
    lastLEDToggle = millis();
    breatheValue = 0;
  }

  switch (currentLEDState) {
    case LED_OFF:
      setLED(CRGB(0, 0, 0));
      break;
    case LED_FAST_BLINK:
      if (millis() - lastLEDToggle > 200) {
        ledBlinkState = !ledBlinkState;
        setLED(ledBlinkState ? CRGB(0, 0, 255) : CRGB(0, 0, 0));
        lastLEDToggle = millis();
      }
      break;
    case LED_SLOW_BLINK:
      if (millis() - lastLEDToggle > 1000) {
        ledBlinkState = !ledBlinkState;
        setLED(ledBlinkState ? CRGB(0, 255, 255) : CRGB(0, 0, 0));
        lastLEDToggle = millis();
      }
      break;
    case LED_BREATHE:
      if (millis() - lastLEDToggle > 20) {
        breatheValue += 5;
        if (breatheValue > 255) breatheValue = 0;
        // 低功耗呼吸为蓝色
        setLED(CRGB(0, 0, breatheValue));
        lastLEDToggle = millis();
      }
      break;
    case LED_SOLID_GREEN:
      setLED(CRGB(0, 255, 0));
      break;
    case LED_SOLID_YELLOW:
      setLED(CRGB(255, 255, 0));
      break;
    case LED_SOLID_RED:
      setLED(CRGB(255, 0, 0));
      break;
    case LED_SINGLE_FLASH:
      // handled above as non-blocking
      break;
    case LED_SD_ERROR:
      if (millis() - lastLEDToggle > 300) {
        ledBlinkState = !ledBlinkState;
        setLED(ledBlinkState ? CRGB(255, 255, 255) : CRGB(0, 0, 0));
        lastLEDToggle = millis();
      }
      break;
    case LED_LOW_BATTERY:
      if (millis() - lastLEDToggle > 500) {
        ledBlinkState = !ledBlinkState;
        setLED(ledBlinkState ? CRGB(255, 0, 0) : CRGB(0, 0, 0));
        lastLEDToggle = millis();
      }
      break;
    default:
      break;
  }
}

void flashLED() {
  previousLEDState = currentLEDState;
  currentLEDState = LED_SINGLE_FLASH;
}

String getDateString() {
  // Return system start time for unique log filename
  return String(systemStartTime / 1000);
}

// ==================== Power Control Functions ====================
void powerOn() {
  Serial.println("-> Power ON operation...");
  // Pull GPIO low for 1 second
  digitalWrite(POWER_CONTROL_PIN, LOW);
  delay(1000);
  digitalWrite(POWER_CONTROL_PIN, HIGH);
  Serial.println("OK Power ON operation complete");
}

void powerOff() {
  Serial.println("-> Power OFF operation...");
  // Pull GPIO low for 1 second
  digitalWrite(POWER_CONTROL_PIN, LOW);
  delay(1000);
  digitalWrite(POWER_CONTROL_PIN, HIGH);
  Serial.println("OK Power OFF operation complete");
}

void triggerShutdown() {
  Serial.println("-> Trigger shutdown hint...");
  // Pull GPIO low for 1 second
  digitalWrite(POWER_CONTROL_PIN, LOW);
  delay(1000);
  digitalWrite(POWER_CONTROL_PIN, HIGH);
  Serial.println("OK Shutdown hint triggered");
}

void resetCPU() {
  Serial.println("-> Reset operation...");
  // Pull GPIO low for 1 second
  digitalWrite(RESET_CONTROL_PIN, LOW);
  delay(1000);
  digitalWrite(RESET_CONTROL_PIN, HIGH);
  Serial.println("OK Reset operation complete");
}

void printHelp() {
  Serial.println("\nAT Command Help:");
  Serial.println("  AT+HELP/AT+?    - Show this help");
  Serial.println("  AT+SWITCH       - Switch working mode");
  Serial.println("  AT+RESET        - Reset to default settings");
  Serial.println("  AT+STATUS       - Show system status");
  Serial.println("  AT+CPU          - Show CPU & memory status");
  Serial.println("  AT+CLIENTS      - Show connected clients (server mode)");
  Serial.println("  AT+SELECT       - Show selectable client list");
  Serial.println("  AT+SELECT <id>  - Select client for transparent mode (-1 to cancel)");
  Serial.println("  AT+LOGNAME <name> - Set log filename");
  Serial.println("  AT+LOGTIME=ON/OFF - Enable/disable timestamp in log");
  Serial.println("  AT+BAUD <rate>  - Set UART2 baud rate");
  Serial.println("  AT+BAUD1 <rate> - Set UART1 baud rate (IO19/IO20)");
  Serial.println("  AT+DEBUG=ON/OFF - Enable/disable debug mode");
  Serial.println("  AT+RESTART      - Restart system");
  Serial.println("  AT+RAW          - Enable RAW transparent mode");
  Serial.println("  AT+EXIT / +++   - Disable RAW mode and restore secure filtering");
  Serial.println("  AT+CONFIG       - Enter WiFi config mode (non-blocking)");
  Serial.println("  AT+EXITCONFIG   - Exit WiFi config mode");
  Serial.println("  Transparent data: RAW pass-through is enabled by default for TCP/UART traffic");
  Serial.println("  AT+POWER=ON     - Power ON operation");
  Serial.println("  AT+POWER=OFF    - Power OFF operation");
  Serial.println("  AT+POWER=TRIGGER - Trigger shutdown hint");
  Serial.println("  AT+RESET=CPU    - Reset CPU");
  Serial.println("\nLong press button 5s: Reset to default");
  Serial.println("Short press button 2s: Switch mode");
}
