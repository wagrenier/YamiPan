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

void gfxInitRenderTargets();                                             // FS2 0x0012e3f0
void gfxReleaseRenderTargets();                                          // FS2 0x0012e3a0

// ready: flip to the current display record; kickPacket: send its GIF
// packet; putDrawEnv: sceGsPutDrawEnv its draw env.
void gfxFlipDisplay(int ready, int kickPacket, int putDrawEnv);          // FS2 0x0012e0e0

#endif /* _GAME_GFXTARGET_H */
