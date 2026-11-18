// SPDX-License-Identifier: MIT
#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb_truetype.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
    assert(argc==6);stbtt_fontinfo fonts[5];unsigned char *data[5];
    for(int i=0;i<5;i++){
        FILE *f=fopen(argv[i+1],"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);assert(n>64);
        data[i]=malloc((size_t)n);assert(data[i]);assert(fread(data[i],1,(size_t)n,f)==(size_t)n);fclose(f);
        int offset=stbtt_GetFontOffsetForIndex(data[i],0);assert(offset>=0&&stbtt_InitFont(&fonts[i],data[i],offset));
    }
    int chars[]={0xe9,0x3a9,0x416,0x1d400,0x1d49c,0x1d56c,0x1d5d4,0x2605,0x1f600,0x1fabf,0x1f15c,0x1f15e,0x1f153,0x1f154,0x1f161};
    for(size_t n=0;n<sizeof(chars)/sizeof(chars[0]);n++){
        int found=0;
        for(int i=0;i<5;i++)if(stbtt_FindGlyphIndex(&fonts[i],chars[n])){
            int w,h,x,y;float scale=stbtt_ScaleForPixelHeight(&fonts[i],22);
            unsigned char *bitmap=stbtt_GetCodepointBitmap(&fonts[i],0,scale,chars[n],&w,&h,&x,&y);
            assert(bitmap&&w>0&&h>0);int nonzero=0;for(int j=0;j<w*h;j++)nonzero|=bitmap[j];assert(nonzero);free(bitmap);found=1;break;
        }
        if(!found)fprintf(stderr,"Missing test glyph U+%X\n",chars[n]);
        assert(found);
    }
    for(int i=0;i<5;i++)free(data[i]);
    puts("PASS actual embedded font files rasterize accented, Greek/Cyrillic, styled mathematical letters, symbols and emoji");
}
