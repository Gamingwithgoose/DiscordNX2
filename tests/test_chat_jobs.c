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
static char session_token[1024]="synthetic-token";
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
void dnx2LiveStop(void){}
static void tls_close(TlsSession *s){(void)s;}
static int http_request(TlsSession *s,const char *host,const char *method,const char *path,const char *body,const char *token,char *out,size_t cap){
    (void)s;last_http_status=http_fail?403:200;
    if(!strcmp(host,"cdn.discordapp.com")){assert(!token&&!strcmp(method,"GET"));assert(!strncmp(path,"/icons/",7)||!strncmp(path,"/emojis/",8)||!strncmp(path,"/stickers/",10));assert(png_size<cap);memcpy(out,png,png_size);last_http_body_size=png_size;cdn_calls++;return !http_fail;}
    assert(!strcmp(host,API_HOST)&&!strcmp(token,"synthetic-token"));
    if(!strcmp(method,"POST")){
        post_calls++;char text[200];assert(dj_get(dj_root(body),"content",text,sizeof(text)));assert(!strcmp(text,"hello"));
        DnxJson ids=dj_field(dj_root(body),"sticker_ids");if(ids.p)assert(strstr(body,"300000000000000001"));strcpy(out,"{}");return !http_fail;
    }
    const char *response=strstr(path,"/emojis")?"[{\"id\":\"400000000000000001\",\"name\":\"goose\",\"animated\":true}]":strstr(path,"/stickers")?"[{\"id\":\"300000000000000001\",\"name\":\"Wave\",\"format_type\":1}]":strstr(path,"/guilds/")?"[{\"id\":\"200000000000000001\",\"name\":\"general\",\"type\":0,\"position\":1}]":empty_history?"[]":"[{\"id\":\"900000000000000001\",\"author\":{\"username\":\"fixture\"},\"content\":\"fresh\"}]";
    snprintf(out,cap,"%s",response);last_http_body_size=strlen(out);return !http_fail;
}
static void voice_control_preflight(const char *t,const char *g,const char *c,const char *u,char notice[160],int *stage,unsigned *code){(void)t;(void)g;(void)c;(void)u;snprintf(notice,160,"Engine unavailable");*stage=0;*code=0;}
#include "chat_jobs.inc"
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
    dnx2ChatReset();assert(!dnx2ChatImage(1,0)&&!dnx2MediaCatalog()->loaded);free(png);
    puts("PASS production chat jobs: deferred scoped results, single-send, failed-send preservation, empty history, emoji/sticker catalog, unauthenticated CDN, real PNG decode/bounds and cleanup");
}
