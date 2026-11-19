// SPDX-License-Identifier: MIT
#define _POSIX_C_SOURCE 200809L
#define DNX2_RESIDENT_RUNTIME 1
#include "resident_voice.h"
#include <assert.h>
#include <stdio.h>
typedef unsigned u32;typedef int Result;typedef int Mutex;
#define R_FAILED(v) ((v)!=0)
static void mutexLock(Mutex *m){(void)m;}static void mutexUnlock(Mutex *m){(void)m;}
static int csrngInitialize(void){return 0;}static void csrngExit(void){}
static int csrngGetRandomBytes(void *p,size_t n){memset(p,23,n);return 0;}
static int starts,stops,muted;
int dnx2ResidentEngineStart(const DnxResidentConfig *c){assert(dnx_resident_config_valid(c));starts++;return 1;}
void dnx2ResidentEngineStatus(DnxResidentStatus *s){memset(s,0,sizeof(*s));s->magic=DNX_RESIDENT_MAGIC;s->version=1;s->available=1;s->running=starts>stops;}
void dnx2ResidentEngineSnapshot(DnxResidentSnapshot *s){memset(s,0,sizeof(*s));dnx2ResidentEngineStatus(&s->status);strcpy(s->label,"Synthetic room");}
int dnx2VoiceMuted(void){return muted;}void dnx2VoiceToggleMute(void){muted=!muted;}void dnx2VoiceStop(void){stops++;}int dnx2VoiceApply(void){return 0;}
#include "../plugin/source/resident_service.inc"
static unsigned char output[64];static size_t size;
static int command(unsigned c,const void *in,size_t n){memset(output,0,sizeof(output));size=0;int rc=resident_command(c,in,n,output,&size);assert(size<=64);return rc;}
static uint32_t begin(void){DnxResidentBegin b={0,sizeof(DnxResidentConfig)};assert(command(DNX_R_BEGIN,&b,sizeof(b))==1&&size==4);uint32_t id;memcpy(&id,output,4);assert(id);return id;}
static void stage(const DnxResidentConfig *c,uint32_t id){for(size_t at=0;at<sizeof(*c);at+=48){DnxResidentPart p={.id=id,.offset=at,.size=sizeof(*c)-at};if(p.size>48)p.size=48;memcpy(p.bytes,(const char*)c+at,p.size);assert(command(DNX_R_WRITE,&p,sizeof(p))==1);}}
int main(void){
 DnxResidentConfig c={.magic=DNX_RESIDENT_MAGIC,.version=1};strcpy(c.guild,"100000000000000001");strcpy(c.channel,"200000000000000001");strcpy(c.user,"300000000000000001");strcpy(c.token,"synthetic-token-for-tests-only");assert(dnx_resident_config_valid(&c));
 c.muted=2;assert(!dnx_resident_config_valid(&c));c.muted=0;c.channel[0]='x';assert(!dnx_resident_config_valid(&c));c.channel[0]='2';
 assert(command(3,NULL,0)==0);assert(command(DNX_R_STATUS,NULL,0)==1&&size==sizeof(DnxResidentStatus));
 uint32_t id=begin();assert(command(DNX_R_START,&id,4)==-1&&!starts);
 DnxResidentPart p={.id=id,.offset=1,.size=1};assert(command(DNX_R_WRITE,&p,sizeof(p))==-1);p.offset=0;p.size=49;assert(command(DNX_R_WRITE,&p,sizeof(p))==-1);
 stage(&c,id);uint32_t wrong=id+1;assert(command(DNX_R_START,&wrong,4)==-1&&!starts);assert(command(DNX_R_START,&id,4)==1&&starts==1&&!resident.pending.token[0]&&!resident.transfer);
 uint32_t value=1;assert(command(DNX_R_MUTE,&value,4)==1&&muted);assert(command(DNX_R_MUTE,&value,4)==1&&muted);value=2;assert(command(DNX_R_MUTE,&value,4)==-1);
 assert(command(DNX_R_SNAPSHOT,NULL,0)==1);DnxResidentBegin b;memcpy(&b,output,sizeof(b));assert(b.size==sizeof(DnxResidentSnapshot));
 DnxResidentSnapshot snap={0};for(size_t at=0;at<sizeof(snap);at+=48){p.id=b.id;p.offset=at;p.size=sizeof(snap)-at;if(p.size>48)p.size=48;assert(command(DNX_R_READ,&p,sizeof(p))==1&&size==sizeof(p));DnxResidentPart reply;memcpy(&reply,output,sizeof(reply));assert(reply.id==p.id&&reply.offset==at&&reply.size==p.size);memcpy((char*)&snap+at,reply.bytes,reply.size);}assert(!strcmp(snap.label,"Synthetic room"));
 p.offset=sizeof(snap);p.size=1;assert(command(DNX_R_READ,&p,sizeof(p))==-1);p.offset=0;p.id++;assert(command(DNX_R_READ,&p,sizeof(p))==-1);
 id=begin();stage(&c,id);for(int i=0;i<500;i++)resident_tick();assert(!resident.transfer&&!resident.pending.token[0]&&!resident.written);assert(command(DNX_R_START,&id,4)==-1);
 assert(command(DNX_R_STOP,NULL,0)==1&&stops==1);
 puts("PASS resident IPC: scalar bounds, ordered chunks, token validation/wipe/expiry, snapshots, mute, explicit stop");
}
