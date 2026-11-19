# How it works

*A plain-English, slightly technical look at DiscordNX2.*

DiscordNX2 is an **unofficial Discord client for Nintendo Switch 2 homebrew**. Instead of streaming a Discord window from your PC or sending everything through somebody else's DiscordNX2 server, the Switch runs its own interface and talks to Discord over the internet.

The goal is simple: sign in, read and send messages, join a voice channel, and eventually keep talking while playing a game. Some of that already works on hardware; other parts are still experimental.

## 1. How logging in works

**Short version:** Your phone says "yes, that's me," and Discord gives the Switch a digital key.

1. **The Switch creates a secure login session.** DiscordNX2 generates a temporary RSA-2048 key pair on the console and connects to Discord's remote-auth service using an encrypted connection (TLS and WebSocket).
2. **You scan the QR code.** The official Discord app on your phone recognizes the request and asks you to approve it. Your Discord password isn't typed into the Switch.
3. **Discord sends back a login ticket.** The Switch exchanges that ticket directly with Discord. The resulting account token is encrypted for the temporary key pair; DiscordNX2 decrypts it locally and checks that it can load your account.
4. **If Discord asks for a CAPTCHA, you finish it on your phone.** Because Nintendo's browser isn't working for this use yet, the project has a temporary HTTPS verification page. Your phone and Switch need to be on the same Wi-Fi for the encrypted CAPTCHA result to return to the Switch. The page is a handoff for the challenge, **not** a server that stores or relays your Discord account token.
5. **You're signed in only once the Switch finishes the exchange.** Approving the prompt on your phone is not, by itself, a successful login.

**What is a token?** Imagine a special key card for your Discord account. The app can use it to make authenticated requests without knowing your password. It's sensitive: anyone who gets a usable token may be able to access your account, so don't share it.

### Remembering your login

You can choose to save the token in `/switch/DiscordNX2/session.bin` and unlock it with a **4- or 8-digit PIN** later. The saved file uses **AES-256-GCM encryption**; the app derives its encryption key from your PIN using **PBKDF2-SHA256**. Neither the PIN nor that derived key is saved beside the file.

This is useful protection against casually reading the SD card, but it isn't magic: a short PIN can potentially be guessed offline by someone who copies the encrypted file. An eight-digit, hard-to-guess PIN is preferable. If the token expires or Discord revokes it, you'll need to sign in again. Logging out deletes the saved session.

## 2. How messages and servers work

After login, DiscordNX2 requests your server list, channels and messages from Discord's API over **HTTPS**.

- **HTTPS/REST requests** are like asking Discord for a specific thing: "Show my channels" or "Send this message."
- **WebSocket Gateway** is a connection Discord can use to tell the Switch when something happens, such as a new message arriving. If live updates aren't available, the app can check for updates periodically.

The Switch draws its **own native interface**. It's not a browser displaying discord.com, and it doesn't need a PC running in the background.

## 3. How voice chat works

**Short version:** Opus makes your voice small enough to send efficiently. DAVE makes it private.

When you select a voice channel, DiscordNX2 tells Discord you want to join. Discord supplies the voice-server connection information, and the app establishes its voice signaling and **UDP** audio transport.

Here's the outgoing path:

```text
Your microphone
    ↓
Raw audio captured on the Switch
    ↓
Opus: compresses the sound into small packets
    ↓
DAVE: encrypts the packets for the call
    ↓
UDP: delivers packets through Discord's voice infrastructure
    ↓
Your friends' Discord clients decrypt and play the sound
```

Incoming sound goes the opposite way: receive packets, decrypt them, decode the Opus audio, mix the voices, and send sound to the Switch's audio output.

### What are the technical pieces?

- **Opus** is the audio codec. It shrinks voice data while keeping it intelligible and responsive. It's about *audio quality, size and latency*, not privacy.
- **DAVE** means **Discord Audio & Video End-to-End Encryption**. It uses group key agreement (MLS) so participants can establish the keys needed to encrypt/decrypt call media. It's about *privacy*, not compression.
- **UDP** is the network transport for audio packets. It's designed to avoid holding up live conversation while waiting to retry old packets.
- **WebSockets** carry the control side: joining, voice state, key negotiation and other coordination. Think of this as the call's instructions, while UDP carries its audio.

DiscordNX2's code uses Discord's **libdave**, **MLS++**, **Opus** and cryptographic libraries rather than trying to invent its own voice encryption. It is designed **not to fall back to unencrypted voice** if secure key negotiation fails.

## 4. Does DiscordNX2 use somebody else's server?

**Not for normal Discord messages or voice.** The NRO connects to Discord's services directly. Voice packets pass through Discord's voice infrastructure, as they do for a normal Discord call.

There is a **temporary project-hosted web page for phone CAPTCHA verification** when Discord requires it. That page isn't a Discord message proxy, voice relay, or account database. Its availability isn't guaranteed, which is one reason we're investigating a working on-console browser.

## 5. Why we're investigating the browser

Today, logging in can require a phone, especially for CAPTCHA. We'd like the Switch to handle more of the login process itself.

The Switch 2 has Nintendo browser applets, but hardware testing on **firmware 23.0.1** showed a white browser screen with a **"This feature is not available"** message for several launch modes. That means the applet interface appears; it **does not** prove that arbitrary web pages can load. The standalone Hardware Tester is being used to narrow down why.

A working browser could improve login and verification. It **would not automatically solve voice chat**, since browser access and native audio/voice are separate systems.

## 6. What about Discord voice while playing another game?

The current foreground client is designed to let you stay in a call while moving between DiscordNX2's own chats and menus. **Background voice while a different Switch 2 game is running is a separate challenge.**

There's an **experimental resident NSO/plugin approach** for keeping voice alive outside the DiscordNX2 app. The code exists, but real game-time audio, microphone ownership and reliability have **not been verified on Switch 2 hardware**.

The Switch 2 **C button** currently brings up Nintendo GameChat's user selector in our tests. It isn't exposed as a normal button through the controller API we've tested, so C-button push-to-talk isn't implemented.

## 7. What actually works today?

| Feature | Status |
| --- | --- |
| QR login and account connection | **Confirmed on hardware** |
| Phone CAPTCHA handoff | **Confirmed in testing**, but may vary with Discord's challenge |
| Native server/message interface | Implemented; some features still incomplete |
| Voice channel signaling/join | Hardware channel join confirmed |
| DAVE + Opus voice send/receive | Implemented for testing; **two-way calls not yet confirmed on hardware** |
| Background voice during another game | Experimental plugin; **not verified** |
| Native browser for Discord login | **Not working yet** |
| C-button push-to-talk | **Not working yet** |

**Bottom line:** DiscordNX2 isn't just a Discord-themed menu: it makes real authenticated requests to Discord and contains a native encrypted voice pipeline. But it's also **work in progress**, and a successful login or voice-channel join isn't proof that every feature works end to end.

## Privacy and an important caveat

DiscordNX2 is **unofficial** and isn't affiliated with, endorsed by or supported by Discord or Nintendo. Using an unofficial client may carry account or compatibility risks.

Don't share screenshots of live login QR codes or any token, CAPTCHA result, PIN, password or secret. Diagnostic reports are intended to record technical stages and error codes, not your conversations or credentials.

For installation, builds, known limitations and detailed troubleshooting, see the main [README](README.md).
