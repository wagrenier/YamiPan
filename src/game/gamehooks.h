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
#include "sdk/libgraph.h"     /* sceGsDrawEnv1 */
#include "sdk/libvifpk.h"
#include "sdk/libvu0.h"     /* sceVu0FVECTOR */
#include "sdk/scetypes.h"
#include "sound/sound.h"

class GamePacketManager;        // game/gamepacket.h
class TexAreaList;              // game/gsblockmap.h

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
// Deletes what loadMenuPack made (the pack, its SePlayer); a stage needs
// the memory.
void unloadMenuPack();                                                   // FS2 0x002c3db0

// What game/menuscene.cpp calls.
// title<lang>.d2b, without its intro unless withIntro: TitleAnswer.
int  runTitleScreen(bool withIntro);                                     // FS2 0x00231c90
// help<lang>.d2b at a page; always 0.
int  runHelpScreen(int page);                                            // FS2 0x002acff0
// The mode behind the top menu's answer 10, in slot 3 (0x2c4320; 16 scored
// entries of the save, unread): true when it changed the save.
int  runExtraMode(int arg);                                              // FS2 0x002c42c0

// The navi screen's answer: a node (stage in the low byte, these bits) or
// one of the two below alone.
constexpr int kNaviAnswerStageMask = 0xff;
enum NaviAnswer
{
    NaviAnswer_Variant1 = 0x100,    // StageSelect::variant
    NaviAnswer_Flag200  = 0x200,    // StageSelect::_10
    NaviAnswer_Help     = 0x400,    // runHelpScreen
    NaviAnswer_Leave    = 0x800,    // back to the top menu (also a thread message)
};
// navi<lang>.d2b from the current node, blocking.
int  runNaviFile();                                                      // FS2 0x00295470
// The record as a node (negated after a clear) / a node into the record.
int  currentNaviAnswer();                                                // FS2 0x0029ad40
int  applyNaviAnswer(int answer);                                        // FS2 0x0029acb0
// The new game's getSaveExtra block (unread).
void saveExtraInit();                                                    // FS2 0x0029ef70

// The layouts' paths (static buffers): 2dEU/<name><lang>.d2b.
const char *archiveLayoutPath();                                         // FS2 0x002ec7f0
const char *optionLayoutPath();                                          // FS2 0x002ec660
const char *naviTuLayoutPath();                                          // FS2 0x002ec5c0
const char *mcBaseLayoutPath();                                          // FS2 0x002ec610
const char *memAltLayoutPath();                                          // FS2 0x002ec6b0
const char *newGameLayoutPath();                                         // FS2 0x002ec700
const char *sceLogoLayoutPath();                                         // FS2 0x002ec750
const char *topMenuLayoutPath();                                         // FS2 0x002ec7a0
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
    InitItem_Spotlight = 2,     // StageFiles::loadCommon: the renderer's spotlight
    InitItem_Shadow    = 3,     // shadow_00
    InitItem_Fill      = 4,     // the screen fills' image
    InitItem_PartyImage = 5,    // StageFiles::loadList: StageParty::setImage
    InitItem_MenuProg  = 6,     // StageScene::createMenuProg
    InitItem_MenuImages = 7,    // 7..0xc: StageFiles::loadList, MenuProgSet::load
    InitItem_PartyImages = 0xd, // 0xd..0x10: StageParty::addImage
    InitItem_ItemSheet = 0x11,  // UiItemList::setSheet
    InitItem_MenuData  = 0x12,  // StageMenu::setData
    InitItem_PoolImage = 0x13,  // ScenePool::setImage
    InitItem_KeepData  = 0x14,  // KeepList's +0
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
    StageObject_Kind3  = 3,     // takes the motion named CAR (StageFiles::applyBgMotionTarget)
    StageObject_Kind6  = 6,     // takes the motion named Seven_Keys
    StageObject_Kind9  = 9,     // the main camera "camera" (StageMainCamera): hears from its m_linked's head
    StageObject_Kind10 = 0xa,   // the camera "camview" and its two variants (ViewCamera)
    StageObject_Kind11 = 0xb,   // the party's "camjack" camera: its m_linked record may muffle
    StageObject_Kind13 = 0xd,   // a sub-scene's "camera_demo" (DemoCamera)
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
class VcmTable;                 // game/vcmtable.h
struct StageModelData
{
    u_char _0[0x5];
    u_char drawGroup;           // FS2 +0x5: 1, or 0x20 for what the view target looks at (StageScene::prepareDrawFlags)
};
static_assert(offsetof(StageModelData, drawGroup) == 5, "StageModelData::drawGroup");
// The model's pose solver (+0xc; the run unassigned@00148db0). Only the
// test before the call is read: +0x150 a count of something to solve.
// layout: host -- EE: +0x150.
struct ModelPose
{
    // iterations 3, count -1 (all), flaggedOnly 0 at the one caller here.
    int solve(int iterations, int count, int flaggedOnly);               // FS2 0x0014e4d0

    int pending;                // FS2 +0x150
};
class StageObject;
// What hangs from a model's bones (+0x18): objects, in a list.
// layout: host -- EE: +0 the object, +0x10 the next.
struct ModelLink
{
    StageObject *object;        // FS2 +0x00
    ModelLink   *next;          // FS2 +0x10
};
// layout: host -- EE offsets in the comments.
struct StageModel
{
    // Hangs the bones' link nodes and the object's matrix on them; once,
    // from StageScene::setupCameras.
    void bindBones();                                                    // FS2 0x00187d10
    void placeBones(StageObject *owner);                                 // FS2 0x00187b70
    void updateVcm();                                                    // FS2 0x00187230

    StageModelData *data;       // FS2 +0x00: the model file (updateVcm tests it)
    VcmTable  *vcm;             // FS2 +0x04
    ModelPose *pose;            // FS2 +0x0c
    BoneNode  *bones;           // FS2 +0x14: StageSound finds "bn_head" in it
    ModelLink *links;           // FS2 +0x18
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
class StageObjectTree;
struct PlayerMotion;
class ScriptRunner;
class Pad;                      // game/pad.h
// What drives a character from the pad (+0x21c). layout: host -- EE: +0
// the flag, +8 the pad.
struct CharacterInput
{
    void setActive(bool on);                                             // FS2 0x00239bb0
};
// What drives a character otherwise (+0x220, null without one). The name
// is a guess: it is switched the other way round from the input. EE: +0x174
// the flag.
// layout: host
struct CharacterBrain
{
    void setEnabled(bool on);                                            // FS2 0x001dd3f0
    // A frame: its script runs while the flag holds and it has a route.
    void update();                                                       // FS2 0x001dd330
};
// One of a character's two parts (+0x240 / +0x244): what they are is
// unread. EE layout, as far as read.
struct CharacterPart
{
    u_char _0[0xd];
    u_char drawMode;            // FS2 +0xd: StageScene::prepareDrawFlags (0 off, 2 on)
};
static_assert(offsetof(CharacterPart, drawMode) == 0xd, "CharacterPart::drawMode");
// layout: host
class StageObject
{
public:
    // The family's base (vptrs +0, +0x1c, +0xf4; the node state, the
    // script runner at +0x40, the transform at +0x140). Vtable 0x314770.
    StageObject();                                                       // FS2 0x0018b700

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
    // Copies name to m_name (first thing in every class's addToScene).
    void setName(const char *name);                                      // FS2 0x0018b4d0
    // Slot +0x24c: the object is the view stack's top again
    // (StageViewStack::remove). A plain stub until its TU.
    void onViewTop();                                                    // FS2 vtable +0x24c
    // Slot +0x20: the object's matrices from its transform ("calcmatrix").
    // A plain stub until its TU.
    void calcMatrix();                                                   // FS2 vtable +0x20
    // Slot +0x25c: a camera's step for the frame. A plain stub until its TU.
    void cameraStep();                                                   // FS2 vtable +0x25c
    // Slot +0x260: a camera's m_linked changed. A plain stub until its TU.
    void onLinkChanged();                                                // FS2 vtable +0x260
    // The node's position goes to its slot and down the children.
    void propagate();                                                    // FS2 0x001d8260
    // A door hangs itself from the Root object's lists.
    void attachToRoot();                                                 // FS2 0x00173cb0
    // A character of the mission's list, around the frame's scripts
    // (StageScene::runStageScripts): what they do is unread.
    void beforeScripts();                                                // FS2 0x0021c7c0
    void afterScripts();                                                 // FS2 0x0021c310
    // An inactive character of the list comes back: canSpawn asks (no when
    // its position record +0x66c is valid and +0x6ac is 0), spawn takes the
    // script entry and the position.
    bool canSpawn();                                                     // FS2 0x002185a0
    void spawn();                                                        // FS2 0x00218630
    // Slot +0x270 of the map objects (doors, CAR, Seven_Keys): a motion
    // list of the bgmotion target. The two flags are unread. A plain stub
    // until its TU.
    void setMotionList(void *list, int arg, int arg2);                   // FS2 vtable +0x270
    // A door's fields from gDoorRecords[m_doorRecord].
    void applyDoorRecord();                                              // FS2 0x00173880
    // The object belongs to motion target index (stageMotionTargetName).
    bool isMotionTarget(int index);                                      // FS2 0x00175c40
    // Slot +0x60 of the +0xf4 table, on the object and all of its tree.
    // What the flag is is unread (StageScene::requestSubScene clears it on
    // lamps).
    void setTreeScriptFlag(bool on);                                     // FS2 0x00185fc0
    // The script runner every object holds at +0x40. The host stand-ins
    // keep no room for it: the character TU gives it a member.
    ScriptRunner *scriptRunner() { return reinterpret_cast<ScriptRunner *>(_03c + 4); }
    // A character's action as the menus see it (StageScene::playerAction).
    int  getActionState();                                               // FS2 0x00214fc0

    // This is the scene's player (gStageScene->player).
    bool isPlayer();                                                     // FS2 0x00215ef0
    // This is the scene's partner (gStageScene->partner).
    bool isPartner();                                                    // FS2 0x00215ed0
    // +0x400 set and +0x3c8 == 2 (the player's isMode1 has 1).
    bool isMode2();                                                      // FS2 0x002152a0
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

    // A camera's matrices. PC: the stand-in family has no fields there
    // (+0x200 and +0x240 mean other things in other classes); both go when
    // the camera class gets its own layout.
    const void *cameraScreen() const { return &m_itemDef; }              // FS2 +0x200
    const void *cameraView() const { return &m_parts; }                  // FS2 +0x240

    u_char _000[0x4];
    StageObject *m_parent;      // FS2 +0x004: its parent node; null for a root
    u_char _008[0x10];
    u_char m_viewActive;        // FS2 +0x018: 1 from the ctor; cleared off the view stack and on the map's cameras
    u_char _019[0x7];
    // The name ends before +0x30 (a character's model there): 0x10 like an
    // ItemDef name. Was sized to +0x200 before (open-items.md TU 44).
    char   m_name[0x10];        // FS2 +0x020: an item's / weapon's name (an ItemDef name), the object's map key
    StageModel *m_model;        // FS2 +0x030: a character's model (StageSound: its bone map)
    u_char m_active;            // FS2 +0x034: the list walks (StageScene::raycast) skip an inactive object
    u_char _035[0x2];
    u_char m_dead;              // FS2 +0x037: StageScene::update deletes the object at the frame's end; raycast leaves it out
    StageObjectTree *m_tree;    // FS2 +0x038: its node in the object tree (StageScene::restoreObjectFlags)
    u_char _03c[0x104];
    NodeTransform m_xform;      // FS2 +0x140: the placement the map file sets
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
    u_char _210[0xc];
    CharacterInput *m_input;    // FS2 +0x21c: a character's
    CharacterBrain *m_brain;    // FS2 +0x220: a character's; null none
    PlayerMotion *m_motion;     // FS2 +0x224: a character's
    u_char _228[0x18];
    union {
        CharacterPart *m_parts[2]; // FS2 +0x240: a character's; StagePlayer::isMode1 picks which one shows in a view camera
        struct {
            int m_doorType;     // FS2 +0x240: a door's StageDoorType
            int m_doorIndex;    // FS2 +0x244: a door's number from 1 (StageScene::findDoorOfFace)
        };
    };
    u_char _248[0x48];
    void  *m_290;               // FS2 +0x290: a character's (StageScene::partnerResponds; unread)
    u_char _294[0x4];
    int    m_doorRecord;        // FS2 +0x298: a door's gDoorRecords index (StageFiles::applyBgTarget)
    u_char _29c[0x6];
    u_char m_keepFlags;         // FS2 +0x2a2: a light's: SceneArrayPair::restoreFlags leaves it alone
    u_char _2a3[0x5];
    int    m_doorState;         // FS2 +0x2a8: a door's StageDoorState
    u_char _2ac[0x20];
    float  m_range;             // FS2 +0x2cc: a camera's; with m_range2 the draw distance
    float  m_range2;            // FS2 +0x2d0: a camera's
    u_char _2d4[0x2c];
    StageObject *m_linked;      // FS2 +0x300: what a camera follows (StageScene::setPlayer); the object a Kind9 / Kind11 listener hears through (StageSound)
    u_char _304[0xc];
    int    m_demoCut;           // FS2 +0x310: a DemoCamera's (StageScene::draw: below 2 the filter runs in d0101)
    u_char _314[0x8d];
    u_char m_canPause;          // FS2 +0x3a1: the player's: clear: no pause menu
    u_char _3a2[0x1e];
    u_char m_3c0;               // FS2 +0x3c0: StageScene::partnerResponds (unread)
    u_char m_3c1;               // FS2 +0x3c1: the same, of the partner
    u_char _3c2;
    u_char m_3c3;               // FS2 +0x3c3: the same (unread)
    u_char _3c4[0x8c];
    int    m_weaponDef;         // FS2 +0x450: a weapon's gWeaponDefs index (StageFiles::placeItem)
    union {
        int    m_ammoB;         // FS2 +0x454: a firearm's (findSprite's second number)
        u_char m_altAction;     // FS2 +0x454: the player's: which alternate action (StageScene::playerAction)
    };
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
// layout: host -- EE: +0xc00 the look state.
struct PlayerLookState;
// The record at +0xbd8: a state, 1 or 2 draw something (0x2b9ef0 / 0x2ba8d0,
// beside PrgMcCard). layout: host -- EE: +0 the state.
struct PlayerOverlay
{
    void draw();                                                         // FS2 0x002bb260
};
struct PlayerMotion
{
    void refresh();                                                      // FS2 0x001e66c0

    u_char _000[0xbd8];
    PlayerOverlay *overlay;     // FS2 +0xbd8: StageScene::draw
    u_char _bdc[0x24];
    PlayerLookState *look;      // FS2 +0xc00
};
// What StageScene::partnerResponds reads of it; the angle's unit and what
// the -190..-160 window means are unread. EE layout.
struct PlayerLookState
{
    u_char _00[0x60];
    float  angle;               // FS2 +0x60
    u_char _64[0x2c];
    u_char active;              // FS2 +0x90
};
static_assert(offsetof(PlayerLookState, active) == 0x90, "PlayerLookState::active");

// StageObject::m_doorType.
enum StageDoorType
{
    StageDoorType_T = 1,        // takes the motion named MODEL_dr_t
    StageDoorType_R = 2,        // MODEL_dr_r
};
// The name of motion target index 1..8 (CAT, CROW, tonbi, oMT04_04,
// oZS09_04, oDC07_05, oMK21_09, oMT04_99); UNKNOWN for any other.
const char *stageMotionTargetName(int index);                            // FS2 0x00175c00

// The player character of a stage (the stage object's +0x78). Only what
// game/uigame.cpp reads.
// layout: host -- EE: +0x150 / +0x154 position x / z, +0x168 yaw, +0x22c
// the items, +0x234 the map's holder, +0x238 / +0x23c the two weapons.
struct StagePlayer
{
    // +0x400 set and +0x3c8 == 1: StageScene::drawPaused / update then
    // pass StageSound::update sceneFlag 1. What mode 1 is is unread.
    bool isMode1();                                                      // FS2 0x002152d0
    // +0x364 set: +0x2ca = 1. What the two bytes are is unread.
    void latchAltAction();                                               // FS2 0x0020b1d0
    // The player is a character (a StageObject) in the ROM; the host
    // stand-ins keep separate layouts until the character TU joins them.
    StageObject *character() { return reinterpret_cast<StageObject *>(this); }

    u_char _000[0x150];
    float  posX;                // FS2 +0x150
    float  posZ;                // FS2 +0x154
    float  height;              // FS2 +0x158: the world's up axis (StageScene::getFloor)
    float  posW;                // FS2 +0x15c
    u_char _160[0x8];
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
// An object's node in the tree of objects (StageObject +0x38). EE 0x10.
class StageObjectTree
{
public:
    // Sets the node's and its children's m_active and m_viewActive.
    void setActive(bool on);                                             // FS2 0x0018bdc0
    // A virtual (+0x244) on the node's object and its children; what it
    // sets is unread.
    void setShown(bool on);                                              // FS2 0x0018bbe0
};
// Every character with m_viewActive: three steps on its motion (+0x224).
void stageUpdateCharacterMotions();                                      // FS2 0x0017df60
// The object's shown state (0x18b290).
bool stageObjectIsShown(StageObject *object);                            // FS2 0x0018b290

// layout: host
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
    // ScreenFill::setImage on the record's +0x3c.
    void setFillImage(const void *image);                                // FS2 0x001fe9b0
    // A new Tim2Texture of image joins the record's list (+0x54).
    void addImage(const void *image);                                    // FS2 0x001fe890
    // The record's +0x40 texture and the +0x44 object over it, rebuilt.
    void setImage(const void *image);                                    // FS2 0x001fe900
    // The frame's step and draw on the record (0x203240 / 0x2024c0).
    void update();                                                       // FS2 0x001feb80
    void draw();                                                         // FS2 0x001feb70
    // The record's "camjack" camera.
    void createCamera();                                                 // FS2 0x001febb0
    // Mode 4, object the companion.
    void setCompanion(StageObject *object);                              // FS2 0x001fe9c0
};

// A singly linked list whose nodes {next, flags} carry a keep bit (the
// stage object's +0x30). Only what stageThread calls.
// layout: host -- EE: +0x4 head, +0x8 tail.
struct KeepList
{
    KeepList();                                                          // FS2 0x002bc770: three words zero
    void *data;                 // FS2 +0x0: the init pack's item (StageFiles::loadList)
    ~KeepList();                                                         // FS2 0x002bc6f0: deleting; deletes every node
    // Unlinks and deletes every node whose flags bit 0 is clear.
    void removeUnkept();                                                 // FS2 0x002bc5d0
    // Advances the nodes by frames; the nodes of source (null all) give the
    // two motors' values and up to max flags and tags.
    void update(float frames, u_char *motorA, u_char *motorB, char *flags, int *tags,
                int max, StageObject *source);                           // FS2 0x002bc100
};

// The stage object's +0xec: what a sub-scene starts from (the 0x287xxx
// classes, vtables 0x318020..; their own TU). Only what stageThread calls.
// layout: host -- EE: vptr at +0x2c.
enum StageEventState
{
    StageEvent_Active = 2,      // StageScene::isEventActive; the name is a guess
};
// StageEvent::poll: the event has ended (StageScene::runStageScripts
// deletes it).
constexpr int kStageEventFinished = 3;
class ScriptRunner;
// layout: host
class StageEvent
{
public:
    // The first subtitle font for a language of the set {1, 5..9}, the
    // event's own otherwise (the argument is unused; the language's
    // meaning is unread).
    bool usesLanguageFont();                                             // FS2 0x00287af0
    // SubSceneObject's flag: the event is not d0101, or SaveData flag
    // (1, 0x15) is set. "Skippable" is a guess.
    bool isSkippable();                                                  // FS2 0x00287b70
    // out = the voice stream's file, .xwb; null with no voice.
    const char *getVoicePath(char *out);                                 // FS2 0x00287bb0

    // Slot 2 (vtable +0x10, vptr +0x2c): the event's state; a plain stub
    // until its TU.
    int poll();
    // A virtual in the ROM (slot 3, vtable +0x14); a plain stub until its
    // TU. What StageScene::enterSubScene builds the sub-scene from.
    void *getSubSceneData();
    // The deleting dtor (vtable +0x8, vptr +0x2c: StageScene's dtor); a
    // plain stub until its TU.
    void destroy();
    // Slot 1 (vtable +0xc, vptr +0x2c): set, StageScene::draw leaves the
    // pool's text out. A plain stub until its TU.
    bool hidesText();

    char   name[0x24];          // FS2 +0x00: the event's id (d0101); the extent is a guess
    int    state;               // FS2 +0x24: StageEventState
    ScriptRunner *runner;       // FS2 +0x28: StageScene::isBusy: its script may go on while the event runs
    u_char _2c[0x8];
    const void *voice;          // FS2 +0x34: the voice stream's header (DemoStream::open); null none
    void       *fsm;            // FS2 +0x38: subtitles (DemoStream::setFsm)
    const void *subtitleImage;  // FS2 +0x3c: their font's image
    const void *subtitleFont;   // FS2 +0x40: their font
};

// The event a sub-scene runs from (vtable 0x318010, EE 0xf4: a FileLoader
// at +0x44): arg goes to +0x20. The name is a guess.
// layout: host -- EE 0xf4.
class StageFileEvent : public StageEvent
{
public:
    StageFileEvent(float arg, const char *name);                         // FS2 0x002882b0
};
// The other one (vtable 0x318030). The name is a guess.
// layout: host -- EE 0x34.
class StageSimpleEvent : public StageEvent
{
public:
    StageSimpleEvent(float arg, const char *name);                       // FS2 0x00288850
};

class Clock;                    // game/clock.h

// What StageScene::raycast answers (from 0x233810). Only the flag
// StageSound tests.
// layout: host -- EE: +0x40 flags.
enum StageCollisionFlag
{
    StageCollision_Thin = 0x400,    // sound passes once (StageSound: twice the distance)
    StageCollision_Door = 0x4000000, // doorId is a door's (StageScene::isDoorFace)
};
struct StageCollisionFace
{
    u_char _00[0x40];
    u_int  flags;               // FS2 +0x40: StageCollisionFlag
    u_short attr;               // FS2 +0x44: StageScene::getFloorAttr
    short  doorId;              // FS2 +0x46: kDoorFaceBase + what StageScene::findDoorOfFace decodes
};
static_assert(offsetof(StageCollisionFace, flags) == 0x40, "StageCollisionFace::flags");
static_assert(offsetof(StageCollisionFace, attr) == 0x44, "StageCollisionFace::attr");
static_assert(offsetof(StageCollisionFace, doorId) == 0x46, "StageCollisionFace::doorId");

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
    // The sphere at pos touches the mesh. mode 1 at every call read; out
    // takes a word nobody reads.
    bool sphereHits(float radius, int mode, void *out, u_int mask, u_int flags,
                    const float *pos);                                   // FS2 0x00232870
    // The segment from..to touches the mesh.
    bool segmentHits(const float *from, const float *to, u_int mask, u_int flags,
                     int arg);                                           // FS2 0x00233160
    // A mesh without nodes: the face under x, y, *height its height.
    StageCollisionFace *heightAt(float x, float y, float range, float *height, u_int mask,
                                 u_int flags, int arg);                  // FS2 0x002342c0

    void *nodes;                // FS2 +0x0: StageScene::findFloorFace casts a ray through a mesh with nodes
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
    // The subtitle of the stream's time.
    void drawSubtitles(sceVif1Packet *packet);                           // FS2 0x002b0d60
    // The subtitles' fsm file, relocated; false when it is not one.
    bool setFsm(void *fsm);                                              // FS2 0x002b11a0
    // A new printer on font and image; false when either is null or the
    // font does not take.
    bool setFont(const void *font, const void *image);                   // FS2 0x002b1010
    // Opens the voice: header, the directory + name.
    bool open(const void *header, const char *name);                     // FS2 0x002b1270
    bool play(int volume);                                               // FS2 0x002b1240: left = right = volume
    bool setPaused(bool on);                                             // FS2 0x002b1110
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
    // The same in mode 1.
    bool isFullInMode1();                                                // FS2 0x001512e0
    // Slot 2 (vtable +0xc, vptr +4). A plain stub until its TU.
    void draw(sceVif1Packet *packet, TexAreaList *texList);
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
    // The frame's action steps over the five lists: slots +8, +0xc, +0x10,
    // +0x14 of every entry that is on (the perf bars' names).
    void actionStart();                                                  // FS2 0x001714e0
    void actionControl();                                                // FS2 0x00171440
    void actionMovement();                                               // FS2 0x001713a0
    void actionTerminate();                                              // FS2 0x00171300
};

// +0x18: four arrays of records reserved for 0x1fe each. layout: host (EE 0x70).
struct SceneRecordBuckets
{
    SceneRecordBuckets();                                                // FS2 0x001f6510
    ~SceneRecordBuckets();                                               // FS2 0x001f63f0: deleting
    void collide();                                                      // FS2 0x001f5250
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
    // The particles within range of view; arg is 0 here.
    void update(float range, int arg, StageObject *view, StagePlayer *player); // FS2 0x00190a90
    void draw(GamePacketManager *packets, StageObject *camera);          // FS2 0x00190a80
};

// +0x2c: tex areas, an AreaGrid and a 0x5a10 table; its dtor puts the
// parent scene's draw settings back. layout: host (EE 0xe70).
class AreaBox;
class Tim2Texture;              // game/gsutil.h
struct SceneRender
{
    SceneRender();                                                       // FS2 0x001fa4a0
    ~SceneRender();                                                      // FS2 0x001fa230: deleting
    // The map's ENV_CB / ENV_SB boxes (grids +0xdfc / +0xe00; what they
    // gate is unread).
    void addCbBox(AreaBox *box);                                         // FS2 0x001f6ad0
    void addSbBox(AreaBox *box);                                         // FS2 0x001f6ac0
    // loadCommon's spotlight and shadow_00 (+0xe2c / +0xe58).
    void setSpotlightTexture(Tim2Texture *texture);                      // FS2 0x001fa1c0
    void setShadowTexture(Tim2Texture *texture);                         // FS2 0x001fa150
    void update();                                                       // FS2 0x001f73c0
    void updateLights();                                                 // FS2 0x001f8620
    // gfxSetClearColor with the scene's colour.
    void applyClearColor();                                              // FS2 0x001f7380
    // yami_pl / demo_pl and the bn_ links, once.
    void setup();                                                        // FS2 0x001f94c0

    u_char spotActive;          // FS2 +0xe24: cleared each frame by StageScene::update
    int    spotGroup;           // FS2 +0xe28: sceneRenderGroup(0) each frame
};
// 2 -> 0x12, 1 -> 0x13, else -1.
int sceneRenderGroup(int mode);                                          // FS2 0x001f9c70

// +0x4c and +0x8c..+0x94: objects with the vptr at +0 (slot 0 their
// deleting dtor), set outside the ctor; unread. layout: host (unread).
class SceneOwnedObject
{
public:
    void destroy();                                                      // FS2 vtable +0x8
    // Slot +0x3c: it holds the player's actions (StageScene::playerAction).
    bool isBusy();                                                       // FS2 vtable +0x3c
    // The frame: slot +0xc once (setupCameras), +0x30 update, +0x34 draw.
    void prepare();                                                      // FS2 vtable +0xc
    void update();                                                       // FS2 vtable +0x30
    void draw();                                                         // FS2 vtable +0x34
    // Slot +0x40: it has a target; +0x38 which (prepareDrawFlags).
    bool hasTarget();                                                    // FS2 vtable +0x40
    StageObject *getTarget();                                            // FS2 vtable +0x38
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
    // Entry index of the first table (0x1c bytes each; inline in the ROM).
    // PC: table stays an offset from the script (above), so relocate's TU
    // must leave it one.
    const void *entry(int index) const { return (const char *)this + table + index * 0x1c; }
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
    void setup();                                                        // FS2 0x002dcc30
    void update(StageObject *camera);                                    // FS2 0x002dbd10
};

// A map file (the map keyword): six record sections after this 0x24-byte
// header, in file order. EE layout: it is the file. relocate sets the six
// section refs from the counts, in place: its TU must use S2dRef::set, not
// write 8-byte pointers. Each record starts with its name; the name's prefix
// picks the object (StageFiles::applyMapTarget).
struct StageMapPoint            // what stands somewhere: characters, items, P_ and BA_ marks
{
    char  name[0x10];           // FS2 +0x00
    float pos[3];               // FS2 +0x10
    float rot[3];               // FS2 +0x1c
    u_char _28[0x4];
};
static_assert(sizeof(StageMapPoint) == 0x2c, "StageMapPoint: EE 0x2c");
struct StageMapCamera
{
    char  name[0x10];           // FS2 +0x00
    float pos[3];               // FS2 +0x10
    float aim[3];               // FS2 +0x1c: from pos to what it looks at
    u_char _28[0x8];
};
static_assert(sizeof(StageMapCamera) == 0x30, "StageMapCamera: EE 0x30");
struct StageMapSpot             // ENV_CL, ENV_SL, YAM_CL, YAM_SL
{
    char  name[0x10];           // FS2 +0x00
    float pos[3];               // FS2 +0x10
    float rot[3];               // FS2 +0x1c
    float angle;                // FS2 +0x28: the full cone, radians
    float radius;               // FS2 +0x2c
    u_char _30[0x4];
};
static_assert(sizeof(StageMapSpot) == 0x34, "StageMapSpot: EE 0x34");
struct StageMapLight            // ENV_PL, YAM_PL; any other name a SphereVolume
{
    char  name[0x10];           // FS2 +0x00
    float pos[3];               // FS2 +0x10
    float radius;               // FS2 +0x1c
    u_char _20[0x4];
};
static_assert(sizeof(StageMapLight) == 0x24, "StageMapLight: EE 0x24");
struct StageMapBox              // the ENV_ boxes and the rest (applyMapTarget)
{
    char  name[0x10];           // FS2 +0x00
    float pos[3];               // FS2 +0x10
    float size[3];              // FS2 +0x1c: width, depth, height
    float rot[3];               // FS2 +0x28
    u_char _34[0x4];
};
static_assert(sizeof(StageMapBox) == 0x38, "StageMapBox: EE 0x38");
struct StageMapMark             // the last section: points without a rotation
{
    char  name[0x10];           // FS2 +0x00
    float pos[3];               // FS2 +0x10
    u_char _1c[0x4];
};
static_assert(sizeof(StageMapMark) == 0x20, "StageMapMark: EE 0x20");
struct StageMapFile
{
    void relocate();                                                     // FS2 0x00186ee0

    S2dRef<StageMapPoint>  points;      // FS2 +0x00
    S2dRef<StageMapCamera> cameras;     // FS2 +0x04
    S2dRef<StageMapSpot>   spots;       // FS2 +0x08
    S2dRef<StageMapLight>  lights;      // FS2 +0x0c
    S2dRef<StageMapBox>    boxes;       // FS2 +0x10
    S2dRef<StageMapMark>   marks;       // FS2 +0x14
    short pointCount;                   // FS2 +0x18
    short cameraCount;                  // FS2 +0x1a
    short spotCount;                    // FS2 +0x1c
    short lightCount;                   // FS2 +0x1e
    short boxCount;                     // FS2 +0x20
    short markCount;                    // FS2 +0x22
};
static_assert(sizeof(StageMapFile) == 0x24, "StageMapFile: EE 0x24");

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

// What a stage's map file becomes (StageFiles::applyMapTarget). The ROM
// news each with operator new aligned 0x40; the sizes are in the comments.
// All names but ENV_ / YAM_ / P_SPW are the port's. The classes' virtuals
// are read only as far as these calls.

// A script-run object: the map's name is the script's. Stage objects have
// one at +0x40 (so has a sub-scene, as its base).
// layout: host -- EE: vptr +0xb4; +4 the run depth.
class ScriptRunner
{
public:
    ScriptRunner();                                                      // FS2 0x001d3130
    // The script entry it runs.
    void setEntry(const void *entry);                                    // FS2 0x001d2f40
    // One step: it is gRunningScript meanwhile.
    void run(int arg);                                                   // FS2 0x001d27e0
    // The deleting dtor (vtable +0x18, vptr +0xb4); a plain stub until its TU.
    void destroy();

    int    m_running;           // FS2 +0xa8: non-zero while the script has more to do (StageScene::update)
    u_char m_skippable;         // FS2 +0xb9: a sub-scene's: start ends it
};
extern ScriptRunner *gRunningScript;    /* sbss 0x31ae18 (gp-0x7a58) */

// Named places with their script (vtables 0x3151c0 / 0x3151e4 / 0x3151fc;
// the ctor inline in the ROM; dtor 0x1a4800). layout: host -- EE 0x200.
class StageMarker : public StageObject
{
public:
    // The object list's group 6, then its script by name.
    void addToScene(const char *name);                                   // FS2 0x0017ef10: StageObjectList::add6
};

// vtable 0x311860; used for the map's cameras and by setupCameras.
// layout: host -- EE 0x300.
class StageCamera : public StageObject
{
public:
    // scrZ is the view-screen distance of sceVu0ViewScreenMatrix; farZ
    // only stored here.
    StageCamera(float scrZ, float width, float height, float farZ);      // FS2 0x00177d60
    void addToScene(const char *name);                                   // FS2 0x00177d20: StageObjectList::add0
    // Turns the transform's rotation to look at target.
    void lookAt(const float *target);                                    // FS2 0x00177900
    // The view matrix (+0x240) and view-screen matrix (+0x280) from the
    // object's matrix; each frame for the active camera.
    void calcViewMatrix();                                               // FS2 0x00177810
};
// The stage's "camera" (kind 9; vtable 0x311fa0). layout: host -- EE 0x3c0.
class StageMainCamera : public StageCamera
{
public:
    StageMainCamera();                                                   // FS2 0x00179700
};
// "camview", "camview_zoom", "camview_snipe" (kind 10; vtable 0x3185b0; run
// unassigned@00299d90). layout: host -- EE 0x440.
class ViewCamera : public StageCamera
{
public:
    ViewCamera();                                                        // FS2 0x0029aae0
};
// A sub-scene's "camera_demo" (kind 13; vtable 0x316ed0; run
// unassigned@002681e0). layout: host -- EE 0x340.
class DemoCamera : public StageCamera
{
public:
    DemoCamera();                                                        // FS2 0x0026ce20
    // The fades on list 9 after the 3D frame.
    void drawOverlay();                                                  // FS2 0x0026c920
};
// A bullet (named "bullet", group 4; vtables 0x312ca0 / 0x312f40; the args
// are 0 here: unread). setupCameras makes one, off, at the start.
// layout: host -- EE 0x2c0.
class StageBullet : public StageObject
{
public:
    StageBullet(int arg0, int arg1);                                     // FS2 0x001803e0
};

// The two light classes' base (ctor 0x1866f0; vtables 0x313e30 / 0x3141b0).
// layout: host -- EE offsets in the comments.
class AreaBox;
class StageLight : public StageObject
{
public:
    StageLight();                                                        // FS2 0x001866f0
    void setRadiusSquared(float radiusSquared);                          // FS2 0x00185bb0
    // The vector +0x220; the map's ENV_ lights all pass (1, 0, 0). What it
    // is (a colour, an axis) is unread.
    void setVector(float x, float y, float z);                           // FS2 0x00185c40
    void setEnabled(bool on);                                            // FS2 0x00185fb0: +0x200 and +0x36
    // withShade: the applyMapTarget calls always pass 1 (0x185c60 runs).
    void addToScene(const char *name, bool withShade);                   // FS2 0x001861b0: StageObjectList::add5
    // A sub-scene's light shows what the parent's light of its name shows.
    void copyShownFrom(StageLight *other);                               // FS2 0x00186160

    u_char m_envLight;          // FS2 +0x271: ENV_ lights
    StageList<AreaBox *> m_boxes; // FS2 +0x278: the boxes named after the light
    u_char m_yamLight;          // FS2 +0x2a1: YAM_ lights; off at the start
};
// ENV_CL, ENV_SL, YAM_CL, YAM_SL. vtable 0x313e30. layout: host -- EE 0x300.
class SpotLight : public StageLight
{
public:
    SpotLight();                                                         // FS2 0x00185260
    void setConeAngle(int degrees);                                      // FS2 0x00185210: half the cone
};
// ENV_PL, YAM_PL. vtable 0x3141b0. layout: host -- EE 0x2c0.
class PointLight : public StageLight
{
public:
    PointLight();                                                        // FS2 0x001856b0
};

// The rest of the light and box sections: scripted volumes (base vtable
// 0x3127f0). "Volume" is a guess: their script hears them.
// layout: host -- EE 0x240.
class StageVolume : public StageObject
{
public:
    // The object list's group 7, then its script by name.
    void addToScene(const char *name);                                   // FS2 0x0017ecb0: StageObjectList::add7
};
// layout: host
class SphereVolume : public StageVolume
{
public:
    SphereVolume();                                                      // FS2 0x0017ec30
    void setRadius(float radius);                                        // FS2 0x0017ec20
};
// layout: host
class BoxVolume : public StageVolume
{
public:
    BoxVolume();                                                         // FS2 0x0017ea90
    void setSize(float width, float height, float depth);                // FS2 0x0017ea50
};

// A box of the map (vtable 0x314180 over StageNode, EE 0x140): the
// AreaGrids hold these. The box +0x100..+0x118 is centred on x / y; z runs
// from 0 to height, whichever way height points.
// layout: host
class AreaBox : public StageNode
{
public:
    AreaBox();                                                           // FS2 0x00284950
    void setSize(float width, float height, float depth);                // FS2 0x002848d0
};
// The boxes with a number in their name (+0x140; EE 0x180, the vtables
// 0x315190 / 0x315160 and their dtors inline). ENV_CB%d: unread.
// layout: host
class CbAreaBox : public AreaBox
{
public:
    int m_number = 0;           // FS2 +0x140
};
// ENV_MB_f%02d: the floor the box is on (StageScene::getFloor).
// layout: host
class FloorAreaBox : public AreaBox
{
public:
    int m_floor = 0;            // FS2 +0x140
};

// A character of the map (the F_ and player points). vtable at 0x17de30.
// layout: host -- EE 0x800.
class StageCharacter : public StageObject
{
public:
    StageCharacter();                                                    // FS2 0x0017de30
    // Model, textures, the model's two motion lists and mdl.
    void setup(const char *name, ModelInstance *instance, StringMap<Tim2Texture *> *textures,
               StageList<void *> *motions, StageList<void *> *motions2, void *mdl); // FS2 0x0017c700
};

// The item classes (StageFiles::placeItem, by ItemDef::kind and name).
// layout: host -- EE sizes in the comments.
class StageItem : public StageObject
{
public:
    StageItem();                                                         // FS2 0x00183f40: EE 0x240
    // Slot +0x24c in these classes. A plain stub until its TU.
    void setupItem(const char *name, ModelInstance *instance,
                   StringMap<Tim2Texture *> *textures, StageList<void *> *motions); // FS2 vtable +0x24c
    // A weapon, once m_weaponDef is set.
    void initWeapon();                                                   // FS2 0x001823f0
};
// layout: host
class StageItemE : public StageItem     // ItemKind_E
{
public:
    StageItemE();                                                        // FS2 0x00182ad0: EE 0x280
};
// layout: host
class StageWeaponItem : public StageItem    // kinds below 7
{
public:
    StageWeaponItem();                                                   // FS2 0x00182670: EE 0x480
};
// layout: host
class StageTrapItem : public StageItem      // I_WANA
{
public:
    StageTrapItem();                                                     // FS2 0x00181200: EE 0x280
};
// layout: host
class StageThrownItem : public StageItem    // I_HATUEN, I_AKIKAN, I_PET, I_NUT
{
public:
    StageThrownItem();                                                   // FS2 0x00180d30: EE 0x480
};

// A sub-scene's characters and items (vtables at their ctors; a lighter
// family like SubSceneMapObject). layout: host.
class SubSceneCharacter
{
public:
    explicit SubSceneCharacter(int arg);                                 // FS2 0x002bbab0: always 2; EE 0x280
    void setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures);                      // FS2 0x002bb870
};
// layout: host
class SubSceneItem
{
public:
    explicit SubSceneItem(int arg);                                      // FS2 0x002bb7a0: always 3; EE 0x280
    void setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures);                      // FS2 0x002bb680
};
// Any other item; vtables 0x315420 / 0x315444 / 0x31545c, the ctor inline
// in the ROM over 0x269d00. EE 0x240.
// layout: host
class SubSceneModelObject
{
public:
    explicit SubSceneModelObject(int arg);                               // FS2 0x00269d00
};
// layout: host
class SubSceneProp : public SubSceneModelObject
{
public:
    explicit SubSceneProp(int arg) : SubSceneModelObject(arg) {}
    void setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures);                      // FS2 0x002bb810
};

// The script's number of a player spawn name (P_SPW%02d); -1 not one.
int parseSpawnId(const char *name);                                      // FS2 0x0029fb30

class StageObjectList;          // game/stageobjlist.h
struct SceneRender;
// +0x9c: eight blocks from +0x18. layout: host.
struct SceneBufferSet
{
    ~SceneBufferSet();                                                   // FS2 0x002ce5a0: deleting
    // The first time, the characters fill the eight blocks.
    void attachCharacters(StageObjectList *objects);                     // FS2 0x002ce3e0
    // Each block whose owner became or stopped being linked: shown or
    // hidden, render->spotActive set.
    void setActiveFor(StageObject *linked, SceneRender *render);         // FS2 0x002ce0c0
    // A block near a listed object within range shows.
    void updateRange(float range, StageObject *linked, SceneRender *render); // FS2 0x002ce1c0
    // Each block's +4 = its owner is linked.
    void storeActive(StageObject *linked);                               // FS2 0x002ce180

    int used;                   // FS2 +0x14: zeroed each frame by StageScene::update
};
// +0x98: only the byte StageScene::update sets. layout: host -- EE: +0xc.
struct SceneBlock98
{
    // A change of on: the node shown or hidden, render->spotActive set.
    void setActive(bool on, SceneRender *render);                        // FS2 0x002ceb70
    void updateVisible(SceneRender *render);                             // FS2 0x002ce790
    // The node hangs from a bone of the player.
    void attachTo(StagePlayer *player);                                  // FS2 0x002cebd0

    u_char active;              // FS2 +0x4
    u_char dirty;               // FS2 +0xc
};

// +0xa0: a colour and a screen rect (vtable 0x311280 over StageFade's base
// 0x311260, vptr +4). layout: host (EE 0x24).
class ScreenFill
{
public:
    ScreenFill(u_int rgba, int x, int y, int w, int h);                  // FS2 0x00151f20
    // A new Tim2Texture of image at +8, the old one deleted.
    void setImage(const void *image);                                    // FS2 0x00151e10
    void destroy();                                                      // FS2 vtable +0x8 (vptr +4)
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 vtable +0xc (vptr +4)
};

// +0xa4 / +0xa8: menu program holders (beside PrgMcCard), set outside the
// ctor. layout: host.
struct MenuProgHolder
{
    // A new 0x1cc0-byte program object (0x2aef00) on the init pack's item.
    MenuProgHolder(const void *data, int stage);                         // FS2 0x002ad100
    ~MenuProgHolder();                                                   // FS2 0x002ad070: deleting
    // The program's frame (on the scratchpad stack in the ROM).
    void draw(float deltaSeconds, GamePacketManager *packets, StageObject *camera); // FS2 0x002ad030
};
// layout: host
struct MenuProgSet
{
    MenuProgSet();                                                       // FS2 0x002bb4e0: seven words zero
    ~MenuProgSet();                                                      // FS2 0x002bb3e0: deleting
    // Six textures over the init pack's images.
    void load(void *image0, void *image1, void *image2, void *image3, void *image4,
              void *image5);                                             // FS2 0x002bb2b0
};

// +0xac..+0xb8: four area grids (a list, 0x40 cells, a box at +0x210).
// layout: host (EE 0x240).
class AreaBox;
struct AreaGrid
{
    AreaGrid();                                                          // FS2 0x00284640
    ~AreaGrid();                                                         // FS2 0x00284580: deleting
    // pos inside one of the grid's areas.
    int  contains(const sceVu0FVECTOR pos);                              // FS2 0x00284190
    void addBox(AreaBox *box);                                           // FS2 0x00284560
    // The boxes at pos, at most max of them; how many.
    int  collect(const sceVu0FVECTOR pos, AreaBox **out, int max);       // FS2 0x00283ff0
    // Bounds and cells from the boxes added so far.
    void build();                                                        // FS2 0x002842f0
};

// The frame-buffer filter of the view cameras and event d0101: passes
// times over the w x h frame of env, block the VRAM block (32-block
// pages), arg 1 or 5.
void gsFrameFilter(sceVif1Packet *packet, const sceGsDrawEnv1 *env, int w, int h,
                   u_short block, int passes, int arg);                  // FS2 0x002849c0
// A flat sprite from (x, y) to (x2, y2) of the 512 x 512 frame.
void gsFillRect(sceVif1Packet *packet, int x, int y, int x2, int y2, u_int rgba); // FS2 0x002eaeb0

// +0xc0: (count 4, 0x800, 0x878). layout: host (EE 0x2c).
class WideFontPrinter;          // game/fontdata.h
struct ScenePool
{
    ScenePool(u_int count, int size, int size2);                         // FS2 0x00289be0
    ~ScenePool();                                                        // FS2 0x00289b60: deleting
    // Text for slot index; -1 past the last.
    int  setSlot(int index, const u_short *text);                        // FS2 0x00289b20
    void setImage(const void *image);                                    // FS2 0x002894b0
    void setPrinter(WideFontPrinter *printer);                           // FS2 0x00289b50
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x00289800
};

// +0xc4: plain data beside the pause menu's code. layout: host (EE 0x678).
struct PauseMenuState
{
    PauseMenuState();                                                    // FS2 0x00207ef0
    ~PauseMenuState();                                                   // FS2 0x00207ea0: deleting
    void update();                                                       // FS2 0x00207500
    // One of its five entries is on and holds the scene clock's time.
    bool isActive();                                                     // FS2 0x002078d0
};

class WideFontPrinter;          // game/fontdata.h

// +0xcc: a list view over the scene's printer (vtable 0x316530 at +0x88).
// layout: host (EE 0x8c).
// One of UiItemList's two slots (front and back). Named from the fields'
// use only. layout: host -- EE 0x14.
struct UiPickSlot
{
    // object set and timer past 6.
    bool isReady();                                                      // FS2 0x0023e740

    void  *object;              // FS2 +0x00
    int    timer;               // FS2 +0x04
    int    _08;                 // FS2 +0x08
    const void *label;          // FS2 +0x0c
    u_char enabled;             // FS2 +0x10
};
// layout: host
class UiItemList
{
public:
    UiItemList(WideFontPrinter *printer, int arg);                       // FS2 0x0023f0b0
    void destroy();                                                      // FS2 vtable +0x8 (vptr +0x88)
    void update();                                                       // FS2 0x0023e7b0
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x0023e770
    // The front slot is used; negative when nothing could be.
    int  pick();                                                         // FS2 0x0023ee90
    // The init pack's sheet (the ROM asks its size too, and drops it).
    void setSheet(void *sheet);                                          // FS2 0x0023f030

    // The slot in front (flip's bit 0 picks it; update fills the other).
    UiPickSlot *frontSlot() { return &slots[flip & 1]; }

    UiPickSlot slots[2];        // FS2 +0x58 (0x14 each)
    u_int      flip;            // FS2 +0x80
    int        pendingCount;    // FS2 +0x84: StageScene::getMenuMode; the name is a guess
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
    void update();                                                       // FS2 vtable +0xc (vptr +8)
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 vtable +0x10 (vptr +8)
    void setData(void *data);                                            // FS2 0x00230810

    int    state;               // FS2 +0x0: StageMenuState
    u_char _04[0xc];
    int    pending;             // FS2 +0x10: StageScene::getMenuMode; the name is a guess
    u_char pendingOn;           // FS2 +0x14
};

// +0xdc: 0xc-byte entries after a count. layout: host (EE 0xc4).
struct SceneBoxList
{
    SceneBoxList();                                                      // FS2 0x0023ad60
    ~SceneBoxList();                                                     // FS2 0x0023ad10: deleting
    void update();                                                       // FS2 0x0023aca0
};

// +0xe0. layout: host (EE 0x3cc).
struct SceneUiState
{
    SceneUiState();                                                      // FS2 0x00243150
    ~SceneUiState();                                                     // FS2 0x00243100: deleting
    // Sixteen entries look their sound source up (StageSound::findSource)
    // and fill the back one of two lists; then the lists flip.
    void pollSoundSources();                                             // FS2 0x00242e80
};

// +0xe4: a 48-bit LCG (drand48's multiplier 0x5deece66d; the run
// 0x2681e0..0x268310). layout: host (EE 8).
struct Rand48
{
    Rand48();                                                            // FS2 0x002682e0
    // A number in 0..n-1.
    int next(int n);                                                     // FS2 0x002681e0
    void seed(u_int value);                                              // FS2 0x00268290
};

// +0xf4: the ALERT / CAUTION state machine (game/stagealert.cpp).
// layout: host (EE 0x1c).
struct StageAlert
{
    StageAlert();                                                        // FS2 0x00264070
    ~StageAlert();                                                       // FS2 0x00264000: deleting
    u_long update();                                                     // FS2 0x00263c10
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x00263ba0
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
    GameOverSequence(int arg, int arg2);                                 // FS2 0x002b1cb0
    int getState();                                                      // FS2 0x002b1c50
    void update();                                                       // FS2 0x002b1670
};
// layout: host (unread).
struct ClearSequence
{
    ClearSequence();                                                     // FS2 0x002b1660
    int getState();                                                      // FS2 0x002b1620
    void update();                                                       // FS2 0x002b1500
};

// +0x120 (StageScene::update deletes it too). layout: host.
struct SceneDirector
{
    SceneDirector();                                                     // FS2 0x002b22d0
    ~SceneDirector();                                                    // FS2 0x002b2280: deleting
    // A colour ramp still has frames to go.
    bool isRunning();                                                    // FS2 0x002b2270
    // From colour from to colour to over frames frames.
    void start(int frames, u_int from, u_int to);                        // FS2 0x002b2250
    // Ramps the colour and fills the screen with it.
    void draw(sceVif1Packet *packet);                                    // FS2 0x002b20a0

    u_int colour;               // FS2 +0x0: the ramp's colour now
};

// +0x124: a record of two arrays. The ROM frees the second as an array
// with CodeWarrior's 0x10-byte header (p - 0x10) and runs no element dtor.
// layout: host -- EE: +8, +0xc.
struct SceneArrayPair
{
    // Room for every object but group 4's (inline in setupCameras).
    SceneArrayPair();
    // Every object's saved flags back (3 bytes each, group 4 left out).
    void restoreFlags();                                                 // FS2 0x0019c6a0
    // The same walk the other way: the bytes and every transform.
    void saveFlags();                                                    // FS2 0x0019c960

    u_long         count;       // FS2 +0x0
    u_char        *first;       // FS2 +0x8: delete[]; 3 bytes per object
    NodeTransform *second;      // FS2 +0xc: delete[] (header at -0x10 on the EE); one per object
};

// +0x12c: an object with a member at +0x24 the dtor destroys in place.
// layout: host.
struct SceneModel
{
    ~SceneModel();                                                       // FS2 0x002cff30 (in place: flag -1)
};
struct SceneModelHolder
{
    SceneModelHolder(StageNodeTree *tree, int stage);                    // FS2 0x002d07a0
    void draw(float deltaSeconds, sceVif1Packet *packet, StageObject *camera); // FS2 0x002d0550

    u_char     _00[0x24];
    SceneModel model;           // FS2 +0x24
};

// The draw system's record stack (gp-0x7994, saved in gp-0x7990): a
// sub-scene draws into its own record.
void drawSysPush();                                                      // FS2 0x002d7c70
void drawSysPop();                                                       // FS2 0x002d7c00
// The record's camera object (+0xc): the view and screen matrices go there.
// layout: host -- EE: slots +0x18, +0x1c.
class DrawSysCamera
{
public:
    void setViewMatrix(const void *matrix);                              // FS2 vtable +0x18
    void setScreenMatrix(const void *matrix);                            // FS2 vtable +0x1c
};
// layout: host -- EE: +0xc the camera.
struct DrawSysRecord
{
    DrawSysCamera *camera;      // FS2 +0xc
};
DrawSysRecord *drawSysCurrent();                                         // FS2 0x002d4fd0
// The frame's 3D draw ("rena draw"); env is the frame's, flags 0 here.
void drawSysRender(GamePacketManager *packets, const sceGsDrawEnv1 *env, int flags); // FS2 0x002d5560
// Advances every record by ticks of 1/300 s.
void drawSysTick(int ticks);                                             // FS2 0x002d4ef0
// The flag beside the view target's model mark (prepareDrawFlags).
void drawSysClearTargetView();                                           // FS2 0x002d4780
void drawSysSetTargetView();                                             // FS2 0x002d4790
float getDrawDistance();                                                 // FS2 0x002d9aa0
void  setDrawDistance(float distance);                                   // FS2 0x002d9ab0

// 44 groups at 0x599830 / 0x599860 reset (a byte zero, 32 words one each):
// a stage's ctor. What the groups are is unread.
void resetStageGroupTable();                                             // FS2 0x002cf510

// A sub-scene's own scene object (StageScene +0x8, from enterSubScene):
// the vptr at +0xb4 like the character classes; only the deleting dtor
// (vtable +0x18) is called here. layout: host (unread).
class SubSceneObject : public ScriptRunner
{
public:
    // vtable 0x314d30. flag: +0xb9; "skippable" is a guess.
    explicit SubSceneObject(bool skippable);                             // FS2 0x0026c8c0
    void destroy();                                                      // FS2 vtable +0x18 (vptr +0xb4)
    // Slot 13 (+0x3c) of 0x314d30 / 0x315850 is empty: FS2 0x0019cf80.
};

// What enterSubScene keeps at StageScene +0xc (one pointer, EE 4 bytes).
// layout: host.
struct SubSceneData
{
    explicit SubSceneData(void *pack) : pack(pack) {}                    // FS2 0x0026c8b0
    // The pack's script; null with none.
    StageScript *getScript();                                            // FS2 0x0026c880

    void *pack;                 // FS2 +0x0
};
// Goes over a relocated script's text tables (magic LBL, STR, SST); read
// only by its shape.
void scriptPrepare(StageScript *script);                                 // FS2 0x0029f050

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
    char   name[0x10];          // FS2 +0x00: a map point's (StageFiles::placeCharacter)
    int    kind;                // FS2 +0x10
    u_char _14[0x4];
    char   items[2][0x10];      // FS2 +0x18: items placed with the character; empty none
    u_char _38[0xc4];
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
    ItemKind_E            = 0xe,    // StageItemE
    ItemKind_F            = 0xf,    // the named special items
};

// getKind's value of a weapon item (StageFiles::placeItem).
constexpr int kStageObjectKindWeapon = 0x15;
// The weapon definitions (bss 0x576010, 0x50 x 0x54): only the name read.
constexpr int kWeaponDefCount = 0x50;
struct WeaponDef
{
    char   name[0x10];          // FS2 +0x00
    u_char _10[0x44];
};
static_assert(sizeof(WeaponDef) == 0x54, "WeaponDef: EE 0x54");
extern WeaponDef gWeaponDefs[kWeaponDefCount];  /* bss 0x576010 */

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
// the movie widget lends its player; menuPlayMovie sets it to the stage
// pack's buffer around a stage's opening movie.
void *getUiWorkArea();                                                   // FS2 0x00243380
void  setUiWorkArea(void *area);                                         // FS2 0x00243370

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
