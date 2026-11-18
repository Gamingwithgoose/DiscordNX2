// SPDX-License-Identifier: MIT
#include "chat_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    DnxHit h=dnx_chat_hit(410,650,0,0,0,0,1);assert(h.kind==DNX_INPUT);
    h=dnx_chat_hit(1210,650,0,0,0,0,1);assert(h.kind==DNX_SEND);
    h=dnx_chat_hit(180,144,0,9,0,0,1);assert(h.kind==DNX_CHANNEL&&h.index==10);
    h=dnx_chat_hit(32,160,2,0,0,0,1);assert(h.kind==DNX_SERVER&&h.index==17);
    h=dnx_chat_hit(420,240,0,0,8,0,1);assert(h.kind==DNX_MESSAGE&&h.index==9);
    h=dnx_chat_hit(420,280,0,0,0,1,0);assert(h.kind==DNX_LOGIN);
    h=dnx_chat_hit(420,380,0,0,0,1,0);assert(h.kind==DNX_REMEMBER);
    h=dnx_chat_hit(1200,600,0,0,0,0,1);assert(h.kind==DNX_FALLBACK);
    h=dnx_chat_hit(1280,650,0,0,0,0,1);assert(!h.kind);
    h=dnx_chat_hit(1200,650,0,0,0,0,0);assert(!h.kind);
    puts("PASS shared touch/layout geometry: input, Send, channels, servers, message anchors, account and bounds");
}
