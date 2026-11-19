#!/bin/sh
set -eu
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
if [ ! -f "$DEVKITPRO/libounce/switch2_cpu.mk" ]; then
 echo "Switch 2 toolchain not found at $DEVKITPRO"; exit 1
fi
if [ ! -x "$DEVKITPRO/devkitA64/bin/aarch64-none-elf-gcc" ]; then
 echo "devkitA64 cross-compiler missing"; exit 1
fi
bash "$(dirname "$0")/tools/voice/build.sh"
case "${1:-all}" in
 app) make app-only ;;
 all) make all ;;
 resident) make -C plugin VOICE=1; mkdir -p dist/resident-voice; cp plugin/discordnx2-voice.nso dist/resident-voice/discordnx2.nso ;;
 *) echo "Usage: build-local.sh [app|all|resident]"; exit 1 ;;
esac
