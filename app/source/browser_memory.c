// SPDX-License-Identifier: MIT
#include "browser_memory.h"
#include <stdint.h>
#include <string.h>

#define PAGE_SIZE 0x1000u
static Result invalid(void) { return MAKERESULT(Module_Libnx, LibnxError_BadInput); }
static void trace(Dnx2BrowserCode *c,const char *stage,Result rc) {
    if(c->log)c->log(stage,rc);
}

Result dnx2BrowserCodeCreate(Dnx2BrowserCode *c,size_t size) {
    if(!c || c->live || !size || size>SIZE_MAX-(PAGE_SIZE-1))return invalid();
    memset(c,0,sizeof(*c));
    // jitCreate chooses CodeMemory, process-code permissions, or the Switch 2
    // loader callbacks. Absent legacy SVC hints do not disable loader callbacks.
    Result rc=jitCreate(&c->jit,size);
    if(R_SUCCEEDED(rc)) {
        c->live=1;
        c->size=size;
        c->executable=jitGetRxAddr(&c->jit);
    }
    return rc;
}

Result dnx2BrowserCodeWrite(Dnx2BrowserCode *c,const void *bytes,size_t length) {
    if(!c || !c->live || !bytes || !length || length>c->size)return invalid();
    trace(c,"libounce transition to writable begin",0);
    Result rc=jitTransitionToWritable(&c->jit);
    trace(c,"libounce transition to writable end",rc);
    if(R_FAILED(rc))return rc;
    void *staging=jitGetRwAddr(&c->jit);
    if(!staging)return invalid();
    trace(c,"Write fixed code to staging buffer begin",0);
    memset(staging,0,c->size);
    memcpy(staging,bytes,length);
    trace(c,"Write fixed code to staging buffer end",0);
    // Switch 2's helper publishes staging bytes and maintains the RX cache.
    // No raw cache instructions or direct privileged syscalls in this adapter.
    trace(c,"libounce publish executable code begin",0);
    rc=jitTransitionToExecutable(&c->jit);
    trace(c,"libounce publish executable code end",rc);
    return rc;
}

Result dnx2BrowserCodeDestroy(Dnx2BrowserCode *c) {
    if(!c)return invalid();
    if(!c->live)return 0;
    trace(c,"libounce close JIT buffer begin",0);
    Result rc=jitClose(&c->jit);
    trace(c,"libounce close JIT buffer end",rc);
    if(R_SUCCEEDED(rc))memset(c,0,sizeof(*c));
    return rc;
}

int dnx2BrowserMemoryCheck(Dnx2BrowserMemoryLog log) {
    if (!log) return 0;
    // One attempt per process: a failed cleanup must not accumulate JIT buffers.
    static int attempted;
    if (attempted) { log("Already tested; relaunch before another attempt", invalid()); return 0; }
    attempted = 1;
    log("Using libounce JIT API; loader selects the supported backend",0);
    Dnx2BrowserCode code = {0};
    log("Allocate executable-code storage", 0);
    Result rc = dnx2BrowserCodeCreate(&code, PAGE_SIZE);
    log("Allocate executable-code storage result", rc);
    code.log=log;
    int passed = 0;
    if (R_SUCCEEDED(rc)) {
        // bti c; mov w0,#42; ret. BTI landing pad is a hint on earlier CPUs.
        uint32_t instructions[] = {0xd503245f, 0x52800540, 0xd65f03c0};
        log("Publish executable code through libounce", 0);
        rc = dnx2BrowserCodeWrite(&code, instructions, sizeof(instructions));
        log("Publish executable code through libounce result", rc);
        if (R_SUCCEEDED(rc)) {
            MemoryInfo info = {0};
            u32 page_info = 0;
            rc = svcQueryMemory(&info, &page_info, (u64)code.executable);
            if (R_SUCCEEDED(rc)) {
                log("Executable view has read permission", (info.perm & Perm_R) ? 0 : invalid());
                log("Executable view has execute permission", (info.perm & Perm_X) ? 0 : invalid());
                log("Executable view is not writable", (info.perm & Perm_W) ? invalid() : 0);
                if (info.perm != Perm_Rx) rc = invalid();
            }
            log("Verify executable alias is read/execute only", rc);
        }
        if (R_SUCCEEDED(rc)) {
            log("Execute fixed instruction sequence", 0);
            unsigned value = ((unsigned (*)(void))code.executable)();
            rc = value == 42 ? 0 : invalid();
            log("Execute fixed instruction sequence result", rc);
        }
        if (R_SUCCEEDED(rc)) {
            // Rewrite to 53 to exercise staging publication and cache coherency.
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
