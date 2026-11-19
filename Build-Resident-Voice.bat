@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "DISCORDNX2_ROOT=%CD%"
echo DiscordNX2 optional background voice plugin build
set "BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
if not exist "%BASH%" goto fail
echo Checking secure QR-login cryptography dependency...
if not exist "C:\devkitPro\portlibs\switch\lib\libmbedcrypto.a" if not exist "C:\devkitPro\portlibs\switch2\lib\libmbedcrypto.a" (
    echo Installing maintained devkitPro switch-mbedtls package for QR authentication.
    "%BASH%" -lc "pacman -S --needed --noconfirm switch-mbedtls"
    if errorlevel 1 goto fail
)
"%BASH%" -lc "cd \"$(cygpath -u \"$DISCORDNX2_ROOT\")\" && ./build-local.sh resident"
if errorlevel 1 goto fail
if not exist "dist\resident-voice\discordnx2.nso" goto fail
echo Optional plugin built: dist\resident-voice\discordnx2.nso
echo Back up the existing /kaihen/plugins/discordnx2.nso before replacing it.
echo Use the next normal public jailbreak startup to activate it. No hot reload.
echo Also update your NRO with Build-NRO.bat. Background game audio needs testing.
pause
exit /b 0
:fail
echo RESIDENT VOICE BUILD FAILED
pause
exit /b 1
