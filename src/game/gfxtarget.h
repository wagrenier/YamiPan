/* ==========================================================================
 *  game/gfxtarget.h
 *
 *  Display and render-target set-up (game/gfxtarget.cpp, not written yet):
 *  the 256x256 and 512x512 offscreen draw envs and VRAM managers
 *  (gfxInitRenderTargets), and the display records 0x4ae280 (0x540 bytes
 *  each) gfxFlipDisplay writes to the GS display registers when a frame is
 *  ready. Minimal: only what game/gamesystem.cpp and game/scene.cpp touch.
 * ======================================================================== */
#ifndef _GAME_GFXTARGET_H
#define _GAME_GFXTARGET_H

#include <stddef.h>

#include "sdk/libgraph.h"
#include "sdk/scetypes.h"

// A display record (0x540 bytes at 0x4ae280; layout not written yet).
struct GfxDisplay;
// The draw env packet at +0x80 of a display record: a GIF tag, then the
// context 1 registers in sceGsDrawEnv1's order (the UI presets and
// UiDrawer::resetScissor read FRAME_1 / ZBUF_1 / SCISSOR_1 from it). The
// rest (the clear colour RGBA at +0xb0) is not written yet.
struct GfxDrawEnv
{
    u_long128     tag;          // FS2 +0x00
    sceGsDrawEnv1 env;          // FS2 +0x10
};
static_assert(offsetof(GfxDrawEnv, env) == 0x10, "GfxDrawEnv::env");
static_assert(offsetof(GfxDrawEnv, env.scissor1) == 0x40, "GfxDrawEnv::env.scissor1");

void gfxInitRenderTargets();                                             // FS2 0x0012e3f0
void gfxReleaseRenderTargets();                                          // FS2 0x0012e3a0

// ready: flip to the current display record; kickPacket: send its GIF
// packet; putDrawEnv: sceGsPutDrawEnv its draw env.
void gfxFlipDisplay(int ready, int kickPacket, int putDrawEnv);          // FS2 0x0012e0e0

// The display record in use (index gp-0x7acc).
GfxDisplay *gfxGetCurrentDisplay();                                      // FS2 0x0012e050
// Moves the picture: stores x / y in the record and rewrites its DISPLAY1 /
// DISPLAY2 DX / DY. 0x12ead0 is in the unassigned run after this file's
// range (0x12ea60..): likely this file.
void gfxSetDisplayOffset(GfxDisplay *display, short x, short y);         // FS2 0x0012ead0

// The current display record's draw env (gfxGetCurrentDisplay() + 0x80).
GfxDrawEnv *gfxGetCurrentDrawEnv();                                      // FS2 0x0012e080
// enable: the env packet clears the frame (GIF tag NLOOP 0xe, else 8) to
// r, g, b, a. In the same unassigned run as gfxSetDisplayOffset.
void gfxSetDrawEnvClear(GfxDrawEnv *env, int enable, u_char r, u_char g,
                        u_char b, u_char a);                             // FS2 0x0012ea80

// GS texture pages 0x3700..0x3fbe handed out per packet list (EE 0x814,
// gfxInitRenderTargets builds it: unassigned@00142dd0). Layout draws take
// the list of the packet list they draw into. layout: host -- not declared.
// PC: the host draw module keeps textures on the GPU; this goes with the
// GS upload path (the user's rule: no GS memory model).
class TexArea
{
public:
    TexArea(int firstPage, int lastPage, int lists, int arg);            // FS2 0x00143270
    void *getList(int list);                                             // FS2 0x00142dd0
};
// The one TexArea (gp-0x7ad0). In the same unassigned run as
// gfxSetDisplayOffset.
TexArea *gfxGetTexArea();                                                // FS2 0x0012f160

// A GS texture built from an image record (EE 0x60 bytes, vptr 0x3164e8
// at +0x44; unassigned@00171300). The UI tracks own one per textured
// shape. layout: host -- not declared.
// PC: the host draw module decodes the image at its source to a GPU
// texture keyed by this object (no GS upload).
class GsTexture
{
public:
    GsTexture(const void *image);                                        // FS2 0x00231360
    virtual ~GsTexture();                                                // FS2 0x002313b0: deleting (obj, 1)

    // The image's size (a render target's own, +0x54 / +0x58).
    void getSize(int *width, int *height);                               // FS2 0x002311c0
    // PC: the GS side -- TEX0 into a register (0x231200) and the image
    // transfer (upload 0x2e1c50) -- is not declared: the host draw module
    // binds the texture by its owner.
};

#endif /* _GAME_GFXTARGET_H */
