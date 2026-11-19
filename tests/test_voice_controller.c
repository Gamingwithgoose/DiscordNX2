// SPDX-License-Identifier: MIT
#include "native_login.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
typedef int Thread;typedef int Result;typedef uint64_t u64;
#define R_FAILED(x) ((x)!=0)
#define R_SUCCEEDED(x) ((x)==0)
static Dnx2Account account;static char session_token[1024]="token";
static char voice_gateway_session[160],voice_gateway_resume_host[256];static long long voice_gateway_sequence=-1;static u64 voice_gateway_identify_after;static int voice_stop_request,services,joined,closed,state;static unsigned voice_mic_level;
static void wipe(void *p,size_t n){memset(p,0,n);}static void voice_set(unsigned s){state=(int)s;}
static void voice_members_reset(const char *id,const char *name){assert(!strcmp(id,"300000000000000001")&&!strcmp(name,"Self"));}
static int socketInitialize(void *p){(void)p;services++;return 0;}static int sslInitialize(int v){(void)v;services++;return 0;}static void socketExit(void){services--;}static void sslExit(void){services--;}
static int threadCreate(Thread *t,void (*f)(void*),void *arg,void *stack,size_t n,int priority,int core){(void)t;(void)f;(void)arg;(void)stack;(void)priority;(void)core;assert(n>=512*1024);return 0;}static int threadStart(Thread *t){(void)t;return 0;}static void threadWaitForExit(Thread *t){(void)t;}static void threadClose(Thread *t){(void)t;closed++;}
static int voice_control_session(const char *token,const char *guild,const char *channel,const char *user,char notice[160],int *stage,unsigned *code){assert(services==2);assert(!strcmp(token,"token")&&!strcmp(guild,"100000000000000001")&&!strcmp(channel,"200000000000000001")&&!strcmp(user,"300000000000000001"));joined++;*stage=7;*code=0;snprintf(notice,160,"Call ended");return 1;}
static void voice_members_self_profile(const char *a,int d,const char *g){(void)a;(void)d;(void)g;}
#include "resident_voice.h"
typedef int Mutex;static void mutexLock(Mutex *p){(void)p;}static void mutexUnlock(Mutex *p){(void)p;}
static Mutex voice_members_mutex;static int voice_muted;static struct {int count;Dnx2VoiceMember list[33];} voice_members;
static void voice_members_changed(void){}
int dnx2VoiceMuted(void){return voice_muted;}
static DnxResidentSnapshot remote;static int detaches,remote_stops;
void dnx2ResidentClientStart(void){}void dnx2ResidentClientDetach(void){detaches++;}
int dnx2ResidentClientRead(DnxResidentSnapshot *out){*out=remote;return 1;}
int dnx2ResidentClientLaunch(const DnxResidentConfig *c){assert(dnx_resident_config_valid(c));remote.status.available=remote.status.running=1;remote.status.state=4;strcpy(remote.channel,c->channel);strcpy(remote.guild,c->guild);strcpy(remote.label,c->label);return 1;}
int dnx2ResidentClientStop(void){remote_stops++;remote.status.running=0;return 1;}
#include "voice_controller.inc"
int main(void){account.signed_in=1;account.server_count=1;account.selected_channel=0;account.channel_count=2;strcpy(account.display_name,"Self");strcpy(account.user_id,"300000000000000001");strcpy(account.servers[0].id,"100000000000000001");strcpy(account.channels[0].id,"200000000000000001");strcpy(account.channels[0].name,"Voice room");account.channels[0].type=2;strcpy(account.channels[1].id,"200000000000000002");account.channels[1].type=0;
assert(dnx2VoiceStart()&&dnx2VoiceRunning()&&dnx2VoiceSelected());assert(dnx2VoiceStart()&&closed==0);account.selected_channel=1;strcpy(account.servers[0].id,"100000000000000002");assert(dnx2VoiceRunning()&&!dnx2VoiceSelected());voice_worker(NULL);assert(joined==1&&services==0&&!voice_call.token[0]);assert(dnx2VoiceApply()&&!dnx2VoiceRunning()&&closed==1&&!strcmp(dnx2VoiceNotice(),"Call ended"));assert(!dnx2VoiceStart());dnx2VoiceStop();assert(state==0);remote.status.available=1;dnx2VoiceApply();strcpy(session_token,"synthetic-token-for-test-only");account.selected_channel=0;
assert(dnx2VoiceBackgroundStart()&&dnx2VoiceBackground());dnx2VoiceApply();assert(dnx2VoiceRunning()&&dnx2VoiceSelected());
dnx2VoiceAppExit();assert(detaches==1&&!remote_stops&&remote.status.running);/* IPC detach must not hang up */
dnx2VoiceApply();assert(dnx2VoiceBackground()&&dnx2VoiceRunning());/* reopen adopts the still-active call */
account.selected_channel=1;assert(!dnx2VoiceSelected()&&dnx2VoiceRunning());
dnx2VoiceStop();dnx2VoiceApply();assert(remote_stops==1&&!dnx2VoiceRunning());
puts("PASS resident handoff, exit detach, reopen/adopt, explicit remote disconnect; independent call: immutable credentials/context through navigation, same channel no-op, text navigation preserves call, done/join/cleanup");}
