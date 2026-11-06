/* ==========================================================================
 *  game/vectortrack.h
 *
 *  Vectors that move over frames: game/vectortrack.cpp. VectorFade from one
 *  value to another, VectorTrack along chained keys (VectorKey).
 * ======================================================================== */
#ifndef _GAME_VECTORTRACK_H
#define _GAME_VECTORTRACK_H

#include "sdk/libvu0.h"     /* sceVu0FVECTOR */
#include "sdk/scetypes.h"

// A vector moving from one value to another over a number of frames: the
// seven at StageModel +0x140 (EE 0x38 in 0x40) and SceneRender's own.
// setFrom, frames and frame are set first; each frame setTo, then step.
struct alignas(0x40) VectorFade
{
    VectorFade();                                                        // FS2 0x00296cc0
    void setFrom(const sceVu0FVECTOR v);                                 // FS2 0x00296cb0
    void setTo(const sceVu0FVECTOR v);                                   // FS2 0x00296ca0
    void step();                                                         // FS2 0x00296c10

    sceVu0FVECTOR from;         // FS2 +0x00
    sceVu0FVECTOR to;           // FS2 +0x10
    sceVu0FVECTOR value;        // FS2 +0x20
    int           frames;       // FS2 +0x30
    int           frame;        // FS2 +0x34
};
static_assert(sizeof(VectorFade) == 0x40, "VectorFade: EE 0x40");

// A keyed vector over frames, EE 0x20: the lights' colour, every value of
// SceneRender. value is the frame's vector; its w the weight (StageLight::
// getWeight). The file was cut out of the run unassigned@00299d90.
// layout: host -- EE offsets in the comments; a key is EE 0x40.
struct VectorKey
{
    VectorKey(const float *from, int keyFrames);                         // FS2 0x0029a6e0
    explicit VectorKey(int keyFrames);                                   // FS2 0x0029a700
    // The key moves to time frames from the start, trading frames with its
    // neighbours; below 0 it stays.
    void setTime(int time);                                              // FS2 0x0029a5b0

    sceVu0FVECTOR value;        // FS2 +0x00
    sceVu0FVECTOR ctrlOut;      // FS2 +0x10: the Bezier point after the key
    sceVu0FVECTOR ctrlIn;       // FS2 +0x20: the Bezier point before it
    int           frames;       // FS2 +0x30: to the next key
    VectorKey    *prev;         // FS2 +0x38
    VectorKey    *next;         // FS2 +0x3c
};
// layout: host -- EE 0x20.
struct VectorTrack
{
    VectorTrack();                                                       // FS2 0x0029a580
    ~VectorTrack();                                                      // FS2 0x0029a500
    // One frame: index and frame move, value is worked out.
    void step();                                                         // FS2 0x00299d90
    // Back to the start; with two keys or more the first key's value.
    void reset();                                                        // FS2 0x00299f40
    // Chains key last; time places it, computeFrames (1 here) makes the
    // frames from the distance when the key has none.
    void addKey(VectorKey *key, int time, bool computeFrames);           // FS2 0x0029a370
    // The index-th key, null past the last.
    VectorKey *keyAt(int index);                                         // FS2 0x0029a4c0

    sceVu0FVECTOR value;        // FS2 +0x00: w the weight (+0xc)
    u_char     linear;          // FS2 +0x10: 1 linear (SceneRender's), 0 a Bezier
    u_char     loop;            // FS2 +0x11
    u_char     done;            // FS2 +0x12
    u_char     curve;           // FS2 +0x13: 2 wraps the values as angles
    int        frame;           // FS2 +0x14
    short      count;           // FS2 +0x18: keys
    short      index;           // FS2 +0x1a: the key playing
    VectorKey *keys;            // FS2 +0x1c

private:
    void sample(VectorKey *key);                                         // FS2 0x00299f90
    void append(VectorKey *key, bool computeFrames);                     // FS2 0x0029a3b0
    void computeControls();                                              // FS2 0x0029a160
};

#endif
