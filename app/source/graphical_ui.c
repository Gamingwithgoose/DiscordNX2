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
    for(int i=0;s[i]&&i<maxchars && x+(i+1)*advance<=right;i++) {
        unsigned char ch=(unsigned char)s[i];
        if(ch>=128)ch='?';
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
    if(!url || strncmp(url,"https://discord.com/",20)!=0)return 0;
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
static void draw(const char *feedback,int tab,int selection,int focus,int qr_preview) {
    (void)feedback;(void)qr_preview;
    const Dnx2Account *p=dnx2Account();
    const char *tabs[]={"Dashboard","Servers","Voice chat"};
    rect(0,0,W,H,C_BG);
    rounded(16,16,252,688,20,C_SIDEBAR);
    text_right=250;
    brand(28,30);
    text_line(104,50,"DiscordNX2",C_WHITE,2,20);
    text_line(32,102,"Switch 2 Native",C_MUTED,2,24);
    for(int t=0;t<3;t++){
        int y=180+t*76;
        if(t==tab)rounded(28,y,228,56,14,focus==0?C_CARD_HI:C_CARD);
        if(t==tab)rounded(28,y+14,4,28,2,C_PRIMARY);
        text_line(48,y+17,tabs[t],t==tab?C_WHITE:C_MUTED,3,24);
    }
    text_right=1240;
    text_line(312,48,tabs[tab],C_WHITE,4,32);
    line(312,112,920,C_LINE);
    if(tab==0){
        block(312,160,920,340,C_CARD);
        text_line(344,196,p->signed_in?"Your account":"Welcome to DiscordNX2",C_WHITE,3,46);
        text_line(344,248,p->signed_in?p->display_name:"Sign in using the Discord app on your phone.",C_MUTED,2,64);
        item(344,332,856,96,p->signed_in?"Log out":"Log in with QR code",
             p->signed_in?"Clear this session":"Scan the code, then approve on your phone",focus==1);
    }else if(tab==1){
        if(!p->signed_in){
            block(312,160,920,170,C_CARD);
            text_line(344,200,"Your servers will appear here",C_WHITE,3,50);
            text_line(344,252,"Log in from Dashboard to continue.",C_MUTED,2,60);
        }else{
            int start=(selection/7)*7;
            if(!p->server_count)text_line(344,200,"This account has no servers.",C_MUTED,3,50);
            for(int i=0;i<7 && start+i<p->server_count;i++){
                int n=start+i,y=152+i*64;
                rounded(312,y,920,52,12,focus==1&&selection==n?C_CARD_HI:C_CARD);
                if(p->selected==n)rounded(322,y+14,4,24,2,C_GREEN);
                text_line(344,y+16,p->servers[n].name,C_WHITE,2,70);
            }
        }
    }else{
        block(312,160,920,178,C_CARD);
        text_line(344,190,"Server",C_MUTED,2,30);
        text_line(344,224,p->signed_in&&p->server_count?p->servers[p->selected].name:"No server selected",C_WHITE,3,55);
        text_line(344,280,"Channel: Not connected",C_MUTED,2,60);
        block(312,364,920,248,C_CARD);
        text_line(344,398,"Members",C_WHITE,3,40);
        text_line(344,450,"No voice channel connected.",C_MUTED,2,60);
    }
    if(tab==0 && auth_notice[0] && !p->signed_in)text_line(344,536,auth_notice,C_MUTED,2,96);
    text_line(312,670,"B: Back    A: Select    +: Exit",C_MUTED,2,60);
}
static void draw_auth(const char *phase,const char *qr_url){
    text_right=1216;
    rect(0,0,W,H,C_BG);
    brand(64,38);
    text_line(150,54,"DiscordNX2",C_WHITE,4,30);
    text_line(64,124,"Sign in with your phone",C_MUTED,2,60);
    block(64,180,1152,420,C_SIDEBAR);
    if(qr_url && strncmp(qr_url,"https://discord.com/ra/",23)==0){
        qr_render_url(800,216,344,qr_url);
        text_right=760;
        text_line(100,234,"Open Discord on your phone",C_WHITE,3,50);
        text_line(100,294,"Go to Settings, then Scan QR code.",C_MUTED,2,60);
        text_line(100,338,"Approve this login on your phone.",C_MUTED,2,60);
    }else{
        text_line(100,234,"Connecting your account",C_WHITE,3,50);
        text_line(100,294,"Please wait. You can cancel with B.",C_MUTED,2,60);
    }
    text_right=1180;
    char readable[128];
    snprintf(readable,sizeof(readable),"%s",phase?phase:"Connecting");
    for(size_t i=0;readable[i];i++)readable[i]=(char)tolower((unsigned char)readable[i]);
    readable[0]=(char)toupper((unsigned char)readable[0]);
    text_line(100,526,readable,C_GREEN,2,96);
    text_line(64,650,"B: Cancel    +: Cancel",C_MUTED,2,60);
}
static int options_for(int tab){const Dnx2Account *p=dnx2Account();return tab==0?1:tab==1&&p->signed_in?p->server_count:0;}
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
    int tab=0,sel=0,action=0;
    int focus=0; /* 0 = left sidebar; 1 = right content */
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
        if(down & HidNpadButton_B) {focus=0;dirty=1;}
        else if(down & HidNpadButton_AnyLeft) {
            if(focus==1) {focus=0;dirty=1;}
        } else if(down & HidNpadButton_AnyRight) {
            if(focus==0 && options_for(tab)>0) {focus=1;sel=0;dirty=1;}
            /* Voice has no actions; Servers can test remote-auth gateway. */
        }
        int n=focus==0?3:options_for(tab);
        if(n>0&&(down & HidNpadButton_AnyUp)) {
            if(focus==0)tab=(tab+n-1)%n;
            else sel=(sel+n-1)%n;
            dirty=1;
        }
        if(n>0&&(down & HidNpadButton_AnyDown)) {
            if(focus==0)tab=(tab+1)%n;
            else sel=(sel+1)%n;
            dirty=1;
        }
        if(down & HidNpadButton_A) {
            if(focus==0) {
                if(options_for(tab)>0){focus=1;sel=0;dirty=1;}
            } else if(tab==0) {
                if(dnx2Account()->signed_in){dnx2AccountLogout();sel=0;dirty=1;}
                else {action=6;break;}
            } else if(tab==1) {
                dnx2AccountSelect(sel);tab=2;focus=0;sel=0;dirty=1;
            }
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
void dnx2GraphicalAuthBegin(void) {
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
