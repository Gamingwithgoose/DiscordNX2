# Native Switch 2 browser port - initial platform backend

This directory starts a native browser port without a hosted relay or Nintendo WebApplet. It is **not a working browser or CAPTCHA solution** and is deliberately excluded from Build-NRO.bat until engine integration is functional.

`switch2_surface.c` is original MIT-licensed platform code implementing the real libnsfb surface contract: a 1280x720 ABGR8888 software buffer, clipped pointer, VI framebuffer presentation respecting output stride, controller mouse/key events, quit, and cleanup. It replaces the RAM surface and must be linked without upstream ram.c/SDL drivers. Upstream NetSurf licensing and all dependency notices must be preserved when building/distributing the full browser; no upstream engine is bundled in this commit.

Run `python browser-port/check-port.py` with devkitA64/libounce installed. It fetches six interface headers from etonedemid/netsurf-switch at immutable commit `3dabe65c245ab29a22f243e88d407caf2c09b5c2` into ignored `.local/`, then cross-compiles this backend with warnings treated as errors. No Docker, Actions, NSO changes or browser download binaries are involved.

## Remaining implementation

- Rebuild NetSurf, libnsfb software plotters, QuickJS-ng and their libraries for libounce/Switch 2 rather than linking original-Switch SDL/libnx graphics binaries.
- Replace the original port's fixed libnx specs/A57 settings and media dependencies. Disable unnecessary ffmpeg/audio to keep the verification browser focused.
- Validate HTML/DOM/JavaScript, iframes, postMessage, canvas, fetch/XHR, TLS peer/hostname verification, origins/CORS and CSP. The engine's README is not evidence that hCaptcha works.
- Implement ephemeral navigation, no credential/history logging, and a session-bound result handoff to DiscordNX2; do not weaken provider origin/domain checks.
- Build and verify full browser rendering on console, then test user-completed hCaptcha on the approved page. Only integrate into DiscordNX2 after those gates pass.

Inspected alternatives: NetSurf Switch has a software-framebuffer/QuickJS path; Brewser's nx.js-based runtime needs its separate V8/Skia engine and web-platform validation; Chromium NX uses LKL plus a Debian image (roughly 1.9GB browser memory per its author), making it a much larger change to the NRO-only workflow.

Sources: https://github.com/etonedemid/netsurf-switch, https://github.com/natureglass/Brewser, https://github.com/shodowlo/chromium-nx, https://docs.hcaptcha.com/faq.
