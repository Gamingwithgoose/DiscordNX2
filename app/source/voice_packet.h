// SPDX-License-Identifier: MIT
// DiscordNX2 experimental LAN diagnostic framing, NOT Discord RTP or encryption.
#ifndef DNX2_VOICE_PACKET_H
#define DNX2_VOICE_PACKET_H
#include <stdint.h>
#include <stddef.h>
#define DNX2_FRAME_FRAMES 480u
#define DNX2_PACKET_HEADER 24u
#define DNX2_PACKET_PCM_BYTES (DNX2_FRAME_FRAMES * 2u)
#define DNX2_PACKET_BYTES (DNX2_PACKET_HEADER + DNX2_PACKET_PCM_BYTES)
#define DNX2_PACKET_VERSION 1u
#define DNX2_PACKET_ACTIVE 1u
typedef struct {
    uint32_t sequence;
    uint32_t timestamp;
    uint16_t rms;
    uint16_t peak;
    uint8_t active;
} Dnx2PacketInfo;
typedef struct {
    unsigned hangover;
    uint16_t threshold_rms;
} Dnx2VoiceGate;
// Input: 480 interleaved stereo PCM16 frames. Output: 984 byte little-endian
// mono PCM datagram. Checksum is non-cryptographic; this is NOT network secure.
size_t dnx2PacketEncode(const int16_t *stereo, uint32_t sequence,
                        Dnx2VoiceGate *gate, uint8_t *out, size_t cap,
                        Dnx2PacketInfo *info);
int dnx2PacketDecode(const uint8_t *packet, size_t len,
                     int16_t *mono, size_t samples,
                     Dnx2PacketInfo *info);
#endif
