// SPDX-License-Identifier: MIT
/* Explicitly user-confirmed LAN metadata transfer from a PC that has already
 * completed Discord OAuth PKCE. This does NOT obtain or transfer account tokens.
 * Discord voice access and Discord API access from NRO are NOT granted.
 */
#include "discord_lan_pair.h"
#include <switch.h>
#include <switch/services/bsd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#define DNX2_PAIR_PORT 39274
#define DNX2_HTTP_PORT 39275
static Dnx2PairProfile profile;
static char inbound[32768];

const Dnx2PairProfile *dnx2PairProfile(void){return &profile;}
void dnx2PairSelect(int index) {
    if(index>=0&&index<profile.count)profile.selected=index;
}
static int read_json_string(const char *start,const char *end,
                            const char *key,char *out,size_t cap) {
    if(!start||!end||start>=end||!key||!out||cap==0)return 0;
    char label[64];
    snprintf(label,sizeof(label),"\"%s\"",key);
    const char *p=start;
    for(;;) {
        p=strstr(p,label);
        if(!p||p>=end)return 0;
        p+=strlen(label);
        while(p<end&&(*p==' '||*p=='\t'))p++;
        if(p>=end||*p!=':')continue;
        p++;
        while(p<end&&(*p==' '||*p=='\t'))p++;
        if(p>=end||*p!='"')continue;
        p++;
        size_t n=0;
        while(p<end&&*p!='"') {
            unsigned char ch=(unsigned char)*p++;
            if(ch=='\\') {
                if(p>=end)break;
                ch=(unsigned char)*p++;
                if(ch=='u') {
                    if(end-p<4)break;
                    p+=4;ch='?'; /* ASCII fallback in embedded 5x7 font. */
                } else if(ch=='n'||ch=='r'||ch=='t')ch=' ';
                else if(ch!='"'&&ch!='\\'&&ch!='/')ch='?';
            }
            if(ch<32||ch>126)ch='?';
            if(n+1<cap)out[n++]=(char)ch;
        }
        out[n]=0;
        return p<end;
    }
}
static int validate_id(const char *id) {
    size_t n=strlen(id);
    if(n<15||n>22)return 0;
    for(size_t i=0;i<n;i++)if(id[i]<'0'||id[i]>'9')return 0;
    return 1;
}
static int parse_profile(const char *body,size_t bytes) {
    if(bytes==0||bytes>32000||strncmp(body,"{",1))return 0;
    if(!strstr(body,"\"protocol\":\"dnx2-pair-v1\""))return 0;
    Dnx2PairProfile tmp={0};
    const char *end=body+bytes;
    if(!read_json_string(body,end,"username",tmp.username,sizeof(tmp.username)))return 0;
    if(!read_json_string(body,end,"display_name",tmp.display_name,sizeof(tmp.display_name)))return 0;
    const char *list=strstr(body,"\"guilds\"");
    if(!list)return 0;
    list=strchr(list,'[');if(!list)return 0;
    while(tmp.count<DNX2_MAX_GUILDS) {
        const char *left=strchr(list,'{');if(!left)break;
        const char *stop=strchr(list,']');
        if(!stop||stop<left)break;
        const char *right=strchr(left,'}');
        if(!right||right>=stop)break;
        Dnx2Guild *guild=&tmp.guilds[tmp.count];
        if(!read_json_string(left,right,"id",guild->id,sizeof(guild->id)) ||
           !read_json_string(left,right,"name",guild->name,sizeof(guild->name)))return 0;
        if(!validate_id(guild->id))return 0;
        tmp.count++;
        list=right+1;
    }
    tmp.paired=1;
    tmp.selected=tmp.count?0:-1;
    profile=tmp;
    return 1;
}
static void msg(char *out,size_t cap,const char *s) {
    if(cap)snprintf(out,cap,"%s",s);
}
int dnx2PairReceive(unsigned code,char *message,size_t message_size) {
    msg(message,message_size,"STARTING LOCAL NETWORK...");
    if(code>999999)return 0;
    Result rc=socketInitialize(NULL);
    if(R_FAILED(rc)) {msg(message,message_size,"NETWORK SERVICE UNAVAILABLE");return 0;}
    int udp=socket(AF_INET,SOCK_DGRAM,0),tcp=-1;
    if(udp<0){msg(message,message_size,"UDP SOCKET FAILED");goto finish;}
    struct sockaddr_in here={0};
    here.sin_family=AF_INET;
    here.sin_port=htons(DNX2_PAIR_PORT);
    here.sin_addr.s_addr=htonl(INADDR_ANY);
    int reuse=1;setsockopt(udp,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    if(bind(udp,(struct sockaddr*)&here,sizeof(here))!=0) {
        msg(message,message_size,"DISCOVERY PORT BUSY");goto finish;
    }
    /* PC advertises presence only, never profile data. Limit to 15 seconds. */
    struct sockaddr_in peer={0};socklen_t peerlen=sizeof(peer);
    int found=0;
    for(int i=0;i<15;i++) {
        fd_set fds;FD_ZERO(&fds);FD_SET(udp,&fds);
        struct timeval timeout={1,0};
        if(select(udp+1,&fds,NULL,NULL,&timeout)<=0)continue;
        char beacon[48]={0};
        int received=(int)recvfrom(udp,beacon,sizeof(beacon)-1,0,
                                    (struct sockaddr*)&peer,&peerlen);
        if(received==18 && memcmp(beacon,"DNX2_PAIR_V1:39275",18)==0) {
            found=1;break;
        }
    }
    if(!found){msg(message,message_size,"NO PC BRIDGE FOUND ON LOCAL NETWORK");goto finish;}
    tcp=socket(AF_INET,SOCK_STREAM,0);
    if(tcp<0){msg(message,message_size,"TCP SOCKET FAILED");goto finish;}
    struct timeval timeout={55,0};
    setsockopt(tcp,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    timeout.tv_sec=5;
    setsockopt(tcp,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    peer.sin_port=htons(DNX2_HTTP_PORT);
    if(connect(tcp,(struct sockaddr*)&peer,sizeof(peer))!=0) {
        msg(message,message_size,"CANNOT REACH PC BRIDGE - CHECK FIREWALL");goto finish;
    }
    char req[256];
    int req_len=snprintf(req,sizeof(req),
      "GET /pair?code=%06u HTTP/1.1\r\n"
      "Host: discordnx2.local\r\n"
      "Connection: close\r\n"
      "Accept: application/json\r\n\r\n",code);
    if(req_len<0||(size_t)req_len>=sizeof(req))goto finish;
    size_t sent=0;
    while(sent<(size_t)req_len) {
        ssize_t n=send(tcp,req+sent,(size_t)req_len-sent,0);
        if(n<=0){msg(message,message_size,"PAIR REQUEST FAILED");goto finish;}
        sent+=(size_t)n;
    }
    size_t used=0;
    while(used<sizeof(inbound)-1) {
        ssize_t n=recv(tcp,inbound+used,sizeof(inbound)-1-used,0);
        if(n<=0)break;
        used+=(size_t)n;
    }
    inbound[used]=0;
    if(strncmp(inbound,"HTTP/1.1 200 ",13)!=0 &&
       strncmp(inbound,"HTTP/1.0 200 ",13)!=0) {
        msg(message,message_size,"PAIRING DENIED OR EXPIRED ON PC");goto finish;
    }
    char *body=strstr(inbound,"\r\n\r\n");
    if(!body){msg(message,message_size,"INVALID BRIDGE RESPONSE");goto finish;}
    body+=4;
    if(!parse_profile(body,used-(size_t)(body-inbound))) {
        msg(message,message_size,"INVALID PROFILE RESPONSE");goto finish;
    }
    msg(message,message_size,"SIGNED-IN PROFILE LOADED - VOICE NOT ENABLED");
    if(tcp>=0)close(tcp);
    if(udp>=0)close(udp);
    socketExit();
    return 1;
finish:
    if(tcp>=0)close(tcp);
    if(udp>=0)close(udp);
    socketExit();
    return 0;
}
