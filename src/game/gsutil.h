/* ==========================================================================
 *  game/gsutil.h
 *
 *  GS helpers of the 2D layouts (game/gsutil.cpp, 0x230980..0x231410): the
 *  draw-state presets UiDrawer::setDrawPreset picks (A+D runs of fixed
 *  registers, then FRAME_1 / ZBUF_1 from the current draw env with the
 *  preset's write masks) and GsTexture, rena's TIM2 texture that can also
 *  stand for one of the GS render targets. The file name is the port's
 *  (no source string).
 * ======================================================================== */
#ifndef _GAME_GSUTIL_H
#define _GAME_GSUTIL_H

#include "rena/rena_2d_texture_tim2.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

// ──────────────────────────────────────────────────────────────────────
// The presets. Every one but DrawEnv resets TEST_1 / ALPHA_1 / TEX1_1,
// writes FRAME_1 with the masks below and ZBUF_1 with ZMSK (no depth
// writes); DrawEnv puts back the env's own FRAME_1 / ZBUF_1 / SCISSOR_1.

// Colour and alpha written, normal blend; also COLCLAMP on and CLAMP_1
// clamp (after a repeat write: the ROM's table writes it twice).
void gfxUiPresetNormal(sceVif1Packet *packet);                           // FS2 0x00231090
// Only the frame's alpha written (FBMSK 0x00ffffff), alpha test always.
void gfxUiPresetAlphaOnly1(sceVif1Packet *packet);                       // FS2 0x00230e40
// Only alpha, where the source alpha >= 0x80.
void gfxUiPresetAlphaOnly2(sceVif1Packet *packet);                       // FS2 0x00230d10
// Only colour (FBMSK 0xff000000), every pixel through the alpha test's
// fail path RGB_ONLY, only where the frame's alpha bit is 0 (DATE).
void gfxUiPresetRgbOnly3(sceVif1Packet *packet);                         // FS2 0x00230be0
// As RgbOnly3, where the frame's alpha bit is 1 (DATM 1).
void gfxUiPresetRgbOnly4(sceVif1Packet *packet);                         // FS2 0x00230ab0
// Only colour, blended by the frame's alpha ((Cs - Cd) * Ad + Cd).
void gfxUiPresetRgbOnly5(sceVif1Packet *packet);                         // FS2 0x00230980
// The draw env's FRAME_1 / ZBUF_1 / SCISSOR_1 as they are (UiDrawer::end).
void gfxUiPresetDrawEnv(sceVif1Packet *packet);                          // FS2 0x00230f70

// ──────────────────────────────────────────────────────────────────────
// The render targets an image record can name in its TIM2 header's
// reserved word (+0xc): the 512x512 buffers of gfxInitRenderTargets.
enum GsTargetTag
{
    GsTargetTag_Zbuf = 0x6675625a,  // 'Zbuf': PSMCT32 at block 0x1800 (copyTarget's backup)
    GsTargetTag_Draw = 0x77617244,  // 'Draw': PSMCT32 at block 0x800
    GsTargetTag_View = 0x77656956,  // 'View': PSMCT16 at block 0
};

// A UI texture (EE 0x60; vptr 0x3164e8 at +0x44): a Tim2Texture, or with
// a target tag a fixed TEX0 on that render target. The UI tracks own one
// per textured shape. layout: host -- EE offsets in the comments.
// PC: the host draw module decodes the image at its source to a GPU
// texture (TextureStore), or samples its own copy of the target
// (m_isTarget / m_targetTex0's TBP0): no GS upload.
class GsTexture : public Tim2Texture
{
public:
    GsTexture(const void *image);                                        // FS2 0x00231360
    virtual ~GsTexture();                                                // FS2 0x002313b0: deleting (obj, flag)

    // The image's size, a target's own 512 x 512.
    void getSize(int *width, int *height);                               // FS2 0x002311c0
    // putTex0 of the base, or the target's fixed TEX0 (block ignored).
    void putTex0(u_long **cursor, int block, u_int reg);                 // FS2 0x00231200

    // Is the image a target tag? Sets the target fields; 0 otherwise.
    int readTarget(const void *image);                                   // FS2 0x00231290

    u_long m_targetTex0;        // FS2 +0x48
    u_char m_isTarget;          // FS2 +0x50
    int    m_targetWidth;       // FS2 +0x54
    int    m_targetHeight;      // FS2 +0x58
};

#endif /* _GAME_GSUTIL_H */
