// SPDX-License-Identifier: MIT
/* Native Discord remote-auth cryptography; linked to devkitPro's packaged
 * Apache-2.0 mbedTLS portlib when available. Never substitutes homegrown RSA.
 * No credentials, private keys or public-key fingerprint are persisted.
 */
#include "auth_crypto.h"
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef DNX2_USE_MBEDTLS
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/md.h>

typedef struct {
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    int active,csrng_active;
    char fingerprint[80];
    char public_key[900];
} DnxCrypto;
static DnxCrypto crypto;
static int auth_crypto_error;
static int auth_crypto_mbed_error;

static int hw_entropy(void *ctx,unsigned char *out,size_t len,size_t *olen) {
    (void)ctx;
    if(R_FAILED(csrngGetRandomBytes(out,len)))return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    *olen=len;return 0;
}
static void clear_volatile(void *p,size_t n) {
    volatile unsigned char *q=(volatile unsigned char*)p;
    while(n--)*q++=0;
}
static int base64_url(const unsigned char *bytes,size_t n,char *out,size_t cap) {
    size_t olen=0;
    if(mbedtls_base64_encode((unsigned char*)out,cap,&olen,bytes,n)!=0)return 0;
    for(size_t i=0;i<olen;i++) {
        if(out[i]=='+')out[i]='-';
        if(out[i]=='/')out[i]='_';
    }
    while(olen>0&&out[olen-1]=='=')olen--;
    if(olen>=cap)return 0;
    out[olen]=0;return 1;
}
static int decode_base64(const char *str,unsigned char *out,size_t cap,size_t *decoded) {
    if(!str || !decoded)return 0;
    size_t n=strlen(str),length=n;
    if(!n||n>1100)return 0;
    char buf[1120];
    memcpy(buf,str,n);
    for(size_t i=0;i<n;i++) {
        if(buf[i]=='-')buf[i]='+';
        else if(buf[i]=='_')buf[i]='/';
        else if(!((buf[i]>='A'&&buf[i]<='Z') ||
                   (buf[i]>='a'&&buf[i]<='z') ||
                   (buf[i]>='0'&&buf[i]<='9') ||
                   buf[i]=='+'||buf[i]=='/'||buf[i]=='='))return 0;
    }
    while(length%4)buf[length++]='=';
    buf[length]=0;
    int rc=mbedtls_base64_decode(out,cap,decoded,(unsigned char*)buf,length);
    clear_volatile(buf,sizeof(buf));
    return rc==0;
}
int dnx2AuthCryptoAvailable(void){return 1;}
int dnx2AuthCryptoError(void){return auth_crypto_error;}
int dnx2AuthCryptoMbedError(void){return auth_crypto_mbed_error;}
void dnx2AuthCryptoClose(void) {
    mbedtls_pk_free(&crypto.pk);
    mbedtls_ctr_drbg_free(&crypto.drbg);
    mbedtls_entropy_free(&crypto.entropy);
    if(crypto.csrng_active)csrngExit();
    clear_volatile(&crypto,sizeof(crypto));
}
int dnx2AuthCryptoOpen(void) {
    dnx2AuthCryptoClose();
    auth_crypto_error=0;
    auth_crypto_mbed_error=0;
    mbedtls_pk_init(&crypto.pk);
    mbedtls_entropy_init(&crypto.entropy);
    mbedtls_ctr_drbg_init(&crypto.drbg);
    if(R_FAILED(csrngInitialize()))goto fail;
    crypto.csrng_active=1;
    if(mbedtls_entropy_add_source(&crypto.entropy,hw_entropy,NULL,32,
                                 MBEDTLS_ENTROPY_SOURCE_STRONG)!=0)goto fail;
    const unsigned char personalization[]="DiscordNX2 remote auth RSA-2048";
    if(mbedtls_ctr_drbg_seed(&crypto.drbg,mbedtls_entropy_func,&crypto.entropy,
                            personalization,sizeof(personalization)-1)!=0)goto fail;
    if(mbedtls_pk_setup(&crypto.pk,mbedtls_pk_info_from_type(MBEDTLS_PK_RSA))!=0)goto fail;
    if(mbedtls_rsa_gen_key(mbedtls_pk_rsa(crypto.pk),mbedtls_ctr_drbg_random,
                           &crypto.drbg,2048,65537)!=0)goto fail;
    mbedtls_rsa_set_padding(mbedtls_pk_rsa(crypto.pk),MBEDTLS_RSA_PKCS_V21,MBEDTLS_MD_SHA256);
    unsigned char der[1024];
    int n=mbedtls_pk_write_pubkey_der(&crypto.pk,der,sizeof(der));
    if(n<0||n>600)goto fail;
    const unsigned char *pub=der+sizeof(der)-(size_t)n;
    unsigned char hash[32];
    if(mbedtls_sha256_ret(pub,(size_t)n,hash,0)!=0)goto fail;
    if(!base64_url(hash,sizeof(hash),crypto.fingerprint,sizeof(crypto.fingerprint)))goto fail;
    size_t len=0;
    if(mbedtls_base64_encode((unsigned char*)crypto.public_key,
                            sizeof(crypto.public_key),&len,pub,(size_t)n)!=0)goto fail;
    crypto.active=1;
    clear_volatile(der,sizeof(der));clear_volatile(hash,sizeof(hash));
    return 1;
fail:
    dnx2AuthCryptoClose();return 0;
}
int dnx2AuthCryptoPublicKey(char *out,size_t cap) {
    if(!crypto.active||strlen(crypto.public_key)+1>cap)return 0;
    strcpy(out,crypto.public_key);return 1;
}
int dnx2AuthCryptoFingerprint(char *out,size_t cap) {
    if(!crypto.active||strlen(crypto.fingerprint)+1>cap)return 0;
    strcpy(out,crypto.fingerprint);return 1;
}
int dnx2AuthCryptoDecrypt(const char *cipher,unsigned char *plain,
                          size_t cap,size_t *plain_len) {
    auth_crypto_error=0;
    auth_crypto_mbed_error=0;
    if(!crypto.active||!cipher||!plain||!plain_len) {
        auth_crypto_error=1;return 0;
    }
    unsigned char encrypted[512]={0};
    size_t bytes=0,olen=0;
    int ok=decode_base64(cipher,encrypted,sizeof(encrypted),&bytes);
    if(!ok) {auth_crypto_error=2;goto fail;}
    if(bytes!=mbedtls_rsa_get_len(mbedtls_pk_rsa(crypto.pk))) {
        auth_crypto_error=3;goto fail;
    }
    int rc=mbedtls_rsa_rsaes_oaep_decrypt(mbedtls_pk_rsa(crypto.pk),
         mbedtls_ctr_drbg_random,&crypto.drbg,MBEDTLS_RSA_PRIVATE,
         NULL,0,&olen,encrypted,plain,cap);
    if(rc!=0) {
        /* Expose category only, never ciphertext, nonce, or crypto buffers. */
        auth_crypto_error=4;
        auth_crypto_mbed_error=rc;
        goto fail;
    }
    *plain_len=olen;
    clear_volatile(encrypted,sizeof(encrypted));
    return 1;
fail:
    clear_volatile(encrypted,sizeof(encrypted));
    return 0;
}
/* Before trusting a remote challenge, verify this platform's RSA-2048
 * encryption/decryption integration with an ephemeral, non-secret test. */
int dnx2AuthCryptoSelfTest(void) {
    if(!crypto.active){auth_crypto_error=1;return 0;}
    static const unsigned char sample[]="DiscordNX2 OAEP selftest v1";
    unsigned char cipher[256]={0},result[256]={0};
    size_t n=0;
    int ret=mbedtls_rsa_rsaes_oaep_encrypt(mbedtls_pk_rsa(crypto.pk),
        mbedtls_ctr_drbg_random,&crypto.drbg,MBEDTLS_RSA_PUBLIC,
        NULL,0,sizeof(sample)-1,sample,cipher);
    int ok=(ret==0);
    if(ok){
        ret=mbedtls_rsa_rsaes_oaep_decrypt(mbedtls_pk_rsa(crypto.pk),
            mbedtls_ctr_drbg_random,&crypto.drbg,MBEDTLS_RSA_PRIVATE,
            NULL,0,&n,cipher,result,sizeof(result));
        ok=ret==0&&n==sizeof(sample)-1&&memcmp(sample,result,n)==0;
    }
    clear_volatile(cipher,sizeof(cipher));
    clear_volatile(result,sizeof(result));
    auth_crypto_error=ok?0:4;
    auth_crypto_mbed_error=ok?0:ret;
    return ok;
}
/* Current v2 remote-auth field is "nonce": the decrypted RSA-OAEP
 * nonce base64url-encoded. The 2021-era "proof" SHA-256 variant is not
 * compatible with TriCord's newer v2 exchange. */
int dnx2AuthCryptoNonceProof(const char *cipher,char *proof,size_t proof_cap) {
    unsigned char nonce[256]={0};
    size_t n=0;
    int ok=dnx2AuthCryptoDecrypt(cipher,nonce,sizeof(nonce),&n);
    if(ok){ok=base64_url(nonce,n,proof,proof_cap);
        if(!ok)auth_crypto_error=5;}
    clear_volatile(nonce,sizeof(nonce));
    return ok;
}

#else
int dnx2AuthCryptoAvailable(void){return 0;}
int dnx2AuthCryptoError(void){return 1;}
int dnx2AuthCryptoMbedError(void){return 0;}
int dnx2AuthCryptoSelfTest(void){return 0;}
int dnx2AuthCryptoOpen(void){return 0;}
void dnx2AuthCryptoClose(void){}
int dnx2AuthCryptoPublicKey(char*out,size_t n){(void)out;(void)n;return 0;}
int dnx2AuthCryptoFingerprint(char*out,size_t n){(void)out;(void)n;return 0;}
int dnx2AuthCryptoDecrypt(const char*c,unsigned char*p,size_t n,size_t*o){(void)c;(void)p;(void)n;(void)o;return 0;}
int dnx2AuthCryptoNonceProof(const char*c,char*p,size_t n){(void)c;(void)p;(void)n;return 0;}
#endif
