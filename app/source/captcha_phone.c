// SPDX-License-Identifier: MIT
// Direct LAN result delivery. Discord tickets/account tokens never leave this NRO.
#include "captcha.h"
#include "captcha_phone_wire.h"
#include "auth_crypto.h"
#include <switch.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef DNX2_PHONE_CAPTCHA_PAGE_URL
#define DNX2_PHONE_CAPTCHA_PAGE_URL "https://mmonsterpatch.gamingwithgoose.com/discordnx2-phone.html"
#endif
// Opt-in only while the normal public-jailbreak, on-console route is tested.
#ifndef DNX2_ENABLE_PHONE_HANDOFF
#define DNX2_ENABLE_PHONE_HANDOFF 0
#endif
static const char *failure="Verification was not completed.";
static unsigned phone_error;
unsigned dnx2CaptchaError(void){return phone_error;}
const char *dnx2CaptchaFailure(void){return failure;}
static uint64_t now_ms(void) {
    struct timespec t={0};clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
}
static int encode(const char *s,char *out,size_t cap) {
    static const char hex[]="0123456789ABCDEF";size_t j=0;
    for(size_t i=0;s[i];i++) {
        unsigned char c=(unsigned char)s[i];
        int plain=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~';
        if(j+(plain?1:3)>=cap)return 0;
        if(plain)out[j++]=(char)c;else {out[j++]='%';out[j++]=hex[c>>4];out[j++]=hex[c&15];}
    }
    out[j]=0;return 1;
}
static int nonblock(int fd) {
    int flags=fcntl(fd,F_GETFL,0);return flags>=0&&fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0;
}
static void reply(int fd,int accepted) {
    const char *body=accepted?
        "<!doctype html><meta name=viewport content='width=device-width'><title>Sent to Switch</title><p>Encrypted verification received. Check your Switch for the login result. You can close this page.</p>":
        "<!doctype html><title>Not accepted</title><p>This verification was not accepted. Check your Switch and start a new login if needed.</p>";
    char response[800];int n=snprintf(response,sizeof(response),
        "HTTP/1.1 %s\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %u\r\nCache-Control: no-store\r\nContent-Security-Policy: default-src 'none'\r\nReferrer-Policy: no-referrer\r\nConnection: close\r\n\r\n%s",
        accepted?"200 OK":"400 Bad Request",(unsigned)strlen(body),body);
    if(n<=0||(size_t)n>=sizeof(response))return;
    size_t sent=0;uint64_t until=now_ms()+750;
    while(sent<(size_t)n&&now_ms()<until) {
        ssize_t size=send(fd,response+sent,(size_t)n-sent,0);
        if(size>0)sent+=(size_t)size;
        else if(size<0&&(errno==EWOULDBLOCK||errno==EAGAIN||errno==EINTR))svcSleepThread(10000000);
        else break;
    }
}
typedef struct {
    char site[288],data[12288],pubkey[900],pub[2700],back[256];
    char url[17000],request[DNX2_PHONE_REQUEST_MAX+1],state[65];
    unsigned char random[32];
} PhoneSession;
int dnx2CaptchaShow(const Dnx2Captcha *c,char *solution,size_t cap,unsigned local_ip,
                   Dnx2CaptchaProgress progress,Dnx2CaptchaCancel cancel) {
    phone_error=1;failure="Verification was not completed.";
    if(!DNX2_ENABLE_PHONE_HANDOFF) {
        failure="On-console CAPTCHA is not ready yet. Browser testing is in progress.";
        if(solution&&cap)dnx2CaptchaWipe(solution,cap);
        return 0;
    }
    if(!solution||!cap)return 0;
    solution[0]=0;
    if(!c||strcmp(c->service,"hcaptcha")||!progress||!dnx2CaptchaPageUrl(DNX2_PHONE_CAPTCHA_PAGE_URL))return 0;
    PhoneSession *s=calloc(1,sizeof(*s));int listener=-1,client=-1,ok=0;
    if(!s){failure="Not enough memory for phone verification.";return 0;}
    // Use the local address of the already-working Discord socket. No extra
    // network-management permissions or guessed subnet mask are needed.
    u32 ip=local_ip;
    if(!dnx2PhonePrivateIp(ntohl(ip))) {
        failure="Phone verification needs a private Wi-Fi IPv4 connection.";goto done;
    }
    Result rc=csrngInitialize();
    if(R_FAILED(rc)){failure="Secure phone verification could not start.";phone_error=rc;goto done;}
    rc=csrngGetRandomBytes(s->random,sizeof(s->random));csrngExit();
    if(R_FAILED(rc)){failure="Secure phone verification could not start.";phone_error=rc;goto done;}
    for(size_t i=0;i<32;i++)snprintf(s->state+i*2,3,"%02x",s->random[i]);
    listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(listener<0||!nonblock(listener)){failure="Could not open the local phone connection.";goto done;}
    struct sockaddr_in local={0};local.sin_family=AF_INET;local.sin_addr.s_addr=ip;
    if(bind(listener,(struct sockaddr*)&local,sizeof(local))||listen(listener,2)) {
        failure="Could not listen for phone verification.";goto done;
    }
    socklen_t local_size=sizeof(local);
    if(getsockname(listener,(struct sockaddr*)&local,&local_size)){failure="Could not read the local phone address.";goto done;}
    char destination[128];
    snprintf(destination,sizeof(destination),"http://%s:%u/captcha-return",inet_ntoa(local.sin_addr),ntohs(local.sin_port));
    if(!dnx2AuthCryptoPublicKey(s->pubkey,sizeof(s->pubkey))||
       !encode(c->sitekey,s->site,sizeof(s->site))||!encode(c->rqdata,s->data,sizeof(s->data))||
       !encode(s->pubkey,s->pub,sizeof(s->pub))||!encode(destination,s->back,sizeof(s->back))){failure="Phone verification request could not be encoded.";goto done;}
    int count=snprintf(s->url,sizeof(s->url),"%s?page=35#v=1&limit=4095&state=%s&sitekey=%s&rqdata=%s&pub=%s&return=%s",
        DNX2_PHONE_CAPTCHA_PAGE_URL,s->state,s->site,s->data,s->pub,s->back);
    if(count<=0||(size_t)count>=sizeof(s->url)||count>2200) {
        failure="Challenge too large for a phone QR. No data was truncated.";goto done;
    }
    progress("SCAN VERIFICATION QR WITH YOUR PHONE CAMERA",s->url);
    const uint64_t deadline=now_ms()+150000;
    unsigned invalid=0;
    while(now_ms()<deadline&&invalid<8&&appletMainLoop()) {
        if(cancel&&cancel()){failure="Phone verification canceled.";goto done;}
        struct sockaddr_in peer={0};socklen_t peer_size=sizeof(peer);
        client=accept(listener,(struct sockaddr*)&peer,&peer_size);
        if(client<0) {
            if(errno!=EWOULDBLOCK&&errno!=EAGAIN&&errno!=EINTR){failure="Local phone connection stopped.";goto done;}
            svcSleepThread(16000000);continue;
        }
        if(!dnx2PhonePrivateIp(ntohl(peer.sin_addr.s_addr))||!nonblock(client)) {close(client);client=-1;continue;}
        size_t used=0;uint64_t receive_until=now_ms()+3000;
        if(receive_until>deadline)receive_until=deadline;
        const char *payload=NULL;size_t length=0;int parsed=0;
        while(now_ms()<receive_until&&used<DNX2_PHONE_REQUEST_MAX) {
            if(cancel&&cancel()){failure="Phone verification canceled.";goto done;}
            ssize_t n=recv(client,s->request+used,DNX2_PHONE_REQUEST_MAX-used,0);
            if(n>0) {
                used+=(size_t)n;s->request[used]=0;
                parsed=dnx2PhoneRequest(s->request,used,&payload,&length);
                if(parsed)break;
            }else if(n==0)break;
            else if(errno!=EWOULDBLOCK&&errno!=EAGAIN&&errno!=EINTR)break;
            svcSleepThread(10000000);
        }
        // The state-bound AES-GCM envelope is the authorization, not Origin or IP.
        // Browser navigation may send Origin:null when moving HTTPS to local HTTP.
        progress(parsed==1?"PHONE RESPONSE RECEIVED - CHECKING ENCRYPTION":"PHONE RESPONSE WAS INVALID",s->url);
        ok=parsed==1&&dnx2PhoneDecrypt(payload,length,s->state,solution,cap);
        reply(client,ok);close(client);client=-1;
        dnx2CaptchaWipe(s->request,sizeof(s->request));
        if(ok){progress("ENCRYPTED PHONE VERIFICATION RECEIVED",NULL);goto done;}
        invalid++;
    }
    failure=invalid>=8?"Too many invalid phone responses. Start a new login.":
        "Phone verification timed out. Use the same Wi-Fi and try again.";
done:
    if(client>=0)close(client);
    if(listener>=0)close(listener);
    dnx2CaptchaWipe(s,sizeof(*s));free(s);
    if(ok)phone_error=0;
    else dnx2CaptchaWipe(solution,cap);
    return ok;
}
