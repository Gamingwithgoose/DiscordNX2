// SPDX-License-Identifier: MIT
/* Only authenticated Gateway/Voice Gateway events identify members. RTP never does. */
#define VOICE_MEMBERS_MAX 33
static Mutex voice_members_mutex;static unsigned voice_members_revision;
static struct {Dnx2VoiceMember list[VOICE_MEMBERS_MAX];unsigned char requested[VOICE_MEMBERS_MAX];int count;char self[24];} voice_members;
unsigned dnx2VoiceMembersRevision(void){return __atomic_load_n(&voice_members_revision,__ATOMIC_ACQUIRE);}
int dnx2VoiceMembers(Dnx2VoiceMember *out,int cap){mutexLock(&voice_members_mutex);int n=voice_members.count;if(n>cap)n=cap;if(n>0)memcpy(out,voice_members.list,(size_t)n*sizeof(*out));mutexUnlock(&voice_members_mutex);return n;}
static void voice_members_changed(void){__atomic_add_fetch(&voice_members_revision,1,__ATOMIC_RELEASE);}
static int voice_member_index(const char *id,int add){
 if(!dnx_id(id))return -1;
 for(int i=0;i<voice_members.count;i++)if(!strcmp(id,voice_members.list[i].id))return i;
 if(!add||voice_members.count==VOICE_MEMBERS_MAX)return -1;
 int i=voice_members.count++;memset(&voice_members.list[i],0,sizeof(voice_members.list[i]));snprintf(voice_members.list[i].id,24,"%s",id);snprintf(voice_members.list[i].name,321,"Member %.20s",id);voice_members.requested[i]=0;return i;
}
static void voice_member_remove(const char *id){int i=voice_member_index(id,0);if(i<0)return;voice_members.count--;memmove(voice_members.list+i,voice_members.list+i+1,(size_t)(voice_members.count-i)*sizeof(*voice_members.list));memmove(voice_members.requested+i,voice_members.requested+i+1,(size_t)(voice_members.count-i));}
static int voice_bool(DnxJson d,const char *key){DnxJson v=dj_field(d,key);return v.p&&*v.p=='t';}
static void voice_member_name(int i,DnxJson member){
 if(i<0)return;
 DnxJson user=dj_field(member,"user");char name[321]={0};
 if(!dj_get(member,"nick",name,sizeof(name))||!name[0])if(!dj_get(user,"global_name",name,sizeof(name))||!name[0])dj_get(user,"username",name,sizeof(name));
 char avatar[80]={0},disc[8]={0};if(dj_get(user,"avatar",avatar,sizeof(avatar)))snprintf(voice_members.list[i].avatar,80,"%s",avatar);dj_get(user,"discriminator",disc,sizeof(disc));voice_members.list[i].default_avatar=dnx_default_avatar(voice_members.list[i].id,disc);
 if(dj_get(member,"avatar",avatar,sizeof(avatar)))snprintf(voice_members.list[i].guild_avatar,80,"%s",avatar);
 if(name[0]){snprintf(voice_members.list[i].name,321,"%s",name);voice_members.list[i].named=1;}
}
static void voice_members_reset(const char *self,const char *name){mutexLock(&voice_members_mutex);memset(&voice_members,0,sizeof(voice_members));snprintf(voice_members.self,24,"%s",self);int i=voice_member_index(self,1);if(i>=0){snprintf(voice_members.list[i].name,321,"%s",name);voice_members.list[i].named=1;voice_members.list[i].self=1;}voice_members_changed();mutexUnlock(&voice_members_mutex);}
static void voice_members_self_profile(const char *avatar,unsigned fallback,const char *guild){mutexLock(&voice_members_mutex);if(voice_members.count){snprintf(voice_members.list[0].avatar,80,"%s",avatar);voice_members.list[0].default_avatar=fallback;snprintf(voice_members.list[0].guild,24,"%s",guild);}mutexUnlock(&voice_members_mutex);}
static void voice_member_state(const DnxVoiceCredentials *c,DnxJson d){char id[24],channel[24];if(!dj_get(d,"user_id",id,24))return;if(!dj_get(d,"channel_id",channel,24)||strcmp(channel,c->channel)){voice_member_remove(id);return;}int i=voice_member_index(id,1);if(i<0)return;voice_member_name(i,dj_field(d,"member"));voice_members.list[i].muted=voice_bool(d,"mute")||voice_bool(d,"self_mute");voice_members.list[i].deaf=voice_bool(d,"deaf")||voice_bool(d,"self_deaf");voice_members.list[i].self=!strcmp(id,c->user);snprintf(voice_members.list[i].guild,24,"%s",c->guild);}
static void voice_members_guild(const DnxVoiceCredentials *c,DnxJson guild){
 char id[24];if(!dj_get(guild,"id",id,24)||strcmp(id,c->guild))return;
 const char *p=NULL;DnxJson state;while(dj_next(dj_field(guild,"voice_states"),&p,&state))voice_member_state(c,state);
 p=NULL;DnxJson member;while(dj_next(dj_field(guild,"members"),&p,&member)){char user[24];if(dj_get(dj_field(member,"user"),"id",user,24))voice_member_name(voice_member_index(user,0),member);}
 voice_members_changed();
}
static void voice_members_gateway(const DnxVoiceCredentials *c,const char *kind,DnxJson d){
 mutexLock(&voice_members_mutex);char guild[24]={0};
 if(!strcmp(kind,"VOICE_STATE_UPDATE")){if(!dj_get(d,"guild_id",guild,24)||!strcmp(guild,c->guild)){voice_member_state(c,d);voice_members_changed();}}
 else if(!strcmp(kind,"GUILD_MEMBERS_CHUNK")&&dj_get(d,"guild_id",guild,24)&&!strcmp(guild,c->guild)){const char *p=NULL;DnxJson member;while(dj_next(dj_field(d,"members"),&p,&member)){char id[24];if(dj_get(dj_field(member,"user"),"id",id,24)){int i=voice_member_index(id,0);voice_member_name(i,member);if(i>=0)snprintf(voice_members.list[i].guild,24,"%s",c->guild);}}voice_members_changed();}
 else if(!strcmp(kind,"GUILD_CREATE"))voice_members_guild(c,d);
 else if(!strcmp(kind,"READY")){const char *p=NULL;DnxJson guild;while(dj_next(dj_field(d,"guilds"),&p,&guild))voice_members_guild(c,guild);}
 mutexUnlock(&voice_members_mutex);
}
static void voice_members_signal(const char *json){DnxJson root=dj_root(json),d=dj_field(root,"d");int op=dj_int(root,"op",-1);if(op!=5&&op!=11&&op!=13)return;mutexLock(&voice_members_mutex);
 if(op==11){const char *p=NULL;DnxJson id;while(dj_next(dj_field(d,"user_ids"),&p,&id)){char value[24];if(dj_string(id,value,24))voice_member_index(value,1);}}
 else {char id[24];if(dj_get(d,"user_id",id,24)){if(op==13)voice_member_remove(id);else {int i=voice_member_index(id,1);if(i>=0)voice_members.list[i].speaking=dj_int(d,"speaking",0)!=0;}}}voice_members_changed();mutexUnlock(&voice_members_mutex);
}
static int voice_members_request(const DnxVoiceCredentials *c,char *out,size_t cap){
 mutexLock(&voice_members_mutex);char ids[900]={0};size_t at=0;int selected[VOICE_MEMBERS_MAX],count=0;
 for(int i=0;i<voice_members.count;i++)if(!voice_members.requested[i]){int n=snprintf(ids+at,sizeof(ids)-at,"%s\"%s\"",count?",":"",voice_members.list[i].id);if(n<0||(size_t)n>=sizeof(ids)-at)break;at+=(size_t)n;selected[count++]=i;}
 int n=count?snprintf(out,cap,"{\"op\":8,\"d\":{\"guild_id\":\"%s\",\"user_ids\":[%s],\"presences\":false,\"nonce\":\"dnx-voice\"}}",c->guild,ids):0;
 if(n>0&&(size_t)n<cap)for(int i=0;i<count;i++)voice_members.requested[selected[i]]=1;else n=0;
 mutexUnlock(&voice_members_mutex);return n;
}
