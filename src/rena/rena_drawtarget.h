/* ==========================================================================
 *  rena/rena_drawtarget.h
 *
 *  The frame's draw target (rena/rena_drawtarget.cpp, 0x2ec110..0x2ec15c):
 *  one object behind a guarded function-local static (bss 0x5a52d0, guard
 *  sbss 0x31af30), built on first use. drawSysRender stores the frame's
 *  draw env in it, drawSysInit reads the screen size from it, and rena's
 *  pass setup (rena_draw.cpp) asks for it. It sits between rena's packet
 *  manager and the layout paths, with no file string: the file name is the
 *  port's.
 * ======================================================================== */
#ifndef _RENA_RENA_DRAWTARGET_H
#define _RENA_RENA_DRAWTARGET_H

#include "sdk/libgraph.h"   /* sceGsDrawEnv1 */

// layout: host (a pointer at +0x14). The ROM's first-use init writes only
// +0x00..+0x08 and +0x10; +0x0c is left as the bss had it.
struct DrawTarget
{
    int   _00 = 100;            // FS2 +0x00 (unread)
    int   _04 = 100;            // FS2 +0x04 (unread)
    int   _08 = 100;            // FS2 +0x08 (unread)
    float _10 = 1.0f;           // FS2 +0x10 (unread)
    const sceGsDrawEnv1 *env = nullptr; // FS2 +0x14: the frame's
    int   width = 0;            // FS2 +0x18
    int   height = 0;           // FS2 +0x1c
    int   originX = 0;          // FS2 +0x20
    int   originY = 0;          // FS2 +0x24
};

// The one draw target, built on the first call.
DrawTarget *getDrawTarget();    // FS2 0x002ec110

#endif /* _RENA_RENA_DRAWTARGET_H */
