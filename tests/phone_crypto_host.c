// SPDX-License-Identifier: MIT
#include "auth_crypto.h"
#include "captcha_phone_wire.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
static int line(char *s,size_t cap){if(!fgets(s,(int)cap,stdin))return 0;size_t n=strlen(s);if(!n||s[n-1]!='\n')return 0;s[n-1]=0;return 1;}
int main(void){
    char pub[900];assert(dnx2AuthCryptoOpen()&&dnx2AuthCryptoSelfTest()&&dnx2AuthCryptoPublicKey(pub,sizeof(pub)));
    puts(pub);fflush(stdout);
    char state[66],payload[6150],expected[4097],out[4096];
    while(line(state,sizeof(state))&&line(payload,sizeof(payload))&&line(expected,sizeof(expected))){
        int ok=dnx2PhoneDecrypt(payload,strlen(payload),state,out,sizeof(out));
        puts(ok?(strcmp(out,expected)==0?"OK":"MISMATCH"):"REJECT");fflush(stdout);
    }
    dnx2AuthCryptoClose();return 0;
}
