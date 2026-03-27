// ==================== Battery Monitor ====================

// 过滤ANSI转义序列
String filterAnsiEscape(String input) {
  if (input.length() == 0) return input;
  
  String output = "";
  int len = input.length();
  
  for (int i = 0; i < len; i++) {
    if (input[i] == 0x1B) {
      int j = i + 1;
      if (j < len && input[j] == '[') {
        j++;
        while (j < len) {
          char c = input[j];
          if ((c >= '0' && c <= '9') || c == ';' || c == '?' || c == '>' || c == '<' || c == '=') {
            j++;
          } else {
            break;
          }
        }
        if (j < len) {
          char c = input[j];
          if (c >= 0x40 && c <= 0x7E) {
            j++;
          }
        }
        i = j - 1;
        continue;
      }
    }
    output += input[i];
  }
  return output;
}

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
      createDirectory("/client_local");
    } else {
      createDirectory("/server");
      createDirectory("/server/system");
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

String sanitizeFilename(String input) {
  String result = "";
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '-') {
      result += c;
    } else if (c == ' ') {
      result += '_';
    }
  }
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
  char clientDir[64];

  clientId.toCharArray(safeClientId, sizeof(safeClientId));
  sanitizeFilenameInPlace(safeClientId);

  if (isServer) {
    snprintf(clientDir, sizeof(clientDir), "/server/%s", safeClientId);
    snprintf(path, sizeof(path), "%s/%s_%s.txt",
             clientDir, logFileName.c_str(), getDateString().c_str());
    createDirectory("/server");
    createDirectory(clientDir);
  } else {
    snprintf(clientDir, sizeof(clientDir), "/client_local");
    snprintf(path, sizeof(path), "%s/%s_%s.txt",
             clientDir, logFileName.c_str(), getDateString().c_str());
    createDirectory("/client_local");
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

void saveServerSystemLog(String data) {
  if (!sdCardReady) {
    return;
  }

  createDirectory("/server");
  createDirectory("/server/system");

  String fileName = "system_" + getDateString() + ".txt";
  char path[64];
  snprintf(path, sizeof(path), "/server/system/%s", fileName.c_str());

  File file = SD.open(path, FILE_APPEND);
  if (file) {
    file.println(data);
    file.close();
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

// ==================== SD异步写入队列 ====================
#define SD_WRITE_QUEUE_SIZE 256
#define SD_WRITE_BATCH_SIZE 32
#define SD_WRITE_INTERVAL_MS 1000  // 每1秒批量写入一次

typedef struct {
  String data;
  String clientId;
  bool isServer;
} SDLogEntry;

SDLogEntry sdWriteQueue[SD_WRITE_QUEUE_SIZE];
int sdQueueHead = 0;
int sdQueueTail = 0;
int sdQueueCount = 0;
unsigned long lastSDWriteTime = 0;

bool enqueueSDLog(String data, String clientId, bool isServer) {
  if (!sdCardReady || data.length() == 0) return false;
  if (sdQueueCount >= SD_WRITE_QUEUE_SIZE) return false;
  
  // 过滤ANSI转义序列后再保存
  String filteredData = filterAnsiEscape(data);
  sdWriteQueue[sdQueueHead].data = filteredData;
  sdWriteQueue[sdQueueHead].clientId = clientId;
  sdWriteQueue[sdQueueHead].isServer = isServer;
  sdQueueHead = (sdQueueHead + 1) % SD_WRITE_QUEUE_SIZE;
  sdQueueCount++;
  return true;
}

void processSDWriteQueue() {
  if (!sdCardReady) return;
  if (sdQueueCount == 0) return;
  
  unsigned long now = millis();
  if (now - lastSDWriteTime < SD_WRITE_INTERVAL_MS && sdQueueCount < SD_WRITE_BATCH_SIZE) return;
  
  lastSDWriteTime = now;
  
  int batchSize = min(sdQueueCount, SD_WRITE_BATCH_SIZE);
  
  for (int i = 0; i < batchSize; i++) {
    SDLogEntry entry = sdWriteQueue[sdQueueTail];
    sdQueueTail = (sdQueueTail + 1) % SD_WRITE_QUEUE_SIZE;
    sdQueueCount--;
    
    char path[128];
    char safeClientId[32];
    char clientDir[64];
    
    entry.clientId.toCharArray(safeClientId, sizeof(safeClientId));
    sanitizeFilenameInPlace(safeClientId);
    
    if (entry.isServer) {
      snprintf(clientDir, sizeof(clientDir), "/server/%s", safeClientId);
      snprintf(path, sizeof(path), "%s/%s_%s.txt",
               clientDir, logFileName.c_str(), getDateString().c_str());
    } else {
      snprintf(clientDir, sizeof(clientDir), "/client_local");
      snprintf(path, sizeof(path), "%s/%s_%s.txt",
               clientDir, logFileName.c_str(), getDateString().c_str());
    }
    
    File file = SD.open(path, FILE_APPEND);
    if (file) {
      file.println(entry.data);
      file.close();
    }
  }
}