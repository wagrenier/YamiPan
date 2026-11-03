/* ==========================================================================
 *  game/pad.h
 *
 *  Pad (game/pad.cpp, not written yet): one controller port on libpad.
 *  padSysInit 0x130130 opens the ports and keeps 16 listener slots at
 *  0x4af000; padSysUpdate calls each listening pad's update (its one
 *  virtual) every frame. GameSystem holds three (game/gamesystem.h).
 *
 *  Button words are libpad's, inverted to active-high: the SCE_PAD* bits
 *  of sdk/libpad.h (L2 0x1, R2 0x2, L1 0x4, R1 0x8, ... select 0x100,
 *  L3 0x200, R3 0x400, start 0x800).
 *
 *  Minimal: only what game/gamesystem.cpp touches. Its TU completes it.
 * ======================================================================== */
#ifndef _GAME_PAD_H
#define _GAME_PAD_H

#include "sdk/scetypes.h"

// Each port's per-pressure-button actuator record, set to {7, 9, 0} by the
// constructors (meaning unread).
struct PadActRecord
{
    u_short a;                  // FS2 +0x0: 7
    u_short b;                  // FS2 +0x2: 9
    int     c;                  // FS2 +0x4: 0
};
static_assert(sizeof(PadActRecord) == 0x8, "PadActRecord: EE 0x8");

// layout: host -- polymorphic; on the EE the vptr follows the data at +0x120
// (vtable 0x311050, one slot), EE size 0x124.
class Pad
{
public:
    Pad(int port, bool listen);                                          // FS2 0x0012fcf0
    Pad(bool listen);                                                    // FS2 0x0012fb10
    ~Pad();                                                              // FS2 0x0012fa90

    // EE slot 0 (+0x8, through the thunk 0x130060): read the port; nonzero
    // when a report was read this frame.
    virtual int update();                                                // FS2 0x0012f300
    void clear();                                                        // FS2 0x0012f960
    void setListening(bool on);                                          // FS2 0x0012fed0

    int     m_frame;            // FS2 +0x00: input frame read
    int     m_port;             // FS2 +0x04
    u_char  m_listening;        // FS2 +0x08: in padSysUpdate's listener table
    int     m_termMode;         // FS2 +0x0c: terminal mode
    u_int   m_previous;         // FS2 +0x10
    u_int   m_held;             // FS2 +0x14
    u_int   m_pressed;          // FS2 +0x18
    u_int   m_released;         // FS2 +0x1c
    u_int   _20;                // FS2 +0x20: cleared by the constructors
    u_char  m_analog[4];        // FS2 +0x24: raw stick bytes, 0x80 = centre
    float   m_stick[4];         // FS2 +0x28: left x, left y, right x, right y (-1..1, y down)
    u_char  m_pressure[25];     // FS2 +0x38
    PadActRecord m_act[25];     // FS2 +0x54
    u_char  _11c;               // FS2 +0x11c: cleared by the constructors
};

void padSysInit();                                                       // FS2 0x00130130
void padSysUpdate();                                                     // FS2 0x0012ffa0
void padSysShutdown();                                                   // FS2 0x00130070

#endif /* _GAME_PAD_H */
