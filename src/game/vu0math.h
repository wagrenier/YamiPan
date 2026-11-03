/* ==========================================================================
 *  game/vu0math.h
 *
 *  The game's VU0 macro-mode vector helpers, 0x135990..0x1366c0 (the run
 *  unassigned@00135990 right before klib.s; not cut into a file yet, so
 *  the file name is provisional). Only what other files call; the
 *  rest (matrix from rotation 0x1359f0 / 0x135b90, sin / cos tables
 *  0x135c30 / 0x135d50, distance 0x135f70, identity / copy
 *  matrix 0x136400 / 0x136430, ...) waits for that TU.
 * ======================================================================== */
#ifndef _GAME_VU0MATH_H
#define _GAME_VU0MATH_H

#include "sdk/libvu0.h"     /* sceVu0FVECTOR */

// The distance between a and b (xyz).
float vecDistance(const sceVu0FVECTOR a, const sceVu0FVECTOR b);                  // FS2 0x00135f20
// out = in resized to length len (xyz; out.w = in.w).
void setVecLength(float len, sceVu0FVECTOR out, const sceVu0FVECTOR in);          // FS2 0x00135fe0
// out = in * s, all four lanes.
void scaleVec4(float s, sceVu0FVECTOR out, const sceVu0FVECTOR in);               // FS2 0x00136490

#endif /* _GAME_VU0MATH_H */
