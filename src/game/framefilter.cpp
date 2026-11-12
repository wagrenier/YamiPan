/* ==========================================================================
 *  game/framefilter.cpp
 *
 *  The frame filter: the frame shrunk, blurred and grown back. framefilter.h
 *  has the idea.
 *
 *  The ROM packs the GS registers of every step (FRAME_1 the buffer drawn,
 *  TEX0_1 the buffer read, SCISSOR_1, CLAMP_1, TEX1_1, ALPHA_1 0x44, TEST_1)
 *  and cuts each sprite into strips one page wide (gsPageWidth). Here every
 *  decision stays -- how many halves, which buffer, when to blur, each tap's
 *  shift and alpha, the order -- and each sprite is a step of the host's
 *  frame filter (yamipan_drawui.h), which keeps the VRAM blocks as its own
 *  scratch textures. No GS memory.
 * ======================================================================== */
#include "game/framefilter.h"

#include "yamipan/rendering/yamipan_drawui.h"

namespace {

// Debug switches no code writes, both 1 in the ROM's data: blur on the way
// down, and on the way up.
u_char gFrameFilterBlurDown = 1;    /* data 0x31ac10 (gp-0x7c60) */
u_char gFrameFilterBlurUp   = 1;    /* data 0x31ac14 (gp-0x7c5c) */

// Sizes past this are not blurred (the first half of a 512 frame is).
constexpr u_int kBlurMaxSize = 0x100;

// A tap's alpha, ALPHA_1 0x44: (Cs - Cd) * As / 128 + Cd. The first
// replaces, the second takes half, the third a third: the mean of three
// pixels, near enough (0x2a / 0x80 is 0.328).
constexpr int kTapFirst  = 0x80;
constexpr int kTapSecond = 0x40;
constexpr int kTapThird  = 0x2a;

// The way the down blur takes its second tap; the up blur takes it the other way.
constexpr int kDownSide = -1;
constexpr int kUpSide   = 1;

constexpr int kPageWidthByte = 0x80;
constexpr int kPageWidthWord = 0x40;

void addStep(sceVif1Packet *packet, int source, int target, int x, int y, int w, int h,
             int sourceW, int sourceH, int alpha)
{
    YamiPanFrameFilterStep step;
    step.source = source;
    step.target = target;
    step.x = x;
    step.y = y;
    step.width = w;
    step.height = h;
    step.sourceWidth = sourceW;
    step.sourceHeight = sourceH;
    step.alpha = alpha;
    YamiPan_DrawUiFrameFilterStep(packet, &step);
}

} // namespace

// FS2 0x002866f0
// callcheck: (none)

int gsPageWidth(int psm)
{
    switch (psm) {
    case SCE_GS_PSMT4:
    case SCE_GS_PSMT8:
        return kPageWidthByte;
    case SCE_GS_PSMZ16S:
    case SCE_GS_PSMZ16:
    case SCE_GS_PSMCT16S:
    case SCE_GS_PSMCT16:
    case SCE_GS_PSMT4HH:
    case SCE_GS_PSMT4HL:
    case SCE_GS_PSMT8H:
    case SCE_GS_PSMZ24:
    case SCE_GS_PSMZ32:
    case SCE_GS_PSMCT24:
    case SCE_GS_PSMCT32:
        return kPageWidthWord;
    default:
        return 0;
    }
}

// FS2 0x00285450
// One tap of the box blur: the whole w x h of the buffer the caller's TEX0_1
// names, drawn shifted by (dx, dy) pixels over the buffer its FRAME_1 names,
// blended by alpha (the vertex's RGBAQ, grey 0x808080). The ROM sets the
// sprite's XY shifted, its UV from the half texel, in page-wide strips of
// psm's page.
// PC: the ROM takes psm and leaves the buffers to the caller's registers;
// the host's step needs them, so the caller passes the two blocks.
// callcheck: gsPageWidth (the strips: one sprite on the host)

static void gsFrameTap(sceVif1Packet *packet, int target, int source, int dx, int dy,
                       u_int w, u_int h, int alpha)
{
    addStep(packet, source, target, dx, dy, (int)w, (int)h, (int)w, (int)h, alpha);
}

// FS2 0x00286070
// The w x h picture in block source down to w/2 x h/2 at target, not
// blended: TEX0_1 the source with target's width and PSM, FRAME_1 target,
// SCISSOR_1 the half size, page-wide strips whose UV runs from texel 1,
// bilinear by the caller's TEX1_1 0x60, so each pixel is the mean of four.
// callcheck: gsPageWidth (the strips: one sprite on the host)

static void gsFrameHalve(sceVif1Packet *packet, const sceGsFrame *target, u_int w, u_int h, int source)
{
    addStep(packet, source, (int)target->FBP, 0, 0, (int)(w >> 1), (int)(h >> 1), (int)w, (int)h,
            kTapFirst);
}

// FS2 0x002859b0
// The w x h picture in block source up to 2w x 2h at target, not blended,
// bilinear: as gsFrameHalve, the other way (SCISSOR_1 and CLAMP_1 twice the
// size).
// callcheck: gsPageWidth (the strips: one sprite on the host)

static void gsFrameDouble(sceVif1Packet *packet, const sceGsFrame *target, u_int w, u_int h, int source)
{
    addStep(packet, source, (int)target->FBP, 0, 0, (int)(w * 2), (int)(h * 2), (int)w, (int)h,
            kTapFirst);
}

// arg passes of the 3-tap box blur at w x h: along x from source into
// target, then along y back into source, so the picture stays in source.
// side is where the second tap comes from, the third from the other side.
// The ROM sets SCISSOR_1 and CLAMP_1 (REGION_CLAMP) to w x h first.
// PC: the ROM writes this body out twice in gsFrameFilter, once per side.

static void gsFrameBlur(sceVif1Packet *packet, sceGsFrame *target, sceGsFrame *source,
                        u_int w, u_int h, int arg, int side)
{
    for (int i = 0; i < arg; i++) {
        gsFrameTap(packet, (int)target->FBP, (int)source->FBP, 0, 0, w, h, kTapFirst);
        gsFrameTap(packet, (int)target->FBP, (int)source->FBP, side, 0, w, h, kTapSecond);
        gsFrameTap(packet, (int)target->FBP, (int)source->FBP, -side, 0, w, h, kTapThird);

        gsFrameTap(packet, (int)source->FBP, (int)target->FBP, 0, 0, w, h, kTapFirst);
        gsFrameTap(packet, (int)source->FBP, (int)target->FBP, 0, side, w, h, kTapSecond);
        gsFrameTap(packet, (int)source->FBP, (int)target->FBP, 0, -side, w, h, kTapThird);
    }
}

// FS2 0x002849c0
// passes halves of the w x h frame of env, ping-ponging between two scratch
// buffers (block, and block past the first's half width of pages), each size
// up to 256 blurred arg times; then passes - 1 doubles back up, blurred the
// same, stopping short of half the frame; the last double lands in the frame
// itself. TEST_1 0x33001 and ALPHA_1 0x44 first; CLAMP_1 0, TEST_1 0x50000
// and the frame's FRAME_1 back last.
// callcheck: gsPageWidth, gsFrameHalve, gsFrameTap x6, gsFrameDouble, gsFrameTap x6, gsFrameDouble (the blur's taps: gsFrameBlur)

void gsFrameFilter(sceVif1Packet *packet, const sceGsDrawEnv1 *env, int w, int h,
                   u_short block, int passes, int arg)
{
    if (passes <= 0)
        return;

    const sceGsFrame &frame = env->frame1;
    const int pageWidth = gsPageWidth((int)frame.PSM);
    sceGsFrame target = frame;      // the buffer drawn
    target.FBP = block;
    sceGsFrame source = frame;      // the buffer holding the picture

    YamiPan_DrawUiFrameFilterBegin(packet, (int)frame.FBP);

    // TEX1_1 0x60: bilinear while shrinking.
    u_int sizeW = (u_int)w;
    u_int sizeH = (u_int)h;
    for (int level = 0; level < passes; level++) {
        const u_int fromW = sizeW;
        const u_int fromH = sizeH;
        gsFrameHalve(packet, &target, fromW, fromH, (int)source.FBP);

        // The frame is never drawn into on the way down: the second scratch
        // buffer stands in for it.
        u_int next = source.FBP;
        if (source.FBP == frame.FBP) {
            // PC: a PSM with no page width would divide by zero; FS2's frame is never one.
            const int pages = pageWidth != 0 ? (int)((u_int)w >> 1) / pageWidth : 0;
            next = (u_int)(block + pages);
        }
        sizeW = fromW >> 1;
        sizeH = fromH >> 1;
        source.FBP = target.FBP;
        target.FBP = next;

        if (((fromW & fromH) >> 1) & 1)
            break;      // a size of 2 mod 4 halves no further
        if (gFrameFilterBlurDown && sizeW <= kBlurMaxSize) {
            // TEX1_1 0: nearest for the taps.
            gsFrameBlur(packet, &target, &source, sizeW, sizeH, arg, kDownSide);
        }
    }

    // TEX1_1 0x60 again; the up blur keeps it.
    for (int level = 0; level < passes - 1; level++) {
        gsFrameDouble(packet, &target, sizeW, sizeH, (int)source.FBP);
        sizeW *= 2;
        sizeH *= 2;
        const u_int drawn = target.FBP;
        target.FBP = source.FBP;
        source.FBP = drawn;

        if ((u_int)w >> 1 <= sizeW)
            break;
        if (gFrameFilterBlurUp && sizeW <= kBlurMaxSize)
            gsFrameBlur(packet, &target, &source, sizeW, sizeH, arg, kUpSide);
    }

    gsFrameDouble(packet, &frame, (u_int)w >> 1, (u_int)h >> 1, (int)source.FBP);
}
