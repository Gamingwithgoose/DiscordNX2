// SPDX-License-Identifier: MIT
#include "copy_core.h"
#include <assert.h>
#include <stdio.h>
typedef struct {unsigned char input[257],output[257];int read_fail,write_fail,zero_read,oversize,cancel,cancel_after;size_t reads,writes;} Fixture;
static int read_data(void *p,uint64_t at,void *buffer,size_t requested,size_t *got){Fixture *f=p;f->reads++;if(f->read_fail)return 0;if(f->zero_read){*got=0;return 1;}if(f->oversize){*got=requested+1;return 1;}*got=requested>7?7:requested;memcpy(buffer,f->input+at,*got);return 1;}
static int write_data(void *p,uint64_t at,const void *buffer,size_t size){Fixture *f=p;f->writes++;if(f->write_fail)return 0;memcpy(f->output+at,buffer,size);return 1;}
static int cancel_data(void *p){Fixture *f=p;return f->cancel||(f->cancel_after&&f->writes>=(size_t)f->cancel_after);}
int main(void){Fixture f={0};for(size_t i=0;i<sizeof(f.input);i++)f.input[i]=(unsigned char)i;unsigned char buffer[64];uint64_t copied;
 assert(browser_component("main.npdm",10));assert(!browser_component("../main",8));assert(!browser_component("..",3));assert(!browser_component(".",2));assert(!browser_component("",1));assert(!browser_component("sdmc:",6));assert(!browser_component("a\\b",4));assert(!browser_component("a\nb",4));assert(!browser_component("abcd",4));
 assert(browser_copy(&f,read_data,write_data,cancel_data,257,257,buffer,sizeof(buffer),&copied)==0&&copied==257&&!memcmp(f.input,f.output,257));
 f.reads=f.writes=0;assert(browser_copy(&f,read_data,write_data,cancel_data,258,257,buffer,64,&copied)==BROWSER_COPY_LIMIT&&!f.reads&&!f.writes);
 f.zero_read=1;assert(browser_copy(&f,read_data,write_data,cancel_data,257,257,buffer,64,&copied)==BROWSER_COPY_READ&&!copied);f.zero_read=0;
 f.oversize=1;assert(browser_copy(&f,read_data,write_data,cancel_data,257,257,buffer,64,&copied)==BROWSER_COPY_READ&&!copied);f.oversize=0;
 f.read_fail=1;assert(browser_copy(&f,read_data,write_data,cancel_data,257,257,buffer,64,&copied)==BROWSER_COPY_READ);f.read_fail=0;
 f.write_fail=1;assert(browser_copy(&f,read_data,write_data,cancel_data,257,257,buffer,64,&copied)==BROWSER_COPY_WRITE&&!copied);f.write_fail=0;
 f.reads=f.writes=0;f.cancel_after=3;assert(browser_copy(&f,read_data,write_data,cancel_data,257,257,buffer,64,&copied)==BROWSER_COPY_CANCEL&&copied==21);f.cancel_after=0;
 f.cancel=1;assert(browser_copy(&f,read_data,write_data,cancel_data,257,257,buffer,64,&copied)==BROWSER_COPY_CANCEL&&!copied);
 assert(browser_copy(&f,read_data,write_data,cancel_data,0,257,buffer,64,&copied)==0&&!copied);
 puts("PASS bounded exporter: partial reads, exact bytes, limit-before-I/O, EOF/error rejection, cancellation, unsafe path components");
}
