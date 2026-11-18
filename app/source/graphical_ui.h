// SPDX-License-Identifier: MIT
#ifndef DNX2_GRAPHICAL_UI_H
#define DNX2_GRAPHICAL_UI_H
/* Return -1 if graphical services are unavailable, 0 to exit,
 * or 1-4 to run a diagnostic action in the main application. */
int dnx2GraphicalMenu(void);
void dnx2GraphicalAuthBegin(void);
void dnx2GraphicalAuthStatus(const char *stage,const char *qr_url);
int dnx2GraphicalAuthCancel(void);
void dnx2GraphicalAuthEnd(void);
#endif
