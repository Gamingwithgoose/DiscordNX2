# Changelog

## November 17, 2026 — v0.21 remote-auth handshake correction

Real hardware v0.20 proved remote-auth DNS/TCP/TLS passed but remote-auth HTTP upgrade returned 403. v0.21 adds the explicit Discord web Origin header present in TriCord's request and bounded allowlisted response metadata (server, content-type, Cloudflare trace, request ID, Retry-After) with no response bodies, cookies, credentials or tokens logged. Does not fake a QR login and does not send any user authentication. Existing microphone, general Gateway, GUI and resident NSO untouched.

## November 17, 2026 — v0.20 standalone native auth foundation

Removed v0.19 Windows PC pairing workflow from the NRO startup and Servers screen; preserved the previous implementation only for reference. Added a direct Switch 2 remote-auth WebSocket TLS+HELLO probe to Discord's remote-auth Gateway with no credentials or token handling, wired it into the graphical Servers/Diagnostics views, imported Project Nayuki MIT QR library and linked an on-device QR renderer for future legitimate session-bound URLs. No QR is presented as a working login before a verified remote-auth session exists. The actual secure RSA exchange, fingerprint validation, session authentication, server navigation and voice support remain unimplemented. Existing audio stack and installed resident NSO untouched.

## November 17, 2026 — v0.19 Discord OAuth LAN profile pairing

Added opt-in PC OAuth PKCE metadata bridge (Discord-Pair.bat, tools/discord_pair.py) with UDP discovery, explicit matching six-digit approval, and one-shot HTTP metadata delivery to Switch 2; no Discord tokens leave PC memory. Added NRO LAN pairing client, secure random challenge generation, account display name + real server list rendering and server selection on the existing graphical Servers tab, with bounded network timeout and no stored profile on SD. Integrated new module into NRO Makefile. Voice remains unavailable; Discord identify/guilds OAuth does not authorize voice and the PC bridge is temporary, local-LAN-only, and cannot replace a fully native user client. No resident NSO change.

## November 17, 2026 — v0.18 sidebar/content focus correction

Changed D-pad and left-stick navigation to two-column focus: Up/Down selects sidebar sections, Right/A enters section content when actionable, Left/B returns to sidebar, Up/Down selects actions, and A activates. Distinct focus highlights; joystick edge detection prevents fast tab cycling. No changes to existing v0.17 audio/network tests or NSO; no Discord login or voice calls added.

## November 17, 2026 — v0.18 native graphical client shell

Added a real software-rendered RGBA framebuffer interface on Switch 2 with a dark graphical theme, sidebar navigation, highlighted menu items, Dashboard/Servers/Voice/Diagnostics views, controller inputs, and retained diagnostic triggers. The code uses libounce's Switch 2 framebufferMakeLinear requirement and never pretends disconnected account/servers are real data. A console fallback is preserved for graphics init errors. All audio and Discord Gateway checks remain unchanged. No authentication, guild listing or voice call yet because no supported DiscordNX2 developer-owned authorization/voice flow is configured. No changes to resident NSO.

## November 17, 2026 — v0.17 SSL resource fix

Keep the SSL socket wrapper's returned descriptor open throughout the TLS handshake and Gateway upgrade; close it during teardown. Request a bounded SSL I/O timeout. No NSO changes.

## November 17, 2026 — v0.17 Native interactive Switch 2 mega-test

Replaced unconditional startup diagnostics with a Joy-Con/controller menu (A full suite, X Discord public Gateway, Y microphone+packets, L bounded long microphone capture, + exit), explicit completion status, retained reporting to `/kaihen/discordnx2-hardware.txt`, and a new TLS/WebSocket Gateway public HELLO probe. This probes DNS, TCP 443, libounce SSL with host validation, WebSocket HTTP Upgrade and the Gateway heartbeat interval. Never uses Discord account credentials or sends audio to Discord. Keeps successful v0.14 capture logic and the existing background NSO unchanged. Not yet cross-compiled or tested on Switch 2.

## November 17, 2026 — Discord browser OAuth2 sign-in companion

Added a Windows one-click launcher and Python standard-library OAuth2 Authorization Code + PKCE companion with random state, callback issuer validation, localhost-only redirect, token exchange, `identify guilds` user/server account lookup, and offline tests. Credentials/tokens stay in memory and are not persisted. This is genuine Discord account authorization, not Discord user voice access. No NRO/NSO modifications.

## November 17, 2026 — v0.15 sustained microphone stress capture

Added an approximately 15-second bounded, three-buffer microphone stress test after the familiar record/replay and packet validation. It follows the successful v0.14 prequeue/requeue pattern, performs no SD writes during capture and retains no PCM afterward. Reports buffer counts, failed waits/requeues, approximate total PCM duration, and peak sample. This validates more sustained microphone operation before a background Discord client is attempted, without requiring an NSO update. It is not a continuous voice call and has not been tested on hardware yet.

## November 17, 2026 — v0.15 portable tests and POSIX compatibility

Added portable host-native tests for the packet encoder/decoder, silence, nonzero voice, hangover, data corruption and 16-bit edge values; fixed the missing POSIX `unistd.h` declaration for `close()` in the socket test. The tests can be run with `sh tests/run-host-tests.sh` on a machine with `cc`. These tests do not substitute for local Switch 2 compilation or hardware verification. The installed NSO remains unchanged.

## November 17, 2026 — v0.15 portable voice packet pipeline and socket fix

Real-hardware report: v0.14 eliminated audible popping. Built around that proven three-buffer mic capture rather than changing its timing. Implemented a standalone, bounded 10ms stereo PCM16 → mono PCM16 versioned packet format with sequence/timestamp, RMS/peak telemetry, VAD hangover, checksumming and a verified decode path. Added NRO packet roundtrip/negative corruption tests over every full captured frame. Replaced the failed raw `bsdInitialize` call, which used an invalid zero-version default configuration, with libounce's `socketInitialize` wrapper and standard socket APIs. The transport test runs exclusively on localhost and sends at most one diagnostic voice frame plus a marker. This is local transport preparation, not Discord integration or an actual call; there is no Opus encoder, authentication, remote network transmission or DAVE. The background NSO is untouched. Requires local compilation and console validation.

## November 17, 2026 — v0.14 prequeued microphone capture

v0.13 rotating audio-output buffers did not improve slight popping on physical Switch 2 hardware. A more plausible issue in the source was that the microphone had only one submitted buffer at a time and ran per-buffer statistics and SD-card checkpoint writes before re-submitting the next one, potentially creating capture gaps. Changed the foreground NRO capture path to prequeue three aligned 16 KiB microphone buffers, reclaim and requeue promptly, and postpone statistics and disk writes until capture stops. Logs real between-buffer sample discontinuities after recording. Keeps v0.13 three-buffer playback, unity gain and reference tone. Does not touch the installed NSO. The change still requires local build and hardware verification; dropouts are a hypothesis, not a proven root cause.

## November 17, 2026 — v0.13 streaming output queue based on KaiPlayer

The reference tone also clicked slightly near its ending and recorded speech still popped after a silence-tail experiment. Inspected KaiPlayer (MIT) and switch2iptv; KaiPlayer uses three 8-KiB AudioOutBuffer slots, a persistent output service, and a queue/reclaim loop, whereas switch2iptv delegates its audio path to mpv. Replaced the one-large-buffer output in the foreground NRO with three rotating aligned buffers for both reference tone and microphone playback, retaining an end-of-stream silence tail and bounded diagnostics. Added flush on stop. This targets output scheduling/queue reuse rather than changing microphone gain. The implementation is adapted to our short buffered test, not a full copy of KaiPlayer's thread architecture. Nothing alters the resident NSO. Requires local compilation and physical console testing.

## November 17, 2026 — v0.12 output silence transition test

User noticed the generated tone clicks near its stop as well as popping during voice playback, making output transition a plausible contributor. Added a 250ms aligned zero-PCM buffer queued behind tone and voice audio, bounded buffer-drain logging, and a brief delay before stopping each output stream. Preserved unity gain and existing audio capture. This is a diagnostic hypothesis, not a confirmed correction. No changes to NSO or kaiHEN; local build and hardware testing required.

## November 17, 2026 — v0.11 unity-gain audio comparison

The generated reference tone was clean, but recorded speech still popped. Changed only recorded-speech playback from the adaptive amplification used in v0.9/v0.10 back to unamplified 1.0x PCM, removing the 24000 peak limiter. The 10 ms start/end fades, roughly three-second capture, reference tone, diagnostic logging and localhost network probe remain unchanged. This isolates whether the amplification and limiting contribute to popping; it does not claim to fix capture continuity. No NSO modifications. Requires local compilation and real hardware playback comparison.

## November 17, 2026 — v0.10 controlled audio-output comparison

In the v0.9 real hardware report, all 36 PCM capture buffers succeeded and audio output released its buffer, but the user still heard popping. The source PCM showed four large same-channel sample jumps, although no input clipping; the localhost UDP test also failed during `bsdInitialize` with `00001759`. To distinguish speaker/output-path artifacts from microphone processing artifacts, this release adds a short, quiet synthetic 400 Hz triangle-wave reference tone using the same audio-output service, with a fade at both ends, before recorded speech playback. The source and NSO are not changed beyond this foreground diagnostic. It does not claim the pop is fixed. Local compile/hardware testing still required.

## November 17, 2026 — v0.9 forward declaration build fix

The local NRO-only build caught a missing forward declaration for `voice_prepare_playback` after v0.9 reorganized playback processing. Added its `static` prototype before `voice_capture_test` so the compiler knows its signature. The unused historical diagnostic-function warnings are unrelated. No background NSO or runtime changes; requires a local rebuild.

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

## November 17, 2026 - v0.22 first native QR-login attempt and fonts

Replaced handmade 5x7 text with public-domain 8x8 fallback, added optional antialiased system TrueType rendering via pl:u and stb_truetype (no proprietary font distributed), implemented native memory-only RSA-2048/OAEP-SHA256 via devkitPro switch-mbedtls, direct remote-auth v2 WebSocket with Origin and Sec-WebSocket-Accept verification, decrypted nonce proof, fingerprint-bound live QR, mobile confirmation and ticket exchange over TLS, ephemeral user token, real account/guild metadata retrieval and graphical selection. Added Build-NRO.bat switch-mbedtls package bootstrap, logout, and stage-only sanitized auth diagnostics. Unofficial Discord protocol, CAPTCHA, dependency versions, and Switch2 font/audio service compatibility are not yet hardware-tested. Not guaranteed to log in. Resident NSO unchanged. Voice remains unimplemented.

## November 17, 2026 - v0.22 QR login build fix: Switch monotonic clock

User's first v0.22 Build-NRO.bat installed switch-mbedtls-2.28.10-1 successfully and compiled main/voice_packet/gateway/graphical_ui/remote_probe/QR encoder/auth_crypto; stopped at native_login.c:122 with an implicit declaration of osGetTime (3DS-only API). Replaced all 11 usages with dnx2MonotonicMs() using C clock_gettime(CLOCK_MONOTONIC), explicit u64 millisecond return, and added time.h; preserves heartbeat and timeout semantics. No rebuild verified on the user's devkitPro yet; other files later in build may expose additional errors. All prior functionality and resident NSO unchanged.

## November 17, 2026 - v0.22 native QR nonce challenge diagnostics

Real Switch2 log from discordnx2-hardware(20261118-002427).txt confirmed: RSA session creation, remote-auth connection, then FAILED TO VERIFY ENCRYPTED NONCE before QR was generated. Added local RSA-2048 OAEP encrypt/decrypt self-test before connecting and replaced ambiguous nonce failure with stage-specific messages for absent/invalid encrypted_nonce, base64 decoding, ciphertext size mismatch, OAEP decrypt failure, and proof encoding failure. These statuses contain no nonce, keys, fingerprint or tokens. Build and actual QR login not yet verified after this fix. Existing font, audio and resident NSO unchanged.


## v0.23 — native nonce capacity, custom icon and rounded menus (2026-11-17)

- Based on verified main 5c39f8c8d46ce262f3b277eb3536e400d3b61313. Fix latest proof-encoding failure by sizing nonce proof for the full 190-byte RSA-2048/OAEP-SHA256 plaintext bound, including mbedTLS padding/terminator. Enlarge and wipe outgoing JSON.
- Add deterministic Base64URL boundary/roundtrip checks to local crypto selftest; reject truncated JSON fields and initialize cleanup state before early TLS/WS failures.
- Embed user-supplied numeral-2 artwork in the native UI and actual NRO icon; NACP version 0.23. Use clipped rounded panels, rows, buttons, focus markers and sidebar; prevent short-row hint overflow.
- Preserve audio, Gateway diagnostics, controls, system font/fallback and resident NSO. Build-NRO.bat still builds NRO only.
- Validation: clean native NRO compile/link/package passed; existing voice-packet host tests passed; production encoder host tests with mbedTLS 2.28.10 passed at 32/64/96/128/190 bytes, including exact/insufficient capacities and old failure reproduction. NRO icon and NACP verified. Hardware login and UI pending.
- Pull main, Build-NRO.bat, FTP only dist/DiscordNX2.nro, inspect menus and run Servers > QR sign-in once. Phone approval, ticket exchange and real guilds remain unverified until console testing; no claim of successful login or Discord voice.


## v0.24 - menu cleanup and Pretendo trial

- Replace numeral-2 icon with updated user artwork in NRO and UI; move Switch 2 Native down 3px.
- Remove verification badges, focus/status/build overlays and development copy; retain controls and explicit diagnostics.
- Embed user-supplied Pretendo.ttf as default with fallback for missing digits/punctuation, system font and bitmap initialization fallback. Clip text to column width.
- Mobile approval reported, console account loading still unresolved. Add safe HTTP status and token/profile/guild failure categories without logging response bodies or credentials.
- Clean NRO build and host-rendered menu checks passed. Hardware font and post-approval login require testing. FTP only NRO; resident NSO unchanged.


## v0.25 - phone-approved ticket exchange

- Hardware v0.24 confirms phone approval, followed by HTTP400 at ticket exchange; account/guild requests never started.
- Align ticket endpoint with current reference client (API v10), include explicit Origin/Referer, retain honest app user agent, and redeem before local remote-session teardown.
- Decode non-200 HTTP bodies to identify only fixed CAPTCHA/rate-limit categories and bounded numeric API error codes. Never print response messages, CAPTCHA fields, ticket/token or raw bodies. No CAPTCHA bypass or automatic retries.
- Add production-path host HTTP regression tests; passed successful token framing, outgoing request, CAPTCHA, chunked API rejection, truncation, rate limiting and state reset. Clean NRO build passed; live login remains unverified.
- Pull main, Build-NRO.bat, FTP only NRO, retry phone approval. If blocked, stage-only hardware report now distinguishes the rejection. Audio, menus, font, controls, Gateway probes and resident NSO unchanged.


## v0.26 - Discord-only interface and bounded login reads

- Replace test dashboard with login/logout; retain only Dashboard, Servers and Voice chat. Remove diagnostic tab, connectivity/audio tests and test shortcuts from active interface.
- Use smaller mixed-case shared-font text (Pretendo/bitmap fallback), explicit column bounds and generous QR padding. Fix auth text incorrectly clipped to sidebar width.
- Save sanitized login progress at every stage, before UI/network waits. Use bounded nonblocking TLS I/O; preserve partial WebSocket headers/payloads across polls, reassemble text fragments and handle interleaved ping/pong.
- Add production WS tests for partial frames, fragments, binary ping, oversized/masked/orphan rejection. Existing HTTP regression tests pass. NRO build and host menu/auth rendering checks passed.
- Server rows use real cached guilds after successful login. Voice displays selected server plus channel/member disconnected states; channel/member networking and Discord voice remain unimplemented. Live login and console appearance still need hardware verification.
- Pull main, Build-NRO.bat, transfer only NRO, Dashboard > Log in. No resident NSO changes. Stage-only hardware log is now written during login even if a later wait fails.


## v0.27 - preserve confirmed CAPTCHA blocker

- v0.26 hardware log confirms QR and phone approval succeed; ticket exchange requires CAPTCHA and no account session is obtained.
- Preserve the CAPTCHA, rate-limit or API rejection explanation through cleanup and on Dashboard instead of replacing it with generic QR failure/expiry.
- Native app has no interactive CAPTCHA completion flow. This is the remaining authentication blocker, not a successful login. No bypass or fake server list added.
- NRO build and HTTP/WS regression tests passed. No repeated hardware QR test needed merely to re-establish this blocker. Optional NRO-only update displays the accurate explanation; resident NSO unchanged.


## v0.28 - user-completed CAPTCHA browser integration

- Implement bounded RAM-only CAPTCHA challenge parsing and one same-ticket resubmission with provider solution plus optional enterprise request/session headers.
- Add native browser applet flow and exact callback URL validation bound to a random 256-bit state. Release/restore auth framebuffer; wipe challenge, solution, headers and browser config/reply.
- Add static verification frontend using official hCaptcha widget and enterprise rqdata. Only CAPTCHA widget fields enter it; ticket and Discord account token remain native. No automatic solving, host spoofing or CAPTCHA bypass.
- Hosted isolated page on user-supplied https://mmonsterpatch.gamingwithgoose.com/discordnx2-captcha.html via community repo commit 74feae5; HTTP200 verified. Existing site pages unchanged.
- Native build/link, HTTP/WS/CAPTCHA regression tests and synthetic frontend tests pass. Browser-enabled compilation was checked too. Real Switch browser launch, provider domain/browser acceptance, callback and complete login still require hardware testing. No claim of successful CAPTCHA or login.
- Pull main, Build-NRO.bat, FTP only NRO. Approve QR login, complete browser CAPTCHA yourself if it opens, then verify real account/servers or send stage-only report. NSO unchanged.


## v0.29 - distinguish unavailable system browser

- User reports This feature is not available; v0.28 browser API returned zero without a callback. Read the applet exit reason separately and accept only LastUrl callback exits.
- Log both Result and exit reason. Error-dialog exit 7/API failure keeps an explicit browser-unavailable explanation on Dashboard.
- Embedded-browser assessment: no ready compatible engine installed; lightweight HTML rendering alone does not establish hCaptcha compatibility. No browser-engine port or phone relay is claimed.
- NRO build and HTTP/WS/CAPTCHA regressions passed. Actual system-browser availability and successful login remain blocked/unverified. Resident NSO unchanged.


## Native browser port groundwork - after v0.29

- User selected entirely-on-console browser direction and withdrew phone relay; uncommitted relay backend discarded, no relay deployed and Z: server files untouched.
- Add original libounce framebuffer/HID surface backend for pinned NetSurf/libnsfb interfaces and reproducible cross-compilation check. No system browser applet or original-Switch SDL graphics dependency in this adapter.
- Adapter compiles with warnings as errors. This is not a full browser build or a CAPTCHA fix. HTML/JS/HTTPS engine integration, security and hCaptcha compatibility remain unfinished. Current Discord NRO and resident NSO unchanged; no new console test requested.


## Chromium Switch 2 runtime components - after v0.29

- Add reproducible local LLVM/AArch64 LKL build and libounce host adaptation; compile real Chromium NX ELF/memory/syscall/CDP/trampoline core against pinned revisions. Fix generated host logging after freeing a joined thread.
- Add original separate Linux runtime test NRO: real host worker thread, kernel boot, uname and tmpfs roundtrip, clean halt, dedicated log, main-thread display ownership. Linked locally; console execution remains unverified.
- Add optional Build-Browser-Runtime.bat; it does not overwrite DiscordNX2.nro. Clarify Build-NRO.bat still builds the existing app; no CAPTCHA/login fix is claimed.
- NetSurf native dependency libraries compile, but source audit rejects its runtime for CAPTCHA use (Math.random-backed getRandomValues, same-page messaging, unsupported Workers). BrowseNX is a system applet launcher, not an embedded engine.
- Chromium rendering, guest image, session-bound CAPTCHA integration and provider acceptance remain unfinished. A one-time browser data image is needed beyond NRO-only transfer. No relay/server changes, NSO changes, Docker or Actions.


## v0.30 - in-app executable-memory compatibility check

- Hardware confirmed the separate LKL runtime boots, completes file checks and shuts down (result 0).
- Add original executable-memory backend using libounce process-code mappings, guarded virtual address reservation, read/execute-only permissions, cache maintenance, replacement and cleanup. Failed unmap retains backing storage rather than freeing memory still mapped by Horizon.
- Dashboard R+X explicitly runs one small check per launch: fixed built-in AArch64 returns 42, then rewritten code returns 53. Logs each stage immediately to the existing hardware report and shows a dedicated result screen. No diagnostics tab, automatic login test, account data or networking added.
- Included in normal Build-NRO.bat; no Linux/browser dependency rebuild or extra SD assets. Native source is checked with the existing compiler; final NRO build left to user as requested.
- CAPTCHA login and full Chromium integration remain unfinished. Audio, networking, normal login controls and resident NSO unchanged.


## v0.31 - guard browser code-memory operations

- v0.30 hardware allocated storage, then crashed inside memory setup before any test code ran. Exact failing operation is not yet known.
- Check loader hints for privileged cache, mapping, unmapping and permission calls before allocating or calling them; report unavailable and skip when not advertised. Missing hints are not proof of kernel incompatibility.
- Replace raw ARM cache operations with a checked system data-cache flush and kernel code mapping/permission transitions. Add stage checkpoints before/after individual operations so a crash retains the exact last step.
- Source checks only; user builds with Build-NRO.bat and repeats Dashboard R+X. No dependency builds, audio/network/NSO changes or CAPTCHA success claim.


## v0.32 - use the Switch 2 toolchain JIT interface

- v0.31 loader does not advertise legacy process-code/cache syscalls; the guard skipped them. Logged 00001159 was generated by the test, not returned by Horizon.
- Replace the original direct mapping/cache adapter with installed libounce JIT APIs. The Switch 2 implementation supports loader-provided staging/publication callbacks, so absent legacy syscall hints must not gate this route.
- Keep R+X opt-in, per-operation stage logging, RX verification, fixed 42/53 execution/replacement checks and checked closure. No dependency rebuild or NRO build performed; compiler and installed-library symbols checked.
- Full browser and CAPTCHA integration remain unfinished. Resident NSO, audio and networking unchanged.


## v0.33 - link required kaiHEN JIT hooks

- v0.32 jitCreate safely reports 00004759, LibnxError_JitUnavailable. Found the app omitted -lkaihen required by the installed kaihen.h JIT instructions.
- Link -lkaihen before -lounce_core. Its linker script includes kaihen-jit.o explicitly, supplying loader map/unmap/alias/publication hooks referenced weakly by libounce.
- Verify hooks with a link check using cached application objects only; no NRO or dependency rebuild. User repeats normal Build-NRO.bat and Dashboard R+X. Actual JIT execution and CAPTCHA remain unverified; NSO unchanged.


## v0.34 - encrypted direct phone CAPTCHA test

- User confirmed v0.33 JIT test passes. Native Chromium still needs additional loader services; user requires normal public jailbreak compatibility and selected direct phone handoff. No loader/bootstrap/NSO update made.
- Replace unavailable system-browser backend with second phone-camera QR after Discord app approval. Official hCaptcha remains user-completed on an isolated HTTPS static page; no community relay or PC companion.
- Encrypt result using WebCrypto AES-256-GCM, RSA-2048 OAEP/SHA256 wrapped key and a fresh 256-bit state authenticated in AAD/plaintext. Only ciphertext reaches the Switch's private-LAN HTTP listener. Discord ticket/account token/private key stay native and RAM-only.
- Bounded temporary listener, private-address checks, exact-path POST parser, strict length and transfer-encoding rejection, cancellation/timeouts/attempt limits, secret wiping and one same-ticket retry. Preserve specific failure text. Larger verification QR and sentence-case instructions distinguish phone camera from Discord scanner.
- Publish isolated discordnx2-phone.html in existing community website (4823cfa). No server relay, Z: edits, jailbreak or plugin changes.
- Changed native source passes compiler checks; host parser and actual WebCrypto/mbedTLS interoperability tests pass, including tampering/wrong-state rejection. Existing login regressions pass. No NRO or browser dependency build performed; user runs Build-NRO.bat.
- Hardware phone delivery, provider domain/cross-device acceptance and final login remain unverified. Browser may warn about local HTTP navigation; payload is already encrypted. No CAPTCHA bypass or browser/origin spoofing.


## v0.35 - phone verification button and result length

- Fix silent rejection of hCaptcha results longer than 2048 bytes, which could leave Send to Switch disabled. Negotiate a 4095-byte maximum in the new QR and enlarge native buffers within the existing header limit; never truncate results.
- Phone page shows explicit result-size and encryption-stage failures. Read completed provider responses as a callback fallback; no synthesized token or bypass. Old QR requests keep their 2048-byte bound and receive a useful error.
- Add actual WebCrypto/mbedTLS tests for 4095-byte results, oversize rejection and legacy limits. Source checks only; user builds NRO.


## v0.36 - direct system Wi-Fi browser launch test

- User prioritizes self-contained operation with normal public jailbreak: no DNS/90DNS changes, PC host, custom payload or plugin replacement unless proved necessary.
- Add opt-in Dashboard R+Y test using public webWifiCreate/webWifiShow, fixed Example Domain HTTPS URL and no connection-test override. Release and restore framebuffer, persist each stage, log API status separately from user confirmation that the actual page appeared.
- Default phone handoff is disabled while the on-console route is tested; pending phone-page/button fixes remain local and are not deployed to the personal website. No real login/CAPTCHA is attempted by this test.
- Source checks only, no NRO/browser/dependency build. No DNS, loader, payload, NSO or audio/network implementation changes. Hardware availability and hCaptcha compatibility remain unverified.


## v0.37 - temporary verification site in this repository

- User authorized GitHub Pages in DiscordNX2 while proper Switch 2 browser support is developed. Add buildless docs/index.html and canonical docs/verify.html, published from main:/docs with .nojekyll.
- README and both site pages explicitly state hosting may stop at any time, without notice, with no uptime guarantee. CAPTCHA logins may stop if hosting stops; self-hosting via configurable HTTPS URL remains available.
- Re-enable the temporary phone fallback and change the default from the separate community website to https://gamingwithgoose.github.io/DiscordNX2/verify.html. Include prepared result-size, callback fallback and visible-error fixes; tests now exercise the published source.
- v0.36 direct Wi-Fi browser test returned applet reply 0016a86e and no actual test page; no browser success claimed. Normal public jailbreak, DNS, resident NSO and local-only NRO builds remain unchanged.
- No custom Actions workflow, Docker or NRO cloud build. GitHub Pages uses its managed deployment infrastructure. No custom domain or Cloudflare change is required.

- Repository cleanup: replace the accumulated README test history with a complete current guide and a prominent incomplete-functionality disclaimer. Document supported menus, controls, setup, NRO-only transfers, two QR steps, known blockers, privacy, self-hosting and developer checks; retain history in Git/CHANGELOG. Setup now directs users to Build-NRO.bat.
- User made the repository public; GitHub Pages main:/docs enabled successfully with HTTPS and no custom domain. NACP metadata now depends on its Makefile so version changes are rebuilt.
- Live publication verified: project home and verify.html both return HTTP200 over HTTPS and match the tested source exactly. Phone encryption/page tests and AArch64 strict syntax checks passed; no NRO built, real Discord acceptance still awaits hardware testing.


## v0.38 - channels, text chat, Unicode names and saved login

- Hardware milestone: user confirmed v0.37 account connection and real server list through the project-hosted CAPTCHA page.
- Preserve UTF-8 and JSON Unicode escapes/surrogate pairs instead of replacing account/server names with question marks. Add bounded code-point glyph caching and shared-font fallback; unavailable glyphs/emoji still need font support.
- Read Discord's server-folder ordering without changing settings; flatten folders, support up to 200 servers, indicate fallback API order.
- Server selection now loads category/channel navigation. Text/Voice tabs filter real channels; voice/stage selection remains explicitly disconnected because Discord voice calls are unfinished.
- Basic text/announcement history: latest 30 messages in chronological order, expanded scrollable reading, manual refresh and native controller composer (ASCII, 512 characters). Explicit Send action, escaped JSON, no automatic retry or mention parsing. No real account messages sent by development tests.
- Optional PIN-protected saved session with PBKDF2-SHA256 and AES-256-GCM on SD; confirmed eight-digit PIN, startup unlock and validated account reload. Wrong PIN/network failure permits retry, logout forgets saved credentials; no PIN/key stored alongside ciphertext. Exit clears RAM without forgetting saved login.
- Update README with actual supported behavior, unfinished voice/rich chat and first-time save/reopen test instructions. Maintain HANDOFF. Preserve existing audio, networking, resident plugin and normal public jailbreak; NRO-only update.
- Host data/serialization and cross-process encrypted-session tests, existing login/CAPTCHA regressions and changed-source AArch64 syntax checks passed. No NRO built; hardware verification of these new features remains required.


## v0.39 - touch conversation layout, native keyboard and live updates

- Replace large tab/card flow with a server rail, channel/category sidebar, conversation pane and persistent tappable message input/Send. Initialize touchscreen coordinates, release-only taps and per-pane swipe scrolling; retain controller navigation and add PIN touch controls.
- Request the Switch native QWERTY software-keyboard applet when the input is activated. Keep draft separate from Send; preserve it on cancellation/failure and unconfirmed sends. Provide an explicit embedded touch QWERTY overlay if system applet access is unavailable. Actual native keyboard/touch compatibility requires hardware testing.
- Add foreground Gateway message event worker with Hello/Identify/Ready, heartbeat/ACK, Resume and selected-channel subscription. Refresh on relevant message events and periodic resync; automatic five-second REST fallback when Gateway is unavailable. Respect rate-limit backoff, stop on denied access, bound frames, and join/wipe before other account actions. No background NSO change or fake live success.
- Apply pending messages on UI thread, preserve draft and older-message anchors, follow newest only when already at bottom, and keep expanded reading stable. Add sanitized live connection report without account/channel/message or credential values.
- Embed unmodified OFL Noto Sans/Math/Symbols2/monochrome Emoji fonts with pinned provenance and license notices. Test actual styled mathematical letters, multilingual characters and emoji. Add pixel-aware wrapping; unsupported sequences/complex shaping still limited.
- Source and font-assembly checks plus touch geometry, synthetic live-worker and actual-font rasterization tests passed. No NRO built. Normal public jailbreak, existing saved-session format, audio, networking and resident NSO preserved. Discord voice calling remains unfinished.
- Final regression cycle: existing HTTP/WS/QR/CAPTCHA, encrypted-session persistence and client data tests pass. Bundled Noto Sans is the UI primary font so text rendering no longer depends on console font initialization. No NRO built.


## v0.40 - in-place conversations, server icons and mobile input

- User confirmed v0.39 touch works; photos identified full loading-screen switches on channel/send, unreadable rail initials and an oversized labeled fallback keyboard.
- Keep the chat framebuffer alive for immutable background channel/send/refresh/catalog/image jobs. Apply results on UI thread only to matching context; block duplicate in-flight sends, preserve unconfirmed drafts, and apply empty histories. Login/native applet flows remain separate. No full loading/auth overlay for channel selection or sends.
- Load actual server icons from fixed Discord CDN paths without account Authorization. Add bounded 48-entry RGBA cache, PNG decoder with pinned upstream license/provenance, shape clipping, larger fallback initials and full-name controller tooltip. Image cache remains RAM-only.
- Replace fallback keyboard with compact full-width phone-style QWERTY, larger tightly spaced keys, separate number/symbol pages and smiley. Remove Touch QWERTY heading and instructional footers; use neutral gray chat styling. Done does not send.
- Add Unicode/current-server emoji and server-sticker picker, explicit staged sends, bounded custom-emoji markup rendering and static PNG/APNG sticker previews. Honor unavailable entries/API permissions. GIF search, animated media, full attachment/rich-text support and voice calling remain unfinished.
- Test actual production jobs with deferred/stale result fixtures, duplicate-send rejection, failure preservation, empty history, catalogs, unauthenticated CDN and real PNG limits. Verify mobile keyboard geometry, binary HTTP bodies and existing live/font/auth/session regressions. Changed-source AArch64 checks pass; no NRO built. Resident NSO, DNS/public jailbreak and established audio/network pipeline preserved.

## v0.41 — 2026-11-18

- Open phone-style keyboard directly from composer/X; remove native keyboard action and closed-keyboard instruction.
- Display message nicknames, avatar/default/server avatars and UTC time; fetch artwork asynchronously without account credentials.
- Render first posted image/embed preview, bounded PNG/JPEG and small animated GIF; larger animations use a first-frame preview. Video/WebP/GIF search remain unsupported.
- Preserve Unicode/newlines; vendor pinned MIT utf8proc for grapheme-aware deletion/wrapping/scrolling and NFC display normalization. Complex-script shaping, RTL and joined/color emoji rendering remain incomplete.
- Add explicit native voice connection test with muted/deafened join, scoped session/server events, TLS voice WebSocket, 4017 handling and best-effort automatic leave. This is not voice calling: Opus and DAVE/MLS crypto port remain required; microphone/audio never start.
- Synthetic production-path media/Unicode/voice tests and existing chat/live/session tests pass; source syntax checked without an NRO build. User hardware build/test pending. Resident NSO, audio diagnostics, public loader and login encryption preserved.

## v0.42 — 2026-11-18

- Move message attachments beneath captions with taller rows, aspect-preserving previews, larger bounded image decoding and resized Discord PNG requests; accept image filenames when MIME metadata is missing. Failed images show unavailable instead of indefinite loading.
- Add blinking end caret and scrolling draft tail; hold Backspace to repeat/accelerate by whole graphemes. Done sends through the existing guarded send job. New x/B/Plus cancel clears the draft and selected sticker.
- Apply complete Gateway incoming messages and deletions directly, avoiding an extra history request. Keep Gateway open during artwork downloads and keyboard entry; same network owner services one bounded image request between pending Gateway events.
- Add real1200x900 image, variable attachment geometry, held-delete timing, direct-event delivery and delegated artwork cancellation tests. Source and host tests pass; no NRO built and console verification pending.

## v0.43 — 2026-11-18

- Request portrait/media previews with original proportions; replace stale transform parameters while retaining signed query values, rather than forcing landscape dimensions.
- Show reply author/snippet/deleted state. Avatar/name opens a read-only profile card; body tap retains message reading. Add actual server nickname/member-role lookups, primary role name colors, guild tags/badges, APP labels and named mentions.
- Add basic inline bold/italic/underline/strike/code. Full Discord Markdown, profile effects/actions and reactions remain unfinished.
- Add existing direct/group-DM list, history, send and live-update paths, separated from guild context. App icon opens DMs; sidebar account name opens settings. No new-DM creation or DM calling.
- Correct voice test's misleading Gateway cooldown failure: cancellable wait with countdown before Identify; clearer resume/stage/code failures. Voice audio still requires DAVE/Opus.
- Synthetic data/job/live/cooldown regression fixtures and strict source checks pass; hardware comparison remains pending. No NRO/dependency build, NSO/payload changes or real account messages sent by tests.

## v0.44 — 2026-11-18

- Extend actual GIF animation beyond12 small frames with bounded incremental compositing, <=60 sampled preview frames and preserved timing. MP4/WebM GIF-style embeds still require video support; oversized/work-heavy GIFs remain still previews.
- Embed pinned OFL Noto Sans Symbols and test the exact enclosed MODDER letters; enlarge font fallback capacity.
- Persist per-account/per-server last channel and last DM as bounded ID-only preferences; automatically reopen valid selections, without voice autojoin.
- Load DM recipient/default avatars and group conversation icons in the sidebar.
- Add sentence capitals, double-space punctuation and explicit offline English word/next-word suggestions, keeping messages local until Send/Done.
- Use validated regional Gateway Resume hosts, raise uncompressed Ready capacity to4MiB and clear stale pre-Ready sessions. Expand safe voice diagnostics; stage1/code0 exact cause remains unconfirmed and voice audio remains unfinished.
- Add synthetic GIF/font/preferences/DM-art/keyboard/Gateway regression checks. Source-only update: no NRO/dependency build, resident NSO or loader changes.

## v0.45 — 2026-11-18

- Follow up hardware stage2 report: Gateway Ready and sent join are confirmed; missing voice event and permissions are not yet established.
- Subscribe selected guild before voice request, explicitly disable video, scope guild-less voice states by exact account/channel, and accept valid endpoint ports with verified TLS and matching Host header. Legacy :80 uses TLS443.
- Begin a full confirmation wait after join; avoid duplicate Ready/Resume join requests. Distinguish absent/state-only/server-only/pending allocation/invalid endpoint/self-disconnected outcomes and log fixed event/leave counters without private data.
- Replace verbose voice warning/footer with a concise, accurate unavailable-calls message. DAVE/Opus audio is still unfinished; this is not a working call implementation.
- Extend synthetic voice fixtures for scope, missing events and port handling; preserve cancellation and automatic leave. No NRO, NSO, dependency or loader build.
