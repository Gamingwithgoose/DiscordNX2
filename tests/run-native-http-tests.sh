#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
trap 'rm -f tests/native_http.generated.c tests/native_http.bin' EXIT
"${PYTHON:-python}" tests/test_native_http.py > tests/native_http.generated.c
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror tests/native_http.generated.c -o tests/native_http.bin
./tests/native_http.bin
