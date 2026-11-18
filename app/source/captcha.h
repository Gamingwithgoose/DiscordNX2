// SPDX-License-Identifier: MIT
#ifndef DNX2_CAPTCHA_H
#define DNX2_CAPTCHA_H
#include <stddef.h>
typedef struct {
 char service[32],sitekey[96],rqdata[4096],rqtoken[2048],session_id[512];
} Dnx2Captcha;
/* RAM only. No ticket or account token may enter the browser. */
int dnx2CaptchaHeaders(const Dnx2Captcha *challenge,const char *solution,char *out,size_t cap);
int dnx2CaptchaReturn(const char *url,const char *state,char *solution,size_t cap);
int dnx2CaptchaPageUrl(const char *base);
void dnx2CaptchaWipe(void *ptr,size_t size);
/* Direct encrypted phone result; ticket/account token stay on Switch. */
typedef void (*Dnx2CaptchaProgress)(const char *stage,const char *qr_url);
typedef int (*Dnx2CaptchaCancel)(void);
unsigned dnx2CaptchaError(void);
const char *dnx2CaptchaFailure(void);
int dnx2CaptchaShow(const Dnx2Captcha *challenge,char *solution,size_t cap,
                   unsigned local_ip,Dnx2CaptchaProgress progress,Dnx2CaptchaCancel cancel);
#endif
