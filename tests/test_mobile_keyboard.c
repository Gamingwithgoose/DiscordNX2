// SPDX-License-Identifier: MIT
#include "mobile_keyboard.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    DnxKey keys[40];int n=dnx_keys(keys,0,0);assert(n<=40&&!strcmp(keys[0].label,"q")&&!strcmp(keys[9].label,"p"));
    for(int page=0;page<3;page++){
        n=dnx_keys(keys,page,0);
        for(int i=0;i<n;i++){
            DnxKey *k=&keys[i];assert(k->x>=0&&k->y>=418&&k->x+k->w<=1280&&k->y+k->h<=720&&k->h>=50&&k->w>=70);
            assert(dnx_key_hit(keys,n,k->x+k->w/2,k->y+k->h/2)==i);
            if(!page&&k->action==DNX_KEY_TEXT)assert(!(k->label[0]>='0'&&k->label[0]<='9'));
            for(int j=0;j<i;j++)assert(k->x>=keys[j].x+keys[j].w||k->x+k->w<=keys[j].x||k->y>=keys[j].y+keys[j].h||k->y+k->h<=keys[j].y);
        }
    }
    n=dnx_keys(keys,0,1);assert(!strcmp(keys[0].label,"Q"));assert(dnx_key_move(keys,n,0,1,0)==1);assert(dnx_key_move(keys,n,0,0,1)>=10);
    puts("PASS phone keyboard: QWERTY order, separate number/symbol pages, large nonoverlapping touch keys and controller navigation");
}
