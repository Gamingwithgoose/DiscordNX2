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
/* Returns 1 only for a user-completed browser result, 0 canceled/unavailable. */
unsigned dnx2CaptchaBrowserError(void);
int dnx2CaptchaShow(const Dnx2Captcha *challenge,char *solution,size_t cap);
#endif
