# DiscordNX2

> **INCOMPLETE FUNCTIONALITY AT THIS TIME.** DiscordNX2 is experimental Switch 2 homebrew under active development. Account connection has been confirmed on hardware. Touch has been confirmed on hardware. The latest in-place requests, server icons, redesigned keyboard and media picker still need hardware testing. **Encrypted voice calling is implemented for foreground testing, but has not yet been confirmed on Switch hardware. Voice member lists and concurrent text browsing are implemented for testing; an optional resident voice build is ready for background-game testing. Full official-client parity is not available.** Phone approval or a completed CAPTCHA does not by itself mean the Switch has logged in.

DiscordNX2 aims to bring Discord voice chat to Switch 2. The current v0.49 source adds larger voice names, real member avatars, selectable4/8 digit PINs and optional resident-call ownership, alongside native mic/folder icons and normal empty-channel joining, and retains the v0.47 fixes for native audio buffer delivery and playback pacing, adds physical microphone mute and voice members, and keeps DAVE/Opus voice connected while browsing/text chatting, alongside reply context, author profile cards, server nickname/role-color lookups, guild tags and resolved mentions, basic inline text styles, and existing direct/group-DM conversations. Image-preview requests now preserve source proportions instead of forcing portrait media into landscape dimensions. Tapping the message box opens the phone-style keyboard directly. QR/CAPTCHA account connection, touch and PIN-protected saved login remain in place. This is an unofficial project, not affiliated with Discord or Nintendo.

## Current functionality

| Area | Current status |
| --- | --- |
| Interface | Real server-icon rail, category/channel sidebar and conversation with persistent input. Channel changes, sends, refreshes and image loading use in-place background jobs rather than the login/loading screen. Neutral gray styling, touch and controller navigation. |
| Native QR login | User confirmed successful account connection after the project-hosted phone CAPTCHA handoff in v0.37. |
| Phone CAPTCHA | User completes hCaptcha on the project website; an encrypted result returns directly to the Switch over the same Wi-Fi. User confirmed account connection; individual future attempts may still be rejected or expire. |
| Servers | Loads up to 200 real servers. Reads Discord folder/order settings and flattens folders in order; falls back to API order if unavailable. |
| Channels / text | Text/announcement channels and existing DMs show the latest 30 messages. A foreground Gateway worker applies complete incoming messages and deletions directly, and refreshes history for partial edits or resync; five-second automatic polling is the fallback. Tap input to open the compact full-width phone-style QWERTY layout with separate symbol pages. |
| Voice chat | Join / Disconnect / Mute with DAVE/Opus, a live member list and persistent call panel while browsing/chatting. Hardware channel join is confirmed; two-way audio remains unconfirmed. Stage speaking remains unfinished. Optional resident call ownership is implemented; game-time audio is unverified. |
| Emoji / stickers | Common Unicode emoji and current-server emoji/sticker picker. Emoji PNG and PNG/APNG sticker first-frame previews; explicit Send for drafts/stickers. Posted PNG/JPEG/GIF images and embed image/thumbnail previews are supported within limits below. GIF search and full rich media remain unfinished. |
| Saved login | Optional four- or eight-digit PIN encrypts credentials on SD. Enter PIN on future launches instead of scanning QR; expiry/revocation may require login again. New in v0.38. |
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
4. Run **Build-NRO.bat**. It checks for the maintained `switch-mbedtls` crypto library and attempts to install it through devkitPro's package manager if missing. Internet is needed for that first dependency installation. Voice additionally needs MSYS2 `cmake`, `ninja`, `perl`, `make` and `curl`. The first build downloads hash-checked, pinned source archives and compiles the static voice libraries into `.local/voice/`; this takes several minutes. Subsequent builds reuse that cache. If a build reports a missing tool, install it with devkitPro MSYS2 `pacman -S --needed cmake ninja perl make curl`, then retry. It does not build a browser or replace global crypto libraries.
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
2. Choose **4 digits** (default for a new save) or **8 digits**, then enter the PIN twice to confirm. Touch the length selector or press Y to switch. Up/Down changes a digit, Left/Right moves between digits, and A advances/finishes. B or Plus cancels.
3. Check that Home says the login was saved. Close the app and reopen it.
4. Enter the PIN when prompted. DiscordNX2 decrypts the saved token and validates it by loading your account and servers. A successful restore needs no QR or CAPTCHA.
5. B skips startup unlock. **Unlock saved login** on Home lets you retry later.

The encrypted file is `/switch/DiscordNX2/session.bin` on the SD card. Its key is derived from your PIN with PBKDF2-HMAC-SHA256 (100,000 iterations); the credential uses AES-256-GCM with a random salt/IV. No decryption key or PIN is stored alongside it. This is PIN protection, not a console hardware keystore; someone with a copy of the SD file can try PIN guesses offline. Keep the file private and avoid an easily guessed PIN.

Wrong PINs and temporary network failures preserve the saved file for retry. A confirmed expired/revoked credential requires a new login. **Log out** clears the account and deletes the saved file. If you forget the PIN, use QR login again or delete this file. The PIN is local to DiscordNX2 and is not your Discord password. Existing eight-digit saved logins still unlock with their original PIN. Choosing Remember login again writes the new format with the selected length; older NROs cannot read that new format. Builds older than v0.38 never saved their token, so those users must log in once before choosing Remember login.

## Browse channels and use text chat

The interface has three areas: a narrow **server rail** on the left, the selected server's **channel sidebar** in the middle, and the **conversation** on the right. Tap the app icon at the top left for direct messages when signed in. Tap your account name at the bottom of the sidebar for Home and saved-login/logout settings.

1. Tap a server icon, or focus the rail with Left, choose a server with Up/Down and press A. The rail downloads the server icons from Discord. Initials are a temporary/missing-image fallback; controller focus shows the full server name. Swipe the rail or use its arrow buttons for more servers.
2. Tap a text/announcement channel or a DM in the channel sidebar. Swipe the sidebar to scroll categories and channels. With a controller, move Right to channels, choose a row and press A.
3. New messages update automatically while the conversation is open. **Live updates** means Gateway Ready/Resume succeeded; message-create/update/delete events trigger history refresh. **Auto refresh (5 s)** means Gateway was unavailable and REST polling is active. A slow connection, rate limit or access failure can delay updates. A periodic resync also catches missed events. This is foreground-only, not a background plugin feature.
4. Swipe the conversation or use Up/Down to read recent messages. New messages follow the bottom only if you were already at the bottom; older reading keeps an ID anchor where possible. Tap a message, or press A in the conversation, for expanded reading. B closes expanded reading; live snapshots wait until you return so the message does not change underneath you.
5. Tap the **message box** at the bottom, or press X, to open the phone-style touch keyboard. **Done sends the draft** and closes the keyboard; the main Send button/R also sends. Incoming updates never send or overwrite it. A failed/unconfirmed send preserves it; check history before retrying.
6. Letter mode uses QWERTY rows. **?123** opens numbers/symbols, **#+=** switches symbol sets, and **ABC** returns to letters. Shift, Space, Delete and Done have touch buttons; controller equivalents are Y, X, L and R. **Done sends.** The smiley opens the media picker; The small **x**, B or Plus closes the keyboard and clears the entire draft, including a selected sticker. Backspace deletes a complete Unicode grapheme, including joined emoji or combining accents. Hold it (or controller L/A on Delete) to repeat after 400 ms, accelerating after 1.6 seconds. A blinking end cursor follows the visible end of long drafts.
7. Drafts hold up to 512 UTF-8 bytes. The keyboard has an English letter layout plus emoji selection, automatic sentence capitals, double-space punctuation (within 400 ms), and a small offline English suggestion bar. Tap a suggestion to replace the current word prefix or insert a common next word; suggestions never auto-replace text or send messages. This is not a full multilingual input method. Automatic mention parsing is disabled. History remains limited to the latest 30 messages.

### Profiles, images and Unicode

Messages show available nicknames, author avatars (including server-specific avatars), default-avatar fallback and UTC times. Images load in the background without replacing the conversation. Tap a message for a larger preview. PNG/JPEG and GIF attachment or embed image/thumbnail previews use approved Discord, Tenor and Giphy media hosts only; no account credential is sent to image hosts.

Previews accept downloads below 1 MiB and sources up to 2048x2048 (4 megapixels), resized to at most 640x360 for stills. Discord attachment URLs request resized PNG previews; image attachments without MIME metadata can be recognized by filename. Images appear beneath captions with aspect ratio preserved; tap a message for the larger view. Actual GIF files animate within a 240-source-frame and 32-million decoded-pixel work limit. Previews retain at most 60 sampled frames, 256x256 pixels each, and 4 MiB per animation; skipped frames retain their combined timing. Larger/work-heavy GIFs use a still preview. Tenor/Giphy posts delivered as MP4/WebM (often labeled GIF by Discord) still use thumbnails because video decoding is not implemented. Four media previews are cached in memory. Only the first image per message is shown. Video, WebP, arbitrary website embeds, full-resolution viewing and GIF search are not implemented.

Unicode text is preserved in storage and messages. Display normalizes decomposed accents to NFC; wrapping, fitting and deletion respect grapheme boundaries. Font fallbacks cover many Unicode characters, but this renderer does not implement Arabic/Indic shaping, bidirectional layout or joined/color emoji ligatures. A grapheme staying intact does not guarantee its font can render it correctly.

### Test encrypted voice (v0.49)

1. Use a normal **voice** channel you can join and speak in, including an empty channel. You can stay joined, use the microphone locally, mute/unmute and browse chat without another participant. The local member indicator responds to captured microphone activity. To test actual two-way speech, have another person join later using official Discord.
2. Select **Join voice**. After Discord confirms membership and UDP transport, the app starts native microphone/playback and applies the selected mute state. It stays connected in an empty channel without a waiting-for-others screen or timeout. The encryptor emits microphone speech only after an executed MLS sending key exists; local capture and connection state are independent of that key. Shared encrypted audio starts automatically as participants join and negotiate keys. It never sends unencrypted speech.
3. Once **Connected** appears, test speech in both directions. Tap **Mute microphone** or press **X** to mute/unmute. Headphones help avoid speaker feedback; echo cancellation is not implemented.
4. Browsing another server, a text channel or DMs, typing, and sending messages keep the existing call connected. The compact panel at the bottom of the channel sidebar shows the active voice channel. Tap its name to return to call controls, its microphone button to mute/unmute, or **x** to disconnect. On the voice screen, tap **Disconnect**, or use Up/Down and A to select Disconnect/Mute. **B navigates back without disconnecting.** Explicitly selecting another voice channel moves the call; restoring a remembered channel while browsing never moves it automatically. Logout ends the call. Exiting ends a local call; after an explicit background handoff, exiting only detaches the app. The existing probe-only NSO cannot own a call. See the optional background test below.
5. If joining fails or audio is silent, share `/kaihen/discordnx2-voice.txt`. It records stage, close code and packet/key-transition counts, without account/channel IDs or credentials. It now includes actual capture buffers/sample counts and peak level, native mute/capture state, encoded/sent/received packet counts, speaker mappings, decrypt/Opus rejection counts, playback buffers/peak and audio service errors. Levels are numeric only; audio is never saved. Stage5 means UDP discovery; stage6 means protocol/MLS setup; stage7 means local audio started. Stage7 alone does not prove the other person heard audio. In an empty channel, `secure=0`, `tx=0` and `mls=0` are expected while native capture and microphone levels still work. Connected means confirmed membership/transport and local audio readiness, not verified two-way speech.

The engine uses pinned [Discord libdave](https://github.com/discord/libdave), MLS++, OpenSSL 3 and Opus, with Monocypher for XChaCha20-Poly1305. Both advertised transport modes are authenticated; there is no legacy/unencrypted fallback. Group membership comes from authenticated Discord voice signaling. Keys, audio buffers and voice credentials are cleared on disconnect. There is no new browser, relay, DNS setting, NSO or jailbreak loader requirement.

This is an initial in-app implementation: up to32 remote participants, bounded PCM queues and simple mixing; adaptive jitter recovery, echo cancellation, automatic call resumption, Stage speaker approval remain unfinished. Optional background ownership is implemented but game-time device access/audio remains unverified. Network/provider acceptance and audio timing must still be tested on hardware. A 4017 response means Discord did not accept the advertised DAVE negotiation; send the updated report rather than assuming success.

### Emoji, server emoji and stickers

Tap the **smiley in the message box** (or the keyboard's smiley) to open the picker. Emoji contains common Unicode characters; Server contains the selected server's emoji; Stickers contains its sticker catalog. Swipe to page, or use controller directions/A with L/R to change tabs. Selections are staged in the input and never sent automatically. Tap **Send** to post. The small x in the input removes a staged sticker.

Server emoji use Discord's normal custom-emoji markup. Incoming custom emoji can render as downloaded PNG thumbnails. PNG/APNG stickers have static first-frame previews; Lottie/GIF sticker previews are not rendered. Server permissions, boost availability and account restrictions still apply. A failed send keeps the draft/sticker for review; there is no automatic retry.

Image downloads are restricted to fixed Discord CDN paths, use TLS without the account Authorization header, and are bounded before PNG decoding. A memory-only thumbnail cache is cleared on logout; no server art or messages are saved to SD. Animated server icons/emoji currently use PNG stills. The same icon can temporarily show initials if loading failed or no icon exists.

**GIF search, animated-media playback, general attachment previews and full Discord rich-text rendering are not implemented in this build.** There is no fake GIF-search control. These features require additional provider/media work.

Voice selection opens the foreground call controls described above. The member list uses authenticated voice join/leave/speaking and guild voice-state updates, with nickname/name resolution through Gateway member metadata. A temporary Member/ID fallback appears until a name is available. Stage speaker approval remains unfinished. Categories are headings; forum thread browsing and message editing from this UI remain unfinished. Discord permissions and rate limits apply.

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

### v0.43 replies, profiles, names and direct messages

- Tap the app icon to load existing direct/group-DM conversations, then select one to read/send with the same keyboard and foreground live-update path. Y refreshes the conversation/list. Tap your account name in the sidebar footer to return to account settings. Starting new DMs, friend management, DM calls and older-history pagination are not implemented.
- Replies show the referenced author and a snippet above the message. Deleted originals are labeled. Tapping a reply header opens the original only if it is among the latest 30 loaded messages; fetching older originals is unfinished.
- Tap an author's avatar/name to open a profile card; tap its x, outside it, or B to close. Controller L opens the focused message author's card. The card shows available identity, avatar, username, tag, server roles and join date. It is a read-only card, without full official profile actions, bio/presence, Nitro effects or friend controls.
- Read-only guild-role/member requests supply server nicknames and the highest primary role color, including when message history omitted member details. Requests run between Gateway receives using the existing network worker. Loading can be delayed or denied by Discord; unavailable details use the original message identity. Y retries failed member/artwork entries. Role gradients, nameplate effects and avatar decorations are not implemented.
- Guild tags/badges and APP labels render when the actual author payload supplies them. User/role/channel mention tokens resolve to available names. Basic bold, italic, underline, strikethrough and inline code are supported; full Discord Markdown, clickable mentions, reactions and rich embeds remain unfinished.
- Image URLs retain signed parameters, replace old resize/format settings, and request source-proportional dimensions (or one axis when dimensions are missing). This addresses the prior landscape transformation that could crop portrait attachments. Hardware verification is still required, including the reported GIF.
- Voice connection checks wait out the existing Gateway Identify cooldown with a visible countdown rather than returning the misleading session-not-ready error. B cancels. Other failures report fixed stage/code information; `/kaihen/discordnx2-voice.txt` remains the stage-only report. This does not add DAVE/Opus audio.

Object fields and read-only endpoints follow Discord's [message](https://docs.discord.com/developers/resources/message), [guild/member](https://docs.discord.com/developers/resources/guild), [user](https://docs.discord.com/developers/resources/user) and [CDN](https://docs.discord.com/developers/reference#image-formatting) references. Compatibility with this unofficial user client remains experimental; passing synthetic tests does not establish provider acceptance on hardware.

### v0.44 channel memory, DM artwork and troubleshooting

Each account remembers its last selected channel separately for each server and its last DM. Returning to that server/list reopens the saved selection if the channel still exists. Merely selecting a voice channel does not join a call. Choices survive closure in `/switch/DiscordNX2/channels-<account-id>.txt`; this file contains only guild/channel IDs, not login credentials or messages. It is separate from PIN-encrypted login storage. If SD writing fails, choices still work during the current run.

DM rows now request recipient avatars/default avatars and actual group-DM icons. The bundled OFL Noto Sans Symbols font adds enclosed-letter coverage, including `🅜🅞🅓🅓🅔🅡`; its pinned revision and SHA-256 are in the font manifest. It does not make font coverage universal.

For stage-1 voice failures, the client now retains Discord's validated `resume_gateway_url` host and uses it when resuming, as required by the [Gateway protocol](https://docs.discord.com/developers/events/gateway#resuming). The uncompressed WebSocket bound is now 4 MiB instead of 1 MiB to accommodate larger Ready payloads. A session that ends before Ready is cleared so the next attempt can identify again, subject to the existing cooldown. The voice report adds fixed Hello/Identify/Ready and receive-result fields; it never logs session/token/host or message data. These changes address plausible causes of stage 1 / code 0, but do not establish the exact hardware cause or enable voice audio. If it still fails, share the updated `/kaihen/discordnx2-voice.txt` stage report.

### v0.45 voice confirmation

The supplied v0.44 report reached Gateway Ready and sent a join request, then timed out at stage 2. That report did not say which confirmation event was missing; it did not establish a permission problem or a successful voice join.

This revision subscribes to the selected guild before the muted/deafened join request, accepts an omitted guild ID on the voice-state object only when both the account and selected channel match, and supports valid non-default numeric endpoint ports over verified TLS. The legacy `:80` endpoint suffix maps to TLS443; there is no plaintext fallback. The confirmation wait begins after sending the join request. Voice-server credentials always require the exact selected guild.

The report now distinguishes state/server events seen and matched, null allocation endpoints, rejected endpoints, disconnected self state and whether the leave request was sent. No identifying payloads or credentials are logged. If the check still stops, share the updated `/kaihen/discordnx2-voice.txt`.

The screen now states the limitation briefly: voice calls are not supported in this version. The button still checks signaling and leaves. DAVE/Opus call audio remains unimplemented. The [voice connection protocol](https://docs.discord.com/developers/topics/voice-connections#retrieving-voice-server-information) requires both confirmation events, and the [voice-state object](https://docs.discord.com/developers/resources/voice#voice-state-object) marks guild ID optional. Runtime acceptance in this unofficial client remains unverified for these changes.


### v0.46 validation

DAVE MLS group creation/commit/welcome, explicit transition gating, stereo Opus encode/decode, both AEAD transport modes, authenticated encrypted audio round-trips, tamper/replay rejection and member removal have host tests in `tests/run-voice-tests.sh` (host OpenSSL3 development files required). The NRO links with the Switch compiler. These checks establish implementation/build correctness, not a successful live Discord call. Voice dependency notices are in `licenses/voice/` and embedded in the NRO.


### v0.47 follow-up

The user confirmed channel join and a Connected status in v0.46, but reported silence in both directions and unresponsive mute. This update drains the actual released native audio buffers instead of guessing completion with ContainsBuffer, honors the returned capture byte count, fixes unsigned playback-deadline underflow, stops native capture while muted and gates every encoded frame against the current mute request. It also installs receive keys for late authenticated speaker mappings without resetting existing replay state.

The call now owns a separate worker, Gateway session, WebSocket readers and cancellation state. Chat jobs and live updates retain their own owner. Reference-counted socket/TLS service lifetimes and a three-session TLS pool keep a text request from shutting down or monopolizing the active call's services. The update reuses the existing pinned voice libraries; there is no dependency, NSO or loader rebuild requirement.

Audio queue/mute, member events, independent call snapshots, shared service lifetimes, late speaker mappings, replay rejection, existing chat/auth regressions and the NRO build were checked locally. These tests do not confirm two-way audio or concurrent-call behavior on the console; test those together and send the updated voice report if silent.


### v0.48 follow-up

The supplied v0.47 report reached voice stage6 with transport ready, no remote peers or shared keys, and no started audio devices. The user confirmed being alone in the channel. That report establishes a real voice-server/UDP connection, but not audio delivery; no microphone samples were captured. The previous requirement for a shared MLS sending key before starting local audio was inappropriate for an empty channel.

Empty channels now remain joined indefinitely, start local capture/playback, show actual microphone activity and apply native mute without waiting for another participant. Raw microphone speech is still gated by the DAVE sending key, and a second participant triggers the ordinary secure group exchange automatically. No dependency, NSO or loader changes are needed. The report adds fixed counts for key packages, external-sender messages, proposals, commits, welcomes and execute transitions to distinguish ordinary empty-channel state from failed multi-party negotiation.

The sidebar microphone button and voice channel rows now draw native microphone icons, with a slash for mute. Forum/media rows draw a native folder icon. These do not depend on Unicode emoji/font coverage. Local empty-channel policy, mute/capture and encrypted speech gating were tested; real two-way audio remains unconfirmed on hardware.


### v0.49: voice avatars, PIN choice and optional background test

Voice member names are larger. Their real Discord avatar (server avatar when supplied, then account/default avatar) replaces the placeholder. A thin green outline appears while speaking; local speaking uses captured microphone activity and respects mute. Avatar downloads use the existing unauthenticated CDN/cache worker.

The normal update remains **GitHub Desktop → Build-NRO.bat → FTP dist/DiscordNX2.nro**. This covers the member UI and PIN changes without replacing the installed plugin. Choose Remember login again to change an existing eight-digit PIN to four digits. Your current save remains readable until then.

Background voice needs a separate resident process: an NRO cannot run its own call after closing. The optional build uses the [public kaiHEN plugin interface](https://github.com/ouncebrew/kaihen/blob/main/libkaihen/PLUGINS.md), without a custom loader, relay or DNS change. This is implemented for testing, **not yet confirmed to capture/play audio while a retail game is running**. Audio resource sharing, game performance and stability need hardware verification.

1. Build the matching NRO normally. Run **Build-Resident-Voice.bat** to produce `dist/resident-voice/discordnx2.nso`, reusing cached voice dependencies. The ordinary diagnostic plugin build is preserved separately.
2. Back up your installed `/kaihen/plugins/discordnx2.nso`. Replace that file with the optional output under the same name; do not install both copies. Activate it at the next normal public jailbreak/kaiHEN startup. There is no hot reload or special payload.
3. Open the updated NRO, join a voice channel, and tap **Y: Background** (or press Y in call controls). The call reconnects once under the resident owner (the existing Gateway cooldown can add up to60 seconds, shown as a countdown); the status changes to **Background**. If the button is absent, that plugin is unavailable and the foreground call continues normally.
4. Exit the NRO normally and launch a game. With another participant present, test receive/transmit, then mute/unmute after reopening DiscordNX2. A still-active call should reappear automatically. Explicit Disconnect or Log out ends it; ordinary server/text/DM browsing does not. A system restart also ends it; saved login never auto-joins a call at boot.
5. If it fails, reopen DiscordNX2 and copy `/kaihen/discordnx2-voice.txt` before retrying. Note whether the failure happened before exit, on game launch, or during gameplay. To revert, restore the backed-up probe plugin and use the next normal startup; the updated NRO still supports local voice.

Only an explicit background handoff transfers the credentials to the resident process, through bounded kernel IPC. Credentials stay in RAM and are wiped on completion/disconnect; voice audio is never saved. The resident build reserves a 64 MiB heap and initializes call networking/audio only when requested. Its legacy probe commands remain compatible. Closing the NRO deliberately leaves the background microphone at the selected mute state until you disconnect or the session ends.
