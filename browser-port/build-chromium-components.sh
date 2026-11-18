#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Component build only, using pinned upstream source and libounce. No Docker.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
cross="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-"
engine="$root/.local/chromium-nx"
out="$root/.local/browser-engine/chromium-components"
lkl="$root/.local/browser-engine/lkl"
test -f "$lkl/lkl-aarch64.o" || { echo 'Build the LKL component with build-lkl-llvm.sh first.'; exit 1; }
test "$(git -C "$engine" rev-parse HEAD)" = 87a8095d20cf73b0fde0eee3215e70ebc887f048
test "$(cat "$lkl/source-revision.txt")" = d0f76a77e8c2ca68bd7d96190b8da09040eab072
mkdir -p "$out/obj" "$out/include" "$out/host"
cp -a "$lkl/include/." "$out/include/"
printf '#define LKL_HOST_CONFIG_AARCH64 y\n#define LKL_HOST_CONFIG_VIRTIO_NET y\n#define LKL_HOST_CONFIG_VIRTIO_BLK y\n' > "$out/include/lkl_autoconf.h"
# newlib already declares iovec. Do not redeclare it in the LKL interface.
"${PYTHON:-python3}" "$root/browser-port/prepare-chromium-host.py" "$engine" "$out"
for f in iomem.c iomem.h jmp_buf.c jmp_buf.h utils.c; do cp "$lkl/host-source/$f" "$out/host/$f"; done
cflags=(-mcpu=cortex-a78c+crypto -mtp=soft -fPIE -std=gnu11 -O2 -g
 -Wall -Wextra -ffunction-sections -fdata-sections -D__SWITCH__ -D__SWITCH2__
 -I"$DEVKITPRO/libounce/include" -I"$out/include" -I"$out/host" -I"$engine/gx")
for f in "$out/host/nx_host.c" "$out/host/iomem.c" "$out/host/jmp_buf.c" "$out/host/utils.c" "$root/browser-port/lkl_probe.c"; do
 "${cross}gcc" "${cflags[@]}" -c "$f" -o "$out/obj/$(basename "${f%.*}").o"
done
# Compile the real ELF/memory/syscall/Chromium-pipe core, without claiming it links to a browser.
for f in gmem.c gelf.c gsys.c cdp.c tramp.S; do
 "${cross}gcc" "${cflags[@]}" -c "$engine/gx/$f" -o "$out/obj/$(basename "${f%.*}").o"
done
"${cross}ar" rcs "$out/libchromium-core.a" "$out/obj/"{gmem,gelf,gsys,cdp,tramp}.o
"${cross}objcopy" --strip-debug --set-section-flags .rodata=alloc,load,contents,data \
 --rename-section .rodata=.kernel.rodata "$lkl/lkl-aarch64.o" "$out/lkl-nro.o"
"${cross}gcc" -mcpu=cortex-a78c+crypto -mtp=soft -fPIE \
 -specs="$DEVKITPRO/libounce/switch.specs" -Wl,--gc-sections \
 "$out/obj/"{lkl_probe,nx_host,iomem,jmp_buf,utils}.o "$out/lkl-nro.o" \
 -L"$DEVKITPRO/libounce/lib" -lounce_core -lm -o "$out/runtime-probe.elf"
"$DEVKITPRO/tools/bin/nacptool" --create 'DiscordNX2 runtime test' 'Gamingwithgoose' '0.1' "$out/runtime-probe.nacp"
"$DEVKITPRO/tools/bin/elf2nro" "$out/runtime-probe.elf" "$out/DiscordNX2-Runtime-Test.nro" --nacp="$out/runtime-probe.nacp"
echo "Built $out/DiscordNX2-Runtime-Test.nro"
echo 'Runtime test only. Browser rendering, guest image and CAPTCHA integration are unfinished.'
