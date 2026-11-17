# DiscordNX2

I want Discord voice chat on my Switch 2 while playing games, with the C button as push-to-talk. This is the first hardware test, **not a working Discord voice client**.

Right now the project builds a homebrew NRO plus a kaiHEN background NSO. The NRO asks the background plugin for a known test response and saves the result to the SD card. Before attempting Discord, I need to prove that a plugin can stay running while a game is open, that C can be read during gameplay, and that microphone and audio playback work at the same time as a game.

## Building on your own PC

There are no GitHub Actions, cloud builds, or Docker requirements here. This project uses devkitPro and ounce-devkit installed locally. Regular MSYS2 by itself is not the Switch 2 cross-compiler.

If you already have devkitPro installed but `Build.bat` complains that the Switch 2 toolchain is missing, double-click **Setup-Toolchain.bat** first. It asks for your permission and then runs ounce-devkit's official installer. That installer adds its package signing key/repository to devkitPro and installs the packages. It needs internet access for setup, but all actual compilation stays on your PC.

If you don't have devkitPro, install it first using https://devkitpro.org/wiki/Getting_Started. You can also install ounce-devkit manually using https://github.com/ouncebrew/ounce-devkit.

Once the tools are installed:

1. Clone this repo or pull the latest changes in GitHub Desktop.
2. Double-click `Build.bat`.
3. Find `DiscordNX2.nro` and `discordnx2.nso` in the `dist` folder.

If devkitPro lives somewhere other than `C:\devkitPro`, edit the `BASH` path near the top of the batch files. You can also build from devkitPro MSYS2 with `./build-local.sh`.

## Testing on the Switch 2

Copy `dist/DiscordNX2.nro` to your usual homebrew directory. **Also copy** `dist/discordnx2.nso` to `/kaihen/plugins/discordnx2.nso`. Restart kaiHEN so it can start the plugin, then launch the NRO from the homebrew menu.

Read `/kaihen/discordnx2-probe.txt` on the SD card. A successful exchange should say `IPC result: 00000000` and `Service reply: 444e5832`. If it fails, check `/kaihen/plugin-status.txt`. The NRO has no graphical interface yet; it simply writes its diagnostic report and exits.

You can remove the plugin by deleting `/kaihen/plugins/discordnx2.nso` and restarting kaiHEN. The test does not access Discord, capture audio, intercept input, or intentionally write to NAND. kaiHEN is experimental, so use it with care.

## What I want to tackle next

Background persistence during games comes first. C-button access, microphone capture, mixing incoming chat with game audio, and a supported Discord voice integration come later. I won't claim those features work until they've been tested on the actual console.

The build structure and IPC handshake follow the MIT-licensed examples from ouncebrew/ounce-devkit. Thanks to the kaiHEN and ouncebrew teams for their work.
