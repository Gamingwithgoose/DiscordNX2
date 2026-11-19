# NS2 Hardware Tester v0.4 — browser investigation

Standalone diagnostic NRO for public kaiHEN on Switch 2 firmware 23.0.1.
No Discord login, NSO installation, firmware patching, DNS changes, or Chromium dependencies.

## Build and run

1. Pull the latest DiscordNX2 main branch in GitHub Desktop.
2. Double-click `Hardware Tester/build-test.bat`.
3. Copy `Hardware Tester/dist/Hardware-Tester.nro` to your SD card's `/switch/`.
4. Run the NRO from the existing homebrew menu.
5. Select each browser probe with UP/DOWN, launch with A, and describe what appeared onscreen.
6. Press Y to save immediately, or PLUS to save and exit.

**Report**: `/switch/Hardware-Tester-report.txt`; fallback to `/Hardware-Tester-report.txt` if the `/switch/` path cannot be created.
The report is flushed before every applet launch and after applet completion, along with controller events. Running the NRO again overwrites the prior report. Copy it off SD first if needed.

## Browser probes (7)

1. **Web baseline**: `webPageCreate` with `https://example.com/` (same baseline as v0.3).
2. **Web cursor + footer**: baseline plus documented pointer, left-stick, and footer settings.
3. **Web media features**: baseline plus JavaScript-extension and WebAudio flags; tests whether the configuration is accepted, *not* whether JavaScript executes.
4. **Web cache disabled**: baseline with page cache disabled and white boot background.
5. **News**: `webNewsCreate` with `https://example.com/`.
6. **Wi-Fi portal**: existing `webWifiCreate` baseline with no network-profile changes.
7. **Offline HTML control**: intentionally references a missing app-owned HtmlDocument; *not* a valid offline rendering test.

The log records creation/configuration return codes, `webConfigShow` results,
`webReplyGetExitReason` (including ErrorDialog), and the Wi-Fi reply code.
API return code zero means the call completed, **not** that a page loaded.
A white screen with an error dialog must be noted separately.

## Other controls

X enters the existing ordinary Npad button monitor; B returns. The C/GameChat
button is not guaranteed to appear in Npad state; this is not a private HID test.
PLUS saves and exits. Y saves without exiting. No minus-plus exit combination.

## Limitations

The NRO probes only documented public web-applet interfaces. It does not
enumerate privileged Horizon services, bypass restrictions, install browser
content, expose Discord credentials, or create a valid HtmlDocument NCA.
A result of `WebExitReason_ErrorDialog` would narrow the failure to an
applet-reported error but would not tell us its exact root cause.

The changes have been source-reviewed, but have **not been compiled or run
on Switch 2 hardware from this environment**. If build fails, send the
first compiler/linker error. If it runs, send the complete report plus
which probes displayed the Nintendo popup.
