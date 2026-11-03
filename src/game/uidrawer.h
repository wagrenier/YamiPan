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
    void translate(const float *xy);                                     // FS2 0x0024e580
};
static_assert(sizeof(UiMatrix) == 0x18, "UiMatrix: EE 0x18");

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

#endif /* _GAME_UIDRAWER_H */
