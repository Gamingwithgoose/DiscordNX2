// SPDX-License-Identifier: MIT
#ifndef DNX2_VOICE_POLICY_H
#define DNX2_VOICE_POLICY_H
#include "voice_engine.h"
/* Channel/transport membership and local capture do not require a recipient.
 * dnx_voice_encode separately requires a valid executed MLS sending key. */
static inline int dnx_voice_local_audio(int selected,const DnxVoiceStats *s){return selected&&s->transport_ready;}
static inline unsigned dnx_voice_display_state(int muted,const DnxVoiceStats *s){return s->peer_count&&!s->secure_ready?3:muted?5:4;}
static inline int dnx_voice_key_timeout(const DnxVoiceStats *s,uint64_t now,uint64_t *since){
 if(!s->peer_count||s->secure_ready){*since=0;return 0;}
 if(!*since)*since=now;
 return now>=*since&&now-*since>=60000;
}
#endif
