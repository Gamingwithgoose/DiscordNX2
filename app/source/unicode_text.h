// SPDX-License-Identifier: MIT
#ifndef DNX2_UNICODE_TEXT_H
#define DNX2_UNICODE_TEXT_H
#include "client_json.h"
#include "vendor/utf8proc/utf8proc.h"
static inline const char *dnx_grapheme_end(const char *text){
    if(!text||!*text)return text;
    const char *p=text;uint32_t previous=dnx_utf8_next(&p);utf8proc_int32_t state=0;
    while(*p){const char *next=p;uint32_t cp=dnx_utf8_next(&next);if(utf8proc_grapheme_break_stateful((utf8proc_int32_t)previous,(utf8proc_int32_t)cp,&state))break;previous=cp;p=next;}return p;
}
static inline size_t dnx_grapheme_previous(const char *text){const char *p=text,*previous=text;while(*p){previous=p;p=dnx_grapheme_end(p);}return (size_t)(previous-text);}
static inline void dnx_cluster_text(const char *begin,const char *end,char out[256]){
    size_t n=(size_t)(end-begin);if(n>=256){memcpy(out,"\xef\xbf\xbd",4);return;}
    utf8proc_uint8_t *normalized=NULL;utf8proc_ssize_t size=utf8proc_map((const utf8proc_uint8_t*)begin,(utf8proc_ssize_t)n,&normalized,UTF8PROC_STABLE|UTF8PROC_COMPOSE);
    if(size<0||size>=256){free(normalized);memcpy(out,"\xef\xbf\xbd",4);return;}
    memcpy(out,normalized,(size_t)size);out[size]=0;free(normalized);
}
#endif
