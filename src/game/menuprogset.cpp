/* ==========================================================================
 *  game/menuprogset.cpp
 *
 *  The gun sight, 0x2b9ef0..0x2bb600, drawn over the 3D frame in
 *  PacketList_Scene2d. Each sight's mask is one corner picture, 128 x 128:
 *  stretched over the four quarters of the 512 x 512 frame, the right and
 *  bottom ones mirrored. The art is squeezed 3:4 for that frame, so the hole
 *  is round on a 4:3 screen.
 *
 *  PC: no GS packets, no VRAM block: the register values go to the host's
 *  module (yamipan_drawui.h) and the textures are bound by their owner.
 *  What is drawn inside the frame keeps its shape at any aspect
 *  (YamiPan_DrawUiSqueeze), and the mask's outer column is drawn out to the
 *  display's edges.
 * ======================================================================== */
#include "game/menuprogset.h"

#include <math.h>

#include "game/gamehooks.h"
#include "game/gamepacket.h"
#include "game/gamesystem.h"
#include "game/gsutil.h"
#include "game/vu0math.h"
#include "rena/rena_2d_texture_tim2.h"
#include "yamipan/rendering/yamipan_drawui.h"

namespace {

// The frame, its quarters, and the draw env's origin in 1/16 pixel.
constexpr int   kFrameSize   = 0x200;
constexpr float kFrameEnd    = 512.0f;
constexpr float kFrameMiddle = 256.0f;
constexpr int   kSightOrigin = 0x7000;
constexpr int   kFixedPixel  = 0x10;        // pixels and texels to 1/16
constexpr int   kHalfTexel   = 8;           // of 1/16
constexpr int   kSpriteEdge  = 9;           // a sprite's far corner stops short by this, of 1/16
constexpr int   kFullFrameHeight = 0x1c0;   // the image height that fills the frame's 0x200

constexpr int   kMaskColumn  = 64;          // how wide sightDrawMaskQuad's strips are

constexpr u_int kPrimMask  = 0x154;         // TRISTRIP, TME, ABE, FST
constexpr u_int kPrimImage = 0x156;         // SPRITE, TME, ABE, FST

constexpr u_long kAlphaBlend = 0x8000000044ull; // ALPHA_1 (Cs - Cd) * As + Cd, FIX 0x80
constexpr u_long kAlphaOver  = 0x44;            // ALPHA_1 (Cs - Cd) * As + Cd
constexpr u_long kAlphaAdd   = 0x48;            // ALPHA_1 Cs * As + Cd
constexpr u_long kTestColourOnly = 0x33001;     // TEST_1: ATST NEVER, AFAIL RGB_ONLY, Z ALWAYS
constexpr u_long kTestAlways     = 0x30000;     // TEST_1: Z ALWAYS

// PC: the ROM sets neither and draws on what the stage's model passes left
// (rena_draw.cpp): repeating, as the mirrored UVs need, and bilinear.
constexpr u_long kClampRepeat = 0;
constexpr u_long kTex1Linear  = 0x60;       // MMAG, MMIN linear
// PC: the ticks and the lines are drawn 1:1 there. Scaled up here, bilinear
// would mix their clear white neighbours in: a pale edge on the black.
constexpr u_long kTex1Nearest = 0;

constexpr u_int kRgbaWhole = 0x80808080;    // the texture as it is
constexpr u_int kRgbaGrey  = 0x00808080;
constexpr u_int kRgbaSpot  = 0x20808080;    // a quarter of it, added
constexpr int   kBrightenAlpha = 0x20;

// The R_64C's ticks: from x 235 towards the left, on y 249.
constexpr int   kTickCount = 8;
constexpr float kTickStart = 235.0f;
constexpr int   kTickRow   = 0xf9;
constexpr float kTickSpan  = -120.0f;
constexpr float kTickZoomScale = 4.0f;
constexpr float kTickZoomBase  = 2.0f;

// The cross: 8 wide, on the frame's middle.
constexpr int kLineAt    = 0xfc;
constexpr int kLineThick = 8;

void setRgba(u_char out[4], u_int rgba)
{
    out[0] = (u_char)rgba;
    out[1] = (u_char)(rgba >> 8);
    out[2] = (u_char)(rgba >> 16);
    out[3] = (u_char)(rgba >> 24);
}

void putVertex(sceVif1Packet *packet, int u, int v, int x, int y)
{
    YamiPanUiVertex corner = {};
    corner.u = (u_short)u;
    corner.v = (u_short)v;
    corner.x = (u_short)x;
    corner.y = (u_short)y;
    YamiPan_DrawUiVertex(packet, &corner);
}

// One strip of a mask, in pixels and texels: the ROM's first two vertices
// are XYZ3, so a strip starts on its own.
void maskColumn(sceVif1Packet *packet, float x0, float y0, float x1, float y1,
                float u0, float v0, float u1, float v1)
{
    const int left   = (int)(x0 * 16.0f + (float)kSightOrigin);
    const int right  = (int)(x1 * 16.0f + (float)kSightOrigin);
    const int top    = (int)(y0 * 16.0f + (float)kSightOrigin);
    const int bottom = (int)(y1 * 16.0f + (float)kSightOrigin);
    const int uLeft   = (int)(u0 * 16.0f + (float)kHalfTexel);
    const int uRight  = (int)(u1 * 16.0f + (float)kHalfTexel);
    const int vTop    = (int)(v0 * 16.0f + (float)kHalfTexel);
    const int vBottom = (int)(v1 * 16.0f + (float)kHalfTexel);

    YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_UV);
    putVertex(packet, uLeft, vTop, left, top);
    putVertex(packet, uRight, vTop, right, top);
    putVertex(packet, uLeft, vBottom, left, bottom);
    putVertex(packet, uRight, vBottom, right, bottom);
}

// FS2 0x002babc0
// A quad of the bound texture over the frame's colours, in columns 64 wide.
// callcheck: gfxGetDrawEnv, __fixunssfdi, __fixunssfdi, __fixunssfdi, __fixunssfdi, __fixunssfdi, __fixunssfdi, __fixunssfdi, __fixunssfdi (PC: YamiPan_DrawUiPrim / Vertex in maskColumn)

void sightDrawMaskQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1,
                       sceVif1Packet *packet)
{
    u_char rgbaq[4];
    setRgba(rgbaq, kRgbaWhole);
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kAlphaBlend);
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kTestColourOnly);
    YamiPan_DrawUiPrim(packet, kPrimMask, rgbaq);

    const int columns = (int)((x1 - x0) / (float)kMaskColumn);
    const float step = (u1 - u0) / (float)columns;
    for (int i = 0; i < columns; i++) {
        maskColumn(packet, x0 + (float)(i * kMaskColumn), y0, x0 + (float)((i + 1) * kMaskColumn), y1,
                   (float)i * step + u0 + 0.0f, v0, (float)(i + 1) * step + u0 + 0.0f, v1);
    }
}

// The power of two a side of the texture sits in: the ROM's PLZCW. The mask's
// UVs count from it, the texture repeating, so that -1 can be written.
int textureSpan(int size)
{
    int span = 1;
    while (span < size)
        span <<= 1;
    return span;
}

// The corner picture over the frame's four quarters. `past` (0 or 1) is how
// far past its last texel the first quarter reads, as each sight's ROM does.
// PC: wider than 4:3 the frame's quarters stay at their shape and the
// picture's outer column goes on to the display's edges.
void drawMask(sceVif1Packet *packet, const Tim2Texture *texture, int past)
{
    const int spanU = textureSpan(texture->m_width);
    const int spanV = textureSpan(texture->m_height);
    const int lastU = spanU + texture->m_width - 1;
    const int lastV = spanV + texture->m_height - 1;
    const float firstU = (float)spanU;
    const float firstV = (float)spanV;
    const float farU = (float)(lastU + past);
    const float farV = (float)(lastV + past);
    // PC: the ROM ends these at span - 1, past the outer texel: the last two
    // pixels wrap to the picture's inner edge, a clear line at the screen's
    // edge there and in the middle of a wider display here.
    const float backU = firstU;
    const float backV = firstV;

    YamiPan_DrawUiTexture(packet, texture);
    YamiPan_DrawUiSqueeze(packet, 1);
    sightDrawMaskQuad(0.0f, 0.0f, kFrameMiddle, kFrameMiddle, firstU, firstV, farU, farV, packet);
    sightDrawMaskQuad(kFrameMiddle, 0.0f, kFrameEnd, kFrameMiddle, (float)lastU, firstV, backU, farV, packet);
    sightDrawMaskQuad(0.0f, kFrameMiddle, kFrameMiddle, kFrameEnd, firstU, (float)lastV, farU, backV, packet);
    sightDrawMaskQuad(kFrameMiddle, kFrameMiddle, kFrameEnd, kFrameEnd, (float)lastU, (float)lastV, backU, backV,
                      packet);

    const float wide = YamiPan_DrawUiSqueezeFactor();
    if (wide < 1.0f) {
        const float reach = kFrameMiddle / wide - kFrameMiddle;
        const float left = -reach;
        const float right = kFrameEnd + reach;
        maskColumn(packet, left, 0.0f, 0.0f, kFrameMiddle, firstU, firstV, firstU, farV);
        maskColumn(packet, kFrameEnd, 0.0f, right, kFrameMiddle, firstU, firstV, firstU, farV);
        maskColumn(packet, left, kFrameMiddle, 0.0f, kFrameEnd, firstU, (float)lastV, firstU, backV);
        maskColumn(packet, kFrameEnd, kFrameMiddle, right, kFrameEnd, firstU, (float)lastV, firstU, backV);
    }
}

// FS2 0x002ba310
// The whole frame lighter by grey at alpha, on any aspect: eight sprites 64
// wide in the ROM, one fill here.
// callcheck: gfxGetDrawEnv (PC: gsFillRect)

void sightBrighten(sceVif1Packet *packet, int alpha)
{
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kAlphaAdd);
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kTestAlways);
    gsFillRect(packet, 0, 0, kFrameSize, kFrameSize, (u_int)alpha << 24 | kRgbaGrey);
}

// FS2 0x002ba5d0
// callcheck: gfxGetDrawEnv, __fixunssfdi, __fixunssfdi (PC: YamiPan_DrawUiPrim / Vertex)

void sightDrawTicks(float zoom, sceVif1Packet *packet, int w, int h)
{
    u_char rgbaq[4];
    setRgba(rgbaq, kRgbaWhole);
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kAlphaBlend);
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kTestColourOnly);
    YamiPan_DrawUiPrim(packet, kPrimImage, rgbaq);
    YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_UV);

    const float out = 1.0f - zoom;
    const float step = kTickSpan / (out * out * kTickZoomScale + kTickZoomBase);
    for (int i = 0; i < kTickCount; i++) {
        const float x = (float)i * step + kTickStart;
        const int left = (int)(x * 16.0f + (float)kSightOrigin + 0.0f);
        const int right = (int)((((float)w + x) * 16.0f + (float)kSightOrigin + 0.0f) - (float)kSpriteEdge);
        putVertex(packet, kHalfTexel, kHalfTexel, left, kSightOrigin + kTickRow * kFixedPixel);
        putVertex(packet, w * kFixedPixel - 1, h * kFixedPixel + kHalfTexel, right,
                  kSightOrigin + (h + kTickRow) * kFixedPixel);
    }
}

// FS2 0x002bb020
// All of texture at (x, y), w x h, in the caller's blend and colour.
// callcheck: upload, putTex0, gfxGetDrawEnv (PC: YamiPan_DrawUiTexture, YamiPan_DrawUiPrim / Vertex)

void sightDrawImage(sceVif1Packet *packet, const Tim2Texture *texture, int x, int y, int w, int h, u_int rgba)
{
    u_char rgbaq[4];
    setRgba(rgbaq, rgba);
    YamiPan_DrawUiTexture(packet, texture);
    YamiPan_DrawUiPrim(packet, kPrimImage, rgbaq);
    YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_UV);
    putVertex(packet, kHalfTexel, kHalfTexel, kSightOrigin + x * kFixedPixel, kSightOrigin + y * kFixedPixel);
    putVertex(packet, texture->m_width * kFixedPixel - 1, texture->m_height * kFixedPixel - 1,
              kSightOrigin + (x + w) * kFixedPixel - kSpriteEdge, kSightOrigin + (y + h) * kFixedPixel - kSpriteEdge);
}

// The spot, added, centred on a frame whose 448 lines fill 512.
void drawSpot(sceVif1Packet *packet, const Tim2Texture *texture)
{
    const int w = texture->m_width;
    const int h = texture->m_height * kFrameSize / kFullFrameHeight;
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kAlphaAdd);
    sightDrawImage(packet, texture, (kFrameSize - w) / 2, (kFrameSize - h) / 2, w, h, kRgbaSpot);
}

// PC: the state the mask's picture needs.
void setSightSampling(sceVif1Packet *packet)
{
    YamiPan_DrawUiReg(packet, SCE_GS_CLAMP_1, kClampRepeat);
    YamiPan_DrawUiReg(packet, SCE_GS_TEX1_1, kTex1Linear);
}

} // namespace

// FS2 0x002b9ef0
// callcheck: openList, sightBrighten, gfxGetVramBlock, upload, putTex0, sightDrawMaskQuad, sightDrawMaskQuad, sightDrawMaskQuad, sightDrawMaskQuad, upload, putTex0, sightDrawTicks, sightDrawImage, sightDrawImage, sightDrawImage, closeList (drawMask and drawSpot hold four and one)

void MenuProgSet::drawR64C(float zoom)
{
    if (m_r64cMask == nullptr || m_tick == nullptr)
        return;
    GamePacketManager *packets = &gGameSystem->m_packets;
    sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
    setSightSampling(packet);
    sightBrighten(packet, kBrightenAlpha);
    drawMask(packet, m_r64cMask, 1);
    YamiPan_DrawUiTexture(packet, m_tick);
    YamiPan_DrawUiReg(packet, SCE_GS_TEX1_1, kTex1Nearest);
    sightDrawTicks(zoom, packet, m_tick->m_width, m_tick->m_height);
    YamiPan_DrawUiReg(packet, SCE_GS_TEX1_1, kTex1Linear);
    drawSpot(packet, m_spot);
    YamiPan_DrawUiReg(packet, SCE_GS_TEX1_1, kTex1Nearest);
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kAlphaOver);
    sightDrawImage(packet, m_lineAcross, 0, kLineAt, kFrameSize, kLineThick, kRgbaWhole);
    sightDrawImage(packet, m_lineDown, kLineAt, 0, kLineThick, kFrameSize, kRgbaWhole);
    YamiPan_DrawUiSqueeze(packet, 0);
    packets->closeList();
}

// FS2 0x002ba8d0
// callcheck: openList, gfxGetVramBlock, upload, putTex0, sightDrawMaskQuad, sightDrawMaskQuad, sightDrawMaskQuad, sightDrawMaskQuad, sightDrawImage, closeList (drawMask and drawSpot hold them)

void MenuProgSet::drawScope()
{
    if (m_scopeMask == nullptr)
        return;
    GamePacketManager *packets = &gGameSystem->m_packets;
    sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
    setSightSampling(packet);
    drawMask(packet, m_scopeMask, 0);
    drawSpot(packet, m_spot);
    YamiPan_DrawUiSqueeze(packet, 0);
    packets->closeList();
}

// FS2 0x002bb260
// callcheck: drawScope, drawR64C

void MenuProgSet::draw(float zoom)
{
    if (state == MenuProgSight_R64C)
        drawR64C(zoom);
    else if (state == MenuProgSight_Scope)
        drawScope();
}

// FS2 0x002bb2b0
// callcheck: operator_new, Tim2Texture, operator_new, Tim2Texture, operator_new, Tim2Texture, operator_new, Tim2Texture, operator_new, Tim2Texture, operator_new, Tim2Texture

void MenuProgSet::load(void *image0, void *image1, void *image2, void *image3, void *image4, void *image5)
{
    m_scopeMask  = new Tim2Texture(image0);
    m_spot       = new Tim2Texture(image1);
    m_r64cMask   = new Tim2Texture(image2);
    m_tick       = new Tim2Texture(image3);
    m_lineAcross = new Tim2Texture(image4);
    m_lineDown   = new Tim2Texture(image5);
}

// FS2 0x002bb3e0
// callcheck: operator_delete (the deleting variant, compiler generated)

MenuProgSet::~MenuProgSet()
{
    delete m_scopeMask;
    delete m_spot;
    delete m_r64cMask;
    delete m_tick;
    delete m_lineAcross;
    delete m_lineDown;
}

// FS2 0x002bb4e0

MenuProgSet::MenuProgSet() {}

// FS2 0x002bb510
// callcheck: vecCross, vecDot, vecNormalize, vecNormalize, vecNormalize

void quatBetween(sceVu0FVECTOR out, const sceVu0FVECTOR from, const sceVu0FVECTOR to, int unit)
{
    sceVu0FVECTOR a;
    sceVu0FVECTOR b;
    if (unit != 0) {
        for (int i = 0; i < 4; i++) {
            a[i] = from[i];
            b[i] = to[i];
        }
    } else {
        vecNormalize(a, from);
        vecNormalize(b, to);
    }

    sceVu0FVECTOR axis;
    vecCross(axis, a, b);
    const float cosine = vecDot(a, b);
    if (cosine == -1.0f) {
        vecNormalize(out, axis);
        out[3] = 0.0f;
    } else {
        // sqrt.s takes the absolute value: a cosine just past -1 is not -1.
        const float scale = sqrtf(fabsf((cosine + 1.0f) * 2.0f));
        out[0] = axis[0] / scale;
        out[1] = axis[1] / scale;
        out[2] = axis[2] / scale;
        out[3] = scale / 2.0f;
    }
}
