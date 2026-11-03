/* ==========================================================================
 *  game/gamehooks.h
 *
 *  Subsystems main and its loop call that have no TU of their own yet:
 *  the rx runtime's game layer and SPK.ROM (game/spkrom.cpp), the empty
 *  debug hooks
 *  0x151270..0x151290, the scene controller's callees (game/scene.cpp), and
 *  the UI layout and stage object game/stageresult.cpp calls.
 *  Each moves to its TU's header when that TU is written. Minimal.
 * ======================================================================== */
#ifndef _GAME_GAMEHOOKS_H
#define _GAME_GAMEHOOKS_H

#include <stddef.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "game/fileloader.h"
#include "game/gamepaths.h"
#include "game/stageloop.h"
#include "game/stageobjlist.h"
#include "game/stagesound.h"
#include "game/streamring.h"
#include "game/uilayout.h"
#include "rx/rxfile.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"
#include "sound/sound.h"

struct StageSelect;             // game/sysdata.h

// rx boot: thread priorities given absolute, made relative to the caller.
void rxInit(const char *bootPath, int prio0, int prio1, int prio2);      // FS2 0x001527a0
void rxTerm();                                                           // FS2 0x00152750
// The rx file hooks (a file's size: 0 empty, < 0 missing; reading it whole
// into buf) are rx/rxfile.h's rxFileCallSizeHook 0x1556d0 /
// rxFileCallLoadHook 0x155690; the game reaches them through jump thunks of
// the same names beside rxTerm (0x152740 / 0x152730).

// Unpacks an "SLDI" block (LZ copies and literal runs) into dst; the
// unpacked size, -1 for another magic or a length that does not add up
// (unassigned@002b0d50; PackJob::load).
int sldiDecompress(const void *src, void *dst);                          // FS2 0x002d0a10

// Empty in retail: a stripped system's init / per-frame / teardown (guess).
void debugSysInit();                                                     // FS2 0x00151290
void debugSysUpdate();                                                   // FS2 0x00151270
void debugSysShutdown();                                                 // FS2 0x00151280

// True when the point (16-aligned xyzw) lies in the box whose world-to-unit-
// cube matrix is m (VU0 transform + vclip against w; game/boxgrid.cpp).
bool boxContainsPoint(const float m[4][4], const float p[4]);            // FS2 0x002362a0

// What game/scene.cpp calls.
// The resident menu pack: dataEU/packmenuEU.pak at 0x31ae98, loaded only
// while not loaded (loadInitPack: game/gamepaths.h).
void loadMenuPack();                                                     // FS2 0x002c3e10
// The scene table's only scene, "Menu (Pack File)": the menu flow 0x2b5770(1).
void menuSceneThread(void *arg);                                         // FS2 0x002b5740

// 2dEU/archive<lang>.d2b and 2dEU/option<lang>.d2b (static buffers).
const char *archiveLayoutPath();                                         // FS2 0x002ec7f0
const char *optionLayoutPath();                                          // FS2 0x002ec660
// The event movies stageMoviePath's table names (game/gamepaths.cpp):
// movieEU/ev<id><lang>.d2b in the static buffer 0x5a5460.
const char *eventMoviePath0102();                                        // FS2 0x002ec520
const char *eventMoviePath0202();                                        // FS2 0x002ec4d0
const char *eventMoviePath1206();                                        // FS2 0x002ec480
const char *eventMoviePath1404();                                        // FS2 0x002ec430
const char *eventMoviePath1405();                                        // FS2 0x002ec3e0
const char *eventMoviePath1505();                                        // FS2 0x002ec390
const char *eventMoviePath1506();                                        // FS2 0x002ec340
const char *eventMoviePath1608();                                        // FS2 0x002ec2f0
const char *eventMoviePath2103();                                        // FS2 0x002ec2a0
const char *eventMoviePath2205();                                        // FS2 0x002ec250
const char *eventMoviePath2301();                                        // FS2 0x002ec200
const char *eventMoviePath2502();                                        // FS2 0x002ec1b0
const char *eventMoviePath2506();                                        // FS2 0x002ec160

// getInitPackItem indices game/stageresult.cpp asks for (their content is
// not read yet).
enum InitPackItem
{
    InitItem_21     = 0x15,     // getInitItem21
    InitItem_LangDE = 0x16,
    InitItem_LangEN = 0x17,     // every language but the four below
    InitItem_LangES = 0x18,
    InitItem_LangFR = 0x19,
    InitItem_LangIT = 0x1a,
};

// getLangPackItem indices: game/screen.cpp's loading indicator and dialogs,
// the clock layouts StageResult::begin builds.
enum LangPackItem
{
    LangItem_Loading        = 0,    // overlayShowLoading
    LangItem_Notice         = 1,    // noticeDialogStart
    LangItem_GameOver       = 2,    // gameOverDialogStart
    LangItem_GameOverAttempt = 3,   // the same while StageResult::hasAttemptFlag
    LangItem_ClockCountUp   = 4,
    LangItem_ClockCountdown = 5,
};

// The tables PackTables::parse reads out of packgameEU.pak (columns ラベル
// "label", ctype, SEtype). The element types are read from the MSL
// instances' destructors only (unverified): a ListMap value is a 4-byte key
// and a vector of 0x10-byte {4 bytes, string} entries, a NameMap value a
// string key and 4 trivially destroyed bytes.
// layout: host -- std::string; EE 0x10.
struct PackEntry
{
    int         id;             // FS2 +0x0 (unverified)
    std::string text;           // FS2 +0x4: MSL string, EE 0xc
};
typedef std::vector<PackEntry> EntryList;
typedef std::map<int, EntryList> ListMap;
typedef std::map<std::string, int> NameMap;

// layout: host -- EE 0x80: eight MSL maps of 0x10, ListMap at +0x00 / +0x20 /
// +0x40 / +0x60, NameMap at +0x10 / +0x30 / +0x50 / +0x70 (0x2c16b0 /
// 0x2c1710); the destructor is implicit (inline in loadGamePackTables).
class PackTables
{
public:
    PackTables();                                                        // FS2 0x002c0dc0
    void parse(PackFile *pack);                                          // FS2 0x002bcf10

    ListMap lists0;             // FS2 +0x00
    NameMap names0;             // FS2 +0x10
    ListMap lists1;             // FS2 +0x20
    NameMap names1;             // FS2 +0x30
    ListMap lists2;             // FS2 +0x40
    NameMap names2;             // FS2 +0x50
    ListMap lists3;             // FS2 +0x60
    NameMap names3;             // FS2 +0x70
};

// StageSound moved to game/stagesound.h (game/stagesound.cpp).

// StageObject::getKind: the ones game/uigame.cpp tests.
enum StageObjectKind
{
    StageObject_Door   = 2,     // StageObject::m_doorState is read
    StageObject_Kind21 = 0x15,  // left out of the inventory (unidentified)
};

// StageObject::m_doorState (a door's +0x2a8). The S2D asks for it as
// rDunknown / rDunlock / rDblocked / rDlock / rDNone (UiDoorState).
enum StageDoorState
{
    StageDoor_Unknown  = 0,
    StageDoor_Open1    = 1,     // both read as "unlock"
    StageDoor_Open2    = 2,
    StageDoor_Locked   = 3,
    StageDoor_Blocked  = 4,
};

// An object of the stage: doors, the player's items and weapons (one
// family: getKind is the same vtable slot in all). Only what
// game/uigame.cpp reads.
// layout: host -- EE: vptr at +0 (getKind is slot 123, vtable +0x1f4).
class StageObject
{
public:
    // A virtual in the ROM (slot 123); a plain stub until its TU.
    int getKind();                                                       // FS2 vtable +0x1f4
    // The deleting dtor (slot 2, flag 1: StageObjectList's teardown); a
    // plain stub until its TU.
    void destroy();                                                      // FS2 vtable +0x10

    u_char _000[0x20];
    char   m_name[0x1e0];       // FS2 +0x020: an item's / weapon's name (an ItemDef name)
    int    m_itemDef;           // FS2 +0x200: gItemDefs index
    u_char _204[0x4];
    int    m_id;                // FS2 +0x208: a character's number (StageObjectList::addCharacter)
    int    m_count;             // FS2 +0x20c: an item's count
    u_char _210[0x98];
    int    m_doorState;         // FS2 +0x2a8: a door's StageDoorState
    u_char _2ac[0x1a8];
    int    m_ammoB;             // FS2 +0x454: a firearm's (findSprite's second number)
    int    m_ammoA;             // FS2 +0x458: a firearm's (the first)
};

// A node of the player's item list (the player's +0x22c): the item through
// a handle.
// layout: host -- EE 8 bytes.
struct StageItemNode
{
    StageObject  **item;        // FS2 +0x0
    StageItemNode *next;        // FS2 +0x4
};

// The map's holder in the player's equipment: +0x240 -> ** -> +0x214
// (whether the map shows the player; the S2D's rMapP). Names are guesses.
// layout: host -- EE: +0x214 the flag.
struct StageMapState
{
    u_char _000[0x214];
    u_char showsPlayer;         // FS2 +0x214
};
// layout: host -- EE: +0x240 the state's handle.
struct StageMapHolder
{
    u_char          _000[0x240];
    StageMapState **state;      // FS2 +0x240
};

// The player's +0x224 object (its +0 the player). Guess: the motion set,
// picked by the weapon carried.
// layout: host -- contents unread.
struct PlayerMotion
{
    void refresh();                                                      // FS2 0x001e66c0
};

// The player character of a stage (the stage object's +0x78). Only what
// game/uigame.cpp reads.
// layout: host -- EE: +0x150 / +0x154 position x / z, +0x168 yaw, +0x22c
// the items, +0x234 the map's holder, +0x238 / +0x23c the two weapons.
struct StagePlayer
{
    u_char _000[0x150];
    float  posX;                // FS2 +0x150
    float  posZ;                // FS2 +0x154
    u_char _158[0x10];
    float  yaw;                 // FS2 +0x168
    u_char _16c[0xb8];
    PlayerMotion   *motion;     // FS2 +0x224
    u_char _228[0x4];
    StageItemNode  *items;      // FS2 +0x22c
    u_char _230[0x4];
    StageMapHolder *map;        // FS2 +0x234
    StageObject    *weapon[2];  // FS2 +0x238: long (rW00), short (rW01); null none
};

// The four characters the stage keeps across a reload (the stage object's
// +0x88; its +0 holds them at +0x1c..+0x28). save / restore go through the
// ids at 0x598b20 (StageObject::m_id, -1 none).
// layout: host -- contents unread.
struct StageParty
{
    void resetSaved();                                                   // FS2 0x001fe720: the four ids to -1
    void save();                                                         // FS2 0x001fe6d0
    void restore();                                                      // FS2 0x001fe6c0
};

// A singly linked list whose nodes {next, flags} carry a keep bit (the
// stage object's +0x30). Only what stageThread calls.
// layout: host -- EE: +0x4 head, +0x8 tail.
struct KeepList
{
    // Unlinks and deletes every node whose flags bit 0 is clear.
    void removeUnkept();                                                 // FS2 0x002bc5d0
};

// The stage object's +0xec: what a sub-scene starts from (the 0x287xxx
// classes, vtables 0x318020..; their own TU). Only what stageThread calls.
// layout: host -- EE: vptr at +0x2c.
class StageEvent
{
public:
    // A virtual in the ROM (slot 3, vtable +0x14); a plain stub until its
    // TU. What StageScene::enterSubScene builds the sub-scene from.
    void *getSubSceneData();
};

class Clock;                    // game/clock.h
class StageFiles;

// The current stage object (0x130 bytes): built by stageLoad 0x17fb90 for
// the stage and by stageThread for a sub-scene (game/stageloop.cpp).
// layout: host -- EE offsets in the comments.
struct StageScene
{
    StageScene(bool isSubScene, StageSound *sound);                      // FS2 0x0019c180
    ~StageScene();                                                       // FS2 0x0019bbe0: deleting

    // The frame: update, then draw for the active character.
    void update();                                                       // FS2 0x00199400
    void draw(StageObject *active);                                      // FS2 0x00198970
    // The frame while a dialog or menu thread runs.
    void drawPaused();                                                   // FS2 0x00198fc0
    // The character the player controls (null none).
    StageObject *getActiveCharacter();                                   // FS2 0x0019bbb0
    // Once, before the stage's first frame.
    void beginPlay();                                                    // FS2 0x001963a0
    void setupCameras();                                                 // FS2 0x00199f00
    void enterSubScene(void *data);                                      // FS2 0x00195e10
    void clearSubSceneRequest();                                         // FS2 0x00196050: subSceneRequest = 0

    bool isGameOver();                                                   // FS2 0x001956e0
    bool isCleared();                                                    // FS2 0x00195640
    // A sub-scene whose own scene (subScene) has ended.
    bool isSubSceneDone();                                               // FS2 0x00195d80
    // Start / select on pad 0 while the stage allows its menu.
    bool wantsPauseMenu();                                               // FS2 0x00195a50
    bool wantsMapMenu();                                                 // FS2 0x001958b0

    // The floor the player is on (the S2D's rF01..rFDF: 0..9); negative
    // none.
    int getFloor(StagePlayer *player);                                   // FS2 0x0019aea0

    u_char           isSubScene;      // FS2 +0x00: the constructor's first argument
    u_char           _01[0x3];
    int              subSceneRequest; // FS2 +0x04: set: stageThread builds a sub-scene
    void            *subScene;        // FS2 +0x08: a sub-scene's own scene (enterSubScene); null once it ended
    u_char           _0c[0x4];
    StageObjectList *objects;         // FS2 +0x10
    u_char           _14[0x10];
    StageSound      *sound;           // FS2 +0x24
    u_char           _28[0x8];
    KeepList        *keepList;        // FS2 +0x30
    u_char           _34[0x2c];
    Clock           *clock;           // FS2 +0x60: paused while a menu runs
    u_char           _64[0x14];
    StagePlayer     *player;          // FS2 +0x78
    u_char           _7c[0xc];
    StageParty      *party;           // FS2 +0x88
    u_char           _8c[0x60];
    StageEvent      *event;           // FS2 +0xec
    u_char           _f0[0x1c];
    StageScene      *parent;          // FS2 +0x10c: a sub-scene's (stageThread)
    StageFiles      *files;           // FS2 +0x110: a sub-scene's (stageThread)
};

// The navi menu (0x29a700..; sbss gp-0x79f8, null while there is none).
// Only what getGameValue reads; the extents are guesses (each array ends
// where the next one starts).
// The stage the menu points at is values[113] (+0x46c): inside values as
// its extent is guessed.
constexpr int kNaviMenuStageValue = (0x46c - 0x2a8) / 4;
struct NaviMenu
{
    int stage() const { return values[kNaviMenuStageValue]; }

    u_char _000[0x2a8];
    int    values[0xb7];        // FS2 +0x2a8: GameValue_NaviValue
    u_char bits[0x5c];          // FS2 +0x584: GameValue_NaviBits
    u_char flagsE0[0x10];       // FS2 +0x5e0: GameValue_NaviFlagE0
    u_char flagsF0[0x10];       // FS2 +0x5f0: GameValue_NaviFlagF0
};
static_assert(offsetof(NaviMenu, values) == 0x2a8, "NaviMenu::values");
static_assert(offsetof(NaviMenu, bits) == 0x584, "NaviMenu::bits");
static_assert(offsetof(NaviMenu, flagsE0) == 0x5e0, "NaviMenu::flagsE0");
static_assert(offsetof(NaviMenu, flagsF0) == 0x5f0, "NaviMenu::flagsF0");
NaviMenu *getNaviMenu();                                                 // FS2 0x0029efe0

// The stage's current objective as 16-bit text (the S2D's rPurpose):
// 0x196870's record + 4.
const u_short *getPurposeText();                                         // FS2 0x00195d50

// The memory-card menu's save manager (sbss 0x31ae94, gp-0x79dc; set and
// deleted by the menu 0x2b8600.., null while there is none): the S2D's
// rMcTotalTime0..3 show its saves' play times. Data the build must define.
class Memcard;                  // game/memcard.h
extern Memcard *gMenuMemcard;

// The stage's files (sbss 0x31adfc: getStageFiles; built by stageLoad over
// the stage loader, game/stageloop.cpp): the loaded lists and scripts.
// layout: host -- EE 0x160: +0 the loader, +4 a flag (1), 22 containers
// from +0xc.
class StageFiles
{
public:
    explicit StageFiles(FileLoader *loader);                             // FS2 0x001a4270
    ~StageFiles();                                                       // FS2 0x001a3f90: deleting

    // Loads and reads a script file (the stage's, through the loader).
    int  load(const char *path);                                         // FS2 0x001a37e0
    // spotlight / shadow_00.
    void loadCommon();                                                   // FS2 0x001a38e0
    // ../data/listfile/s%02dlist.lst for a stage (not a sub-scene).
    void loadList();                                                     // FS2 0x001a3a60
    // Called for a sub-scene where loadList would be: empty in retail.
    void loadSubSceneList();                                             // FS2 0x001a3db0

    FileLoader *loader;         // FS2 +0x0
};

// The stage's pause menu (start: a thread in ThreadSlot_Dialog running the
// stage screen layout, 0x20ad20) and map menu (select: 0x20b010), polled
// by the stage loop while StageScene::drawPaused keeps the stage on screen.
// What the pause menu's layout answered (pauseMenuResult).
enum PauseMenuChoice
{
    PauseMenu_Running  = -1,
    PauseMenu_Resume   = 0,
    PauseMenu_Restart  = 1,     // the stage loop: StageResult::restart
    PauseMenu_Continue = 2,     // the stage loop: StageResult::addContinue
    PauseMenu_Quit     = 3,     // keeps the BG movie screen up
    PauseMenu_Navi     = 4,     // a stage picked in the navigator
};
void pauseMenuStart();                                                   // FS2 0x0020acf0
int  pauseMenuResult();                                                  // FS2 0x0020acb0: PauseMenuChoice
void mapMenuStart();                                                     // FS2 0x0020afe0
int  mapMenuResult();                                                    // FS2 0x0020afa0: -1 while open

// The item definitions (bss 0x574c10, 160 x 0x20; loaded with the stage
// data, not read here). Names are guesses from findSprite and the
// inventory scans.
constexpr int kItemDefCount = 160;
struct ItemDef
{
    char   name[0x10];          // FS2 +0x00: the TimM sprite names use it
    int    kind;                // FS2 +0x10: ItemKind
    u_char _14[0x5];
    u_char stackKind;           // FS2 +0x19: > 1 shows the count (UiItemSlot::showsCount)
    u_char _1a[0x4];
    short  maxCount;            // FS2 +0x1e: UiItemSlot::max (guess: the most one can carry)
};
static_assert(sizeof(ItemDef) == 0x20, "ItemDef: EE 0x20");
extern ItemDef gItemDefs[kItemDefCount];  /* bss 0x574c10 */

// ItemDef::kind: 2..6 are firearms (they show ammo), 7 and up items the
// inventory lists; below 7 the weapons' sprite set.
enum ItemKind
{
    ItemKind_FirearmFirst = 2,
    ItemKind_FirearmLast  = 6,
    ItemKind_ItemFirst    = 7,
};

// ──────────────────────────────────────────────────────────────────────
// The sound-effect player outside a stage (0x292ac0.., in the run
// unassigned@002681e0, not cut yet). Only what game/uigame.cpp calls.

// An effect (0x30 bytes): SePlayer::play reads its bank (+4), program /
// note (+0x14 / +0x18), pitch spread (+0x20), id (+0x26), volume (+0x28).
// +0x1d: resumable (StageSound pauses it while turned down and plays it
// then; the others do not play while turned down).
// layout: host -- EE 0x30 (+0 a 32-bit name pointer).
struct SeEntry
{
    const char *name;           // FS2 +0x00: StageSound::stopNamed compares it
    int         bank;           // FS2 +0x04
    u_char      _08[0xc];       // FS2 +0x08
    int         program;        // FS2 +0x14
    int         note;           // FS2 +0x18
    u_char      _1c;            // FS2 +0x1c
    u_char      resumable;      // FS2 +0x1d
    u_char      _1e[2];         // FS2 +0x1e
    float       pitchSpread;    // FS2 +0x20
    u_char      _24[2];         // FS2 +0x24
    short       id;             // FS2 +0x26
    u_char      volume;         // FS2 +0x28: 0..0x80, scales left / right
    u_char      _29[7];         // FS2 +0x29
};

// The effect table's name index, sorted by name.
// layout: host -- EE 8 bytes.
struct SeName
{
    int         index;          // FS2 +0x0: SeTable::entries index
    const char *name;           // FS2 +0x4
};

// The effect table (the player's +0).
// layout: host -- EE: +0xc count, +0x428 name count, +0x42c the names,
// +0x430 the entries.
struct SeTable
{
    u_char  _000[0xc];
    int     count;              // FS2 +0x00c
    u_char  _010[0x418];
    int     nameCount;          // FS2 +0x428
    SeName *names;              // FS2 +0x42c
    SeEntry entries[1];         // FS2 +0x430: count

    // The effect called name; null none. A binary search over names,
    // inline in the ROM (UiLayout::playSound, StageSound's play 0x1d66e0);
    // like the ROM it reads names[0] even when there are none.
    const SeEntry *find(const char *name)
    {
        int hi = nameCount;
        int lo = 0;
        int index = -1;
        do {
            int mid = (hi + lo) >> 1;
            int cmp = strcmp(names[mid].name, name);
            if (cmp == 0) {
                index = names[mid].index;
                break;
            }
            if (cmp < 1)
                lo = mid + 1;
            else
                hi = mid;
        } while (hi != lo);
        if (index < 0)
            return nullptr;
        if (index < count)
            return &entries[index];
        return nullptr;
    }
};

// SeHandle: game/stagesound.h.

// The effect player (0x1298 bytes): StageSound owns one, getSePlayer is the
// one outside a stage. A handle names a bank voice or a stream; its top
// nibble set marks it paused (pause / resume).
// layout: host -- EE: +0 the effect table, +4 16 SoundSeBank, +0x44 16
// SoundStream, +0x84 the master volume, +0x88..+0x94 its fade, then 3 x 0x40
// voice records of 0x18 from +0x98.
class SePlayer
{
public:
    SePlayer();                                                          // FS2 0x00292450
    ~SePlayer();                                                         // FS2 0x002923a0: deleting

    // Plays an effect at a left / right volume 0..0x80 (random pitch).
    SeHandle play(const SeEntry *se, int left, int right);              // FS2 0x00292ac0
    // play, not under the master volume (the voice record's +0x26 flag 0).
    SeHandle playUnscaled(const SeEntry *se, int left, int right);      // FS2 0x00292830
    // 0 stopped (or -1 nothing); non-zero failed.
    int      stop(SeHandle handle);                                      // FS2 0x00292db0
    // A paused handle (top nibble) counts as playing.
    bool     isPlaying(SeHandle handle);                                 // FS2 0x00292f60
    // Stops a voice and answers its paused handle; -2 failed.
    SeHandle pause(SeHandle handle);                                     // FS2 0x00291d80
    // Plays a paused handle's entry again: the new handle; -2 an unpaused
    // one that no longer plays, -3 none. left / right are not read.
    SeHandle resume(SeHandle handle, int left, int right);               // FS2 0x00291c00
    // 0 done, -2 not playing, -3 none.
    int      setVolume(SeHandle handle, int left, int right);            // FS2 0x00291e50
    // A paused handle does nothing; 0 done, 1 not playing.
    int      setAttr(SeHandle handle, int attr);                         // FS2 0x00291ac0
    // Fades the master volume from from to to over frames (0: at once).
    void     fadeMaster(int frames, int from, int to);                   // FS2 0x00292360

    SeTable *m_table;           // FS2 +0x00
    int      m_masterVolume;    // FS2 +0x84: 0..0x80
};

// A SoundStream with a name, an fsm subtitle file and an StDT stream table
// (the requests it plays), the effect player's 0x2908d0.. run (not cut yet):
// StageSound's four channels.
// layout: host -- EE 0x90: +0x60 the StDT table, +0x74 / +0x78 a position
// offset, +0x7c the subtitles, +0x80 -1, +0x84 / +0x88 names, +0x8c a flag.
class SoundTrack : public SoundStream
{
public:
    SoundTrack();                                                        // FS2 0x00290ca0: name "snd", 0x10000 buffer
    ~SoundTrack();                                                       // FS2 0x00290c20: deleting

    // Playing or paused (the SoundStream state 1 / 2).
    bool isActive();                                                     // FS2 0x002916c0
    // Stops it; the position it stopped at, -1 not playing.
    int  stop();                                                         // FS2 0x002916f0
    // false: not an fsm / StDT file.
    bool setSubtitles(void *file);                                       // FS2 0x00290b10
    void setName(const char *name);                                      // FS2 0x00290bb0: a copy; null none
    bool setTable(void *file);                                           // FS2 0x00290d60: relocates it once
};
SePlayer *getSePlayer();                                                 // FS2 0x002c4140: sbss 0x31aea0

// What game/gfxtarget.cpp calls.
class GsRegionAlloc;            // game/gfxtarget.h
// Builds the draw system (about nine objects) on the screen origin and size
// with the texture region allocator: gfxInitRenderTargets calls it once,
// last. What it draws is unread.
void drawSysInit(int originX, int originY, int width, int height,
                 GsRegionAlloc *alloc);                                  // FS2 0x002d7cf0

// What game/vcmtable.cpp reads of rena's model (unassigned@002e2970).
// The geometry / material names of a part's +8 chain are guesses: only the
// name at its end is read.
// layout: host -- EE: name +4.
struct ModelMaterial
{
    const char *name;           // FS2 +0x4
};
// layout: host -- EE: material +8.
struct ModelGeometry
{
    ModelMaterial *material;    // FS2 +0x8
};
// One part of the model file (0x18 on the EE).
// layout: host -- EE: name +0, geometry +8.
struct ModelPart
{
    const char    *name;        // FS2 +0x0
    ModelGeometry *geometry;    // FS2 +0x8
};
// layout: host -- EE: parts +0x14, the part count (u16) +0x2a.
struct ModelHeader
{
    ModelPart *parts;           // FS2 +0x14
    u_short    partCount;       // FS2 +0x2a
};
// The model instance's record of each part (0x10 on the EE, parallel to
// the header's parts): the parts it groups and what VcmTable::update
// writes for the renderer (unread there).
// layout: host -- EE: +4 / +8 the first part / count (u16), +0xa .. +0xc
// the VCM bytes.
struct ModelPartRec
{
    u_short firstChild;         // FS2 +0x4
    u_short childCount;         // FS2 +0x8: 0 for a part grouping none
    u_char  vcmAlpha;           // FS2 +0xa: 0..0x80
    u_char  vcmFadeBits;        // FS2 +0xb
    u_char  vcmOffBits;         // FS2 +0xc
};
// layout: host -- EE: the header +0xc, the part records +0x1c.
class RenaModel
{
public:
    // The record whose children range holds rec's index (the group rec is
    // in), null at the top.
    ModelPartRec *findGroup(const ModelPartRec *rec) const;             // FS2 0x002e4100

    ModelHeader  *m_header;     // FS2 +0xc
    ModelPartRec *m_partRecs;   // FS2 +0x1c
};

// What VcmTable::attach / update read of the objects driving the VCM
// parts (unassigned@00171300; their class is unread). The name is the
// port's.
// layout: host -- EE: the name inline at +0x20, the active byte +0x34.
class VcmController
{
public:
    // How much of the controlled parts shows: 0 off .. 1 fully (a progress
    // over a total, or the motion's +0xc, times +0x2a8).
    float getWeight() const;                                             // FS2 0x00185af0

    char m_name[0x14];          // FS2 +0x20 (extent: up to m_active)
    bool m_active;              // FS2 +0x34
};

// What the S2D widgets (game/uiwidget.cpp) call of systems without a TU.

// The word at sbss 0x31ae40 (gp-0x7a30; unassigned@0023e740): a work area
// the movie widget lends its player. Who sets it is unread.
void *getUiWorkArea();                                                   // FS2 0x00243380

// fsm subtitle files (unassigned@002681e0): tracks of timed 16-bit texts,
// relocated in place once.
bool           fsmRelocate(void *file);                                  // FS2 0x0028fd30: false not an fsm
// The text of track whose time range holds seconds, null none.
const u_short *fsmFindText(float seconds, const void *file, int track);  // FS2 0x0028fb80

#endif /* _GAME_GAMEHOOKS_H */
