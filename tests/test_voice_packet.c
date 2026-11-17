// SPDX-License-Identifier: MIT
// Portable native unit tests: gcc/clang, no Nintendo toolchain needed.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../app/source/voice_packet.h"
static int16_t input[DNX2_FRAME_FRAMES*2];
static int16_t restored[DNX2_FRAME_FRAMES];
static uint8_t bytes[DNX2_PACKET_BYTES];
static void test_silence(void) {
    memset(input,0,sizeof(input));
    Dnx2VoiceGate gate={.threshold_rms=220};
    Dnx2PacketInfo meta={0},decoded={0};
    assert(dnx2PacketEncode(input,7,&gate,bytes,sizeof(bytes),&meta)==DNX2_PACKET_BYTES);
    assert(dnx2PacketDecode(bytes,sizeof(bytes),restored,DNX2_FRAME_FRAMES,&decoded));
    assert(decoded.sequence==7 && decoded.timestamp==7*DNX2_FRAME_FRAMES);
    assert(decoded.rms==0 && decoded.peak==0 && decoded.active==0);
    for(unsigned i=0;i<DNX2_FRAME_FRAMES;i++)assert(restored[i]==0);
}
static void test_voice_and_gate(void) {
    Dnx2VoiceGate gate={.threshold_rms=220};
    Dnx2PacketInfo meta={0};
    for(unsigned i=0;i<DNX2_FRAME_FRAMES;i++) {
        input[2*i]=1600;
        input[2*i+1]=-800;
    }
    assert(dnx2PacketEncode(input,9,&gate,bytes,sizeof(bytes),&meta)==DNX2_PACKET_BYTES);
    assert(meta.active==1 && meta.peak==400 && meta.rms==400);
    assert(dnx2PacketDecode(bytes,sizeof(bytes),restored,DNX2_FRAME_FRAMES,NULL));
    for(unsigned i=0;i<DNX2_FRAME_FRAMES;i++)assert(restored[i]==400);
    memset(input,0,sizeof(input));
    for(unsigned j=0;j<12;j++) {
        assert(dnx2PacketEncode(input,10+j,&gate,bytes,sizeof(bytes),&meta)==DNX2_PACKET_BYTES);
        assert(meta.active==1);
    }
    assert(dnx2PacketEncode(input,22,&gate,bytes,sizeof(bytes),&meta)==DNX2_PACKET_BYTES);
    assert(meta.active==0);
}
static void test_corruption(void) {
    Dnx2VoiceGate gate={.threshold_rms=220};
    Dnx2PacketInfo info={0};
    assert(dnx2PacketEncode(input,11,&gate,bytes,sizeof(bytes),&info)==DNX2_PACKET_BYTES);
    assert(!dnx2PacketDecode(bytes,sizeof(bytes)-1,restored,DNX2_FRAME_FRAMES,NULL));
    assert(!dnx2PacketDecode(bytes,sizeof(bytes),restored,0,NULL));
    bytes[100]^=1;
    assert(!dnx2PacketDecode(bytes,sizeof(bytes),restored,DNX2_FRAME_FRAMES,NULL));
    bytes[100]^=1;
    assert(dnx2PacketDecode(bytes,sizeof(bytes),restored,DNX2_FRAME_FRAMES,NULL));
    bytes[0]=0;
    assert(!dnx2PacketDecode(bytes,sizeof(bytes),restored,DNX2_FRAME_FRAMES,NULL));
}
static void test_edge_samples(void) {
    Dnx2VoiceGate gate={.threshold_rms=220};
    for(unsigned i=0;i<DNX2_FRAME_FRAMES;i++) {
        input[2*i]=-32768;
        input[2*i+1]=-32768;
    }
    assert(dnx2PacketEncode(input,0,&gate,bytes,sizeof(bytes),NULL)==DNX2_PACKET_BYTES);
    assert(dnx2PacketDecode(bytes,sizeof(bytes),restored,DNX2_FRAME_FRAMES,NULL));
    for(unsigned i=0;i<DNX2_FRAME_FRAMES;i++)assert(restored[i]==-32768);
}
int main(void) {
    test_silence();
    test_voice_and_gate();
    test_corruption();
    test_edge_samples();
    puts("PASS voice_packet: silence, speech, hangover, corruption, edge samples");
    return 0;
}
