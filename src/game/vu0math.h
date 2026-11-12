/* ==========================================================================
 *  game/vu0math.h
 *
 *  The game's VU0 macro-mode vector and matrix helpers: game/vu0math.cpp,
 *  0x135990..0x1366c0. From rotateMatrixY on that code is libvu0's; see the
 *  file. Vectors and matrices as the VU0 takes them: a matrix is four rows,
 *  applyMatrix sums them weighted by the vector's lanes, so for the vectors
 *  it takes multiplyMatrix(out, a, b) means b first, then a.
 *
 *  Every function allows out to be one of its inputs. The rotations want
 *  angles in -pi..pi: past it the ROM's series and signs drift, and so do
 *  these (wrapAngleVec first).
 *
 *  Past the math: invertAffineMatrix (inlined by the ROM), boxContains and
 *  BoundingSphere (the run unassigned@00231d40) live here until their TU.
 * ======================================================================== */
#ifndef _GAME_VU0MATH_H
#define _GAME_VU0MATH_H

#include <math.h>
#include <string.h>

#include "sdk/libvu0.h"     /* sceVu0FVECTOR */

// out = R * in, for R the turns of angles (rotationMatrixXyz): in first, then
// the turn. Each of in's rows goes through R.
void rotateByAngles(sceVu0FMATRIX out, const sceVu0FVECTOR *in, const sceVu0FVECTOR angles); // FS2 0x00135990
// out = the turns of rotation (radians), Rz Rx Ry for applyMatrix's vectors:
// around y first, z last. Row 3 = (0, 0, 0, 1).
void rotationMatrixZxy(sceVu0FMATRIX out, const sceVu0FVECTOR rotation);        // FS2 0x001359f0
void rotationMatrixXyzRegs(sceVu0FMATRIX out, const sceVu0FVECTOR rotation);    // FS2 0x00135a90
// The same in the order Rx Ry Rz: around z first, x last.
void rotationMatrixXyz(sceVu0FMATRIX out, const sceVu0FVECTOR rotation);        // FS2 0x00135b90
// The sines and cosines of the first three lanes (the ROM answers in f1..f6: s,
// c, s, c, s, c). One Taylor series on all lanes at once.
void sinCosVec(float *sines, float *cosines, const sceVu0FVECTOR angles);       // FS2 0x00135c30
// The sine and cosine of one angle, at out[0] and out[1]: the same series.
void sinCosPair(float angle, float *out);                                       // FS2 0x00135d50
// All four lanes into -pi..pi, when any lies outside it (the VU's sticky sign
// flag over pi + a and pi - a).
void wrapAngleVec(sceVu0FVECTOR out, const sceVu0FVECTOR in);                   // FS2 0x00135e20
// One angle into -pi..pi.
float wrapAngleLane(float angle);                                               // FS2 0x00135ec0
// The distance between a and b, and its square (xyz).
float vecDistance(const sceVu0FVECTOR a, const sceVu0FVECTOR b);                // FS2 0x00135f20
float vecDistanceSq(const sceVu0FVECTOR a, const sceVu0FVECTOR b);              // FS2 0x00135f70
// out = a + b * s, all four lanes.
void addScaledVec4(float s, sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b); // FS2 0x00135fa0
// The same in xyz; w is b's.
void addScaledVec3(float s, sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b); // FS2 0x00135fc0
// out = in resized to length len (xyz; w stays in's). A zero vector stays zero.
void setVecLength(float len, sceVu0FVECTOR out, const sceVu0FVECTOR in);        // FS2 0x00135fe0
// (a - b) / |a - b| in xyz, w 0; a == b gives zero.
void vecDirection(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b); // FS2 0x00136020
// out = in * s, xyz; w stays in's.
void scaleVec3(float s, sceVu0FVECTOR out, const sceVu0FVECTOR in);             // FS2 0x00136070
// v through m, divided by w, to 12.4 fixed: mode 0 all four lanes, otherwise
// z and w are whole numbers (sceVu0RotTransPers). w itself is not divided.
void rotTransPers(sceVu0IVECTOR out, const sceVu0FVECTOR *m, const sceVu0FVECTOR v, int mode); // FS2 0x00136090
// out's rows = in's rows through a turn: R[0] = (c, 0, -s, 0), R[2] = (s, 0, c, 0).
void rotateMatrixY(float angle, sceVu0FMATRIX out, sceVu0FMATRIX in);           // FS2 0x001360e0
// R[1] = (0, c, s, 0), R[2] = (0, -s, c, 0).
void rotateMatrixX(float angle, sceVu0FMATRIX out, sceVu0FMATRIX in);           // FS2 0x001361c0
// R[0] = (c, s, 0, 0), R[1] = (-s, c, 0, 0).
void rotateMatrixZ(float angle, sceVu0FMATRIX out, sceVu0FMATRIX in);           // FS2 0x001362a0
void identityMatrix(sceVu0FMATRIX out);                                         // FS2 0x00136400
void copyMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *m);                     // FS2 0x00136430
// out = m with v added to its position (row 3, xyz; its w stays).
void translateMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *m, const sceVu0FVECTOR v); // FS2 0x00136460
// out = in * s, all four lanes.
void scaleVec4(float s, sceVu0FVECTOR out, const sceVu0FVECTOR in);             // FS2 0x00136490
// out = a * t + b * (1 - t).
void blendVec4(float t, sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b); // FS2 0x001364b0
// out = in / s, all four lanes.
void divideVec4(float s, sceVu0FVECTOR out, const sceVu0FVECTOR in);            // FS2 0x001364e0
// out = the inverse of a rotation + translation: rows 0..2 transposed with
// w 0, row 3 = -(R^T t) keeping t.w.
void invertRigidMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *m);              // FS2 0x00136500
// out = in at length 1 (xyz), w 0. A zero vector stays zero.
void vecNormalize(sceVu0FVECTOR out, const sceVu0FVECTOR in);                   // FS2 0x00136560
// a . b (xyz).
float vecDot(const sceVu0FVECTOR a, const sceVu0FVECTOR b);                     // FS2 0x001365a0
// out = a x b (xyz). The ROM's w is whatever vf6 held; nothing reads it, here 0.
void vecCross(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b); // FS2 0x001365d0
// out's rows 0..2 = b's through the turn of a; row 3 stays.
void multiplyTurn(sceVu0FMATRIX out, const sceVu0FVECTOR *a, const sceVu0FVECTOR *b); // FS2 0x001365f0
// out's rows = b's rows through a (each is applyMatrix(a, b[i])).
void multiplyMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *a, const sceVu0FVECTOR *b); // FS2 0x00136640
// out = m[0] v.x + m[1] v.y + m[2] v.z + m[3] v.w.
void applyMatrix(sceVu0FVECTOR out, const sceVu0FVECTOR *m, const sceVu0FVECTOR v); // FS2 0x00136690

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

// point lies between the two corners (each lane of xyz; the VU's sign flags).
// FS2 0x00231d40
inline bool boxContains(const sceVu0FVECTOR low, const sceVu0FVECTOR high, const sceVu0FVECTOR point)
{
    for (int lane = 0; lane < 3; lane++) {
        if (point[lane] < low[lane] || high[lane] < point[lane])
            return false;
    }
    return true;
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

    // Sphere and segment (matrix, not null, moves the sphere), the radius
    // grown by margin: the closest point of the segment's line, between the
    // ends only.
    bool hitsSegment(float margin, const sceVu0FVECTOR from, const sceVu0FVECTOR to,
                     const sceVu0FVECTOR *matrix) const;                 // FS2 0x002366c0
    // The same with margin 0, not moved.
    bool hitsSegment0(const sceVu0FVECTOR from, const sceVu0FVECTOR to) const; // FS2 0x00236800
    // Sphere and capsule. The capsule's radius counts at its two ends only:
    // its middle is tested as a line (the ROM's).
    bool hitsCapsule(float capsuleRadius, const sceVu0FVECTOR from, const sceVu0FVECTOR to,
                     const sceVu0FVECTOR *matrix) const;                 // FS2 0x00236310
    // Sphere and sphere (the other's center, radius). offset, when not null,
    // takes this center less the other, and on a hit the distance in w.
    bool hitsSphere(float otherRadius, const sceVu0FVECTOR otherCenter,
                    const sceVu0FVECTOR *matrix, sceVu0FVECTOR offset) const; // FS2 0x00236480
    // The edge a..b comes nearer the center than the radius: offset, when not
    // null, the closest point less the center, w the distance.
    bool edgeTouches(const sceVu0FVECTOR a, const sceVu0FVECTOR b,
                     sceVu0FVECTOR offset) const;                        // FS2 0x00236570
    // The sphere around count points; false none.
    bool fromPoints(int count, const sceVu0FVECTOR *points);             // FS2 0x00236810

    float         radius;       // FS2 +0x00
    float         radiusSq;     // FS2 +0x04
    sceVu0FVECTOR center;       // FS2 +0x10
};
static_assert(sizeof(BoundingSphere) == 0x20, "BoundingSphere: EE 0x20");

#endif /* _GAME_VU0MATH_H */
