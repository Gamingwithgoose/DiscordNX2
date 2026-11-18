#!/usr/bin/env sh
# Host-only tests. No browser engine or Switch NRO is built.
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -Iapp/source tests/test_captcha_phone_wire.c app/source/captcha_phone_wire.c -o /var/tmp/discordnx2-phone-wire-test
/var/tmp/discordnx2-phone-wire-test
cc -std=gnu11 -Wall -Wextra -Werror -DDNX2_USE_MBEDTLS=1 -include tests/phone_host/mbedtls_compat.h -Itests/phone_host -Iapp/source \
 tests/phone_crypto_host.c app/source/auth_crypto.c app/source/captcha_phone_crypto.c app/source/captcha.c \
 -lmbedcrypto -o /var/tmp/discordnx2-phone-crypto-test
echo 'Host crypto harness ready; run node tests/test_captcha_phone.js from Windows.'
