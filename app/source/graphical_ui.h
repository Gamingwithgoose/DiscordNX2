// SPDX-License-Identifier: MIT
#ifndef DNX2_GRAPHICAL_UI_H
#define DNX2_GRAPHICAL_UI_H
/* Return -1 if graphical services are unavailable, 0 to exit,
 * or an application action: 6 QR, 7 memory, 8 Wi-Fi browser,
 * 9 channels, 10 messages, 11 send draft, 12 save login, 13 restore login, 14 native keyboard. */
int dnx2GraphicalMenu(void);
const char *dnx2GraphicalDraft(void);
void dnx2GraphicalClearDraft(void);
int dnx2GraphicalPin(const char *title,char pin[9]);
void dnx2GraphicalAuthBegin(void);
void dnx2GraphicalAuthStatus(const char *stage,const char *qr_url);
int dnx2GraphicalAuthCancel(void);
void dnx2GraphicalAuthEnd(void);
/* Uses an already-open auth framebuffer, without changing the account notice. */
void dnx2GraphicalBrowserCheckStatus(const char *message, int complete);
int dnx2GraphicalWifiBrowserStatus(const char *message,int question);
/* 1 = A/yes, -1 = B/no, -2 = input unavailable; wait for held launch/close buttons to release first. */
int dnx2GraphicalBrowserAnswer(void);
#endif
