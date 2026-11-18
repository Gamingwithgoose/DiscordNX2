// SPDX-License-Identifier: MIT
#ifndef DNX2_CHANNEL_MEMORY_H
#define DNX2_CHANNEL_MEMORY_H
#include "client_data.h"
typedef struct {char guild[24],channel[24];} DnxChannelChoice;
typedef struct {DnxChannelChoice items[201];int count;} DnxChannelMemory;
static inline int dnx_choice_key(const char *key){return !strcmp(key,"dm")||dnx_id(key);}
static inline const char *dnx_choice_get(const DnxChannelMemory *m,const char *guild){for(int i=0;i<m->count;i++)if(!strcmp(m->items[i].guild,guild))return m->items[i].channel;return "";}
static inline void dnx_choice_set(DnxChannelMemory *m,const char *guild,const char *channel){if(!dnx_choice_key(guild)||!dnx_id(channel))return;int i;for(i=0;i<m->count;i++)if(!strcmp(m->items[i].guild,guild))break;if(i>=201)return;if(i==m->count)m->count++;snprintf(m->items[i].guild,24,"%s",guild);snprintf(m->items[i].channel,24,"%s",channel);}
static inline void dnx_choice_load(DnxChannelMemory *m,const char *path){memset(m,0,sizeof(*m));FILE *f=fopen(path,"r");if(!f)return;char line[96],guild[24],channel[24],extra;if(!fgets(line,sizeof(line),f)||strcmp(line,"DNX2-CHANNELS-1\n")){fclose(f);return;}int lines=0;while(lines++<201&&fgets(line,sizeof(line),f))if(sscanf(line,"%23s %23s %c",guild,channel,&extra)==2)dnx_choice_set(m,guild,channel);fclose(f);}
static inline int dnx_choice_save(const DnxChannelMemory *m,const char *path){char temp[300];int n=snprintf(temp,sizeof(temp),"%s.tmp",path);if(n<0||(size_t)n>=sizeof(temp))return 0;FILE *f=fopen(temp,"w");if(!f)return 0;int ok=fputs("DNX2-CHANNELS-1\n",f)>=0;for(int i=0;i<m->count&&ok;i++)ok=fprintf(f,"%s %s\n",m->items[i].guild,m->items[i].channel)>0;if(fflush(f))ok=0;if(fclose(f))ok=0;if(ok&&rename(temp,path)==0)return 1;remove(temp);return 0;}
#endif
