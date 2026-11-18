#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source_dir=/var/tmp/discordnx2-browser-port/linux
revision=d0f76a77e8c2ca68bd7d96190b8da09040eab072
for tool in git clang ld.lld llvm-readelf python3 make flex bison bc; do
 command -v "$tool" >/dev/null || { echo "Debian WSL is missing $tool. Install build dependencies before retrying."; exit 1; }
done
if ! test -d "$source_dir"; then
 mkdir -p "$(dirname "$source_dir")"
 git init "$source_dir"
 git -C "$source_dir" remote add origin https://github.com/lkl/linux.git
 git -C "$source_dir" fetch --depth=1 origin "$revision"
 git -C "$source_dir" checkout --detach FETCH_HEAD
fi
bash "$root/browser-port/build-lkl-llvm.sh" "$source_dir"
