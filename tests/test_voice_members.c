// SPDX-License-Identifier: MIT
#include "native_login.h"
#include "voice_control.h"
#include <assert.h>
#include <stdio.h>
typedef int Mutex;static void mutexLock(Mutex *m){(void)m;}static void mutexUnlock(Mutex *m){(void)m;}
#include "voice_members.inc"
int main(void){DnxVoiceCredentials c={0};strcpy(c.guild,"100000000000000001");strcpy(c.channel,"200000000000000001");strcpy(c.user,"300000000000000001");voice_members_reset(c.user,"Self");
voice_members_signal("{\"op\":11,\"d\":{\"user_ids\":[\"300000000000000001\",\"300000000000000002\"]}}");char body[2048];assert(voice_members_request(&c,body,sizeof(body))>0&&strstr(body,"300000000000000002"));assert(!voice_members_request(&c,body,sizeof(body)));
voice_members_gateway(&c,"GUILD_MEMBERS_CHUNK",dj_root("{\"guild_id\":\"100000000000000002\",\"members\":[{\"user\":{\"id\":\"300000000000000002\",\"username\":\"Wrong\"}}]}"));
voice_members_gateway(&c,"GUILD_MEMBERS_CHUNK",dj_root("{\"guild_id\":\"100000000000000001\",\"members\":[{\"nick\":\"Voice nickname\",\"user\":{\"id\":\"300000000000000002\",\"username\":\"Peer\"}}]}"));
voice_members_signal("{\"op\":5,\"d\":{\"user_id\":\"300000000000000002\",\"speaking\":1}}");Dnx2VoiceMember list[33];assert(dnx2VoiceMembers(list,33)==2&&!strcmp(list[1].name,"Voice nickname")&&list[1].speaking);
voice_members_gateway(&c,"VOICE_STATE_UPDATE",dj_root("{\"guild_id\":\"100000000000000001\",\"user_id\":\"300000000000000002\",\"channel_id\":\"200000000000000001\",\"self_mute\":true}"));assert(dnx2VoiceMembers(list,33)==2&&list[1].muted);
voice_members_signal("{\"op\":13,\"d\":{\"user_id\":\"300000000000000002\"}}");assert(dnx2VoiceMembers(list,33)==1);voice_members_gateway(&c,"READY",dj_root("{\"guilds\":[{\"id\":\"100000000000000001\",\"voice_states\":[{\"user_id\":\"300000000000000003\",\"channel_id\":\"200000000000000001\"}],\"members\":[{\"user\":{\"id\":\"300000000000000003\",\"global_name\":\"Already joined\"}}]}]}"));assert(dnx2VoiceMembers(list,33)==2&&!strcmp(list[1].name,"Already joined"));puts("PASS voice members: channel scope, authenticated join/leave/speaking/mute, name resolution, single metadata request");}
