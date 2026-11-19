// SPDX-License-Identifier: MIT
#ifndef DNX2_RESIDENT_VOICE_H
#define DNX2_RESIDENT_VOICE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "native_login.h"
#define DNX_RESIDENT_MAGIC 0x44525631u
#define DNX_RESIDENT_VERSION 1u
#define DNX_RESIDENT_MEMBERS 33
#define DNX_RESIDENT_PART 48u
enum {DNX_R_STATUS=4,DNX_R_BEGIN,DNX_R_WRITE,DNX_R_START,DNX_R_MUTE,DNX_R_STOP,DNX_R_SNAPSHOT,DNX_R_READ};
typedef struct {uint32_t magic,version;char token[1024],guild[24],channel[24],user[24],label[401],name[321],avatar[80];uint32_t default_avatar,muted;uint64_t identify_after;} DnxResidentConfig;
typedef struct {uint32_t magic,version,available,running,state,muted,mic_level,members_revision,member_count,error,wait_seconds;} DnxResidentStatus;
typedef struct {DnxResidentStatus status;char label[401],guild[24],channel[24];Dnx2VoiceMember members[DNX_RESIDENT_MEMBERS];} DnxResidentSnapshot;
typedef struct {uint32_t id,offset,size;unsigned char bytes[DNX_RESIDENT_PART];} DnxResidentPart;
typedef struct {uint32_t id,size;} DnxResidentBegin;
static inline size_t dnx_resident_length(const char *s,size_t cap){size_t n=0;while(n<cap&&s[n])n++;return n;}
static inline int dnx_resident_id(const char *s){size_t n=dnx_resident_length(s,24);if(n<15||n>20)return 0;for(size_t i=0;i<n;i++)if(s[i]<'0'||s[i]>'9')return 0;return 1;}
static inline int dnx_resident_config_valid(const DnxResidentConfig *c){
 if(!c||c->magic!=DNX_RESIDENT_MAGIC||c->version!=DNX_RESIDENT_VERSION||c->muted>1||!dnx_resident_id(c->guild)||!dnx_resident_id(c->channel)||!dnx_resident_id(c->user))return 0;
 size_t n=dnx_resident_length(c->token,sizeof(c->token));if(n<16||n>=sizeof(c->token))return 0;for(size_t i=0;i<n;i++)if((unsigned char)c->token[i]<33||(unsigned char)c->token[i]>126||c->token[i]=='"'||c->token[i]=='\\')return 0;
 return memchr(c->label,0,sizeof(c->label))&&memchr(c->name,0,sizeof(c->name))&&memchr(c->avatar,0,sizeof(c->avatar));
}
_Static_assert(sizeof(DnxResidentPart)<=64,"Plugin scalar size");
_Static_assert(sizeof(DnxResidentStatus)<=64,"Plugin status size");
#ifdef DNX2_RESIDENT_RUNTIME
int dnx2ResidentEngineStart(const DnxResidentConfig *c);
void dnx2ResidentEngineStatus(DnxResidentStatus *out);
void dnx2ResidentEngineSnapshot(DnxResidentSnapshot *out);
#else
void dnx2ResidentClientStart(void);
void dnx2ResidentClientDetach(void);
int dnx2ResidentClientRead(DnxResidentSnapshot *out);
int dnx2ResidentClientLaunch(const DnxResidentConfig *config);
int dnx2ResidentClientMute(int muted);
int dnx2ResidentClientStop(void);
#endif
#endif
