// SPDX-License-Identifier: MIT
// Switch 2 Hardware Tester - standalone, non-invasive diagnostics.
// No Discord credentials, external services, raw HID injection, or firmware modification.
#include <switch.h>
#include <switch/applets/web.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>

// Direct FS service instead of fsdevMountSdmc + stdio, which can fail on kaiHEN.
static char diagnostics[131072];
static size_t diagnostics_used=0;
static Result last_save=0xffffffffu,sd_access=0xffffffffu;
static const char *save_path="/switch/Hardware-Tester-report.txt";
static unsigned saved_events=0;
static char last_result[160]="No browser probe run.";
static unsigned event_count=0;

static Result save_report(void){
    FsFileSystem sd;
    Result rc=fsOpenSdCardFileSystem(&sd);
    sd_access=rc;
    if(R_FAILED(rc)){last_save=rc;return rc;}
    // /switch exists on standard SD installs. Fall back to SD root otherwise.
    const char *attempts[]={"/switch/Hardware-Tester-report.txt","/Hardware-Tester-report.txt"};
    for(unsigned i=0;i<2;i++){
        const char *path=attempts[i];
        (void)fsFsDeleteFile(&sd,path);
        rc=fsFsCreateFile(&sd,path,(s64)diagnostics_used,0);
        if(R_FAILED(rc))continue;
        FsFile f;
        rc=fsFsOpenFile(&sd,path,FsOpenMode_Write,&f);
        if(R_SUCCEEDED(rc)){
            rc=fsFileWrite(&f,0,diagnostics,(u64)diagnostics_used,FsWriteOption_Flush);
            fsFileClose(&f);
        }
        if(R_SUCCEEDED(rc)){save_path=path;break;}
    }
    fsFsClose(&sd);
    last_save=rc;
    if(R_SUCCEEDED(rc))saved_events=event_count;
    return rc;
}
static void report(const char *fmt, ...){
    if(diagnostics_used>=sizeof(diagnostics)-512)return;
    va_list ap;va_start(ap,fmt);
    int n=vsnprintf(diagnostics+diagnostics_used,sizeof(diagnostics)-diagnostics_used,fmt,ap);
    va_end(ap);
    if(n>0){
        size_t left=sizeof(diagnostics)-diagnostics_used;
        diagnostics_used+=(size_t)n<left?(size_t)n:left-1;
    }
}
static const char *button_name(unsigned bit){
    static const char *names[]={
        "A","B","X","Y","StickL","StickR","L","R","ZL","ZR","Plus","Minus",
        "DLeft","DUp","DRight","DDown","LStickLeft","LStickUp","LStickRight","LStickDown",
        "RStickLeft","RStickUp","RStickRight","RStickDown",
        "LeftSL","LeftSR","RightSL","RightSR","Palma","Verification","HandheldLeftB",
        "N64CLeft","N64CUp","N64CRight","N64CDown"
    };
    return bit<sizeof(names)/sizeof(names[0])?names[bit]:"Unknown/reserved";
}
static void record_buttons(u64 down,u64 up,u64 held){
    if(!(down|up))return;
    event_count++;
    report("event=%u down=%016" PRIx64 " up=%016" PRIx64 " held=%016" PRIx64 "\n",
        event_count,(uint64_t)down,(uint64_t)up,(uint64_t)held);
    for(unsigned bit=0;bit<64;bit++){
        u64 mask=1ULL<<bit;
        if((down|up)&mask)report("  bit=%02u %-18s %s\n",bit,button_name(bit),
            down&mask?"DOWN":"UP");
    }
}
static void browser_test(int which){
    const char *name=which==0?"Web page":which==1?"News page":which==2?"Offline HTML":"Wi-Fi portal";
    printf("\x1b[2J\x1b[H%s\n\n",name);
    puts("Launching a system applet may replace this screen.");
    puts("If it opens, close it normally to return.\n");
    consoleUpdate(NULL);
    report("BROWSER BEGIN %s\n",name);
    Result rc=appletInitialize();
    report("  appletInitialize=%08x\n",rc);
    if(R_FAILED(rc)){snprintf(last_result,sizeof(last_result),"%s: init=%08x",name,rc);(void)save_report();return;}
    if(which==3){
        WebWifiConfig cfg; WebWifiReturnValue reply={0};Uuid uuid={0};
        reply.res=~0u;
        webWifiCreate(&cfg,NULL,"https://example.com/",uuid,0);
        rc=webWifiShow(&cfg,&reply);
        report("  webWifiShow=%08x reply=%08x\n",rc,reply.res);
        snprintf(last_result,sizeof(last_result),"%s show=%08x reply=%08x",name,rc,reply.res);
    }else{
        WebCommonConfig cfg;WebCommonReply reply={0};Result created;
        if(which==0)created=webPageCreate(&cfg,"https://example.com/");
        else if(which==1)created=webNewsCreate(&cfg,"https://example.com/");
        else created=webOfflineCreate(&cfg,WebDocumentKind_OfflineHtmlPage,0,"test.htdocs/index.html");
        report("  configCreate=%08x\n",created);
        if(R_SUCCEEDED(created)){
            rc=webConfigShow(&cfg,&reply);
            report("  webConfigShow=%08x\n",rc);
            snprintf(last_result,sizeof(last_result),"%s create=%08x show=%08x",name,created,rc);
        }else snprintf(last_result,sizeof(last_result),"%s create=%08x",name,created);
    }
    appletExit();
    report("BROWSER END %s\n",name);
}
int main(void){
    consoleInit(NULL);
    Result h=hidInitialize();
    report("NS2 Hardware Tester v0.2 (direct FS service)\\nHID init=%08x\\n",h);
    report("Capture scope: standard Npad state (not physical HID bus).\\n");
    (void)save_report();
    if(R_FAILED(h)){
        printf("HID failed: %08x\nPress HOME to exit.\n",h);
        consoleUpdate(NULL);
        (void)save_report();
        return 1;
    }
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    PadState pad={0};padInitializeDefault(&pad);
    int mode=0,selection=0,frame=0;
    const char *opts[]={"Web page (example.com)","News page (example.com)",
        "Offline HTML (no bundled document)","Wi-Fi portal (example.com)"};
    u64 prior=0;
    while(appletMainLoop()){
        padUpdate(&pad);
        u64 held=padGetButtons(&pad),down=held&~prior,up=prior&~held;prior=held;
        record_buttons(down,up,held);
        if(mode==0){
            if(down&HidNpadButton_Up)selection=(selection+3)%4;
            if(down&HidNpadButton_Down)selection=(selection+1)%4;
            if(down&HidNpadButton_X){mode=1;report("INPUT MONITOR START\n");}
            if(down&HidNpadButton_A){browser_test(selection);prior=0;}
        }else if(down&HidNpadButton_B){
            mode=0;report("INPUT MONITOR END\n");
        }
        if(down&HidNpadButton_Y)(void)save_report();\n        if((down&HidNpadButton_Plus) && (held&HidNpadButton_Minus)){
            report("USER EXIT\n");break;
        }
        if((frame++%4)==0 || down || up){
            printf("\x1b[2J\x1b[H");
            puts("NS2 HARDWARE TESTER  v0.2");
            puts("--------------------------");
            printf("HID=%08x  SD=%08x  events=%u\n",h,sd,event_count);
            if(mode==0){
                puts("\nBrowser tests (UP/DOWN, A launch):");
                for(int i=0;i<4;i++)printf("%c %s\n",selection==i?'>':' ',opts[i]);
                puts("\nX: automatic button monitor");
                puts("Y: save now  |  MINUS + PLUS: exit");
                puts("\nLast result:");
                puts(last_result);
            }else{
                puts("\nLIVE INPUT MONITOR");
                puts("Press every controller button, especially C.");
                puts("Changes are recorded automatically.");
                puts("B: back to browser list");
                puts("Y: save now  |  MINUS + PLUS: exit");
                printf("\nHELD  %016" PRIx64 "\n",(uint64_t)held);
                printf("DOWN  %016" PRIx64 "\n",(uint64_t)down);
                printf("UP    %016" PRIx64 "\n",(uint64_t)up);
                puts("\nNOTE: If C opens GameChat without a mask change,");
                puts("it may be intercepted by Horizon.");
            }
            puts("\nLog: /switch/HardwareTester/report.txt");
            consoleUpdate(NULL);
        }
        svcSleepThread(16000000LL);
    }
    report("FINAL event_count=%u\n",event_count);
    if(logfile)fclose(logfile);
    if(R_SUCCEEDED(sd))fsdevUnmountDevice("sdmc");
    hidExit();
    consoleExit(NULL);
    return 0;
}
