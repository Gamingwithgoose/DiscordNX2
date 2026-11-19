// SPDX-License-Identifier: MIT
/* Chunked, RAM-only credentials and snapshots over the existing scalar IPC API. */
#include "resident_voice.h"
#include <stdio.h>
typedef struct {DnxResidentConfig pending;size_t written;uint32_t transfer,snapshot,expiry;DnxResidentSnapshot cache;Mutex lock;} ResidentContext;
static ResidentContext resident;
static void resident_clear(void *p,size_t n){volatile unsigned char *v=p;while(n--)*v++=0;}
static int resident_command(u32 command,const void *input,size_t input_size,void *output,size_t *out_size){
 if(command<DNX_R_STATUS||command>DNX_R_READ)return 0;
 mutexLock(&resident.lock);int ok=1;*out_size=0;
 if(command==DNX_R_STATUS){DnxResidentStatus out;dnx2ResidentEngineStatus(&out);memcpy(output,&out,sizeof(out));*out_size=sizeof(out);}
 else if(command==DNX_R_BEGIN){DnxResidentBegin request={0};if(!input||input_size<sizeof(request)){ok=0;goto done;}memcpy(&request,input,sizeof(request));if(request.size!=sizeof(DnxResidentConfig)){ok=0;goto done;}
  resident_clear(&resident.pending,sizeof(resident.pending));resident.written=0;resident.transfer=0;
  if(R_FAILED(csrngInitialize())){ok=0;goto done;}Result rc=csrngGetRandomBytes(&resident.transfer,sizeof(resident.transfer));csrngExit();if(R_FAILED(rc)||!resident.transfer){ok=0;goto done;}resident.expiry=500;
  memcpy(output,&resident.transfer,4);*out_size=4;
 }
 else if(command==DNX_R_WRITE){DnxResidentPart part={0};if(!input||input_size<sizeof(part)){ok=0;goto done;}memcpy(&part,input,sizeof(part));
  if(!resident.transfer||part.id!=resident.transfer||!part.size||part.size>DNX_RESIDENT_PART||part.offset!=resident.written||part.size>sizeof(resident.pending)-resident.written){resident_clear(&part,sizeof(part));ok=0;goto done;}
  memcpy((unsigned char*)&resident.pending+resident.written,part.bytes,part.size);resident.written+=part.size;resident.expiry=500;resident_clear(&part,sizeof(part));
 }
 else if(command==DNX_R_START){uint32_t id=0;if(input&&input_size>=4)memcpy(&id,input,4);
  if(!id||id!=resident.transfer||resident.written!=sizeof(resident.pending)||!dnx_resident_config_valid(&resident.pending)){ok=0;goto done;}
  ok=dnx2ResidentEngineStart(&resident.pending);resident_clear(&resident.pending,sizeof(resident.pending));resident.transfer=resident.expiry=0;resident.written=0;
 }
 else if(command==DNX_R_MUTE){uint32_t muted=2;if(input&&input_size>=4)memcpy(&muted,input,4);if(muted>1){ok=0;goto done;}if((unsigned)dnx2VoiceMuted()!=muted)dnx2VoiceToggleMute();}
 else if(command==DNX_R_STOP)dnx2VoiceStop();
 else if(command==DNX_R_SNAPSHOT){dnx2ResidentEngineSnapshot(&resident.cache);if(++resident.snapshot==0)resident.snapshot=1;DnxResidentBegin out={resident.snapshot,sizeof(resident.cache)};memcpy(output,&out,sizeof(out));*out_size=sizeof(out);}
 else if(command==DNX_R_READ){DnxResidentPart part={0};if(!input||input_size<12){ok=0;goto done;}memcpy(&part,input,12);
  if(!part.id||part.id!=resident.snapshot||!part.size||part.size>DNX_RESIDENT_PART||part.offset>sizeof(resident.cache)||part.size>sizeof(resident.cache)-part.offset){ok=0;goto done;}
  memcpy(part.bytes,(const unsigned char*)&resident.cache+part.offset,part.size);memcpy(output,&part,sizeof(part));*out_size=sizeof(part);
 }
 done:mutexUnlock(&resident.lock);return ok?1:-1;
}
static void resident_tick(void){mutexLock(&resident.lock);dnx2VoiceApply();if(resident.expiry&&!--resident.expiry){resident_clear(&resident.pending,sizeof(resident.pending));resident.transfer=0;resident.written=0;}mutexUnlock(&resident.lock);}
