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
#include <switch/applets/swkbd.h>
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
static int tab=0,sel=0,focus=0;
static int message_expanded=0,message_offset=0;
static char draft[513];
const char *dnx2GraphicalDraft(void){return draft;}
void dnx2GraphicalClearDraft(void){volatile char *p=draft;for(size_t i=0;i<sizeof(draft);i++)p[i]=0;}
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
        char linebuf[512];int bytes=dnx2FontFit(p,text_right-x,2,columns);
        if(bytes<=0)break;
        if(bytes>=(int)sizeof(linebuf))bytes=(int)sizeof(linebuf)-1;
        memcpy(linebuf,p,(size_t)bytes);linebuf[bytes]=0;p+=bytes;
        text_line(x,y+row*23,linebuf,C_WHITE,2,columns);
    }
}
static int server_cursor=-1,server_page=0,channel_cursor=0,channel_first=0;
static int message_first=0,follow_latest=1;
static int keyboard_failed;
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
static int selected_text(void){const Dnx2Account *a=dnx2Account();return a->signed_in&&a->selected_channel>=0&&(a->channels[a->selected_channel].type==0||a->channels[a->selected_channel].type==5);}
static void draw(const char *feedback,int view,int selection,int content_focus,int qr_preview){
    (void)feedback;(void)selection;(void)content_focus;(void)qr_preview;
    const Dnx2Account *a=dnx2Account();int home=view==0;
    rect(0,0,W,H,C_BG);rect(0,0,88,H,C_DARK);rect(88,0,268,H,C_SIDEBAR);
    brand(12,10);text_right=82;
    for(int row=0;row<8;row++){
        int index=server_page*8+row;if(index>=a->server_count)break;int y=98+row*62;
        int active=!home&&index==a->selected;
        rounded(15,y,58,52,active?15:22,active?C_PRIMARY:C_CARD);
        if(focus==0&&server_cursor==index)rounded(3,y+12,4,28,2,C_WHITE);
        char label[16]={0};const char *p=a->servers[index].name;size_t n=0;
        for(int i=0;i<2&&*p;i++){const char *start=p;dnx_utf8_next(&p);size_t k=(size_t)(p-start);if(n+k<sizeof(label)){memcpy(label+n,start,k);n+=k;}}
        text_line(25,y+16,label,C_WHITE,2,2);
    }
    rounded(8,602,32,34,8,C_CARD);rounded(48,602,32,34,8,C_CARD);text_line(18,610,"<",C_WHITE,2,1);text_line(58,610,">",C_WHITE,2,1);
    text_line(12,670,"Exit",C_MUTED,2,5);
    text_right=344;
    text_line(104,26,home?"DiscordNX2":a->server_count?a->servers[a->selected].name:"Select a server",C_WHITE,3,28);
    line(88,76,268,C_LINE);
    if(home){text_line(106,112,"Your Discord on Switch 2",C_MUTED,2,26);text_line(106,158,"Tap a server to browse.",C_MUTED,2,26);}
    else for(int row=0;row<12;row++){
        int index=channel_first+row;if(index>=a->channel_count)break;const Dnx2Channel *c=&a->channels[index];int y=100+44*row;
        int active=index==a->selected_channel;
        if(active||(focus==1&&channel_cursor==index))rounded(96,y,252,40,8,active?C_CARD_HI:C_CARD);
        char label[440];snprintf(label,sizeof(label),"%s%s",c->type==4?"":c->type==2||c->type==13?"Voice  ":c->type==15||c->type==16?"Forum  ":"# ",c->name);
        text_line(c->type==4?104:112,y+12,label,c->type==4?C_MUTED:C_WHITE,2,30);
    }
    rect(88,650,268,70,C_DARK);text_line(104,666,a->signed_in?a->display_name:"Signed out",C_WHITE,2,30);
    text_line(104,693,"Home: account and saved login",C_MUTED,1,30);
    text_right=1256;
    if(home){
        text_line(392,46,"Welcome to DiscordNX2",C_WHITE,4,64);
        text_line(392,158,a->signed_in?a->display_name:"Sign in using Discord on your phone.",C_WHITE,3,70);
        item(392,254,824,90,a->signed_in?"Log out":"Log in with QR code",a->signed_in?"Forget this account and saved login":"Approve your Switch login on your phone",focus==2&&sel==0);
        item(392,366,824,90,a->signed_in?"Remember login with PIN":"Unlock saved login",a->signed_in?"Save encrypted access for future launches":"Enter your saved-login PIN",focus==2&&sel==1);
        if(auth_notice[0])text_wrap(392,504,auth_notice,4,70);
        return;
    }
    if(!a->signed_in){text_line(392,100,"Open Home to sign in.",C_MUTED,3,60);return;}
    if(a->selected_channel<0){text_line(392,32,"Choose a channel",C_WHITE,3,60);text_wrap(392,158,a->notice[0]?a->notice:"Your channels are on the left. Tap a text channel to open the conversation.",4,70);return;}
    const Dnx2Channel *c=&a->channels[a->selected_channel];char heading[440];snprintf(heading,sizeof(heading),"%s%s",selected_text()?"# ":"Voice  ",c->name);
    text_line(380,26,heading,C_WHITE,3,72);line(356,76,924,C_LINE);
    if(selected_text()){
        text_line(380,80,dnx2LiveStatus(),C_MUTED,1,70);
        if(message_expanded&&sel<a->message_count){
            const Dnx2Message *m=&a->messages[sel];text_line(380,112,m->author,C_PRIMARY,2,72);
            const char *text=m->content;for(int i=0;i<message_offset&&*text;i++)dnx_utf8_next(&text);
            text_wrap(380,152,text,19,74);
        }else for(int row=0;row<4;row++){
            int index=message_first+row;if(index>=a->message_count)break;int y=100+row*122;
            const Dnx2Message *m=&a->messages[index];
            if(focus==2&&sel==index)rounded(366,y,900,116,8,C_CARD);
            text_line(382,y+8,m->author,C_PRIMARY,2,72);text_wrap(382,y+36,m->content,3,74);
        }
        if(!a->message_count)text_line(392,158,"No messages in this channel yet.",C_MUTED,2,72);
        rounded(372,626,768,58,12,focus==3?C_CARD_HI:C_CARD);
        text_line(390,644,draft[0]?draft:"Message this channel...",draft[0]?C_WHITE:C_MUTED,2,70);
        rounded(1150,626,112,58,12,draft[0]?C_PRIMARY:C_CARD);text_line(1170,644,"Send",draft[0]?C_WHITE:C_MUTED,2,8);
        text_right=1060;
        if(keyboard_notice[0])text_line(380,600,keyboard_notice,C_WARN,1,90);
        else if(a->notice[0])text_line(380,600,a->notice,C_WARN,1,90);
        text_right=1256;rounded(1080,590,180,30,8,C_CARD);text_line(1112,599,"QWERTY",C_MUTED,1,16);
        text_line(380,697,"Tap to type    X: Keyboard    R: Send    Y: Refresh",C_MUTED,1,96);
    }else{
        text_line(392,160,"Not connected",C_WARN,3,60);text_wrap(392,220,"Voice calls are still being implemented. Selecting this channel does not join a call.",4,72);
        text_line(392,360,"Members unavailable until voice is connected.",C_MUTED,2,72);
    }
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
    static const char *rows[]={"1234567890","qwertyuiop","asdfghjkl","zxcvbnm.,?","!@#&()-_:/"};
    int row=1,column=0,upper=0,armed=0;
    for(;;){
        u32 pitch=0;pixels=(uint32_t*)framebufferBegin(fb,&pitch);if(!pixels)return 0;stride_pixels=(int)(pitch/4);
        draw(NULL,tab,sel,focus,0);rect(356,278,924,442,C_SIDEBAR);text_right=1260;
        text_line(380,292,"Touch QWERTY",C_WHITE,3,60);text_line(380,334,draft[0]?draft:"Write your message",C_WHITE,2,80);
        for(int r=0;r<5;r++)for(int c=0;c<(int)strlen(rows[r]);c++){
            int x=380+c*86,y=382+r*47;rounded(x,y,78,41,8,r==row&&c==column?C_PRIMARY:C_CARD);
            char text[2]={upper?(char)toupper((unsigned char)rows[r][c]):rows[r][c],0};text_line(x+28,y+11,text,C_WHITE,2,1);
        }
        const char *labels[]={"Shift","Space","Delete","Done"};
        for(int c=0;c<4;c++){rounded(380+c*214,626,202,44,8,C_CARD);text_line(398+c*214,640,labels[c],C_WHITE,2,12);}
        text_line(380,690,"A: Add    X: Space    Y: Shift    L: Delete    R: Done    B: Back",C_MUTED,1,110);
        framebufferEnd(fb);pixels=NULL;padUpdate(pad);u64 down=padGetButtonsDown(pad);int x=0,y=0,scroll=0,tap=touch_event(&x,&y,&scroll);
        if(!armed){if(!padGetButtons(pad)&&!touch_input.held)armed=1;svcSleepThread(16000000LL);continue;}
        if(down&(HidNpadButton_B|HidNpadButton_Plus))return 0;
        if(down&HidNpadButton_AnyLeft)column=(column+(int)strlen(rows[row])-1)%(int)strlen(rows[row]);
        if(down&HidNpadButton_AnyRight)column=(column+1)%(int)strlen(rows[row]);
        if(down&HidNpadButton_AnyUp)row=(row+4)%5;
        if(down&HidNpadButton_AnyDown)row=(row+1)%5;
        if(column>=(int)strlen(rows[row]))column=(int)strlen(rows[row])-1;
        int add=!!(down&HidNpadButton_A),space=!!(down&HidNpadButton_X),del=!!(down&HidNpadButton_L),done=!!(down&HidNpadButton_R);
        if(down&HidNpadButton_Y)upper=!upper;
        if(tap==1&&x>=380&&x<1240){
            if(y>=382&&y<617){int r=(y-382)/47,c=(x-380)/86;if(r<5&&c<(int)strlen(rows[r])){row=r;column=c;add=1;}}
            else if(y>=626&&y<670){int key=(x-380)/214;if(key==0)upper=!upper;else if(key==1)space=1;else if(key==2)del=1;else if(key==3)done=1;}
        }
        size_t n=strlen(draft);
        if(del&&n){do{n--;}while(n&&((unsigned char)draft[n]&0xc0)==0x80);draft[n]=0;}
        if((add||space)&&n<512){draft[n++]=space?' ':upper?(char)toupper((unsigned char)rows[row][column]):rows[row][column];draft[n]=0;}
        if(done)return 1;
        svcSleepThread(16000000LL);
    }
}
static void scroll_messages(int delta){
    const Dnx2Account *a=dnx2Account();int max=a->message_count>4?a->message_count-4:0;
    if(message_expanded){message_offset+=delta*296;if(message_offset<0)message_offset=0;
        const char *p=a->messages[sel].content;int count=0;while(*p){dnx_utf8_next(&p);count++;}if(message_offset>=count)message_offset=count>296?count-296:0;return;}
    message_first+=delta;if(message_first<0)message_first=0;if(message_first>max)message_first=max;follow_latest=message_first==max;sel=message_first;
}
static int open_channel(int index){
    const Dnx2Account *a=dnx2Account();if(index<0||index>=a->channel_count)return 0;
    int type=a->channels[index].type;if(type==4)return 0;
    if(type!=0&&type!=5&&type!=2&&type!=13){snprintf(keyboard_notice,sizeof(keyboard_notice),"Forum threads are not available yet.");return 0;}
    if(index!=a->selected_channel)dnx2GraphicalClearDraft();
    dnx2LiveStop();dnx2ChannelSelect(index);channel_cursor=index;tab=(type==0||type==5)?2:3;focus=2;message_expanded=0;message_offset=0;follow_latest=1;sel=0;keyboard_notice[0]=0;
    return tab==2?10:0;
}
int dnx2GraphicalMenu(void){
    Result rc=hidInitialize();if(R_FAILED(rc))return -1;padConfigureInput(1,HidNpadStyleSet_NpadStandard);touch_init();
    Framebuffer fb={0};rc=framebufferCreate(&fb,nwindowGetDefault(),W,H,PIXEL_FORMAT_RGBA_8888,2);
    if(R_FAILED(rc)){hidExit();return -1;}rc=framebufferMakeLinear(&fb);if(R_FAILED(rc)){framebufferClose(&fb);hidExit();return -1;}
    PadState pad={0};padInitializeDefault(&pad);int action=0,dirty=1,analog_prior=0,input_armed=0;const char *last_live="";
    if(!dnx2Account()->signed_in)tab=0;
    if(tab==2&&selected_text()){dnx2LiveStart();if(follow_latest)message_first=dnx2Account()->message_count>4?dnx2Account()->message_count-4:0;}
    while(1){
        const Dnx2Account *a=dnx2Account();
        if(tab==2&&selected_text()){
            char anchor[24]={0};if(message_first<a->message_count)snprintf(anchor,sizeof(anchor),"%s",a->messages[message_first].id);
            if(!message_expanded&&dnx2LiveApply()){
                if(follow_latest)message_first=a->message_count>4?a->message_count-4:0;
                else for(int i=0;i<a->message_count;i++)if(anchor[0]&&!strcmp(a->messages[i].id,anchor)){message_first=i;break;}
                if(message_first>=a->message_count)message_first=a->message_count>4?a->message_count-4:0;
                if(sel>=a->message_count)sel=a->message_count?a->message_count-1:0;
                dirty=1;
            }
            const char *state=dnx2LiveStatus();if(state!=last_live){last_live=state;dirty=1;}
        }
        if(dirty){u32 stride=0;pixels=(uint32_t*)framebufferBegin(&fb,&stride);if(!pixels){action=-1;break;}stride_pixels=(int)(stride/4);draw(NULL,tab,sel,focus,0);framebufferEnd(&fb);pixels=NULL;dirty=0;}
        padUpdate(&pad);u64 down=padGetButtonsDown(&pad);HidAnalogStickState stick=padGetStickPos(&pad,0);
        int analog=stick.x<-16000?1:stick.x>16000?2:stick.y>16000?3:stick.y<-16000?4:0;
        if(analog&&analog!=analog_prior)down|=analog==1?HidNpadButton_Left:analog==2?HidNpadButton_Right:analog==3?HidNpadButton_Up:HidNpadButton_Down;
        analog_prior=analog;
        int x=0,y=0,scroll=0,touch=touch_event(&x,&y,&scroll);
        if(!input_armed){if(!padGetButtons(&pad)&&!touch_input.held)input_armed=1;down=0;touch=0;}
        if(down&HidNpadButton_Plus)break;
        if(tab==0&&(padGetButtons(&pad)&HidNpadButton_R)&&(down&HidNpadButton_X)){action=7;break;}
        if(tab==0&&(padGetButtons(&pad)&HidNpadButton_R)&&(down&HidNpadButton_Y)){action=8;break;}
        int choose_server=-2,choose_channel=-1,keyboard=0,send=0,account_action=-1;
        if(touch==2){
            if(x<88){server_page+=scroll;if(server_page<0)server_page=0;int max=(a->server_count-1)/8;if(server_page>max)server_page=max;}
            else if(x<356){channel_first+=scroll;if(channel_first<0)channel_first=0;int max=a->channel_count>12?a->channel_count-12:0;if(channel_first>max)channel_first=max;}
            else if(selected_text())scroll_messages(scroll);
            dirty=1;
        }
        if(touch==1){DnxHit hit=dnx_chat_hit(x,y,server_page,channel_first,message_first,tab==0,selected_text());
            if(hit.kind==DNX_HOME){dnx2LiveStop();tab=0;focus=2;sel=0;dirty=1;}
            else if(hit.kind==DNX_SERVER)choose_server=hit.index;
            else if(hit.kind==DNX_CHANNEL){choose_channel=hit.index;focus=1;}
            else if(hit.kind==DNX_INPUT)keyboard=1;
            else if(hit.kind==DNX_FALLBACK)keyboard=2;
            else if(hit.kind==DNX_SEND)send=1;
            else if(hit.kind==DNX_MESSAGE&&hit.index<a->message_count){if(!message_expanded)sel=hit.index;message_expanded=!message_expanded;message_offset=0;focus=2;dirty=1;}
            else if(hit.kind==DNX_PREVIOUS||hit.kind==DNX_NEXT){server_page+=hit.kind==DNX_NEXT?1:-1;if(server_page<0)server_page=0;int max=(a->server_count-1)/8;if(server_page>max)server_page=max;dirty=1;}
            else if(hit.kind==DNX_LOGIN)account_action=0;
            else if(hit.kind==DNX_REMEMBER)account_action=1;
            else if(hit.kind==DNX_EXIT)break;
        }
        if(down&HidNpadButton_B){if(message_expanded){message_expanded=0;message_offset=0;}else if(focus>0)focus--;else{dnx2LiveStop();tab=0;}dirty=1;}
        if(down&HidNpadButton_AnyLeft){if(focus>0)focus--;dirty=1;}
        if(down&HidNpadButton_AnyRight){if(focus<3)focus++;dirty=1;}
        int delta=(down&HidNpadButton_AnyUp)?-1:(down&HidNpadButton_AnyDown)?1:0;
        if(delta){
            if(focus==0){server_cursor+=delta;if(server_cursor<-1)server_cursor=a->server_count-1;if(server_cursor>=a->server_count)server_cursor=-1;if(server_cursor>=0)server_page=server_cursor/8;}
            else if(tab==0){sel=(sel+1)%2;}
            else if(focus==1){channel_cursor+=delta;if(channel_cursor<0)channel_cursor=0;if(channel_cursor>=a->channel_count)channel_cursor=a->channel_count?a->channel_count-1:0;if(channel_cursor<channel_first)channel_first=channel_cursor;if(channel_cursor>=channel_first+12)channel_first=channel_cursor-11;}
            else if(selected_text())scroll_messages(delta);
            dirty=1;
        }
        if(down&HidNpadButton_A){
            if(focus==0){if(server_cursor<0){dnx2LiveStop();tab=0;focus=2;sel=0;dirty=1;}else choose_server=server_cursor;}
            else if(tab==0)account_action=sel;
            else if(focus==1)choose_channel=channel_cursor;
            else if(focus==3)keyboard=1;
            else if(selected_text()&&a->message_count){sel=message_first;message_expanded=!message_expanded;message_offset=0;dirty=1;}
        }
        if(account_action>=0){dnx2LiveStop();if(account_action==1){action=a->signed_in?12:13;break;}
            if(a->signed_in){dnx2AccountLogout();dnx2GraphicalClearDraft();tab=0;focus=2;dirty=1;}else{action=6;break;}}
        if(choose_server>=0&&choose_server<a->server_count){dnx2LiveStop();dnx2GraphicalClearDraft();dnx2AccountSelect(choose_server);server_cursor=choose_server;server_page=choose_server/8;channel_first=channel_cursor=0;tab=1;focus=1;message_expanded=0;action=9;break;}
        if(choose_channel>=0){int next=open_channel(choose_channel);dirty=1;if(next){action=next;break;}}
        if((down&HidNpadButton_X)&&tab!=0&&selected_text())keyboard=1;
        if((down&HidNpadButton_R)&&tab!=0&&selected_text())send=1;
        if((down&HidNpadButton_Y)&&tab!=0){action=selected_text()?10:9;break;}
        if(keyboard&&selected_text()){
            dnx2LiveStop();focus=3;
            if(keyboard==2||keyboard_failed){fallback_keyboard(&fb,&pad);dirty=1;dnx2LiveStart();}
            else{action=14;break;}
        }
        if(send&&selected_text()&&draft[0]){action=11;break;}
        svcSleepThread(16000000LL);
    }
    dnx2LiveStop();framebufferClose(&fb);hidExit();return action;
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
        const char *buttons[]={"Up","Down","Next / Finish","Cancel"};
        for(int i=0;i<4;i++){rounded(64+i*290,430,270,48,10,C_CARD);text_line(84+i*290,446,buttons[i],C_WHITE,2,24);}
        text_line(64,500,"Up/Down: Change digit    Left/Right: Move    A: Next / Finish",C_MUTED,2,100);
        text_line(64,554,"B / +: Cancel",C_WHITE,2,64);
        framebufferEnd(&auth_fb);pixels=NULL;padUpdate(&auth_pad);
        u64 down=padGetButtonsDown(&auth_pad);
        int tx=0,ty=0,scroll=0;int tap=touch_event(&tx,&ty,&scroll);
        if(tap==1){
            if(ty>=290&&ty<390&&tx>=120&&tx<1160){digit=(tx-120)/130;if(digit>7)digit=7;}
            if(ty>=430&&ty<478&&tx>=64){int button=(tx-64)/290;down|=button==0?HidNpadButton_Up:button==1?HidNpadButton_Down:button==2?HidNpadButton_A:HidNpadButton_B;}
        }
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

int dnx2GraphicalNativeKeyboard(unsigned *result){
    /* The normal menu closes its framebuffer and message worker first. */
    if(result)*result=0;
    Result rc=appletInitialize();if(R_FAILED(rc)){keyboard_failed=1;if(result)*result=rc;snprintf(keyboard_notice,sizeof(keyboard_notice),"System keyboard unavailable. Tap the input for touch QWERTY.");return -1;}
    SwkbdConfig config;rc=swkbdCreate(&config,0);int created=R_SUCCEEDED(rc),ok=0;
    char text[sizeof(draft)]={0};
    if(created){
        swkbdConfigMakePresetDefault(&config);swkbdConfigSetType(&config,SwkbdType_QWERTY);
        swkbdConfigSetHeaderText(&config,"DiscordNX2 message");swkbdConfigSetGuideText(&config,"Write a message");
        swkbdConfigSetOkButtonText(&config,"Done");swkbdConfigSetStringLenMax(&config,128);swkbdConfigSetStringLenMin(&config,0);
        swkbdConfigSetInitialText(&config,draft);rc=swkbdShow(&config,text,sizeof(text));
        if(R_SUCCEEDED(rc)){snprintf(draft,sizeof(draft),"%s",text);ok=1;keyboard_failed=0;keyboard_notice[0]=0;}
        swkbdClose(&config);
    }
    appletExit();volatile char *p=text;for(size_t i=0;i<sizeof(text);i++)p[i]=0;
    if(R_FAILED(rc)){
        /* libnx/libounce return BadAppletExit for Cancel as well as an applet
         * that failed to show. Keep the existing draft; never auto-send. */
        keyboard_failed=rc!=MAKERESULT(Module_Libnx,LibnxError_LibAppletBadExit);
        snprintf(keyboard_notice,sizeof(keyboard_notice),"Keyboard closed. Tap QWERTY if the system keyboard is unavailable.");
    }
    if(result)*result=rc;
    return ok?1:R_FAILED(rc)?-1:0;
}
