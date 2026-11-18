// SPDX-License-Identifier: MIT
/* Switch shared standard font, rasterized by stb_truetype (public domain/MIT).
 * Only references console-provided font memory; does not redistribute font.
 * Public-domain font8x8 is the safe fallback when pl:u is unavailable.
 */
#include "native_font.h"
#include "pretendo_font.h"
extern char font8x8_basic[128][8];
#include <switch.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb_truetype.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef struct {
    unsigned char *bitmap;
    int xoff,yoff,w,h,advance;
} CachedGlyph;
static struct {
    stbtt_fontinfo info;
    CachedGlyph glyphs[3][128];
    int initialized,pl_live;
} font_state;
static const int heights[3]={16,24,32};
static void destroy_cache(void) {
    for(int sz=0;sz<3;sz++)for(int c=0;c<128;c++){
        free(font_state.glyphs[sz][c].bitmap);
        font_state.glyphs[sz][c].bitmap=NULL;
    }
}
int dnx2FontInit(void) {
    if(font_state.initialized)return 1;
    const unsigned char *data=pretendo_font;
    int idx=stbtt_GetFontOffsetForIndex(data,0);
    if(idx<0 || !stbtt_InitFont(&font_state.info,data,idx)) {
        if(R_FAILED(plInitialize(PlServiceType_User)))return 0;
        font_state.pl_live=1;
        PlFontData font={0};
        if(R_FAILED(plGetSharedFontByType(&font,PlSharedFontType_Standard)) ||
           !font.address || font.size<64)goto fail;
        data=(const unsigned char*)font.address;
    }
    /* Avoid parsing non-font wrappers as a TTF. */
    if(!((data[0]==0&&data[1]==1&&data[2]==0&&data[3]==0)||
         memcmp(data,"OTTO",4)==0))goto fail;
    idx=stbtt_GetFontOffsetForIndex(data,0);
    if(idx<0||!stbtt_InitFont(&font_state.info,data,idx))goto fail;
    for(int sz=0;sz<3;sz++){
        float scale=stbtt_ScaleForPixelHeight(&font_state.info,(float)heights[sz]);
        for(int code=32;code<=126;code++){
            CachedGlyph *g=&font_state.glyphs[sz][code];
            int advance=0,lsb=0;
            stbtt_GetCodepointHMetrics(&font_state.info,code,&advance,&lsb);
            (void)lsb;
            g->advance=(int)(advance*scale+0.5f);
            if(g->advance<4)g->advance=4;
            g->bitmap=stbtt_GetCodepointBitmap(&font_state.info,0,scale,code,
                                                &g->w,&g->h,&g->xoff,&g->yoff);
            /* Pretendo omits digits and some punctuation. Keep labels complete. */
            if(code!=' ' && (stbtt_FindGlyphIndex(&font_state.info,code)==0 || !g->bitmap || g->w==0)){
                free(g->bitmap);
                int xs=heights[sz]/12,ys=heights[sz]/8;
                g->w=8*xs;g->h=8*ys;g->xoff=0;g->yoff=2-heights[sz];
                g->advance=g->w+2;g->bitmap=calloc((size_t)g->w*g->h,1);
                if(g->bitmap)for(int y=0;y<g->h;y++)for(int x=0;x<g->w;x++)
                    g->bitmap[y*g->w+x]=(font8x8_basic[code][y/ys]&(1u<<(x/xs)))?255:0;
            }
        }
    }
    font_state.initialized=1;
    return 1;
fail:
    destroy_cache();
    if(font_state.pl_live)plExit();
    memset(&font_state,0,sizeof(font_state));
    return 0;
}
void dnx2FontExit(void) {
    destroy_cache();
    if(font_state.pl_live)plExit();
    memset(&font_state,0,sizeof(font_state));
}
static inline uint32_t blend(uint32_t src,uint32_t color,unsigned a){
    const unsigned ia=255-a;
    unsigned sr=(src&255),sg=((src>>8)&255),sb=((src>>16)&255);
    unsigned fr=(color&255),fg=((color>>8)&255),fb=((color>>16)&255);
    unsigned r=(sr*ia+fr*a+127)/255;
    unsigned g=(sg*ia+fg*a+127)/255;
    unsigned b=(sb*ia+fb*a+127)/255;
    return r|(g<<8)|(b<<16)|(255u<<24);
}
int dnx2FontDraw(uint32_t *pixels,int stride,int width,int height,
                int x,int y,const char *str,uint32_t color,int size,int maxchars) {
    if(!font_state.initialized||!pixels||!str)return 0;
    const int sz=size<=2?0:size==3?1:2;
    const int baseline=y+heights[sz]-2;
    int cursor=x;
    for(int i=0;str[i]&&i<maxchars;i++){
        unsigned char cp=(unsigned char)str[i];
        if(cp>=127||cp<32)cp='?';
        CachedGlyph *g=&font_state.glyphs[sz][cp];
        if(cursor+g->advance>width)break;
        if(g->bitmap && g->w>0&&g->h>0){
            for(int gy=0;gy<g->h;gy++){
                int py=baseline+g->yoff+gy;
                if(py<0||py>=height)continue;
                for(int gx=0;gx<g->w;gx++){
                    int px=cursor+g->xoff+gx;
                    if(px<0||px>=width)continue;
                    unsigned a=g->bitmap[gy*g->w+gx];
                    if(a)pixels[py*stride+px]=blend(pixels[py*stride+px],color,a);
                }
            }
        }
        cursor+=g->advance;
        if(cursor>=width)break;
    }
    return 1;
}
