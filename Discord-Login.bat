@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo DiscordNX2 - Official Discord OAuth2 Sign-In
echo Uses localhost browser authorization; no password or secret requested.
echo.
where py >nul 2>nul
if not errorlevel 1 (
    py -3 "tools\discord_login.py"
    goto done
)
where python >nul 2>nul
if errorlevel 1 (
    echo Python 3 not found. Install Python 3 first.
    goto done
)
python "tools\discord_login.py"
:done
echo.
pause
