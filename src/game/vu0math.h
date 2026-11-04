/* ==========================================================================
 *  game/vu0math.h
 *
 *  The game's VU0 macro-mode vector helpers, 0x135990..0x1366c0 (the run
 *  unassigned@00135990 right before klib.s; not cut into a file yet, so
 *  the file name is provisional). Only what other files call; the
 *  rest (the sin / cos table 0x135d50, the third rotation order 0x135a90,
 *  ...) waits for that TU.
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
// FS2 0x00136500
inline void invertRigidMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *m)
{
    sceVu0InversMatrix(out, reinterpret_cast<float (*)[4]>(const_cast<sceVu0FVECTOR *>(m)));
}
// out = the inverse of any affine matrix (the upper 3x3 by cofactors): rows
// 0..2 w 0, row 3 keeps m's w. Inlined by the ROM (ModelInstance::init, twice).
inline void invertAffineMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *m)
{
    const float a[3] = { m[0][0], m[1][0], m[2][0] };
    const float b[3] = { m[0][1], m[1][1], m[2][1] };
    const float c[3] = { m[0][2], m[1][2], m[2][2] };
    float inverse[3][3] = {
        { b[1] * c[2] - b[2] * c[1], b[2] * c[0] - b[0] * c[2], b[0] * c[1] - b[1] * c[0] },
        { c[1] * a[2] - c[2] * a[1], c[2] * a[0] - c[0] * a[2], c[0] * a[1] - c[1] * a[0] },
        { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] },
    };
    const float scale = 1.0f / (inverse[0][0] * a[0] + inverse[0][1] * a[1] + inverse[0][2] * a[2]);
    const float t[3] = { -m[3][0], -m[3][1], -m[3][2] };
    const float w = m[3][3];
    for (int row = 0; row < 3; row++) {
        for (int lane = 0; lane < 3; lane++)
            out[row][lane] = inverse[row][lane] * scale;
        out[row][3] = 0.0f;
    }
    for (int lane = 0; lane < 3; lane++)
        out[3][lane] = out[0][lane] * t[0] + out[1][lane] * t[1] + out[2][lane] * t[2];
    out[3][3] = w;
}
// out = m[0] v.x + m[1] v.y + m[2] v.z + m[3] v.w; out may be v.
// FS2 0x00136690
inline void applyMatrix(sceVu0FVECTOR out, const sceVu0FVECTOR *m, const sceVu0FVECTOR v)
{
    float x = v[0], y = v[1], z = v[2], w = v[3];
    for (int lane = 0; lane < 4; lane++)
        out[lane] = m[0][lane] * x + m[1][lane] * y + m[2][lane] * z + m[3][lane] * w;
}
// out's rows = in's rows through a turn around x (sceVu0RotMatrixX's matrix).
// FS2 0x001361c0
inline void rotateMatrixX(float angle, sceVu0FMATRIX out, sceVu0FMATRIX in)
{
    sceVu0RotMatrixX(out, in, angle);
}
// out's rows = in's rows through a turn around z: R[0] = (c, s, 0, 0),
// R[1] = (-s, c, 0, 0).
// FS2 0x001362a0
inline void rotateMatrixZ(float angle, sceVu0FMATRIX out, sceVu0FMATRIX in)
{
    float c = cosf(angle);
    float s = sinf(angle);
    for (int row = 0; row < 4; row++) {
        float x = in[row][0];
        float y = in[row][1];
        out[row][0] = c * x - s * y;
        out[row][1] = s * x + c * y;
        out[row][2] = in[row][2];
        out[row][3] = in[row][3];
    }
}

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

// out = m with v added to its position (row 3, xyz; its w stays). The VU0's
// vadd.xyz. out may be m.
// FS2 0x00136460
inline void translateMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *m, const sceVu0FVECTOR v)
{
    float x = m[3][0] + v[0], y = m[3][1] + v[1], z = m[3][2] + v[2], w = m[3][3];
    for (int row = 0; row < 3; row++)
        for (int lane = 0; lane < 4; lane++)
            out[row][lane] = m[row][lane];
    out[3][0] = x;
    out[3][1] = y;
    out[3][2] = z;
    out[3][3] = w;
}
// out's rows = b's rows through a (each is applyMatrix(a, b[i])), a * b for
// the column vectors applyMatrix takes. out may be a or b.
// FS2 0x00136640
inline void multiplyMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *a, const sceVu0FVECTOR *b)
{
    sceVu0MulMatrix(out, reinterpret_cast<float (*)[4]>(const_cast<sceVu0FVECTOR *>(a)),
                    reinterpret_cast<float (*)[4]>(const_cast<sceVu0FVECTOR *>(b)));
}

// The sines and cosines of the first three lanes. The ROM folds the angle
// into -pi/2..pi/2 and runs one Taylor series on all lanes at once, so it
// counts on -pi..pi; the host's functions agree with it there. It answers in
// f1..f6 (s, c, s, c, s, c), no C signature.
// FS2 0x00135c30
inline void sinCosVec(float *sines, float *cosines, const sceVu0FVECTOR angles)
{
    for (int lane = 0; lane < 3; lane++) {
        sines[lane] = sinf(angles[lane]);
        cosines[lane] = cosf(angles[lane]);
    }
}
// out = the turns of rotation (radians), Rz Rx Ry for applyMatrix's vectors:
// around y first, z last. Row 3 = (0, 0, 0, 1).
// FS2 0x001359f0
inline void rotationMatrixZxy(sceVu0FMATRIX out, const sceVu0FVECTOR rotation)
{
    float s[3], c[3];
    sinCosVec(s, c, rotation);
    out[0][0] = c[1] * c[2] - s[0] * s[1] * s[2];
    out[0][1] = c[1] * s[2] + s[0] * s[1] * c[2];
    out[0][2] = -c[0] * s[1];
    out[0][3] = 0.0f;
    out[1][0] = -c[0] * s[2];
    out[1][1] = c[0] * c[2];
    out[1][2] = s[0];
    out[1][3] = 0.0f;
    out[2][0] = s[1] * c[2] + s[0] * c[1] * s[2];
    out[2][1] = s[1] * s[2] - s[0] * c[1] * c[2];
    out[2][2] = c[0] * c[1];
    out[2][3] = 0.0f;
    out[3][0] = out[3][1] = out[3][2] = 0.0f;
    out[3][3] = 1.0f;
}
// The same in the order Rx Ry Rz: around z first, x last.
// FS2 0x00135b90
inline void rotationMatrixXyz(sceVu0FMATRIX out, const sceVu0FVECTOR rotation)
{
    float s[3], c[3];
    sinCosVec(s, c, rotation);
    out[0][0] = c[1] * c[2];
    out[0][1] = c[0] * s[2] + s[0] * s[1] * c[2];
    out[0][2] = s[0] * s[2] - c[0] * s[1] * c[2];
    out[0][3] = 0.0f;
    out[1][0] = -c[1] * s[2];
    out[1][1] = c[0] * c[2] - s[0] * s[1] * s[2];
    out[1][2] = s[0] * c[2] + c[0] * s[1] * s[2];
    out[1][3] = 0.0f;
    out[2][0] = s[1];
    out[2][1] = -c[1] * s[0];
    out[2][2] = c[1] * c[0];
    out[2][3] = 0.0f;
    out[3][0] = out[3][1] = out[3][2] = 0.0f;
    out[3][3] = 1.0f;
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
