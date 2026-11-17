@echo off
setlocal EnableExtensions
cd /d "%~dp0"
python tools\discord_pair.py
if errorlevel 1 echo DiscordNX2 pairing failed. Check setup or firewall.
pause
