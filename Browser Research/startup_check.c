// SPDX-License-Identifier: MIT
/* Focused read-only diagnostic for the exported 23.0.1 openweb build. */
#include <switch.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

typedef struct { u8 bytes[48]; } StartupSettings;
_Static_assert(sizeof(StartupSettings)==48,"Exact observed IPC output size");
static char report[8192], path[160], status[160]="No check run yet.";
static size_t used;
static FsFileSystem sd;
static int sd_ready;
static void log_line(const char *format, ...){
    if(used>=sizeof(report)-1)return;
    va_list ap;va_start(ap,format);
    int n=vsnprintf(report+used,sizeof(report)-used,format,ap);va_end(ap);
    if(n>0)used+=(size_t)n<sizeof(report)-used?(size_t)n:sizeof(report)-used-1;
}
static void result(const char *label,Result rc){
    u32 value=rc&0x003fffffU;
    log_line("%s=%08x value=%08x module=%u description=%u\n",label,rc,value,value&511U,value>>9);
}
static void save(void){
    if(!sd_ready||!path[0])return;
    FsFile file={0};Result rc=fsFsOpenFile(&sd,path,FsOpenMode_Write,&file);
    if(R_SUCCEEDED(rc)){
        rc=fsFileSetSize(&file,(s64)used);
        if(R_SUCCEEDED(rc))rc=fsFileWrite(&file,0,report,used,FsWriteOption_Flush);
        fsFileClose(&file);
    }
    if(R_FAILED(rc))snprintf(status,sizeof(status),"Report save failed: %08x",rc);
}
static void check(int launch){
    used=0;report[0]=0;path[0]=0;
    if(sd_ready){
        (void)fsFsCreateDirectory(&sd,"/switch");
        (void)fsFsCreateDirectory(&sd,"/switch/DiscordNX2-Browser");
        snprintf(path,sizeof(path),"/switch/DiscordNX2-Browser/startup-%016" PRIx64 ".txt",(uint64_t)armGetSystemTick());
        Result rc=fsFsCreateFile(&sd,path,0,0);
        if(R_FAILED(rc)){snprintf(status,sizeof(status),"Cannot create fresh report: %08x",rc);path[0]=0;return;}
    }
    log_line("Browser Startup Check v0.1\nRead-only system queries; optional normal example.com applet launch.\nNo settings writes, patches, firmware installation, credentials or Discord requests.\n");
    log_line("SDK_HOS_VERSION=%08x\n",hosversionGet());
    SetSysFirmwareVersion fw={0};Result init=setsysInitialize();result("setsysInitialize",init);
    Result frc=init;
    if(R_SUCCEEDED(init)){frc=setsysGetFirmwareVersion(&fw);result("firmwareResult",frc);}
    log_line("physicalFirmware=%u.%u.%u\n",fw.major,fw.minor,fw.micro);
    if(R_SUCCEEDED(init)&&R_SUCCEEDED(frc)&&fw.major==23&&fw.minor==0&&fw.micro==1){
        StartupSettings settings={0};
        /* Native openweb: set:sys command265, no input, 48-byte inline output.
           Its meaning is not named yet. Do not substitute a guessed setter. */
        Result rc=serviceDispatchOut(setsysGetServiceSession(),265,settings);
        result("settingsReadCommand265",rc);
        if(R_SUCCEEDED(rc)){
            log_line("settings48=");
            for(unsigned i=0;i<sizeof(settings.bytes);i++)log_line("%02x",settings.bytes[i]);
            log_line("\nfirstByteBit0=%u\n",settings.bytes[0]&1U);
            log_line("One observed dialog path tests NOT(firstByteBit0). This does not identify which path the live applet took.\n");
            snprintf(status,sizeof(status),"Settings read OK. First bit: %u. See report.",settings.bytes[0]&1U);
        }else snprintf(status,sizeof(status),"Settings read failed: %08x. See report.",rc);
    }else{
        log_line("settingsReadCommand265=SKIPPED (requires verified physical23.0.1)\n");
        snprintf(status,sizeof(status),"Version check did not match23.0.1; special query skipped.");
    }
    if(R_SUCCEEDED(init))setsysExit();
    save();
    if(launch){
        log_line("BASELINE_BEGIN url=https://example.com/\n");save();
        puts("Opening example.com through the normal browser applet...\nClose any page or dialog normally to return.");consoleUpdate(NULL);
        Result rc=appletInitialize();result("appletInitialize",rc);
        if(R_SUCCEEDED(rc)){
            WebCommonConfig cfg;WebCommonReply reply={0};
            rc=webPageCreate(&cfg,"https://example.com/");result("webPageCreate",rc);
            if(R_SUCCEEDED(rc)){
                save();rc=webConfigShow(&cfg,&reply);result("webConfigShow",rc);
                if(R_SUCCEEDED(rc)){
                    WebExitReason why=(WebExitReason)0xffffffffU;
                    rc=webReplyGetExitReason(&reply,&why);result("webReplyGetExitReason",rc);
                    if(R_SUCCEEDED(rc))log_line("exitReason=%u (7 means error dialog; API success is not rendering success)\n",(unsigned)why);
                }
            }
            appletExit();
        }
        log_line("BASELINE_END\n");save();
    }
}
int main(void){
    consoleInit(NULL);Result rc=hidInitialize();
    if(R_FAILED(rc)){printf("Input failed: %08x\n",rc);consoleUpdate(NULL);svcSleepThread(2000000000LL);consoleExit(NULL);return 1;}
    PadState pad;padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);
    rc=fsOpenSdCardFileSystem(&sd);sd_ready=R_SUCCEEDED(rc);
    int dirty=1,armed=0;
    for(;;){
        padUpdate(&pad);u64 down=padGetButtonsDown(&pad);
        if(!armed){if(!padGetButtons(&pad))armed=1;down=0;}
        if(down&HidNpadButton_Plus)break;
        if(down&(HidNpadButton_A|HidNpadButton_X)){check((down&HidNpadButton_X)!=0);dirty=1;armed=0;}
        if(dirty){
            printf("\x1b[2J\x1b[HBROWSER STARTUP CHECK v0.1\n\nA: read startup settings only\nX: read settings, then normal example.com launch\nPLUS: exit\n\nNo system settings are changed.\nSpecial query is limited to physical firmware23.0.1.\nThis is a diagnostic, not an unlock.\n\n%s\n\nSD: %s\nReport: %s\n",status,sd_ready?"available":"unavailable (results will not be saved)",path[0]?path:"(none)");
            consoleUpdate(NULL);dirty=0;
        }
        svcSleepThread(16000000LL);
    }
    if(sd_ready)fsFsClose(&sd);
    hidExit();consoleExit(NULL);return 0;
}
