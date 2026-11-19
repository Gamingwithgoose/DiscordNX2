# NS2 Hardware Tester (standalone)

Run `build-test.bat` on your existing Windows devkitPro/ounce-devkit setup.
Output: `dist/Hardware-Tester.nro`. Copy only this NRO to the Switch 2,
e.g. `/switch/Hardware-Tester.nro`. Launch using the normal public kaiHEN homebrew menu.

No changes to DiscordNX2 NRO, voice NSO, existing saved accounts, browser data,
loader or firmware. No Discord login, capture of audio or networking.

## Controls
- X opens **automatic input monitor**. No separate raw capture button.
  Press C/GameChat and normal buttons; changes auto-save to SD after input events.
  B returns to browser menu.
- UP/DOWN selects Web Page, News Page, Offline HTML or Wi-Fi portal.
  A attempts the selected system applet. Close any launched applet normally.
- Press PLUS alone to save and exit; Y also saves without exiting.
- Output: SD `/switch/Hardware-Tester-report.txt` (fallback: `/Hardware-Tester-report.txt`). The tester displays SD and save result codes onscreen.

## Interpreting results
The monitor observes libounce's **standard 64-bit Npad button mask** for
the active default controller, not physical USB/Bluetooth reports or the
private HID bus. Nintendo's C button is not a documented libnx Npad bit.
No change while C is pressed means this access path did not observe it,
not that a privileged service could never observe it.

Web Page and News applets may require a full Application context and/or
the host game's whitelist. Offline HTML intentionally has no bundled
HtmlDocument NCA: failure can indicate absent host content rather than
the applet itself being unavailable. Wi-Fi portal uses the established
DiscordNX2 probe configuration with no network-profile mutation.

Browser launch functions may block until the applet closes. A system
applet could fail, present an error UI, or temporarily take focus.
Run these tests one at a time. Report both the screen behavior and
the numeric error values. Do not interpret zero as browser support
until a page visibly renders.

This v0.1 intentionally does **not** enumerate private services, read
unmapped memory, intercept system controls, inspect firmware or
install a resident plugin. Those investigations require a separate
permission and hardware study.

### Build failure?
Copy the first compiler/linker error and the output of `build-test.bat`.
The code has not been compiled in a local ounce-devkit environment yet.
