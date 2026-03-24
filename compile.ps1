# Arduino CLI 编译脚本
$arduinoCLI = "C:\Users\liuyong\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$sketchPath = "C:\Users\liuyong\Documents\aily-project\dual-mode-uart-enhanced"
$board = "AirM2M:AirMCU:AirM2M_ESP32S3"

Write-Host "编译项目: $sketchPath"
Write-Host "目标板: $board"

# 编译
& $arduinoCLI compile --fqbn $board --build-property "compiler.cpp.extra_flags=-DBOARD_HAS_PSRAM" $sketchPath

if ($LASTEXITCODE -eq 0) {
    Write-Host "编译成功!" -ForegroundColor Green
} else {
    Write-Host "编译失败!" -ForegroundColor Red
}