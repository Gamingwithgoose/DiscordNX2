# Browser Research v0.1

Standalone Switch 2 browser investigation for the normal public kaiHEN loader.
This is an extraction/access test, **not a browser unlock or working browser**.
The DiscordNX2 app, voice engine, Hardware Tester and installed plugins are not changed.

## Run the next test

1. Pull the repo in GitHub Desktop and double-click `Browser Research/build-browser.bat`.
2. Copy `Browser Research/dist/Browser-Research.nro` to `/switch/` on the SD card.
3. Launch it through your existing homebrew menu. Press **X** to attempt export of the six browser components. **A** performs an inventory/access check without exporting their contents.
4. Wait for the menu to return. **B** cancels an export; **PLUS** saves the current results and exits. A cancelled export leaves any in-progress file with a `.part` suffix.
5. Copy the latest `report.txt` from `/switch/DiscordNX2-Browser/run-<number>/`. Share that report first. Preserve the whole run folder locally so successfully exported code/resources can be examined next.

Every run uses a fresh folder. A normal successful file copy is renamed from `.part` only after the exact byte count, output flush and final rename succeed. Completed file hashes are recorded. An access failure is a useful result; it is not evidence that the browser itself is absent or broken.

Only SD output is written. The program does not install firmware, replace an NSO, change DNS/settings, patch a process, launch a browser, access accounts, retrieve keys, or contact a network service. It targets only the six fixed browser title IDs below. The source code does not request any write operation on a source filesystem/storage.

## Why this is the next step

The supplied 23.0.1 dump contains encrypted archives for these components. Their readable metadata identifies targets, but does not expose the availability checks in executable code. This tool asks the running system to expose installed content through its normal filesystem interfaces. Whether the public loader permits that access needs a console test.

| Target | Title ID | Content type |
|---|---|---|
| LibAppletWeb / openweb | `0400000000001043` | Program |
| SystemWeb | `0400000000001042` | Program |
| LibAppletOff / offlineweb0 | `040000000000100F` | Program |
| LibAppletOff / offlineweb3 | `040000000000104B` | Program |
| BrowserDll | `0400000000000803` | Data |
| BrowserCoreDll | `040000000000085D` | Program |

Names come from [Switch 2 title research](https://www.switchbrew.org/wiki/Switch_2%3A_Title_list); they have not been read from decrypted binaries here. The content type for each matches the supplied dump's content metadata. The tool resolves the console's **currently installed** content rather than assuming the NCA filenames or version from the desktop dump.

The old WebApplet suffix `100A` is absent from that dump, but `openweb` at `1043` is present. Missing old IDs do not establish that the browser engine is missing. Applet enum values and installed title IDs are different identifier spaces; this tool does not alter either.

## Access routes and output

- NCM provides the latest installed metadata and registered content path from BuiltInSystem storage. These are read-only queries; no content registration, deletion or database commit is requested.
- For Program content, `fsldrOpenCodeFileSystem` attempts to expose ExeFS. Successful output goes under `<title-id>/exefs/`, including whatever executable/process-policy files the source filesystem provides.
- `fsOpenFileSystemWithId(ContentData)` attempts a browsable resource filesystem. Successful output goes under `<title-id>/romfs/`.
- BrowserDll also gets an independent `fsOpenDataStorageByDataId` attempt. If it works, `romfs.bin` is a decrypted raw filesystem image, **not an encrypted NCA**. This route can still provide useful output if NCM queries fail.

The [public kaiHEN documentation](https://github.com/ouncebrew/kaihen/blob/main/hen/README.md) describes loader/filesystem overrides. This tool uses those interfaces; it does not assume that homebrew has arbitrary system-process patch access. Source-filesystem opening may fail because of permissions, content layout or unsupported mount type. The report separates each stage so those failures are distinguishable.

Limits: 256 MiB per file/image; 512 MiB attempted export bytes per run; 4,096 directory entries; 12 directory levels; 64 KiB copy buffer. Directory entries are checked against traversal/separator/unterminated-name cases. Source sizes are checked before allocation/copy. Failed or cancelled output is retained only as `.part`. Existing run folders are never reused. The report is capped at 128 KiB and explicitly marks truncation if that limit is reached. It is checkpointed before operations and after files/targets; abrupt system termination may still interrupt the latest checkpoint.

## What a successful export would establish

It would establish readable access to particular installed files. It would **not** yet establish a working browser, an independent reusable engine, Discord web compatibility, or reduced account enforcement risk.

The next analysis will examine executable dependencies, process policy, browser resources and the failure paths producing the feature-unavailable response. We can then distinguish a launch/configuration problem from a check requiring a firmware-specific change. The existing Hardware Tester's missing-document offline probe is not a valid HTML rendering control; a valid document test remains a separate step.

## Validation

The NRO compiled and linked locally against the installed Switch 2 libounce/libkaihen toolchain with `-Wall -Wextra -Werror`. The portable production copy loop passed native host tests with AddressSanitizer and UndefinedBehaviorSanitizer for short reads, exact byte copies, limits before I/O, premature EOF, oversized read reports, source/output failure, cancellation, empty files and unsafe path components.

Run the small host test with a native compiler:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined test_copy.c -o /tmp/browser-copy-test
/tmp/browser-copy-test
```

**Console extraction and service compatibility are unverified.** No proprietary browser binaries are included in the source repo or the compiled research NRO. Keep local exported firmware files out of public commits.
