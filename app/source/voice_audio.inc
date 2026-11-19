// SPDX-License-Identifier: MIT
/* Native audio queues are owned by the service until it RELEASES a descriptor.
 * ContainsBuffer is not a completion notification. Never infer capture from it. */
#define VOICE_INPUT_BYTES 4096
#define VOICE_INPUT_SLOTS 4
#define VOICE_OUTPUT_SLOTS 8
typedef struct {
 AudioInBuffer in[VOICE_INPUT_SLOTS];AudioOutBuffer out[VOICE_OUTPUT_SLOTS];
 void *input[VOICE_INPUT_SLOTS],*output[VOICE_OUTPUT_SLOTS];unsigned char out_owned[VOICE_OUTPUT_SLOTS];
 unsigned out_next,mono_used;int16_t mono[960];int input_open,output_open,started,capturing;
 u64 output_at;unsigned captures,samples,input_peak,encoded,sent,send_blocked,playbacks,output_peak,mute_changes;Result error;unsigned error_stage;
} VoiceAudio;
static int voice_audio_result(VoiceAudio *a,Result rc,unsigned stage){if(R_FAILED(rc)){a->error=rc;a->error_stage=stage;return 0;}return 1;}
static void voice_audio_close(VoiceAudio *a){
 if(!a)return;
 if(a->input_open){audinStopAudioIn();audinExit();a->input_open=0;}
 if(a->output_open){audoutStopAudioOut();audoutExit();a->output_open=0;}
 for(int i=0;i<VOICE_INPUT_SLOTS;i++)if(a->input[i]){wipe(a->input[i],VOICE_INPUT_BYTES);free(a->input[i]);a->input[i]=NULL;}
 for(int i=0;i<VOICE_OUTPUT_SLOTS;i++)if(a->output[i]){wipe(a->output[i],4096);free(a->output[i]);a->output[i]=NULL;}
 wipe(a->mono,sizeof(a->mono));memset(a->out_owned,0,sizeof(a->out_owned));a->mono_used=0;a->started=a->capturing=0;
}
static int voice_audio_open(VoiceAudio *a){
 if(!voice_audio_result(a,audinInitialize(),1))return 0;
 a->input_open=1;
 if(!voice_audio_result(a,audoutInitialize(),2))return 0;
 a->output_open=1;
 if(audinGetSampleRate()!=48000||audinGetChannelCount()!=2||audinGetPcmFormat()!=PcmFormat_Int16||audoutGetSampleRate()!=48000||audoutGetChannelCount()!=2||audoutGetPcmFormat()!=PcmFormat_Int16){a->error_stage=3;return 0;}
 for(int i=0;i<VOICE_INPUT_SLOTS;i++){
  a->input[i]=memalign(4096,VOICE_INPUT_BYTES);if(!a->input[i])return 0;
  memset(a->input[i],0,VOICE_INPUT_BYTES);memset(&a->in[i],0,sizeof(a->in[i]));a->in[i].buffer=a->input[i];a->in[i].buffer_size=a->in[i].data_size=VOICE_INPUT_BYTES;
  if(!voice_audio_result(a,audinAppendAudioInBuffer(&a->in[i]),4))return 0;
 }
 for(int i=0;i<VOICE_OUTPUT_SLOTS;i++){
  a->output[i]=memalign(4096,4096);if(!a->output[i])return 0;
  memset(a->output[i],0,4096);memset(&a->out[i],0,sizeof(a->out[i]));a->out[i].buffer=a->output[i];a->out[i].buffer_size=4096;a->out[i].data_size=3840;
 }
 if(!voice_audio_result(a,audinStartAudioIn(),5))return 0;
 a->capturing=1;
 if(!voice_audio_result(a,audoutStartAudioOut(),6))return 0;
 for(int i=0;i<2;i++){if(!voice_audio_result(a,audoutAppendAudioOutBuffer(&a->out[i]),7))return 0;a->out_owned[i]=1;}
 a->out_next=2;a->started=1;a->output_at=dnx2MonotonicMs()+20;return 1;
}
static int voice_audio_mute(VoiceAudio *a,int muted){
 if(a->capturing==!muted)return 1;
 if(!voice_audio_result(a,muted?audinStopAudioIn():audinStartAudioIn(),8))return 0;
 a->capturing=!muted;a->mute_changes++;wipe(a->mono,sizeof(a->mono));a->mono_used=0;
 if(muted){for(int i=0;i<VOICE_INPUT_SLOTS;i++)wipe(a->input[i],VOICE_INPUT_BYTES);a->input_peak=0;}
 return 1;
}
static int voice_audio_tick(VoiceAudio *a,DnxVoiceEngine *e,int udp,int muted){
 if(!voice_audio_mute(a,muted))return 0;
 /* Poll the released queue first, using its exact pointer and returned byte count.
  * A completed descriptor can still be reported as contained until this dequeue. */
 for(unsigned work=0;work<VOICE_INPUT_SLOTS;work++){
  AudioInBuffer *released=NULL;u32 count=0;
  if(!voice_audio_result(a,audinGetReleasedAudioInBuffer(&released,&count),9))return 0;
  if(!count)break;
  int slot=-1;for(int i=0;i<VOICE_INPUT_SLOTS;i++)if(released==&a->in[i]){slot=i;break;}
  if(slot<0||count!=1||released->data_size>VOICE_INPUT_BYTES||(released->data_size&3)){a->error_stage=10;return 0;}
  size_t frames=(size_t)released->data_size/4;a->captures++;a->samples+=(unsigned)frames;
  const int16_t *pcm=a->input[slot];
  if(!muted)for(size_t j=0;j<frames;j++){
   int sample=((int)pcm[j*2]+pcm[j*2+1])/2;unsigned peak=(unsigned)(sample<0?-sample:sample);if(peak>a->input_peak)a->input_peak=peak;
   a->mono[a->mono_used++]=(int16_t)sample;
   if(a->mono_used==960){unsigned char packet[2048];int n=0;
    /* Recheck the atomic UI request at every frame, including a capture backlog. */
    if(!dnx2VoiceMuted())n=dnx_voice_encode(e,a->mono,packet);
    if(n>0){a->encoded++;int written=send(udp,packet,(size_t)n,0);if(written==n)a->sent++;else if(written<0&&(errno==EAGAIN||errno==EWOULDBLOCK))a->send_blocked++;else{wipe(packet,sizeof(packet));a->error_stage=11;return 0;}}
    wipe(packet,sizeof(packet));wipe(a->mono,sizeof(a->mono));a->mono_used=0;
   }
  }
  wipe(a->input[slot],VOICE_INPUT_BYTES);memset(&a->in[slot],0,sizeof(a->in[slot]));a->in[slot].buffer=a->input[slot];a->in[slot].buffer_size=a->in[slot].data_size=VOICE_INPUT_BYTES;
  if(!voice_audio_result(a,audinAppendAudioInBuffer(&a->in[slot]),12))return 0;
 }
 for(unsigned work=0;work<VOICE_OUTPUT_SLOTS;work++){
  AudioOutBuffer *released=NULL;u32 count=0;
  if(!voice_audio_result(a,audoutGetReleasedAudioOutBuffer(&released,&count),13))return 0;
  if(!count)break;
  int slot=-1;for(int i=0;i<VOICE_OUTPUT_SLOTS;i++)if(released==&a->out[i]){slot=i;break;}
  if(slot<0||count!=1||!a->out_owned[slot]){a->error_stage=14;return 0;}a->out_owned[slot]=0;
 }
 u64 now=dnx2MonotonicMs();
 if(now>=a->output_at){
  unsigned i=a->out_next%VOICE_OUTPUT_SLOTS;
  if(!a->out_owned[i]){
   dnx_voice_mix(e,a->output[i]);const int16_t *pcm=a->output[i];for(int j=0;j<1920;j++){int sample=pcm[j];unsigned peak=(unsigned)(sample<0?-sample:sample);if(peak>a->output_peak)a->output_peak=peak;}
   if(!voice_audio_result(a,audoutAppendAudioOutBuffer(&a->out[i]),15))return 0;
   a->out_owned[i]=1;a->out_next++;a->playbacks++;
   /* Compare before advancing; unsigned now-future underflow used to queue early. */
   if(now-a->output_at>60)a->output_at=now;
   a->output_at+=20;
  }
 }
 return 1;
}
