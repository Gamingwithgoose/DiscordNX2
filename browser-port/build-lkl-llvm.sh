#!/usr/bin/env bash
# Local Debian/WSL component build. No Docker, NRO packaging or server changes.
set -euo pipefail
source_dir=${1:?Usage: build-lkl-llvm.sh /absolute/path/to/pinned/lkl-linux}
root=$(cd "$(dirname "$0")/.." && pwd)
expected=d0f76a77e8c2ca68bd7d96190b8da09040eab072
actual=$(git -C "$source_dir" rev-parse HEAD)
test "$actual" = "$expected" || { echo "Unexpected LKL source revision: $actual"; exit 1; }
for tool in clang ld.lld llvm-ar llvm-nm llvm-objcopy llvm-objdump llvm-readelf make flex bison bc; do
 command -v "$tool" >/dev/null || { echo "Missing build tool: $tool"; exit 1; }
done
if ! grep -q 'filter $(CROSS_COMPILE),aarch64-none-elf' "$source_dir/tools/lkl/Makefile.autoconf"; then
 patch -d "$source_dir" -p1 < "$root/browser-port/lkl-aarch64-llvm.patch"
fi
cd "$source_dir"
make -C tools/lkl LLVM=1 CROSS_COMPILE=aarch64-none-elf conf
if ! test -f .config; then
 make ARCH=lkl LLVM=1 CLANG_TARGET_FLAGS_lkl=aarch64-none-elf defconfig
fi
scripts/config --set-str OUTPUT_FORMAT elf64-littleaarch64 \
 --enable UNIX --enable TIMERFD --enable SIGNALFD --enable FILE_LOCKING \
 --enable INOTIFY_USER --enable ADVISE_SYSCALLS \
 --disable XFS_FS --disable BTRFS_FS --disable IPV6 \
 --disable BLK_DEV_NVME --disable NVME_CORE
make ARCH=lkl LLVM=1 CLANG_TARGET_FLAGS_lkl=aarch64-none-elf \
 KCFLAGS=-Wno-unused-command-line-argument KCPPFLAGS=-Wno-unused-command-line-argument olddefconfig
make -C tools/lkl LLVM=1 CROSS_COMPILE=aarch64-none-elf \
 KCFLAGS=-Wno-unused-command-line-argument KCPPFLAGS=-Wno-unused-command-line-argument \
 -j4 "$source_dir/tools/lkl/lib/lkl.o"
llvm-readelf -h tools/lkl/lib/lkl.o | grep 'Machine:.*AArch64'
echo 'AArch64 LKL component built. This is not a runnable Chromium NRO.'
component_out="$root/.local/browser-engine/lkl"
mkdir -p "$component_out/include"
cp tools/lkl/lib/lkl.o "$component_out/lkl-aarch64.o"
cp -a tools/lkl/include/. "$component_out/include/"

# Retain exactly the host glue sources used by the Windows native compiler.
mkdir -p "$component_out/host-source"
for f in iomem.c iomem.h jmp_buf.c jmp_buf.h utils.c; do
 cp "tools/lkl/lib/$f" "$component_out/host-source/$f"
done
printf '%s\n' "$expected" > "$component_out/source-revision.txt"
cp .config "$component_out/kernel.config"
