/* ==========================================================================
 *  game/uidrawer.h
 *
 *  The 2D draw helper a UiLayout draws its sequences through
 *  (UiLayout::m_drawer, 0xe0 bytes) and its 2D affine matrix. The drawer's
 *  code sits at 0x24e580..0x250c30 in the rest of unassigned@00246230;
 *  what game/uilayout.cpp and game/uisequence.cpp need of it is declared
 *  here until that TU is cut. The names are the port's (no source string).
 *
 *  Per actor, UiSequence::draw loads the drawer's matrix-stack top, its
 *  colour and offset colour, then lets each playing track draw; an actor
 *  with a non-zero offset colour is drawn a second time in the additive
 *  pass (setAddPass).
 * ======================================================================== */
#ifndef _GAME_UIDRAWER_H
#define _GAME_UIDRAWER_H

#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

// Four floats (UiLayout's colour and offset, the drawer's colours).
struct UiVec4
{
    float x, y, z, w;

    void set(float x_, float y_, float z_, float w_);                    // FS2 0x00243890 (game/uilayout.cpp)
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

// The GS PRIM values the tracks hand UiDrawer::setPrim / setPrimColor:
// a primitive type and the flag bits (the add pass forces UiPrim_Blend).
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
    UiBlend_AddDestAlpha = 2,   // Cs * Ad + Cd
    UiBlend_NormalDestAlpha = 3, // (Cs - Cd) * Ad + Cd
};

// GS register numbers the tracks write through UiDrawer::setGsReg.
constexpr int kGsRegTex1_1 = 0x14;

class GsTexture;                // game/gfxtarget.h
class UiFont;                   // game/uitrack.h
struct TimSheetSprite;          // game/uitrack.h

// The drawer's matrix stack depth (+0x1c..+0x7c).
constexpr int kUiDrawerStackDepth = 4;

// layout: host -- EE 0xe0; only what the sequences touch is declared.
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
    int setNoAlphaTest(u_char on);                                       // FS2 0x0024fef0: TEST_1 0, else 5
    int setBlend(int mode);                                              // FS2 0x0024ffa0: UiBlend
    // One of seven gfx state set-ups (0x230980..0x231090, meaning unread);
    // resets the blend.
    int setDrawPreset(int preset);                                       // FS2 0x00250130
    // Four GS registers from table 0x3076f0 (on) or 0x307730 (meaning
    // unread).
    int setTargetRegs(u_char on);                                        // FS2 0x0024e8a0
    int setScissor(const float *rect);                                   // FS2 0x00250520: x0 y0 x1 y1, through m_matrix
    int resetScissor();                                                  // FS2 0x002506c0: the draw env's

    // Vertex lists, drawn with the prim set before (count vertices; the
    // last argument is always 0 here).
    void drawFlat(const void *verts, int count, int arg);                // FS2 0x00252680: {xy}
    void drawGouraud(const void *verts, int count, int arg);             // FS2 0x002524a0: {rgba, xy}
    void drawTexFlat(const void *verts, int count, int arg);             // FS2 0x00252040: {uv, xy}
    void drawTexGouraud(const void *verts, int count, int arg);          // FS2 0x00252260: {rgba, uv, xy}
    // Sprites cut from a TimM sheet: uv 0..1 within the sprite's rect.
    int  drawSheetFlat(const void *verts, int count, const TimSheetSprite *sprite, int arg);    // FS2 0x00250750: {uv, xy}
    int  drawSheetGouraud(const void *verts, int count, const TimSheetSprite *sprite, int arg); // FS2 0x002509b0: {rgba, uv, xy}
    // A text track's text: its font, message (fMsg index, or
    // kUiTextLiteral for the font's own text), the record from +0x1c and
    // the track's position.
    int  drawText(UiFont *font, int message, const void *block, float x, float y); // FS2 0x0024eba0

    u_char   _00[0x1c];
    UiMatrix m_stack[kUiDrawerStackDepth]; // FS2 +0x1c
    int      m_top;             // FS2 +0x7c: m_stack index
    UiVec4   m_color;           // FS2 +0x80: multiplied into what the tracks draw
    u_char   _90[8];            // FS2 +0x90: saved by setAddPass
    UiVec4   m_addColor;        // FS2 +0x98: the additive pass's colour
    u_char   m_addPass;         // FS2 +0xa8
    u_char   _ac[8];            // FS2 +0xac
    u_char   m_b4;              // FS2 +0xb4: UiDrawState::drawerFlag (meaning unread)
    float    m_translate[2];    // FS2 +0xb8
    int      m_mode;            // FS2 +0xc0: setMode
    UiMatrix m_matrix;          // FS2 +0xc4: the matrix the tracks draw with
    u_char   m_pixelMode;       // FS2 +0xdc
};

// The textured strip meshes of track kinds 8 / 0x24 (record mode first,
// the drawer second; uvRect the constant { 0.5, 0, 0.5, 1 }).
void uiDrawTexStripFlat(int mode, UiDrawer *drawer, const void *verts, int count, const UiVec4 *uvRect);    // FS2 0x00251350
void uiDrawTexStripGouraud(int mode, UiDrawer *drawer, const void *verts, int count, const UiVec4 *uvRect); // FS2 0x002519a0

#endif /* _GAME_UIDRAWER_H */
