// SPDX-License-Identifier: MIT
#ifndef DNX_BROWSER_COPY_CORE_H
#define DNX_BROWSER_COPY_CORE_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef int (*BrowserRead)(void*,uint64_t,void*,size_t,size_t*);
typedef int (*BrowserWrite)(void*,uint64_t,const void*,size_t);
typedef int (*BrowserCancel)(void*);
enum {BROWSER_COPY_OK=0,BROWSER_COPY_READ=-1,BROWSER_COPY_WRITE=-2,BROWSER_COPY_CANCEL=-3,BROWSER_COPY_LIMIT=-4};
static inline int browser_component(const char *name,size_t cap){
 size_t n=0;while(n<cap&&name[n])n++;
 if(!n||n==cap||!strcmp(name,".")||!strcmp(name,".."))return 0;
 for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)name[i];if(c<32||c==127||c=='/'||c=='\\'||c==':')return 0;}
 return 1;
}
/* The adapters own the exact-size source, temporary output and final rename. */
static inline int browser_copy(void *ctx,BrowserRead read,BrowserWrite write,BrowserCancel cancel,
 uint64_t size,uint64_t limit,void *buffer,size_t capacity,uint64_t *copied){
 *copied=0;if(size>limit||!capacity)return BROWSER_COPY_LIMIT;
 while(*copied<size){
  if(cancel&&cancel(ctx))return BROWSER_COPY_CANCEL;
  size_t wanted=(size-*copied)>capacity?capacity:(size_t)(size-*copied),got=0;
  if(!read(ctx,*copied,buffer,wanted,&got)||!got||got>wanted)return BROWSER_COPY_READ;
  if(!write(ctx,*copied,buffer,got))return BROWSER_COPY_WRITE;
  *copied+=got;
 }
 return BROWSER_COPY_OK;
}
#endif
