/* ==========================================================================
 *  game/rcnttimer.h
 *
 *  RcntTimer: a stopwatch on RCNT1, which gameInit (rcnt1Start) or the first
 *  one constructed sets counting H-blanks. PerfMeter is one, and the only
 *  one: the retail build records the count and never reads it back.
 * ======================================================================== */
#ifndef _GAME_RCNTTIMER_H
#define _GAME_RCNTTIMER_H

#include <stddef.h>

#include "sdk/scetypes.h"

void rcnt1Start();                                                       // FS2 0x0012f260

class RcntTimer
{
public:
    // usPerTick = 1e6 / tickRate. CodeWarrior passes the floats in f12 / f13
    // and the int in a1, so the parameter order is a guess.
    RcntTimer(float tickRate, float fieldRate, int fieldsPerFrame);      // FS2 0x0012f200
    ~RcntTimer();                                                        // FS2 0x0012f1b0

    void reset();                                                        // FS2 0x0012f190

    u_int  m_startCount;        // FS2 +0x00: T1_COUNT at the last reset, never read in retail
    u_long m_08;                // FS2 +0x08: zeroed with m_startCount, never read in retail
    float  m_usPerTick;         // FS2 +0x10
    float  m_fieldRate;         // FS2 +0x14
    int    m_fieldsPerFrame;    // FS2 +0x18
};

static_assert(offsetof(RcntTimer, m_startCount) == 0x00);
static_assert(offsetof(RcntTimer, m_08) == 0x08);
static_assert(offsetof(RcntTimer, m_usPerTick) == 0x10);
static_assert(offsetof(RcntTimer, m_fieldRate) == 0x14);
static_assert(offsetof(RcntTimer, m_fieldsPerFrame) == 0x18);
static_assert(sizeof(RcntTimer) == 0x20);

#endif /* _GAME_RCNTTIMER_H */
