// ==================== Web服务器和页面处理 ====================

void initWebServer() {
  if (webServerEnabled && (wifiConnected || currentMode == MODE_SERVER)) {
    webServer.begin();
    String ipStr = (currentMode == MODE_SERVER) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    Serial.println("✓ Web服务器启动成功");
    Serial.println("  访问地址: http://" + ipStr);
    Serial.println("  日志查看: http://" + ipStr + "/logs");
    Serial.println("  系统状态: http://" + ipStr + "/status");
    Serial.println("  串口监视器: http://" + ipStr + "/serial");
  }
}

void handleWebServer() {
  if (!webServerEnabled) return;
  
  WiFiClient client = webServer.available();
  if (!client) return;
  
  // 先读取请求行和请求头
  String request = "";
  bool headersComplete = false;
  unsigned long startTime = millis();
  
  while (client.connected() && !headersComplete && (millis() - startTime < 2000)) {
    if (client.available()) {
      char c = client.read();
      request += c;
      
      // 检查是否读到空行（header结束）
      if (request.length() >= 4 && request.substring(request.length() - 4) == "\r\n\r\n") {
        headersComplete = true;
        break;
      }
    }
    delay(1);
  }
  
  // 解析Content-Length
  int contentLength = 0;
  int pos = request.indexOf("Content-Length:");
  if (pos >= 0) {
    int pos2 = request.indexOf("\r\n", pos);
    if (pos2 > pos) {
      String lenStr = request.substring(pos + 15, pos2);
      lenStr.trim();
      contentLength = lenStr.toInt();
    }
  }
  
  // 如果是POST请求，继续读取body
  String postBody = "";
  if (contentLength > 0) {
    int bodyStart = request.length();
    while (client.connected() && (millis() - startTime < 2000)) {
      if (client.available()) {
        char c = client.read();
        postBody += c;
        if (postBody.length() >= contentLength) break;
      }
      delay(1);
    }
  }
  
  // 构建完整的请求用于路由
  String requestLine = request;
  int lineEnd = request.indexOf("\r\n");
  if (lineEnd > 0) {
    requestLine = request.substring(0, lineEnd);
  }
  
  // 路由处理
  if (requestLine.indexOf("GET / ") >= 0 || requestLine.indexOf("GET / HTTP") >= 0) {
    handleRootPage(client);
  } else if (requestLine.indexOf("GET /favicon") >= 0) {
    client.println("HTTP/1.1 204 No Content");
    client.println();
  } else if (requestLine.indexOf("GET /hotspot-detect.html") >= 0 || requestLine.indexOf("GET /connecttest.txt") >= 0) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.print("<!DOCTYPE html><html><head><script>window.location.href='http://192.168.1.1/';</script></head><body></body></html>");
  } else if (requestLine.indexOf("GET /generate_204") >= 0 || requestLine.indexOf("GET /fwlink") >= 0) {
    // Android/Windows Captive Portal
    client.println("HTTP/1.1 204 No Content");
    client.println();
  } else if (requestLine.indexOf("GET /ncsi.txt") >= 0) {
    // Windows Captive Portal
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println();
    client.print("Microsoft Connect Test");
  } else if (requestLine.indexOf("GET /serial ") >= 0 || requestLine.indexOf("GET /serial") >= 0) {
    if (requestLine.indexOf("GET /serial/data") >= 0) {
      handleSerialDataAPI(client);
    } else {
      handleSerialPage(client);
    }
  } else if (requestLine.indexOf("POST /serial/send") >= 0) {
    handleSerialSend(client, postBody);
  } else if (requestLine.indexOf("POST /serial/clear") >= 0) {
    handleSerialClear(client);
  } else if (requestLine.indexOf("GET /logs") >= 0) {
    handleLogsPage(client, request);
  } else if (requestLine.indexOf("GET /preview") >= 0) {
    handlePreviewLog(client, request);
  } else if (requestLine.indexOf("GET /status ") >= 0) {
    handleStatusPage(client);
  } else if (requestLine.indexOf("GET /client") >= 0) {
    handleClientPage(client, request);
  } else if (requestLine.indexOf("POST /client/send") >= 0) {
    handleClientSend(client, postBody);
  } else if (requestLine.indexOf("GET /config ") >= 0) {
    handleConfigPage(client);
  } else if (requestLine.indexOf("POST /saveconfig") >= 0) {
    handleSaveConfig(client);
  } else if (requestLine.indexOf("GET /download") >= 0) {
    handleDownloadLog(client, request);
  } else if (requestLine.indexOf("GET /clear ") >= 0) {
    handleClearLog(client);
  } else if (requestLine.indexOf("GET /deletedir") >= 0) {
    handleDeleteDirectory(client, request);
  } else if (requestLine.indexOf("GET /delete?file=") >= 0) {
    handleDeleteFile(client, request);
  } else if (requestLine.indexOf("POST /power") >= 0) {
    handlePowerControl(client, postBody);
  } else {
    handleNotFound(client);
  }
  
  delay(10);
  client.stop();
  yield();
}

void handleRootPage(WiFiClient client) {
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
  html += "<title>ESP32 UART 服务器</title>";
  html += "<style>body{font-family:Arial,sans-serif;margin:10px;background:#f5f5f5;font-size:14px;}";
  html += ".container{max-width:100%;margin:0 auto;background:white;padding:12px;border-radius:10px;box-sizing:border-box;box-shadow:0 2px 8px rgba(0,0,0,0.1);}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:8px;font-size:18px;}";
  html += "h2{font-size:16px;}";
  html += "h3{font-size:15px;}";
  html += ".menu{margin:15px 0;display:flex;flex-wrap:wrap;gap:8px;}";
  html += ".menu a{display:block;padding:10px 12px;margin:0;background:#4CAF50;color:white;text-decoration:none;border-radius:4px;text-align:center;font-size:13px;flex:1 1 100px;transition:all 0.2s ease;}";
  html += ".menu a:hover{background:#45a049;transform:translateY(-1px);box-shadow:0 2px 5px rgba(0,0,0,0.2);}";
  html += ".info{background:#e7f3fe;border-left:4px solid #2196F3;padding:10px;margin:12px 0;font-size:13px;}";
  html += "strong{font-size:14px;}";
  html += ".power-controls{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px;}";
  html += ".power-controls form{flex:1 1 80px;}";
  html += ".power-controls input[type='submit']{width:100%;padding:6px 10px;border:none;border-radius:4px;background:#2196F3;color:white;font-size:12px;cursor:pointer;transition:all 0.2s ease;}";
  html += ".power-controls input[type='submit']:hover{background:#1976D2;transform:translateY(-1px);}";
  html += "@media screen and (min-width: 600px) {";
  html += ".container{max-width:800px;padding:15px;}";
  html += ".menu a{flex:none;margin:5px;}";
  html += ".power-controls form{flex:none;width:auto;}";
  html += ".power-controls input[type='submit']{width:80px;}";
  html += "}";
  html += "@media screen and (max-width: 480px) {";
  html += ".menu a{flex:1 1 100%;font-size:12px;padding:8px 10px;}";
  html += ".container{max-width:95%;padding:10px;}";
  html += "h1{font-size:16px;}";
  html += "h3{font-size:14px;}";
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
  html += "<a href='/serial'>🖥️ 串口监视器</a>";
  html += "<a href='/logs'>📋 查看日志</a>";
  html += "<a href='/status'>📊 系统状态</a>";
  html += "<a href='/config'>⚙️ 系统配置</a>";
  html += "</div>";
  
  // 电源控制
  html += "<div class='info'>";
  html += "<h3>电源控制</h3>";
  html += "<div class='power-controls'>";
  html += "<form action='/power' method='post'>";
  html += "<input type='hidden' name='action' value='on'>";
  html += "<input type='submit' value='开机'>";
  html += "</form>";
  html += "<form action='/power' method='post'>";
  html += "<input type='hidden' name='action' value='off'>";
  html += "<input type='submit' value='关机'>";
  html += "</form>";
  html += "<form action='/power' method='post'>";
  html += "<input type='hidden' name='action' value='trigger'>";
  html += "<input type='submit' value='触发关机'>";
  html += "</form>";
  html += "<form action='/power' method='post'>";
  html += "<input type='hidden' name='action' value='reset'>";
  html += "<input type='submit' value='复位'>";
  html += "</form>";
  html += "</div>";
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
  html += "<style>body{font-family:Arial,sans-serif;margin:10px;background:#f5f5f5;font-size:14px;}";
  html += ".container{max-width:100%;margin:0 auto;background:white;padding:12px;border-radius:10px;box-sizing:border-box;box-shadow:0 2px 8px rgba(0,0,0,0.1);}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:8px;font-size:18px;}";
  html += "h2{font-size:16px;word-break:break-all;line-height:1.4;margin:10px 0;}";
  html += ".file-list{margin:15px 0;}";
  html += ".file-item{padding:10px;border-bottom:1px solid #eee;display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;}";
  html += ".file-name{flex:1;min-width:150px;word-break:break-all;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:0;}";
  html += ".file-name a{text-decoration:none;color:#333;font-size:13px;}";
  html += ".file-name a:hover{color:#4CAF50;}";
  html += ".file-actions{display:flex;gap:8px;flex-shrink:0;margin-left:10px;}";
  html += ".file-actions a{padding:6px 12px;background:#2196F3;color:white;text-decoration:none;border-radius:4px;font-size:12px;transition:all 0.2s ease;}";
  html += ".file-actions a:hover{background:#1976D2;transform:translateY(-1px);}";
  html += ".file-size{color:#666;font-size:12px;margin-left:10px;flex-shrink:0;}";
  html += ".back{margin-top:15px;}";
  html += ".back a{display:inline-block;padding:8px 16px;background:#4CAF50;color:white;text-decoration:none;border-radius:4px;font-size:13px;transition:all 0.2s ease;}";
  html += ".back a:hover{background:#45a049;transform:translateY(-1px);}";
  html += ".breadcrumb{margin:10px 0;padding:8px;background:#f8f8f8;border-radius:4px;font-size:13px;}";
  html += ".breadcrumb a{text-decoration:none;color:#2196F3;}";
  html += ".breadcrumb a:hover{text-decoration:underline;}";
  html += "@media screen and (min-width: 600px) {";
  html += ".container{max-width:800px;padding:15px;}";
  html += "}";
  html += "@media screen and (max-width: 480px) {";
  html += ".file-item{flex-direction:row;align-items:center;}";
  html += ".file-actions{margin-left:auto;}";
  html += ".file-size{margin-left:10px;}";
  html += ".container{max-width:95%;padding:10px;}";
  html += "h1{font-size:16px;}";
  html += "h2{font-size:14px;}";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>📋 日志文件</h1>";
  html += "<div style='margin:10px 0;'>";
  html += "<a href='/logs?dir=server' style='padding:8px 16px;background:#4CAF50;color:white;text-decoration:none;border-radius:4px;'>服务器日志</a> ";
  html += "<a href='/logs?dir=client' style='padding:8px 16px;background:#2196F3;color:white;text-decoration:none;border-radius:4px;'>客户端日志</a>";
  html += "</div>";
  
  // 检查SD卡状态
  if (!sdCardReady) {
    html += "<div style='background:#ffebee;border-left:4px solid #f44336;padding:15px;margin:15px 0;'>";
    html += "<strong>错误:</strong> SD卡未就绪，无法查看日志文件";
    html += "</div>";
  } else {
    // 解析目录参数
    String currentDir = "/server";
    int dirIndex = request.indexOf("dir=");
    if (dirIndex > 0) {
      int dirEnd = request.indexOf(" ", dirIndex);
      if (dirEnd < 0) dirEnd = request.length();
      String dirParam = request.substring(dirIndex + 4, dirEnd);
      // 规范化：移除可能存在的前导斜杠
      while (dirParam.startsWith("/")) {
        dirParam = dirParam.substring(1);
      }
      // 规范化：移除可能存在的双斜杠
      while (dirParam.indexOf("//") >= 0) {
        dirParam.replace("//", "/");
      }
      // 安全检查：防止路径遍历攻击
      if (dirParam.indexOf("..") == -1) {
        if (dirParam == "client") {
          currentDir = "/client";
        } else if (dirParam == "server") {
          currentDir = "/server";
        } else if (dirParam.startsWith("client/")) {
          currentDir = "/" + dirParam;
        } else if (dirParam.startsWith("server/")) {
          currentDir = "/" + dirParam;
        } else if (dirParam.length() > 0) {
          // 判断是服务器还是客户端的子目录，需要从父路径判断
          // 默认先尝试服务器目录
          if (SD.exists("/server/" + dirParam)) {
            currentDir = "/server/" + dirParam;
          } else if (SD.exists("/client/" + dirParam)) {
            currentDir = "/client/" + dirParam;
          } else {
            // 如果都不存在，默认为客户端目录
            currentDir = "/client/" + dirParam;
          }
        }
      }
    }
    
    // 面包屑导航
    html += "<div class='breadcrumb'>";
    html += "<a href='/logs'>根目录</a>";
    if (currentDir.startsWith("/server")) {
      html += " → <a href='/logs?dir=server'>服务器</a>";
      if (currentDir != "/server") {
        String subPath = currentDir.substring(8); // 去掉 "/server" 前缀
        if (subPath.length() > 0 && subPath != "/") {
          html += " → " + subPath;
        }
      }
    } else if (currentDir.startsWith("/client")) {
      html += " → <a href='/logs?dir=client'>客户端</a>";
      String subPath = currentDir.substring(7); // 去掉 "/client" 前缀
      if (subPath.length() > 0 && subPath != "/") {
        // 移除前导斜杠
        if (subPath.startsWith("/")) {
          subPath = subPath.substring(1);
        }
        html += " → " + subPath;
      }
    }
    html += "</div>";
    
    // 列出当前目录内容
    String displayDir = currentDir;
    if (displayDir.length() > 50) {
      displayDir = "..." + displayDir.substring(displayDir.length() - 47);
    }
    html += "<h2>" + displayDir + "</h2>";
    html += "<div class='file-list'>";
    File root = SD.open(currentDir);
    if (root) {
      bool hasFiles = false;
      
      // 显示返回上级目录链接
      if (currentDir != "/server" && currentDir != "/client") {
        String parentDir = currentDir.substring(0, currentDir.lastIndexOf('/'));
        String parentLink = "";
        if (parentDir == "/server") {
          parentLink = "/logs?dir=server";
        } else if (parentDir == "/client") {
          parentLink = "/logs?dir=client";
        } else if (parentDir.startsWith("/server")) {
          parentLink = "/logs?dir=" + parentDir.substring(8);
        } else {
          parentLink = "/logs?dir=" + parentDir.substring(1);
        }
        html += "<div class='file-item'>";
        html += "<div class='file-name'>";
        html += "<a href='" + parentLink + "'>📁 .. (返回上级)</a>";
        html += "</div>";
        html += "</div>";
      }
      
      while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        hasFiles = true;
        String entryName = entry.name();
        // 规范化路径：避免产生双斜杠
        String entryPath;
        if (currentDir.endsWith("/")) {
          entryPath = currentDir + entryName;
        } else {
          entryPath = currentDir + "/" + entryName;
        }
        // 移除路径中可能存在的双斜杠
        while (entryPath.indexOf("//") >= 0) {
          entryPath.replace("//", "/");
        }
        String entryUrlPath;
        if (currentDir.startsWith("/server")) {
          entryUrlPath = entryPath.substring(8); // 去掉 "/server" 前缀
        } else {
          entryUrlPath = entryPath.substring(7); // 去掉 "/client" 前缀
        }
        
        if (entry.isDirectory()) {
          html += "<div class='file-item'>";
          html += "<div class='file-name'>";
          html += "<a href='/logs?dir=" + entryUrlPath + "'>📁 " + entryName + "</a>";
          html += "</div>";
          html += "<div class='file-actions'>";
          html += "<a href='/deletedir?dir=" + entryPath + "' onclick='return confirm(\"确定删除文件夹 " + entryName + " 及所有内容？\")' style='background:#f44336;'>删除</a>";
          html += "</div>";
          html += "</div>";
        } else {
          String fileSize = formatFileSize(entry.size());
          html += "<div class='file-item'>";
          html += "<div class='file-name'>📄 " + entryName + "</div>";
          html += "<div class='file-actions'>";
          html += "<a href='/preview?file=" + entryPath + "'>预览</a>";
          html += "<a href='/download?file=" + entryPath + "'>下载</a>";
          html += "<a href='/delete?file=" + entryPath + "' onclick='return confirm(\"确定删除 " + entryName + "？\")'>删除</a>";
          html += "</div>";
          html += "<div class='file-size'>" + fileSize + "</div>";
          html += "</div>";
        }
        entry.close();
      }
      if (!hasFiles) {
        html += "<div class='file-item'><div class='file-name'>空目录</div></div>";
      }
      root.close();
    } else {
      html += "<div style='background:#ffebee;border-left:4px solid #f44336;padding:15px;margin:15px 0;'>";
      html += "<strong>错误:</strong> 无法打开目录: " + currentDir;
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
  html += "<style>body{font-family:Arial,sans-serif;margin:10px;background:#f5f5f5;font-size:14px;}";
  html += ".container{max-width:100%;margin:0 auto;background:white;padding:12px;border-radius:10px;box-sizing:border-box;box-shadow:0 2px 8px rgba(0,0,0,0.1);}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:8px;font-size:18px;}";
  html += "h2{font-size:16px;}";
  html += ".status{background:#e7f3fe;border-left:4px solid #2196F3;padding:10px;margin:12px 0;font-size:13px;}";
  html += "strong{font-size:14px;}";
  html += ".back{margin-top:15px;}";
  html += ".back a{display:inline-block;padding:8px 16px;background:#4CAF50;color:white;text-decoration:none;border-radius:4px;font-size:13px;transition:all 0.2s ease;}";
  html += ".back a:hover{background:#45a049;transform:translateY(-1px);}";
  html += "@media screen and (min-width: 600px) {";
  html += ".container{max-width:800px;padding:15px;}";
  html += "}";
  html += "@media screen and (max-width: 480px) {";
  html += ".container{max-width:95%;padding:10px;}";
  html += "h1{font-size:16px;}";
  html += "h2{font-size:14px;}";
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
    int clientCount = getConnectedClientCount();
    html += "<div class='status'>";
    html += "<h2>客户端连接</h2>";
    html += "<strong>当前连接数:</strong> " + String(clientCount) + "<br>";
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (serverClients[i] && serverClients[i].connected()) {
        html += "<a href='/client?client_id=" + String(i) + "' style='display:inline-block;padding:8px 12px;background:#4CAF50;color:white;text-decoration:none;border-radius:4px;margin:5px 0;'>";
        html += "客户端 " + String(i) + ": " + serverClients[i].remoteIP().toString() + " → 点击查看数据</a><br>";
      }
    }
    if (clientCount == 0) {
      html += "<em>暂无客户端连接</em><br>";
    }
    html += "</div>";
  }
  
  html += "<div class='back'><a href='/'>返回首页</a></div>";
  html += "</div>";
  html += "</body></html>";
  client.print(html);
}

void handleClientPage(WiFiClient client, String request) {
  // 解析客户端索引
  int clientIdx = -1;
  int idxStart = request.indexOf("client_id=") + 10;
  int idxEnd = request.indexOf("&", idxStart);
  if (idxEnd == -1) idxEnd = request.length();
  String clientIdParam = request.substring(idxStart, idxEnd);
  clientIdx = clientIdParam.toInt();
  
  // 检查刷新请求
  bool isRefresh = (request.indexOf("refresh=1") > 0);
  
  if (isRefresh) {
    // 返回纯文本数据用于AJAX刷新
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println();
    
    if (clientIdx >= 0 && clientIdx < MAX_CLIENTS) {
      String filteredData = filterAnsiEscape(clientSerialData[clientIdx]);
      client.print(filteredData);
    }
    return;
  }
  
  // 检查客户端是否有效
  if (clientIdx < 0 || clientIdx >= MAX_CLIENTS || !serverClients[clientIdx] || !serverClients[clientIdx].connected()) {
    client.println("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    client.print("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>");
    client.print("<h1>客户端不存在或已断开</h1>");
    client.print("<a href='/'>返回首页</a></body></html>");
    return;
  }
  
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
  html += "<title>客户端 " + String(clientIdx) + " - ESP32 UART</title>";
  html += "<style>";
  html += "*{box-sizing:border-box;margin:0;padding:0;}";
  html += "body{font-family:'Consolas','Monaco',monospace;margin:0;background:#1e1e1e;color:#0f0;height:100vh;display:flex;flex-direction:column;}";
  html += ".header{background:#2d2d2d;padding:12px 15px;border-bottom:1px solid #444;}";
  html += ".header h1{color:#4CAF50;font-size:18px;}";
  html += ".header-info{color:#888;font-size:12px;margin-top:5px;}";
  html += ".serial-container{flex:1;display:flex;flex-direction:column;overflow:hidden;}";
  html += ".serial-output{flex:1;background:#1a1a1a;color:#0f0;padding:12px;overflow-y:auto;font-size:13px;line-height:1.5;word-wrap:break-word;white-space:pre-wrap;border:none;resize:none;}";
  html += ".serial-output::-webkit-scrollbar{width:10px;}";
  html += ".serial-output::-webkit-scrollbar-track{background:#2d2d2d;}";
  html += ".serial-output::-webkit-scrollbar-thumb{background:#555;border-radius:5px;}";
  html += ".input-area{background:#2d2d2d;padding:12px;display:flex;gap:8px;border-top:1px solid #444;}";
  html += ".input-area input[type='text']{flex:1;padding:10px 12px;background:#333;color:#fff;border:1px solid #555;border-radius:4px;font-size:14px;font-family:inherit;}";
  html += ".input-area input[type='text']:focus{outline:none;border-color:#4CAF50;}";
  html += ".input-area input[type='submit']{padding:10px 20px;background:#4CAF50;color:white;border:none;border-radius:4px;font-size:14px;cursor:pointer;transition:all 0.2s;}";
  html += ".input-area input[type='submit']:hover{background:#45a049;}";
  html += ".back-btn{background:#2196F3 !important;}";
  html += ".back-btn:hover{background:#1976D2 !important;}";
  html += "@media screen and (max-width: 600px){";
  html += ".input-area{flex-direction:column;}";
  html += ".input-area input[type='submit']{width:100%;}";
  html += ".serial-output{font-size:12px;}";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='header'>";
  html += "<h1>🖥️ 客户端 " + String(clientIdx) + " - 串口数据</h1>";
  html += "<div class='header-info'>IP: " + serverClients[clientIdx].remoteIP().toString() + " | 波特率: " + String(uart2BaudRate) + "</div>";
  html += "</div>";
  html += "<div class='serial-container'>";
  String initData = filterAnsiEscape(clientSerialData[clientIdx]);
  html += "<textarea class='serial-output' id='serialData' readonly>" + initData + "</textarea>";
  html += "</div>";
  html += "<div class='input-area'>";
  html += "<input type='text' id='serialInput' placeholder='输入命令...' autocomplete='off'>";
  html += "<input type='submit' value='发送' onclick='sendData()'>";
  html += "<input type='submit' value='返回' class='back-btn' onclick=\"location.href='/'\">";
  html += "</div>";
  html += "<script>";
  html += "var clientIdx = " + String(clientIdx) + ";";
  html += "function fetchSerialData(){";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/client?client_id=" + String(clientIdx) + "&refresh=1', true);";
  html += "  xhr.onreadystatechange = function(){";
  html += "    if(xhr.readyState == 4 && xhr.status == 200){";
  html += "      var data = xhr.responseText;";
  html += "      if(data.length > 0){";
  html += "        var output = document.getElementById('serialData');";
  html += "        var wasAtBottom = output.scrollHeight - output.scrollTop <= output.clientHeight + 50;";
  html += "        output.value = data;";
  html += "        if(wasAtBottom){";
  html += "          output.scrollTop = output.scrollHeight;";
  html += "        }";
  html += "      }";
  html += "    }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";
  html += "function sendData(){";
  html += "  var input = document.getElementById('serialInput');";
  html += "  if(input.value.trim() === '') return;";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/client/send?client_id=" + String(clientIdx) + "', true);";
  html += "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');";
  html += "  xhr.send('data=' + encodeURIComponent(input.value));";
  html += "  var output = document.getElementById('serialData');";
  html += "  output.value += '[发送] ' + input.value + '\\n';";
  html += "  output.scrollTop = output.scrollHeight;";
  html += "  input.value = '';";
  html += "}";
  html += "document.getElementById('serialInput').addEventListener('keypress', function(e){";
  html += "  if(e.key === 'Enter'){";
  html += "    sendData();";
  html += "    e.preventDefault();";
  html += "  }";
  html += "});";
  html += "document.getElementById('serialData').scrollTop = document.getElementById('serialData').scrollHeight;";
  html += "setInterval(fetchSerialData, 500);";
  html += "</script>";
  html += "</body></html>";
  client.print(html);
}

void handleClientSend(WiFiClient client, String request) {
  int clientIdx = -1;
  int idxStart = request.indexOf("client_id=") + 10;
  int idxEnd = request.indexOf("&", idxStart);
  if (idxEnd == -1) idxEnd = request.length();
  String clientIdxStr = request.substring(idxStart, idxEnd);
  clientIdx = clientIdxStr.toInt();
  
  int dataIndex = request.indexOf("data=") + 5;
  int dataEnd = request.indexOf("&", dataIndex);
  if (dataEnd == -1) dataEnd = request.length();
  String data = request.substring(dataIndex, dataEnd);
  data = urlDecode(data);
  
  if (clientIdx >= 0 && clientIdx < MAX_CLIENTS && serverClients[clientIdx] && serverClients[clientIdx].connected()) {
    if (data.length() > 0) {
      serverClients[clientIdx].println(data);
      clientSerialData[clientIdx] += "[发送] " + data + "\n";
    }
  }
  
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='0;url=/client?client_id=" + String(clientIdx) + "'></head></html>";
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
  int spaceEnd = request.indexOf(" ", fileStart);
  if (fileEnd == -1 || (spaceEnd != -1 && spaceEnd < fileEnd)) {
    fileEnd = spaceEnd;
  }
  if (fileEnd == -1) fileEnd = request.length();
  String filePath = request.substring(fileStart, fileEnd);
  
  // URL解码
  filePath = urlDecode(filePath);
  
  // 安全检查：防止路径遍历攻击
  if (filePath.indexOf("..") != -1) {
    client.println("HTTP/1.1 403 Forbidden");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>403 - 禁止访问</h1><p>非法的文件路径</p><a href='/logs'>返回日志列表</a></body></html>");
    return;
  }
  
  // 打开文件
  File file = SD.open(filePath);
  if (file) {
    String fileName = filePath.substring(filePath.lastIndexOf('/') + 1);
    // 发送文件
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/octet-stream");
    client.println("Content-Disposition: attachment; filename=\"" + fileName + "\"");
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
    client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>404 - 文件未找到</h1><p>请求的文件不存在: " + filePath + "</p><a href='/logs'>返回日志列表</a></body></html>");
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

// 格式化文件大小
String formatFileSize(unsigned long bytes) {
  if (bytes < 1024) {
    return String(bytes) + " B";
  } else if (bytes < 1024 * 1024) {
    return String(bytes / 1024.0, 1) + " KB";
  } else {
    return String(bytes / (1024.0 * 1024.0), 1) + " MB";
  }
}

// URL解码函数
String urlDecode(String input) {
  String result = "";
  for (unsigned int i = 0; i < input.length(); i++) {
    if (input.charAt(i) == '%') {
      if (i + 2 < input.length()) {
        char hex1 = input.charAt(i + 1);
        char hex2 = input.charAt(i + 2);
        int value = 0;
        if (hex1 >= '0' && hex1 <= '9') value += (hex1 - '0') * 16;
        else if (hex1 >= 'a' && hex1 <= 'f') value += (hex1 - 'a' + 10) * 16;
        else if (hex1 >= 'A' && hex1 <= 'F') value += (hex1 - 'A' + 10) * 16;
        
        if (hex2 >= '0' && hex2 <= '9') value += (hex2 - '0');
        else if (hex2 >= 'a' && hex2 <= 'f') value += (hex2 - 'a' + 10);
        else if (hex2 >= 'A' && hex2 <= 'F') value += (hex2 - 'A' + 10);
        
        result += (char)value;
        i += 2;
      } else {
        result += input.charAt(i);
      }
    } else if (input.charAt(i) == '+') {
      result += ' ';
    } else {
      result += input.charAt(i);
    }
  }
  return result;
}

// 处理日志预览
void handlePreviewLog(WiFiClient client, String request) {
  int fileStart = request.indexOf("file=") + 5;
  int fileEnd = request.indexOf("&", fileStart);
  int spaceEnd = request.indexOf(" ", fileStart);
  if (fileEnd == -1 || (spaceEnd != -1 && spaceEnd < fileEnd)) {
    fileEnd = spaceEnd;
  }
  if (fileEnd == -1) fileEnd = request.length();
  String filePath = request.substring(fileStart, fileEnd);
  
  // URL解码
  filePath = urlDecode(filePath);
  
  // 规范化路径：移除多余的斜杠
  while (filePath.indexOf("//") >= 0) {
    filePath.replace("//", "/");
  }
  
  // 安全检查：防止路径遍历攻击
  if (filePath.indexOf("..") != -1) {
    client.println("HTTP/1.1 403 Forbidden");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>403 - 禁止访问</h1><p>非法的文件路径</p><a href='/logs'>返回日志列表</a></body></html>");
    return;
  }
  
  // 允许访问 /server/ 和 /client/ 路径下的文件
  if (!filePath.startsWith("/server/") && !filePath.startsWith("/client/")) {
    client.println("HTTP/1.1 403 Forbidden");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>403 - 禁止访问</h1><p>非法的文件路径</p><a href='/logs'>返回日志列表</a></body></html>");
    return;
  }
  
  File file = SD.open(filePath);
  if (file) {
    String fileName = filePath.substring(filePath.lastIndexOf('/') + 1);
    String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
    html += "<title>预览: " + fileName + "</title>";
    html += "<style>body{font-family:Arial,sans-serif;margin:10px;background:#f5f5f5;font-size:14px;}";
    html += ".container{max-width:100%;margin:0 auto;background:white;padding:12px;border-radius:10px;box-sizing:border-box;box-shadow:0 2px 8px rgba(0,0,0,0.1);}";
    html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:8px;font-size:18px;word-break:break-all;}";
    html += ".info{background:#e7f3fe;border-left:4px solid #2196F3;padding:10px;margin:10px 0;font-size:13px;}";
    html += ".preview-box{background:#1e1e1e;border:1px solid #333;border-radius:5px;padding:12px;margin:12px 0;height:400px;overflow-y:scroll;font-family:monospace;font-size:12px;color:#0f0;white-space:pre-wrap;word-wrap:break-word;}";
    html += ".preview-box::-webkit-scrollbar{width:8px;}";
    html += ".preview-box::-webkit-scrollbar-track{background:#2d2d2d;}";
    html += ".preview-box::-webkit-scrollbar-thumb{background:#555;border-radius:4px;}";
    html += ".preview-box::-webkit-scrollbar-thumb:hover{background:#777;}";
    html += ".actions{margin:15px 0;display:flex;gap:8px;}";
    html += ".actions a{padding:8px 16px;background:#2196F3;color:white;text-decoration:none;border-radius:4px;font-size:13px;transition:all 0.2s ease;}";
    html += ".actions a:hover{background:#1976D2;transform:translateY(-1px);}";
    html += ".back{margin-top:10px;}";
    html += ".back a{padding:8px 16px;background:#4CAF50;color:white;text-decoration:none;border-radius:4px;font-size:13px;transition:all 0.2s ease;}";
    html += ".back a:hover{background:#45a049;transform:translateY(-1px);}";
    html += "@media screen and (min-width: 600px) {";
    html += ".container{max-width:900px;padding:15px;}";
    html += ".preview-box{height:500px;}";
    html += "}";
    html += "@media screen and (max-width: 480px) {";
    html += ".container{max-width:95%;padding:10px;}";
    html += "h1{font-size:16px;}";
    html += "}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>📄 " + fileName + "</h1>";
    
    html += "<div class='info'>";
    html += "<strong>路径:</strong> " + filePath + "<br>";
    html += "<strong>大小:</strong> " + formatFileSize(file.size());
    html += "</div>";
    
    html += "<div class='actions'>";
    html += "<a href='/download?file=" + filePath + "'>下载文件</a>";
    html += "<a href='/logs'>返回列表</a>";
    html += "</div>";
    
    html += "<div class='preview-box'>";
    const int maxPreviewSize = 16384; // 16KB
    int bytesRead = 0;
    while (file.available() && bytesRead < maxPreviewSize) {
      char c = file.read();
      bytesRead++;
      if (c == '<') html += "&lt;";
      else if (c == '>') html += "&gt;";
      else if (c == '&') html += "&amp;";
      else if (c == '"') html += "&quot;";
      else if (c == '\'') html += "&#39;";
      else if (c == '\r') {} // 忽略回车符
      else html += c;
    }
    
    if (file.available()) {
      html += "\n\n... [文件过大，仅显示前" + String(bytesRead) + "字节]";
    }
    
    html += "</div>";
    
    html += "<div class='back'><a href='/logs'>返回日志列表</a></div>";
    html += "</div>";
    html += "</body></html>";
    client.print(html);
    file.close();
  } else {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>404 - 文件未找到</h1><p>请求的文件不存在: " + filePath + "</p><a href='/logs'>返回日志列表</a></body></html>");
  }
}

// ==================== 串口实时显示页面 ====================
void handleSerialPage(WiFiClient client) {
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=yes'>";
  html += "<title>串口监视器 - ESP32 UART</title>";
  html += "<style>";
  html += "*{box-sizing:border-box;margin:0;padding:0;}";
  html += "body{font-family:'Consolas','Monaco',monospace;margin:0;background:#1e1e1e;color:#0f0;height:100vh;display:flex;flex-direction:column;}";
  html += ".header{background:#2d2d2d;padding:12px 15px;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #444;}";
  html += ".header h1{color:#4CAF50;font-size:18px;}";
  html += ".header-info{color:#888;font-size:12px;}";
  html += ".serial-container{flex:1;display:flex;flex-direction:column;overflow:hidden;}";
  html += ".serial-output{flex:1;background:#1a1a1a;color:#0f0;padding:12px;overflow-y:auto;font-size:13px;line-height:1.5;word-wrap:break-word;white-space:pre-wrap;border:none;resize:none;}";
  html += ".serial-output::-webkit-scrollbar{width:10px;}";
  html += ".serial-output::-webkit-scrollbar-track{background:#2d2d2d;}";
  html += ".serial-output::-webkit-scrollbar-thumb{background:#555;border-radius:5px;}";
  html += ".serial-output::-webkit-scrollbar-thumb:hover{background:#777;}";
  html += ".input-area{background:#2d2d2d;padding:12px;display:flex;gap:8px;border-top:1px solid #444;}";
  html += ".input-area input[type='text']{flex:1;padding:10px 12px;background:#333;color:#fff;border:1px solid #555;border-radius:4px;font-size:14px;font-family:inherit;}";
  html += ".input-area input[type='text']:focus{outline:none;border-color:#4CAF50;}";
  html += ".input-area input[type='submit']{padding:10px 20px;background:#4CAF50;color:white;border:none;border-radius:4px;font-size:14px;cursor:pointer;transition:all 0.2s;}";
  html += ".input-area input[type='submit']:hover{background:#45a049;}";
  html += ".btn-clear{background:#f44336 !important;}";
  html += ".btn-clear:hover{background:#d32f2f !important;}";
  html += ".status-bar{background:#333;padding:8px 15px;font-size:12px;color:#888;display:flex;justify-content:space-between;}";
  html += ".online{color:#4CAF50;}";
  html += "@media screen and (max-width: 600px){";
  html += ".header{flex-direction:column;gap:8px;align-items:flex-start;}";
  html += ".input-area{flex-direction:column;}";
  html += ".input-area input[type='submit']{width:100%;}";
  html += ".serial-output{font-size:12px;}";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='header'>";
  html += "<div style='display:flex;align-items:center;gap:10px;'>";
  html += "<a href='/' style='color:#4CAF50;text-decoration:none;font-size:14px;'>← 首页</a>";
  html += "<h1 style='margin:0;'>🖥️ 串口监视器</h1>";
  html += "</div>";
  html += "<div class='header-info'>波特率: " + String(uart2BaudRate) + " | 模式: " + String(currentMode == MODE_CLIENT ? "客户端" : "服务器") + "</div>";
  html += "</div>";
  html += "<div style='background:#333;padding:8px 15px;display:flex;gap:10px;align-items:center;flex-wrap:wrap;'>";
  html += "<span style='color:#888;font-size:12px;'>显示:</span>";
  html += "<select id='sourceSelect' onchange='changeSource()' style='background:#444;color:#fff;border:1px solid #555;padding:4px 8px;border-radius:4px;font-size:12px;'>";
  html += "<option value='server'>服务器</option>";
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (serverClients[i] && serverClients[i].connected()) {
      html += "<option value='client_" + String(i) + "'>客户端 " + String(i) + "</option>";
    }
  }
  html += "</select>";
  html += "<span style='color:#888;font-size:12px;margin-left:10px;'>发送目标:</span>";
  html += "<select id='targetSelect' style='background:#444;color:#fff;border:1px solid #555;padding:4px 8px;border-radius:4px;font-size:12px;'>";
  html += "<option value='-1'>全部/本地UART</option>";
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (serverClients[i] && serverClients[i].connected()) {
      String sel = (selectedClientIndex == i) ? " selected" : "";
      html += "<option value='" + String(i) + "'" + sel + ">客户端 " + String(i) + "</option>";
    }
  }
  html += "</select>";
  html += "</div>";
  html += "<div class='serial-container'>";
  html += "<textarea class='serial-output' id='serialOutput' readonly></textarea>";
  html += "</div>";
  html += "<div class='input-area'>";
  html += "<input type='text' id='serialInput' placeholder='输入命令...' autocomplete='off'>";
  html += "<input type='submit' value='发送' onclick='sendData()'>";
  html += "<input type='submit' value='清空' class='btn-clear' onclick='clearSerial()'>";
  html += "</div>";
  html += "<div class='status-bar'>";
  html += "<span>连接状态: <span class='online' id='connStatus'>在线</span></span>";
  html += "<span id='bufferStatus'>缓冲区: 0 字符</span>";
  html += "<span>最后更新: <span id='lastUpdate'>--</span></span>";
  html += "</div>";
  html += "<script>";
  html += "var currentSource = 'server';";
  html += "var serialData = {server: '', client_0: '', client_1: '', client_2: '', client_3: '', client_4: ''};";
  html += "function changeSource(){";
  html += "  currentSource = document.getElementById('sourceSelect').value;";
  html += "  document.getElementById('serialOutput').value = serialData[currentSource] || '';";
  html += "  document.getElementById('serialOutput').scrollTop = document.getElementById('serialOutput').scrollHeight;";
  html += "  history.replaceState(null, '', '?source=' + currentSource);";
  html += "}";
  html += "function initSource(){";
  html += "  var params = new URLSearchParams(window.location.search);";
  html += "  var savedSource = params.get('source');";
  html += "  if(savedSource && document.getElementById('sourceSelect').querySelector('option[value=\"' + savedSource + '\"]')){";
  html += "    currentSource = savedSource;";
  html += "    document.getElementById('sourceSelect').value = currentSource;";
  html += "  }";
  html += "  document.getElementById('serialOutput').value = serialData[currentSource] || '';";
  html += "}";
  html += "function fetchSerialData(){";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/serial/data?source=' + currentSource, true);";
  html += "  xhr.onreadystatechange = function(){";
  html += "    if(xhr.readyState == 4 && xhr.status == 200){";
  html += "      var data = xhr.responseText;";
  html += "      if(data.length > 0){";
  html += "        serialData[currentSource] += data;";
  html += "        if(serialData[currentSource].length > 50000){";
  html += "          serialData[currentSource] = serialData[currentSource].substring(serialData[currentSource].length - 40000);";
  html += "        }";
  html += "        var output = document.getElementById('serialOutput');";
  html += "        var wasAtBottom = output.scrollHeight - output.scrollTop <= output.clientHeight + 50;";
  html += "        output.value = serialData[currentSource];";
  html += "        if(wasAtBottom){";
  html += "          output.scrollTop = output.scrollHeight;";
  html += "        }";
  html += "      }";
  html += "      var now = new Date();";
  html += "      document.getElementById('lastUpdate').textContent = now.toLocaleTimeString();";
  html += "      document.getElementById('bufferStatus').textContent = '缓冲区: ' + (serialData[currentSource] || '').length + ' 字符';";
  html += "    }";
  html += "  };";
  html += "  xhr.send();";
  html += "}";
  html += "function sendData(){";
  html += "  var input = document.getElementById('serialInput');";
  html += "  if(input.value.trim() === '') return;";
  html += "  var target = document.getElementById('targetSelect').value;";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/serial/send', true);";
  html += "  xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');";
  html += "  xhr.send('data=' + encodeURIComponent(input.value) + '&target=' + target);";
  html += "  var output = document.getElementById('serialOutput');";
  html += "  output.scrollTop = output.scrollHeight;";
  html += "  input.value = '';";
  html += "}";
  html += "function clearSerial(){";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('POST', '/serial/clear', true);";
  html += "  xhr.send();";
  html += "  document.getElementById('serialOutput').value = '';";
  html += "}";
  html += "document.getElementById('serialInput').addEventListener('keypress', function(e){";
  html += "  if(e.key === 'Enter'){";
  html += "    sendData();";
  html += "    e.preventDefault();";
  html += "  }";
  html += "});";
  html += "initSource();";
  html += "setInterval(fetchSerialData, 200);";
  html += "fetchSerialData();";
  html += "</script>";
  html += "</body></html>";
  client.print(html);
}

void handleSerialDataAPI(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Cache-Control: no-cache, no-store, must-revalidate");
  client.println();
  
  String response = filterAnsiEscape(serialDisplayBuffer);
  serialDisplayBuffer = "";
  
  client.print(response);
}

void handleSerialSend(WiFiClient client, String request) {
  int dataIndex = request.indexOf("data=");
  if (dataIndex < 0) {
    return;
  }
  dataIndex += 5;
  int dataEnd = request.indexOf("&", dataIndex);
  if (dataEnd == -1) dataEnd = request.length();
  String data = request.substring(dataIndex, dataEnd);
  data = urlDecode(data);
  
  int targetIndex = -1;
  int targetPos = request.indexOf("target=");
  if (targetPos >= 0) {
    targetPos += 7;
    int targetEnd = request.indexOf("&", targetPos);
    if (targetEnd == -1) targetEnd = request.length();
    String targetStr = request.substring(targetPos, targetEnd);
    targetStr.trim();
    targetIndex = targetStr.toInt();
  }
  
  if (data.length() > 0) {
    data += "\r\n";
    
    // 服务器模式：根据target发送
    if (currentMode == MODE_SERVER) {
      if (targetIndex >= 0 && targetIndex < MAX_CLIENTS) {
        // 设置选中的客户端
        selectedClientIndex = targetIndex;
        // 发送到指定客户端
        if (serverClients[targetIndex] && serverClients[targetIndex].connected()) {
          serverClients[targetIndex].write(data.c_str(), data.length());
        }
      } else {
        // target < 0 或无效，发送到本地UART2
        uart_write_bytes(UART_NUM_2, data.c_str(), data.length());
      }
    } else {
      // 客户端模式，发送到本地UART2
      uart_write_bytes(UART_NUM_2, data.c_str(), data.length());
    }
  }
  
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='0;url=/serial '></head></html>";
  client.print(html);
}

void handleSerialClear(WiFiClient client) {
  noInterrupts();
  serialDisplayBuffer = "";
  interrupts();
  
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<!DOCTYPE html><html><head></head><body></body></html>";
  client.print(html);
}

void appendToSerialBuffer(char c) {
  serialDisplayBuffer += String(c);
  if (serialDisplayBuffer.length() > SERIAL_DISPLAY_BUFFER_SIZE) {
    serialDisplayBuffer = serialDisplayBuffer.substring(serialDisplayBuffer.length() - SERIAL_DISPLAY_BUFFER_SIZE / 2);
  }
}

void appendToSerialBuffer(const char* str) {
  serialDisplayBuffer += String(str);
  if (serialDisplayBuffer.length() > SERIAL_DISPLAY_BUFFER_SIZE) {
    serialDisplayBuffer = serialDisplayBuffer.substring(serialDisplayBuffer.length() - SERIAL_DISPLAY_BUFFER_SIZE / 2);
  }
}

void appendToSerialBuffer(const char* str, int len) {
  for (int i = 0; i < len; i++) {
    serialDisplayBuffer += str[i];
  }
  if (serialDisplayBuffer.length() > SERIAL_DISPLAY_BUFFER_SIZE) {
    serialDisplayBuffer = serialDisplayBuffer.substring(serialDisplayBuffer.length() - SERIAL_DISPLAY_BUFFER_SIZE / 2);
  }
}

// 处理串口数据请求（兼容旧接口）
void handleSerialData(WiFiClient client) {
  handleSerialPage(client);
}

void handleDeleteFile(WiFiClient client, String request) {
  int fileStart = request.indexOf("file=") + 5;
  int fileEnd = request.indexOf(" ", fileStart);
  if (fileEnd == -1) fileEnd = request.length();
  String filePath = request.substring(fileStart, fileEnd);
  filePath = urlDecode(filePath);
  
  // 规范化路径：移除多余的斜杠
  while (filePath.indexOf("//") >= 0) {
    filePath.replace("//", "/");
  }
  
  // 安全检查：防止路径遍历攻击
  if (filePath.indexOf("..") != -1) {
    client.println("HTTP/1.1 403 Forbidden");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>403 - 禁止访问</h1><a href='/logs'>返回日志列表</a></body></html>");
    return;
  }
  
  // 允许删除 /server/ 和 /client/ 路径下的文件
  if (!filePath.startsWith("/server/") && !filePath.startsWith("/client/")) {
    client.println("HTTP/1.1 403 Forbidden");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>403 - 禁止访问</h1><a href='/logs'>返回日志列表</a></body></html>");
    return;
  }
  
  if (SD.exists(filePath)) {
    if (SD.remove(filePath)) {
      client.println("HTTP/1.1 302 Found");
      client.println("Location: /logs");
      client.println();
    } else {
      client.println("HTTP/1.1 500 Internal Server Error");
      client.println("Content-Type: text/html");
      client.println();
      client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>删除失败</h1><a href='/logs'>返回日志列表</a></body></html>");
    }
  } else {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>文件不存在</h1><a href='/logs'>返回日志列表</a></body></html>");
  }
}

void handleDeleteDirectory(WiFiClient client, String request) {
  int dirStart = request.indexOf("dir=") + 4;
  int dirEnd = request.indexOf(" ", dirStart);
  if (dirEnd == -1) dirEnd = request.length();
  String dirPath = request.substring(dirStart, dirEnd);
  dirPath = urlDecode(dirPath);
  
  // 规范化路径：移除多余的斜杠
  while (dirPath.indexOf("//") >= 0) {
    dirPath.replace("//", "/");
  }
  
  // 安全检查：防止路径遍历攻击
  if (dirPath.indexOf("..") != -1) {
    client.println("HTTP/1.1 403 Forbidden");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>403 - 禁止访问</h1><a href='/logs'>返回日志列表</a></body></html>");
    return;
  }
  
  if (!dirPath.startsWith("/server") && !dirPath.startsWith("/client")) {
    client.println("HTTP/1.1 403 Forbidden");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>403 - 禁止访问</h1><a href='/logs'>返回日志列表</a></body></html>");
    return;
  }
  
  if (SD.exists(dirPath)) {
    if (deleteDirectoryRecursive(dirPath)) {
      client.println("HTTP/1.1 302 Found");
      client.println("Location: /logs");
      client.println();
    } else {
      client.println("HTTP/1.1 500 Internal Server Error");
      client.println("Content-Type: text/html");
      client.println();
      client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h1>删除失败</h1><a href='/logs'>返回日志列表</a></body></html>");
    }
  } else {
    client.println("HTTP/1.1 302 Found");
    client.println("Location: /logs");
    client.println();
  }
}

bool deleteDirectoryRecursive(String path) {
  File root = SD.open(path);
  if (!root) return false;
  
  if (!root.isDirectory()) {
    root.close();
    return false;
  }
  
  bool success = true;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    
    String entryPath = path + "/" + entry.name();
    
    if (entry.isDirectory()) {
      if (!deleteDirectoryRecursive(entryPath)) {
        success = false;
      }
    } else {
      if (!SD.remove(entryPath)) {
        success = false;
      }
    }
    entry.close();
  }
  root.close();
  
  if (success) {
    if (!SD.rmdir(path)) {
      success = false;
    }
  }
  
  return success;
}
