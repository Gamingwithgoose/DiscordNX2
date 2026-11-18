// SPDX-License-Identifier: MIT
#include "captcha.h"
#include "graphical_ui.h"
#include <switch.h>
#include <switch/applets/web.h>
#include <stdio.h>
#include <string.h>
/* Set to an HTTPS deployment of app/assets/captcha.html after ownership and
 * provider compatibility are established. No default third-party broker. */
#ifndef DNX2_CAPTCHA_PAGE_URL
#define DNX2_CAPTCHA_PAGE_URL "https://mmonsterpatch.gamingwithgoose.com/discordnx2-captcha.html"
#endif
static unsigned browser_error;
static unsigned browser_exit=~0u;
unsigned dnx2CaptchaBrowserExit(void){return browser_exit;}
unsigned dnx2CaptchaBrowserError(void){return browser_error;}
static int encode(const char *s,char *out,size_t cap){
 const char *hex="0123456789ABCDEF";size_t j=0;
 for(size_t i=0;s[i];i++){
  unsigned char c=(unsigned char)s[i];
  int plain=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~';
  if(j+(plain?1:3)>=cap)return 0;
  if(plain)out[j++]=(char)c;else{out[j++]='%';out[j++]=hex[c>>4];out[j++]=hex[c&15];}
 }
 out[j]=0;return 1;
}
int dnx2CaptchaShow(const Dnx2Captcha *c,char *solution,size_t cap){
 browser_error=0;browser_exit=~0u;
 if(!c||strcmp(c->service,"hcaptcha") || !dnx2CaptchaPageUrl(DNX2_CAPTCHA_PAGE_URL))return 0;
 /* Web applet URL slots are bounded: reject, never truncate enterprise data. */
 unsigned char random[32];char state[65],site[288],data[2048],url[3072],last[4096];
 WebCommonConfig config;WebCommonReply reply;
 int ok=0,applet_live=0;
 if(R_FAILED(csrngInitialize()))return 0;
 Result rc=csrngGetRandomBytes(random,sizeof(random));csrngExit();
 if(R_FAILED(rc))return 0;
 for(size_t i=0;i<32;i++)snprintf(state+2*i,3,"%02x",random[i]);
 if(!encode(c->sitekey,site,sizeof(site))||!encode(c->rqdata,data,sizeof(data)))goto done;
 int n=snprintf(url,sizeof(url),"%s#state=%s&sitekey=%s&rqdata=%s",DNX2_CAPTCHA_PAGE_URL,state,site,data);
 if(n<0||(size_t)n>=sizeof(url))goto done;
 dnx2GraphicalAuthEnd();
 rc=appletInitialize();
 if(R_FAILED(rc)){browser_error=rc;dnx2GraphicalAuthBegin();goto done;}
 applet_live=1;
 rc=webPageCreate(&config,url);
 if(R_SUCCEEDED(rc))rc=webConfigSetCallbackUrl(&config,"https://discordnx2.invalid/captcha-return");
 if(R_SUCCEEDED(rc))rc=webConfigShow(&config,&reply);
 if(R_SUCCEEDED(rc)){
  WebExitReason reason;
  Result exit_rc=webReplyGetExitReason(&reply,&reason);
  if(R_SUCCEEDED(exit_rc))browser_exit=(unsigned)reason;
  /* Applet API success is not page navigation success. Do not interpret an
   * error dialog or ordinary B/X close as a completed verification. */
  if(R_FAILED(exit_rc)||reason!=WebExitReason_LastUrl)goto browser_closed;
  size_t size=0;rc=webReplyGetLastUrl(&reply,last,sizeof(last),&size);
  if(R_SUCCEEDED(rc)&&size<sizeof(last))ok=dnx2CaptchaReturn(last,state,solution,cap);
 }
browser_closed:
 if(R_FAILED(rc))browser_error=rc;
 if(applet_live)appletExit();
 dnx2GraphicalAuthBegin();
done:
 dnx2CaptchaWipe(random,sizeof(random));dnx2CaptchaWipe(state,sizeof(state));
 dnx2CaptchaWipe(&config,sizeof(config));dnx2CaptchaWipe(&reply,sizeof(reply));
 dnx2CaptchaWipe(site,sizeof(site));
 dnx2CaptchaWipe(url,sizeof(url));dnx2CaptchaWipe(last,sizeof(last));dnx2CaptchaWipe(data,sizeof(data));
 return ok;
}
