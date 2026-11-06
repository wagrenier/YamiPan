/* ==========================================================================
 *  game/ripplesheet.h
 *
 *  What SightJack keeps at +0xb4 on stage 0x1b only (kStageBoss): the view
 *  through F_E27_BOSS bends and glints as if seen through moving water. A
 *  grid of points, each pushed sideways by one list of waves and down by
 *  another; the waves come and go at random. The name is the port's reading.
 *
 *  0x2d10f0..0x2d3700, with the std::vector inlines of its members.
 * ======================================================================== */
#ifndef _GAME_RIPPLESHEET_H
#define _GAME_RIPPLESHEET_H

#include <vector>

#include "sdk/scetypes.h"
#include "sdk/libvifpk.h"

// One wave: it swells from nothing and back over `life`. EE 0x10.
struct RippleWave
{
    float age;      // FS2 +0x0: 0..life
    float life;     // FS2 +0x4
    float speed;    // FS2 +0x8: its phase runs by this per unit of age
    float phase;    // FS2 +0xc: 0..1
};
static_assert(sizeof(RippleWave) == 0x10, "the ROM's new[] sizes");

typedef std::vector<RippleWave> RippleWaves;

// What a new wave is drawn from, and how far a wave pushes by its speed.
// The object's +0x18. EE 0x18.
struct RippleSpec
{
    float pushOverSpeed;    // FS2 +0x0
    float push;             // FS2 +0x4: a wave pushes push + pushOverSpeed / speed
    float speedMin;         // FS2 +0x8
    float speedMax;         // FS2 +0xc
    float lifeMin;          // FS2 +0x10
    float lifeMax;          // FS2 +0x14

    float pushOf(float speed) const { return push + pushOverSpeed / speed; }

    // Sets push and pushOverSpeed so that a wave of speedMin pushes 1, by the
    // ROM's sum, and the fast ones about peak. peak under 2 is flat.
    void setPeak(float peak);                                            // FS2 0x002d2c20
    // A new wave's life, speed and phase.
    void roll(RippleWave &wave) const;                                   // inline in 0x002d2c90, 0x002d2e10
};
static_assert(sizeof(RippleSpec) == 0x18, "the object's +0x18..+0x30");

// layout: host (std::vector members; EE 0x30, offsets in the comments)
class RippleSheet
{
public:
    // columns x rows points, each list `waves` long. EE: the fills 0x002d3340,
    // 0x002d3470, 0x002d35b0 are std::vector's.
    RippleSheet(u_int columns, u_int rows, u_int waves);                 // FS2 0x002d31d0

    RippleSpec &spec() { return m_spec; }

    // Every wave new.
    void restart();                                                      // FS2 0x002d2f50
    // The waves go on by seconds; those at the end of their life start anew.
    void advance(float seconds);                                         // FS2 0x002d3080
    // The frame so far through the sheet; tint is what the ROM's caller
    // passes to gsFrameContrast after it.
    void draw(sceVif1Packet *packet, u_int tint) const;                  // FS2 0x002d10f0

private:
    std::vector<RippleWaves> m_columns; // FS2 +0x00: capacity, +0x04 count, +0x08 data: sideways, a list a column
    std::vector<RippleWaves> m_rows;    // FS2 +0x0c: capacity, +0x10 count, +0x14 data: down, a list a row
    RippleSpec m_spec;                  // FS2 +0x18
};

#endif
