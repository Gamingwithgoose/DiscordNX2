// SPDX-License-Identifier: MIT
#ifndef DNX2_VOICE_ENGINE_H
#define DNX2_VOICE_ENGINE_H
#include <stdint.h>
#include <stddef.h>
typedef struct DnxVoiceEngine DnxVoiceEngine;
typedef int (*DnxVoiceSignal)(void *user,int binary,const unsigned char *data,size_t size);
typedef struct {unsigned tx,rx,dropped,mls_updates,udp_received,unknown_ssrc,no_key,transport_rejected,e2ee_rejected,opus_rejected,encode_errors,peer_count,ssrc_count,key_packages,external_senders,proposals,commits,welcomes,executes;int secure_ready,transport_ready,failed;} DnxVoiceStats;
unsigned dnx_voice_max_version(void);
DnxVoiceEngine *dnx_voice_create(const char *self,const char *channel,DnxVoiceSignal signal,void *user);
void dnx_voice_destroy(DnxVoiceEngine *engine);
int dnx_voice_signal_json(DnxVoiceEngine *engine,const char *json);
int dnx_voice_signal_binary(DnxVoiceEngine *engine,const unsigned char *data,size_t size);
void dnx_voice_ssrc(DnxVoiceEngine *engine,uint32_t ssrc);
int dnx_voice_encode(DnxVoiceEngine *engine,const int16_t pcm[960],unsigned char packet[2048]);
int dnx_voice_receive(DnxVoiceEngine *engine,const unsigned char *packet,size_t size);
void dnx_voice_mix(DnxVoiceEngine *engine,int16_t stereo[1920]);
void dnx_voice_stats(const DnxVoiceEngine *engine,DnxVoiceStats *stats);
#endif
