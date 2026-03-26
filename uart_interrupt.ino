// ==================== UART Interrupt Transparent Mode ====================

#define UART_RING_BUFFER_SIZE 8192

volatile uint8_t uart2RingBuffer[UART_RING_BUFFER_SIZE];
volatile uint16_t uart2RingHead = 0;
volatile uint16_t uart2RingTail = 0;
volatile bool uart2RingOverflow = false;

#define TCP_SEND_BUFFER_SIZE 1024
uint8_t tcpSendBuffer[TCP_SEND_BUFFER_SIZE];
uint16_t tcpSendBufferLen = 0;

int selectedClientIndex = -1;

void IRAM_ATTR uart2RxISR() {
  while (Serial2.available()) {
    uint16_t nextHead = (uart2RingHead + 1) % UART_RING_BUFFER_SIZE;
    if (nextHead != uart2RingTail) {
      uart2RingBuffer[uart2RingHead] = Serial2.read();
      uart2RingHead = nextHead;
    } else {
      uart2RingOverflow = true;
      Serial2.read();
    }
  }
}

void initUARTInterrupt() {
  Serial2.onReceive(uart2RxISR);
  if (debugMode) {
    Serial.println("UART interrupt mode enabled");
  }
}

int readUART2Buffer() {
  if (uart2RingHead == uart2RingTail) {
    return -1;
  }
  uint8_t data = uart2RingBuffer[uart2RingTail];
  uart2RingTail = (uart2RingTail + 1) % UART_RING_BUFFER_SIZE;
  return data;
}

int peekUART2Buffer() {
  if (uart2RingHead == uart2RingTail) {
    return -1;
  }
  return uart2RingBuffer[uart2RingTail];
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
      }
    } else {
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (serverClients[i] && serverClients[i].connected()) {
          serverClients[i].write(tcpSendBuffer, tcpSendBufferLen);
        }
      }
    }
  } else if (currentMode == MODE_CLIENT && tcpConnected) {
    tcpClient.write(tcpSendBuffer, tcpSendBufferLen);
  }
  
  tcpSendBufferLen = 0;
}

void handleHighSpeedUART() {
  int ch;
  static bool lastWasCR = false;
  
  while ((ch = readUART2Buffer()) >= 0) {
    if (ch == '\r') {
      lastWasCR = true;
      tcpSendBuffer[tcpSendBufferLen++] = (uint8_t)ch;
    } else if (ch == '\n') {
      Serial.print("\r\n");
      tcpSendBuffer[tcpSendBufferLen++] = (uint8_t)ch;
      lastWasCR = false;
    } else {
      Serial.write((uint8_t)ch);
      tcpSendBuffer[tcpSendBufferLen++] = (uint8_t)ch;
      lastWasCR = false;
    }
    
    if (tcpSendBufferLen >= TCP_SEND_BUFFER_SIZE) {
      flushTCPBuffer();
    }
  }
  
  flushTCPBuffer();
  
  if (uart2RingOverflow) {
    Serial.println("\nUART2 buffer overflow");
    uart2RingOverflow = false;
  }
}

void handleHighSpeedUARTWithWebBuffer() {
  static unsigned long lastCheck = 0;
  
  if (millis() - lastCheck > 3000) {
    lastCheck = millis();
  }
  
  int ch;
  static bool lastWasCR = false;
  
  while ((ch = readUART2Buffer()) >= 0) {
    // \r或\n都产生新行
    if (ch == '\r') {
      Serial.write('\r');
      Serial.write('\n');
      tcpSendBuffer[tcpSendBufferLen++] = (uint8_t)ch;
      appendToSerialBuffer("\r\n");
      lastWasCR = true;
    } else if (ch == '\n') {
      if (!lastWasCR) {
        Serial.write('\r');
        Serial.write('\n');
        tcpSendBuffer[tcpSendBufferLen++] = '\r';
        appendToSerialBuffer("\r\n");
      } else {
        Serial.write('\n');
      }
      tcpSendBuffer[tcpSendBufferLen++] = (uint8_t)ch;
      appendToSerialBuffer("\n");
      lastWasCR = false;
    } else {
      Serial.write((uint8_t)ch);
      tcpSendBuffer[tcpSendBufferLen++] = (uint8_t)ch;
      appendToSerialBuffer((char)ch);
      lastWasCR = false;
    }
    
    if (tcpSendBufferLen >= TCP_SEND_BUFFER_SIZE) {
      flushTCPBuffer();
    }
  }
  
  flushTCPBuffer();
  
  if (uart2RingOverflow) {
    Serial.println("\nUART2 buffer overflow");
    uart2RingOverflow = false;
  }
}

// Select client for transparent mode
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

// Display selectable client list
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
