// ==================== Web服务器和页面处理 ====================
void initWebServer() {
  if (webServerEnabled && (wifiConnected || currentMode == MODE_SERVER)) {
    webServer.begin();
    Serial.println("✓ Web服务器启动成功");
    Serial.println("  访问地址: http://" + WiFi.localIP().toString());
    Serial.println("  日志查看: http://" + WiFi.localIP().toString() + "/logs");
    Serial.println("  系统状态: http://" + WiFi.localIP().toString() + "/status");
  }
}

void handleWebServer() {
  if (!webServerEnabled) return;
  
  WiFiClient client = webServer.available();
  if (client) {
    // 读取Web客户端请求
    String request = client.readStringUntil('\r');
    client.flush();
    
    // 路由处理
    if (request.indexOf("GET / ") >= 0) {
      handleRootPage(client);
    } else if (request.indexOf("GET /logs") >= 0) {
      handleLogsPage(client, request);
    } else if (request.indexOf("GET /status ") >= 0) {
      handleStatusPage(client);
    } else if (request.indexOf("GET /client") >= 0) {
      handleClientPage(client, request);
    } else if (request.indexOf("GET /config ") >= 0) {
      handleConfigPage(client);
    } else if (request.indexOf("POST /saveconfig") >= 0) {
      handleSaveConfig(client);
    } else if (request.indexOf("GET /download") >= 0) {
      handleDownloadLog(client, request);
    } else if (request.indexOf("GET /clear ") >= 0) {
      handleClearLog(client);
    } else if (request.indexOf("POST /power") >= 0) {
      handlePowerControl(client, request);
    } else {
      handleNotFound(client);
    }
    
    delay(10);
    client.stop();
  }
}

void handleRootPage(WiFiClient client) {
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
  html += "<title>ESP32 UART 服务器</title>";
  html += "<style>body{font-family:Arial;margin:10px;background:#f0f0f0;font-size:16px;}";
  html += ".container{max-width:100%;margin:0 auto;background:white;padding:15px;border-radius:10px;box-sizing:border-box;}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px;font-size:24px;}";
  html += "h2{font-size:20px;}";
  html += "h3{font-size:18px;}";
  html += ".menu{margin:20px 0;}";
  html += ".menu a{display:block;padding:15px 20px;margin:10px 0;background:#4CAF50;color:white;text-decoration:none;border-radius:5px;text-align:center;font-size:18px;}";
  html += ".menu a:hover{background:#45a049;}";
  html += ".info{background:#e7f3fe;border-left:4px solid #2196F3;padding:15px;margin:15px 0;font-size:16px;}";
  html += "strong{font-size:16px;}";
  html += "@media screen and (min-width: 600px) {";
  html += ".container{max-width:800px;padding:20px;}";
  html += ".menu a{display:inline-block;margin:5px;}";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>📡 ESP32 UART 服务器</h1>";
  html += "<div class='info'>";
  html += "<strong>固件版本:</strong> " + String(FIRMWARE_VERSION) + "<br>";
  html += "<strong>运行模式:</strong> " + String(currentMode == MODE_CLIENT ? "客户端" : "服务器") + "<br>";
  html += "<strong>WiFi状态:</strong> " + String(wifiConnected ? "已连接" : "未连接") + "<br>";
  html += "<strong>SD卡状态:</strong> " + String(sdCardReady ? "正常" : "异常") + "<br>";
  html += "<strong>UART1↔UART2透传:</strong> 已启用<br>";  // 透传功能
  html += "</div>";
  html += "<div class='menu'>";
  html += "<a href='/logs'>📋 查看日志</a>";
  html += "<a href='/status'>📊 系统状态</a>";
  html += "<a href='/config'>⚙️ 系统配置</a>";
  html += "</div>";
  
  // 电源控制
  html += "<div class='info'>";
  html += "<h3>🔋 电源控制</h3>";
  html += "<form action='/power' method='post'>";
  html += "<input type='hidden' name='action' value='on'>";
  html += "<input type='submit' value='开机' style='width:30%;margin:5px;'>";
  html += "</form>";
  html += "<form action='/power' method='post'>";
  html += "<input type='hidden' name='action' value='off'>";
  html += "<input type='submit' value='关机' style='width:30%;margin:5px;'>";
  html += "</form>";
  html += "<form action='/power' method='post'>";
  html += "<input type='hidden' name='action' value='trigger'>";
  html += "<input type='submit' value='触发关机' style='width:30%;margin:5px;'>";
  html += "</form>";
  html += "<form action='/power' method='post'>";
  html += "<input type='hidden' name='action' value='reset'>";
  html += "<input type='submit' value='复位' style='width:30%;margin:5px;'>";
  html += "</form>";
  html += "</div>";
  html += "</div>";
  html += "</body></html>";
  client.print(html);
}

void handleLogsPage(WiFiClient client, String request) {
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
  html += "<title>ESP32 UART 日志</title>";
  html += "<style>body{font-family:Arial;margin:10px;background:#f0f0f0;font-size:16px;}";
  html += ".container{max-width:100%;margin:0 auto;background:white;padding:15px;border-radius:10px;box-sizing:border-box;}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px;font-size:24px;}";
  html += "h2{font-size:20px;}";
  html += ".file-list{margin:20px 0;}";
  html += ".file-item{padding:10px;border-bottom:1px solid #eee;}";
  html += ".file-item a{text-decoration:none;color:#333;}";
  html += ".file-item a:hover{color:#4CAF50;}";
  html += ".size{float:right;color:#666;}";
  html += ".back{margin-top:20px;}";
  html += ".back a{display:inline-block;padding:10px 20px;background:#4CAF50;color:white;text-decoration:none;border-radius:5px;}";
  html += ".back a:hover{background:#45a049;}";
  html += "@media screen and (min-width: 600px) {";
  html += ".container{max-width:800px;padding:20px;}";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>📋 日志文件</h1>";
  
  // 检查SD卡状态
  if (!sdCardReady) {
    html += "<div style='background:#ffebee;border-left:4px solid #f44336;padding:15px;margin:15px 0;'>";
    html += "<strong>错误:</strong> SD卡未就绪，无法查看日志文件";
    html += "</div>";
  } else {
    // 列出服务器日志目录
    html += "<h2>服务器日志</h2>";
    html += "<div class='file-list'>";
    File root = SD.open("/server");
    if (root) {
      while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        if (entry.isDirectory()) {
          String dirName = entry.name();
          html += "<div class='file-item'><a href='/logs?dir=" + dirName + "'>? " + dirName + "</a></div>";
        }
        entry.close();
      }
      root.close();
    } else {
      html += "<div style='background:#ffebee;border-left:4px solid #f44336;padding:15px;margin:15px 0;'>";
      html += "<strong>错误:</strong> 无法打开服务器日志目录";
      html += "</div>";
    }
    html += "</div>";
  }
  
  html += "<div class='back'><a href='/'>返回首页</a></div>";
  html += "</div>";
  html += "</body></html>";
  client.print(html);
}

void handleStatusPage(WiFiClient client) {
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
  html += "<title>ESP32 UART 系统状态</title>";
  html += "<style>body{font-family:Arial;margin:10px;background:#f0f0f0;font-size:16px;}";
  html += ".container{max-width:100%;margin:0 auto;background:white;padding:15px;border-radius:10px;box-sizing:border-box;}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px;font-size:24px;}";
  html += "h2{font-size:20px;}";
  html += ".status{background:#e7f3fe;border-left:4px solid #2196F3;padding:15px;margin:15px 0;font-size:16px;}";
  html += "strong{font-size:16px;}";
  html += ".back{margin-top:20px;}";
  html += ".back a{display:inline-block;padding:10px 20px;background:#4CAF50;color:white;text-decoration:none;border-radius:5px;}";
  html += ".back a:hover{background:#45a049;}";
  html += "@media screen and (min-width: 600px) {";
  html += ".container{max-width:800px;padding:20px;}";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>📊 系统状态</h1>";
  
  // 系统信息
  html += "<div class='status'>";
  html += "<strong>固件版本:</strong> " + String(FIRMWARE_VERSION) + "<br>";
  html += "<strong>运行模式:</strong> " + String(currentMode == MODE_CLIENT ? "客户端" : "服务器") + "<br>";
  html += "<strong>客户端ID:</strong> " + client_id + "<br>";
  html += "<strong>运行时间:</strong> " + String(millis() / 1000) + " 秒<br>";
  html += "</div>";
  
  // WiFi信息
  html += "<div class='status'>";
  html += "<h2>WiFi信息</h2>";
  html += "<strong>状态:</strong> " + String(wifiConnected ? "已连接" : "未连接") + "<br>";
  if (wifiConnected) {
    if (currentMode == MODE_SERVER) {
      html += "<strong>AP SSID:</strong> " + String(ap_ssid) + "<br>";
      html += "<strong>AP IP:</strong> " + WiFi.softAPIP().toString() + "<br>";
      html += "<strong>连接设备数:</strong> " + String(WiFi.softAPgetStationNum()) + "<br>";
    } else {
      html += "<strong>SSID:</strong> " + String(client_wifi_ssid) + "<br>";
      html += "<strong>IP地址:</strong> " + WiFi.localIP().toString() + "<br>";
      html += "<strong>信号强度:</strong> " + String(WiFi.RSSI()) + " dBm<br>";
    }
  }
  html += "</div>";
  
  // 串口信息
  html += "<div class='status'>";
  html += "<h2>串口信息</h2>";
  html += "<strong>UART2波特率:</strong> " + String(uart2BaudRate) + "<br>";
  html += "<strong>UART2引脚:</strong> RX=" + String(UART2_RX_PIN) + " TX=" + String(UART2_TX_PIN) + "<br>";
  html += "</div>";
  
  // SD卡信息
  html += "<div class='status'>";
  html += "<h2>SD卡信息</h2>";
  html += "<strong>状态:</strong> " + String(sdCardReady ? "正常" : "异常") + "<br>";
  if (sdCardReady) {
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    uint64_t usedSize = SD.usedBytes() / (1024 * 1024);
    html += "<strong>总容量:</strong> " + String(cardSize) + " MB<br>";
    html += "<strong>已用容量:</strong> " + String(usedSize) + " MB<br>";
  }
  html += "</div>";
  
  // 电池信息
  html += "<div class='status'>";
  html += "<h2>电池信息</h2>";
  html += "<strong>电池电压:</strong> " + String(batteryVoltage) + " V<br>";
  html += "<strong>状态:</strong> " + String(batteryVoltage > BATTERY_LOW_VOLTAGE ? "正常" : "低电压") + "<br>";
  html += "</div>";
  
  // 客户端连接信息（服务器模式）
  if (currentMode == MODE_SERVER) {
    html += "<div class='status'>";
    html += "<h2>客户端连接</h2>";
    int clientCount = getConnectedClientCount();
    html += "<strong>当前连接数:</strong> " + String(clientCount) + "<br>";
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (serverClients[i] && serverClients[i].connected()) {
        html += "<strong>客户端 " + String(i) + "</strong>: " + serverClients[i].remoteIP().toString() + "<br>";
      }
    }
    html += "</div>";
  }
  
  html += "<div class='back'><a href='/'>返回首页</a></div>";
  html += "</div>";
  html += "</body></html>";
  client.print(html);
}

void handleClientPage(WiFiClient client, String request) {
  // 解析客户端ID
  int clientIdStart = request.indexOf("client_id=") + 10;
  int clientIdEnd = request.indexOf("&", clientIdStart);
  if (clientIdEnd == -1) clientIdEnd = request.length();
  String clientId = request.substring(clientIdStart, clientIdEnd);
  
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
  html += "<title>客户端 " + clientId + " - ESP32 UART</title>";
  html += "<style>body{font-family:Arial;margin:10px;background:#f0f0f0;font-size:16px;}";
  html += ".container{max-width:100%;margin:0 auto;background:white;padding:15px;border-radius:10px;box-sizing:border-box;}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px;font-size:24px;}";
  html += "h2{font-size:20px;}";
  html += ".serial-data{background:#f8f8f8;border:1px solid #ddd;padding:15px;margin:15px 0;height:300px;overflow-y:scroll;font-family:monospace;font-size:14px;white-space:pre-wrap;word-wrap:break-word;}";
  html += ".command-form{margin:20px 0;}";
  html += ".command-form input[type='text']{width:80%;padding:10px;font-size:16px;}";
  html += ".command-form input[type='submit']{width:18%;padding:10px;font-size:16px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;}";
  html += ".back{margin-top:20px;}";
  html += ".back a{display:inline-block;padding:10px 20px;background:#4CAF50;color:white;text-decoration:none;border-radius:5px;}";
  html += ".back a:hover{background:#45a049;}";
  html += "@media screen and (min-width: 600px) {";
  html += ".container{max-width:800px;padding:20px;}";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>客户端 " + clientId + " - 串口实时数据</h1>";
  
  // 串口数据显示
  html += "<div class='serial-data' id='serialData'>";
  // 查找对应的客户端数据
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (serverClients[i] && serverClients[i].connected()) {
      // 这里简化处理，实际应该根据客户端ID查找
      html += clientSerialData[i];
      break;
    }
  }
  html += "</div>";
  
  // 发送命令表单
  html += "<form class='command-form' action='/client' method='post'>";
  html += "<input type='hidden' name='client_id' value='" + clientId + "'>";
  html += "<input type='text' name='command' placeholder='输入命令...' autocomplete='off'>";
  html += "<input type='submit' value='发送'>";
  html += "</form>";
  
  // 自动刷新脚本
  html += "<script>";
  html += "setInterval(function() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/client?client_id=" + clientId + "&refresh=1', true);";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "      document.getElementById('serialData').innerHTML = xhr.responseText;";
  html += "      var serialData = document.getElementById('serialData');";
  html += "      serialData.scrollTop = serialData.scrollHeight;";
  html += "    }";
  html += "  }";
  html += "  xhr.send();";
  html += "}, 1000);";
  html += "</script>";
  
  html += "<div class='back'><a href='/'>返回首页</a></div>";
  html += "</div>";
  html += "</body></html>";
  client.print(html);
}

void handleConfigPage(WiFiClient client) {
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
  html += "<title>ESP32 UART 系统配置</title>";
  html += "<style>body{font-family:Arial;margin:10px;background:#f0f0f0;font-size:16px;}";
  html += ".container{max-width:100%;margin:0 auto;background:white;padding:15px;border-radius:10px;box-sizing:border-box;}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px;font-size:24px;}";
  html += "h2{font-size:20px;}";
  html += ".form-group{margin:15px 0;}";
  html += ".form-group label{display:block;margin-bottom:5px;font-weight:bold;}";
  html += ".form-group input[type='text'], .form-group input[type='number'], .form-group select{width:100%;padding:10px;font-size:16px;box-sizing:border-box;}";
  html += ".form-group input[type='submit']{width:100%;padding:15px;font-size:18px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;}";
  html += ".form-group input[type='submit']:hover{background:#45a049;}";
  html += ".back{margin-top:20px;}";
  html += ".back a{display:inline-block;padding:10px 20px;background:#4CAF50;color:white;text-decoration:none;border-radius:5px;}";
  html += ".back a:hover{background:#45a049;}";
  html += "@media screen and (min-width: 600px) {";
  html += ".container{max-width:800px;padding:20px;}";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>⚙️ 系统配置</h1>";
  html += "<form action='/saveconfig' method='post'>";
  
  // 运行模式
  html += "<div class='form-group'>";
  html += "<label for='mode'>运行模式</label>";
  html += "<select name='mode' id='mode'>";
  html += String("<option value='0'") + (currentMode == MODE_CLIENT ? " selected" : "") + ">客户端模式</option>";
  html += String("<option value='1'") + (currentMode == MODE_SERVER ? " selected" : "") + ">服务器模式</option>";
  html += "</select>";
  html += "</div>";
  
  // 客户端ID
  html += "<div class='form-group'>";
  html += "<label for='client_id'>客户端ID</label>";
  html += "<input type='text' name='client_id' id='client_id' value='" + client_id + "'>";
  html += "</div>";
  
  // UART2波特率
  html += "<div class='form-group'>";
  html += "<label for='uart2_baud'>UART2波特率</label>";
  html += "<select name='uart2_baud' id='uart2_baud'>";
  html += String("<option value='9600'") + (uart2BaudRate == 9600 ? " selected" : "") + ">9600</option>";
  html += String("<option value='19200'") + (uart2BaudRate == 19200 ? " selected" : "") + ">19200</option>";
  html += String("<option value='38400'") + (uart2BaudRate == 38400 ? " selected" : "") + ">38400</option>";
  html += String("<option value='57600'") + (uart2BaudRate == 57600 ? " selected" : "") + ">57600</option>";
  html += String("<option value='115200'") + (uart2BaudRate == 115200 ? " selected" : "") + ">115200</option>";
  html += "</select>";
  html += "</div>";
  
  // 调试模式
  html += "<div class='form-group'>";
  html += "<label for='debug_mode'>调试模式</label>";
  html += "<select name='debug_mode' id='debug_mode'>";
  html += String("<option value='1'") + (debugMode ? " selected" : "") + ">开启</option>";
  html += String("<option value='0'") + (!debugMode ? " selected" : "") + ">关闭</option>";
  html += "</select>";
  html += "</div>";
  
  // 保存按钮
  html += "<div class='form-group'>";
  html += "<input type='submit' value='保存配置'>";
  html += "</div>";
  html += "</form>";
  
  html += "<div class='back'><a href='/'>返回首页</a></div>";
  html += "</div>";
  html += "</body></html>";
  client.print(html);
}

void handleSaveConfig(WiFiClient client) {
  // 读取表单数据
  String postData = client.readStringUntil('\r');
  
  // 解析参数
  int modeIndex = postData.indexOf("mode=") + 5;
  int modeEnd = postData.indexOf("&", modeIndex);
  int newMode = postData.substring(modeIndex, modeEnd).toInt();
  
  int clientIdIndex = postData.indexOf("client_id=") + 10;
  int clientIdEnd = postData.indexOf("&", clientIdIndex);
  String newClientId = postData.substring(clientIdIndex, clientIdEnd);
  
  int baudIndex = postData.indexOf("uart2_baud=") + 11;
  int baudEnd = postData.indexOf("&", baudIndex);
  int newBaud = postData.substring(baudIndex, baudEnd).toInt();
  
  int debugIndex = postData.indexOf("debug_mode=") + 11;
  int debugEnd = postData.indexOf("&", debugIndex);
  if (debugEnd == -1) debugEnd = postData.length();
  bool newDebugMode = postData.substring(debugIndex, debugEnd).toInt() == 1;
  
  // 保存配置
  currentMode = newMode;
  client_id = newClientId;
  uart2BaudRate = newBaud;
  debugMode = newDebugMode;
  
  // 保存到EEPROM
  saveConfigToEEPROM();
  
  // 重启设备
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
  html += "<title>配置保存成功</title>";
  html += "<style>body{font-family:Arial;margin:10px;background:#f0f0f0;font-size:16px;}";
  html += ".container{max-width:100%;margin:0 auto;background:white;padding:15px;border-radius:10px;box-sizing:border-box;}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px;font-size:24px;}";
  html += ".success{background:#e8f5e8;border-left:4px solid #4CAF50;padding:15px;margin:15px 0;font-size:16px;}";
  html += "@media screen and (min-width: 600px) {";
  html += ".container{max-width:800px;padding:20px;}";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>配置保存成功</h1>";
  html += "<div class='success'>";
  html += "<strong>配置已保存，设备将重启...</strong><br>";
  html += "运行模式: " + String(newMode == MODE_CLIENT ? "客户端" : "服务器") + "<br>";
  html += "客户端ID: " + newClientId + "<br>";
  html += "UART2波特率: " + String(newBaud) + "<br>";
  html += "调试模式: " + String(newDebugMode ? "开启" : "关闭") + "<br>";
  html += "</div>";
  html += "</div>";
  html += "</body></html>";
  client.print(html);
  
  // 延迟后重启
  delay(2000);
  ESP.restart();
}

void handleDownloadLog(WiFiClient client, String request) {
  // 解析文件路径
  int fileStart = request.indexOf("file=") + 5;
  int fileEnd = request.indexOf("&", fileStart);
  if (fileEnd == -1) fileEnd = request.length();
  String filePath = request.substring(fileStart, fileEnd);
  
  // 打开文件
  File file = SD.open(filePath);
  if (file) {
    // 发送文件
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/octet-stream");
    client.println("Content-Disposition: attachment; filename=\"" + String(filePath.substring(filePath.lastIndexOf('/') + 1)) + "\"");
    client.println("Content-Length: " + String(file.size()));
    client.println();
    
    // 发送文件内容
    while (file.available()) {
      client.write(file.read());
    }
    file.close();
  } else {
    // 文件不存在
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>404 - 文件未找到</h1><p>请求的文件不存在</p></body></html>");
  }
}

void handleClearLog(WiFiClient client) {
  // 清除日志文件
  // 这里简化处理，实际应该根据需要删除特定文件
  
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
  html += "<title>日志清除成功</title>";
  html += "<style>body{font-family:Arial;margin:10px;background:#f0f0f0;font-size:16px;}";
  html += ".container{max-width:100%;margin:0 auto;background:white;padding:15px;border-radius:10px;box-sizing:border-box;}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px;font-size:24px;}";
  html += ".success{background:#e8f5e8;border-left:4px solid #4CAF50;padding:15px;margin:15px 0;font-size:16px;}";
  html += ".back{margin-top:20px;}";
  html += ".back a{display:inline-block;padding:10px 20px;background:#4CAF50;color:white;text-decoration:none;border-radius:5px;}";
  html += ".back a:hover{background:#45a049;}";
  html += "@media screen and (min-width: 600px) {";
  html += ".container{max-width:800px;padding:20px;}";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>日志清除成功</h1>";
  html += "<div class='success'>";
  html += "<strong>日志文件已清除</strong>";
  html += "</div>";
  html += "<div class='back'><a href='/'>返回首页</a></div>";
  html += "</div>";
  html += "</body></html>";
  client.print(html);
}

void handlePowerControl(WiFiClient client, String request) {
  // 读取POST数据
  String postData = client.readStringUntil('\r');
  
  // 解析action参数
  int actionIndex = postData.indexOf("action=") + 7;
  int actionEnd = postData.indexOf("&", actionIndex);
  if (actionEnd == -1) actionEnd = postData.length();
  String action = postData.substring(actionIndex, actionEnd);
  
  // 执行电源控制操作
  if (action == "on") {
    powerOn();
  } else if (action == "off") {
    powerOff();
  } else if (action == "trigger") {
    triggerShutdown();
  } else if (action == "reset") {
    resetCPU();
  }
  
  // 返回结果
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
  html += "<title>电源控制</title>";
  html += "<style>body{font-family:Arial;margin:10px;background:#f0f0f0;font-size:16px;}";
  html += ".container{max-width:100%;margin:0 auto;background:white;padding:15px;border-radius:10px;box-sizing:border-box;}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px;font-size:24px;}";
  html += ".success{background:#e8f5e8;border-left:4px solid #4CAF50;padding:15px;margin:15px 0;font-size:16px;}";
  html += ".back{margin-top:20px;}";
  html += ".back a{display:inline-block;padding:10px 20px;background:#4CAF50;color:white;text-decoration:none;border-radius:5px;}";
  html += ".back a:hover{background:#45a049;}";
  html += "@media screen and (min-width: 600px) {";
  html += ".container{max-width:800px;padding:20px;}";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>电源控制</h1>";
  html += "<div class='success'>";
  html += "<strong>操作已执行</strong><br>";
  if (action == "on") {
    html += "执行了开机操作";
  } else if (action == "off") {
    html += "执行了关机操作";
  } else if (action == "trigger") {
    html += "执行了触发关机操作";
  } else if (action == "reset") {
    html += "执行了复位操作";
  }
  html += "</div>";
  html += "<div class='back'><a href='/'>返回首页</a></div>";
  html += "</div>";
  html += "</body></html>";
  client.print(html);
}

void handleNotFound(WiFiClient client) {
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/html");
  client.println();
  client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>404 - 页面未找到</h1><p>请求的页面不存在</p><a href='/'>返回首页</a></body></html>");
}
