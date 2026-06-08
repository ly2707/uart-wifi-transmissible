# 构建、烧录与运维

本文档面向测试、交付和现场工程师，说明如何构建固件、烧录设备、理解运行时行为以及使用 Web 与日志能力进行运维。

## 1. 依赖环境

根据当前工程和本地构建脚本，推荐环境如下：

- Arduino CLI
- ESP32 Arduino Core 3.3.8
- WiFiManager 2.0.17
- FastLED 3.10.3
- Windows PowerShell 5.1+
- esptool（用于烧录）

## 2. 编译方式

### 2.1 推荐方式

```powershell
.\compile.ps1
```

行为说明：

- 固定目标板为 esp32:esp32:esp32s3
- 固定输出目录为 build/esp32.esp32.esp32s3
- 使用 clean 构建，减少旧对象文件干扰
- 附加编译宏 BOARD_HAS_PSRAM

### 2.2 其他脚本

| 脚本 | 用途 |
| --- | --- |
| compile.ps1 | 推荐的 Arduino CLI 编译入口 |
| compile_esp32.ps1 | 使用特定 arduino-cli 配置的编译脚本 |
| compile_project.bat | 面向手工流程的辅助脚本 |

### 2.3 SecureCRT 自动化测试资产

| 资源 | 用途 |
| --- | --- |
| [scripts/securecrt/d3000_automation.vbs](../../scripts/securecrt/d3000_automation.vbs) | SecureCRT 自动化主脚本，用于飞腾 D3000 Linux 测试机的登录、控制和测试编排 |
| [SecureCRT 自动化测试说明](securecrt_automation.md) | 脚本配置、网页计划格式、执行顺序和注意事项 |
| [scripts/securecrt/plans/full_regression_plan.txt](../../scripts/securecrt/plans/full_regression_plan.txt) | 综合自动化回归模板 |

## 3. 烧录方式

```powershell
.\build_and_flash.ps1 -Port COM19
```

脚本特点：

- 默认端口为 COM19，可通过 -Port 覆盖
- 使用合并后的 .merged.bin 做整片写入
- 默认烧录波特率为 921600

## 4. 启动场景

### 4.1 客户端模式

1. 读取 EEPROM 配置。
2. 若没有有效 WiFi 配置，进入 WiFiManager 配网页面。
3. 若有有效配置，尝试连接 STA。
4. 连接成功后建立到 192.168.1.1:8080 的 TCP 连接。
5. UART2 数据与 TCP 通道之间双向透传，入口统一经过安全帧校验。

### 4.2 服务器模式

1. 启动 SoftAP。
2. 启动 TCP Server，监听 8080。
3. 启动 Web Server，监听 80。
4. 接收最多 5 个客户端。
5. UART2 数据会被封装成安全帧后广播，或定向发送给选中客户端。

## 5. Web 路由总览

| 路由 | 方法 | 说明 |
| --- | --- | --- |
| / | GET | 首页，总览入口 |
| /status | GET | 系统状态页 |
| /config | GET | 配置页 |
| /script | GET | 网页脚本编程页，支持脚本编辑、保存和执行 |
| /saveconfig | POST | 保存配置 |
| /script/save | POST | 保存网页脚本到 SD 卡 |
| /script/run | POST | 按行执行网页脚本 |
| /serial | GET | 串口监视页 |
| /serial/data | GET | 串口缓冲拉取接口 |
| /serial/send | POST | Web 发送串口数据 |
| /serial/clear | POST | 清空串口显示缓冲 |
| /logs | GET | 日志列表 |
| /preview | GET | 日志预览 |
| /download | GET | 日志下载 |
| /clear | GET | 清理日志 |
| /delete | GET | 删除文件 |
| /deletedir | GET | 删除目录 |
| /client | GET | 服务器模式下的客户端详情页 |
| /client/send | POST | 向指定客户端发送数据 |
| /power | POST | 电源控制接口；不带 client_id 时控制本机，带 client_id 时向指定在线客户端下发远程电源控制指令 |

说明：HTTP 请求头和请求体都带长度上限，超限会直接返回 413。

## 6. 日志目录结构

SD 卡目录会根据模式和串口通道自动区分：

| 目录 | 含义 |
| --- | --- |
| /client_local | 客户端模式 UART2 日志 |
| /client_u1 | 客户端模式 UART1 日志 |
| /server/system | 服务器模式系统事件日志 |
| /server/&lt;client&gt; | 服务器模式各客户端 UART2 日志 |
| /server/uart1 | 服务器模式 UART1 日志 |

日志文件名格式：

```text
<logFileName>_<date>.txt
```

## 7. 运维建议

### 7.1 首次部署

1. 先确认板级引脚接线与供电稳定。
2. 编译后完成一次烧录。
3. 客户端模式先完成配网，再验证 TCP 连通。
4. 服务器模式验证 AP、Web 页面和 TCP 接入数。
5. 插入 SD 卡后检查日志目录是否创建成功。

### 7.2 现场调试优先级

1. 查看 LED 状态
2. 通过 USB 执行 AT+STATUS
3. 打开 Web 状态页 /status
4. 检查日志目录和 system 日志
5. 再定位串口/网络/SD 的具体链路问题

## 8. 常见问题

### 编译卡住或无输出

- 优先直接调用 Arduino CLI，而不是只看包装脚本输出
- 固定 build-path，避免 Windows 临时目录对象文件占用
- 确认 Arduino CLI、ESP32 Core 和库版本与工程一致

### 客户端一直不连网

- 检查 EEPROM 是否已有合法 WiFi 配置
- 必要时执行恢复默认，再重新配网
- 确认目标 AP 的 SSID/密码满足安全校验规则

### 服务器有 AP 但客户端收不到数据

- 检查是否命中安全帧校验
- 检查 selectedClientIndex 是否被定向到单一客户端
- 检查 Web 或 USB 发送内容是否符合白名单规则

### SD 不写日志

- 检查 SD 卡挂载是否成功
- 检查目录是否已创建
- 确认 logToSD 为开启状态
- 查看是否出现 SD 卡热拔插或错误告警

## 9. 建议阅读下一篇

如果需要做飞腾 D3000 调试机自动化联调，先阅读 [SecureCRT 自动化测试说明](securecrt_automation.md)；继续了解固件输入校验和攻击缓解机制，请阅读 [安全设计](../security/README.md)。
