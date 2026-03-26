// ==================== UART Transparent Mode ====================

#define UART_BUFFER_SIZE 1024

void handleUART2ToDebug() {
  // Handle UART2 receive from ring buffer, send to debug serial
  // This uses the interrupt-driven ring buffer from uart_interrupt.ino
  handleHighSpeedUART();
}

void handleUSBSerial() {
  while (Serial.available()) {
    char incoming = Serial.read();
    
    if (incoming == '\n' || incoming == '\r') {
      if (usbRxBuffer.length() > 0) {
        String command = usbRxBuffer;
        usbRxBuffer = "";
        handleCommand(command);
      }
    } else if (incoming >= 32 || incoming == '\t' || incoming == 27) {
      usbRxBuffer += incoming;
      if (usbRxBuffer.length() > UART_BUFFER_SIZE) {
        usbRxBuffer = "";
        Serial.println("X Input buffer overflow");
      }
    }
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
    Serial.println("\nSystem Status:");
    Serial.println("  Mode: " + String(currentMode == MODE_CLIENT ? "Client" : "Server"));
    Serial.println("  Config Mode: " + String(inConfigMode ? "Yes" : "No"));
    Serial.println("  WiFi: " + String(wifiConnected ? "Connected" : "Disconnected"));
    Serial.println("  TCP: " + String(tcpConnected ? "Connected" : "Disconnected"));
    Serial.println("  SD Card: " + String(sdCardReady ? "Ready" : "Not Ready"));
    Serial.println("  Log: " + logFileName + " (" + String(logCount) + " entries)");
    Serial.println("  Battery: " + String(batteryVoltage) + "V" + (lowBattery ? " (Low)" : ""));
  } else if (command.startsWith("AT+LOGNAME ")) {
    String newName = command.substring(10);
    logFileName = newName;
    saveConfigToEEPROM();
    Serial.println("OK Log filename set to: " + logFileName);
  } else if (command.startsWith("AT+BAUD ")) {
    String baudStr = command.substring(8);
    unsigned long newBaud = baudStr.toInt();
    if (newBaud >= 9600 && newBaud <= 921600) {
      uart2BaudRate = newBaud;
      Serial2.begin(uart2BaudRate, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
      initUARTInterrupt();
      saveConfigToEEPROM();
      Serial.println("OK UART2 baud rate set to: " + String(uart2BaudRate));
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
    Serial.println("OK RAW mode enabled - data sent directly to UART2");
    Serial.println("Type +++ or ESC to exit");
  } else if (command == "AT+EXIT" || command == "+++") {
    if (rawTransmitMode) {
      rawTransmitMode = false;
      Serial.println("OK RAW mode disabled - back to command mode");
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
  } else if (command == "AT+CLIENTS") {
    if (currentMode == MODE_SERVER) {
      int count = getConnectedClientCount();
      Serial.println("\nConnected clients: " + String(count));
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (serverClients[i] && serverClients[i].connected()) {
          String selected = (selectedClientIndex == i) ? " [Selected]" : "";
          Serial.println("  Client " + String(i) + ": " + serverClients[i].remoteIP().toString() + selected);
        }
      }
      if (selectedClientIndex >= 0) {
        Serial.println("\nCurrent transparent: Client " + String(selectedClientIndex));
      }
    } else {
      Serial.println("X This command is only available in server mode");
    }
  } else {
    // Unknown command, send to UART2
    Serial2.println(command);
    Serial.println("[Sent] " + command);
  }
}

// ==================== Utility Functions ====================
void setLED(CRGB color) {
  leds[0] = color;
  FastLED.show();
}

void updateLEDStatus() {
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
        setLED(ledBlinkState ? CRGB(0, 255, 0) : CRGB(0, 0, 0));
        lastLEDToggle = millis();
      }
      break;
    case LED_BREATHE:
      if (millis() - lastLEDToggle > 20) {
        breatheValue += 5;
        if (breatheValue > 255) breatheValue = 0;
        setLED(CRGB(0, breatheValue, 0));
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
      setLED(CRGB(255, 255, 255));
      delay(100);
      setLED(CRGB(0, 0, 0));
      currentLEDState = previousLEDState;
      break;
    case LED_SD_ERROR:
      if (millis() - lastLEDToggle > 300) {
        ledBlinkState = !ledBlinkState;
        setLED(ledBlinkState ? CRGB(255, 0, 0) : CRGB(0, 0, 0));
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
  // Pull GPIO low for 1 second in power-off mode
  digitalWrite(POWER_CONTROL_PIN, LOW);
  delay(1000);
  digitalWrite(POWER_CONTROL_PIN, HIGH);
  Serial.println("OK Power ON operation complete");
}

void powerOff() {
  Serial.println("-> Power OFF operation...");
  // Pull GPIO low for 5 seconds to force power off
  Serial.println("!! Executing power off, pulling GPIO low for 5 seconds...");
  digitalWrite(POWER_CONTROL_PIN, LOW);
  delay(5000);
  digitalWrite(POWER_CONTROL_PIN, HIGH);
  Serial.println("OK Power OFF operation complete");
}

void triggerShutdown() {
  Serial.println("-> Trigger shutdown hint...");
  // Pull low for 1 second as button hint
  digitalWrite(POWER_CONTROL_PIN, LOW);
  delay(1000);
  digitalWrite(POWER_CONTROL_PIN, HIGH);
  Serial.println("OK Shutdown hint triggered");
}

void resetCPU() {
  Serial.println("-> Reset operation...");
  // Pull low to reset
  digitalWrite(RESET_CONTROL_PIN, LOW);
  delay(100);
  digitalWrite(RESET_CONTROL_PIN, HIGH);
  Serial.println("OK Reset operation complete");
}

void printHelp() {
  Serial.println("\nAT Command Help:");
  Serial.println("  AT+HELP/AT+?    - Show this help");
  Serial.println("  AT+SWITCH       - Switch working mode");
  Serial.println("  AT+RESET        - Reset to default settings");
  Serial.println("  AT+STATUS       - Show system status");
  Serial.println("  AT+CLIENTS      - Show connected clients (server mode)");
  Serial.println("  AT+SELECT       - Show selectable client list");
  Serial.println("  AT+SELECT <id>  - Select client for transparent mode (-1 to cancel)");
  Serial.println("  AT+LOGNAME <name> - Set log filename");
  Serial.println("  AT+BAUD <rate>  - Set UART2 baud rate");
  Serial.println("  AT+DEBUG=ON/OFF - Enable/disable debug mode");
  Serial.println("  AT+RESTART      - Restart system");
  Serial.println("  AT+CONFIG       - Enter WiFi config mode (non-blocking)");
  Serial.println("  AT+EXITCONFIG   - Exit WiFi config mode");
  Serial.println("  AT+RAW          - Enter raw transparent mode (AT+EXIT to exit)");
  Serial.println("  AT+POWER=ON     - Power ON operation");
  Serial.println("  AT+POWER=OFF    - Power OFF operation");
  Serial.println("  AT+POWER=TRIGGER - Trigger shutdown hint");
  Serial.println("  AT+RESET=CPU    - Reset CPU");
  Serial.println("  Other commands  - Send to UART2");
  Serial.println("\nLong press button 5s: Reset to default");
  Serial.println("Short press button 2s: Switch mode");
}
