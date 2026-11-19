// SPDX-License-Identifier: MIT
#ifndef DNX2_NATIVE_WS_H
#define DNX2_NATIVE_WS_H
#include <stdint.h>
#include <string.h>
#ifndef DNX2_WS_MAX
#define DNX2_WS_MAX 8192
#endif
typedef struct {
    unsigned char wire[DNX2_WS_MAX+14],message[DNX2_WS_MAX+1];
    size_t used,message_used,control_size;
    int fragmented,allow_binary,message_type,is_binary;size_t completed_size;unsigned close_code;
} Dnx2WsReader;
/* 0 incomplete/continuation, 1 text, 2 ping, 3 pong, 4 binary (opt-in), -1 invalid, -2 close.
 * Consume one complete frame; preserve both partial frames and fragments. */
static int dnx2WsNext(Dnx2WsReader *r,unsigned char *out,size_t cap){
    if(r->used<2)return 0;
    unsigned char *p=r->wire;unsigned op=p[0]&15,fin=p[0]&128;
    if((p[0]&112)||(p[1]&128))return -1;
    uint64_t n=p[1]&127;size_t h=2;
    if(n==126){if(r->used<4)return 0;n=((unsigned)p[2]<<8)|p[3];h=4;}
    else if(n==127){if(r->used<10)return 0;n=0;for(int i=2;i<10;i++)n=(n<<8)|p[i];h=10;}
    if(n>DNX2_WS_MAX || (op>=8 && (!fin||n>125)))return -1;
    if(r->used<h+(size_t)n)return 0;
    int result=0;
    if(op==8){r->close_code=n>=2?((unsigned)p[h]<<8)|p[h+1]:0;result=-2;}
    else if(op==9||op==10){
        if(n>=cap)return -1;
        memcpy(out,p+h,(size_t)n);out[n]=0;r->control_size=(size_t)n;result=op==9?2:3;
    }else{
        if(op!=0 && op!=1 && !(op==2&&r->allow_binary))return -1;
        if((op==0&&!r->fragmented)||(op!=0&&r->fragmented))return -1;
        if(op)r->message_type=op;
        if(n>DNX2_WS_MAX-r->message_used)return -1;
        memcpy(r->message+r->message_used,p+h,(size_t)n);r->message_used+=(size_t)n;
        r->fragmented=!fin;
        if(fin){
            if(r->message_used>=cap)return -1;
            memcpy(out,r->message,r->message_used);out[r->message_used]=0;
            r->completed_size=r->message_used;r->is_binary=r->message_type==2;
            memset(r->message,0,r->message_used);r->message_used=0;result=r->is_binary?4:1;
        }
    }
    r->used-=h+(size_t)n;memmove(r->wire,r->wire+h+(size_t)n,r->used);
    return result;
}
#endif
