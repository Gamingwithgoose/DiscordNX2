# Browser Research v0.2

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


## v0.1 hardware result and v0.2 follow-up

The supplied 23.0.1 reports confirm a successful BrowserDll raw data export: 16,652,528 bytes, SHA-256 `874d0f7124039eb712b24c5d5ce3fe3950b5f3ad30a0c182c26b08aa68d3a9cb`. Two independent exports are byte-identical. Local RomFS parsing found 1,776 files in 69 directories. These are browser resources (ICU data, styles, scripts, translations, UI assets and certificates); no NSO/NRO executable files were found. No working browser or unlock is established.

The eleven filesystem-opening errors have different high context bits but all normalize to `0x002EE202`: module 2, description 6001, in the filesystem InvalidArgument range. They are not eleven different diagnoses and do not establish an access-denied policy. See the [original result definitions](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libvapours/include/vapours/results/fs_results.hpp).

v0.2 uses attributes read from the installed matching NCM content entry instead of `FsContentAttributes_All`. If that nonzero flag yields the same generic InvalidArgument result, it makes one read-only retry with default attributes. It records effective/raw flags, resolved content path, normalized module/description, and code-verification buffer size. It also records the SDK's runtime firmware version; if the SDK would dispatch a pre-20 request while the firmware service successfully reports a current version, it aligns the version **inside this process only**. It changes no system setting. These are request-correction/diagnostic changes; the exact cause and outcome still need a console run.

Update the research NRO, press X once, and send the newest report. Keep all existing successful exports. The rest of DiscordNX2 remains untouched.

`inspect_romfs.py` is a standard-library-only host inspection tool. It validates table/name/data bounds and detects metadata cycles before optional extraction into a new directory. It never modifies its input image. Extracted firmware files stay local; only the parser and synthetic tests are committed.

```sh
python inspect_romfs.py /path/to/romfs.bin --manifest /path/to/manifest.json
python inspect_romfs.py /path/to/romfs.bin --extract /path/to/new-private-folder
python -m unittest test_inspect_romfs.py
```

The parser's tests cover synthetic valid extraction, no overwrite, truncation, parent/name validation, data bounds and metadata cycles. The production C request-policy tests cover context-bit normalization, bounded default retry and SDK-version synchronization conditions. v0.2 compiles with warnings as errors and passes the sanitized copy/request tests; its console result is pending.


## v0.2 console success: executable exports obtained

The supplied `run-000003a226035c8b` includes code filesystems for openweb, SystemWeb, offlineweb0, offlineweb3, and BrowserCoreDll. The matching installed content attribute `01` succeeds directly. The SDK reports20.0.0 while the console reports23.0.1; the pre-20 dispatch fallback did not run. The report's code-verification data size is420bytes. Code access works through the existing public loader; no new NSO or custom payload was required.

There are14NSO modules and5NPDM process-policy files. Local analysis unpacked all14 modules and verified all42 enabled SHA-256 segment hashes. No `.part` file is present. This establishes complete, internally consistent segment data, not independent verification of Nintendo's signature chain. The NPDM names and ACI0 IDs match the five requested code targets.

BrowserDll also exports its1,776resource files. The run contains1,796exported files plus its report. The report reaches its128KiB cap while listing those resources, before logging BrowserCoreDll; the later code files nevertheless exist and validate. Do not treat report truncation as a failed code export or infer the final error count from the incomplete log. Program-content resource mounting still returns filesystem description6000 in the visible sections; those failures are separate from successful ExeFS/code access.

The engine's dynamic symbols include initialization/finalization, view creation, and URL loading. The applet code also contains a reference to the localized startup-unavailable dialog. These are useful analysis targets, **not a verified bypass or a standalone reusable browser**. The exact blocking condition and the host integration needed for the engine remain unresolved. No further extraction run is needed for the current analysis.

`inspect_nso.py` validates NSO header/segment bounds and enabled segment hashes. Optional private unpacking writes into a new directory and refuses an existing destination. Compressed NSOs need the Python `lz4` package; no proprietary binaries or third-party package copies are committed.

```sh
python inspect_nso.py /path/to/exefs/main
python inspect_nso.py /path/to/exefs/main --unpack /path/to/new-private-folder
python -m unittest test_inspect_nso.py test_inspect_romfs.py
```

Synthetic NSO tests cover valid unpacking, no overwrite, corrupted hashes, truncation, memory limits, overlapping segments, and optional LZ4 decompression. Browser research remains separate from the Discord client and voice work.


## Focused browser startup check

The exported openweb code has three authenticated callback references to its unavailable-dialog factory. One path derives a state flag from the first bit of a 48-byte read-only `set:sys` command 265 response. Its meaning and relevance to the live failure are still unconfirmed.

`build-startup-check.bat` builds the separate `dist/Browser-Startup-Check.nro` (or run `make DEVKITPRO=/c/devkitPro startup-check`). **X** reads that response on verified physical 23.0.1, saves a small fresh SD report, and runs the unchanged normal example.com browser launch for correlation. **A** only reads settings. This does not change settings or unlock the browser. See [STARTUP-CHECK.md](STARTUP-CHECK.md) for the exact trace, test steps, and limitations. No repeat content export is required.
