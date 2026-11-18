// SPDX-License-Identifier: MIT
#include "chat_media_data.h"
#include "unicode_text.h"
#include <assert.h>
static Dnx2Account account;
int main(int argc,char **argv){
    assert(argc==2);
    const char *accent="e\xcc\x81",*family="\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb",*flag="\xf0\x9f\x87\xba\xf0\x9f\x87\xb8";
    assert(dnx_grapheme_end(accent)==accent+strlen(accent));assert(dnx_grapheme_end(family)==family+strlen(family));assert(dnx_grapheme_end(flag)==flag+strlen(flag));
    char text[100];snprintf(text,sizeof(text),"A%s",family);assert(dnx_grapheme_previous(text)==1);text[dnx_grapheme_previous(text)]=0;assert(!strcmp(text,"A"));
    char composed[256];dnx_cluster_text(accent,accent+strlen(accent),composed);assert(!strcmp(composed,"\xc3\xa9"));
    char decoded[100];assert(dj_get(dj_root("{\"text\":\"\\u00e9\\n\\ud83d\\ude00\"}"),"text",decoded,sizeof(decoded)));assert(!strcmp(decoded,"\xc3\xa9\n\xf0\x9f\x98\x80"));assert(!dj_get(dj_root("{\"text\":\"\\u0000\"}"),"text",decoded,sizeof(decoded)));
    char host[48],path[1536];assert(dnx_media_url("https://media.discordapp.net/attachments/123/test.png?ex=1",host,path));
    assert(!dnx_media_url("https://media.discordapp.net.evil.example/x.png",host,path));assert(!dnx_media_url("http://cdn.discordapp.com/x.png",host,path));assert(!dnx_media_url("https://127.0.0.1/x.png",host,path));assert(!dnx_media_url("https://cdn.discordapp.com/x\r\nHeader: bad",host,path));
    dnx_messages(&account,"[{\"id\":\"900000000000000001\",\"timestamp\":\"2026-11-18T08:36:00Z\",\"author\":{\"id\":\"100000000000000001\",\"username\":\"global\",\"avatar\":\"0123456789abcdef0123456789abcdef\",\"discriminator\":\"1234\"},\"member\":{\"nick\":\"Server Nick\",\"avatar\":\"abcdef0123456789abcdef0123456789\"},\"content\":\"caption\",\"attachments\":[{\"id\":\"800000000000000001\",\"content_type\":\"image/gif\",\"proxy_url\":\"https://media.discordapp.net/attachments/123/test.gif?ex=1\"}]}]");
    assert(account.message_count==1);const Dnx2Message *m=&account.messages[0];assert(!strcmp(m->author,"Server Nick")&&!strcmp(m->author_id,"100000000000000001")&&m->default_avatar==4&&m->avatar[0]&&m->guild_avatar[0]&&strstr(m->media_url,"width=256")&&m->media_id[0]);
    FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t n=(size_t)ftell(f);rewind(f);unsigned char *bytes=malloc(n);assert(bytes&&fread(bytes,1,n,f)==n);fclose(f);
    Dnx2DecodedMedia media={0};assert(dnx2DecodeMedia(bytes,n,&media));assert(media.frames==2&&media.width==1&&media.height==1);assert(media.pixels[0]==255&&media.pixels[2]==0&&media.pixels[4]==0&&media.pixels[6]==255);assert(media.delays[0]==100);dnx2FreeMedia(&media);assert(!media.pixels);
    assert(!dnx2DecodeMedia(bytes,20,&media));free(bytes);
    puts("PASS Unicode grapheme editing/NFC/newlines, avatar/nickname metadata, approved image URLs and actual bounded animated GIF decoding");
}
