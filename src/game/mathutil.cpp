// game/mathutil.cpp -- the game's math helpers (game/mathutil.h).
//
// asinAcos, atan2Cw and sinCos are W. J. Cody and W. Waite's single
// precision algorithms ("Software Manual for the Elementary Functions"):
// they split the float into sign, exponent and mantissa by hand, which the
// port keeps (floatBits) so the branches match the ROM's. The ROM's
// polynomials run on the EE's accumulator (adda / madd with a 0.0 term);
// the port writes them as plain expressions.

#include "game/mathutil.h"

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "game/vu0math.h"

// ──────────────────────────────────────────────────────────────────────
// Float fields.

constexpr u_int kFloatSignBit     = 0x80000000;
constexpr u_int kFloatExpMask     = 0x7f800000;
constexpr int   kFloatExpShift    = 23;
constexpr u_int kFloatMantMask    = 0x007fffff;
constexpr u_int kFloatImplicitBit = 0x00800000;
constexpr int   kFloatExpInfNan   = 0xff;
// A float's biased exponent: 0x73 is 2^-12 (below it x + x^3 / 6 == x),
// 0x7d 0.25, 0x7e 0.5, 0x7f 1.0, 0x80 2, 0x81 4, 0x96 the first with no
// fraction bits (2^23).
constexpr int   kFloatExpTiny     = 0x73;
constexpr int   kFloatExpQuarter  = 0x7d;
constexpr int   kFloatExpHalf     = 0x7e;
constexpr int   kFloatExpOne      = 0x7f;
constexpr int   kFloatExpTwo      = 0x80;
constexpr int   kFloatExpFour     = 0x81;
constexpr int   kFloatExpNoFrac   = 0x96;
// A double's: 0x433 the first with no fraction bits (2^52).
constexpr uint64_t kDoubleExpMask = 0x7ff0000000000000ull;
constexpr int   kDoubleExpShift   = 52;
constexpr int   kDoubleExpNoFrac  = 0x433;

static inline u_int floatBits(float f)
{
    u_int bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

static inline float bitsFloat(u_int bits)
{
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

static inline uint64_t doubleBits(double d)
{
    uint64_t bits;
    memcpy(&bits, &d, sizeof(bits));
    return bits;
}

static inline double bitsDouble(uint64_t bits)
{
    double d;
    memcpy(&d, &bits, sizeof(d));
    return d;
}

static inline int floatExp(u_int bits)
{
    return (int)((bits & kFloatExpMask) >> kFloatExpShift);
}

// The low n mantissa bits: the fraction a float with exponent
// kFloatExpNoFrac - n carries (the tables 0x300c30, 25 words, and
// 0x300ca0, 53 double words; the float table's last entry is all ones).
static inline u_int floatFracMask(int n)
{
    return n >= 24 ? 0xffffffffu : (1u << n) - 1;
}

static inline uint64_t doubleFracMask(int n)
{
    return (1ull << n) - 1;
}

// ──────────────────────────────────────────────────────────────────────
// Constants.

// matrixToRot*: how close to +-pi/2 the middle angle may come before the
// other two turn about the same axis.
constexpr float kGimbalEpsilon = 0.0001f;               // 0x38d1b717

constexpr float kColorMax   = 255.0f;
constexpr float kColorMaxGs = 127.5f;                   // 0x80 -> 1.004

// getBounds' starting box: the minimum at FLT_MAX, the maximum at FLT_MIN
// (0x2f3910 / 0x2f3920) -- the smallest positive float, not -FLT_MAX, so a
// point set below 0 on a lane keeps 1.2e-38 as its maximum.
static const float kBoundsMinStart[4] = { FLT_MAX, FLT_MAX, FLT_MAX, 0.0f };
static const float kBoundsMaxStart[4] = { FLT_MIN, FLT_MIN, FLT_MIN, 0.0f };

// asinAcos: the rational approximation P(g) / Q(g) of Cody & Waite
// (0x300b90..0x300b9c) and the offsets a (0x300ba0) / b (0x300ba8),
// indexed by whether the (1 - |x|) / 2 reduction ran.
constexpr float kAsinP0 = 0.933935835f;                 // 0x3f6f166b
constexpr float kAsinP1 = -0.504400557f;                // 0xbf012065
constexpr float kAsinQ0 = 5.60363004f;                  // 0x40b350f0
constexpr float kAsinQ1 = -5.54846723f;                 // 0xc0b18d0b
static const float kAsinA[2] = { 0.0f, kMathPi / 4 };   // 0x300ba0
static const float kAsinB[2] = { kMathHalfPi, kMathPi / 4 }; // 0x300ba8

// atan2Cw: the reduction past 2 - sqrt(3) (0x3e8930a5 is its mantissa
// with exponent 0x7d), its constants sqrt(3) - 1 and sqrt(3), the
// rational approximation (0x300bc0 / 0x300bc8 / 0x300bcc) and the offsets
// per reduction (0x300bb0: none, the sqrt(3) step, 1/f, both).
constexpr u_int kAtanReduceMant = 0x930a5;
constexpr float kSqrt3Minus1    = 0.732050776f;         // 0x3f3b67ae
constexpr float kSqrt3          = 1.73205078f;          // 0x3fddb3d7
constexpr float kAtanP0 = -0.470832527f;                // 0xbef110f6
constexpr float kAtanP1 = -0.0509095825f;               // 0xbd508691
constexpr float kAtanQ0 = 1.41250074f;                  // 0x3fb4ccd3
static const float kAtanOffset[4] = { 0.0f, kMathPi / 6, kMathHalfPi, kMathPi / 3 }; // 0x300bb0
// Exponent differences past which atan2Cw answers pi/2 or 0 outright.
constexpr int kAtanExpDiffHuge = 0x81;
constexpr int kAtanExpDiffTiny = -0x7c;

// sinCos: pi/2 .. 2pi split in a short high part (0x300c10) and the rest
// (0x300c20), for the multiples the first branches subtract.
static const float kSinCosHi[4] = { 1.5703125f, 3.140625f, 4.7109375f, 6.28125f };
static const float kSinCosLo[4] = {
    4.83826792e-4f, 9.67653585e-4f, 1.45148044e-3f, 1.93530717e-3f,
};
constexpr float kInvTwoPi = 0.159154937f;               // 0x3e22f983
constexpr float kTwoOverPi = 0.636619747f;              // 0x3f22f983
// 2pi, 1 / 2pi and 2pi in two parts, in double precision (the huge-x
// reduction).
constexpr uint64_t kTwoPiD     = 0x401921fb54442d18ull;
constexpr uint64_t kInvTwoPiD  = 0x3fc45f306dc9c883ull;
constexpr uint64_t kTwoPiHiD   = 0x401921fb00000000ull;
constexpr uint64_t kTwoPiLoD   = 0x3eb5110b46000000ull;
// The sin and cos polynomials in r^2 (0x300bd0 / 0x300bf0), highest first.
static const float kSinPoly[5] = {
    2.75573188e-6f, -1.98412701e-4f, 8.33333377e-3f, -0.166666672f, 1.0f,
};
static const float kCosPoly[5] = {
    2.48015876e-5f, -1.38888892e-3f, 4.16666679e-2f, -0.5f, 1.0f,
};
// sinCos's exponent thresholds (|x| from 2^12 loses whole turns first:
// in float while x / 2pi is below 2^22, in double below 2^35), and the
// mantissas splitting its first branches at pi/4, 3pi/4, 5pi/4, 7pi/4 and
// 9pi/4.
constexpr int   kSinCosExpHuge    = 0x8b;
constexpr int   kSinCosExpDouble  = 0x95;
constexpr int   kSinCosExpDomain  = 0xa2;
constexpr u_int kMantQuarterPi      = 0x490fdc;     // exponent 0x7e
constexpr u_int kMantThreeQuarterPi = 0x16cbe5;     // exponent 0x80
constexpr u_int kMantFiveQuarterPi  = 0x7b53d2;     // exponent 0x80
constexpr u_int kMantSevenQuarterPi = 0x2fede0;     // exponent 0x81
constexpr u_int kMantNineQuarterPi  = 0x6231d7;     // exponent 0x81

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00133800

void copyVec4(float *dst, const float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

// ──────────────────────────────────────────────────────────────────────
// For m = Rz * Rx * Ry: m[1][2] = sin x, m[0][2] / m[2][2] = -cos x sin y /
// cos x cos y, m[1][0] / m[1][1] = -cos x sin z / cos x cos z. At the pole
// the ROM reads atan2(m[0][1], m[2][2]) for z, with m[2][2] = 0 there (so
// +-pi/2): kept.
// FS2 0x00133830
// callcheck: asinAcos, atan2Cw, atan2Cw, atan2Cw

void matrixToRotYXZ(sceVu0FVECTOR out, const sceVu0FMATRIX m)
{
    out[0] = asinAcos(m[1][2], AsinAcos_Asin);
    if (fabsf(fabsf(out[0]) - kMathHalfPi) <= kGimbalEpsilon) {
        out[1] = 0.0f;
        out[2] = atan2Cw(m[0][1], m[2][2]);
    } else {
        out[1] = atan2Cw(-m[0][2], m[2][2]);
        out[2] = atan2Cw(-m[1][0], m[1][1]);
    }
}

// ──────────────────────────────────────────────────────────────────────
// For m = Rx * Ry * Rz: m[2][0] = sin y, m[2][1] / m[2][2] = -sin x cos y /
// cos x cos y, m[1][0] / m[0][0] = -cos y sin z / cos y cos z. At the pole
// x takes the whole turn (asin(-m[0][1])) and z is 0.
// FS2 0x001338f0
// callcheck: asinAcos, asinAcos, atan2Cw, atan2Cw

void matrixToRotZYX(sceVu0FVECTOR out, const sceVu0FMATRIX m)
{
    out[1] = asinAcos(m[2][0], AsinAcos_Asin);
    if (fabsf(fabsf(out[1]) - kMathHalfPi) <= kGimbalEpsilon) {
        out[0] = asinAcos(-m[0][1], AsinAcos_Asin);
        out[2] = 0.0f;
    } else {
        out[0] = atan2Cw(-m[2][1], m[2][2]);
        out[2] = atan2Cw(-m[1][0], m[0][0]);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001339c0

float normalizeAngle(float angle)
{
    if (angle < -kMathPi)
        angle += kMathTwoPi;
    else if (angle > kMathPi)
        angle -= kMathTwoPi;
    return angle;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00133a30

void colorToFloat(float out[4], const int rgba[4])
{
    out[0] = (float)rgba[0] / kColorMax;
    out[1] = (float)rgba[1] / kColorMax;
    out[2] = (float)rgba[2] / kColorMax;
    out[3] = (float)rgba[3] / kColorMax;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00133a90

void colorToFloatGs(float out[4], const int rgba[4])
{
    out[0] = (float)rgba[0] / kColorMaxGs;
    out[1] = (float)rgba[1] / kColorMaxGs;
    out[2] = (float)rgba[2] / kColorMaxGs;
    out[3] = (float)rgba[3] / kColorMaxGs;
}

// ──────────────────────────────────────────────────────────────────────
// One lane of approachAnglesXZ. Inline in the ROM.

static inline float approachAngle(float rate, float maxStep, float from, float to)
{
    float step = wrapAngle(to - from) * rate;
    if (step > maxStep)
        step = maxStep;
    else if (step < -maxStep)
        step = -maxStep;
    return wrapAngle(from + step);
}

// x and z are angles here (out is then a direction's rotation with no
// y, w 1).
// FS2 0x00133af0
// callcheck: wrapAngle, wrapAngle, wrapAngle, wrapAngle (approachAngle, inline)

void approachAnglesXZ(float rate, float maxStep, sceVu0FVECTOR out,
                      const sceVu0FVECTOR from, const sceVu0FVECTOR to)
{
    out[0] = approachAngle(rate, maxStep, from[0], to[0]);
    out[2] = approachAngle(rate, maxStep, from[2], to[2]);
    out[1] = 0.0f;
    out[3] = 1.0f;
}

// ──────────────────────────────────────────────────────────────────────
// VU0 in the ROM (vsub, vadd: all four lanes).
// FS2 0x00133be0

void lerpVec4(float t, sceVu0FVECTOR out, const sceVu0FVECTOR from, const sceVu0FVECTOR to)
{
    sceVu0FVECTOR d;
    for (int i = 0; i < 4; i++)
        d[i] = to[i] - from[i];
    scaleVec4(t, d, d);
    for (int i = 0; i < 4; i++)
        out[i] = from[i] + d[i];
}

// ──────────────────────────────────────────────────────────────────────
// The step grows with the SQUARED distance (rate * |d|^2, as the ROM
// sums the vmul lanes without a square root), capped at maxStep; there
// is no test against overshooting `to`. VU0 in the ROM (vsub / vmul over
// four lanes, vadd.xyz).
// FS2 0x00133c40

void approachVec3(float rate, float maxStep, sceVu0FVECTOR out,
                  const sceVu0FVECTOR from, const sceVu0FVECTOR to)
{
    sceVu0FVECTOR d, sq;
    for (int i = 0; i < 4; i++)
        d[i] = to[i] - from[i];
    for (int i = 0; i < 4; i++)
        sq[i] = d[i] * d[i];
    for (int i = 0; i < 3; i++) {
        if ((floatBits(sq[i]) & kFloatExpMask) == kFloatExpMask)
            return;
    }
    float step = rate * (sq[2] + (sq[0] + sq[1]));
    if (!(step <= maxStep))
        step = maxStep;
    setVecLength(step, d, d);
    out[0] = from[0] + d[0];
    out[1] = from[1] + d[1];
    out[2] = from[2] + d[2];
    out[3] = from[3];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00133d20

void getBounds(const sceVu0FVECTOR *points, u_int count, sceVu0FVECTOR min, sceVu0FVECTOR max)
{
    float lo[4], hi[4];
    for (int i = 0; i < 4; i++) {
        lo[i] = kBoundsMinStart[i];
        hi[i] = kBoundsMaxStart[i];
    }
    for (u_int n = 0; n < count; n++) {
        for (u_int i = 0; i < 3; i++) {
            if (points[n][i] < lo[i])
                lo[i] = points[n][i];
            if (hi[i] < points[n][i])
                hi[i] = points[n][i];
        }
    }
    for (int i = 0; i < 4; i++) {
        min[i] = lo[i];
        max[i] = hi[i];
    }
}

// ──────────────────────────────────────────────────────────────────────
// Any angle into -pi..pi: fmodf by a turn, then one more turn. Inline in
// the ROM (subAngles, addAngles, addAngle).

static inline float wrapAngleMod(float angle)
{
    if (angle > kMathPi) {
        angle = fmodf(angle, kMathTwoPi);
        if (angle > kMathPi)
            angle = (angle - kMathPi) + -kMathPi;
    } else if (angle < -kMathPi) {
        angle = fmodf(angle, kMathTwoPi);
        if (angle < -kMathPi)
            angle = angle + kMathPi + kMathPi;
    }
    return angle;
}

// FS2 0x00133e30
// callcheck: fmodf, fmodf, fmodf, fmodf, fmodf, fmodf (wrapAngleMod, inline)

void subAngles(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    out[0] = wrapAngleMod(a[0] - b[0]);
    out[1] = wrapAngleMod(a[1] - b[1]);
    out[2] = wrapAngleMod(a[2] - b[2]);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001340f0
// callcheck: fmodf, fmodf, fmodf, fmodf, fmodf, fmodf (wrapAngleMod, inline)

void addAngles(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    out[0] = wrapAngleMod(a[0] + b[0]);
    out[1] = wrapAngleMod(a[1] + b[1]);
    out[2] = wrapAngleMod(a[2] + b[2]);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001343b0
// callcheck: fmodf, fmodf (wrapAngleMod, inline)

float addAngle(float a, float b)
{
    return wrapAngleMod(a + b);
}

// ──────────────────────────────────────────────────────────────────────
// Cody & Waite's R(g) = g P(g) / Q(g) for asin. Inline in the ROM.

static inline float asinRational(float g)
{
    return (g * (kAsinP1 * g + kAsinP0)) / (g * (g + kAsinQ1) + kAsinQ0);
}

// y = |x|; below 0.5 asin y = y + y R(y^2); from 0.5 to 1 it is
// pi/2 - 2 asin(sqrt((1 - y) / 2)), the asin of the reduced argument
// coming out negated (-2 sqrt(g) (1 + R(g))). The tables a / b then add
// pi/4 twice (or pi/2 twice for acos of a negative x) where needed.
// FS2 0x001344a0
// callcheck: __errno

float asinAcos(float x, int mode)
{
    u_int bits = floatBits(x);
    u_int sign = bits & kFloatSignBit;
    int exp = floatExp(bits);
    float y = sign ? -x : x;
    int i;

    if (exp < kFloatExpTiny) {
        i = mode;
    } else if (exp < kFloatExpHalf) {
        i = mode;
        y = y + y * asinRational(y * y);
    } else if (exp < kFloatExpOne || (exp == kFloatExpOne && (bits & kFloatMantMask) == 0)) {
        i = 1 - mode;
        float g = (1.0f - y) / 2.0f;
        float s = -2.0f * sqrtf(g);
        y = s + s * asinRational(g);
    } else {
        errno = EDOM;
        return 0.0f;
    }

    float r;
    if (mode == AsinAcos_Asin) {
        r = (y + kAsinA[i]) + kAsinA[i];
        if (sign)
            r = -r;
    } else if (sign) {
        r = (y + kAsinB[i]) + kAsinB[i];
    } else {
        r = (kAsinA[i] - y) + kAsinA[i];
    }
    return r;
}

// ──────────────────────────────────────────────────────────────────────
// x with a zero exponent (zero or denormal) answers +-pi/2, exponents
// far apart pi/2 or 0; otherwise f = |y / x|, past 1 its inverse (n = 2),
// past 2 - sqrt(3) stepped down by pi/6 (n + 1), then f + f R(f^2) and the
// offset for n; the signs of x and y pick the quadrant last.
// FS2 0x001346a0
// callcheck: __errno

float atan2Cw(float y, float x)
{
    u_int ybits = floatBits(y), xbits = floatBits(x);
    u_int ysign = ybits & kFloatSignBit, xsign = xbits & kFloatSignBit;
    int yexp = floatExp(ybits), xexp = floatExp(xbits);
    float r = 0.0f;
    bool done = false;

    if (xexp == 0) {
        if (yexp == 0) {
            errno = EDOM;
            return 0.0f;
        }
        done = true;
        r = kMathHalfPi;
    }
    if (!done) {
        int diff = yexp - xexp;
        if (diff >= kAtanExpDiffHuge) {
            r = kMathHalfPi;
            done = true;
        } else if (diff < kAtanExpDiffTiny) {
            r = 0.0f;
            done = true;
        }
    }
    if (done) {
        if (xsign)
            r = kMathPi - r;
        if (ysign)
            r = -r;
        return r;
    }

    float f = y / x;
    int exp = floatExp(floatBits(f));
    if (ysign != xsign)
        f = -f;
    int n = 0;
    if (exp >= kFloatExpOne) {
        n = 2;
        f = 1.0f / f;
        exp = floatExp(floatBits(f));
    }
    u_int mant = floatBits(f) & kFloatMantMask;
    if (exp >= kFloatExpHalf || (exp == kFloatExpQuarter && mant >= kAtanReduceMant)) {
        n++;
        f = (f + ((f * kSqrt3Minus1 - 0.5f) - 0.5f)) / (kSqrt3 + f);
        exp = floatExp(floatBits(f));
    }
    if (exp >= kFloatExpTiny) {
        float g = f * f;
        f = f + f * ((g * (kAtanP1 * g + kAtanP0)) / (g + kAtanQ0));
    }
    if (n >= 2)
        f = -f;
    f += kAtanOffset[n];
    if (xsign)
        f = kMathPi - f;
    if (ysign)
        f = -f;
    return f;
}

// ──────────────────────────────────────────────────────────────────────
// Turns (y, x) into the first quadrant (base -pi, -pi/2, 0 or pi/2), then
// into the first octant (pi/2 - atan(x / y) when y > x), so atan2Cw only
// sees 0 <= y <= x.
// FS2 0x00134940
// callcheck: atan2Cw, atan2Cw

float atan2Octant(float y, float x)
{
    float quadrant = 0.0f;
    if (x < 0.0f) {
        if (y < 0.0f) {
            quadrant = -2.0f;
            x = -x;
            y = -y;
        } else {
            quadrant = 1.0f;
            float t = x;
            x = y;
            y = -t;
        }
    } else if (y < 0.0f) {
        quadrant = -1.0f;
        float t = x;
        x = -y;
        y = t;
    }

    float r;
    if (y <= x)
        r = atan2Cw(y, x);
    else
        r = kMathHalfPi - atan2Cw(x, y);
    return r + kMathHalfPi * quadrant;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00134a20

float wrapAngle(float angle)
{
    if (angle < -kMathPi)
        angle += kMathTwoPi;
    else if (angle > kMathPi)
        angle -= kMathTwoPi;
    return angle;
}

// ──────────────────────────────────────────────────────────────────────
// x reduced to r in -pi/4..pi/4 plus q quarter turns: below 9pi/4 by
// fixed multiples of pi/2 (in two parts), else by the nearest multiple
// (k = x 2/pi rounded half away from zero, its integer bits giving q),
// huge x first losing whole turns in double precision. Then with
// q + mode (cos is sin a quarter turn on) the sin or cos polynomial of r,
// negated in the lower half turn.
// FS2 0x00134a90
// callcheck: __extendsfdf2, __extendsfdf2, __muldf3, __subdf3, __truncdfsf2, __extendsfdf2, __muldf3, __extendsfdf2, __muldf3, __subdf3, __muldf3, __subdf3, __truncdfsf2, __errno

float sinCos(float x, int mode)
{
    u_int bits = floatBits(x);
    int exp = floatExp(bits);
    u_int mant = bits & kFloatMantMask;
    int q = 0;
    float r;

    if (exp < kFloatExpTiny)
        return mode != SinCos_Sin ? 1.0f : x;

    if (exp < kFloatExpHalf || (exp == kFloatExpHalf && mant < kMantQuarterPi)) {
        r = x;
    } else {
        bool neg = (bits & kFloatSignBit) != 0;
        if (exp < kFloatExpTwo || (exp == kFloatExpTwo && mant < kMantThreeQuarterPi)) {
            q = neg ? 3 : 1;
            r = neg ? kSinCosLo[0] + (x + kSinCosHi[0]) : (x - kSinCosHi[0]) - kSinCosLo[0];
        } else if (exp == kFloatExpTwo && mant < kMantFiveQuarterPi) {
            q = 2;
            r = neg ? kSinCosLo[1] + (x + kSinCosHi[1]) : (x - kSinCosHi[1]) - kSinCosLo[1];
        } else if (exp < kFloatExpFour || (exp == kFloatExpFour && mant < kMantSevenQuarterPi)) {
            q = neg ? 1 : 3;
            r = neg ? kSinCosLo[2] + (x + kSinCosHi[2]) : (x - kSinCosHi[2]) - kSinCosLo[2];
        } else if (exp == kFloatExpFour && mant < kMantNineQuarterPi) {
            q = 0;
            r = neg ? kSinCosLo[3] + (x + kSinCosHi[3]) : (x - kSinCosHi[3]) - kSinCosLo[3];
        } else {
            if (exp >= kSinCosExpHuge) {
                // Whole turns off first.
                float turns = x * kInvTwoPi;
                u_int tbits = floatBits(turns);
                int texp = floatExp(tbits);
                if (texp < kSinCosExpDouble) {
                    int frac = kFloatExpNoFrac - texp;
                    if (frac > 0)
                        turns = bitsFloat(tbits & ~floatFracMask(frac));
                    x = (float)((double)x - bitsDouble(kTwoPiD) * (double)turns);
                } else if (texp < kSinCosExpDomain) {
                    uint64_t dbits = doubleBits((double)x * bitsDouble(kInvTwoPiD));
                    int frac = kDoubleExpNoFrac - (int)((dbits & kDoubleExpMask) >> kDoubleExpShift);
                    if (frac > 0)
                        dbits &= ~doubleFracMask(frac);
                    double dturns = bitsDouble(dbits);
                    x = (float)(((double)x - bitsDouble(kTwoPiHiD) * dturns)
                                - bitsDouble(kTwoPiLoD) * dturns);
                } else {
                    errno = EDOM;
                    return 0.0f;
                }
            }
            float k = x > 0.0f ? x * kTwoOverPi + 0.5f : x * kTwoOverPi - 0.5f;
            u_int kbits = floatBits(k);
            int frac = kFloatExpNoFrac - floatExp(kbits);
            if (frac > 0) {
                if (frac < 24) {
                    q = (int)(((kbits & kFloatMantMask) | kFloatImplicitBit) >> frac);
                    if (kbits & kFloatSignBit)
                        q = 0x1000000 - q;
                } else {
                    frac = 24;
                }
                k = bitsFloat(kbits & ~floatFracMask(frac));
            }
            r = (x - k * kSinCosHi[0]) - k * kSinCosLo[0];
        }
    }

    q += mode;
    float z = r * r;
    float result;
    if (q & 1) {
        result = (((kCosPoly[0] * z + kCosPoly[1]) * z + kCosPoly[2]) * z + kCosPoly[3]) * z
                 + kCosPoly[4];
    } else {
        result = ((((kSinPoly[0] * z + kSinPoly[1]) * z + kSinPoly[2]) * z + kSinPoly[3]) * z
                  + kSinPoly[4]) * r;
    }
    if (q & 2)
        result = -result;
    return result;
}
