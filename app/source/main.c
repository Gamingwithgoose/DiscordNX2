// SPDX-License-Identifier: MIT
// Read resident plugin state and test whether basic service sessions can open.
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <switch.h>
#include <switch/services/bsd.h>
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

#define VOICE_CHUNK_BYTES 0x4000u
#define VOICE_CHUNKS 36u
#define VOICE_MAX_BYTES (VOICE_CHUNK_BYTES * VOICE_CHUNKS)
#define VOICE_NETWORK_PORT 39272u
static unsigned char voice_input[VOICE_CHUNK_BYTES] __attribute__((aligned(0x1000)));
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

static void voice_capture_test(void) {
    voice_used=0;
    diagf("\n[VOICE: MULTI-BUFFER MIC CAPTURE / NO NETWORK TRANSMISSION]\n");
    diagf("Speak during capture. Do not use speakers and microphone together for live feedback.\n");
    Result rc=audinInitialize();
    diagf("voice audinInitialize=%08x\n",rc);
    if(R_FAILED(rc))return;
    const u32 rate=audinGetSampleRate(),ch=audinGetChannelCount();
    const unsigned fmt=(unsigned)audinGetPcmFormat();
    diagf("voice input rate=%u channels=%u format=%u\n",rate,ch,fmt);
    if(rate!=48000||ch!=2||fmt!=(unsigned)PcmFormat_Int16) {
        diagf("SKIP voice buffer test: expected 48000Hz, stereo, signed 16-bit PCM.\n");
        audinExit();
        return;
    }
    rc=audinStartAudioIn();
    diagf("voice audinStartAudioIn=%08x\n",rc);
    if(R_FAILED(rc)){audinExit();return;}
    unsigned passed=0,failed=0,quiet=0;
    for(unsigned frame=0;frame<VOICE_CHUNKS;frame++) {
        memset(voice_input,0,sizeof(voice_input));
        AudioInBuffer buffer={0};
        buffer.buffer=voice_input;
        buffer.buffer_size=sizeof(voice_input);
        buffer.data_size=sizeof(voice_input);
        Result ar=audinAppendAudioInBuffer(&buffer);
        if(R_FAILED(ar)){
            diagf("frame=%u append_failed=%08x\n",frame,ar);
            failed++;
            break;
        }
        AudioInBuffer *released=NULL;
        u32 count=0;
        Result wr=audinWaitCaptureFinish(&released,&count,1200000000ULL);
        if(R_FAILED(wr) || count!=1 || released!=&buffer) {
            diagf("frame=%u capture_failed=%08x count=%u matches=%u\n",
                  frame,wr,count,released==&buffer);
            failed++;
            /* Stop capturing and do not reuse the still-owned memory. */
            break;
        }
        const size_t length=(buffer.data_size<=sizeof(voice_input))?
                              (size_t)buffer.data_size:sizeof(voice_input);
        const size_t valid=length & ~(size_t)1;
        voice_stats((const int16_t*)voice_input,valid/2,frame);
        int peak=0;
        for(size_t i=0;i<valid/2;i++) {
            int sample=((const int16_t*)voice_input)[i];
            int magnitude=sample<0?-sample:sample;
            if(magnitude>peak)peak=magnitude;
        }
        if(peak<100)quiet++;
        if(voice_used+valid<=sizeof(voice_output)) {
            /* Preserve unchanged PCM until all chunks are captured.
             * One consistent gain avoids hard clipping at loud syllables.
             */
            memcpy(voice_output+voice_used,voice_input,valid);
            voice_used+=valid;
        }
        passed++;
        if(frame%3==2) {
            diagf("voice checkpoint=%08x\n",write_diagnostics());
        }
    }
    Result stop=audinStopAudioIn();
    diagf("voice audinStopAudioIn=%08x\n",stop);
    audinExit();
    voice_prepare_playback();
    diagf("voice captured_frames=%u errors=%u quiet_frames=%u pcm_bytes=%u adaptive_peak_gain=1.5max fade_ms=10\n",
          passed,failed,quiet,(unsigned)voice_used);
    diagf("voice capture checkpoint=%08x\n",write_diagnostics());
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

/* Queue a zero-PCM tail so the output device receives silence before stop. */
static unsigned char output_silence[0xc000] __attribute__((aligned(0x1000)));
static void queue_silence_tail(AudioOutBuffer *tail) {
    memset(output_silence,0,sizeof(output_silence));
    memset(tail,0,sizeof(*tail));
    tail->buffer=output_silence;
    tail->buffer_size=sizeof(output_silence);
    tail->data_size=sizeof(output_silence);
}
static void drain_playback(AudioOutBuffer *main_buffer, AudioOutBuffer *tail, u64 timeout) {
    bool done_main=false,done_tail=false;
    for(unsigned attempt=0;attempt<4 && (!done_main||!done_tail);attempt++) {
        AudioOutBuffer *released=NULL;
        u32 count=0;
        Result wr=audoutWaitPlayFinish(&released,&count,timeout);
        diagf("drain attempt=%u result=%08x count=%u released_main=%u released_tail=%u\n",
            attempt,wr,count,released==main_buffer,released==tail);
        if(R_FAILED(wr))break;
        bool pending_main=true,pending_tail=true;
        Result mr=audoutContainsAudioOutBuffer(main_buffer,&pending_main);
        Result tr=audoutContainsAudioOutBuffer(tail,&pending_tail);
        if(R_SUCCEEDED(mr)&&!pending_main)done_main=true;
        if(R_SUCCEEDED(tr)&&!pending_tail)done_tail=true;
        if(count && released==main_buffer)done_main=true;
        if(count && released==tail)done_tail=true;
    }
    diagf("drain completed main=%u tail=%u\n",done_main,done_tail);
}

static void voice_reference_tone_test(void) {
    diagf("\n[AUDIO OUTPUT REFERENCE: SYNTHETIC TONE]\n");
    const unsigned sample_frames=24000; /* 0.5 seconds, 48 kHz stereo */
    int16_t *pcm=(int16_t*)reference_tone;
    memset(reference_tone,0,sizeof(reference_tone));
    for(unsigned i=0;i<sample_frames;i++) {
        unsigned phase=i%120;  /* 400 Hz */
        int value=(phase<60)?((int)phase*100-3000):(9000-(int)phase*100);
        unsigned ramp=i;
        if(sample_frames-1-i<ramp)ramp=sample_frames-1-i;
        if(ramp<480)value=(value*(int)ramp)/480;
        pcm[2*i]=(int16_t)value;
        pcm[2*i+1]=(int16_t)value;
    }
    Result rc=audoutInitialize();
    diagf("reference audoutInitialize=%08x\n",rc);
    if(R_FAILED(rc))return;
    if(audoutGetSampleRate()!=48000 || audoutGetChannelCount()!=2 ||
       audoutGetPcmFormat()!=PcmFormat_Int16) {
        diagf("reference skipped: output format unsupported\n");
        audoutExit();
        return;
    }
    rc=audoutStartAudioOut();
    diagf("reference start=%08x\n",rc);
    if(R_SUCCEEDED(rc)) {
        AudioOutBuffer b={0};
        b.buffer=reference_tone;
        b.buffer_size=sizeof(reference_tone);
        b.data_size=sample_frames*2*sizeof(int16_t);
        rc=audoutAppendAudioOutBuffer(&b);
        diagf("reference append=%08x\n",rc);
        if(R_SUCCEEDED(rc)) {
            AudioOutBuffer tail={0};
            queue_silence_tail(&tail);
            Result sr=audoutAppendAudioOutBuffer(&tail);
            diagf("reference silence_append=%08x\n",sr);
            if(R_SUCCEEDED(sr))drain_playback(&b,&tail,2500000000ULL);
            else {
                AudioOutBuffer *released=NULL;u32 n=0;
                diagf("reference fallback_wait=%08x\n",audoutWaitPlayFinish(&released,&n,2500000000ULL));
            }
        }
        svcSleepThread(50000000LL);
        diagf("reference stop=%08x\n",audoutStopAudioOut());
    }
    audoutExit();
    diagf("Listen: did this short quiet tone also pop? Compare to speech playback next.\n");
}

static void voice_playback_test(void) {
    diagf("\n[VOICE: AUDIBLE DELAYED PLAYBACK]\n");
    if(!voice_used) {
        diagf("SKIP: no successful captured audio buffers.\n");
        return;
    }
    Result rc=audoutInitialize();
    diagf("voice audoutInitialize=%08x\n",rc);
    if(R_FAILED(rc))return;
    diagf("voice output rate=%u channels=%u format=%u\n",
          audoutGetSampleRate(),audoutGetChannelCount(),(unsigned)audoutGetPcmFormat());
    if(audoutGetSampleRate()!=48000 || audoutGetChannelCount()!=2 ||
       audoutGetPcmFormat()!=PcmFormat_Int16) {
        diagf("SKIP: output format mismatch.\n");
        audoutExit();
        return;
    }
    rc=audoutStartAudioOut();
    diagf("voice audoutStartAudioOut=%08x\n",rc);
    if(R_SUCCEEDED(rc)) {
        AudioOutBuffer buffer={0};
        buffer.buffer=voice_output;
        buffer.buffer_size=sizeof(voice_output);
        buffer.data_size=voice_used;
        rc=audoutAppendAudioOutBuffer(&buffer);
        diagf("voice audoutAppendAudioOutBuffer=%08x data_bytes=%u\n",rc,(unsigned)voice_used);
        if(R_SUCCEEDED(rc)) {
            AudioOutBuffer tail={0};
            queue_silence_tail(&tail);
            Result sr=audoutAppendAudioOutBuffer(&tail);
            diagf("voice silence_append=%08x\n",sr);
            if(R_SUCCEEDED(sr))drain_playback(&buffer,&tail,7500000000ULL);
            else {
                AudioOutBuffer *released=NULL;u32 count=0;
                diagf("voice fallback_wait=%08x\n",audoutWaitPlayFinish(&released,&count,7500000000ULL));
            }
        }
        svcSleepThread(50000000LL);
        Result stop=audoutStopAudioOut();
        diagf("voice audoutStopAudioOut=%08x\n",stop);
    }
    audoutExit();
    diagf("If you did not hear your voice, report that; a successful API call is not audible playback proof.\n");
}

/* A network transport smoke test that sends only a fixed marker to itself.
 * Never sends microphone data or Discord credentials.
 */
static void voice_network_test(void) {
    diagf("\n[NETWORK: LOCAL UDP LOOPBACK, NO AUDIO SENT]\n");
    Result rc=bsdInitialize(bsdGetDefaultInitConfig(),1,0);
    diagf("bsdInitialize=%08x\n",rc);
    if(R_FAILED(rc))return;
    int server=bsdSocket(AF_INET,SOCK_DGRAM,0);
    int client=bsdSocket(AF_INET,SOCK_DGRAM,0);
    diagf("bsdSocket server=%d client=%d\n",server,client);
    if(server>=0&&client>=0) {
        struct sockaddr_in addr={0};
        addr.sin_family=AF_INET;
        addr.sin_port=htons(VOICE_NETWORK_PORT);
        addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        int br=bsdBind(server,(const struct sockaddr *)&addr,sizeof(addr));
        diagf("bsdBind localhost:%u result=%d errno=%d\n",VOICE_NETWORK_PORT,br,errno);
        if(br==0) {
            static const char msg[]="DNX2_LOCAL_UDP_V07";
            ssize_t sent=bsdSendTo(client,msg,sizeof(msg),0,(const struct sockaddr *)&addr,sizeof(addr));
            diagf("bsdSendTo bytes=%d errno=%d\n",(int)sent,errno);
            fd_set fds;
            FD_ZERO(&fds);FD_SET(server,&fds);
            struct timeval wait={0,400000};
            int ready=bsdSelect(server+1,&fds,NULL,NULL,&wait);
            diagf("bsdSelect ready=%d errno=%d\n",ready,errno);
            if(ready>0&&FD_ISSET(server,&fds)) {
                char received[64]={0};
                ssize_t n=bsdRecv(server,received,sizeof(received),0);
                diagf("bsdRecv bytes=%d marker_match=%u errno=%d\n",
                    (int)n,n==(ssize_t)sizeof(msg)&&memcmp(received,msg,sizeof(msg))==0,errno);
            }
        }
    }
    if(client>=0)bsdClose(client);
    if(server>=0)bsdClose(server);
    bsdExit();
    diagf("Network test is LOCAL ONLY; not a Discord voice connection.\n");
}

static void hardware_report(void) {
    diag_len=0;
    diagf("DiscordNX2 output-tail playback diagnostic v0.12\n");
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
    voice_network_test();
    service_trials();
    plugin_snapshot("after");
    diagf("DONE v0.12. Microphone closed, audio output closed, network sockets closed.\n");
    diagf("final report write=%08x\n",write_diagnostics());
    (void)write_diagnostics();
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
