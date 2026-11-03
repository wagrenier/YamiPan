/* ==========================================================================
 *  game/uidrawer.h
 *
 *  The 2D draw helper a UiLayout draws its sequences through
 *  (UiLayout::m_drawer, 0xe0 bytes) and its 2D affine matrix
 *  (game/uidrawer.cpp, 0x24dfa0..0x252840). The names are the port's (no
 *  source string).
 *
 *  Per actor, UiSequence::draw loads the drawer's matrix-stack top, its
 *  colour and offset colour, then lets each playing track draw; an actor
 *  with a non-zero offset colour is drawn a second time in the additive
 *  pass (setAddPass).
 *
 *  In the ROM the drawer is the layouts' GIF packet writer: inside one
 *  DIRECT run of its packet list it writes A+D register runs (state) and
 *  REGLIST runs (PRIM, then the vertices). PC: no GS emulator -- the same
 *  state and vertices go to the host UI draw module
 *  (yamipan/rendering/yamipan_drawui.h) as the register values the ROM
 *  would pack; the DIRECT / GIF run bookkeeping is kept because the return
 *  values follow it.
 * ======================================================================== */
#ifndef _GAME_UIDRAWER_H
#define _GAME_UIDRAWER_H

#include <stddef.h>

#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

// Four floats (UiLayout's colour and offset, the drawer's colours). The
// ROM's out-of-line copy constructor 0x24f310 is the implicit one.
struct UiVec4
{
    float x, y, z, w;

    void set(float x_, float y_, float z_, float w_);                    // FS2 0x00243890 (game/uilayout.cpp)
    void multiply(const UiVec4 &v);                                      // FS2 0x0024f2c0: component-wise
    // The GS RGBAQ bytes: r g b x256, a x128 (the PS2's 0x80 = opaque),
    // each clamped to 0..255.
    void toRgbaq(u_char *out) const;                                     // FS2 0x0024f350
};
static_assert(sizeof(UiVec4) == 0x10, "UiVec4: EE 0x10");

// A 2D affine matrix, rows (a b tx) (c d ty): x' = a x + b y + tx.
struct UiMatrix
{
    float a, b, tx;             // FS2 +0x00
    float c, d, ty;             // FS2 +0x0c

    void identity();                                                     // FS2 0x0024e880
    void copy(const UiMatrix &m);                                        // FS2 0x0024e6d0
    void concat(const UiMatrix &m);                                      // FS2 0x0024e710: this = m * this
    void multiply(const UiMatrix &a, const UiMatrix &b);                 // FS2 0x0024e7c0: this = a * b
    void translate(const float *xy);                                     // FS2 0x0024e580
    void setScale(float sx, float sy);                                   // FS2 0x0024e5b0
    void rotate(float angle);                                            // FS2 0x0024e5d0: this = R(angle) * this
    // The angle of the x axis: atan2 of (1, 0)'s image minus the origin's.
    float angle() const;                                                 // FS2 0x0024e360
    // this = scale(scale) turned to m's angle and moved to m's origin
    // (m's own scale and shear dropped).
    void setScaleRotTrans(const UiMatrix &m, const float *scale);        // FS2 0x0024e3d0
    // this = from blended towards to by t (both decomposed, the angles
    // through atan2).
    void interpolate(const UiMatrix &from, const UiMatrix &to, float t); // FS2 0x0024dfa0
};
static_assert(sizeof(UiMatrix) == 0x18, "UiMatrix: EE 0x18");

// out = m * in; times 16 (GS 12.4 fixed point) when toFixed.
void uiTransformPoint(float *out, const UiMatrix *m, const float *in, int toFixed); // FS2 0x0024e4f0

// The vertex layouts the drawer reads (UiDrawer::draw*; xy in layout
// pixels, uv 0..1 of the bound texture, rgba 0..1 scaled by the drawer's
// colour).
struct UiVertex             { float xy[2]; };
struct UiColorVertex        { UiVec4 rgba; float xy[2]; };
struct UiTexVertex          { float uv[2]; float xy[2]; };
struct UiTexColorVertex     { UiVec4 rgba; float uv[2]; float xy[2]; };
struct UiStripVertex        { float xy[2]; float uv[2]; };
struct UiColorStripVertex   { UiVec4 rgba; float xy[2]; float uv[2]; };
static_assert(sizeof(UiVertex) == 0x8, "UiVertex: EE 0x8");
static_assert(sizeof(UiColorVertex) == 0x18, "UiColorVertex: EE 0x18");
static_assert(sizeof(UiTexVertex) == 0x10, "UiTexVertex: EE 0x10");
static_assert(sizeof(UiTexColorVertex) == 0x20, "UiTexColorVertex: EE 0x20");
static_assert(sizeof(UiStripVertex) == 0x10, "UiStripVertex: EE 0x10");
static_assert(offsetof(UiTexColorVertex, uv) == 0x10, "UiTexColorVertex::uv");
static_assert(offsetof(UiTexColorVertex, xy) == 0x18, "UiTexColorVertex::xy");
static_assert(offsetof(UiColorStripVertex, xy) == 0x10, "UiColorStripVertex::xy");

// The GS PRIM values the tracks hand UiDrawer::setPrim / setPrimColor:
// a primitive type and the flag bits (the add pass clears UiPrim_Textured
// and forces UiPrim_Blend).
enum UiPrim
{
    UiPrim_LineStrip = 0x002,
    UiPrim_Triangle = 0x003,
    UiPrim_TriStrip = 0x004,
    UiPrim_TriFan = 0x005,
    UiPrim_Sprite = 0x006,
    UiPrim_Gouraud = 0x008,     // IIP
    UiPrim_Textured = 0x010,    // TME
    UiPrim_Blend = 0x040,       // ABE
    UiPrim_Antialias = 0x080,   // AA1
    UiPrim_UV = 0x100,          // FST: texel coordinates
};

// UiDrawer::setBlend: the ALPHA_1 equation.
enum UiBlend
{
    UiBlend_Normal = 0,         // (Cs - Cd) * As + Cd
    UiBlend_Add = 1,            // Cs * As + Cd
    UiBlend_AddDestAlpha = 2,   // Cs * Ad + Cd (the add pass)
    UiBlend_NormalDestAlpha = 3, // (Cs - Cd) * Ad + Cd
};

// UiDrawer::setDrawPreset: the gfx state set-ups 0x230980..0x231090
// (game/gamehooks.h). The tracks' kinds 0xf..0x14 pick 5..0.
enum UiPreset
{
    UiPreset_Normal = 0,        // colour and alpha written
    UiPreset_AlphaOnly1 = 1,    // FRAME_1 FBMSK 0x00ffffff
    UiPreset_AlphaOnly2 = 2,
    UiPreset_RgbOnly3 = 3,      // FRAME_1 FBMSK 0xff000000
    UiPreset_RgbOnly4 = 4,
    UiPreset_RgbOnly5 = 5,
    UiPreset_DrawEnv = 6,       // FRAME / ZBUF / SCISSOR back to the draw env's (end)
};

// UiDrawer::openVertices: the registers before each vertex's XYZ (the GIF
// tags 0x307660 + format * 0x10). Grouped: in threes, the first two
// through XYZF3 (no kick), the third through XYZ2.
enum UiVertexFormat
{
    UiVertex_Xy = 0,
    UiVertex_Rgbaq = 1,
    UiVertex_Uv = 2,
    UiVertex_Grouped = 4,
    UiVertex_FormatCount = 8,
};

// GS register numbers the tracks write through UiDrawer::setGsReg.
constexpr int kGsRegTex1_1 = 0x14;

class GsTexture;                // game/gfxtarget.h
class UiFont;                   // game/uitrack.h
struct TimSheetSprite;          // game/uitrack.h

// The drawer's matrix stack depth (+0x1c..+0x7c).
constexpr int kUiDrawerStackDepth = 4;

// layout: host -- EE 0xe0 (pointers); the EE offsets in the comments.
class UiDrawer
{
public:
    UiDrawer();                                                          // FS2 0x0024fb30
    ~UiDrawer();                                                         // FS2 0x0024fae0: deleting (obj, 1)

    void begin(sceVif1Packet *packet, void *texList);                    // FS2 0x0024fad0
    int  setMode(int mode);                                              // FS2 0x0024e990: answers the old one
    void end();                                                          // FS2 0x0024f9e0

    // With it set, a matrix without rotation gets only the drawer's
    // translation, not the screen matrix of the mode. Answers the old one.
    u_char setPixelMode(u_char on);                                      // FS2 0x0024e9d0
    void   setTranslate(const float *xy);                                // FS2 0x0024ea10
    void   updateMatrix();                                               // FS2 0x0024ea30
    // The additive pass the offset colours draw in. Answers the old one.
    u_char setAddPass(u_char on);                                        // FS2 0x0024fc20

    // The track classes' state calls (game/uitrack.cpp). Each flushes the
    // open GIF run first; -1 without a packet.
    int setTexture(GsTexture *tex);                                      // FS2 0x0024f7a0: null keeps the current one
    // A raw image through a temporary GsTexture (the TimM sheet sprites).
    int setImage(const void *image);                                     // FS2 0x0024f580: null keeps the current one
    int setGsReg(int reg, u_long value);                                 // FS2 0x0024fcb0
    int setPrim(u_long prim);                                            // FS2 0x0024fe60: UiPrim; vertex colours follow
    // PRIM and one RGBAQ: rgba times m_color.
    int setPrimColor(u_long prim, const UiVec4 *rgba);                   // FS2 0x0024fd40
    int setTexRepeat(u_char on);                                         // FS2 0x0024fef0: CLAMP_1 REPEAT, else CLAMP
    int setBlend(int mode);                                              // FS2 0x0024ffa0: UiBlend; 1 changed, 0 same
    int setDrawPreset(int preset);                                       // FS2 0x00250130: UiPreset; resets the blend
    // A GS local copy of the 512x512 buffer at block 0x800 to 0x1800
    // (toBackup), or back (track kinds 0x28 / 0x29).
    int copyTarget(u_char toBackup);                                     // FS2 0x0024e8a0
    int setScissor(const float *rect);                                   // FS2 0x00250520: x0 y0 x1 y1, through m_matrix
    int resetScissor();                                                  // FS2 0x002506c0: the draw env's

    // Vertex lists, drawn with the prim set before (count vertices; grouped
    // is always 0 here: UiVertex_Grouped). Answer count, -1 without a packet.
    int drawFlat(const UiVertex *verts, int count, int grouped);         // FS2 0x00252680
    int drawGouraud(const UiColorVertex *verts, int count, int grouped); // FS2 0x002524a0
    int drawTexFlat(const UiTexVertex *verts, int count, int grouped);   // FS2 0x00252040
    int drawTexGouraud(const UiTexColorVertex *verts, int count, int grouped); // FS2 0x00252260
    // Sprites cut from a TimM sheet: uv 0..1 within the sprite's rect.
    int  drawSheetFlat(const void *verts, int count, const TimSheetSprite *sprite, int arg);    // FS2 0x00250750: {uv, xy}
    int  drawSheetGouraud(const void *verts, int count, const TimSheetSprite *sprite, int arg); // FS2 0x002509b0: {rgba, uv, xy}
    // A text track's text: its font, message (fMsg index, or
    // kUiTextLiteral for the font's own text), the record from +0x1c and
    // the track's position.
    int  drawText(UiFont *font, int message, const void *block, float x, float y); // FS2 0x0024eba0

    u_char isAddPass() const;                                            // FS2 0x0024f570
    u_char isFixedColor() const;                                         // FS2 0x0024f340

private:
    int  closeDirect();                                                  // FS2 0x0024f960: 1 if a run was open
    int  openVertices(int format);                                       // FS2 0x00250240: UiVertexFormat
    u_int vertexColor(const UiVec4 *rgba) const;                         // FS2 0x00250c40: packed RGBAQ
    void closeGifRun();         // inline: the open REGLIST / A+D run
    void resetState();          // inline in the ctor and end

public:
    sceVif1Packet *m_packet;    // FS2 +0x00: the packet list drawn into (begin)
    void     *m_texList;        // FS2 +0x04: the TexArea list of that packet list
    u_char    m_directOpen;     // FS2 +0x08: a CNT tag and DIRECT code are open
    u_char    m_gifOpen;        // FS2 +0x09: a vertex REGLIST run is open
    int       m_gifFormat;      // FS2 +0x0c: its UiVertexFormat, -1 none
    int       m_texBlock;       // FS2 +0x10: the bound texture's TBP (TexAreaList::place)
    int       m_texWidth;       // FS2 +0x14: the bound texture's size (GsTexture::getSize):
    int       m_texHeight;      // FS2 +0x18:   uv 0..1 scale to texels
    UiMatrix  m_stack[kUiDrawerStackDepth]; // FS2 +0x1c
    int       m_top;            // FS2 +0x7c: m_stack index
    UiVec4    m_color;          // FS2 +0x80: multiplied into what the tracks draw
    int       m_preset;         // FS2 +0x90: UiPreset, -1 none yet
    int       m_blend;          // FS2 +0x94: UiBlend, -1 none yet
    UiVec4    m_addColor;       // FS2 +0x98: the additive pass's colour
    u_char    m_addPass;        // FS2 +0xa8
    int       m_savedPreset;    // FS2 +0xac: m_preset / m_blend while in the add pass
    int       m_savedBlend;     // FS2 +0xb0
    // UiDrawState::drawerFlag: the drawer colour alone, the tracks' own
    // colours ignored (setPrimColor, vertexColor).
    u_char    m_fixedColor;     // FS2 +0xb4
    float     m_translate[2];   // FS2 +0xb8
    int       m_mode;           // FS2 +0xc0: setMode (the screen matrix pair)
    UiMatrix  m_matrix;         // FS2 +0xc4: the matrix the tracks draw with
    u_char    m_pixelMode;      // FS2 +0xdc
};

// The textured strip meshes of track kinds 8 / 0x24 (record mode first,
// the drawer second; uvRect the constant { 0.5, 0, 0.5, 1 }).
void uiDrawTexStripFlat(int mode, UiDrawer *drawer, const void *verts, int count, const UiVec4 *uvRect);    // FS2 0x00251350
void uiDrawTexStripGouraud(int mode, UiDrawer *drawer, const void *verts, int count, const UiVec4 *uvRect); // FS2 0x002519a0

#endif /* _GAME_UIDRAWER_H */
