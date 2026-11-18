#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp/red.png" <<'PY'
import sys,zlib,struct
def chunk(kind,data):return struct.pack('!I',len(data))+kind+data+struct.pack('!I',zlib.crc32(kind+data)&0xffffffff)
open(sys.argv[1],'wb').write(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!IIBBBBB',1,1,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(b'\x00\xff\x00\x00\xff'))+chunk(b'IEND',b''))
PY
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iapp/source tests/test_chat_jobs.c app/source/chat_assets.c -lm -o "$tmp/jobs"
"$tmp/jobs" "$tmp/red.png"
cc -std=c11 -Wall -Wextra -Werror -Iapp/source tests/test_mobile_keyboard.c -o "$tmp/keyboard"
"$tmp/keyboard"
