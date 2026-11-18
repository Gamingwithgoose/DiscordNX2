// SPDX-License-Identifier: MIT
#ifndef DNX2_BROWSER_MEMORY_H
#define DNX2_BROWSER_MEMORY_H
#include <switch.h>
#include <stddef.h>

/* Zero-initialize before Create. Owns a page-aligned heap buffer and a reserved executable alias. Never RWX.
 * Callers must stop executing the alias before replacing or destroying it.
 * On unmap failure, retain the allocation/reservation until process exit. */
typedef struct {
    void *source, *executable;
    size_t size;
    VirtmemReservation *reservation;
    Handle process;
    int mapped;
} Dnx2BrowserCode;
Result dnx2BrowserCodeCreate(Dnx2BrowserCode *code, size_t size);
Result dnx2BrowserCodeWrite(Dnx2BrowserCode *code, const void *bytes, size_t length);
Result dnx2BrowserCodeDestroy(Dnx2BrowserCode *code);

/* Fixed built-in AArch64 instructions only; no network/downloaded code. */
typedef void (*Dnx2BrowserMemoryLog)(const char *stage, Result result);
int dnx2BrowserMemoryCheck(Dnx2BrowserMemoryLog log);
#endif
