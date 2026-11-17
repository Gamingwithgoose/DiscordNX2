@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "DISCORDNX2_ROOT=%CD%"
echo DiscordNX2 NRO-only local build
set "BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
if not exist "%BASH%" goto fail
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
