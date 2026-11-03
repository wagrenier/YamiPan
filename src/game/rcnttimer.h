/* ==========================================================================
 *  game/rcnttimer.h
 *
 *  RcntTimer (game/rcnttimer.cpp, not written yet): a stopwatch on RCNT1,
 *  which the first one constructed sets counting H-blanks (T1_MODE 0x83).
 *  PerfMeter is one. Minimal: the fields are what its constructor writes.
 * ======================================================================== */
#ifndef _GAME_RCNTTIMER_H
#define _GAME_RCNTTIMER_H

#include "sdk/scetypes.h"

void rcnt1Start();                                                       // FS2 0x0012f260

// layout: host -- EE 0x20, fields from the constructor 0x12f200 only.
class RcntTimer
{
public:
    // usPerTick = 1e6 / tickRate. CodeWarrior passes the floats in f12 / f13
    // and the int in a1, so the parameter order is a guess.
    RcntTimer(float tickRate, float fieldRate, int fieldsPerFrame);      // FS2 0x0012f200
    ~RcntTimer();                                                        // FS2 0x0012f1b0

    void reset();                                                        // FS2 0x0012f190

    u_int  m_startCount;        // FS2 +0x00: T1_COUNT when constructed
    u_long m_08;                // FS2 +0x08: 64-bit, zeroed by the constructor
    float  m_usPerTick;         // FS2 +0x10
    float  m_fieldRate;         // FS2 +0x14
    int    m_fieldsPerFrame;    // FS2 +0x18
};

#endif /* _GAME_RCNTTIMER_H */
