// SPDX-License-Identifier: MIT
// Unauthenticated Discord Gateway connection checks. Never requests or uses
// user credentials, user tokens, bot tokens or client secrets.
#include "discord_gateway_probe.h"
#include <switch.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

#define D_HOST "gateway.discord.gg"
#define D_PORT "443"
#define DNX2_IO_LIMIT 8192u

static unsigned char rxbuf[DNX2_IO_LIMIT];
static char httpheader[2048];

static int ci_has(const char *haystack, const char *needle) {
    size_t n=strlen(needle);
    for(const char *p=haystack;*p;p++) {
        size_t i=0;
        while(i<n && p[i] && tolower((unsigned char)p[i])==tolower((unsigned char)needle[i]))i++;
        if(i==n)return 1;
    }
    return 0;
}
static int http_status(const char *head) {
    const char *sp=strchr(head,' ');
    return sp?atoi(sp+1):0;
}
static unsigned interval_from_hello(const unsigned char *p, size_t n) {
    const char marker[]="\"heartbeat_interval\"";
    for(size_t i=0;i+sizeof(marker)-1<n;i++) {
        if(memcmp(p+i,marker,sizeof(marker)-1)!=0)continue;
        size_t k=i+sizeof(marker)-1;
        while(k<n&&(p[k]==' '||p[k]==':'||p[k]=='\t'))k++;
        unsigned result=0, digits=0;
        while(k<n&&p[k]>='0'&&p[k]<='9'&&digits<7) {
            result=result*10+(unsigned)(p[k++]-'0');digits++;
        }
        if(digits&&result>=1000&&result<1000000)return result;
    }
    return 0;
}
/* Return 1 if a complete server HELLO text frame appears. Receive no
 * authentication messages and send no Identify, token, heartbeat or voice. */
static int ws_hello_from_bytes(const unsigned char *buf,size_t size,unsigned *interval) {
    size_t off=0;
    while(off+2<=size) {
        uint8_t op=buf[off]&15;
        uint8_t len7=buf[off+1]&127;
        if(buf[off+1]&0x80)return 0; /* servers must not mask */
        size_t h=2,payload=0;
        if(len7<126)payload=len7;
        else if(len7==126) {
            if(off+4>size)return 0;
            payload=((size_t)buf[off+2]<<8)|buf[off+3];h=4;
        } else return 0; /* gateway HELLO should be small */
        if(off+h+payload>size)return 0;
        if(op==1) {
            const unsigned char *text=buf+off+h;
            /* Avoid relying on JSON field order. */
            if(payload>0 && memchr(text,'{',payload) &&
               interval_from_hello(text,payload)) {
                *interval=interval_from_hello(text,payload);
                return 1;
            }
        }
        off+=h+payload;
    }
    return 0;
}
static void b64_16(const uint8_t bytes[16],char out[25]) {
    static const char *abc="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned j=0;
    for(unsigned i=0;i<15;i+=3) {
        unsigned x=((unsigned)bytes[i]<<16)|((unsigned)bytes[i+1]<<8)|bytes[i+2];
        out[j++]=abc[(x>>18)&63];out[j++]=abc[(x>>12)&63];
        out[j++]=abc[(x>>6)&63];out[j++]=abc[x&63];
    }
    out[j++]=abc[(bytes[15]>>2)&63];
    out[j++]=abc[(bytes[15]&3)<<4];
    out[j++]='=';out[j++]='=';out[j]=0;
}
static int send_all_ssl(SslConnection *tls,const char *str) {
    size_t n=strlen(str),off=0;
    while(off<n) {
        u32 written=0;
        Result rc=sslConnectionWrite(tls,str+off,(u32)(n-off),&written);
        if(R_FAILED(rc)||written==0)return 0;
        off+=written;
    }
    return 1;
}
static int connect_socket(Dnx2LogFn log, Dnx2DiscordProbe *state) {
    struct addrinfo hints={0},*addresses=NULL;
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    int g=getaddrinfo(D_HOST,D_PORT,&hints,&addresses);
    log("discord DNS host=%s result=%d\n",D_HOST,g);
    if(g!=0||!addresses)return -1;
    int sock=-1;
    for(struct addrinfo *ai=addresses;ai;ai=ai->ai_next) {
        sock=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
        if(sock<0)continue;
        struct timeval io={5,0};
        setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&io,sizeof(io));
        setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,&io,sizeof(io));
        if(connect(sock,ai->ai_addr,ai->ai_addrlen)==0) {
            state->tcp_ok=1;break;
        }
        close(sock);sock=-1;
    }
    freeaddrinfo(addresses);
    log("discord TCP port443 ok=%u errno=%d\n",state->tcp_ok,errno);
    return sock;
}
Dnx2DiscordProbe dnx2DiscordGatewayProbe(Dnx2LogFn log) {
    Dnx2DiscordProbe state={0};
    log("\n[DISCORD: PUBLIC TLS + WEBSOCKET HELLO (NO LOGIN)]\n");
    Result rc=socketInitialize(NULL);
    log("discord socketInitialize=%08x\n",rc);
    if(R_FAILED(rc)) {state.last_result=rc;return state;}
    int sock=connect_socket(log,&state);
    if(sock<0) {state.last_errno=(unsigned)errno;socketExit();return state;}
    rc=sslInitialize(1);
    log("discord sslInitialize=%08x\n",rc);
    if(R_FAILED(rc))goto cleanup_sock;
    SslContext ctx={0};
    SslConnection conn={0};
    rc=sslCreateContext(&ctx,SslVersion_TlsV12|SslVersion_TlsV13);
    log("discord sslCreateContext=%08x\n",rc);
    if(R_FAILED(rc))goto cleanup_ssl;
    rc=sslContextCreateConnection(&ctx,&conn);
    log("discord sslCreateConnection=%08x\n",rc);
    if(R_FAILED(rc))goto cleanup_context;
    rc=sslConnectionSetHostName(&conn,D_HOST,(u32)strlen(D_HOST)+1);
    log("discord TLS setHostName=%08x\n",rc);
    if(R_FAILED(rc))goto cleanup_conn;
    /* libounce BSD socket wrapper may return ENOENT when the SSL service
     * consumes ownership without returning an extra public socket. */
    errno=0;
    int passed=socketSslConnectionSetSocketDescriptor(&conn,sock);
    log("discord TLS socketAttach=%d errno=%d\n",passed,errno);
    if(passed<0&&errno!=ENOENT)goto cleanup_conn;
    if(passed>=0)close(passed);
    u32 certbytes=0,certcount=0;
    rc=sslConnectionDoHandshake(&conn,&certbytes,&certcount,NULL,0);
    log("discord TLS handshake=%08x certs=%u\n",rc,certcount);
    if(R_FAILED(rc))goto cleanup_conn;
    state.tls_ok=1;
    /* Use the public Gateway without an authorization header. */
    uint8_t random[16]={0};
    rc=csrngInitialize();
    if(R_FAILED(rc)) {
        log("discord CSRNG unavailable=%08x\n",rc);goto cleanup_conn;
    }
    rc=csrngGetRandomBytes(random,sizeof(random));
    csrngExit();
    if(R_FAILED(rc)) {
        log("discord CSRNG bytes unavailable=%08x\n",rc);goto cleanup_conn;
    }
    char key[25];b64_16(random,key);
    char request[512];
    snprintf(request,sizeof(request),
      "GET /?v=10&encoding=json HTTP/1.1\r\n"
      "Host: " D_HOST "\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: %s\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "User-Agent: DiscordNX2-native-probe/0.17\r\n"
      "\r\n",key);
    if(!send_all_ssl(&conn,request)) {
        log("discord WSS upgrade request write failed\n");goto cleanup_conn;
    }
    size_t bytes=0,header_end=0;
    /* Header (HTTP 101), then a small public gateway HELLO WebSocket frame.
     * Bound attempts and read size; no credential leakage into diagnostics. */
    for(unsigned tries=0;tries<12 && bytes<sizeof(rxbuf);tries++) {
        u32 read_size=0;
        rc=sslConnectionRead(&conn,rxbuf+bytes,(u32)(sizeof(rxbuf)-bytes),&read_size);
        if(R_FAILED(rc)||read_size==0) {
            log("discord WSS read rc=%08x size=%u tries=%u\n",rc,read_size,tries);
            break;
        }
        bytes+=read_size;
        if(!header_end) {
            for(size_t i=0;i+3<bytes;i++)
                if(rxbuf[i]=='\r'&&rxbuf[i+1]=='\n'&&rxbuf[i+2]=='\r'&&rxbuf[i+3]=='\n'){
                    header_end=i+4;break;
                }
            if(header_end) {
                size_t n=header_end<sizeof(httpheader)-1?header_end:sizeof(httpheader)-1;
                memcpy(httpheader,rxbuf,n);httpheader[n]=0;
                state.http_status=(unsigned)http_status(httpheader);
                state.http_ok=(state.http_status==101);
                state.websocket_ok=state.http_ok&&ci_has(httpheader,"upgrade: websocket");
                log("discord gateway upgrade HTTP=%u websocket=%u\n",
                    state.http_status,state.websocket_ok);
                if(!state.websocket_ok)break;
            }
        }
        if(header_end && state.websocket_ok) {
            unsigned interval=0;
            if(ws_hello_from_bytes(rxbuf+header_end,bytes-header_end,&interval)) {
                state.hello_ok=1;state.hello_interval_ms=interval;
                log("discord Gateway HELLO received heartbeat_interval_ms=%u\n",interval);
                break;
            }
        }
    }
    log("discord summary tcp=%u tls=%u websocket=%u hello=%u bytes=%u\n",
        state.tcp_ok,state.tls_ok,state.websocket_ok,state.hello_ok,(unsigned)bytes);
    log("discord probe sends no user token, bot token, identify, or audio.\n");
cleanup_conn:
    sslConnectionClose(&conn);
cleanup_context:
    sslContextClose(&ctx);
cleanup_ssl:
    sslExit();
cleanup_sock:
    close(sock);
    socketExit();
    return state;
}
