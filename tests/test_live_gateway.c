// SPDX-License-Identifier: MIT
/* Synthetic Gateway/REST fixtures drive the production worker. No network or
 * credentials. Thread entry is called deterministically; mutex use is checked. */
#include "client_data.h"
#include <assert.h>
#include <stdint.h>
typedef uint64_t u64;
typedef struct {int dummy;} Thread;
typedef int Mutex;
typedef struct {int socket_fd,returned_fd;} TlsSession;
#define R_FAILED(x) ((x)!=0)
#define R_SUCCEEDED(x) ((x)==0)
#define DNX2_WS_MAX 8192
#define API_HOST "discord.com"
static Dnx2Account account;
static char session_token[1024],http_body[10000];
static struct {char test[10];} ws_reader;
static int (*active_cancel)(void);
static u64 now;
static int locks,service_count,frames,requests,fail_gateway,http_code=200,identify_sent,resume_sent,subscription_sent,heartbeat_sent;
static unsigned last_http_status;
static u64 dnx2MonotonicMs(void){return now;}
static void wipe(void *p,size_t n){memset(p,0,n);}
static void mutexLock(Mutex *m){assert(!*m);*m=1;locks++;}
static void mutexUnlock(Mutex *m){assert(*m);*m=0;locks--;}
static int socketInitialize(void *p){(void)p;service_count++;return 0;}
static int sslInitialize(int n){(void)n;service_count++;return 0;}
static int csrngInitialize(void){service_count++;return 0;}
static void socketExit(void){service_count--;}
static void sslExit(void){service_count--;}
static void csrngExit(void){service_count--;}
static int threadCreate(Thread *t,void (*f)(void*),void *arg,void *stack,size_t size,int prio,int core){(void)t;(void)f;(void)arg;(void)stack;(void)size;(void)prio;(void)core;return 0;}
static int threadStart(Thread *t){(void)t;return 0;}
static void threadWaitForExit(Thread *t){(void)t;}
static void threadClose(Thread *t){(void)t;}
static int tls_open(TlsSession *s,const char *host){(void)s;assert(!strcmp(host,"gateway.discord.gg"));return !fail_gateway;}
static void tls_close(TlsSession *s){(void)s;}
static int ws_connect_to(TlsSession *s,const char *host,const char *path){(void)s;assert(!strcmp(host,"gateway.discord.gg")&&!strcmp(path,"/?v=10&encoding=json"));return 1;}
static int ws_text(TlsSession *s,const char *body){
    (void)s;DnxJson root=dj_root(body);assert(root.p);int op=dj_int(root,"op",-1);
    if(op==2)identify_sent++;else if(op==6)resume_sent++;else if(op==14)subscription_sent++;else if(op==1)heartbeat_sent++;else assert(0);return 1;
}
static int ws_receive(TlsSession *s,unsigned char *out,size_t cap,u64 deadline){
    (void)s;(void)deadline;
    const char *event=frames==0?"{\"op\":10,\"d\":{\"heartbeat_interval\":1000}}":frames==1?"{\"op\":0,\"s\":1,\"t\":\"READY\",\"d\":{\"session_id\":\"synthetic-session\"}}":frames==2?"{\"op\":0,\"s\":2,\"t\":\"MESSAGE_CREATE\",\"d\":{\"channel_id\":\"200000000000000001\"}}":"{\"op\":11,\"d\":null}";
    frames++;assert(strlen(event)<cap);strcpy((char*)out,event);return (int)strlen(event);
}
static int http_request(TlsSession *s,const char *host,const char *method,const char *path,const char *body,const char *token,char *out,size_t cap){
    (void)s;(void)body;assert(!strcmp(host,API_HOST)&&!strcmp(method,"GET"));assert(strstr(path,"200000000000000001/messages?limit=30"));assert(!strcmp(token,"synthetic-token"));
    requests++;last_http_status=(unsigned)http_code;
    if(http_code==429){snprintf(out,cap,"{\"retry_after\":12}");return 0;}
    snprintf(out,cap,"[{\"id\":\"%d\",\"author\":{\"username\":\"fixture\"},\"content\":\"message %d\"}]",requests,requests);return http_code==200;
}
static void svcSleepThread(long long n);
#include "live_gateway.inc"
static void svcSleepThread(long long n){(void)n;now+=16;if(now>7000)__atomic_store_n(&live_stop,1,__ATOMIC_RELEASE);}
int main(void){
    account.signed_in=1;account.server_count=1;account.channel_count=1;account.selected_channel=0;
    strcpy(account.servers[0].id,"100000000000000001");strcpy(account.channels[0].id,"200000000000000001");strcpy(session_token,"synthetic-token");
    dnx2LiveStart();live_worker(NULL);assert(identify_sent==1&&subscription_sent==1&&heartbeat_sent>0&&requests>=2);
    assert(service_count==0&&locks==0);assert(dnx2LiveApply());assert(account.message_count==1&&strstr(account.messages[0].content,"message"));assert(!strcmp(dnx2LiveStatus(),"Live updates"));
    dnx2LiveStop();assert(!live_token[0]);
    dnx2LiveStart();assert(live_identify(&(TlsSession){0})&&resume_sent==1);dnx2LiveStop();
    strcpy(live_token,"synthetic-token");http_code=429;Dnx2Account *result=calloc(1,sizeof(*result));assert(result);u64 retry=0;
    assert(!live_refresh(result,&retry)&&retry==now+12250&&!strcmp(dnx2LiveStatus(),"Discord rate limited updates"));
    http_code=403;assert(!live_refresh(result,&retry)&&!strcmp(dnx2LiveStatus(),"Channel access unavailable"));free(result);
    strcpy(account.channels[0].id,"300000000000000001");live_generation++;assert(!dnx2LiveApply());
    dnx2LiveReset();assert(!live_session[0]&&!live_token[0]&&!live_count);
    puts("PASS production live worker: Hello/Identify/Ready, subscriptions, message refresh, heartbeat, Resume, rate limit, permission failure, channel binding and cleanup");
}
