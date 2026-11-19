// SPDX-License-Identifier: MIT
/* Native authenticated voice join, followed by the DAVE/Opus runtime. */
#ifndef DNX2_VOICE_RUNTIME_EXTERNAL
#include "voice_runtime.inc"
#endif
// Separate Gateway state is owned exclusively by the call worker.
#ifndef DNX2_VOICE_GATEWAY_EXTERNAL
static char voice_gateway_token[1024],voice_gateway_session[160],voice_gateway_resume_host[256];
static u64 voice_gateway_identify_after;static int voice_stop_request;
static int voice_cancel(void){return __atomic_load_n(&voice_stop_request,__ATOMIC_ACQUIRE);}
static int voice_gateway_heartbeat(TlsSession *socket){char body[100];if(voice_gateway_sequence<0)snprintf(body,sizeof(body),"{\"op\":1,\"d\":null}");else snprintf(body,sizeof(body),"{\"op\":1,\"d\":%lld}",voice_gateway_sequence);return ws_text(socket,body);}
static int voice_gateway_identify(TlsSession *socket){
 if(!dnx_voice_atom(voice_gateway_token)||(voice_gateway_session[0]&&!dnx_voice_atom(voice_gateway_session)))return 0;
 char body[1800];int n;
 if(voice_gateway_session[0]&&voice_gateway_sequence>=0)n=snprintf(body,sizeof(body),"{\"op\":6,\"d\":{\"token\":\"%s\",\"session_id\":\"%s\",\"seq\":%lld}}",voice_gateway_token,voice_gateway_session,voice_gateway_sequence);
 else {if(dnx2MonotonicMs()<voice_gateway_identify_after)return 0;voice_gateway_identify_after=dnx2MonotonicMs()+60000;n=snprintf(body,sizeof(body),"{\"op\":2,\"d\":{\"token\":\"%s\",\"properties\":{\"os\":\"Switch 2\",\"browser\":\"DiscordNX2 Voice\",\"device\":\"DiscordNX2\"},\"compress\":false,\"large_threshold\":50}}",voice_gateway_token);}
 int ok=n>0&&(size_t)n<sizeof(body)&&ws_text(socket,body);wipe(body,sizeof(body));return ok;
}
#endif
static u64 voice_cleanup_deadline,voice_wait_until;
unsigned dnx2VoiceWaitSeconds(void){u64 end=__atomic_load_n(&voice_wait_until,__ATOMIC_ACQUIRE),now=dnx2MonotonicMs();return end>now?(unsigned)((end-now+999)/1000):0;}
static int voice_cleanup_cancel(void){return dnx2MonotonicMs()>=voice_cleanup_deadline;}
static int voice_control_session(const char *token,const char *guild,const char *channel,const char *user,char notice[160],int *stage,unsigned *close_code){
    DnxVoiceCredentials credentials={0};TlsSession gateway={0},voice={0};gateway.socket_fd=gateway.returned_fd=voice.socket_fd=voice.returned_fd=-1;
    Dnx2WsReader *main_reader=calloc(1,sizeof(*main_reader));
    unsigned char *event=calloc(DNX2_WS_MAX+1,1);char body[1800]={0};int runtime_started=0,call_active=0,rng=0,joined=0,identified=0,ready=0,hello_seen=0,receive_result=0,subscribed=0,leave_sent=0;
    *stage=0;*close_code=0;snprintf(notice,160,"Voice connection could not be established.");
    if(!main_reader||!event||!dnx_id(guild)||!dnx_id(channel)||!dnx_id(user)||R_FAILED(csrngInitialize()))goto done;
    rng=1;
    snprintf(credentials.guild,24,"%s",guild);snprintf(credentials.channel,24,"%s",channel);snprintf(credentials.user,24,"%s",user);
    if(!voice_gateway_session[0]||voice_gateway_sequence<0){__atomic_store_n(&voice_wait_until,voice_gateway_identify_after,__ATOMIC_RELEASE);while(dnx2MonotonicMs()<voice_gateway_identify_after){if(voice_cancel()){snprintf(notice,160,"Voice connection cancelled.");goto done;}svcSleepThread(16000000LL);}}
    __atomic_store_n(&voice_wait_until,0,__ATOMIC_RELEASE);
    snprintf(voice_gateway_token,sizeof(voice_gateway_token),"%s",token);
    const char *gateway_host=voice_gateway_session[0]&&voice_gateway_resume_host[0]?voice_gateway_resume_host:"gateway.discord.gg";
    if(!tls_open_port_cancel(&gateway,gateway_host,443,voice_cancel)||!ws_connect_to(&gateway,gateway_host,"/?v=10&encoding=json"))goto done;
    *stage=1;
    u64 deadline=dnx2MonotonicMs()+20000,last_beat=0,interval=40000;
    while((!credentials.have_state||!credentials.have_server)&&dnx2MonotonicMs()<deadline){
        if(voice_cancel())goto done;
        int n=ws_receive_reader(&gateway,main_reader,event,DNX2_WS_MAX+1,dnx2MonotonicMs()+100);receive_result=n;if(n<0){*close_code=main_reader->close_code;if(!ready&&voice_gateway_session[0]){wipe(voice_gateway_session,sizeof(voice_gateway_session));wipe(voice_gateway_resume_host,sizeof(voice_gateway_resume_host));voice_gateway_sequence=-1;snprintf(notice,160,"Gateway ended before Ready. The stale session was cleared; retry the test.");}goto done;}
        if(n>0){DnxJson root=dj_root((char*)event),data=dj_field(root,"d");int op=dj_int(root,"op",-1);
            DnxJson seq=dj_field(root,"s");if(seq.p&&*seq.p!='n'){char *end;long long value=strtoll(seq.p,&end,10);if(end==seq.end&&value>=0)voice_gateway_sequence=value;}
            if(op==10){hello_seen=1;int ms=dj_int(data,"heartbeat_interval",40000);if(ms<1000||ms>120000)goto done;interval=(u64)ms;
                identified=voice_gateway_identify(&gateway);if(!identified){snprintf(notice,160,"Gateway authentication could not start. Reconnect the account and retry.");goto done;}last_beat=dnx2MonotonicMs();(void)voice_gateway_heartbeat(&gateway);}
            else if(op==1)(void)voice_gateway_heartbeat(&gateway);
            else if(op==9){wipe(voice_gateway_session,sizeof(voice_gateway_session));voice_gateway_sequence=-1;snprintf(notice,160,"Discord rejected the saved Gateway session. Retry the connection test.");goto done;}
            else if(op==7)goto done;
            else if(op==0){char kind[64]={0};dj_get(root,"t",kind,sizeof(kind));voice_members_gateway(&credentials,kind,data);
                if(!ready&&(!strcmp(kind,"READY")||!strcmp(kind,"RESUMED"))){
                    if(!strcmp(kind,"READY")){dj_get(data,"session_id",voice_gateway_session,sizeof(voice_gateway_session));voice_gateway_resume_host[0]=0;(void)dnx_gateway_host(data,voice_gateway_resume_host);}
                    ready=1;
                    if(!dnx_voice_subscribe_body(&credentials,body,sizeof(body))||!ws_text(&gateway,body))goto done;
                    subscribed=1;
                    if(!dnx_voice_state_body(&credentials,0,body,sizeof(body))||!ws_text(&gateway,body))goto done;
                    joined=1;*stage=2;deadline=dnx2MonotonicMs()+15000;
                }else if(ready)dnx_voice_event(&credentials,kind,data);
            }
            wipe(event,(size_t)n);
        }
        if(identified&&dnx2MonotonicMs()-last_beat>=interval){if(!voice_gateway_heartbeat(&gateway))goto done;last_beat=dnx2MonotonicMs();}
    }
    if(!credentials.have_state||!credentials.have_server){snprintf(notice,160,"%s",dnx_voice_confirmation_status(&credentials));goto done;}*stage=3;
    if(!tls_open_port_cancel(&voice,credentials.host,credentials.port,voice_cancel)||!ws_connect_to_port(&voice,credentials.host,credentials.port,"/?v=8"))goto done;
    *stage=4;
    if(!dnx_voice_identify_body(&credentials,body,sizeof(body))||!ws_text(&voice,body))goto done;
    wipe(body,sizeof(body));
    runtime_started=1;call_active=voice_media_session(&voice,&gateway,&credentials,event,notice,stage,close_code,interval,last_beat,main_reader);

done:
    __atomic_store_n(&voice_wait_until,0,__ATOMIC_RELEASE);
    if(!strcmp(notice,"Voice connection could not be established."))snprintf(notice,160,"Voice connection failed at stage %d (Discord code %u).",*stage,*close_code);
    if(joined&&dnx_voice_state_body(&credentials,1,body,sizeof(body))){Dnx2AuthCancel previous=gateway.cancel;voice_cleanup_deadline=dnx2MonotonicMs()+2000;gateway.cancel=voice_cleanup_cancel;leave_sent=ws_text(&gateway,body);gateway.cancel=previous;}
    if(!runtime_started){FILE *f=fopen("sdmc:/kaihen/discordnx2-voice.txt","w");if(f){fprintf(f,"DiscordNX2 v0.46 voice signaling - no credentials or IDs\nstage=%d close=%u state=%d server=%d join=%d leave=%d\n",*stage,*close_code,credentials.have_state,credentials.have_server,joined,leave_sent);fclose(f);}}
    tls_close(&voice);tls_close(&gateway);if(rng)csrngExit();if(event){wipe(event,DNX2_WS_MAX+1);free(event);}wipe(body,sizeof(body));wipe(&credentials,sizeof(credentials));wipe(voice_gateway_token,sizeof(voice_gateway_token));if(main_reader){wipe(main_reader,sizeof(*main_reader));free(main_reader);}
    (void)hello_seen;(void)identified;(void)receive_result;(void)subscribed;(void)leave_sent;return call_active;
}
