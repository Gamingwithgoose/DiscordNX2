// SPDX-License-Identifier: MIT
// Original libounce surface adapter for libnsfb. Compile without ram.c/SDL
// surfaces: this registers the RAM slot under the name "switch2".
// This is a platform backend, not a complete or CAPTCHA-compatible browser.
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <switch/display/framebuffer.h>
#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"
#include "nsfb.h"
#include "surface.h"
#include "plot.h"
typedef struct {
 Framebuffer fb;PadState pad;int x,y,fb_live,hid_live;
 nsfb_event_t queue[24];unsigned read,write;
} SwitchSurface;
static int defaults(nsfb_t *s){s->width=1280;s->height=720;s->format=NSFB_FMT_ABGR8888;return select_plotters(s)?0:-1;}
static int geometry(nsfb_t *s,int w,int h,enum nsfb_format_e fmt){
 if((w>0&&w!=1280)||(h>0&&h!=720)||(fmt!=NSFB_FMT_ANY&&fmt!=NSFB_FMT_ABGR8888))return -1;
 s->width=1280;s->height=720;s->format=NSFB_FMT_ABGR8888;s->linelen=1280*4;return select_plotters(s)?0:-1;
}
static int finalise(nsfb_t *s){
 SwitchSurface *p=s->surface_priv;
 if(p){if(p->fb_live)framebufferClose(&p->fb);if(p->hid_live)hidExit();free(p);}
 free(s->ptr);s->ptr=NULL;s->surface_priv=NULL;return 0;
}
static int initialise(nsfb_t *s){
 SwitchSurface *p=calloc(1,sizeof(*p));if(!p)return -1;s->surface_priv=p;
 if(geometry(s,1280,720,NSFB_FMT_ABGR8888)<0)goto fail;
 s->ptr=calloc(720,1280*4);if(!s->ptr)goto fail;
 if(R_FAILED(hidInitialize()))goto fail;
 p->hid_live=1;
 padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&p->pad);
 if(R_FAILED(framebufferCreate(&p->fb,nwindowGetDefault(),1280,720,PIXEL_FORMAT_RGBA_8888,2)))goto fail;
 p->fb_live=1;if(R_FAILED(framebufferMakeLinear(&p->fb)))goto fail;
 p->x=640;p->y=360;return 0;
fail:finalise(s);return -1;
}
static int update(nsfb_t *s,nsfb_bbox_t *box){
 (void)box;SwitchSurface *p=s->surface_priv;if(!p||!s->ptr)return -1;
 u32 stride=0;uint8_t *dst=framebufferBegin(&p->fb,&stride);if(!dst)return -1;
 if(stride<1280*4){framebufferEnd(&p->fb);return -1;}
 for(int y=0;y<720;y++)memcpy(dst+(size_t)y*stride,s->ptr+(size_t)y*s->linelen,1280*4);
 // Visible pointer is presentation-only, never written into webpage pixels.
 for(int n=-5;n<=5;n++){
  if(p->x+n>=0&&p->x+n<1280)((uint32_t*)(dst+(size_t)p->y*stride))[p->x+n]=0xffffffff;
  if(p->y+n>=0&&p->y+n<720)((uint32_t*)(dst+(size_t)(p->y+n)*stride))[p->x]=0xffffffff;
 }
 framebufferEnd(&p->fb);return 0;
}
static void enqueue(SwitchSurface *p,nsfb_event_t e){unsigned next=(p->write+1)%24;if(next!=p->read){p->queue[p->write]=e;p->write=next;}}
static void key(SwitchSurface *p,u64 down,u64 up,u64 mask,enum nsfb_key_code_e code){
 nsfb_event_t e={0};e.value.keycode=code;
 if(down&mask){e.type=NSFB_EVENT_KEY_DOWN;enqueue(p,e);}
 if(up&mask){e.type=NSFB_EVENT_KEY_UP;enqueue(p,e);}
}
static bool input(nsfb_t *s,nsfb_event_t *event,int timeout){
 SwitchSurface *p=s->surface_priv;if(!p||!event)return false;
 if(p->read==p->write){
  padUpdate(&p->pad);u64 down=padGetButtonsDown(&p->pad),up=padGetButtonsUp(&p->pad);
  HidAnalogStickState stick=padGetStickPos(&p->pad,0);int x=p->x,y=p->y;
  if(stick.x>8000)x+=5;else if(stick.x<-8000)x-=5;
  if(stick.y>8000)y-=5;else if(stick.y<-8000)y+=5;
  if(x<0)x=0;
  if(x>1279)x=1279;
  if(y<0)y=0;
  if(y>719)y=719;
  if(x!=p->x||y!=p->y){p->x=x;p->y=y;nsfb_event_t e={.type=NSFB_EVENT_MOVE_ABSOLUTE};e.value.vector.x=x;e.value.vector.y=y;enqueue(p,e);update(s,NULL);}
  key(p,down,up,HidNpadButton_A,NSFB_KEY_MOUSE_1);
  key(p,down,up,HidNpadButton_B,NSFB_KEY_ESCAPE);
  key(p,down,up,HidNpadButton_AnyDown,NSFB_KEY_TAB);
  key(p,down,up,HidNpadButton_L,NSFB_KEY_PAGEUP);
  key(p,down,up,HidNpadButton_R,NSFB_KEY_PAGEDOWN);
  if(down&HidNpadButton_Plus){nsfb_event_t e={.type=NSFB_EVENT_CONTROL};e.value.controlcode=NSFB_CONTROL_QUIT;enqueue(p,e);}
 }
 if(p->read!=p->write){*event=p->queue[p->read];p->read=(p->read+1)%24;return true;}
 if(timeout!=0)svcSleepThread((s64)(timeout<0||timeout>16?16:timeout)*1000000);
 return false;
}
const nsfb_surface_rtns_t switch2_rtns={.defaults=defaults,.initialise=initialise,.finalise=finalise,.geometry=geometry,.input=input,.update=update};
NSFB_SURFACE_DEF(switch2,NSFB_SURFACE_RAM,&switch2_rtns)
