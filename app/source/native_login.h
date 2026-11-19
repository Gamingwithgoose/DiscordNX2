// SPDX-License-Identifier: MIT
#ifndef DNX2_NATIVE_LOGIN_H
#define DNX2_NATIVE_LOGIN_H
#define DNX2_MAX_SERVERS 200
#define DNX2_MAX_CHANNELS 512
#define DNX2_MAX_MESSAGES 30
typedef struct {char id[24];char name[401];char icon[80];} Dnx2Server;
typedef struct {char id[24],parent[24],name[401],last_message[24],icon[80],recipient[24],avatar[80];unsigned default_avatar;int type,position;} Dnx2Channel;
typedef struct {char id[24],author[321],content[8193],sticker_id[24],sticker_name[129];int sticker_format;char author_id[24],avatar[80],guild_avatar[80],timestamp[40],media_id[24],media_url[1536];unsigned default_avatar;
    char username[129],discriminator[8],tag[25],tag_guild[24],tag_badge[80];int bot;
    char reply_id[24],reply_author_id[24],reply_author[321],reply_content[513];int reply_state;
    char roles[32][24];int role_count;
    struct {char id[24],name[321];} mentions[12];int mention_count;
} Dnx2Message;
typedef struct {
    int private_mode,channel_count,selected_channel,message_count,channels_loaded,order_synced;
    char notice[160];
    Dnx2Channel channels[DNX2_MAX_CHANNELS];
    Dnx2Message messages[DNX2_MAX_MESSAGES];
    int signed_in,server_count,selected;
    char username[129],display_name[321],user_id[24];
    Dnx2Server servers[DNX2_MAX_SERVERS];
} Dnx2Account;
typedef void (*Dnx2AuthStatus)(const char *stage,const char *qr_url);
typedef int (*Dnx2AuthCancel)(void);
const Dnx2Account *dnx2Account(void);
void dnx2AccountSelect(int index);
int dnx2OpenDMList(void);
unsigned dnx2VoiceWaitSeconds(void);
void dnx2AccountLogout(void);
void dnx2AccountClearMemory(void);
void dnx2LiveStart(void);
void dnx2LiveStop(void);
void dnx2LiveReset(void);
int dnx2LiveApply(void);
const char *dnx2LiveStatus(void);
int dnx2LoadChannels(Dnx2AuthStatus status,Dnx2AuthCancel cancel);
void dnx2ChannelSelect(int index);
const char *dnx2LastChannel(void);
int dnx2LoadMessages(Dnx2AuthStatus status,Dnx2AuthCancel cancel);
int dnx2SendMessage(const char *text,Dnx2AuthStatus status,Dnx2AuthCancel cancel);
int dnx2RememberSession(const char *pin);
int dnx2RestoreSession(const char *pin,Dnx2AuthStatus status,Dnx2AuthCancel cancel);
/* Returns 1 when account and actual server data fetched; 0 otherwise.
 * No account token, RSA key, ticket, nonce, or QR fingerprint ever logged. */
int dnx2NativeQrLogin(Dnx2AuthStatus status,Dnx2AuthCancel cancel);
typedef struct {char id[24],name[321];int muted,deaf,speaking,self,named;} Dnx2VoiceMember;
int dnx2VoiceMembers(Dnx2VoiceMember *out,int cap);
unsigned dnx2VoiceMembersRevision(void);
int dnx2VoiceStart(void);
void dnx2VoiceStop(void);
void dnx2VoiceReset(void);
int dnx2VoiceRunning(void);
int dnx2VoiceApply(void);
int dnx2VoiceSelected(void);
const char *dnx2VoiceNotice(void);
const char *dnx2VoiceChannelName(void);
/* Voice states: 0 idle,1 signaling,2 discovery,3 MLS,4 live,5 muted. */
/* Joined/local audio readiness is distinct from a shared MLS media key. */
unsigned dnx2VoiceMicLevel(void);
unsigned dnx2VoiceState(void);
int dnx2VoiceMuted(void);
void dnx2VoiceToggleMute(void);

#endif
