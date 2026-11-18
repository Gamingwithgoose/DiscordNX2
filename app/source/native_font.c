// SPDX-License-Identifier: MIT
/* Switch shared standard font, rasterized by stb_truetype (public domain/MIT).
 * Uses console fonts plus embedded OFL-licensed Noto Unicode fallbacks.
 * Public-domain font8x8 is the safe fallback when pl:u is unavailable.
 */
#include "native_font.h"
#include "client_json.h"
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
    uint32_t code;
} CachedGlyph;
extern const unsigned char dnx2_font_sans[],dnx2_font_math[],dnx2_font_symbols[],dnx2_font_emoji[];
static struct {
    stbtt_fontinfo info;
    stbtt_fontinfo fallback[10];
    int fallback_count;
    CachedGlyph glyphs[3][256];
    int initialized,pl_live;
} font_state;
static const int heights[3]={18,22,28};
static void destroy_cache(void) {
    for(int sz=0;sz<3;sz++)for(int c=0;c<256;c++){
        free(font_state.glyphs[sz][c].bitmap);
        font_state.glyphs[sz][c].bitmap=NULL;
    }
}
int dnx2FontInit(void) {
    if(font_state.initialized)return 1;
    const unsigned char *data=dnx2_font_sans;
    int idx;
    if(!R_FAILED(plInitialize(PlServiceType_User))){
        font_state.pl_live=1;
    }
    /* Avoid parsing non-font wrappers as a TTF. */
    if(!((data[0]==0&&data[1]==1&&data[2]==0&&data[3]==0)||
         memcmp(data,"OTTO",4)==0))goto fail;
    idx=stbtt_GetFontOffsetForIndex(data,0);
    if(idx<0||!stbtt_InitFont(&font_state.info,data,idx))goto fail;
    if(font_state.pl_live)for(int type=0;type<PlSharedFontType_Total;type++){
        PlFontData shared={0};
        if(R_FAILED(plGetSharedFontByType(&shared,(PlSharedFontType)type))||!shared.address||shared.size<64)continue;
        int offset=stbtt_GetFontOffsetForIndex(shared.address,0);
        if(offset>=0&&stbtt_InitFont(&font_state.fallback[font_state.fallback_count],shared.address,offset))font_state.fallback_count++;
    }
    const unsigned char *bundled[]={dnx2_font_sans,dnx2_font_math,dnx2_font_symbols,dnx2_font_emoji};
    for(int i=0;i<4;i++){
        int offset=stbtt_GetFontOffsetForIndex(bundled[i],0);
        if(offset>=0&&stbtt_InitFont(&font_state.fallback[font_state.fallback_count],bundled[i],offset))font_state.fallback_count++;
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
static CachedGlyph *glyph(int sz,uint32_t cp){
    CachedGlyph *g=&font_state.glyphs[sz][cp%256];
    if(g->code==cp&&g->advance)return g;
    free(g->bitmap);memset(g,0,sizeof(*g));g->code=cp;
    stbtt_fontinfo *info=&font_state.info;
    if(!stbtt_FindGlyphIndex(info,(int)cp))for(int i=0;i<font_state.fallback_count;i++){
        if(stbtt_FindGlyphIndex(&font_state.fallback[i],(int)cp)){info=&font_state.fallback[i];break;}
    }
    float scale=stbtt_ScaleForPixelHeight(info,(float)heights[sz]);
    int advance=0,lsb=0;stbtt_GetCodepointHMetrics(info,(int)cp,&advance,&lsb);
    g->advance=(int)(advance*scale+0.5f);if(g->advance<4)g->advance=4;
    g->bitmap=stbtt_GetCodepointBitmap(info,0,scale,(int)cp,&g->w,&g->h,&g->xoff,&g->yoff);
    if(cp!=' '&&(!stbtt_FindGlyphIndex(info,(int)cp)||!g->bitmap||!g->w)){
        free(g->bitmap);int xs=heights[sz]/12,ys=heights[sz]/8;
        g->w=8*xs;g->h=8*ys;g->xoff=0;g->yoff=2-heights[sz];g->advance=g->w+2;
        g->bitmap=calloc((size_t)g->w*g->h,1);
        if(g->bitmap)for(int y=0;y<g->h;y++)for(int x=0;x<g->w;x++){
            /* Missing Unicode glyph: one outlined box per code point. */
            int on=cp<128?(font8x8_basic[cp][y/ys]&(1u<<(x/xs))):x==0||y==0||x==g->w-1||y==g->h-1;
            g->bitmap[y*g->w+x]=on?255:0;
        }
    }
    return g;
}
int dnx2FontWidth(const char *text,int size){
    int width=0,sz=size<=2?0:size==3?1:2;if(!text)return 0;
    const char *p=text;while(*p){uint32_t cp=dnx_utf8_next(&p);if(cp==0xfe0e||cp==0xfe0f||cp==0x200d)continue;width+=font_state.initialized?glyph(sz,cp)->advance:6*size;}
    return width;
}
int dnx2FontFit(const char *text,int width,int size,int maxchars){
    if(!text||width<1)return 0;
    int sz=size<=2?0:size==3?1:2,used=0;const char *p=text,*end=text;
    for(int i=0;*p&&i<maxchars;i++){
        uint32_t cp=dnx_utf8_next(&p);int advance=font_state.initialized?glyph(sz,cp)->advance:6*size;
        if(cp==0xfe0e||cp==0xfe0f||cp==0x200d)advance=0;
        if(used+advance>width)break;
        used+=advance;end=p;
    }
    return (int)(end-text);
}
int dnx2FontDraw(uint32_t *pixels,int stride,int width,int height,
                int x,int y,const char *str,uint32_t color,int size,int maxchars) {
    if(!font_state.initialized||!pixels||!str)return 0;
    const int sz=size<=2?0:size==3?1:2;
    const int baseline=y+heights[sz]-2;
    int cursor=x;const char *p=str;
    for(int i=0;*p&&i<maxchars;i++){
        uint32_t cp=dnx_utf8_next(&p);if(cp==0xfe0e||cp==0xfe0f||cp==0x200d)continue;if(cp<32)cp=' ';
        CachedGlyph *g=glyph(sz,cp);
        if(cursor+g->advance>width)break;
        if(g->bitmap && g->w>0&&g->h>0){
            for(int gy=0;gy<g->h;gy++){
                int py=baseline+g->yoff+gy;if(py<0||py>=height)continue;
                for(int gx=0;gx<g->w;gx++){
                    int px=cursor+g->xoff+gx;if(px<0||px>=width)continue;
                    unsigned a=g->bitmap[gy*g->w+gx];
                    if(a)pixels[py*stride+px]=blend(pixels[py*stride+px],color,a);
                }
            }
        }
        cursor+=g->advance;if(cursor>=width)break;
    }
    return 1;
}
