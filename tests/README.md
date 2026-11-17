# Portable voice protocol tests

Run `sh tests/run-host-tests.sh` from a machine with a native C compiler (cc, gcc or clang). These tests compile only the platform-independent voice-packet module; **they do not** build or verify the Switch 2 NRO. They check 10ms mono downmix, packet timestamps, peak/RMS and hangover behavior, PCM samples, malformed packets and checksum corruption.

The normal user workflow remains **GitHub Desktop → Build-NRO.bat → FTP only dist/DiscordNX2.nro**; these host tests are optional for developers.
