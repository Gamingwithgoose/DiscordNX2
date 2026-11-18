// SPDX-License-Identifier: MIT
#include "voice_control.h"
#include <assert.h>
#include <stdint.h>
typedef uint64_t u64;
typedef int (*Dnx2AuthCancel)(void);
typedef struct {int kind,socket_fd,returned_fd;} TlsSession;
#define R_FAILED(x) ((x)!=0)
#define DNX2_WS_MAX 8192
static struct {unsigned close_code;} ws_reader;
static char live_token[1024],live_session[160]="main-session";
static long long live_sequence=1;
static Dnx2AuthCancel active_cancel;
static u64 now;static int rng,main_frame,voice_frame,joins,leaves,voice_identifies,cancelled,invalid_endpoint;
static u64 dnx2MonotonicMs(void){now+=10;return now;}
static void wipe(void *p,size_t n){memset(p,0,n);}
static int csrngInitialize(void){rng++;return 0;}
static void csrngExit(void){rng--;}
static int tls_open(TlsSession *s,const char *host){s->kind=!strcmp(host,"gateway.discord.gg")?1:2;if(s->kind==2)assert(!strcmp(host,"voice-test.discord.media"));return 1;}
static int ws_connect_to(TlsSession *s,const char *host,const char *path){(void)host;assert(!strcmp(path,s->kind==1?"/?v=10&encoding=json":"/?v=8"));return 1;}
static void tls_close(TlsSession *s){(void)s;}
static int live_identify(TlsSession *s){(void)s;assert(!strcmp(live_token,"synthetic-account-token"));return 1;}
static int live_heartbeat(TlsSession *s){(void)s;return 1;}
static int ws_text(TlsSession *s,const char *body){
    DnxJson root=dj_root(body),data=dj_field(root,"d");int op=dj_int(root,"op",-1);
    if(s->kind==1){assert(op==4);DnxJson channel=dj_field(data,"channel_id");if(channel.p&&*channel.p=='n')leaves++;else joins++;assert(strstr(body,"\"self_mute\":true")&&strstr(body,"\"self_deaf\":true"));}
    else {assert(op==0||op==3);if(op==0){voice_identifies++;assert(dj_int(data,"max_dave_protocol_version",-1)==0);}}
    return 1;
}
static int cancel(void){return cancelled;}
static int ws_receive(TlsSession *s,unsigned char *out,size_t cap,u64 deadline){
    (void)deadline;const char *event=NULL;
    if(s->kind==1){
        const char *events[]={"{\"op\":10,\"d\":{\"heartbeat_interval\":40000}}","{\"op\":0,\"s\":1,\"t\":\"RESUMED\",\"d\":{}}","{\"op\":0,\"s\":2,\"t\":\"VOICE_STATE_UPDATE\",\"d\":{\"guild_id\":\"100000000000000001\",\"channel_id\":\"200000000000000001\",\"user_id\":\"300000000000000001\",\"session_id\":\"voice-session\"}}","{\"op\":0,\"s\":3,\"t\":\"VOICE_SERVER_UPDATE\",\"d\":{\"guild_id\":\"100000000000000001\",\"endpoint\":\"voice-test.discord.media:443\",\"token\":\"synthetic-voice-token\"}}"};
        if(main_frame>=4)return 0;
        event=events[main_frame++];
        if(invalid_endpoint&&main_frame==4)event="{\"op\":0,\"t\":\"VOICE_SERVER_UPDATE\",\"d\":{\"guild_id\":\"100000000000000001\",\"endpoint\":\"127.0.0.1:443\",\"token\":\"x\"}}";
    }else{
        if(voice_frame++){ws_reader.close_code=4017;return -2;}event="{\"op\":8,\"d\":{\"heartbeat_interval\":40000}}";
    }
    assert(strlen(event)<cap);strcpy((char*)out,event);return (int)strlen(event);
}
#include "voice_connect.inc"
int main(void){
    char host[256];assert(dnx_voice_host("voice-test.discord.media:443",host));assert(!dnx_voice_host("voice-test.discord.media.evil:443",host));assert(!dnx_voice_host("127.0.0.1:443",host));assert(!dnx_voice_host("voice-test.discord.media:1234",host));
    char notice[160];int stage;unsigned close_code;active_cancel=cancel;
    voice_control_preflight("synthetic-account-token","100000000000000001","200000000000000001","300000000000000001",notice,&stage,&close_code);
    assert(stage==4&&close_code==4017&&joins==1&&leaves==1&&voice_identifies==1&&!rng);assert(strstr(notice,"DAVE")&&!live_token[0]);
    main_frame=voice_frame=joins=leaves=voice_identifies=0;invalid_endpoint=1;
    voice_control_preflight("synthetic-account-token","100000000000000001","200000000000000001","300000000000000001",notice,&stage,&close_code);assert(stage==2&&joins==1&&leaves==1&&voice_identifies==0&&!rng);
    puts("PASS production voice signaling: scoped session/server events, approved TLS endpoint, muted join, honest no-DAVE identify, 4017 handling, guaranteed leave attempt and credential cleanup; no audio path");
}
