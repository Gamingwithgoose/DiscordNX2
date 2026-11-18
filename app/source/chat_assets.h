// SPDX-License-Identifier: MIT
#ifndef DNX2_CHAT_ASSETS_H
#define DNX2_CHAT_ASSETS_H
#include <stddef.h>
#define DNX2_MEDIA_MAX 128
typedef struct {char id[24],name[129];int animated,format;} Dnx2Media;
typedef struct {char guild[24];int loaded,emoji_count,sticker_count;Dnx2Media emoji[128],stickers[128];} Dnx2MediaCatalog;
int dnx2DecodePng(const unsigned char *bytes,size_t length,unsigned char rgba[64*64*4]);
int dnx2JobStart(int op,const char *text,const char *sticker);
int dnx2JobBusy(void);
int dnx2JobKind(void);
void dnx2JobStop(void);
int dnx2JobApply(void);
const char *dnx2JobDraft(void);
const char *dnx2JobSticker(void);
int dnx2RequestImages(int type,int first,int count);
const unsigned char *dnx2ChatImage(int type,int index);
const Dnx2MediaCatalog *dnx2MediaCatalog(void);
const unsigned char *dnx2MessageImage(int type,const char *id);
int dnx2RequestMessageImages(void);
void dnx2ChatReset(void);
#endif
