/* ==========================================================================
 *  game/uigame.h
 *
 *  The S2D UI's game side (game/uigame.cpp, 0x25fa50..0x2640e8): what the
 *  layout player (game/uilayout.h) and its tracks (game/uitrack.h) ask of
 *  the game. Here: the font a text track draws with, the two resource files
 *  the S2D images can be besides plain TIM2 pictures -- fMsg message files
 *  (named 16-bit strings) and TimM sprite sheets (named rects of TIM2
 *  pictures) -- and the game hooks: in a stage or not, the stage pack's
 *  data, the game values the scripts read and write. The layout's own
 *  hooks (sounds, game values, game state) and the tracks' game targets
 *  are members declared with their classes.
 *
 *  Both resource files relocate in place once, like the S2D file: every
 *  pointer is an S2dRef (game/s2ddata.h), so they keep their EE layout on
 *  the host (asserted; the magic and offsets checked against ROM.001).
 *  The file and class names are the port's (no source string in the range).
 * ======================================================================== */
#ifndef _GAME_UIGAME_H
#define _GAME_UIGAME_H

#include <stddef.h>

#include "game/s2ddata.h"
#include "sdk/scetypes.h"

class GsTexture;                // game/gsutil.h

// ──────────────────────────────────────────────────────────────────────
// The font.

// A font: its glyph table and texture, the text it draws and its spacing
// (EE 0x14 bytes).
// layout: host
class UiFont
{
public:
    UiFont(const void *glyphs, const void *image);                       // FS2 0x0025fac0: spacing 2, 0
    ~UiFont();                                                           // FS2 0x0025fa50: deleting (obj, 1)

    const void     *m_glyphs;   // FS2 +0x00: a FontData (game/fontdata.h)
    GsTexture      *m_texture;  // FS2 +0x04: owned
    const u_short  *m_text;     // FS2 +0x08: UiTextTrack::m_text
    // Pixels taken off each glyph's advance and each line's height
    // (UiDrawer::drawText): the record's.
    float           m_spacing[2]; // FS2 +0x0c
};

// ──────────────────────────────────────────────────────────────────────
// fMsg message files: named 16-bit strings. In the file a name / text is
// an index into its table (negative: none) and a table entry an offset
// from its base; fMsgRelocate makes them pointers.

// One message.
struct FMsgEntry
{
    S2dRef<const char>    name; // FS2 +0x0
    S2dRef<const u_short> text; // FS2 +0x4: null none
};
static_assert(sizeof(FMsgEntry) == 0x8, "FMsgEntry: EE 0x8");

struct FMsgFile
{
    char      magic[4];         // FS2 +0x00: "fMsg"
    u_char    relocated;        // FS2 +0x04
    u_char    _05[3];
    float     _08;              // FS2 +0x08: 1.04 on the disc (a version?); unread
    float     _0c;              // FS2 +0x0c: 1.0 on the disc; unread
    int       count;            // FS2 +0x10: entries
    int       nameCount;        // FS2 +0x14
    S2dRef<S2dRef<const char> >    names;    // FS2 +0x18: offset from the file
    int                            nameBase; // FS2 +0x1c: the names' offsets are from here
    int       textCount;        // FS2 +0x20
    S2dRef<S2dRef<const u_short> > texts;    // FS2 +0x24: offset from the file
    int                            textBase; // FS2 +0x28
    int       _2c;              // FS2 +0x2c: 0 on the disc
    FMsgEntry entries[1];       // FS2 +0x30: count
};
static_assert(offsetof(FMsgFile, count) == 0x10, "FMsgFile::count");
static_assert(offsetof(FMsgFile, names) == 0x18, "FMsgFile::names");
static_assert(offsetof(FMsgFile, texts) == 0x24, "FMsgFile::texts");
static_assert(offsetof(FMsgFile, entries) == 0x30, "FMsgFile::entries");

// Relocates an fMsg file in place (once). False when it is not one.
bool fMsgRelocate(void *file);                                           // FS2 0x0025fbd0
// The index of the message called name (any case); -1 none.
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

// ──────────────────────────────────────────────────────────────────────
// TimM sprite sheets: named rects of the sheet's TIM2 pictures (the item
// icons: "LW_NIGHTSTICK" on S_OBJ_011.TM2 ...). Relocated as fMsg files.

// A sprite: which picture, and its rect in it (texels).
struct TimSheetSprite
{
    short name;                 // FS2 +0x0: TimSheet::names index
    short image;                // FS2 +0x2: TimSheet::images index
    short u, v;                 // FS2 +0x4
    short w, h;                 // FS2 +0x8
    int   _0c;                  // FS2 +0xc: 0 on the disc; unread
};
static_assert(sizeof(TimSheetSprite) == 0x10, "TimSheetSprite: EE 0x10");
static_assert(offsetof(TimSheetSprite, w) == 0x8, "TimSheetSprite::w");

// A picture of the sheet.
struct TimSheetImage
{
    S2dRef<const char> name;    // FS2 +0x0: its file name ("S_OBJ_011.TM2")
    S2dRef<const void> image;   // FS2 +0x4: the TIM2, what UiDrawer::setImage binds
};
static_assert(sizeof(TimSheetImage) == 0x8, "TimSheetImage: EE 0x8");

struct TimSheet
{
    char           magic[4];    // FS2 +0x00: "TimM"
    u_char         relocated;   // FS2 +0x04
    u_char         _05[3];
    float          _08;         // FS2 +0x08: 1.02 on the disc; unread
    float          _0c;         // FS2 +0x0c: 0.05 on the disc; unread
    int            spriteCount; // FS2 +0x10
    int            nameCount;   // FS2 +0x14
    S2dRef<S2dRef<const char> > names;   // FS2 +0x18: offset from the file
    int            nameBase;    // FS2 +0x1c: the names' offsets are from here
    int            imageCount;  // FS2 +0x20
    S2dRef<TimSheetImage> images;        // FS2 +0x24: offset from the file
    int            imageNameBase; // FS2 +0x28: the pictures' names are from here
    int            imageDataBase; // FS2 +0x2c: the pictures' TIM2s are from here
    TimSheetSprite sprites[1];  // FS2 +0x30: spriteCount
};
static_assert(offsetof(TimSheet, spriteCount) == 0x10, "TimSheet::spriteCount");
static_assert(offsetof(TimSheet, names) == 0x18, "TimSheet::names");
static_assert(offsetof(TimSheet, images) == 0x24, "TimSheet::images");
static_assert(offsetof(TimSheet, sprites) == 0x30, "TimSheet::sprites");

// Relocates a TimM sheet in place (once). False when it is not one.
bool timSheetRelocate(void *sheet);                                      // FS2 0x0025fe10
// The index of the sprite called name (any case); -1 none.
int  timSheetFind(void *sheet, const char *name);                        // FS2 0x0025fd60

// Inline in the ROM (findSprite): sprite index of a sheet, null out of
// range or when it is not a sheet.
inline const TimSheetSprite *timSheetSprite(TimSheet *sheet, int index)
{
    if (!timSheetRelocate(sheet))
        return nullptr;
    if (index < 0)
        return nullptr;
    if (index < sheet->spriteCount)
        return &sheet->sprites[index];
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The game hooks.

// In a stage (gStageScene set).
bool  inStage();                                                         // FS2 0x00262e60
// The stage pack's data by name (null outside a stage).
void *getStagePackData(const char *name);                                // FS2 0x00262df0

// The game values the scripts read, write and test (S2dOp_ReadGameValue /
// WriteGameValue, S2dCond_GameValue): kind and up to three arguments.
// The kinds are getGameValue's switch (unread); the one the layout hooks
// handle themselves:
enum GameValueKind
{
    GameValue_StageFlags = 13,  // SaveData::stageFlags: (set, stage, bits) / (expected, stage, mask)
};
int  getGameValue(int kind, int a, int b, int c);                       // FS2 0x002633f0
void  setGameValue(int kind, int a, int b, int c);                       // FS2 0x00262e70

#endif /* _GAME_UIGAME_H */
