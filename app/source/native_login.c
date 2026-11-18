// SPDX-License-Identifier: MIT
/* Experimental first-party-on-device unofficial Discord remote-auth session.
 * WARNING: Discord does not officially support third-party user clients.
 * RSA private key and eventual access token are strictly RAM-only.
 * Requires devkitPro's switch-mbedtls 2.28 package, never homegrown crypto.
 */
#include "native_login.h"
#include "auth_crypto.h"
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
#include <ctype.h>

#ifdef DNX2_USE_MBEDTLS
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>
#endif

#define REMOTE_HOST "remote-auth-gateway.discord.gg"
#define API_HOST "discord.com"
#define WS_BUF 8192
#define HTTP_BUF 131072
static Dnx2Account account;
static unsigned char http_body[HTTP_BUF];
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
    if(s->returned_fd>=0)close(s->returned_fd);
    if(s->socket_fd>=0)close(s->socket_fd);
    memset(s,0,sizeof(*s));
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
    return 1;
fail:
    tls_close(s);return 0;
}
static int tls_send(TlsSession *s,const void *data,size_t n){
    const unsigned char *p=(const unsigned char*)data;
    size_t at=0;
    while(at<n){
        u32 written=0;
        if(R_FAILED(sslConnectionWrite(&s->conn,p+at,(u32)(n-at),&written))||written==0)return 0;
        at+=written;
    }
    return 1;
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
static int tls_exact(TlsSession *s,unsigned char *buf,size_t n,u64 deadline){
    size_t at=0;
    while(at<n && osGetTime()<deadline) {
        int got=tls_get(s,buf+at,n-at);
        if(got>0)at+=(size_t)got;
        else if(got==0)return 0;
        else svcSleepThread(10000000LL);
    }
    return at==n;
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
    for(int reads=0;reads<20 && length<sizeof(hdr)-1;reads++){
        u32 n=0;
        Result rc=sslConnectionRead(&s->conn,hdr+length,sizeof(hdr)-1-length,&n);
        if(R_FAILED(rc)||n==0)return 0;
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
    unsigned char head[2],extra[8];
    if(!tls_exact(s,head,2,deadline))return -1;
    if(head[1]&0x80)return -1; /* server frames MUST NOT be masked */
    size_t n=head[1]&127;unsigned opcode=head[0]&15;
    if(n==126){
        if(!tls_exact(s,extra,2,deadline))return -1;
        n=((size_t)extra[0]<<8)|extra[1];
    }else if(n==127){
        if(!tls_exact(s,extra,8,deadline))return -1;
        n=0;
        for(int i=0;i<8;i++)n=(n<<8)|extra[i];
    }
    if(n>=cap)return -1;
    if(!tls_exact(s,out,n,deadline))return -1;
    out[n]=0;
    if(opcode==8)return -2;
    if(opcode==9){ws_send(s,10,(const char*)out,n);return 0;}
    if(opcode!=1)return 0;
    if(!(head[0]&0x80))return -1; /* fragmented frames unsupported */
    return (int)n;
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
        if(j+1<cap)out[j++]=(char)ch;
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
static int http_request(TlsSession *s,const char *host,const char *method,
                        const char *path,const char *body,const char *token,
                        char *out,size_t cap){
    if(!tls_open(s,host))return 0;
    size_t bodylen=body?strlen(body):0;
    char req[4096];int len=snprintf(req,sizeof(req),
      "%s %s HTTP/1.1\r\nHost: %s\r\nAccept: application/json\r\n"
      "Accept-Encoding: identity\r\nUser-Agent: DiscordNX2-native/0.22\r\n"
      "Connection: close\r\n%s%s%s"
      "Content-Type: application/json\r\nContent-Length: %u\r\n\r\n",
      method,path,host,
      token?"Authorization: ":"",token?token:"",token?"\r\n":"",
      (unsigned)bodylen);
    if(len<=0||(size_t)len>=sizeof(req))return 0;
    int sent=tls_send(s,req,(size_t)len);
    wipe(req,sizeof(req));
    if(!sent)return 0;
    if(bodylen&&!tls_send(s,body,bodylen))return 0;
    size_t used=0;
    while(used+1<cap){
        u32 got=0;
        Result rc=sslConnectionRead(&s->conn,out+used,(u32)(cap-1-used),&got);
        if(R_FAILED(rc)||got==0)break;
        used+=got;
    }
    out[used]=0;
    if(used<16||strncmp(out,"HTTP/1.1 200 ",13)!=0)return 0;
    char *split=strstr(out,"\r\n\r\n");
    if(!split)return 0;
    size_t offset=(size_t)(split-out)+4;
    memmove(out,out+offset,used-offset);
    out[used-offset]=0;
    /* Discord responses typically include Content-Length. On chunked
     * transfers, decode hexadecimal chunks into one bounded JSON buffer. */

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
    if(!ok||!parse_identity((char*)http_body))return 0;
    wipe(http_body,sizeof(http_body));
    status("LOADING REAL DISCORD SERVERS",NULL);
    ok=http_request(&https,API_HOST,"GET","/api/v10/users/@me/guilds?limit=200",
                    NULL,token,(char*)http_body,sizeof(http_body));
    tls_close(&https);
    if(!ok)return 0;
    parse_guilds((char*)http_body);
    wipe(http_body,sizeof(http_body));
    account.signed_in=1;
    return 1;
}
int dnx2NativeQrLogin(Dnx2AuthStatus status,Dnx2AuthCancel cancel) {
    dnx2AccountLogout();
    if(!dnx2AuthCryptoAvailable()){status("MBEDTLS NOT INSTALLED - CANNOT SIGN IN",NULL);return 0;}
    status("GENERATING SECURE RSA-2048 SESSION",NULL);
    if(!dnx2AuthCryptoOpen()){status("SECURE RSA KEY GENERATION FAILED",NULL);return 0;}
    char fingerprint[80]={0},pubkey[900]={0};
    if(!dnx2AuthCryptoPublicKey(pubkey,sizeof(pubkey)) ||
       !dnx2AuthCryptoFingerprint(fingerprint,sizeof(fingerprint)))goto fail;
    if(R_FAILED(socketInitialize(NULL)))goto fail;
    if(R_FAILED(sslInitialize(1))){socketExit();goto fail;}
    int success=0,network_ready=1;
    TlsSession remote;
    status("CONNECTING TO DISCORD QR LOGIN",NULL);
    if(!tls_open(&remote,REMOTE_HOST))goto network_cleanup;
    if(!ws_connect(&remote)) {status("REMOTE AUTH WEBSOCKET REJECTED",NULL);goto remote_cleanup;}
    unsigned heartbeat=41250;u64 last_heartbeat=osGetTime();
    u64 expire=osGetTime()+120000;
    int qr_shown=0;
    char ticket[700]={0};
    for(unsigned attempts=0;attempts<180 && osGetTime()<expire;attempts++){
        if(cancel&&cancel()){status("LOGIN CANCELED",NULL);break;}
        int n=ws_receive(&remote,ws_payload,sizeof(ws_payload),osGetTime()+4500);
        if(n==-2)break;
        if(n<=0) {
            if(osGetTime()-last_heartbeat>(u64)heartbeat) {
                if(!ws_text(&remote,"{\"op\":\"heartbeat\"}"))break;
                last_heartbeat=osGetTime();
            }
            continue;
        }
        const char *json=(const char*)ws_payload;
        if(json_operation(json,"hello")){
            unsigned ms=0,timeout=0;
            if(uint_field(json,"heartbeat_interval",&ms)&&ms>=1000&&ms<90000)heartbeat=ms;
            if(uint_field(json,"timeout_ms",&timeout)&&timeout>=5000&&timeout<180000)
                expire=osGetTime()+timeout;
            if(!ws_text(&remote,"{\"op\":\"heartbeat\"}"))break;
            last_heartbeat=osGetTime();
            char init[1200];
            int len=snprintf(init,sizeof(init),
                 "{\"op\":\"init\",\"encoded_public_key\":\"%s\"}",pubkey);
            if(len<=0||(size_t)len>=sizeof(init)||!ws_text(&remote,init))break;
            wipe(init,sizeof(init));
        }else if(json_operation(json,"nonce_proof")){
            char encrypted[700]={0},proof[100]={0};
            if(!json_string(json,"encrypted_nonce",encrypted,sizeof(encrypted))||
               !dnx2AuthCryptoNonceProof(encrypted,proof,sizeof(proof))){
                status("FAILED TO VERIFY ENCRYPTED NONCE",NULL);break;
            }
            char response[220];
            int len=snprintf(response,sizeof(response),
                   "{\"op\":\"nonce_proof\",\"nonce\":\"%s\"}",proof);
            if(len<=0||(size_t)len>=sizeof(response)||
               !ws_text(&remote,response))break;
            wipe(encrypted,sizeof(encrypted));wipe(proof,sizeof(proof));
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
        if(osGetTime()-last_heartbeat>(u64)heartbeat) {
            if(!ws_text(&remote,"{\"op\":\"heartbeat\"}"))break;
            last_heartbeat=osGetTime();
        }
    }
remote_cleanup:
    tls_close(&remote);
    if(ticket[0]&&qr_shown){
        TlsSession https;
        char json[1000];
        int len=snprintf(json,sizeof(json),"{\"ticket\":\"%s\"}",ticket);
        wipe(ticket,sizeof(ticket));
        if(len>0&&(size_t)len<sizeof(json) &&
           http_request(&https,API_HOST,"POST","/api/v9/users/@me/remote-auth/login",
                        json,NULL,(char*)http_body,sizeof(http_body))) {
            char encrypted[1100]={0};unsigned char access[1024]={0};
            size_t n=0;
            if(json_string((char*)http_body,"encrypted_token",encrypted,sizeof(encrypted)) &&
               dnx2AuthCryptoDecrypt(encrypted,access,sizeof(access)-1,&n)&&n>15&&n<sizeof(access)){
                access[n]=0;
                success=fetch_profile((const char*)access,status);
            }
            wipe(encrypted,sizeof(encrypted));wipe(access,sizeof(access));
        }
        tls_close(&https);wipe(json,sizeof(json));
    }
network_cleanup:
    if(network_ready){sslExit();socketExit();}
    wipe(http_body,sizeof(http_body));wipe(ws_payload,sizeof(ws_payload));
    wipe(ticket,sizeof(ticket));wipe(fingerprint,sizeof(fingerprint));
    wipe(pubkey,sizeof(pubkey));
    dnx2AuthCryptoClose();
    if(success)status("SIGNED IN - REAL SERVER LIST LOADED",NULL);
    else {dnx2AccountLogout();status("QR LOGIN FAILED OR EXPIRED",NULL);}
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
