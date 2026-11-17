#!/bin/sh
set -eu
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
if [ ! -f "$DEVKITPRO/libounce/switch2_cpu.mk" ]; then
 echo "Switch 2 toolchain not found at $DEVKITPRO"; exit 1
fi
if [ ! -x "$DEVKITPRO/devkitA64/bin/aarch64-none-elf-gcc" ]; then
 echo "devkitA64 cross-compiler missing"; exit 1
fi
case "${1:-all}" in
 app) make app-only ;;
 all) make all ;;
 *) echo "Usage: build-local.sh [app|all]"; exit 1 ;;
esac
