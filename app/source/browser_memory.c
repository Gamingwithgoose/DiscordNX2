// SPDX-License-Identifier: MIT
#include "browser_memory.h"
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 0x1000u
static Result invalid(void) { return MAKERESULT(Module_Libnx, LibnxError_BadInput); }
static Result unavailable(void) { return MAKERESULT(Module_Libnx, LibnxError_NotInitialized); }
static void trace(Dnx2BrowserCode *c,const char *stage,Result rc) {
    if(c->log)c->log(stage,rc);
}
static int permissions_advertised(void) {
    return envIsSyscallHinted(0x5f) && envIsSyscallHinted(0x73) &&
           envIsSyscallHinted(0x77) && envIsSyscallHinted(0x78);
}


Result dnx2BrowserCodeCreate(Dnx2BrowserCode *c, size_t size) {
    if (!c || !size || size > SIZE_MAX - (PAGE_SIZE - 1)) return invalid();
    // A live descriptor must be destroyed successfully before it can be reused.
    if (c->source || c->reservation || c->mapped) return invalid();
    if (!permissions_advertised()) return unavailable();
    memset(c, 0, sizeof(*c));
    c->process = envGetOwnProcessHandle(); // borrowed: never close the loader's handle
    if (c->process == INVALID_HANDLE)
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    c->size = (size + PAGE_SIZE - 1) & ~(size_t)(PAGE_SIZE - 1);
    c->source = memalign(PAGE_SIZE, c->size);
    if (!c->source) return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    memset(c->source, 0, c->size);
    virtmemLock();
    c->executable = virtmemFindCodeMemory(c->size, PAGE_SIZE);
    if (c->executable) c->reservation = virtmemAddReservation(c->executable, c->size);
    virtmemUnlock();
    if (!c->reservation) {
        free(c->source);
        memset(c, 0, sizeof(*c));
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    return 0;
}

static Result unmap(Dnx2BrowserCode *c) {
    if (!c->mapped) return 0;
    trace(c,"Unmap executable alias begin",0);
    Result rc = svcUnmapProcessCodeMemory(c->process, (u64)c->executable,
                                          (u64)c->source, c->size);
    trace(c,"Unmap executable alias end",rc);
    if (R_SUCCEEDED(rc)) c->mapped = 0;
    return rc;
}

Result dnx2BrowserCodeWrite(Dnx2BrowserCode *c, const void *bytes, size_t length) {
    if (!c || !c->source || !c->reservation || !bytes || !length || length > c->size)
        return invalid();
    Result rc = unmap(c);
    if (R_FAILED(rc)) return rc; // source is inaccessible while aliased
    if (!permissions_advertised()) return unavailable();
    trace(c,"Write fixed code bytes begin",0);
    memset(c->source, 0, c->size);
    memcpy(c->source, bytes, length);
    trace(c,"Write fixed code bytes end",0);
    // Use a checked kernel call rather than direct userspace cache instructions.
    trace(c,"System data cache flush begin",0);
    rc = svcFlushProcessDataCache(c->process,(uintptr_t)c->source,c->size);
    trace(c,"System data cache flush end",rc);
    if (R_FAILED(rc)) return rc;
    trace(c,"Map executable alias begin",0);
    rc = svcMapProcessCodeMemory(c->process, (u64)c->executable, (u64)c->source, c->size);
    trace(c,"Map executable alias end",rc);
    if (R_FAILED(rc)) return rc;
    c->mapped = 1;
    trace(c,"Set read/execute permission begin",0);
    rc = svcSetProcessMemoryPermission(c->process, (u64)c->executable, c->size, Perm_Rx);
    trace(c,"Set read/execute permission end",rc);
    // Code mapping/permission transitions perform kernel instruction-cache maintenance.
    // The replacement test checks that the new instruction stream is observed.
    // Even if permission setup fails, mapped stays true until cleanup succeeds.
    return rc;
}

Result dnx2BrowserCodeDestroy(Dnx2BrowserCode *c) {
    if (!c) return invalid();
    Result rc = unmap(c);
    if (R_FAILED(rc)) return rc; // never free backing storage still mapped by Horizon
    if (c->source) {
        volatile unsigned char *p = c->source;
        for (size_t i = 0; i < c->size; ++i) p[i] = 0;
        free(c->source);
    }
    if (c->reservation) {
        virtmemLock();
        virtmemRemoveReservation(c->reservation);
        virtmemUnlock();
    }
    memset(c, 0, sizeof(*c));
    return 0;
}

int dnx2BrowserMemoryCheck(Dnx2BrowserMemoryLog log) {
    if (!log) return 0;
    // One attempt per process: a failed cleanup must not accumulate mappings.
    static int attempted;
    if (attempted) { log("Already tested; relaunch before another attempt", invalid()); return 0; }
    attempted = 1;
    // A missing loader hint is treated conservatively, not proof of kernel support.
    log("Loader advertises cache flush (SVC 5F)",envIsSyscallHinted(0x5f)?0:unavailable());
    log("Loader advertises code permissions (SVC 73)",envIsSyscallHinted(0x73)?0:unavailable());
    log("Loader advertises code mapping (SVC 77)",envIsSyscallHinted(0x77)?0:unavailable());
    log("Loader advertises code unmapping (SVC 78)",envIsSyscallHinted(0x78)?0:unavailable());
    if (!permissions_advertised()) {
        log("Code-memory access not advertised; privileged calls skipped",unavailable());
        return 0;
    }
    Dnx2BrowserCode code = {0};
    log("Allocate executable-code storage", 0);
    Result rc = dnx2BrowserCodeCreate(&code, PAGE_SIZE);
    log("Allocate executable-code storage result", rc);
    code.log=log;
    int passed = 0;
    if (R_SUCCEEDED(rc)) {
        // bti c; mov w0,#42; ret. BTI landing pad is a hint on earlier CPUs.
        uint32_t instructions[] = {0xd503245f, 0x52800540, 0xd65f03c0};
        log("Map code and set read/execute permission", 0);
        rc = dnx2BrowserCodeWrite(&code, instructions, sizeof(instructions));
        log("Map code and set read/execute permission result", rc);
        if (R_SUCCEEDED(rc)) {
            MemoryInfo info = {0};
            u32 page_info = 0;
            rc = svcQueryMemory(&info, &page_info, (u64)code.executable);
            if (R_SUCCEEDED(rc) && info.perm != Perm_Rx) rc = invalid();
            log("Verify executable alias is read/execute only", rc);
        }
        if (R_SUCCEEDED(rc)) {
            log("Execute fixed instruction sequence", 0);
            unsigned value = ((unsigned (*)(void))code.executable)();
            rc = value == 42 ? 0 : invalid();
            log("Execute fixed instruction sequence result", rc);
        }
        if (R_SUCCEEDED(rc)) {
            // Rewrite to 53 to exercise JIT-style unmap/write/remap/cache coherency.
            instructions[1] = 0x528006a0;
            log("Replace executable code and refresh caches", 0);
            rc = dnx2BrowserCodeWrite(&code, instructions, sizeof(instructions));
            log("Replace executable code result", rc);
            if (R_SUCCEEDED(rc)) {
                log("Execute replaced instruction sequence", 0);
                rc = ((unsigned (*)(void))code.executable)() == 53 ? 0 : invalid();
                log("Execute replaced instruction sequence result", rc);
            }
        }
        passed = R_SUCCEEDED(rc);
    }
    Result cleanup = dnx2BrowserCodeDestroy(&code);
    log("Release executable mapping and storage", cleanup);
    passed = passed && R_SUCCEEDED(cleanup);
    log(passed ? "Executable memory check passed" : "Executable memory check failed", rc ? rc : cleanup);
    return passed;
}
