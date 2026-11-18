// SPDX-License-Identifier: MIT
#include "chat_assets.h"
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 2048
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
static int gif_preview(const unsigned char *bytes,size_t n,int w,int h,int count,Dnx2DecodedMedia *out){
    int width=w,height=h;if(width>256){height=height*256/width;width=256;}if(height>256){width=width*256/height;height=256;}if(width<1)width=1;if(height<1)height=1;
    int step=(count+59)/60,frames=(count+step-1)/step;size_t stride=(size_t)width*height*4;
    while(stride*(size_t)frames>4194304){step++;frames=(count+step-1)/step;}
    out->pixels=calloc((size_t)frames,stride);stbi__gif *g=calloc(1,sizeof(*g));unsigned char *saved=malloc((size_t)w*h*4);int ok=0,c=0;
    if(!out->pixels||!g||!saved)goto done;
    memset(saved,0,(size_t)w*h*4);stbi__context input;stbi__start_mem(&input,bytes,(int)n);int have_saved=0;
    for(int f=0;f<count;f++){
        unsigned char *pixels=stbi__gif_load_next(&input,g,&c,4,have_saved?saved:NULL);if(!pixels||pixels==(unsigned char*)&input)goto done;
        int index=f/step;if(f%step==0)for(int y=0;y<height;y++)for(int x=0;x<width;x++)memcpy(out->pixels+(size_t)index*stride+4*(y*width+x),pixels+4*((y*h/height)*w+x*w/width),4);
        int delay=g->delay;out->delays[index]+=delay<20?20:delay>2000?2000:delay;
        if((g->eflags&0x1c)!=0x0c){memcpy(saved,pixels,(size_t)w*h*4);have_saved=1;}
    }
    out->width=width;out->height=height;out->frames=frames;ok=1;
 done:
    if(g){free(g->out);free(g);}free(saved);if(!ok)dnx2FreeMedia(out);return ok;
}
int dnx2DecodeMedia(const unsigned char *bytes,size_t n,Dnx2DecodedMedia *out){
    if(!bytes||!out||n<8||n>1048575)return 0;
    memset(out,0,sizeof(*out));int w=0,h=0,c=0;
    if(!stbi_info_from_memory(bytes,(int)n,&w,&h,&c)||w<1||h<1||w>2048||h>2048||(size_t)w*h>4194304)return 0;
    int gif=!memcmp(bytes,"GIF8",4),count=gif?gif_frames(bytes,n):1;if(!count)return 0;
    if(gif&&count>1&&count<=240&&(size_t)w*h*(size_t)count<=33554432)return gif_preview(bytes,n,w,h,count,out);
    unsigned char *decoded=stbi_load_from_memory(bytes,(int)n,&w,&h,&c,4);if(!decoded)return 0;
    int width=w,height=h;if(width>640){height=height*640/width;width=640;}if(height>360){width=width*360/height;height=360;}if(width<1)width=1;if(height<1)height=1;
    out->pixels=malloc((size_t)width*height*4);if(!out->pixels){stbi_image_free(decoded);return 0;}
    for(int y=0;y<height;y++)for(int x=0;x<width;x++)memcpy(out->pixels+4*(y*width+x),decoded+4*((y*h/height)*w+x*w/width),4);
    out->width=width;out->height=height;out->frames=1;out->delays[0]=100;stbi_image_free(decoded);return 1;
}
