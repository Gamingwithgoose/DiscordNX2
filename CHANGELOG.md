# Changelog

## November 17, 2026 — Foreground NRO-only hardware survey

After the real Switch 2 tests confirmed plugin persistence, I added a foreground survey to the NRO without modifying the installed NSO. It samples controller raw button masks and transitions for about twelve seconds, and checks audio-input initialization. The libounce HID header has no named C-button bit, so this survey does not claim to detect C specifically. It does not capture microphone audio.

Added Build-NRO.bat and an app-only make target to avoid compiling or transferring the plugin. No remote build was used. This change has not been locally compiled or tested on hardware yet.

## November 17, 2026 — Larger diagnostic plugin

Expanded the background NSO before the next hardware test so I can check more than a single handshake after going through the exploit again. It now reports its version and in-memory counters, remembers the last service probe, and can try opening the HID, audio input, audio output and network service sessions when the NRO asks it to. The sessions are immediately closed; nothing records microphone audio or reads controller input yet.

The NRO asks for all these results and writes them to the existing diagnostic file. This should help determine whether the plugin stayed alive across NRO launches and whether its process can even reach the relevant services. It still needs compilation and real hardware testing. Discord calling, background microphone capture, and C-button PTT are not implemented.

## November 17, 2026 — Easier toolchain setup

The build reached devkitPro's MSYS2 shell but couldn't find the Switch 2 toolchain. I added Setup-Toolchain.bat so the first-time setup can be done from the repo, with a confirmation before it changes the devkitPro package configuration. It uses ounce-devkit's official installer rather than trying to invent a separate toolchain setup.

## November 17, 2026 — Local build adjustment

I originally used Docker for the first build script because it was an easy way to bundle the development tools. That was overkill for this project. Build.bat now uses the devkitPro MSYS2 installation directly. You still have to install ounce-devkit's Switch 2 cross-compiler once, but after that, builds happen entirely on your own PC without Docker.

## November 17, 2026 — First hardware test

Started the project with two pieces: a Switch 2 NRO for a basic diagnostic check and a kaiHEN NSO that runs as a separate plugin. Added Build.bat so I can pull the source, compile locally, and copy the files to my console. Nothing uses GitHub Actions.

The NRO currently asks the plugin for a test reply and writes the result to the SD card. That's all it does so far. I want a real hardware result before I spend time on Discord voice, C-button handling, or audio. The first build still needs testing on the Switch 2.
