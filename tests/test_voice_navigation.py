# SPDX-License-Identifier: MIT
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'app/source/graphical_ui.c').read_text()
a=s.index('static int open_channel(');b=s.index('int dnx2GraphicalMenu(',a)
print('''#include "native_login.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Dnx2Account account;
static int profile_open,voice_panel,voice_action,voice_member_first,channel_cursor,tab,focus,message_expanded,message_offset,follow_latest,sel,loads,moves;
static char keyboard_notice[160],call_channel[24];
const Dnx2Account *dnx2Account(void){return &account;}
static void dnx2GraphicalClearDraft(void){}
static void dnx2JobStop(void){}void dnx2LiveStop(void){}
void dnx2ChannelSelect(int i){account.selected_channel=i;}
static int dnx2JobStart(int op,const char *text,const char *sticker){(void)text;(void)sticker;assert(op==10);loads++;return 1;}
int dnx2VoiceRunning(void){return 1;}
int dnx2VoiceSelected(void){return !strcmp(call_channel,account.channels[account.selected_channel].id);}
int dnx2VoiceStart(void){moves++;strcpy(call_channel,account.channels[account.selected_channel].id);return 1;}
'''+s[a:b]+'''
int main(void){
account.signed_in=1;account.channel_count=3;account.selected_channel=0;account.channels[0].type=2;account.channels[1].type=0;account.channels[2].type=2;
strcpy(account.channels[0].id,"100000000000000001");strcpy(account.channels[1].id,"100000000000000002");strcpy(account.channels[2].id,"100000000000000003");strcpy(call_channel,account.channels[0].id);
open_channel(1,1);assert(loads==1&&!moves&&!strcmp(call_channel,account.channels[0].id));
open_channel(2,0);assert(!moves);/* automatic remembered channel must not move voice */
open_channel(2,1);assert(moves==1&&!strcmp(call_channel,account.channels[2].id));
open_channel(2,1);assert(moves==1);/* selecting same voice does not reconnect */
puts("PASS production channel navigation: text preserves call, remembered voice never moves it, explicit different voice moves once");}
''')
