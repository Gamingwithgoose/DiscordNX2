# Changelog

## November 17, 2026 — v0.9 reduced popping and diagnostic audio processing

Real-hardware testing confirmed v0.8 playback is substantially louder, but audible cracking/pops remain. Removed per-buffer hard sample clipping in favor of retaining captured PCM and applying one whole-recording gain factor (up to 1.5x, reduced when needed to stay near a 24000 peak). Added 10ms fade-in/fade-out and extra peak/discontinuity diagnostics. This should reduce clipping and clicks at recording boundaries without losing the v0.8 level on quiet speech. It is not proof that all popping was caused by the prior limiter; capture continuity or device behavior may still matter. The code only changes foreground NRO playback, preserves the NSO, and must be verified in a local build and another hardware test.

## November 17, 2026 — v0.8 longer microphone test and safer gain

The first actual local playback was audible on Switch 2 hardware, but only when speaking loudly. Extended the foreground PCM recording from 12 to 36 aligned 16-KiB chunks (approximately 3.07 seconds at stereo 48 kHz, 16-bit), changed combined digital gain from 0.5x to 1.5x, and clamped resulting samples to +/-24000 before playback to reduce clipping and loud spikes. Extended the playback wait to 7.5 seconds so the longer voice clip is allowed to finish. This is software gain, not an undocumented hardware microphone sensitivity setting. No Nintendo audio mix is modified, and separate voice/game volume controls remain a future background feature. Audio is neither saved nor network-transmitted. The resident NSO remains unchanged. The code still needs local build and hardware validation.

## November 17, 2026 — v0.7 BSD compile fix

The first local v0.7 build failed because the new UDP loopback test called low-level `bsd*` APIs without including their declarations. I added the explicit libounce `switch/services/bsd.h` header. The previous unused-function warnings are not build errors. This is a source correction only; it does not touch the installed NSO and still requires a local rebuild and hardware validation.

## November 17, 2026 — v0.7 handoff and NRO safety instructions

Added a structured `HANDOFF.json` recording real hardware results, local build workflow, plugin preservation requirements, test behavior, and DAVE-aware next steps. Updated the beginning of the README to highlight that current tests are NRO-only; historical initial installation steps must not be followed when an NSO is already running. No code or background plugin changes in this follow-up commit.

## November 17, 2026 — v0.7 voice capture, delayed playback and local transport

Moved the next milestone away from C-button probing and toward the voice path. The foreground NRO now collects twelve bounded stereo PCM microphone buffers, reports per-buffer signal statistics, and attempts to play those captured samples back at half amplitude **after** input recording stops. It tests a UDP packet sent only to a local loopback socket, preserving the existing plugin continuity and service-session probes. No audio is transmitted to any network endpoint; no credentials are read. The program closes all services before exiting. The installable background NSO is not changed, and this is not a continuous open-mic Discord client yet. The local toolchain must compile this and the console must verify audible playback.

Discord's current voice calls require DAVE end-to-end encryption. An independently interoperable voice client will need a supported authentication and DAVE-capable transport implementation; a simple Opus/UDP send is not a complete Discord voice implementation.

## November 17, 2026 — v0.6 multi-stream HID C-button investigation

Instead of repeating the same pad polling, the foreground NRO now reads four published libounce HID histories (handheld, full-key, system-extended, and system) separately for the handheld and Player 1 controller IDs. It compares six timed conditions, capturing changed button bits, sampling-number freshness, and state availability for every channel. It also calls read-only controller interface/style/last-active APIs. The test checkpoints the SD report after each stage and retains microphone capture, audio output, service probes, and background NSO continuity checks. No GameChat override or raw Bluetooth/USB interception was added; compatibility with console firmware still needs a local build and hardware test. The resident NSO was not changed.

## November 17, 2026 — C-button-focused trials without NSO replacement

The previous hardware run proved microphone capture and audio-device initialization. Since C still opens GameChat instead of appearing as a standard button, I expanded the NRO-only test into timed stages: no-button baseline; hold C; release C; press C repeatedly; dismiss the GameChat selection with B; and A/B comparison. Every stage polls Player 1, handheld, and merged HID masks separately, logs transitions, and checkpoints results to the existing SD report. Audio recording-level checks, audio-output initialization, service checks, and background-plugin IPC persistence tests remain. This does not prevent GameChat from opening or establish C interception, and it does not connect to Discord. The installed NSO remains unchanged; only the NRO source has been updated. Local compilation and real-console execution are still required.

## November 17, 2026 — Comprehensive NRO diagnostic suite

Expanded the NRO-only hardware test to run several tests in a single launch: four five-second controller input windows (A, B, C, other buttons), plugin IPC and persistent counters before and after, microphone start/capture/stop with a finite wait and in-memory signal-level summary, audio-output initialization/start/stop, and direct service-session checks. The test checkpoints one cumulative report to SD several times, so if GameChat interrupts the foreground app there should still be useful diagnostics. C remains unmapped; this does not intercept or disable GameChat. No raw recorded audio is retained and there is no Discord connection.

Adjusted the app-only make target to clean just the NRO build artifacts first; the installed NSO and plugin sources are not touched. The build still happens on the user's PC and this commit has not been compiled or hardware-tested yet.

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
