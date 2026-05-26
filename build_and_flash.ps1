param(
    [string]$Port = "COM5",
    [switch]$Clean,
    [switch]$NoVerbose,
    [switch]$LogToFile,
    [string]$Fqbn = "esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,PSRAM=disabled,LoopCore=1,EventsCore=1,EraseFlash=none,JTAGAdapter=default,ZigbeeMode=default"
)

$ErrorActionPreference = "Stop"

$arduinoCLI = "C:\Users\liuyong\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$projectPath = $PSScriptRoot
$configFile = Join-Path $projectPath "arduino-cli.yaml"

function Set-Utf8Console {
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [Console]::InputEncoding = $utf8
    [Console]::OutputEncoding = $utf8
    $script:OutputEncoding = $utf8
    cmd /c chcp 65001 > $null
}

function Assert-PathExists {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path $Path)) {
        throw "$Label not found: $Path"
    }
}

Set-Utf8Console

Assert-PathExists -Path $arduinoCLI -Label "Arduino CLI"

$availablePorts = [System.IO.Ports.SerialPort]::GetPortNames()
if (-not $availablePorts.Contains($Port)) {
    throw "Serial port $Port is not available. Current ports: $($availablePorts -join ', ')"
}

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "ESP32 build and flash" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "Project: $projectPath"
Write-Host "FQBN: $Fqbn"
Write-Host "Port: $Port"
Write-Host "Build path: arduino-cli temp build path"

Write-Host ""
Write-Host "Running compile plus upload plus verify..." -ForegroundColor Cyan

$compileArgs = @(
    "compile"
    "--fqbn", $Fqbn
    "--port", $Port
    "--upload"
    "--verify"
    "--export-binaries"
)

if (Test-Path $configFile) {
    $compileArgs += @("--config-file", $configFile)
}

if ($Clean) {
    $compileArgs += "--clean"
}

if (-not $NoVerbose) {
    $compileArgs += "--verbose"
}

if ($LogToFile) {
    $compileArgs += @("--log-file", (Join-Path $projectPath "build\esp32.esp32.esp32s3\compile-and-flash.log"))
}

$compileArgs += $projectPath

& $arduinoCLI @compileArgs
$exitCode = $LASTEXITCODE

if ($exitCode -ne 0) {
    throw "Compile or upload failed with exit code $exitCode"
}

Write-Host ""
Write-Host "Flash completed successfully." -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Cyan