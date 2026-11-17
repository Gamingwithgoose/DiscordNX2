// SPDX-License-Identifier: MIT
// Small background IPC probe, no Discord or microphone access.
#include <string.h>
#include <switch.h>
#include <kaihen/plugin.h>
u32 __nx_applet_type = AppletType_None;
size_t __nx_heap_size = 0x200000;
void __appInit(void) {}
void __appExit(void) {}
static Result command(u32 id, const void *in, size_t in_size,
                      void *out, size_t *out_size, void *ctx) {
    (void)ctx;
    if (id != 0 || in_size < sizeof(u32))
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    u32 challenge = 0;
    memcpy(&challenge, in, sizeof(challenge));
    if (challenge != 0x12345678)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    u32 answer = 0x444e5832; // DNX2
    memcpy(out, &answer, sizeof(answer));
    *out_size = sizeof(answer);
    return 0;
}
int main(void) {
    Handle ready = INVALID_HANDLE, control = INVALID_HANDLE;
    Result rc = pluginRuntimeInitializeWithService(&ready, &control);
    if (R_FAILED(rc)) return rc;
    PluginService service = {};
    rc = pluginServiceStart(&service, control, command, NULL);
    if (R_SUCCEEDED(rc)) rc = svcSignalEvent(ready);
    if (R_SUCCEEDED(rc)) {
        for (;;) svcSleepThread(10000000000LL);
    }
    if (service.started) pluginServiceStop(&service);
    pluginRuntimeExit();
    return rc;
}
