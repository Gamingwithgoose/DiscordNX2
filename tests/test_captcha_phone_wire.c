// SPDX-License-Identifier: MIT
#include "captcha_phone_wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int parse(const char *s) {const char*p;size_t n;return dnx2PhoneRequest(s,strlen(s),&p,&n);}
int main(void) {
    const char *request="POST /captcha-return HTTP/1.1\r\nHost: 192.168.1.2:12345\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 12\r\n\r\npayload=Ab-_";
    const char *p;size_t n;
    for(size_t i=0;i<strlen(request);i++)assert(dnx2PhoneRequest(request,i,&p,&n)==0);
    assert(dnx2PhoneRequest(request,strlen(request),&p,&n)==1&&n==4&&!memcmp(p,"Ab-_",4));
    assert(parse("POST /captcha-return HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 12\r\nContent-Length: 12\r\n\r\npayload=Ab-_")==-1);
    assert(parse("POST /captcha-return HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nTransfer-Encoding: chunked\r\nContent-Length: 12\r\n\r\npayload=Ab-_")==-1);
    assert(parse("POST /captcha-return HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 13\r\n\r\npayload=A&b-_")==-1);
    assert(parse("POST /captcha-return HTTP/1.1\r\nContent-Type: application/json\r\nContent-Length: 12\r\n\r\npayload=Ab-_")==-1);
    assert(parse("GET /captcha-return HTTP/1.1\r\n\r\n")==-1);
    assert(parse("POST /captcha-return HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 99999999999999999999\r\n\r\n")==-1);
    char extra[1024];snprintf(extra,sizeof(extra),"%sX",request);assert(parse(extra)==-1);
    assert(dnx2PhonePrivateIp(0x0a000001)&&dnx2PhonePrivateIp(0xac100001)&&dnx2PhonePrivateIp(0xac1fffff)&&dnx2PhonePrivateIp(0xc0a80101));
    assert(!dnx2PhonePrivateIp(0x7f000001)&&!dnx2PhonePrivateIp(0x08080808)&&!dnx2PhonePrivateIp(0xac200001)&&!dnx2PhonePrivateIp(0));
    puts("PASS phone HTTP: partial input, bounded framing, duplicate length/transfer encoding/injection rejection, private IPv4");
}
