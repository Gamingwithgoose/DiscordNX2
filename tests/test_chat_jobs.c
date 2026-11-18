// SPDX-License-Identifier: MIT
#include "chat_media_data.h"
#include <assert.h>
#include <stdint.h>
typedef int Thread;
typedef int Mutex;
typedef struct {int unused;} TlsSession;
#define R_SUCCEEDED(x) ((x)==0)
#define R_FAILED(x) ((x)!=0)
#define API_HOST "discord.com"
static Dnx2Account account;
static char session_token[1024]="synthetic-token",live_token[1024]="synthetic-token";
static unsigned char http_body[1048576];
static unsigned last_http_status;
static size_t last_http_body_size;
static int (*active_cancel)(void);
static int services,post_calls,http_fail,empty_history,cdn_calls;
static unsigned char *png;static size_t png_size;
static void wipe(void *p,size_t n){memset(p,0,n);}
static void mutexLock(Mutex *m){assert(!*m);*m=1;}
static void mutexUnlock(Mutex *m){assert(*m);*m=0;}
static int socketInitialize(void *p){(void)p;services++;return 0;}
static int sslInitialize(int n){(void)n;services++;return 0;}
static void socketExit(void){services--;}
static void sslExit(void){services--;}
static int threadCreate(Thread *t,void (*f)(void*),void *a,void *stack,size_t n,int p,int c){(void)t;(void)f;(void)a;(void)stack;(void)n;(void)p;(void)c;return 0;}
static int threadStart(Thread *t){(void)t;return 0;}
static void threadWaitForExit(Thread *t){(void)t;}
static void threadClose(Thread *t){(void)t;}
static int live_stops;
void dnx2LiveStop(void){live_stops++;}
static Mutex live_mutex;static int live_accept_images;
typedef uint64_t u64;
static u64 dnx2MonotonicMs(void){return 1000;}
static int live_cancel(void){return 0;}
static void svcSleepThread(long long n){(void)n;}

static void tls_close(TlsSession *s){(void)s;}
static int http_request(TlsSession *s,const char *host,const char *method,const char *path,const char *body,const char *token,char *out,size_t cap){
    (void)s;last_http_status=http_fail?403:200;
    if(!strcmp(host,"cdn.discordapp.com")){assert(!token&&!strcmp(method,"GET"));assert(!strncmp(path,"/icons/",7)||!strncmp(path,"/emojis/",8)||!strncmp(path,"/stickers/",10)||!strncmp(path,"/guild-tag-badges/",18)||!strncmp(path,"/avatars/",9)||!strncmp(path,"/embed/avatars/",15)||!strncmp(path,"/channel-icons/",15));assert(png_size<cap);memcpy(out,png,png_size);last_http_body_size=png_size;cdn_calls++;return !http_fail;}
    assert(!strcmp(host,API_HOST)&&!strcmp(token,"synthetic-token"));
    if(!strcmp(method,"POST")){
        post_calls++;char text[200];assert(dj_get(dj_root(body),"content",text,sizeof(text)));assert(!strcmp(text,"hello"));
        DnxJson ids=dj_field(dj_root(body),"sticker_ids");if(ids.p)assert(strstr(body,"300000000000000001"));strcpy(out,"{}");return !http_fail;
    }
    if(strstr(path,"/roles")){snprintf(out,cap,"[{\"id\":\"600000000000000001\",\"name\":\"Regular\",\"position\":1,\"color\":65280},{\"id\":\"600000000000000002\",\"name\":\"Moderator\",\"position\":2,\"colors\":{\"primary_color\":16711680}}]");last_http_body_size=strlen(out);return !http_fail;}
    if(strstr(path,"/members/")){snprintf(out,cap,"{\"nick\":\"Server nickname\",\"roles\":[\"600000000000000001\",\"600000000000000002\"],\"joined_at\":\"2020-01-01T00:00:00Z\"}");last_http_body_size=strlen(out);return !http_fail;}
    if(!strcmp(path,"/api/v10/users/@me/channels")){snprintf(out,cap,"[{\"id\":\"200000000000000001\",\"type\":1,\"recipients\":[{\"id\":\"300000000000000001\",\"global_name\":\"DM friend\"}]}]");last_http_body_size=strlen(out);return !http_fail;}
    const char *response=strstr(path,"/emojis")?"[{\"id\":\"400000000000000001\",\"name\":\"goose\",\"animated\":true}]":strstr(path,"/stickers")?"[{\"id\":\"300000000000000001\",\"name\":\"Wave\",\"format_type\":1}]":strstr(path,"/guilds/")?"[{\"id\":\"200000000000000001\",\"name\":\"general\",\"type\":0,\"position\":1}]":empty_history?"[]":"[{\"id\":\"900000000000000001\",\"author\":{\"username\":\"fixture\"},\"content\":\"fresh\"}]";
    snprintf(out,cap,"%s",response);last_http_body_size=strlen(out);return !http_fail;
}
static void voice_control_preflight(const char *t,const char *g,const char *c,const char *u,char notice[160],int *stage,unsigned *code){(void)t;(void)g;(void)c;(void)u;snprintf(notice,160,"Engine unavailable");*stage=0;*code=0;}
#include "chat_jobs.inc"
static void delegated_check(void){live_accept_images=1;int before=live_stops;assert(dnx2RequestImages(1,0,1));assert(chat_job.delegated&&delegated_images&&live_stops==before);live_image_step();assert(chat_job.done&&!delegated_images);assert(dnx2JobApply()==20);dnx2ChatReset();assert(dnx2RequestImages(1,0,1));__atomic_store_n(&chat_job.stop,1,__ATOMIC_RELEASE);live_image_step();assert(chat_job.done&&!delegated_images);dnx2JobStop();live_accept_images=0;}
static int run(void){job_worker(NULL);assert(!services);return dnx2JobApply();}
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);png_size=(size_t)ftell(f);rewind(f);png=malloc(png_size);assert(png&&fread(png,1,png_size,f)==png_size);fclose(f);
    account.signed_in=1;account.server_count=1;account.selected_channel=-1;
    strcpy(account.servers[0].id,"100000000000000001");strcpy(account.servers[0].icon,"0123456789abcdef0123456789abcdef");
    assert(dnx2JobStart(9,NULL,NULL));assert(!account.channel_count);assert(run()==9&&account.channel_count==1);
    account.selected_channel=0;assert(dnx2JobStart(10,NULL,NULL));assert(!account.message_count);assert(run()==10&&account.message_count==1);
    assert(dnx2JobStart(10,NULL,NULL));strcpy(account.channels[0].id,"200000000000000002");assert(run()==0);assert(!strcmp(account.messages[0].content,"fresh"));strcpy(account.channels[0].id,"200000000000000001");
    assert(dnx2JobStart(11,"hello","300000000000000001"));assert(!dnx2JobStart(11,"hello",NULL));assert(run()==11&&post_calls==1);assert(!strcmp(dnx2JobDraft(),"hello"));
    http_fail=1;assert(dnx2JobStart(11,"hello",NULL));assert(run()==-11&&post_calls==2);assert(!strcmp(dnx2JobDraft(),"hello"));http_fail=0;
    empty_history=1;assert(dnx2JobStart(10,NULL,NULL)&&run()==10);assert(!account.message_count);empty_history=0;
    assert(dnx2JobStart(15,NULL,NULL)&&run()==15);assert(dnx2MediaCatalog()->emoji_count==1&&dnx2MediaCatalog()->sticker_count==1);
    assert(dnx2RequestImages(1,0,1)&&run()==20);const unsigned char *image=dnx2ChatImage(1,0);assert(image&&image[0]==255&&image[1]==0&&image[2]==0&&image[3]==255);
    assert(!dnx2RequestImages(1,0,1));assert(dnx2RequestImages(2,0,1)&&run()==20);assert(dnx2MessageImage(2,"400000000000000001"));
    assert(cdn_calls==2);unsigned char output[64*64*4];assert(!dnx2DecodePng((unsigned char*)"not a png",9,output));assert(!dnx2DecodePng(png,20,output));
    unsigned char *huge=malloc(png_size);assert(huge);memcpy(huge,png,png_size);huge[16]=0;huge[17]=0;huge[18]=4;huge[19]=0;assert(!dnx2DecodePng(huge,png_size,output));free(huge);
    char id[24],name[33];assert(dnx_custom_emoji("<a:goose:400000000000000001>",id,name)==28);assert(!strcmp(id,"400000000000000001")&&!strcmp(name,"goose"));
    assert(!dnx_custom_emoji("<:../bad:400000000000000001>",id,name));assert(!dnx_custom_emoji("<:goose:1>",id,name));assert(!dnx_custom_emoji("<",id,name));
    assert(dnx_asset_hash("a_0123456789abcdef0123456789abcdef"));assert(!dnx_asset_hash("../../not-a-hash"));
    dnx2ChatReset();assert(!dnx2ChatImage(1,0)&&!dnx2MediaCatalog()->loaded);
    dnx2ChatReset();delegated_check();dnx2ChatReset();
    account.message_count=1;account.messages[0].author_id[0]=0;snprintf(account.messages[0].tag_guild,24,"100000000000000001");snprintf(account.messages[0].tag_badge,80,"0123456789abcdef0123456789abcdef");assert(dnx2RequestMessageImages(0,1)&&run()==20);assert(dnx2TagBadge(account.messages[0].tag_guild,account.messages[0].tag_badge));
    account.message_count=1;snprintf(account.messages[0].author_id,24,"300000000000000001");assert(dnx2RequestProfiles(0,1)&&run()==20);const Dnx2Member *member=dnx2Member("300000000000000001");assert(member&&!strcmp(member->nick,"Server nickname")&&member->count==2);assert(dnx2GuildRoles()->count==2&&dnx_role_color(dnx2GuildRoles(),member->roles,member->count)==16711680);assert(!dnx2RequestProfiles(0,1));strcpy(account.servers[0].id,"100000000000000002");assert(!dnx2Member("300000000000000001"));strcpy(account.servers[0].id,"100000000000000001");
    assert(dnx2OpenDMList()&&account.private_mode&&run()==23);assert(account.channel_count==1&&account.channels[0].type==1&&!strcmp(account.channels[0].name,"DM friend"));assert(dnx2RequestDMImages(0,12)&&run()==20);assert(dnx2DMImage(0));account.channels[1]=account.channels[0];account.channels[1].type=3;snprintf(account.channels[1].id,24,"200000000000000002");snprintf(account.channels[1].icon,80,"0123456789abcdef0123456789abcdef");account.channel_count=2;assert(dnx2RequestDMImages(0,12)&&run()==20);assert(dnx2DMImage(1));account.selected_channel=0;assert(dnx2JobStart(10,NULL,NULL)&&run()==10);assert(dnx2JobStart(11,"hello",NULL)&&run()==11);assert(!dnx2RequestProfiles(0,1)&&!dnx2Member("300000000000000001"));
    assert(dnx2JobStart(10,NULL,NULL));account.private_mode=0;assert(run()==0);account.private_mode=1;dnx2ChatReset();free(png);
    puts("PASS production chat jobs: deferred scoped results, single-send, failed-send preservation, empty history, emoji/sticker catalog, unauthenticated CDN, real PNG decode/bounds and cleanup");
}
