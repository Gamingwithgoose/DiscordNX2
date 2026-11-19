// SPDX-License-Identifier: MIT
/* DiscordNX2 native Switch 2 framebuffer interface.
 *
 * Uses native VI-backed framebuffer, not the console text renderer.
 * No faked Discord account, guild or voice state. Authentication and voice
 * features are locked until an actual supported implementation exists.
 */
#include "graphical_ui.h"
#include "vendor/qrcodegen.h"
#include "native_login.h"
#include "session_store.h"
#include "client_json.h"
#include "chat_layout.h"
#include "chat_assets.h"
#include "chat_media_data.h"
#include "mobile_keyboard.h"
#include "unicode_text.h"
#include "message_style.h"
#include "phone_text.h"
#include <stdlib.h>
#include "native_font.h"
#include "brand_icon.h"
#include <math.h>
#include <ctype.h>
#include "vendor/font8x8_basic.h"
#include <switch.h>
#include <switch/display/framebuffer.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define W 1280
#define H 720
#define C_BG       RGBA8_MAXALPHA(49,51,56)
#define C_SIDEBAR  RGBA8_MAXALPHA(43,45,49)
#define C_CARD     RGBA8_MAXALPHA(56,58,64)
#define C_CARD_HI  RGBA8_MAXALPHA(64,66,73)
#define C_PRIMARY  RGBA8_MAXALPHA(88,101,242)
#define C_GREEN    RGBA8_MAXALPHA(65,196,120)
#define C_WHITE    RGBA8_MAXALPHA(219,222,225)
#define C_MUTED    RGBA8_MAXALPHA(148,155,164)
#define C_WARN     RGBA8_MAXALPHA(242,175,86)
#define C_DARK     RGBA8_MAXALPHA(30,31,34)
#define C_LINE     RGBA8_MAXALPHA(63,65,71)

static uint32_t *pixels;
static int stride_pixels;
static int text_right=1240;
static char auth_notice[128];
static int tab=0,sel=0,focus=0;
static int voice_panel=0,voice_action=0,voice_member_first=0;
static int message_expanded=0,message_offset=0,profile_open=0;
static Dnx2Message profile_message;
static const Dnx2Message *render_message;
static char draft[513];
static char pending_sticker[24],pending_sticker_name[129];
const char *dnx2GraphicalDraft(void){return draft;}
void dnx2GraphicalClearDraft(void){volatile char *p=draft;for(size_t i=0;i<sizeof(draft);i++)p[i]=0;memset(pending_sticker,0,sizeof(pending_sticker));memset(pending_sticker_name,0,sizeof(pending_sticker_name));}
static void rect(int x,int y,int width,int height,uint32_t color) {
    if(!pixels||width<=0||height<=0)return;
    if(x<0){width+=x;x=0;}if(y<0){height+=y;y=0;}
    if(x>=W||y>=H)return;
    if(x+width>W)width=W-x;
    if(y+height>H)height=H-y;
    if(width<=0||height<=0)return;
    for(int py=y;py<y+height;py++) {
        uint32_t *dst=pixels+py*stride_pixels+x;
        for(int px=0;px<width;px++)dst[px]=color;
    }
}
/* Scanline rounded panels: all writes pass through framebuffer clipping. */
static void rounded(int x,int y,int w,int h,int radius,uint32_t color) {
    if(w<=0||h<=0)return;
    if(radius>w/2)radius=w/2;
    if(radius>h/2)radius=h/2;
    if(radius<1){rect(x,y,w,h,color);return;}
    for(int row=0;row<h;row++){
        int dy=row<radius?radius-1-row:row>=h-radius?row-(h-radius):0;
        int inset=dy?radius-(int)sqrtf((float)(radius*radius-dy*dy)):0;
        rect(x+inset,y+row,w-2*inset,1,color);
    }
}
/* Small native UI glyphs. Every shape uses framebuffer-clipped primitives. */
static void mic_icon(int x,int y,uint32_t color,uint32_t background,int muted){
 rounded(x+3,y+8,14,13,7,color);rect(x+3,y+7,14,6,background);rounded(x+5,y+7,10,12,5,background);
 rounded(x+7,y+1,6,15,3,color);rect(x+9,y+20,2,4,color);rect(x+5,y+24,10,2,color);
 if(muted)for(int i=0;i<24;i++)rect(x+i*20/24,y+1+i,3,2,color);
}
static void folder_icon(int x,int y,uint32_t color,uint32_t background){
 rounded(x,y+5,22,17,2,color);rounded(x,y+2,10,7,2,color);rect(x+2,y+8,18,12,background);
}
static void brand(int x,int y) {
    for(int j=0;j<64;j++)for(int i=0;i<64;i++){
        int px=x+i,py=y+j;
        if(px<0||py<0||px>=W||py>=H)continue;
        const unsigned char *s=brand_icon+4*(j*64+i);
        uint32_t *d=pixels+py*stride_pixels+px,old=*d,v=0;
        for(int c=0;c<3;c++)v|=((s[c]*s[3]+((old>>(8*c))&255)*(255-s[3])+127)/255)<<(8*c);
        *d=v|0xff000000u;
    }
}
/* Public-domain 8x8 IBM VGA glyphs: cleaner shapes and visibly distinct
 * punctuation/numerals than the original handmade 5x7 alphabet.
 * Independent horizontal metrics preserve sidebar and card layout widths. */
static void text_line(int x,int y,const char *s,uint32_t color,int scale,int maxchars) {
    if(!s||scale<1)return;
    int right=text_right;
    if(dnx2FontDraw(pixels,stride_pixels,right,H,x,y,s,color,scale,maxchars))return;
    const int horizontal=scale>=3?2:1;
    const int advance=6*scale;
    const char *cursor=s;
    for(int i=0;*cursor&&i<maxchars && x+(i+1)*advance<=right;i++) {
        uint32_t cp=dnx_utf8_next(&cursor);unsigned char ch=cp<128?(unsigned char)cp:'?';
        const unsigned char *g=(const unsigned char*)font8x8_basic[ch];
        for(int row=0;row<8;row++) {
            for(int col=0;col<8;col++) {
                if(((unsigned char)g[row])&(1u<<col))
                    rect(x+i*advance+col*horizontal,y+row*scale,
                         horizontal,scale,color);
            }
        }
    }
}
static void line(int x,int y,int width,uint32_t color){rect(x,y,width,2,color);}
static void block(int x,int y,int w,int h,uint32_t color) {
    rounded(x,y,w,h,18,C_LINE);
    rounded(x+1,y+1,w-2,h-2,17,color);
}
static void item(int x,int y,int w,int h,const char *name,
                 const char *hint,int active) {
    block(x,y,w,h,active?C_CARD_HI:C_CARD);
    if(active)rounded(x+7,y+12,4,h-24,2,C_PRIMARY);
    text_line(x+24,y+15,name,C_WHITE,3,30);
    if(h>=90)text_line(x+24,y+53,hint,C_MUTED,2,44);
}
/* Reusable native QR renderer; only encode URLs provided by a validated
 * future authentication session. No static "login" QR is displayed. */
static uint8_t qr_temp[qrcodegen_BUFFER_LEN_MAX];
static uint8_t qr_data[qrcodegen_BUFFER_LEN_MAX];
static int qr_render_url(int x,int y,int area,const char *url) {
    if(!url || strncmp(url,"https://",8)!=0)return 0;
    if(!qrcodegen_encodeText(url,qr_temp,qr_data,qrcodegen_Ecc_MEDIUM,
                             qrcodegen_VERSION_MIN,qrcodegen_VERSION_MAX,
                             qrcodegen_Mask_AUTO,true))return 0;
    int size=qrcodegen_getSize(qr_data);
    if(size<=0)return 0;
    int pitch=area/(size+8);
    if(pitch<1)return 0;
    int total=(size+8)*pitch;
    rect(x,y,total,total,C_WHITE);
    for(int row=0;row<size;row++)for(int col=0;col<size;col++)
        if(qrcodegen_getModule(qr_data,col,row))
            rect(x+(col+4)*pitch,y+(row+4)*pitch,pitch,pitch,C_DARK);
    return size;
}
static void text_wrap(int x,int y,const char *text,int lines,int columns){
    const char *p=text;
    for(int row=0;row<lines&&*p;row++){
        char linebuf[512];int bytes=dnx2FontFit(p,text_right-x,2,columns);
        if(bytes<=0)break;
        if(bytes>=(int)sizeof(linebuf))bytes=(int)sizeof(linebuf)-1;
        memcpy(linebuf,p,(size_t)bytes);linebuf[bytes]=0;p+=bytes;
        text_line(x,y+row*23,linebuf,C_WHITE,2,columns);
    }
}
static int server_cursor=-1,server_page=0,channel_cursor=0,channel_first=0;
static int message_first=0,follow_latest=1;
static int message_images[DNX2_MAX_MESSAGES];
static void message_geometry(void){const Dnx2Account *a=dnx2Account();for(int i=0;i<a->message_count;i++)message_images[i]=a->messages[i].media_id[0]!=0;}
static int message_bottom(void){message_geometry();return dnx_message_bottom(message_images,dnx2Account()->message_count);}

static char keyboard_notice[128];
typedef struct {int held,start_x,start_y,last_y,moved;u64 sample;} TouchInput;
static TouchInput touch_input;
static void touch_init(void){
    (void)hidSetTouchScreenOutputRanges(W,H);hidInitializeTouchScreen();memset(&touch_input,0,sizeof(touch_input));
}
/* Tap only on release; a drag scrolls, never activates a row or Send. */
static int touch_event(int *x,int *y,int *scroll){
    HidTouchScreenState state={0};*scroll=0;
    if(!hidGetTouchScreenStates(&state,1)||state.sampling_number==touch_input.sample)return 0;
    touch_input.sample=state.sampling_number;
    if(state.count>0){
        int tx=(int)state.touches[0].x,ty=(int)state.touches[0].y;
        if(!touch_input.held){touch_input.held=1;touch_input.start_x=tx;touch_input.start_y=touch_input.last_y=ty;touch_input.moved=0;}
        if(abs(tx-touch_input.start_x)>14||abs(ty-touch_input.start_y)>14)touch_input.moved=1;
        if(touch_input.moved&&abs(ty-touch_input.last_y)>=40){*x=touch_input.start_x;*y=ty;*scroll=ty<touch_input.last_y?1:-1;touch_input.last_y=ty;return 2;}
    }else if(touch_input.held){
        touch_input.held=0;if(!touch_input.moved){*x=touch_input.start_x;*y=touch_input.start_y;return 1;}
    }
    return 0;
}
static int selected_text(void){const Dnx2Account *a=dnx2Account();return a->signed_in&&a->selected_channel>=0&&(a->channels[a->selected_channel].type==0||a->channels[a->selected_channel].type==5||a->channels[a->selected_channel].type==1||a->channels[a->selected_channel].type==3);}
static int media_open,media_tab,media_page,media_cursor;
static const char *common_emoji[]={"\xf0\x9f\x98\x80","\xf0\x9f\x98\x82","\xf0\x9f\x98\x8a","\xf0\x9f\x98\x8d","\xf0\x9f\x98\x8e","\xf0\x9f\xa4\x94","\xf0\x9f\x98\xad","\xf0\x9f\x98\xa1","\xf0\x9f\x91\x8d","\xf0\x9f\x91\x8e","\xf0\x9f\x91\x8f","\xf0\x9f\x99\x8f","\xe2\x9d\xa4","\xf0\x9f\x94\xa5","\xf0\x9f\x8e\x89","\xf0\x9f\x92\xaf","\xf0\x9f\x90\xa5","\xf0\x9f\xa6\x86","\xf0\x9f\xaa\xbf","\xf0\x9f\x91\x80","\xf0\x9f\x8e\xae","\xe2\x9c\x85","\xe2\x9c\xa8","\xf0\x9f\x9a\x80"};
static int media_count(void){const Dnx2MediaCatalog *m=dnx2MediaCatalog();const Dnx2Account *a=dnx2Account();if(media_tab&&(dnx2Account()->private_mode||!m->loaded||!a->server_count||strcmp(m->guild,a->servers[a->selected].id)))return 0;return media_tab==0?24:media_tab==1?m->emoji_count:m->sticker_count;}
static void image_draw(int x,int y,int w,int h,const unsigned char *rgba,int radius){
    if(!rgba)return;
    for(int j=0;j<h;j++)for(int i=0;i<w;i++){
        if(radius){int xx=i<radius?radius-i-1:i>=w-radius?i-(w-radius):0;int yy=j<radius?radius-j-1:j>=h-radius?j-(h-radius):0;if(xx*xx+yy*yy>radius*radius)continue;}
        int px=x+i,py=y+j;if(px<0||py<0||px>=W||py>=H)continue;
        const unsigned char *s=rgba+4*((j*64/h)*64+i*64/w);uint32_t *d=pixels+py*stride_pixels+px,old=*d,v=0;
        for(int c=0;c<3;c++)v|=((s[c]*s[3]+((old>>(8*c))&255)*(255-s[3])+127)/255)<<(8*c);
        *d=v|0xff000000u;
    }
}
static uint64_t ui_ms(void){struct timespec t={0};clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static int media_draw_image(int x,int y,int maxw,int maxh,const Dnx2DecodedMedia *media){
    if(!media||!media->pixels||!media->width||!media->height)return 0;
    int w=maxw,h=media->height*w/media->width;if(h>maxh){h=maxh;w=media->width*h/media->height;}if(w<1||h<1)return 0;
    int frame=0,total=0;for(int i=0;i<media->frames;i++)total+=media->delays[i];
    if(total&&media->frames>1){unsigned position=(unsigned)(ui_ms()%(unsigned)total);for(int i=0;i<media->frames;i++){if(position<(unsigned)media->delays[i]){frame=i;break;}position-=(unsigned)media->delays[i];}}
    const unsigned char *rgba=media->pixels+(size_t)frame*media->width*media->height*4;
    for(int j=0;j<h;j++)for(int i=0;i<w;i++){
        int px=x+i,py=y+j;if(px<0||py<0||px>=W||py>=588)continue;
        const unsigned char *s=rgba+4*((j*media->height/h)*media->width+i*media->width/w);uint32_t *d=pixels+py*stride_pixels+px,old=*d,v=0;
        for(int c=0;c<3;c++)v|=((s[c]*s[3]+((old>>(8*c))&255)*(255-s[3])+127)/255)<<(8*c);
        *d=v|0xff000000u;
    }return h;
}
static const char *author_name(const Dnx2Message *m){const Dnx2Member *member=dnx2Member(m->author_id);return member&&member->nick[0]?member->nick:m->author;}
static uint32_t author_color(const Dnx2Message *m){const Dnx2Account *a=dnx2Account();if(a->private_mode||!a->server_count)return C_WHITE;const Dnx2GuildRoles *roles=dnx2GuildRoles();if(strcmp(roles->guild,a->servers[a->selected].id))return C_WHITE;const Dnx2Member *member=dnx2Member(m->author_id);unsigned color=member?dnx_role_color(roles,member->roles,member->count):dnx_role_color(roles,m->roles,m->role_count);return color?RGBA8_MAXALPHA((color>>16)&255,(color>>8)&255,color&255):(uint32_t)C_WHITE;}
static void reply_draw(int y,const Dnx2Message *m){
    if(!m->reply_state)return;
    line(401,y+10,22,C_MUTED);rect(400,y+10,1,18,C_MUTED);text_right=1234;
    const Dnx2Member *member=dnx2Member(m->reply_author_id);const char *name=member&&member->nick[0]?member->nick:m->reply_author[0]?m->reply_author:m->reply_state==3?"Original message deleted":"Reply";
    unsigned color=member?dnx_role_color(dnx2GuildRoles(),member->roles,member->count):0;uint32_t ink=color?RGBA8_MAXALPHA((color>>16)&255,(color>>8)&255,color&255):(uint32_t)C_MUTED;
    text_right=800;text_line(432,y+2,name,ink,1,50);int next=432+dnx2FontWidth(name,1)+12;if(next>810)next=810;
    char preview[513];snprintf(preview,sizeof(preview),"%s",m->reply_content);for(char *p=preview;*p;p++)if(*p=='\n'||*p=='\r')*p=' ';text_right=1234;text_line(next,y+2,preview,C_MUTED,1,100);text_right=1256;
}
static int author_hit(int x,int y,int top,const Dnx2Message *m){int width=dnx2FontWidth(author_name(m),2);if(width>600)width=600;return y>=top+4&&y<top+46&&((x>=380&&x<420)||(x>=432&&x<432+width));}
static void profile_draw(void){
    const Dnx2Message *m=&profile_message;const Dnx2Member *member=dnx2Member(m->author_id);rounded(560,108,640,478,16,C_DARK);rounded(560,108,640,90,16,C_CARD_HI);rounded(1144,118,40,38,10,C_DARK);text_right=1180;text_line(1156,128,"x",C_WHITE,2,1);
    const unsigned char *avatar=dnx2AuthorAvatar(m->author_id,m->avatar,member&&member->avatar[0]?member->avatar:m->guild_avatar,m->default_avatar);if(avatar)image_draw(584,156,80,80,avatar,40);else rounded(584,156,80,80,40,C_PRIMARY);
    text_line(584,254,author_name(m),author_color(m),3,44);char username[180];snprintf(username,sizeof(username),"%s%s%s",m->username,m->discriminator[0]&&strcmp(m->discriminator,"0")?"#":"",m->discriminator[0]&&strcmp(m->discriminator,"0")?m->discriminator:"");text_line(584,296,username,C_MUTED,2,48);
    if(m->tag[0]){rounded(584,334,160,30,7,C_CARD_HI);text_line(596,341,m->tag,C_WHITE,2,12);}if(m->bot)text_line(780,341,"APP",C_PRIMARY,2,5);
    if(!dnx2Account()->private_mode){const Dnx2GuildRoles *roles=dnx2GuildRoles();text_line(584,384,"Roles",C_MUTED,2,10);int x=584,y=416,shown=0;
        for(int i=0;member&&i<roles->count;i++){const Dnx2Role *role=&roles->roles[i];int has=0;for(int j=0;j<member->count;j++)if(!strcmp(role->id,member->roles[j]))has=1;if(!has)continue;int width=dnx2FontWidth(role->name,1)+30;if(width>260)width=260;if(x+width>1168){x=584;y+=34;}if(y>510)break;rounded(x,y,width,28,7,C_CARD);unsigned c=role->color;rounded(x+7,y+9,8,8,4,c?RGBA8_MAXALPHA((c>>16)&255,(c>>8)&255,c&255):(uint32_t)C_MUTED);text_right=x+width-6;text_line(x+22,y+7,role->name,C_WHITE,1,48);x+=width+8;shown++;}
        text_right=1180;if(!shown)text_line(584,420,member?"No displayed roles":dnx2MemberState(m->author_id)<0?"Server profile unavailable":"Loading server profile...",C_MUTED,2,45);
        if(member&&strlen(member->joined)>=10){char joined[60];snprintf(joined,sizeof(joined),"Member since %.10s",member->joined);text_line(584,548,joined,C_MUTED,1,60);}
    }text_right=1256;
}
static void author_draw(int y,const Dnx2Message *m){
    const Dnx2Member *member=dnx2Member(m->author_id);const unsigned char *avatar=dnx2AuthorAvatar(m->author_id,m->avatar,member&&member->avatar[0]?member->avatar:m->guild_avatar,m->default_avatar);
    if(avatar)image_draw(380,y+8,40,40,avatar,20);
    else {rounded(380,y+8,40,40,20,C_PRIMARY);const char *end=m->author;dnx_utf8_next(&end);char letter[5]={0};size_t n=(size_t)(end-m->author);if(n<sizeof(letter))memcpy(letter,m->author,n);text_line(391,y+17,letter,C_WHITE,2,1);}
    text_right=1010;text_line(432,y+8,author_name(m),author_color(m),2,60);int after=432+dnx2FontWidth(author_name(m),2)+10;if(after>1010)after=1010;text_right=1120;
    if(m->tag[0]){int width=dnx2FontWidth(m->tag,1)+36;rounded(after,y+7,width,22,5,C_CARD_HI);const unsigned char *badge=dnx2TagBadge(m->tag_guild,m->tag_badge);if(badge)image_draw(after+4,y+9,18,18,badge,0);text_line(after+28,y+12,m->tag,C_WHITE,1,16);after+=width+8;}if(m->bot){text_line(after,y+12,"APP",C_PRIMARY,1,5);after+=32;}text_right=1256;
    if(strlen(m->timestamp)>=16){char when[16];snprintf(when,sizeof(when),"%.5s UTC",m->timestamp+11);text_line(after+8,y+12,when,C_MUTED,1,12);}
}
static size_t mention_label(const char *p,char label[380]){if(p[0]!='<'||(p[1]!='@'&&p[1]!='#'))return 0;int kind=p[1];const char *start=p+2;if(*start=='!'||*start=='&')start++;const char *end=start;while(*end>='0'&&*end<='9')end++;size_t n=(size_t)(end-start);if(*end!='>'||n<15||n>22)return 0;char id[24];memcpy(id,start,n);id[n]=0;const char *name=NULL;
    if(kind=='#'){const Dnx2Account *a=dnx2Account();for(int i=0;i<a->channel_count;i++)if(!strcmp(a->channels[i].id,id))name=a->channels[i].name;}
    else if(p[2]=='&'){const Dnx2GuildRoles *roles=dnx2GuildRoles();for(int i=0;i<roles->count;i++)if(!strcmp(roles->roles[i].id,id))name=roles->roles[i].name;}
    else if(render_message){for(int i=0;i<render_message->mention_count;i++)if(!strcmp(render_message->mentions[i].id,id))name=render_message->mentions[i].name;const Dnx2Account *a=dnx2Account();for(int i=0;!name&&i<a->message_count;i++)if(!strcmp(a->messages[i].author_id,id))name=author_name(&a->messages[i]);}
    snprintf(label,380,"%c%s",kind=='#'?'#':'@',name?name:kind=='#'?"unknown-channel":"unknown-user");return (size_t)(end-p+1);
}
static void message_wrap(int x,int y,const char *text,int lines,int columns){
    const char *p=text;unsigned style=0;
    for(int row=0;row<lines&&*p;row++){
        int cursor=x;
        for(int col=0;col<columns&&*p;col++){
            int marker=dnx_message_style(p,&style);if(marker){p+=marker;continue;}
            if(!(style&16)&&*p=='\\'&&p[1]&&strchr("*_~`<>\\",p[1]))p++;
            char mention[380];size_t consumed=style&16?0:mention_label(p,mention);if(consumed){int width=dnx2FontWidth(mention,2);if(width>text_right-cursor&&cursor>x)break;if(width>text_right-cursor)width=text_right-cursor;rounded(cursor,y+row*23,width,22,4,C_CARD_HI);int old_right=text_right;text_right=cursor+width;text_line(cursor,y+row*23,mention,RGBA8_MAXALPHA(201,205,251),2,60);text_right=old_right;cursor+=width;p+=consumed;continue;}
            char id[24],name[33];size_t token=style&16?0:dnx_custom_emoji(p,id,name);
            if(token){
                const unsigned char *image=dnx2MessageImage(2,id);char fallback[36];snprintf(fallback,sizeof(fallback),":%s:",name);
                int width=image?24:dnx2FontWidth(fallback,2);if(cursor+width>text_right)break;
                if(image)image_draw(cursor,y+row*23,21,21,image,0);else text_line(cursor,y+row*23,fallback,C_WHITE,2,36);
                cursor+=width;p+=token;continue;
            }
            const char *start=p;p=dnx_grapheme_end(p);char value[256];dnx_cluster_text(start,p,value);if(value[0]=='\n'||value[0]=='\r')break;int width=dnx2FontWidth(value,2);
            if(cursor+width>text_right){p=start;break;}if(style&16)rounded(cursor,y+row*23,width,22,3,C_DARK);
            if(!dnx2FontDrawStyled(pixels,stride_pixels,text_right,H,cursor,y+row*23,value,C_WHITE,2,1,style))text_line(cursor,y+row*23,value,C_WHITE,2,1);
            if(style&4)rect(cursor,y+row*23+20,width,1,C_WHITE);
            if(style&8)rect(cursor,y+row*23+10,width,1,C_WHITE);
            cursor+=width;
        }
    }
}
static void media_begin(void){
    if(dnx2JobKind()==11)return;
    media_open=!media_open;media_page=media_cursor=0;
    if(media_open&&dnx2Account()->private_mode){media_tab=0;}
    else if(media_open){const Dnx2MediaCatalog *m=dnx2MediaCatalog();const Dnx2Account *a=dnx2Account();if(!m->loaded||strcmp(m->guild,a->servers[a->selected].id))dnx2JobStart(15,NULL,NULL);}
}
static void media_choose(int index){
    const Dnx2MediaCatalog *m=dnx2MediaCatalog();if(index<0||index>=media_count())return;
    char value[200];
    if(media_tab==0)snprintf(value,sizeof(value),"%s",common_emoji[index]);
    else if(media_tab==1){const Dnx2Media *e=&m->emoji[index];snprintf(value,sizeof(value),"<%s:%s:%s>",e->animated?"a":"",e->name,e->id);}
    else {snprintf(pending_sticker,sizeof(pending_sticker),"%s",m->stickers[index].id);snprintf(pending_sticker_name,sizeof(pending_sticker_name),"%s",m->stickers[index].name);media_open=0;return;}
    size_t n=strlen(draft),k=strlen(value);if(n+k<=512)memcpy(draft+n,value,k+1);media_open=0;
}
static void media_draw(void){
    const Dnx2MediaCatalog *m=dnx2MediaCatalog();rounded(372,292,888,326,14,C_SIDEBAR);text_right=1248;
    const char *tabs[]={"Emoji","Server","Stickers"};
    for(int t=0;t<3;t++){if(media_tab==t)rounded(388+t*210,302,196,36,8,C_CARD_HI);text_line(410+t*210,311,tabs[t],media_tab==t?C_WHITE:C_MUTED,2,20);}
    text_line(1218,310,"x",C_MUTED,2,2);
    int first=media_page*24;
    for(int cell=0;cell<24;cell++){
        int index=first+cell;if(index>=media_count())break;int x=388+(cell%8)*106,y=348+(cell/8)*82;
        if(index==media_cursor)rounded(x,y,94,76,10,C_CARD);
        if(media_tab==0)text_line(x+26,y+24,common_emoji[index],C_WHITE,4,2);
        else{
            const unsigned char *image=dnx2ChatImage(media_tab==1?2:3,index);
            if(image)image_draw(x+15,y+4,64,64,image,0);
            else {const Dnx2Media *item=media_tab==1?&m->emoji[index]:&m->stickers[index];text_line(x+5,y+25,item->name,C_MUTED,1,10);}
        }
    }
    if(!media_count()&&!dnx2JobBusy())text_line(402,410,"No items available in this server.",C_MUTED,2,70);
}
static void server_tooltip(void){
 const Dnx2Account *a=dnx2Account();if(focus!=0||server_cursor<0||server_cursor>=a->server_count)return;
 const char *name=a->servers[server_cursor].name;char label[405];int width=dnx2FontWidth(name,2);if(width>296){int n=dnx2FontFit(name,276,2,256);if(n<0)n=0;if(n>400)n=400;memcpy(label,name,(size_t)n);memcpy(label+n,"...",4);name=label;width=dnx2FontWidth(name,2);}width+=24;if(width<80)width=80;if(width>320)width=320;
 int y=98+(server_cursor%8)*62;text_right=92+width-12;rounded(92,y,width,44,8,C_CARD_HI);text_line(104,y+12,name,C_WHITE,2,128);text_right=1256;
}
static void voice_controls(int current){
 int running=dnx2VoiceRunning()&&current;unsigned state=dnx2VoiceState();int muted=dnx2VoiceMuted();
 item(392,124,650,64,running?"Disconnect":"Join voice","",focus>=2&&voice_action==0);
 if(running){
  const char *status=state==4||state==5?(muted?"Microphone muted":"Connected"):state==3?"Establishing encrypted voice":state==2?"Connecting audio":"Connecting...";
  unsigned wait=dnx2VoiceWaitSeconds();char waiting[80];if(wait){snprintf(waiting,sizeof(waiting),"Reconnecting in %u seconds",wait);status=waiting;}
  text_line(392,204,status,state>=4?C_GREEN:C_MUTED,2,72);
  item(392,244,650,64,muted?"Unmute microphone":"Mute microphone","",focus>=2&&voice_action==1);
  if(dnx2VoiceBackground())text_line(1064,265,"Background",C_GREEN,2,16);else if(dnx2VoiceBackgroundAvailable()){rounded(1054,244,190,64,10,C_CARD);text_line(1068,265,"Y: Background",C_WHITE,2,18);}
  Dnx2VoiceMember members[33];int count=dnx2VoiceMembers(members,33);int limit=count>6?count-6:0;if(voice_member_first>limit)voice_member_first=limit;
  for(int row=0;row<6&&row+voice_member_first<count;row++){const Dnx2VoiceMember *m=&members[row+voice_member_first];int y=340+row*52;int speaking=m->self?!muted&&dnx2VoiceMicLevel()>600:m->speaking&&!m->muted;const unsigned char *picture=dnx2AuthorAvatar(m->id,m->avatar,m->guild_avatar,m->default_avatar);
   if(speaking)rounded(390,y,44,44,22,C_GREEN);
   rounded(393,y+3,38,38,19,C_CARD);if(picture)image_draw(393,y+3,38,38,picture,19);
   text_right=1110;text_line(450,y+11,m->name,C_WHITE,3,60);text_right=1256;if(m->self?muted:m->muted)text_line(1130,y+13,"Muted",C_MUTED,2,8);}
 }else text_wrap(392,230,dnx2VoiceNotice()[0]?dnx2VoiceNotice():"Join this voice channel to talk.",4,72);
}
static void voice_toolbar(void){
 if(!dnx2VoiceRunning())return;
 rect(88,574,268,72,C_DARK);text_right=258;text_line(100,584,dnx2VoiceChannelName(),C_WHITE,2,40);text_line(100,618,dnx2VoiceState()>=4?"Voice connected":"Connecting",C_GREEN,1,32);text_right=1256;
 rounded(268,600,36,36,8,dnx2VoiceMuted()?C_WARN:C_CARD);mic_icon(276,605,C_WHITE,dnx2VoiceMuted()?C_WARN:C_CARD,dnx2VoiceMuted());
 rounded(312,600,36,36,8,C_CARD);text_line(324,610,"x",C_WHITE,2,1);
}
static void draw(const char *feedback,int view,int selection,int content_focus,int qr_preview){
    (void)feedback;(void)selection;(void)content_focus;(void)qr_preview;
    const Dnx2Account *a=dnx2Account();int home=view==0;
    rect(0,0,W,H,C_BG);rect(0,0,88,H,C_DARK);rect(88,0,268,H,C_SIDEBAR);
    brand(12,10);text_right=82;
    for(int row=0;row<8;row++){
        int index=server_page*8+row;if(index>=a->server_count)break;int y=98+row*62;
        int active=!home&&!a->private_mode&&index==a->selected;
        rounded(18,y,52,52,active?15:26,active?C_PRIMARY:C_CARD);
        if(focus==0&&server_cursor==index)rounded(3,y+12,4,28,2,C_WHITE);
        char label[16]={0};const char *p=a->servers[index].name;size_t n=0;
        for(int i=0;i<2&&*p;i++){const char *start=p;dnx_utf8_next(&p);size_t k=(size_t)(p-start);if(n+k<sizeof(label)){memcpy(label+n,start,k);n+=k;}}
        const unsigned char *icon=dnx2ChatImage(1,index);
        if(icon)image_draw(18,y,52,52,icon,active?15:26);
        else {label[0]=(char)toupper((unsigned char)label[0]);text_line(29,y+14,label,C_WHITE,3,2);}
    }
    rounded(8,602,32,34,8,C_CARD);rounded(48,602,32,34,8,C_CARD);text_line(18,610,"<",C_WHITE,2,1);text_line(58,610,">",C_WHITE,2,1);
    text_line(12,670,"Exit",C_MUTED,2,5);
    text_right=344;
    text_line(104,26,home?"DiscordNX2":a->private_mode?"Direct messages":a->server_count?a->servers[a->selected].name:"Select a server",C_WHITE,3,28);
    line(88,76,268,C_LINE);
    if(home){text_line(106,112,"Your Discord on Switch 2",C_MUTED,2,26);text_line(106,158,"Tap a server to browse.",C_MUTED,2,26);}
    else for(int row=0;row<(dnx2VoiceRunning()?10:12);row++){
        int index=channel_first+row;if(index>=a->channel_count)break;const Dnx2Channel *c=&a->channels[index];int y=100+44*row;
        int active=index==a->selected_channel;
        if(active||(focus==1&&channel_cursor==index))rounded(96,y,252,40,8,active?C_CARD_HI:C_CARD);
        int voice=c->type==2||c->type==13,forum=c->type==15||c->type==16;
        char label[440];snprintf(label,sizeof(label),"%s%s",c->type==4||voice||forum||a->private_mode?"":"# ",c->name);
        uint32_t row_bg=active||(focus==1&&channel_cursor==index)?active?C_CARD_HI:C_CARD:C_SIDEBAR;
        if(voice)mic_icon(108,y+7,C_MUTED,row_bg,0);else if(forum)folder_icon(106,y+9,C_MUTED,row_bg);
        if(a->private_mode){const unsigned char *picture=dnx2DMImage(index);if(picture)image_draw(104,y+4,32,32,picture,16);else rounded(104,y+4,32,32,16,C_CARD_HI);}
        text_line(a->private_mode?146:voice||forum?140:c->type==4?104:112,y+12,label,c->type==4?C_MUTED:C_WHITE,2,30);
    }
    rect(88,650,268,70,C_DARK);text_line(104,666,a->signed_in?a->display_name:"Signed out",C_WHITE,2,30);

    text_right=1256;
    if(voice_panel&&dnx2VoiceRunning()){text_line(380,26,dnx2VoiceChannelName(),C_WHITE,3,72);line(356,76,924,C_LINE);voice_controls(1);server_tooltip();return;}
    if(home){
        text_line(392,46,"Welcome to DiscordNX2",C_WHITE,4,64);
        text_line(392,158,a->signed_in?a->display_name:"Sign in using Discord on your phone.",C_WHITE,3,70);
        item(392,254,824,90,a->signed_in?"Log out":"Log in with QR code",a->signed_in?"Forget this account and saved login":"Approve your Switch login on your phone",focus==2&&sel==0);
        item(392,366,824,90,a->signed_in?"Remember login with PIN":"Unlock saved login",a->signed_in?"Save encrypted access for future launches":"Enter your saved-login PIN",focus==2&&sel==1);
        if(auth_notice[0])text_wrap(392,504,auth_notice,4,70);
        server_tooltip();return;
    }
    if(!a->signed_in){text_line(392,100,"Open Home to sign in.",C_MUTED,3,60);return;}
    if(a->selected_channel<0){text_line(392,32,a->private_mode?"Choose a conversation":"Choose a channel",C_WHITE,3,60);if(a->notice[0])text_wrap(392,158,a->notice,4,70);server_tooltip();return;}
    const Dnx2Channel *c=&a->channels[a->selected_channel];char heading[440];snprintf(heading,sizeof(heading),"%s%s",a->private_mode?"":selected_text()?"# ":"",c->name);
    if(!selected_text())mic_icon(380,25,C_MUTED,C_BG,0);
    text_line(selected_text()?380:412,26,heading,C_WHITE,3,72);line(356,76,924,C_LINE);
    if(selected_text()){
        rounded(1234,30,8,8,4,dnx2JobBusy()?C_MUTED:!strcmp(dnx2LiveStatus(),"Live updates")?C_GREEN:C_WARN);
        if(message_expanded&&sel<a->message_count){
            const Dnx2Message *m=&a->messages[sel];render_message=m;reply_draw(100,m);int reply=m->reply_state?24:0;author_draw(104+reply,m);
            const char *text=m->content;for(int i=0;i<message_offset&&*text;i++)text=dnx_grapheme_end(text);
            const Dnx2DecodedMedia *media=dnx2MessageMedia(m->media_id);int height=media_draw_image(432,158+reply,760,330-reply,media);
            message_wrap(432,height?height+174+reply:158+reply,text,height?3:18,74);
        }else for(int index=message_first,y=100;index<a->message_count&&y<588;index++){
            const Dnx2Message *m=&a->messages[index];
            if(y+dnx_message_row_height(m->media_id[0]!=0)>588)break;
            if(focus==2&&sel==index)rounded(366,y,900,dnx_message_row_height(m->media_id[0]!=0)-6,8,C_CARD);
            render_message=m;reply_draw(y,m);int reply=m->reply_state?24:0;author_draw(y+reply,m);
            if(m->sticker_id[0]){
                const unsigned char *image=dnx2MessageImage(3,m->sticker_id);
                if(image)image_draw(1170,y+30,64,64,image,0);
                else text_line(1040,y+40,m->sticker_name,C_MUTED,2,20);
                text_right=1136;
            }
            const Dnx2DecodedMedia *media=dnx2MessageMedia(m->media_id);
            message_wrap(432,y+36+reply,m->content,m->media_id[0]||reply?2:3,74);text_right=1256;
            if(media)media_draw_image(432,y+88+reply,640,260-reply,media);
            else if(m->media_id[0]){rounded(432,y+88+reply,284,260-reply,8,C_CARD_HI);text_line(452,y+194,dnx2MessageMediaState(m->media_id)<0?"Image unavailable":"Loading image...",C_MUTED,2,28);}
            y+=dnx_message_row_height(m->media_id[0]!=0);
        }
        if(!a->message_count){
            if(dnx2JobKind()==10){for(int i=0;i<3;i++){rounded(382,116+i*122,150,14,5,C_CARD_HI);rounded(382,148+i*122,600-i*90,12,4,C_CARD);}}
            else text_line(392,158,"No messages in this channel yet.",C_MUTED,2,72);
        }
        rounded(372,626,768,58,12,focus==3?C_CARD_HI:C_CARD);
        char placeholder[460];snprintf(placeholder,sizeof(placeholder),pending_sticker[0]?"Sticker: %s":"Message #%s",pending_sticker[0]?pending_sticker_name:c->name);
        text_right=pending_sticker[0]?1030:1076;text_line(390,644,draft[0]?draft:placeholder,draft[0]?C_WHITE:C_MUTED,2,70);text_right=1256;
        text_line(1090,642,"\xe2\x98\xba",C_MUTED,3,2);
        if(pending_sticker[0])text_line(1044,644,"x",C_MUTED,2,1);
        rounded(1150,626,112,58,12,(draft[0]||pending_sticker[0])&&dnx2JobKind()!=11?C_PRIMARY:C_CARD);text_line(1170,644,"Send",draft[0]||pending_sticker[0]?C_WHITE:C_MUTED,2,8);
        text_right=1060;
        if(a->notice[0])text_line(380,600,a->notice,C_WARN,1,90);
        text_right=1256;rounded(1080,590,180,30,8,C_CARD);text_line(1154,594,"\xe2\x8c\xa8",C_MUTED,3,2);
        if(media_open)media_draw();
        if(profile_open)profile_draw();
    }else{
        voice_controls(dnx2VoiceSelected());
    }
    server_tooltip();
}
static void draw_auth(const char *phase,const char *qr_url){
    text_right=1216;
    rect(0,0,W,H,C_BG);
    brand(64,38);
    text_line(150,54,"DiscordNX2",C_WHITE,4,30);
    text_line(64,124,qr_url?"Sign in with your phone":"Discord account",C_MUTED,2,60);
    block(64,180,1152,420,C_SIDEBAR);
    if(qr_url && strncmp(qr_url,"https://discord.com/ra/",23)==0){
        qr_render_url(800,216,344,qr_url);
        text_right=760;
        text_line(100,234,"Open Discord on your phone",C_WHITE,3,50);
        text_line(100,294,"Go to Settings, then Scan QR code.",C_MUTED,2,60);
        text_line(100,338,"Approve this login on your phone.",C_MUTED,2,60);
    }else if(qr_url && strncmp(qr_url,"https://",8)==0){
        qr_render_url(754,196,420,qr_url);
        text_right=718;
        text_line(100,234,"Complete verification on your phone",C_WHITE,3,44);
        text_line(100,294,"Scan this QR with your phone camera.",C_MUTED,2,52);
        text_line(100,338,"Use the same Wi-Fi as your Switch.",C_MUTED,2,52);
        text_line(100,382,"Solve the challenge, then tap Send to Switch.",C_MUTED,2,52);
    }else{
        text_line(100,234,"DiscordNX2",C_WHITE,3,50);
        text_line(100,294,"Please wait. You can cancel with B.",C_MUTED,2,60);
    }
    text_right=qr_url?718:1180;
    char readable[128];
    snprintf(readable,sizeof(readable),"%s",phase?phase:"Connecting");
    for(size_t i=0;readable[i];i++)readable[i]=(char)tolower((unsigned char)readable[i]);
    readable[0]=(char)toupper((unsigned char)readable[0]);
    text_line(100,526,readable,C_GREEN,2,96);
    rounded(64,630,240,54,12,C_CARD);text_line(84,648,"Cancel",C_WHITE,2,60);
}
static int fallback_keyboard(Framebuffer *fb,PadState *pad){
    int cursor=10,page=0,upper=dnx_keyboard_caps(draft),armed=0,delete_touch=0;DnxDeleteRepeat repeat={0};uint64_t previous_space=0;
    for(;;){
        char suggestions[3][40];int suggestion_count=dnx_suggestions(draft,suggestions);
        DnxKey keys[40];int count=dnx_keys(keys,page,upper);if(cursor>=count)cursor=0;
        u32 pitch=0;pixels=(uint32_t*)framebufferBegin(fb,&pitch);if(!pixels)return 0;stride_pixels=(int)(pitch/4);
        draw(NULL,tab,sel,focus,0);rect(0,418,W,302,RGBA8_MAXALPHA(28,29,33));text_right=1244;
        for(int i=0;i<suggestion_count;i++){rounded(372+i*280,314,268,32,7,C_CARD_HI);text_right=632+i*280;text_line(386+i*280,321,suggestions[i],C_WHITE,2,30);}
        rounded(372,354,820,56,12,C_CARD);text_right=1168;
        const char *tail=draft;int tail_width=dnx2FontWidth(tail,2);
        while(*tail&&tail_width>760){const char *end=dnx_grapheme_end(tail);char cluster[256];dnx_cluster_text(tail,end,cluster);tail_width-=dnx2FontWidth(cluster,2);tail=end;}
        text_line(390,374,*tail?tail:"Message...",C_WHITE,2,512);
        if((ui_ms()/500)%2==0)rect(390+tail_width,370,2,28,C_WHITE);
        text_right=1244;rounded(1200,354,60,56,12,C_CARD_HI);text_line(1220,370,"x",C_WHITE,3,1);
        for(int i=0;i<count;i++){
            DnxKey *k=&keys[i];uint32_t color=k->action==DNX_KEY_TEXT||k->action==DNX_KEY_SPACE?RGBA8_MAXALPHA(79,81,88):RGBA8_MAXALPHA(46,48,54);
            if(cursor==i)color=RGBA8_MAXALPHA(94,98,108);
            if(k->action==DNX_KEY_DONE)color=C_PRIMARY;
            rounded(k->x,k->y,k->w,k->h,7,color);
            int label_width=(int)strlen(k->label)*11;if((unsigned char)k->label[0]>=128)label_width=22;
            text_line(k->x+(k->w-label_width)/2,k->y+17,k->label,C_WHITE,3,12);
        }
        framebufferEnd(fb);pixels=NULL;padUpdate(pad);u64 down=padGetButtonsDown(pad);int x=0,y=0,scroll=0,tap=touch_event(&x,&y,&scroll);
        if(!armed){if(!padGetButtons(pad)&&!touch_input.held)armed=1;svcSleepThread(16000000LL);continue;}
        if((down&(HidNpadButton_B|HidNpadButton_Plus))||(tap==1&&x>=1200&&x<1260&&y>=354&&y<410)){dnx2GraphicalClearDraft();return 0;}
        if(down&HidNpadButton_AnyLeft)cursor=dnx_key_move(keys,count,cursor,-1,0);
        if(down&HidNpadButton_AnyRight)cursor=dnx_key_move(keys,count,cursor,1,0);
        if(down&HidNpadButton_AnyUp)cursor=dnx_key_move(keys,count,cursor,0,-1);
        if(down&HidNpadButton_AnyDown)cursor=dnx_key_move(keys,count,cursor,0,1);
        int suggestion=tap==1?dnx_suggestion_hit(x,y,suggestion_count):-1;if(suggestion>=0){dnx_suggestion_pick(draft,suggestions[suggestion]);upper=dnx_keyboard_caps(draft);previous_space=0;svcSleepThread(16000000LL);continue;}
        int pressed=(down&HidNpadButton_A)?cursor:-1;
        if(tap==1)pressed=dnx_key_hit(keys,count,x,y);
        int action=pressed>=0?keys[pressed].action:-1;
        if(down&HidNpadButton_X)action=DNX_KEY_SPACE;
        if(down&HidNpadButton_Y)action=DNX_KEY_SHIFT;
        u64 held=padGetButtons(pad);int touch_delete=0;
        if(touch_input.held&&!touch_input.moved){int key=dnx_key_hit(keys,count,touch_input.start_x,touch_input.start_y);touch_delete=key>=0&&keys[key].action==DNX_KEY_DELETE;}
        if(touch_delete)delete_touch=1;
        if(!touch_input.held&&tap!=1)delete_touch=0;
        if(tap==1&&delete_touch){if(action==DNX_KEY_DELETE)action=-1;delete_touch=0;}
        if(dnx_delete_repeat(&repeat,touch_delete||(held&HidNpadButton_L)||((held&HidNpadButton_A)&&keys[cursor].action==DNX_KEY_DELETE),ui_ms()))action=DNX_KEY_DELETE;
        else if(action==DNX_KEY_DELETE)action=-1;
        if(down&HidNpadButton_R)action=DNX_KEY_DONE;
        if(action==DNX_KEY_DONE)return 1;
        if(action==DNX_KEY_EMOJI)return 2;
        if(action==DNX_KEY_SHIFT){if(page)page=page==1?2:1;else upper=!upper;}
        if(action==DNX_KEY_PAGE){page=page?0:1;cursor=0;}
        size_t n=strlen(draft);
        if(action==DNX_KEY_DELETE&&n){n=dnx_grapheme_previous(draft);draft[n]=0;upper=dnx_keyboard_caps(draft);previous_space=0;}
        if(action==DNX_KEY_SPACE){dnx_keyboard_space(draft,ui_ms(),&previous_space);upper=dnx_keyboard_caps(draft);n=strlen(draft);}
        if(action==DNX_KEY_TEXT&&pressed>=0){size_t k=strlen(keys[pressed].label);if(n+k<=512){memcpy(draft+n,keys[pressed].label,k+1);upper=dnx_keyboard_caps(draft);previous_space=0;}}
        svcSleepThread(16000000LL);
    }
}
static void scroll_messages(int delta){
    const Dnx2Account *a=dnx2Account();int max=message_bottom();
    if(message_expanded){message_offset+=delta*296;if(message_offset<0)message_offset=0;
        const char *p=a->messages[sel].content;int count=0;while(*p){p=dnx_grapheme_end(p);count++;}if(message_offset>=count)message_offset=count>296?count-296:0;return;}
    message_first+=delta;if(message_first<0)message_first=0;if(message_first>max)message_first=max;follow_latest=message_first==max;sel=message_first;
}
static int open_channel(int index,int user_select){
    const Dnx2Account *a=dnx2Account();if(index<0||index>=a->channel_count)return 0;
    int type=a->channels[index].type;if(type==4)return 0;
    if(type!=0&&type!=5&&type!=1&&type!=3&&type!=2&&type!=13){snprintf(keyboard_notice,sizeof(keyboard_notice),"Forum threads are not available yet.");return 0;}
    profile_open=voice_panel=voice_action=voice_member_first=0;
    if(index!=a->selected_channel)dnx2GraphicalClearDraft();
    dnx2JobStop();dnx2LiveStop();dnx2ChannelSelect(index);channel_cursor=index;tab=(type==0||type==5||type==1||type==3)?2:3;focus=2;message_expanded=0;message_offset=0;follow_latest=1;sel=0;keyboard_notice[0]=0;
    if(tab==2)dnx2JobStart(10,NULL,NULL);else if(user_select&&dnx2VoiceRunning()&&!dnx2VoiceSelected())dnx2VoiceStart();
    return 0;
}
int dnx2GraphicalMenu(void){
    Result rc=hidInitialize();if(R_FAILED(rc))return -1;padConfigureInput(1,HidNpadStyleSet_NpadStandard);touch_init();
    Framebuffer fb={0};rc=framebufferCreate(&fb,nwindowGetDefault(),W,H,PIXEL_FORMAT_RGBA_8888,2);
    if(R_FAILED(rc)){hidExit();return -1;}rc=framebufferMakeLinear(&fb);if(R_FAILED(rc)){framebufferClose(&fb);hidExit();return -1;}
    PadState pad={0};padInitializeDefault(&pad);int action=0,dirty=1,analog_prior=0,input_armed=0;const char *last_live="";uint64_t animation_tick=0;unsigned voice_wait=0,voice_status=0,voice_revision=0,mic_indicator=0;
    (void)dnx2VoiceBackgroundAvailable();
    if(!dnx2Account()->signed_in)tab=0;
    if(tab==2&&selected_text()){dnx2LiveStart();if(follow_latest)message_first=message_bottom();}
    while(1){
        const Dnx2Account *a=dnx2Account();
        unsigned wait=dnx2VoiceWaitSeconds();if(wait!=voice_wait){voice_wait=wait;dirty=1;}
        unsigned state=dnx2VoiceState();if(state!=voice_status){voice_status=state;dirty=1;}
        unsigned mic=dnx2VoiceMicLevel()>600;if(mic!=mic_indicator){mic_indicator=mic;dirty=1;}
        if(dnx2VoiceApply()){if(!dnx2VoiceRunning())voice_panel=0;dirty=1;}unsigned rev=dnx2VoiceMembersRevision();if(rev!=voice_revision){voice_revision=rev;dirty=1;}
        int completed=dnx2JobApply();
        if(completed){
            if(completed==9||completed==23){const char *last=dnx2LastChannel();for(int i=0;i<a->channel_count;i++)if(last[0]&&!strcmp(last,a->channels[i].id)){channel_first=i>5?i-5:0;open_channel(i,0);break;}}
            if(completed==11&&!strcmp(draft,dnx2JobDraft())&&!strcmp(pending_sticker,dnx2JobSticker()))dnx2GraphicalClearDraft();
            if((completed==10||completed==11)&&follow_latest)message_first=message_bottom();
            dirty=1;
        }
        if(!dnx2JobBusy()&&a->signed_in&&(a->server_count||a->private_mode)){
            if(tab==2&&selected_text())dnx2LiveStart();
            int requested=dnx2VoiceRunning()?dnx2RequestVoiceImages():0;
            if(!requested)requested=dnx2RequestImages(1,server_page*8,8);
            if(!requested&&a->private_mode)requested=dnx2RequestDMImages(channel_first,12);
            if(!requested&&media_open&&media_tab)requested=dnx2RequestImages(media_tab==1?2:3,media_page*24,24);
            if(!requested&&tab==2&&selected_text())requested=dnx2RequestMessageImages(message_expanded?sel:message_first,message_expanded?1:4);
            if(!requested&&tab==2&&selected_text())requested=dnx2RequestProfiles(profile_open||message_expanded?sel:message_first,profile_open||message_expanded?1:4);
            if(!requested&&tab==2&&selected_text())dnx2LiveStart();
        }
        if(tab==2&&selected_text()){
            char anchor[24]={0};if(message_first<a->message_count)snprintf(anchor,sizeof(anchor),"%s",a->messages[message_first].id);
            if(!message_expanded&&!profile_open&&dnx2LiveApply()){
                if(follow_latest)message_first=message_bottom();
                else for(int i=0;i<a->message_count;i++)if(anchor[0]&&!strcmp(a->messages[i].id,anchor)){message_first=i;break;}
                if(message_first>=a->message_count)message_first=message_bottom();
                if(sel>=a->message_count)sel=a->message_count?a->message_count-1:0;
                dirty=1;
            }
            const char *state=dnx2LiveStatus();if(state!=last_live){last_live=state;dirty=1;}
        }
        if(tab==2&&selected_text()&&ui_ms()-animation_tick>=80){
            animation_tick=ui_ms();for(int i=message_expanded?sel:message_first;i<a->message_count&&i<(message_expanded?sel+1:message_first+4);i++){const Dnx2DecodedMedia *media=dnx2MessageMedia(a->messages[i].media_id);if(media&&media->frames>1)dirty=1;}
        }
        if(dirty){u32 stride=0;pixels=(uint32_t*)framebufferBegin(&fb,&stride);if(!pixels){action=-1;break;}stride_pixels=(int)(stride/4);draw(NULL,tab,sel,focus,0);voice_toolbar();framebufferEnd(&fb);pixels=NULL;dirty=0;}
        padUpdate(&pad);u64 down=padGetButtonsDown(&pad);HidAnalogStickState stick=padGetStickPos(&pad,0);
        int analog=stick.x<-16000?1:stick.x>16000?2:stick.y>16000?3:stick.y<-16000?4:0;
        if(analog&&analog!=analog_prior)down|=analog==1?HidNpadButton_Left:analog==2?HidNpadButton_Right:analog==3?HidNpadButton_Up:HidNpadButton_Down;
        analog_prior=analog;
        int x=0,y=0,scroll=0,touch=touch_event(&x,&y,&scroll);
        if(!input_armed){if(!padGetButtons(&pad)&&!touch_input.held)input_armed=1;down=0;touch=0;}
        if(down&HidNpadButton_Plus)break;
        if(tab==0&&(padGetButtons(&pad)&HidNpadButton_R)&&(down&HidNpadButton_X)){action=7;break;}
        if(tab==0&&(padGetButtons(&pad)&HidNpadButton_R)&&(down&HidNpadButton_Y)){action=8;break;}
        if(profile_open){if((down&(HidNpadButton_B|HidNpadButton_Plus))||(touch==1&&(x<560||x>=1200||y<108||y>=586||(x>=1144&&y<158)))){profile_open=0;dirty=1;}down=0;touch=0;}
        if((down&HidNpadButton_L)&&selected_text()&&a->message_count&&focus==2){profile_message=a->messages[message_expanded?sel:message_first];profile_open=1;dirty=1;down=0;}
        int choose_server=-2,choose_channel=-1,keyboard=0,send=0,account_action=-1;
        if(media_open){
            if(touch==2&&x>=372&&y>=292){int max=(media_count()-1)/24;media_page+=scroll;if(media_page<0)media_page=0;if(media_page>max)media_page=max;touch=0;dirty=1;}
            if(touch==1&&x>=372&&y>=292&&y<618){
                if(y>=302&&y<338){if(x>=1200)media_open=0;else {int t=(x-388)/210;if(t>=0&&t<3){media_tab=t;media_page=media_cursor=0;}}}
                else if(y>=348&&y<594&&x>=388&&x<1236){int cell=(y-348)/82*8+(x-388)/106;if(cell>=0&&cell<24)media_choose(media_page*24+cell);}
                touch=0;dirty=1;
            }
            if(down&HidNpadButton_B){media_open=0;down&=~HidNpadButton_B;dirty=1;}
            if(down&HidNpadButton_L){media_tab=(media_tab+2)%3;media_page=media_cursor=0;down&=~HidNpadButton_L;dirty=1;}
            if(down&HidNpadButton_R){media_tab=(media_tab+1)%3;media_page=media_cursor=0;down&=~HidNpadButton_R;dirty=1;}
            int move=(down&HidNpadButton_AnyLeft)?-1:(down&HidNpadButton_AnyRight)?1:(down&HidNpadButton_AnyUp)?-8:(down&HidNpadButton_AnyDown)?8:0;
            if(move){media_cursor+=move;if(media_cursor<0)media_cursor=0;if(media_cursor>=media_count())media_cursor=media_count()?media_count()-1:0;media_page=media_cursor/24;down&=~(HidNpadButton_AnyLeft|HidNpadButton_AnyRight|HidNpadButton_AnyUp|HidNpadButton_AnyDown);dirty=1;}
            if(down&HidNpadButton_A){media_choose(media_cursor);down&=~HidNpadButton_A;dirty=1;}
        }
        if(touch==2){
            if(x<88){server_page+=scroll;if(server_page<0)server_page=0;int max=(a->server_count-1)/8;if(server_page>max)server_page=max;}
            else if(x<356){channel_first+=scroll;if(channel_first<0)channel_first=0;int rows=dnx2VoiceRunning()?10:12;int max=a->channel_count>rows?a->channel_count-rows:0;if(channel_first>max)channel_first=max;}
            else if(selected_text())scroll_messages(scroll);
            dirty=1;
        }
        if(touch==1&&dnx2VoiceRunning()&&x>=88&&x<356&&y>=574&&y<646){if(x>=312&&y>=600)dnx2VoiceStop();else if(x>=268&&y>=600)dnx2VoiceToggleMute();else {voice_panel=1;focus=2;voice_action=0;}touch=0;dirty=1;}
        if(touch==2&&(voice_panel||tab==3)&&x>=356){voice_member_first+=scroll;if(voice_member_first<0)voice_member_first=0;touch=0;dirty=1;}
        if(touch==1){DnxHit hit=(voice_panel&&x>=356)||(dnx2VoiceRunning()&&x>=88&&x<356&&y>=540&&y<574)?(DnxHit){0}:dnx_chat_hit(x,y,server_page,channel_first,message_first,tab==0,selected_text());
            if(hit.kind==DNX_MESSAGE){message_geometry();hit.index=message_expanded?sel:dnx_message_hit(message_images,a->message_count,message_first,y);if(hit.index<0)hit.kind=0;}
            if(hit.kind==DNX_HOME){profile_open=0;media_open=0;dnx2GraphicalClearDraft();if(a->signed_in){dnx2OpenDMList();tab=4;focus=1;channel_first=channel_cursor=0;message_expanded=0;}else{tab=0;focus=2;}sel=0;dirty=1;}
            else if(hit.kind==DNX_ACCOUNT){voice_panel=0;dnx2JobStop();dnx2LiveStop();profile_open=media_open=0;tab=0;focus=2;sel=0;dirty=1;}
            else if(hit.kind==DNX_SERVER)choose_server=hit.index;
            else if(hit.kind==DNX_CHANNEL){choose_channel=hit.index;focus=1;}
            else if(hit.kind==DNX_INPUT)keyboard=1;
            else if(hit.kind==DNX_MEDIA){media_begin();dirty=1;}
            else if(hit.kind==DNX_CLEAR_STICKER&&dnx2JobKind()!=11){if(pending_sticker[0])pending_sticker[0]=pending_sticker_name[0]=0;else keyboard=1;dirty=1;}
            else if(hit.kind==DNX_FALLBACK)keyboard=2;
            else if(hit.kind==DNX_SEND)send=1;
            else if(hit.kind==DNX_MESSAGE&&hit.index>=0&&hit.index<a->message_count){int top=message_expanded?104:100;for(int i=message_first;!message_expanded&&i<hit.index;i++)top+=dnx_message_row_height(message_images[i]);const Dnx2Message *m=&a->messages[hit.index];int inset=m->reply_state?24:0;if(author_hit(x,y,top+inset,m)){profile_message=*m;profile_open=1;sel=hit.index;}
                else if(m->reply_state&&y<top+24){for(int i=0;i<a->message_count;i++)if(!strcmp(a->messages[i].id,m->reply_id)){sel=i;message_expanded=1;message_offset=0;break;}}
                else {if(!message_expanded)sel=hit.index;message_expanded=!message_expanded;message_offset=0;}focus=2;dirty=1;}
            else if(hit.kind==DNX_PREVIOUS||hit.kind==DNX_NEXT){server_page+=hit.kind==DNX_NEXT?1:-1;if(server_page<0)server_page=0;int max=(a->server_count-1)/8;if(server_page>max)server_page=max;dirty=1;}
            else if(hit.kind==DNX_LOGIN)account_action=0;
            else if(hit.kind==DNX_REMEMBER)account_action=1;
            else if(hit.kind==DNX_EXIT)break;
            if((voice_panel||(!selected_text()&&tab==3))&&dnx2VoiceRunning()&&(voice_panel||dnx2VoiceSelected())&&x>=1054&&x<1244&&y>=244&&y<308&&dnx2VoiceBackgroundAvailable()&&!dnx2VoiceBackground()){dnx2VoiceBackgroundStart();dirty=1;}
            if((voice_panel||(!selected_text()&&tab==3))&&x>=392&&x<1042){if(y>=124&&y<188){if(voice_panel||dnx2VoiceSelected())dnx2VoiceStop();else dnx2VoiceStart();dirty=1;}else if(y>=244&&y<308&&(voice_panel||dnx2VoiceSelected())&&dnx2VoiceRunning()){dnx2VoiceToggleMute();dirty=1;}}

        }
        if((down&HidNpadButton_Y)&&(voice_panel||tab==3)&&dnx2VoiceRunning()&&(voice_panel||dnx2VoiceSelected())&&dnx2VoiceBackgroundAvailable()&&!dnx2VoiceBackground()){dnx2VoiceBackgroundStart();down&=~HidNpadButton_Y;dirty=1;}
        if((down&HidNpadButton_X)&&dnx2VoiceRunning()&&(voice_panel||!selected_text())){dnx2VoiceToggleMute();dirty=1;}
        if((down&HidNpadButton_B)&&voice_panel){voice_panel=0;dirty=1;down&=~HidNpadButton_B;}
        if(down&HidNpadButton_B){if(message_expanded){message_expanded=0;message_offset=0;}else if(focus>0)focus--;else{dnx2LiveStop();tab=0;}dirty=1;}
        if(down&HidNpadButton_AnyLeft){if(focus>0)focus--;dirty=1;}
        if(down&HidNpadButton_AnyRight){if(focus<3)focus++;dirty=1;}
        int delta=(down&HidNpadButton_AnyUp)?-1:(down&HidNpadButton_AnyDown)?1:0;
        if(delta){
            if(focus==0){server_cursor+=delta;if(server_cursor<-1)server_cursor=a->server_count-1;if(server_cursor>=a->server_count)server_cursor=-1;if(server_cursor>=0)server_page=server_cursor/8;}
            else if(tab==0){sel=(sel+1)%2;}
            else if(focus==1){channel_cursor+=delta;if(channel_cursor<0)channel_cursor=0;if(channel_cursor>=a->channel_count)channel_cursor=a->channel_count?a->channel_count-1:0;if(channel_cursor<channel_first)channel_first=channel_cursor;int rows=dnx2VoiceRunning()?10:12;if(channel_cursor>=channel_first+rows)channel_first=channel_cursor-rows+1;}
            else if(voice_panel||tab==3)voice_action^=1;
            else if(selected_text())scroll_messages(delta);
            dirty=1;
        }
        if(down&HidNpadButton_A){
            if(voice_panel&&focus>=2){if(voice_action==1)dnx2VoiceToggleMute();else {dnx2VoiceStop();voice_panel=0;}dirty=1;}
            else if(focus==0){if(server_cursor<0){if(a->signed_in){dnx2OpenDMList();tab=4;focus=1;channel_first=channel_cursor=0;}else{dnx2LiveStop();tab=0;focus=2;}sel=0;dirty=1;}else choose_server=server_cursor;}
            else if(tab==0)account_action=sel;
            else if(focus==1)choose_channel=channel_cursor;
            else if(focus==3)keyboard=1;
            else if(tab==3||voice_panel){if(voice_action==1&&dnx2VoiceRunning()&&(voice_panel||dnx2VoiceSelected()))dnx2VoiceToggleMute();else if(voice_panel||dnx2VoiceSelected())dnx2VoiceStop();else dnx2VoiceStart();dirty=1;}
            else if(selected_text()&&a->message_count){sel=message_first;message_expanded=!message_expanded;message_offset=0;dirty=1;}
        }
        if(account_action>=0){dnx2JobStop();dnx2LiveStop();if(account_action==1){action=a->signed_in?12:13;break;}
            if(a->signed_in){dnx2AccountLogout();dnx2GraphicalClearDraft();tab=0;focus=2;dirty=1;}else{action=6;break;}}
        if(choose_server>=0&&choose_server<a->server_count&&dnx2JobKind()!=11){voice_panel=0;dnx2JobStop();dnx2LiveStop();media_open=0;dnx2GraphicalClearDraft();dnx2AccountSelect(choose_server);server_cursor=choose_server;server_page=choose_server/8;channel_first=channel_cursor=0;tab=1;focus=1;profile_open=0;message_expanded=0;dnx2JobStart(9,NULL,NULL);dirty=1;}
        if(choose_channel>=0&&dnx2JobKind()!=11){int next=open_channel(choose_channel,1);dirty=1;if(next){action=next;break;}}
        if((down&HidNpadButton_X)&&tab!=0&&selected_text())keyboard=1;
        if((down&HidNpadButton_R)&&tab!=0&&selected_text())send=1;
        if((down&HidNpadButton_Y)&&tab!=0&&dnx2JobKind()!=11){dnx2JobStart(selected_text()?10:a->private_mode?23:9,NULL,NULL);dirty=1;}
        if(keyboard&&!voice_panel&&selected_text()&&dnx2JobKind()!=11){
            dnx2JobStop();focus=3;
            int result=fallback_keyboard(&fb,&pad);if(result==2)media_begin();if(result==1)send=1;dirty=1;
        }
        if(send&&!voice_panel&&selected_text()&&(draft[0]||pending_sticker[0])&&dnx2JobKind()!=11){media_open=0;dnx2JobStart(11,draft,pending_sticker);dirty=1;}
        svcSleepThread(16000000LL);
    }
    if(action<=0)dnx2VoiceAppExit();
    dnx2JobStop();dnx2LiveStop();framebufferClose(&fb);hidExit();return action;
}

/* Dedicated auth framebuffer. The normal menu releases its framebuffer before
 * RSA/TLS start; these callbacks only render actual session-bound URLs.
 * Press B or + to cancel before the QR session expires. */
static Framebuffer auth_fb;
static int auth_fb_ready=0,auth_hid_ready=0;
static PadState auth_pad;
static int browser_answer_armed;
void dnx2GraphicalAuthBegin(void) {
    browser_answer_armed=0;
    Result rc=hidInitialize();
    if(R_SUCCEEDED(rc)){
        auth_hid_ready=1;
        padConfigureInput(1,HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&auth_pad);touch_init();
    }
    rc=framebufferCreate(&auth_fb,nwindowGetDefault(),W,H,PIXEL_FORMAT_RGBA_8888,2);
    if(R_SUCCEEDED(rc)){
        rc=framebufferMakeLinear(&auth_fb);
        if(R_SUCCEEDED(rc))auth_fb_ready=1;
        else framebufferClose(&auth_fb);
    }
}
int dnx2GraphicalAuthCancel(void) {
    if(!auth_hid_ready)return 0;
    padUpdate(&auth_pad);
    u64 buttons=padGetButtonsDown(&auth_pad);
    int x=0,y=0,scroll=0;int tap=touch_event(&x,&y,&scroll);
    return !!(buttons&(HidNpadButton_B|HidNpadButton_Plus))||(tap==1&&y>=620);
}
void dnx2GraphicalAuthStatus(const char *phase,const char *qr_url) {
    snprintf(auth_notice,sizeof(auth_notice),"%s",phase?phase:"");
    if(!auth_fb_ready)return;
    u32 stride=0;
    pixels=(uint32_t*)framebufferBegin(&auth_fb,&stride);
    if(!pixels)return;
    stride_pixels=(int)(stride/4);
    draw_auth(phase,qr_url);
    framebufferEnd(&auth_fb);
    pixels=NULL;
}
void dnx2GraphicalAuthEnd(void) {
    if(auth_fb_ready)framebufferClose(&auth_fb);
    if(auth_hid_ready)hidExit();
    auth_fb_ready=0;auth_hid_ready=0;
}


void dnx2GraphicalBrowserCheckStatus(const char *message, int complete) {
    if(!auth_fb_ready)return;
    u32 stride=0;
    pixels=(uint32_t*)framebufferBegin(&auth_fb,&stride);
    if(!pixels)return;
    stride_pixels=(int)(stride/4);
    text_right=1240;
    rect(0,0,W,H,C_BG);
    brand(48,42);
    text_line(132,62,"Browser compatibility",C_WHITE,3,48);
    block(48,156,1184,388,C_CARD);
    text_line(80,196,message,C_WHITE,3,64);
    text_line(80,276,"Checking support needed to run Chromium on this console.",C_MUTED,2,80);
    text_line(80,324,"This does not open a browser or sign in to Discord.",C_MUTED,2,80);
    text_line(80,398,complete?"Results saved to the hardware log.":"This should take a moment.",C_MUTED,2,80);
    text_line(80,466,complete?"B: Back":"Please wait",C_WHITE,2,50);
    framebufferEnd(&auth_fb);
    pixels=NULL;
}


int dnx2GraphicalBrowserAnswer(void) {
    if(!auth_hid_ready)return -2;
    padUpdate(&auth_pad);
    u64 held=padGetButtons(&auth_pad);
    if(!browser_answer_armed) {
        if(!(held&(HidNpadButton_A|HidNpadButton_B|HidNpadButton_Plus)))browser_answer_armed=1;
        return 0;
    }
    u64 down=padGetButtonsDown(&auth_pad);
    if(down&HidNpadButton_A)return 1;
    if(down&(HidNpadButton_B|HidNpadButton_Plus))return -1;
    return 0;
}
int dnx2GraphicalWifiBrowserStatus(const char *message,int question) {
    if(!auth_fb_ready)return 0;
    u32 stride=0;
    pixels=(uint32_t*)framebufferBegin(&auth_fb,&stride);
    if(!pixels)return 0;
    stride_pixels=(int)(stride/4);text_right=1216;
    rect(0,0,W,H,C_BG);brand(48,42);
    text_line(132,62,"System browser test",C_WHITE,3,48);
    block(48,156,1184,388,C_CARD);
    text_line(80,196,message,C_WHITE,3,72);
    text_line(80,276,question?"Only choose Yes if the page said Example Domain.":"This test opens a public page without signing in.",C_MUTED,2,90);
    text_line(80,324,question?"An error dialog or blank screen counts as No.":"No DNS settings, loader or plugin changes are made.",C_MUTED,2,90);
    text_line(80,398,"Results are saved to the hardware log.",C_MUTED,2,80);
    text_line(80,466,question?"A: Yes    B: No":"Close the browser to return here.",C_WHITE,2,70);
    framebufferEnd(&auth_fb);pixels=NULL;return 1;
}

/* Controller PIN input uses native rendering, not the unavailable web applet. */
int dnx2GraphicalPinForLength(const char *title,char pin[9],int length) {
    int choose=length==0;if(choose)length=dnx2SessionPinDigits();if(length!=8)length=4;
    memset(pin,0,9);memset(pin,'0',(size_t)length);int digit=0,armed=0,ok=0;
    dnx2GraphicalAuthBegin();
    if(!auth_hid_ready||!auth_fb_ready)goto done;
    for(;;){
        u32 pitch=0;pixels=(uint32_t*)framebufferBegin(&auth_fb,&pitch);if(!pixels)break;
        stride_pixels=(int)(pitch/4);text_right=1216;rect(0,0,W,H,C_BG);
        text_line(64,60,title,C_WHITE,4,72);
        char hint[128];snprintf(hint,sizeof(hint),"Your %d-digit PIN protects the saved login on this SD card.",length);text_line(64,144,hint,C_MUTED,2,90);
        text_line(64,190,"Choose a PIN you can remember; a forgotten PIN requires QR login.",C_MUTED,2,90);
        if(choose){rounded(64,218,160,48,10,length==4?C_PRIMARY:C_CARD);text_line(84,234,"4 digits",C_WHITE,2,20);rounded(240,218,160,48,10,length==8?C_PRIMARY:C_CARD);text_line(260,234,"8 digits",C_WHITE,2,20);text_line(432,234,"Y: Switch length",C_MUTED,2,40);}
        int first=(1280-(length*130-28))/2;
        for(int i=0;i<length;i++){
            int x=first+i*130;rounded(x,290,102,100,16,i==digit?C_PRIMARY:C_CARD);
            char shown[2]={i==digit?pin[i]:'*',0};text_line(x+40,326,shown,C_WHITE,4,1);
        }
        const char *buttons[]={"Up","Down","Next / Finish","Cancel"};
        for(int i=0;i<4;i++){rounded(64+i*290,430,270,48,10,C_CARD);text_line(84+i*290,446,buttons[i],C_WHITE,2,24);}
        text_line(64,500,"Up/Down: Change digit    Left/Right: Move    A: Next / Finish",C_MUTED,2,100);
        text_line(64,554,"B / +: Cancel",C_WHITE,2,64);
        framebufferEnd(&auth_fb);pixels=NULL;padUpdate(&auth_pad);
        u64 down=padGetButtonsDown(&auth_pad);
        int tx=0,ty=0,scroll=0;int tap=touch_event(&tx,&ty,&scroll);
        if(tap==1){
            if(choose&&ty>=218&&ty<266&&tx>=64&&tx<400){int wanted=tx<240?4:8;if(wanted!=length)down|=HidNpadButton_Y;}
            if(ty>=290&&ty<390&&tx>=first&&tx<first+length*130){digit=(tx-first)/130;if(digit>=length)digit=length-1;}
            if(ty>=430&&ty<478&&tx>=64){int button=(tx-64)/290;down|=button==0?HidNpadButton_Up:button==1?HidNpadButton_Down:button==2?HidNpadButton_A:HidNpadButton_B;}
        }
        if(!armed){if(!padGetButtons(&auth_pad))armed=1;svcSleepThread(16000000LL);continue;}
        if(down&(HidNpadButton_B|HidNpadButton_Plus))break;
        if(choose&&(down&HidNpadButton_Y)){length=length==4?8:4;memset(pin,0,9);memset(pin,'0',(size_t)length);digit=0;}
        if(down&HidNpadButton_AnyUp)pin[digit]=(char)('0'+(pin[digit]-'0'+1)%10);
        if(down&HidNpadButton_AnyDown)pin[digit]=(char)('0'+(pin[digit]-'0'+9)%10);
        if(down&HidNpadButton_AnyLeft)digit=(digit+length-1)%length;
        if(down&HidNpadButton_AnyRight)digit=(digit+1)%length;
        if(down&HidNpadButton_A){if(digit==length-1){ok=1;break;}digit++;}
        svcSleepThread(16000000LL);
    }
done:
    dnx2GraphicalAuthEnd();if(!ok)memset(pin,0,9);return ok;
}

int dnx2GraphicalPin(const char *title,char pin[9]){return dnx2GraphicalPinForLength(title,pin,0);}
