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
    if(!dnx_media_url(candidate,host,path)){if(!dj_get(value,"url",candidate,sizeof(candidate))||!dnx_media_url(candidate,host,path))return 0;}
    if(!strcmp(host,"cdn.discordapp.com")&&!strncmp(path,"/attachments/",13))snprintf(host,sizeof(host),"media.discordapp.net");
    int proxy=!strcmp(host,"media.discordapp.net")||!strncmp(host,"images-ext-",11);
    /* Preserve signed query values, replace old transform parameters, and
     * request dimensions with the source ratio. A forced 640x360 transform can crop portraits. */
    char clean[1536];size_t used=0;const char *query=strchr(path,'?');size_t base=query?(size_t)(query-path):strlen(path);
    memcpy(clean,path,base);used=base;clean[used]=0;
    if(query){const char *p=query+1;while(*p){const char *end=strchr(p,'&');if(!end)end=p+strlen(p);const char *eq=memchr(p,'=',(size_t)(end-p));size_t key=eq?(size_t)(eq-p):(size_t)(end-p);
        int transform=(key==5&&!strncmp(p,"width",5))||(key==6&&!strncmp(p,"height",6))||(key==6&&!strncmp(p,"format",6))||(key==3&&!strncmp(p,"fit",3));
        if(!proxy||!transform){size_t len=(size_t)(end-p);if(used+len+2>=sizeof(clean))return 0;char separator=used==base?'?':'&';clean[used++]=separator;memcpy(clean+used,p,len);used+=len;clean[used]=0;}p=*end?end+1:end;
    }}
    int width=dj_int(value,"width",0),height=dj_int(value,"height",0);char transform[160]={0};
    if(proxy){if(width>0&&height>0&&width<=65536&&height<=65536){if(width>640){height=(int)((long long)height*640/width);width=640;}if(height>360){width=(int)((long long)width*360/height);height=360;}if(width<1)width=1;if(height<1)height=1;snprintf(transform,sizeof(transform),"%cwidth=%d&height=%d&fit=contain",strchr(clean,'?')?'&':'?',width,height);}
        else snprintf(transform,sizeof(transform),"%cwidth=640&fit=contain",strchr(clean,'?')?'&':'?');
        if(!strstr(clean,".gif"))strcat(transform,"&format=png");}
    int n=snprintf(url,cap,"https://%s%s%s",host,clean,transform);
    return n>0&&(size_t)n<cap;
}
static inline unsigned dnx_default_avatar(const char *id,const char *discriminator){
    unsigned old=discriminator?(unsigned)strtoul(discriminator,NULL,10):0;
    return old?old%5:(unsigned)((strtoull(id?id:"0",NULL,10)>>22)%6);
}
#endif
