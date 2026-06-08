# SecureCRT 自动化测试说明

本文档说明如何使用仓库中新增的 SecureCRT VBScript 和网页测试计划模板，对飞腾 D3000 Linux 测试机执行串口自动化控制、稳定性测试以及网页导入测试计划。

## 1. 文件位置

| 文件 | 用途 |
| --- | --- |
| [scripts/securecrt/d3000_automation.vbs](../../scripts/securecrt/d3000_automation.vbs) | SecureCRT 主脚本，负责登录、执行命令、重启、复位、拉取测试计划和记日志 |
| [scripts/securecrt/plans/reboot_stability_plan.txt](../../scripts/securecrt/plans/reboot_stability_plan.txt) | 重启稳定性测试模板 |
| [scripts/securecrt/plans/power_recovery_plan.txt](../../scripts/securecrt/plans/power_recovery_plan.txt) | 断电恢复测试模板 |
| [scripts/securecrt/plans/network_recovery_plan.txt](../../scripts/securecrt/plans/network_recovery_plan.txt) | 网络恢复测试模板 |
| [scripts/securecrt/plans/web_api_test_plan.txt](../../scripts/securecrt/plans/web_api_test_plan.txt) | 网页接口测试模板 |
| [scripts/securecrt/plans/full_regression_plan.txt](../../scripts/securecrt/plans/full_regression_plan.txt) | 综合自动化回归模板 |

## 2. 适用场景

该脚本适合以下场景：

- 通过 SecureCRT 串口会话控制飞腾 D3000 Linux 测试机
- 统一执行重启、软复位、开机、关机再上电等控制动作
- 对调试机开展重启稳定性、网络恢复、压力测试和网页接口测试
- 通过 HTTP 地址导入纯文本测试计划并自动执行
- 将执行过程和目标机输出统一记录到 SecureCRT 日志文件

## 3. 使用前准备

### 3.1 SecureCRT 环境

- 在 Windows 主机安装 SecureCRT，并确认可以通过串口或 SSH 连接测试机
- 推荐优先使用串口 Console 会话，便于在重启后继续等待登录提示
- 为当前会话确认登录提示符，例如以 `#` 或 `$` 结尾的 shell 提示符

### 3.2 目标机环境

- 确认飞腾 D3000 Linux 系统可通过串口登录
- 确认目标机支持执行 `reboot`、`ip addr`、`ip link`、`ping`、`dmesg` 等常用命令
- 如果需要 CPU/内存压力测试，建议预装 `stress-ng` 或 `stress`
- 如果需要软复位，目标机应允许 `sysrq-trigger`，并保证当前用户拥有足够权限

### 3.3 外部电源控制能力

仅依靠串口无法在设备彻底断电后直接开机，因此以下动作依赖外部电源控制能力：

- 开机
- 关机再上电
- 断电恢复测试

主脚本已预留两类接入方式：

- HTTP 模式：调用 PDU、继电器控制器、BMC 或测试平台提供的 HTTP 接口
- command 模式：在运行 SecureCRT 的 Windows 主机上调用 `ipmitool`、`wolcmd`、PowerShell 或其他本地命令

如果当前没有外部电源控制设备，请暂时不要执行 `POWER_ON` 和 `POWER_CYCLE` 相关动作。

## 4. 需要先修改的脚本配置

首次使用前，请编辑 [scripts/securecrt/d3000_automation.vbs](../../scripts/securecrt/d3000_automation.vbs)，至少检查以下常量：

| 配置项 | 说明 |
| --- | --- |
| `LOGIN_USER` / `LOGIN_PASSWORD` | 目标机登录账号与密码 |
| `PRIMARY_PROMPT` / `SECONDARY_PROMPT` | shell 提示符，需与实际环境匹配 |
| `USE_SUDO` | 非 root 用户登录时改为 `True` |
| `REBOOT_CMD` | 默认使用 `/sbin/reboot`，如环境不同可调整 |
| `SOFT_RESET_CMD` | 默认通过 sysrq 触发软复位 |
| `POWER_PROVIDER` | 选择 `http`、`command` 或 `none` |
| `POWER_ON_URL` / `POWER_CYCLE_URL` | HTTP 电源控制接口地址 |
| `POWER_ON_CMD` / `POWER_CYCLE_CMD` | 本地电源控制命令 |
| `DEFAULT_LOG_DIR` | SecureCRT 日志输出目录 |

## 5. 脚本支持的动作

启动脚本后会弹出菜单，支持以下动作：

| 菜单编号 | 动作 | 说明 |
| --- | --- | --- |
| 1 | Health check | 收集时间、内核、系统版本、内存、磁盘、网络和 dmesg 尾部日志 |
| 2 | Single reboot | 对目标机执行一次重启，并等待恢复 |
| 3 | Single soft reset | 通过 sysrq 执行软复位，并等待恢复 |
| 4 | Power on | 调用外部电源接口执行开机 |
| 5 | Power cycle | 调用外部电源接口执行断电后再上电 |
| 6 | Reboot stability test | 连续执行多轮重启稳定性测试 |
| 7 | CPU/memory stress test | 运行 `stress-ng` 或 `stress` |
| 8 | Load plan from URL | 从网页 URL 拉取纯文本测试计划并自动执行 |

## 6. 网页测试计划格式

脚本通过 HTTP 拉取纯文本计划文件，建议服务端返回 `text/plain`。计划文件规则如下：

- 每行一条动作
- 以 `#` 开头的行为注释
- 参数分隔符固定为 `@@`
- 支持 `%TIMESTAMP%` 宏，用于日志文件名

示例：

```text
# Example plan
LOG_START@@C:\SecureCRTLogs\demo_%TIMESTAMP%.log
CHECK_LOGIN@@180
HEALTH_CHECK
RUN@@uname -a@@20
REBOOT_LOOP@@20@@180
LOG_STOP
```

### 6.1 已支持的计划动作

| 动作 | 参数 | 说明 |
| --- | --- | --- |
| `LOG_START` | `path` | 开始写 SecureCRT 日志 |
| `LOG_STOP` | 无 | 停止日志 |
| `CHECK_LOGIN` | `timeout` | 确保已进入 shell |
| `RUN` | `command`, `timeout` | 执行目标机命令 |
| `EXPECT` | `command`, `expect`, `timeout` | 执行命令并校验输出包含指定内容 |
| `REBOOT` | `boot_timeout` | 单次重启 |
| `RESET` | `boot_timeout` | 单次软复位 |
| `POWER_ON` | `boot_timeout` | 开机 |
| `POWER_CYCLE` | `off_delay`, `boot_timeout` | 断电后上电 |
| `HEALTH_CHECK` | 无 | 执行健康检查 |
| `REBOOT_LOOP` | `rounds`, `boot_timeout` | 连续重启测试 |
| `STRESS_CPU` | `seconds` | CPU/内存压力测试 |
| `PING` | `host`, `count`, `timeout` | 连通性测试 |
| `WAIT` | `seconds` | 等待 |
| `HTTP_GET` | `url`, `expect` | 由 Windows 主机直接访问网页并校验返回内容 |

## 7. 自带测试计划模板说明

### 7.1 重启稳定性测试

使用 [scripts/securecrt/plans/reboot_stability_plan.txt](../../scripts/securecrt/plans/reboot_stability_plan.txt) 验证多轮重启后的可恢复性、可登录性和基础运行状态。

### 7.2 断电恢复测试

使用 [scripts/securecrt/plans/power_recovery_plan.txt](../../scripts/securecrt/plans/power_recovery_plan.txt) 验证断电再上电后的启动恢复能力。该测试依赖外部电源控制能力。

### 7.3 网络恢复测试

使用 [scripts/securecrt/plans/network_recovery_plan.txt](../../scripts/securecrt/plans/network_recovery_plan.txt) 验证网卡 down/up 之后的网络恢复流程。请先把模板中的 `eth0` 和目标网关地址替换成实际值。

### 7.4 网页接口测试

使用 [scripts/securecrt/plans/web_api_test_plan.txt](../../scripts/securecrt/plans/web_api_test_plan.txt) 验证网页首页、状态页和配置页访问。请先把模板中的 IP 地址替换成实际设备地址。

### 7.5 综合回归测试

使用 [scripts/securecrt/plans/full_regression_plan.txt](../../scripts/securecrt/plans/full_regression_plan.txt) 顺序执行基础检查、网页检查、网络恢复、压力测试、重启循环和断电恢复测试。

## 8. 建议执行顺序

为了降低一次性联调复杂度，建议按以下顺序落地：

1. 先完成 [scripts/securecrt/d3000_automation.vbs](../../scripts/securecrt/d3000_automation.vbs) 中的账号、提示符和日志目录配置
2. 再运行健康检查，确认串口登录链路正常
3. 先验证重启稳定性测试
4. 再验证网络恢复和网页接口测试
5. 最后在外部电源控制能力确认可用后，再验证断电恢复和综合回归测试

## 9. 常见注意事项

- 如果使用 SSH 会话而不是串口会话，目标机重启后会话可能断开，需要在 SecureCRT 会话属性中配合自动重连策略使用
- 如果提示符不是常见的 `#` 或 `$` 风格，必须手动调整脚本中的提示符配置，否则脚本会一直等待超时
- `HTTP_GET` 是由运行 SecureCRT 的 Windows 主机发起，而不是由飞腾 D3000 测试机发起
- 网页计划文件建议由 Nginx、Apache、Python 简易 HTTP 服务或自有测试平台提供，只要能返回纯文本即可

## 10. 相关文档

- [构建、烧录与运维总览](README.md)
- [配置与接口](../configuration/README.md)
- [安全设计](../security/README.md)
