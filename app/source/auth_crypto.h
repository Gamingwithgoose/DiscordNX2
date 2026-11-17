// SPDX-License-Identifier: MIT
#ifndef DNX2_AUTH_CRYPTO_H
#define DNX2_AUTH_CRYPTO_H
#include <stddef.h>
/* Security boundary: no private key or token is serialized to the SD card.
 * mbedTLS 2.28 / Switch portlib supplies vetted RSA-2048/OAEP and SHA-256. */
int dnx2AuthCryptoOpen(void);
void dnx2AuthCryptoClose(void);
int dnx2AuthCryptoPublicKey(char *out,size_t cap);
int dnx2AuthCryptoFingerprint(char *out,size_t cap);
int dnx2AuthCryptoDecrypt(const char *cipher,unsigned char *plain,size_t cap,size_t *plain_len);
int dnx2AuthCryptoNonceProof(const char *cipher,char *proof,size_t proof_cap);
int dnx2AuthCryptoAvailable(void);
#endif
