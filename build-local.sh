#!/bin/sh
# Runs inside devkitPro MSYS2. No remote builder involved.
set -eu
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
if [ ! -f "$DEVKITPRO/libounce/switch2_cpu.mk" ]; then
  echo "Switch 2 toolchain not found at $DEVKITPRO"
  echo "Install devkitPro, then follow the ounce-devkit install.sh instructions."
  echo "https://github.com/ouncebrew/ounce-devkit"
  exit 1
fi
if [ ! -x "$DEVKITPRO/devkitA64/bin/aarch64-none-elf-gcc" ]; then
  echo "devkitA64 cross-compiler is missing."
  exit 1
fi
if [ ! -x "$DEVKITPRO/tools/bin/elf2nro" ] || [ ! -x "$DEVKITPRO/tools/bin/elf2nso" ]; then
  echo "Switch 2 conversion tools are missing."
  exit 1
fi
make
