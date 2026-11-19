// SPDX-License-Identifier: MIT
/* Foreground call audio. Stop services before freeing DMA buffers. */
typedef struct {AudioInBuffer in[4];AudioOutBuffer out[8];void *input[4],*output[8];unsigned in_next,out_next,mono_used;int16_t mono[960];int input_open,output_open,started;u64 output_at;} VoiceAudio;
static void voice_audio_close(VoiceAudio *a){if(!a)return;if(a->input_open){audinStopAudioIn();audinExit();}if(a->output_open){audoutStopAudioOut();audoutExit();}for(int i=0;i<4;i++){if(a->input[i]){wipe(a->input[i],4096);free(a->input[i]);}}for(int i=0;i<8;i++){if(a->output[i]){wipe(a->output[i],4096);free(a->output[i]);}}wipe(a,sizeof(*a));}
static int voice_audio_open(VoiceAudio *a){
 if(R_FAILED(audinInitialize()))return 0;
 a->input_open=1;
 if(R_FAILED(audoutInitialize()))return 0;
 a->output_open=1;
 if(audinGetSampleRate()!=48000||audinGetChannelCount()!=2||audinGetPcmFormat()!=PcmFormat_Int16||audoutGetSampleRate()!=48000||audoutGetChannelCount()!=2||audoutGetPcmFormat()!=PcmFormat_Int16)return 0;
 for(int i=0;i<4;i++){a->input[i]=memalign(4096,4096);if(!a->input[i])return 0;memset(a->input[i],0,4096);a->in[i].buffer=a->input[i];a->in[i].buffer_size=a->in[i].data_size=4096;if(R_FAILED(audinAppendAudioInBuffer(&a->in[i])))return 0;}
 for(int i=0;i<8;i++){a->output[i]=memalign(4096,4096);if(!a->output[i])return 0;memset(a->output[i],0,4096);a->out[i].buffer=a->output[i];a->out[i].buffer_size=4096;a->out[i].data_size=3840;}
 if(R_FAILED(audinStartAudioIn())||R_FAILED(audoutStartAudioOut()))return 0;
 /* 40 ms initial playout cushion, then a bounded native output queue. */
 for(int i=0;i<2;i++)if(R_FAILED(audoutAppendAudioOutBuffer(&a->out[a->out_next++])))return 0;
 a->started=1;a->output_at=dnx2MonotonicMs();return 1;
}
static int voice_audio_tick(VoiceAudio *a,DnxVoiceEngine *e,int udp,int muted){
 for(unsigned work=0;work<4;work++){unsigned i=a->in_next%4;bool contains=true;if(R_FAILED(audinContainsAudioInBuffer(&a->in[i],&contains)))return 0;if(contains)break;
  AudioInBuffer *released=NULL;u32 count=0;(void)audinGetReleasedAudioInBuffer(&released,&count);
  const int16_t *pcm=a->input[i];for(unsigned j=0;j<1024;j++){a->mono[a->mono_used++]=(int16_t)(((int)pcm[j*2]+pcm[j*2+1])/2);if(a->mono_used==960){if(!muted){unsigned char packet[2048];int n=dnx_voice_encode(e,a->mono,packet);if(n>0&&send(udp,packet,(size_t)n,0)!=n){if(errno!=EAGAIN&&errno!=EWOULDBLOCK)return 0;}wipe(packet,sizeof(packet));}a->mono_used=0;}}
  wipe(a->input[i],4096);a->in[i].data_size=4096;if(R_FAILED(audinAppendAudioInBuffer(&a->in[i])))return 0;a->in_next++;
 }
 u64 now=dnx2MonotonicMs();if(now>=a->output_at){unsigned i=a->out_next%8;bool contains=true;if(R_FAILED(audoutContainsAudioOutBuffer(&a->out[i],&contains)))return 0;if(!contains){AudioOutBuffer *released=NULL;u32 count=0;(void)audoutGetReleasedAudioOutBuffer(&released,&count);dnx_voice_mix(e,a->output[i]);if(R_FAILED(audoutAppendAudioOutBuffer(&a->out[i])))return 0;a->out_next++;a->output_at+=20;if(now-a->output_at>60)a->output_at=now;}}
 return 1;
}
