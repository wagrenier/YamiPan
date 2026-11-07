/* ==========================================================================
 *  game/framefilter.h
 *
 *  The frame filter of the view cameras and event d0101 (StageScene::draw,
 *  StageScene::drawViewFilter): the frame shrunk by halves `passes` times,
 *  each size up to 256 blurred `arg` times by a 3-tap box along x then y,
 *  and grown back the same way into the frame. The names are the port's (no
 *  source string); FS2's own gs* prefix is kept, as gsFillRect's.
 *
 *  0x2849c0..0x286790, between AreaBox (game/areagrid.cpp) and StageMission
 *  (game/demoevent.cpp).
 * ======================================================================== */
#ifndef _GAME_FRAMEFILTER_H
#define _GAME_FRAMEFILTER_H

#include "sdk/libgraph.h"     /* sceGsDrawEnv1 */
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

// The frame-buffer filter: passes times over the w x h frame of env, block
// the VRAM block (32-block pages) of its first scratch buffer, arg the blur
// passes at each size (1 or 5 in FS2's calls).
void gsFrameFilter(sceVif1Packet *packet, const sceGsDrawEnv1 *env, int w, int h,
                   u_short block, int passes, int arg);                  // FS2 0x002849c0

// A GS pixel format's page width in pixels: 128 for PSMT8 / PSMT4, 64 for
// the 32 / 24 / 16-bit colour and Z formats, 0 for any other.
int gsPageWidth(int psm);                                                // FS2 0x002866f0

#endif /* _GAME_FRAMEFILTER_H */
