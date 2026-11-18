# DiscordNX2

> **INCOMPLETE FUNCTIONALITY AT THIS TIME.** DiscordNX2 is experimental Switch 2 homebrew under active development. Account connection has been confirmed on hardware. Touch has been confirmed on hardware. The latest in-place requests, server icons, redesigned keyboard and media picker still need hardware testing. **Discord voice calling, live voice member lists and background Discord calls are not implemented.** Phone approval or a completed CAPTCHA does not by itself mean the Switch has logged in.

DiscordNX2 aims to bring Discord voice chat to Switch 2. The current v0.42 source adds chat avatars/nicknames, image and bounded animated-GIF previews, Unicode grapheme editing and accent normalization. Tapping the message box opens the phone-style keyboard directly. QR/CAPTCHA account connection, touch and PIN-protected saved login remain in place. This is an unofficial project, not affiliated with Discord or Nintendo.

## Current functionality

| Area | Current status |
| --- | --- |
| Interface | Real server-icon rail, category/channel sidebar and conversation with persistent input. Channel changes, sends, refreshes and image loading use in-place background jobs rather than the login/loading screen. Neutral gray styling, touch and controller navigation. |
| Native QR login | User confirmed successful account connection after the project-hosted phone CAPTCHA handoff in v0.37. |
| Phone CAPTCHA | User completes hCaptcha on the project website; an encrypted result returns directly to the Switch over the same Wi-Fi. User confirmed account connection; individual future attempts may still be rejected or expire. |
| Servers | Loads up to 200 real servers. Reads Discord folder/order settings and flattens folders in order; falls back to API order if unavailable. |
| Channels / text | Text/announcement channels show the latest 30 messages. A foreground Gateway worker applies complete incoming messages and deletions directly, and refreshes history for partial edits or resync; five-second automatic polling is the fallback. Tap input to open the compact full-width phone-style QWERTY layout with separate symbol pages. |
| Voice chat | Voice/stage channels expose an explicit connection test: briefly join muted/deafened, check the voice WebSocket and leave. No call audio or live members. DAVE/Opus integration remains blocked. |
| Emoji / stickers | Common Unicode emoji and current-server emoji/sticker picker. Emoji PNG and PNG/APNG sticker first-frame previews; explicit Send for drafts/stickers. Posted PNG/JPEG/GIF images and embed image/thumbnail previews are supported within limits below. GIF search and full rich media remain unfinished. |
| Saved login | Optional eight-digit PIN encrypts credentials on SD. Enter PIN on future launches instead of scanning QR; expiry/revocation may require login again. New in v0.38. |
| Special characters | Embedded OFL Noto Sans, Math, Symbols 2 and monochrome Emoji supplement console fonts, including styled mathematical letters. NFC accent normalization and grapheme-aware editing/wrapping keep combined characters together. Missing glyphs, complex shaping, right-to-left layout and joined/color emoji rendering remain incomplete. |
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

1. Open **Home**. Press Right to focus its content, highlight **Log in with QR code**, and press **A**.
2. Scan the first QR using **Discord's own mobile QR scanner** and approve your own Switch login. Keep DiscordNX2 open.
3. If Discord requests CAPTCHA, the Switch displays a **second QR**. Scan this one with your phone camera/browser, rather than Discord's scanner.
4. Confirm the phone and Switch are on the **same Wi-Fi**. Keep the Switch on its verification screen.
5. Complete the hCaptcha challenge yourself on the page. Wait for the message that verification is complete, then tap **Send to Switch**. If your browser asks to continue to a local address, the form contains only the encrypted verification result.
6. Wait for the Switch to finish the login attempt. Success must show the account and actual server list on the Switch. A phone success message alone is insufficient.

The local return listener lasts about 150 seconds. If it expires, cancel and start a fresh login; old QR links and challenge results cannot be reused. The app retries the pending login with the returned challenge result once. Provider restrictions may still reject a legitimate cross-device challenge.

No Discord developer application, manually copied account token, PC companion or password entry on this website is required. Private keys and login tickets stay in app memory. The signed-in access token remains in memory for channel/text requests; it is saved to SD only when you choose Remember login with PIN. Exiting clears in-memory credentials; logging out also deletes saved login.

## Remember login across launches

1. After signing in, return to **Home** and choose **Remember login with PIN**.
2. Choose an **eight-digit PIN**, then enter it again to confirm. Up/Down changes a digit, Left/Right moves between digits, and A advances/finishes. B or Plus cancels.
3. Check that Home says the login was saved. Close the app and reopen it.
4. Enter the PIN when prompted. DiscordNX2 decrypts the saved token and validates it by loading your account and servers. A successful restore needs no QR or CAPTCHA.
5. B skips startup unlock. **Unlock saved login** on Home lets you retry later.

The encrypted file is `/switch/DiscordNX2/session.bin` on the SD card. Its key is derived from your PIN with PBKDF2-HMAC-SHA256 (100,000 iterations); the credential uses AES-256-GCM with a random salt/IV. No decryption key or PIN is stored alongside it. This is PIN protection, not a console hardware keystore; someone with a copy of the SD file can try PIN guesses offline. Keep the file private and avoid an easily guessed PIN.

Wrong PINs and temporary network failures preserve the saved file for retry. A confirmed expired/revoked credential requires a new login. **Log out** clears the account and deletes the saved file. If you forget the PIN, use QR login again or delete this file. The PIN is local to DiscordNX2 and is not your Discord password. Existing v0.38 saved logins keep the same file format and PIN. Builds older than v0.38 never saved their token, so those users must log in once before choosing Remember login.

## Browse channels and use text chat

The interface has three areas: a narrow **server rail** on the left, the selected server's **channel sidebar** in the middle, and the **conversation** on the right. Tap the app icon at the top left for Home and saved-login/logout settings.

1. Tap a server icon, or focus the rail with Left, choose a server with Up/Down and press A. The rail downloads the server icons from Discord. Initials are a temporary/missing-image fallback; controller focus shows the full server name. Swipe the rail or use its arrow buttons for more servers.
2. Tap a text/announcement channel in the channel sidebar. Swipe the sidebar to scroll categories and channels. With a controller, move Right to channels, choose a row and press A.
3. New messages update automatically while the conversation is open. **Live updates** means Gateway Ready/Resume succeeded; message-create/update/delete events trigger history refresh. **Auto refresh (5 s)** means Gateway was unavailable and REST polling is active. A slow connection, rate limit or access failure can delay updates. A periodic resync also catches missed events. This is foreground-only, not a background plugin feature.
4. Swipe the conversation or use Up/Down to read recent messages. New messages follow the bottom only if you were already at the bottom; older reading keeps an ID anchor where possible. Tap a message, or press A in the conversation, for expanded reading. B closes expanded reading; live snapshots wait until you return so the message does not change underneath you.
5. Tap the **message box** at the bottom, or press X, to open the phone-style touch keyboard. **Done sends the draft** and closes the keyboard; the main Send button/R also sends. Incoming updates never send or overwrite it. A failed/unconfirmed send preserves it; check history before retrying.
6. Letter mode uses QWERTY rows. **?123** opens numbers/symbols, **#+=** switches symbol sets, and **ABC** returns to letters. Shift, Space, Delete and Done have touch buttons; controller equivalents are Y, X, L and R. **Done sends.** The smiley opens the media picker; The small **x**, B or Plus closes the keyboard and clears the entire draft, including a selected sticker. Backspace deletes a complete Unicode grapheme, including joined emoji or combining accents. Hold it (or controller L/A on Delete) to repeat after 400 ms, accelerating after 1.6 seconds. A blinking end cursor follows the visible end of long drafts.
7. Drafts hold up to 512 UTF-8 bytes. The keyboard has an English letter layout plus emoji selection, not a full multilingual input method. Automatic mention parsing is disabled. History remains limited to the latest 30 messages.

### Profiles, images and Unicode

Messages show available nicknames, author avatars (including server-specific avatars), default-avatar fallback and UTC times. Images load in the background without replacing the conversation. Tap a message for a larger preview. PNG/JPEG and GIF attachment or embed image/thumbnail previews use approved Discord, Tenor and Giphy media hosts only; no account credential is sent to image hosts.

Previews accept downloads below 1 MiB and sources up to 2048x2048 (4 megapixels), resized to at most 640x360 for stills. Discord attachment URLs request resized PNG previews; image attachments without MIME metadata can be recognized by filename. Images appear beneath captions with aspect ratio preserved; tap a message for the larger view. Small GIFs (up to 12 frames and 256x256 source) animate; longer/larger GIFs show the first frame. Four media previews are cached in memory. Only the first image per message is shown. Video, WebP, arbitrary website embeds, full-resolution viewing and GIF search are not implemented.

Unicode text is preserved in storage and messages. Display normalizes decomposed accents to NFC; wrapping, fitting and deletion respect grapheme boundaries. Font fallbacks cover many Unicode characters, but this renderer does not implement Arabic/Indic shaping, bidirectional layout or joined/color emoji ligatures. A grapheme staying intact does not guarantee its font can render it correctly.

### Voice connection test and remaining blocker

Select a voice channel and choose **Check voice connection**. This performs a brief muted/deafened Gateway and voice-WebSocket test, then attempts to leave automatically. It never opens the microphone or sends audio. The fixed-stage report is `/kaihen/discordnx2-voice.txt`; B cancels. A failed network connection can delay Discord observing the leave.

Working calls require an Opus codec and Discord's DAVE encryption engine. Discord documents E2EE-only voice since March 1, 2026 ([voice protocol](https://docs.discord.com/developers/topics/voice-connections)); [libdave](https://github.com/discord/libdave) brings MLS and crypto dependencies that are not yet ported into this NRO. The connection test honestly advertises no DAVE support and reports rejection code 4017 where applicable. Existing microphone/PCM tests do not supply this missing Discord voice transport.

### Emoji, server emoji and stickers

Tap the **smiley in the message box** (or the keyboard's smiley) to open the picker. Emoji contains common Unicode characters; Server contains the selected server's emoji; Stickers contains its sticker catalog. Swipe to page, or use controller directions/A with L/R to change tabs. Selections are staged in the input and never sent automatically. Tap **Send** to post. The small x in the input removes a staged sticker.

Server emoji use Discord's normal custom-emoji markup. Incoming custom emoji can render as downloaded PNG thumbnails. PNG/APNG stickers have static first-frame previews; Lottie/GIF sticker previews are not rendered. Server permissions, boost availability and account restrictions still apply. A failed send keeps the draft/sticker for review; there is no automatic retry.

Image downloads are restricted to fixed Discord CDN paths, use TLS without the account Authorization header, and are bounded before PNG decoding. A memory-only thumbnail cache is cleared on logout; no server art or messages are saved to SD. Animated server icons/emoji currently use PNG stills. The same icon can temporarily show initials if loading failed or no icon exists.

**GIF search, animated-media playback, general attachment previews and full Discord rich-text rendering are not implemented in this build.** There is no fake GIF-search control. These features require additional provider/media work.

Voice/stage selection displays the actual selected channel and disconnected state. **It does not join a voice call.** Categories are headings. Forums/media channels are listed but thread browsing, DMs, general attachment/embedded media, edits/deletes from this UI and live voice members remain unfinished. Discord permissions and rate limits apply.

## Touch and controller controls

| Input | Action |
| --- | --- |
| Tap server / channel | Select and open it. |
| Swipe server rail / channel sidebar / conversation | Scroll that area; a drag does not activate a row or Send. |
| Tap message box / X | Open the phone-style keyboard. |
| Tap Send / R / keyboard Done | Send the current nonempty draft. |
| Left / Right | Change focus between rail, channels, conversation and input. |
| Up / Down | Navigate or scroll the focused area. |
| A | Activate focused item; read a message; edit the input when input is focused. |
| Y | Manually refresh the conversation or channel list. |
| B | Close expanded reading or move focus back. |
| Plus / Exit | Close DiscordNX2. |

Touch was confirmed in v0.39. It uses a 1280x720 output range matching the framebuffer. The latest keyboard/picker targets and previews still need hardware verification; docked use retains controller navigation. PIN save/unlock also has touch buttons and digit selection.

Developer shortcuts remain on Home: **hold R + X** for the executable-memory check and **hold R + Y** for the Wi-Fi browser probe. Neither is needed for ordinary login. The Wi-Fi browser probe failed on the tested setup; the chat composer now uses its own phone-style keyboard.

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
| No voice audio or channel members | Only the explicit connection test is implemented; DAVE/Opus call audio is still missing. |
| Account/server name shows boxes | v0.39 embeds Unicode, Math, Symbols and monochrome Emoji fallbacks. Some combining sequences, scripts or emoji can still be unsupported. Share the character names/code points or an example display name, never login data. |
| Keyboard does not open | Tap the message box or press X for the phone-style keyboard; no system keyboard applet is needed. |
| Messages do not update immediately | Read the update status. Gateway fallback polls every five seconds; rate limits/network errors delay it. The live log records connection stages without message content. |
| Server order differs | The app uses flattened Discord folder order when settings are available. It indicates when API order is being used instead. |
| Saved login will not unlock | Check the PIN, network and final status. Retry from Home; if the token was revoked, use QR login again. |

After a failed login/keyboard test, copy **`/kaihen/discordnx2-hardware.txt`** from the SD card before running another test. Share the stage-only report, NRO version and phone error text. **Never share QR links, CAPTCHA results, login tickets, account tokens, passwords or private keys.** Do not post a QR screenshot containing an active login session. Channel/send/media jobs also checkpoint this report using operation/status fields only. For automatic-update problems, also copy `/kaihen/discordnx2-live.txt` after leaving the conversation or exiting the app. It contains only fixed connection state/counters/status codes.

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

Staged job/single-send isolation, safe image decoding and mobile keyboard tests run with `sh tests/run-chat-job-tests.sh`. Touch geometry, synthetic production live-worker events and actual embedded-font rasterization tests run with `sh tests/run-chat-tests.sh`. New client parsing, message serialization and cross-process session storage tests run with `sh tests/run-client-tests.sh` in Debian with a C compiler and mbedTLS development headers. They use synthetic credentials and temporary files.

The phone encryption integration test currently uses Windows Node.js plus a Debian WSL host harness with a C compiler and mbedTLS development headers. Prepare it with `wsl -d Debian -- sh /mnt/c/path/to/DiscordNX2/tests/run-phone-handoff-tests.sh`, then run `node tests/test_captcha_phone.js` from Windows. These use synthetic results and do not solve or submit a real CAPTCHA. Passing local tests does not prove Discord will accept login on hardware.

Thanks to the ouncebrew, kaiHEN, devkitPro and upstream projects referenced in the source for the homebrew tooling and libraries.

### Bundled font licenses

`app/assets/fonts/` contains unmodified Noto fonts from Google Fonts under SIL Open Font License 1.1. Each font ships with its OFL notice, and `sources.json` records the exact upstream commit, source URLs and SHA-256 hashes. The fonts are embedded in the NRO; users do not need to copy font files to the SD card. Console-provided fonts are not redistributed. Monochrome glyph support does not provide full emoji sequence rendering or complex-script shaping.

The PNG/JPEG/GIF decoder in `app/source/vendor/stb_image.h` retains its upstream public-domain/MIT license; `stb_image-source.txt` records the exact source revision.

Unicode segmentation and normalization use vendored utf8proc, pinned in `app/source/vendor/utf8proc/SOURCE.txt`, with its MIT license included. Run `sh tests/run-presentation-tests.sh` for grapheme/NFC, message metadata, URL validation, actual GIF decoding and synthetic production voice-signaling tests. These checks never contact a real account or start a call.

### v0.42 message delivery

Complete Gateway MESSAGE_CREATE events appear without a second REST request; deletes apply directly. Partial edits still fetch history, with the existing periodic resync and five-second polling fallback. Artwork is fetched one item at a time by the same network worker while retaining the Gateway connection; queued Gateway frames are handled first. Image requests have a 2.5-second cancellation deadline while live updates run, so a slow image can still briefly delay messages. Opening the phone keyboard keeps the live worker connected. Network/provider restrictions can still force polling; desktop-equivalent delivery is not guaranteed. Failed/unsupported previews say Image unavailable rather than loading forever. Y/manual history refresh clears failed-image entries so they can retry. New source changes need console verification.
