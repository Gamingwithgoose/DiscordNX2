@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ========================================
echo DiscordNX2 - One-time Switch 2 setup
echo ========================================
echo.
echo This will run the ounce-devkit installer locally.
echo It may add a package signing key and repository to devkitPro.
echo No Docker or GitHub Actions will be used.
echo.
choice /C YN /M "Continue with local toolchain installation"
if errorlevel 2 exit /b 0
set "DISCORDNX2_ROOT=%CD%"
set "BASH="
if exist "C:\devkitPro\msys2\usr\bin\bash.exe" set "BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
if not defined BASH if exist "C:\devkitPro\msys2\mingw64\bin\bash.exe" set "BASH=C:\devkitPro\msys2\mingw64\bin\bash.exe"
if not defined BASH (
 echo Could not locate devkitPro MSYS2.
 echo Install devkitPro first: https://devkitpro.org/wiki/Getting_Started
 goto fail
)
"%BASH%" -lc "cd \"$(cygpath -u \"$DISCORDNX2_ROOT\")\" && sh ./setup-toolchain.sh"
if errorlevel 1 goto fail
echo.
echo Setup complete. You can run Build.bat now.
pause
exit /b 0
:fail
echo.
echo Setup failed. Check the first error above.
pause
exit /b 1
