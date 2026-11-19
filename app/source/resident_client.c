// SPDX-License-Identifier: MIT
/* Only kernel IPC carries credentials. Background service never returns a token. */
#include "resident_voice.h"
#include <switch.h>
#include <kaihen.h>
#include <stdio.h>
#include <stdlib.h>
static Service resident_service;static Mutex resident_ipc_mutex,resident_cache_mutex;static Thread resident_thread;static int resident_started,resident_stop,resident_changed;static DnxResidentSnapshot resident_cache;
static void resident_wipe(void *p,size_t n){volatile unsigned char *v=p;while(n--)*v++=0;}
static int resident_open(void){
 if(serviceIsActive(&resident_service))return 1;
 Service ro={0},hen={0};Result rc=kaihenGetRoSession(&ro);if(R_SUCCEEDED(rc))rc=kaihenOpenHomebrewSession(&ro,&hen);
 HenPluginName name={0};memcpy(name.name,"discordnx2",sizeof("discordnx2"));if(R_SUCCEEDED(rc))rc=henOpenPlugin(&hen,&name,&resident_service);
 if(serviceIsActive(&hen))serviceClose(&hen);if(serviceIsActive(&ro))serviceClose(&ro);return R_SUCCEEDED(rc);
}
static int resident_status(DnxResidentStatus *s){return resident_open()&&R_SUCCEEDED(serviceDispatchOut(&resident_service,DNX_R_STATUS,*s))&&s->magic==DNX_RESIDENT_MAGIC&&s->version==DNX_RESIDENT_VERSION&&s->available;}
static void resident_poll_worker(void *unused){
 (void)unused;DnxResidentSnapshot *next=calloc(1,sizeof(*next));if(!next)return;
 while(!__atomic_load_n(&resident_stop,__ATOMIC_ACQUIRE)){
  DnxResidentStatus status={0};mutexLock(&resident_ipc_mutex);int ok=resident_status(&status);
  if(ok){
   int refresh=status.running&&(status.members_revision!=next->status.members_revision||!next->status.running||!next->label[0]);
   if(refresh){DnxResidentBegin begin={0};ok=R_SUCCEEDED(serviceDispatchOut(&resident_service,DNX_R_SNAPSHOT,begin))&&begin.id&&begin.size==sizeof(*next);
    for(size_t at=0;ok&&at<sizeof(*next);at+=DNX_RESIDENT_PART){DnxResidentPart request={0},response={0};request.id=begin.id;request.offset=(uint32_t)at;request.size=(uint32_t)(sizeof(*next)-at);if(request.size>DNX_RESIDENT_PART)request.size=DNX_RESIDENT_PART;
     ok=R_SUCCEEDED(serviceDispatchInOut(&resident_service,DNX_R_READ,request,response))&&response.id==request.id&&response.offset==request.offset&&response.size==request.size;if(ok)memcpy((unsigned char*)next+at,response.bytes,response.size);
    }
   }else next->status=status;
   if(next->status.member_count>DNX_RESIDENT_MEMBERS)ok=0;
   if(ok){next->label[sizeof(next->label)-1]=0;next->guild[23]=next->channel[23]=0;for(unsigned i=0;i<next->status.member_count;i++){next->members[i].id[23]=0;next->members[i].name[320]=0;next->members[i].avatar[79]=0;next->members[i].guild_avatar[79]=0;next->members[i].guild[23]=0;}}
  }
  if(!ok){if(serviceIsActive(&resident_service))serviceClose(&resident_service);memset(&next->status,0,sizeof(next->status));}
  mutexLock(&resident_cache_mutex);if(memcmp(&resident_cache,next,sizeof(*next))){resident_cache=*next;__atomic_store_n(&resident_changed,1,__ATOMIC_RELEASE);}mutexUnlock(&resident_cache_mutex);mutexUnlock(&resident_ipc_mutex);
  for(int i=0;i<50&&!__atomic_load_n(&resident_stop,__ATOMIC_ACQUIRE);i++)svcSleepThread(10000000LL);
 }
 resident_wipe(next,sizeof(*next));free(next);
}
void dnx2ResidentClientStart(void){if(resident_started)return;__atomic_store_n(&resident_stop,0,__ATOMIC_RELEASE);if(R_SUCCEEDED(threadCreate(&resident_thread,resident_poll_worker,NULL,NULL,64*1024,0x3b,-2))){if(R_SUCCEEDED(threadStart(&resident_thread))){resident_started=1;return;}threadClose(&resident_thread);}}
void dnx2ResidentClientDetach(void){if(resident_started){__atomic_store_n(&resident_stop,1,__ATOMIC_RELEASE);threadWaitForExit(&resident_thread);threadClose(&resident_thread);resident_started=0;}mutexLock(&resident_ipc_mutex);if(serviceIsActive(&resident_service))serviceClose(&resident_service);mutexUnlock(&resident_ipc_mutex);mutexLock(&resident_cache_mutex);resident_wipe(&resident_cache,sizeof(resident_cache));mutexUnlock(&resident_cache_mutex);}
int dnx2ResidentClientRead(DnxResidentSnapshot *out){if(!out)return 0;mutexLock(&resident_cache_mutex);*out=resident_cache;mutexUnlock(&resident_cache_mutex);return __atomic_exchange_n(&resident_changed,0,__ATOMIC_ACQ_REL);}
int dnx2ResidentClientLaunch(const DnxResidentConfig *config){
 if(!dnx_resident_config_valid(config))return 0;dnx2ResidentClientStart();mutexLock(&resident_ipc_mutex);DnxResidentStatus status;int ok=resident_status(&status);uint32_t id=0;
 if(ok){DnxResidentBegin begin={0,sizeof(*config)};ok=R_SUCCEEDED(serviceDispatchInOut(&resident_service,DNX_R_BEGIN,begin,id))&&id;}
 for(size_t at=0;ok&&at<sizeof(*config);at+=DNX_RESIDENT_PART){DnxResidentPart part={0};part.id=id;part.offset=(uint32_t)at;part.size=(uint32_t)(sizeof(*config)-at);if(part.size>DNX_RESIDENT_PART)part.size=DNX_RESIDENT_PART;memcpy(part.bytes,(const unsigned char*)config+at,part.size);ok=R_SUCCEEDED(serviceDispatchIn(&resident_service,DNX_R_WRITE,part));resident_wipe(&part,sizeof(part));}
 if(ok)ok=R_SUCCEEDED(serviceDispatchIn(&resident_service,DNX_R_START,id));if(ok){DnxResidentStatus current={0};if(resident_status(&current)){mutexLock(&resident_cache_mutex);memset(&resident_cache,0,sizeof(resident_cache));resident_cache.status=current;snprintf(resident_cache.label,401,"%s",config->label);snprintf(resident_cache.guild,24,"%s",config->guild);snprintf(resident_cache.channel,24,"%s",config->channel);__atomic_store_n(&resident_changed,1,__ATOMIC_RELEASE);mutexUnlock(&resident_cache_mutex);}}mutexUnlock(&resident_ipc_mutex);return ok;
}
int dnx2ResidentClientMute(int muted){uint32_t value=muted!=0;mutexLock(&resident_ipc_mutex);int ok=serviceIsActive(&resident_service)&&R_SUCCEEDED(serviceDispatchIn(&resident_service,DNX_R_MUTE,value));if(ok){mutexLock(&resident_cache_mutex);resident_cache.status.muted=value;__atomic_store_n(&resident_changed,1,__ATOMIC_RELEASE);mutexUnlock(&resident_cache_mutex);}mutexUnlock(&resident_ipc_mutex);return ok;}
int dnx2ResidentClientStop(void){mutexLock(&resident_ipc_mutex);int ok=serviceIsActive(&resident_service)&&R_SUCCEEDED(serviceDispatch(&resident_service,DNX_R_STOP));if(ok){mutexLock(&resident_cache_mutex);resident_cache.status.running=resident_cache.status.state=resident_cache.status.member_count=0;__atomic_store_n(&resident_changed,1,__ATOMIC_RELEASE);mutexUnlock(&resident_cache_mutex);}mutexUnlock(&resident_ipc_mutex);return ok;}
