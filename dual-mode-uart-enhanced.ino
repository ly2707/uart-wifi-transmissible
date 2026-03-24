#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <SD.h>
#include <SPI.h>
#include <FastLED.h>
#include <esp_sleep.h>
#include <driver/uart.h>

// ==================== Version Info ====================
#define FIRMWARE_VERSION "v2.3.4"
#define VERSION_MAJOR 2
#define VERSION_MINOR 3
#define VERSION_PATCH 4

// Version history:
// v2.3.4 - 2024-03-24 - Fix log filename garbled characters, add filename sanitization
// v2.3.3 - 2024-03-24 - Fix encoding issues causing Guru Meditation Error
// v2.3.2 - 2024-03-23 - Fix log download 404, serial garbled text, add log preview
// v2.3.1 - 2024-03-23 - Fix UART2 data display, correct line breaks like Xshell
// v2.3.0 - 2024-03-23 - Interrupt-driven high-speed transparent, Web real-time serial display, AT+SELECT client selection, fix log view
// v2.2.5 - 2024-03-23 - Fix config mode blocking, support non-blocking config
// v2.2.4 - 2024-03-13 - Add UART debug info, fix receive issues
// v2.2.3 - 2024-03-13 - Add UART1<->UART2 bidirectional transparent
// v2.2.2 - 2024-03-13 - Fix UART rx/tx issues, USB forwarding, debug serial display
// v2.2.1 - 2024-03-13 - Fix switch case compile error
// v2.2 - 2024-03-13 - Add WiFi log view, custom log name
// v2.1 - 2024-03-13 - Optimize SD card pin config (CS=GPIO10)
// v2.0 - 2024-03-13 - Add low power, bidirectional serial, smart config
// v1.0 - 2024-03-11 - Initial version (dual mode UART transparent)

// ==================== Configuration Parameters ====================
#define MODE_CLIENT   0
#define MODE_SERVER   1

#define EEPROM_SIZE   256
#define EEPROM_MODE_ADDR          0
#define EEPROM_CLIENT_ID_ADDR     10
#define EEPROM_UART2_BAUD_ADDR    46
#define EEPROM_LOWPOWER_TIMEOUT   54
#define EEPROM_WIFI_SSID_ADDR     60
#define EEPROM_WIFI_PASS_ADDR     100

#define BUTTON_PIN    0
#define SHORT_PRESS   2000
#define LONG_PRESS    5000

#define CONFIG_MODE_PIN  2

#define POWER_CONTROL_PIN  3
#define RESET_CONTROL_PIN  4

#define LED_PIN       48
#define LED_NUM       1
#define LED_TYPE      NEOPIXEL

#define UART2_RX_PIN  16
#define UART2_TX_PIN  17
#define DEFAULT_UART2_BAUD  115200

#define SD_CS_PIN     10
#define SD_MOSI       11
#define SD_MISO       13
#define SD_SCK        12
#define SD_POWER_PIN  8

#define BATTERY_ADC_PIN  1
#define BATTERY_LOW_VOLTAGE  3.3

const char* ap_ssid = "ESP32_UART_Server";
const char* ap_password = "12345678";
const int server_listen_port = 8080;

const char* client_wifi_ssid = ap_ssid;
const char* client_wifi_password = ap_password;
const char* server_ip = "192.168.1.1";
const int server_port = 8080;

const char* wifimanager_ssid = "ESP32_Config";
const char* wifimanager_password = "12345678";

String client_id = "ESP32_CLIENT_001";

// ==================== Global Variables ====================
int currentMode = MODE_CLIENT;
bool modeChanged = false;
unsigned long lastActivityTime = 0;

unsigned long systemStartTime = 0;

unsigned long uart2BaudRate = DEFAULT_UART2_BAUD;

unsigned long buttonPressTime = 0;
bool buttonPressed = false;

CRGB leds[LED_NUM];
unsigned long lastLEDToggle = 0;
int ledBlinkState = 0;
int breatheValue = 0;

enum LEDState {
  LED_OFF,
  LED_FAST_BLINK,
  LED_SLOW_BLINK,
  LED_BREATHE,
  LED_SOLID_GREEN,
  LED_SOLID_YELLOW,
  LED_SOLID_RED,
  LED_SINGLE_FLASH,
  LED_SD_ERROR,
  LED_LOW_BATTERY
};

LEDState currentLEDState = LED_OFF;
LEDState previousLEDState = LED_OFF;

WiFiClient tcpClient;
WiFiServer tcpServer(server_listen_port);
WiFiManager wm;
bool wifiConnected = false;
bool tcpConnected = false;
bool configMode = false;

bool inConfigMode = false;
unsigned long configModeStartTime = 0;
const unsigned long configModeTimeout = 300000;

WiFiServer webServer(80);
bool webServerEnabled = true;

String logFileName = "uart_log";
String logFilePath = "";
bool logToSD = true;
unsigned long logFileSize = 0;
unsigned int logCount = 0;

bool debugMode = true;

#define MAX_CLIENTS  5
WiFiClient serverClients[MAX_CLIENTS];
String clientSerialData[MAX_CLIENTS];

extern int selectedClientIndex;

bool sdCardReady = false;
bool sdCardError = false;

float batteryVoltage = 0;
bool lowBattery = false;

// ==================== Function Declarations ====================
void setup();
void loop();
void loadConfigFromEEPROM();
void saveModeToEEPROM();
void saveConfigToEEPROM();
void switchMode();
void resetToDefault();
void handleButton();
void checkBattery();
void initSDCard();
void powerOffSDCard();
void powerOnSDCard();
void createDirectory(String path);
void saveDataToSD(String data, String clientId, bool isServer);
String sanitizeFilename(String input);
void checkSDCardStatus();
void initWebServer();
void handleWebServer();
void handleRootPage(WiFiClient client);
void handleLogsPage(WiFiClient client, String request);
void handleStatusPage(WiFiClient client);
void handleConfigPage(WiFiClient client);
void handleDownloadLog(WiFiClient client, String request);
void handleClearLog(WiFiClient client);
void handleSaveConfig(WiFiClient client);
void handleClientPage(WiFiClient client, String request);
void handleNotFound(WiFiClient client);
void startConfigMode();
void handleConfigMode();
void exitConfigMode();
void configModeCallback(WiFiManager *myWiFiManager);
void initClientMode();
void connectToServer();
void runClientMode();
void initServerMode();
void runServerMode();
String parseClientId(String data);
int getConnectedClientCount();
void handleUART2ToDebug();
void handleUSBSerial();
void initUARTInterrupt();
void handleHighSpeedUART();
void selectClient(int index);
void listSelectableClients();
void handleCommand(String command);
void setLED(CRGB color);
void updateLEDStatus();
void flashLED();
String getDateString();
void printHelp();
void powerOn();
void powerOff();
void triggerShutdown();
void resetCPU();

// ==================== Setup Function ====================
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println("\n========================================");
  Serial.println("  ESP32-S3 UART2 Transparent System");
  Serial.println("  Version: " + String(FIRMWARE_VERSION));
  Serial.println("  Function: Debug Serial <-> UART2 Bidirectional");
  Serial.println("========================================");
  
  EEPROM.begin(EEPROM_SIZE);
  loadConfigFromEEPROM();
  
  Serial2.begin(uart2BaudRate, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
  
  initUARTInterrupt();
  
  if (debugMode) {
    Serial.print("* UART2 initialized, baud rate: ");
    Serial.println(uart2BaudRate);
    Serial.print("* UART2 pins: RX=");
    Serial.print(UART2_RX_PIN);
    Serial.print(" TX=");
    Serial.println(UART2_TX_PIN);
    
    Serial2.println("UART2 Test Message");
    Serial.println("* UART2 test message sent");
  }
  
  FastLED.addLeds<LED_TYPE, LED_PIN>(leds, LED_NUM);
  FastLED.setBrightness(50);
  FastLED.show();
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  pinMode(SD_POWER_PIN, OUTPUT);
  digitalWrite(SD_POWER_PIN, HIGH);
  
  pinMode(BATTERY_ADC_PIN, INPUT);
  
  pinMode(POWER_CONTROL_PIN, OUTPUT);
  pinMode(RESET_CONTROL_PIN, OUTPUT);
  digitalWrite(POWER_CONTROL_PIN, HIGH);
  digitalWrite(RESET_CONTROL_PIN, HIGH);
  
  initSDCard();
  
  checkBattery();
  
  if (currentMode == MODE_CLIENT) {
    initClientMode();
  } else {
    initServerMode();
  }
  
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  client_id = "ESP32_CLIENT_" + String(chipId, HEX);
  saveConfigToEEPROM();
  
  systemStartTime = millis();
  
  initWebServer();
  
  lastActivityTime = millis();
  
  if (debugMode) {
    Serial.println("========================================");
    Serial.println("System startup complete!");
    Serial.println("Current mode: " + String(currentMode == MODE_CLIENT ? "Client" : "Server"));
    Serial.println("UART2 baud rate: " + String(uart2BaudRate));
    Serial.println("Client ID: " + client_id);
    Serial.println("Log filename: " + logFileName);
    Serial.println("Battery voltage: " + String(batteryVoltage) + "V");
    Serial.println("========================================\n");
    
    printHelp();
  }
}

// ==================== Main Loop ====================
void loop() {
  handleButton();
  
  handleConfigMode();
  
  if (millis() % 10000 == 0) {
    checkBattery();
  }
  
  if (millis() % 2000 == 0) {
    checkSDCardStatus();
  }
  
  handleUSBSerial();
  
  handleHighSpeedUART();
  
  handleWebServer();
  
  if (!inConfigMode) {
    if (currentMode == MODE_CLIENT) {
      runClientMode();
    } else {
      runServerMode();
    }
  }
  
  updateLEDStatus();
}
