// SPDX-License-Identifier: MIT
// Read resident plugin state and test whether basic service sessions can open.
#include <stdio.h>
#include <string.h>
#include <switch.h>
#include <kaihen.h>

#define DNX2_MAGIC 0x444e5832u
typedef struct {
    u32 magic, abi, command_count, probe_count;
    u32 generation, heartbeat, flags, reserved;
} Dnx2Status;
typedef struct {
    Result hid, audin, audout, bsd;
    u32 probe_count, reserved[3];
} Dnx2Services;

static void save_report(Result connect_rc, Result ping_rc, Result status_rc,
                        Result probe_rc, Result last_rc, u32 count,
                        u32 reply, const Dnx2Status *status,
                        const Dnx2Services *probe, const Dnx2Services *last) {
    FsFileSystem sd;
    if (R_FAILED(fsOpenSdCardFileSystem(&sd))) return;
    char report[1024];
    int length = snprintf(report, sizeof(report),
        "DiscordNX2 diagnostic v0.2\n"
        "Plugin connection result: %08x\n"
        "Visible plugins: %u\n"
        "Handshake result: %08x, reply: %08x (expected 444e5832)\n"
        "Status result: %08x\n"
        "Plugin magic: %08x, ABI: %u, commands: %u, probes: %u, generation: %u, heartbeat: %u\n"
        "Service probe result: %08x\n"
        "hid: %08x\naud in: %08x\naud out: %08x\nbsd: %08x\n"
        "Probe count: %u\n"
        "Read last-probe result: %08x, last count: %u\n"
        "A service result of 00000000 means its session opened, NOT that its feature works.\n"
        "No Discord connection, microphone recording, or C-button capture is active.\n",
        connect_rc, count, ping_rc, reply, status_rc,
        status->magic, status->abi, status->command_count,
        status->probe_count, status->generation, status->heartbeat,
        probe_rc, probe->hid, probe->audin, probe->audout, probe->bsd,
        probe->probe_count, last_rc, last->probe_count);
    const char *path = "/kaihen/discordnx2-probe.txt";
    fsFsDeleteFile(&sd, path);
    if (length > 0 && (size_t)length < sizeof(report) &&
        R_SUCCEEDED(fsFsCreateFile(&sd, path, (s64)length, 0))) {
        FsFile file;
        if (R_SUCCEEDED(fsFsOpenFile(&sd, path, FsOpenMode_Write, &file))) {
            fsFileWrite(&file, 0, report, (u64)length, FsWriteOption_Flush);
            fsFileClose(&file);
        }
    }
    fsFsClose(&sd);
}

static void hardware_report(void) {
    char report[4096];
    size_t used = 0;
#define PUT(...) do { if (used < sizeof(report)) { int n=snprintf(report+used,sizeof(report)-used,__VA_ARGS__); if(n>0) { size_t avail=sizeof(report)-used; used+=(size_t)n<avail?(size_t)n:avail; } } } while(0)
    PUT("DiscordNX2 NRO hardware survey v0.3\n");
    PUT("Foreground sampling for 12 seconds; press normal buttons and C.\n");
    Result hid_rc = hidInitialize();
    PUT("hidInitialize: %08x\n",hid_rc);
    if (R_SUCCEEDED(hid_rc)) {
        padConfigureInput(1,HidNpadStyleSet_NpadStandard);
        PadState pad = {};
        padInitializeDefault(&pad);
        u64 seen=0, previous=0;
        u32 connected=0, transitions=0;
        for (u32 i=0;i<600;i++) {
            padUpdate(&pad);
            u64 held=padGetButtons(&pad);
            seen|=held;
            if (padIsConnected(&pad)) connected++;
            if (held != previous) {
                if (transitions < 20) PUT("change %u sample=%u held=%016llx down=%016llx up=%016llx\n",
                    transitions,i,(unsigned long long)held,
                    (unsigned long long)padGetButtonsDown(&pad),
                    (unsigned long long)padGetButtonsUp(&pad));
                transitions++;
                previous=held;
            }
            svcSleepThread(20000000LL);
        }
        PUT("samples=600 connected=%u transitions=%u seen=%016llx\n",
            connected,transitions,(unsigned long long)seen);
        hidExit();
    }
    Result audio_rc=audinInitialize();
    PUT("audinInitialize: %08x\n",audio_rc);
    if (R_SUCCEEDED(audio_rc)) {
        PUT("audio sample_rate=%u channels=%u format=%u\n",
            audinGetSampleRate(),audinGetChannelCount(),(unsigned)audinGetPcmFormat());
        audinExit();
    }
    PUT("No microphone recording or transmission. C bit not mapped in current headers.\n");
#undef PUT
    FsFileSystem sd;
    if (R_FAILED(fsOpenSdCardFileSystem(&sd))) return;
    const char *path="/kaihen/discordnx2-hardware.txt";
    fsFsDeleteFile(&sd,path);
    if (used>0 && used<sizeof(report) && R_SUCCEEDED(fsFsCreateFile(&sd,path,(s64)used,0))) {
        FsFile file;
        if (R_SUCCEEDED(fsFsOpenFile(&sd,path,FsOpenMode_Write,&file))) {
            fsFileWrite(&file,0,report,(u64)used,FsWriteOption_Flush);
            fsFileClose(&file);
        }
    }
    fsFsClose(&sd);
}

int main(void) {
    Service ro = {}, hen = {}, plugin = {};
    Dnx2Status status = {};
    Dnx2Services probe = {}, last = {};
    u32 count = 0, reply = 0;
    Result rc = kaihenGetRoSession(&ro);
    if (R_SUCCEEDED(rc)) rc = kaihenOpenHomebrewSession(&ro, &hen);
    if (R_SUCCEEDED(rc)) rc = henGetPluginCount(&hen, &count);
    HenPluginName name = {};
    memcpy(name.name, "discordnx2", sizeof("discordnx2"));
    if (R_SUCCEEDED(rc)) rc = henOpenPlugin(&hen, &name, &plugin);
    Result ping_rc = rc, status_rc = rc, probe_rc = rc, last_rc = rc;
    if (R_SUCCEEDED(rc)) {
        u32 challenge = 0x12345678;
        ping_rc = serviceDispatchInOut(&plugin, 0, challenge, reply);
        status_rc = serviceDispatchOut(&plugin, 1, status);
        // Avoid asking an old plugin to perform unsupported commands.
        if (R_SUCCEEDED(status_rc) && status.magic == DNX2_MAGIC && status.abi == 2) {
            probe_rc = serviceDispatchOut(&plugin, 2, probe);
            last_rc = serviceDispatchOut(&plugin, 3, last);
        } else {
            probe_rc = MAKERESULT(Module_Libnx, LibnxError_BadInput);
            last_rc = probe_rc;
        }
    }
    if (serviceIsActive(&plugin)) serviceClose(&plugin);
    save_report(rc, ping_rc, status_rc, probe_rc, last_rc, count,
                reply, &status, &probe, &last);
    if (serviceIsActive(&hen)) serviceClose(&hen);
    if (serviceIsActive(&ro)) serviceClose(&ro);
    hardware_report();
    return rc;
}
