// ==================== UART DMA Transparent Mode ====================
#include "driver/uart.h"

static QueueHandle_t uartQueue = NULL;
static TaskHandle_t uartTaskHandle = NULL;
bool uartDmaInitialized = false;
volatile bool uart2StartupForwardingEnabled = false;
portMUX_TYPE tcpSendBufferMux = portMUX_INITIALIZER_UNLOCKED;

#define UART_DMA_RX_BUF_SIZE 8192
#define UART_DMA_TX_BUF_SIZE 2048

// 环形缓冲区用于TCP发送
#define UART_RING_BUFFER_SIZE 16384
volatile uint8_t uart2RingBuffer[UART_RING_BUFFER_SIZE];
volatile uint16_t uart2RingHead = 0;
volatile uint16_t uart2RingTail = 0;
volatile bool uart2RingOverflow = false;

#define TCP_SEND_BUFFER_SIZE 1024
uint8_t tcpSendBuffer[TCP_SEND_BUFFER_SIZE];
uint16_t tcpSendBufferLen = 0;

int selectedClientIndex = -1;

void queueTCPWrite(uint8_t byte) {
  portENTER_CRITICAL(&tcpSendBufferMux);
  if (tcpSendBufferLen < TCP_SEND_BUFFER_SIZE) {
    tcpSendBuffer[tcpSendBufferLen++] = byte;
  }
  portEXIT_CRITICAL(&tcpSendBufferMux);
}

// DMA接收任务
void uartRxTask(void *arg) {
  uart_event_t event;
  static String uartLineBuffer = "";
  static uint8_t buf[UART_DMA_RX_BUF_SIZE];
  
  while (true) {
    if (xQueueReceive(uartQueue, &event, portMAX_DELAY) == pdTRUE) {
      if (event.type == UART_DATA) {
        int toRead = event.size;
        if (toRead > UART_DMA_RX_BUF_SIZE) {
          toRead = UART_DMA_RX_BUF_SIZE;
        }

        int len = uart_read_bytes(UART_NUM_2, buf, toRead, 0);
        if (len > 0) {
          if (!uart2StartupForwardingEnabled) {
            continue;
          }

          // 过滤ANSI转义序列到临时缓冲区
          static uint8_t filteredBuf[1024];
          int filteredLen = 0;
          for (int i = 0; i < len && filteredLen < 1024; i++) {
            if (buf[i] == 0x1B) {
              int j = i + 1;
              if (j < len && buf[j] == '[') {
                j++;
                while (j < len) {
                  char c = buf[j];
                  if ((c >= '0' && c <= '9') || c == ';' || c == '?' || c == '>' || c == '<' || c == '=') {
                    j++;
                  } else {
                    break;
                  }
                }
                if (j < len && buf[j] >= 0x40 && buf[j] <= 0x7E) {
                  j++;
                }
                i = j - 1;
                continue;
              }
            }
            filteredBuf[filteredLen++] = buf[i];
          }
          
          // USB串口保持纯透传，不能过滤ANSI控制序列，否则Linux终端/htop会乱码
          Serial.write((char*)buf, len);
          
          // 写入Web串口显示缓冲区（只在非透传模式下写入）
          if (selectedClientIndex < 0) {
            appendToSerialBuffer((char*)filteredBuf, filteredLen);
          }
          
          // TCP透传同样保持原始数据，Web/日志才使用过滤后的内容
          if (currentMode == MODE_SERVER || (currentMode == MODE_CLIENT && tcpConnected)) {
            portENTER_CRITICAL(&tcpSendBufferMux);
            for (int i = 0; i < len; i++) {
              if (tcpSendBufferLen < TCP_SEND_BUFFER_SIZE) {
                tcpSendBuffer[tcpSendBufferLen++] = buf[i];
              }
            }
            portEXIT_CRITICAL(&tcpSendBufferMux);
          }
          
          // 客户端模式：记录发送的日志到SD卡
          if (currentMode == MODE_CLIENT && logToSD && sdCardReady) {
            for (int i = 0; i < filteredLen; i++) {
              char c = filteredBuf[i];
              if (c == '\n') {
                if (uartLineBuffer.length() > 1) {
                  enqueueSDLog(uartLineBuffer, client_id, false);
                }
                uartLineBuffer = "";
              } else if (c != '\r') {
                uartLineBuffer += c;
              }
            }
          }
        }
      } else if (event.type == UART_FIFO_OVF_ERROR) {
        static unsigned long lastOverflowPrint = 0;
        if (debugMode && millis() - lastOverflowPrint > 5000) {
          Serial.println("UART DMA overflow!");
          lastOverflowPrint = millis();
        }
      } else if (event.type == UART_BUFFER_FULL) {
        static unsigned long lastBufFullPrint = 0;
        if (debugMode && millis() - lastBufFullPrint > 5000) {
          Serial.println("DMA buffer full!");
          lastBufFullPrint = millis();
        }
      }
    }
  }
}

void initUARTInterrupt(bool reinstall) {
  // 配置UART参数（使用配置文件中的波特率）
  uart_config_t uart_config = {
    .baud_rate = uart2BaudRate,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };

  if (reinstall || !uartDmaInitialized) {
    if (uartDmaInitialized) {
      uart_driver_delete(UART_NUM_2);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // 安装UART驱动（DMA模式）
    esp_err_t err = uart_driver_install(UART_NUM_2, 
      UART_DMA_RX_BUF_SIZE, 
      UART_DMA_TX_BUF_SIZE,
      32, 
      &uartQueue, 
      0);
    
    if (err != ESP_OK) {
      if (debugMode) {
        Serial.println("UART DMA install failed: " + String(err));
      }
      return;
    }
  }

  // 更新波特率
  uart_param_config(UART_NUM_2, &uart_config);
  uart_set_rx_full_threshold(UART_NUM_2, 16);
  uart_set_rx_timeout(UART_NUM_2, 2);

  if (!uartDmaInitialized) {
    esp_err_t err = uart_set_pin(UART_NUM_2, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
      if (debugMode) {
        Serial.println("UART DMA pin failed: " + String(err));
      }
      return;
    }

    // 创建DMA接收任务（绑定到CPU核心1）
    xTaskCreatePinnedToCore(uartRxTask, "uart_rx", 4096, NULL, 12, &uartTaskHandle, 1);
    
    uartDmaInitialized = true;
    if (debugMode) {
      Serial.println("UART DMA mode enabled!");
    }
  }  // 始终打印调试信息
}

void finishUART2StartupForwarding() {
  if (uartDmaInitialized) {
    uart_flush_input(UART_NUM_2);
  }

  uart2StartupForwardingEnabled = true;
}

// 备用：中断模式读取函数（当DMA不可用时）
int readUART2Buffer() {
  if (uart2RingHead == uart2RingTail) {
    return -1;
  }
  uint8_t data = uart2RingBuffer[uart2RingTail];
  uart2RingTail = (uart2RingTail + 1) % UART_RING_BUFFER_SIZE;
  return data;
}

bool uart2BufferAvailable() {
  return uart2RingHead != uart2RingTail;
}

int uart2BufferLength() {
  if (uart2RingHead >= uart2RingTail) {
    return uart2RingHead - uart2RingTail;
  } else {
    return UART_RING_BUFFER_SIZE - uart2RingTail + uart2RingHead;
  }
}

void flushTCPBuffer() {
  uint8_t localBuffer[TCP_SEND_BUFFER_SIZE];
  uint16_t localLen = 0;

  portENTER_CRITICAL(&tcpSendBufferMux);
  if (tcpSendBufferLen > 0) {
    localLen = tcpSendBufferLen;
    memcpy(localBuffer, tcpSendBuffer, localLen);
    tcpSendBufferLen = 0;
  }
  portEXIT_CRITICAL(&tcpSendBufferMux);

  if (localLen == 0) return;

  String payload = "";
  payload.reserve(SECURITY_MAX_TRANSPARENT_PAYLOAD);

  auto sendPayloadChunk = [&](const String &chunk) {
    if (chunk.length() == 0) {
      return;
    }

    if (currentMode == MODE_SERVER) {
      if (selectedClientIndex >= 0 && selectedClientIndex < MAX_CLIENTS) {
        if (serverClients[selectedClientIndex] && serverClients[selectedClientIndex].connected()) {
          sendFramedPayloadToClient(serverClients[selectedClientIndex], chunk);
        } else {
          selectedClientIndex = -1;
        }
      } else {
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (serverClients[i] && serverClients[i].connected()) {
            sendFramedPayloadToClient(serverClients[i], chunk);
          }
        }
      }
    } else if (currentMode == MODE_CLIENT && tcpConnected) {
      if (!sendFramedPayloadToClient(tcpClient, chunk)) {
        tcpConnected = false;
      }
    }
  };
  
  for (uint16_t i = 0; i < localLen; i++) {
    char value = (char)localBuffer[i];
    if (!isAllowedPayloadCharacter(value)) {
      sendPayloadChunk(payload);
      payload = "";
      continue;
    }

    payload += value;
    if (payload.length() >= SECURITY_MAX_TRANSPARENT_PAYLOAD) {
      sendPayloadChunk(payload);
      payload = "";
    }
  }

  sendPayloadChunk(payload);
}

// 主循环调用（刷新TCP缓冲区）
void handleHighSpeedUARTWithWebBuffer() {
  flushTCPBuffer();
}

void selectClient(int index) {
  if (currentMode != MODE_SERVER) {
    Serial.println("Only available in server mode");
    return;
  }
  
  if (index < -1 || index >= MAX_CLIENTS) {
    Serial.println("Invalid client index");
    return;
  }
  
  if (index >= 0 && (!serverClients[index] || !serverClients[index].connected())) {
    Serial.println("Client not connected");
    return;
  }
  
  selectedClientIndex = index;
  
  if (index == -1) {
    Serial.println("Client selection cancelled, forwarding to all clients");
  } else {
    Serial.println("Selected client " + String(index) + " for transparent mode");
    Serial.println("  Client IP: " + maskIpAddress(serverClients[index].remoteIP()));
  }
}

// ==================== UART1 DMA 独立接收（IO19/IO20）====================
static QueueHandle_t uart1Queue = NULL;
static TaskHandle_t uart1TaskHandle = NULL;
bool uart1DmaInitialized = false;

// UART1 DMA 接收任务（绑定到 Core 1，与 UART2 任务独立）
void uartRxTask1(void *arg) {
  uart_event_t event;
  static String uart1LineBuffer = "";
  static uint8_t buf[UART_DMA_RX_BUF_SIZE];

  while (true) {
    if (xQueueReceive(uart1Queue, &event, portMAX_DELAY) == pdTRUE) {
      if (event.type == UART_DATA) {
        int toRead = event.size;
        if (toRead > UART_DMA_RX_BUF_SIZE) toRead = UART_DMA_RX_BUF_SIZE;

        int len = uart_read_bytes(UART_NUM_1, buf, toRead, 0);
        if (len <= 0) continue;

        // 过滤 ANSI 转义序列（用于 Web 显示和 SD 存储）
        static uint8_t filteredBuf[1024];
        int filteredLen = 0;
        for (int i = 0; i < len && filteredLen < 1024; i++) {
          if (buf[i] == 0x1B) {
            int j = i + 1;
            if (j < len && buf[j] == '[') {
              j++;
              while (j < len) {
                char c = buf[j];
                if ((c >= '0' && c <= '9') || c == ';' || c == '?' ||
                    c == '>' || c == '<' || c == '=') {
                  j++;
                } else {
                  break;
                }
              }
              if (j < len && buf[j] >= 0x40 && buf[j] <= 0x7E) j++;
              i = j - 1;
              continue;
            }
          }
          filteredBuf[filteredLen++] = buf[i];
        }

        // 写入 UART1 Web 显示缓冲区（独立于 UART2）
        appendToSerial1Buffer((char*)filteredBuf, filteredLen);

        // 逐行写入 SD 卡（通道标识为 1）
        if (logToSD && sdCardReady) {
          for (int i = 0; i < filteredLen; i++) {
            char c = filteredBuf[i];
            if (c == '\n') {
              if (uart1LineBuffer.length() > 1) {
                enqueueSDLog(uart1LineBuffer, "uart1", false, 1);
              }
              uart1LineBuffer = "";
            } else if (c != '\r') {
              uart1LineBuffer += c;
            }
          }
        }
      } else if (event.type == UART_FIFO_OVF_ERROR || event.type == UART_BUFFER_FULL) {
        static unsigned long lastWarn1 = 0;
        if (debugMode && millis() - lastWarn1 > 5000) {
          Serial.println("UART1 DMA buffer warning!");
          lastWarn1 = millis();
        }
      }
    }
  }
}

void initUART1Interrupt(bool reinstall) {
  uart_config_t uart_config = {
    .baud_rate = (int)uart1BaudRate,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };

  if (reinstall || !uart1DmaInitialized) {
    if (uart1DmaInitialized) {
      uart_driver_delete(UART_NUM_1);
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    esp_err_t err = uart_driver_install(UART_NUM_1,
      UART_DMA_RX_BUF_SIZE,
      UART_DMA_TX_BUF_SIZE,
      32,
      &uart1Queue,
      0);

    if (err != ESP_OK) {
      if (debugMode) Serial.println("UART1 DMA install failed: " + String(err));
      return;
    }
  }

  uart_param_config(UART_NUM_1, &uart_config);
  uart_set_rx_full_threshold(UART_NUM_1, 16);
  uart_set_rx_timeout(UART_NUM_1, 2);

  if (!uart1DmaInitialized) {
    // TX=IO20, RX=IO19
    esp_err_t err = uart_set_pin(UART_NUM_1,
      UART1_TX_PIN, UART1_RX_PIN,
      UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
      if (debugMode) Serial.println("UART1 DMA pin failed: " + String(err));
      return;
    }

    // 优先级略低于 UART2 任务（11 vs 12），同绑 Core 1
    xTaskCreatePinnedToCore(uartRxTask1, "uart1_rx", 4096, NULL, 11, &uart1TaskHandle, 1);

    uart1DmaInitialized = true;
    if (debugMode) Serial.println("UART1 DMA mode enabled! (IO" +
      String(UART1_RX_PIN) + "/" + String(UART1_TX_PIN) + ")");
  }
}

void listSelectableClients() {
  if (currentMode != MODE_SERVER) {
    Serial.println("Only available in server mode");
    return;
  }
  
  Serial.println("\nSelectable clients:");
  Serial.println("  -1: Cancel selection (forward to all clients)");
  
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (serverClients[i] && serverClients[i].connected()) {
      String selected = (selectedClientIndex == i) ? " [Selected]" : "";
      Serial.println("  " + String(i) + ": " + maskIpAddress(serverClients[i].remoteIP()) + selected);
    }
  }
  
  if (selectedClientIndex >= 0) {
    Serial.println("\nCurrent: Client " + String(selectedClientIndex));
  } else {
    Serial.println("\nCurrent: Forwarding to all clients");
  }
}
