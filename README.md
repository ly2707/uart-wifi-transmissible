# ESP32-S3 增强版双模式串口数据处理系统

## 版本信息
- **固件版本**: v2.0-enhanced
- **更新日期**: 2024
- **新增功能**: 低功耗待机、串口双向通信、智能配网、波特率配置

---

## 📋 目录
1. [功能特性](#功能特性)
2. [硬件接线](#硬件接线)
3. [配置手册](#配置手册)
4. [测试步骤](#测试步骤)
5. [功耗优化建议](#功耗优化建议)
6. [常见问题](#常见问题)

---

## 🎯 功能特性

### 核心功能
- ✅ 单固件双模式（客户端/服务器）
- ✅ 按键切换模式（短按<2秒切换，长按≥5秒恢复默认）
- ✅ 模式状态掉电保存（EEPROM）
- ✅ 智能配网（WiFiManager）
- ✅ 波特率动态配置

### 新增功能
- ✅ **低功耗待机**（Light Sleep + Deep Sleep）
- ✅ **串口双向通信**（UART + USB CDC）
- ✅ **锂电池供电支持**
- ✅ **电池电量监测**
- ✅ **多状态LED指示**

### LED状态指示
| 状态 | LED表现 | 说明 |
|------|---------|------|
| 客户端模式 | 🔵 蓝色快闪（200ms） | 未连接 |
| 服务器模式 | 🟦 青色慢闪（1000ms） | 无客户端 |
| 已连接 | 🟢 绿色常亮 | WiFi+TCP已连接 |
| 部分连接 | 🟡 黄色常亮 | WiFi已连接，TCP未连接 |
| 低功耗待机 | 🔵 蓝色呼吸灯 | Light Sleep模式 |
| 收到指令 | ⚪ 白色短闪 | 指令响应确认 |
| SD卡错误 | ⚪ 白色快闪 | SD卡读写异常 |
| 低电量 | 🔴 红色慢闪 | 电池电压<3.3V |

---

## 🔌 硬件接线

### ESP32-S3 Dev开发板引脚定义

| 功能 | 引脚 | 说明 |
|------|------|------|
| **RGB LED** | GPIO48 | 板载WS2812/NeoPixel |
| **BOOT按键** | GPIO0 | 板载按键 |
| **UART2 RX** | GPIO16 | 串口接收 |
| **UART2 TX** | GPIO17 | 串口发送 |
| **SD卡CS** | GPIO10 | SPI片选 |
| **SD卡MOSI** | GPIO11 | SPI数据输出 |
| **SD卡MISO** | GPIO13 | SPI数据输入 |
| **SD卡SCK** | GPIO12 | SPI时钟 |
| **SD卡电源** | GPIO4 | SD卡电源控制（可选） |
| **电池ADC** | GPIO1 | 电池电压监测 |
| **USB CDC** | USB-C | 虚拟串口 |

### 锂电池充放电模块接线

#### 推荐模块：TP4056 + DW01（带保护）

```
锂电池模块        ESP32-S3
  BAT+    -->    5V/VIN（通过升压模块）
  BAT-    -->    GND
  OUT+    -->    5V
  OUT-    -->    GND
  
电池电压监测（分压电路）：
  BAT+ ----[10kΩ]----+----[10kΩ]---- GND
                     |
                     +----> GPIO1 (ADC)
```

#### 分压计算
- 电池电压范围：3.0V - 4.2V
- 分压比：2:1
- ADC输入范围：1.5V - 2.1V（安全范围）

### SD卡模块接线（SPI接口）

```
SD卡模块        ESP32-S3
  CS    -->    GPIO10
  MOSI  -->    GPIO11
  MISO  -->    GPIO13
  SCK   -->    GPIO12
  VCC   -->    3.3V（或GPIO4控制）
  GND   -->    GND
```

### 完整接线图

```
┌─────────────────────────────────────────┐
│         ESP32-S3 Dev开发板               │
│                                         │
│  GPIO48 ────> RGB LED (WS2812)         │
│  GPIO0  ────> BOOT按键                  │
│  GPIO16 ────> UART2 RX                  │
│  GPIO17 ────> UART2 TX                  │
│  GPIO10 ────> SD卡 CS                   │
│  GPIO11 ────> SD卡 MOSI                 │
│  GPIO13 ────> SD卡 MISO                 │
│  GPIO12 ────> SD卡 SCK                  │
│  GPIO4  ────> SD卡电源控制（可选）       │
│  GPIO1  ────> 电池电压ADC               │
│  USB-C  ────> USB CDC虚拟串口           │
│  5V     <──── 锂电池升压模块输出         │
│  GND    <──── 公共地                     │
└─────────────────────────────────────────┘
```

---

## ⚙️ 配置手册

### 1. 修改WiFi配置

#### 客户端模式WiFi
在 `dual-mode-uart-enhanced.ino` 中找到：

```cpp
const char* server_ip = "192.168.1.100";  // 服务器IP
const int server_port = 8080;              // 服务器端口
```

**方法1：智能配网（推荐）**
- 上电后发送 `CONFIG` 命令
- 连接热点 `ESP32_Config`（密码：12345678）
- 访问 192.168.4.1 进行配置

**方法2：代码修改**
- 直接修改代码中的WiFi配置
- 重新烧录固件

#### 服务器模式WiFi
```cpp
const char* ap_ssid = "ESP32_UART_Server";  // 热点名称
const char* ap_password = "12345678";        // 热点密码（至少8位）
const int server_listen_port = 8080;        // 监听端口
```

### 2. 修改串口参数

#### 波特率配置
**方法1：指令配置（动态）**
```
CMD:CONFIG|BAUD:9600
CMD:CONFIG|BAUD:115200
CMD:CONFIG|BAUD:921600
```

**方法2：代码修改**
```cpp
#define DEFAULT_UART_BAUD  115200  // 默认波特率
```

#### 串口引脚修改
```cpp
#define UART_RX_PIN   16  // UART接收引脚
#define UART_TX_PIN   17  // UART发送引脚
```

### 3. 修改低功耗参数

#### 低功耗超时时间
**方法1：指令配置**
```
CMD:CONFIG|TIMEOUT:10   // 10秒
CMD:CONFIG|TIMEOUT:30   // 30秒
CMD:CONFIG|TIMEOUT:60   // 60秒
```

**方法2：代码修改**
```cpp
#define DEFAULT_LOWPOWER_TIMEOUT  30000  // 默认30秒（毫秒）
```

#### 低功耗模式选择
```cpp
// Light Sleep（推荐，可被串口唤醒）
esp_sleep_enable_timer_wakeup(1000000);
esp_light_sleep_start();

// Deep Sleep（功耗更低，需按键唤醒）
esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
esp_deep_sleep_start();
```

### 4. 修改客户端ID

**方法1：代码修改**
```cpp
String client_id = "ESP32_CLIENT_001";  // 修改为唯一标识符
```

**方法2：EEPROM存储**
- 客户端ID存储在EEPROM中
- 掉电保存

### 5. 修改SD卡引脚

```cpp
#define SD_CS_PIN     10  // CS引脚
#define SD_MOSI       11  // MOSI引脚
#define SD_MISO       13  // MISO引脚
#define SD_SCK        12  // SCK引脚
#define SD_POWER_PIN  4   // SD卡电源控制引脚（可选）
```

### 6. 修改电池监测参数

```cpp
#define BATTERY_ADC_PIN  1              // ADC引脚
#define BATTERY_LOW_VOLTAGE  3.3        // 低电压阈值（V）
```

**分压比调整**（如果使用不同分压电路）：
```cpp
batteryVoltage = (adcValue / 4095.0) * 3.3 * 2;  // 修改分压比
```

---

## 🧪 测试步骤

### 1. 低功耗唤醒测试

#### 准备工作
- 连接锂电池
- 连接万用表（测量电流）
- 插入SD卡

#### 测试流程
1. **上电启动**
   - 观察LED状态（蓝色快闪）
   - 查看串口输出

2. **等待进入低功耗**
   - 无操作30秒（默认超时）
   - 观察LED变为呼吸灯
   - 测量电流（应≤10mA）

3. **串口唤醒测试**
   - 通过UART发送数据
   - 观察LED恢复常亮
   - 查看串口输出"退出低功耗模式"

4. **按键唤醒测试**
   - 按下BOOT按键
   - 观察LED恢复
   - 查看串口输出

5. **网络唤醒测试**（服务器模式）
   - 用客户端连接服务器
   - 观察LED恢复

#### 验证要点
- ✅ 待机电流≤10mA（Light Sleep）
- ✅ 唤醒时间<5秒
- ✅ SD卡正常断电/上电
- ✅ WiFi正常重连

### 2. 串口双向通信测试

#### 准备工作
- 连接UART设备（或USB转TTL）
- 打开串口监视器（115200）

#### 测试流程

**1. UART → USB CDC**
```
UART发送: Hello World
USB接收: [UART] Hello World
```

**2. USB CDC → UART**
```
USB发送: Test Message
UART接收: Test Message
```

**3. 指令测试**
```
USB发送: CMD:QUERY|TYPE:STATUS
USB接收: RESP:STATUS|MODE:客户端|SD:正常|WIFI:已连接|POWER:正常|BATTERY:3.9V
```

**4. 波特率切换**
```
USB发送: CMD:CONFIG|BAUD:9600
USB接收: RESP:CONFIG|RESULT:成功|BAUD:9600
（重新打开串口监视器，波特率9600）
```

#### 验证要点
- ✅ 双向通信正常
- ✅ 指令解析正确
- ✅ 波特率切换成功
- ✅ LED指令闪烁

### 3. 双模式切换测试

#### 测试流程

**1. 客户端 → 服务器**
```
短按BOOT按键（<2秒）
→ LED白色闪烁5次
→ 系统重启
→ 进入服务器模式（青色慢闪）
```

**2. 服务器 → 客户端**
```
短按BOOT按键（<2秒）
→ LED白色闪烁5次
→ 系统重启
→ 进入客户端模式（蓝色快闪）
```

**3. 恢复默认**
```
长按BOOT按键（≥5秒）
→ LED红色闪烁10次
→ 系统重启
→ 恢复客户端模式
```

#### 验证要点
- ✅ 模式切换正确
- ✅ EEPROM保存成功
- ✅ 重启后模式正确

### 4. 无SD卡运行测试

#### 测试流程
1. **拔出SD卡**
2. **上电启动**
   - 查看串口输出："SD卡初始化失败 - 存储功能已禁用"
   - 观察LED白色快闪

3. **功能测试**
   - WiFi连接正常
   - 串口通信正常
   - USB CDC正常
   - 仅存储功能禁用

4. **插入SD卡**
   - 发送 `CMD:QUERY|TYPE:STATUS`
   - 查看SD状态："SD:正常"

#### 验证要点
- ✅ 无SD卡时系统正常运行
- ✅ LED正确提示SD卡状态
- ✅ 其他功能不受影响

### 5. 智能配网测试

#### 测试流程
1. **首次上电**（无WiFi配置）
   - 自动进入配网模式

2. **手动配网**
   - 发送 `CONFIG` 命令
   - 连接热点 `ESP32_Config`
   - 访问 192.168.4.1
   - 输入WiFi信息

3. **验证连接**
   - 查看串口输出
   - 观察LED状态

#### 验证要点
- ✅ 配网热点创建成功
- ✅ Web配置界面正常
- ✅ WiFi信息保存成功

---

## 🔋 功耗优化建议

### 硬件优化

#### 1. 电源管理
- **使用高效DC-DC转换器**（效率>90%）
- **选择低静态电流LDO**（<10μA）
- **添加电源开关**（完全断电外设）

#### 2. SD卡优化
- **使用SD卡电源控制**
  - GPIO控制MOSFET开关
  - 低功耗时完全断电

- **选择低功耗SD卡**
  - 容量不宜过大（4-16GB）
  - Class 10或更高

#### 3. LED优化
- **降低LED亮度**
  ```cpp
  FastLED.setBrightness(50);  // 降低到50%
  ```

- **使用单色LED替代RGB**
  - 功耗更低
  - 控制更简单

#### 4. 电池监测优化
- **使用分压电阻+MOSFET**
  - 仅在测量时通电
  - 减少静态功耗

### 软件优化

#### 1. CPU频率调整
```cpp
// 正常模式：240MHz
setCpuFrequencyMhz(240);

// 低功耗模式：80MHz
setCpuFrequencyMhz(80);

// 超低功耗：40MHz
setCpuFrequencyMhz(40);
```

#### 2. WiFi功耗优化
```cpp
// 客户端模式：完全断开
WiFi.disconnect(true);
WiFi.mode(WIFI_OFF);

// 服务器模式：省电模式
WiFi.setSleep(true);

// 降低发射功率
WiFi.setTxPower(WIFI_POWER_5dBm);  // 最低功率
```

#### 3. 外设断电
```cpp
// 关闭SD卡
powerOffSDCard();

// 关闭不用的外设
periph_module_disable(PERIPH_UART1_MODULE);
periph_module_disable(PERIPH_I2C0_MODULE);
periph_module_disable(PERIPH_SPI2_MODULE);
```

#### 4. 定时唤醒
```cpp
// Light Sleep：1秒唤醒一次
esp_sleep_enable_timer_wakeup(1000000);
esp_light_sleep_start();

// Deep Sleep：10秒唤醒一次
esp_sleep_enable_timer_wakeup(10000000);
esp_deep_sleep_start();
```

#### 5. 数据缓存优化
```cpp
// 减少SD卡写入频率
#define SD_WRITE_INTERVAL  5000  // 5秒写入一次

// 批量写入
String dataBuffer[100];
int bufferIndex = 0;

void bufferData(String data) {
  dataBuffer[bufferIndex++] = data;
  
  if (bufferIndex >= 100) {
    flushBuffer();
  }
}

void flushBuffer() {
  if (sdCardReady) {
    File file = SD.open("/buffer.txt", FILE_APPEND);
    for (int i = 0; i < bufferIndex; i++) {
      file.println(dataBuffer[i]);
    }
    file.close();
    bufferIndex = 0;
  }
}
```

### 功耗测试数据

| 模式 | CPU频率 | WiFi状态 | SD卡 | 电流（mA） |
|------|---------|----------|------|-----------|
| 正常工作 | 240MHz | 开启 | 开启 | 80-120 |
| 正常工作 | 240MHz | 开启 | 关闭 | 70-100 |
| 低功耗 | 80MHz | 省电 | 关闭 | 30-50 |
| Light Sleep | 80MHz | 关闭 | 关闭 | 5-10 |
| Deep Sleep | 关闭 | 关闭 | 关闭 | 0.01-0.1 |

### 功耗优化检查清单

- [ ] 降低CPU频率（80MHz）
- [ ] WiFi省电模式或关闭
- [ ] SD卡断电
- [ ] LED亮度降低
- [ ] 关闭不用的外设
- [ ] 使用Light Sleep
- [ ] 减少SD卡写入频率
- [ ] 批量数据处理
- [ ] 定时唤醒策略
- [ ] 电池监测优化

---

## ❓ 常见问题

### 1. 低功耗无法唤醒

**现象**：进入低功耗后无法唤醒

**排查步骤**：
1. 检查串口唤醒配置
2. 检查按键引脚
3. 查看串口输出日志

**解决方案**：
```cpp
// 确保串口唤醒已配置
uart_enable_pattern_det_baud_intr(UART_NUM_2, '+', 1, 9, 0, 0);

// 确保按键唤醒已配置
esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);
```

### 2. 电池电量显示不准

**现象**：电池电压读数异常

**排查步骤**：
1. 检查分压电路
2. 校准ADC
3. 测量实际电压

**解决方案**：
```cpp
// ADC校准
esp_adc_cal_characteristics_t adc_chars;
esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);

// 读取校准后的电压
uint32_t voltage = esp_adc_cal_raw_to_voltage(analogRead(BATTERY_ADC_PIN), &adc_chars);
```

### 3. SD卡写入失败

**现象**：SD卡初始化成功但无法写入

**排查步骤**：
1. 检查SD卡写保护
2. 检查文件路径
3. 检查SD卡容量

**解决方案**：
```cpp
// 检查SD卡状态
if (SD.cardType() == CARD_NONE) {
  Serial.println("SD卡未检测到");
}

// 检查剩余空间
uint64_t totalBytes = SD.totalBytes();
uint64_t usedBytes = SD.usedBytes();
Serial.printf("剩余空间: %llu MB\n", (totalBytes - usedBytes) / (1024 * 1024));
```

### 4. WiFi连接不稳定

**现象**：WiFi频繁断开

**排查步骤**：
1. 检查WiFi信号强度
2. 检查路由器设置
3. 检查电源供电

**解决方案**：
```cpp
// 设置自动重连
WiFi.setAutoReconnect(true);
WiFi.setMinSecurity(WIFI_AUTH_WEP);  // 降低安全级别

// 检查信号强度
int rssi = WiFi.RSSI();
if (rssi < -80) {
  Serial.println("WiFi信号弱");
}
```

### 5. 串口数据丢失

**现象**：高速数据传输时丢失数据

**排查步骤**：
1. 检查波特率
2. 检查缓冲区大小
3. 检查数据处理速度

**解决方案**：
```cpp
// 增大串口缓冲区
Serial2.setRxBufferSize(2048);

// 使用中断处理
void IRAM_ATTR onSerialData() {
  // 快速读取数据
}

// 提高处理优先级
xTaskCreate(serialTask, "SerialTask", 4096, NULL, 10, NULL);
```

---

## 📊 性能参数

| 参数 | 数值 |
|------|------|
| 最大客户端数 | 5个（服务器模式） |
| UART缓冲区 | 2048字节 |
| 波特率范围 | 9600-921600 |
| 低功耗超时 | 10-300秒（可配置） |
| 待机电流 | ≤10mA（Light Sleep） |
| 深度睡眠电流 | ≤100μA |
| 唤醒时间 | <5秒 |
| SD卡容量 | 最大32GB（FAT32） |

---

## 📝 更新日志

### v2.0-enhanced
- ✅ 新增低功耗待机功能
- ✅ 新增串口双向通信
- ✅ 新增智能配网
- ✅ 新增波特率动态配置
- ✅ 新增电池电量监测
- ✅ 新增多状态LED指示
- ✅ 新增电脑指令解析
- ✅ 优化SD卡容错处理
- ✅ 优化功耗管理

### v1.0
- 初始版本
- 双模式切换
- TCP通信
- SD卡存储
- USB CDC交互

---

## 📞 技术支持

如有问题，请检查：
1. 串口输出日志
2. LED状态指示
3. SD卡存储状态
4. 网络连接状态
5. 电池电量状态

---

## 📄 许可证

本项目开源，仅供学习和研究使用。