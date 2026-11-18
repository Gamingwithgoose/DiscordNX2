// SPDX-License-Identifier: MIT
/* Experimental first-party-on-device unofficial Discord remote-auth session.
 * WARNING: Discord does not officially support third-party user clients.
 * RSA private key and eventual access token are strictly RAM-only.
 * Requires devkitPro's switch-mbedtls 2.28 package, never homegrown crypto.
 */
#include "native_login.h"
#include "auth_crypto.h"
#include "captcha.h"
#include "native_ws.h"
#include <switch.h>
#include <switch/services/bsd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>

#ifdef DNX2_USE_MBEDTLS
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>
#endif

/* Monotonic milliseconds, matching the 3DS dnx2MonotonicMs() semantics.
 * Switch 2/libounce does not export dnx2MonotonicMs(). */
static u64 dnx2MonotonicMs(void) {
    struct timespec ts={0};
    if(clock_gettime(CLOCK_MONOTONIC,&ts)!=0)return 0;
    return (u64)ts.tv_sec*1000u+(u64)ts.tv_nsec/1000000u;
}
#define REMOTE_HOST "remote-auth-gateway.discord.gg"
#define API_HOST "discord.com"
#define WS_BUF 8192
#define HTTP_BUF 131072
static Dnx2Account account;
static unsigned char http_body[HTTP_BUF];
static char captcha_headers[8192];
static Dnx2Captcha captcha_challenge;
static char captcha_solution[2049];
static unsigned last_http_status;
static unsigned last_discord_error;
static int last_captcha_required;
static Dnx2WsReader ws_reader;
static Dnx2AuthCancel active_cancel;
static unsigned char ws_payload[WS_BUF+1];

const Dnx2Account *dnx2Account(void){return &account;}
void dnx2AccountSelect(int i){if(i>=0&&i<account.server_count)account.selected=i;}
static void wipe(void *p,size_t n){volatile unsigned char *c=(volatile unsigned char*)p;while(n--)*c++=0;}
void dnx2AccountLogout(void){wipe(&account,sizeof(account));}

#ifdef DNX2_USE_MBEDTLS
typedef struct {
    SslContext ctx;
    SslConnection conn;
    int socket_fd,returned_fd;
    int context_live,conn_live;
    unsigned char pending[WS_BUF];
    size_t pending_off,pending_size;
} TlsSession;
static int starts_ci(const char *p,const char *prefix) {
    while(*prefix) {
        if(tolower((unsigned char)*p++)!=tolower((unsigned char)*prefix++))return 0;
    }
    return 1;
}
static void tls_close(TlsSession *s){
    if(s->conn_live)sslConnectionClose(&s->conn);
    if(s->context_live)sslContextClose(&s->ctx);
    if(s->returned_fd>=0&&s->returned_fd!=s->socket_fd)close(s->returned_fd);
    if(s->socket_fd>=0)close(s->socket_fd);
    memset(s,0,sizeof(*s));
    s->socket_fd=-1;s->returned_fd=-1; /* make repeat cleanup safe */
}
static int tls_open(TlsSession *s,const char *host) {
    memset(s,0,sizeof(*s));s->socket_fd=-1;s->returned_fd=-1;
    struct addrinfo hints={0},*list=NULL;
    hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
    if(getaddrinfo(host,"443",&hints,&list)!=0||!list)return 0;
    for(struct addrinfo *p=list;p;p=p->ai_next) {
        int fd=socket(p->ai_family,p->ai_socktype,p->ai_protocol);
        if(fd<0)continue;
        struct timeval t={5,0};
        setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&t,sizeof(t));
        setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&t,sizeof(t));
        if(connect(fd,p->ai_addr,p->ai_addrlen)==0){s->socket_fd=fd;break;}
        close(fd);
    }
    freeaddrinfo(list);
    if(s->socket_fd<0)return 0;
    if(R_FAILED(sslCreateContext(&s->ctx,SslVersion_TlsV12|SslVersion_TlsV13)))goto fail;
    s->context_live=1;
    if(R_FAILED(sslContextCreateConnection(&s->ctx,&s->conn)))goto fail;
    s->conn_live=1;
    if(R_FAILED(sslConnectionSetHostName(&s->conn,host,(u32)strlen(host)+1)))goto fail;
    errno=0;
    int returned=socketSslConnectionSetSocketDescriptor(&s->conn,s->socket_fd);
    if(returned<0&&errno!=ENOENT)goto fail;
    s->returned_fd=returned>=0?returned:-1;
    (void)sslConnectionSetIoTimeout(&s->conn,2500);
    u32 bytes=0,certs=0;
    if(R_FAILED(sslConnectionDoHandshake(&s->conn,&bytes,&certs,NULL,0)))goto fail;
    /* PeerCA + HostName verification are enabled by default by libounce SSL;
     * never set SkipDefaultVerify or disable SSL hostname checking. */
    if(R_FAILED(sslConnectionSetIoMode(&s->conn,SslIoMode_NonBlocking)))goto fail;
    return 1;
fail:
    tls_close(s);return 0;
}
static int tls_send(TlsSession *s,const void *data,size_t n){
    const unsigned char *p=(const unsigned char*)data;
    size_t at=0;u64 deadline=dnx2MonotonicMs()+5000;
    while(at<n && dnx2MonotonicMs()<deadline){
        u32 written=0;
        if(active_cancel&&active_cancel())return 0;
        if(R_FAILED(sslConnectionWrite(&s->conn,p+at,(u32)(n-at),&written))){svcSleepThread(8000000LL);continue;}
        if(written==0)return 0;
        at+=written;
    }
    return at==n;
}
static int tls_get(TlsSession *s,unsigned char *out,size_t max){
    if(s->pending_off<s->pending_size) {
        size_t size=s->pending_size-s->pending_off;
        if(size>max)size=max;
        memcpy(out,s->pending+s->pending_off,size);
        s->pending_off+=size;
        return (int)size;
    }
    u32 got=0;
    Result rc=sslConnectionRead(&s->conn,out,(u32)max,&got);
    return R_SUCCEEDED(rc)?(int)got:-1;
}
static int random_bytes(unsigned char *out,size_t n){
    Result rc=csrngInitialize();
    if(R_FAILED(rc))return 0;
    rc=csrngGetRandomBytes(out,n);
    csrngExit();
    return R_SUCCEEDED(rc);
}
static int b64(const unsigned char *in,size_t n,char *out,size_t cap){
    size_t result=0;
    return mbedtls_base64_encode((unsigned char*)out,cap,&result,in,n)==0;
}
static int ws_connect(TlsSession *s){
    unsigned char rnd[16];
    if(!random_bytes(rnd,sizeof(rnd)))return 0;
    char key[32]={0};
    if(!b64(rnd,sizeof(rnd),key,sizeof(key)))return 0;
    char req[700];
    int len=snprintf(req,sizeof(req),
        "GET /?v=2 HTTP/1.1\r\nHost: " REMOTE_HOST "\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n"
        "Origin: https://discord.com\r\n"
        "User-Agent: DiscordNX2-native/0.22\r\n\r\n",key);
    if(len<=0||(size_t)len>=sizeof(req)||!tls_send(s,req,(size_t)len))return 0;
    unsigned char hdr[4096]={0};
    size_t length=0,where=0;
    u64 upgrade_deadline=dnx2MonotonicMs()+10000;
    while(dnx2MonotonicMs()<upgrade_deadline && length<sizeof(hdr)-1){
        u32 n=0;
        Result rc=sslConnectionRead(&s->conn,hdr+length,sizeof(hdr)-1-length,&n);
        if(R_FAILED(rc)){svcSleepThread(8000000LL);continue;}
        if(n==0)return 0;
        length+=n;hdr[length]=0;
        char *sep=strstr((char*)hdr,"\r\n\r\n");
        if(sep){where=(size_t)(sep-(char*)hdr)+4;break;}
    }
    if(!where||strncmp((char*)hdr,"HTTP/1.1 101 ",13)!=0)return 0;
    if(!strstr((char*)hdr,"Upgrade: websocket") &&
       !strstr((char*)hdr,"upgrade: websocket"))return 0;
    /* RFC6455 handshake accept MUST match randomized challenge; don't accept
     * an intercepted 101 with the wrong binding to our request key. */
    char concat[128],expected[48]={0};
    int n=snprintf(concat,sizeof(concat),"%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11",key);
    if(n<=0||(size_t)n>=sizeof(concat))return 0;
    unsigned char digest[20];
    if(mbedtls_sha1_ret((unsigned char*)concat,(size_t)n,digest)!=0 ||
       !b64(digest,sizeof(digest),expected,sizeof(expected)))return 0;
    int accept_ok=0;
    const char *line=(const char*)hdr;
    while(line<(const char*)hdr+where){
        const char *end=strstr(line,"\r\n");
        if(!end||end>(const char*)hdr+where)break;
        if(starts_ci(line,"sec-websocket-accept:")){
            const char *val=line+21;
            while(val<end&&(*val==' '||*val=='\t'))val++;
            accept_ok=(size_t)(end-val)==strlen(expected) &&
                       memcmp(val,expected,strlen(expected))==0;
        }
        line=end+2;
    }
    if(!accept_ok)return 0;
    s->pending_size=length-where;
    if(s->pending_size>sizeof(s->pending))return 0;
    memcpy(s->pending,hdr+where,s->pending_size);
    s->pending_off=0;
    wipe(hdr,sizeof(hdr));wipe(concat,sizeof(concat));
    return 1;
}
static int ws_send(TlsSession *s,unsigned opcode,const char *data,size_t len){
    if(len>4096)return 0;
    unsigned char header[8],mask[4];
    if(!random_bytes(mask,sizeof(mask)))return 0;
    size_t hn=0;header[hn++]=0x80|(unsigned char)opcode;
    if(len<126)header[hn++]=(unsigned char)(0x80|len);
    else {header[hn++]=0xfe;header[hn++]=(unsigned char)(len>>8);header[hn++]=(unsigned char)len;}
    memcpy(header+hn,mask,4);hn+=4;
    if(!tls_send(s,header,hn))return 0;
    unsigned char tmp[4096];
    for(size_t i=0;i<len;i++)tmp[i]=(unsigned char)data[i]^mask[i%4];
    int ok=tls_send(s,tmp,len);wipe(tmp,sizeof(tmp));return ok;
}
static int ws_text(TlsSession *s,const char *msg){
    return ws_send(s,1,msg,strlen(msg));
}
static int ws_receive(TlsSession *s,unsigned char *out,size_t cap,u64 deadline){
    while(dnx2MonotonicMs()<deadline){
        if(active_cancel&&active_cancel())return -2;
        size_t before=ws_reader.used;
        int result=dnx2WsNext(&ws_reader,out,cap);
        if(result<0)return result;
        if(result==1)return (int)strlen((char*)out);
        if(result==2){if(!ws_send(s,10,(char*)out,ws_reader.control_size))return -1;}
        if(ws_reader.used<before)continue;
        if(ws_reader.used==sizeof(ws_reader.wire))return -1;
        int n=tls_get(s,ws_reader.wire+ws_reader.used,sizeof(ws_reader.wire)-ws_reader.used);
        if(n>0)ws_reader.used+=(size_t)n;
        else if(n==0)return -2;
        else svcSleepThread(8000000LL);
    }
    return 0; /* keep partial frame for next poll */
}
static int json_string(const char *body,const char *key,char *out,size_t cap){
    if(!body||!key||!out||cap==0)return 0;
    char name[80];snprintf(name,sizeof(name),"\"%s\"",key);
    const char *p=strstr(body,name);
    if(!p)return 0;
    p+=strlen(name);
    while(*p==' '||*p=='\t')p++;
    if(*p++!=':')return 0;
    while(*p==' '||*p=='\t')p++;
    if(*p++!='"')return 0;
    size_t j=0;
    while(*p&&*p!='"'){
        unsigned char ch=(unsigned char)*p++;
        if(ch=='\\'){
            ch=(unsigned char)*p++;
            if(ch=='u') {if(strlen(p)<4)return 0;p+=4;ch='?';}
            else if(ch=='n'||ch=='r'||ch=='t')ch=' ';
        }
        if(ch<32||ch>126)ch='?';
        if(j+1>=cap){out[0]=0;return 0;}
        out[j++]=(char)ch;
    }
    out[j]=0;return *p=='"';
}
static int uint_field(const char *body,const char *key,unsigned *out){
    char name[80];snprintf(name,sizeof(name),"\"%s\"",key);
    const char *p=strstr(body,name);if(!p)return 0;
    p=strchr(p,':');if(!p)return 0;
    *out=(unsigned)strtoul(p+1,NULL,10);return *out!=0;
}
static int json_operation(const char *json,const char *op){
    char v[64];
    return json_string(json,"op",v,sizeof(v))&&strcmp(v,op)==0;
}
/* Never log requests or API response bodies (they can carry access tokens). */
static int http_request(TlsSession *s,const char *host,const char *method,
                        const char *path,const char *body,const char *token,
                        char *out,size_t cap){
    last_http_status=0;last_discord_error=0;last_captcha_required=0;
    if(!tls_open(s,host))return 0;
    size_t bodylen=body?strlen(body):0;
    char req[12288];
    int len=snprintf(req,sizeof(req),
      "%s %s HTTP/1.1\r\nHost: %s\r\nAccept: application/json\r\n"
      "Accept-Encoding: identity\r\nUser-Agent: DiscordNX2-native/0.28\r\n"
      "Origin: https://discord.com\r\nReferer: https://discord.com/login\r\n"
      "Connection: close\r\n%s%s%s"
      "%sContent-Type: application/json\r\nContent-Length: %u\r\n\r\n",
      method,path,host,
      token?"Authorization: ":"",token?token:"",token?"\r\n":"",
      captcha_headers,(unsigned)bodylen);
    if(len<=0||(size_t)len>=sizeof(req))return 0;
    int sent=tls_send(s,req,(size_t)len);
    wipe(req,sizeof(req));
    if(!sent)return 0;
    if(bodylen&&!tls_send(s,body,bodylen))return 0;
    size_t used=0;u64 read_deadline=dnx2MonotonicMs()+15000;
    while(used+1<cap && dnx2MonotonicMs()<read_deadline){
        if(active_cancel&&active_cancel())return 0;
        u32 got=0;
        Result rc=sslConnectionRead(&s->conn,out+used,(u32)(cap-1-used),&got);
        if(R_FAILED(rc)){svcSleepThread(8000000LL);continue;}
        if(got==0)break;
        used+=got;
    }
    out[used]=0;
    if(used>=12 && strncmp(out,"HTTP/1.",7)==0)last_http_status=(unsigned)strtoul(out+9,NULL,10);
    if(used<16 || last_http_status<100 || last_http_status>599)return 0;
    char *split=strstr(out,"\r\n\r\n");
    if(!split)return 0;
    size_t offset=(size_t)(split-out)+4;
    /* The header ends exactly at offset. We must check framing BEFORE
     * memmove removes the header, or incomplete HTTP bodies look valid. */
    int chunked=0,has_length=0;
    size_t expected_length=0;
    const char *p=out;
    while(p<out+offset){
        const char *end=strstr(p,"\r\n");
        if(!end || end>out+offset)break;
        if(starts_ci(p,"transfer-encoding:")){
            const char *v=p+18;
            while(v<end && (*v==' '||*v=='\t'))v++;
            if(end-v>=7 && starts_ci(v,"chunked"))chunked=1;
        }
        if(starts_ci(p,"content-length:")){
            char *after=NULL;
            unsigned long value=strtoul(p+15,&after,10);
            if(after!=p+15 && value<cap) {
                expected_length=(size_t)value;has_length=1;
            }
        }
        p=end+2;
    }
    size_t payload=used-offset;
    memmove(out,out+offset,payload);
    out[payload]=0;
    if(chunked){
        size_t read_at=0,write_at=0;
        while(read_at<payload){
            char *line_end=strstr(out+read_at,"\r\n");
            if(!line_end)return 0;
            unsigned long count=strtoul(out+read_at,NULL,16);
            if(count>cap || count>payload)return 0;
            read_at=(size_t)(line_end-out)+2;
            if(count==0){out[write_at]=0;goto body_complete;}
            if(read_at+count+2>payload ||
               out[read_at+count]!='\r'||out[read_at+count+1]!='\n')return 0;
            memmove(out+write_at,out+read_at,count);
            write_at+=count;
            read_at+=count+2;
        }
        return 0;
    }
    if(has_length && payload<expected_length)return 0;
    if(payload==0)return 0;
body_complete:
    if(last_http_status!=200){
        /* Only fixed categories and bounded public API error codes escape RAM.
         * Never expose message, captcha payload/sitekey, ticket or token. */
        last_captcha_required=strstr(out,"\"captcha_key\"")!=NULL ||
                              strstr(out,"\"captcha_sitekey\"")!=NULL;
        unsigned code=0;
        if(uint_field(out,"code",&code) && code<=999999)last_discord_error=code;
        return 0;
    }
    return 1;
}
static int parse_identity(const char *json){
    char username[64]={0},display[80]={0};
    if(!json_string(json,"username",username,sizeof(username)))return 0;
    (void)json_string(json,"global_name",display,sizeof(display));
    snprintf(account.username,sizeof(account.username),"%s",username);
    snprintf(account.display_name,sizeof(account.display_name),"%s",display[0]?display:username);
    return 1;
}
static void parse_guilds(const char *json) {
    const char *p=json;
    while(account.server_count<DNX2_MAX_SERVERS) {
        const char *left=strchr(p,'{');if(!left)break;
        const char *right=strchr(left,'}');if(!right)break;
        if((size_t)(right-left)>4096)break;
        char entry[4097];size_t n=(size_t)(right-left)+1;
        memcpy(entry,left,n);entry[n]=0;
        char id[24]={0},name[80]={0};
        if(json_string(entry,"id",id,sizeof(id)) &&
           json_string(entry,"name",name,sizeof(name))){
            int valid=1;
            if(strlen(id)<15||strlen(id)>22)valid=0;
            for(const char *digit=id;*digit;digit++)
                if(*digit<'0'||*digit>'9')valid=0;
            if(valid) {
                Dnx2Server *g=&account.servers[account.server_count++];
                strcpy(g->id,id);strcpy(g->name,name);
            }
        }
        p=right+1;
    }
}
static int fetch_profile(const char *token,Dnx2AuthStatus status){
    TlsSession https;
    status("AUTHENTICATED - LOADING ACCOUNT",NULL);
    int ok=http_request(&https,API_HOST,"GET","/api/v10/users/@me",NULL,token,
                        (char*)http_body,sizeof(http_body));
    tls_close(&https);
    if(!ok||!parse_identity((char*)http_body)){status("ACCOUNT PROFILE COULD NOT BE LOADED",NULL);return 0;}
    wipe(http_body,sizeof(http_body));
    status("LOADING REAL DISCORD SERVERS",NULL);
    ok=http_request(&https,API_HOST,"GET","/api/v10/users/@me/guilds?limit=200",
                    NULL,token,(char*)http_body,sizeof(http_body));
    tls_close(&https);
    if(!ok){status("SERVER LIST COULD NOT BE LOADED",NULL);return 0;}
    parse_guilds((char*)http_body);
    wipe(http_body,sizeof(http_body));
    account.signed_in=1;
    return 1;
}
int dnx2NativeQrLogin(Dnx2AuthStatus status,Dnx2AuthCancel cancel) {
    memset(&ws_reader,0,sizeof(ws_reader));active_cancel=cancel;
    dnx2AccountLogout();
    if(!dnx2AuthCryptoAvailable()){status("MBEDTLS NOT INSTALLED - CANNOT SIGN IN",NULL);return 0;}
    status("GENERATING SECURE RSA-2048 SESSION",NULL);
    if(!dnx2AuthCryptoOpen()){status("SECURE RSA KEY GENERATION FAILED",NULL);return 0;}
    char fingerprint[80]={0},pubkey[900]={0};
    if(!dnx2AuthCryptoSelfTest()){
        char self_error[90];
        snprintf(self_error,sizeof(self_error),"LOCAL CRYPTO SELFTEST FAILED MBEDTLS %d",dnx2AuthCryptoMbedError());
        status(self_error,NULL);goto fail;
    }
    status("LOCAL RSA OAEP AND BASE64 SELFTEST PASSED",NULL);
    if(!dnx2AuthCryptoPublicKey(pubkey,sizeof(pubkey)) ||
       !dnx2AuthCryptoFingerprint(fingerprint,sizeof(fingerprint)))goto fail;
    if(R_FAILED(socketInitialize(NULL)))goto fail;
    if(R_FAILED(sslInitialize(1))){socketExit();goto fail;}
    int success=0,network_ready=1;
    char failure_reason[128]="QR login failed or expired";
    int qr_shown=0;
    char ticket[700]={0};
    TlsSession remote;
    status("CONNECTING TO DISCORD QR LOGIN",NULL);
    if(!tls_open(&remote,REMOTE_HOST))goto network_cleanup;
    if(!ws_connect(&remote)) {status("REMOTE AUTH WEBSOCKET REJECTED",NULL);goto remote_cleanup;}
    unsigned heartbeat=41250;u64 last_heartbeat=dnx2MonotonicMs();
    u64 expire=dnx2MonotonicMs()+120000;
    for(unsigned attempts=0;attempts<180 && dnx2MonotonicMs()<expire;attempts++){
        if(cancel&&cancel()){status("LOGIN CANCELED",NULL);break;}
        int n=ws_receive(&remote,ws_payload,sizeof(ws_payload),dnx2MonotonicMs()+4500);
        if(n==-2){status("LOGIN CONNECTION CLOSED OR CANCELED",NULL);break;}
        if(n<0){status("LOGIN MESSAGE INVALID - PLEASE RETRY",NULL);break;}
        if(n<=0) {
            if(dnx2MonotonicMs()-last_heartbeat>(u64)heartbeat) {
                if(!ws_text(&remote,"{\"op\":\"heartbeat\"}"))break;
                last_heartbeat=dnx2MonotonicMs();
            }
            continue;
        }
        const char *json=(const char*)ws_payload;
        if(json_operation(json,"hello")){
            unsigned ms=0,timeout=0;
            if(uint_field(json,"heartbeat_interval",&ms)&&ms>=1000&&ms<90000)heartbeat=ms;
            if(uint_field(json,"timeout_ms",&timeout)&&timeout>=5000&&timeout<180000)
                expire=dnx2MonotonicMs()+timeout;
            if(!ws_text(&remote,"{\"op\":\"heartbeat\"}"))break;
            last_heartbeat=dnx2MonotonicMs();
            char init[1200];
            int len=snprintf(init,sizeof(init),
                 "{\"op\":\"init\",\"encoded_public_key\":\"%s\"}",pubkey);
            if(len<=0||(size_t)len>=sizeof(init)||!ws_text(&remote,init))break;
            wipe(init,sizeof(init));
        }else if(json_operation(json,"nonce_proof")){
            char encrypted[700]={0},proof[DNX2_AUTH_PROOF_CAP]={0};
            if(!json_string(json,"encrypted_nonce",encrypted,sizeof(encrypted))){
                status("NONCE FIELD MISSING OR INVALID",NULL);break;
            }
            if(!dnx2AuthCryptoNonceProof(encrypted,proof,sizeof(proof))){
                int error=dnx2AuthCryptoError();
                if(error==2)status("NONCE ERROR: BASE64 DECODE FAILED",NULL);
                else if(error==3)status("NONCE ERROR: CIPHERTEXT LENGTH",NULL);
                else if(error==4){
                    char reason[90];
                    snprintf(reason,sizeof(reason),"NONCE RSA OAEP FAILED MBEDTLS %d",dnx2AuthCryptoMbedError());
                    status(reason,NULL);
                }
                else if(error==5)status("NONCE ERROR: PROOF ENCODE FAILED",NULL);
                else status("NONCE ERROR: NO ACTIVE CRYPTO SESSION",NULL);
                wipe(encrypted,sizeof(encrypted));
                break;
            }
            char response[DNX2_AUTH_PROOF_CAP+64];
            int len=snprintf(response,sizeof(response),
                   "{\"op\":\"nonce_proof\",\"nonce\":\"%s\"}",proof);
            int sent=len>0 && (size_t)len<sizeof(response) && ws_text(&remote,response);
            wipe(response,sizeof(response));
            wipe(encrypted,sizeof(encrypted));wipe(proof,sizeof(proof));
            if(!sent){status("NONCE RESPONSE SEND FAILED",NULL);break;}
            status("NONCE PROOF SENT - WAITING FOR QR",NULL);
        }else if(json_operation(json,"pending_remote_init")){
            char received[80]={0};
            if(!json_string(json,"fingerprint",received,sizeof(received)) ||
               strlen(received)!=strlen(fingerprint) ||
               memcmp(received,fingerprint,strlen(fingerprint))!=0){
                status("FINGERPRINT MISMATCH - LOGIN BLOCKED",NULL);break;
            }
            char url[160];
            snprintf(url,sizeof(url),"https://discord.com/ra/%s",fingerprint);
            status("SCAN QR WITH DISCORD MOBILE APP",url);
            qr_shown=1;
        }else if(json_operation(json,"pending_ticket")){
            status("PHONE DETECTED - CONFIRM LOGIN ON PHONE",NULL);
        }else if(json_operation(json,"pending_login")){
            if(!qr_shown || !json_string(json,"ticket",ticket,sizeof(ticket)))break;
            status("PHONE CONFIRMED - EXCHANGING SESSION",NULL);
            break;
        }else if(json_operation(json,"cancel")){
            status("LOGIN CANCELED ON PHONE",NULL);break;
        }
        if(dnx2MonotonicMs()-last_heartbeat>(u64)heartbeat) {
            if(!ws_text(&remote,"{\"op\":\"heartbeat\"}"))break;
            last_heartbeat=dnx2MonotonicMs();
        }
    }
    if(!ticket[0] && dnx2MonotonicMs()>=expire)status("LOGIN TIMED OUT - PLEASE RETRY",NULL);
remote_cleanup:
    /* Redeem promptly before releasing our remote session. */
    if(ticket[0]&&qr_shown){
        TlsSession https={.socket_fd=-1,.returned_fd=-1};
        char json[1000];
        int len=snprintf(json,sizeof(json),"{\"ticket\":\"%s\"}",ticket);
        wipe(ticket,sizeof(ticket));
        int exchange_ok=0,browser_unavailable=0;
        if(len>0&&(size_t)len<sizeof(json)){
            exchange_ok=http_request(&https,API_HOST,"POST","/api/v10/users/@me/remote-auth/login",
                                    json,NULL,(char*)http_body,sizeof(http_body));
            if(!exchange_ok && last_http_status==400 && last_captcha_required){
                memset(&captcha_challenge,0,sizeof(captcha_challenge));
                int valid=json_string((char*)http_body,"captcha_service",captcha_challenge.service,sizeof(captcha_challenge.service)) &&
                    json_string((char*)http_body,"captcha_sitekey",captcha_challenge.sitekey,sizeof(captcha_challenge.sitekey));
                const char *keys[]={"captcha_rqdata","captcha_rqtoken","captcha_session_id"};
                char *fields[]={captcha_challenge.rqdata,captcha_challenge.rqtoken,captcha_challenge.session_id};
                size_t sizes[]={sizeof(captcha_challenge.rqdata),sizeof(captcha_challenge.rqtoken),sizeof(captcha_challenge.session_id)};
                for(int i=0;i<3;i++)if(strstr((char*)http_body,keys[i])&&!json_string((char*)http_body,keys[i],fields[i],sizes[i]))valid=0;
                tls_close(&https);
                wipe(http_body,sizeof(http_body));
                status("CAPTCHA REQUIRED - PREPARING USER VERIFICATION",NULL);
                if(valid && dnx2CaptchaShow(&captcha_challenge,captcha_solution,sizeof(captcha_solution)) &&
                   dnx2CaptchaHeaders(&captcha_challenge,captcha_solution,captcha_headers,sizeof(captcha_headers))){
                    status("VERIFICATION COMPLETED - SUBMITTING TO DISCORD",NULL);
                    exchange_ok=http_request(&https,API_HOST,"POST","/api/v10/users/@me/remote-auth/login",
                                            json,NULL,(char*)http_body,sizeof(http_body));
                }else {
                    char browser_stage[100];
                    snprintf(browser_stage,sizeof(browser_stage),"CAPTCHA BROWSER RESULT %08x EXIT %08x",dnx2CaptchaBrowserError(),dnx2CaptchaBrowserExit());
                    browser_unavailable=dnx2CaptchaBrowserError()!=0 || dnx2CaptchaBrowserExit()==7;
                    status(browser_stage,NULL);
                }
                wipe(captcha_headers,sizeof(captcha_headers));wipe(captcha_solution,sizeof(captcha_solution));
                wipe(&captcha_challenge,sizeof(captcha_challenge));
            }
        }
        if(exchange_ok){
            char encrypted[1100]={0};unsigned char access[1024]={0};
            size_t n=0;
            if(json_string((char*)http_body,"encrypted_token",encrypted,sizeof(encrypted)) &&
               dnx2AuthCryptoDecrypt(encrypted,access,sizeof(access)-1,&n)&&n>15&&n<sizeof(access)){
                access[n]=0;
                success=fetch_profile((const char*)access,status);
            }
            else status("LOGIN TOKEN RESPONSE INVALID OR DECRYPT FAILED",NULL);
            wipe(encrypted,sizeof(encrypted));wipe(access,sizeof(access));
        }else {
            char reason[96];
            if(browser_unavailable){
                snprintf(failure_reason,sizeof(failure_reason),"System browser unavailable. CAPTCHA could not open.");
            }else if(last_captcha_required){
                snprintf(failure_reason,sizeof(failure_reason),"CAPTCHA was not completed or accepted. Check the login log.");
            }else if(last_http_status==429){
                snprintf(failure_reason,sizeof(failure_reason),"Discord rate limited login. Please try again later.");
            }else {
                snprintf(reason,sizeof(reason),"Session exchange failed: HTTP %u, Discord code %u",last_http_status,last_discord_error);
                snprintf(failure_reason,sizeof(failure_reason),"%s",reason);
            }
            status(failure_reason,NULL);
        }
        tls_close(&https);wipe(json,sizeof(json));
    }
    tls_close(&remote);
network_cleanup:
    if(network_ready){sslExit();socketExit();}
    wipe(http_body,sizeof(http_body));wipe(ws_payload,sizeof(ws_payload));
    wipe(&ws_reader,sizeof(ws_reader));active_cancel=NULL;
    wipe(captcha_headers,sizeof(captcha_headers));wipe(captcha_solution,sizeof(captcha_solution));wipe(&captcha_challenge,sizeof(captcha_challenge));
    wipe(ticket,sizeof(ticket));wipe(fingerprint,sizeof(fingerprint));
    wipe(pubkey,sizeof(pubkey));
    dnx2AuthCryptoClose();
    if(success)status("SIGNED IN - REAL SERVER LIST LOADED",NULL);
    else {dnx2AccountLogout();status(failure_reason,NULL);}
    return success;
fail:
    dnx2AuthCryptoClose();
    wipe(pubkey,sizeof(pubkey));wipe(fingerprint,sizeof(fingerprint));
    status("NATIVE AUTH CRYPTO OR NETWORK INIT FAILED",NULL);
    return 0;
}
#else
int dnx2NativeQrLogin(Dnx2AuthStatus status,Dnx2AuthCancel cancel){
    (void)cancel;
    if(status)status("INSTALL SWITCH-MBEDTLS TO ENABLE QR LOGIN",NULL);
    return 0;
}
#endif
