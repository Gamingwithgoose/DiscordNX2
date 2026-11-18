// SPDX-License-Identifier: MIT
#include "client_data.h"
#include <assert.h>
static Dnx2Account a;
static void string_tests(void){
    char out[200];assert(dj_get(dj_root("{\"nested\":{\"name\":\"wrong\"},\"name\":\"Caf\\u00e9 \\ud83d\\ude80 東京\"}"),"name",out,sizeof(out)));
    assert(!strcmp(out,"Café 🚀 東京"));
    assert(!dj_get(dj_root("{\"name\":\"\\ud800\"}"),"name",out,sizeof(out)));
    assert(!dj_get(dj_root("{\"name\":\"\\udc00\"}"),"name",out,sizeof(out)));
    assert(!dj_get(dj_root("{\"name\":\"long\"}"),"name",out,3));
    const char *p="\xf0\x9f\x9a\x80é";assert(dnx_utf8_next(&p)==0x1f680);assert(dnx_utf8_next(&p)==0xe9);assert(!*p);
    p="\xf0";assert(dnx_utf8_next(&p)==0xfffd);assert(!*p);
}
static void data_tests(void){
    dnx_guilds(&a,"[{\"id\":\"100000000000000001\",\"name\":\"A } \\\" café\",\"nested\":{}},{\"id\":\"100000000000000002\",\"name\":\"B\"}]");
    assert(a.server_count==2);assert(!strcmp(a.servers[0].name,"A } \" café"));
    unsigned char pb[32]={0x72,0x14,0x0a,0x12,0x0a,0x10};
    uint64_t id=100000000000000002ULL;for(int i=0;i<8;i++)pb[6+i]=(unsigned char)(id>>(8*i));
    id--;for(int i=0;i<8;i++)pb[14+i]=(unsigned char)(id>>(8*i));
    assert(dnx_order_proto(&a,pb,22));assert(!strcmp(a.servers[0].id,"100000000000000002"));
    unsigned char bad[]={0x72,0xff};assert(!dnx_order_proto(&a,bad,sizeof(bad)));
    dnx_channels(&a,"[{\"id\":\"200000000000000003\",\"name\":\"Voice 🚀\",\"type\":2,\"position\":1,\"parent_id\":\"200000000000000002\",\"permission_overwrites\":[{\"id\":\"300000000000000000\",\"type\":0}]},{\"id\":\"200000000000000002\",\"name\":\"Category\",\"type\":4,\"position\":4},{\"id\":\"200000000000000001\",\"name\":\"general\",\"type\":0,\"position\":0,\"parent_id\":null}]");
    assert(a.channel_count==3&&a.selected_channel==-1);assert(a.channels[0].type==0&&a.channels[1].type==4&&a.channels[2].type==2);
    assert(!strcmp(a.channels[2].name,"Voice 🚀"));
    dnx_messages(&a,"[{\"id\":\"1\",\"author\":{\"username\":\"new\",\"global_name\":\"Élodie\"},\"content\":\"newest { }\",\"embeds\":[{\"name\":\"not author\"}]},{\"author\":{\"username\":\"old\",\"global_name\":null},\"content\":\"oldest\"}]");
    assert(a.message_count==2);assert(!strcmp(a.messages[0].content,"oldest"));assert(!strcmp(a.messages[1].author,"Élodie"));
}
int main(void){string_tests();data_tests();
    char body[3200],content[100];assert(dnx_message_body("A \"quote\" \\ line\n",body,sizeof(body)));
    assert(dj_get(dj_root(body),"content",content,sizeof(content))&&!strcmp(content,"A \"quote\" \\ line\n"));
    assert(strstr(body,"\"allowed_mentions\":{\"parse\":[]}"));assert(!dnx_message_body("",body,sizeof(body)));
puts("PASS UTF-8 JSON, surrogate pairs, nested fields, server order protobuf, channel categories and chronological messages");}
