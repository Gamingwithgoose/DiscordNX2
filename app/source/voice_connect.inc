// SPDX-License-Identifier: MIT
/* Native authenticated voice join, followed by the DAVE/Opus runtime. */
#ifndef DNX2_VOICE_RUNTIME_EXTERNAL
#include "voice_runtime.inc"
#endif
static u64 voice_cleanup_deadline,voice_wait_until;
unsigned dnx2VoiceWaitSeconds(void){u64 end=__atomic_load_n(&voice_wait_until,__ATOMIC_ACQUIRE),now=dnx2MonotonicMs();return end>now?(unsigned)((end-now+999)/1000):0;}
static int voice_cleanup_cancel(void){return dnx2MonotonicMs()>=voice_cleanup_deadline;}
static int voice_control_session(const char *token,const char *guild,const char *channel,const char *user,char notice[160],int *stage,unsigned *close_code){
    DnxVoiceCredentials credentials={0};TlsSession gateway={0},voice={0};gateway.socket_fd=gateway.returned_fd=voice.socket_fd=voice.returned_fd=-1;
    unsigned char *event=calloc(DNX2_WS_MAX+1,1);char body[1800]={0};int runtime_started=0,call_active=0,rng=0,joined=0,identified=0,ready=0,hello_seen=0,receive_result=0,subscribed=0,leave_sent=0;
    *stage=0;*close_code=0;snprintf(notice,160,"Voice connection could not be established.");
    if(!event||!dnx_id(guild)||!dnx_id(channel)||!dnx_id(user)||R_FAILED(csrngInitialize()))goto done;
    rng=1;
    snprintf(credentials.guild,24,"%s",guild);snprintf(credentials.channel,24,"%s",channel);snprintf(credentials.user,24,"%s",user);
    if(!live_session[0]||live_sequence<0){__atomic_store_n(&voice_wait_until,live_identify_after,__ATOMIC_RELEASE);while(dnx2MonotonicMs()<live_identify_after){if(active_cancel&&active_cancel()){snprintf(notice,160,"Voice connection cancelled.");goto done;}svcSleepThread(16000000LL);}}
    __atomic_store_n(&voice_wait_until,0,__ATOMIC_RELEASE);
    snprintf(live_token,sizeof(live_token),"%s",token);memset(&ws_reader,0,sizeof(ws_reader));
    const char *gateway_host=live_session[0]&&live_resume_host[0]?live_resume_host:"gateway.discord.gg";
    if(!tls_open(&gateway,gateway_host)||!ws_connect_to(&gateway,gateway_host,"/?v=10&encoding=json"))goto done;
    *stage=1;
    u64 deadline=dnx2MonotonicMs()+20000,last_beat=0,interval=40000;
    while((!credentials.have_state||!credentials.have_server)&&dnx2MonotonicMs()<deadline){
        if(active_cancel&&active_cancel())goto done;
        int n=ws_receive(&gateway,event,DNX2_WS_MAX+1,dnx2MonotonicMs()+100);receive_result=n;if(n<0){*close_code=ws_reader.close_code;if(!ready&&live_session[0]){wipe(live_session,sizeof(live_session));wipe(live_resume_host,sizeof(live_resume_host));live_sequence=-1;snprintf(notice,160,"Gateway ended before Ready. The stale session was cleared; retry the test.");}goto done;}
        if(n>0){DnxJson root=dj_root((char*)event),data=dj_field(root,"d");int op=dj_int(root,"op",-1);
            DnxJson seq=dj_field(root,"s");if(seq.p&&*seq.p!='n'){char *end;long long value=strtoll(seq.p,&end,10);if(end==seq.end&&value>=0)live_sequence=value;}
            if(op==10){hello_seen=1;int ms=dj_int(data,"heartbeat_interval",40000);if(ms<1000||ms>120000)goto done;interval=(u64)ms;
                identified=live_identify(&gateway);if(!identified){snprintf(notice,160,"Gateway authentication could not start. Reconnect the account and retry.");goto done;}last_beat=dnx2MonotonicMs();(void)live_heartbeat(&gateway);}
            else if(op==1)(void)live_heartbeat(&gateway);
            else if(op==9){wipe(live_session,sizeof(live_session));live_sequence=-1;snprintf(notice,160,"Discord rejected the saved Gateway session. Retry the connection test.");goto done;}
            else if(op==7)goto done;
            else if(op==0){char kind[64]={0};dj_get(root,"t",kind,sizeof(kind));
                if(!ready&&(!strcmp(kind,"READY")||!strcmp(kind,"RESUMED"))){
                    if(!strcmp(kind,"READY")){dj_get(data,"session_id",live_session,sizeof(live_session));live_resume_host[0]=0;(void)dnx_gateway_host(data,live_resume_host);}
                    ready=1;
                    if(!dnx_voice_subscribe_body(&credentials,body,sizeof(body))||!ws_text(&gateway,body))goto done;
                    subscribed=1;
                    if(!dnx_voice_state_body(&credentials,0,body,sizeof(body))||!ws_text(&gateway,body))goto done;
                    joined=1;*stage=2;deadline=dnx2MonotonicMs()+15000;
                }else if(ready)dnx_voice_event(&credentials,kind,data);
            }
            wipe(event,(size_t)n);
        }
        if(identified&&dnx2MonotonicMs()-last_beat>=interval){if(!live_heartbeat(&gateway))goto done;last_beat=dnx2MonotonicMs();}
    }
    if(!credentials.have_state||!credentials.have_server){snprintf(notice,160,"%s",dnx_voice_confirmation_status(&credentials));goto done;}*stage=3;
    if(!tls_open_port(&voice,credentials.host,credentials.port)||!ws_connect_to_port(&voice,credentials.host,credentials.port,"/?v=8"))goto done;
    *stage=4;
    if(!dnx_voice_identify_body(&credentials,body,sizeof(body))||!ws_text(&voice,body))goto done;
    wipe(body,sizeof(body));
    runtime_started=1;call_active=voice_media_session(&voice,&gateway,&credentials,event,notice,stage,close_code,interval,last_beat);

done:
    __atomic_store_n(&voice_wait_until,0,__ATOMIC_RELEASE);
    if(!strcmp(notice,"Voice connection could not be established."))snprintf(notice,160,"Voice connection failed at stage %d (Discord code %u).",*stage,*close_code);
    if(joined&&dnx_voice_state_body(&credentials,1,body,sizeof(body))){Dnx2AuthCancel previous=active_cancel;voice_cleanup_deadline=dnx2MonotonicMs()+2000;active_cancel=voice_cleanup_cancel;leave_sent=ws_text(&gateway,body);active_cancel=previous;}
    if(!runtime_started){FILE *f=fopen("sdmc:/kaihen/discordnx2-voice.txt","w");if(f){fprintf(f,"DiscordNX2 v0.46 voice signaling - no credentials or IDs\nstage=%d close=%u state=%d server=%d join=%d leave=%d\n",*stage,*close_code,credentials.have_state,credentials.have_server,joined,leave_sent);fclose(f);}}
    tls_close(&voice);tls_close(&gateway);if(rng)csrngExit();if(event){wipe(event,DNX2_WS_MAX+1);free(event);}wipe(body,sizeof(body));wipe(&credentials,sizeof(credentials));wipe(live_token,sizeof(live_token));wipe(&ws_reader,sizeof(ws_reader));
    (void)hello_seen;(void)identified;(void)receive_result;(void)subscribed;(void)leave_sent;return call_active;
}
