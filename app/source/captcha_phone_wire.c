// SPDX-License-Identifier: MIT
#include "captcha_phone_wire.h"
#include <string.h>
#include <ctype.h>
static int equal(const char *s,size_t n,const char *key) {
    if(n!=strlen(key))return 0;
    for(size_t i=0;i<n;i++)if(tolower((unsigned char)s[i])!=tolower((unsigned char)key[i]))return 0;
    return 1;
}
int dnx2PhonePrivateIp(uint32_t ip) {
    return (ip>>24)==10 || (ip>>20)==0xac1 || (ip>>16)==0xc0a8;
}
int dnx2PhoneRequest(const char *s,size_t n,const char **payload,size_t *length) {
    if(!s||!payload||!length||n>DNX2_PHONE_REQUEST_MAX)return -1;
    *payload=NULL;*length=0;
    if(memchr(s,0,n))return -1;
    size_t end=0;
    for(size_t i=3;i<n;i++)if(!memcmp(s+i-3,"\r\n\r\n",4)){end=i+1;break;}
    if(!end)return n>4096?-1:0;
    if(end>4096)return -1;
    const char *first="POST /captcha-return HTTP/1.1\r\n";
    size_t pos=strlen(first);
    if(end<pos || memcmp(s,first,pos))return -1;
    size_t body=0;int have_length=0,have_type=0;
    while(pos<end-2) {
        size_t line=pos;
        while(pos+1<end && memcmp(s+pos,"\r\n",2))pos++;
        if(pos+1>=end || s[line]==' '||s[line]=='\t')return -1;
        const char *colon=memchr(s+line,':',pos-line);
        if(!colon)return -1;
        const char *value=colon+1,*limit=s+pos;
        while(value<limit&&(*value==' '||*value=='\t'))value++;
        while(limit>value&&(limit[-1]==' '||limit[-1]=='\t'))limit--;
        if(equal(s+line,(size_t)(colon-s-line),"transfer-encoding"))return -1;
        if(equal(s+line,(size_t)(colon-s-line),"content-length")) {
            if(have_length++||value==limit)return -1;
            for(;value<limit;value++) {
                if(*value<'0'||*value>'9'||body>DNX2_PHONE_REQUEST_MAX/10)return -1;
                body=body*10+(size_t)(*value-'0');
            }
        } else if(equal(s+line,(size_t)(colon-s-line),"content-type")) {
            const char *type="application/x-www-form-urlencoded";
            size_t size=strlen(type);
            if(have_type++||(size_t)(limit-value)<size||!equal(value,size,type)||
               ((size_t)(limit-value)>size&&value[size]!=';'))return -1;
        }
        pos+=2;
    }
    if(!have_length||!have_type||body<=8||body>DNX2_PHONE_PAYLOAD_MAX+8||body>DNX2_PHONE_REQUEST_MAX-end)return -1;
    if(n<end+body)return 0;
    if(n!=end+body||memcmp(s+end,"payload=",8))return -1;
    for(size_t i=end+8;i<n;i++)if(!isalnum((unsigned char)s[i])&&s[i]!='-'&&s[i]!='_')return -1;
    *payload=s+end+8;*length=body-8;return 1;
}
