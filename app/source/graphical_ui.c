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
    int right=x<268?252:(y<80 && x<1080?1060:1240);
    if(dnx2FontDraw(pixels,stride_pixels,right,H,x,y,s,color,scale,maxchars))return;
    const int horizontal=scale>=3?2:1;
    const int advance=6*scale;
    for(int i=0;s[i]&&i<maxchars;i++) {
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
static void pill(int x,int y,int w,const char *label,uint32_t color) {
    rounded(x,y,w,35,11,color);
    text_line(x+15,y+9,label,C_WHITE,2,28);
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
    rect(0,0,W,H,C_BG);
    (void)feedback;
    (void)qr_preview; /* QR URLs only shown by an authenticated live session. */
    rounded(10,14,258,H-28,22,C_SIDEBAR);
    brand(27,33);
    text_line(102,54,"DISCORDNX2",C_WHITE,2,18);
    text_line(29,91,"SWITCH 2 NATIVE",C_MUTED,2,22);
    line(23,135,218,C_LINE);
    const char *tabs[4]={"DASHBOARD","SERVERS","VOICE CHAT","DIAGNOSTICS"};
    for(int t=0;t<4;t++) {
        int y=175+t*86;
        if(tab==t)rounded(20,y-9,232,65,14,focus==0?C_CARD_HI:C_CARD);
        if(tab==t)rounded(20,y+7,4,33,2,focus==0?C_GREEN:C_PRIMARY);
        text_line(34,y+7,tabs[t],tab==t?C_WHITE:C_MUTED,3,18);
    }

    const Dnx2Account *profile=dnx2Account();
    pill(1080,36,160,profile->signed_in?"SIGNED IN":"NO LOGIN",profile->signed_in?C_GREEN:C_CARD_HI);
    text_line(308,40,tabs[tab],C_WHITE,4,26);
    text_line(309,93,"YOUR SERVERS. YOUR COMMUNITY.",C_MUTED,2,50);
    line(310,139,930,C_LINE);
    if(tab==0) {
        block(310,224,930,130,C_CARD);
        text_line(339,248,"WELCOME TO DISCORDNX2",C_WHITE,3,38);
        text_line(339,291,"OPEN SERVERS TO SIGN IN WITH YOUR PHONE.",C_MUTED,2,60);
        item(310,376,930,107,"CONNECTIVITY CHECK","PUBLIC GATEWAY - NO ACCOUNT LOGIN",focus==1&&selection==0);
        item(310,501,930,107,"FULL HARDWARE TEST","AUDIO, PACKETS, UDP AND DISCORD GATEWAY",focus==1&&selection==1);
    } else if(tab==1) {
        if(profile->signed_in) {
            pill(310,165,245,"ACCOUNT LOADED",C_GREEN);
            text_line(573,178,profile->display_name,C_WHITE,3,28);
            text_line(315,220,"YOUR DISCORD SERVERS",C_WHITE,3,38);
            int start=(selection/7)*7;
            if(start>=profile->server_count)start=0;
            if(profile->server_count==0)
                text_line(315,292,"YOUR ACCOUNT HAS NO SERVERS",C_MUTED,3,40);
            for(int i=0;i<7&&start+i<profile->server_count;i++){
                int index=start+i,y=262+i*46;
                rounded(310,y,925,40,12,index==profile->selected?C_CARD_HI:C_CARD);
                if(focus==1&&selection==index)rounded(316,y+8,4,24,2,C_GREEN);
                text_line(333,y+8,profile->servers[index].name,C_WHITE,2,54);
            }
            if(selection>=profile->server_count || profile->server_count<7)
                item(310,570,925,63,"LOG OUT","CLEAR SESSION METADATA",focus==1&&selection==profile->server_count);
            text_line(314,614,"VOICE CHAT IS NOT AVAILABLE YET.",C_WARN,2,59);
        } else {
            block(310,172,930,433,C_CARD);
            pill(343,201,315,"DISCORD SIGN IN",C_WARN);
            text_line(343,265,"SIGN IN TO DISCORD",C_WHITE,4,35);
            text_line(343,322,"SCAN WITH THE DISCORD APP ON YOUR PHONE.",C_MUTED,2,58);
            text_line(343,369,"USE A TEST ACCOUNT - UNOFFICIAL CLIENT.",C_WARN,2,58);
            item(343,430,864,110,"SIGN IN WITH QR CODE",
                 "APPROVE SIGN IN ON YOUR PHONE",focus==1);
            text_line(343,566,"TOKENS AND PRIVATE KEYS ARE NEVER SAVED.",C_MUTED,2,58);
        }
    } else if(tab==2) {
        block(310,172,930,355,C_CARD);
        pill(342,200,300,"VOICE DISCONNECTED",C_WARN);
        text_line(342,271,"VOICE CHANNEL",C_WHITE,4,38);
        text_line(342,333,"NO ACTIVE DISCORD VOICE SESSION",C_MUTED,3,49);
        text_line(342,383,"VOICE CHAT IS COMING IN A FUTURE UPDATE.",C_MUTED,2,58);
        text_line(342,420,"OPEN MIC IS NOT TRANSMITTING.",C_MUTED,2,52);
        if(profile->signed_in && profile->server_count>0)
            text_line(342,473,profile->servers[profile->selected].name,C_WHITE,2,58);
    } else {
        item(310,174,930,98,"FULL INTEGRATION TEST","MIC, PLAYBACK, PACKETS, UDP AND GATEWAY",focus==1&&selection==0);
        item(310,287,930,98,"DISCORD GATEWAY TEST","REAL TLS + WEBSOCKET HELLO",focus==1&&selection==1);
        item(310,400,930,98,"MICROPHONE AND PLAYBACK","LOCAL RECORD, PLAY, ENCODE AND DECODE",focus==1&&selection==2);
        item(310,513,930,75,"SUSTAINED MICROPHONE","BOUNDED CAPTURE STABILITY",focus==1&&selection==3);
        item(310,590,930,48,"REMOTE AUTH GATEWAY","SECURE QR LOGIN NETWORK FOUNDATION",focus==1&&selection==4);
    }
    rounded(294,655,963,49,14,C_SIDEBAR);
    text_line(315,670,"LEFT: MENU   RIGHT: CONTENT   UP/DOWN: MOVE   A: OPEN",C_MUTED,2,68);
}
static int options_for(int tab){const Dnx2Account *p=dnx2Account();return tab==0?2:tab==1?(p->signed_in?p->server_count+1:1):tab==3?5:0;}
static int action_for(int tab,int selection) {
    if(tab==0)return selection==0?2:1;
    if(tab==3)return selection==0?1:selection==1?2:selection==2?3:selection==3?4:5;
    return 0;
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
        int n=focus==0?4:options_for(tab);
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
            } else if(tab==1) {
                const Dnx2Account *account=dnx2Account();
                if(!account->signed_in){action=6;break;}
                if(sel==account->server_count){dnx2AccountLogout();sel=0;dirty=1;}
                else {dnx2AccountSelect(sel);snprintf(status,sizeof(status),"SERVER SELECTED - VOICE COMING NEXT");dirty=1;}
            } else {
                action=action_for(tab,sel);
                if(action>0)break;
            }
        }
        /* Preserve v0.17 quick diagnostic shortcuts without switching focus. */
        if(down & HidNpadButton_X){action=2;break;}
        if(down & HidNpadButton_Y){action=3;break;}
        if(down & HidNpadButton_L){action=4;break;}
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
    if(!auth_fb_ready)return;
    u32 stride=0;
    pixels=(uint32_t*)framebufferBegin(&auth_fb,&stride);
    if(!pixels)return;
    stride_pixels=(int)(stride/4);
    rect(0,0,W,H,C_BG);
    rect(0,0,W,13,C_PRIMARY);
    brand(65,40);
    text_line(150,60,"DISCORDNX2",C_WHITE,4,35);
    text_line(65,119,"SIGN IN WITH YOUR PHONE",C_MUTED,2,55);
    block(60,175,1160,444,C_SIDEBAR);
    if(qr_url && strncmp(qr_url,"https://discord.com/ra/",23)==0) {
        if(qr_render_url(735,209,370,qr_url)){
            text_line(86,232,"SCAN USING DISCORD MOBILE",C_WHITE,3,55);
            text_line(86,292,"SETTINGS - SCAN QR CODE",C_MUTED,2,55);
            text_line(86,335,"CONFIRM THIS LOGIN ON YOUR PHONE",C_WARN,2,55);
        }else text_line(86,245,"QR ENCODING ERROR",C_WARN,3,42);
    }else {
        text_line(88,240,"ESTABLISHING A SECURE SESSION",C_WHITE,3,48);
        text_line(88,300,"NEVER SHARE ACCOUNT TOKENS",C_MUTED,2,50);
    }
    text_line(87,542,phase?phase:"CONNECTING",C_GREEN,2,62);
    text_line(87,650,"B: CANCEL    PLUS: CANCEL    NO PASSWORDS REQUIRED",C_MUTED,2,65);
    framebufferEnd(&auth_fb);
    pixels=NULL;
}
void dnx2GraphicalAuthEnd(void) {
    if(auth_fb_ready)framebufferClose(&auth_fb);
    if(auth_hid_ready)hidExit();
    auth_fb_ready=0;auth_hid_ready=0;
}
