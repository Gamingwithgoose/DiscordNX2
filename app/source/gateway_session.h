// SPDX-License-Identifier: MIT
#ifndef DNX2_GATEWAY_SESSION_H
#define DNX2_GATEWAY_SESSION_H
#include "client_json.h"
static inline int dnx_gateway_host(DnxJson data,char host[256]){char url[300];if(!dj_get(data,"resume_gateway_url",url,sizeof(url))||strncmp(url,"wss://",6))return 0;char *p=url+6,*slash=strchr(p,'/');if(slash){if(strcmp(slash,"/"))return 0;*slash=0;}size_t n=strlen(p);if(n<12||n>=256||(strcmp(p,"gateway.discord.gg")&&(n<=11||strcmp(p+n-11,".discord.gg"))))return 0;for(const char *q=p;*q;q++)if(!((*q>='a'&&*q<='z')||(*q>='0'&&*q<='9')||*q=='-'||*q=='.'))return 0;snprintf(host,256,"%s",p);return 1;}
#endif
