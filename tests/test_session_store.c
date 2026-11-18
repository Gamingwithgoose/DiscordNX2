// SPDX-License-Identifier: MIT
#include "session_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *token="synthetic-account-token-never-a-real-credential";
int main(int argc,char **argv){
    char out[1024];
    if(argc==2&&!strcmp(argv[1],"save")){
        dnx2SessionForget();assert(dnx2SessionSave(token,"19283746"));assert(dnx2SessionExists());
        FILE *f=fopen(DNX2_SESSION_PATH,"rb");assert(f);char data[1200]={0};size_t n=fread(data,1,sizeof(data),f);fclose(f);
        for(size_t i=0;i+strlen(token)<=n;i++)assert(memcmp(data+i,token,strlen(token)));
        puts("PASS session save: encrypted bytes without plaintext credential");return 0;
    }
    assert(dnx2SessionLoad(out,sizeof(out),"19283746")&&!strcmp(out,token));
    assert(!dnx2SessionLoad(out,sizeof(out),"19283747")&&!out[0]);
    assert(!dnx2SessionLoad(out,8,"19283746")&&!out[0]);
    FILE *f=fopen(DNX2_SESSION_PATH,"r+b");assert(f);fseek(f,54,SEEK_SET);int c=fgetc(f);fseek(f,54,SEEK_SET);fputc(c^1,f);fclose(f);
    assert(!dnx2SessionLoad(out,sizeof(out),"19283746")&&!out[0]);
    dnx2SessionForget();assert(!dnx2SessionExists());puts("PASS cross-process restore, wrong PIN, small output, tamper rejection and forget");
}
