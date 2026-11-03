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
#include "game/seplayer.h"
#include "game/stageloop.h"
#include "game/stageobjlist.h"
#include "game/stagelist.h"
#include "game/stagenode.h"
#include "game/stagesound.h"
#include "game/streamring.h"
#include "game/uilayout.h"
#include "rx/rxfile.h"
#include "sdk/libvifpk.h"
#include "sdk/libvu0.h"     /* sceVu0FVECTOR */
#include "sdk/scetypes.h"
#include "sound/sound.h"

struct StageSelect;             // game/sysdata.h

// rx boot: thread priorities given absolute, made relative to the caller.
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

// StageObject::getKind: the ones game/uigame.cpp and StageSound test.
enum StageObjectKind
{
    StageObject_Door   = 2,     // StageObject::m_doorState is read
    StageObject_Kind9  = 9,     // a listener that hears from its m_linked's head (unidentified)
    StageObject_Kind11 = 0xb,   // a listener whose m_linked record may muffle (unidentified)
    StageObject_Kind21 = 0x15,  // left out of the inventory (unidentified)
};

// A model's bone map node (0x177270's tree: {value, key, left, right},
// sorted by strcmp; the same shape as StageNameNode). Only find is used.
// layout: host -- EE 0x10 (four words).
class StageNode;
struct BoneNode
{
    // The node for key under node (null: empty); a node without a value
    // counts as missing.
    static BoneNode *find(BoneNode *node, const char *key);              // FS2 0x00177270

    StageNode  *value;          // FS2 +0x0: the bone
    const char *key;            // FS2 +0x4
    BoneNode   *left;           // FS2 +0x8
    BoneNode   *right;          // FS2 +0xc
};

// A character's model (the object's +0x30). Only the bone map's root.
// layout: host -- EE: +0x14 the root.
struct StageModel
{
    u_char    _00[0x14];
    BoneNode *bones;            // FS2 +0x14: StageSound finds "bn_head" in it
};

// StageNode: game/stagenode.h (the scene-node family's stand-ins).

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

class StageCollision;

// An object of the stage: doors, the player's items and weapons, the Root
// objects (the map's pieces, group 1) -- one family: getKind is the same
// vtable slot in all. Only what game/uigame.cpp, StageSound and
// StageScene read; where the subclasses use an offset differently, a union
// holds both.
// layout: host -- EE: vptr at +0 (getKind is slot 123, vtable +0x1f4).
class Tim2Texture;              // game/gsutil.h
struct ModelInstance;
class StageObject
{
public:
    // A virtual in the ROM (slot 123); a plain stub until its TU.
    int getKind();                                                       // FS2 vtable +0x1f4
    // The deleting dtor (slot 2, flag 1: StageObjectList's teardown); a
    // plain stub until its TU.
    void destroy();                                                      // FS2 vtable +0x10
    // Slot 3 (vtable +0x14): the object's matrix, row 1 its facing axis,
    // row 3 its position (StageSound's listener); a plain stub until its TU.
    const sceVu0FVECTOR *getMatrix();                                    // FS2 vtable +0x14
    // A new map object takes its node: the scene's lists, instance,
    // textures, collision.
    void setupMapObject(const char *name, ModelInstance *instance,
                        StringMap<Tim2Texture *> *textures, void *collision); // FS2 0x001760c0

    // This is the scene's player (gStageScene->player).
    bool isPlayer();                                                     // FS2 0x00215ef0
    // StageSound::update's playerFlag for a character (the X_J / X_D
    // alternate entries): for the player the party's flag byte (0x1fe800),
    // for another character whether its +0x224 record's +0x40 is 0x11 or
    // 0x2a (unread).
    u_char getAltSoundFlag();                                            // FS2 0x00214f60

    // A Root object's mesh i; null past the last (inline in the ROM).
    StageCollision *collisionAt(u_int i) const
    {
        return m_collisions == nullptr || i >= m_collisionCount ? nullptr : m_collisions[i];
    }

    u_char _000[0x20];
    // The name ends before +0x30 (a character's model there): 0x10 like an
    // ItemDef name. Was sized to +0x200 before (open-items.md TU 44).
    char   m_name[0x10];        // FS2 +0x020: an item's / weapon's name (an ItemDef name), the object's map key
    StageModel *m_model;        // FS2 +0x030: a character's model (StageSound: its bone map)
    u_char m_active;            // FS2 +0x034: the list walks (StageScene::raycast) skip an inactive object
    u_char _035[0x2];
    u_char m_hidden;            // FS2 +0x037: (guess) set: StageScene::raycast leaves the object out
    u_char _038[0x1c8];
    union {
        int    m_itemDef;       // FS2 +0x200: gItemDefs index; gStageObjRecords too (StageSound)
        u_char m_collides;      // FS2 +0x200: a Root object's: it has collision meshes (StageScene::raycast)
    };
    u_char _204[0x4];
    union {
        int    m_id;            // FS2 +0x208: a character's number (StageObjectList::addCharacter)
        u_int  m_collisionCount; // FS2 +0x208: a Root object's mesh count
    };
    union {
        int    m_count;         // FS2 +0x20c: an item's count
        StageCollision **m_collisions; // FS2 +0x20c: a Root object's meshes
    };
    u_char _210[0x88];
    int    m_doorRecord;        // FS2 +0x298: a door's gDoorRecords index (StageFiles::applyBgTarget)
    u_char _29c[0xc];
    int    m_doorState;         // FS2 +0x2a8: a door's StageDoorState
    u_char _2ac[0x54];
    StageObject *m_linked;      // FS2 +0x300: the object a Kind9 / Kind11 listener hears through (StageSound)
    u_char _304[0x150];
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
    // +0x400 set and +0x3c8 == 1: StageScene::drawPaused / update then
    // pass StageSound::update sceneFlag 1. What mode 1 is is unread.
    bool isMode1();                                                      // FS2 0x002152d0
    // The player is a character (a StageObject) in the ROM; the host
    // stand-ins keep separate layouts until the character TU joins them.
    StageObject *character() { return reinterpret_cast<StageObject *>(this); }

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
    StageParty();                                                        // FS2 0x001fecf0: +0 = new 0x148-byte record (0x203d70)
    ~StageParty();                                                       // FS2 0x001fec90: deleting
    void resetSaved();                                                   // FS2 0x001fe720: the four ids to -1
    void save();                                                         // FS2 0x001fe6d0
    void restore();                                                      // FS2 0x001fe6c0
    // Of the +0 record, by its mode (+0xb8): +0x18 (modes 3, 4), +0x14
    // (mode 1), else null. The name is a guess (StageScene::drawPaused
    // compares it with the view target's character).
    StageObject *getCompanion();                                         // FS2 0x001fea50
};

// A singly linked list whose nodes {next, flags} carry a keep bit (the
// stage object's +0x30). Only what stageThread calls.
// layout: host -- EE: +0x4 head, +0x8 tail.
struct KeepList
{
    KeepList();                                                          // FS2 0x002bc770: three words zero
    ~KeepList();                                                         // FS2 0x002bc6f0: deleting; deletes every node
    // Unlinks and deletes every node whose flags bit 0 is clear.
    void removeUnkept();                                                 // FS2 0x002bc5d0
};

// The stage object's +0xec: what a sub-scene starts from (the 0x287xxx
// classes, vtables 0x318020..; their own TU). Only what stageThread calls.
// layout: host -- EE: vptr at +0x2c.
enum StageEventState
{
    StageEvent_Active = 2,      // StageScene::isEventActive; the name is a guess
};
class StageEvent
{
public:
    // A virtual in the ROM (slot 3, vtable +0x14); a plain stub until its
    // TU. What StageScene::enterSubScene builds the sub-scene from.
    void *getSubSceneData();
    // The deleting dtor (vtable +0x8, vptr +0x2c: StageScene's dtor); a
    // plain stub until its TU.
    void destroy();

    u_char _00[0x24];
    int    state;               // FS2 +0x24: StageEventState
};

class Clock;                    // game/clock.h

// What StageScene::raycast answers (from 0x233810). Only the flag
// StageSound tests.
// layout: host -- EE: +0x40 flags.
enum StageCollisionFlag
{
    StageCollision_Thin = 0x400,    // sound passes once (StageSound: twice the distance)
};
struct StageCollisionFace
{
    u_char _00[0x40];
    u_int  flags;               // FS2 +0x40: StageCollisionFlag
};

// One collision mesh of a Root object (StageObject::m_collisions; the run
// unassigned@00231c90). Only the ray test StageScene::raycast uses.
// layout: host -- contents unread.
class StageCollision
{
public:
    // The face the segment from..to hits (hit: where), null none; mask /
    // flags / arg select the faces (StageSound's values).
    StageCollisionFace *raycast(sceVu0FVECTOR hit, const sceVu0FVECTOR from,
                                const sceVu0FVECTOR to, u_int mask,
                                u_int flags, int arg);                   // FS2 0x00233810
};

// A sub-scene's demo voice (StageScene +0x28; the run unassigned@002b0d50):
// a SoundStream with a directory (streamEU/demo/) and what looks like a
// subtitle colour and position. Only what StageScene calls.
// layout: host -- EE 0x80: SoundStream's 0x60, +0x60..+0x70 zero (+0x64
// the directory, +0x68 a string, +0x70 an object with the vptr at +0x10),
// +0x74 0x80606060, +0x78 0x100, +0x7c 0x1bc.
class DemoStream : public SoundStream
{
public:
    DemoStream();                                                        // FS2 0x002b1490
    ~DemoStream();                                                       // FS2 0x002b13e0: deleting
    void setDir(const char *dir);                                        // FS2 0x002b1370: a heap copy, null none
    // SoundStream state Playing or Paused.
    bool isPlaying();                                                    // FS2 0x002b1140
    // SoundStream::stop(false); -1 when it failed.
    int  stop();                                                         // FS2 0x002b1170
    // A jump to SoundStream::update: once a frame.
    void update();                                                       // FS2 0x002b1100
};

// The scene's fade (StageScene +0xfc; 0x1512a0 / 0x1512e0 in the run
// unassigned@00148db0). Class and mode meaning are guesses.
// layout: host -- EE: +0x24 level, +0x34 mode.
class StageFade
{
public:
    // (x, y, w, h, arg): StageScene passes 0x700, 0x700, 0x200, 0x200, 0
    // (GS window units: the 512-pixel screen at the 1792 offset); arg is
    // unread.
    StageFade(int x, int y, int w, int h, int arg);                      // FS2 0x00151600
    // The deleting dtor (vtable 0x311270 at +4, slot 0); a plain stub
    // until its TU.
    void destroy();
    // Mode 2 at level 1.0 (StageSound does not resume a cue stream then).
    bool isFullInMode2();                                                // FS2 0x001512a0
};

// -- The stage scene's members (StageScene's ctor 0x19c180 / dtor 0x19bbe0;
// batch 080). Each lives in a run not read yet: only the ctor / dtor and
// what game/stagescene.cpp calls are declared. Class names are the port's
// guesses from where they sit and what they allocate (the batch's why).
// The ones the ROM deletes through a vtable get a plain `destroy()` stub
// (the deleting dtor's slot) until their TU, like StageObject's.

// +0x14: five {0, 0} pairs (0x1716c0); contents unread. layout: host (EE 0x2c).
struct ScenePairTable
{
    ScenePairTable();                                                    // FS2 0x00171680
    ~ScenePairTable();                                                   // FS2 0x00171620: deleting
};

// +0x18: four arrays of records reserved for 0x1fe each. layout: host (EE 0x70).
struct SceneRecordBuckets
{
    SceneRecordBuckets();                                                // FS2 0x001f6510
    ~SceneRecordBuckets();                                               // FS2 0x001f63f0: deleting
};

// +0x1c: owns one 0x40-byte object (0x192080, built with this). layout: host (EE 4).
struct StageNodeTree
{
    StageNodeTree();                                                     // FS2 0x00190c20
    ~StageNodeTree();                                                    // FS2 0x00190bc0: deleting
    // Replaces the inner object's BoxGrid with a new one over a bbx file.
    void setBoxes(void *bbx);                                            // FS2 0x00190900
    // The particle target's: the 16 textures from common01, then its first
    // two particle files (which is which is unread).
    void addParticleTexture(Tim2Texture *texture);                       // FS2 0x00190aa0
    void setParticleFileA(void *file);                                   // FS2 0x00190b50
    void setParticleFileB(void *file);                                   // FS2 0x00190ae0
};

// +0x2c: tex areas, an AreaGrid and a 0x5a10 table; its dtor puts the
// parent scene's draw settings back. layout: host (EE 0xe70).
struct SceneRender
{
    SceneRender();                                                       // FS2 0x001fa4a0
    ~SceneRender();                                                      // FS2 0x001fa230: deleting
};

// +0x4c and +0x8c..+0x94: objects with the vptr at +0 (slot 0 their
// deleting dtor), set outside the ctor; unread. layout: host (unread).
class SceneOwnedObject
{
public:
    void destroy();                                                      // FS2 vtable +0x8
};

// +0x4c: what runs the stage's rute file (vtable 0x316a78; run
// unassigned@00252840); what it does is unread. layout: host (EE 0xc).
class RoutePlayer : public SceneOwnedObject
{
public:
    explicit RoutePlayer(void *rute);                                    // FS2 0x00253180
    void start(int arg);                                                 // FS2 0x00252b10: empty here
};

class Tim2Texture;              // game/gsutil.h

// +0x8c, built by the `effect` target from the texture "siglamp" (vtable
// 0x314c40; run unassigned@00299d90). layout: host (EE 8).
class SigLamp : public SceneOwnedObject
{
public:
    explicit SigLamp(Tim2Texture *texture);                              // FS2 0x002a7bf0
};

// A script file as loaded (script/ in the stage script; 0x1d3a40 turns its
// offsets into pointers; run unassigned@001a68b0).
// layout: file -- 32-bit on the disc; only the counts are read here.
struct StageScript
{
    // PC: the two tables at +0 / +4 are 32-bit fields in the file; the
    // relocate of their TU must keep the pointers off the image.
    int   table;                // FS2 +0x0
    int   table2;               // FS2 +0x4
    short count;                // FS2 +0x8
    short slotCount;            // FS2 +0xa: StageScene::scriptSlots

    StageScript *relocate(bool withTable2);                              // FS2 0x001d3a40
};
static_assert(sizeof(StageScript) == 0xc, "StageScript: the file header");

// A collision file as loaded ("SSS\x1a" first; run unassigned@00231c90).
// layout: file -- only the magic is read here.
enum { kCollisionMagic = 0x1a535353 };
struct CollisionFile
{
    void relocate();                                                     // FS2 0x0023ae40

    int magic;                  // FS2 +0x0: kCollisionMagic
};
static_assert(sizeof(CollisionFile) == 4, "CollisionFile: only the magic so far");

// +0x58: built over the areaclip keyword's file (setAreaClip): an 8 x 8 grid
// of index lists (0x2db080 picks the file's 0x40-byte records per cell), a
// pointer array (+0x30, count +0x2c) of objects deleted through their vptr.
// layout: host (EE 0x34).
struct AreaClip
{
    explicit AreaClip(void *file);                                       // FS2 0x002dccf0
    ~AreaClip();                                                         // FS2 0x002dcab0: deleting
};

// A map file (the map keyword). relocate sets its six section pointers
// +0x0..+0x14 in place: their TU must not write 8-byte pointers into them.
// layout: host (nothing read here).
struct StageMapFile
{
    void relocate();                                                     // FS2 0x00186ee0
};

// A node's record in a model file (0x30 bytes). EE layout: it lives in the
// file.
struct ModelFileNode
{
    int                _00;     // FS2 +0x00
    S2dRef<const char> name;    // FS2 +0x04: a map model's: the object it becomes
};
static_assert(offsetof(ModelFileNode, name) == 4, "ModelFileNode::name");

// What ModelFile::createInstance makes (vtable 0x31a7f0, 0x100 bytes,
// 0x80-aligned): only the record is read here.
// layout: host -- EE: the record at +0xc.
struct ModelInstance
{
    ModelFileNode *node;        // FS2 +0xc
};

// A model file (the model keyword). EE layout: the file itself; relocate
// (its TU, unassigned@002e2970) turns the offsets into pointers in place,
// so the fields read here are S2dRef.
struct ModelFile
{
    bool isValid();                                                      // FS2 0x002e2cf0: the magic at gp-0x7b48
    void relocate();                                                     // FS2 0x002e2d20
    int  getNodeCount();                                                 // FS2 0x002e5200
    // A new instance of node index, or of the node called name when name
    // is not null.
    ModelInstance *createInstance(u_int index, const char *name);        // FS2 0x002e5210

    u_char  _00[0x1e];
    u_short textureCount;       // FS2 +0x1e
    u_char  _20[0x1c];
    S2dRef<S2dRef<const char> > textureNames; // FS2 +0x3c: textureCount, each a .tm2 of the .tpk without its extension
};
static_assert(offsetof(ModelFile, textureCount) == 0x1e, "ModelFile::textureCount");
static_assert(offsetof(ModelFile, textureNames) == 0x3c, "ModelFile::textureNames");

// An lpt file (the lpt keyword): magic kLptMagic until applyBgTarget made
// +8 a pointer, then kLptRelocated.
constexpr u_int kLptMagic     = 0x0074706c;     // "lpt"
constexpr u_int kLptRelocated = 0x0054504c;     // "LPT"
struct LptFile
{
    u_int        magic;         // FS2 +0x0
    int          _04;           // FS2 +0x4
    S2dRef<void> data;          // FS2 +0x8: file: an offset from the header
};
static_assert(offsetof(LptFile, data) == 8, "LptFile::data");

// A door's record (0x38 bytes, kDoorRecords of them; bss 0x537d80, filled
// at run time), found by its map object's name.
constexpr int kDoorRecords = 0x87;
struct DoorRecord
{
    char name[0x34];            // FS2 +0x00: extent unread
    int  value;                 // FS2 +0x34: SubSceneDoorObject::m_recordValue (unread)
};
static_assert(sizeof(DoorRecord) == 0x38, "DoorRecord: EE 0x38");
extern DoorRecord gDoorRecords[kDoorRecords];   /* bss 0x537d80 */

// The stage's map object for a model's node called name; null none. The
// class comes from the name.
StageObject *createMapObject(const char *name, void *collision);         // FS2 0x001766c0
// oGK99_01 and oDC99_01 ("gate" is a guess).
bool isGateModelName(const char *name);                                  // FS2 0x001769e0

// The map objects of a sub-scene (the bg target; vtables 0x319c90 and
// 0x319a20, operator new 0x280 / 0x2c0 aligned 0x40). The ctor's argument is
// always 1 here.
// layout: host
class SubSceneMapObject
{
public:
    explicit SubSceneMapObject(int arg);                                 // FS2 0x002bc040
    void setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures, void *collision);     // FS2 0x002bbed0
};
// layout: host
class SubSceneGateObject
{
public:
    explicit SubSceneGateObject(int arg);                                // FS2 0x002bbdb0
    void setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures, void *collision);     // FS2 0x002bbc10
};
// A _dp / _ds node (a door's parts). Its constructor is inline in the ROM:
// the base's, then the vtables 0x314ef0 / 0x314f14 / 0x314f2c. layout: host
// -- EE 0x2c0.
class SubSceneDoorObject : public SubSceneMapObject
{
public:
    explicit SubSceneDoorObject(int arg) : SubSceneMapObject(arg), m_recordValue(0) {}

    int m_recordValue;          // FS2 +0x280: DoorRecord::value
};

// +0x9c: eight blocks from +0x18. layout: host.
struct SceneBufferSet
{
    ~SceneBufferSet();                                                   // FS2 0x002ce5a0: deleting
};

// +0xa0: a colour and a screen rect (vtable 0x311280 over StageFade's base
// 0x311260, vptr +4). layout: host (EE 0x24).
class ScreenFill
{
public:
    ScreenFill(u_int rgba, int x, int y, int w, int h);                  // FS2 0x00151f20
    void destroy();                                                      // FS2 vtable +0x8 (vptr +4)
};

// +0xa4 / +0xa8: menu program holders (beside PrgMcCard), set outside the
// ctor. layout: host.
struct MenuProgHolder
{
    ~MenuProgHolder();                                                   // FS2 0x002ad070: deleting
};
struct MenuProgSet
{
    ~MenuProgSet();                                                      // FS2 0x002bb3e0: deleting
};

// +0xac..+0xb8: four area grids (a list, 0x40 cells, a box at +0x210).
// layout: host (EE 0x240).
struct AreaGrid
{
    AreaGrid();                                                          // FS2 0x00284640
    ~AreaGrid();                                                         // FS2 0x00284580: deleting
    // pos inside one of the grid's areas.
    int  contains(const sceVu0FVECTOR pos);                              // FS2 0x00284190
    void addId(int id);                                                  // FS2 0x00284560
};

// +0xc0: (count 4, 0x800, 0x878). layout: host (EE 0x2c).
struct ScenePool
{
    ScenePool(u_int count, int size, int size2);                         // FS2 0x00289be0
    ~ScenePool();                                                        // FS2 0x00289b60: deleting
};

// +0xc4: plain data beside the pause menu's code. layout: host (EE 0x678).
struct PauseMenuState
{
    PauseMenuState();                                                    // FS2 0x00207ef0
    ~PauseMenuState();                                                   // FS2 0x00207ea0: deleting
};

class WideFontPrinter;          // game/fontdata.h

// +0xcc: a list view over the scene's printer (vtable 0x316530 at +0x88).
// layout: host (EE 0x8c).
class UiItemList
{
public:
    UiItemList(WideFontPrinter *printer, int arg);                       // FS2 0x0023f0b0
    void destroy();                                                      // FS2 vtable +0x8 (vptr +0x88)
};

// +0xd0: a menu over the scene's printer (vtable 0x3164b0 at +8; a 0xaa8
// object at +0xc). StageScene::getOpenMenu reads its state.
// layout: host -- EE 0x18: +0 state.
enum StageMenuState
{
    StageMenu_Open = 2,         // getOpenMenu's test; the other values unread
};
class StageMenu
{
public:
    StageMenu(WideFontPrinter *printer, int arg);                        // FS2 0x002308a0
    void destroy();                                                      // FS2 vtable +0x8 (vptr +8)

    int state;                  // FS2 +0x0: StageMenuState
};

// +0xdc: 0xc-byte entries after a count. layout: host (EE 0xc4).
struct SceneBoxList
{
    SceneBoxList();                                                      // FS2 0x0023ad60
    ~SceneBoxList();                                                     // FS2 0x0023ad10: deleting
};

// +0xe0. layout: host (EE 0x3cc).
struct SceneUiState
{
    SceneUiState();                                                      // FS2 0x00243150
    ~SceneUiState();                                                     // FS2 0x00243100: deleting
};

// +0xe4: a 48-bit LCG (drand48's multiplier 0x5deece66d; the run
// 0x2681e0..0x268310). layout: host (EE 8).
struct Rand48
{
    Rand48();                                                            // FS2 0x002682e0
    // A number in 0..n-1.
    int next(int n);                                                     // FS2 0x002681e0
};

// +0xf4: the ALERT / CAUTION state machine (game/stagealert.cpp).
// layout: host (EE 0x1c).
struct StageAlert
{
    StageAlert();                                                        // FS2 0x00264070
    ~StageAlert();                                                       // FS2 0x00264000: deleting
};

// +0x118 / +0x11c: the game-over and stage-clear sequences (set outside
// the ctor, beside DemoStream). Their state reads as done (5 / 2) once
// the scene's fade is full in mode 2. layout: host -- EE: +0 the state.
enum
{
    kGameOverSequenceDone = 5,  // StageScene::isGameOver
    kClearSequenceDone    = 2,  // StageScene::isCleared
};
// layout: host (unread).
struct GameOverSequence
{
    int getState();                                                      // FS2 0x002b1c50
};
// layout: host (unread).
struct ClearSequence
{
    int getState();                                                      // FS2 0x002b1620
};

// +0x120 (StageScene::update deletes it too). layout: host.
struct SceneDirector
{
    ~SceneDirector();                                                    // FS2 0x002b2280: deleting
};

// +0x124: a record of two arrays. The ROM frees the second as an array
// with CodeWarrior's 0x10-byte header (p - 0x10) and runs no element dtor.
// layout: host -- EE: +8, +0xc.
struct SceneArrayPair
{
    u_char  _00[0x8];
    u_char *first;              // FS2 +0x8: delete[]
    u_char *second;             // FS2 +0xc: delete[] (header at -0x10 on the EE)
};

// +0x12c: an object with a member at +0x24 the dtor destroys in place.
// layout: host.
struct SceneModel
{
    ~SceneModel();                                                       // FS2 0x002cff30 (in place: flag -1)
};
struct SceneModelHolder
{
    u_char     _00[0x24];
    SceneModel model;           // FS2 +0x24
};

// The draw system's record stack (gp-0x7994, saved in gp-0x7990): a
// sub-scene draws into its own record.
void drawSysPush();                                                      // FS2 0x002d7c70
void drawSysPop();                                                       // FS2 0x002d7c00

// 44 groups at 0x599830 / 0x599860 reset (a byte zero, 32 words one each):
// a stage's ctor. What the groups are is unread.
void resetStageGroupTable();                                             // FS2 0x002cf510

// A sub-scene's own scene object (StageScene +0x8, from enterSubScene):
// the vptr at +0xb4 like the character classes; only the deleting dtor
// (vtable +0x18) is called here. layout: host (unread).
class SubSceneObject
{
public:
    void destroy();                                                      // FS2 vtable +0x18 (vptr +0xb4)
};

// StageScene (the current stage object) and StageFiles (the stage's
// files): game/stagescene.h.
#include "game/stagescene.h"

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


// The memory-card menu's save manager (sbss 0x31ae94, gp-0x79dc; set and
// deleted by the menu 0x2b8600.., null while there is none): the S2D's
// rMcTotalTime0..3 show its saves' play times. Data the build must define.
class Memcard;                  // game/memcard.h
extern Memcard *gMenuMemcard;

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

// A record table per StageKind (bss 0x539b10: 3 x 0x140 x 0xfc), indexed
// by an object's m_itemDef; only kind is read (StageSound: 0xf muffles a
// Kind11 listener). What it describes is unread.
constexpr int kStageObjRecordSets  = 3;
constexpr int kStageObjRecordCount = 0x140;
constexpr int kStageObjRecordMuffles = 0xf;
struct StageObjRecord
{
    u_char _00[0x10];
    int    kind;                // FS2 +0x10
    u_char _14[0xe8];
};
static_assert(sizeof(StageObjRecord) == 0xfc, "StageObjRecord: EE 0xfc");
extern StageObjRecord gStageObjRecords[kStageObjRecordSets][kStageObjRecordCount]; /* bss 0x539b10 */

// ItemDef::kind: 2..6 are firearms (they show ammo), 7 and up items the
// inventory lists; below 7 the weapons' sprite set.
enum ItemKind
{
    ItemKind_FirearmFirst = 2,
    ItemKind_FirearmLast  = 6,
    ItemKind_ItemFirst    = 7,
};

// ──────────────────────────────────────────────────────────────────────
// The sound players are game/seplayer.h. The one outside a stage:
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
// The track whose key is key (strcasecmp), -1 none or not an fsm.
int            fsmFindTrack(void *file, const char *key);               // FS2 0x0028fc80
// The track has a non-empty text.
bool           fsmTrackHasText(void *file, int track);                  // FS2 0x0028fa90

// A placed sound's volume at ratio (its distance over its range, 0..1;
// 0 beyond 1) by the entry's falloff curve (SeFalloff), from volume
// (unassigned@002681e0; SePlayer / SoundTrack).
int seDistanceVolume(float ratio, int falloff, int volume);              // FS2 0x00283df0

// The StDT stream table's header readers stdtIsRelocated / stdtEntryCount:
// game/stagescene.h.

#endif /* _GAME_GAMEHOOKS_H */
