// SPDX-License-Identifier: MIT
#ifndef DNX2_VOICE_CONTROL_H
#define DNX2_VOICE_CONTROL_H
#include "client_data.h"
typedef struct {char guild[24],channel[24],user[24],session[160],token[512],host[256];int have_state,have_server;} DnxVoiceCredentials;
static inline int dnx_voice_atom(const char *s){if(!s||!*s)return 0;for(;*s;s++)if((unsigned char)*s<33||(unsigned char)*s>126||*s=='"'||*s=='\\')return 0;return 1;}
static inline int dnx_voice_host(const char *endpoint,char out[256]){
    if(!endpoint||!*endpoint||strlen(endpoint)>=256)return 0;
    char host[256];snprintf(host,sizeof(host),"%s",endpoint);
    char *colon=strchr(host,':');if(colon){if(strcmp(colon,":443"))return 0;*colon=0;}
    size_t n=strlen(host);int allowed=(n>14&&!strcmp(host+n-14,".discord.media"))||(n>11&&!strcmp(host+n-11,".discord.gg"));
    if(!allowed)return 0;
    for(const char *p=host;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='.'||*p=='-'))return 0;
    snprintf(out,256,"%s",host);return 1;
}
static inline int dnx_voice_state_body(const DnxVoiceCredentials *c,int leave,char *out,size_t cap){
    if(!dnx_id(c->guild)||(!leave&&!dnx_id(c->channel)))return 0;
    int n=leave?snprintf(out,cap,"{\"op\":4,\"d\":{\"guild_id\":\"%s\",\"channel_id\":null,\"self_mute\":true,\"self_deaf\":true}}",c->guild):snprintf(out,cap,"{\"op\":4,\"d\":{\"guild_id\":\"%s\",\"channel_id\":\"%s\",\"self_mute\":true,\"self_deaf\":true}}",c->guild,c->channel);
    return n>0&&(size_t)n<cap;
}
static inline int dnx_voice_event(DnxVoiceCredentials *c,const char *kind,DnxJson data){
    char guild[24];if(!dj_get(data,"guild_id",guild,sizeof(guild))||strcmp(guild,c->guild))return 0;
    if(!strcmp(kind,"VOICE_STATE_UPDATE")){
        char user[24],channel[24],session[160];
        if(!dj_get(data,"user_id",user,sizeof(user))||strcmp(user,c->user)||!dj_get(data,"channel_id",channel,sizeof(channel))||strcmp(channel,c->channel)||!dj_get(data,"session_id",session,sizeof(session))||!dnx_voice_atom(session))return 0;
        snprintf(c->session,sizeof(c->session),"%s",session);c->have_state=1;return 1;
    }
    if(!strcmp(kind,"VOICE_SERVER_UPDATE")){
        char endpoint[256],token[512],host[256];
        if(!dj_get(data,"endpoint",endpoint,sizeof(endpoint))||!dnx_voice_host(endpoint,host)||!dj_get(data,"token",token,sizeof(token))||!dnx_voice_atom(token))return 0;
        snprintf(c->host,sizeof(c->host),"%s",host);snprintf(c->token,sizeof(c->token),"%s",token);c->have_server=1;return 1;
    }return 0;
}
static inline int dnx_voice_identify_body(const DnxVoiceCredentials *c,char *out,size_t cap){
    if(!c->have_state||!c->have_server||!dnx_id(c->user)||!dnx_voice_atom(c->session)||!dnx_voice_atom(c->token))return 0;
    /* This build has no DAVE/Opus engine: never advertise support it lacks. */
    int n=snprintf(out,cap,"{\"op\":0,\"d\":{\"server_id\":\"%s\",\"user_id\":\"%s\",\"session_id\":\"%s\",\"token\":\"%s\",\"max_dave_protocol_version\":0}}",c->guild,c->user,c->session,c->token);return n>0&&(size_t)n<cap;
}
#endif
