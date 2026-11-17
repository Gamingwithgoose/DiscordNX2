# DiscordNX2

I want Discord voice chat on my Switch 2 while playing games, with the C button as push-to-talk. This is the first hardware test, **not a working Discord voice client**.

**CURRENT TESTING INSTRUCTIONS (v0.7): Build-NRO.bat only; transfer ONLY the NRO. Do not transfer or replace the NSO or restart kaiHEN. The initial setup steps further below are historical.**

Right now the project builds a homebrew NRO plus a kaiHEN background NSO. The NRO asks the background plugin for a handshake, its status and counters, and permission to open basic HID, audio-input, audio-output and network service sessions. It saves the results to the SD card. Before attempting Discord, I need to prove that a plugin can stay running while a game is open, that C can be read during gameplay, and that microphone and audio playback work at the same time as a game.

## Building on your own PC

There are no GitHub Actions, cloud builds, or Docker requirements here. This project uses devkitPro and ounce-devkit installed locally. Regular MSYS2 by itself is not the Switch 2 cross-compiler.

If you already have devkitPro installed but `Build.bat` complains that the Switch 2 toolchain is missing, double-click **Setup-Toolchain.bat** first. It asks for your permission and then runs ounce-devkit's official installer. That installer adds its package signing key/repository to devkitPro and installs the packages. It needs internet access for setup, but all actual compilation stays on your PC.

If you don't have devkitPro, install it first using https://devkitpro.org/wiki/Getting_Started. You can also install ounce-devkit manually using https://github.com/ouncebrew/ounce-devkit.

Once the tools are installed:

1. Clone this repo or pull the latest changes in GitHub Desktop.
2. Double-click `Build.bat`.
3. Find `DiscordNX2.nro` and `discordnx2.nso` in the `dist` folder.

If devkitPro lives somewhere other than `C:\devkitPro`, edit the `BASH` path near the top of the batch files. You can also build from devkitPro MSYS2 with `./build-local.sh`.

## Testing on the Switch 2

Copy `dist/DiscordNX2.nro` to your usual homebrew directory. **Also copy** `dist/discordnx2.nso` to `/kaihen/plugins/discordnx2.nso`. Restart kaiHEN so it can start the plugin, then launch the NRO from the homebrew menu.

Read `/kaihen/discordnx2-probe.txt` on the SD card. A successful handshake should show `Handshake result: 00000000` and `reply: 444e5832`. The report also includes `Plugin magic`, `ABI`, counters and service-session result codes. A zero for a service only means opening a session worked, **not** that controller monitoring, microphone recording or game-audio mixing works. If it fails, check `/kaihen/plugin-status.txt`. The NRO has no graphical interface yet; it simply writes its diagnostic report and exits.

You can remove the plugin by deleting `/kaihen/plugins/discordnx2.nso` and restarting kaiHEN. The test does not access Discord, capture audio, read controller button states, or intentionally write to NAND. It does briefly try opening HID, audio and networking service sessions, then closes them immediately. kaiHEN is experimental, so use it with care.

## What I want to tackle next

Background persistence during games comes first. C-button access, microphone capture, mixing incoming chat with game audio, and a supported Discord voice integration come later. I won't claim those features work until they've been tested on the actual console.

The build structure and IPC handshake follow the MIT-licensed examples from ouncebrew/ounce-devkit. Thanks to the kaiHEN and ouncebrew teams for their work.

## Getting several useful tests from one plugin installation

Version 0.2 of the background plugin keeps an in-memory count of requests and service probes. Every time you run the diagnostic NRO, the report gives these counts and the service-open results. The counters reset when the plugin process restarts. This makes it possible to run the NRO, leave it, play a game, then run it again and see whether you reached the **same** plugin process. It is not yet proof that audio or input will work during a game.

The app overwrites its previous report at `/kaihen/discordnx2-probe.txt` each run, so **copy that file to your PC before running the test again** if you want to compare results. The NRO has no interactive UI for this version.

For this version, copy **both the new NRO and new NSO** before your next kaiHEN startup. You can keep replacing the NRO afterward without restarting, but changes to the NSO itself still need another plugin startup. That's why this release adds several read-only diagnostic commands in one go.

The persistent service includes an ABI/version check so a newer NRO can recognize an older plugin rather than issuing unknown test commands. This does not magically hot-reload compiled code. I'm keeping that distinction clear while investigating safer ways to reduce restarts.

## NRO-only hardware survey (no new jailbreak)

Keep your working v0.2 NSO installed and running. Pull this commit, double-click **Build-NRO.bat**, FTP only `dist/DiscordNX2.nro`, then launch the NRO in hbmenu. During its roughly 12-second sampling window, press/release normal controller buttons and the C button. Read `/kaihen/discordnx2-hardware.txt` afterward, and send the report for analysis. The previous `/kaihen/discordnx2-probe.txt` diagnostic is also generated.

This only checks foreground HID raw masks and `audinInitialize`. Current libounce HID enums do not identify the Switch 2 C button, so the test cannot claim C detection or background input access. No microphone samples are recorded or transmitted. The survey has no interactive display; it simply exits after sampling. The new build has not yet been compiled or tested on hardware.

## Comprehensive foreground diagnostics (v0.4)

Keep the existing NSO installed and active. Pull and run `Build-NRO.bat` to force-rebuild ONLY the app, then FTP ONLY `dist/DiscordNX2.nro`. Run it once in hbmenu. The NRO automatically runs four controller windows, roughly five seconds each: press A in window 1, B in window 2, C in window 3, and other controls in window 4. It also captures a short audio buffer in memory to report its peak/nonzero sample counts (no recording saved), tests output open/start/stop, probes services, and reads the plugin before and after. Wait for the app to close, then send `/kaihen/discordnx2-hardware.txt`. It checkpoints the report during the test in case GameChat steals focus. The test does NOT suppress GameChat or deliver Discord voice; a GameChat popup may interrupt the test. The original `/kaihen/discordnx2-probe.txt` is still written by the older compatibility code.

This is experimental native code: it has not been compiled on your machine or confirmed on a console yet. Do NOT update the `.nso` for this release. If the report stops at a stage, send the partial file rather than restarting kaiHEN.

## v0.5 C-button observation test

**Do not replace the NSO or restart kaiHEN.** Pull the latest commit, run `Build-NRO.bat`, and transfer only `dist/DiscordNX2.nro`. Launch it in hbmenu. Its approximate input schedule is: first 3 seconds no buttons; next 15 seconds **hold C continuously**; next 5 seconds release; next 10 seconds press C repeatedly; next 8 seconds press B to dismiss GameChat; final 6 seconds press A/B to compare. It then runs the previous microphone, output, service, and plugin tests. It records raw HID for Player 1, handheld, and the merged pad and writes `/kaihen/discordnx2-hardware.txt` after every phase. The C button is not yet exposed through our known HID API and GameChat may take focus. If that happens, the checkpointed report still helps. There is **no GameChat override** and no actual Discord connection in this test.

## v0.6 — Additional HID history streams, no NSO replacement

Pull, run **Build-NRO.bat** on Windows, FTP **only** `dist/DiscordNX2.nro`, and launch it from the homebrew menu. Do not copy or rebuild the installed NSO; do not restart kaiHEN. The input test still takes approximately 47 seconds: baseline 3 seconds, hold C 15 seconds, release 5 seconds, tap C for 10 seconds, dismiss GameChat with B for 8 seconds, and A/B comparison for 6 seconds. The NRO reads the published libounce controller histories `HandheldState`, `FullKeyState`, `SystemExtState` and `SystemState` for handheld and Player 1. It also queries controller style, device type, last active controller, and interface type using read-only hidsys APIs. It logs any state change, sample freshness, and per-stage summaries. Audio capture/output probes and background plugin checks remain.

Output: `/kaihen/discordnx2-hardware.txt` (same as before). Look for the header **DiscordNX2 four-stream C-button diagnostic v0.6**; send the report after testing. The NRO does **not** read raw Bluetooth/USB controller traffic or patch GameChat, and no observed HID event is yet identified as C. These newly added streams might still expose the same filtered input; console testing determines whether they add information. If the build fails, send the actual compiler errors, without touching the resident plugin.

## v0.7 — microphone playback milestone (NRO only)

**Pull and run `Build-NRO.bat`; FTP only `dist/DiscordNX2.nro`.** Keep the existing NSO and kaiHEN session. Launch the NRO. Speak a few clear words while it captures twelve short audio buffers (~1 second total at 48 kHz stereo). It will then attempt a single delayed playback of those samples at **50% amplitude**. Use headphones or keep system volume low to avoid acoustic feedback, especially if using an external mic/speaker. It runs a local-only UDP marker send/receive test, checks background plugin continuity and service sessions, and saves `/kaihen/discordnx2-hardware.txt`. Tell us whether you could **hear intelligible speech**, not just whether the NRO exited. It stores no WAV/PCM file and transmits no microphone audio. The test is foreground-only and closes microphone, output, and networking before exit. There is no Discord authentication, open mic while gaming, or C-button handling yet. If you see compiler errors, provide the full output; no remote build was performed.

## v0.8 — Longer, louder voice loopback

Voice playback has now been heard on real Switch 2 hardware, though it required unusually loud speech. Pull and run `Build-NRO.bat`; transfer **ONLY** `dist/DiscordNX2.nro`, never the current resident NSO. The foreground test captures about three seconds of speech and plays it back after recording ends, with 1.5x combined digital amplitude versus 0.5x in v0.7, capped at +/-24000 to reduce clipping. Speak at a normal volume and keep the Switch 2 playback volume modest. The log remains `/kaihen/discordnx2-hardware.txt`. This does not yet change hardware microphone sensitivity, adjust game volume, or mix Discord call audio during gameplay. Independent voice and game volume settings are design targets for the later voice engine, not functional controls in this test.

## v0.9 — smoother speech playback

User confirmed v0.8 was louder but had audible popping/crackling. v0.9 keeps the ~3-second capture and uses a whole-recording peak-aware gain (up to 1.5x) instead of hard clipping individual samples. It adds 10ms fade-in/out and logs raw peak and likely discontinuities. Pull, run `Build-NRO.bat`, transfer ONLY `dist/DiscordNX2.nro` and do not touch NSO. After speaking normally, send `/kaihen/discordnx2-hardware.txt` and say whether pops occur during words or only at playback start/end. It still doesn't make live Discord calls or mix game volume.
