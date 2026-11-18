#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp/animation.gif" <<'PY'
import sys,struct,zlib
header=b'GIF89a'+struct.pack('<HHBBB',1,1,0x80,0,0)+bytes([255,0,0,0,0,255])
def frame(code):return b'!\xf9\x04\x00\x0a\x00\x00\x00'+b',\x00\x00\x00\x00\x01\x00\x01\x00\x00'+bytes([2,2,code,1,0])
open(sys.argv[1],'wb').write(header+frame(0x44)+frame(0x4c)+b';')
open(sys.argv[1]+'.long.gif','wb').write(header+b''.join(frame(0x44 if i%2==0 else 0x4c) for i in range(130))+b';')
def chunk(k,d):return struct.pack('!I',len(d))+k+d+struct.pack('!I',zlib.crc32(k+d)&0xffffffff)
open(sys.argv[1]+'.png','wb').write(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!IIBBBBB',1200,900,8,6,0,0,0))+chunk(b'IDAT',zlib.compress((b'\x00'+b'\x20\x60\xa0\xff'*1200)*900))+chunk(b'IEND',b''))
PY
cc -std=c11 -Wall -Wextra -Werror -DUTF8PROC_STATIC -fsanitize=address,undefined -Iapp/source tests/test_presentation.c app/source/chat_assets.c app/source/vendor/utf8proc/utf8proc.c -lm -o "$tmp/presentation"
"$tmp/presentation" "$tmp/animation.gif"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iapp/source tests/test_voice_control.c -o "$tmp/voice"
"$tmp/voice"

cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iapp/source tests/test_gateway_limits.c -o "$tmp/limits"
"$tmp/limits"
