// SPDX-License-Identifier: MIT
#ifndef DNX2_CHAT_ASSETS_H
#define DNX2_CHAT_ASSETS_H
#include <stddef.h>
#include "profile_data.h"
#define DNX2_MEDIA_MAX 128
typedef struct {char id[24],name[129];int animated,format;} Dnx2Media;
typedef struct {char guild[24];int loaded,emoji_count,sticker_count;Dnx2Media emoji[128],stickers[128];} Dnx2MediaCatalog;
typedef struct {unsigned char *pixels;int width,height,frames,delays[60];} Dnx2DecodedMedia;
int dnx2DecodeMedia(const unsigned char *bytes,size_t length,Dnx2DecodedMedia *out);
void dnx2FreeMedia(Dnx2DecodedMedia *media);
const Dnx2DecodedMedia *dnx2MessageMedia(const char *id);
const unsigned char *dnx2AuthorAvatar(const char *id,const char *avatar,const char *guild_avatar,unsigned default_avatar);
int dnx2DecodePng(const unsigned char *bytes,size_t length,unsigned char rgba[64*64*4]);
int dnx2JobStart(int op,const char *text,const char *sticker);
int dnx2JobBusy(void);
int dnx2JobKind(void);
void dnx2JobStop(void);
int dnx2JobApply(void);
const char *dnx2JobDraft(void);
const char *dnx2JobSticker(void);
int dnx2RequestVoiceImages(void);
int dnx2RequestDMImages(int first,int count);
const unsigned char *dnx2DMImage(int index);
int dnx2RequestImages(int type,int first,int count);
const unsigned char *dnx2ChatImage(int type,int index);
const Dnx2MediaCatalog *dnx2MediaCatalog(void);
const unsigned char *dnx2TagBadge(const char *guild,const char *hash);
int dnx2MessageMediaState(const char *id);
const unsigned char *dnx2MessageImage(int type,const char *id);
int dnx2MemberState(const char *id);
const Dnx2Member *dnx2Member(const char *id);
const Dnx2GuildRoles *dnx2GuildRoles(void);
int dnx2RequestProfiles(int first,int count);
int dnx2RequestMessageImages(int first,int count);
void dnx2ChatReset(void);
#endif
