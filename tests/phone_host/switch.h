// SPDX-License-Identifier: MIT
// Host-only crypto test shim. Production always uses the real Switch CSRNG.
#ifndef PHONE_TEST_SWITCH_H
#define PHONE_TEST_SWITCH_H
#include <stddef.h>
#include <sys/random.h>
#include <errno.h>
#define R_FAILED(x) ((x)!=0)
static inline int csrngInitialize(void){return 0;}
static inline void csrngExit(void){}
static inline int csrngGetRandomBytes(void *data,size_t length){
    unsigned char *p=data;
    while(length){ssize_t n=getrandom(p,length,0);if(n<0&&errno==EINTR)continue;if(n<=0)return -1;p+=n;length-=(size_t)n;}
    return 0;
}
#endif
