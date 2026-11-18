// SPDX-License-Identifier: MIT
// Read resident plugin state and test whether basic service sessions can open.
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <switch.h>
#include <switch/services/bsd.h>
#include <kaihen.h>
#include "voice_packet.h"
#include "discord_gateway_probe.h"
#include "graphical_ui.h"
#include "native_login.h"
#include "native_font.h"
#include "browser_memory.h"
#include "wifi_browser_probe.h"

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


/* One NRO, many tests, with finite waits and a persistent single report.
 * No service replacement, no raw audio file, no Discord credentials.
 */
#include <stdlib.h>
#include <stdint.h>

static char diagnostics[12288];
static size_t diag_len;

static void diagf(const char *fmt, ...) {
    if (diag_len >= sizeof(diagnostics)-1) return;
    va_list args;
    va_start(args,fmt);
    int n=vsnprintf(diagnostics+diag_len,sizeof(diagnostics)-diag_len,fmt,args);
    va_end(args);
    if (n > 0) {
        size_t available=sizeof(diagnostics)-diag_len;
        diag_len+=(size_t)n<available?(size_t)n:available-1;
    }
}
static Result write_diagnostics(void) {
    FsFileSystem sd;
    Result rc=fsOpenSdCardFileSystem(&sd);
    if (R_FAILED(rc)) return rc;
    const char *path="/kaihen/discordnx2-hardware.txt";
    fsFsDeleteFile(&sd,path);
    rc=fsFsCreateFile(&sd,path,(s64)diag_len,0);
    if (R_SUCCEEDED(rc)) {
        FsFile file;
        rc=fsFsOpenFile(&sd,path,FsOpenMode_Write,&file);
        if (R_SUCCEEDED(rc)) {
            rc=fsFileWrite(&file,0,diagnostics,(u64)diag_len,FsWriteOption_Flush);
            fsFileClose(&file);
        }
    }
    fsFsClose(&sd);
    return rc;
}
static void plugin_snapshot(const char *phase) {
    Service ro={},hen={},plugin={};
    Result rc=kaihenGetRoSession(&ro);
    if (R_SUCCEEDED(rc)) rc=kaihenOpenHomebrewSession(&ro,&hen);
    u32 count=0;
    if (R_SUCCEEDED(rc)) rc=henGetPluginCount(&hen,&count);
    HenPluginName name={};
    memcpy(name.name,"discordnx2",sizeof("discordnx2"));
    if (R_SUCCEEDED(rc)) rc=henOpenPlugin(&hen,&name,&plugin);
    diagf("plugin %s: connection=%08x visible=%u\n",phase,rc,count);
    if (R_SUCCEEDED(rc)) {
        Dnx2Status state={};
        Result sr=serviceDispatchOut(&plugin,1,state);
        diagf("plugin %s: status=%08x magic=%08x abi=%u cmds=%u probes=%u generation=%u heartbeat=%u\n",
            phase,sr,state.magic,state.abi,state.command_count,state.probe_count,state.generation,state.heartbeat);
        if (R_SUCCEEDED(sr) && state.magic==DNX2_MAGIC && state.abi==2) {
            Dnx2Services services={};
            Result pr=serviceDispatchOut(&plugin,2,services);
            diagf("plugin %s: probe=%08x hid=%08x in=%08x out=%08x bsd=%08x count=%u\n",
                phase,pr,services.hid,services.audin,services.audout,services.bsd,services.probe_count);
        }
    }
    if(serviceIsActive(&plugin)) serviceClose(&plugin);
    if(serviceIsActive(&hen)) serviceClose(&hen);
    if(serviceIsActive(&ro)) serviceClose(&ro);
}

/* C-button-focused observation. These stages do not suppress GameChat.
 * Compare both Player 1 and handheld input, in addition to the normal
 * merged pad mask. A C-only event may never be published to HID.
 */
static const char *stream_names[]={
    "HandheldState","FullKeyState","SystemExtState","SystemState"
};

/* Poll the four different HID state histories exposed by libounce.
 * No direct controller-transport access, writes or unknown IPC commands.
 */
static size_t read_hid_stream(unsigned stream,HidNpadIdType id,
                              HidNpadCommonState *out) {
    switch(stream) {
    case 0: return hidGetNpadStatesHandheld(id,(HidNpadHandheldState *)out,1);
    case 1: return hidGetNpadStatesFullKey(id,(HidNpadFullKeyState *)out,1);
    case 2: return hidGetNpadStatesSystemExt(id,(HidNpadSystemExtState *)out,1);
    case 3: return hidGetNpadStatesSystem(id,(HidNpadSystemState *)out,1);
    default: return 0;
    }
}

static void controller_trials(void) {
    diagf("\n[C-BUTTON / FOUR HID STREAMS x TWO CONTROLLER IDS]\n");
    diagf("Stages: baseline 3s, C hold 15s, release 5s, C taps 10s, dismiss with B 8s, A/B 6s.\n");
    diagf("Channels are HandheldState, FullKeyState, SystemExtState, SystemState.\n");
    diagf("No GameChat override, no system patches, no arbitrary shared-memory scan.\n");
    Result rc=hidInitialize();
    diagf("hidInitialize=%08x\n",rc);
    if (R_FAILED(rc)) return;
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    const HidNpadIdType ids[]={HidNpadIdType_Handheld,HidNpadIdType_No1};
    const char *idnames[]={"handheld","player1"};
    const char *stage_names[]={"baseline","HOLD C","release C","tap C","dismiss GameChat B","A/B reference"};
    const u32 samples[]={150,750,250,500,400,300};
    for(unsigned id=0;id<2;id++) {
        diagf("controller=%s style=%08x device_type=%08x\n",
              idnames[id],hidGetNpadStyleSet(ids[id]),hidGetNpadDeviceType(ids[id]));
    }
    diagf("read-only hidsys metadata\n");
    Result hs=hidsysInitialize();
    diagf("hidsysInitialize=%08x\n",hs);
    if(R_SUCCEEDED(hs)) {
        u32 last=0;
        Result lr=hidsysGetLastActiveNpad(&last);
        diagf("last-active rc=%08x id=%u\n",lr,last);
        for(unsigned i=0;i<2;i++) {
            u8 interface_type=0;
            Result ir=hidsysGetNpadInterfaceType(ids[i],&interface_type);
            diagf("interface %s rc=%08x type=%u\n",idnames[i],ir,(unsigned)interface_type);
        }
        hidsysExit();
    }
    diagf("checkpoint=%08x\n",write_diagnostics());
    for(unsigned stage=0;stage<6;stage++) {
        u64 seen[2][4] = {{0}};
        u64 prior[2][4] = {{0}};
        u64 prior_sample[2][4] = {{0}};
        u32 valid[2][4] = {{0}},changes[2][4]={{0}},fresh[2][4]={{0}};
        unsigned recorded=0;
        diagf("stage %u (%s) start duration_ms=%u\n",stage+1,stage_names[stage],samples[stage]*20);
        for(u32 tick=0;tick<samples[stage];tick++) {
            for(unsigned id=0;id<2;id++) for(unsigned stream=0;stream<4;stream++) {
                HidNpadCommonState state={0};
                size_t n=read_hid_stream(stream,ids[id],&state);
                if(!n) continue;
                valid[id][stream]++;
                const u64 buttons=state.buttons;
                seen[id][stream] |= buttons;
                if(prior_sample[id][stream]!=state.sampling_number) {
                    fresh[id][stream]++;
                    prior_sample[id][stream]=state.sampling_number;
                }
                if(buttons!=prior[id][stream]) {
                    if(recorded<32)
                        diagf(" tick=%u id=%s stream=%s buttons=%016llx changed=%016llx sample=%llu attr=%08x\n",
                            tick,idnames[id],stream_names[stream],
                            (unsigned long long)buttons,
                            (unsigned long long)(buttons^prior[id][stream]),
                            (unsigned long long)state.sampling_number,state.attributes);
                    recorded++;
                    changes[id][stream]++;
                    prior[id][stream]=buttons;
                }
            }
            svcSleepThread(20000000LL);
        }
        for(unsigned id=0;id<2;id++) for(unsigned stream=0;stream<4;stream++) {
            diagf(" summary id=%s stream=%s valid=%u fresh=%u changes=%u seen=%016llx\n",
                idnames[id],stream_names[stream],valid[id][stream],fresh[id][stream],
                changes[id][stream],(unsigned long long)seen[id][stream]);
        }
        diagf("stage=%u checkpoint=%08x\n",stage+1,write_diagnostics());
    }
    diagf("If no stream changes during C, ordinary HID histories do not expose C here.\n");
    diagf("This does not establish anything about controller transport reports or GameChat internal APIs.\n");
    hidExit();
}
static void audio_trials(void) {
    diagf("\n[AUDIO INPUT]\n");
    Result rc=audinInitialize();
    diagf("audinInitialize=%08x\n",rc);
    if(R_SUCCEEDED(rc)) {
        AudioInState state=AudioInState_Stopped;
        diagf("sample_rate=%u channels=%u pcm_format=%u initial_state=%u\n",
             audinGetSampleRate(),audinGetChannelCount(),(unsigned)audinGetPcmFormat(),(unsigned)audinGetDeviceState());
        rc=audinGetAudioInState(&state);
        diagf("audinGetAudioInState=%08x state=%u\n",rc,(unsigned)state);
        rc=audinStartAudioIn();
        diagf("audinStartAudioIn=%08x\n",rc);
        if(R_SUCCEEDED(rc)) {
            /* 4096 aligned and sized for short PCM capture, bounded event wait.
             * Buffer remains allocated until audio input has been stopped.
             */
            static unsigned char samples[0x4000] __attribute__((aligned(0x1000)));
            memset(samples,0,sizeof(samples));
            AudioInBuffer input={0};
            input.buffer=samples;
            input.buffer_size=sizeof(samples);
            input.data_size=sizeof(samples);
            Result ar=audinAppendAudioInBuffer(&input);
            diagf("audinAppendAudioInBuffer=%08x\n",ar);
            if(R_SUCCEEDED(ar)) {
                AudioInBuffer *released=NULL;
                u32 released_count=0;
                Result wr=audinWaitCaptureFinish(&released,&released_count,1000000000ULL);
                diagf("audinWaitCaptureFinish=%08x released_count=%u same_buffer=%u data_size=%llu\n",
                    wr,released_count,released==&input,
                    (unsigned long long)input.data_size);
                if(R_SUCCEEDED(wr)&&released_count && released==&input) {
                    int peak=0;
                    unsigned nonzero=0;
                    const int16_t *pcm=(const int16_t *)samples;
                    size_t n=input.data_size/sizeof(int16_t);
                    if(n>sizeof(samples)/sizeof(int16_t)) n=sizeof(samples)/sizeof(int16_t);
                    for(size_t i=0;i<n;i++) {
                        int value=pcm[i];
                        int absval=value<0?-value:value;
                        if(absval>peak) peak=absval;
                        if(value) nonzero++;
                    }
                    diagf("captured samples=%u nonzero=%u peak=%d (no PCM saved)\n",
                        (unsigned)n,nonzero,peak);
                }
            }
            Result stop=audinStopAudioIn();
            diagf("audinStopAudioIn=%08x\n",stop);
        }
        audinExit();
    }
    diagf("\n[AUDIO OUTPUT]\n");
    Result orc=audoutInitialize();
    diagf("audoutInitialize=%08x\n",orc);
    if(R_SUCCEEDED(orc)) {
        diagf("sample_rate=%u channels=%u pcm_format=%u state=%u\n",
            audoutGetSampleRate(),audoutGetChannelCount(),
            (unsigned)audoutGetPcmFormat(),(unsigned)audoutGetDeviceState());
        AudioOutState state=AudioOutState_Stopped;
        Result sr=audoutGetAudioOutState(&state);
        diagf("audoutGetAudioOutState=%08x state=%u\n",sr,(unsigned)state);
        Result start=audoutStartAudioOut();
        diagf("audoutStartAudioOut=%08x\n",start);
        if(R_SUCCEEDED(start)) {
            Result stop=audoutStopAudioOut();
            diagf("audoutStopAudioOut=%08x\n",stop);
        }
        audoutExit();
    }
}
static void service_trials(void) {
    diagf("\n[SERVICE SESSIONS: open/close only]\n");
    const char *names[]={"hid","audin:u","audout:u","bsd:u","aud:u"};
    for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++) {
        Service svc={};
        Result rc=smGetService(&svc,names[i]);
        diagf("smGetService %-8s =%08x\n",names[i],rc);
        if(R_SUCCEEDED(rc)) serviceClose(&svc);
    }
}

/* v0.7: bounded 48kHz stereo PCM capture and delayed playback.
 * Audio is kept in static memory and never written to the SD card.
 * No unattended/open mic after exiting the diagnostic NRO.
 */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>

#define VOICE_CHUNK_BYTES 0x4000u
#define VOICE_CHUNKS 36u
#define VOICE_MAX_BYTES (VOICE_CHUNK_BYTES * VOICE_CHUNKS)
#define VOICE_NETWORK_PORT 39272u
static unsigned char voice_output[VOICE_MAX_BYTES] __attribute__((aligned(0x1000)));
static size_t voice_used;

static void voice_stats(const int16_t *pcm,size_t count,unsigned frame) {
    unsigned nonzero=0,active=0,clips=0;
    unsigned long long sumabs=0;
    int peak=0;
    for(size_t i=0;i<count;i++) {
        int value=(int)pcm[i];
        int absval=value<0?-value:value;
        if(absval>peak)peak=absval;
        sumabs+=(unsigned)absval;
        if(value!=0)nonzero++;
        if(absval>=400)active++;
        if(absval>=32000)clips++;
    }
    const unsigned avg=count?(unsigned)(sumabs/count):0;
    diagf(" frame=%u samples=%u peak=%d avg_abs=%u active_ge400=%u nonzero=%u clips=%u\n",
        frame,(unsigned)count,peak,avg,active,nonzero,clips);
}

/* Capture a longer spoken phrase and compute basic signal diagnostics.
 * Apply modest software microphone gain and clamp before playback.
 * Captured audio never leaves this NRO's memory.
 */
static void voice_prepare_playback(void);

/* Pre-queue three microphone buffers before starting the capture.
 * Each released buffer is replaced as soon as possible; the logging,
 * PCM scanning and SD checkpoints run AFTER capture is stopped.
 */
#define CAPTURE_SLOTS 3
static unsigned char capture_pcm[CAPTURE_SLOTS][VOICE_CHUNK_BYTES] __attribute__((aligned(0x1000)));
static AudioInBuffer capture_desc[CAPTURE_SLOTS];
static unsigned capture_lengths[VOICE_CHUNKS];
static void voice_capture_test(void) {
    voice_used=0;
    memset(capture_lengths,0,sizeof(capture_lengths));
    diagf("\n[VOICE: PREQUEUED 3-BUFFER CAPTURE]\n");
    Result rc=audinInitialize();
    diagf("capture initialize=%08x\n",rc);
    if(R_FAILED(rc))return;
    const u32 rate=audinGetSampleRate(),ch=audinGetChannelCount();
    const unsigned fmt=(unsigned)audinGetPcmFormat();
    diagf("capture rate=%u channels=%u format=%u\n",rate,ch,fmt);
    if(rate!=48000||ch!=2||fmt!=(unsigned)PcmFormat_Int16) {
        diagf("SKIP capture: format mismatch\n");
        audinExit();
        return;
    }
    memset(capture_desc,0,sizeof(capture_desc));
    for(unsigned i=0;i<CAPTURE_SLOTS;i++) {
        memset(capture_pcm[i],0,VOICE_CHUNK_BYTES);
        capture_desc[i].buffer=capture_pcm[i];
        capture_desc[i].buffer_size=VOICE_CHUNK_BYTES;
        capture_desc[i].data_size=VOICE_CHUNK_BYTES;
    }
    unsigned queued_count=0;
    for(unsigned i=0;i<CAPTURE_SLOTS;i++) {
        Result qr=audinAppendAudioInBuffer(&capture_desc[i]);
        if(R_FAILED(qr)) {
            diagf("capture initial queue error slot=%u result=%08x\n",i,qr);
            break;
        }
        queued_count++;
    }
    if(queued_count!=CAPTURE_SLOTS) {
        /* Do not free or reuse buffer memory until device has stopped. */
        audinExit();
        return;
    }
    rc=audinStartAudioIn();
    diagf("capture start=%08x queued=%u\n",rc,queued_count);
    if(R_FAILED(rc)){audinExit();return;}
    unsigned recorded=0,submitted=queued_count,failed=0,bad_release=0;
    while(recorded<VOICE_CHUNKS) {
        AudioInBuffer *released=NULL;
        u32 count=0;
        Result wr=audinWaitCaptureFinish(&released,&count,1200000000ULL);
        if(R_FAILED(wr)||!count||!released) {
            diagf("capture wait failed at=%u rc=%08x count=%u\n",recorded,wr,count);
            failed++;
            break;
        }
        int slot=-1;
        for(unsigned i=0;i<CAPTURE_SLOTS;i++)if(released==&capture_desc[i]){slot=(int)i;break;}
        if(slot<0) {
            diagf("capture unexpected release count=%u\n",count);
            bad_release++;
            break;
        }
        size_t length=released->data_size;
        if(length>VOICE_CHUNK_BYTES)length=VOICE_CHUNK_BYTES;
        length &= ~(size_t)3; /* stereo 16-bit frame alignment */
        if(!length || voice_used+length>sizeof(voice_output)) {
            diagf("capture invalid length=%u slot=%d\n",(unsigned)length,slot);
            failed++;
            break;
        }
        memcpy(voice_output+voice_used,capture_pcm[slot],length);
        voice_used+=length;
        capture_lengths[recorded]=(unsigned)length;
        recorded++;
        if(submitted<VOICE_CHUNKS) {
            /* Queue the released descriptor immediately, before stats/logging. */
            memset(&capture_desc[slot],0,sizeof(capture_desc[slot]));
            capture_desc[slot].buffer=capture_pcm[slot];
            capture_desc[slot].buffer_size=VOICE_CHUNK_BYTES;
            capture_desc[slot].data_size=VOICE_CHUNK_BYTES;
            Result qr=audinAppendAudioInBuffer(&capture_desc[slot]);
            if(R_FAILED(qr)) {
                diagf("capture requeue error frame=%u rc=%08x\n",recorded,qr);
                failed++;
                break;
            }
            submitted++;
        }
    }
    Result stop=audinStopAudioIn();
    diagf("capture stop=%08x recorded=%u submitted=%u failures=%u unexpected=%u bytes=%u\n",
          stop,recorded,submitted,failed,bad_release,(unsigned)voice_used);
    audinExit();
    /* Post-capture analysis must not hold up the hardware queue. */
    size_t offset=0;
    unsigned quiet=0,large_boundary=0,max_boundary=0;
    for(unsigned i=0;i<recorded;i++) {
        const unsigned length=capture_lengths[i];
        const int16_t *samples=(const int16_t*)(voice_output+offset);
        int peak=0;
        for(unsigned k=0;k<length/2;k++) {
            int x=samples[k],a=x<0?-x:x;
            if(a>peak)peak=a;
        }
        if(peak<100)quiet++;
        if(i>0 && length>=4 && offset>=4) {
            const int16_t *prev=(const int16_t*)(voice_output+offset-4);
            for(unsigned channel=0;channel<2;channel++) {
                int delta=(int)samples[channel]-(int)prev[channel];
                if(delta<0)delta=-delta;
                if(delta>max_boundary)max_boundary=delta;
                if(delta>10000)large_boundary++;
            }
        }
        voice_stats(samples,length/2,i);
        offset+=length;
    }
    diagf("capture boundaries_jump_gt10000=%u max_boundary_jump=%u quiet_frames=%u\n",
          large_boundary,max_boundary,quiet);
    voice_prepare_playback();
    diagf("voice captured_frames=%u errors=%u pcm_bytes=%u unity_gain=1.0 fade_ms=10\n",
          recorded,failed,(unsigned)voice_used);
    diagf("voice capture checkpoint=%08x\n",write_diagnostics());
}

/* v0.15 sustained, non-recording stress test of the successful
 * prequeued microphone architecture. No PCM is saved or transmitted.
 * Requeue immediately and do all diagnostics after stopping.
 * A fixed upper bound means this cannot become an unattended open mic.
 */
#define STRESS_CAPTURE_BUFFERS 180u /* nominal ~15.36 seconds */
static void voice_capture_stability_test(void) {
    diagf("\n[VOICE: SUSTAINED MICROPHONE STABILITY TEST]\n");
    Result rc=audinInitialize();
    diagf("stress initialize=%08x\n",rc);
    if(R_FAILED(rc))return;
    if(audinGetSampleRate()!=48000 || audinGetChannelCount()!=2 ||
       audinGetPcmFormat()!=PcmFormat_Int16) {
        diagf("stress skipped: unexpected format\n");
        audinExit();return;
    }
    for(unsigned i=0;i<CAPTURE_SLOTS;i++) {
        memset(capture_pcm[i],0,sizeof(capture_pcm[i]));
        memset(&capture_desc[i],0,sizeof(capture_desc[i]));
        capture_desc[i].buffer=capture_pcm[i];
        capture_desc[i].buffer_size=VOICE_CHUNK_BYTES;
        capture_desc[i].data_size=VOICE_CHUNK_BYTES;
    }
    unsigned queued=0;
    for(unsigned i=0;i<CAPTURE_SLOTS;i++) {
        Result qr=audinAppendAudioInBuffer(&capture_desc[i]);
        if(R_FAILED(qr)) {
            diagf("stress queue failure=%08x slot=%u\n",qr,i);
            break;
        }
        queued++;
    }
    if(queued!=CAPTURE_SLOTS){audinExit();return;}
    rc=audinStartAudioIn();
    diagf("stress start=%08x\n",rc);
    if(R_FAILED(rc)){audinExit();return;}
    unsigned received=0,submitted=queued,errors=0;
    unsigned long long bytes=0;
    unsigned peak=0;
    for(;received<STRESS_CAPTURE_BUFFERS;) {
        AudioInBuffer *released=NULL;
        u32 count=0;
        Result wr=audinWaitCaptureFinish(&released,&count,1500000000ULL);
        if(R_FAILED(wr)||!released||count==0) {
            diagf("stress wait fail rc=%08x count=%u at=%u\n",wr,count,received);
            errors++;break;
        }
        int slot=-1;
        for(unsigned i=0;i<CAPTURE_SLOTS;i++)
            if(released==&capture_desc[i]){slot=(int)i;break;}
        if(slot<0){errors++;diagf("stress release invalid\n");break;}
        size_t size=released->data_size;
        if(size>VOICE_CHUNK_BYTES)size=VOICE_CHUNK_BYTES;
        bytes+=size;
        /* Only a small peak scan, no filesystem access on capture path. */
        const int16_t *samples=(const int16_t*)capture_pcm[slot];
        for(size_t j=0;j<size/2;j++) {
            int v=samples[j],a=v<0?-v:v;
            if((unsigned)a>peak)peak=(unsigned)a;
        }
        received++;
        if(submitted<STRESS_CAPTURE_BUFFERS) {
            memset(released,0,sizeof(*released));
            released->buffer=capture_pcm[slot];
            released->buffer_size=VOICE_CHUNK_BYTES;
            released->data_size=VOICE_CHUNK_BYTES;
            Result qr=audinAppendAudioInBuffer(released);
            if(R_FAILED(qr)){
                diagf("stress requeue fail rc=%08x at=%u\n",qr,received);
                errors++;break;
            }
            submitted++;
        }
    }
    Result stop=audinStopAudioIn();
    audinExit();
    diagf("stress end stopped=%08x buffers=%u expected=%u submitted=%u errors=%u bytes=%llu peak=%u\n",
          stop,received,(unsigned)STRESS_CAPTURE_BUFFERS,submitted,errors,bytes,peak);
    diagf("stress capture is local-only and never writes PCM or sends network audio.\n");
    diagf("stress checkpoint=%08x\n",write_diagnostics());
}

/* Process the entire recording as one signal rather than hard-clipping
 * each chunk separately. A global gain and short fade at the endpoints
 * avoid some distortion and abrupt playback clicks. Other pops could
 * still originate in capture discontinuities or audio hardware.
 */
static void voice_prepare_playback(void) {
    if(!voice_used) return;
    int16_t *samples=(int16_t*)voice_output;
    const size_t total=voice_used/sizeof(int16_t);
    int peak=0, discontinuities=0, boundary_jumps=0;
    int previous=0;
    for(size_t i=0;i<total;i++) {
        int v=samples[i],a=v<0?-v:v;
        if(a>peak)peak=a;
        if(i>=2 && (i%2)==0) {
            int delta=v-previous;
            if(delta<0) delta=-delta;
            if(delta>10000) discontinuities++;
        }
        previous=v;
        /* 16 KiB recording chunks; compare corresponding channel.
         * A large jump across this boundary may indicate dropped samples.
         */
        if(i>=VOICE_CHUNK_BYTES/2 && ((i%(VOICE_CHUNK_BYTES/2))<2)) {
            int diff=v-(int)samples[i-2];
            if(diff<0)diff=-diff;
            if(diff>10000)boundary_jumps++;
        }
    }
    /* Diagnostic A/B test: unity gain (no boost or peak limiter).
     * Keep only the short fade to avoid playback start/end clicks.
     */
    const float gain=1.0f;
    const size_t fade_frames=480; /* 10 ms at 48kHz */
    const size_t frames=total/2;
    for(size_t frame=0;frame<frames;frame++) {
        float fade=1.0f;
        if(frame<fade_frames)fade=(float)frame/(float)fade_frames;
        if(frames-1-frame<fade_frames) {
            float end=(float)(frames-1-frame)/(float)fade_frames;
            if(end<fade)fade=end;
        }
        for(size_t channel=0;channel<2;channel++) {
            size_t i=frame*2+channel;
            int result=(int)((float)samples[i]*fade);
            samples[i]=(int16_t)result;
        }
    }
    diagf("signal original_peak=%d gain_x100=%u limiting_target=none channel_jump_gt10000=%d chunk_boundary_flags=%d fade_ms=10\n",
          peak,(unsigned)(gain*100.0f),discontinuities,boundary_jumps);
}

/* Controlled playback reference: no microphone or gain processing involved.
 * Quiet 400 Hz triangle tone with a 10 ms fade at both ends.
 * A clean tone followed by crackling speech points toward the speech path.
 */
static unsigned char reference_tone[0x18000] __attribute__((aligned(0x1000)));

/* Three rotating 8 KiB buffers, modeled after KaiPlayer's playback
 * scheduling (MIT; https://github.com/mizuiroame/kaiplayer).
 * The source samples live in memory; copy them only into a released slot.
 * Keep service running while queued, then stop and flush on teardown.
 */
#define PLAYBACK_SLOTS 3
#define PLAYBACK_SLOT_BYTES 8192u
#define PLAYBACK_SILENCE_BYTES 49152u  /* 256 ms @ 48 kHz stereo s16 */
static unsigned char playback_slots[PLAYBACK_SLOTS][PLAYBACK_SLOT_BYTES] __attribute__((aligned(0x1000)));
static AudioOutBuffer playback_desc[PLAYBACK_SLOTS];

static void play_buffered_pcm(const unsigned char *pcm, size_t bytes, const char *tag) {
    diagf("\n[%s / 3-BUFFER STREAMING OUTPUT]\n",tag);
    Result rc=audoutInitialize();
    diagf("stream initialize=%08x\n",rc);
    if(R_FAILED(rc))return;
    const u32 rate=audoutGetSampleRate(),ch=audoutGetChannelCount();
    const unsigned fmt=(unsigned)audoutGetPcmFormat();
    diagf("stream format rate=%u channels=%u pcm=%u\n",rate,ch,fmt);
    if(rate!=48000 || ch!=2 || fmt!=(unsigned)PcmFormat_Int16){
        diagf("stream SKIP: unsupported format\n");
        audoutExit();
        return;
    }
    memset(playback_desc,0,sizeof(playback_desc));
    bool queued[PLAYBACK_SLOTS]={false};
    const size_t total=bytes+PLAYBACK_SILENCE_BYTES;
    size_t position=0;
    unsigned submissions=0, releases=0, queue_errors=0, wait_errors=0;
    unsigned iterations=0;
    Result start=audoutStartAudioOut();
    diagf("stream start=%08x samples_bytes=%u silent_tail_bytes=%u\n",
          start,(unsigned)bytes,(unsigned)PLAYBACK_SILENCE_BYTES);
    if(R_SUCCEEDED(start)) {
        /* Long enough for ~3 seconds speech and a short output tail.
         * Each wait is bounded; a stalled audio service cannot loop forever.
         */
        while(iterations++<650) {
            for(unsigned i=0;i<PLAYBACK_SLOTS;i++) {
                if(queued[i] || position>=total)continue;
                size_t amount=total-position;
                if(amount>PLAYBACK_SLOT_BYTES)amount=PLAYBACK_SLOT_BYTES;
                memset(playback_slots[i],0,PLAYBACK_SLOT_BYTES);
                size_t from_voice=0;
                if(position<bytes) {
                    from_voice=bytes-position;
                    if(from_voice>amount)from_voice=amount;
                    memcpy(playback_slots[i],pcm+position,from_voice);
                }
                AudioOutBuffer *buffer=&playback_desc[i];
                memset(buffer,0,sizeof(*buffer));
                buffer->buffer=playback_slots[i];
                buffer->buffer_size=PLAYBACK_SLOT_BYTES;
                buffer->data_size=amount;
                Result qr=audoutAppendAudioOutBuffer(buffer);
                if(R_FAILED(qr)){
                    diagf("stream queue failure=%08x slot=%u position=%u\n",
                          qr,i,(unsigned)position);
                    queue_errors++;
                    break;
                }
                queued[i]=true;
                position+=amount;
                submissions++;
            }
            unsigned pending=0;
            for(unsigned i=0;i<PLAYBACK_SLOTS;i++)pending+=queued[i]?1:0;
            if((position>=total && pending==0) || queue_errors)break;
            AudioOutBuffer *released=NULL;
            u32 released_count=0;
            Result wr=audoutWaitPlayFinish(&released,&released_count,20000000ULL);
            if(R_FAILED(wr))wait_errors++;
            /* The API can return a released count >1. Poll all slots,
             * not just its first released pointer.
             */
            for(unsigned i=0;i<PLAYBACK_SLOTS;i++) {
                if(!queued[i])continue;
                bool contains=true;
                Result cr=audoutContainsAudioOutBuffer(&playback_desc[i],&contains);
                if(R_SUCCEEDED(cr) && !contains) {
                    queued[i]=false;
                    releases++;
                } else if(R_FAILED(cr) && released==&playback_desc[i]) {
                    queued[i]=false;
                    releases++;
                }
            }
        }
        diagf("stream completed bytes_queued=%u total=%u submissions=%u releases=%u waits_failed=%u queue_errors=%u iterations=%u\n",
              (unsigned)position,(unsigned)total,submissions,releases,wait_errors,queue_errors,iterations);
        svcSleepThread(50000000LL);
        Result stop=audoutStopAudioOut();
        diagf("stream stop=%08x\n",stop);
        bool flushed=false;
        Result fr=audoutFlushAudioOutBuffers(&flushed);
        diagf("stream flush=%08x flushed=%u\n",fr,flushed);
    }
    audoutExit();
}

static unsigned char reference_tone[0x18000] __attribute__((aligned(0x1000)));
static void voice_reference_tone_test(void) {
    diagf("\n[AUDIO REFERENCE TONE]\n");
    const unsigned frames=24000;
    int16_t *pcm=(int16_t*)reference_tone;
    memset(reference_tone,0,sizeof(reference_tone));
    for(unsigned i=0;i<frames;i++) {
        unsigned phase=i%120;
        int value=phase<60?(int)phase*100-3000:9000-(int)phase*100;
        unsigned ramp=i;
        if(frames-1-i<ramp)ramp=frames-1-i;
        if(ramp<480)value=(value*(int)ramp)/480;
        pcm[2*i]=(int16_t)value;
        pcm[2*i+1]=(int16_t)value;
    }
    play_buffered_pcm(reference_tone,frames*2*sizeof(int16_t),"REFERENCE TONE");
}

static void voice_playback_test(void) {
    if(!voice_used){
        diagf("voice playback skipped: no captured PCM\n");
        return;
    }
    play_buffered_pcm(voice_output,voice_used,"CAPTURED SPEECH");
}
/* A network transport smoke test that sends only a fixed marker to itself.
 * Never sends microphone data or Discord credentials.
 */
static uint8_t voice_packet_data[DNX2_PACKET_BYTES];
static int16_t voice_decoded[DNX2_FRAME_FRAMES];
static uint8_t latest_packet[DNX2_PACKET_BYTES];
static int latest_packet_available;
static void voice_packet_test(void) {
    diagf("\n[VOICE CORE: 10ms MONO FRAMES / GATE / PACKET VALIDATION]\n");
    if(voice_used<DNX2_FRAME_FRAMES*4u) {
        diagf("packet test skipped: no PCM\n");return;
    }
    Dnx2VoiceGate gate={.hangover=0,.threshold_rms=220};
    unsigned frames=(unsigned)(voice_used/(DNX2_FRAME_FRAMES*4u));
    unsigned valid=0,invalid=0,active=0;
    unsigned peak=0,max_rms=0;
    latest_packet_available=0;
    for(unsigned i=0;i<frames;i++) {
        const int16_t *stereo=(const int16_t*)(voice_output+(size_t)i*DNX2_FRAME_FRAMES*4u);
        Dnx2PacketInfo encoded={0},decoded={0};
        size_t len=dnx2PacketEncode(stereo,i,&gate,voice_packet_data,
                                   sizeof(voice_packet_data),&encoded);
        int ok=len==DNX2_PACKET_BYTES &&
               dnx2PacketDecode(voice_packet_data,len,voice_decoded,
                                DNX2_FRAME_FRAMES,&decoded);
        if(ok) {
            for(unsigned j=0;j<DNX2_FRAME_FRAMES;j++) {
                int mixed=((int)stereo[2*j]+(int)stereo[2*j+1])/2;
                if(voice_decoded[j]!=mixed){ok=0;break;}
            }
        }
        if(ok && encoded.sequence==decoded.sequence &&
           decoded.timestamp==i*DNX2_FRAME_FRAMES) {
            valid++;if(decoded.active)active++;
            if(decoded.peak>peak)peak=decoded.peak;
            if(decoded.rms>max_rms)max_rms=decoded.rms;
            if(i==frames/2u) {
                memcpy(latest_packet,voice_packet_data,sizeof(latest_packet));
                latest_packet_available=1;
            }
        } else invalid++;
    }
    /* Verify corruption detection without modifying the saved test packet. */
    unsigned rejected_corrupt=0;
    if(latest_packet_available) {
        latest_packet[DNX2_PACKET_HEADER+4]^=0x40;
        rejected_corrupt=!dnx2PacketDecode(latest_packet,sizeof(latest_packet),
                                           voice_decoded,DNX2_FRAME_FRAMES,NULL);
        latest_packet[DNX2_PACKET_HEADER+4]^=0x40;
    }
    diagf("PCM frames_10ms=%u valid=%u invalid=%u active=%u peak=%u max_rms=%u corruption_rejected=%u\n",
          frames,valid,invalid,active,peak,max_rms,rejected_corrupt);
    diagf("packet size=%u mono_rate=48000 frame_ms=10 checksum=FNV1a (NOT encryption)\n",
          (unsigned)DNX2_PACKET_BYTES);
}

static void voice_network_test(void) {
    diagf("\n[NETWORK: SWITCH SOCKET DRIVER / LOCAL UDP / NEVER SEND MIC DATA]\n");
    Result rc=socketInitialize(NULL); /* proper BSD protocol version selection */
    diagf("socketInitialize(default)=%08x\n",rc);
    if(R_FAILED(rc)) {
        SocketInitConfig cfg=*socketGetDefaultInitConfig();
        cfg.num_bsd_sessions=1;
        cfg.bsd_service_type=BsdServiceType_User;
        rc=socketInitialize(&cfg);
        diagf("socketInitialize(single-session)=%08x\n",rc);
    }
    if(R_FAILED(rc)) {
        diagf("socket setup unavailable; no remote traffic attempted\n");
        return;
    }
    int server=socket(AF_INET,SOCK_DGRAM,0);
    int client=socket(AF_INET,SOCK_DGRAM,0);
    diagf("UDP sockets server=%d client=%d errno=%d\n",server,client,errno);
    if(server>=0&&client>=0) {
        struct sockaddr_in addr={0};
        addr.sin_family=AF_INET;
        addr.sin_port=htons(VOICE_NETWORK_PORT);
        addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        int br=bind(server,(const struct sockaddr *)&addr,sizeof(addr));
        diagf("bind localhost:%u result=%d errno=%d\n",VOICE_NETWORK_PORT,br,errno);
        if(br==0) {
            static const char marker[]="DNX2_V15_UDP_LOCAL";
            ssize_t sent=sendto(client,marker,sizeof(marker),0,
                                (const struct sockaddr *)&addr,sizeof(addr));
            fd_set fds;FD_ZERO(&fds);FD_SET(server,&fds);
            struct timeval wait={0,500000};
            int ready=select(server+1,&fds,NULL,NULL,&wait);
            char recvbuf[64]={0};
            ssize_t received=ready>0?recv(server,recvbuf,sizeof(recvbuf),0):-1;
            diagf("UDP marker sent=%d ready=%d received=%d match=%u errno=%d\n",
                  (int)sent,ready,(int)received,
                  received==(ssize_t)sizeof(marker)&&
                  memcmp(marker,recvbuf,sizeof(marker))==0,errno);
            if(latest_packet_available) {
                sent=sendto(client,latest_packet,sizeof(latest_packet),0,
                            (const struct sockaddr *)&addr,sizeof(addr));
                FD_ZERO(&fds);FD_SET(server,&fds);wait.tv_sec=0;wait.tv_usec=500000;
                ready=select(server+1,&fds,NULL,NULL,&wait);
                unsigned char packet[DNX2_PACKET_BYTES]={0};
                received=ready>0?recv(server,packet,sizeof(packet),0):-1;
                int ok=received==(ssize_t)sizeof(packet)&&
                  dnx2PacketDecode(packet,sizeof(packet),voice_decoded,DNX2_FRAME_FRAMES,NULL);
                diagf("UDP diagnostic PCM frame sent=%d received=%d validated=%u local_only=1\n",
                      (int)sent,(int)received,ok);
            }
        }
    }
    if(client>=0)close(client);
    if(server>=0)close(server);
    socketExit();
    diagf("No remote host contacted, no Discord authentication attempted.\n");
}

static void hardware_report(void) {
    diag_len=0;
    diagf("DiscordNX2 native full integration v0.17\n");
    diagf("Foreground-only, ~3 seconds PCM capture, unity gain 1.0x and 10ms fade, then playback.\n");
    diagf("No C-button interception, no GameChat changes, no persistent mic recording.\n");
    diagf("initial report write=%08x\n",write_diagnostics());
    plugin_snapshot("before");
    diagf("plugin checkpoint=%08x\n",write_diagnostics());
    voice_capture_test();
    voice_reference_tone_test();
    diagf("reference checkpoint=%08x\n",write_diagnostics());
    voice_playback_test();
    diagf("audio checkpoint=%08x\n",write_diagnostics());
    voice_packet_test();
    voice_capture_stability_test();
    diagf("packet checkpoint=%08x\n",write_diagnostics());
    voice_network_test();
    /* Network requests are unauthenticated public Discord Gateway only. */
    Dnx2DiscordProbe gateway=dnx2DiscordGatewayProbe(diagf);
    diagf("discord result tcp=%u tls=%u websocket=%u hello=%u interval=%u\n",
          gateway.tcp_ok,gateway.tls_ok,gateway.websocket_ok,gateway.hello_ok,gateway.hello_interval_ms);
    diagf("discord checkpoint=%08x\n",write_diagnostics());
    service_trials();
    plugin_snapshot("after");
    diagf("DONE v0.17. Microphone closed, audio output closed, network sockets closed.\n");
    diagf("final report write=%08x\n",write_diagnostics());
    (void)write_diagnostics();
}

/* Foreground Switch 2 native controls.
 * The test suite is explicitly started by the user, never on startup.
 * This UI does NOT ask for or persist Discord account credentials.
 */
static void run_discord_only(void) {
    diag_len=0;
    diagf("DiscordNX2 v0.17 Discord Public Gateway Test\n");
    plugin_snapshot("before");
    Dnx2DiscordProbe g=dnx2DiscordGatewayProbe(diagf);
    diagf("Discord Gateway result TCP=%u TLS=%u WebSocket=%u HELLO=%u HEARTBEAT_MS=%u HTTP=%u\n",
          g.tcp_ok,g.tls_ok,g.websocket_ok,g.hello_ok,g.hello_interval_ms,g.http_status);
    plugin_snapshot("after");
    diagf("Final report write=%08x\n",write_diagnostics());
    (void)write_diagnostics();
}
static void run_voice_only(void) {
    diag_len=0;
    diagf("DiscordNX2 v0.17 audio capture+playback+packet test\n");
    plugin_snapshot("before");
    voice_capture_test();
    voice_reference_tone_test();
    voice_playback_test();
    voice_packet_test();
    plugin_snapshot("after");
    diagf("Final report write=%08x\n",write_diagnostics());
    (void)write_diagnostics();
}
static void run_stress_only(void) {
    diag_len=0;
    diagf("DiscordNX2 v0.17 extended microphone capture\n");
    plugin_snapshot("before");
    voice_capture_stability_test();
    plugin_snapshot("after");
    diagf("Final report write=%08x\n",write_diagnostics());
    (void)write_diagnostics();
}
static void show_main_menu(const char *status) {
    consoleClear();
    printf("=======================================\n");
    printf(" DiscordNX2 v0.17 - Switch 2 Native\n");
    printf("=======================================\n\n");
    printf(" A  MASSIVE FULL TEST\n");
    printf("    Mic, playback, packet transport,\n");
    printf("    ~15s sustained mic, local UDP,\n");
    printf("    Discord TLS + WebSocket HELLO.\n\n");
    printf(" X  Discord Gateway connectivity\n");
    printf(" Y  Audio + voice packet pipeline\n");
    printf(" L  Sustained microphone stability\n");
    printf(" +  Exit\n\n");
    printf("---------------------------------------\n");
    printf(" Status: %s\n",status);
    printf(" Discord login/voice NOT yet enabled.\n");
    printf(" No user tokens, passwords or audio\n");
    printf(" are transmitted to Discord.\n\n");
    printf(" Report: /kaihen/discordnx2-hardware.txt\n");
    printf("---------------------------------------\n");
    consoleUpdate(NULL);
}
static void run_console_ui(void);
static void dnx2AuthProgressAndReport(const char *phase,const char *qr_url) {
    /* Only static status labels reach SD diagnostics. Never QR URLs, tokens, IDs. */
    diagf("auth stage=%s\n",phase?phase:"UNKNOWN");
    (void)write_diagnostics();
    dnx2GraphicalAuthStatus(phase,qr_url);
}
static void browser_memory_report(const char *stage,Result result) {
    /* Persist before executing code so a console fault retains the last step. */
    diagf("browser memory: %s result=%08x\n",stage,result);
    (void)write_diagnostics();
}
static void wifi_browser_report(const char *stage,Result result) {
    diagf("wifi browser: %s result=%08x\n",stage,result);
    (void)write_diagnostics();
}
static void run_gui_action(int action) {
    if(action==8) {
        diag_len=0;
        diagf("DiscordNX2 v0.36 Wi-Fi browser launch test - no account data or network changes\n");
        dnx2GraphicalAuthBegin();
        dnx2GraphicalWifiBrowserStatus("Opening the system Wi-Fi browser...",0);
        dnx2GraphicalAuthEnd();
        int returned=dnx2WifiBrowserProbe(wifi_browser_report);
        diagf("wifi browser: API completed=%u (not proof a page opened)\n",returned);
        (void)write_diagnostics();
        dnx2GraphicalAuthBegin();
        int visible=dnx2GraphicalWifiBrowserStatus("Did you see the Example Domain page?",1);
        int answer=0;
        while(visible&&appletMainLoop()&&!(answer=dnx2GraphicalBrowserAnswer()))svcSleepThread(16000000LL);
        diagf("wifi browser: user confirmed Example Domain page=%u answered=%u\n",answer==1,answer==1||answer==-1);
        (void)write_diagnostics();
        dnx2GraphicalAuthEnd();
    }
    else if(action==7) {
        diag_len=0;
        diagf("DiscordNX2 v0.36 executable memory check - no network or account data\n");
        dnx2GraphicalAuthBegin();
        dnx2GraphicalBrowserCheckStatus("Checking executable memory...",0);
        int passed=dnx2BrowserMemoryCheck(browser_memory_report);
        dnx2GraphicalBrowserCheckStatus(passed?"Executable memory check passed":"Executable memory check failed",1);
        while(appletMainLoop()&&!dnx2GraphicalAuthCancel())svcSleepThread(16000000LL);
        dnx2GraphicalAuthEnd();
    }
    else if(action==1)hardware_report();
    else if(action==2)run_discord_only();
    else if(action==3)run_voice_only();
    else if(action==4)run_stress_only();
    else if(action==6) {
        diag_len=0;
        diagf("DiscordNX2 v0.36 QR login stage-only diagnostics - no credentials or account data\n");
        dnx2GraphicalAuthBegin();
        int ok=dnx2NativeQrLogin(dnx2AuthProgressAndReport,dnx2GraphicalAuthCancel);
        dnx2GraphicalAuthEnd();
        diagf("auth session complete=%u visible_servers=%d\n",ok,(int)dnx2Account()->server_count);
        (void)write_diagnostics();
    }
    else if(action==5) {
        diag_len=0;
        diagf("DiscordNX2 v0.20 Remote Auth Gateway only - NO ACCOUNT LOGIN\n");
        Dnx2DiscordProbe result=dnx2DiscordRemoteAuthProbe(diagf);
        diagf("remote-auth tcp=%u tls=%u websocket=%u hello=%u interval=%u\n",result.tcp_ok,result.tls_ok,result.websocket_ok,result.hello_ok,result.hello_interval_ms);
        diagf("report_write=%08x\n",write_diagnostics());
        (void)write_diagnostics();
    }
}
static void run_ui(void) {
    /* The framebuffer is released before an audio/network action begins, so
     * the native overlay never races AudioIn/AudioOut service ownership. */
    int action=dnx2GraphicalMenu();
    if(action>=0) {
        while(action>0) {
            run_gui_action(action);
            action=dnx2GraphicalMenu();
        }
        if(action==0)return;
    }
    /* Fallback for systems where the Switch 2 framebuffer service fails. */
    consoleInit(NULL);
    printf("DiscordNX2 could not open the display.\nPlease relaunch the app.\n");
    consoleUpdate(NULL);
    svcSleepThread(2000000000LL);
    consoleExit(NULL);
}
static void run_console_ui(void) {
    Result rc=hidInitialize();
    if(R_FAILED(rc)) {
        diagf("v0.17 UI HID failed=%08x; running entire diagnostic headless\n",rc);
        hardware_report();return;
    }
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    PadState pad={0};
    padInitializeDefault(&pad);
    consoleInit(NULL);
    const char *status="Ready - press A for everything";
    show_main_menu(status);
    unsigned close_requested=0;
    while(!close_requested) {
        padUpdate(&pad);
        u64 down=padGetButtonsDown(&pad);
        if(down & HidNpadButton_Plus){close_requested=1;break;}
        int action=0;
        if(down & HidNpadButton_A)action=1;
        else if(down & HidNpadButton_X)action=2;
        else if(down & HidNpadButton_Y)action=3;
        else if(down & HidNpadButton_L)action=4;
        if(action) {
            consoleClear();
            printf("DiscordNX2 v0.17 - Test running\n\n");
            if(action==1)printf("Full hardware + Discord Gateway suite.\n");
            if(action==2)printf("Public Discord Gateway WebSocket/TLS.\n");
            if(action==3)printf("Microphone recording, tone, replay + packets.\n");
            if(action==4)printf("Bounded ~15.36-second microphone capture.\n");
            printf("\nDo not exit during audio capture.\n\n");
            printf("Writing /kaihen/discordnx2-hardware.txt\n");
            consoleUpdate(NULL);
            if(action==1)hardware_report();
            if(action==2)run_discord_only();
            if(action==3)run_voice_only();
            if(action==4)run_stress_only();
            status="Finished - report saved. Press A to rerun";
            show_main_menu(status);
        }
        svcSleepThread(16000000LL);
    }
    consoleExit(NULL);
    hidExit();
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
    (void)dnx2FontInit();
    run_ui();
    dnx2FontExit();
    return 0;
}
