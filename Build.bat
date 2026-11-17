@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "DISCORDNX2_ROOT=%CD%"
echo ========================================
echo DiscordNX2 - Local Switch 2 build
echo ========================================
echo.
echo No Docker or GitHub Actions needed.
set "BASH="
if exist "C:\devkitPro\msys2\usr\bin\bash.exe" set "BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
if not defined BASH if exist "C:\devkitPro\msys2\mingw64\bin\bash.exe" set "BASH=C:\devkitPro\msys2\mingw64\bin\bash.exe"
if not defined BASH (
  echo Couldn't find devkitPro's MSYS2 bash.
  echo Install the Windows devkitPro toolchain first:
  echo https://devkitpro.org/wiki/Getting_Started
  echo Then install ounce-devkit following:
  echo https://github.com/ouncebrew/ounce-devkit
  goto fail
)
"%BASH%" -lc "cd \"$(cygpath -u \"$DISCORDNX2_ROOT\")\" && ./build-local.sh"
if errorlevel 1 goto fail
if not exist "dist\DiscordNX2.nro" goto fail
if not exist "dist\discordnx2.nso" goto fail
echo.
echo BUILD COMPLETE
echo dist\DiscordNX2.nro
echo dist\discordnx2.nso
pause
exit /b 0
:fail
echo.
echo BUILD FAILED. Check the message above.
pause
exit /b 1
