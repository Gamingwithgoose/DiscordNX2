// SPDX-License-Identifier: MIT
#include "voice_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){DnxVoiceStats s={0};uint64_t since=0;assert(!dnx_voice_local_audio(1,&s));s.transport_ready=1;
 assert(dnx_voice_local_audio(1,&s)&&!dnx_voice_local_audio(0,&s));assert(dnx_voice_display_state(0,&s)==4&&dnx_voice_display_state(1,&s)==5);
 for(uint64_t t=1;t<=3600000;t+=1000)assert(!dnx_voice_key_timeout(&s,t,&since)&&!since);/* empty channel stays joined for an hour */
 s.peer_count=1;assert(dnx_voice_display_state(0,&s)==3);assert(!dnx_voice_key_timeout(&s,100,&since));assert(dnx_voice_key_timeout(&s,60100,&since));
 s.secure_ready=1;assert(dnx_voice_display_state(0,&s)==4&&!dnx_voice_key_timeout(&s,70000,&since)&&!since);
 s.secure_ready=0;s.peer_count=0;assert(dnx_voice_local_audio(1,&s)&&dnx_voice_display_state(0,&s)==4&&!dnx_voice_key_timeout(&s,80000,&since));
 puts("PASS empty-channel policy: local capture/join without peers or MLS, no solo timeout, real peer key deadline, mute, peer leave");}
