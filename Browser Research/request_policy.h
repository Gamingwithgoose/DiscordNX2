// SPDX-License-Identifier: MIT
#ifndef DNX_BROWSER_REQUEST_POLICY_H
#define DNX_BROWSER_REQUEST_POLICY_H
#include <stdint.h>
static inline uint32_t browser_result_value(uint32_t result){return result&0x3fffffu;}
static inline unsigned browser_result_module(uint32_t result){return result&0x1ffu;}
static inline unsigned browser_result_description(uint32_t result){return (result>>9)&0x1fffu;}
static inline int browser_retry_default_attributes(uint32_t result,unsigned attributes){return attributes!=0&&browser_result_value(result)==0x2ee202u;}
/* Correct this process's SDK dispatch only when firmware was obtained successfully. */
static inline int browser_sync_sdk_version(uint32_t sdk,uint32_t firmware){return sdk<0x140000u&&firmware>=0x140000u&&firmware<0x200000u;}
#endif
