@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "BROWSER_RESEARCH_ROOT=%CD%"
set "BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
echo Building standalone Browser Research - no Discord app or plugin changes
if not exist "%BASH%" goto fail
"%BASH%" -lc "cd \"$(cygpath -u \"$BROWSER_RESEARCH_ROOT\")\" && make DEVKITPRO=/c/devkitPro"
if errorlevel 1 goto fail
if not exist "dist\Browser-Research.nro" goto fail
echo Built Browser Research\dist\Browser-Research.nro
echo Transfer this NRO to /switch/. No NSO or firmware installation.
pause
exit /b 0
:fail
echo Browser Research build failed.
pause
exit /b 1
