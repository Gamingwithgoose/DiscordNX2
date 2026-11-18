// SPDX-License-Identifier: MIT
#define DNX2_WS_MAX (4*1024*1024)
#include "native_ws.h"
#include "gateway_session.h"
#include <assert.h>
#include <stdlib.h>
int main(void){Dnx2WsReader *reader=calloc(1,sizeof(*reader));unsigned char *out=malloc(DNX2_WS_MAX+1);assert(reader&&out);size_t n=1500000;reader->wire[0]=0x81;reader->wire[1]=127;for(int i=0;i<8;i++)reader->wire[2+i]=(unsigned char)((uint64_t)n>>(56-8*i));
const char *prefix="{\"op\":0,\"t\":\"READY\",\"d\":{\"session_id\":\"fixture\",\"resume_gateway_url\":\"wss://gateway-region.discord.gg\"},\"padding\":\"";size_t size=strlen(prefix);memcpy(reader->wire+10,prefix,size);memset(reader->wire+10+size,'a',n-size-2);memcpy(reader->wire+10+n-2,"\"}",2);reader->used=n+10;assert(dnx2WsNext(reader,out,DNX2_WS_MAX+1)==1);char host[256];assert(dnx_gateway_host(dj_field(dj_root((char*)out),"d"),host)&&!strcmp(host,"gateway-region.discord.gg"));reader->used=10;reader->wire[0]=0x81;reader->wire[1]=127;n=DNX2_WS_MAX+1;for(int i=0;i<8;i++)reader->wire[2+i]=(unsigned char)((uint64_t)n>>(56-8*i));assert(dnx2WsNext(reader,out,DNX2_WS_MAX+1)==-1);free(reader);free(out);puts("PASS actual Gateway framing: 1.5MiB Ready accepted, validated resume host, >4MiB rejected");}
