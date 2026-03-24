// ==================== UART Interrupt Transparent Mode ====================
// Use interrupt to improve serial transparent speed

// Ring buffer size
#define UART_RING_BUFFER_SIZE 2048

// UART2 ring buffer (interrupt driven)
volatile uint8_t uart2RingBuffer[UART_RING_BUFFER_SIZE];
volatile uint16_t uart2RingHead = 0;
volatile uint16_t uart2RingTail = 0;
volatile bool uart2RingOverflow = false;

// Client transparent selection
int selectedClientIndex = -1;  // -1: not selected, 0-4: client index

// ISR - UART2 receive
void IRAM_ATTR uart2RxISR() {
  while (Serial2.available()) {
    uint16_t nextHead = (uart2RingHead + 1) % UART_RING_BUFFER_SIZE;
    
    if (nextHead != uart2RingTail) {
      uart2RingBuffer[uart2RingHead] = Serial2.read();
      uart2RingHead = nextHead;
    } else {
      // Buffer overflow
      uart2RingOverflow = true;
      Serial2.read();  // Discard data
    }
  }
}

// Initialize UART interrupt
void initUARTInterrupt() {
  // Enable UART2 receive interrupt
  Serial2.onReceive(uart2RxISR);
  
  if (debugMode) {
    Serial.println("UART interrupt mode enabled");
  }
}

// Read data from UART2 ring buffer
int readUART2Buffer() {
  if (uart2RingHead == uart2RingTail) {
    return -1;  // Buffer empty
  }
  
  uint8_t data = uart2RingBuffer[uart2RingTail];
  uart2RingTail = (uart2RingTail + 1) % UART_RING_BUFFER_SIZE;
  return data;
}

// Check if UART2 buffer has data
bool uart2BufferAvailable() {
  return uart2RingHead != uart2RingTail;
}

// Get UART2 buffer data length
int uart2BufferLength() {
  if (uart2RingHead >= uart2RingTail) {
    return uart2RingHead - uart2RingTail;
  } else {
    return UART_RING_BUFFER_SIZE - uart2RingTail + uart2RingHead;
  }
}

// High speed UART transparent handling (interrupt driven)
void handleHighSpeedUART() {
  // Handle UART2 receive data - output directly like Xshell
  if (uart2BufferAvailable()) {
    // Output characters directly, don't wait for newline
    while (uart2BufferAvailable()) {
      int ch = readUART2Buffer();
      if (ch >= 0) {
        // Output to debug serial directly
        Serial.write((uint8_t)ch);
        
        // Server mode: forward to selected client or all clients
        if (currentMode == MODE_SERVER) {
          if (selectedClientIndex >= 0 && selectedClientIndex < MAX_CLIENTS) {
            // Forward to selected client
            if (serverClients[selectedClientIndex] && serverClients[selectedClientIndex].connected()) {
              serverClients[selectedClientIndex].write((uint8_t)ch);
            }
          } else {
            // Forward to all clients
            for (int i = 0; i < MAX_CLIENTS; i++) {
              if (serverClients[i] && serverClients[i].connected()) {
                serverClients[i].write((uint8_t)ch);
              }
            }
          }
        }
        
        // Client mode: forward to server
        if (currentMode == MODE_CLIENT && tcpConnected) {
          tcpClient.write((uint8_t)ch);
        }
      }
    }
  }
  
  // Check overflow flag
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
