# DiscordNX2

I want Discord voice chat on my Switch 2 while playing games, with the C button as push-to-talk. This is the first hardware test, **not a working Discord voice client**.

**CURRENT TESTING INSTRUCTIONS (v0.15): Build-NRO.bat only; transfer ONLY the NRO. Do not transfer or replace the NSO or restart kaiHEN. The initial setup steps further below are historical.**

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

## v0.10 — Reference tone versus microphone playback

A 0.5-second quiet synthetic tone now plays before the familiar recorded voice. Listen for any clicks during the tone and compare with the speech playback. If the tone is clean but voice cracks, microphone capture/processing is a more likely cause; if the tone also pops, output buffering/device behavior is suspect. Pull, run `Build-NRO.bat` and copy ONLY the NRO. Retrieve `/kaihen/discordnx2-hardware.txt`; do not replace NSO or reboot. UDP test initialization returned `00001759` in the last report and remains an open networking issue.

## v0.11 — Unity-gain playback comparison

The synthetic reference tone sounded clean, but recorded speech still popped at v0.10. This test plays the recorded voice at its native PCM level (gain 1.0x, **no amplification and no clipping limiter**) with the same 10ms fade at both ends. It retains a quiet reference tone and the original three-second recording. Speak naturally; recorded speech will sound quieter than v0.8/v0.9. Pull, run `Build-NRO.bat`, and FTP ONLY `dist/DiscordNX2.nro`, leaving the existing resident NSO untouched. Compare how often the voice pops, and send `/kaihen/discordnx2-hardware.txt`. No Discord call or game-audio mixing yet.

## v0.12 — Playback stop transition comparison

Generated reference tone now receives an extra 250ms silence tail, as does microphone speech. Compare whether the tone-end click and speech popping improve. Build with `Build-NRO.bat` and FTP ONLY `dist/DiscordNX2.nro`; do not touch the NSO or restart kaiHEN. Send `/kaihen/discordnx2-hardware.txt`. This is a foreground-only diagnostic test.

## v0.13 — KaiPlayer-inspired audio streaming buffer comparison

Based on the MIT-licensed [KaiPlayer](https://github.com/mizuiroame/kaiplayer) output scheduling model, the v0.13 foreground NRO feeds three rotating aligned 8 KiB buffers (2,048 stereo frames each) and reuses a buffer only after the audio service reports it is no longer queued. Both the 400 Hz reference tone and the three-second recorded speech now use this path, followed by 256 ms of silent PCM. It logs the queue/release counts, wait errors, errors and flush status. This is a targeted test of buffer scheduling, not a claim that the popping is fixed. Pull then run `Build-NRO.bat`; FTP **only** `dist/DiscordNX2.nro` and leave the running NSO alone. Report any compile errors and whether the end-tone pop or recorded-speech pops change; retrieve `/kaihen/discordnx2-hardware.txt`. The separate [switch2iptv](https://github.com/davidkalil10/switch2iptv) project uses libmpv rather than exposing its own AudioOutBuffer scheduling, so it served as a secondary architectural reference, not code copied into this project.

## v0.14 — Prequeued microphone input buffers

v0.13 output buffering did not improve popping. The microphone capture path previously submitted one buffer, waited, computed statistics, wrote diagnostics to SD, then submitted another. v0.14 prequeues three input buffers and requeues completed slots before performing costly analysis, deferring sample statistics and SD writes until capture finishes. The generated tone and voice still use the v0.13 streaming output, with unity gain. Pull and run `Build-NRO.bat`, FTP **ONLY** `dist/DiscordNX2.nro`; do not touch the installed NSO. Compare crackling and send `/kaihen/discordnx2-hardware.txt`. No Discord connection or background microphone yet.

## v0.15 — Voice-packet pipeline and socket-driver milestone

The v0.14 three-buffer capture fixed audible popping on real Switch 2 hardware; this release **preserves that capture design**. Added portable `app/source/voice_packet.c/.h`: 10ms 48kHz stereo-to-mono PCM16 frames (480 samples), 984-byte versioned packet framing, monotonically increasing sequence/timestamps, RMS/peak meters, threshold + 120ms voice-activity hangover, and payload corruption detection. The NRO checks PCM pack/unpack bit-exactly against its captured samples, reports frame counts and VAD results, and tests two UDP messages **only on 127.0.0.1**: a marker and one diagnostic voice packet. Network initialization now calls libounce's documented `socketInitialize` wrapper (which negotiates the BSD version) instead of the prior invalid zero-version raw BSD call; it tries a single-session configuration if the default fails and records both results. This is not Opus, Discord RTP, DAVE, authenticated voice chat, or a microphone transmission to a remote host. No Nintendo services are unblocked. Build locally with `Build-NRO.bat`; transfer only `dist/DiscordNX2.nro` and keep the resident NSO unchanged. Examine `/kaihen/discordnx2-hardware.txt` for packets, corruption check, and UDP result.

### Extended v0.15 capture stability run

After the normal tone, recorded voice and packet checks, v0.15 also runs a separate **~15-second microphone-only stress test** with the same prequeued three-buffer approach. It discards the PCM rather than playing it back or sending it anywhere, and logs buffer counts/errors only after the capture stops. This is intended to expose long-duration microphone dropouts without requiring you to remain on the short three-second test. The background NSO remains unchanged. The console will stay in the NRO longer by design.

## Official Discord OAuth sign-in companion

Use `Discord-Login.bat` on your PC for a real Discord browser login with PKCE and `identify guilds` permissions. See [tools/DISCORD_LOGIN.md](tools/DISCORD_LOGIN.md). No client secret, password or token is entered or saved. This does **not** connect the Switch 2 to a Discord voice channel. No NRO/NSO changes or jailbreak restart are needed for this test.

## DiscordNX2 v0.17 — Native interactive mega-test

v0.17 boots to a controller-operated Switch 2 text UI rather than immediately running diagnostics. Press **A** for the complete suite: audio, recorded speech playback, packet encode/decode and corruption, ~15s sustained microphone capture, localhost UDP and a **real public Discord Gateway TLS/WebSocket upgrade/HELLO handshake without signing in**. **X** runs only Discord Gateway; **Y** runs microphone audio and packets; **L** runs the long microphone stress test; **+** exits. All results write to `/kaihen/discordnx2-hardware.txt`. Discord credentials and account tokens are never requested or transmitted, and there is no Discord voice channel join yet. This build does not replace the resident NSO. GitHub Desktop pull → `Build-NRO.bat` → FTP **only** `dist/DiscordNX2.nro`. If compiler/toolchain errors occur, send their output; a new TLS path has not been verified on actual Switch 2 hardware.

## v0.18 — Native graphical interface

The Switch 2 NRO now starts with a **real RGBA8 framebuffer-rendered interface** with a dark theme, colored status tiles, a navigation sidebar, controller focus highlighting, and Dashboard / Servers / Voice Chat / Diagnostics sections. Use the D-pad left/right to change sections, up/down to select entries, **A** to activate, **B** to return to the dashboard, and **+** to exit. Existing **X** (Discord Gateway), **Y** (microphone/packet test), and **L** (microphone stress) shortcuts remain supported. The framebuffer closes before the hardware test begins and reopens afterward. A console fallback remains available if Switch 2 graphics initialization fails.

**Account sign-in, real guild lists, and joining voice are *not* implemented in this revision.** The new GUI explicitly displays disconnected/not signed in, rather than fake server data. The code has no configured DiscordNX2 developer-owned OAuth application or callback service, and standard OAuth identify/guilds cannot join voice on behalf of a user. TriCord's unofficial user-token authentication also introduces Discord ToS/account risks and cannot safely be represented as approved account login. Do not input or share Discord credentials or tokens.

Build exactly as before: GitHub Desktop pull, run `Build-NRO.bat`, FTP only `dist/DiscordNX2.nro`, no NSO replacement and no kaiHEN restart. Existing validated Discord Gateway and local audio/network tests remain accessible.

### v0.18 navigation correction (November 17, 2026)

The UI now uses **two-column focus** rather than left/right cycling tabs. Start focused on the left sidebar: Up/Down chooses Dashboard, Servers, Voice Chat, or Diagnostics, and A or Right enters the chosen content when actionable. Left or B returns from content to sidebar; Right on an unimplemented Server/Voice screen does not invent a selectable control. Up/Down in content moves between actionable cards, A launches the highlighted test. The left analog stick uses threshold-crossing events to avoid rapid auto-scroll, matching D-pad directional behavior. X/Y/L diagnostic shortcuts and + exit remain active. A separate focus highlight distinguishes the sidebar from its content. Navigation-only hardware verification does not require another audio test. **No sign-in, server loading, or voice calls are enabled by this correction.**


## v0.19 — Live Discord account metadata LAN pairing experiment

This is a **real profile-and-server-navigation test**, not simulated server data and **not Discord voice**. It uses the preexisting OAuth PKCE PC helper to authorize with Discord in a PC browser, retrieves the authenticated account's identity and server names through the Discord API, and delivers only that sanitized read-only metadata over a **trusted local network** to the graphical Switch 2 NRO after six-digit code approval.

**One-time developer setup:** The DiscordNX2 project owner creates **one** Discord OAuth application (Public Client) at https://discord.com/developers/applications and registers exactly \`http://127.0.0.1:8765/discordnx2/callback\` for OAuth2. Copy its public Application ID. Ordinary users do **not** create developer applications. This is a short-lived PC prototype; the final goal is Switch-only authentication with no PC.

**Test sequence (one integrated account test):**
1. Update the repo in GitHub Desktop. Run \`Build-NRO.bat\` and FTP only \`dist/DiscordNX2.nro\`. Do not touch the kaiHEN NSO.
2. On the **same trusted LAN**, double-click \`Discord-Pair.bat\` on Windows. Paste the one shared DiscordNX2 Public Client ID. Your PC browser opens Discord's actual OAuth \`identify guilds\` consent flow.
3. Once authorized, leave the PC window open. Allow **Private network only** in Windows Firewall if prompted. PC emits a harmless UDP discovery beacon on port 39274, and listens for pair requests on TCP port 39275.
4. On Switch 2, navigate to **Servers**, press Right/A to focus content, then **A on PAIR WITH PC**. A secure random six-digit code appears on the screen.
5. The PC displays the request source IP and six-digit code. Type exactly \`YES\` if they match. The Switch then loads the **actual OAuth account display name and up to 64 guild names/IDs**, in RAM only.
6. Use Up/Down to navigate the real server list and A to select one; the Voice Chat view shows the selected server but remains disconnected.

**Security:** The PC never transfers its Discord OAuth access token, refresh token, password, email, or client secret to the Switch. The PC does not write them to disk. It shares only **metadata** (account display name, username, server names, server IDs) over HTTP on your LAN, after explicit code matching and approval. **Only run on a trusted private LAN**: the metadata is not encrypted on the LAN. Pairing exits after the first approved transfer. This is not authentication to Discord on the Switch and does not authorize voice calls. Never expose TCP 39275 to the Internet or forward ports.

**Limitations:** Discord's \`identify guilds\` OAuth scopes do not authorize voice, do not list voice-channel membership or grant remote user Gateway access. QR login is *not* available in this build. This is a temporary, optional companion for verifying real account metadata and graphical server navigation, **not a replacement for a fully native voice client**. A new login backend or Discord-approved voice access is still necessary before two-way voice testing. The PC must have Python 3 and must be on the same LAN as the Switch. Report pairing errors without sharing authorization URLs, callback codes, access tokens or secrets.

## v0.20 native QR-auth groundwork — currently no account login

The NRO now directly tests DNS/TCP/TLS/WebSocket and the server HELLO at Discord's **remote-auth Gateway** (`remote-auth-gateway.discord.gg/?v=2`) using the proven libounce TLS connection path. Navigate Servers > Test Remote Auth Connection, or Diagnostics > Remote Auth Gateway, then inspect `/kaihen/discordnx2-hardware.txt`. **This only tests connectivity**; it does not send an RSA public key, obtain a fingerprint, generate a login QR, accept tokens, sign in, or navigate real guilds. It retains the existing media diagnostics and resident NSO unchanged.

The MIT-licensed Project Nayuki QR library is now vendored and linked in `app/source/vendor/qrcodegen.[ch]`; the graphical UI has a native QR renderer for **future validated Discord authorization URLs**. The scanner is deliberately **not activated** until a secure identity binding can generate a genuine session-specific URL. V0.19 PC-pairing source is left in the repo as historical reference but **not linked into this build**; the new UI no longer asks for a Windows companion.

**Critical security note:** Discord remote-auth is an unofficial user-token protocol. Implementing its RSA private-key and nonce proof exchanges, verified fingerprint binding, account-token lifetime controls, and real Discord API session securely is still required. The official console device OAuth flow requires an application registration and in the manual flow a backend for client secrets. Do not scan a generic activate URL as proof of successful Switch login. No QR sign-in or voice-call functionality is claimed by v0.20.

### v0.21 Remote Auth HTTP handshake troubleshooting

Real Switch 2 tests showed HTTP 403 from remote-auth WebSocket despite functional TLS. v0.21 adds a standard `Origin: https://discord.com` header consistent with the TriCord WebSocket client and logs a small **allowlist of non-secret HTTP response metadata** on rejection to distinguish protocol rejection from edge protection. Run the Servers remote-auth test once and inspect the same hardware log. This does **not** implement QR login or account authentication.

## v0.22 - Native QR login experiment and font upgrade

This is the first *experimental* scan-to-sign-in attempt directly on Switch 2. It implements Discord's undocumented remote-auth v2 user protocol, which may violate Discord's Terms of Service and may trigger restrictions/CAPTCHA. Use a separate test account. No Windows companion application or developer app is needed for the native QR attempt.

1. Pull main with GitHub Desktop. Run Build-NRO.bat. If missing, the batch automatically attempts to install devkitPro's maintained switch-mbedtls crypto package via pacman, changing the local devkitPro package installation. An unsuccessful install prevents a secure QR build.
2. Transfer ONLY dist/DiscordNX2.nro. Do NOT change /kaihen/plugins/discordnx2.nso or restart kaiHEN.
3. Open DiscordNX2 > Servers > SIGN IN WITH QR CODE. The console generates RSA-2048 in RAM, connects to Discord's remote-auth v2 WebSocket with the Origin header, decrypts the nonce challenge, verifies the server fingerprint, and displays a genuine session-bound QR. It never displays a static sample login code.
4. Use Discord's own mobile QR scanner to scan and approve the login from your own Switch 2. B or Plus cancels. Discord may reject unofficial clients or require extra verification.
5. If Discord returns a login ticket and token, DiscordNX2 fetches the real account name and up to 64 Discord server names from Discord's API, then immediately wipes its token buffers. Guild metadata remains in NRO RAM until logout or exit. No token, private key, ticket, or QR fingerprint is written to SD.
6. Select a server using the graphical list. Voice, channels, messages, Opus/DAVE, and background microphone are still NOT implemented in v0.22.

The sanitized /kaihen/discordnx2-hardware.txt report logs only fixed authentication stage descriptions, success state, and visible server count. Share this file on failure, but do NOT share raw tokens, tickets, QR URLs or phone confirmation codes.

Font: DiscordNX2 first tries the console's built-in shared TrueType font via pl:u and stb_truetype, with antialiased glyph rendering. It distributes no proprietary font file. If the Switch 2 shared-font service is incompatible, it falls back to the bundled public-domain IBM VGA 8x8 glyph font, replacing the old 5x7 lettering.

Build validation: v0.21 Remote Auth Gateway connection and v0.18 UI were real-hardware verified; v0.22 source changes have NOT been compiled on the user's devkitPro or tested on their Switch. The first build/login may reveal toolchain or Discord-protocol issues. This source is NOT a guarantee of successful sign-in. TLS peer and hostname validation are required. There is no claim of working voice chat in this revision.

### v0.22 first compiler fix (Switch 2)

The first user build installed switch-mbedtls successfully but failed to compile native_login.c due to a leftover 3DS osGetTime call. The source now uses the standard monotonic millisecond clock. Pull main and rerun Build-NRO.bat; package installation should be skipped because switch-mbedtls is already present. Compilation, linking, and actual sign-in remain to be verified.

### v0.22 native QR nonce fix investigation

Actual Switch 2 test reached `GENERATING SECURE RSA-2048 SESSION`, `CONNECTING TO DISCORD QR LOGIN`, then `FAILED TO VERIFY ENCRYPTED NONCE`; no QR generated. A follow-up revision adds a local RSA-OAEP round-trip self-test and static stage-specific nonce failure labels, so the next attempt distinguishes malformed challenge, base64 decode, ciphertext size, RSA private decrypt, and proof encoding. No secret values are logged. Pull main, rebuild NRO only, and share the stage-only report if it still fails. This does NOT yet claim working QR sign-in.


### v0.23 native QR and appearance update

Pull main with GitHub Desktop, run `Build-NRO.bat`, then FTP **only `dist/DiscordNX2.nro`**. Keep the installed resident NSO. This update fixes nonce-proof serialization capacity, adds Base64URL boundary selftests, packages your numeral-2 icon, and rounds the native menus. No PC companion or raw token is needed.

Local NRO compilation and packet/nonce host tests passed. Open Servers > Sign in with QR code, scan with Discord mobile and approve using a test account. Confirm the real profile/server list. Live login and console appearance require hardware verification. Authentication remains unofficial; Discord voice is not implemented. If login fails, share only the stage-only hardware log, never QR URLs or credentials.


### v0.24 appearance trial

The NRO now embeds the supplied Pretendo font and updated icon without the numeral. Missing characters use bitmap fallback. Verification badges and development overlays are removed; controls and Diagnostics remain available. Pull main, run Build-NRO.bat and FTP only the NRO. Mobile approval has been reported but console account loading is unresolved; the stage-only log now identifies session-exchange HTTP, token, profile and guild failures.


### v0.25 phone approval follow-through

Phone approval alone does not mean the console has obtained an account session. The v0.24 hardware log showed HTTP400 at ticket redemption. The v0.25 request uses API v10 and explicit Discord origin/referer, and safely classifies rejected responses. Live account loading still requires console testing. A CAPTCHA requirement stops login; it is not bypassed. Share only the stage-only hardware report if login fails.

Host regression test: run `sh tests/run-native-http-tests.sh` with Python and a host C compiler available. The test uses simulated TLS responses and no real account credentials.


### v0.26 Discord-only menus

Dashboard contains login/logout. Servers lists actual cached guilds after successful login. Selecting a server opens Voice chat, which shows that server and honest disconnected channel/member states; live channel membership and Discord voice are not yet connected. Diagnostics and connectivity tests are removed from the active UI.

Text is smaller and mixed case using the console shared font, with Pretendo and bitmap fallbacks. QR labels use full-width/padded bounds. Login progress is flushed to the stage-only hardware log before each wait, and TLS/WebSocket waits are bounded and cancelable. Pull main, Build-NRO.bat and FTP only the NRO. Live login still requires hardware verification.
