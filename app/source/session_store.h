// SPDX-License-Identifier: MIT
#ifndef DNX2_SESSION_STORE_H
#define DNX2_SESSION_STORE_H
#include <stddef.h>
int dnx2SessionExists(void);
int dnx2SessionSave(const char *token,const char *pin);
int dnx2SessionLoad(char *token,size_t cap,const char *pin);
void dnx2SessionForget(void);
#endif
