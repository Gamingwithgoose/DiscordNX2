// SPDX-License-Identifier: MIT
// Talk to the background plugin and write a diagnostic report to SD.
#include <stdio.h>
#include <string.h>
#include <switch.h>
#include <kaihen.h>
static void write_report(Result rc, u32 count, u32 answer) {
    FsFileSystem sd;
    if (R_FAILED(fsOpenSdCardFileSystem(&sd))) return;
    char report[256];
    int len = snprintf(report, sizeof(report),
        "DiscordNX2 hardware probe v0.1\nIPC result: %08x\nVisible plugins: %u\nService reply: %08x\nExpected: 444e5832\nNo voice, C-button or microphone support yet.\n",
        rc, count, answer);
    const char *path = "/kaihen/discordnx2-probe.txt";
    fsFsDeleteFile(&sd, path);
    if (len > 0 && (size_t)len < sizeof(report) &&
        R_SUCCEEDED(fsFsCreateFile(&sd, path, len, 0))) {
        FsFile file;
        if (R_SUCCEEDED(fsFsOpenFile(&sd, path, FsOpenMode_Write, &file))) {
            fsFileWrite(&file, 0, report, len, FsWriteOption_Flush);
            fsFileClose(&file);
        }
    }
    fsFsClose(&sd);
}
int main(void) {
    Service ro = {}, hen = {}, plugin = {};
    u32 count = 0, answer = 0;
    Result rc = kaihenGetRoSession(&ro);
    if (R_SUCCEEDED(rc)) rc = kaihenOpenHomebrewSession(&ro, &hen);
    if (R_SUCCEEDED(rc)) rc = henGetPluginCount(&hen, &count);
    HenPluginName name = {};
    memcpy(name.name, "discordnx2", sizeof("discordnx2"));
    if (R_SUCCEEDED(rc)) rc = henOpenPlugin(&hen, &name, &plugin);
    if (R_SUCCEEDED(rc)) {
        u32 challenge = 0x12345678;
        rc = serviceDispatchInOut(&plugin, 0, challenge, answer);
    }
    if (serviceIsActive(&plugin)) serviceClose(&plugin);
    write_report(rc, count, answer);
    if (serviceIsActive(&hen)) serviceClose(&hen);
    if (serviceIsActive(&ro)) serviceClose(&ro);
    return rc;
}
