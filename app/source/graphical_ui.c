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

#define W 1280
#define H 720
#define C_BG       RGBA8_MAXALPHA(18,20,28)
#define C_SIDEBAR  RGBA8_MAXALPHA(27,30,42)
#define C_CARD     RGBA8_MAXALPHA(37,41,57)
#define C_CARD_HI  RGBA8_MAXALPHA(54,61,85)
#define C_PRIMARY  RGBA8_MAXALPHA(100,93,229)
#define C_GREEN    RGBA8_MAXALPHA(65,196,120)
#define C_WHITE    RGBA8_MAXALPHA(234,238,249)
#define C_MUTED    RGBA8_MAXALPHA(150,160,180)
#define C_WARN     RGBA8_MAXALPHA(242,175,86)
#define C_DARK     RGBA8_MAXALPHA(10,14,22)
#define C_LINE     RGBA8_MAXALPHA(68,74,92)

static uint32_t *pixels;
static int stride_pixels;
static int text_right=1240;
static char auth_notice[128];
static int tab=0,sel=0,focus=0,channel_view=0,message_view=0;
static int message_expanded=0,message_offset=0;
static char draft[513];
const char *dnx2GraphicalDraft(void){return draft;}
void dnx2GraphicalClearDraft(void){volatile char *p=draft;for(size_t i=0;i<sizeof(draft);i++)p[i]=0;}
static int channel_at(int view,int row){
    const Dnx2Account *p=dnx2Account();
    for(int i=0;i<p->channel_count;i++){
        int t=p->channels[i].type;
        if(view==1||(view==2&&(t==0||t==5))||(view==3&&(t==2||t==13)))if(row--==0)return i;
    }return -1;
}
static int channel_options(int view){int n=0;while(channel_at(view,n)>=0)n++;return n;}

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
        char linebuf[512];size_t n=0;
        for(int col=0;col<columns&&*p;col++){
            const char *start=p;dnx_utf8_next(&p);size_t k=(size_t)(p-start);
            if(n+k>=sizeof(linebuf))break;
            memcpy(linebuf+n,start,k);n+=k;
        }linebuf[n]=0;text_line(x,y+row*23,linebuf,C_WHITE,2,columns);
    }
}
static void draw(const char *feedback,int view,int selection,int content_focus,int qr_preview) {
    (void)feedback;(void)qr_preview;
    const Dnx2Account *p=dnx2Account();
    const char *tabs[]={"Dashboard","Servers","Text chat","Voice chat"};
    rect(0,0,W,H,C_BG);rounded(16,16,252,688,20,C_SIDEBAR);text_right=250;
    brand(28,30);text_line(104,50,"DiscordNX2",C_WHITE,2,20);text_line(32,105,"Switch 2 Native",C_MUTED,2,24);
    for(int t=0;t<4;t++){
        int y=180+t*76;if(t==view)rounded(28,y,228,56,14,content_focus==0?C_CARD_HI:C_CARD);
        if(t==view)rounded(28,y+14,4,28,2,C_PRIMARY);
        text_line(48,y+17,tabs[t],t==view?C_WHITE:C_MUTED,3,24);
    }
    text_right=1240;text_line(312,48,tabs[view],C_WHITE,4,32);line(312,112,920,C_LINE);
    if(view==0){
        text_line(344,154,p->signed_in?p->display_name:"Welcome to DiscordNX2",C_WHITE,3,64);
        item(312,220,920,96,p->signed_in?"Log out":"Log in with QR code",p->signed_in?"Forget this account and its saved login":"Scan and approve on your phone",content_focus==1&&selection==0);
        item(312,338,920,96,p->signed_in?"Remember login with PIN":"Unlock saved login",p->signed_in?"Save encrypted access for future launches":dnx2SessionExists()?"Enter your PIN instead of scanning QR":"No saved login on this SD card",content_focus==1&&selection==1);
        if(auth_notice[0])text_wrap(344,476,auth_notice,3,72);
    }else if(!p->signed_in){text_line(344,200,"Log in from Dashboard to continue.",C_MUTED,2,64);
    }else if(view==1&&!channel_view){
        int start=(selection/7)*7;
        if(!p->server_count)text_line(344,200,"This account has no servers.",C_MUTED,2,64);
        for(int i=start;i<start+7&&i<p->server_count;i++){
            int y=152+(i-start)*64;rounded(312,y,920,52,12,content_focus==1&&selection==i?C_CARD_HI:C_CARD);
            text_line(344,y+16,p->servers[i].name,C_WHITE,2,70);
        }
        text_line(312,620,p->order_synced?"Your Discord server order":"Discord order unavailable; showing API order",C_MUTED,2,80);
    }else if(view==2&&message_view){
        if(p->selected_channel>=0)text_line(312,134,p->channels[p->selected_channel].name,C_WHITE,3,64);
        if(message_expanded&&selection<p->message_count){
            const Dnx2Message *m=&p->messages[selection];text_line(332,182,m->author,C_MUTED,2,70);
            const char *content=m->content;for(int i=0;i<message_offset&&*content;i++)dnx_utf8_next(&content);
            text_wrap(332,226,content,16,66);
            text_line(312,620,"Up/Down: Scroll    B: Message list",C_MUTED,2,80);
        }else{
            int start=(selection/4)*4;
            for(int i=start;i<start+4&&i<p->message_count;i++){
                int y=184+(i-start)*104;rounded(312,y,920,96,12,selection==i&&content_focus?C_CARD_HI:C_CARD);text_line(332,y+8,p->messages[i].author,C_MUTED,2,64);
                text_wrap(332,y+34,p->messages[i].content,2,66);
            }
            if(!p->message_count&&!p->notice[0])text_line(344,220,"No messages returned for this channel.",C_MUTED,2,64);
            text_line(312,620,"A: Read    X: Write    Y: Refresh    B: Channels",C_MUTED,2,80);
        }
    }else if(view==3&&message_view){
        text_line(344,170,p->server_count?p->servers[p->selected].name:"No server selected",C_WHITE,3,64);
        if(p->selected_channel>=0)text_line(344,224,p->channels[p->selected_channel].name,C_WHITE,3,64);
        text_line(344,300,"Not connected",C_WARN,3,64);
        text_wrap(344,350,"Voice calling is still being implemented. Selecting a channel does not join a call.",3,64);
        text_line(344,480,"Members: unavailable until voice is connected",C_MUTED,2,72);
    }else{
        if(!p->server_count)text_line(344,200,"Select a server first.",C_MUTED,2,64);
        else{
            text_line(312,130,p->servers[p->selected].name,C_MUTED,2,70);
            int count=channel_options(view),start=(selection/7)*7;
            for(int i=start;i<start+7&&i<count;i++){
                int idx=channel_at(view,i),y=170+(i-start)*61;const Dnx2Channel *c=&p->channels[idx];char label[460];
                const char *prefix=c->type==4?"-- ":c->type==2||c->type==13?"Voice: ":c->type==15||c->type==16?"Forum: ":"# ";
                snprintf(label,sizeof(label),"%s%s",prefix,c->name);
                rounded(312,y,920,52,12,content_focus==1&&selection==i?C_CARD_HI:C_CARD);
                text_line(334,y+16,label,c->type==4?C_MUTED:C_WHITE,2,70);
            }
            if(!count&&!p->notice[0])text_line(344,220,p->channels_loaded?"No channels of this type returned.":"Select a server to load its channels.",C_MUTED,2,72);
            text_line(312,620,"Y: Refresh channels    B: Back",C_MUTED,2,72);
        }
    }
    if(p->notice[0]&&view!=0)text_line(312,644,p->notice,C_WARN,2,90);
    text_line(312,684,"B: Back    A: Select    +: Exit",C_MUTED,2,60);
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
    text_line(64,650,"B: Cancel    +: Cancel",C_MUTED,2,60);
}
static int options_for(int view){
    const Dnx2Account *p=dnx2Account();if(view==0)return 2;if(!p->signed_in)return 0;
    if(view==1&&!channel_view)return p->server_count;
    if(view==2&&message_view)return p->message_count;
    if(view==3&&message_view)return 0;
    return channel_options(view);
}
static int compose(Framebuffer *fb,PadState *pad){
    static const char keys[]="abcdefghijklmnopqrstuvwxyz0123456789.,!?'-:;/@#&()_+=*\"";
    int cell=0,upper=0,armed=0;size_t length=0;dnx2GraphicalClearDraft();
    for(;;){
        u32 pitch=0;pixels=(uint32_t*)framebufferBegin(fb,&pitch);if(!pixels)return 0;stride_pixels=(int)(pitch/4);text_right=1200;
        rect(0,0,W,H,C_BG);text_line(64,40,"Write a message",C_WHITE,4,64);text_wrap(64,108,draft,4,80);
        for(int i=0;i<(int)strlen(keys);i++){
            int x=64+(i%10)*108,y=236+(i/10)*54;rounded(x,y,94,46,10,cell==i?C_PRIMARY:C_CARD);
            char c[2]={upper?(char)toupper((unsigned char)keys[i]):keys[i],0};text_line(x+34,y+10,c,C_WHITE,3,1);
        }
        text_line(64,604,"A: Add    X: Space    Y: Shift    L: Delete",C_MUTED,2,90);
        text_line(64,648,"R: Send message    B: Cancel",C_WHITE,2,80);
        framebufferEnd(fb);pixels=NULL;padUpdate(pad);u64 down=padGetButtonsDown(pad);
        if(!armed){if(!padGetButtons(pad))armed=1;svcSleepThread(16000000LL);continue;}
        if(down&(HidNpadButton_B|HidNpadButton_Plus)){dnx2GraphicalClearDraft();return 0;}
        if(down&HidNpadButton_AnyLeft)cell=(cell+(int)strlen(keys)-1)%(int)strlen(keys);
        if(down&HidNpadButton_AnyRight)cell=(cell+1)%(int)strlen(keys);
        if(down&HidNpadButton_AnyUp)cell=(cell+(int)strlen(keys)-10)%(int)strlen(keys);
        if(down&HidNpadButton_AnyDown)cell=(cell+10)%(int)strlen(keys);
        if(down&HidNpadButton_Y)upper=!upper;
        if((down&HidNpadButton_L)&&length)draft[--length]=0;
        if(length<512&&(down&(HidNpadButton_A|HidNpadButton_X))){draft[length++]=(down&HidNpadButton_X)?' ':upper?(char)toupper((unsigned char)keys[cell]):keys[cell];draft[length]=0;}
        if((down&HidNpadButton_R)&&length)return 1;
        svcSleepThread(16000000LL);
    }
}
int dnx2GraphicalMenu(void) {
    Result rc=hidInitialize();
    if(R_FAILED(rc))return -1;
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    Framebuffer fb={0};
    rc=framebufferCreate(&fb,nwindowGetDefault(),W,H,PIXEL_FORMAT_RGBA_8888,2);
    if(R_FAILED(rc)){hidExit();return -1;}
    rc=framebufferMakeLinear(&fb);
    if(R_FAILED(rc)){framebufferClose(&fb);hidExit();return -1;}
    PadState pad={0};padInitializeDefault(&pad);
    int action=0;
    if(sel>=options_for(tab))sel=0;
    /* persistent menu state across network actions */
    /* 0 = left sidebar; 1 = right content */
    char status[128]="SELECT AN OPTION";
    int analog_prior=0; /* analog stick edges; no auto-repeating tabs */
    int dirty=1;
    while(1) {
        if(dirty) {
            u32 stride=0;
            pixels=(uint32_t*)framebufferBegin(&fb,&stride);
            if(!pixels) {action=-1;break;}
            stride_pixels=(int)(stride/4);
            draw(status,tab,sel,focus,0);
            framebufferEnd(&fb);
            pixels=NULL;dirty=0;
        }
        padUpdate(&pad);
        u64 down=padGetButtonsDown(&pad);
        HidAnalogStickState stick=padGetStickPos(&pad,0);
        /* Distinct stick threshold crossings, alongside D-pad edges. */
        int analog=0;
        if(stick.x<-16000)analog=1;
        else if(stick.x>16000)analog=2;
        else if(stick.y>16000)analog=3;
        else if(stick.y<-16000)analog=4;
        if(analog && analog!=analog_prior) {
            if(analog==1)down|=HidNpadButton_Left;
            if(analog==2)down|=HidNpadButton_Right;
            if(analog==3)down|=HidNpadButton_Up;
            if(analog==4)down|=HidNpadButton_Down;
        }
        analog_prior=analog;
        if(down & HidNpadButton_Plus)break;
        /* Deliberate test shortcut; no diagnostic tab or automatic login probes. */
        if(tab==0 && (down&HidNpadButton_Y) && (padGetButtons(&pad)&HidNpadButton_R)) {
            action=8;break;
        }
        if(tab==0 && (down&HidNpadButton_X) && (padGetButtons(&pad)&HidNpadButton_R)) {
            action=7;break;
        }
        if(down & HidNpadButton_B) {
            if(message_expanded){message_expanded=0;message_offset=0;}
            else if(focus==1&&message_view){message_view=0;sel=0;}
            else if(focus==1&&tab==1&&channel_view){channel_view=0;sel=dnx2Account()->selected;}
            else focus=0;
            dirty=1;
        }
        else if(down & HidNpadButton_AnyLeft) {
            if(focus==1) {focus=0;dirty=1;}
        } else if(down & HidNpadButton_AnyRight) {
            if(focus==0 && options_for(tab)>0) {focus=1;sel=0;dirty=1;}
            /* Channel filters use the currently selected server. */
        }
        int n=focus==0?4:options_for(tab);
        if(message_expanded&&focus==1){
            if(down&HidNpadButton_AnyUp){message_offset=message_offset>330?message_offset-330:0;dirty=1;}
            if(down&HidNpadButton_AnyDown){
                const char *q=dnx2Account()->messages[sel].content;int chars=0;while(*q){dnx_utf8_next(&q);chars++;}
                if(message_offset+330<chars)message_offset+=330;
                dirty=1;
            }
        }
        if(!(message_expanded&&focus==1)&&n>0&&(down & HidNpadButton_AnyUp)) {
            if(focus==0){tab=(tab+n-1)%n;sel=0;message_view=message_expanded=0;}
            else sel=(sel+n-1)%n;
            dirty=1;
        }
        if(!(message_expanded&&focus==1)&&n>0&&(down & HidNpadButton_AnyDown)) {
            if(focus==0){tab=(tab+1)%n;sel=0;message_view=message_expanded=0;}
            else sel=(sel+1)%n;
            dirty=1;
        }
        if(down & HidNpadButton_A) {
            if(focus==0) {
                if(options_for(tab)>0){focus=1;sel=0;dirty=1;}
            } else if(tab==0) {
                if(sel==1){action=dnx2Account()->signed_in?12:13;break;}
                if(dnx2Account()->signed_in){dnx2AccountLogout();dnx2GraphicalClearDraft();channel_view=message_view=message_expanded=0;sel=0;dirty=1;}
                else {action=6;break;}
            } else if(tab==1&&!channel_view) {
                if(sel<dnx2Account()->server_count){dnx2AccountSelect(sel);channel_view=1;sel=0;action=9;break;}
            } else if(tab==2&&message_view){
                message_expanded=1;message_offset=0;dirty=1;
            } else if(!message_view) {
                int i=channel_at(tab,sel);
                if(i>=0){int type=dnx2Account()->channels[i].type;
                    if(type==0||type==5){dnx2ChannelSelect(i);tab=2;message_view=1;sel=0;action=10;break;}
                    if(type==2||type==13){dnx2ChannelSelect(i);tab=3;message_view=1;sel=0;dirty=1;}
                }
            }
        }
        if((down&HidNpadButton_Y)&&focus==1&&dnx2Account()->signed_in){
            if(tab==2&&message_view){message_expanded=0;action=10;break;}
            if(channel_view||tab>=2){action=9;message_view=0;sel=0;break;}
        }
        if((down&HidNpadButton_X)&&focus==1&&tab==2&&message_view){
            if(compose(&fb,&pad)){action=11;break;}dirty=1;
        }
        svcSleepThread(16000000LL);
    }
    framebufferClose(&fb);
    hidExit();
    return action;
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
        padInitializeDefault(&auth_pad);
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
    return !!(buttons&(HidNpadButton_B|HidNpadButton_Plus));
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
int dnx2GraphicalPin(const char *title,char pin[9]) {
    memset(pin,'0',8);pin[8]=0;int digit=0,armed=0,ok=0;
    dnx2GraphicalAuthBegin();
    if(!auth_hid_ready||!auth_fb_ready)goto done;
    for(;;){
        u32 pitch=0;pixels=(uint32_t*)framebufferBegin(&auth_fb,&pitch);if(!pixels)break;
        stride_pixels=(int)(pitch/4);text_right=1216;rect(0,0,W,H,C_BG);
        text_line(64,60,title,C_WHITE,4,72);
        text_line(64,144,"Your 8-digit PIN protects the saved login on this SD card.",C_MUTED,2,90);
        text_line(64,190,"Choose a PIN you can remember; a forgotten PIN requires QR login.",C_MUTED,2,90);
        for(int i=0;i<8;i++){
            int x=120+i*130;rounded(x,290,102,100,16,i==digit?C_PRIMARY:C_CARD);
            char shown[2]={i==digit?pin[i]:'*',0};text_line(x+40,326,shown,C_WHITE,4,1);
        }
        text_line(64,500,"Up/Down: Change digit    Left/Right: Move    A: Next / Finish",C_MUTED,2,100);
        text_line(64,554,"B / +: Cancel",C_WHITE,2,64);
        framebufferEnd(&auth_fb);pixels=NULL;padUpdate(&auth_pad);
        u64 down=padGetButtonsDown(&auth_pad);
        if(!armed){if(!padGetButtons(&auth_pad))armed=1;svcSleepThread(16000000LL);continue;}
        if(down&(HidNpadButton_B|HidNpadButton_Plus))break;
        if(down&HidNpadButton_AnyUp)pin[digit]=(char)('0'+(pin[digit]-'0'+1)%10);
        if(down&HidNpadButton_AnyDown)pin[digit]=(char)('0'+(pin[digit]-'0'+9)%10);
        if(down&HidNpadButton_AnyLeft)digit=(digit+7)%8;
        if(down&HidNpadButton_AnyRight)digit=(digit+1)%8;
        if(down&HidNpadButton_A){if(digit==7){ok=1;break;}digit++;}
        svcSleepThread(16000000LL);
    }
done:
    dnx2GraphicalAuthEnd();if(!ok)memset(pin,0,9);return ok;
}
