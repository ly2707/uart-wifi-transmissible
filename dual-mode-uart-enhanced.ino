#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiManager.h>  // 智能配网
#include <EEPROM.h>
#include <SD.h>
#include <SPI.h>
#include <FastLED.h>
#include <esp_sleep.h>
#include <driver/uart.h>

// ==================== 版本信息 ====================
#define FIRMWARE_VERSION "v2.5.0"  // 优化串口透传速度，批量写入TCP缓冲区
#define VERSION_MAJOR 2
#define VERSION_MINOR 5
#define VERSION_PATCH 0

// 版本历史：
// v2.5.0 - 2026-03-26 - 优化串口透传速度，批量写入TCP缓冲区，移除重复读取
// v2.4.0 - 2026-03-26 - 修复UART中断未初始化问题，完善Web串口监视器实时显示，优化客户端页面
// v2.3.5 - 2024-03-24 - 修复网页重复显示，添加串口数据实时同步
// v2.3.4 - 2024-03-24 - 修复日志文件名乱码，添加文件名安全处理
// v2.3.3 - 2024-03-24 - 修复编码问题导致的Guru Meditation Error
// v2.3.2 - 2024-03-23 - 修复日志下载404，串口乱码，添加日志预览
// v2.3.1 - 2024-03-23 - 修复UART2数据显示，正确处理换行
// v2.3.0 - 2024-03-23 - 中断驱动高速透传，Web实时串口显示，AT+SELECT客户端选择
// v2.2.4 - 2024-03-13 - 新增UART调试信息，修复接收问题
// v2.2.3 - 2024-03-13 - 新增UART1↔UART2双向透传
// v2.2.2 - 2024-03-13 - 修复UART收发异常、USB转发失效、调试串口显示
// v2.2.1 - 2024-03-13 - 修复switch case编译错误
// v2.2 - 2024-03-13 - 新增WiFi日志查看、自定义日志名称
// v2.1 - 2024-03-13 - 优化SD卡引脚配置（CS=GPIO10）
// v2.0 - 2024-03-13 - 新增低功耗、双向串口、智能配网
// v1.0 - 2024-03-11 - 初始版本（双模式UART透传）

// ==================== 配置参数 ====================
// 模式定义
#define MODE_CLIENT   0
#define MODE_SERVER   1

// EEPROM地址分配
#define EEPROM_SIZE   256
#define EEPROM_MODE_ADDR          0
#define EEPROM_CLIENT_ID_ADDR     10
#define EEPROM_UART2_BAUD_ADDR    46
#define EEPROM_LOWPOWER_TIMEOUT   54
#define EEPROM_WIFI_SSID_ADDR     60
#define EEPROM_WIFI_PASS_ADDR     100

// 按键配置
#define BUTTON_PIN    0
#define SHORT_PRESS   2000   // 短按阈值（毫秒）
#define LONG_PRESS    5000   // 长按阈值（毫秒）

// 配网模式触发引脚
#define CONFIG_MODE_PIN  42   // GPIO2 用于触发配网模式

// 电源和复位控制引脚
#define POWER_CONTROL_PIN  5   // GPIO3 用于控制开机关机
#define RESET_CONTROL_PIN  4   // GPIO4 用于控制CPU复位

// LED配置
#define LED_PIN       48
#define LED_NUM       1
#define LED_TYPE      NEOPIXEL

// ========== UART2配置 ==========
#define UART2_RX_PIN  16
#define UART2_TX_PIN  17
#define DEFAULT_UART2_BAUD  115200

// SD卡配置
#define SD_CS_PIN     10
#define SD_MOSI       11
#define SD_MISO       13
#define SD_SCK        12
#define SD_POWER_PIN  8   // 修改：避免与UART1引脚冲突

// 电池监测（ADC引脚）
#define BATTERY_ADC_PIN  1
#define BATTERY_LOW_VOLTAGE  3.3   // 低电压阈值

// 低功耗配置（已禁用）
// #define DEFAULT_LOWPOWER_TIMEOUT  30000   // 默认30秒
// #define LIGHT_SLEEP_CURRENT_LIMIT  10     // 浅睡眠目标电流(mA)
// #define DEEP_SLEEP_CURRENT_LIMIT   0.1   // 深度睡眠目标电流(mA)

// WiFi配置（服务器模式）
const char* ap_ssid = "ESP32_UART_Server";
const char* ap_password = "12345678";
const int server_listen_port = 8080;

// WiFi配置（客户端模式）
const char* client_wifi_ssid = ap_ssid;  // 使用服务器端的WiFi名称
const char* client_wifi_password = ap_password;  // 使用服务器端的WiFi密码
const char* server_ip = "192.168.1.1";  // 服务器模式的IP地址
const int server_port = 8080;

// 智能配网
const char* wifimanager_ssid = "ESP32_Config";
const char* wifimanager_password = "12345678";

// 客户端ID
String client_id = "ESP32_CLIENT_001";

// ==================== 全局变量 ====================
// 系统状态
int currentMode = MODE_CLIENT;
bool modeChanged = false;
// bool lowPowerMode = false;  // 低功耗模式已禁用
unsigned long lastActivityTime = 0;
// unsigned long lowPowerTimeout = DEFAULT_LOWPOWER_TIMEOUT;  // 低功耗超时已禁用

// 系统启动时间（用于生成唯一的日志文件名）
unsigned long systemStartTime = 0;

// ========== UART2波特率 ==========
unsigned long uart2BaudRate = DEFAULT_UART2_BAUD;

// 按键相关
unsigned long buttonPressTime = 0;
bool buttonPressed = false;

// LED相关
CRGB leds[LED_NUM];
unsigned long lastLEDToggle = 0;
int ledBlinkState = 0;
int breatheValue = 0;

// 配网模式相关
bool inConfigMode = false;
unsigned long configModeStartTime = 0;
const unsigned long configModeTimeout = 300000; // 5分钟超时

// 串口缓冲区
String usbRxBuffer = "";

// RAW透传模式
bool rawTransmitMode = false;

// LED状态枚举
enum LEDState {
  LED_OFF,
  LED_FAST_BLINK,      // 快闪 - 客户端模式
  LED_SLOW_BLINK,      // 慢闪 - 服务器模式
  LED_BREATHE,         // 呼吸灯 - 低功耗待机
  LED_SOLID_GREEN,     // 常亮绿 - 已连接
  LED_SOLID_YELLOW,    // 常亮黄 - 部分连接
  LED_SOLID_RED,       // 常亮红 - 未连接
  LED_SINGLE_FLASH,    // 单次短闪 - 收到指令
  LED_SD_ERROR,        // SD卡错误闪烁
  LED_LOW_BATTERY      // 低电量闪烁
};

LEDState currentLEDState = LED_OFF;
LEDState previousLEDState = LED_OFF;

// 网络相关
WiFiClient tcpClient;
WiFiServer tcpServer(server_listen_port);
WiFiManager wm;
bool wifiConnected = false;
bool tcpConnected = false;
bool configMode = false;

// Web服务器（日志查看）
WiFiServer webServer(80);
bool webServerEnabled = true;

// 日志配置
String logFileName = "uart_log";  // 默认日志文件名
String logFilePath = "";          // 日志文件完整路径
bool logToSD = true;              // 是否记录到SD卡
unsigned long logFileSize = 0;    // 日志文件大小
unsigned int logCount = 0;        // 日志条数

// Debug模式控制
bool debugMode = true;             // 默认开启debug模式

// 客户端列表（服务器模式）
#define MAX_CLIENTS  5
WiFiClient serverClients[MAX_CLIENTS];
String clientSerialData[MAX_CLIENTS];  // 存储每个客户端的实时串口数据
String clientLineBuffer[MAX_CLIENTS];  // 客户端日志行缓冲

// SD卡状态
bool sdCardReady = false;
bool sdCardError = false;

// 电池状态
float batteryVoltage = 0;
bool lowBattery = false;

// ========== UART2/USB缓冲区 ==========
String uart2RxBuffer = "";  // UART2接收缓冲区
#define UART_BUFFER_SIZE  1024  // 缓冲区大小限制

// ========== 串口实时显示缓冲区 ==========
#define SERIAL_DISPLAY_BUFFER_SIZE  4096
String serialDisplayBuffer = "";  // Web串口显示缓冲区
unsigned long lastSerialUpdate = 0;

// ==================== 函数声明 ====================
// 初始化函数
void setup();
void loop();

// 配置管理
void loadConfigFromEEPROM();
void saveModeToEEPROM();
void saveConfigToEEPROM();

// 模式切换
void switchMode();
void resetToDefault();

// 按键处理
void handleButton();

// 电池监测
void checkBattery();

// SD卡管理
void initSDCard();
void powerOffSDCard();
void powerOnSDCard();
void createDirectory(String path);
void saveDataToSD(String data, String clientId, bool isServer);
void saveServerSystemLog(String data);
bool enqueueSDLog(String data, String clientId, bool isServer);
void processSDWriteQueue();
void checkSDCardStatus();

// Web服务器
void initWebServer();
void handleWebServer();
void handleRootPage(WiFiClient client);
void handleLogsPage(WiFiClient client, String request);
void handlePreviewLog(WiFiClient client, String request);
void handleStatusPage(WiFiClient client);
void handleConfigPage(WiFiClient client);
void handleDownloadLog(WiFiClient client, String request);
void handleClearLog(WiFiClient client);
void handleSaveConfig(WiFiClient client);
void handleClientPage(WiFiClient client, String request);
void handleClientSend(WiFiClient client, String request);
void handleDeleteFile(WiFiClient client, String request);
void handleDeleteDirectory(WiFiClient client, String request);
bool deleteDirectoryRecursive(String path);
void handleNotFound(WiFiClient client);
void handleSerialPage(WiFiClient client);
void handleSerialDataAPI(WiFiClient client);
void handleSerialSend(WiFiClient client, String request);
void handleSerialClear(WiFiClient client);
void handlePowerControl(WiFiClient client, String request);
void appendToSerialBuffer(char c);
void appendToSerialBuffer(const char* str);
void appendToSerialBuffer(const char* str, int len);
String formatFileSize(unsigned long bytes);
String urlDecode(String input);

// 智能配网
void startConfigMode();
void configModeCallback(WiFiManager *myWiFiManager);

// 客户端模式
void initClientMode();
void connectToServer();
void runClientMode();

// 服务器模式
void initServerMode();
void runServerMode();
String parseClientId(String data);
int getConnectedClientCount();

// UART透传
void handleUART2ToDebug();
void handleHighSpeedUART();
void handleHighSpeedUARTWithWebBuffer();
void initUARTInterrupt(bool reinstall = false);
void handleUSBSerial();
String formatClientData(String data);
String filterAnsiEscape(String input);

// 指令解析
void handleCommand(String command);

// 工具函数
void setLED(CRGB color);
void updateLEDStatus();
void flashLED();
String getDateString();
void printHelp();

// ==================== 初始化函数 ====================
void setup() {
  // 冷启动延迟，等待电源稳定
  delay(500);
  
  // 初始化调试串口（优先）
  Serial.begin(115200);
  delay(200);
  
  Serial.println("\n========================================");
  Serial.println("  ESP32-S3 UART2透传系统");
  Serial.println("  版本: " + String(FIRMWARE_VERSION));
  Serial.println("  功能: 调试串口 ↔ UART2 双向透传");
  Serial.println("========================================");
  
  // 检查是否是冷启动
  if (esp_reset_reason() == ESP_RST_POWERON) {
    Serial.println("冷启动，额外等待电源稳定...");
    delay(1000);
  }
  
  // 初始化EEPROM（先加载配置，再初始化UART）
  EEPROM.begin(EEPROM_SIZE);
  loadConfigFromEEPROM();
  
  // ========== 初始化UART2 ==========
  // 使用DMA驱动，不调用Serial2.begin避免冲突
  initUARTInterrupt();
  
  if (debugMode) {
    Serial.print("✓ UART2初始化完成，波特率: ");
    Serial.println(uart2BaudRate);
    Serial.print("✓ UART2引脚: RX=");
    Serial.print(UART2_RX_PIN);
    Serial.print(" TX=");
    Serial.println(UART2_TX_PIN);
    Serial.println("✓ UART DMA模式已启用");
    
    // 测试UART2发送
    const char* testMsg = "UART2 Test Message\r\n";
    uart_write_bytes(UART_NUM_2, testMsg, strlen(testMsg));
    Serial.println("✓ UART2测试消息已发送");
  }
  
  // 初始化LED
  FastLED.addLeds<LED_TYPE, LED_PIN>(leds, LED_NUM);
  FastLED.setBrightness(100);
  FastLED.show();
  
  // 初始化按键
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // 初始化SD卡电源控制（修改引脚避免冲突）
  pinMode(SD_POWER_PIN, OUTPUT);
  digitalWrite(SD_POWER_PIN, HIGH);
  
  // 初始化电池监测
  pinMode(BATTERY_ADC_PIN, INPUT);
  
  // 初始化电源和复位控制引脚
  pinMode(POWER_CONTROL_PIN, OUTPUT);
  pinMode(RESET_CONTROL_PIN, OUTPUT);
  // 初始状态为高电平
  digitalWrite(POWER_CONTROL_PIN, HIGH);
  digitalWrite(RESET_CONTROL_PIN, HIGH);
  
  // 初始化SD卡
  initSDCard();
  yield();
  
  // 检测电池电压
  checkBattery();
  yield();
  
  // 根据模式初始化网络
  if (currentMode == MODE_CLIENT) {
    initClientMode();
  } else {
    initServerMode();
  }
  yield();
  
  // 使用芯片ID生成唯一客户端ID
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  client_id = "ESP32_CLIENT_" + String(chipId, HEX);
  
  // 避免重复保存，只在需要时保存
  // saveConfigToEEPROM();  // 注释掉，避免每次启动都保存
  
  // 初始化系统启动时间
  systemStartTime = millis();
  
  // 初始化Web服务器（日志查看）
  initWebServer();
  
  // 更新活动时间
  lastActivityTime = millis();
  
  if (debugMode) {
    Serial.println("========================================");
    Serial.println("系统启动完成！");
    Serial.println("当前模式: " + String(currentMode == MODE_CLIENT ? "客户端" : "服务器"));
    Serial.println("UART2波特率: " + String(uart2BaudRate));
    Serial.println("客户端ID: " + client_id);
    Serial.println("日志文件名: " + logFileName);
    Serial.println("电池电压: " + String(batteryVoltage) + "V");
    Serial.println("========================================\n");
    
    printHelp();
  }
}

// ==================== 主循环 ====================
void loop() {
  yield();
  
  // 处理按键
  handleButton();
  
  // 检查电池
  if (millis() % 10000 == 0) {  // 每10秒检查一次
    checkBattery();
  }
  
  // 检查SD卡状态（热插拔检测）
  if (millis() % 2000 == 0) {  // 每2秒检查一次
    checkSDCardStatus();
  }
  
  // 低功耗功能已禁用
  // if (!lowPowerMode) {
  //   checkLowPowerCondition();
  // }
  
  // 处理USB CDC串口（双向通信）
  handleUSBSerial();
  
  // ========== UART2透传 ==========
  handleHighSpeedUARTWithWebBuffer();  // UART2 RX → 调试串口 + TCP + Web缓冲区
  
  // 处理Web服务器（日志查看）
  handleWebServer();
  
  // 根据模式执行不同逻辑（低功耗功能已禁用）
  // if (!lowPowerMode) {
    if (currentMode == MODE_CLIENT) {
      runClientMode();
      processSDWriteQueue();
    } else {
      runServerMode();
      processSDWriteQueue();
    }
  // } else {
  //   // 低功耗模式处理
  //   handleLowPowerMode();
  // }
  
  // 更新LED状态
  updateLEDStatus();
  
  yield();
}
