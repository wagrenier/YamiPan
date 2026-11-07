/* ==========================================================================
 *  yamipan_drawui.h
 *
 *  The native draw module of the 2D layouts (architecture.md "PC platform
 *  layer"). game/uidrawer.cpp keeps every decision FS2's UiDrawer makes --
 *  primitive, colours, texture, blend, wrap, scissor, presets, order -- and
 *  hands them here as the GS register values it would have packed, instead
 *  of writing GIF packets. No GS emulator: the module turns the values into
 *  host draws; textures arrive as their owner (the game's GsTexture) and
 *  are decoded at their source to GPU textures, never through GS memory.
 *
 *  `list` is the packet the game drew into (one per packet list): the list
 *  id is the layer, drawn in id order (game/gamepacket.h). Coordinates are
 *  the GS window's (12.4 fixed point, 0x8000 = 2048.0, the draw env's
 *  centre): the module maps them to its frame from the renderer-owned
 *  display size and aspect, never a fixed 4:3.
 *
 *  Declarations only until bootstrap 6 (the platform layer) implements them.
 * ======================================================================== */
#ifndef YAMIPAN_DRAWUI_H
#define YAMIPAN_DRAWUI_H

#include <stdint.h>

/* One vertex as the GS would get it: RGBAQ bytes (alpha 0x80 opaque), UV
 * in texels times 16, XY in window coordinates times 16, Z the register's
 * 32 bits (FS2 always 0xff0fffff). Only what the open format names is
 * meaningful. */
struct YamiPanUiVertex
{
    uint8_t  rgba[4];
    uint16_t u, v;
    uint16_t x, y;
    uint32_t z;
};

/* The vertex formats (UiVertexFormat): bit 0 RGBAQ, bit 1 UV, bit 2 in
 * groups of three (only every third vertex kicks). */
enum
{
    YAMIPAN_UIV_RGBAQ   = 1,
    YAMIPAN_UIV_UV      = 2,
    YAMIPAN_UIV_GROUPED = 4,
};

/* One A+D register write (ALPHA_1, CLAMP_1, TEX1_1, SCISSOR_1, FRAME_1,
 * ZBUF_1, and the BITBLTBUF / TRXPOS / TRXREG / TRXDIR set of a local copy,
 * which the module performs between its render targets on TRXDIR). */
void YamiPan_DrawUiReg(const void *list, int reg, uint64_t value);

/* PRIM, and with `rgbaq` non-null the colour every following vertex
 * shares (the flat draws). */
void YamiPan_DrawUiPrim(const void *list, uint64_t prim, const uint8_t *rgbaq);

/* Binds the texture of `owner` (a GsTexture; null keeps the bound one). */
void YamiPan_DrawUiTexture(const void *list, const void *owner);

/* Non-zero: the vertices that follow are pulled towards the frame's
 * centre, by as much as the display is wider than the 4:3 set the 2D was
 * composed for, so that what is drawn centred in raw window coordinates
 * (text, its strip, a prompt) keeps its shape at any aspect. Zero ends it.
 * The factor is 1 at 4:3 and on anything narrower. */
void YamiPan_DrawUiSqueeze(const void *list, int on);

/* What YamiPan_DrawUiSqueeze pulls by: 1 at 4:3 and narrower, under 1 wider.
 * For a shape placed anywhere over the frame: its own width times this keeps
 * its shape where its place still spreads over the whole display. */
float YamiPan_DrawUiSqueezeFactor(void);

/* Passes over the whole frame so far, at any aspect: the colour buffer
 * times `factor`, 0..4 (the GS's A = Cd, C = FIX), and pushed away from a
 * colour, (Cd - Cs) * As + Cd (ALPHA_1 0x41, `rgba` as RGBAQ, As on the
 * 0x80 scale). Neither touches the frame's alpha. */
void YamiPan_DrawUiFrameScale(const void *list, float factor);
void YamiPan_DrawUiFrameContrast(const void *list, uint32_t rgba);

/* The whole frame so far through a ramp, at any aspect: halved, each pixel's
 * brightness picks one of the 256 colours of `ramp`, and that is stretched
 * back with no blend. `ramp` is RGBA words in a GS CLUT's order (PSMCT32,
 * CSM1: 8..f and 0x10..0x17 of each 0x20 swapped), the game's own table: the
 * module keeps a picture of it by its address. Leaves the frame's alpha. */
void YamiPan_DrawUiFramePalette(const void *list, const uint32_t *ramp);

/* The sight-jack's film over the frame so far, at any aspect: the frame's
 * four corners each `shown` (a Tim2Texture) with `leaving` over it at
 * `leavingAlpha`, the right and bottom ones mirrored about the frame's centre.
 * The green of that, 0..255, is how much of `color` (RGB) lies on the frame:
 * none below `clearBelow`, all above `solidAbove`, between them
 * (green - bandStart) / bandLength, in 128ths. */
typedef struct YamiPanFilmCorner
{
    const void *shown;
    const void *leaving;
    float leavingAlpha;
} YamiPanFilmCorner;

typedef struct YamiPanFilm
{
    YamiPanFilmCorner corners[4];
    int bandStart;
    int bandLength;
    int clearBelow;
    int solidAbove;
    uint32_t color;
} YamiPanFilm;

void YamiPan_DrawUiFilm(const void *list, const YamiPanFilm *film);

/* The frame so far seen through moving water, at any aspect (game/ripplesheet.cpp).
 * `points` are `columns` x `rows` RippleSheet points, row by row, inside a
 * sheet of (columns + 1) x (rows + 1) cells laid over the 512 x 512 frame. Each
 * bends the frame by x, y in 1/16 of a frame unit; the sheet's edge stays. Then
 * the frame is pushed away from `tint`, as YamiPan_DrawUiFrameContrast, and
 * where the height at a pixel, the low byte of `height` between the points
 * (0x80 at the edge), is 0x80 or over, it is scaled by height / 128 twice,
 * clamping each time. Leaves the frame's alpha, where the GS leaves the
 * height. */
typedef struct YamiPanRipplePoint
{
    int x;
    int y;
    int height;
} YamiPanRipplePoint;

typedef struct YamiPanRipple
{
    const YamiPanRipplePoint *points;
    int columns;
    int rows;
    uint32_t tint;
} YamiPanRipple;

void YamiPan_DrawUiFrameRipple(const void *list, const YamiPanRipple *ripple);

/* The frame filter's sprites (game/framefilter.cpp): buffers are the game's
 * VRAM blocks, `frameBlock` the frame so far, any other a scratch buffer the
 * host keeps at the frame's render size, no GS memory. A step draws the
 * source's top-left sourceWidth x sourceHeight at (x, y) as width x height
 * into the target, inside 0..width x 0..height, bilinear, in pixels of the
 * 512 x 512 frame; alpha 0x80 replaces, below it blends (Cs - Cd) * alpha /
 * 128 + Cd, as ALPHA_1 0x44. Begin opens the filter, each step adds to it. */
typedef struct YamiPanFrameFilterStep
{
    int source;
    int target;
    int x, y;
    int width, height;
    int sourceWidth, sourceHeight;
    int alpha;
} YamiPanFrameFilterStep;

void YamiPan_DrawUiFrameFilterBegin(const void *list, int frameBlock);
void YamiPan_DrawUiFrameFilterStep(const void *list, const YamiPanFrameFilterStep *step);

/* Starts a vertex run of `format`; YamiPan_DrawUiVertex adds to it. */
void YamiPan_DrawUiBegin(const void *list, int format);
void YamiPan_DrawUiVertex(const void *list, const YamiPanUiVertex *vertex);

/* A movie picture (game/movieplayer.cpp): `macroblocks` are the decoded
 * 16 x 16 RGBA32 blocks (0x400 bytes each, alpha 0x80 opaque), `columns`
 * x `rows` of them in column-major order, as the game would have sent
 * them by image transfer into the buffer at `tbp` (`tbw` pages of 64
 * wide) at (x, y) in pixels. The module keeps them as the texture of
 * `owner` (the MoviePlayer), updated each call, and draws them where the
 * transfer would have put them: decoded at the source, never through GS
 * memory. */
void YamiPan_DrawUiMovie(const void *list, const void *owner, const void *macroblocks,
                         int columns, int rows, int tbp, int tbw, int x, int y);

/* The end of a frame: the packet lists that hold draws, in chain order
 * (= the layer order), as Vif1PktManage::Kick would send them on VIF1.
 * The module draws those layers in this order. */
void YamiPan_DrawUiKick(const void *const *lists, int count);

#endif /* YAMIPAN_DRAWUI_H */
