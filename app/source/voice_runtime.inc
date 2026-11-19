// SPDX-License-Identifier: MIT
/* This file runs on the single networking owner. No plaintext audio fallback. */
#include <malloc.h>
#include <fcntl.h>
#include "voice_audio.inc"
static unsigned voice_state,voice_muted; /* UI reads atomically; no shared strings. */
unsigned dnx2VoiceState(void){return __atomic_load_n(&voice_state,__ATOMIC_ACQUIRE);}
int dnx2VoiceMuted(void){return (int)__atomic_load_n(&voice_muted,__ATOMIC_ACQUIRE);}
void dnx2VoiceToggleMute(void){__atomic_xor_fetch(&voice_muted,1,__ATOMIC_ACQ_REL);}
static void voice_set(unsigned state){__atomic_store_n(&voice_state,state,__ATOMIC_RELEASE);}
static int voice_rand(unsigned char *out,int n){return n>=0&&R_SUCCEEDED(csrngGetRandomBytes(out,(size_t)n));}
static int voice_rand_status(void){return 1;}
static const RAND_METHOD voice_rng={NULL,voice_rand,NULL,NULL,voice_rand,voice_rand_status};
static int voice_signal(void *user,int binary,const unsigned char *data,size_t n){return ws_send((TlsSession*)user,binary?2:1,(const char*)data,n);}
static int voice_uint(DnxJson d,const char *key,uint32_t *out){DnxJson v=dj_field(d,key);if(!v.p||*v.p<'0'||*v.p>'9')return 0;char *end;unsigned long long n=strtoull(v.p,&end,10);if(end!=v.end||n>UINT32_MAX)return 0;*out=(uint32_t)n;return 1;}
static int voice_udp(DnxJson d,uint32_t *ssrc,char mode[48]){
 char ip[80]={0};int port=dj_int(d,"port",0);if(!dj_get(d,"ip",ip,sizeof(ip))||!voice_uint(d,"ssrc",ssrc)||!*ssrc||port<1||port>65535)return -1;
 mode[0]=0;const char *cursor=NULL;DnxJson item;while(dj_next(dj_field(d,"modes"),&cursor,&item)){char v[80];if(dj_string(item,v,sizeof(v))){if(!strcmp(v,"aead_aes256_gcm_rtpsize")){strcpy(mode,"aead_aes256_gcm_rtpsize");break;}if(!strcmp(v,"aead_xchacha20_poly1305_rtpsize"))strcpy(mode,"aead_xchacha20_poly1305_rtpsize");}}if(!mode[0])return -1;
 struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_port=htons((uint16_t)port);if(inet_pton(AF_INET,ip,&addr.sin_addr)!=1)return -1;
 int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(fd<0)return -1;
 if(connect(fd,(struct sockaddr*)&addr,sizeof(addr))<0||fcntl(fd,F_SETFL,O_NONBLOCK)<0){close(fd);return -1;}
 unsigned char packet[74]={0};packet[1]=1;packet[3]=70;packet[4]=(unsigned char)(*ssrc>>24);packet[5]=(unsigned char)(*ssrc>>16);packet[6]=(unsigned char)(*ssrc>>8);packet[7]=(unsigned char)*ssrc;
 if(send(fd,packet,sizeof(packet),0)!=(int)sizeof(packet)){close(fd);return -1;}return fd;
}
static int voice_discovered(const unsigned char *p,size_t n,uint32_t ssrc,char address[64],unsigned *port){
 if(n!=74||p[0]||p[1]!=2||p[2]||p[3]!=70||((uint32_t)p[4]<<24|(uint32_t)p[5]<<16|(uint32_t)p[6]<<8|p[7])!=ssrc)return 0;
 const unsigned char *end=memchr(p+8,0,64);if(!end||end==p+8)return 0;memcpy(address,p+8,(size_t)(end-p-8));address[end-p-8]=0;struct in_addr ip;if(inet_pton(AF_INET,address,&ip)!=1)return 0;*port=(unsigned)p[72]<<8|p[73];return *port!=0;
}
static int voice_media_session(TlsSession *voice,TlsSession *gateway,const DnxVoiceCredentials *c,unsigned char *event,char notice[160],int *stage,unsigned *close_code,u64 gateway_interval,u64 gateway_beat){
 Dnx2WsReader *reader=calloc(1,sizeof(*reader));VoiceAudio *audio=calloc(1,sizeof(*audio));DnxVoiceEngine *engine=NULL;int udp=-1,selected=0,unmuted=-1,ever_active=0,voice_ack=1,gateway_ack=1;uint32_t ssrc=0;char body[512],mode[48]={0};DnxVoiceStats stats={0};u64 interval=0,beat=0,discovery_until=0,start=dnx2MonotonicMs(),last_report=0;int sequence=-1;
 snprintf(notice,160,"The encrypted voice session ended before audio was ready.");
 if(!reader||!audio||RAND_set_rand_method(&voice_rng)!=1)goto done;
 reader->allow_binary=1;engine=dnx_voice_create(c->user,c->channel,voice_signal,voice);if(!engine){snprintf(notice,160,"The encrypted voice engine could not initialize.");goto done;}voice_set(1);
 for(;;){u64 now=dnx2MonotonicMs();if(active_cancel&&active_cancel()){snprintf(notice,160,"Voice disconnected.");break;}
  int n=ws_receive_reader(voice,reader,event,DNX2_WS_MAX+1,now+2);if(n<0){*close_code=reader->close_code;snprintf(notice,160,"Discord closed voice (code %u). Rejoin to reconnect.",*close_code);break;}
  if(n>0){int result;if(reader->is_binary){if(n>=3)sequence=(int)((unsigned)event[0]<<8|event[1]);result=dnx_voice_signal_binary(engine,event,(size_t)n);}else {DnxJson root=dj_root((char*)event),d=dj_field(root,"d");int op=dj_int(root,"op",-1);int seq=dj_int(root,"seq",-1);if(seq>=0)sequence=seq;
    if(op==8){int ms=dj_int(d,"heartbeat_interval",0);if(ms<1000||ms>120000)goto done;interval=(u64)ms;beat=now;snprintf(body,sizeof(body),"{\"op\":3,\"d\":{\"t\":%llu,\"seq_ack\":%d}}",(unsigned long long)now,sequence);if(!ws_text(voice,body))goto done;voice_ack=0;}
    else if(op==6)voice_ack=1;
    else if(op==2){if(udp>=0)goto done;udp=voice_udp(d,&ssrc,mode);if(udp<0){snprintf(notice,160,"Voice UDP discovery could not start.");goto done;}dnx_voice_ssrc(engine,ssrc);discovery_until=now+10000;*stage=5;voice_set(2);}
    result=dnx_voice_signal_json(engine,(char*)event);
   }wipe(event,(size_t)n);if(result<0){snprintf(notice,160,"Encrypted voice negotiation failed. Rejoin to retry.");goto done;}}
  if(interval&&now-beat>=interval){if(!voice_ack){snprintf(notice,160,"Discord voice heartbeat timed out.");goto done;}snprintf(body,sizeof(body),"{\"op\":3,\"d\":{\"t\":%llu,\"seq_ack\":%d}}",(unsigned long long)now,sequence);if(!ws_text(voice,body))goto done;beat=now;voice_ack=0;}
  n=ws_receive(gateway,event,DNX2_WS_MAX+1,dnx2MonotonicMs()+2);if(n<0){snprintf(notice,160,"Discord Gateway disconnected. Rejoin voice to retry.");goto done;}
  if(n>0){DnxJson root=dj_root((char*)event),d=dj_field(root,"d"),seq=dj_field(root,"s");if(seq.p&&*seq.p!='n'){char *end;long long value=strtoll(seq.p,&end,10);if(end==seq.end&&value>=0)live_sequence=value;}int op=dj_int(root,"op",-1);if(op==11)gateway_ack=1;else if(op==1){if(!live_heartbeat(gateway))goto done;}else if(op==7||op==9)goto done;else if(op==0){char kind[48],uid[24],cid[24];dj_get(root,"t",kind,sizeof(kind));if(!strcmp(kind,"VOICE_STATE_UPDATE")&&dj_get(d,"user_id",uid,24)&&!strcmp(uid,c->user)&&(!dj_get(d,"channel_id",cid,24)||strcmp(cid,c->channel))){snprintf(notice,160,"Discord moved or disconnected this voice session.");goto done;}}wipe(event,(size_t)n);}
  if(now-gateway_beat>=gateway_interval){if(!gateway_ack||!live_heartbeat(gateway)){snprintf(notice,160,"Discord Gateway heartbeat timed out.");goto done;}gateway_beat=now;gateway_ack=0;}
  if(udp>=0){for(int work=0;work<32;work++){unsigned char packet[2048];n=recv(udp,packet,sizeof(packet),0);if(n<0){if(errno!=EWOULDBLOCK&&errno!=EAGAIN)goto done;break;}if(n==0)break;
    if(!selected){char address[64]={0};unsigned port;if(voice_discovered(packet,(size_t)n,ssrc,address,&port)){snprintf(body,sizeof(body),"{\"op\":1,\"d\":{\"protocol\":\"udp\",\"data\":{\"address\":\"%s\",\"port\":%u,\"mode\":\"%s\"}}}",address,port,mode);if(!ws_text(voice,body))goto done;selected=1;*stage=6;voice_set(3);}}
    else (void)dnx_voice_receive(engine,packet,(size_t)n);wipe(packet,sizeof(packet));}}
  if(udp>=0&&!selected&&now>=discovery_until){snprintf(notice,160,"Discord UDP discovery timed out. Check network access.");goto done;}
  dnx_voice_stats(engine,&stats);if(stats.failed)goto done;
  if(stats.secure_ready&&stats.transport_ready&&selected){if(!audio->started){if(!voice_audio_open(audio)){snprintf(notice,160,"The Switch audio devices could not start for this call.");goto done;}*stage=7;ever_active=1;}
    int muted=dnx2VoiceMuted();if(unmuted!=!muted){snprintf(body,sizeof(body),"{\"op\":4,\"d\":{\"guild_id\":\"%s\",\"channel_id\":\"%s\",\"self_mute\":%s,\"self_deaf\":false,\"self_video\":false}}",c->guild,c->channel,muted?"true":"false");if(!ws_text(gateway,body))goto done;snprintf(body,sizeof(body),"{\"op\":5,\"d\":{\"speaking\":%d,\"delay\":0,\"ssrc\":%u}}",muted?0:1,ssrc);if(!ws_text(voice,body))goto done;unmuted=!muted;}
    voice_set(muted?5:4);if(!voice_audio_tick(audio,engine,udp,muted)){snprintf(notice,160,"The voice audio stream stopped. Rejoin to retry.");goto done;}
  }else {if(audio->started){voice_audio_close(audio);unmuted=-1;if(dnx_voice_state_body(c,0,body,sizeof(body)))ws_text(gateway,body);}if(selected)voice_set(3);}
  if(!ever_active&&now-start>60000){snprintf(notice,160,"Encrypted voice is still waiting for a group key. Have another member join, then retry.");goto done;}
  if(now-last_report>=5000){FILE *f=fopen("sdmc:/kaihen/discordnx2-voice.txt","w");if(f){fprintf(f,"DiscordNX2 v0.46 encrypted voice - no credentials or IDs\nstage=%d transport=%d secure=%d tx=%u rx=%u dropped=%u mls=%u\n",*stage,stats.transport_ready,stats.secure_ready,stats.tx,stats.rx,stats.dropped,stats.mls_updates);fclose(f);}last_report=now;}
  svcSleepThread(1000000LL);
 }
 done:
 if(engine)dnx_voice_stats(engine,&stats);
 if(audio){voice_audio_close(audio);free(audio);}if(udp>=0)close(udp);dnx_voice_destroy(engine);if(reader){wipe(reader,sizeof(*reader));free(reader);}voice_set(0);
 FILE *f=fopen("sdmc:/kaihen/discordnx2-voice.txt","w");if(f){fprintf(f,"DiscordNX2 v0.46 encrypted voice - no credentials or IDs\nstage=%d close=%u transport=%d secure=%d tx=%u rx=%u dropped=%u mls=%u\n",*stage,*close_code,stats.transport_ready,stats.secure_ready,stats.tx,stats.rx,stats.dropped,stats.mls_updates);fclose(f);}return ever_active;
}
