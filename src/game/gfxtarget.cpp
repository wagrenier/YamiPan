// game/gfxtarget.cpp -- the display records, the render targets and the
// display flip (game/gfxtarget.h).
//
// PC: there is no GS (no GS emulator). The GS register writes land in the
// host SDK's register variables (sdk/eeregs.h) and the GIF packets in its
// GIF channel, which drops them; the host renderer owns the window, its
// size and aspect, and presents its own frame. The records keep the
// game's values so that what reads them (the UI presets' FRAME_1 / ZBUF_1,
// the scissor, the clear colour) reads what the ROM would.

#include "game/gfxtarget.h"

#include <string.h>

#include "game/gamehooks.h"
#include "sdk/eekernel.h"
#include "sdk/eeregs.h"
#include "sdk/libdma.h"
#include "sdk/libgraph.h"
#include "sdk/scetypes.h"

// Each record's display buffer is PSMCT16 at block 0; its draw buffer and
// Z buffer the Draw / Zbuf VRAM areas (in 2048-word pages for FBP / ZBP).
constexpr int kDrawFrameFbp = 0x800 / 32;
constexpr int kDrawZbufZbp = 0x1800 / 32;

// PMODE: both read circuits blended with alpha 0x80 (the interlaced
// record's flicker filter: circuit 1 reads one line lower), or circuit 2
// alone; MMOD 1 (the ALP value), AMOD 1 on the progressive record.
constexpr u_long kPmodeInterlaced = 0x8023;     // EN1, EN2, MMOD, ALP 0x80
constexpr u_long kPmodeProgressive = 0x62;      // EN2, MMOD, AMOD

// The draw env packet: one PACKED A+D loop of the draw env (8) and the
// clear (6), or of the draw env alone.
constexpr int kDrawEnvWritesWithClear = 14;
constexpr int kDrawEnvWritesNoClear = 8;

// gfxSetDisplayOffset: the picture's origin per video mode (DISPLAY DX in
// VCK units, DY in lines) and the VCK width of a line.
constexpr int kDisplayDxPal = 0x290;
constexpr int kDisplayDyPal = 0x48;
constexpr int kDisplayDxNtsc = 0x27c;
constexpr int kDisplayDyNtsc = 0x32;
constexpr int kDisplayLineVck = 0xa00;

// The copy packet: 17 fixed writes (draw env 8, tex env 4, DIMX, TEXA,
// PRIM, RGBAQ, ALPHA), then four per strip.
constexpr int kCopyPacketWrites = 17 + 4 * kGfxCopyStripsMax;
constexpr int kCopyPacketQwords = 1 + kCopyPacketWrites;
static_assert(kCopyPacketQwords * 16 == sizeof(GfxCopyPacket), "copy packet qwords");
// A strip is 64 texels wide (12.4 fixed point: 0x400).
constexpr int kCopyStripTexels = 64;
constexpr u_int kCopyStripMaxU = kCopyStripTexels << 4;
// The dither matrix, rows DM0x..DM3x: {3, -1, 3, -1}, {0, -4, -1, -4},
// {2, -2, 2, -2}, {0, -3, 1, -3} (3-bit fields every 4 bits).
constexpr u_long kCopyDimx = 0x5150626247407373ull;
// TEXA: 16-bit texels with A 0 / 1 read as alpha 0x80.
constexpr u_long kCopyTexa = SCE_GS_SET_TEXA(0x80, 0, 0x80);
// A textured, blended sprite with UV coordinates.
constexpr u_long kCopyPrim = SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0);
constexpr u_long kCopyRgbaq = 0x80808080;       // white at 1.0, Q 0
// ALPHA: Cs * FIX (0x80 = 1.0) + 0: the source as it is.
constexpr u_long kCopyAlpha = SCE_GS_SET_ALPHA(0, 2, 2, 2, 0x80);
// What an unused strip writes to (no such GS register).
constexpr u_long kGsRegNop = 0x7f;

alignas(64) static GfxDisplay sDisplays[GfxDisplay_Count];  /* bss 0x4ae280 */
static GsRegionAlloc *sRegionAlloc;     /* sbss 0x31ad98 (gp-0x7ad8) */
static GsBlockMap    *sBlockMap;        /* sbss 0x31ad9c (gp-0x7ad4) */
static TexArea       *sTexArea;         /* sbss 0x31ada0 (gp-0x7ad0) */
static int            sCurrentDisplay;  /* sbss 0x31ada4 (gp-0x7acc) */
static int            sRequestedDisplay;/* sbss 0x31ada8 (gp-0x7ac8) */

// ──────────────────────────────────────────────────────────────────────
// Helpers, inline in the ROM.

// The bits needed for n - 1: log2 of n rounded up (TEX0 TW / TH).
static int ceilLog2(u_int n)
{
    int bits = 0;
    for (u_int v = n - 1; v != 0; v >>= 1)
        bits++;
    return bits;
}

// One display record: the display registers, the draw env (Draw / Zbuf
// buffers, size x size, clearing from (clearOrigin, clearOrigin)) and the
// copy packet. The ROM writes this out twice in gfxInitRenderTargets.
static void setupDisplay(GfxDisplay *display, short size, u_long pmode, short clearOrigin,
                         int lineAdjust)
{
    sceGsSetDefDispEnv(&display->disp, SCE_GS_PSMCT16, size, size, 0, 0);
    display->width = size;
    display->height = size;
    display->offsetX = 0;
    display->offsetY = 0;
    *(u_long *)&display->disp.pmode = pmode;
    display->disp.dispfb.FBP = 0;
    display->display1 = display->disp.display;
    display->dispfb1 = display->disp.dispfb;
    display->dispfb1.DBY = 1;
    display->display1.DH = display->disp.display.DH - 1;

    GfxDrawEnv *env = &display->drawEnv;
    memset(env, 0, sizeof *env);
    sceGsSetDefDrawEnv(&env->env, SCE_GS_PSMCT32, size, size, SCE_GS_ZGEQUAL, SCE_GS_PSMZ32);
    env->tag.NLOOP = kDrawEnvWritesWithClear;
    env->tag.EOP = 1;
    env->tag.NREG = 1;
    env->tag.REGS0 = SCE_GIF_PACKED_AD;
    sceGsSetDefClear(&env->clear, SCE_GS_ZALWAYS, clearOrigin, clearOrigin, size, size,
                     0, 0, 0, 0, 0);
    env->env.frame1.FBP = kDrawFrameFbp;
    env->env.zbuf1.ZBP = kDrawZbufZbp;
    gfxBuildCopyPacket(&display->copy, display, env, lineAdjust);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0012dfa0
// callcheck: gfxSetDrawEnvClear (inline in the ROM)

void gfxSetClearColor(int enable, u_char r, u_char g, u_char b, u_char a)
{
    gfxSetDrawEnvClear(&sDisplays[sCurrentDisplay].drawEnv, enable, r, g, b, a);
}

// FS2 0x0012e020

GfxCopyPacket *gfxGetCurrentCopyPacket()
{
    return &sDisplays[sCurrentDisplay].copy;
}

// FS2 0x0012e050

GfxDisplay *gfxGetCurrentDisplay()
{
    return &sDisplays[sCurrentDisplay];
}

// FS2 0x0012e080

GfxDrawEnv *gfxGetCurrentDrawEnv()
{
    return &sDisplays[sCurrentDisplay].drawEnv;
}

// FS2 0x0012e0b0

GfxDrawEnv *gfxGetDrawEnv()
{
    return &sDisplays[sCurrentDisplay].drawEnv;
}

// ──────────────────────────────────────────────────────────────────────
// The vsync callback's flip (gameFrameReady): on a ready frame, switch the
// video mode if another record was selected, write the record's display
// registers, send its copy packet and draw env as asked, empty the texture
// lists and turn the frame clear back on, black.
// PC: the registers and packets go to the host SDK (see the top); the
// host renderer presents.
// FS2 0x0012e0e0
// callcheck: gfxSetDrawEnvClear, sceGsSyncVCallback, sceGsResetGraph (the clear inline in the ROM; the mode switch written once per branch)

void gfxFlipDisplay(int ready, int kickPacket, int putDrawEnv)
{
    if (!ready)
        return;

    FlushCache(WRITEBACK_DCACHE);
    if (sCurrentDisplay != sRequestedDisplay) {
        sCurrentDisplay = sRequestedDisplay;
        if (sRequestedDisplay == GfxDisplay_Progressive256) {
            sceGsVCallbackFunc saved = sceGsSyncVCallback(nullptr);
            sceGsResetGraph(0, SCE_GS_NOINTERLACE, SCE_GS_PAL, SCE_GS_FRAME);
            sceGsSyncVCallback(saved);
        } else {
            sceGsVCallbackFunc saved = sceGsSyncVCallback(nullptr);
            sceGsResetGraph(0, SCE_GS_INTERLACE, SCE_GS_PAL, SCE_GS_FIELD);
            sceGsSyncVCallback(saved);
        }
    }

    GfxDisplay *display = &sDisplays[sCurrentDisplay];
    REG_GS_PMODE = *(u_long *)&display->disp.pmode;
    REG_GS_SMODE2 = *(u_long *)&display->disp.smode2;
    REG_GS_DISPFB2 = *(u_long *)&display->disp.dispfb;
    REG_GS_DISPFB1 = *(u_long *)&display->dispfb1;
    REG_GS_DISPLAY2 = *(u_long *)&display->disp.display;
    REG_GS_DISPLAY1 = *(u_long *)&display->display1;
    REG_GS_BGCOLOR = *(u_long *)&display->disp.bgcolor;

    if (kickPacket == 1) {
        sceDmaSync(sceDmaGetChan(SCE_DMA_GIF), 0, 1000);
        sceDmaSendN(sceDmaGetChan(SCE_DMA_GIF), &display->copy, kCopyPacketQwords);
        sceGsSyncPath(0, 0);
    }
    if (putDrawEnv == 1) {
        sceGsPutDrawEnv(&sDisplays[sCurrentDisplay].drawEnv.tag);
        sceGsSyncPath(0, 0);
    }

    sTexArea->resetLists();
    gfxSetDrawEnvClear(&sDisplays[sCurrentDisplay].drawEnv, 1, 0, 0, 0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// main's teardown. The TexArea is never deleted.
// FS2 0x0012e3a0
// callcheck: ~GsBlockMap (delete sBlockMap; the region allocator's slot 0 through delete)

void gfxReleaseRenderTargets()
{
    if (sRegionAlloc != nullptr)
        delete sRegionAlloc;
    delete sBlockMap;
    sceGsSyncPath(0, 0);
}

// ──────────────────────────────────────────────────────────────────────
// gameInit: both display records (the 256x256 one first, leaving the GS in
// the 512x512 interlaced mode), the GS block map with the render areas in
// use, the texture lists, the region allocator for the texture area and
// the draw system on it. Display 0 is current.
// FS2 0x0012e3f0
// callcheck: sceGsResetGraph, sceGsResetPath, sceGsSetDefDispEnv, sceGsSetDefDrawEnv, sceGsSetDefClear, gfxBuildCopyPacket, operator_new, setupDisplay, mark (setupDisplay is the record set-up the ROM writes out twice; new for the three allocators; mark through GsBlockMap's slot 0)

void gfxInitRenderTargets()
{
    sceGsResetGraph(0, SCE_GS_NOINTERLACE, SCE_GS_PAL, SCE_GS_FRAME);
    sceGsResetPath();
    // 32 lines fewer of the source: the 256 record's draw buffer is read
    // only down to line 224.
    setupDisplay(&sDisplays[GfxDisplay_Progressive256], 256, kPmodeProgressive, 0x780, -32);

    sceGsResetGraph(0, SCE_GS_INTERLACE, SCE_GS_PAL, SCE_GS_FIELD);
    sceGsResetPath();
    setupDisplay(&sDisplays[GfxDisplay_Interlaced512], 512, kPmodeInterlaced, 0x700, 0);

    sBlockMap = new GsBlockMap(0, 0x4000);
    sBlockMap->mark(0, 0x800, 1);               // View
    sBlockMap->mark(0x800, 0x1800, 1);          // Draw
    sBlockMap->mark(0x1800, 0x2800, 1);         // Zbuf
    sBlockMap->mark(0x2800, 0x2800, 1);         // (empty)
    sBlockMap->mark(0x2800, 0x3700, 1);         // GsRegionAlloc's
    sTexArea = new TexArea(0x3700, 0x3fbe, 14, 0x20);
    sRegionAlloc = new GsRegionAlloc(0x2800, 0xf00);
    drawSysInit(0x700, 0x700, 512, 512, sRegionAlloc);
    sCurrentDisplay = GfxDisplay_Interlaced512;
    FlushCache(WRITEBACK_DCACHE);
}

// ──────────────────────────────────────────────────────────────────────
// The inline virtual, out of line here.
// FS2 0x0012ea60

void GsBlockMap::mark(u_int lo, u_int hi, int used)
{
    setRange(lo, hi, used);
}

// FS2 0x0012ea70

void gfxSelectDisplay(int index)
{
    sRequestedDisplay = index;
}

// FS2 0x0012ea80

void gfxSetDrawEnvClear(GfxDrawEnv *env, int enable, u_char r, u_char g, u_char b, u_char a)
{
    env->tag.NLOOP = enable ? kDrawEnvWritesWithClear : kDrawEnvWritesNoClear;
    env->clear.rgbaq.R = r;
    env->clear.rgbaq.G = g;
    env->clear.rgbaq.B = b;
    env->clear.rgbaq.A = a;
}

// ──────────────────────────────────────────────────────────────────────
// The options' screen position: x in pixels (scaled to VCKs by the
// display width), y in lines, from the PAL or NTSC origin.
// PC: DISPLAY DX / DY mean nothing to the host renderer (the picture
// fills the window); it can read offsetX / offsetY if a nudge is wanted.
// FS2 0x0012ead0

void gfxSetDisplayOffset(GfxDisplay *display, short x, short y)
{
    display->offsetX = x;
    display->offsetY = y;

    short dy;
    u_short dx;
    short vckPerPixel = (short)(kDisplayLineVck / display->width);
    if (sceGsGetGParam()->sceGsOutMode == SCE_GS_PAL) {
        dy = y + kDisplayDyPal;
        dx = x * vckPerPixel + kDisplayDxPal;
    } else {
        dy = y + kDisplayDyNtsc;
        dx = x * vckPerPixel + kDisplayDxNtsc;
    }
    display->disp.display.DX = dx;
    display->disp.display.DY = dy;
    display->display1.DX = dx;
    display->display1.DY = dy;
}

// ──────────────────────────────────────────────────────────────────────
// The copy packet: draw into the display buffer (its FBP / PSM, the
// display size, no Z, dithering on) from the draw buffer as a texture (the
// source's FRAME_1, its size from SCISSOR_1, RGB only, bilinear), as
// 64-texel sprites spread over the display width, half-texel inset; the
// rest of the strips write nothing.
// FS2 0x0012ec10
// callcheck: ceilLog2, memset (inline in the ROM: plzcw and a byte loop)

void gfxBuildCopyPacket(GfxCopyPacket *packet, const GfxDisplay *display,
                        const GfxDrawEnv *source, int lineAdjust)
{
    memset(packet, 0, sizeof *packet);
    packet->tag.NLOOP = kCopyPacketWrites;
    packet->tag.EOP = 1;
    packet->tag.NREG = 1;
    packet->tag.FLG = SCE_GIF_PACKED;
    packet->tag.REGS0 = SCE_GIF_PACKED_AD;

    u_int displayFbp = display->disp.dispfb.FBP;
    sceGsSetDefDrawEnv(&packet->env, display->disp.dispfb.PSM, display->width,
                       display->height, SCE_GS_ZNOUSE, 0);
    packet->env.frame1.FBP = displayFbp;
    packet->env.dthe.DTHE = 1;
    packet->env.xyoffset1.OFX = 0;
    packet->env.xyoffset1.OFY = 0;

    const sceGsDrawEnv1 *src = &source->env;
    sceGsSetDefTexEnv(&packet->tex, 1, src->frame1.FBP * 32, src->frame1.FBW, src->frame1.PSM,
                      ceilLog2(src->scissor1.SCAX1 + 1), ceilLog2(src->scissor1.SCAY1 + 1),
                      0, 0, 0, 0, 1);
    packet->tex.tex0.TCC = 0;

    packet->dimx.DATA = kCopyDimx;
    packet->dimx.ADDR = SCE_GS_DIMX;
    packet->texa.DATA = kCopyTexa;
    packet->texa.ADDR = SCE_GS_TEXA;
    packet->prim.DATA = kCopyPrim;
    packet->prim.ADDR = SCE_GS_PRIM;
    packet->rgbaq.DATA = kCopyRgbaq;
    packet->rgbaq.ADDR = SCE_GS_RGBAQ;
    packet->alpha.DATA = kCopyAlpha;
    packet->alpha.ADDR = SCE_GS_ALPHA_1;

    u_int lastColumn = src->scissor1.SCAX1;
    int sourceLines = (int)src->scissor1.SCAY1 + 1 + lineAdjust;
    u_int strips = (lastColumn + kCopyStripTexels) / kCopyStripTexels;
    int stepX = (display->width << 4) / (int)strips;
    int x = 0;
    for (u_int i = 0; i < strips; i++) {
        u_int stripU = (lastColumn + 1) * 16 - i * kCopyStripMaxU;
        if (stripU > kCopyStripMaxU)
            stripU = kCopyStripMaxU;
        GfxCopyStrip *strip = &packet->strips[i];
        strip->uv0.DATA = (u_long)(int)(stripU * i + 8) | (8ull << 16);
        strip->uv0.ADDR = SCE_GS_UV;
        strip->xyz0.DATA = (u_long)x;
        strip->xyz0.ADDR = SCE_GS_XYZ2;
        x += stepX;
        strip->uv1.DATA = (u_long)(int)(stripU * (i + 1) - 7)
                        | (u_long)(sourceLines * 16 - 7) << 16;
        strip->uv1.ADDR = SCE_GS_UV;
        strip->xyz1.DATA = (u_long)(x - 0xf) | (u_long)(display->height << 4) << 16;
        strip->xyz1.ADDR = SCE_GS_XYZ2;
    }
    for (u_int i = strips; i < (u_int)kGfxCopyStripsMax; i++) {
        GfxCopyStrip *strip = &packet->strips[i];
        strip->uv0.DATA = 0;
        strip->xyz0.DATA = 0;
        strip->uv1.DATA = 0;
        strip->xyz1.DATA = 0;
        strip->uv0.ADDR = kGsRegNop;
        strip->xyz0.ADDR = kGsRegNop;
        strip->uv1.ADDR = kGsRegNop;
        strip->xyz1.ADDR = kGsRegNop;
    }
    FlushCache(WRITEBACK_DCACHE);
}

// FS2 0x0012f160

TexArea *gfxGetTexArea()
{
    return sTexArea;
}

// FS2 0x0012f170

int gfxGetVramBlock(int area)
{
    static const int kVramBlocks[] = {          // 0x2f3850
        0, 0x800, 0x1800, 0x2800, 0x2800, 0x3700,
    };
    return kVramBlocks[area];
}
