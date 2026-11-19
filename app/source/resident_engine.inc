// SPDX-License-Identifier: MIT
/* Headless exports only. The linker drops account/chat/auth UI code in an NSO. */
int dnx2ResidentEngineStart(const DnxResidentConfig *c){
 if(!dnx_resident_config_valid(c))return 0;
 dnx2VoiceStop();u64 now=dnx2MonotonicMs();if(c->identify_after>voice_gateway_identify_after)voice_gateway_identify_after=c->identify_after>now+60000?now+60000:c->identify_after;memset(&account,0,sizeof(account));account.signed_in=1;account.server_count=1;account.channel_count=1;account.selected_channel=0;
 snprintf(account.user_id,24,"%s",c->user);snprintf(account.display_name,321,"%s",c->name);snprintf(account.avatar,80,"%s",c->avatar);account.default_avatar=c->default_avatar;
 snprintf(account.servers[0].id,24,"%s",c->guild);snprintf(account.channels[0].id,24,"%s",c->channel);snprintf(account.channels[0].name,401,"%s",c->label);account.channels[0].type=2;
 snprintf(session_token,sizeof(session_token),"%s",c->token);__atomic_store_n(&voice_muted,c->muted,__ATOMIC_RELEASE);int ok=dnx2VoiceStart();wipe(session_token,sizeof(session_token));wipe(&account,sizeof(account));account.selected_channel=-1;return ok;
}
void dnx2ResidentEngineStatus(DnxResidentStatus *out){memset(out,0,sizeof(*out));out->magic=DNX_RESIDENT_MAGIC;out->version=DNX_RESIDENT_VERSION;out->available=1;out->running=dnx2VoiceRunning();out->state=dnx2VoiceState();out->wait_seconds=dnx2VoiceWaitSeconds();out->muted=dnx2VoiceMuted();out->mic_level=dnx2VoiceMicLevel();out->members_revision=dnx2VoiceMembersRevision();mutexLock(&voice_members_mutex);out->member_count=(uint32_t)voice_members.count;mutexUnlock(&voice_members_mutex);}

void dnx2ResidentEngineSnapshot(DnxResidentSnapshot *out){memset(out,0,sizeof(*out));dnx2ResidentEngineStatus(&out->status);snprintf(out->label,sizeof(out->label),"%s",voice_call.label);snprintf(out->guild,24,"%s",voice_call.guild);snprintf(out->channel,24,"%s",voice_call.channel);out->status.member_count=(uint32_t)dnx2VoiceMembers(out->members,DNX_RESIDENT_MEMBERS);}
