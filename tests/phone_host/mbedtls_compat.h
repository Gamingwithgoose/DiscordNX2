// SPDX-License-Identifier: MIT
// Host tests only: adapt existing production 2.28 calls to Debian mbedTLS 3.
#include <mbedtls/version.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>
#if MBEDTLS_VERSION_MAJOR >= 3
#define MBEDTLS_RSA_PUBLIC 0
#define MBEDTLS_RSA_PRIVATE 1
#define mbedtls_sha256_ret mbedtls_sha256
static inline int host_oaep_decrypt(mbedtls_rsa_context *ctx,int (*rng)(void*,unsigned char*,size_t),void *arg,int mode,
 const unsigned char *label,size_t len,size_t *olen,const unsigned char *in,unsigned char *out,size_t cap) {
 (void)mode;return mbedtls_rsa_rsaes_oaep_decrypt(ctx,rng,arg,label,len,olen,in,out,cap);
}
static inline int host_oaep_encrypt(mbedtls_rsa_context *ctx,int (*rng)(void*,unsigned char*,size_t),void *arg,int mode,
 const unsigned char *label,size_t len,size_t count,const unsigned char *in,unsigned char *out) {
 (void)mode;return mbedtls_rsa_rsaes_oaep_encrypt(ctx,rng,arg,label,len,count,in,out);
}
#define mbedtls_rsa_rsaes_oaep_decrypt host_oaep_decrypt
#define mbedtls_rsa_rsaes_oaep_encrypt host_oaep_encrypt
#endif
