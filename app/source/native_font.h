// SPDX-License-Identifier: MIT
#ifndef DNX2_NATIVE_FONT_H
#define DNX2_NATIVE_FONT_H
#include <stdint.h>
int dnx2FontInit(void);
void dnx2FontExit(void);
/* UTF-8 byte count fitting the pixel width; never splits a code point. */
int dnx2FontFit(const char *text,int width,int size,int maxchars);
/* Returns 1 if native anti-aliased TrueType rendered; 0 requests bitmap fallback. */
int dnx2FontDraw(uint32_t *pixels,int stride,int width,int height,
                int x,int y,const char *str,uint32_t color,int size,int maxchars);
#endif
