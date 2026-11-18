// SPDX-License-Identifier: MIT
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../app/source/captcha.h"
int main(void){
 Dnx2Captcha c={0};char headers[8192],solution[100],url[400];
 strcpy(c.rqtoken,"request-token");strcpy(c.session_id,"session-id");
 assert(dnx2CaptchaHeaders(&c,"human-solution",headers,sizeof(headers)));
 assert(strcmp(headers,"X-Captcha-Key: human-solution\r\nX-Captcha-Rqtoken: request-token\r\nX-Captcha-Session-Id: session-id\r\n")==0);
 assert(!dnx2CaptchaHeaders(&c,"x\r\nInjected: value",headers,sizeof(headers)));
 assert(!dnx2CaptchaHeaders(&c,"",headers,sizeof(headers)));
 assert(!dnx2CaptchaHeaders(&c,"solution",headers,10));
 c.rqtoken[0]=0;c.session_id[0]=0;assert(dnx2CaptchaHeaders(&c,"x",headers,sizeof(headers)));assert(strcmp(headers,"X-Captcha-Key: x\r\n")==0);
 const char *state="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
 snprintf(url,sizeof(url),"https://discordnx2.invalid/captcha-return#state=%s&token=human%%2Fsolution%%2B",state);
 assert(dnx2CaptchaReturn(url,state,solution,sizeof(solution)));assert(strcmp(solution,"human/solution+")==0);
 assert(!dnx2CaptchaReturn(url,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",solution,sizeof(solution)));
 assert(!dnx2CaptchaReturn("https://attacker.invalid/",state,solution,sizeof(solution)));
 snprintf(url,sizeof(url),"https://discordnx2.invalid/captcha-return#state=%s&token=x%%0D%%0A",state);assert(!dnx2CaptchaReturn(url,state,solution,sizeof(solution)));
 snprintf(url,sizeof(url),"https://discordnx2.invalid/captcha-return#state=%s&token=%%",state);assert(!dnx2CaptchaReturn(url,state,solution,sizeof(solution)));
 assert(dnx2CaptchaPageUrl("https://mmonsterpatch.gamingwithgoose.com/discordnx2-captcha.html"));
 assert(!dnx2CaptchaPageUrl("http://host/page"));assert(!dnx2CaptchaPageUrl("https://user@host/page"));assert(!dnx2CaptchaPageUrl("https://host/page#untrusted"));
 dnx2CaptchaWipe(&c,sizeof(c));for(size_t i=0;i<sizeof(c);i++)assert(((unsigned char*)&c)[i]==0);
 puts("PASS CAPTCHA: challenge headers, optional bindings, injection/overflow rejection, state-bound callback, token decoding, URL constraints, wipe");
}
