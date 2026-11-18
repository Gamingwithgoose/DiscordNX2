// SPDX-License-Identifier: MIT
#ifndef DNX2_CAPTCHA_PHONE_WIRE_H
#define DNX2_CAPTCHA_PHONE_WIRE_H
#include <stddef.h>
#include <stdint.h>
#define DNX2_PHONE_REQUEST_MAX 8192u
#define DNX2_PHONE_PAYLOAD_MAX 4096u
/* 1 complete, 0 needs more bytes, -1 invalid. Payload points into request. */
int dnx2PhoneRequest(const char *request,size_t count,const char **payload,size_t *length);
int dnx2PhonePrivateIp(uint32_t host_order);
/* RSA-OAEP(SHA256) wraps AES-256; GCM authenticates state + solution. */
int dnx2PhoneDecrypt(const char *payload,size_t length,const char *state,char *solution,size_t cap);
#endif
