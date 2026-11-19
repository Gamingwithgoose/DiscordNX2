// SPDX-License-Identifier: MIT
static struct {Thread thread;int running,done;char token[1024],guild[24],channel[24],user[24],notice[160],label[401];} voice_call;
static char voice_notice[160];
#ifndef DNX2_RESIDENT_RUNTIME
static int voice_resident_apply(void);
#include "resident_bridge.inc"
#endif
int dnx2VoiceRunning(void){return voice_call.running
#ifndef DNX2_RESIDENT_RUNTIME
 ||(voice_resident_mode&&voice_remote.status.running)
#endif
 ;}
const char *dnx2VoiceNotice(void){return voice_notice;}
const char *dnx2VoiceChannelName(void){
#ifndef DNX2_RESIDENT_RUNTIME
 if(voice_resident_mode)return voice_remote.label;
#endif
 return voice_call.label;}
int dnx2VoiceSelected(void){return dnx2VoiceRunning()&&account.selected_channel>=0&&!strcmp(account.channels[account.selected_channel].id,voice_call.channel);}
static void voice_worker(void *unused){
 (void)unused;int stage=0;unsigned code=0;int sockets=0,ssl=0;
 if(R_FAILED(socketInitialize(NULL)))goto done;
 sockets=1;
 if(R_FAILED(sslInitialize(1)))goto done;
 ssl=1;
 (void)voice_control_session(voice_call.token,voice_call.guild,voice_call.channel,voice_call.user,voice_call.notice,&stage,&code);
 done:
 if(ssl)sslExit();
 if(sockets)socketExit();
 wipe(voice_call.token,sizeof(voice_call.token));__atomic_store_n(&voice_call.done,1,__ATOMIC_RELEASE);
}
int dnx2VoiceApply(void){
#ifndef DNX2_RESIDENT_RUNTIME
 int remote_changed=voice_resident_apply();if(voice_resident_mode)return remote_changed;
#else
 int remote_changed=0;
#endif
 if(!voice_call.running||!__atomic_load_n(&voice_call.done,__ATOMIC_ACQUIRE))return remote_changed;
 threadWaitForExit(&voice_call.thread);threadClose(&voice_call.thread);voice_call.running=0;
 snprintf(voice_notice,sizeof(voice_notice),"%s",voice_call.notice[0]?voice_call.notice:"Voice connection ended.");return 1;
}
void dnx2VoiceStop(void){
#ifndef DNX2_RESIDENT_RUNTIME
 if(voice_resident_mode){(void)dnx2ResidentClientStop();voice_resident_mode=0;memset(&voice_remote,0,sizeof(voice_remote));}
#endif
 if(voice_call.running){__atomic_store_n(&voice_stop_request,1,__ATOMIC_RELEASE);threadWaitForExit(&voice_call.thread);threadClose(&voice_call.thread);voice_call.running=0;}
 wipe(voice_call.token,sizeof(voice_call.token));snprintf(voice_notice,sizeof(voice_notice),"Voice disconnected.");__atomic_store_n(&voice_mic_level,0,__ATOMIC_RELEASE);voice_set(0);
}
void dnx2VoiceReset(void){dnx2VoiceStop();wipe(voice_gateway_session,sizeof(voice_gateway_session));wipe(voice_gateway_resume_host,sizeof(voice_gateway_resume_host));voice_gateway_sequence=-1;voice_gateway_identify_after=0;}
int dnx2VoiceStart(void){
 if(!account.signed_in||account.private_mode||account.selected_channel<0||!session_token[0])return 0;
 int type=account.channels[account.selected_channel].type;if(type!=2&&type!=13)return 0;
 if(dnx2VoiceSelected())return 1;
#ifndef DNX2_RESIDENT_RUNTIME
 if(voice_resident_mode)return dnx2VoiceBackgroundStart();
#endif
 dnx2VoiceStop();memset(&voice_call,0,sizeof(voice_call));voice_notice[0]=0;
 snprintf(voice_call.token,sizeof(voice_call.token),"%s",session_token);
 snprintf(voice_call.guild,24,"%s",account.servers[account.selected].id);snprintf(voice_call.channel,24,"%s",account.channels[account.selected_channel].id);snprintf(voice_call.label,sizeof(voice_call.label),"%s",account.channels[account.selected_channel].name);snprintf(voice_call.user,24,"%s",account.user_id);
 voice_members_reset(voice_call.user,account.display_name[0]?account.display_name:account.username);
 voice_members_self_profile(account.avatar,account.default_avatar,voice_call.guild);
 __atomic_store_n(&voice_stop_request,0,__ATOMIC_RELEASE);
 if(R_SUCCEEDED(threadCreate(&voice_call.thread,voice_worker,NULL,NULL,512*1024,0x3b,-2))){if(R_SUCCEEDED(threadStart(&voice_call.thread))){voice_call.running=1;return 1;}threadClose(&voice_call.thread);}
 wipe(voice_call.token,sizeof(voice_call.token));snprintf(voice_notice,sizeof(voice_notice),"Voice worker could not start.");return 0;
}
