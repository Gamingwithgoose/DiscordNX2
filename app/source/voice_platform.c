// SPDX-License-Identifier: MIT
/* Switch applets have no Unix real/effective UID or set-id execution.
 * OpenSSL's privilege check therefore has no elevated-identity case here. */
#include <stdlib.h>
#include <malloc.h>
#include <errno.h>
#include <stdint.h>
int OPENSSL_issetugid(void){return 0;}
/* libounce/newlib exposes memalign, but not its POSIX spelling. */
int posix_memalign(void **out,size_t alignment,size_t size){
 if(alignment<sizeof(void*)||(alignment&(alignment-1))||alignment%sizeof(void*))return EINVAL;
 void *p=memalign(alignment,size);if(!p)return ENOMEM;*out=p;return 0;
}
