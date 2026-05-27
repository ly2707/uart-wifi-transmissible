// ==================== Configuration Management ====================
const char DEFAULT_SERVER_AP_SSID[] = "UART_SERVER";
const char DEFAULT_SERVER_AP_PASSWORD[] = "12345678";

bool isSafeConfigCharacter(char value) {
  return value >= 0x20 && value <= 0x7E && value != '<' && value != '>' && value != '"' && value != '\\';
}

bool copyStringToBuffer(const String &value, char *target, size_t targetSize) {
  if (target == NULL || targetSize == 0 || value.length() >= targetSize) {
    return false;
  }

  memset(target, 0, targetSize);
  for (unsigned int i = 0; i < value.length(); i++) {
    target[i] = value[i];
  }
  target[value.length()] = '\0';
  return true;
}

void writeEEPROMString(int address, const char *value, size_t capacity) {
  for (size_t i = 0; i < capacity; i++) {
    char nextValue = 0;
    if (value != NULL && value[i] != '\0') {
      nextValue = value[i];
    }
    EEPROM.write(address + i, nextValue);
  }
}

void readEEPROMString(int address, char *buffer, size_t capacity) {
  if (buffer == NULL || capacity == 0) {
    return;
  }

  memset(buffer, 0, capacity);
  for (size_t i = 0; i < capacity - 1; i++) {
    uint8_t rawValue = EEPROM.read(address + i);
    if (rawValue == 0 || rawValue == 0xFF) {
      break;
    }

    char value = (char)rawValue;
    if (!isSafeConfigCharacter(value) && value != ' ') {
      buffer[0] = '\0';
      return;
    }

    buffer[i] = value;
  }
  buffer[capacity - 1] = '\0';
}

String buildDeviceToken() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i += 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xFF) << i;
  }

  String token = String(chipId, HEX);
  token.toUpperCase();
  return token;
}

void applyDefaultClientId() {
  client_id = "ESP32_CLIENT_" + buildDeviceToken();
}

void buildDefaultWifiField(const String &prefix, char *target, size_t targetSize) {
  String value = prefix + buildDeviceToken();
  copyStringToBuffer(value, target, targetSize);
}

void buildDefaultWifiPassword(char *target, size_t targetSize) {
  String value = "ESP32#" + buildDeviceToken() + "!";
  copyStringToBuffer(value, target, targetSize);
}

void applyDefaultServerAccessPointConfig(char *ssidTarget, size_t ssidSize, char *passwordTarget, size_t passwordSize) {
  copyStringToBuffer(String(DEFAULT_SERVER_AP_SSID), ssidTarget, ssidSize);
  copyStringToBuffer(String(DEFAULT_SERVER_AP_PASSWORD), passwordTarget, passwordSize);
}

bool matchesLegacyDynamicWifiConfig(const char *ssidValue, const char *passwordValue, const String &ssidPrefix) {
  if (ssidValue == NULL || passwordValue == NULL) {
    return false;
  }

  char legacySsid[WIFI_SSID_MAX_LEN + 1] = {0};
  char legacyPassword[WIFI_PASSWORD_MAX_LEN + 1] = {0};
  buildDefaultWifiField(ssidPrefix, legacySsid, sizeof(legacySsid));
  buildDefaultWifiPassword(legacyPassword, sizeof(legacyPassword));

  return strcmp(ssidValue, legacySsid) == 0 && strcmp(passwordValue, legacyPassword) == 0;
}

bool migrateLegacyDefaultWiFiConfig() {
  bool migrated = false;

  if (matchesLegacyDynamicWifiConfig(ap_ssid, ap_password, "ESP32_UART_")) {
    applyDefaultServerAccessPointConfig(ap_ssid, sizeof(ap_ssid), ap_password, sizeof(ap_password));
    migrated = true;
  }

  if (matchesLegacyDynamicWifiConfig(client_wifi_ssid, client_wifi_password, "ESP32_UART_")) {
    copyStringToBuffer(String(DEFAULT_SERVER_AP_SSID), client_wifi_ssid, sizeof(client_wifi_ssid));
    copyStringToBuffer(String(DEFAULT_SERVER_AP_PASSWORD), client_wifi_password, sizeof(client_wifi_password));
    migrated = true;
  }

  return migrated;
}

bool validateClientIdValue(const String &value) {
  if (value.length() == 0 || value.length() > CLIENT_ID_MAX_LEN) {
    return false;
  }

  for (unsigned int i = 0; i < value.length(); i++) {
    char current = value[i];
    bool isAllowed = (current >= '0' && current <= '9') ||
                     (current >= 'A' && current <= 'Z') ||
                     (current >= 'a' && current <= 'z') ||
                     current == '_' || current == '-';
    if (!isAllowed) {
      return false;
    }
  }

  return true;
}

bool validateWiFiSsidValue(const String &value) {
  if (value.length() == 0 || value.length() > WIFI_SSID_MAX_LEN) {
    return false;
  }

  for (unsigned int i = 0; i < value.length(); i++) {
    if (!isSafeConfigCharacter(value[i]) && value[i] != ' ') {
      return false;
    }
  }

  return true;
}

bool validateWiFiPasswordValue(const String &value, bool allowEmpty) {
  if (value.length() == 0) {
    return allowEmpty;
  }

  if (value.length() < 8 || value.length() > (WIFI_PASSWORD_MAX_LEN - 1)) {
    return false;
  }

  for (unsigned int i = 0; i < value.length(); i++) {
    if (!isSafeConfigCharacter(value[i]) && value[i] != ' ') {
      return false;
    }
  }

  return true;
}

bool hasConfiguredClientWiFi() {
  return validateWiFiSsidValue(String(client_wifi_ssid)) &&
         validateWiFiPasswordValue(String(client_wifi_password), false);
}

String maskSensitiveValue(const char *value) {
  if (value == NULL || value[0] == '\0') {
    return "未配置";
  }

  size_t length = strlen(value);
  if (length <= 2) {
    return "**";
  }

  String masked = String(value[0]) + String(value[1]);
  for (size_t i = 2; i < length; i++) {
    masked += '*';
  }
  return masked;
}

String maskIpAddress(IPAddress address) {
  return String(address[0]) + "." + String(address[1]) + ".*.*";
}

void applyDefaultWiFiConfig() {
  applyDefaultServerAccessPointConfig(ap_ssid, sizeof(ap_ssid), ap_password, sizeof(ap_password));

  copyStringToBuffer(String(ap_ssid), client_wifi_ssid, sizeof(client_wifi_ssid));
  copyStringToBuffer(String(ap_password), client_wifi_password, sizeof(client_wifi_password));

  buildDefaultWifiField("ESP32_CFG_", wifimanager_ssid, sizeof(wifimanager_ssid));
  buildDefaultWifiPassword(wifimanager_password, sizeof(wifimanager_password));
}

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

  // Load UART1 baud rate
  EEPROM.get(EEPROM_UART1_BAUD_ADDR, uart1BaudRate);
  if (uart1BaudRate < 9600 || uart1BaudRate > 921600) {
    uart1BaudRate = DEFAULT_UART1_BAUD;
  }
  
  // Load client ID (will be overwritten by chip ID based ID)
  char idBuffer[32];
  memset(idBuffer, 0, sizeof(idBuffer));
  for (int i = 0; i < 31; i++) {
    idBuffer[i] = EEPROM.read(EEPROM_CLIENT_ID_ADDR + i);
    if (idBuffer[i] == 0 || idBuffer[i] == 255) break;
  }
  if (validateClientIdValue(String(idBuffer))) {
    client_id = String(idBuffer);
  } else {
    applyDefaultClientId();
  }

  readEEPROMString(EEPROM_AP_SSID_ADDR, ap_ssid, sizeof(ap_ssid));
  readEEPROMString(EEPROM_AP_PASS_ADDR, ap_password, sizeof(ap_password));
  readEEPROMString(EEPROM_STA_SSID_ADDR, client_wifi_ssid, sizeof(client_wifi_ssid));
  readEEPROMString(EEPROM_STA_PASS_ADDR, client_wifi_password, sizeof(client_wifi_password));
  readEEPROMString(EEPROM_PORTAL_SSID_ADDR, wifimanager_ssid, sizeof(wifimanager_ssid));
  readEEPROMString(EEPROM_PORTAL_PASS_ADDR, wifimanager_password, sizeof(wifimanager_password));

  bool wifiConfigValid = EEPROM.read(EEPROM_WIFI_MAGIC_ADDR) == EEPROM_WIFI_CONFIG_MAGIC &&
                         validateWiFiSsidValue(String(ap_ssid)) &&
                         validateWiFiPasswordValue(String(ap_password), false) &&
                         validateWiFiSsidValue(String(client_wifi_ssid)) &&
                         validateWiFiPasswordValue(String(client_wifi_password), false) &&
                         validateWiFiSsidValue(String(wifimanager_ssid)) &&
                         validateWiFiPasswordValue(String(wifimanager_password), false);

  bool wifiConfigMigrated = false;

  if (!wifiConfigValid) {
    applyDefaultWiFiConfig();
  } else {
    wifiConfigMigrated = migrateLegacyDefaultWiFiConfig();
  }
  
  // Load log timestamp setting
  logWithTimestamp = (EEPROM.read(EEPROM_LOGTIME_ADDR) == 1);

  int savedDebugMode = EEPROM.read(EEPROM_DEBUGMODE_ADDR);
  if (savedDebugMode == 0 || savedDebugMode == 1) {
    debugMode = (savedDebugMode == 1);
  }

  if (!wifiConfigValid || wifiConfigMigrated) {
    saveConfigToEEPROM();
  }
  
  if (debugMode) {
    Serial.println("Config loaded from EEPROM");
  }
}

void saveModeToEEPROM() {
  EEPROM.write(EEPROM_MODE_ADDR, currentMode);
  EEPROM.commit();
}

void saveConfigToEEPROM() {
  // Save UART2 baud rate
  EEPROM.put(EEPROM_UART2_BAUD_ADDR, uart2BaudRate);

  // Save UART1 baud rate
  EEPROM.put(EEPROM_UART1_BAUD_ADDR, uart1BaudRate);
  
  // Save client ID
  for (int i = 0; i < client_id.length() && i < 32; i++) {
    EEPROM.write(EEPROM_CLIENT_ID_ADDR + i, client_id[i]);
  }
  EEPROM.write(EEPROM_CLIENT_ID_ADDR + client_id.length(), 0);

  EEPROM.write(EEPROM_WIFI_MAGIC_ADDR, EEPROM_WIFI_CONFIG_MAGIC);
  writeEEPROMString(EEPROM_AP_SSID_ADDR, ap_ssid, sizeof(ap_ssid));
  writeEEPROMString(EEPROM_AP_PASS_ADDR, ap_password, sizeof(ap_password));
  writeEEPROMString(EEPROM_STA_SSID_ADDR, client_wifi_ssid, sizeof(client_wifi_ssid));
  writeEEPROMString(EEPROM_STA_PASS_ADDR, client_wifi_password, sizeof(client_wifi_password));
  writeEEPROMString(EEPROM_PORTAL_SSID_ADDR, wifimanager_ssid, sizeof(wifimanager_ssid));
  writeEEPROMString(EEPROM_PORTAL_PASS_ADDR, wifimanager_password, sizeof(wifimanager_password));
  
  // Save log timestamp setting
  EEPROM.write(EEPROM_LOGTIME_ADDR, logWithTimestamp ? 1 : 0);
  EEPROM.write(EEPROM_DEBUGMODE_ADDR, debugMode ? 1 : 0);
  
  EEPROM.commit();
  if (debugMode) {
    Serial.println("Config saved to EEPROM");
  }
}

// ==================== Mode Switch ====================
void switchMode() {
  currentMode = (currentMode == MODE_CLIENT) ? MODE_SERVER : MODE_CLIENT;
  saveModeToEEPROM();
  
  Serial.println("\n*** Mode Switch!");
  Serial.println("New mode: " + String(currentMode == MODE_CLIENT ? "Client" : "Server"));
  Serial.println("System will restart...");
  
  // LED flash notification (非阻塞多次闪烁)
  requestMultiFlash(CRGB(255, 255, 255), 5, 100);
  
  delay(500);
  ESP.restart();
}

void resetToDefault() {
  currentMode = MODE_CLIENT;
  uart2BaudRate = DEFAULT_UART2_BAUD;
  uart1BaudRate = DEFAULT_UART1_BAUD;
  applyDefaultClientId();
  applyDefaultWiFiConfig();
  saveModeToEEPROM();
  saveConfigToEEPROM();
  
  Serial.println("\n*** Reset to Default!");
  Serial.println("System will restart...");
  
  // LED flash notification (非阻塞多次闪烁)
  requestMultiFlash(CRGB(255, 0, 0), 10, 100);
  
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
