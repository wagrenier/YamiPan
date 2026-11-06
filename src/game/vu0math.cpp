/* ==========================================================================
 *  game/vu0math.cpp
 *
 *  The game's VU0 macro-mode vector and matrix code, 0x135990..0x1366c0, in
 *  the ROM's order. From rotateMatrixY 0x1360e0 on it is libvu0 itself, in
 *  reverse order (sceVu0RotMatrixY / X / Z, _ecossin, UnitMatrix, CopyMatrix,
 *  TransMatrix, ScaleVector, InterVector, DivVector, InversMatrix, Normalize,
 *  InnerProduct, OuterProduct, MulMatrix, ApplyMatrix), and rotTransPers
 *  0x136090 is sceVu0RotTransPers; before it the code is the game's own, in
 *  the same hand. The names are the port's. vu0math.h says what each does.
 *
 *  The VU answers some things a PC does not: a division by zero is the largest
 *  float of its sign, so a zero vector times it stays zero where a PC makes a
 *  NaN (vuInverse), and a square root takes the absolute value. Only what the
 *  code here meets is carried over. Overflow past the largest float is not.
 *
 *  tools/reverse/vu0cmp.py runs the ROM's code beside this file.
 * ======================================================================== */
#include "game/vu0math.h"

#include <float.h>

namespace {

constexpr float kPi = 3.14159274f;         // 0x40490fdb
constexpr float kHalfPi = 1.57079637f;     // 0x3fc90fdb
constexpr float kTwoPi = 6.28318548f;      // 0x40c90fdb
constexpr float kOverTwoPi = 0.159154937f; // 0x3e22f983

// The Taylor series of the sine: x^3, x^5, x^7, x^9. data 0x300e60, and the same
// four again at 0x300e50 for sinCosPair.
constexpr float kSineSeries[4] = { -0.1666666716f, 0.008333333768f, -0.0001984127011f, 2.755731884e-06f };

// libvu0's fitted sine, by the VU's lanes x..w: x^9, x^7, x^5, x^3. data 0x2f3930
constexpr float kFittedSine[4] = { 2.601886990e-06f, -0.0001980741363f, 0.008333025500f, -0.1666665673f };

// 1 / d, as the VU's div.
inline float vuInverse(float d)
{
    return d != 0.0f ? 1.0f / d : copysignf(FLT_MAX, d);
}

// 1 / sqrt(square), as the VU's rsqrt: one rounding, and a zero is the largest float.
inline float vuInverseRoot(float square)
{
    return square != 0.0f ? static_cast<float>(1.0 / sqrt(static_cast<double>(square))) : FLT_MAX;
}

// ftoi0 and cvt.w.s: towards zero, held at the ends of an int.
inline int vuToInt(float value)
{
    if (value >= 2147483648.0f)
        return 0x7fffffff;
    if (value <= -2147483648.0f)
        return static_cast<int>(0x80000000u);
    return static_cast<int>(value);
}

// The series on an angle within -pi/2..pi/2; the powers multiplied as the ROM does.
float sineSeries(float x)
{
    const float x2 = x * x;
    const float x3 = x2 * x;
    const float x4 = x2 * x2;
    const float x5 = x2 * x3;
    const float x7 = x4 * x3;
    const float x9 = x5 * x4;
    float sum = x + x3 * kSineSeries[0];
    sum += x5 * kSineSeries[1];
    sum += x7 * kSineSeries[2];
    return sum + x9 * kSineSeries[3];
}

// An angle of -pi..pi to the one within -pi/2..pi/2 with the same sine. Past
// that range the series runs on what it gets: the callers wrap first.
inline float foldAngle(float angle)
{
    if (fabsf(angle) <= kHalfPi)
        return angle;
    const float below = -kPi - angle;
    return angle < 0.0f ? below : below + kTwoPi;      // the ROM's pi - angle
}

// sine = the sine of a turn, cosine its cosine, from the turn's complement: the
// series gives the cosine, 1 - c^2 the size of the sine and negative its sign,
// so a turn past -pi..pi comes out mirrored. libvu0's _ecossin. It answers in
// vf4 (sine, cosine, 0, 0) and leaves vf5 zero, which the rotations build on.
// FS2 0x00136380
void ecossin(float complement, bool negative, float &sine, float &cosine)
{
    const float x = complement;
    const float x2 = x * x;
    float sum = x + kFittedSine[3] * x * x2;
    sum += kFittedSine[2] * x * x2 * x2;
    sum += kFittedSine[1] * x * x2 * x2 * x2;
    sum += kFittedSine[0] * x * x2 * x2 * x2 * x2;
    cosine = sum;
    const float size = sqrtf(fabsf(1.0f - sum * sum));
    sine = negative ? 0.0f - size : 0.0f + size;
}

// The first lines of the three rotations.
inline void turnSinCos(float angle, float &sine, float &cosine)
{
    const bool negative = angle < 0.0f;
    ecossin(negative ? kHalfPi + angle : kHalfPi - angle, negative, sine, cosine);
}

} // namespace

// FS2 0x00135990
// callcheck: rotationMatrixXyzRegs
void rotateByAngles(sceVu0FMATRIX out, const sceVu0FVECTOR *in, const sceVu0FVECTOR angles)
{
    sceVu0FMATRIX turn;
    rotationMatrixXyzRegs(turn, angles);
    for (int row = 0; row < 4; row++) {
        const float x = in[row][0], y = in[row][1], z = in[row][2], w = in[row][3];
        for (int lane = 0; lane < 4; lane++)
            out[row][lane] = turn[0][lane] * x + turn[1][lane] * y + turn[2][lane] * z + turn[3][lane] * w;
    }
}

// FS2 0x001359f0
// callcheck: sinCosVec
void rotationMatrixZxy(sceVu0FMATRIX out, const sceVu0FVECTOR rotation)
{
    float s[3], c[3];
    sinCosVec(s, c, rotation);
    out[0][3] = 0.0f;
    const float sysz = s[1] * s[2];
    const float sycz = s[1] * c[2];
    const float cysz = c[1] * s[2];
    const float cycz = c[1] * c[2];
    const float minusCx = -c[0];
    out[0][0] = cycz - s[0] * sysz;
    out[0][1] = cysz + s[0] * sycz;
    out[1][3] = 0.0f;
    out[2][0] = sycz + s[0] * cysz;
    out[2][1] = sysz - s[0] * cycz;
    out[2][3] = 0.0f;
    out[0][2] = minusCx * s[1];
    out[1][0] = minusCx * s[2];
    out[1][1] = c[0] * c[2];
    out[2][2] = c[0] * c[1];
    out[1][2] = s[0];
    out[3][0] = out[3][1] = out[3][2] = 0.0f;
    out[3][3] = 1.0f;
}

// The matrix of rotationMatrixXyz, left in vf4..vf7 for rotateByAngles. Here
// it comes back in a matrix.
// FS2 0x00135a90
// callcheck: sinCosVec
void rotationMatrixXyzRegs(sceVu0FMATRIX out, const sceVu0FVECTOR rotation)
{
    rotationMatrixXyz(out, rotation);
}

// FS2 0x00135b90
// callcheck: sinCosVec
void rotationMatrixXyz(sceVu0FMATRIX out, const sceVu0FVECTOR rotation)
{
    float s[3], c[3];
    sinCosVec(s, c, rotation);
    out[0][3] = 0.0f;
    const float sxsz = s[0] * s[2];
    const float sxcz = s[0] * c[2];
    const float cxsz = c[0] * s[2];
    const float cxcz = c[0] * c[2];
    const float minusCy = -c[1];
    out[0][1] = cxsz + s[1] * sxcz;
    out[0][2] = sxsz - s[1] * cxcz;
    out[1][3] = 0.0f;
    out[1][1] = cxcz - s[1] * sxsz;
    out[1][2] = sxcz + s[1] * cxsz;
    out[2][3] = 0.0f;
    out[0][0] = c[1] * c[2];
    out[1][0] = minusCy * s[2];
    out[2][1] = minusCy * s[0];
    out[2][2] = c[1] * c[0];
    out[2][0] = s[1];
    out[3][0] = out[3][1] = out[3][2] = 0.0f;
    out[3][3] = 1.0f;
}

// FS2 0x00135c30
void sinCosVec(float *sines, float *cosines, const sceVu0FVECTOR angles)
{
    for (int lane = 0; lane < 3; lane++) {
        sines[lane] = sineSeries(foldAngle(angles[lane]));
        cosines[lane] = sineSeries(kHalfPi - fabsf(angles[lane]));
    }
}

// FS2 0x00135d50
void sinCosPair(float angle, float *out)
{
    const float sine = sineSeries(foldAngle(angle));
    const float cosine = sineSeries(kHalfPi - fabsf(angle));
    out[0] = sine;
    out[1] = cosine;
}

// FS2 0x00135e20
void wrapAngleVec(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    bool outside = false;
    for (int lane = 0; lane < 4; lane++)
        outside = outside || in[lane] + kPi < 0.0f || kPi - in[lane] < 0.0f;
    for (int lane = 0; lane < 4; lane++)
        out[lane] = outside ? wrapAngleLane(in[lane]) : in[lane];
}

// FS2 0x00135ec0
float wrapAngleLane(float angle)
{
    const float turns = angle * kOverTwoPi;
    const float whole = static_cast<float>(vuToInt(turns));
    const float carry = static_cast<float>(vuToInt((1.5f + turns) - whole));
    return ((angle + kTwoPi) - whole * kTwoPi) - carry * kTwoPi;
}

// FS2 0x00135f20
float vecDistance(const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// FS2 0x00135f70
float vecDistanceSq(const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

// FS2 0x00135fa0
void addScaledVec4(float s, sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int lane = 0; lane < 4; lane++)
        out[lane] = a[lane] + b[lane] * s;
}

// FS2 0x00135fc0
void addScaledVec3(float s, sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    const float w = b[3];
    for (int lane = 0; lane < 3; lane++)
        out[lane] = a[lane] + b[lane] * s;
    out[3] = w;
}

// FS2 0x00135fe0
void setVecLength(float len, sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    const float scale = vuInverseRoot(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);
    const float w = in[3];
    for (int lane = 0; lane < 3; lane++)
        out[lane] = in[lane] * len * scale;
    out[3] = w;
}

// FS2 0x00136020
void vecDirection(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    const float scale = vuInverse(sqrtf(dx * dx + dy * dy + dz * dz));
    out[0] = dx * scale;
    out[1] = dy * scale;
    out[2] = dz * scale;
    out[3] = 0.0f;
}

// FS2 0x00136070
void scaleVec3(float s, sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    const float w = in[3];
    for (int lane = 0; lane < 3; lane++)
        out[lane] = in[lane] * s;
    out[3] = w;
}

// FS2 0x00136090
void rotTransPers(sceVu0IVECTOR out, const sceVu0FVECTOR *m, const sceVu0FVECTOR v, int mode)
{
    sceVu0FVECTOR point;
    applyMatrix(point, m, v);
    const float scale = vuInverse(point[3]);
    constexpr float kFixed4 = 16.0f;    // ftoi4
    const float x = point[0] * scale;
    const float y = point[1] * scale;
    const float z = point[2] * scale;
    out[0] = vuToInt(x * kFixed4);
    out[1] = vuToInt(y * kFixed4);
    out[2] = vuToInt(mode != 0 ? z : z * kFixed4);
    out[3] = vuToInt(mode != 0 ? point[3] : point[3] * kFixed4);
}

// FS2 0x001360e0
// callcheck: ecossin
void rotateMatrixY(float angle, sceVu0FMATRIX out, sceVu0FMATRIX in)
{
    float s, c;
    turnSinCos(angle, s, c);
    for (int row = 0; row < 4; row++) {
        const float x = in[row][0];
        const float z = in[row][2];
        out[row][0] = c * x + s * z;
        out[row][1] = in[row][1];
        out[row][2] = c * z - s * x;
        out[row][3] = in[row][3];
    }
}

// FS2 0x001361c0
// callcheck: ecossin
void rotateMatrixX(float angle, sceVu0FMATRIX out, sceVu0FMATRIX in)
{
    float s, c;
    turnSinCos(angle, s, c);
    for (int row = 0; row < 4; row++) {
        const float y = in[row][1];
        const float z = in[row][2];
        out[row][0] = in[row][0];
        out[row][1] = c * y - s * z;
        out[row][2] = s * y + c * z;
        out[row][3] = in[row][3];
    }
}

// FS2 0x001362a0
// callcheck: ecossin
void rotateMatrixZ(float angle, sceVu0FMATRIX out, sceVu0FMATRIX in)
{
    float s, c;
    turnSinCos(angle, s, c);
    for (int row = 0; row < 4; row++) {
        const float x = in[row][0];
        const float y = in[row][1];
        out[row][0] = c * x - s * y;
        out[row][1] = s * x + c * y;
        out[row][2] = in[row][2];
        out[row][3] = in[row][3];
    }
}

// FS2 0x00136400
void identityMatrix(sceVu0FMATRIX out)
{
    memset(out, 0, sizeof(sceVu0FMATRIX));
    out[0][0] = out[1][1] = out[2][2] = out[3][3] = 1.0f;
}

// FS2 0x00136430
void copyMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *m)
{
    memmove(out, m, sizeof(sceVu0FMATRIX));
}

// FS2 0x00136460
void translateMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *m, const sceVu0FVECTOR v)
{
    const float x = m[3][0] + v[0];
    const float y = m[3][1] + v[1];
    const float z = m[3][2] + v[2];
    const float w = m[3][3];
    memmove(out, m, 3 * sizeof(sceVu0FVECTOR));
    out[3][0] = x;
    out[3][1] = y;
    out[3][2] = z;
    out[3][3] = w;
}

// FS2 0x00136490
void scaleVec4(float s, sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    for (int lane = 0; lane < 4; lane++)
        out[lane] = in[lane] * s;
}

// FS2 0x001364b0
void blendVec4(float t, sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int lane = 0; lane < 4; lane++)
        out[lane] = (b[lane] + a[lane] * t) - b[lane] * t;
}

// FS2 0x001364e0
void divideVec4(float s, sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    scaleVec4(vuInverse(s), out, in);
}

// FS2 0x00136500
void invertRigidMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *m)
{
    const float tx = 0.0f - m[3][0];
    const float ty = 0.0f - m[3][1];
    const float tz = 0.0f - m[3][2];
    const float w = m[3][3];
    sceVu0FMATRIX turn;
    for (int row = 0; row < 3; row++) {
        for (int lane = 0; lane < 3; lane++)
            turn[row][lane] = m[lane][row];
        turn[row][3] = 0.0f;
    }
    for (int lane = 0; lane < 3; lane++)
        turn[3][lane] = turn[0][lane] * tx + turn[1][lane] * ty + turn[2][lane] * tz;
    turn[3][3] = w;
    memcpy(out, turn, sizeof(sceVu0FMATRIX));
}

// FS2 0x00136560
void vecNormalize(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    const float scale = vuInverse(sqrtf(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]));
    out[0] = in[0] * scale;
    out[1] = in[1] * scale;
    out[2] = in[2] * scale;
    out[3] = 0.0f;
}

// FS2 0x001365a0
float vecDot(const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    return a[1] * b[1] + a[0] * b[0] + a[2] * b[2];
}

// FS2 0x001365d0
void vecCross(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    const float x = a[1] * b[2] - b[1] * a[2];
    const float y = a[2] * b[0] - b[2] * a[0];
    const float z = a[0] * b[1] - b[0] * a[1];
    out[0] = x;
    out[1] = y;
    out[2] = z;
    out[3] = 0.0f;
}

// FS2 0x001365f0
void multiplyTurn(sceVu0FMATRIX out, const sceVu0FVECTOR *a, const sceVu0FVECTOR *b)
{
    sceVu0FMATRIX result;
    for (int row = 0; row < 3; row++) {
        for (int lane = 0; lane < 4; lane++)
            result[row][lane] = a[0][lane] * b[row][0] + a[1][lane] * b[row][1] + a[2][lane] * b[row][2];
    }
    memcpy(out, result, 3 * sizeof(sceVu0FVECTOR));
}

// FS2 0x00136640
void multiplyMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR *a, const sceVu0FVECTOR *b)
{
    sceVu0FMATRIX result;
    for (int row = 0; row < 4; row++) {
        for (int lane = 0; lane < 4; lane++)
            result[row][lane] = a[0][lane] * b[row][0] + a[1][lane] * b[row][1] + a[2][lane] * b[row][2]
                              + a[3][lane] * b[row][3];
    }
    memcpy(out, result, sizeof(sceVu0FMATRIX));
}

// FS2 0x00136690
void applyMatrix(sceVu0FVECTOR out, const sceVu0FVECTOR *m, const sceVu0FVECTOR v)
{
    const float x = v[0], y = v[1], z = v[2], w = v[3];
    sceVu0FVECTOR result;
    for (int lane = 0; lane < 4; lane++)
        result[lane] = m[0][lane] * x + m[1][lane] * y + m[2][lane] * z + m[3][lane] * w;
    memcpy(out, result, sizeof(sceVu0FVECTOR));
}
