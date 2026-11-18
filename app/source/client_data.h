// SPDX-License-Identifier: MIT
#ifndef DNX2_CLIENT_DATA_H
#define DNX2_CLIENT_DATA_H
#include "client_json.h"
#include "native_login.h"
/* Order schema: discord-userdoccers/discord-protos, PreloadedUserSettings
 * field14 GuildFolders; folders field1; GuildFolder guild_ids field1 fixed64.
 * Read-only, flattened folders; never modifies Discord settings. */
static inline int dnx_id(const char *s){size_t n=strlen(s);if(n<15||n>22)return 0;for(;*s;s++)if(*s<'0'||*s>'9')return 0;return 1;}
static inline void dnx_guilds(Dnx2Account *a,const char *json){
    a->server_count=0;DnxJson root=dj_root(json),e;const char *p=NULL;
    while(a->server_count<DNX2_MAX_SERVERS&&dj_next(root,&p,&e)){
        Dnx2Server g={0};if(dj_get(e,"id",g.id,sizeof(g.id))&&dnx_id(g.id)&&dj_get(e,"name",g.name,sizeof(g.name))){dj_get(e,"icon",g.icon,sizeof(g.icon));a->servers[a->server_count++]=g;}
    }
}
static inline int dnx_pbvar(const unsigned char **p,const unsigned char *end,uint64_t *n){
    *n=0;for(int i=0;i<10;i++){if(*p>=end)return 0;unsigned c=*(*p)++;if(i==9&&c>1)return 0;*n|=(uint64_t)(c&127)<<(7*i);if(!(c&128))return 1;}return 0;
}
typedef struct {unsigned field,wire;const unsigned char *p;size_t n;} DnxPb;
static inline int dnx_pbnext(const unsigned char **p,const unsigned char *end,DnxPb *f){
    uint64_t tag,n=0;if(!dnx_pbvar(p,end,&tag)||!tag)return 0;f->field=(unsigned)(tag>>3);f->wire=(unsigned)(tag&7);
    if(f->wire==2){if(!dnx_pbvar(p,end,&n))return 0;}else if(f->wire==1)n=8;else if(f->wire==5)n=4;else if(f->wire==0){f->p=*p;if(!dnx_pbvar(p,end,&n))return 0;f->n=(size_t)(*p-f->p);return 1;}else return 0;
    if(n>(uint64_t)(end-*p))return 0;
    f->p=*p;f->n=(size_t)n;*p+=n;return 1;
}
static inline void dnx_order_add(Dnx2Account *a,const char *id,int *at){
    for(int i=*at;i<a->server_count;i++)if(!strcmp(a->servers[i].id,id)){
        Dnx2Server g=a->servers[i];memmove(&a->servers[*at+1],&a->servers[*at],(size_t)(i-*at)*sizeof(g));a->servers[(*at)++]=g;break;
    }
}
static inline void dnx_order_ids(Dnx2Account *a,DnxPb f,int *at){
    if((f.wire!=1&&f.wire!=2)||f.n%8)return;
    for(size_t i=0;i<f.n;i+=8){uint64_t id=0;for(int b=0;b<8;b++)id|=(uint64_t)f.p[i+b]<<(8*b);char text[24];snprintf(text,sizeof(text),"%llu",(unsigned long long)id);dnx_order_add(a,text,at);}
}
static inline int dnx_order_proto(Dnx2Account *a,const unsigned char *data,size_t n){
    const unsigned char *p=data,*end=data+n;DnxPb f;int at=0;
    while(p<end){if(!dnx_pbnext(&p,end,&f))return 0;if(f.field!=14||f.wire!=2)continue;
        const unsigned char *q=f.p,*qe=q+f.n;DnxPb g;
        while(q<qe){if(!dnx_pbnext(&q,qe,&g))return 0;if(g.field!=1||g.wire!=2)continue;
            const unsigned char *v=g.p,*ve=v+g.n;DnxPb ids;while(v<ve){if(!dnx_pbnext(&v,ve,&ids))return 0;if(ids.field==1)dnx_order_ids(a,ids,&at);}}
        if(!at){q=f.p;while(q<qe){if(!dnx_pbnext(&q,qe,&g))return 0;if(g.field==2)dnx_order_ids(a,g,&at);}}
    }return at>0;
}
static inline int dnx_channel_compare(const void *l,const void *r){
    const Dnx2Channel *a=l,*b=r;if(a->position!=b->position)return a->position<b->position?-1:1;
    size_t x=strlen(a->id),y=strlen(b->id);return x==y?strcmp(a->id,b->id):x<y?-1:1;
}
static inline void dnx_channels(Dnx2Account *a,const char *json){
    a->channel_count=0;a->selected_channel=-1;a->message_count=0;DnxJson e,root=dj_root(json);const char *p=NULL;
    while(a->channel_count<DNX2_MAX_CHANNELS&&dj_next(root,&p,&e)){
        Dnx2Channel c={0};c.type=dj_int(e,"type",-1);c.position=dj_int(e,"position",0);
        if(c.type!=0&&c.type!=2&&c.type!=4&&c.type!=5&&c.type!=13&&c.type!=15&&c.type!=16)continue;
        if(!dj_get(e,"id",c.id,sizeof(c.id))||!dnx_id(c.id)||!dj_get(e,"name",c.name,sizeof(c.name)))continue;
        (void)dj_get(e,"parent_id",c.parent,sizeof(c.parent));a->channels[a->channel_count++]=c;
    }
    qsort(a->channels,(size_t)a->channel_count,sizeof(Dnx2Channel),dnx_channel_compare);
    Dnx2Channel *sorted=calloc(DNX2_MAX_CHANNELS,sizeof(*sorted));if(!sorted)return;
    int used[DNX2_MAX_CHANNELS]={0},n=0;
    for(int i=0;i<a->channel_count;i++)if(a->channels[i].type!=4&&!a->channels[i].parent[0]){sorted[n++]=a->channels[i];used[i]=1;}
    for(int i=0;i<a->channel_count;i++)if(a->channels[i].type==4){sorted[n++]=a->channels[i];used[i]=1;
        for(int j=0;j<a->channel_count;j++)if(!used[j]&&!strcmp(a->channels[j].parent,a->channels[i].id)){sorted[n++]=a->channels[j];used[j]=1;}}
    for(int i=0;i<a->channel_count;i++)if(!used[i])sorted[n++]=a->channels[i];
    memcpy(a->channels,sorted,(size_t)n*sizeof(*sorted));free(sorted);
}
static inline int dnx_message_body(const char *text,char *body,size_t cap){
    if(!text||!*text||strlen(text)>512||cap<3200)return 0;
    size_t at=0;memcpy(body,"{\"content\":\"",12);at=12;
    for(const unsigned char *p=(const unsigned char*)text;*p;p++){
        if(*p=='"'||*p=='\\')body[at++]='\\';
        if(*p<32){int n=snprintf(body+at,cap-at,"\\u%04x",*p);at+=(size_t)n;}else body[at++]=(char)*p;
    }
    snprintf(body+at,cap-at,"\",\"allowed_mentions\":{\"parse\":[]}}");return 1;
}
static inline void dnx_messages(Dnx2Account *a,const char *json){
    a->message_count=0;DnxJson root=dj_root(json),e;const char *p=NULL;
    while(a->message_count<DNX2_MAX_MESSAGES&&dj_next(root,&p,&e)){
        Dnx2Message m={0};dj_get(e,"id",m.id,sizeof(m.id));DnxJson author=dj_field(e,"author");
        if(!dj_get(author,"global_name",m.author,sizeof(m.author))||!m.author[0])dj_get(author,"username",m.author,sizeof(m.author));
        if(!dj_get(e,"content",m.content,sizeof(m.content)))snprintf(m.content,sizeof(m.content),"[Message is too long to display]");
        DnxJson stickers=dj_field(e,"sticker_items"),first;const char *cursor=NULL;
        if(dj_next(stickers,&cursor,&first)){dj_get(first,"id",m.sticker_id,sizeof(m.sticker_id));dj_get(first,"name",m.sticker_name,sizeof(m.sticker_name));m.sticker_format=dj_int(first,"format_type",0);if(!dnx_id(m.sticker_id))m.sticker_id[0]=0;}
        if(!m.content[0]&&!m.sticker_id[0])snprintf(m.content,sizeof(m.content),"[Attachment, embed or empty message]");
        a->messages[a->message_count++]=m;
    }
    for(int i=0;i<a->message_count/2;i++){Dnx2Message m=a->messages[i];a->messages[i]=a->messages[a->message_count-1-i];a->messages[a->message_count-1-i]=m;}
}
#endif
