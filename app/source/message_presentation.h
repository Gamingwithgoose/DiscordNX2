// SPDX-License-Identifier: MIT
#ifndef DNX2_MESSAGE_PRESENTATION_H
#define DNX2_MESSAGE_PRESENTATION_H
#include "client_json.h"
static inline int dnx_media_url(const char *url,char host[48],char path[1536]){
    if(!url||strncmp(url,"https://",8))return 0;
    const char *slash=strchr(url+8,'/');if(!slash)return 0;size_t n=(size_t)(slash-url-8);if(!n||n>=48||strlen(slash)>=1536)return 0;
    memcpy(host,url+8,n);host[n]=0;
    if(strcmp(host,"cdn.discordapp.com")&&strcmp(host,"media.discordapp.net")&&strcmp(host,"images-ext-1.discordapp.net")&&strcmp(host,"images-ext-2.discordapp.net")&&strcmp(host,"media.tenor.com")&&strcmp(host,"c.tenor.com")&&strcmp(host,"media.giphy.com")&&strcmp(host,"i.giphy.com"))return 0;
    for(const unsigned char *p=(const unsigned char*)slash;*p;p++)if(*p<33||*p>126||*p=='#')return 0;
    snprintf(path,1536,"%s",slash);return 1;
}
static inline int dnx_message_media(DnxJson value,char *url,size_t cap){
    char candidate[1536]={0},host[48],path[1536];
    if(!dj_get(value,"proxy_url",candidate,sizeof(candidate)))dj_get(value,"url",candidate,sizeof(candidate));
    if(!dnx_media_url(candidate,host,path))return 0;
    int proxy=!strcmp(host,"media.discordapp.net")||!strncmp(host,"images-ext-",11);
    int gif=strstr(path,".gif")!=NULL;int n=snprintf(url,cap,"https://%s%s%s",host,path,proxy?(strchr(path,'?')?"&width=256&height=144":"?width=256&height=144"):"");
    (void)gif;return n>0&&(size_t)n<cap;
}
static inline unsigned dnx_default_avatar(const char *id,const char *discriminator){
    unsigned old=discriminator?(unsigned)strtoul(discriminator,NULL,10):0;
    return old?old%5:(unsigned)((strtoull(id?id:"0",NULL,10)>>22)%6);
}
#endif
