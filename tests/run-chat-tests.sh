#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -Wall -Wextra -Werror -Iapp/source tests/test_chat_layout.c -o "$tmp/layout"
"$tmp/layout"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iapp/source tests/test_live_gateway.c -o "$tmp/live"
"$tmp/live"
cc -std=c11 -Wall -Wextra -Werror -Iapp/source tests/test_unicode_fonts.c -lm -o "$tmp/fonts"
"$tmp/fonts" 'app/assets/fonts/NotoSans[wdth,wght].ttf' app/assets/fonts/NotoSansMath-Regular.ttf app/assets/fonts/NotoSansSymbols2-Regular.ttf 'app/assets/fonts/NotoEmoji[wght].ttf'
