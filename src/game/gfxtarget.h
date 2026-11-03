/* ==========================================================================
 *  game/gfxtarget.h
 *
 *  Display and render-target set-up (game/gfxtarget.cpp, not written yet):
 *  the 256x256 and 512x512 offscreen draw envs and VRAM managers
 *  (gfxInitRenderTargets), and the display records 0x4ae280 (0x540 bytes
 *  each) gfxFlipDisplay writes to the GS display registers when a frame is
 *  ready. Minimal: only what game/gamesystem.cpp touches.
 * ======================================================================== */
#ifndef _GAME_GFXTARGET_H
#define _GAME_GFXTARGET_H

// A display record (0x540 bytes at 0x4ae280; layout not written yet).
struct GfxDisplay;

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

#endif /* _GAME_GFXTARGET_H */
