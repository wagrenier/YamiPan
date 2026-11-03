/* ==========================================================================
 *  game/uitrack.h
 *
 *  The track classes of the 2D layouts (game/uitrack.cpp,
 *  0x249550..0x24d530): one class per S2D track record kind
 *  (UiTrackKind), built by the factory UiAnim::init. Each overrides one of
 *  UiTrack's slots:
 *  - draw (the shapes and the drawer state kinds): what the drawer draws
 *    for the actor, with its matrix and colours;
 *  - apply (the transform and colour kinds): what the actor's matrix,
 *    colour or offset colour becomes for the frame (UiAnim::apply);
 *  - getAnchor (the shapes): the first vertex (x, y), where UiWidget::create
 *    places widgets.
 *  The vtables sit at 0x316540..0x316a60, one 0x20-byte block per class
 *  ({type_info, 0} then the four slots), vptr at +0x10 after the base's
 *  four fields. A "Flat" class (one colour in the record) derives from its
 *  vertex-coloured sibling where the vtables chain them.
 *
 *  The records are the S2D file's (game/s2ddata.h): ints and floats after
 *  {kind, count}, the vertex arrays last. The names are the port's (no
 *  source string in the range).
 * ======================================================================== */
#ifndef _GAME_UITRACK_H
#define _GAME_UITRACK_H

#include <stddef.h>
#include <stdint.h>

#include "game/gfxtarget.h"
#include "game/s2ddata.h"
#include "game/uidrawer.h"
#include "game/uisequence.h"
#include "sdk/scetypes.h"

// ──────────────────────────────────────────────────────────────────────
// Records

// The vertex layouts the drawer reads (UiDrawer::draw*; xy in layout
// pixels, uv in texels, rgba 0..1 scaled by the drawer's colour).
struct UiVertex             { float xy[2]; };
struct UiColorVertex        { UiVec4 rgba; float xy[2]; };
struct UiTexVertex          { float uv[2]; float xy[2]; };
struct UiTexColorVertex     { UiVec4 rgba; float uv[2]; float xy[2]; };
struct UiStripVertex        { float xy[2]; float uv[2]; };
struct UiColorStripVertex   { UiVec4 rgba; float xy[2]; float uv[2]; };
static_assert(sizeof(UiVertex) == 0x8, "UiVertex: EE 0x8");
static_assert(sizeof(UiColorVertex) == 0x18, "UiColorVertex: EE 0x18");
static_assert(sizeof(UiTexVertex) == 0x10, "UiTexVertex: EE 0x10");
static_assert(sizeof(UiStripVertex) == 0x10, "UiStripVertex: EE 0x10");
static_assert(offsetof(UiTexColorVertex, xy) == 0x18, "UiTexColorVertex::xy");
static_assert(offsetof(UiColorStripVertex, xy) == 0x10, "UiColorStripVertex::xy");

// S2dTrianglesRec::mode and its kin: how the vertex list is joined.
enum UiTriMode
{
    UiTriMode_List = 0,
    UiTriMode_Fan = 1,
    UiTriMode_Strip = 2,
};

// Kind 0 / 0x1d flags.
constexpr u_int kUiLineAntialias = 0x1;

struct S2dLineStripRec          // UiTrackKind_LineStrip
{
    int           kind;         // FS2 +0x00
    int           count;        // FS2 +0x04
    u_int         flags;        // FS2 +0x08: kUiLineAntialias
    UiColorVertex verts[1];     // FS2 +0x0c
};
static_assert(offsetof(S2dLineStripRec, verts) == 0xc, "S2dLineStripRec::verts");
struct S2dLineStripFlatRec      // UiTrackKind_LineStripFlat
{
    int      kind;              // FS2 +0x00
    int      count;             // FS2 +0x04
    u_int    flags;             // FS2 +0x08: kUiLineAntialias
    UiVec4   rgba;              // FS2 +0x0c
    UiVertex verts[1];          // FS2 +0x1c
};
static_assert(offsetof(S2dLineStripFlatRec, verts) == 0x1c, "S2dLineStripFlatRec::verts");

struct S2dSpriteRec             // UiTrackKind_Sprite
{
    int           kind;         // FS2 +0x00
    int           count;        // FS2 +0x04
    UiColorVertex verts[1];     // FS2 +0x08
};
static_assert(offsetof(S2dSpriteRec, verts) == 0x8, "S2dSpriteRec::verts");
struct S2dSpriteFlatRec         // UiTrackKind_SpriteFlat
{
    int      kind;              // FS2 +0x00
    int      count;             // FS2 +0x04
    UiVec4   rgba;              // FS2 +0x08
    UiVertex verts[1];          // FS2 +0x18
};
static_assert(offsetof(S2dSpriteFlatRec, verts) == 0x18, "S2dSpriteFlatRec::verts");

struct S2dTrianglesRec          // UiTrackKind_Triangles
{
    int           kind;         // FS2 +0x00
    int           count;        // FS2 +0x04
    int           mode;         // FS2 +0x08: UiTriMode
    UiColorVertex verts[1];     // FS2 +0x0c
};
static_assert(offsetof(S2dTrianglesRec, verts) == 0xc, "S2dTrianglesRec::verts");
struct S2dTrianglesFlatRec      // UiTrackKind_TrianglesFlat
{
    int      kind;              // FS2 +0x00
    int      count;             // FS2 +0x04
    int      mode;              // FS2 +0x08: UiTriMode
    UiVec4   rgba;              // FS2 +0x0c
    UiVertex verts[1];          // FS2 +0x1c
};
static_assert(offsetof(S2dTrianglesFlatRec, verts) == 0x1c, "S2dTrianglesFlatRec::verts");

struct S2dScissorRec            // UiTrackKind_Scissor
{
    int   kind;                 // FS2 +0x00
    int   count;                // FS2 +0x04
    int   enable;               // FS2 +0x08: 0: the draw env's scissor
    float rect[4];              // FS2 +0x0c: x0 y0 x1 y1
};
static_assert(offsetof(S2dScissorRec, rect) == 0xc, "S2dScissorRec::rect");

// The text record: two images (by S2D index, or by init-pack name when
// negative: string -1 - index), the font scale, and from +0x1c what the
// text draw takes (not read yet).
struct S2dTextRec               // UiTrackKind_Text
{
    int    kind;                // FS2 +0x00
    int    count;               // FS2 +0x04
    int    fontImage;           // FS2 +0x08: the font texture
    int    glyphImage;          // FS2 +0x0c: the glyph table
    int    _10;
    float  scale[2];            // FS2 +0x14: UiFont::m_scale
    UiVec4 _1c;                 // FS2 +0x1c: handed to the text draw (0x24eba0)
    float  xy[2];               // FS2 +0x2c: the anchor
};
static_assert(offsetof(S2dTextRec, xy) == 0x2c, "S2dTextRec::xy");

struct S2dTexSpriteRec          // UiTrackKind_TexSprite
{
    int              kind;      // FS2 +0x00
    int              count;     // FS2 +0x04
    int              _08;
    int              image;     // FS2 +0x0c: S2dData::images index
    u_int            tex1;      // FS2 +0x10: the GS TEX1_1 value
    UiTexColorVertex verts[1];  // FS2 +0x14
};
static_assert(offsetof(S2dTexSpriteRec, verts) == 0x14, "S2dTexSpriteRec::verts");
struct S2dTexSpriteFlatRec      // UiTrackKind_TexSpriteFlat
{
    int         kind;           // FS2 +0x00
    int         count;          // FS2 +0x04
    int         _08;
    int         image;          // FS2 +0x0c
    u_int       tex1;           // FS2 +0x10
    UiVec4      rgba;           // FS2 +0x14
    UiTexVertex verts[1];       // FS2 +0x24
};
static_assert(offsetof(S2dTexSpriteFlatRec, verts) == 0x24, "S2dTexSpriteFlatRec::verts");

// Kinds 6 / 0x22: a sprite cut from a TimM sheet image, picked by name
// (+0x14) through UiSheetSpriteTrack::findSprite.
// layout: host -- EE: +0xc image, +0x10 TEX1, +0x14 name (an offset in
// the file), vertices at +0x28 (kind 6, UiTexColorVertex) or the colour at
// +0x28 and vertices at +0x38 (kind 0x22, UiTexVertex); anchor +0x40 both.
struct S2dSheetSpriteRec        // UiTrackKind_SheetSprite
{
    int              kind;      // FS2 +0x00
    int              count;     // FS2 +0x04
    int              _08;
    int              image;     // FS2 +0x0c: S2dData::images index (a TimM sheet)
    u_int            tex1;      // FS2 +0x10: the GS TEX1_1 value
    const char      *name;      // FS2 +0x14: the sprite's name in the sheet
    u_char           _18[0x10];
    UiTexColorVertex verts[1];  // FS2 +0x28
};
// layout: host (see S2dSheetSpriteRec)
struct S2dSheetSpriteFlatRec    // UiTrackKind_SheetSpriteFlat
{
    int         kind;           // FS2 +0x00
    int         count;          // FS2 +0x04
    int         _08;
    int         image;          // FS2 +0x0c
    u_int       tex1;           // FS2 +0x10
    const char *name;           // FS2 +0x14
    u_char      _18[0x10];
    UiVec4      rgba;           // FS2 +0x28
    UiTexVertex verts[1];       // FS2 +0x38
};

struct S2dTexTrianglesRec       // UiTrackKind_TexTriangles, _TexTrianglesNoTest
{
    int              kind;      // FS2 +0x00
    int              count;     // FS2 +0x04
    int              _08;
    int              image;     // FS2 +0x0c
    u_int            tex1;      // FS2 +0x10
    int              mode;      // FS2 +0x14: UiTriMode
    UiTexColorVertex verts[1];  // FS2 +0x18
};
static_assert(offsetof(S2dTexTrianglesRec, verts) == 0x18, "S2dTexTrianglesRec::verts");
struct S2dTexTrianglesFlatRec   // UiTrackKind_TexTrianglesFlat
{
    int         kind;           // FS2 +0x00
    int         count;          // FS2 +0x04
    int         _08;
    int         image;          // FS2 +0x0c
    u_int       tex1;           // FS2 +0x10
    int         mode;           // FS2 +0x14: UiTriMode
    UiVec4      rgba;           // FS2 +0x18
    UiTexVertex verts[1];       // FS2 +0x28
};
static_assert(offsetof(S2dTexTrianglesFlatRec, verts) == 0x28, "S2dTexTrianglesFlatRec::verts");

struct S2dTexStripRec           // UiTrackKind_TexStrip
{
    int                kind;    // FS2 +0x00
    int                count;   // FS2 +0x04
    int                _08;
    int                image;   // FS2 +0x0c
    int                _10;
    int                mode;    // FS2 +0x14: the mesh's first argument
    UiColorStripVertex verts[1]; // FS2 +0x18
};
static_assert(offsetof(S2dTexStripRec, verts) == 0x18, "S2dTexStripRec::verts");
struct S2dTexStripFlatRec       // UiTrackKind_TexStripFlat
{
    int           kind;         // FS2 +0x00
    int           count;        // FS2 +0x04
    int           _08;
    int           image;        // FS2 +0x0c
    int           _10;
    int           mode;         // FS2 +0x14
    UiVec4        rgba;         // FS2 +0x18
    UiStripVertex verts[1];     // FS2 +0x28
};
static_assert(offsetof(S2dTexStripFlatRec, verts) == 0x28, "S2dTexStripFlatRec::verts");

// Keyed kinds 9..0xb: one value per frame (length frames).
struct S2dMatrixFramesRec       // UiTrackKind_KeyedA
{
    int      kind;              // FS2 +0x00
    int      length;            // FS2 +0x04
    UiMatrix frames[1];         // FS2 +0x08
};
static_assert(offsetof(S2dMatrixFramesRec, frames) == 0x8, "S2dMatrixFramesRec::frames");
struct S2dColorFramesRec        // UiTrackKind_KeyedB, _KeyedC
{
    int    kind;                // FS2 +0x00
    int    length;              // FS2 +0x04
    UiVec4 frames[1];           // FS2 +0x08
};
static_assert(offsetof(S2dColorFramesRec, frames) == 0x8, "S2dColorFramesRec::frames");

// Keyed kinds 0xc..0xe: keys by frame, interpolated linearly between.
struct S2dMatrixKey
{
    int      frame;             // FS2 +0x0
    UiMatrix value;             // FS2 +0x4
};
static_assert(sizeof(S2dMatrixKey) == 0x1c, "S2dMatrixKey: EE 0x1c");
struct S2dColorKey
{
    int    frame;               // FS2 +0x0
    UiVec4 value;               // FS2 +0x4
};
static_assert(sizeof(S2dColorKey) == 0x14, "S2dColorKey: EE 0x14");
struct S2dMatrixKeysRec         // UiTrackKind_KeyedA2
{
    int          kind;          // FS2 +0x00
    int          length;        // FS2 +0x04
    int          keyCount;      // FS2 +0x08
    S2dMatrixKey keys[1];       // FS2 +0x0c
};
static_assert(offsetof(S2dMatrixKeysRec, keys) == 0xc, "S2dMatrixKeysRec::keys");
struct S2dColorKeysRec          // UiTrackKind_KeyedB2, _KeyedC2
{
    int         kind;           // FS2 +0x00
    int         length;         // FS2 +0x04
    int         keyCount;       // FS2 +0x08
    S2dColorKey keys[1];        // FS2 +0x0c
};
static_assert(offsetof(S2dColorKeysRec, keys) == 0xc, "S2dColorKeysRec::keys");

// Kinds 0x15..0x1a.
// layout: host -- EE: +8 the rotation (0x19 / 0x1a) or the pin mode
// (0x15..0x18), +0xc the target name of 0x1a, +0xc the pin's scale and
// +0x14 the target name of 0x16 / 0x18 (offsets in the file:
// "rCharactorPostion" binds to the game camera).
struct S2dRotateRec
{
    int         kind;           // FS2 +0x00
    int         count;          // FS2 +0x04
    float       angle;          // FS2 +0x08: radians
    const char *target;         // FS2 +0x0c: kind 0x1a
};
// layout: host (see S2dRotateRec)
struct S2dPinRec
{
    int         kind;           // FS2 +0x00
    int         count;          // FS2 +0x04
    int         mode;           // FS2 +0x08: UiPinMode
    float       scale[2];       // FS2 +0x0c: the pinned matrix's scale
    const char *target;         // FS2 +0x14: kinds 0x16 / 0x18
};

// S2dPinRec::mode: what the pin keeps of the combined matrix besides its
// origin (the scale is always the record's).
enum UiPinMode
{
    UiPinMode_Upright = 0,      // no rotation
    UiPinMode_Turned = 1,       // any other value: the combined angle
};

// ──────────────────────────────────────────────────────────────────────
// Stand-ins for the classes the tracks hold, from later TUs.

// A font: its glyph table and texture, the text it draws and its scale
// (EE 0x14 bytes; 0x25fa50..0x25fbd0, with the fMsg message files).
// layout: host
class UiFont
{
public:
    UiFont(const void *glyphs, const void *image);                       // FS2 0x0025fac0: scale 2, 0
    ~UiFont();                                                           // FS2 0x0025fa50: deleting (obj, 1)

    const void     *m_glyphs;   // FS2 +0x00
    GsTexture      *m_texture;  // FS2 +0x04
    const u_short  *m_text;     // FS2 +0x08: UiTextTrack::m_text
    float           m_scale[2]; // FS2 +0x0c
};

// Relocates a TimM sheet image in place (once; "TimM" magic). False when
// it is not one. With the layout hooks (0x25fe10).
bool timSheetRelocate(void *sheet);                                      // FS2 0x0025fe10

// A sprite of a TimM sheet: which of the sheet's images, and its rect in
// it (texels).
struct TimSheetSprite
{
    short _00;
    short image;                // FS2 +0x2: TimSheet::images index
    short u, v;                 // FS2 +0x4
    short w, h;                 // FS2 +0x8
};
static_assert(offsetof(TimSheetSprite, w) == 0x8, "TimSheetSprite::w");

// layout: host -- EE: +0 "TimM", +4 relocated, +0x14 / +0x18 the sprites,
// +0x20 / +0x24 the images {?, image} (8 bytes each); pointers after
// timSheetRelocate.
struct TimSheetImage
{
    int         _00;
    const void *image;          // FS2 +0x4: what UiDrawer::setImage binds
};
// layout: host (see TimSheetImage)
struct TimSheet
{
    char            magic[4];   // FS2 +0x00: "TimM"
    u_char          relocated;  // FS2 +0x04
    u_char          _05[0xf];
    int             spriteCount; // FS2 +0x14
    TimSheetSprite *sprites;    // FS2 +0x18
    int             imageCount; // FS2 +0x20
    TimSheetImage  *images;     // FS2 +0x24
};

// An fMsg message file (with the fonts, 0x25fa50..): named wide-string
// messages.
// layout: host -- EE: +0 "fMsg", +4 relocated, +0x10 count, +0x30 the
// entries {name, text} (8 bytes each); pointers after fMsgRelocate.
struct FMsgEntry
{
    const char    *name;        // FS2 +0x0
    const u_short *text;        // FS2 +0x4
};
// layout: host (see FMsgEntry)
struct FMsgFile
{
    char      magic[4];         // FS2 +0x00: "fMsg"
    u_char    relocated;        // FS2 +0x04
    u_char    _05[0xb];
    int       count;            // FS2 +0x10
    u_char    _14[0x1c];
    FMsgEntry entries[1];       // FS2 +0x30
};

// Relocates an fMsg file in place (once). False when it is not one.
bool fMsgRelocate(void *file);                                           // FS2 0x0025fbd0
// The index of the message called name; -1 none.
int  fMsgFind(void *file, const char *name);                             // FS2 0x0025fb30

// Inline in the ROM (UiTextTrack's setters): file when it is an fMsg.
inline FMsgFile *fMsgCheck(void *file)
{
    if (file == nullptr)
        return nullptr;
    fMsgRelocate(file);
    const char *magic = (const char *)file;
    if (!(magic[0] == 'f' && magic[1] == 'M' && magic[2] == 's' && magic[3] == 'g'))
        return nullptr;
    return (FMsgFile *)file;
}
// Inline in the ROM: entry index of an fMsg, null out of range.
inline const FMsgEntry *fMsgEntry(FMsgFile *file, int index)
{
    if (!fMsgRelocate(file))
        return nullptr;
    if (index < 0)
        return nullptr;
    if (index < file->count)
        return &file->entries[index];
    return nullptr;
}

// The game's 16-bit strings (0x2d4440.. beside the char versions).
int      wstrlen(const u_short *s);                                      // FS2 0x002d4650: 0 for null
u_short *wstrcpy(u_short *dst, const u_short *src);                      // FS2 0x002d4510: null src copies nothing

// ──────────────────────────────────────────────────────────────────────
// The track classes, in vtable order (0x316540..0x316a60).

// Kind 0x2b: moves the actor by an offset the widgets set (EE 0x1c).
// layout: host (vptr; EE fields at the offsets noted)
class UiOffsetTrack : public UiTrack
{
public:
    UiOffsetTrack(S2dTrackRec *rec) : UiTrack(rec) { m_offset[0] = 0.0f; m_offset[1] = 0.0f; }
    ~UiOffsetTrack();                                                    // FS2 0x0024a730
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024a720

    float m_offset[2];          // FS2 +0x14
};

// Kinds 0x28 / 0x29: UiDrawer::setTargetRegs (EE 0x18).
// layout: host (vptr; EE fields at the offsets noted)
class UiTargetTrack : public UiTrack
{
public:
    UiTargetTrack(S2dTrackRec *rec, int alt) : UiTrack(rec), m_alt(alt) {}
    ~UiTargetTrack();                                                    // FS2 0x0024a7d0
    void draw(UiDrawer *drawer);                                         // FS2 0x0024a790

    int m_alt;                  // FS2 +0x14: 0 kind 0x28, 1 kind 0x29
};

// Kind 0x2a: the dest-alpha additive blend from here on.
// layout: host (vptr; EE fields at the offsets noted)
class UiDestAlphaBlendTrack : public UiTrack
{
public:
    UiDestAlphaBlendTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiDestAlphaBlendTrack();                                            // FS2 0x0024a840
    void draw(UiDrawer *drawer);                                         // FS2 0x0024a830
};

// Kind 0x1b: the additive blend from here on.
// layout: host (vptr; EE fields at the offsets noted)
class UiAddBlendTrack : public UiTrack
{
public:
    UiAddBlendTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiAddBlendTrack();                                                  // FS2 0x0024a8b0
    void draw(UiDrawer *drawer);                                         // FS2 0x0024a8a0
};

// Kinds 0xf..0x14: UiDrawer::setDrawPreset(Preset). One vtable each
// (Preset 5 at 0x316600 down to 0 at 0x3166a0).
// layout: host (vptr; EE fields at the offsets noted)
template <int Preset>
class UiPresetTrack : public UiTrack
{
public:
    UiPresetTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiPresetTrack();                                                    // FS2 0x0024a920 (5) .. 0x0024ab50 (0)
    void draw(UiDrawer *drawer);                                         // FS2 0x0024a910 (5) .. 0x0024ab40 (0)
};

// Kind 0x26: sets UiDrawState::drawerFlag.
// layout: host (vptr; EE fields at the offsets noted)
class UiDrawerFlagTrack : public UiTrack
{
public:
    UiDrawerFlagTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiDrawerFlagTrack();                                                // FS2 0x0024abc0
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024abb0
};

// Kind 0x25 (UiTrackKind_LayoutColor).
// layout: host (vptr; EE fields at the offsets noted)
class UiLayoutColorTrack : public UiTrack
{
public:
    UiLayoutColorTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiLayoutColorTrack();                                               // FS2 0x0024ac40
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024ac20
};

// Kind 0x1c (UiTrackKind_BaseColor).
// layout: host (vptr; EE fields at the offsets noted)
class UiBaseColorTrack : public UiTrack
{
public:
    UiBaseColorTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiBaseColorTrack();                                                 // FS2 0x0024acc0
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024aca0
};

// Kind 0x1a: turns the actor to the combined matrix's angle plus its own
// and a target's, in pixel coordinates (EE 0x1c).
// layout: host (vptr; EE fields at the offsets noted)
class UiRotateToTrack : public UiTrack
{
public:
    UiRotateToTrack(S2dTrackRec *rec)
        : UiTrack(rec), m_angle(((S2dRotateRec *)rec)->angle), m_targetAngle(0.0f)
    {
    }
    ~UiRotateToTrack();                                                  // FS2 0x0024ae50
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024ad20
    // "rCharactorPostion": the camera's yaw, else 0 (with the layout hooks).
    void bindTarget(const char *name);                                   // FS2 0x00260a20

    float m_angle;              // FS2 +0x14: the record's
    float m_targetAngle;        // FS2 +0x18
};

// Kind 0x19: as UiRotateToTrack without a target (EE 0x18).
// layout: host (vptr; EE fields at the offsets noted)
class UiRotateTrack : public UiTrack
{
public:
    UiRotateTrack(S2dTrackRec *rec) : UiTrack(rec), m_angle(((S2dRotateRec *)rec)->angle) {}
    ~UiRotateTrack();                                                    // FS2 0x0024afd0
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024aeb0

    float m_angle;              // FS2 +0x14
};

// Kinds 0x16 / 0x18: UiPinTrack after moving by a target's position and
// angle (EE 0x20).
// layout: host (vptr; EE fields at the offsets noted)
class UiPinRotTrack : public UiTrack
{
public:
    UiPinRotTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiPinRotTrack();                                                    // FS2 0x0024b150
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024b030
    // "rCharactorPostion": the camera's position / 10 and yaw, else 0
    // (with the layout hooks).
    void bindTarget(const char *name);                                   // FS2 0x00260ab0

    float m_pos[2];             // FS2 +0x14
    float m_angle;              // FS2 +0x1c
};

// Kinds 0x15 / 0x17: pins the actor at the combined matrix's origin: the
// origin goes to the parent, the actor keeps the record's scale and
// (UiPinMode) the combined angle.
// layout: host (vptr; EE fields at the offsets noted)
class UiPinTrack : public UiTrack
{
public:
    UiPinTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiPinTrack();                                                       // FS2 0x0024b2a0
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024b1b0
};

// Kind 0xe: adds the keyed rgba to the offset colour.
// layout: host (vptr; EE fields at the offsets noted)
class UiOffsetKeysTrack : public UiTrack
{
public:
    UiOffsetKeysTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiOffsetKeysTrack();                                                // FS2 0x0024b460
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024b300
};

// Kind 0xd: multiplies the colour by the keyed rgba.
// layout: host (vptr; EE fields at the offsets noted)
class UiColorKeysTrack : public UiTrack
{
public:
    UiColorKeysTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiColorKeysTrack();                                                 // FS2 0x0024b620
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024b4c0
};

// Kind 0xc: concatenates the keyed matrix.
// layout: host (vptr; EE fields at the offsets noted)
class UiMatrixKeysTrack : public UiTrack
{
public:
    UiMatrixKeysTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiMatrixKeysTrack();                                                // FS2 0x0024b7c0
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024b680
};

// Kind 0xb: adds the frame's rgba to the offset colour.
// layout: host (vptr; EE fields at the offsets noted)
class UiOffsetFramesTrack : public UiTrack
{
public:
    UiOffsetFramesTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiOffsetFramesTrack();                                              // FS2 0x0024b8a0
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024b820
};

// Kind 0xa: multiplies the colour by the frame's rgba.
// layout: host (vptr; EE fields at the offsets noted)
class UiColorFramesTrack : public UiTrack
{
public:
    UiColorFramesTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiColorFramesTrack();                                               // FS2 0x0024b980
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024b900
};

// Kind 9: concatenates the frame's matrix.
// layout: host (vptr; EE fields at the offsets noted)
class UiMatrixFramesTrack : public UiTrack
{
public:
    UiMatrixFramesTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiMatrixFramesTrack();                                              // FS2 0x0024ba40
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x0024b9e0
};

// The textured shapes own a GsTexture of their S2D image (EE 0x1c):
// +0x14 the texture, +0x18 the image entry's id.

// Kind 8: a textured strip mesh, vertex colours.
// layout: host (vptr; EE fields at the offsets noted)
class UiTexStripTrack : public UiTrack
{
public:
    UiTexStripTrack(S2dTrackRec *rec) : UiTrack(rec), m_texture(nullptr) {}
    ~UiTexStripTrack();                                                  // FS2 0x0024bcb0
    void draw(UiDrawer *drawer);                                         // FS2 0x0024bc10
    const float *getAnchor();                                            // FS2 0x0024d430

    GsTexture *m_texture;       // FS2 +0x14
    int        m_imageId;       // FS2 +0x18: S2dImage::id
};

// Kind 0x24: kind 8 in one colour.
// layout: host (vptr; EE fields at the offsets noted)
class UiTexStripFlatTrack : public UiTexStripTrack
{
public:
    UiTexStripFlatTrack(S2dTrackRec *rec) : UiTexStripTrack(rec) {}
    ~UiTexStripFlatTrack();                                              // FS2 0x0024bb40
    void draw(UiDrawer *drawer);                                         // FS2 0x0024baa0
    const float *getAnchor();                                            // FS2 0x0024d420
};

// Kind 0x27: kind 7 with the alpha test off (EE 0x1c).
// layout: host (vptr; EE fields at the offsets noted)
class UiTexTrianglesNoTestTrack : public UiTrack
{
public:
    UiTexTrianglesNoTestTrack(S2dTrackRec *rec) : UiTrack(rec), m_texture(nullptr) {}
    ~UiTexTrianglesNoTestTrack();                                        // FS2 0x0024be20
    void draw(UiDrawer *drawer);                                         // FS2 0x0024bd40
    const float *getAnchor();                                            // FS2 0x0024d440

    GsTexture *m_texture;       // FS2 +0x14
    int        m_imageId;       // FS2 +0x18
};

// Kind 7: textured triangles (list / fan / strip), vertex colours.
// layout: host (vptr; EE fields at the offsets noted)
class UiTexTrianglesTrack : public UiTrack
{
public:
    UiTexTrianglesTrack(S2dTrackRec *rec) : UiTrack(rec), m_texture(nullptr) {}
    ~UiTexTrianglesTrack();                                              // FS2 0x0024c110
    void draw(UiDrawer *drawer);                                         // FS2 0x0024c050
    const float *getAnchor();                                            // FS2 0x0024d460

    GsTexture *m_texture;       // FS2 +0x14
    int        m_imageId;       // FS2 +0x18
};

// Kind 0x23: kind 7 in one colour.
// layout: host (vptr; EE fields at the offsets noted)
class UiTexTrianglesFlatTrack : public UiTexTrianglesTrack
{
public:
    UiTexTrianglesFlatTrack(S2dTrackRec *rec) : UiTexTrianglesTrack(rec) {}
    ~UiTexTrianglesFlatTrack();                                          // FS2 0x0024bf80
    void draw(UiDrawer *drawer);                                         // FS2 0x0024beb0
    const float *getAnchor();                                            // FS2 0x0024d450
};

// Kind 6: a sprite cut from a TimM sheet of the S2D (not owned; EE 0x1c).
// layout: host (vptr; EE fields at the offsets noted)
class UiSheetSpriteTrack : public UiTrack
{
public:
    UiSheetSpriteTrack(S2dTrackRec *rec) : UiTrack(rec), m_sheet(nullptr) {}
    ~UiSheetSpriteTrack();                                               // FS2 0x0024c380
    void draw(UiDrawer *drawer);                                         // FS2 0x0024c2d0
    const float *getAnchor();                                            // FS2 0x0024d480
    // The sheet's sprite named by the record (with the layout hooks);
    // null: nothing drawn.
    const TimSheetSprite *findSprite();                                  // FS2 0x00261d00

    TimSheet *m_sheet;          // FS2 +0x14: S2dImage::data, relocated
    int   m_imageId;            // FS2 +0x18
};

// Kind 0x22: kind 6 in one colour.
// layout: host (vptr; EE fields at the offsets noted)
class UiSheetSpriteFlatTrack : public UiSheetSpriteTrack
{
public:
    UiSheetSpriteFlatTrack(S2dTrackRec *rec) : UiSheetSpriteTrack(rec) {}
    ~UiSheetSpriteFlatTrack();                                           // FS2 0x0024c260
    void draw(UiDrawer *drawer);                                         // FS2 0x0024c1a0
    const float *getAnchor();                                            // FS2 0x0024d470
};

// Kind 5: a textured sprite, vertex colours.
// layout: host (vptr; EE fields at the offsets noted)
class UiTexSpriteTrack : public UiTrack
{
public:
    UiTexSpriteTrack(S2dTrackRec *rec) : UiTrack(rec), m_texture(nullptr), m_imageId(0) {}
    ~UiTexSpriteTrack();                                                 // FS2 0x0024c5a0
    void draw(UiDrawer *drawer);                                         // FS2 0x0024c530
    const float *getAnchor();                                            // FS2 0x0024d4a0

    GsTexture *m_texture;       // FS2 +0x14
    int        m_imageId;       // FS2 +0x18
};

// Kind 0x21: kind 5 in one colour.
// layout: host (vptr; EE fields at the offsets noted)
class UiTexSpriteFlatTrack : public UiTexSpriteTrack
{
public:
    UiTexSpriteFlatTrack(S2dTrackRec *rec) : UiTexSpriteTrack(rec) {}
    ~UiTexSpriteFlatTrack();                                             // FS2 0x0024c460
    void draw(UiDrawer *drawer);                                         // FS2 0x0024c3e0
    const float *getAnchor();                                            // FS2 0x0024d490
};

// Kind 4: the scissor rectangle from here on.
// layout: host (vptr; EE fields at the offsets noted)
class UiScissorTrack : public UiTrack
{
public:
    UiScissorTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiScissorTrack();                                                   // FS2 0x0024c680
    void draw(UiDrawer *drawer);                                         // FS2 0x0024c630
    const float *getAnchor();                                            // FS2 0x0024d4b0
};

// UiTextTrack::m_message of a text set as a string (setTextAt).
constexpr int kUiTextLiteral = 10000;

// Kind 3: a text in a font: a message of an fMsg file or a wide string
// (EE 0x3c). The widgets set it (UiWidget::create and the classes of
// 0x25b8e0..0x25ce30).
// layout: host (vptr; EE fields at the offsets noted)
class UiTextTrack : public UiTrack
{
public:
    UiTextTrack(S2dTrackRec *rec, const void *glyphs, const void *fontImage)
        : UiTrack(rec), m_font(new UiFont(glyphs, fontImage)), m_message(-1), m_x(0.0f), m_y(0.0f),
          m_visible(1), m_fontKey(0), m_glyphKey(0), m_30(0), m_text(nullptr), m_ownsText(0)
    {
        const S2dTextRec *text = (const S2dTextRec *)rec;
        m_font->m_scale[0] = text->scale[0];
        m_font->m_scale[1] = text->scale[1];
    }
    ~UiTextTrack();                                                      // FS2 0x0024cbd0
    void draw(UiDrawer *drawer);                                         // FS2 0x0024cb80
    const float *getAnchor();                                            // FS2 0x0024d4c0

    const u_short *getText();                                            // FS2 0x0024c6e0
    // A wide string, copied or borrowed; the message is left as it was.
    int setText(const u_short *text, bool copy);                         // FS2 0x0024c700
    // A copied wide string at (x, y); the message becomes kUiTextLiteral.
    int setTextAt(float x, float y, const u_short *text);                // FS2 0x0024c7c0
    // Message index of an fMsg file (null: none). Answers it, or -1.
    int setMessage(int index, void *msgFile);                            // FS2 0x0024c870
    // The message called name, plus offset, at (x, y). Answers it, or -1.
    int setMessageByName(float x, float y, const char *name, void *msgFile, int offset); // FS2 0x0024ca00

    UiFont         *m_font;     // FS2 +0x14
    int             m_message;  // FS2 +0x18: fMsg index, kUiTextLiteral; -1 none (nothing drawn)
    float           m_x;        // FS2 +0x1c: UiDrawer::drawText's position
    float           m_y;        // FS2 +0x20
    u_char          m_visible;  // FS2 +0x24
    // The images' ids, or (negative record index) the init-pack name the
    // image came by: the ROM keeps whichever it had in hand.
    intptr_t        m_fontKey;  // FS2 +0x28
    intptr_t        m_glyphKey; // FS2 +0x2c
    int             m_30;       // FS2 +0x30
    const u_short  *m_text;     // FS2 +0x34: also UiFont::m_text
    u_char          m_ownsText; // FS2 +0x38: m_text is a new[] copy
};

// Kind 0x1f: triangles (list / fan / strip) in one colour.
// layout: host (vptr; EE fields at the offsets noted)
class UiTrianglesFlatTrack : public UiTrack
{
public:
    UiTrianglesFlatTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiTrianglesFlatTrack();                                             // FS2 0x0024cd10
    void draw(UiDrawer *drawer);                                         // FS2 0x0024cc70
    const float *getAnchor();                                            // FS2 0x0024d4d0
};

// Kind 2: triangles, vertex colours.
// layout: host (vptr; EE fields at the offsets noted)
class UiTrianglesTrack : public UiTrack
{
public:
    UiTrianglesTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiTrianglesTrack();                                                 // FS2 0x0024ce20
    void draw(UiDrawer *drawer);                                         // FS2 0x0024cd80
    const float *getAnchor();                                            // FS2 0x0024d4e0
};

// Kind 0x1e: sprites in one colour.
// layout: host (vptr; EE fields at the offsets noted)
class UiSpriteFlatTrack : public UiTrack
{
public:
    UiSpriteFlatTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiSpriteFlatTrack();                                                // FS2 0x0024ced0
    void draw(UiDrawer *drawer);                                         // FS2 0x0024ce80
    const float *getAnchor();                                            // FS2 0x0024d4f0
};

// Kind 1: sprites, vertex colours.
// layout: host (vptr; EE fields at the offsets noted)
class UiSpriteTrack : public UiTrack
{
public:
    UiSpriteTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiSpriteTrack();                                                    // FS2 0x0024cf90
    void draw(UiDrawer *drawer);                                         // FS2 0x0024cf40
    const float *getAnchor();                                            // FS2 0x0024d500
};

// Kind 0x1d: a line strip in one colour.
// layout: host (vptr; EE fields at the offsets noted)
class UiLineStripFlatTrack : public UiTrack
{
public:
    UiLineStripFlatTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiLineStripFlatTrack();                                             // FS2 0x0024d060
    void draw(UiDrawer *drawer);                                         // FS2 0x0024cff0
    const float *getAnchor();                                            // FS2 0x0024d510
};

// Kind 0: a line strip, vertex colours.
// layout: host (vptr; EE fields at the offsets noted)
class UiLineStripTrack : public UiTrack
{
public:
    UiLineStripTrack(S2dTrackRec *rec) : UiTrack(rec) {}
    ~UiLineStripTrack();                                                 // FS2 0x0024d140
    void draw(UiDrawer *drawer);                                         // FS2 0x0024d0d0
    const float *getAnchor();                                            // FS2 0x0024d520
};

#endif /* _GAME_UITRACK_H */
