/* ==========================================================================
 *  game/clock.h
 *
 *  The game clocks 0x1351b0..0x135900 (unassigned run from 0x133800): a
 *  Clock scales its parent's delta by an 8.8 factor and passes it on to its
 *  children; MasterClock is the root, advanced once per frame by the vsyncs
 *  the frame took. Minimal: only what game/gamesystem.cpp touches.
 * ======================================================================== */
#ifndef _GAME_CLOCK_H
#define _GAME_CLOCK_H

#include "sdk/scetypes.h"

// Clock::m_time's units per second (the int at 0x300e48; the play-time
// ticks game/uigame.cpp shows run at 25).
constexpr int kClockTimePerSecond = 300;

// layout: host -- holds the parent and child pointers; EE 0x20.
class Clock
{
public:
    ~Clock();                                                            // FS2 0x001351b0
    Clock *createChild();                                                // FS2 0x00135210
    // m_time in seconds (kClockTimePerSecond).
    float  getSeconds();                                                 // FS2 0x00135420
    void   resetTime();                                                  // FS2 0x001357b0: m_time = 0

    int     m_state;            // FS2 +0x00: 1 paused
    int     m_time;             // FS2 +0x04
    int     m_delta;            // FS2 +0x08
    int     m_time2;            // FS2 +0x0c
    int     m_scale;            // FS2 +0x10: 8.8 fixed point, 0x100 = 1
    Clock  *m_parent;           // FS2 +0x14
    u_short m_childCapacity;    // FS2 +0x18: PtrList {u16 capacity, u16 count, void **data}
    u_short m_childCount;       // FS2 +0x1a
    u_char  m_flag1c;           // FS2 +0x1c: MasterClock's last constructor argument; meaning open
};

// layout: host -- EE 0x30.
class MasterClock : public Clock
{
public:
    MasterClock(int ticksPerFrame, int interval, bool variableRate, bool flag1c); // FS2 0x00135390
    ~MasterClock();                                                      // FS2 0x00135330
    void update(int vsyncs);                                             // FS2 0x001352d0

    int     m_ticksPerFrame;    // FS2 +0x20
    int     m_vsyncTotal;       // FS2 +0x24
    int     m_interval;         // FS2 +0x28: vsyncs per frame
    u_char  m_variableRate;     // FS2 +0x2c
};

#endif /* _GAME_CLOCK_H */
