// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
typedef uint64_t u64;typedef uint32_t u32;typedef int Result;
#define R_FAILED(x) ((x)!=0)
#define PcmFormat_Int16 2
typedef struct AudioInBuffer {struct AudioInBuffer *next;void *buffer;u64 buffer_size,data_size,data_offset;} AudioInBuffer;
typedef struct AudioOutBuffer {struct AudioOutBuffer *next;void *buffer;u64 buffer_size,data_size,data_offset;} AudioOutBuffer;
typedef struct DnxVoiceEngine {int unused;} DnxVoiceEngine;
static u64 now;static int muted,capturing,stops,starts,tx,mixed,input_requeue,out_append,done_out;
static AudioInBuffer *in_desc[4],*release_input;static AudioOutBuffer *out_desc[8];
static void wipe(void *p,size_t n){memset(p,0,n);}
static void *memalign(size_t align,size_t n){return aligned_alloc(align,n);}
static u64 dnx2MonotonicMs(void){return now;}
static int dnx2VoiceMuted(void){return muted;}
static Result audinInitialize(void){return 0;}static Result audoutInitialize(void){return 0;}
static void audinExit(void){}static void audoutExit(void){}
static int audinGetSampleRate(void){return 48000;}static int audoutGetSampleRate(void){return 48000;}
static int audinGetChannelCount(void){return 2;}static int audoutGetChannelCount(void){return 2;}
static int audinGetPcmFormat(void){return 2;}static int audoutGetPcmFormat(void){return 2;}
static Result audinStartAudioIn(void){capturing=1;starts++;return 0;}
static Result audinStopAudioIn(void){capturing=0;stops++;return 0;}
static Result audoutStartAudioOut(void){return 0;}static Result audoutStopAudioOut(void){return 0;}
static Result audinAppendAudioInBuffer(AudioInBuffer *b){for(int i=0;i<4;i++)if(in_desc[i]==b)return ++input_requeue,0;for(int i=0;i<4;i++)if(!in_desc[i]){in_desc[i]=b;break;}return 0;}
static Result audinGetReleasedAudioInBuffer(AudioInBuffer **b,u32 *n){*b=release_input;*n=release_input?1:0;release_input=NULL;return 0;}
static Result audoutAppendAudioOutBuffer(AudioOutBuffer *b){out_append++;for(int i=0;i<8;i++)if(out_desc[i]==b)return 0;for(int i=0;i<8;i++)if(!out_desc[i]){out_desc[i]=b;break;}return 0;}
static Result audoutGetReleasedAudioOutBuffer(AudioOutBuffer **b,u32 *n){*n=0;*b=NULL;if(done_out){*b=out_desc[0];*n=1;done_out=0;}return 0;}
static int dnx_voice_encode(DnxVoiceEngine *e,const int16_t *pcm,unsigned char *packet){(void)e;assert(pcm[0]==1234);packet[0]=1;tx++;return 32;}
static int send(int fd,const void *p,size_t n,int flags){(void)fd;(void)p;(void)flags;return (int)n;}
static void dnx_voice_mix(DnxVoiceEngine *e,int16_t *pcm){(void)e;mixed++;for(int i=0;i<1920;i++)pcm[i]=567;}
#include "voice_audio.inc"
int main(void){VoiceAudio a={0};DnxVoiceEngine e={0};assert(voice_audio_open(&a)&&capturing);assert(out_append==2);assert(voice_audio_tick(&a,&e,1,0)&&mixed==0&&a.captures==0);
/* Completion order deliberately differs from submission order. Only 960 frames valid. */
for(int i=0;i<1920;i++)((int16_t*)a.input[2])[i]=1234;
a.in[2].data_size=3840;release_input=&a.in[2];now=20;assert(voice_audio_tick(&a,&e,1,0));assert(a.captures==1&&a.samples==960&&a.sent==1&&tx==1&&a.input_peak==1234);assert(mixed==1&&a.output_at==40);
now=21;assert(voice_audio_tick(&a,&e,1,0)&&mixed==1);now=39;assert(voice_audio_tick(&a,&e,1,0)&&mixed==1);now=40;assert(voice_audio_tick(&a,&e,1,0)&&mixed==2);
muted=1;release_input=&a.in[1];assert(voice_audio_tick(&a,&e,1,1)&&!capturing&&stops==1&&tx==1);assert(a.mono_used==0&&a.input_peak==0);for(int i=0;i<4;i++)for(int j=0;j<4096;j++)assert(!((unsigned char*)a.input[i])[j]);
assert(voice_audio_tick(&a,&e,1,1)&&stops==1);muted=0;assert(voice_audio_tick(&a,&e,1,0)&&capturing&&starts==2);voice_audio_close(&a);assert(!a.input_open&&!a.output_open&&!a.started);puts("PASS native audio queue: exact released buffer/length, no early playback, native mute/zero/send gating, unmute and cleanup");}
