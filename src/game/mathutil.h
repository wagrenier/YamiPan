/* ==========================================================================
 *  game/mathutil.h
 *
 *  The game's math helpers (game/mathutil.cpp, 0x133800..0x1350d0): vector
 *  copies and colour scaling, Euler angles out of a matrix, angle wrapping
 *  and stepping, and the game's own Cody & Waite asin / acos, atan2 and
 *  sin / cos (single precision, errno EDOM on a bad argument; newlib's libm
 *  at 0x116fb8 is linked too but these are what the game calls).
 *
 *  Vectors are sceVu0FVECTOR (x, y, z, w; 16-aligned), matrices
 *  sceVu0FMATRIX in the SDK's layout: m[i] is the image of basis vector i
 *  (sceVu0ApplyMatrix: v' = m[0] v.x + m[1] v.y + m[2] v.z + m[3] v.w).
 *  Angles are radians.
 * ======================================================================== */
#ifndef _GAME_MATHUTIL_H
#define _GAME_MATHUTIL_H

#include "sdk/libvu0.h"     /* sceVu0FVECTOR / sceVu0FMATRIX */
#include "sdk/scetypes.h"

constexpr float kMathPi     = 3.14159265f;  // 0x40490fdb
constexpr float kMathHalfPi = 1.57079633f;  // 0x3fc90fdb
constexpr float kMathTwoPi  = 6.28318531f;  // 0x40c90fdb

// asinAcos's second argument.
enum AsinAcosMode
{
    AsinAcos_Asin = 0,
    AsinAcos_Acos = 1,
};

// sinCos's second argument: the quarter turns added before the polynomial.
enum SinCosMode
{
    SinCos_Sin = 0,
    SinCos_Cos = 1,
};

// dst = src, four word copies (no 16-byte load: the caller may pass
// unaligned data).
void  copyVec4(float *dst, const float *src);                                     // FS2 0x00133800

// The Euler angles (out x, y, z; w untouched) of a rotation matrix built
// Y first, then X, then Z (Rz * Rx * Ry).
void  matrixToRotYXZ(sceVu0FVECTOR out, const sceVu0FMATRIX m);                   // FS2 0x00133830
// The same for Z first, then Y, then X (Rx * Ry * Rz: sceVu0RotMatrix's
// order), so it inverts sceVu0RotMatrix.
void  matrixToRotZYX(sceVu0FVECTOR out, const sceVu0FMATRIX m);                   // FS2 0x001338f0

// An angle wrapped into -pi..pi by one turn at most.
float normalizeAngle(float angle);                                                // FS2 0x001339c0

// An integer RGBA (0..255 per channel) as floats 0..1.
void  colorToFloat(float out[4], const int rgba[4]);                              // FS2 0x00133a30
// The same on the GS scale: 0x80 is (about) 1, 0xff almost 2.
void  colorToFloatGs(float out[4], const int rgba[4]);                            // FS2 0x00133a90

// Steps x and z of `from` toward `to` (the wrapped difference times
// rate, clamped to +-maxStep), out y = 0, w = 1.
void  approachAnglesXZ(float rate, float maxStep, sceVu0FVECTOR out,
                       const sceVu0FVECTOR from, const sceVu0FVECTOR to);         // FS2 0x00133af0
// out = from + (to - from) * t, all four lanes.
void  lerpVec4(float t, sceVu0FVECTOR out, const sceVu0FVECTOR from,
               const sceVu0FVECTOR to);                                           // FS2 0x00133be0
// Moves from toward to by min(rate * |to - from|^2, maxStep) (xyz; w kept
// from `from`); out is untouched when the squared difference overflows.
void  approachVec3(float rate, float maxStep, sceVu0FVECTOR out,
                   const sceVu0FVECTOR from, const sceVu0FVECTOR to);             // FS2 0x00133c40

// The xyz bounding box of `count` points (w of both 0).
void  getBounds(const sceVu0FVECTOR *points, u_int count, sceVu0FVECTOR min,
                sceVu0FVECTOR max);                                               // FS2 0x00133d20

// Per lane xyz, a - b / a + b wrapped into -pi..pi (fmodf, any size).
void  subAngles(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b); // FS2 0x00133e30
void  addAngles(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b); // FS2 0x001340f0
// a + b wrapped into -pi..pi (fmodf, any size).
float addAngle(float a, float b);                                                 // FS2 0x001343b0

// asin(x) or acos(x) (AsinAcosMode); |x| > 1 sets errno EDOM, answers 0.
float asinAcos(float x, int mode);                                                // FS2 0x001344a0
// atan2(y, x); both with a zero exponent sets errno EDOM, answers 0.
float atan2Cw(float y, float x);                                                  // FS2 0x001346a0
// atan2(y, x) reduced to the first octant before atan2Cw.
float atan2Octant(float y, float x);                                              // FS2 0x00134940
// The same as normalizeAngle (a second copy in the ROM).
float wrapAngle(float angle);                                                     // FS2 0x00134a20
// sin(x) (SinCos_Sin) or cos(x) (SinCos_Cos); |x| / 2pi from 2^35 on
// sets errno EDOM, answers 0.
float sinCos(float x, int mode);                                                  // FS2 0x00134a90

#endif /* _GAME_MATHUTIL_H */
