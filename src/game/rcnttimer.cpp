// game/rcnttimer.cpp -- RcntTimer, the stopwatch on RCNT1 (game/rcnttimer.h). The file name is a guess:
// no source-path string, the extent is the class and rcnt1Start between gfxtarget and pad.

#include "game/rcnttimer.h"

#include "sdk/eeregs.h"

// T1_MODE: CLKS 3 clocks the counter on H-blank, CUE starts it.
constexpr u_int kRcntModeClockHblank = 0x03;
constexpr u_int kRcntModeCount = 0x80;

constexpr float kMicrosecondsPerSecond = 1000000.0f;

// Set once RCNT1 is running.
static bool s_rcnt1Started; /* sbss gp-0x7ac4 (0x31adac) */

// ──────────────────────────────────────────────────────────────────────
// PerfMeter::beginFrame restarts the stopwatch each frame.
// FS2 0x0012f190

void RcntTimer::reset()
{
    m_08 = 0;
    m_startCount = REG_RCNT1_COUNT;
}

// ──────────────────────────────────────────────────────────────────────
// Nothing to undo; RCNT1 keeps counting.
// FS2 0x0012f1b0
// callcheck: operator_delete (the deleting variant, flags > 0)

RcntTimer::~RcntTimer()
{
}

// ──────────────────────────────────────────────────────────────────────
// The ROM has rcnt1Start and reset inlined at the end.
// FS2 0x0012f200

RcntTimer::RcntTimer(float tickRate, float fieldRate, int fieldsPerFrame)
{
    m_usPerTick = kMicrosecondsPerSecond / tickRate;
    m_fieldRate = fieldRate;
    m_fieldsPerFrame = fieldsPerFrame;
    rcnt1Start();
    reset();
}

// ──────────────────────────────────────────────────────────────────────
// gameInit calls it once the sound is up.
// FS2 0x0012f260

void rcnt1Start()
{
    if (!s_rcnt1Started) {
        REG_RCNT1_MODE = kRcntModeClockHblank | kRcntModeCount;
        s_rcnt1Started = true;
    }
}
