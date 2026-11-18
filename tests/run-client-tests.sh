#!/usr/bin/env sh
# Small host tests only: no Switch application or browser engine build.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Iapp/source tests/test_client_data.c -o "$tmp/data"
"$tmp/data"
cc -std=c11 -Wall -Wextra -Werror -DDNX2_USE_MBEDTLS=1 -DDNX2_SESSION_PATH="\"$tmp/session.bin\"" -Itests/phone_host -Iapp/source \
 tests/test_session_store.c app/source/session_store.c -lmbedcrypto -o "$tmp/session"
"$tmp/session" save
"$tmp/session" restore
