// SPDX-License-Identifier: MIT
#include "chat_assets.h"
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 512
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"
#include <string.h>
int dnx2DecodePng(const unsigned char *png,size_t length,unsigned char rgba[64*64*4]){
    if(!png||!rgba||length<8||length>131072||memcmp(png,"\x89PNG\r\n\x1a\n",8))return 0;
    int w=0,h=0,n=0;
    if(!stbi_info_from_memory(png,(int)length,&w,&h,&n)||w<1||h<1||w>512||h>512)return 0;
    unsigned char *pixels=stbi_load_from_memory(png,(int)length,&w,&h,&n,4);if(!pixels)return 0;
    for(int y=0;y<64;y++)for(int x=0;x<64;x++)memcpy(rgba+4*(y*64+x),pixels+4*((y*h/64)*w+x*w/64),4);
    stbi_image_free(pixels);return 1;
}

/* Bounded media preview decode; no video, network or persistent storage here. */
static int gif_blocks(const unsigned char *p,size_t n,size_t *at){
    while(*at<n){unsigned size=p[(*at)++];if(!size)return 1;if(size>n-*at)return 0;*at+=size;}return 0;
}
static int gif_frames(const unsigned char *p,size_t n){
    if(n<13||memcmp(p,"GIF8",4))return 0;
    size_t at=13;
    if(p[10]&128)at+=3u*(2u<<(p[10]&7));
    if(at>n)return 0;
    int frames=0;
    while(at<n){unsigned marker=p[at++];if(marker==0x3b)return frames;
        if(marker==0x21){if(at>=n)return 0;at++;if(!gif_blocks(p,n,&at))return 0;}
        else if(marker==0x2c){if(n-at<9)return 0;unsigned flags=p[at+8];at+=9;if(flags&128)at+=3u*(2u<<(flags&7));if(at>=n)return 0;at++;if(!gif_blocks(p,n,&at))return 0;frames++;}
        else return 0;
    }return 0;
}
void dnx2FreeMedia(Dnx2DecodedMedia *m){if(m){free(m->pixels);memset(m,0,sizeof(*m));}}
int dnx2DecodeMedia(const unsigned char *bytes,size_t n,Dnx2DecodedMedia *out){
    if(!bytes||!out||n<8||n>524288)return 0;
    memset(out,0,sizeof(*out));
    int w=0,h=0,c=0;if(!stbi_info_from_memory(bytes,(int)n,&w,&h,&c)||w<1||h<1||w>512||h>512)return 0;
    int frames=1,*delays=NULL,gif=!memcmp(bytes,"GIF8",4),count=gif?gif_frames(bytes,n):1;
    if(!count)return 0;
    unsigned char *decoded;
    if(gif&&count<=12&&w<=256&&h<=256)decoded=stbi_load_gif_from_memory(bytes,(int)n,&delays,&w,&h,&frames,&c,4);
    else decoded=stbi_load_from_memory(bytes,(int)n,&w,&h,&c,4);
    if(!decoded||frames<1||frames>12){stbi_image_free(decoded);stbi_image_free(delays);return 0;}
    int width=w,height=h;if(width>256){height=height*256/width;width=256;}if(height>144){width=width*144/height;height=144;}if(width<1)width=1;if(height<1)height=1;
    size_t size=(size_t)width*height*4*(size_t)frames;out->pixels=malloc(size);if(!out->pixels){stbi_image_free(decoded);stbi_image_free(delays);return 0;}
    for(int f=0;f<frames;f++){
        for(int y=0;y<height;y++)for(int x=0;x<width;x++)memcpy(out->pixels+4*((size_t)f*width*height+y*width+x),decoded+4*((size_t)f*w*h+(y*h/height)*w+x*w/width),4);
        int delay=delays?delays[f]:100;out->delays[f]=delay<20?20:delay>2000?2000:delay;
    }
    out->width=width;out->height=height;out->frames=frames;stbi_image_free(decoded);stbi_image_free(delays);return 1;
}
