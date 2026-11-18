// SPDX-License-Identifier: MIT
#ifndef DNX2_CHAT_MEDIA_DATA_H
#define DNX2_CHAT_MEDIA_DATA_H
#include "client_data.h"
#include "chat_assets.h"
static inline int dnx_asset_hash(const char *p){size_t n=p?strlen(p):0;if(n!=32&&n!=34)return 0;if(n==34){if(p[0]!='a'||p[1]!='_')return 0;p+=2;}for(;*p;p++)if(!(*p>='0'&&*p<='9')&&!(*p>='a'&&*p<='f'))return 0;return 1;}
static inline size_t dnx_custom_emoji(const char *text,char id[24],char name[33]){
    if(!text||text[0]!='<')return 0;
    const char *p=text+1;if(*p=='a')p++;if(*p++!=':')return 0;
    size_t n=0;while(*p&&*p!=':'&&n<32){char c=*p++;if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return 0;name[n++]=c;}
    name[n]=0;if(!n||*p++!=':')return 0;n=0;while(*p>='0'&&*p<='9'&&n<23)id[n++]=*p++;id[n]=0;
    if(*p!='>'||!dnx_id(id))return 0;
    return (size_t)(p-text)+1;
}
static inline int dnx_media_parse(Dnx2Media *out,const char *json,int sticker){
    DnxJson root=dj_root(json),e;const char *p=NULL;int count=0;
    while(count<DNX2_MEDIA_MAX&&dj_next(root,&p,&e)){
        DnxJson available=dj_field(e,"available");if(available.p&&!strncmp(available.p,"false",5))continue;
        Dnx2Media item={0};if(!dj_get(e,"id",item.id,sizeof(item.id))||!dnx_id(item.id)||!dj_get(e,"name",item.name,sizeof(item.name)))continue;
        DnxJson animated=dj_field(e,"animated");item.animated=animated.p&&!strncmp(animated.p,"true",4);
        item.format=sticker?dj_int(e,"format_type",0):1;
        if(!sticker){int valid=1;for(const char *s=item.name;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='_'))valid=0;if(!valid||!item.name[0])continue;}
        out[count++]=item;
    }return count;
}
static inline int dnx_media_body(const char *text,const char *sticker,char *out,size_t cap){
    if(!sticker||!*sticker)return dnx_message_body(text,out,cap);
    if(!dnx_id(sticker)||!text||strlen(text)>512||cap<3300)return 0;
    char base[3200];if(!dnx_message_body(*text?text:" ",base,sizeof(base)))return 0;
    size_t n=strlen(base);if(!n)return 0;base[n-1]=0;
    int result=snprintf(out,cap,"%s,\"sticker_ids\":[\"%s\"]}",base,sticker);return result>0&&(size_t)result<cap;
}
#endif
