@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ========================================
echo DiscordNX2 - Local Switch 2 build
echo ========================================
where docker >nul 2>&1
if errorlevel 1 goto nodocker
where git >nul 2>&1
if errorlevel 1 goto nogit
docker info >nul 2>&1
if errorlevel 1 goto noengine
if not exist ".local\ounce-devkit\Dockerfile" (
  echo Fetching local toolchain source...
  git clone --depth 1 https://github.com/ouncebrew/ounce-devkit.git ".local\ounce-devkit"
  if errorlevel 1 goto fail
)
echo Building toolchain image locally...
docker build -t discordnx2-devkit:local ".local\ounce-devkit"
if errorlevel 1 goto fail
echo Building app and plugin locally...
docker run --rm -v "%cd%:/project" -w /project discordnx2-devkit:local make
if errorlevel 1 goto fail
if not exist "dist\DiscordNX2.nro" goto fail
if not exist "dist\discordnx2.nso" goto fail
echo.
echo Build complete: dist\DiscordNX2.nro and dist\discordnx2.nso
pause
exit /b 0
:nodocker
echo Docker is not installed.
goto fail
:nogit
echo Git is not installed.
goto fail
:noengine
echo Docker Desktop is not running with Linux containers.
goto fail
:fail
echo Build failed. See the error above.
pause
exit /b 1
