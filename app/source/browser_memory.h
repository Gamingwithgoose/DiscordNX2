// SPDX-License-Identifier: MIT
#ifndef DNX2_BROWSER_MEMORY_H
#define DNX2_BROWSER_MEMORY_H
#include <switch.h>
#include <stddef.h>

typedef void (*Dnx2BrowserMemoryLog)(const char *stage, Result result);

/* Zero-initialize before Create. libounce owns loader-specific JIT mappings.
 * Keep all threads out of this code while writing, publishing or closing it.
 * Preserve the descriptor if jitClose reports failure. */
typedef struct {
    Jit jit;
    void *executable;
    size_t size;
    int live;
    Dnx2BrowserMemoryLog log;
} Dnx2BrowserCode;
Result dnx2BrowserCodeCreate(Dnx2BrowserCode *code, size_t size);
Result dnx2BrowserCodeWrite(Dnx2BrowserCode *code, const void *bytes, size_t length);
Result dnx2BrowserCodeDestroy(Dnx2BrowserCode *code);

/* Fixed built-in AArch64 instructions only; no network/downloaded code. */
int dnx2BrowserMemoryCheck(Dnx2BrowserMemoryLog log);
#endif
