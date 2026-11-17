// SPDX-License-Identifier: MIT
#include "voice_packet.h"
static void put16(uint8_t *p, uint16_t n) {
    p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;
}
static void put32(uint8_t *p, uint32_t n) {
    p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);
    p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;
}
static uint16_t get16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);
}
static uint32_t get32(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
           ((uint32_t)p[2]<<8)|p[3];
}
static uint32_t checksum(const uint8_t *p, size_t n) {
    uint32_t h=2166136261u;
    for(size_t i=0;i<n;i++){h^=p[i];h*=16777619u;}
    return h;
}
static uint16_t integer_sqrt(uint32_t n) {
    uint32_t root=0,bit=1u<<30;
    while(bit>n)bit>>=2;
    while(bit) {
        if(n>=root+bit){n-=root+bit;root=(root>>1)+bit;}
        else root>>=1;
        bit>>=2;
    }
    return (uint16_t)root;
}
size_t dnx2PacketEncode(const int16_t *stereo, uint32_t sequence,
                        Dnx2VoiceGate *gate, uint8_t *out, size_t cap,
                        Dnx2PacketInfo *info) {
    if(!stereo||!out||!gate||cap<DNX2_PACKET_BYTES)return 0;
    uint64_t energy=0;
    unsigned peak=0;
    out[0]='D';out[1]='N';out[2]='X';out[3]='2';
    out[4]=DNX2_PACKET_VERSION;out[5]=0;
    put16(out+6,DNX2_PACKET_PCM_BYTES);
    put32(out+8,sequence);
    put32(out+12,sequence*DNX2_FRAME_FRAMES);
    for(unsigned i=0;i<DNX2_FRAME_FRAMES;i++) {
        int mixed=((int)stereo[2*i]+(int)stereo[2*i+1])/2;
        int magnitude=mixed<0?-mixed:mixed;
        if((unsigned)magnitude>peak)peak=(unsigned)magnitude;
        energy+=(uint64_t)((int64_t)mixed*(int64_t)mixed);
        uint16_t u=(uint16_t)(int16_t)mixed;
        out[DNX2_PACKET_HEADER+i*2]=(uint8_t)u;
        out[DNX2_PACKET_HEADER+i*2+1]=(uint8_t)(u>>8);
    }
    uint32_t mean=(uint32_t)(energy/DNX2_FRAME_FRAMES);
    unsigned rms=integer_sqrt(mean);
    unsigned active=0;
    if(rms>=gate->threshold_rms) {
        gate->hangover=12;active=1;
    } else if(gate->hangover) {
        gate->hangover--;active=1;
    }
    if(active)out[5]=DNX2_PACKET_ACTIVE;
    put16(out+16,(uint16_t)rms);
    put16(out+18,(uint16_t)peak);
    put32(out+20,checksum(out+DNX2_PACKET_HEADER,DNX2_PACKET_PCM_BYTES));
    if(info) {
        info->sequence=sequence;info->timestamp=sequence*DNX2_FRAME_FRAMES;
        info->rms=(uint16_t)rms;info->peak=(uint16_t)peak;
        info->active=(uint8_t)active;
    }
    return DNX2_PACKET_BYTES;
}
int dnx2PacketDecode(const uint8_t *packet, size_t len,
                     int16_t *mono, size_t samples,
                     Dnx2PacketInfo *info) {
    if(!packet||len!=DNX2_PACKET_BYTES||!mono||samples<DNX2_FRAME_FRAMES)return 0;
    if(packet[0]!='D'||packet[1]!='N'||packet[2]!='X'||packet[3]!='2'||
       packet[4]!=DNX2_PACKET_VERSION||get16(packet+6)!=DNX2_PACKET_PCM_BYTES||
       (packet[5]&~DNX2_PACKET_ACTIVE))return 0;
    if(get32(packet+20)!=checksum(packet+DNX2_PACKET_HEADER,DNX2_PACKET_PCM_BYTES))return 0;
    for(unsigned i=0;i<DNX2_FRAME_FRAMES;i++) {
        uint16_t sample=(uint16_t)packet[DNX2_PACKET_HEADER+i*2]|
                        ((uint16_t)packet[DNX2_PACKET_HEADER+i*2+1]<<8);
        mono[i]=(int16_t)sample;
    }
    if(info) {
        info->sequence=get32(packet+8);info->timestamp=get32(packet+12);
        info->rms=get16(packet+16);info->peak=get16(packet+18);
        info->active=packet[5]&DNX2_PACKET_ACTIVE;
    }
    return 1;
}
