@echo off

echo =========================================
echo ESP32 UART透传系统 - 编译脚本
echo =========================================
echo.

rem 打开Arduino IDE并编译项目
echo 请在Arduino IDE中打开项目并编译...
echo 编译完成后，按任意键继续...
pause >nul

echo.
echo 编译完成！固件文件已更新。
echo 现在可以运行 build_and_flash.ps1 脚本进行烧录。
echo.
echo =========================================
echo 编译脚本执行完成。
echo =========================================
pause
