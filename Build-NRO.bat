@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "DISCORDNX2_ROOT=%CD%"
echo DiscordNX2 NRO-only local build
echo Includes direct phone CAPTCHA handoff. Phone and Switch must use the same Wi-Fi.
set "BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
if not exist "%BASH%" goto fail
echo Checking secure QR-login cryptography dependency...
if not exist "C:\devkitPro\portlibs\switch\lib\libmbedcrypto.a" if not exist "C:\devkitPro\portlibs\switch2\lib\libmbedcrypto.a" (
    echo Installing maintained devkitPro switch-mbedtls package for QR authentication.
    "%BASH%" -lc "pacman -S --needed --noconfirm switch-mbedtls"
    if errorlevel 1 goto fail
)
"%BASH%" -lc "cd \"$(cygpath -u \"$DISCORDNX2_ROOT\")\" && ./build-local.sh app"
if errorlevel 1 goto fail
if not exist "dist\DiscordNX2.nro" goto fail
echo NRO built. Transfer ONLY dist\DiscordNX2.nro. Do not touch NSO.
pause
exit /b 0
:fail
echo NRO BUILD FAILED
pause
exit /b 1
