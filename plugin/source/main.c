// SPDX-License-Identifier: MIT
// A resident kaiHEN service with read-only capability probes and runtime counters.
// This service does NOT capture audio, manipulate game processes, or log raw input.
#include <string.h>
#include <switch.h>
#include <kaihen/plugin.h>

u32 __nx_applet_type = AppletType_None;
size_t __nx_heap_size = 0x200000;
void __appInit(void) {}
void __appExit(void) {}

#define DNX2_MAGIC 0x444e5832u
#define DNX2_ABI 2u
typedef struct {
    u32 magic;
    u32 abi;
    u32 command_count;
    u32 probe_count;
    u32 generation;
    u32 heartbeat;
    u32 flags;
    u32 reserved;
} Dnx2Status;

typedef struct {
    Result hid;
    Result audin;
    Result audout;
    Result bsd;
    u32 probe_count;
    u32 reserved[3];
} Dnx2Services;

typedef struct {
    Dnx2Status status;
    Dnx2Services last;
} Dnx2Context;

static Result check_service(const char *name) {
    Service service = {};
    Result rc = smGetService(&service, name);
    if (R_SUCCEEDED(rc)) serviceClose(&service);
    return rc;
}
static Result handler(u32 command_id, const void *input, size_t input_size,
                      void *output, size_t *output_size, void *context) {
    Dnx2Context *ctx = (Dnx2Context *)context;
    if (!ctx || !output || !output_size)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    ctx->status.command_count++;
    // 0: legacy handshake remains available to older NRO test builds.
    if (command_id == 0 && input && input_size >= sizeof(u32)) {
        u32 challenge = 0;
        memcpy(&challenge, input, sizeof(challenge));
        if (challenge != 0x12345678u)
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        const u32 answer = DNX2_MAGIC;
        memcpy(output, &answer, sizeof(answer));
        *output_size = sizeof(answer);
        return 0;
    }
    // 1: status plus counters. Calling it does not change any system settings.
    if (command_id == 1) {
        ctx->status.heartbeat++;
        memcpy(output, &ctx->status, sizeof(ctx->status));
        *output_size = sizeof(ctx->status);
        return 0;
    }
    // 2: Try opening service sessions, close them immediately, report error codes.
    // A successful open DOES NOT prove microphone/audio/HID usage during games.
    if (command_id == 2) {
        ctx->last.hid = check_service("hid");
        ctx->last.audin = check_service("audin:u");
        ctx->last.audout = check_service("audout:u");
        ctx->last.bsd = check_service("bsd:u");
        ctx->status.probe_count++;
        ctx->last.probe_count = ctx->status.probe_count;
        memcpy(output, &ctx->last, sizeof(ctx->last));
        *output_size = sizeof(ctx->last);
        return 0;
    }
    // 3: Read the most recent service probe without opening services.
    if (command_id == 3) {
        memcpy(output, &ctx->last, sizeof(ctx->last));
        *output_size = sizeof(ctx->last);
        return 0;
    }
    return MAKERESULT(Module_Libnx, LibnxError_BadInput);
}

int main(void) {
    Handle ready = INVALID_HANDLE, control = INVALID_HANDLE;
    Result rc = pluginRuntimeInitializeWithService(&ready, &control);
    if (R_FAILED(rc)) return rc;
    Dnx2Context context = {};
    context.status.magic = DNX2_MAGIC;
    context.status.abi = DNX2_ABI;
    context.status.generation = 1;
    PluginService service = {};
    rc = pluginServiceStart(&service, control, handler, &context);
    if (R_SUCCEEDED(rc)) rc = svcSignalEvent(ready);
    if (R_SUCCEEDED(rc)) {
        // No background system probing until the NRO explicitly requests it.
        for (;;) svcSleepThread(10000000000LL);
    }
    if (service.started) pluginServiceStop(&service);
    pluginRuntimeExit();
    return rc;
}
