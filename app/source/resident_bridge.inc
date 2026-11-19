// SPDX-License-Identifier: MIT
/* Main-thread adapter. IPC polling runs elsewhere and never blocks audio. */
static int voice_resident_mode,voice_resident_available;static DnxResidentSnapshot voice_remote;
int dnx2VoiceBackgroundAvailable(void){dnx2ResidentClientStart();return voice_resident_available;}
int dnx2VoiceBackground(void){return voice_resident_mode;}
unsigned dnx2VoiceBackgroundWaitSeconds(void){return voice_resident_mode?voice_remote.status.wait_seconds:0;}
int dnx2VoiceBackgroundStart(void){
 if(!account.signed_in||!dnx2VoiceBackgroundAvailable())return 0;
 DnxResidentConfig *config=calloc(1,sizeof(*config));if(!config)return 0;
 config->magic=DNX_RESIDENT_MAGIC;config->version=DNX_RESIDENT_VERSION;config->muted=dnx2VoiceMuted();config->identify_after=voice_gateway_identify_after;
 const char *guild=voice_call.running?voice_call.guild:account.private_mode?"":account.servers[account.selected].id;
 const char *channel=voice_call.running?voice_call.channel:account.selected_channel>=0?account.channels[account.selected_channel].id:"";
 const char *label=voice_call.running?voice_call.label:account.selected_channel>=0?account.channels[account.selected_channel].name:"";
 snprintf(config->guild,24,"%s",guild);snprintf(config->channel,24,"%s",channel);snprintf(config->label,401,"%s",label);snprintf(config->user,24,"%s",account.user_id);snprintf(config->name,321,"%s",account.display_name);snprintf(config->avatar,80,"%s",account.avatar);config->default_avatar=account.default_avatar;snprintf(config->token,1024,"%s",session_token);
 if(!dnx_resident_config_valid(config)){wipe(config,sizeof(*config));free(config);return 0;}
 /* Finish local Leave before the resident process sends its new Join. */
 dnx2VoiceStop();int ok=dnx2ResidentClientLaunch(config);
 if(ok){voice_resident_mode=1;memset(&voice_remote,0,sizeof(voice_remote));voice_remote.status.running=1;voice_remote.status.state=1;voice_remote.status.muted=config->muted;snprintf(voice_remote.label,401,"%s",config->label);snprintf(voice_call.channel,24,"%s",config->channel);voice_set(1);}
 else snprintf(voice_notice,sizeof(voice_notice),"Resident voice could not start. Use Join voice to retry locally.");
 wipe(config,sizeof(*config));free(config);return ok;
}
static int voice_resident_apply(void){
 DnxResidentSnapshot *copy=malloc(sizeof(*copy));if(!copy)return 0;int changed=dnx2ResidentClientRead(copy);voice_resident_available=copy->status.available;
 if(!voice_resident_mode&&copy->status.running&&!voice_call.running)voice_resident_mode=1;
 if(voice_resident_mode&&changed){voice_remote=*copy;snprintf(voice_call.guild,24,"%s",copy->guild);snprintf(voice_call.channel,24,"%s",copy->channel);voice_set(copy->status.state);__atomic_store_n(&voice_muted,copy->status.muted,__ATOMIC_RELEASE);__atomic_store_n(&voice_mic_level,copy->status.mic_level,__ATOMIC_RELEASE);
  mutexLock(&voice_members_mutex);voice_members.count=(int)copy->status.member_count;memcpy(voice_members.list,copy->members,(size_t)voice_members.count*sizeof(*voice_members.list));voice_members_changed();mutexUnlock(&voice_members_mutex);
  if(!copy->status.running){voice_resident_mode=0;snprintf(voice_notice,sizeof(voice_notice),"Resident voice ended or could not be reached.");}
 }
 free(copy);return changed;
}
void dnx2VoiceAppExit(void){
 if(voice_resident_mode){/* Detach IPC only; the NSO owns its credentials and devices. */voice_resident_mode=0;memset(&voice_remote,0,sizeof(voice_remote));dnx2ResidentClientDetach();voice_set(0);}else {dnx2VoiceStop();dnx2ResidentClientDetach();}
}
