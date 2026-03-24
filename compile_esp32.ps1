# ESP32编译脚本
$projectPath = "C:\Users\liuyong\Documents\aily-project\dual-mode-uart-enhanced"

# 创建arduino-cli配置文件
$configDir = "$env:LOCALAPPDATA\Arduino15"
if (-not (Test-Path $configDir)) { New-Item -ItemType Directory -Path $configDir -Force | Out-Null }

$configFile = "$configDir\arduino-cli.yaml"
$config = @"
board_manager:
  additional_urls:
    - https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
directories:
  data: D:\arduino-ide\Arduino15
  downloads: D:\arduino-ide\Arduino15\staging
  user: D:\arduino-ide\Arduino
"@

Set-Content -Path $configFile -Value $config -Encoding UTF8

Write-Host "编译项目: $projectPath"
Write-Host "目标: esp32:esp32:esp32s3"

& "C:\Users\liuyong\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "$projectPath" 2>&1