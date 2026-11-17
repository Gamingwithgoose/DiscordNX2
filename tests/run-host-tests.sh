#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc}
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -pedantic -o tests/test_voice_packet.bin tests/test_voice_packet.c app/source/voice_packet.c
./tests/test_voice_packet.bin
rm -f tests/test_voice_packet.bin
