// SPDX-License-Identifier: MIT
#include "captcha.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
void dnx2CaptchaWipe(void *ptr,size_t n){volatile unsigned char *p=ptr;while(n--)*p++=0;}
static int field(const char *s,size_t max){
 size_t n=0;if(!s)return 0;
 for(;s[n];n++)if(n>=max || (unsigned char)s[n]<33 || (unsigned char)s[n]>126)return 0;
 return 1;
}
int dnx2CaptchaPageUrl(const char *url){
 if(!url || strncmp(url,"https://",8)!=0 || !field(url,512))return 0;
 const char *host=url+8,*end=strchr(host,'/');
 if(!end || end==host)return 0;
 for(const char *p=host;p<end;p++)if(!(isalnum((unsigned char)*p)||*p=='.'||*p=='-'))return 0;
 return !strpbrk(end,"?#\\");
}
int dnx2CaptchaHeaders(const Dnx2Captcha *c,const char *solution,char *out,size_t cap){
 if(!out||!cap)return 0;
 out[0]=0;
 if(!c||!solution||!*solution||!field(solution,4095)||!field(c->rqtoken,sizeof(c->rqtoken)-1)||!field(c->session_id,sizeof(c->session_id)-1))return 0;
 int n=snprintf(out,cap,"X-Captcha-Key: %s\r\n%s%s%s%s%s%s",solution,
 c->rqtoken[0]?"X-Captcha-Rqtoken: ":"",c->rqtoken,c->rqtoken[0]?"\r\n":"",
 c->session_id[0]?"X-Captcha-Session-Id: ":"",c->session_id,c->session_id[0]?"\r\n":"");
 if(n<0||(size_t)n>=cap){dnx2CaptchaWipe(out,cap);return 0;}return 1;
}
static int hex(unsigned char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
int dnx2CaptchaReturn(const char *url,const char *state,char *out,size_t cap){
 const char *prefix="https://discordnx2.invalid/captcha-return#state=";
 if(!url||!state||strlen(state)!=64||!out||cap<2)return 0;
 out[0]=0;size_t n=strlen(prefix);
 if(strncmp(url,prefix,n)!=0 || strncmp(url+n,state,64)!=0 || strncmp(url+n+64,"&token=",7)!=0)return 0;
 const char *p=url+n+71;size_t j=0;
 while(*p){unsigned char ch=(unsigned char)*p++;
  if(ch=='%'){if(!p[0]||!p[1])goto fail;int a=hex(p[0]),b=hex(p[1]);if(a<0||b<0)goto fail;ch=(unsigned char)(a*16+b);p+=2;}
  else if(ch=='&'||ch=='#'||ch=='+')goto fail;
  if(ch<33||ch>126||j+1>=cap)goto fail;
  out[j++]=(char)ch;
 }
 out[j]=0;return j>0;
fail:dnx2CaptchaWipe(out,cap);return 0;
}
