// SPDX-License-Identifier: MIT
#include "voice_control.h"
#include "gateway_session.h"
#include <assert.h>
#include <stdint.h>
typedef uint64_t u64;
typedef int (*Dnx2AuthCancel)(void);
typedef struct {int kind,socket_fd,returned_fd;} TlsSession;
#define R_FAILED(x) ((x)!=0)
#define DNX2_WS_MAX 8192
static struct {unsigned close_code;} ws_reader;
static char live_token[1024],live_session[160]="main-session",live_resume_host[256]="gateway-region.discord.gg";
static long long live_sequence=1;
static Dnx2AuthCancel active_cancel;
static u64 live_identify_after;
static void svcSleepThread(long long n){(void)n;}
static u64 now;static int rng,main_frame,voice_frame,joins,leaves,voice_identifies,cancelled,invalid_endpoint,scenario,subscriptions,used_port;
static u64 dnx2MonotonicMs(void){now+=10;return now;}
static void wipe(void *p,size_t n){memset(p,0,n);}
static int csrngInitialize(void){rng++;return 0;}
static void csrngExit(void){rng--;}
static int tls_open(TlsSession *s,const char *host){s->kind=strstr(host,"gateway")?1:2;if(s->kind==2)assert(!strcmp(host,"voice-test.discord.media"));return 1;}
static int tls_open_port(TlsSession *s,const char *host,unsigned port){assert(port==443||port==2048);used_port=(int)port;return tls_open(s,host);}
static int ws_connect_to(TlsSession *s,const char *host,const char *path){(void)host;assert(!strcmp(path,s->kind==1?"/?v=10&encoding=json":"/?v=8"));return 1;}
static int ws_connect_to_port(TlsSession *s,const char *host,unsigned port,const char *path){assert((int)port==used_port);return ws_connect_to(s,host,path);}
static void tls_close(TlsSession *s){(void)s;}
static int live_identify(TlsSession *s){(void)s;assert(!strcmp(live_token,"synthetic-account-token"));return 1;}
static int live_heartbeat(TlsSession *s){(void)s;return 1;}
static int ws_text(TlsSession *s,const char *body){
    DnxJson root=dj_root(body),data=dj_field(root,"d");int op=dj_int(root,"op",-1);
    if(s->kind==1){if(op==14){subscriptions++;assert(dj_field(data,"channels").p);return 1;}assert(op==4&&subscriptions>0);DnxJson channel=dj_field(data,"channel_id");if(channel.p&&*channel.p=='n')leaves++;else joins++;assert(strstr(body,"\"self_mute\":true")&&strstr(body,"\"self_deaf\":true"));}
    else {assert(op==0||op==3);if(op==0){voice_identifies++;assert(dj_int(data,"max_dave_protocol_version",-1)==1);}}
    return 1;
}
static int cancel(void){return cancelled;}
static int ws_receive(TlsSession *s,unsigned char *out,size_t cap,u64 deadline){
    (void)deadline;const char *event=NULL;
    if(s->kind==1){
        const char *events[]={"{\"op\":10,\"d\":{\"heartbeat_interval\":40000}}","{\"op\":0,\"s\":1,\"t\":\"RESUMED\",\"d\":{}}","{\"op\":0,\"s\":2,\"t\":\"VOICE_STATE_UPDATE\",\"d\":{\"guild_id\":\"100000000000000001\",\"channel_id\":\"200000000000000001\",\"user_id\":\"300000000000000001\",\"session_id\":\"voice-session\"}}","{\"op\":0,\"s\":3,\"t\":\"VOICE_SERVER_UPDATE\",\"d\":{\"guild_id\":\"100000000000000001\",\"endpoint\":\"voice-test.discord.media:443\",\"token\":\"synthetic-voice-token\"}}"};
        if(main_frame>=4||(scenario==3&&main_frame>=2)||(scenario==4&&main_frame>=3))return 0;
        event=events[main_frame++];
        if(scenario==2&&main_frame==3)event="{\"op\":0,\"t\":\"VOICE_STATE_UPDATE\",\"d\":{\"channel_id\":\"200000000000000001\",\"user_id\":\"300000000000000001\",\"session_id\":\"voice-session\"}}";
        if(scenario==7&&main_frame==4)event="{\"op\":0,\"t\":\"VOICE_SERVER_UPDATE\",\"d\":{\"guild_id\":\"100000000000000001\",\"endpoint\":\"voice-test.discord.media:2048\",\"token\":\"synthetic-voice-token\"}}";
        if(invalid_endpoint&&main_frame==4)event="{\"op\":0,\"t\":\"VOICE_SERVER_UPDATE\",\"d\":{\"guild_id\":\"100000000000000001\",\"endpoint\":\"127.0.0.1:443\",\"token\":\"x\"}}";
    }else{
        if(voice_frame++){ws_reader.close_code=4017;return -2;}event="{\"op\":8,\"d\":{\"heartbeat_interval\":40000}}";
    }
    assert(strlen(event)<cap);strcpy((char*)out,event);return (int)strlen(event);
}
#define DNX2_VOICE_RUNTIME_EXTERNAL
unsigned dnx_voice_max_version(void){return 1;}
static int voice_media_session(TlsSession *v,TlsSession *g,const DnxVoiceCredentials *c,unsigned char *event,char notice[160],int *stage,unsigned *code,u64 interval,u64 beat){
 (void)g;(void)c;(void)interval;(void)beat;int n;do{n=ws_receive(v,event,DNX2_WS_MAX+1,now+100);}while(n>0);if(n<0)*code=ws_reader.close_code;snprintf(notice,160,"DAVE test media stub");(void)stage;return 0;
}
#include "voice_connect.inc"
int main(void){
    char host[256];assert(dnx_voice_host("voice-test.discord.media:443",host));assert(!dnx_voice_host("voice-test.discord.media.evil:443",host));assert(!dnx_voice_host("127.0.0.1:443",host));assert(dnx_voice_host("voice-test.discord.media:1234",host));unsigned port;assert(dnx_voice_endpoint("voice-test.discord.media:80",host,&port)&&port==443);assert(!dnx_voice_endpoint("voice-test.discord.media:0",host,&port));assert(!dnx_voice_endpoint("voice-test.discord.media:65536",host,&port));assert(!dnx_voice_endpoint("voice-test.discord.media:443@evil",host,&port));
    DnxVoiceCredentials scope={0};snprintf(scope.guild,24,"100000000000000001");snprintf(scope.channel,24,"200000000000000001");snprintf(scope.user,24,"300000000000000001");
    assert(!dnx_voice_event(&scope,"VOICE_STATE_UPDATE",dj_root("{\"channel_id\":\"200000000000000002\",\"user_id\":\"300000000000000001\",\"session_id\":\"x\"}")));
    assert(!dnx_voice_event(&scope,"VOICE_STATE_UPDATE",dj_root("{\"guild_id\":\"100000000000000002\",\"channel_id\":\"200000000000000001\",\"user_id\":\"300000000000000001\",\"session_id\":\"x\"}")));
    assert(!dnx_voice_event(&scope,"VOICE_SERVER_UPDATE",dj_root("{\"endpoint\":\"voice-test.discord.media:443\",\"token\":\"x\"}")));
    assert(!dnx_voice_event(&scope,"VOICE_SERVER_UPDATE",dj_root("{\"guild_id\":\"100000000000000001\",\"endpoint\":null,\"token\":\"x\"}"))&&scope.null_endpoint==1&&strstr(dnx_voice_confirmation_status(&scope),"allocating"));
    assert(!dnx_voice_event(&scope,"VOICE_STATE_UPDATE",dj_root("{\"guild_id\":\"100000000000000001\",\"channel_id\":null,\"user_id\":\"300000000000000001\",\"session_id\":\"x\"}"))&&scope.self_disconnected==1);
    assert(strstr(dnx_voice_confirmation_status(&scope),"disconnected"));scope=(DnxVoiceCredentials){.have_server=1};assert(strstr(dnx_voice_confirmation_status(&scope),"voice state"));
    char notice[160];int stage;unsigned close_code;active_cancel=cancel;
    voice_control_session("synthetic-account-token","100000000000000001","200000000000000001","300000000000000001",notice,&stage,&close_code);
    assert(stage==4&&close_code==4017&&joins==1&&leaves==1&&voice_identifies==1&&!rng);assert(strstr(notice,"DAVE")&&!live_token[0]);
    main_frame=voice_frame=joins=leaves=voice_identifies=0;invalid_endpoint=1;
    voice_control_session("synthetic-account-token","100000000000000001","200000000000000001","300000000000000001",notice,&stage,&close_code);assert(stage==2&&joins==1&&leaves==1&&voice_identifies==0&&!rng);
    main_frame=voice_frame=joins=leaves=voice_identifies=0;invalid_endpoint=0;live_session[0]=0;live_sequence=-1;u64 allowed=now+1000;live_identify_after=allowed;
    voice_control_session("synthetic-account-token","100000000000000001","200000000000000001","300000000000000001",notice,&stage,&close_code);assert(now>=allowed&&joins==1&&leaves==1);
    main_frame=voice_frame=joins=leaves=0;live_session[0]=0;live_identify_after=now+1000;cancelled=1;voice_control_session("synthetic-account-token","100000000000000001","200000000000000001","300000000000000001",notice,&stage,&close_code);assert(!joins&&!leaves&&!rng&&strstr(notice,"cancelled"));
    cancelled=0;invalid_endpoint=0;live_identify_after=0;
    for(int which=2;which<=7;which++){if(which==5||which==6)continue;scenario=which;main_frame=voice_frame=joins=leaves=voice_identifies=0;voice_control_session("synthetic-account-token","100000000000000001","200000000000000001","300000000000000001",notice,&stage,&close_code);assert(joins==1&&leaves==1&&!rng);
        if(which==3)assert(stage==2&&!voice_identifies&&strstr(notice,"neither"));
        else if(which==4)assert(stage==2&&!voice_identifies&&strstr(notice,"did not provide"));
        else assert(stage==4&&voice_identifies==1);
        if(which==7)assert(used_port==2048);
    }
    DnxVoiceCredentials c={0};snprintf(c.guild,24,"100000000000000001");snprintf(c.channel,24,"200000000000000001");snprintf(c.user,24,"300000000000000001");
    assert(!dnx_voice_event(&c,"VOICE_STATE_UPDATE",dj_root("{\"channel_id\":\"200000000000000002\",\"user_id\":\"300000000000000001\",\"session_id\":\"x\"}"))&&!c.have_state);
    assert(!dnx_voice_event(&c,"VOICE_SERVER_UPDATE",dj_root("{\"guild_id\":\"100000000000000002\",\"endpoint\":\"voice-test.discord.media\",\"token\":\"x\"}"))&&!c.have_server);
    assert(!dnx_voice_event(&c,"VOICE_SERVER_UPDATE",dj_root("{\"guild_id\":\"100000000000000001\",\"endpoint\":null,\"token\":\"x\"}"))&&c.null_endpoint==1&&strstr(dnx_voice_confirmation_status(&c),"allocating"));
    c.null_endpoint=0;c.have_server=1;assert(strstr(dnx_voice_confirmation_status(&c),"account's voice state"));
    puts("PASS production voice signaling: scoped and guild-less states, missing confirmations, guild subscription, approved TLS ports, muted join, linked DAVE version identify, 4017 handling, guaranteed leave attempt and credential cleanup; isolated media stub");
}
