#!/bin/sh
# One-time local toolchain setup using ounce-devkit's own official installer.
set -eu
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
if [ -f "$DEVKITPRO/libounce/switch2_cpu.mk" ] &&
   [ -f "$DEVKITPRO/portlibs/switch2/include/kaihen.h" ] &&
   [ -x "$DEVKITPRO/devkitA64/bin/aarch64-none-elf-gcc" ]; then
    echo "Switch 2 compiler and libounce/libkaihen appear to be installed."
    exit 0
fi
if ! command -v git >/dev/null 2>&1; then
    echo "Git is needed to download the upstream ounce-devkit installer."
    exit 1
fi
mkdir -p .local
if [ ! -f .local/ounce-devkit/install.sh ]; then
    echo "Downloading ounce-devkit installer source..."
    git clone --depth 1 https://github.com/ouncebrew/ounce-devkit.git .local/ounce-devkit
fi
echo "Running the official ounce-devkit installer under devkitPro MSYS2..."
sh .local/ounce-devkit/install.sh
echo "Checking installed Switch 2 toolchain..."
if [ ! -f "$DEVKITPRO/libounce/switch2_cpu.mk" ]; then
    echo "libounce is still missing from $DEVKITPRO/libounce."
    exit 1
fi
if [ ! -x "$DEVKITPRO/devkitA64/bin/aarch64-none-elf-gcc" ]; then
    echo "devkitA64 compiler not found."
    exit 1
fi
echo "The Switch 2 toolchain is installed."
