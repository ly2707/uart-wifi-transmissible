# Arduino CLI build script
param(
    [switch]$Clean,
    [switch]$NoVerbose,
    [switch]$LogToFile,
    [string]$BuildPath,
    [string]$Fqbn = "esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,PSRAM=disabled,LoopCore=1,EventsCore=1,EraseFlash=none,JTAGAdapter=default,ZigbeeMode=default"
)

$ErrorActionPreference = "Stop"

$arduinoCLI = "C:\Users\liuyong\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$sketchPath = $PSScriptRoot
$configFile = Join-Path $sketchPath "arduino-cli.yaml"
$defaultBuildPath = Join-Path $sketchPath "build\esp32.esp32.esp32s3"

if ([string]::IsNullOrWhiteSpace($BuildPath)) {
    $buildPath = $null
    $logPath = Join-Path $defaultBuildPath "compile.log"
} else {
    $buildPath = $BuildPath
    $logPath = Join-Path $buildPath "compile.log"
}

function Set-Utf8Console {
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [Console]::InputEncoding = $utf8
    [Console]::OutputEncoding = $utf8
    $script:OutputEncoding = $utf8
    cmd /c chcp 65001 > $null
}

if (-not (Test-Path $arduinoCLI)) {
    throw "Arduino CLI not found: $arduinoCLI"
}

if ($buildPath -and -not (Test-Path $buildPath)) {
    New-Item -ItemType Directory -Path $buildPath -Force | Out-Null
}

Set-Utf8Console

$compileArgs = @(
    "compile"
    "--fqbn", $Fqbn
)

if ($buildPath) {
    $compileArgs += @("--build-path", $buildPath)
}

if (Test-Path $configFile) {
    $compileArgs += @("--config-file", $configFile)
}

if ($Clean) {
    $compileArgs += "--clean"
}

if (-not $NoVerbose) {
    $compileArgs += "--verbose"
}

$compileArgs += $sketchPath

if (Test-Path $logPath) {
    Remove-Item $logPath -Force
}

$modeLabel = if ($Clean) { "clean" } else { "incremental" }
$verboseLabel = if ($NoVerbose) { "compact" } else { "verbose" }
$configLabel = if (Test-Path $configFile) { $configFile } else { "not used" }
$logLabel = if ($LogToFile) { $logPath } else { "disabled" }
$buildPathLabel = if ($buildPath) { $buildPath } else { "arduino-cli temp build path" }

Write-Host "Sketch: $sketchPath"
Write-Host "FQBN: $Fqbn"
Write-Host "Build path: $buildPathLabel"
Write-Host "Config file: $configLabel"
Write-Host "Build mode: $modeLabel"
Write-Host "Log level: $verboseLabel"
Write-Host "Log file: $logLabel"

$startTime = Get-Date
if ($LogToFile) {
    if (Test-Path $logPath) {
        Remove-Item $logPath -Force
    }

    & $arduinoCLI @compileArgs 2>&1 | Tee-Object -FilePath $logPath -Append
} else {
    & $arduinoCLI @compileArgs
}
$exitCode = $LASTEXITCODE
$elapsed = (Get-Date) - $startTime

if ($exitCode -eq 0) {
    Write-Host "Build succeeded. Elapsed: $($elapsed.ToString('hh\:mm\:ss'))" -ForegroundColor Green
} else {
    Write-Host "Build failed. Elapsed: $($elapsed.ToString('hh\:mm\:ss'))" -ForegroundColor Red
    exit $exitCode
}
