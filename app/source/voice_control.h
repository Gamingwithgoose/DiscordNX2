// SPDX-License-Identifier: MIT
#ifndef DNX2_VOICE_CONTROL_H
#define DNX2_VOICE_CONTROL_H
#include "voice_engine.h"
#include "client_data.h"
typedef struct {char guild[24],channel[24],user[24],session[160],token[512],host[256];unsigned port,state_seen,self_seen,server_seen,server_scoped,invalid_endpoint,null_endpoint,self_disconnected;int have_state,have_server;} DnxVoiceCredentials;
static inline int dnx_voice_atom(const char *s){if(!s||!*s)return 0;for(;*s;s++)if((unsigned char)*s<33||(unsigned char)*s>126||*s=='"'||*s=='\\')return 0;return 1;}
static inline int dnx_voice_endpoint(const char *endpoint,char out[256],unsigned *port){
    if(!endpoint||!*endpoint||strlen(endpoint)>=256)return 0;
    char host[256];snprintf(host,sizeof(host),"%s",endpoint);*port=443;
    char *colon=strchr(host,':');if(colon){const char *p=colon+1;if(!*p)return 0;unsigned value=0;for(;*p;p++){if(*p<'0'||*p>'9')return 0;value=value*10+(unsigned)(*p-'0');if(value>65535)return 0;}if(!value)return 0;*port=value==80?443:value;*colon=0;}
    size_t n=strlen(host);int allowed=(n>14&&!strcmp(host+n-14,".discord.media"))||(n>11&&!strcmp(host+n-11,".discord.gg"));if(!allowed)return 0;
    for(const char *p=host;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='.'||*p=='-'))return 0;
    snprintf(out,256,"%s",host);return 1;
}
static inline int dnx_voice_host(const char *endpoint,char out[256]){unsigned port;return dnx_voice_endpoint(endpoint,out,&port);}
static inline int dnx_voice_subscribe_body(const DnxVoiceCredentials *c,char *out,size_t cap){if(!dnx_id(c->guild))return 0;int n=snprintf(out,cap,"{\"op\":14,\"d\":{\"guild_id\":\"%s\",\"typing\":false,\"activities\":false,\"threads\":false,\"channels\":{}}}",c->guild);return n>0&&(size_t)n<cap;}
static inline int dnx_voice_state_body(const DnxVoiceCredentials *c,int leave,char *out,size_t cap){
    if(!dnx_id(c->guild)||(!leave&&!dnx_id(c->channel)))return 0;
    int n=leave?snprintf(out,cap,"{\"op\":4,\"d\":{\"guild_id\":\"%s\",\"channel_id\":null,\"self_mute\":true,\"self_deaf\":true,\"self_video\":false}}",c->guild):snprintf(out,cap,"{\"op\":4,\"d\":{\"guild_id\":\"%s\",\"channel_id\":\"%s\",\"self_mute\":true,\"self_deaf\":true,\"self_video\":false}}",c->guild,c->channel);
    return n>0&&(size_t)n<cap;
}
static inline int dnx_voice_event(DnxVoiceCredentials *c,const char *kind,DnxJson data){
    char guild[24]={0};int has_guild=dj_get(data,"guild_id",guild,sizeof(guild));
    if(!strcmp(kind,"VOICE_STATE_UPDATE")){
        c->state_seen++;char user[24],channel[24],session[160];
        if(!dj_get(data,"user_id",user,sizeof(user))||strcmp(user,c->user))return 0;
        c->self_seen++;
        if(has_guild&&strcmp(guild,c->guild))return 0;
        DnxJson target=dj_field(data,"channel_id");if(has_guild&&target.p&&*target.p=='n'){c->self_disconnected++;return 0;}
        /* Omitted guild_id is safe only with this account AND exact channel. */
        if(!dj_get(data,"channel_id",channel,sizeof(channel))||strcmp(channel,c->channel)||!dj_get(data,"session_id",session,sizeof(session))||!dnx_voice_atom(session))return 0;
        snprintf(c->session,sizeof(c->session),"%s",session);c->have_state=1;return 1;
    }
    if(!strcmp(kind,"VOICE_SERVER_UPDATE")){
        c->server_seen++;if(!has_guild||strcmp(guild,c->guild))return 0;
        c->server_scoped++;
        char endpoint[256],token[512],host[256];unsigned port;DnxJson value=dj_field(data,"endpoint");
        if(value.p&&*value.p=='n'){c->null_endpoint++;c->have_server=0;memset(c->token,0,sizeof(c->token));return 0;}
        if(!dj_get(data,"endpoint",endpoint,sizeof(endpoint))||!dnx_voice_endpoint(endpoint,host,&port)){c->invalid_endpoint++;return 0;}
        if(!dj_get(data,"token",token,sizeof(token))||!dnx_voice_atom(token))return 0;
        snprintf(c->host,sizeof(c->host),"%s",host);snprintf(c->token,sizeof(c->token),"%s",token);c->port=port;c->have_server=1;return 1;
    }return 0;
}
static inline const char *dnx_voice_confirmation_status(const DnxVoiceCredentials *c){
    if(!c->have_state&&c->self_disconnected)return "Discord returned a disconnected voice state. Try another voice channel.";
    if(!c->have_server&&c->invalid_endpoint)return "Discord sent an unsupported voice endpoint. See the voice report.";
    if(!c->have_server&&c->null_endpoint)return "Discord is allocating a voice server. Retry the check shortly.";
    if(!c->have_state&&!c->have_server)return "Gateway is ready, but neither voice confirmation arrived.";
    if(!c->have_state)return "Voice server received, but this account's voice state was not confirmed.";
    return "Voice state confirmed, but Discord did not provide a voice server.";
}
static inline int dnx_voice_identify_body(const DnxVoiceCredentials *c,char *out,size_t cap){
    if(!c->have_state||!c->have_server||!dnx_id(c->user)||!dnx_voice_atom(c->session)||!dnx_voice_atom(c->token))return 0;
    /* Advertise the engine linked into this NRO. */
    int n=snprintf(out,cap,"{\"op\":0,\"d\":{\"server_id\":\"%s\",\"user_id\":\"%s\",\"session_id\":\"%s\",\"token\":\"%s\",\"max_dave_protocol_version\":%u}}",c->guild,c->user,c->session,c->token,dnx_voice_max_version());return n>0&&(size_t)n<cap;
}
#endif
