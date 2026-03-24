// ==================== Configuration Management ====================
void loadConfigFromEEPROM() {
  // Load mode
  int savedMode = EEPROM.read(EEPROM_MODE_ADDR);
  if (savedMode == MODE_CLIENT || savedMode == MODE_SERVER) {
    currentMode = savedMode;
  } else {
    currentMode = MODE_CLIENT;
    saveModeToEEPROM();
  }
  
  // Load UART2 baud rate
  EEPROM.get(EEPROM_UART2_BAUD_ADDR, uart2BaudRate);
  if (uart2BaudRate < 9600 || uart2BaudRate > 921600) {
    uart2BaudRate = DEFAULT_UART2_BAUD;
  }
  
  // Load client ID (will be overwritten by chip ID based ID)
  char idBuffer[32];
  memset(idBuffer, 0, sizeof(idBuffer));
  for (int i = 0; i < 31; i++) {
    idBuffer[i] = EEPROM.read(EEPROM_CLIENT_ID_ADDR + i);
    if (idBuffer[i] == 0 || idBuffer[i] == 255) break;
  }
  
  Serial.println("Config loaded from EEPROM");
}

void saveModeToEEPROM() {
  EEPROM.write(EEPROM_MODE_ADDR, currentMode);
  EEPROM.commit();
}

void saveConfigToEEPROM() {
  // Save UART2 baud rate
  EEPROM.put(EEPROM_UART2_BAUD_ADDR, uart2BaudRate);
  
  // Save client ID
  for (int i = 0; i < client_id.length() && i < 32; i++) {
    EEPROM.write(EEPROM_CLIENT_ID_ADDR + i, client_id[i]);
  }
  EEPROM.write(EEPROM_CLIENT_ID_ADDR + client_id.length(), 0);
  
  EEPROM.commit();
  Serial.println("Config saved to EEPROM");
}

// ==================== Mode Switch ====================
void switchMode() {
  currentMode = (currentMode == MODE_CLIENT) ? MODE_SERVER : MODE_CLIENT;
  saveModeToEEPROM();
  
  Serial.println("\n*** Mode Switch!");
  Serial.println("New mode: " + String(currentMode == MODE_CLIENT ? "Client" : "Server"));
  Serial.println("System will restart...");
  
  // LED flash notification
  for (int i = 0; i < 5; i++) {
    setLED(CRGB(255, 255, 255));
    delay(100);
    setLED(CRGB(0, 0, 0));
    delay(100);
  }
  
  delay(500);
  ESP.restart();
}

void resetToDefault() {
  currentMode = MODE_CLIENT;
  uart2BaudRate = DEFAULT_UART2_BAUD;
  saveModeToEEPROM();
  saveConfigToEEPROM();
  
  Serial.println("\n*** Reset to Default!");
  Serial.println("System will restart...");
  
  // LED flash notification
  for (int i = 0; i < 10; i++) {
    setLED(CRGB(255, 0, 0));
    delay(100);
    setLED(CRGB(0, 0, 0));
    delay(100);
  }
  
  delay(500);
  ESP.restart();
}

// ==================== Button Handling ====================
void handleButton() {
  int buttonState = digitalRead(BUTTON_PIN);
  
  if (buttonState == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      buttonPressTime = millis();
    }
  } else {
    if (buttonPressed) {
      unsigned long pressDuration = millis() - buttonPressTime;
      
      if (pressDuration >= LONG_PRESS) {
        Serial.println("Long press detected (" + String(pressDuration) + "ms) - Reset to default");
        resetToDefault();
      } else if (pressDuration >= 50) {
        Serial.println("Short press detected (" + String(pressDuration) + "ms) - Switch mode");
        switchMode();
      }
      
      buttonPressed = false;
    }
  }
}
