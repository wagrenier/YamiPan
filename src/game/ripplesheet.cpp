/* ==========================================================================
 *  game/ripplesheet.cpp
 *
 *  The water over the boss's view on stage 0x1b. ripplesheet.h has the idea.
 * ======================================================================== */
#include "game/ripplesheet.h"

#include <math.h>

#include "game/heap.h"
#include "game/mathutil.h"
#include "yamipan/rendering/yamipan_drawui.h"

namespace {

constexpr float kPi = 3.1415927f;
constexpr float kTwoPi = 6.2831855f;
constexpr float kOverTwoPi = 0.15915494f;
constexpr float kRandRange = 2.1474836e+09f;
constexpr float kSheetSpan = 2.0f;          // a wave's phase runs over the sheet by this, in half turns
constexpr float kFlatPeak = 2.0f;           // RippleSpec::setPeak: under it, one flat push
constexpr float kGsUnits = 16.0f;           // the GS's 1/16 pixel
constexpr float kHeightRange = 120.0f;      // a point's height: 128 give or take this at a full push
constexpr float kHeightLevel = 128.0f;
constexpr int   kSineMode = 0;              // sinCos's mode

float rollBetween(float low, float high)
{
    return low + ((high - low) * (float)(int)randNext()) / kRandRange;
}

// Sums a list at `across` of the way over the sheet, in half turns, as the
// mean of its waves. Each swells to its height at half its life.
float sumWaves(const RippleWaves &waves, const RippleSpec &spec, float across)
{
    float sum = 0.0f;
    for (const RippleWave &wave : waves) {
        float angle = (wave.phase + wave.age * wave.speed + across + 0.0f) * kPi;
        if (fabsf(angle) > kPi) {
            const float turns = angle * kOverTwoPi;
            const float wrapped = (turns - (float)(int)turns) + 1.5f;
            angle = (wrapped - (float)(int)wrapped) * kTwoPi - kPi;
        }
        float swell = wave.age / wave.life * 2.0f;
        if (swell >= 1.0f)
            swell = 2.0f - swell;
        sum = sinCos(angle, kSineMode) * swell * spec.pushOf(wave.speed) + sum + 0.0f;
    }
    return sum / (float)waves.size();
}

// FS2 0x002d2e10
// callcheck: randNext, randNext, randNext (PC: roll)

void restartWaves(RippleWaves &waves, const RippleSpec &spec)
{
    for (RippleWave &wave : waves) {
        wave.age = 0.0f;
        spec.roll(wave);
    }
}

// FS2 0x002d2c90
// callcheck: randNext, randNext, randNext (PC: roll)

void advanceWaves(RippleWaves &waves, const RippleSpec &spec, float step)
{
    for (RippleWave &wave : waves) {
        wave.age += step;
        if (wave.life <= wave.age) {
            wave.age = 0.0f;
            spec.roll(wave);
        }
    }
}

} // namespace

// FS2 0x002d2c20

void RippleSpec::setPeak(float peak)
{
    if (speedMin < speedMax && peak >= kFlatPeak) {
        push = (speedMin * kFlatPeak - peak * speedMax) / (speedMin - speedMax);
        pushOverSpeed = (1.0f - push) * speedMin;
    } else {
        push = peak;
        pushOverSpeed = 0.0f;
    }
}

void RippleSpec::roll(RippleWave &wave) const
{
    wave.life = rollBetween(lifeMin, lifeMax);
    wave.speed = rollBetween(speedMin, speedMax);
    wave.phase = (float)(int)randNext() / kRandRange;
}

// FS2 0x002d31d0
// callcheck: FUN_002d3340, FUN_002d3470, ~RippleWaves, FUN_002d3340, FUN_002d3470, ~RippleWaves, FUN_002d2f50 (PC: std::vector, restart)

RippleSheet::RippleSheet(u_int columns, u_int rows, u_int waves)
    : m_columns(columns, RippleWaves(waves, RippleWave{})),
      m_rows(rows, RippleWaves(waves, RippleWave{})),
      m_spec{0.0f, 1.0f, 0.3f, 8.0f, 0.5f, 10.0f}
{
    restart();
}

// FS2 0x002d2f50
// callcheck: FUN_002d2e10, FUN_002d2e10 (PC: restartWaves)

void RippleSheet::restart()
{
    for (RippleWaves &waves : m_columns)
        restartWaves(waves, m_spec);
    for (RippleWaves &waves : m_rows)
        restartWaves(waves, m_spec);
}

// FS2 0x002d3080
// callcheck: FUN_002d2c90, FUN_002d2c90 (PC: advanceWaves)

void RippleSheet::advance(float seconds)
{
    for (RippleWaves &waves : m_columns)
        advanceWaves(waves, m_spec, seconds * kPi);
    for (RippleWaves &waves : m_rows)
        advanceWaves(waves, m_spec, seconds * kPi);
}

// FS2 0x002d10f0
// Every point's push and its height: where the two pushes add up the frame
// will brighten. The ROM's points go to the scratchpad, or the heap past 0x4000
// bytes; a byte at 0x31aed8 no code writes would turn the pass off.
// PC: the ROM draws the sheet from the frame into the other buffer, its alpha
// the height, and copies that back in sprites: the host's pass does that with
// no GS memory, and the two passes SightJack::drawLive lays over it.
// callcheck: operator_new[], sinCos, sinCos, gfxGetDrawEnv, operator_delete[] (PC: YamiPan_DrawUiFrameRipple)

void RippleSheet::draw(sceVif1Packet *packet, u_int tint) const
{
    const u_int columns = (u_int)m_columns.size();
    const u_int rows = (u_int)m_rows.size();
    const float heightOfPush = kHeightRange / m_spec.pushOf(m_spec.speedMax);

    std::vector<YamiPanRipplePoint> points;
    points.reserve((size_t)columns * rows);
    for (u_int row = 0; row < rows; row++) {
        const float down = ((float)(int)row * kSheetSpan) / (float)rows;
        for (u_int column = 0; column < columns; column++) {
            const float pushX = sumWaves(m_columns[column], m_spec, down);
            const float pushY = sumWaves(m_rows[row], m_spec, ((float)(int)column * kSheetSpan) / (float)columns);
            YamiPanRipplePoint point;
            point.x = (int)(pushX * kGsUnits);
            point.y = (int)(pushY * kGsUnits);
            point.height = (int)(heightOfPush * (pushX + pushY) + kHeightLevel);
            points.push_back(point);
        }
    }

    YamiPanRipple ripple;
    ripple.points = points.data();
    ripple.columns = (int)columns;
    ripple.rows = (int)rows;
    ripple.tint = tint;
    YamiPan_DrawUiFrameRipple(packet, &ripple);
}
