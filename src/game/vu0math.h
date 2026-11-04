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

#include <math.h>
#include <string.h>

#include "sdk/libvu0.h"     /* sceVu0FVECTOR */

// The distance between a and b (xyz).
float vecDistance(const sceVu0FVECTOR a, const sceVu0FVECTOR b);                  // FS2 0x00135f20
// out = in resized to length len (xyz; out.w = in.w).
void setVecLength(float len, sceVu0FVECTOR out, const sceVu0FVECTOR in);          // FS2 0x00135fe0
// out = in * s, all four lanes.
void scaleVec4(float s, sceVu0FVECTOR out, const sceVu0FVECTOR in);               // FS2 0x00136490
// out = the inverse of a rotation + translation: rows 0..2 transposed with
// w 0, row 3 = -(R^T t) keeping t.w.
void invertRigidMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *m);                // FS2 0x00136500
// out = m[0] v.x + m[1] v.y + m[2] v.z + m[3] v.w; out may be v.
void applyMatrix(sceVu0FVECTOR out, const sceVu0FVECTOR *m, const sceVu0FVECTOR v); // FS2 0x00136690

// a . b (xyz).
// FS2 0x001365a0
inline float vecDot(const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    return a[2] * b[2] + a[0] * b[0] + a[1] * b[1];
}
// The squared distance between a and b (xyz).
// FS2 0x00135f70
inline float vecDistanceSq(const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}
// out = in at length 1 (xyz). The ROM leaves out's w to the register's last
// value, so nothing reads it; here it is in's. out may be in.
// FS2 0x00136560
inline void vecNormalize(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    float scale = 1.0f / sqrtf(vecDot(in, in));
    float w = in[3];
    out[0] = in[0] * scale;
    out[1] = in[1] * scale;
    out[2] = in[2] * scale;
    out[3] = w;
}

// A sphere as the collision world keeps it, EE 0x20. The run
// unassigned@00231d40.
struct BoundingSphere
{
    // FS2 0x00236910
    BoundingSphere() : radius(0.0f), radiusSq(0.0f), center{0.0f, 0.0f, 0.0f, 0.0f} {}
    // box[0] = the low corner, box[1] the high one (xyz; w is left alone).
    // FS2 0x002362f0
    void toBox(sceVu0FVECTOR *box) const
    {
        for (int i = 0; i < 3; i++) {
            box[0][i] = center[i] - radius;
            box[1][i] = center[i] + radius;
        }
    }

    float         radius;       // FS2 +0x00
    float         radiusSq;     // FS2 +0x04
    sceVu0FVECTOR center;       // FS2 +0x10
};
static_assert(sizeof(BoundingSphere) == 0x20, "BoundingSphere: EE 0x20");

// out = m.
// FS2 0x00136430
inline void copyMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *m)
{
    memcpy(out, m, sizeof(sceVu0FMATRIX));
}
// out = the identity.
// FS2 0x00136400
inline void identityMatrix(sceVu0FMATRIX out)
{
    memset(out, 0, sizeof(sceVu0FMATRIX));
    out[0][0] = out[1][1] = out[2][2] = out[3][3] = 1.0f;
}

// (a - b) / |a - b|, w too.
// FS2 0x00136020
inline void vecDirection(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    sceVu0FVECTOR diff = { a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3] };
    float scale = 1.0f / sqrtf(diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2]);
    for (int i = 0; i < 4; i++)
        out[i] = diff[i] * scale;
}

#endif /* _GAME_VU0MATH_H */
