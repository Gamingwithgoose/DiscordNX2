# Native Switch 2 browser work

**CAPTCHA login is still blocked.** Build-NRO.bat builds the current DiscordNX2 app; it does not include an embedded browser. The installed resident NSO is unchanged. No phone relay or server changes are used.

## Chromium NX direction

Chromium NX is a real Chromium/ARM64 runtime using LKL (Linux as a library). BrowseNX only opens Nintendo's system browser, which the user's console reports unavailable. Chromium NX's existing original-Switch GPU backend cannot be assumed compatible with Switch 2/libounce. Its large browser data image also needs a one-time SD installation; an NRO alone does not contain Chromium.

Implemented components:

- `build-lkl-llvm.sh` plus `lkl-aarch64-llvm.patch`: build pinned LKL with Debian WSL LLVM for AArch64, preserve kernel config, object, headers and host glue in ignored `.local/`. No Docker.
- `prepare-chromium-host.py`: adapt generated LKL headers for libounce/newlib, fix upstream host thread-join logging after free, preserve upstream source notices.
- `build-chromium-components.sh`: compile actual upstream ELF loader, guest memory/syscall handling, Chromium debugging-pipe and trampoline into a component archive; link the kernel and host glue with an original runtime test into an NRO. A component archive does not constitute a complete browser.
- `lkl_probe.c`: optional separate test boots the Linux runtime on a real Horizon thread, checks uname and an in-memory file roundtrip, shuts it down, and reports the result. Main thread owns display; kernel threads write only an isolated runtime log. No account/network access or guest image needed. This executable has compiled/linked but has **not run on hardware**.

`Build-Browser-Runtime.bat` reproduces this optional test using Python, Git, Debian WSL, and installed devkitPro/libounce. Debian tools required: clang/lld/llvm, make, flex, bison, bc, libelf-dev, cpio, rsync. Sources build on Linux storage under `/var/tmp/discordnx2-browser-port/linux`. Output: `dist/browser-runtime/DiscordNX2-Runtime-Test.nro`. It does not overwrite `dist/DiscordNX2.nro`.

Next hardware gate: run the separate test and check `sdmc:/discordnx2-browser-runtime.log`. A successful kernel test establishes only basic runtime compatibility. Rendering/input, executable memory/exception handling, Chromium guest image, trusted TLS/origin behavior, ephemeral CAPTCHA callback and actual provider acceptance remain unfinished. Do not claim working login until all those checks pass. No provider checks or automation indicators are suppressed.

Pinned upstream sources (fetched into ignored `.local/`, not vendored here):

- Chromium NX: https://github.com/shodowlo/chromium-nx/tree/87a8095d20cf73b0fde0eee3215e70ebc887f048
- LKL: https://github.com/lkl/linux/tree/d0f76a77e8c2ca68bd7d96190b8da09040eab072

Retain Chromium NX's and Linux/LKL's GPL-2.0 license/source requirements when redistributing linked binaries. The probe is built locally; this commit contains source/build adaptations, no third-party binary release. The upstream source and our exact patch remain available for corresponding-source reconstruction.

## Earlier NetSurf experiment

`switch2_surface.c` is original MIT framebuffer/HID platform code implementing libnsfb. `check-port.py` cross-compiles it with warnings as errors. `prepare-engine.py` and `build-support-libraries.sh` produced 14 AArch64 native support libraries against pinned NetSurf source `3dabe65c245ab29a22f243e88d407caf2c09b5c2`.

`audit-runtime.py` exits 2 on this pinned source: the JavaScript runtime uses Math.random for getRandomValues, implements only same-page postMessage and rejects Workers. These are security/compatibility blockers, so this engine is not integrated into login. The source-pattern audit is a rejection gate, not a complete security review or proof of support if it ever passes.


## v0.30 normal-app compatibility check

The separate runtime probe passed on the user's console (boot, file roundtrip, shutdown, result 0). The next test is implemented directly in the normal DiscordNX2 source: Build-NRO.bat, then hold R and press X on Dashboard. This executes only fixed built-in instructions in a read/execute-only alias, replaces them and verifies the replacement, and cleans up. Results go to the existing `/kaihen/discordnx2-hardware.txt`; B returns to Dashboard. It runs once per process, so relaunch before retrying. It requires no browser dependencies or extra SD assets. Full browser rendering and CAPTCHA remain unfinished.


v0.31 found no advertised legacy privileged syscalls and skipped them. v0.32 changes the normal-app code adapter to libounce's JIT API, whose Switch 2 interface supports loader-managed publication independently of those hints. Repeat normal Build-NRO.bat and Dashboard R+X to verify this route; no browser dependency rebuild is needed.


## Current direction: v0.34 direct phone handoff

v0.33 JIT checks passed on hardware. The Chromium `nx2` branch (ae3db90e112975cf6e27789f149de12173172dff) additionally requires gx-memory RO commands; that referenced loader branch was unavailable, and these services are not assumed present in the normal public jailbreak. User requires public compatibility and selected direct encrypted phone verification. Native browser integration is deferred; no custom loader is deployed. Build-NRO.bat now builds the phone handoff using existing app dependencies, with no browser data image or new console component. See HANDOFF.json for the protocol and remaining hardware/provider checks.


## v0.36 on-console priority

The user now prioritizes self-contained operation on the normal public jailbreak. Dashboard R+Y tests the direct system Wi-Fi browser API with Example Domain, without DNS changes, a PC host or replacement loader/plugin. The app logs API results and asks whether the page actually appeared. This is not a CAPTCHA test. Phone hosting is disabled by default; its pending button fixes are retained in source but not published to the personal website.
