#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build only the three static voice libraries, once per pinned source/toolchain.
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
cache="$root/.local/voice"
export PATH="$DEVKITPRO/devkitA64/bin:$PATH"
for tool in curl sha256sum tar perl make cmake ninja aarch64-none-elf-gcc; do
 command -v "$tool" >/dev/null || { echo "Voice dependencies need $tool. Install devkitPro MSYS2 make, cmake, ninja, perl and curl, then retry.";exit 1; }
done
mkdir -p "$cache/downloads" "$cache/src" "$cache/prefix"
fingerprint=$({ cat "$root/tools/voice/build.sh" "$root/tools/voice/CMakeLists.txt" "$root/tools/voice/switch2.cmake";aarch64-none-elf-gcc --version; } | sha256sum | cut -d' ' -f1)
if [ -f "$cache/ready" ] && [ "$(cat "$cache/ready")" = "$fingerprint" ] && [ -f "$cache/prefix/lib/libdnx_dave.a" ] && [ -f "$cache/prefix/lib/libopus.a" ] && [ -f "$cache/prefix/lib/libcrypto.a" ];then exit 0;fi
fetch() {
 name=$1;sha=$2;url=$3
 file="$cache/downloads/$name"
 if ! [ -f "$file" ] || ! echo "$sha  $file" | sha256sum -c - >/dev/null 2>&1;then
  curl --fail --location --retry 2 --proto '=https' --tlsv1.2 "$url" -o "$file.part"
  echo "$sha  $file.part" | sha256sum -c - || { echo 'Voice source checksum mismatch.';exit 1; }
  mv "$file.part" "$file"
 fi
}
unpack(){ if [ ! -f "$cache/src/$1/.extracted" ];then mkdir -p "$cache/src/$1";tar -xzf "$cache/downloads/$2" --strip-components=1 -C "$cache/src/$1";touch "$cache/src/$1/.extracted";fi; }
echo 'Preparing pinned DAVE, MLS++, OpenSSL and Opus sources. First build takes several minutes; later NRO builds reuse them.'
fetch dave.tar.gz 7e1e3950e3b87bec716db21a7ded4521674988b97319e8a192045127e4d601a3 https://codeload.github.com/discord/libdave/tar.gz/b5463c443df74db75a713c775b09a847621e6d62
fetch mls.tar.gz 7a9d6318627e548903bc65c3dc5a4de4a90290983efea9a49af8561ed3f999f5 https://codeload.github.com/cisco/mlspp/tar.gz/1cc50a124a3bc4e143a787ec934280dc70c1034d
fetch openssl.tar.gz f54250b21795bb01240f3f9b42c384868d04460e339e39275aaaf93487f2d903 https://codeload.github.com/openssl/openssl/tar.gz/45e844fa2a14ec92d146bd8f5778ac130b6625fb
fetch opus.tar.gz 18e9d9e26565e279753aafb6823a482ef85ea9fddbfdcdbff8fd095b12dd96b6 https://codeload.github.com/xiph/opus/tar.gz/ddbe48383984d56acd9e1ab6a090c54ca6b735a6
fetch json.hpp aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63 https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp
for name in dave mls openssl opus;do unpack "$name" "$name.tar.gz";done
mkdir -p "$cache/src/json/nlohmann"
cp "$cache/downloads/json.hpp" "$cache/src/json/nlohmann/json.hpp"
jobs=${DNX2_VOICE_BUILD_JOBS:-4}
mkdir -p "$cache/crypto-build"
(
 cd "$cache/crypto-build"
 export CFLAGS="-O2 -DNO_SYSLOG -mcpu=cortex-a78c+crypto -mtp=soft -fPIE -D__SWITCH__ -D__SWITCH2__ -I$DEVKITPRO/libounce/include -ffunction-sections -fdata-sections"
 perl "$cache/src/openssl/Configure" gcc --cross-compile-prefix=aarch64-none-elf- --prefix="$cache/prefix" --libdir=lib no-asm no-shared no-threads no-sock no-dso no-engine no-module no-tests no-apps no-autoload-config no-async no-ui-console --with-rand-seed=none
 make -j"$jobs" build_libs
 make install_dev
) > "$cache/crypto-build.log" 2>&1 || { tail -n 35 "$cache/crypto-build.log";exit 1; }
cmake -S "$root/tools/voice" -B "$cache/libs-build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$root/tools/voice/switch2.cmake" -DDNX_VOICE_SOURCES="$cache/src" -DOPENSSL_INCLUDE_DIR="$cache/prefix/include" -DOPENSSL_CRYPTO_LIBRARY="$cache/prefix/lib/libcrypto.a" -DCMAKE_INSTALL_PREFIX="$cache/prefix" > "$cache/libs-build.log" 2>&1 && cmake --build "$cache/libs-build" -j"$jobs" >> "$cache/libs-build.log" 2>&1 && cmake --install "$cache/libs-build" >> "$cache/libs-build.log" 2>&1 || { tail -n 35 "$cache/libs-build.log";exit 1; }
printf '%s\n' "$fingerprint" > "$cache/ready"
echo 'Switch encrypted voice libraries are ready.'
