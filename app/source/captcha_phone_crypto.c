// SPDX-License-Identifier: MIT
#include "captcha_phone_wire.h"
#include "auth_crypto.h"
#include "captcha.h"
#include <string.h>
#ifdef DNX2_USE_MBEDTLS
#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
int dnx2PhoneDecrypt(const char *payload,size_t length,const char *state,char *out,size_t cap) {
    unsigned char envelope[4608]={0},key[32]={0},plain[4161]={0};
    char encoded[DNX2_PHONE_PAYLOAD_MAX+4]={0},wrapped[345]={0},aad[87]={0};
    size_t bytes=0,key_size=0,encoded_size=length,wrapped_size=0;int ok=0;
    mbedtls_gcm_context gcm;mbedtls_gcm_init(&gcm);
    if(!out||!cap)goto done;
    out[0]=0;
    if(!payload||length<400||length>DNX2_PHONE_PAYLOAD_MAX||length%4==1||!state||strlen(state)!=64)goto done;
    for(size_t i=0;i<64;i++)if(!((state[i]>='0'&&state[i]<='9')||(state[i]>='a'&&state[i]<='f')))goto done;
    for(size_t i=0;i<length;i++) {
        char c=payload[i];
        if(c=='-')c='+';else if(c=='_')c='/';
        else if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')))goto done;
        encoded[i]=c;
    }
    while(encoded_size%4)encoded[encoded_size++]='=';
    if(mbedtls_base64_decode(envelope,sizeof(envelope),&bytes,(unsigned char*)encoded,encoded_size)||
       bytes<351||envelope[0]!=1)goto done;
    if(mbedtls_base64_encode((unsigned char*)wrapped,sizeof(wrapped),&wrapped_size,envelope+1,256))goto done;
    wrapped[wrapped_size]=0;
    if(!dnx2AuthCryptoDecrypt(wrapped,key,sizeof(key),&key_size)||key_size!=32)goto done;
    size_t cipher_size=bytes-269-16;
    if(cipher_size<66||cipher_size>4160)goto done;
    memcpy(aad,"DiscordNX2 CAPTCHA v1:",22);memcpy(aad+22,state,64);
    if(mbedtls_gcm_setkey(&gcm,MBEDTLS_CIPHER_ID_AES,key,256)||
       mbedtls_gcm_auth_decrypt(&gcm,cipher_size,envelope+257,12,(unsigned char*)aad,86,
                              envelope+bytes-16,16,envelope+269,plain))goto done;
    unsigned diff=0;for(size_t i=0;i<64;i++)diff|=plain[i]^(unsigned char)state[i];
    size_t token_size=cipher_size-65;
    if(diff||plain[64]!='\n'||!token_size||token_size>4095||token_size>=cap)goto done;
    for(size_t i=65;i<cipher_size;i++)if(plain[i]<33||plain[i]>126)goto done;
    memcpy(out,plain+65,token_size);out[token_size]=0;ok=1;
done:
    mbedtls_gcm_free(&gcm);
    dnx2CaptchaWipe(envelope,sizeof(envelope));dnx2CaptchaWipe(key,sizeof(key));
    dnx2CaptchaWipe(plain,sizeof(plain));dnx2CaptchaWipe(encoded,sizeof(encoded));
    dnx2CaptchaWipe(wrapped,sizeof(wrapped));dnx2CaptchaWipe(aad,sizeof(aad));
    if(!ok&&out&&cap)dnx2CaptchaWipe(out,cap);
    return ok;
}
#else
int dnx2PhoneDecrypt(const char*p,size_t n,const char*s,char*out,size_t cap) {
    (void)p;(void)n;(void)s;if(out&&cap)dnx2CaptchaWipe(out,cap);return 0;
}
#endif
