// SPDX-License-Identifier: MIT
#ifndef DNX2_MOBILE_KEYBOARD_H
#define DNX2_MOBILE_KEYBOARD_H
#include <string.h>
enum {DNX_KEY_TEXT,DNX_KEY_SHIFT,DNX_KEY_DELETE,DNX_KEY_PAGE,DNX_KEY_EMOJI,DNX_KEY_SPACE,DNX_KEY_DONE};
typedef struct {int x,y,w,h,action;char label[12];} DnxKey;
static inline void dnx_key_add(DnxKey *keys,int *count,int x,int y,int w,const char *label,int action){DnxKey *k=&keys[(*count)++];k->x=x;k->y=y;k->w=w;k->h=54;k->action=action;size_t n=strlen(label);if(n>=sizeof(k->label))n=sizeof(k->label)-1;memcpy(k->label,label,n);k->label[n]=0;}
static inline int dnx_keys(DnxKey keys[40],int page,int upper){
    int n=0;const char *rows[2];
    rows[0]=page==0?"qwertyuiop":page==1?"1234567890":"[]{}#%^*+=";
    rows[1]=page==0?"asdfghjkl":page==1?"@#$%&*-+()":"_\\|~<>$&@";
    for(int row=0;row<2;row++){int count=(int)strlen(rows[row]),x=(1280-count*116+6)/2;
        for(int i=0;i<count;i++){char c[2]={rows[row][i],0};if(!page&&upper&&c[0]>='a'&&c[0]<='z')c[0]-=32;dnx_key_add(keys,&n,x+i*116,430+row*60,110,c,DNX_KEY_TEXT);}}
    dnx_key_add(keys,&n,61,550,110,page?"#+=":"\xe2\x87\xa7",DNX_KEY_SHIFT);
    const char *third=page==0?"zxcvbnm":page==1?".,?!'\":":";:!?.,/";
    for(int i=0;i<7;i++){char c[2]={third[i],0};if(!page&&upper&&c[0]>='a'&&c[0]<='z')c[0]-=32;dnx_key_add(keys,&n,177+i*116,550,110,c,DNX_KEY_TEXT);}
    dnx_key_add(keys,&n,989,550,230,"\xe2\x8c\xab",DNX_KEY_DELETE);
    dnx_key_add(keys,&n,36,612,136,page?"ABC":"?123",DNX_KEY_PAGE);
    dnx_key_add(keys,&n,184,612,88,"\xe2\x98\xba",DNX_KEY_EMOJI);
    dnx_key_add(keys,&n,284,612,76,",",DNX_KEY_TEXT);
    dnx_key_add(keys,&n,372,612,516,"space",DNX_KEY_SPACE);
    dnx_key_add(keys,&n,900,612,76,".",DNX_KEY_TEXT);
    dnx_key_add(keys,&n,988,612,256,"Done",DNX_KEY_DONE);return n;
}
static inline int dnx_key_hit(const DnxKey *keys,int count,int x,int y){for(int i=0;i<count;i++)if(x>=keys[i].x&&x<keys[i].x+keys[i].w&&y>=keys[i].y&&y<keys[i].y+keys[i].h)return i;return -1;}
static inline int dnx_key_move(const DnxKey *keys,int count,int from,int dx,int dy){
    int best=from,best_score=2147483647,x=keys[from].x+keys[from].w/2,y=keys[from].y;
    for(int i=0;i<count;i++){int xx=keys[i].x+keys[i].w/2-x,yy=keys[i].y-y;if((dx&&xx*dx<=0)||(dy&&yy*dy<=0))continue;
        int score=xx*xx+yy*yy;if(dx)score+=yy*yy*8;else score+=xx*xx*4;if(score<best_score){best=i;best_score=score;}}
    return best;
}
#endif
