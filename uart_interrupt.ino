// ==================== UART DMA Transparent Mode ====================
#include "driver/uart.h"

static QueueHandle_t uartQueue = NULL;
static TaskHandle_t uartTaskHandle = NULL;
bool uartDmaInitialized = false;

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
  if (tcpSendBufferLen < TCP_SEND_BUFFER_SIZE) {
    tcpSendBuffer[tcpSendBufferLen++] = byte;
  }
}

// DMA接收任务
void uartRxTask(void *arg) {
  uart_event_t event;
  static String uartLineBuffer = "";
  
  while (true) {
    if (xQueueReceive(uartQueue, &event, portMAX_DELAY) == pdTRUE) {
      if (event.type == UART_DATA) {
        uint8_t *buf = (uint8_t *)malloc(event.size);
        if (buf) {
          int len = uart_read_bytes(UART_NUM_2, buf, event.size, 0);
          if (len > 0) {
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
            
            // 写入Serial
            Serial.write((char*)filteredBuf, filteredLen);
            
            // 写入Web串口显示缓冲区（只在非透传模式下写入）
            if (selectedClientIndex < 0) {
              appendToSerialBuffer((char*)filteredBuf, filteredLen);
            }
            
            // 写入TCP缓冲区（服务器模式转发到客户端，客户端模式转发到服务器）
            if (currentMode == MODE_SERVER || (currentMode == MODE_CLIENT && tcpConnected)) {
              for (int i = 0; i < filteredLen; i++) {
                if (tcpSendBufferLen < TCP_SEND_BUFFER_SIZE) {
                  tcpSendBuffer[tcpSendBufferLen++] = filteredBuf[i];
                }
              }
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
          free(buf);
        }
      } else if (event.type == UART_FIFO_OVF_ERROR) {
        Serial.println("UART DMA overflow!");
      } else if (event.type == UART_BUFFER_FULL) {
        Serial.println("DMA buffer full!");
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
      Serial.println("UART DMA install failed: " + String(err));
      return;
    }
  }

  // 更新波特率
  uart_param_config(UART_NUM_2, &uart_config);

  if (!uartDmaInitialized) {
    esp_err_t err = uart_set_pin(UART_NUM_2, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
      Serial.println("UART DMA pin failed: " + String(err));
      return;
    }

    // 创建DMA接收任务（绑定到CPU核心1）
    xTaskCreatePinnedToCore(uartRxTask, "uart_rx", 4096, NULL, 12, &uartTaskHandle, 1);
    
    uartDmaInitialized = true;
    Serial.println("UART DMA mode enabled!");
  }  // 始终打印调试信息
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
  if (tcpSendBufferLen == 0) return;
  
  if (currentMode == MODE_SERVER) {
    if (selectedClientIndex >= 0 && selectedClientIndex < MAX_CLIENTS) {
      if (serverClients[selectedClientIndex] && serverClients[selectedClientIndex].connected()) {
        serverClients[selectedClientIndex].write(tcpSendBuffer, tcpSendBufferLen);
      } else {
        selectedClientIndex = -1;
      }
    } else {
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (serverClients[i] && serverClients[i].connected()) {
          serverClients[i].write(tcpSendBuffer, tcpSendBufferLen);
        }
      }
    }
  } else if (currentMode == MODE_CLIENT && tcpConnected) {
    int sent = tcpClient.write(tcpSendBuffer, tcpSendBufferLen);
    if (sent <= 0) {
      tcpConnected = false;
    }
  }
  
  tcpSendBufferLen = 0;
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
    Serial.println("  Client IP: " + serverClients[index].remoteIP().toString());
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
      Serial.println("  " + String(i) + ": " + serverClients[i].remoteIP().toString() + selected);
    }
  }
  
  if (selectedClientIndex >= 0) {
    Serial.println("\nCurrent: Client " + String(selectedClientIndex));
  } else {
    Serial.println("\nCurrent: Forwarding to all clients");
  }
}