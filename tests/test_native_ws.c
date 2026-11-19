// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include "../app/source/native_ws.h"
static Dnx2WsReader reader;
static unsigned char out[8193];
static void feed(const unsigned char *b,size_t n){memcpy(reader.wire+reader.used,b,n);reader.used+=n;}
static void voice_binary(void){Dnx2WsReader r={0};unsigned char out[80];r.allow_binary=1;unsigned char first[]={0x02,3,0,7,25};memcpy(r.wire,first,sizeof(first));r.used=sizeof(first);assert(dnx2WsNext(&r,out,sizeof(out))==0);unsigned char last[]={0x80,2,0,9};memcpy(r.wire,last,sizeof(last));r.used=sizeof(last);assert(dnx2WsNext(&r,out,sizeof(out))==4&&r.completed_size==5&&r.is_binary&&out[0]==0&&out[3]==0);}
int main(void){voice_binary();
 const unsigned char text[]={0x81,5,'h','e','l','l','o'};
 for(size_t i=0;i<sizeof(text);i++){feed(text+i,1);int rc=dnx2WsNext(&reader,out,sizeof(out));assert(rc==(i==sizeof(text)-1?1:0));}
 assert(strcmp((char*)out,"hello")==0);
 const unsigned char parts[]={0x01,2,'h','e',0x89,2,0,'p',0x80,3,'l','l','o'};
 feed(parts,sizeof(parts));assert(dnx2WsNext(&reader,out,sizeof(out))==0);
 assert(dnx2WsNext(&reader,out,sizeof(out))==2 && reader.control_size==2 && out[0]==0 && out[1]=='p');
 assert(dnx2WsNext(&reader,out,sizeof(out))==1 && strcmp((char*)out,"hello")==0);
 const unsigned char masked[]={0x81,0x80};feed(masked,sizeof(masked));assert(dnx2WsNext(&reader,out,sizeof(out))==-1);
 memset(&reader,0,sizeof(reader));const unsigned char big[]={0x81,127,0x80,0,0,0,0,0,0,0};feed(big,sizeof(big));assert(dnx2WsNext(&reader,out,sizeof(out))==-1);
 memset(&reader,0,sizeof(reader));const unsigned char orphan[]={0x80,1,'x'};feed(orphan,sizeof(orphan));assert(dnx2WsNext(&reader,out,sizeof(out))==-1);
 puts("PASS native WS: bytewise partial frames, text fragments, interleaved binary ping, masked/oversized/orphan rejection");
}
