// SPDX-License-Identifier: MIT
/* Saved credentials require a user PIN; no decryption key is stored on SD.
 * PBKDF2-SHA256 + AES-256-GCM from packaged mbedTLS. No token logging. */
#include "session_store.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <switch.h>
#ifdef DNX2_USE_MBEDTLS
#include <mbedtls/md.h>
#include <mbedtls/version.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/gcm.h>
#endif
#ifndef DNX2_SESSION_PATH
#define DNX2_SESSION_PATH "sdmc:/switch/DiscordNX2/session.bin"
#endif
static inline void clear(void *p,size_t n){volatile unsigned char *v=p;while(n--)*v++=0;}
int dnx2SessionExists(void){FILE *f=fopen(DNX2_SESSION_PATH,"rb");if(!f)return 0;fclose(f);return 1;}
void dnx2SessionForget(void){remove(DNX2_SESSION_PATH);remove(DNX2_SESSION_PATH ".tmp");}
int dnx2SessionPinDigits(void){unsigned char header[9]={0};FILE *f=fopen(DNX2_SESSION_PATH,"rb");if(!f)return 0;size_t n=fread(header,1,sizeof(header),f);fclose(f);if(n>=8&&!memcmp(header,"DNX2SES1",8))return 8;if(n==9&&!memcmp(header,"DNX2SES2",8)&&(header[8]==4||header[8]==8))return header[8];return 0;}
#ifdef DNX2_USE_MBEDTLS
static int derive(const char *pin,const unsigned char *salt,unsigned char key[32]){
    size_t digits=pin?strlen(pin):0;if(digits!=4&&digits!=8)return 0;
    for(size_t i=0;i<digits;i++)if(pin[i]<'0'||pin[i]>'9')return 0;
#if MBEDTLS_VERSION_MAJOR >= 3
    return mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,(const unsigned char*)pin,digits,salt,16,100000,32,key)==0;
#else
    mbedtls_md_context_t md;mbedtls_md_init(&md);
    int rc=mbedtls_md_setup(&md,mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),1);
    if(!rc)rc=mbedtls_pkcs5_pbkdf2_hmac(&md,(const unsigned char*)pin,digits,salt,16,100000,32,key);
    mbedtls_md_free(&md);return rc==0;
#endif
}
static int token_ok(const char *p,size_t n){if(n<16||n>1023)return 0;for(size_t i=0;i<n;i++)if((unsigned char)p[i]<33||(unsigned char)p[i]>126)return 0;return 1;}
int dnx2SessionSave(const char *token,const char *pin){
    unsigned char data[1079]={0},key[32]={0};int ok=0;
    size_t n=token?strlen(token):0;if(!token_ok(token,n))return 0;
    size_t digits=pin?strlen(pin):0;if(digits!=4&&digits!=8)return 0;
    memcpy(data,"DNX2SES2",8);data[8]=(unsigned char)digits;data[37]=(unsigned char)n;data[38]=(unsigned char)(n>>8);
    if(R_FAILED(csrngInitialize()))goto done;
    int rc=csrngGetRandomBytes(data+9,28);csrngExit();if(R_FAILED(rc)||!derive(pin,data+9,key))goto done;
    mbedtls_gcm_context gcm;mbedtls_gcm_init(&gcm);
    int err=mbedtls_gcm_setkey(&gcm,MBEDTLS_CIPHER_ID_AES,key,256);
    if(!err)err=mbedtls_gcm_crypt_and_tag(&gcm,MBEDTLS_GCM_ENCRYPT,n,data+25,12,data,39,(const unsigned char*)token,data+55,16,data+39);
    mbedtls_gcm_free(&gcm);if(err)goto done;
    (void)mkdir("sdmc:/switch",0700);(void)mkdir("sdmc:/switch/DiscordNX2",0700);
    FILE *f=fopen(DNX2_SESSION_PATH ".tmp","wb");if(!f)goto done;
    int written=fwrite(data,1,55+n,f)==55+n;int closed=fclose(f)==0;
    if(written&&closed&&rename(DNX2_SESSION_PATH ".tmp",DNX2_SESSION_PATH)==0)ok=1;
    else remove(DNX2_SESSION_PATH ".tmp");
done:clear(data,sizeof(data));clear(key,sizeof(key));return ok;
}
int dnx2SessionLoad(char *token,size_t cap,const char *pin){
    unsigned char data[1079]={0},key[32]={0};int ok=0;if(!token||!cap)return 0;token[0]=0;
    FILE *f=fopen(DNX2_SESSION_PATH,"rb");if(!f)return 0;size_t got=fread(data,1,sizeof(data),f);int extra=fgetc(f);fclose(f);
    int old=!memcmp(data,"DNX2SES1",8),current=!memcmp(data,"DNX2SES2",8);
    size_t header=old?38:39,salt=old?8:9,nonce=old?24:25,digits=old?8:data[8];
    size_t n=(size_t)data[header-2]|((size_t)data[header-1]<<8);
    if(extra!=EOF||(!old&&!current)||(digits!=4&&digits!=8)||got!=header+16+n||n<16||n>1023||cap<=n||!pin||strlen(pin)!=digits||!derive(pin,data+salt,key))goto done;
    mbedtls_gcm_context gcm;mbedtls_gcm_init(&gcm);int err=mbedtls_gcm_setkey(&gcm,MBEDTLS_CIPHER_ID_AES,key,256);
    if(!err)err=mbedtls_gcm_auth_decrypt(&gcm,n,data+nonce,12,data,header,data+header,16,data+header+16,(unsigned char*)token);
    mbedtls_gcm_free(&gcm);token[n]=0;if(!err&&token_ok(token,n))ok=1;
done:if(!ok)clear(token,cap);clear(data,sizeof(data));clear(key,sizeof(key));return ok;
}
#else
int dnx2SessionSave(const char *t,const char *p){(void)t;(void)p;return 0;}
int dnx2SessionLoad(char *t,size_t n,const char *p){(void)p;if(n)t[0]=0;return 0;}
#endif
