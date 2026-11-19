@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "HARDWARE_TESTER_ROOT=%CD%"
set "BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
echo NS2 Hardware Tester - local libounce build
if not exist "%BASH%" goto fail
"%BASH%" -lc "cd \"$(cygpath -u \"$HARDWARE_TESTER_ROOT\")\" && make clean all DEVKITPRO=/opt/devkitpro"
if errorlevel 1 goto fail
if not exist "dist\Hardware-Tester.nro" goto fail
echo.
echo SUCCESS: Hardware Tester\dist\Hardware-Tester.nro
echo Copy to /switch/Hardware-Tester.nro on the Switch 2 SD card.
pause
exit /b 0
:fail
echo.
echo HARDWARE TESTER BUILD FAILED - review the compiler error above.
pause
exit /b 1
