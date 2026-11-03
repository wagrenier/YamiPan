/* ==========================================================================
 *  game/wstring.h
 *
 *  The game's 16-bit strings (game/wstring.cpp, 0x2d4150..0x2d46a0): the
 *  UI text (fMsg messages, widget text) is u_short units ending in 0, and
 *  these are its strlen / strcpy / strcmp / strstr plus the widening copy
 *  from char text. Every one refuses a null or odd (misaligned) pointer:
 *  the answer is then 0 / null, except that a null source leaves the
 *  copies' destination untouched and answers it. The copies answer the end
 *  of what they wrote (the 0 unit), not dst.
 * ======================================================================== */
#ifndef _GAME_WSTRING_H
#define _GAME_WSTRING_H

#include "sdk/scetypes.h"

// 0 / 1 / -1 over the first wstrlen(a) units only: b starting with a
// answers 0. The callers test == 0.
int      wstrcmp(const u_short *a, const u_short *b);                    // FS2 0x002d4150
// The first place in hay that holds needle, null when there is none; an
// empty needle answers hay.
u_short *wstrstr(const u_short *hay, const u_short *needle);             // FS2 0x002d42a0
// Widens a char string into dst; dst may be src itself (copied first).
u_short *wstrFromAscii(u_short *dst, const char *src);                   // FS2 0x002d4440
u_short *wstrcpy(u_short *dst, const u_short *src);                      // FS2 0x002d4510
int      wstrlen(const u_short *s);                                      // FS2 0x002d4650

#endif /* _GAME_WSTRING_H */
