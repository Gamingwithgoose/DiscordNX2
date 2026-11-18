// SPDX-License-Identifier: MIT
#ifndef DNX2_CLIENT_JSON_H
#define DNX2_CLIENT_JSON_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
typedef struct {const char *p,*end;} DnxJson;
static inline const char *dj_space(const char *p){while(*p==' '||*p=='\r'||*p=='\n'||*p=='\t')p++;return p;}
static inline const char *dj_end(const char *p,int depth){
    p=dj_space(p);if(depth>32||!*p)return NULL;
    if(*p=='"'){for(p++;*p;p++){if(*p=='"')return p+1;if((unsigned char)*p<32)return NULL;if(*p=='\\'){p++;if(!*p)return NULL;}}return NULL;}
    if(*p=='{'||*p=='['){char close=*p=='{'?'}':']';p=dj_space(p+1);if(*p==close)return p+1;
        for(;;){if(close=='}'){if(*p!='"')return NULL;p=dj_end(p,depth+1);if(!p)return NULL;p=dj_space(p);if(*p++!=':')return NULL;}
            p=dj_end(p,depth+1);if(!p)return NULL;p=dj_space(p);if(*p==close)return p+1;if(*p++!=',')return NULL;p=dj_space(p);}}
    const char *start=p;while(*p&&*p!=','&&*p!=']'&&*p!='}'&&*p!=' '&&*p!='\r'&&*p!='\n'&&*p!='\t')p++;return p>start?p:NULL;
}
static inline int dj_hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
static inline int dj_u16(const char *p,uint32_t *v){*v=0;for(int i=0;i<4;i++){int h=dj_hex(p[i]);if(h<0)return 0;*v=(*v<<4)|(unsigned)h;}return 1;}
static inline size_t dj_utf8(uint32_t c,char out[4]){
    if(c<0x80){out[0]=(char)c;return 1;}if(c<0x800){out[0]=(char)(0xc0|(c>>6));out[1]=(char)(0x80|(c&63));return 2;}
    if(c<0x10000){out[0]=(char)(0xe0|(c>>12));out[1]=(char)(0x80|((c>>6)&63));out[2]=(char)(0x80|(c&63));return 3;}
    out[0]=(char)(0xf0|(c>>18));out[1]=(char)(0x80|((c>>12)&63));out[2]=(char)(0x80|((c>>6)&63));out[3]=(char)(0x80|(c&63));return 4;
}
static inline uint32_t dnx_utf8_next(const char **cursor){
    const unsigned char *p=(const unsigned char*)*cursor;unsigned c=*p++;if(c<128){*cursor=(const char*)p;return c;}
    unsigned n=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:0;
    if(!n){*cursor=(const char*)p;return 0xfffd;}uint32_t v=c&((1u<<(6-n))-1);
    for(unsigned i=0;i<n;i++){if((p[i]&0xc0)!=0x80){*cursor=(const char*)p;return 0xfffd;}v=(v<<6)|(p[i]&63);}
    *cursor=(const char*)(p+n);if(v>0x10ffff||(v>=0xd800&&v<=0xdfff)||(n==1&&v<0x80)||(n==2&&v<0x800)||(n==3&&v<0x10000))return 0xfffd;return v;
}
static inline int dj_string(DnxJson j,char *out,size_t cap){
    if(!cap)return 0;
    out[0]=0;if(!j.p||!j.end||*j.p!='"')return 0;
    const char *p=j.p+1,*end=j.end-1;size_t used=0;
    while(p<end){char bytes[4];size_t n=1;unsigned char c=(unsigned char)*p++;
        if(c=='\\'){if(p>=end)return 0;c=(unsigned char)*p++;
            if(c=='u'){uint32_t cp,low;if(end-p<4||!dj_u16(p,&cp))return 0;p+=4;
                if(cp>=0xd800&&cp<=0xdbff){if(end-p<6||p[0]!='\\'||p[1]!='u'||!dj_u16(p+2,&low)||low<0xdc00||low>0xdfff)return 0;p+=6;cp=0x10000+((cp-0xd800)<<10)+low-0xdc00;}
                else if(cp>=0xdc00&&cp<=0xdfff)return 0;
                if(cp<32)cp=' ';
                n=dj_utf8(cp,bytes);
            }else {if(c=='n'||c=='r'||c=='t'||c=='b'||c=='f')c=' ';else if(c!='"'&&c!='\\'&&c!='/')return 0;bytes[0]=(char)c;}
        }else if(c>=128){const char *q=p-1;uint32_t cp=dnx_utf8_next(&q);if(q>end)return 0;p=q;n=dj_utf8(cp,bytes);}
        else {if(c<32)return 0;bytes[0]=(char)c;}
        if(used+n>=cap){out[used]=0;return 0;}memcpy(out+used,bytes,n);used+=n;
    }out[used]=0;return 1;
}
static inline DnxJson dj_field(DnxJson obj,const char *key){
    DnxJson none={0};if(!obj.p||*obj.p!='{')return none;const char *p=dj_space(obj.p+1);
    while(p<obj.end&&*p!='}'){const char *e=dj_end(p,0);if(!e||e>obj.end)return none;char k[80];if(!dj_string((DnxJson){p,e},k,sizeof(k)))return none;
        p=dj_space(e);if(*p++!=':')return none;p=dj_space(p);e=dj_end(p,0);if(!e||e>obj.end)return none;
        if(!strcmp(k,key))return (DnxJson){p,e};
        p=dj_space(e);if(*p==',')p=dj_space(p+1);else break;
    }return none;
}
static inline DnxJson dj_root(const char *p){const char *e=p?dj_end(p,0):NULL;return (DnxJson){e?dj_space(p):NULL,e};}
static inline int dj_get(DnxJson j,const char *key,char *out,size_t cap){return dj_string(dj_field(j,key),out,cap);}
static inline int dj_int(DnxJson j,const char *key,int fallback){DnxJson v=dj_field(j,key);if(!v.p)return fallback;char *e;long n=strtol(v.p,&e,10);return e==v.end&&n>=-2147483647&&n<=2147483647?(int)n:fallback;}
static inline int dj_next(DnxJson array,const char **cursor,DnxJson *entry){
    if(!array.p||*array.p!='[')return 0;
    const char *p=*cursor?*cursor:array.p+1;p=dj_space(p);if(*p==',')p=dj_space(p+1);if(*p==']'||p>=array.end)return 0;
    const char *e=dj_end(p,0);if(!e||e>array.end)return 0;*entry=(DnxJson){p,e};*cursor=e;return 1;
}
#endif
