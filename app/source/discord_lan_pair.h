// SPDX-License-Identifier: MIT
#ifndef DNX2_LAN_PAIR_H
#define DNX2_LAN_PAIR_H
#include <stddef.h>
#define DNX2_MAX_GUILDS 64
typedef struct { char id[24];char name[72];} Dnx2Guild;
typedef struct {
    int paired, count, selected;
    char username[64],display_name[64];
    Dnx2Guild guilds[DNX2_MAX_GUILDS];
} Dnx2PairProfile;
const Dnx2PairProfile *dnx2PairProfile(void);
void dnx2PairSelect(int index);
int dnx2PairReceive(unsigned code,char *message,size_t message_size);
#endif
