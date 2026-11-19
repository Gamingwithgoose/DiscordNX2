@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "BROWSER_STARTUP_ROOT=%CD%"
set "BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
if not exist "%BASH%" goto fail
"%BASH%" -lc "cd \"$(cygpath -u \"$BROWSER_STARTUP_ROOT\")\" && make DEVKITPRO=/c/devkitPro startup-check"
if errorlevel 1 goto fail
if not exist "dist\Browser-Startup-Check.nro" goto fail
echo Built dist\Browser-Startup-Check.nro
echo Transfer only this NRO to /switch/. See STARTUP-CHECK.md.
pause
exit /b 0
:fail
echo Browser startup check build failed.
pause
exit /b 1
