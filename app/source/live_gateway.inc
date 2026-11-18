// SPDX-License-Identifier: MIT
/* One foreground text subscription worker. No UI/HID calls from this thread.
 * Main thread owns the visible account; pending snapshots cross a mutex.
 * All other account requests run only after this worker has joined. */
static Thread live_thread;
static Mutex live_mutex;
static int live_running,live_stop,live_mode;
static unsigned live_generation,live_applied;
static int live_count;
static char live_channel[24],live_guild[24],live_token[1024];
static char live_session[160];
static long long live_sequence=-1;
static u64 live_identify_after;
static Dnx2Message live_pending[DNX2_MAX_MESSAGES];
static int live_cancel(void){return __atomic_load_n(&live_stop,__ATOMIC_ACQUIRE);}
static void live_set_mode(int mode){__atomic_store_n(&live_mode,mode,__ATOMIC_RELEASE);}
const char *dnx2LiveStatus(void){
    static const char *labels[]={"", "Connecting live updates", "Live updates", "Auto refresh (5 s)", "Connection interrupted", "Discord rate limited updates", "Channel access unavailable"};
    int mode=__atomic_load_n(&live_mode,__ATOMIC_ACQUIRE);return labels[mode>=0&&mode<7?mode:0];
}
static void live_publish(const Dnx2Account *result){
    mutexLock(&live_mutex);
    if(live_count!=result->message_count||memcmp(live_pending,result->messages,(size_t)result->message_count*sizeof(Dnx2Message))){
        live_count=result->message_count;memcpy(live_pending,result->messages,(size_t)live_count*sizeof(Dnx2Message));live_generation++;
    }
    mutexUnlock(&live_mutex);
}
int dnx2LiveApply(void){
    int changed=0;mutexLock(&live_mutex);
    if(live_applied!=live_generation&&account.selected_channel>=0&&!strcmp(account.channels[account.selected_channel].id,live_channel)){
        account.message_count=live_count;memcpy(account.messages,live_pending,(size_t)live_count*sizeof(Dnx2Message));live_applied=live_generation;changed=1;
    }
    mutexUnlock(&live_mutex);return changed;
}
static int live_refresh(Dnx2Account *result,u64 *retry){
    char path[112];snprintf(path,sizeof(path),"/api/v10/channels/%s/messages?limit=30",live_channel);
    TlsSession https;int ok=http_request(&https,API_HOST,"GET",path,NULL,live_token,(char*)http_body,sizeof(http_body));tls_close(&https);
    if(ok){dnx_messages(result,(char*)http_body);live_publish(result);}
    else if(last_http_status==429){
        DnxJson delay=dj_field(dj_root((char*)http_body),"retry_after");double seconds=delay.p?strtod(delay.p,NULL):5;
        if(!(seconds>=1&&seconds<=300))seconds=30;
        *retry=dnx2MonotonicMs()+(u64)(seconds*1000)+250;live_set_mode(5);
    }else if(last_http_status==401||last_http_status==403)live_set_mode(6);
    else {*retry=dnx2MonotonicMs()+15000;live_set_mode(4);}
    wipe(http_body,sizeof(http_body));return ok;
}
static int live_heartbeat(TlsSession *socket){char body[100];if(live_sequence<0)snprintf(body,sizeof(body),"{\"op\":1,\"d\":null}");else snprintf(body,sizeof(body),"{\"op\":1,\"d\":%lld}",live_sequence);return ws_text(socket,body);}
static int live_identify(TlsSession *socket){
    /* Tokens are supplied by Discord; reject JSON metacharacters rather than
     * interpolate an invalid credential. No browser impersonation. */
    for(const char *p=live_token;*p;p++)if(*p=='"'||*p=='\\'||(unsigned char)*p<33)return 0;
    for(const char *p=live_session;*p;p++)if(*p=='"'||*p=='\\'||(unsigned char)*p<33)return 0;
    char body[1800];int n;
    if(live_session[0]&&live_sequence>=0)n=snprintf(body,sizeof(body),"{\"op\":6,\"d\":{\"token\":\"%s\",\"session_id\":\"%s\",\"seq\":%lld}}",live_token,live_session,live_sequence);
    else {
        if(dnx2MonotonicMs()<live_identify_after)return 0;
        live_identify_after=dnx2MonotonicMs()+60000;
        n=snprintf(body,sizeof(body),"{\"op\":2,\"d\":{\"token\":\"%s\",\"properties\":{\"os\":\"Switch 2\",\"browser\":\"DiscordNX2\",\"device\":\"DiscordNX2\"},\"compress\":false,\"large_threshold\":50}}",live_token);
    }
    int ok=n>0&&(size_t)n<sizeof(body)&&ws_text(socket,body);wipe(body,sizeof(body));return ok;
}
static void live_subscribe(TlsSession *socket){
    char body[300];snprintf(body,sizeof(body),"{\"op\":14,\"d\":{\"guild_id\":\"%s\",\"typing\":false,\"activities\":false,\"threads\":false,\"channels\":{\"%s\":[[0,99]]}}}",live_guild,live_channel);(void)ws_text(socket,body);
}
static void live_worker(void *unused){
    (void)unused;Dnx2Account *result=calloc(1,sizeof(*result));unsigned char *event=calloc(DNX2_WS_MAX+1,1);
    int sockets=0,ssl=0,rng=0,connected=0,ready=0,awaiting=0,identified=0;
    unsigned dispatches=0,refreshes=0;
    u64 heartbeat=40000,last_beat=0,next_refresh=dnx2MonotonicMs()+5000,hello_deadline=0,retry_until=0;
    TlsSession gateway={0};gateway.socket_fd=gateway.returned_fd=-1;
    if(!result||!event||R_FAILED(socketInitialize(NULL)))goto done;
    sockets=1;
    if(R_FAILED(sslInitialize(1)))goto done;
    ssl=1;
    if(R_FAILED(csrngInitialize()))goto done;
    rng=1;active_cancel=live_cancel;last_http_status=0;
    memset(&ws_reader,0,sizeof(ws_reader));live_set_mode(1);
    if(tls_open(&gateway,"gateway.discord.gg")&&ws_connect_to(&gateway,"gateway.discord.gg","/?v=10&encoding=json")){
        connected=1;hello_deadline=dnx2MonotonicMs()+15000;
    }else {tls_close(&gateway);live_set_mode(3);}
    while(!live_cancel()){
        u64 now=dnx2MonotonicMs();
        if(connected){
            if((!ready&&now>hello_deadline)||(identified&&now-last_beat>=heartbeat&&awaiting))connected=0;
            if(connected&&identified&&now-last_beat>=heartbeat){if(!live_heartbeat(&gateway))connected=0;last_beat=now;awaiting=1;}
            int n=connected?ws_receive(&gateway,event,DNX2_WS_MAX+1,now+40):0;
            if(n<0)connected=0;
            if(n>0){
                DnxJson root=dj_root((char*)event),data=dj_field(root,"d");int op=dj_int(root,"op",-1);
                DnxJson seq=dj_field(root,"s");if(seq.p&&*seq.p!='n'){char *end;long long value=strtoll(seq.p,&end,10);if(end==seq.end&&value>=0)live_sequence=value;}
                if(op==10){int ms=dj_int(data,"heartbeat_interval",40000);if(ms<1000||ms>120000)connected=0;else {heartbeat=(u64)ms;identified=live_identify(&gateway);if(!identified)connected=0;last_beat=now-heartbeat;}}
                else if(op==11)awaiting=0;
                else if(op==1){if(!live_heartbeat(&gateway))connected=0;last_beat=now;awaiting=1;}
                else if(op==7)connected=0;
                else if(op==9){wipe(live_session,sizeof(live_session));live_sequence=-1;connected=0;}
                else if(op==0){dispatches++;char kind[64]={0};dj_get(root,"t",kind,sizeof(kind));
                    if(!strcmp(kind,"READY")||!strcmp(kind,"RESUMED")){
                        if(!strcmp(kind,"READY"))dj_get(data,"session_id",live_session,sizeof(live_session));
                        ready=1;live_set_mode(2);live_subscribe(&gateway);next_refresh=now;
                    }else if(!strcmp(kind,"MESSAGE_CREATE")||!strcmp(kind,"MESSAGE_UPDATE")||!strcmp(kind,"MESSAGE_DELETE")||!strcmp(kind,"MESSAGE_DELETE_BULK")){
                        char channel[24]={0};if(dj_get(data,"channel_id",channel,sizeof(channel))&&!strcmp(channel,live_channel)&&now>=retry_until&&next_refresh>now+150)next_refresh=now+150;
                    }
                }
                wipe(event,(size_t)n);
            }
            if(!connected){tls_close(&gateway);live_set_mode(3);}
        }
        now=dnx2MonotonicMs();
        if(now>=next_refresh){
            if(identified&&connected){(void)live_heartbeat(&gateway);last_beat=now;awaiting=1;}
            u64 retry=0;refreshes++;int ok=live_refresh(result,&retry);
            if(__atomic_load_n(&live_mode,__ATOMIC_ACQUIRE)==6)break;
            if(ok)live_set_mode(connected&&ready?2:3);
            retry_until=retry;next_refresh=retry?retry:dnx2MonotonicMs()+(connected&&ready?15000:5000);
        }
        svcSleepThread(16000000LL);
    }
done:
    if(!live_cancel()&&__atomic_load_n(&live_mode,__ATOMIC_ACQUIRE)==1)live_set_mode(4);
    FILE *report=fopen("sdmc:/kaihen/discordnx2-live.txt","w");
    if(report){fprintf(report,"DiscordNX2 v0.40 live text updates - no account/channel/message/credential data\nGateway ready=%d dispatches=%u refreshes=%u mode=%d last_http=%u cancelled=%d\n",ready,dispatches,refreshes,__atomic_load_n(&live_mode,__ATOMIC_ACQUIRE),last_http_status,live_cancel());fclose(report);}
    tls_close(&gateway);if(rng)csrngExit();if(ssl)sslExit();if(sockets)socketExit();active_cancel=NULL;
    wipe(&ws_reader,sizeof(ws_reader));if(event){wipe(event,DNX2_WS_MAX+1);free(event);}if(result){wipe(result,sizeof(*result));free(result);}
}
void dnx2LiveStop(void){
    if(live_running){__atomic_store_n(&live_stop,1,__ATOMIC_RELEASE);threadWaitForExit(&live_thread);threadClose(&live_thread);live_running=0;}
    wipe(live_token,sizeof(live_token));
}
void dnx2LiveReset(void){dnx2LiveStop();wipe(live_pending,sizeof(live_pending));wipe(live_channel,sizeof(live_channel));wipe(live_guild,sizeof(live_guild));live_count=0;live_generation=live_applied=0;wipe(live_session,sizeof(live_session));live_sequence=-1;live_identify_after=0;live_set_mode(0);}
void dnx2LiveStart(void){
    if(dnx2JobBusy()||live_running||!account.signed_in||account.selected_channel<0)return;
    int type=account.channels[account.selected_channel].type;if(type!=0&&type!=5)return;
    snprintf(live_channel,sizeof(live_channel),"%s",account.channels[account.selected_channel].id);
    snprintf(live_guild,sizeof(live_guild),"%s",account.servers[account.selected].id);snprintf(live_token,sizeof(live_token),"%s",session_token);
    live_count=account.message_count;memcpy(live_pending,account.messages,(size_t)live_count*sizeof(Dnx2Message));live_generation=live_applied=0;
    __atomic_store_n(&live_stop,0,__ATOMIC_RELEASE);
    if(R_SUCCEEDED(threadCreate(&live_thread,live_worker,NULL,NULL,128*1024,0x3b,-2))){
        if(R_SUCCEEDED(threadStart(&live_thread))){live_running=1;return;}threadClose(&live_thread);
    }
    wipe(live_token,sizeof(live_token));live_set_mode(4);
}
