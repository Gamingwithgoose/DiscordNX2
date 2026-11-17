# DiscordNX2 — Real account sign-in (PC)

This uses **official Discord OAuth2 authorization code with PKCE**.
It is real browser-based sign-in but not yet a Discord voice connection.

## Setup once

1. Go to https://discord.com/developers/applications and create an application (e.g. DiscordNX2).
2. In **OAuth2**, enable **Public Client**.
3. Add this exact Redirect URI: `http://127.0.0.1:8765/discordnx2/callback`.
4. Copy the public **Application ID**. Do not share your client secret, password, or tokens.

## Test

Double-click `Discord-Login.bat` on your Windows PC. Python 3 must be installed.
Paste the public Application ID. Your normal browser opens Discord's authorization
page for **identify** and **guilds** only. After approval, the local program displays
your username and a list of server names. The callback listens on `127.0.0.1`
only. OAuth code verification uses PKCE and a random state. The access token is
used in RAM for two HTTPS API calls, then discarded. No tokens or secrets are
written to disk, console logs, GitHub, or the Switch 2.

If there's an error, share only the plain error message—never the callback URL
with its code, any token, or the full authorization link.

**Limit:** This does NOT connect your user account to a voice channel. OAuth
identify/guilds is not voice access. Discord's user `voice` scope requires
Discord approval; bot voice integration is separate and requires DAVE.

No NRO/NSO update or jailbreak restart is necessary for this PC sign-in test.
Offline tests: `python -m unittest discover -s tests -p test_discord_oauth.py`.
