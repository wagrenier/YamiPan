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
struct ButtonMap;               // game/sysdata.h

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
    // The lines of the text: -1 without glyphs, 0 without text.
    int countLines();                                                    // FS2 0x0025fa10 (game/uiwidget.cpp)

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

// The game states a layout can test by name (S2dCond_GameState:
// UiLayout::testGameState): the index into the name table (0x599490,
// filled by the TU's static initializer 0x310b60), as the names say.
enum UiGameState
{
    UiState_WideScreen = 0,         // r16_9: GameOptions::wideScreen
    UiState_FloorFirst = 1,         // rF01..rF09, rFDF: the player's floor is n (0..9)
    UiState_FloorLast = 10,
    UiState_ItemFirst = 11,         // rI00..rI07: the inventory has more than n kinds
    UiState_ItemLast = 18,
    UiState_WeaponLong = 19,        // rW00: a long weapon carried
    UiState_WeaponShort = 20,       // rW01: a short weapon carried
    UiState_DoorFirst = 21,         // rDunknown..rDNone: the door named by the argument is UiDoorState n
    UiState_DoorLast = 25,
    UiState_CheckPoint = 26,        // rCheckPoint: StageResult::hasAttemptFlag
    UiState_Hint = 27,              // rHint: GameOptions::hints
    UiState_MapShowsPlayer = 28,    // rMapP
    UiState_MsgUpLimit = 29,        // the argument's scroll widget at its top
    UiState_MsgDownLimit = 30,      // at its bottom
    UiState_MsgUseScroll = 31,      // it scrolls at all
    UiState_InGame = 32,            // rInGame: in a stage
    UiState_ProgramStatusNone = 33, // rProgramStatusNone, 00..03: the argument widget's status is n - 1
    UiState_ProgramStatusLast = 37,
    UiState_MapKey0 = 38,           // rMapKey0 / rMapKey1: the stage's navi trigger 0 / 1 is the argument (k0 / k1 / k2)
    UiState_MapKey1 = 39,
    UiState_ConnectedPad = 40,      // rConnectedPad: pad 0 is a DualShock 2 in pressure mode
    UiState_Count = 41,             // 0x31abe8
};

// What the door state names stand for (getDoorState).
enum UiDoorState
{
    UiDoor_Unknown = 0,             // rDunknown
    UiDoor_Unlocked = 1,            // rDunlock
    UiDoor_Blocked = 2,             // rDblocked
    UiDoor_Locked = 3,              // rDlock
    UiDoor_None = 4,                // rDNone: no such door, or not a door
};

// The state of the door called name (in a stage: the stage object's;
// outside one a preview from its 11th character).
int getDoorState(const char *name);                                      // FS2 0x002626f0

// The inventory as the S2D shows it: up to eight kinds of item, the
// player's items merged by name in list order.
constexpr int kUiItemSlots = 8;
// layout: host -- EE 0x14.
struct UiItemSlot
{
    u_char      used;           // FS2 +0x00
    const char *name;           // FS2 +0x04: StageObject::m_name
    int         count;          // FS2 +0x08: summed over the merged items
    int         max;            // FS2 +0x0c: ItemDef::maxCount
    u_char      showsCount;     // FS2 +0x10: ItemDef::stackKind > 1
};
// Fills slots (all cleared first) from the player's items; the kinds.
int scanItems(UiItemSlot slots[kUiItemSlots]);                           // FS2 0x002628a0

// The two weapons as the S2D shows them (long, short).
constexpr int kUiWeaponSlots = 2;
// layout: host -- EE 0x14.
struct UiWeaponSlot
{
    u_char      used;           // FS2 +0x00: always set
    const char *name;           // FS2 +0x04: null none
    u_char      isFirearm;      // FS2 +0x08: ItemKind_FirearmFirst..Last
    int         ammoA;          // FS2 +0x0c: StageObject::m_ammoA
    int         ammoB;          // FS2 +0x10: StageObject::m_ammoB
};
// Fills both slots from the player's weapons; answers 2.
int scanWeapons(UiWeaponSlot slots[kUiWeaponSlots]);                     // FS2 0x00262a90

// The options menu's working copy of the key configuration (bss
// 0x599470): what the S2D's rKcL* sprites show while it is edited.
extern ButtonMap gUiButtonMapDraft;
ButtonMap *getButtonMapDraft();                                          // FS2 0x00263a70
// The stage pack's data by name (null outside a stage).
void *getStagePackData(const char *name);                                // FS2 0x00262df0

// The game values the scripts read, write and test (S2dOp_ReadGameValue /
// WriteGameValue, S2dCond_GameValue). setGameValue takes (kind, value, i,
// j), getGameValue (kind, i, j, expected): the tested value, which only the
// clock kinds look at. Most are the system data's fields (game/sysdata.h);
// the names say which. Kinds 5, 6, 21, 22, 25, 28, 30, 31 and 34 are
// read only.
enum GameValueKind
{
    GameValue_SoundMode = 0,        // GameOptions::soundMode
    GameValue_Option08 = 1,         // GameOptions::_08 byte i (1: the pad byte after it); written as a bool
    GameValue_ScreenX = 2,          // GameOptions::screenX; writing moves the display
    GameValue_ScreenY = 3,          // GameOptions::screenY
    GameValue_Option10 = 4,         // GameOptions::_10
    GameValue_None = 5,             // reads 0
    GameValue_BrightnessValue = 6,  // GameOptions::getBrightnessValue
    GameValue_Language = 7,         // getLanguage / setLanguage
    GameValue_WideScreen = 8,       // GameOptions::wideScreen
    GameValue_MapMode = 9,          // GameOptions::mapMode
    GameValue_Hints = 10,           // GameOptions::hints
    GameValue_Option4c = 11,        // GameOptions::_4c (bool)
    GameValue_Archive = 12,         // SaveData::archive[i]; from kGameValueArchiveStage, a stage's flags
    GameValue_StageFlags = 13,      // SaveData::stageFlags: (bits, stage) / (stage)
    GameValue_Variant3ac = 14,      // SaveData::variant3ac (written through pickVariant3ac)
    GameValue_Flag = 15,            // FlagTable entry: (value, index) / (index)
    GameValue_Medals1 = 16,         // SaveData::medals[1][i][j]: written ORed in
    GameValue_Medals2 = 17,         // SaveData::medals[2][i][j]
    GameValue_SelectStage = 18,     // StageSelect::stage
    GameValue_SelectKind = 19,      // StageSelect::kind
    GameValue_SelectVariant = 20,   // StageSelect::variant
    GameValue_NaviValue = 21,       // NaviMenu::values[i]; 0 without the menu
    GameValue_NaviBits = 22,        // NaviMenu::bits[i] & j
    GameValue_Option4d = 23,        // GameOptions::_4d (bool)
    GameValue_Unlock468 = 24,       // SaveData::unlock468 (bool)
    GameValue_Fixed = 25,           // kGameValueFixed[i], else 0xfe (unread what for)
    GameValue_ButtonMap = 26,       // GameOptions::buttonMap: (button, action) / (action)
    GameValue_ButtonMapDraft = 27,  // gUiButtonMapDraft
    GameValue_PerfectBits = 28,     // StageResult::getPerfectBits
    GameValue_StageClock = 29,      // StageSelect::_10 (the count-up clock); stage 29 reads 1
    GameValue_NaviFlagF0 = 30,      // NaviMenu::flagsF0[i] != 0
    GameValue_NaviFlagE0 = 31,      // NaviMenu::flagsE0[i] != 0
    GameValue_Option14 = 32,        // GameOptions::_14 (bool)
    GameValue_Option15 = 33,        // GameOptions::_15 (bool)
    GameValue_BeatsRecord = 34,     // StageResult::beatsRecord
    GameValue_Option1c = 35,        // GameOptions::_1c (bool)
    GameValue_Clock2Amount = 36,    // StageResult::clock2's pending amount (kGameValueClock*)
    GameValue_ClockAmount = 37,     // StageResult::clock's
    GameValue_Brightness = 38,      // GameOptions::brightness
    GameValue_Option04 = 39,        // GameOptions::_04
    GameValue_Count = 40,
};

// GameValue_Archive: ids from here read stage (id - 150)'s flags; writes
// below it index the save from SaveData::archive on (101..149 land in
// SaveData::stageFlags, as in the ROM).
constexpr int kGameValueArchiveStage = 150;
// The archive whose writes also draw SaveData::variant3ac.
constexpr int kGameValueArchiveVariant = 64;

// The clock kinds' values: an amount index arms the clock (StageClock::
// state 1, target the amount); these three act on the armed amount and,
// as getGameValue's expected value, test for what they did.
enum GameValueClock
{
    GameValueClock_Cancel = 253,    // state |= StageClockState_Cancelled
    GameValueClock_Add = 254,       // value += target, target 0; Added | Settled
    GameValueClock_Mark = 255,      // state |= StageClockState_Marked
};

int  getGameValue(int kind, int i, int j, int expected);                // FS2 0x002633f0
void setGameValue(int kind, int value, int i, int j);                    // FS2 0x00262e70

// A value text variable (the S2D's rItem00_N .. rBonusScore: counts,
// ammo, the objective, times and scores) as 16-bit text into out; false
// when name is none of them. UiWidget::expandTextVariable's last resort.
bool formatTextValue(const char *name, u_short *out);                    // FS2 0x002612c0

// Stage 29, the one whose clock2 keeps the highest score.
bool isHighestWinsStage(int stage);                                      // FS2 0x00261cf0

// The player as the map widgets show it: map position (x, -z) / 10,
// heading pi - yaw, and whether the map shows the player at all (inline in
// the tracks' binds and UiLayout::testGameState).
void   getPlayerMapPos(float pos[2]);                                    // FS2 0x002625c0
u_char mapShowsPlayer();                                                 // FS2 0x00262640
float  getPlayerHeading();                                               // FS2 0x002627e0
// The weapon preview's next step: outside a stage the weapon sprites'
// cycle (1), in one the player's motion set (0; 1 without a player).
int    stepWeaponPreview();                                              // FS2 0x00262840

// The key-configuration draft from / back into the options (and the pad).
void applyButtonMapDraft();                                              // FS2 0x00263a80
void loadButtonMapDraft();                                               // FS2 0x00263b10

#endif /* _GAME_UIGAME_H */
