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

#include "sdk/scetypes.h"

// A display record (0x540 bytes at 0x4ae280; layout not written yet).
struct GfxDisplay;
// The draw env packet at +0x80 of a display record (layout not written yet:
// a GIF tag at +0, the clear colour RGBA at +0xb0).
struct GfxDrawEnv;

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

#endif /* _GAME_GFXTARGET_H */
