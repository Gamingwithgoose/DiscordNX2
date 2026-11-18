// SPDX-License-Identifier: MIT
#ifndef DNX2_GRAPHICAL_UI_H
#define DNX2_GRAPHICAL_UI_H
/* Return -1 if graphical services are unavailable, 0 to exit,
 * or an application action (6: QR login, 7: opt-in browser memory check). */
int dnx2GraphicalMenu(void);
void dnx2GraphicalAuthBegin(void);
void dnx2GraphicalAuthStatus(const char *stage,const char *qr_url);
int dnx2GraphicalAuthCancel(void);
void dnx2GraphicalAuthEnd(void);
/* Uses an already-open auth framebuffer, without changing the account notice. */
void dnx2GraphicalBrowserCheckStatus(const char *message, int complete);
#endif
