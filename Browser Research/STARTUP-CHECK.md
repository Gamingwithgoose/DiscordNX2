# Browser Startup Check v0.1

There is now a focused console test. It does not unlock the browser or change system settings.

## Run it

1. Transfer **Browser-Startup-Check.nro** to `/switch/` on the SD card.
2. Start it through your usual public homebrew setup.
3. Press **X** once. It reads the candidate startup setting, saves a report, and asks the normal browser applet to open `https://example.com/`.
4. Close the browser page or any error dialog normally to return. Press **PLUS** to exit the test.
5. Retrieve the new `/switch/DiscordNX2-Browser/startup-....txt` file. Report whether an actual example.com page appeared or only an error dialog.

**A** runs only the settings read, without launching the browser. No firmware, NSO or jailbreak replacement is needed. No Discord login is involved. The special settings read only runs after physical firmware is verified as **23.0.1**. If the report says the query failed or was skipped, that is useful evidence; do not change console settings to force it to succeed.

## Why this test is different

The exported openweb executable contains three code locations that pass the startup-unavailable dialog factory as an authenticated callback. One path checks a byte in a global object's state before selecting that dialog. Its initialization traces to **set:sys command 265**, with no input and an inline **48-byte output**. The state byte is initialized as `1 & ~response[0]`.

That establishes a concrete read-only query worth testing. It does **not** establish the setting's public name, whether it is a browser-wide lock, or whether this is the path taken by the failing live launch. The diagnostic logs all 48 bytes, the first bit, physical/SDK firmware versions, and normal applet results. A successful launch API result alone does not demonstrate rendering; the visible outcome matters.

The same normal example.com launch has been tested before. Its purpose here is to pair that outcome with the newly identified setting in one report, not to suggest a new launch method has already been found.

## Trace details

Offsets below are relative to the decompressed `0400000000001043` main module from the supplied export, not runtime process addresses. They are build-specific.

| Evidence | Offset / value |
|---|---|
| Unavailable-dialog factory | `0x7bac60` |
| Authenticated callback address materializations | `0x7981fc`, `0x7eaf8c`, `0x82d72c` |
| State read immediately before third callback | byte 50 of object returned by `0x4ef38`; compared with 1 |
| State initialization | `0x7b58a4` calls `0x6a0710`; `0x7b58b8` stores inverted first bit at object+50 |
| Settings wrapper | `0x6a0710` -> `0x6a10c0` |
| Service acquisition | `0x6a1ffc` -> string `set:sys` at `0xaa8afc` |
| Virtual method slot | vtable `0xc689a0` + `0x5a8`, relocation target `0x6a2e00` |
| IPC wrapper | command`0x109` /265 at `0x6a2e04` |
| Inline output copy | `0x4aa778` through `0x4aa78c`: six 64-bit words /48 bytes |

Other paths remain to be explained. The first callback path checks object state through `0x74d2bc` / `0x7b6a40`; the second appears within an event dispatcher. Finding these paths corrects the earlier incomplete search for direct branch calls: callbacks are materialized with `ADR` and signed with pointer authentication before indirect use.

## Validation and limits

The new test is independent of the content exporter and Discord app. It builds with `-Wall -Wextra -Werror`, links without unresolved symbols, and writes only fresh small SD reports. Source review confirms the only direct settings command is the observed read command 265; no setter or patch is included. It has not yet been run on the console.

An error can mean the calling homebrew context cannot make the same request as the native applet. A returned first bit is evidence for comparing hypotheses, not permission to name or change an unknown setting. This test cannot establish Discord web compatibility or lower account-enforcement risk by itself.
