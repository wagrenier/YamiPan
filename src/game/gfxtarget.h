/* ==========================================================================
 *  game/gfxtarget.h
 *
 *  Display and render-target set-up (game/gfxtarget.cpp): the two display
 *  records 0x4ae280 (0x540 bytes each: 0 the game's 512x512 interlaced
 *  screen, 1 a 256x256 progressive one), each with its display registers,
 *  its draw env packet and the packet that copies the draw buffer to the
 *  display buffer; gfxFlipDisplay writes the current record to the GS when
 *  a frame is ready. gfxInitRenderTargets also makes the GS memory
 *  allocators (GsBlockMap and TexArea: game/gsblockmap.h; GsRegionAlloc: a
 *  stand-in here, its TU is not cut yet).
 * ======================================================================== */
#ifndef _GAME_GFXTARGET_H
#define _GAME_GFXTARGET_H

#include <stddef.h>

#include "game/gsblockmap.h"
#include "sdk/libgraph.h"
#include "sdk/scetypes.h"

// The display records: gfxSelectDisplay's index.
enum GfxDisplayIndex
{
    GfxDisplay_Interlaced512 = 0,   // 512x512, interlaced field mode (the game)
    GfxDisplay_Progressive256 = 1,  // 256x256, progressive frame mode (0x2c4320)
    GfxDisplay_Count
};

// A display record's draw env packet (+0x80): a PACKED A+D GIF tag of 14
// writes (draw env + clear) or 8 (draw env only: gfxSetDrawEnvClear), the
// context 1 registers in sceGsDrawEnv1's order (the UI presets and
// UiDrawer::resetScissor read FRAME_1 / ZBUF_1 / SCISSOR_1 from it), then
// the clear sprite. 0xf0 bytes.
struct GfxDrawEnv
{
    sceGifTag     tag;          // FS2 +0x00
    sceGsDrawEnv1 env;          // FS2 +0x10
    sceGsClear    clear;        // FS2 +0x90: its RGBAQ at +0xb0
};
static_assert(sizeof(GfxDrawEnv) == 0xf0, "GfxDrawEnv size");
static_assert(offsetof(GfxDrawEnv, env) == 0x10, "GfxDrawEnv::env");
static_assert(offsetof(GfxDrawEnv, env.scissor1) == 0x40, "GfxDrawEnv::env.scissor1");
static_assert(offsetof(GfxDrawEnv, clear) == 0x90, "GfxDrawEnv::clear");
static_assert(offsetof(GfxDrawEnv, clear.rgbaq) == 0xb0, "GfxDrawEnv::clear.rgbaq");

// One 64-texel column of the copy: a sprite from (u0, v0) / (x0, y0) to
// (u1, v1) / (x1, y1), or four NOP writes when unused.
struct GfxCopyStrip
{
    sceGifPackAd uv0;           // FS2 +0x00
    sceGifPackAd xyz0;          // FS2 +0x10
    sceGifPackAd uv1;           // FS2 +0x20
    sceGifPackAd xyz1;          // FS2 +0x30
};
static_assert(sizeof(GfxCopyStrip) == 0x40, "GfxCopyStrip size");

// Strips in a copy packet: room for 640 texels.
constexpr int kGfxCopyStripsMax = 10;

// The display record's copy packet (+0x180, gfxBuildCopyPacket): one
// PACKED A+D GIF tag of 57 writes that draws the draw buffer (a texture)
// into the display buffer with dithering, in vertical strips (0x3a
// qwords, gfxFlipDisplay sends it on DMA channel 2).
struct GfxCopyPacket
{
    sceGifTag     tag;          // FS2 +0x000
    sceGsDrawEnv1 env;          // FS2 +0x010: the display buffer as FRAME_1
    sceGsTexEnv   tex;          // FS2 +0x090: the draw buffer as TEX0_1
    sceGifPackAd  dimx;         // FS2 +0x0d0
    sceGifPackAd  texa;         // FS2 +0x0e0
    sceGifPackAd  prim;         // FS2 +0x0f0
    sceGifPackAd  rgbaq;        // FS2 +0x100
    sceGifPackAd  alpha;        // FS2 +0x110
    GfxCopyStrip  strips[kGfxCopyStripsMax]; // FS2 +0x120
};
static_assert(sizeof(GfxCopyPacket) == 0x3a0, "GfxCopyPacket size");
static_assert(offsetof(GfxCopyPacket, env) == 0x10, "GfxCopyPacket::env");
static_assert(offsetof(GfxCopyPacket, tex) == 0x90, "GfxCopyPacket::tex");
static_assert(offsetof(GfxCopyPacket, dimx) == 0xd0, "GfxCopyPacket::dimx");
static_assert(offsetof(GfxCopyPacket, alpha) == 0x110, "GfxCopyPacket::alpha");
static_assert(offsetof(GfxCopyPacket, strips) == 0x120, "GfxCopyPacket::strips");

// A display record (bss 0x4ae280, two of them).
struct GfxDisplay
{
    sceGsDispEnv  disp;         // FS2 +0x000: PMODE, SMODE2, DISPFB2, DISPLAY2, BGCOLOR
    tGS_DISPFB2   dispfb1;      // FS2 +0x028: DISPFB2 read one line lower (DBY 1)
    tGS_DISPLAY2  display1;     // FS2 +0x030: DISPLAY2 one line shorter
    short         width;        // FS2 +0x038: the display buffer's size
    short         height;       // FS2 +0x03a
    int           offsetX;      // FS2 +0x03c: gfxSetDisplayOffset's x / y
    int           offsetY;      // FS2 +0x040
    u_char        _044[0x3c];   // FS2 +0x044: unread
    GfxDrawEnv    drawEnv;      // FS2 +0x080
    u_char        _170[0x10];   // FS2 +0x170: unread
    GfxCopyPacket copy;         // FS2 +0x180
    u_char        _520[0x20];   // FS2 +0x520: unread
};
static_assert(sizeof(GfxDisplay) == 0x540, "GfxDisplay size");
static_assert(offsetof(GfxDisplay, dispfb1) == 0x28, "GfxDisplay::dispfb1");
static_assert(offsetof(GfxDisplay, display1) == 0x30, "GfxDisplay::display1");
static_assert(offsetof(GfxDisplay, width) == 0x38, "GfxDisplay::width");
static_assert(offsetof(GfxDisplay, offsetX) == 0x3c, "GfxDisplay::offsetX");
static_assert(offsetof(GfxDisplay, offsetY) == 0x40, "GfxDisplay::offsetY");
static_assert(offsetof(GfxDisplay, drawEnv) == 0x80, "GfxDisplay::drawEnv");
static_assert(offsetof(GfxDisplay, copy) == 0x180, "GfxDisplay::copy");

void gfxInitRenderTargets();                                             // FS2 0x0012e3f0
void gfxReleaseRenderTargets();                                          // FS2 0x0012e3a0

// ready: flip to the current display record (0 does nothing); kickPacket:
// send its copy packet; putDrawEnv: sceGsPutDrawEnv its draw env.
void gfxFlipDisplay(int ready, int kickPacket, int putDrawEnv);          // FS2 0x0012e0e0
// The display record the next ready frame switches to (GfxDisplayIndex).
void gfxSelectDisplay(int index);                                        // FS2 0x0012ea70

// The display record in use (index gp-0x7acc).
GfxDisplay *gfxGetCurrentDisplay();                                      // FS2 0x0012e050
// Moves the picture: stores x / y in the record and rewrites its DISPLAY1 /
// DISPLAY2 DX / DY from the video mode's origin.
void gfxSetDisplayOffset(GfxDisplay *display, short x, short y);         // FS2 0x0012ead0

// The current display record's draw env (gfxGetCurrentDisplay() + 0x80);
// gfxGetDrawEnv is the same function twice in the ROM.
GfxDrawEnv *gfxGetCurrentDrawEnv();                                      // FS2 0x0012e080
GfxDrawEnv *gfxGetDrawEnv();                                             // FS2 0x0012e0b0
// The current display record's copy packet.
GfxCopyPacket *gfxGetCurrentCopyPacket();                                // FS2 0x0012e020
// enable: the env packet clears the frame (GIF tag NLOOP 14, else 8) to
// r, g, b, a. gfxFlipDisplay turns the clear back on, black, every frame.
void gfxSetDrawEnvClear(GfxDrawEnv *env, int enable, u_char r, u_char g,
                        u_char b, u_char a);                             // FS2 0x0012ea80
// gfxSetDrawEnvClear on the current draw env.
void gfxSetClearColor(int enable, u_char r, u_char g, u_char b, u_char a); // FS2 0x0012dfa0
// Builds a display record's copy packet: display's buffer drawn from the
// draw env's FRAME_1; lineAdjust adds lines to the source height (0, or
// -32 for the 256x256 record).
void gfxBuildCopyPacket(GfxCopyPacket *packet, const GfxDisplay *display,
                        const GfxDrawEnv *source, int lineAdjust);       // FS2 0x0012ec10

// The VRAM areas of gfxGetVramBlock (table 0x2f3850: blocks 0, 0x800,
// 0x1800, 0x2800, 0x2800, 0x3700). 0..2 hold the View / Draw / Zbuf
// targets (game/gsutil.h GsTargetTag); 3..4 are GsRegionAlloc's (0x2800..
// 0x3700), 5 TexArea's (0x3700..0x3fbe).
enum GfxVramArea
{
    GfxVram_View    = 0,
    GfxVram_Draw    = 1,
    GfxVram_Zbuf    = 2,
    GfxVram_Texture = 3,        // the font printer's default texture block
};
// The GS block (64-word unit) where a VRAM area starts.
int gfxGetVramBlock(int area);                                           // FS2 0x0012f170

// The one TexArea (gp-0x7ad0).
TexArea *gfxGetTexArea();                                                // FS2 0x0012f160

// A region allocator over GS blocks [base, base + size) (EE 0x1838: base +0,
// size +4, node and region pools, the vptr +0x1834 = Vt_0031a7b0).
// layout: host -- not declared beyond the calls; its TU (0x2e1440.., unassigned) is
// not cut yet.
class GsRegionAlloc
{
public:
    GsRegionAlloc(int base, int size);                                   // FS2 0x002e14d0
    virtual ~GsRegionAlloc();                                            // FS2 0x002e1440: EE slot 0 (+0x8)
};

// A region GsRegionAlloc gave a texture (Tim2Texture::m_region): +0x8 its GS
// block, +0x12 its state (1 to send, 2 sent, 3 released), +0x14 its texture.
// layout: host -- only the call; same TU.
class GsRegion
{
public:
    void release();                                                      // FS2 0x002e0840
};

#endif /* _GAME_GFXTARGET_H */
