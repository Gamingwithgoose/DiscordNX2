// SPDX-License-Identifier: MIT
/* Foreground UI jobs: one networking owner, immutable request snapshots,
 * main-thread result application. No framebuffer/auth overlay ownership. */
#define DNX_ASSET_CACHE 48
typedef struct {int type,valid;unsigned age;char id[24],hash[80],host[48],path[1536];Dnx2DecodedMedia media;unsigned char rgba[64*64*4];} ChatImage;
static ChatImage image_cache[DNX_ASSET_CACHE];
static unsigned image_age;
static void image_clear(ChatImage *image){dnx2FreeMedia(&image->media);memset(image,0,sizeof(*image));}
static Dnx2MediaCatalog media_catalog;
typedef struct {
    Thread thread;Mutex mutex;int running,stop,done,op,ok,post_confirmed,messages_loaded;
    char guild[24],channel[24],user[24],token[1024],text[513],sticker[24],notice[160];
    Dnx2Account *data;Dnx2MediaCatalog *media;
    ChatImage images[8];int image_count,image_ready,image_applied;
} ChatJob;
static ChatJob chat_job;
int dnx2JobBusy(void){return chat_job.running;}
int dnx2JobKind(void){return chat_job.running?chat_job.op:0;}
const char *dnx2JobDraft(void){return chat_job.text;}
const char *dnx2JobSticker(void){return chat_job.sticker;}
const Dnx2MediaCatalog *dnx2MediaCatalog(void){return &media_catalog;}
static int job_cancel(void){return __atomic_load_n(&chat_job.stop,__ATOMIC_ACQUIRE);}
static void job_error(void){
    snprintf(chat_job.notice,sizeof(chat_job.notice),"%s",last_http_status==401?"Session expired. Sign in again.":last_http_status==403?"Discord denied this request.":last_http_status==429?"Discord rate limited this request. Try again later.":"Could not load this content. Try again.");
}
static int job_get(const char *path){TlsSession https;int ok=http_request(&https,API_HOST,"GET",path,NULL,chat_job.token,(char*)http_body,sizeof(http_body));tls_close(&https);return ok;}
static int image_slot(int type,const char *id,const char *hash){
    for(int i=0;i<DNX_ASSET_CACHE;i++)if(image_cache[i].valid&&image_cache[i].type==type&&!strcmp(image_cache[i].id,id)&&!strcmp(image_cache[i].hash,hash))return i;
    return -1;
}
const unsigned char *dnx2ChatImage(int type,int index){
    const char *id=NULL,*hash="";
    if(type==1){if(index<0||index>=account.server_count)return NULL;id=account.servers[index].id;hash=account.servers[index].icon;}
    else {const Dnx2Media *list=type==2?media_catalog.emoji:media_catalog.stickers;int count=type==2?media_catalog.emoji_count:media_catalog.sticker_count;if(index<0||index>=count)return NULL;id=list[index].id;}
    int slot=image_slot(type,id,hash);if(slot<0)return NULL;image_cache[slot].age=++image_age;return image_cache[slot].valid>0?image_cache[slot].rgba:NULL;
}
static int job_apply_images(void){
    int changed=0;mutexLock(&chat_job.mutex);
    while(chat_job.image_applied<chat_job.image_ready){
        ChatImage *image=&chat_job.images[chat_job.image_applied++];int slot=image_slot(image->type,image->id,image->hash);
        if(slot<0){
            int media_count=0,old_media=-1;for(int i=0;i<DNX_ASSET_CACHE;i++)if(image_cache[i].valid&&image_cache[i].type==5){media_count++;if(old_media<0||image_cache[i].age<image_cache[old_media].age)old_media=i;}
            if(image->type==5&&media_count>=4)slot=old_media;
            else {slot=0;for(int i=0;i<DNX_ASSET_CACHE;i++){if(!image_cache[i].valid){slot=i;break;}if(image_cache[i].age<image_cache[slot].age)slot=i;}}
        }
        image_clear(&image_cache[slot]);image_cache[slot]=*image;image->media.pixels=NULL;image_cache[slot].age=++image_age;changed=1;
    }
    mutexUnlock(&chat_job.mutex);return changed;
}
static void job_images(void){
    for(int i=0;i<chat_job.image_count&&!job_cancel();i++){
        ChatImage *image=&chat_job.images[i];char path[1536];const char *host="cdn.discordapp.com";
        if(image->type==1)snprintf(path,sizeof(path),"/icons/%s/%s.png?size=64",image->id,image->hash);
        else if(image->type==2)snprintf(path,sizeof(path),"/emojis/%s.png?size=64",image->id);
        else if(image->type==3)snprintf(path,sizeof(path),"/stickers/%s.png?size=128",image->id);
        else if(image->type==4)snprintf(path,sizeof(path),"/avatars/%s/%s.png?size=64",image->id,image->hash);
        else if(image->type==7||image->type==8)snprintf(path,sizeof(path),"%s",image->path);
        else {snprintf(path,sizeof(path),"%s",image->path);host=image->host;}
        TlsSession https;int ok=http_request(&https,host,"GET",path,NULL,NULL,(char*)http_body,image->type==5?524288:131072);tls_close(&https);
        if(job_cancel())break;
        image->valid=ok&&(image->type==5?dnx2DecodeMedia(http_body,last_http_body_size,&image->media):dnx2DecodePng(http_body,last_http_body_size,image->rgba))?1:-1;wipe(http_body,131072);
        mutexLock(&chat_job.mutex);chat_job.image_ready=i+1;mutexUnlock(&chat_job.mutex);
    }
    chat_job.ok=1;
}
static void job_worker(void *unused){
    (void)unused;int sockets=0,ssl=0;char path[140];active_cancel=job_cancel;last_http_status=0;
    if(R_FAILED(socketInitialize(NULL)))goto done;
    sockets=1;
    if(R_FAILED(sslInitialize(1)))goto done;
    ssl=1;
    if(chat_job.op==20){job_images();goto done;}
    if(chat_job.op==9){
        snprintf(path,sizeof(path),"/api/v10/guilds/%s/channels",chat_job.guild);
        chat_job.ok=job_get(path);if(chat_job.ok)dnx_channels(chat_job.data,(char*)http_body);
    }else if(chat_job.op==10){
        snprintf(path,sizeof(path),"/api/v10/channels/%s/messages?limit=30",chat_job.channel);
        chat_job.ok=job_get(path);if(chat_job.ok){dnx_messages(chat_job.data,(char*)http_body);chat_job.messages_loaded=1;}
    }else if(chat_job.op==11){
        char body[3400]={0};
        if(dnx_media_body(chat_job.text,chat_job.sticker,body,sizeof(body))){
            snprintf(path,sizeof(path),"/api/v10/channels/%s/messages",chat_job.channel);TlsSession https;
            chat_job.post_confirmed=http_request(&https,API_HOST,"POST",path,body,chat_job.token,(char*)http_body,sizeof(http_body));tls_close(&https);
        }
        wipe(body,sizeof(body));chat_job.ok=chat_job.post_confirmed;
        if(chat_job.post_confirmed&&!job_cancel()){
            snprintf(path,sizeof(path),"/api/v10/channels/%s/messages?limit=30",chat_job.channel);
            if(job_get(path)){dnx_messages(chat_job.data,(char*)http_body);chat_job.messages_loaded=1;}
        }else snprintf(chat_job.notice,sizeof(chat_job.notice),"Send not confirmed. Check the channel before retrying.");
    }else if(chat_job.op==16){
        int stage=0;unsigned close_code=0;
        voice_control_preflight(chat_job.token,chat_job.guild,chat_job.channel,chat_job.user,chat_job.notice,&stage,&close_code);
        chat_job.ok=0; /* Signaling is not a working encrypted audio call. */
    }else if(chat_job.op==15){
        snprintf(chat_job.media->guild,sizeof(chat_job.media->guild),"%s",chat_job.guild);
        snprintf(path,sizeof(path),"/api/v10/guilds/%s/emojis",chat_job.guild);
        if(job_get(path))chat_job.media->emoji_count=dnx_media_parse(chat_job.media->emoji,(char*)http_body,0);
        else job_error();
        if(!job_cancel()){
            snprintf(path,sizeof(path),"/api/v10/guilds/%s/stickers",chat_job.guild);
            if(job_get(path))chat_job.media->sticker_count=dnx_media_parse(chat_job.media->stickers,(char*)http_body,1);
            else job_error();
        }
        chat_job.media->loaded=1;chat_job.ok=1;
    }
    if(!chat_job.ok&&!chat_job.notice[0])job_error();
done:
    if(chat_job.op!=20){
        FILE *report=fopen("sdmc:/kaihen/discordnx2-hardware.txt","w");
        if(report){fprintf(report,"DiscordNX2 v0.41 in-place chat request - no credentials, names, IDs or message content\noperation=%d complete=%d post_confirmed=%d http=%u cancelled=%d\n",chat_job.op,chat_job.ok,chat_job.post_confirmed,last_http_status,job_cancel());fclose(report);}
    }
    if(!chat_job.ok&&!chat_job.notice[0])snprintf(chat_job.notice,sizeof(chat_job.notice),"Network request could not finish.");
    wipe(http_body,sizeof(http_body));if(ssl)sslExit();if(sockets)socketExit();active_cancel=NULL;
    wipe(chat_job.token,sizeof(chat_job.token));__atomic_store_n(&chat_job.done,1,__ATOMIC_RELEASE);
}
static int job_launch(void){
    if(R_SUCCEEDED(threadCreate(&chat_job.thread,job_worker,NULL,NULL,128*1024,0x3b,-2))){
        if(R_SUCCEEDED(threadStart(&chat_job.thread))){chat_job.running=1;return 1;}threadClose(&chat_job.thread);
    }
    free(chat_job.data);free(chat_job.media);chat_job.data=NULL;chat_job.media=NULL;wipe(chat_job.token,sizeof(chat_job.token));snprintf(account.notice,sizeof(account.notice),"Request could not start.");return 0;
}
void dnx2JobStop(void){
    if(chat_job.running){__atomic_store_n(&chat_job.stop,1,__ATOMIC_RELEASE);threadWaitForExit(&chat_job.thread);threadClose(&chat_job.thread);if(chat_job.op==20)job_apply_images();chat_job.running=0;}
    if(chat_job.data){wipe(chat_job.data,sizeof(*chat_job.data));free(chat_job.data);chat_job.data=NULL;}
    if(chat_job.media){wipe(chat_job.media,sizeof(*chat_job.media));free(chat_job.media);chat_job.media=NULL;}
    for(int i=0;i<8;i++)dnx2FreeMedia(&chat_job.images[i].media);
    wipe(chat_job.token,sizeof(chat_job.token));
}
static int job_begin(int op){
    if(!account.signed_in||account.server_count<=0||(chat_job.running&&chat_job.op==11))return 0;
    dnx2JobStop();dnx2LiveStop();memset(&chat_job,0,sizeof(chat_job));chat_job.op=op;
    snprintf(chat_job.guild,sizeof(chat_job.guild),"%s",account.servers[account.selected].id);
    if(account.selected_channel>=0)snprintf(chat_job.channel,sizeof(chat_job.channel),"%s",account.channels[account.selected_channel].id);
    snprintf(chat_job.token,sizeof(chat_job.token),"%s",session_token);snprintf(chat_job.user,sizeof(chat_job.user),"%s",account.user_id);return 1;
}
int dnx2JobStart(int op,const char *text,const char *sticker){
    if(op!=9&&op!=10&&op!=11&&op!=15&&op!=16)return 0;
    if((op==10||op==11||op==16)&&account.selected_channel<0)return 0;
    if(op==11&&(!text||strlen(text)>512||((!sticker||!*sticker)&&!*text)))return 0;
    if(!job_begin(op))return 0;
    if(text)snprintf(chat_job.text,sizeof(chat_job.text),"%s",text);
    if(sticker)snprintf(chat_job.sticker,sizeof(chat_job.sticker),"%s",sticker);
    if(op==15)chat_job.media=calloc(1,sizeof(*chat_job.media));else chat_job.data=calloc(1,sizeof(*chat_job.data));
    if((op==15&&!chat_job.media)||(op!=15&&!chat_job.data)){dnx2JobStop();return 0;}
    account.notice[0]=0;return job_launch();
}
int dnx2RequestImages(int type,int first,int count){
    if(chat_job.running||!account.signed_in)return 0;
    ChatImage *requests=calloc(8,sizeof(*requests));if(!requests)return 0;int used=0;
    for(int i=first;i<first+count&&used<8;i++){
        const char *id=NULL,*hash="";
        if(type==1){if(i<0||i>=account.server_count)continue;id=account.servers[i].id;hash=account.servers[i].icon;if(!dnx_asset_hash(hash))continue;}
        else{const Dnx2Media *list=type==2?media_catalog.emoji:media_catalog.stickers;int total=type==2?media_catalog.emoji_count:media_catalog.sticker_count;if(i<0||i>=total)continue;if(type==3&&list[i].format!=1&&list[i].format!=2)continue;id=list[i].id;}
        if(image_slot(type,id,hash)>=0)continue;
        requests[used].type=type;snprintf(requests[used].id,sizeof(requests[used].id),"%s",id);snprintf(requests[used].hash,sizeof(requests[used].hash),"%s",hash);used++;
    }
    if(!used||!job_begin(20)){free(requests);return 0;}
    memcpy(chat_job.images,requests,(size_t)used*sizeof(*requests));free(requests);chat_job.image_count=used;return job_launch();
}
const unsigned char *dnx2MessageImage(int type,const char *id){int i=image_slot(type,id,"");return i>=0&&image_cache[i].valid>0?image_cache[i].rgba:NULL;}
const unsigned char *dnx2AuthorAvatar(const char *id,const char *avatar,const char *guild_avatar,unsigned default_avatar){
    char fallback[16];snprintf(fallback,sizeof(fallback),"%u",default_avatar);
    int type=dnx_asset_hash(guild_avatar)?7:dnx_asset_hash(avatar)?4:8;const char *hash=type==7?guild_avatar:type==4?avatar:fallback;
    int slot=image_slot(type,id,hash);if(slot<0)return NULL;image_cache[slot].age=++image_age;return image_cache[slot].valid>0?image_cache[slot].rgba:NULL;
}
const Dnx2DecodedMedia *dnx2MessageMedia(const char *id){int slot=image_slot(5,id,"");if(slot<0)return NULL;image_cache[slot].age=++image_age;return image_cache[slot].valid>0?&image_cache[slot].media:NULL;}
int dnx2RequestMessageImages(int first,int count){
    if(chat_job.running||!account.signed_in||account.selected_channel<0)return 0;
    ChatImage *requests=calloc(8,sizeof(*requests));if(!requests)return 0;int used=0;
    for(int i=first;i<first+count&&i<account.message_count&&used<8;i++){
        if(i<0)continue;
        const Dnx2Message *m=&account.messages[i];
        if(dnx_id(m->author_id)){
            char fallback[16];snprintf(fallback,sizeof(fallback),"%u",m->default_avatar);
            int type=dnx_asset_hash(m->guild_avatar)?7:dnx_asset_hash(m->avatar)?4:8;const char *hash=type==7?m->guild_avatar:type==4?m->avatar:fallback;int duplicate=0;
            for(int j=0;j<used;j++)if(requests[j].type==type&&!strcmp(requests[j].id,m->author_id)&&!strcmp(requests[j].hash,hash))duplicate=1;
            if(!duplicate&&image_slot(type,m->author_id,hash)<0){ChatImage *request=&requests[used++];request->type=type;snprintf(request->id,24,"%s",m->author_id);snprintf(request->hash,80,"%s",hash);
                if(type==7)snprintf(request->path,sizeof(request->path),"/guilds/%s/users/%s/avatars/%s.png?size=64",account.servers[account.selected].id,m->author_id,hash);
                if(type==8)snprintf(request->path,sizeof(request->path),"/embed/avatars/%u.png",m->default_avatar);
            }
        }
        if(m->media_id[0]&&used<8&&image_slot(5,m->media_id,"")<0){ChatImage *request=&requests[used];if(dnx_media_url(m->media_url,request->host,request->path)){request->type=5;snprintf(request->id,24,"%s",m->media_id);used++;}}
        if(m->sticker_id[0]&&used<8&&(m->sticker_format==1||m->sticker_format==2)&&image_slot(3,m->sticker_id,"")<0){requests[used].type=3;snprintf(requests[used++].id,24,"%s",m->sticker_id);}
        const char *p=m->content;
        while(*p&&used<8){char id[24],name[33];size_t n=dnx_custom_emoji(p,id,name);
            if(n){int found=image_slot(2,id,"")>=0;for(int j=0;j<used;j++)if(requests[j].type==2&&!strcmp(requests[j].id,id))found=1;
                if(!found){requests[used].type=2;snprintf(requests[used++].id,24,"%s",id);}p+=n;
            }else p++;
        }
    }
    if(!used||!job_begin(20)){free(requests);return 0;}memcpy(chat_job.images,requests,(size_t)used*sizeof(*requests));free(requests);chat_job.image_count=used;return job_launch();
}
int dnx2JobApply(void){
    if(!chat_job.running)return 0;
    int images=chat_job.op==20?job_apply_images():0;
    if(!__atomic_load_n(&chat_job.done,__ATOMIC_ACQUIRE))return images?20:0;
    threadWaitForExit(&chat_job.thread);threadClose(&chat_job.thread);chat_job.running=0;
    int scoped=account.signed_in&&account.server_count&&!strcmp(account.servers[account.selected].id,chat_job.guild);
    if((chat_job.op==10||chat_job.op==11||chat_job.op==16)&&scoped)scoped=account.selected_channel>=0&&!strcmp(account.channels[account.selected_channel].id,chat_job.channel);
    if(scoped){
        if(chat_job.ok&&chat_job.op==9){account.channel_count=chat_job.data->channel_count;memcpy(account.channels,chat_job.data->channels,(size_t)account.channel_count*sizeof(Dnx2Channel));account.channels_loaded=1;account.selected_channel=-1;account.message_count=0;}
        if(chat_job.ok&&(chat_job.op==10||chat_job.op==11)&&chat_job.messages_loaded){account.message_count=chat_job.data->message_count;memcpy(account.messages,chat_job.data->messages,(size_t)account.message_count*sizeof(Dnx2Message));}
        if(chat_job.ok&&chat_job.op==15)media_catalog=*chat_job.media;
        if(chat_job.notice[0])snprintf(account.notice,sizeof(account.notice),"%s",chat_job.notice);
    }
    int result=chat_job.ok?chat_job.op:-chat_job.op;
    if(chat_job.data){wipe(chat_job.data,sizeof(*chat_job.data));free(chat_job.data);chat_job.data=NULL;}
    if(chat_job.media){wipe(chat_job.media,sizeof(*chat_job.media));free(chat_job.media);chat_job.media=NULL;}
    return scoped||chat_job.op==20?result:0;
}
void dnx2ChatReset(void){dnx2JobStop();for(int i=0;i<DNX_ASSET_CACHE;i++)image_clear(&image_cache[i]);wipe(&media_catalog,sizeof(media_catalog));wipe(&chat_job,sizeof(chat_job));image_age=0;}
