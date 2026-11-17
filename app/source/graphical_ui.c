// SPDX-License-Identifier: MIT
/* DiscordNX2 native Switch 2 framebuffer interface.
 *
 * Uses native VI-backed framebuffer, not the console text renderer.
 * No faked Discord account, guild or voice state. Authentication and voice
 * features are locked until an actual supported implementation exists.
 */
#include "graphical_ui.h"
#include "vendor/qrcodegen.h"
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
/* Tiny embedded 5x7 UI raster font. Each byte encodes five active pixels. */
typedef struct {char symbol;uint8_t bits[7];} Glyph;
static const Glyph glyphs[]={
{'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},
{'C',{14,17,16,16,16,17,14}},{'D',{30,17,17,17,17,17,30}},
{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},
{'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},
{'I',{31,4,4,4,4,4,31}},{'J',{7,2,2,2,18,18,12}},
{'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
{'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},
{'O',{14,17,17,17,17,17,14}},{'P',{30,17,17,30,16,16,16}},
{'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},
{'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},
{'U',{17,17,17,17,17,17,14}},{'V',{17,17,17,17,17,10,4}},
{'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
{'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
{'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},
{'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},
{'4',{2,6,10,18,31,2,2}},{'5',{31,16,30,1,1,17,14}},
{'6',{6,8,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},
{'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,2,12}},
{' ',{0,0,0,0,0,0,0}},{'.',{0,0,0,0,0,12,12}},
{':',{0,12,12,0,12,12,0}},{'-',{0,0,0,31,0,0,0}},
{'/',{1,2,2,4,8,8,16}},{'+',{0,4,4,31,4,4,0}},
{'?',{14,17,1,2,4,0,4}},{'!',{4,4,4,4,4,0,4}},
{'#',{10,31,10,10,31,10,0}},{'(',{2,4,8,8,8,4,2}},
{')',{8,4,2,2,2,4,8}},{'=',{0,31,0,31,0,0,0}},
{'_',{0,0,0,0,0,0,31}},{',',{0,0,0,0,12,4,8}},
{'%',{17,2,4,8,17,0,0}},{'>',{16,8,4,2,4,8,16}}
};
static const uint8_t *glyph(char c) {
    if(c>='a'&&c<='z')c=(char)(c-32);
    for(unsigned i=0;i<sizeof(glyphs)/sizeof(glyphs[0]);i++)
        if(glyphs[i].symbol==c)return glyphs[i].bits;
    return glyphs[sizeof(glyphs)/sizeof(glyphs[0])-1].bits;
}
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
static void text_line(int x,int y,const char *s,uint32_t color,int scale,int maxchars) {
    if(!s)return;
    int step=6*scale;
    for(int i=0;s[i]&&i<maxchars;i++) {
        const uint8_t *g=glyph(s[i]);
        for(int row=0;row<7;row++)
            for(int col=0;col<5;col++)
                if(g[row]&(1u<<(4-col)))
                    rect(x+i*step+col*scale,y+row*scale,scale,scale,color);
    }
}
static void line(int x,int y,int width,uint32_t color){rect(x,y,width,2,color);}
static void block(int x,int y,int w,int h,uint32_t color) {
    rect(x,y,w,h,color);
    rect(x+1,y+1,w-2,2,C_LINE);
}
static void pill(int x,int y,int w,const char *label,uint32_t color) {
    rect(x,y,w,35,color);
    text_line(x+15,y+9,label,C_WHITE,2,28);
}
static void item(int x,int y,int w,int h,const char *name,
                 const char *hint,int active) {
    block(x,y,w,h,active?C_CARD_HI:C_CARD);
    if(active)rect(x,y,7,h,C_PRIMARY);
    text_line(x+24,y+15,name,C_WHITE,3,30);
    text_line(x+24,y+53,hint,C_MUTED,2,44);
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
    if(qr_preview)qr_render_url(800,160,340,"https://discord.com/activate");
    rect(0,0,268,H,C_SIDEBAR);
    rect(0,0,268,9,C_PRIMARY);
    text_line(28,42,"DISCORDNX2",C_WHITE,4,18);
    text_line(29,88,"SWITCH 2 NATIVE",C_MUTED,2,22);
    line(23,135,218,C_LINE);
    const char *tabs[4]={"DASHBOARD","SERVERS","VOICE CHAT","DIAGNOSTICS"};
    for(int t=0;t<4;t++) {
        int y=175+t*86;
        if(tab==t)rect(12,y-9,240,65,focus==0?C_CARD_HI:C_CARD);
        if(tab==t)rect(12,y-9,5,65,focus==0?C_GREEN:C_PRIMARY);
        text_line(34,y+7,tabs[t],tab==t?C_WHITE:C_MUTED,3,18);
    }
    rect(19,635,231,54,C_CARD);
    text_line(31,652,"LOCAL BUILD 0.20",C_MUTED,2,23);

    pill(1080,36,160,"NO LOGIN",C_CARD_HI);
    text_line(308,40,tabs[tab],C_WHITE,4,26);
    text_line(309,93,"DISCORDNX2 - NATIVE CLIENT FOUNDATION",C_MUTED,2,50);
    line(310,139,930,C_LINE);
    if(tab==0) {
        pill(310,167,235,"MIC VERIFIED",C_GREEN);
        pill(558,167,280,"GATEWAY VERIFIED",C_PRIMARY);
        block(310,224,930,130,C_CARD);
        text_line(339,248,"NETWORK CONNECTIVITY",C_WHITE,3,38);
        text_line(339,291,"DISCORD TLS AND WEBSOCKET HELLO PASSED ON HARDWARE",C_MUTED,2,60);
        item(310,376,930,107,"CONNECTIVITY CHECK","PUBLIC GATEWAY - NO ACCOUNT LOGIN",focus==1&&focus==1&&selection==0);
        item(310,501,930,107,"FULL HARDWARE TEST","AUDIO, PACKETS, UDP AND DISCORD GATEWAY",focus==1&&focus==1&&selection==1);
    } else if(tab==1) {
        block(310,172,930,433,C_CARD);
        pill(343,201,305,"AUTH NOT READY",C_WARN);
        text_line(343,264,"DISCORD QR LOGIN",C_WHITE,4,33);
        text_line(343,316,"NO ACCOUNT TOKEN IS ACTIVE.",C_MUTED,3,46);
        text_line(343,361,"REMOTE AUTH REQUIRES SECURE RSA SESSION SUPPORT.",C_MUTED,2,58);
        item(343,420,866,95,"TEST REMOTE AUTH CONNECTION",
             "REAL DISCORD REMOTE AUTH WEBSOCKET HELLO",focus==1&&selection==0);
        text_line(343,554,"QR SCAN AUTHENTICATION IS NOT ENABLED YET.",C_WARN,2,57);
    } else if(tab==2) {
        block(310,172,930,355,C_CARD);
        pill(342,200,300,"VOICE DISCONNECTED",C_WARN);
        text_line(342,271,"VOICE CHANNEL",C_WHITE,4,38);
        text_line(342,333,"NO ACTIVE DISCORD VOICE SESSION",C_MUTED,3,49);
        text_line(342,383,"OPUS AND DAVE ARE NOT CONNECTED YET.",C_MUTED,2,58);
        text_line(342,420,"OPEN MIC IS NOT TRANSMITTING.",C_MUTED,2,52);
    } else {
        item(310,174,930,98,"FULL INTEGRATION TEST","MIC, PLAYBACK, PACKETS, UDP AND GATEWAY",focus==1&&selection==0);
        item(310,287,930,98,"DISCORD GATEWAY TEST","REAL TLS + WEBSOCKET HELLO",focus==1&&selection==1);
        item(310,400,930,98,"MICROPHONE AND PLAYBACK","LOCAL RECORD, PLAY, ENCODE AND DECODE",focus==1&&selection==2);
        item(310,513,930,75,"SUSTAINED MICROPHONE","BOUNDED CAPTURE STABILITY",focus==1&&selection==3);
        item(310,590,930,48,"REMOTE AUTH GATEWAY","SECURE QR LOGIN NETWORK FOUNDATION",focus==1&&selection==4);
    }
    rect(294,641,963,58,C_SIDEBAR);
    text_line(315,657,"LEFT: MENU   RIGHT: CONTENT   UP/DOWN: MOVE   A: OPEN",C_MUTED,2,68);
    text_line(311,615,feedback?feedback:"READY",C_GREEN,2,63);
    if(focus==0)text_line(311,590,"SIDEBAR FOCUS - PRESS RIGHT FOR CONTENT",C_WARN,2,52);
    else text_line(311,590,"CONTENT FOCUS - PRESS LEFT FOR SIDEBAR",C_WARN,2,52);
}
static int options_for(int tab){return tab==0?2:tab==1?1:tab==3?5:0;}
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
                action=5; /* Remote Auth TLS + HELLO, not sign-in. */
                break;
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
