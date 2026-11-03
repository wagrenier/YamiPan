/* ==========================================================================
 *  game/sysdata.h
 *
 *  The game's system data (game/sysdata.cpp, 0x23af30..0x23c020): one
 *  0x920-byte block at 0x598b30 holding the save data (stage flags, the
 *  best records and medals per stage), the play record (which stage, record
 *  kind and variant is being played), the options with their button map,
 *  and a 64-int flag table; plus the language and the region strings.
 * ======================================================================== */
#ifndef _GAME_SYSDATA_H
#define _GAME_SYSDATA_H

#include <stddef.h>

#include "sdk/scetypes.h"

// Stage numbers 0..29 index the record and medal tables; the extra stages
// 80..94 are valid play records too (StageSelect::isValid).
constexpr int kStageCount = 30;
constexpr int kStageExtraFirst = 80;
constexpr int kStageExtraLast = 94;
// The stage whose record keeps the highest value (every other stage keeps
// the lowest: a time, by the default 3599 = 59:59 of kind 0).
constexpr int kStageHighestWins = 29;

// A record is kept per kind (0..2) and variant (0..1) of a stage.
constexpr int kRecordKinds = 3;
constexpr int kRecordVariants = 2;
// What SaveData::getRecord answers for an index of -1 (kind, variant) or -2
// (stage).
constexpr u_int kNoRecord = 123456;

// The archives (2dEU/archive%s.d2b) the save counts, numbered 1..100.
constexpr int kArchiveCount = 100;

// SaveData's stage flag bytes (the stage number indexes them).
constexpr int kStageFlagCount = 69;
// Two flags past the stages; with both set, clearing a stage marks the
// save's byte +0x468 (SystemData::recordStageResult).
constexpr int kStageFlagUnlockA = 66;
constexpr int kStageFlagUnlockB = 67;
// SaveData::testStageFlags: with this bit in the mask, every other mask bit
// must be set (without it, any).
constexpr u_int kStageFlagTestAll = 0x40;

// GameOptions::soundMode, as SoundConfig::setOutputMode takes it.
enum SoundOutput
{
    SoundOutput_Mono     = 0,
    SoundOutput_Stereo   = 1,
    SoundOutput_Surround = 2,   // sdpr2SetSurround(1)
};

// The language codes getLanguageCode returns (string table 0x2fac40).
enum Language
{
    Language_J  = 0,
    Language_US = 1,
    Language_KR = 2,
    Language_CN = 3,
    Language_Ct = 4,
    Language_EN = 5,            // this (European) build's default
    Language_DE = 6,
    Language_FR = 7,
    Language_ES = 8,
    Language_IT = 9,
    Language_Count
};

constexpr int kButtonMapActions = 8;
// Action -> button index table; GamePad keeps its own and points at the
// options' one (GamePad::setButtonMap).
struct ButtonMap
{
    void setDefaults();                                                  // FS2 0x0023bd00
    void assign(int action, int newButton);                              // FS2 0x0023bf80
    static int defaultButton(int action);                                // FS2 0x0023bf00

    int button[kButtonMapActions];  // FS2 +0x00: defaults 6, 5, 4, 7, 3, 1, 2, 0
};
static_assert(sizeof(ButtonMap) == 0x20, "ButtonMap: EE 0x20");

// GameOptions::mapMode: whether the map shows where the player is.
enum MapMode
{
    MapMode_Never  = 0,
    MapMode_Always = 1,
    // Any other value: when the player's map holder says so (UiLayout's rMapP).
};

// The game options at 0x599000 (SystemData +0x4d0). The fields without a
// name are only set to their defaults and compared here.
struct GameOptions
{
    void setDefaults();                                                  // FS2 0x0023be70
    void init();                                                         // FS2 0x0023bef0
    void applySound();                                                   // FS2 0x0023bc50
    void applyPadAndScreen();                                            // FS2 0x0023bcb0
    int  equals(const GameOptions *other) const;                         // FS2 0x0023bd40
    u_char getBrightnessValue() const;                                   // FS2 0x0023bff0

    int       soundMode;        // FS2 +0x00: SoundOutput, default stereo
    int       _04;              // FS2 +0x04: default 1
    u_char    _08;              // FS2 +0x08: default 1
    short     screenX;          // FS2 +0x0a: display offset (gfxSetDisplayOffset), default 0
    short     screenY;          // FS2 +0x0c: default 0
    int       _10;              // FS2 +0x10: default 2
    u_char    _14;              // FS2 +0x14: default 1
    u_char    _15;              // FS2 +0x15: default 0
    int       brightness;       // FS2 +0x18: index into the 19-step table 0x2fac20 (guess: brightness), default 9
    u_char    _1c;              // FS2 +0x1c: default 1
    int       wideScreen;       // FS2 +0x20: 1 the 16:9 screen (the S2D's r16_9), default 0
    ButtonMap buttonMap;        // FS2 +0x24
    int       mapMode;          // FS2 +0x44: MapMode (the S2D's rMapP), default always
    int       hints;            // FS2 +0x48: 1 hints on (the S2D's rHint), default 1
    u_char    _4c;              // FS2 +0x4c: default 1
    u_char    _4d;              // FS2 +0x4d: compared, never set by setDefaults
};
static_assert(offsetof(GameOptions, screenX) == 0x0a, "GameOptions::screenX");
static_assert(offsetof(GameOptions, _10) == 0x10, "GameOptions::_10");
static_assert(offsetof(GameOptions, brightness) == 0x18, "GameOptions::brightness");
static_assert(offsetof(GameOptions, wideScreen) == 0x20, "GameOptions::wideScreen");
static_assert(offsetof(GameOptions, buttonMap) == 0x24, "GameOptions::buttonMap");
static_assert(offsetof(GameOptions, mapMode) == 0x44, "GameOptions::mapMode");
static_assert(offsetof(GameOptions, _4d) == 0x4d, "GameOptions::_4d");
static_assert(sizeof(GameOptions) == 0x50, "GameOptions: EE 0x50");

// The save data at 0x598b30 (SystemData +0).
struct SaveData
{
    void setDefaults();                                                  // FS2 0x0023b280
    int  equals(const SaveData *other) const;                            // FS2 0x0023afa0
    bool markUnlock468();                                                // FS2 0x0023afe0
    bool stageHasVariant1(int stage) const;                              // FS2 0x0023b000
    int  allRecordsSet() const;                                          // FS2 0x0023b040
    bool isRecordSet(int kind, int variant, int stage) const;            // FS2 0x0023b1b0
    int  clearBit(int kind, int variant) const;                          // FS2 0x0023b510
    bool testStageFlags(int stage, u_int mask) const;                    // FS2 0x0023b530
    int  pickVariant3ac(int variant);                                    // FS2 0x0023b580
    bool hasArchive(int id) const;                                       // FS2 0x00296a00 (game/progress.cpp)

    // Inline in the ROM (allRecordsSet, isRecordSet, recordStageResult).
    u_int getRecord(int kind, int variant, int stage) const
    {
        if (kind == -1 || variant == -1 || stage == -2)
            return kNoRecord;
        return record[kind][variant][stage];
    }

    u_int  playTime;            // FS2 +0x000: ticks played, stops at -1 (StageResult::tick); cleared by setDefaults
    u_char archive[kArchiveCount + 1];                     // FS2 +0x004: nonzero when collected, [0] unused (hasArchive)
    u_char stageFlags[kStageFlagCount];                    // FS2 +0x069: clearBit bits per stage
    u_char _0ae[0x27];          // FS2 +0x0ae: then 3 bytes padding (the implicit operator= copies 0x27)
    u_int  record[kRecordKinds][kRecordVariants][kStageCount];  // FS2 +0x0d8: best per stage, stageDefaultRecord when fresh
    int    _3a8;                // FS2 +0x3a8
    int    variant3ac;          // FS2 +0x3ac: 0..3, pickVariant3ac
    int    _3b0;                // FS2 +0x3b0
    u_char medals[kRecordKinds][kRecordVariants][kStageCount];  // FS2 +0x3b4: StageResult::getPerfectBits ORed in
    u_char unlock468;           // FS2 +0x468: seeded from memcardGetState (a first-game save on the card), set once by markUnlock468
    int    _46c[16];            // FS2 +0x46c
    u_char _4ac[16];            // FS2 +0x4ac
};
static_assert(offsetof(SaveData, archive) == 0x004, "SaveData::archive");
static_assert(offsetof(SaveData, stageFlags) == 0x069, "SaveData::stageFlags");
static_assert(offsetof(SaveData, record) == 0x0d8, "SaveData::record");
static_assert(offsetof(SaveData, _3a8) == 0x3a8, "SaveData::_3a8");
static_assert(offsetof(SaveData, variant3ac) == 0x3ac, "SaveData::variant3ac");
static_assert(offsetof(SaveData, medals) == 0x3b4, "SaveData::medals");
static_assert(offsetof(SaveData, unlock468) == 0x468, "SaveData::unlock468");
static_assert(offsetof(SaveData, _46c) == 0x46c, "SaveData::_46c");
static_assert(offsetof(SaveData, _4ac) == 0x4ac, "SaveData::_4ac");
static_assert(sizeof(SaveData) == 0x4bc, "SaveData: EE 0x4bc");

// The play record at 0x598fec (SystemData +0x4bc): the stage being played
// and which of its records it plays for.
// StageSelect::kind, read as the difficulty from the letters the path
// builders give it (game/gamepaths.cpp: e / n / h).
enum StageKind
{
    StageKind_Easy   = 0,
    StageKind_Normal = 1,       // the default
    StageKind_Hard   = 2,       // and anything above
};

struct StageSelect
{
    void setDefaults();                                                  // FS2 0x0023af80
    bool isValid() const;                                                // FS2 0x0023af30

    int    stage;               // FS2 +0x00: default 1
    int    kind;                // FS2 +0x04: StageKind, record kind 0..2, default 1
    int    variant;             // FS2 +0x08: record variant 0..1, default 0
    int    _0c;                 // FS2 +0x0c: default -1
    u_char _10;                 // FS2 +0x10: default 0
};
static_assert(sizeof(StageSelect) == 0x14, "StageSelect: EE 0x14");

// 256 ints at 0x599050 (SystemData +0x520), cleared at boot and on a new
// game.
constexpr int kFlagTableSize = 256;   // clear counts ints to 0x100; the save copies 0x400 bytes
struct FlagTable
{
    void clear();                                                        // FS2 0x0023ba90
    void set(int index, int value);                                      // FS2 0x0023ba70
    int  get(int index) const;                                           // FS2 0x0023ba80
    int  countListed() const;                                            // FS2 0x0023ba20

    int value[kFlagTableSize];  // FS2 +0x00
};
static_assert(sizeof(FlagTable) == 0x400, "FlagTable: EE 0x400");

// The whole block at 0x598b30; every getter returns a part of it.
struct SystemData
{
    void recordStageResult();                                            // FS2 0x0023b630

    SaveData    save;           // FS2 +0x000
    StageSelect select;         // FS2 +0x4bc
    GameOptions options;        // FS2 +0x4d0
    FlagTable   flags;          // FS2 +0x520
};
static_assert(offsetof(SystemData, select) == 0x4bc, "SystemData::select");
static_assert(offsetof(SystemData, options) == 0x4d0, "SystemData::options");
static_assert(offsetof(SystemData, flags) == 0x520, "SystemData::flags");
static_assert(sizeof(SystemData) == 0x920, "SystemData: EE 0x920");

extern SystemData gSystemData;      /* bss 0x598b30 */

void         systemDataInit();                                           // FS2 0x0023bb50
SystemData  *getSystemData();                                            // FS2 0x0023bb30
SaveData    *getSaveData();                                              // FS2 0x0023bb40
StageSelect *getStageSelect();                                           // FS2 0x0023bb10
GameOptions *getGameOptions();                                           // FS2 0x0023baf0
FlagTable   *getFlagTable();                                             // FS2 0x0023bad0
void         setLanguage(int language);                                  // FS2 0x0023bbd0
int          getLanguage();                                              // FS2 0x0023bbe0
const char  *getLanguageCode(int language);                              // FS2 0x0023bc30
const char  *getFileSuffix(int alternate);                               // FS2 0x0023bbf0
const char  *getRegionCode();                                            // FS2 0x0023bc20
int          sysDataReturnZero();                                        // FS2 0x0023c010

#endif /* _GAME_SYSDATA_H */
