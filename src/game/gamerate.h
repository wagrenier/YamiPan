// game/gamerate.h -- PC: a native frame rate (roadmap-framerate.md "Native 60 Hz"); not in the ROM.
//
// The ROM's frame is two PAL fields and 12 master-clock ticks (game/gamesystem.h kVsyncsPerFrame,
// kClockTicksPerFrame). At yamipan.ini's game_rate 50 it is one field and 6 ticks, at 60 one field of a 60 Hz field
// clock and 5 ticks, and the core, which reads the clock, follows by itself. A rule the ROM counts in frames takes
// palTicks() (the 25 Hz frames that ended in this game frame: always 1 at 25; 0 or 1 above) or palScale() (this
// frame's length in 25 Hz frames: 1, 0.5 or 5 / 12);
// framerate-inventory.md lists the rules. At 25 both are exactly the ROM. Defined in game/gamesystem.cpp; a tool
// that compiles one game file alone (the *cmp.py tools) gives stand-ins returning 1.
//
// A system kept in whole 25 Hz steps above 25 (thrown items, critters: roadmap-framerate.md) is drawn between its
// last two steps, palDrawShare() of the way, so it moves every frame; at 25 that share is 1, the ROM's newest place.
#pragma once

int   gameVsyncsPerFrame();      // fields a game frame: the ROM's 2, or 1
int   gameClockTicksPerFrame();  // master-clock ticks a game frame: the ROM's 12, 6 or 5
int   palTicks();                // 25 Hz frames that ended in this game frame
float palScale();                // this game frame's length in 25 Hz frames
float palEase(float rate);       // a linear ease's share a 25 Hz frame -> this frame's: 1 - (1 - rate)^palScale
int   palFrame();                // the 25 Hz frames ended so far: which one a 25 Hz step belongs to
float palDrawShare();            // how far from the place before the last 25 Hz step to the last one to draw: 1 at 25

// The placement `share` of the way from `from` to `to` (sceVu0 rows: three axes, then the origin): the turn by a
// slerp, each axis's length and the origin in a straight line. `to` itself, bit for bit, at a share of 1 or more,
// or when either has no turn a quaternion can hold (a flat or mirrored one).
void  palBlendMatrix(float out[4][4], const float from[4][4], const float to[4][4], float share);
