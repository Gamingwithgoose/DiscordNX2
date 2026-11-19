#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Uses the same pinned sources as Build-NRO; host OpenSSL 3 development files required.
set -eu
cd "$(dirname "$0")/.."
src="$PWD/.local/voice/src"
[ -f "$src/dave/cpp/includes/dave/dave.h" ] || { echo 'Run tools/voice/build.sh once to prepare pinned sources.';exit 1; }
build="$PWD/.local/voice/host"
cmake -S tools/voice -B "$build" -G Ninja -DDNX_VOICE_SOURCES="$src"
cmake --build "$build" -j4
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/opus"
cp "$src/opus/include/"opus*.h "$tmp/opus/"
cc -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -DDAVE_STATIC -Iapp/source -Iapp/source/vendor/monocypher -I"$src/dave/cpp/includes" -I"$tmp" -c app/source/voice_engine.c -o "$tmp/engine.o"
cc -std=c11 -O1 -g -fsanitize=address,undefined -Iapp/source/vendor/monocypher -c app/source/vendor/monocypher/monocypher.c -o "$tmp/monocypher.o"
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -DDAVE_STATIC -Iapp/source -I"$src/dave/cpp/test" -I"$src/dave/cpp/includes" -I"$build" -I"$src/mls/include" -I"$src/mls/lib/bytes/include" -I"$src/mls/lib/hpke/include" -I"$src/mls/lib/tls_syntax/include" -I"$src/mls/third_party" -I"$src/dave/cpp/src" tests/test_voice_engine.cpp "$src/dave/cpp/test/external_sender.cpp" "$tmp/engine.o" "$tmp/monocypher.o" "$build/libdnx_dave.a" "$build/opus/libopus.a" -lcrypto -lpthread -o "$tmp/voice"
"$tmp/voice"

for name in audio members controller policy;do
 cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iapp/source "tests/test_voice_$name.c" -o "$tmp/$name"
 "$tmp/$name"
done
python3 tests/test_voice_network.py > "$tmp/network.c"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined "$tmp/network.c" -o "$tmp/network"
"$tmp/network"

python3 tests/test_voice_navigation.py > "$tmp/navigation.c"
cc -std=c11 -Wall -Wextra -Werror -Iapp/source -fsanitize=address,undefined "$tmp/navigation.c" -o "$tmp/navigation"
"$tmp/navigation"
