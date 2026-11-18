#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp/animation.gif" <<'PY'
import sys,struct
header=b'GIF89a'+struct.pack('<HHBBB',1,1,0x80,0,0)+bytes([255,0,0,0,0,255])
def frame(code):return b'!\xf9\x04\x00\x0a\x00\x00\x00'+b',\x00\x00\x00\x00\x01\x00\x01\x00\x00'+bytes([2,2,code,1,0])
open(sys.argv[1],'wb').write(header+frame(0x44)+frame(0x4c)+b';')
PY
cc -std=c11 -Wall -Wextra -Werror -DUTF8PROC_STATIC -fsanitize=address,undefined -Iapp/source tests/test_presentation.c app/source/chat_assets.c app/source/vendor/utf8proc/utf8proc.c -lm -o "$tmp/presentation"
"$tmp/presentation" "$tmp/animation.gif"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iapp/source tests/test_voice_control.c -o "$tmp/voice"
"$tmp/voice"
