// ==================== Battery Monitor ====================
void checkBattery() {
  int adcValue = analogRead(BATTERY_ADC_PIN);
  batteryVoltage = (adcValue / 4095.0) * 3.3 * 2;
  
  if (batteryVoltage < BATTERY_LOW_VOLTAGE && batteryVoltage > 2.0) {
    if (!lowBattery) {
      lowBattery = true;
      Serial.println("! Low battery warning! Voltage: " + String(batteryVoltage) + "V");
      for (int i = 0; i < 5; i++) {
        setLED(CRGB(255, 0, 0));
        delay(200);
        setLED(CRGB(0, 0, 0));
        delay(200);
      }
    }
  } else {
    lowBattery = false;
  }
}

// ==================== SD Card Status Check ====================
void checkSDCardStatus() {
  static bool lastSDCardReady = sdCardReady;
  
  if (sdCardReady) {
    File root = SD.open("/");
    if (!root) {
      sdCardReady = false;
      sdCardError = true;
      if (lastSDCardReady) {
        if (currentMode == MODE_SERVER) {
          Serial.println("! SD card removed!");
          Serial.println("! Logging paused");
        }
        for (int i = 0; i < 3; i++) {
          setLED(CRGB(255, 0, 0));
          delay(200);
          setLED(CRGB(0, 0, 0));
          delay(200);
        }
      }
    } else {
      root.close();
    }
  } else {
    initSDCard();
    if (sdCardReady && !lastSDCardReady) {
      if (currentMode == MODE_SERVER) {
        Serial.println("! SD card reconnected!");
        Serial.println("! Logging resumed");
      }
      for (int i = 0; i < 3; i++) {
        setLED(CRGB(0, 255, 0));
        delay(200);
        setLED(CRGB(0, 0, 0));
        delay(200);
      }
    }
  }
  
  lastSDCardReady = sdCardReady;
}

// ==================== SD Card Management ====================
void initSDCard() {
  digitalWrite(SD_POWER_PIN, HIGH);
  delay(100);
  
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS_PIN);
  
  if (SD.begin(SD_CS_PIN)) {
    sdCardReady = true;
    sdCardError = false;
    if (currentMode == MODE_SERVER && debugMode) {
      Serial.println("? SD card initialized");
      Serial.println("  Capacity: " + String(SD.cardSize() / (1024 * 1024)) + " MB");
      Serial.println("  SPI pins: SCK=" + String(SD_SCK) + " MISO=" + String(SD_MISO) + " MOSI=" + String(SD_MOSI) + " CS=" + String(SD_CS_PIN));
    }
    
    if (currentMode == MODE_CLIENT) {
      createDirectory("/client");
      createDirectory("/client/" + sanitizeFilename(client_id));
    } else {
      createDirectory("/server");
    }
  } else {
    sdCardReady = false;
    sdCardError = true;
    if (currentMode == MODE_SERVER && debugMode) {
      Serial.println("? SD card init failed - Storage disabled");
      Serial.println("  Check SD card pins: SCK=" + String(SD_SCK) + " MISO=" + String(SD_MISO) + " MOSI=" + String(SD_MOSI) + " CS=" + String(SD_CS_PIN));
    }
  }
}

void powerOffSDCard() {
  if (sdCardReady) {
    SD.end();
    digitalWrite(SD_POWER_PIN, LOW);
    sdCardReady = false;
    Serial.println("SD card powered off");
  }
}

void powerOnSDCard() {
  if (!sdCardReady) {
    digitalWrite(SD_POWER_PIN, HIGH);
    delay(100);
    initSDCard();
  }
}

void createDirectory(String path) {
  if (sdCardReady && !SD.exists(path)) {
    SD.mkdir(path);
    Serial.println("  Created directory: " + path);
  }
}

// Sanitize filename - remove unsafe characters
String sanitizeFilename(String input) {
  String result = "";
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    // Only allow alphanumeric, underscore, hyphen
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
        (c >= '0' && c <= '9') || c == '_' || c == '-') {
      result += c;
    } else if (c == ' ') {
      result += '_';  // Replace space with underscore
    }
    // Skip other characters (including special chars that cause garbled names)
  }
  // If result is empty, use default
  if (result.length() == 0) {
    result = "UNKNOWN";
  }
  return result;
}

void saveDataToSD(String data, String clientId, bool isServer) {
  if (!sdCardReady) {
    return;
  }
  
  char path[128];
  char safeClientId[32];
  
  clientId.toCharArray(safeClientId, sizeof(safeClientId));
  sanitizeFilenameInPlace(safeClientId);
  
  if (isServer) {
    snprintf(path, sizeof(path), "/server/%s/%s_%s_%s.txt", 
             safeClientId, logFileName.c_str(), safeClientId, getDateString().c_str());
    createDirectory("/server");
    createDirectory(("/server/" + String(safeClientId)).c_str());
  } else {
    snprintf(path, sizeof(path), "/client/%s/%s_%s_%s.txt", 
             safeClientId, logFileName.c_str(), safeClientId, getDateString().c_str());
    createDirectory("/client");
    createDirectory(("/client/" + String(safeClientId)).c_str());
  }
  
  File file = SD.open(path, FILE_APPEND);
  if (file) {
    file.println(data);
    logFileSize = file.size();
    logCount++;
    file.close();
  } else {
    sdCardError = true;
    sdCardReady = false;
  }
}

void sanitizeFilenameInPlace(char* str) {
  int j = 0;
  for (int i = 0; str[i]; i++) {
    char c = str[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
        (c >= '0' && c <= '9') || c == '_' || c == '-') {
      str[j++] = c;
    } else if (c == ' ') {
      str[j++] = '_';
    }
  }
  str[j] = '\0';
  if (j == 0) {
    strcpy(str, "UNKNOWN");
  }
}
