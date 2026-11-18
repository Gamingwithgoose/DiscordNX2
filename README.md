# DiscordNX2

> **INCOMPLETE FUNCTIONALITY AT THIS TIME.** DiscordNX2 is experimental Switch 2 homebrew under active development. Full Discord login is still being tested. **Discord voice chat, text messaging, live voice channels/member lists and background Discord calls are not implemented.** Phone approval or a completed CAPTCHA does not by itself mean the Switch has logged in.

DiscordNX2 aims to bring Discord voice chat to Switch 2. The current v0.37 build provides native graphical menus, an experimental QR sign-in flow and a temporary phone CAPTCHA handoff. This is an unofficial project, not affiliated with Discord or Nintendo.

## Current functionality

| Area | Current status |
| --- | --- |
| Interface | Dashboard, Servers and Voice chat menus with controller navigation, rounded panels and custom branding. |
| Native QR login | Real hardware reaches phone scan/approval. Discord has then required CAPTCHA; complete account login remains unverified. |
| Phone CAPTCHA | User completes hCaptcha on the project website; an encrypted result returns directly to the Switch over the same Wi-Fi. Local interoperability tests pass; Discord acceptance requires hardware testing. |
| Servers | Fetches real server names after successful account login; no sample server list. Currently limited to 64 servers. |
| Voice chat | Interface only. Joining channels, live member lists, sending/receiving Discord audio and background calls remain unfinished. |
| Audio and networking | Microphone capture/playback and separate transport/Gateway tests have passed hardware tests. These are not a working Discord call. |
| C button | Not available through the tested controller APIs; no C-button push-to-talk or GameChat replacement. |
| On-console browser | System browser attempts showed "This feature is unavailable". A compatible embedded browser is not included. |

## What you need

- A Switch 2 already running a compatible public ns2jb/kaiHEN homebrew environment and homebrew menu. This repository does not install or provide the jailbreak.
- A Windows PC with [GitHub Desktop](https://desktop.github.com/), [devkitPro](https://devkitpro.org/wiki/Getting_Started) and the [ounce-devkit Switch 2 toolchain](https://github.com/ouncebrew/ounce-devkit).
- An SD-card or FTP transfer method for the NRO.
- For login testing: a phone with Discord installed, a current browser, internet access, and the phone and Switch on the same Wi-Fi for the CAPTCHA return.

The current test uses the normal public jailbreak. It requires no custom payload, DNS/90DNS changes, PC-hosted server or community CAPTCHA relay. Keep your existing resident DiscordNX2 NSO installed if you already have one.

## Build on Windows

### First PC setup

1. Install devkitPro using its official instructions. The batch files expect its default location, `C:\devkitPro`.
2. In GitHub Desktop, clone [Gamingwithgoose/DiscordNX2](https://github.com/Gamingwithgoose/DiscordNX2), or select the existing checkout and pull `main`.
3. If the Switch 2 compiler is missing, run **Setup-Toolchain.bat**. It asks before running ounce-devkit's installer, which can add its package repository/signing key and install packages into devkitPro. This is PC toolchain setup, not a new console payload.
4. Run **Build-NRO.bat**. It checks for the maintained `switch-mbedtls` crypto library and attempts to install it through devkitPro's package manager if missing. Internet is needed for that first dependency installation.
5. Wait for **NRO built**. The output is **`dist/DiscordNX2.nro`**.

All NRO compilation happens on your PC. No Docker or cloud NRO build is used. Ordinary MSYS2 alone does not supply the Switch 2 compiler. If devkitPro is installed elsewhere, adjust the batch file's paths to match your installation.

### Every subsequent update

1. Close DiscordNX2 on the Switch.
2. Pull `main` in GitHub Desktop.
3. Run **Build-NRO.bat** and confirm it finishes successfully.
4. FTP/copy **only `dist/DiscordNX2.nro`** into your usual homebrew folder on the SD card, replacing the previous NRO.
5. Launch the updated NRO from the homebrew menu.

**Do not replace the resident NSO or restart kaiHEN for this update.** Build-NRO.bat rebuilds the application only. If a build fails, do not transfer a leftover NRO from an earlier build; report the first compiler/linker error.

### Other build entry points

- In a configured devkitPro shell, run `./build-local.sh app` for the NRO-only build.
- **Build.bat** / `./build-local.sh all` also builds the resident plugin into `dist/discordnx2.nso`. This is a developer/initial-plugin build, not the current update path. Building it does not install it.
- A first-time resident-plugin installation, if testing that component, places `discordnx2.nso` in `/kaihen/plugins/discordnx2.nso` before the next normal kaiHEN startup. Existing testers should leave their installed plugin unchanged. Plugin diagnostic results do not establish background Discord-call support.
- **Build-Browser-Runtime.bat** and `browser-port/` are separate research tools. They are not required for the login test and do not provide a working browser or CAPTCHA solution.

## Test QR login and CAPTCHA

1. Open **Dashboard**. Press Right to focus its content, highlight **Log in with QR code**, and press **A**.
2. Scan the first QR using **Discord's own mobile QR scanner** and approve your own Switch login. Keep DiscordNX2 open.
3. If Discord requests CAPTCHA, the Switch displays a **second QR**. Scan this one with your phone camera/browser, rather than Discord's scanner.
4. Confirm the phone and Switch are on the **same Wi-Fi**. Keep the Switch on its verification screen.
5. Complete the hCaptcha challenge yourself on the page. Wait for the message that verification is complete, then tap **Send to Switch**. If your browser asks to continue to a local address, the form contains only the encrypted verification result.
6. Wait for the Switch to finish the login attempt. Success must show the account and actual server list on the Switch. A phone success message alone is insufficient.

The local return listener lasts about 150 seconds. If it expires, cancel and start a fresh login; old QR links and challenge results cannot be reused. The app retries the pending login with the returned challenge result once. Provider restrictions may still reject a legitimate cross-device challenge.

No Discord developer application, manually copied account token, PC companion or password entry on this website is required. Login tickets, account tokens and private keys stay in app memory and are cleared after use. Login is not saved between launches; logging out clears cached account/server information.

## Controls and menus

| Control | Action |
| --- | --- |
| D-pad / stick | Navigate menus and lists. |
| Left / Right | Move between sidebar and content. |
| A | Activate the selected item. |
| B | Return focus to the sidebar; cancel during login. |
| Plus | Exit the menu; cancel during login. |

Dashboard provides login/logout. Servers displays the real cached server list after successful login. Selecting a server opens Voice chat, whose channel/member states remain disconnected until that functionality is implemented.

Developer shortcuts remain on Dashboard: **hold R + X** for the executable-memory check, and **hold R + Y** for the system Wi-Fi browser probe. Neither is needed for ordinary login. The Wi-Fi browser probe failed on the tested setup; an API return code of zero does not mean a browser appeared.

## Temporary verification website

- Project site: [gamingwithgoose.github.io/DiscordNX2](https://gamingwithgoose.github.io/DiscordNX2/)
- Verification page: [verify.html](https://gamingwithgoose.github.io/DiscordNX2/verify.html)
- Website source: [`docs/`](docs/), in this same repository.

**Hosting may stop at any time, without notice. No availability, uptime or ongoing hosting is guaranteed. This website is provided only as a temporary fallback until proper browser support is available on Switch 2.** There is no promised date for native browser support. If hosting stops, CAPTCHA-required logins using this address may stop working.

GitHub Pages publishes static files from **main /docs**. No Cloudflare configuration or custom domain is required. GitHub uses its managed Pages deployment infrastructure; this repository adds no custom Actions workflow and performs no cloud NRO compilation.

The website loads hCaptcha's official widget and encrypts its completed result for the active Switch session. A short-lived local listener receives the encrypted form directly. The site has no relay backend and does not receive your Discord login ticket, account token or private key. GitHub and hCaptcha serve their respective web resources. Opening verify.html without a fresh Switch QR does not start a login.

For independent hosting, publish `docs/` on an HTTPS static host. In a configured devkitPro shell, rebuild with your own page address:

```sh
make app-only PHONE_CAPTCHA_PAGE_URL=https://your-host.example/verify.html
```

To build without the phone fallback:

```sh
make app-only PHONE_CAPTCHA_ENABLED=0
```

The default address is compiled into each NRO; moving hosting requires rebuilding. Provider domain/browser restrictions still apply to self-hosted pages.

## Troubleshooting and reports

| Symptom | What to check |
| --- | --- |
| Phone says logged in, Switch has no servers | Phone approval precedes ticket exchange/CAPTCHA. Read the Switch's final status; account login is still experimental. |
| Send to Switch stays disabled | Use a fresh QR from the current NRO. Wait for encryption to finish and read the message above the challenge. Report that text if it fails. |
| Phone cannot return the result | Keep the NRO open and both devices on the same Wi-Fi. Guest-network isolation or a VPN may prevent the direct local connection. Start a fresh attempt after timeout. |
| QR failed/expired | Cancel and start a new attempt. Phone approval is not proof the Switch received an account session. |
| "This feature is unavailable" | This was observed in the system browser experiment. Use the temporary phone flow; changing DNS or replacing the NSO is not an established fix. |
| Compiler/toolchain missing | Check devkitPro's install path and run Setup-Toolchain.bat if Switch 2 packages are absent. |
| No voice audio or channel members | These Discord functions are not implemented yet. |

After a failed login/test, copy **`/kaihen/discordnx2-hardware.txt`** from the SD card before running another test. Share the stage-only report, NRO version and phone error text. **Never share QR links, CAPTCHA results, login tickets, account tokens, passwords or private keys.** Do not post a QR screenshot containing an active login session.

## Source layout and development

| Path | Purpose |
| --- | --- |
| `app/` | Native NRO, menus, authentication, audio and networking. |
| `plugin/` | Resident NSO source and existing IPC interface. |
| `docs/` | Canonical temporary website and verification page. |
| `tests/` | Synthetic protocol, page and encryption regression tests. |
| `browser-port/` | Isolated browser/runtime research; not a shipping browser. |
| `tools/` | Earlier PC login/pairing and relay experiments; not needed by the current native login flow. |
| `HANDOFF.json` | Implementation details, hardware evidence and remaining blockers. |
| `CHANGELOG.md` | Version-by-version development history. |

The previous README's superseded test/install directions have been replaced by this guide. Use Git history and CHANGELOG.md for historical changes rather than mixing old test procedures into the current workflow.

Small developer checks, separate from an NRO build:

```sh
sh tests/run-native-http-tests.sh
node tests/test_captcha_page.js
```

The phone encryption integration test currently uses Windows Node.js plus a Debian WSL host harness with a C compiler and mbedTLS development headers. Prepare it with `wsl -d Debian -- sh /mnt/c/path/to/DiscordNX2/tests/run-phone-handoff-tests.sh`, then run `node tests/test_captcha_phone.js` from Windows. These use synthetic results and do not solve or submit a real CAPTCHA. Passing local tests does not prove Discord will accept login on hardware.

Thanks to the ouncebrew, kaiHEN, devkitPro and upstream projects referenced in the source for the homebrew tooling and libraries.
