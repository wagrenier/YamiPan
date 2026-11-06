// sdk/libvu0.h -- the VU0 library's vector and matrix types, 16-aligned as in Sony's header so that a
// member lands at its EE offset, and the few matrix calls game/drawsys.cpp makes (the rest of FS2's VU0
// macro code is scalar C++ in place).
#pragma once

#include <math.h>

#include "scetypes.h"

#if defined(_MSC_VER) && !defined(__clang__)
typedef __declspec(align(16)) float sceVu0FVECTOR[4];
typedef __declspec(align(16)) int sceVu0IVECTOR[4];
typedef __declspec(align(16)) float sceVu0FMATRIX[4][4];
#else
typedef float sceVu0FVECTOR[4] __attribute__((aligned(16)));
typedef int sceVu0IVECTOR[4] __attribute__((aligned(16)));
typedef float sceVu0FMATRIX[4][4] __attribute__((aligned(16)));
#endif

static_assert(sizeof(sceVu0FVECTOR) == 16 && alignof(sceVu0FVECTOR) == 16);
static_assert(sizeof(sceVu0IVECTOR) == 16 && alignof(sceVu0IVECTOR) == 16);
static_assert(sizeof(sceVu0FMATRIX) == 64 && alignof(sceVu0FMATRIX) == 16);

// out = the identity.
inline void sceVu0UnitMatrix(sceVu0FMATRIX m)
{
    for (int row = 0; row < 4; row++)
        for (int column = 0; column < 4; column++)
            m[row][column] = row == column ? 1.0f : 0.0f;
}

// m0's rows = m2's rows through m1: m0[i] = m1[0] m2[i].x + .. + m1[3] m2[i].w. m0 may be m1 or m2.
inline void sceVu0MulMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FMATRIX m2)
{
    sceVu0FMATRIX out;
    for (int row = 0; row < 4; row++)
        for (int column = 0; column < 4; column++)
            out[row][column] = m1[0][column] * m2[row][0] + m1[1][column] * m2[row][1]
                + m1[2][column] * m2[row][2] + m1[3][column] * m2[row][3];
    for (int row = 0; row < 4; row++)
        for (int column = 0; column < 4; column++)
            m0[row][column] = out[row][column];
}

// The inverse of a rotation + translation: rows 0..2 transposed with w 0, row 3 = -(R^T t), its w
// kept. m0 must not be m1.
inline void sceVu0InversMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    for (int row = 0; row < 3; row++) {
        for (int column = 0; column < 3; column++)
            m0[row][column] = m1[column][row];
        m0[row][3] = 0.0f;
    }
    for (int column = 0; column < 3; column++)
        m0[3][column] = -(m0[0][column] * m1[3][0] + m0[1][column] * m1[3][1] + m0[2][column] * m1[3][2]);
    m0[3][3] = m1[3][3];
}

// The library's sine and cosine: a series for the cosine, the sine from it by sqrt(1 - c * c), so a small
// angle's sine comes out a little off (sin 0.047 = 0.0469817, truly 0.0469827). FS2 0x10ba70 and the
// head of its callers; the four terms at data 0x2f1110.
inline void sceVu0SinCos(float angle, float *sine, float *cosine)
{
    const float halfPi = 1.57079637f;   // 0x3fc90fdb
    const float x = angle < 0.0f ? halfPi + angle : halfPi - angle;
    const float xx = x * x;
    float t9 = 2.60188699e-06f * x;     // 0x362e9c14
    float t7 = -0.000198074136f * x;    // 0xb94fb21f
    float t5 = 0.0083330255f * x;       // 0x3c08873e
    float t3 = -0.166666567f * x;       // 0xbe2aaaa4
    t9 *= xx;
    t7 *= xx;
    t5 *= xx;
    t3 *= xx;
    t9 *= xx;
    t7 *= xx;
    t5 *= xx;
    float c = x + t3;
    t9 *= xx;
    t7 *= xx;
    c += t5;
    t9 *= xx;
    c += t7;
    c += t9;
    const float s = sqrtf(fabsf(1.0f - c * c));
    *sine = angle < 0.0f ? 0.0f - s : s;
    *cosine = c;
}

// m0's rows = m1's rows through a turn of rz around z: R[0] = (c, s, 0, 0), R[1] = (-s, c, 0, 0). The
// library's sine and cosine: FS2 0x10bae8.
inline void sceVu0RotMatrixZ(sceVu0FMATRIX m0, sceVu0FMATRIX m1, float rz)
{
    float s, c;
    sceVu0SinCos(rz, &s, &c);
    for (int row = 0; row < 4; row++) {
        float x = m1[row][0];
        float y = m1[row][1];
        m0[row][0] = c * x - s * y;
        m0[row][1] = s * x + c * y;
        m0[row][2] = m1[row][2];
        m0[row][3] = m1[row][3];
    }
}

// m0's rows = m1's rows through a turn of rx around x: R[1] = (0, c, s, 0), R[2] = (0, -s, c, 0).
inline void sceVu0RotMatrixX(sceVu0FMATRIX m0, sceVu0FMATRIX m1, float rx)
{
    float c = cosf(rx);
    float s = sinf(rx);
    for (int row = 0; row < 4; row++) {
        float y = m1[row][1];
        float z = m1[row][2];
        m0[row][0] = m1[row][0];
        m0[row][1] = c * y - s * z;
        m0[row][2] = s * y + c * z;
        m0[row][3] = m1[row][3];
    }
}

// The perspective matrix of the GS: scrz the screen distance, ax / ay the aspect, cx / cy the centre,
// zmin..zmax the Z range for nearz..farz.
inline void sceVu0ViewScreenMatrix(sceVu0FMATRIX m, float scrz, float ax, float ay, float cx, float cy,
                                   float zmin, float zmax, float nearz, float farz)
{
    float cz = (-zmax * nearz + zmin * farz) / (-nearz + farz);
    float az = farz * nearz * (-zmin + zmax) / (-nearz + farz);
    sceVu0FMATRIX persp;
    sceVu0FMATRIX scale;
    sceVu0UnitMatrix(persp);
    persp[0][0] = scrz;
    persp[1][1] = scrz;
    persp[2][2] = 0.0f;
    persp[3][3] = 0.0f;
    persp[3][2] = 1.0f;
    persp[2][3] = 1.0f;
    sceVu0UnitMatrix(scale);
    scale[0][0] = ax;
    scale[1][1] = ay;
    scale[2][2] = az;
    scale[3][0] = cx;
    scale[3][1] = cy;
    scale[3][2] = cz;
    sceVu0MulMatrix(m, scale, persp);
}
