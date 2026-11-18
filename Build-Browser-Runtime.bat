@echo off
setlocal
cd /d "%~dp0"
echo Optional Chromium runtime test - not a browser or CAPTCHA fix
echo Requires Debian WSL with LLVM, kernel build tools, and devkitPro with libounce.
py -3 browser-port\build-runtime.py
if errorlevel 1 goto failed
echo Built dist\browser-runtime\DiscordNX2-Runtime-Test.nro
echo Keep DiscordNX2.nro and the resident NSO installed. This is a separate test.
pause
exit /b 0
:failed
echo Browser runtime build failed. DiscordNX2.nro was not changed.
pause
exit /b 1
