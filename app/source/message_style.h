// SPDX-License-Identifier: MIT
#ifndef DNX2_MESSAGE_STYLE_H
#define DNX2_MESSAGE_STYLE_H
#include <string.h>
/* Bounded inline Markdown subset; unmatched delimiters remain literal. */
static inline int dnx_message_style(const char *p,unsigned *style){
    if(*p=='`'){if((*style&16)||strchr(p+1,'`')){*style^=16;return 1;}return 0;}
    if(*style&16)return 0;
    const char *markers[]={"**","__","~~","*","_"};const unsigned bits[]={1,4,8,2,2};
    for(int i=0;i<5;i++){size_t n=strlen(markers[i]);if(!strncmp(p,markers[i],n)&&((*style&bits[i])||strstr(p+n,markers[i]))){*style^=bits[i];return (int)n;}}
    return 0;
}
#endif
