/* ==========================================================================
 *  rena/rena_camera.h
 *
 *  rena's camera base and its box test (the run 0x2e2970..0x2e2cf0; no file
 *  string, the name is the port's). The camera base 0x2e2970 (vtable
 *  0x31a418) is written as DrawSysCamera's ctor in game/drawsys.cpp, the only
 *  class built on it.
 *
 *  boxClipTest holds a part's box against the planes rena's frame set-up
 *  works out (0x2e5bb0, into its draw state's +0xc0 block). The host's draws
 *  test their boxes on the host's own projection for now (rena_draw.cpp
 *  boxOutside, widened for every aspect ratio): the planes are VU1 constants
 *  the host does not build yet, so nothing calls this until they are.
 * ======================================================================== */
#ifndef _RENA_RENA_CAMERA_H
#define _RENA_RENA_CAMERA_H

#include "sdk/libvu0.h"

// What boxClipTest answers.
enum BoxClip
{
    BoxClip_Inside   = 0,   // within every frame plane, or crossing none of the guard band's
    BoxClip_Crossing = 1,   // crosses the guard band: its triangles need clipping
    BoxClip_Outside  = 2,   // wholly behind one plane: not drawn
};

// The planes of a frame, as 0x2e5bb0 builds them: the draw state's +0xc0
// (EE 0x120; the state's +0x00 and +0x40 are the two clip matrices of
// 0x2e5fc0, at a margin of 0 and of 300 pixels). A point p is on a plane's
// inner side when dot(plane, p) > 0. layout: host (the two pointers).
struct RenaClipView
{
    enum
    {
        kNear       = 0,    // w - near
        kFar        = 1,    // far - w (the guard band's 500 further)
        kFirstSide  = 2,    // 2..5: the clip matrix's columns
        kPlaneCount = 6,
    };

    sceVu0FMATRIX        viewScreen;               // EE +0x00: copied from 0x59b0c0
    const sceVu0FVECTOR *frameClip;                // EE +0x40: the state's +0x00
    const sceVu0FVECTOR *guardClip;                // EE +0x44: the state's +0x40
    sceVu0FVECTOR        framePlanes[kPlaneCount]; // EE +0x50: margin 0
    sceVu0FVECTOR        guardPlanes[kPlaneCount]; // EE +0xb0: margin 300, far +500
    float                nearDepth;                // EE +0x110
    float                farDepth;                 // EE +0x114
};

// A box (box[0] its centre with w, box[1] its half sizes) through matrix,
// against the frame's planes, then the guard band's planes it crosses.
int boxClipTest(const sceVu0FVECTOR *box, const sceVu0FVECTOR *matrix, const RenaClipView *view); // FS2 0x002e2a60

#endif /* _RENA_RENA_CAMERA_H */
