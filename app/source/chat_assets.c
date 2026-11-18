// SPDX-License-Identifier: MIT
#include "chat_assets.h"
#define STBI_ONLY_PNG
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
