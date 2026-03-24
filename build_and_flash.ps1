# 一键编译和烧录脚本
# 使用方法：.\build_and_flash.ps1 [端口号]

param (
    [string]$Port = "COM19"
)

# 颜色设置
$ErrorColor = "Red"
$SuccessColor = "Green"
$InfoColor = "Cyan"

# 项目路径
$ProjectPath = "$PSScriptRoot"
$BuildPath = "$ProjectPath\build\esp32.esp32.esp32s3"
$MergedBin = "$BuildPath\dual-mode-uart-enhanced.ino.merged.bin"

# 烧录工具路径
$EsptoolPath = "D:\arduino-ide\Arduino15\packages\esp32\tools\esptool_py\5.1.0\esptool.exe"

Write-Host "=========================================" -ForegroundColor $InfoColor
Write-Host "ESP32 UART透传系统 - 一键编译烧录脚本" -ForegroundColor $InfoColor
Write-Host "=========================================" -ForegroundColor $InfoColor

# 检查端口是否存在
$AvailablePorts = [System.IO.Ports.SerialPort]::GetPortNames()
if (-not $AvailablePorts.Contains($Port)) {
    Write-Host "错误：端口 $Port 不存在或不可用！" -ForegroundColor $ErrorColor
    Write-Host "可用端口：$($AvailablePorts -join ", ")" -ForegroundColor $InfoColor
    exit 1
}

Write-Host "目标端口：$Port" -ForegroundColor $InfoColor

# 提示用户在Arduino IDE中编译项目
Write-Host "" -ForegroundColor $InfoColor
Write-Host "请在Arduino IDE中编译项目：" -ForegroundColor $InfoColor
Write-Host "1. 打开 Arduino IDE"
Write-Host "2. 打开项目文件: dual-mode-uart-enhanced.ino"
Write-Host "3. 点击左上角的 '验证' 按钮（对勾图标）"
Write-Host "4. 等待编译完成"
Write-Host "" -ForegroundColor $InfoColor
Write-Host "编译完成后，按任意键继续烧录..." -ForegroundColor $InfoColor
$Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown") | Out-Null

# 检查固件文件是否存在
if (-not (Test-Path $MergedBin)) {
    Write-Host "" -ForegroundColor $ErrorColor
    Write-Host "错误：固件文件不存在！请先在Arduino IDE中编译项目。" -ForegroundColor $ErrorColor
    Write-Host "固件路径：$MergedBin" -ForegroundColor $InfoColor
    exit 1
}

# 显示固件文件信息
$FileInfo = Get-Item $MergedBin
Write-Host "" -ForegroundColor $InfoColor
Write-Host "固件文件：$($FileInfo.Name)" -ForegroundColor $InfoColor
Write-Host "文件大小：$([math]::Round($FileInfo.Length / 1KB, 2)) KB" -ForegroundColor $InfoColor
Write-Host "修改时间：$($FileInfo.LastWriteTime)" -ForegroundColor $InfoColor

# 烧录命令
$FlashCommand = @(
    "--port", $Port,
    "--baud", "921600",
    "--chip", "esp32s3",
    "write-flash",
    "--flash-mode", "dio",
    "--flash-freq", "80m",
    "--flash-size", "4MB",
    "0x0",
    $MergedBin
)

Write-Host "" -ForegroundColor $InfoColor
Write-Host "正在烧录固件到 $Port..." -ForegroundColor $InfoColor
Write-Host "=========================================" -ForegroundColor $InfoColor

# 执行烧录
try {
    & $EsptoolPath $FlashCommand
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "" -ForegroundColor $SuccessColor
        Write-Host "? 烧录成功！" -ForegroundColor $SuccessColor
        Write-Host "=========================================" -ForegroundColor $InfoColor
        Write-Host "设备已成功烧录最新固件。" -ForegroundColor $SuccessColor
        Write-Host "启动后会显示版本号：v2.3.0" -ForegroundColor $SuccessColor
    } else {
        Write-Host "" -ForegroundColor $ErrorColor
        Write-Host "? 烧录失败！" -ForegroundColor $ErrorColor
        Write-Host "错误代码：$LASTEXITCODE" -ForegroundColor $ErrorColor
    }
} catch {
    Write-Host "" -ForegroundColor $ErrorColor
    Write-Host "? 烧录过程中发生错误：" -ForegroundColor $ErrorColor
    Write-Host $_.Exception.Message -ForegroundColor $ErrorColor
}

Write-Host "" -ForegroundColor $InfoColor
Write-Host "脚本执行完成。" -ForegroundColor $InfoColor
Write-Host "=========================================" -ForegroundColor $InfoColor
