// SPDX-License-Identifier: MIT
#ifndef DNX2_CHAT_LAYOUT_H
#define DNX2_CHAT_LAYOUT_H
/* Shared drawing/touch geometry, in the configured 1280x720 touch range. */
enum {DNX_HOME=1,DNX_SERVER,DNX_CHANNEL,DNX_INPUT,DNX_SEND,DNX_MESSAGE,DNX_PREVIOUS,DNX_NEXT,DNX_LOGIN,DNX_REMEMBER,DNX_EXIT,DNX_FALLBACK};
typedef struct {int kind,index;} DnxHit;
static inline DnxHit dnx_chat_hit(int x,int y,int server_page,int channel_first,int message_first,int home,int text){
    DnxHit hit={0,0};
    if(x<0||x>=1280||y<0||y>=720)return hit;
    if(x<88){
        if(y<88)hit.kind=DNX_HOME;
        else if(y>=98&&y<594){hit.kind=DNX_SERVER;hit.index=server_page*8+(y-98)/62;}
        else if(y>=600&&y<640)hit.kind=x<44?DNX_PREVIOUS:DNX_NEXT;
        else if(y>=650)hit.kind=DNX_EXIT;
    }else if(home){
        if(x>=392&&x<1216&&y>=254&&y<344)hit.kind=DNX_LOGIN;
        else if(x>=392&&x<1216&&y>=366&&y<456)hit.kind=DNX_REMEMBER;
    }else if(x<356){
        if(y>=100&&y<628){hit.kind=DNX_CHANNEL;hit.index=channel_first+(y-100)/44;}
    }else if(text){
        if(x>=1080&&y>=590&&y<620){hit.kind=DNX_FALLBACK;}
        else if(y>=626&&y<684){hit.kind=x>=1150?DNX_SEND:DNX_INPUT;}
        else if(y>=100&&y<588){hit.kind=DNX_MESSAGE;hit.index=message_first+(y-100)/122;}
    }
    return hit;
}
#endif
