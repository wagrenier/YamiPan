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

#include "game/areagrid.h"
#include "game/drawsys.h"
#include "game/fileloader.h"
#include "game/gamepaths.h"
#include "game/layoutpaths.h"
#include "game/menupack.h"
#include "game/seplayer.h"
#include "game/stageloop.h"
#include "game/stageobjlist.h"
#include "game/stagelist.h"
#include "game/stagenode.h"
#include "game/stagesound.h"
#include "game/streamring.h"
#include "game/uilayout.h"
#include "game/uiworkarea.h"
#include "game/vu0math.h"
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

// Empty in retail: a stripped system's init / per-frame / teardown (guess).
void debugSysInit();                                                     // FS2 0x00151290
void debugSysUpdate();                                                   // FS2 0x00151270
void debugSysShutdown();                                                 // FS2 0x00151280

// True when the point (16-aligned xyzw) lies in the box whose world-to-unit-
// cube matrix is m (VU0 transform + vclip against w; game/boxgrid.cpp).
bool boxContainsPoint(const float m[4][4], const float p[4]);            // FS2 0x002362a0

// What game/scene.cpp calls.
// The resident menu pack is game/menupack.h; loadInitPack game/gamepaths.h.

// What game/menuscene.cpp calls.
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

// PackTables moved to game/packtables.h (game/packtables.cpp).

// StageSound moved to game/stagesound.h (game/stagesound.cpp).

// StageObject::getKind: the ones game/uigame.cpp and StageSound test.
enum StageObjectKind
{
    StageObject_Character = 0,  // StageCharacter; the player and the partner are ones
    StageObject_MapObject = 1,  // a plain map object (game/mapobject.cpp)
    StageObject_Door   = 2,     // StageObject::m_doorState is read
    StageObject_Kind3  = 3,     // takes the motion named CAR (StageFiles::applyBgMotionTarget)
    StageObject_Kind6  = 6,     // takes the motion named Seven_Keys
    StageObject_Camera = 7,     // StageCamera
    StageObject_Kind8  = 8,     // a camera, vtable 0x311d30 (no host class yet)
    StageObject_Kind9  = 9,     // the main camera "camera" (StageMainCamera): hears from its m_linked's head
    StageObject_Kind10 = 0xa,   // the camera "camview" and its two variants (ViewCamera)
    StageObject_Kind11 = 0xb,   // the party's "camjack" camera: its m_linked record may muffle
    StageObject_Kind13 = 0xd,   // a sub-scene's "camera_demo" (DemoCamera)
    StageObject_Kind21 = 0x15,  // left out of the inventory (unidentified)
    StageObject_Light  = 0xf,   // StageLight
    StageObject_PointLight = 0x10,
    StageObject_SpotLight = 0x11,
    StageObject_Item   = 0x13,  // StageItem
    StageObject_ItemE  = 0x14,
    StageObject_WeaponItem = 0x15, // left out of the inventory
    StageObject_TrapItem = 0x16,
    StageObject_ThrownItem = 0x17,
    StageObject_BulletBase = 0x18, // vtable 0x312f40
    StageObject_Bullet = 0x19,  // StageBullet
    StageObject_Marker = 0x1a,  // StageMarker
    StageObject_Volume = 0x1a,  // StageVolume: the same number as StageMarker
    StageObject_SphereVolume = 0x1b,
    StageObject_BoxVolume = 0x1c,
    StageObject_SubSceneDoor = 0x21, // SubSceneDoorObject
    StageObject_SubSceneProp = 0x24, // SubSceneProp
};

class StageNode;
class StageItemE;                // game/stageitem.h
class StageLight;               // game/stagelight.h
struct RenaSpotLight;           // game/scenerender.h
class VcmTable;                 // game/vcmtable.h
struct ModelNodeBone;
// layout: host -- EE 8.
struct ModelBone
{
    // rec's first word is the bind matrix: a ModelNodeBone, or the
    // ModelFileNode for the root (cast: only the matrix is read of it).
    explicit ModelBone(const ModelNodeBone *rec);                        // FS2 0x002e9790

    const ModelNodeBone *node;   // FS2 +0x0: its name, its parent
    const sceVu0FVECTOR *matrix; // FS2 +0x4: a StageBoneNode's world or a pose bone's, once bound
};
// Model flags (ModelInstance::flags).
enum ModelFlag
{
    ModelFlag_Item   = 0x2,     // every item's, from setupItem
    ModelFlag_CastsShadow = 0x8,  // drawSysBuildShadows takes it (cleared on oXX27_05)
    ModelFlag_Distant     = 0x40, // past a quarter of the squared draw distance, when StageModel::m_mayGoDistant is set
    ModelFlag_Ghost  = 0x100,   // the model ext_ghost (drawSysAddModel; what it does is unread)
    ModelFlag_TagC   = 0x200,   // a name tag _c, or oXX27_05 / oXX27_11 (drawSysAddModel): VU1 culls the faces turned from the eye
    ModelFlag_Frozen = 0x400,   // set by drawSysAddModel, clear only while a free item's matrix is dirty (the name is a guess)
};
class StageObject;
class Clock;                    // game/clock.h
class Tim2Texture;              // game/gsutil.h
struct ModelInstance;
class ModelPose;                // game/modelpose.h
struct ModelMotion;
class StageLight;
struct ModelSurface;
// What a motion's frame reported (StageModel::m_motionEvents).
enum MotionEvent
{
    MotionEvent_HitRightHand = 0x0001, // HIT_.._RH_.._BEGIN, or a HIT_ BEGIN with no limb
    MotionEvent_HitLeftHand  = 0x0002,
    MotionEvent_HitRightLeg  = 0x0004,
    MotionEvent_HitLeftLeg   = 0x0008,
    MotionEvent_HitHead      = 0x0010,
    MotionEvent_HitPelvis    = 0x0020,
    MotionEvent_HitEnd       = 0x0040,
    MotionEvent_HitSwing     = 0x0080,
    MotionEvent_StepLeft     = 0x0100,
    MotionEvent_StepRight    = 0x0200,
    MotionEvent_Click        = 0x0400,
    MotionEvent_Begin        = 0x0800,
    MotionEvent_End          = 0x1000,
    MotionEvent_Cancel       = 0x2000,
    MotionEvent_CancelW      = 0x4000,
    MotionEvent_Sound        = 0x8000,
};
constexpr int kModelMotionLists = 5;
constexpr int kModelSlots = 7;  // 0x1c0 aligned 0x40: three, three and one in the ROM's loops
constexpr int kModelFadeColor = 3;   // StageModel::m_lightFades: after the three directions
constexpr int kModelFadeAmbient = 6;
constexpr int kMotionFileName = 0x10; // a motion file's name
// A vector moving from one value to another over a number of frames: the
// seven at StageModel +0x140 (EE 0x38 in 0x40) and SceneRender's own.
// setFrom, frames and frame are set first; each frame setTo, then step.
struct alignas(0x40) VectorFade
{
    // FS2 0x00296cc0
    VectorFade() : from{}, to{}, value{}, frames(0), frame(0) {}
    // FS2 0x00296cb0
    void setFrom(const sceVu0FVECTOR v) { memcpy(from, v, sizeof(from)); }
    // FS2 0x00296ca0
    void setTo(const sceVu0FVECTOR v) { memcpy(to, v, sizeof(to)); }
    void step();                                                         // FS2 0x00296c10

    sceVu0FVECTOR from;         // FS2 +0x00
    sceVu0FVECTOR to;           // FS2 +0x10
    sceVu0FVECTOR value;        // FS2 +0x20
    int           frames;       // FS2 +0x30
    int           frame;        // FS2 +0x34
};
static_assert(sizeof(VectorFade) == 0x40, "VectorFade: EE 0x40");
typedef StringMap<StageNodeBase *> BoneMap;
typedef StringMap<ModelMotion *> MotionMap;
// What an object holds of its model: game/stagemodel.cpp. The ROM's ctor is
// inline in StageObject::setModel, and init clears everything again.
// layout: host -- EE offsets in the comments.
struct StageModel
{
    void updateVcm();                                                    // FS2 0x00187230
    // frames of the game's 30 of the motion, on position and rotation. arg
    // is not read (the callers pass 0).
    void advance(float frames, sceVu0FVECTOR position, sceVu0FVECTOR rotation, int arg); // FS2 0x00187260
    // Starts motion (an addMotion object); flags go to the pose.
    void playMotion(ModelMotion *motion, bool arg0, bool layered, u_int flags, int arg); // FS2 0x00187720
    // The same by name, from list index.
    void playNamed(int index, const char *name, bool arg0, bool layered, u_int flags, int arg); // FS2 0x00187770
    // The same with a motion file, null for the current one.
    void playMotionFile(void *file, bool arg0, bool layered, u_int flags, int arg); // FS2 0x00187820
    // Solves the pose towards position and rotation.
    void applyMotion(const sceVu0FVECTOR position, const sceVu0FVECTOR rotation); // FS2 0x00187870
    ModelMotion *findMotion(int index, const char *name);                // FS2 0x001879b0
    void *findMotionFile(int index, const char *name);                   // FS2 0x001879f0
    // Every motion list's tree evened out, after the loading.
    void balanceMotions();                                               // FS2 0x00187a30
    // A motion file into list index, under key (null: the file's name).
    ModelMotion *addMotion(void *file, int index, const char *key);      // FS2 0x00187a90
    void attachVcm(StageLight *obj);                                     // FS2 0x00187b40
    // Hangs the bones without a parent under owner.
    void placeBones(StageObject *owner);                                 // FS2 0x00187b70
    // The bones' link nodes; once, after setBaseMotion if there is one.
    void bindBones();                                                    // FS2 0x00187d10
    void setBaseMotion(void *file, int kind);                            // FS2 0x00188140
    ~StageModel();                                                       // FS2 0x00188690: deleting
    // matrix is the owner's.
    void init(ModelInstance *instance, StringMap<Tim2Texture *> *textures,
              const sceVu0FVECTOR *matrix);                              // FS2 0x00188b10

    // Inline in the ROM (bindBones).
    bool poseOwnsBone(int bone) const;

    ModelInstance *data = nullptr; // FS2 +0x00: the model (updateVcm tests it)
    VcmTable  *vcm = nullptr;   // FS2 +0x04
    int        m_poseKind = 0;  // FS2 +0x08: ModelPoseKind
    ModelPose *pose = nullptr;  // FS2 +0x0c
    StageMatrixNode *m_poseNodes = nullptr; // FS2 +0x10: pose->boneCount, a heap block
    BoneMap    boneMap;         // FS2 +0x14: the pose's nodes by bone name; StageSound finds "bn_head" in it
    StageBoneNode *m_boneNodes = nullptr; // FS2 +0x24: a heap block
    int        m_boneNodeCount = 0; // FS2 +0x28
    MotionMap  motions[kModelMotionLists]; // FS2 +0x2c
    sceVu0FVECTOR m_rotationStep = {}; // FS2 +0x80: what the last advance turned
    sceVu0FVECTOR m_positionStep = {}; // FS2 +0x90: what it moved
    float      m_motionFrames = 0.0f;  // FS2 +0xa0
    u_char     m_animateSurface = 0;   // FS2 +0xa4: nothing here sets it
    ModelSurface *m_kind1Surface = nullptr; // FS2 +0xa8
    u_char     m_loop = 1;      // FS2 +0xac: the pose's update flag
    u_int      m_motionEvents = 0; // FS2 +0xb0: MotionEvent
    u_char     m_lit = 0;       // FS2 +0xb4: SceneRender::updateLights gives it its own lights (items: StageObject::markModelAsItem)
    u_char     m_ownAmbient = 0; // FS2 +0xb5: and its own ambient
    StageObject *m_lightProxy = nullptr; // FS2 +0xb8: the lights are worked out at this object's place, not the owner's
    int        m_lightSet = -1; // FS2 +0xbc: the ENV_CB box's light set it stands in; StageObject::setModelValue
    u_char     m_inSbBox = 0;   // FS2 +0xc0: it stands in an ENV_SB box
    sceVu0FVECTOR m_lightDirs[3] = {};   // FS2 +0xd0
    sceVu0FVECTOR m_lightColors[3] = {}; // FS2 +0x100
    sceVu0FVECTOR m_ambient = {};        // FS2 +0x130
    VectorFade *m_lightFades = nullptr;  // FS2 +0x140: kModelSlots: the directions, the colours, the ambient
    sceVu0FVECTOR shadowSize = {};      // FS2 +0x150: x, y, z of the shadow box
    sceVu0FVECTOR shadowOffset = {};    // FS2 +0x160: from bone 2
    sceVu0FVECTOR shadowDirection = {}; // FS2 +0x170
    u_char     _180 = 1;        // FS2 +0x180
    void      *m_motionFile = nullptr; // FS2 +0x184: the base motion's
    u_char     _188 = 0;        // FS2 +0x188
    u_char     m_mayGoDistant = 1; // FS2 +0x189: lets ModelFlag_Distant be set; off on the kkb_ models
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
// vtable slot in all. Written in game/stageobject.cpp; where the subclasses
// use an offset differently, a union holds both.
//
// The ROM's bases, in order: the node (StageNodeBase, vptr +0), the side
// base at +0x18 (vptr +0x1c) and the script runner at +0x40 (vptr +0xf4).
// CodeWarrior keeps one block for the three tables and appends a class's
// new virtuals after the runner's: so the object's own slots read as +0x1f4
// and up from the first vptr.
// layout: host -- EE: as above, the transform at +0x140.
class StageObjectTree;
struct PlayerMotion;
class ScriptRunner;
class Pad;                      // game/pad.h
// What drives a character from the pad (+0x21c). layout: host -- EE: +0
// the flag, +8 the pad.
struct CharacterInput
{
    void setActive(bool on);                                             // FS2 0x00239bb0
    // The pad's +0x12c (a float of the character's pad class, unread): the
    // main camera leans by it while a swaying motion plays.
    float padSway() const;                                               // FS2 +0x8 -> +0x12c

    // The push on the view, -1..1: both are written by 0x239d50. Their
    // signs say which way they turn it (StageMainCamera::cameraStep).
    float lookPitch = 0.0f;     // FS2 +0x10
    float lookYaw = 0.0f;       // FS2 +0x18
};
// What keeps a character on the ground (+0x228, EE 0x1c0, ctor 0x1f3d20,
// CollisionOwner at +0; the run unassigned@00171300). Only what the main
// camera reads. layout: host.
struct GroundVolume;
struct StageCollisionFace;
struct CharacterGround
{
    // The floor near a point of the character's own space: from 70 above
    // it, down to 85 below that.
    StageCollisionFace *faceNear(const float *localPoint);               // FS2 0x001eff00

    GroundVolume *volume;       // FS2 +0x184: 0 from the ctor
};
// A box a character stands in (0x1f3d20 makes it). What it is is unread:
// the camera stays above its top.
// layout: host
struct GroundVolume
{
    u_char        active;       // FS2 +0x03
    sceVu0FVECTOR min;          // FS2 +0x20
    sceVu0FVECTOR max;          // FS2 +0x30
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
// The model instance's record of each mesh (vtable 0x31a7d0, 0x10 on the EE,
// parallel to ModelFileNode::meshes): the mesh tree, threaded through next,
// and what the game switches per part. modelFindPart hands them out: a
// character's +0x240 / +0x244, a light's PartSwitch, VcmTable.
// layout: host -- the EE's +0 is the vptr; the virtuals are trivial here.
struct ModelMesh;
struct ModelPartRec
{
    ModelPartRec(const ModelMesh *mesh, u_short next);                   // FS2 0x002e3ba0
    void   setFlagE(u_char value);                                       // FS2 0x002e3b50: slot 0 of vtable 0x31a7d0
    u_char getFlagE() const;                                             // FS2 0x002e3b60: slot 1

    u_short firstChild;         // FS2 +0x4: a mesh index
    u_short next;               // FS2 +0x6: the next sibling, or the parent's; 0 at the end
    u_short childCount;         // FS2 +0x8: 0 for a part grouping none
    u_char  vcmAlpha;           // FS2 +0xa: 0..0x80 (VcmTable::update)
    u_char  vcmFadeBits;        // FS2 +0xb
    u_char  vcmOffBits;         // FS2 +0xc
    u_char  drawMode;           // FS2 +0xd: StageScene::prepareDrawFlags (0 off, 2 on)
    u_char  flagE;              // FS2 +0xe: only the two virtuals (unread)
    bool    hasMesh;            // FS2 +0xf: ModelMesh::parts is set
};
// A script-run object: the map's name is the script's. Stage objects have
// one at +0x40 (so has a sub-scene, as its base).
//
// Vtable 0x315850, 108 slots: most are the script's commands, and this base
// answers each one with a default (0x315690 forwards them to a target).
// CodeWarrior put a copy of the defaults below beside DemoExecuter's vtable
// in game/stagescene.cpp, so these are the ones written so far; the other
// slots and the commands' meaning wait for this class's TU (unassigned@
// 001a68b0), which also fixes the placeholder names and argument types.
// Host order is not EE order yet: nothing calls a slot.
// layout: host -- EE: vptr +0xb4; +4 the run depth.
class ScriptRunner
{
public:
    ScriptRunner();                                                      // FS2 0x001d3130
    virtual ~ScriptRunner();                                             // FS2 0x001d30c0: deleting
    // The script entry it runs.
    void setEntry(const void *entry);                                    // FS2 0x001d2f40
    // One step: it is gRunningScript meanwhile.
    void run(int arg);                                                   // FS2 0x001d27e0

    // The command's default: logs "unsupported command" (Shift-JIS at
    // 0x304ab0; the log is empty in retail) and answers 0.
    int unsupportedCommand();                                            // FS2 0x001cf1b0
    // The default of most commands: 0.
    int defaultCommand();                                                // FS2 0x001cf1e0

    // Slot 7 (+0x24): the class's name; null here (0x1a7a10).
    virtual const char *getName() { return nullptr; }

    // FS2 0x0019cf50
    virtual int command10() { return 0; }
    // FS2 0x0019cf60
    virtual int command11() { return 0; }
    // FS2 0x0019cf70
    virtual int command12() { return 0; }
    // FS2 0x0019cf80
    virtual void command13(void *) {}
    // FS2 0x0019cf90
    virtual void command14(void *) {}
    // FS2 0x0019cfa0
    virtual void command15(void *) {}
    // FS2 0x0019cfb0
    virtual void command34(void *) {}
    // FS2 0x0019cfc0
    // callcheck: unsupportedCommand
    virtual int command39()
    {
        unsupportedCommand();
        return 0;
    }
    // FS2 0x0019cfe0
    // callcheck: defaultCommand
    virtual float command58(void *) { return (float)defaultCommand(); }
    // FS2 0x0019d010
    // callcheck: defaultCommand
    virtual float command59(void *) { return (float)defaultCommand(); }
    // A tail jump to defaultCommand.
    // FS2 0x0019d050
    virtual int command80() { return defaultCommand(); }
    // A tail jump to defaultCommand.
    // FS2 0x0019d040
    virtual int command83(void *, void *, void *) { return defaultCommand(); }
    // A tail jump to defaultCommand.
    // FS2 0x0019d060
    virtual int command87() { return defaultCommand(); }
    // FS2 0x0019d070
    virtual void command88() {}
    // FS2 0x0019d080
    // callcheck: defaultCommand
    virtual bool command99() { return defaultCommand() != 0; }
    // A tail jump to defaultCommand.
    // FS2 0x0019d0a0
    virtual int command100() { return defaultCommand(); }
    // A tail jump to defaultCommand.
    // FS2 0x0019d0b0
    virtual int command101(void *) { return defaultCommand(); }

    int    m_running = 0;       // FS2 +0xa8: non-zero while the script has more to do (StageScene::update)
    u_char m_skippable = 0;     // FS2 +0xb9: a sub-scene's: start ends it
};
extern ScriptRunner *gRunningScript;    /* sbss 0x31ae18 (gp-0x7a58) */
// The object's second base (EE +0x18): the view flag, then a vtable of four
// slots (empty in the base's table): an entry of ScenePairTable, whose four
// action steps call them. StageMarker's are empty; a light's third is its
// frame (StageLight::sideHook2), a spot's fourth trims it against the map.
// layout: host -- EE: +0 the flag, +4 the vptr.
class StageSideBase
{
public:
    virtual void sideHook0() {}
    virtual void sideHook1() {}
    virtual void sideHook2() {}
    virtual void sideHook3() {}

    u_char m_viewActive = 1;    // FS2 +0x018: 1 from the ctor; cleared off the view stack and on the map's cameras
};
// layout: host
class StageObject : public StageNodeBase, public StageSideBase, public ScriptRunner
{
public:
    // The family's base: vtables 0x314770 / 0x314794 / 0x3147ac; abstract
    // (slot +0x1f4 and the subclasses' slots +0x24c.. are empty there).
    StageObject();                                                       // FS2 0x0018b700
    ~StageObject() override;                                             // FS2 0x0018b4e0: deleting

    // The deleting dtor through the vtable (slot 2, StageObjectList's
    // teardown).
    void destroy() { delete this; }

    // The node's slots, the transform at +0x140.
    // FS2 0x00171ae0
    void setDirty(bool dirty) override { m_xform.dirty = dirty; }
    // The ROM prefetches the transform first.
    // FS2 0x00171af0
    // callcheck: update
    void rebuildMatrix() override { m_xform.update(m_parent); }
    // Row 1 is the facing axis, row 3 the position (StageSound's listener).
    // FS2 0x00171b10
    const sceVu0FVECTOR *getMatrix() override { return m_xform.world; }
    // FS2 0x00171b20
    const sceVu0FVECTOR *getWorldMatrix() override { return m_xform.world; }
    void setDirtyTree(bool dirty) override;                              // FS2 0x0018b0d0

    // The object's own slots (the ROM's +0x1f4..+0x248). +0x1f8, +0x1fc and
    // +0x230 (0x18a5b0, 0x189f40, 0x189ce0) are not written yet.

    // StageObjectKind. Pure in the ROM.
    virtual int getKind() = 0;
    // FS2 0x00172340
    virtual float getPosX() { return m_xform.position[0]; }
    // FS2 0x00172330
    virtual float getPosY() { return m_xform.position[1]; }
    // FS2 0x00172320
    virtual float getPosZ() { return m_xform.position[2]; }
    // FS2 0x00177640
    // callcheck: setPosition
    virtual void setPosition(const float *pos) { m_xform.setPosition(pos); }
    // Copies x, y, z of the rotation.
    // FS2 0x00177630
    // callcheck: setRotationVec
    virtual void setRotation(const float *rot) { m_xform.setRotationVec(rot); }
    // FS2 0x00177620
    virtual void setYaw(float yaw) { m_xform.rotation[2] = yaw; }
    // FS2 0x00172300
    virtual void setModelValue(int value)
    {
        if (m_model != nullptr)
            m_model->m_lightSet = value;
    }
    // FS2 0x00172350
    virtual const char *getObjectName() { return m_name; }
    // The object as a target; a class that stands for another overrides it.
    // FS2 0x001722d0
    virtual StageObject *getSelf() { return this; }
    // FS2 0x001722b0
    // callcheck: distanceToPoint
    virtual float distanceTo(StageObject *other) { return distanceToPoint(other->m_xform.position); }
    virtual float distanceToPoint(const sceVu0FVECTOR point);            // FS2 0x0018af00
    // FS2 0x00172290
    virtual float heightDifference(StageObject *other)
    {
        float diff = m_xform.position[2] - other->m_xform.position[2];
        return diff < 0.0f ? -diff : diff;
    }
    // Plays the named sound here; what can hear reports it.
    virtual void playNamedHeard(const char *name, const float *pos, StageNodeBase *parent); // FS2 0x0018aea0
    // A noise of the given loudness, without a sound.
    virtual void makeNoise(float loudness);                              // FS2 0x0018ae80
    // Empty in the base. What it is called for is unread.
    virtual void noiseHook();                                            // FS2 0x0018ae70
    // FS2 0x00172280
    // callcheck: getShownState
    virtual bool isShown() { return getShownState(); }
    // FS2 0x00172270
    // callcheck: setShownState
    virtual void setShown(bool on) { setShownState(on); }
    virtual void setTreeShown(bool on);                                  // FS2 0x00172260

    // Slots +0x60 / +0x68 of the runner's table, empty in the base; the
    // tree walks call them. On a light they are switched on and broken
    // (StageLight); StageScene::requestSubScene clears the first on lamps.
    // FS2 0x00171bf0
    virtual void setScriptFlag(bool on) { (void)on; }
    // FS2 0x00171c10
    virtual void setScriptFlag2(bool on) { (void)on; }

    // A character's number or, for the rest, the object's pointer: scripts
    // tell the two by the low bit (the serial is odd).
    intptr_t getHandle();                                                // FS2 0x0018af10
    // A new serial in m_serial, skipping 0.
    void assignSerial();                                                 // FS2 0x0018af70
    // Sound helpers: this is not used, the scene's StageSound is.
    void stopSound(StageSoundSource *source);                            // FS2 0x0018b130
    StageSoundSource *playNamed(const char *name, const float *pos, StageNodeBase *parent); // FS2 0x0018b170
    StageSoundSource *playEntry(const SeEntry *entry, const float *pos, StageNodeBase *parent); // FS2 0x0018b180
    // The same, reported to what can hear (entry +0xc loudness above 0).
    StageSoundSource *playEntryHeard(const SeEntry *entry, const float *pos, StageNodeBase *parent); // FS2 0x0018b190
    StageSoundSource *playNamedHeard2(const char *name, const float *pos, StageNodeBase *parent); // FS2 0x0018b210
    // The model's shown flag, or the object's without a model.
    bool getShownState();                                                // FS2 0x0018b290
    void setShownState(bool on);                                         // FS2 0x0018b2d0
    // m_active and m_viewActive together.
    void setActive(bool on);                                             // FS2 0x0018b320
    // The object's clock, under parent.
    void setClock(Clock *parent);                                        // FS2 0x0018b330
    // The object's model from instance; it takes the object's matrix over
    // from the model's root.
    void setModel(ModelInstance *instance, StringMap<Tim2Texture *> *textures); // FS2 0x0018b380
    // A new map object takes its node: the scene's lists, instance,
    // textures, collision.
    void setupMapObject(const char *name, ModelInstance *instance,
                        StringMap<Tim2Texture *> *textures, void *collision); // FS2 0x001760c0
    // The model's two item bytes (setupItem; SubSceneItem's and SubSceneProp's
    // setup call it).
    void markModelAsItem();                                              // FS2 0x00183ea0
    // Copies name to m_name (first thing in every class's addToScene).
    void setName(const char *name);                                      // FS2 0x0018b4d0
    // Slot +0x24c: the object is the view stack's top again
    // (StageViewStack::remove). StageMainCamera has its own; the other
    // cameras' are stubs until their TUs.
    virtual void onViewTop();                                            // FS2 vtable +0x24c
    // Slot +0x25c: a camera's step for the frame. The same.
    virtual void cameraStep();                                           // FS2 vtable +0x25c
    // Slot +0x260: a camera's m_linked changed. The same.
    virtual void onLinkChanged();                                        // FS2 vtable +0x260
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
    // setScriptFlag on the object and all of its tree.
    void setTreeScriptFlag(bool on);                                     // FS2 0x00185fc0
    // The script runner every object has as a base (EE +0x40).
    ScriptRunner *scriptRunner() { return this; }
    // What kind of body the character's record says, as one bit (bit 8:
    // SceneRender::update hangs yami_pl on it).
    u_int getCharacterClass();                                           // FS2 0x00216140
    // The character's model is called yri or kne.
    bool isYriOrKne();                                                   // FS2 0x00215e70
    // +0x5cc above 0: such a character's shadow may come from a light that
    // follows the player (SceneRender::updateLights). A plain stub until its TU.
    bool followsPlayerLight();                                           // FS2 +0x5cc
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

    // A camera's matrices: the object is a StageCamera. PC: the stand-in
    // family has no fields there (+0x200 and +0x240 mean other things in
    // other classes), they are StageCamera's own members.
    const void *cameraScreen() const;                                    // FS2 +0x200
    const void *cameraView() const;                                      // FS2 +0x240

    // A character's camera offsets for its motion, by body size; set: 1 in
    // an AB area (StageMainCamera). The motion's record must exist.
    const float *cameraEyeOffset(int set) const;                         // FS2 0x0017bb80
    const float *cameraFocusOffset(int set) const;                       // FS2 0x0017bbf0
    // How tall the body is, for the camera.
    float cameraBodyHeight() const;                                      // FS2 0x0017bb10

    // The name ends before +0x30 (a character's model there): 0x10 like an
    // ItemDef name. Was sized to +0x200 before (open-items.md TU 44).
    char   m_name[0x10];        // FS2 +0x020: an item's / weapon's name (an ItemDef name), the object's map key
    StageModel *m_model;        // FS2 +0x030: a character's model (StageSound: its bone map)
    u_char m_active;            // FS2 +0x034: the list walks (StageScene::raycast) skip an inactive object
    u_char m_shown;             // FS2 +0x035: 1 from the ctor; the model's flag replaces it (getShownState)
    u_char m_lightOn;           // FS2 +0x036: StageLight::setEnabled; 0 from the ctor
    u_char m_dead;              // FS2 +0x037: StageScene::update deletes the object at the frame's end; raycast leaves it out
    StageObjectTree *m_tree;    // FS2 +0x038: its node in the object tree (StageScene::restoreObjectFlags)
    u_int  m_serial;            // FS2 +0x03c: odd, from a counter (assignSerial); getHandle
    // +0x40: ScriptRunner, EE 0xb8 (a base on the host).
    Clock *m_clock;             // FS2 +0x0f8: StageCharacter's ctor sets one
    StageLight *m_modelLights[3]; // FS2 +0x0fc: the lights SceneRender::updateLights chose last frame; 0 from the ctor
    u_char _108[0x38];
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
        int    m_bodySize;      // FS2 +0x20c: a character's: 0..3 from its model's name (_lh_ _th_, _mh_ _ms_, _sh_ _ss_, _ch_), default 1 (StageCharacter::setup)
        StageCollision **m_collisions; // FS2 +0x20c: a Root object's meshes
    };
    u_char _210[0xc];
    CharacterInput *m_input;    // FS2 +0x21c: a character's
    CharacterBrain *m_brain;    // FS2 +0x220: a character's; null none
    PlayerMotion *m_motion;     // FS2 +0x224: a character's
    CharacterGround *m_ground;  // FS2 +0x228: a character's
    u_char _22c[0x8];
    StageItemE *m_lampItem;     // FS2 +0x234: a character's: the lamp it holds (SceneRender::update)
    StageNodeBase *m_handNode;  // FS2 +0x238: a character's: StageWeaponItem::fire's report comes from it
    u_char _23c[0x4];
    union {
        ModelPartRec *m_parts[2]; // FS2 +0x240: a character's; StagePlayer::isMode1 picks which one shows in a view camera
        struct {
            int m_doorType;     // FS2 +0x240: a door's StageDoorType
            int m_doorIndex;    // FS2 +0x244: a door's number from 1 (StageScene::findDoorOfFace)
        };
    };
    u_char _248[0x44];
    StageObject *m_doorLight;   // FS2 +0x28c: a door's: the light of its record (SceneRender::setup)
    StageObject *m_290;         // FS2 +0x290: a character's. Set: the main camera goes over the shoulder and looks at m_aimPoint; a kind 1..6 is left out of its rays (StageScene::partnerResponds reads it too)
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
    u_char _3c4[0xc];
    StageLight *m_shadowLight;  // FS2 +0x3d0: a character's: what throws its shadow (SceneRender::updateLights); null none
    float  m_shadowWeight;      // FS2 +0x3d4: how strongly
    u_char _3d8[0x28];
    StageObject *m_gear;        // FS2 +0x400: what the player carries; left out of updateLights' head ray
    u_char _404[0x4c];
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
    // The matrix of the lamp the player aims: at bn_head, turned as the
    // player and the motion are.
    void getLampMatrix(sceVu0FMATRIX out);                               // FS2 0x001e60b0

    u_char _000[0x40];
    int    motionId;            // FS2 +0x040: gMotionDefs index; outside the table none
    u_char _044[0x24];
    PlayerLookState *activeLook; // FS2 +0x068: the main camera reacts when this is look
    u_char _06c[0xb44];
    sceVu0FVECTOR position;     // FS2 +0xbb0
    u_char _bc0[0x14];
    u_char noFlash;             // FS2 +0xbd4: a shot shows no flash (StageWeaponItem::fire)
    u_char _bd5[0x3];
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
    // +0x400's +0x2b0: the two lights of what the player carries get
    // m_priority (SceneRender::update).
    void setGearLightOrder(int order);                                   // FS2 0x0028ec70
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
// layout: host -- EE: +0 the parent, +4 the children (StageList, 8), +0xc
// the object.
class StageObjectTree
{
public:
    // A new node belongs to object. Inline in the ROM (StageObject::StageObject).
    explicit StageObjectTree(StageObject *object) : m_parent(nullptr), m_object(object) {}
    // Leaves the parent, orphans the children. Inline in the ROM
    // (StageObject::~StageObject).
    ~StageObjectTree();

    // Leaves the parent's child list, if in it. Inline in the ROM
    // (StageLight::attachEffect).
    void detach()
    {
        if (m_parent == nullptr)
            return;
        for (StageList<StageObjectTree *>::Node *link = m_parent->m_children.head; link != nullptr;
             link = link->next) {
            if (link->value == this) {
                m_parent->m_children.erase(link);
                m_parent = nullptr;
                break;
            }
        }
    }
    // Hangs the node under parent (null: only detached). Inline in the ROM.
    void setParent(StageObjectTree *parent)
    {
        detach();
        if (parent != nullptr) {
            m_parent = parent;
            parent->m_children.pushBack(this);
        }
    }

    // Sets the node's and its children's m_active and m_viewActive.
    void setActive(bool on);                                             // FS2 0x0018bdc0
    // The same on the children only.
    void setChildrenActive(bool on);                                     // FS2 0x0018bd70
    // StageObject::setShown on the node's object and its children.
    void setShown(bool on);                                              // FS2 0x0018bbe0
    // The same on the children only.
    void setChildrenShown(bool on);                                      // FS2 0x0018bb90
    // setScriptFlag2 on the object and its children.
    void setScriptFlag2(bool on);                                        // FS2 0x0018b7f0
    // setScriptFlag on the object and its children.
    void setScriptFlag(bool on);                                         // FS2 0x0018b9e0

    StageObjectTree *m_parent;              // FS2 +0x0
    StageList<StageObjectTree *> m_children; // FS2 +0x4
    StageObject *m_object;                  // FS2 +0xc
};

// One shape of the collision world (SceneRecordBuckets), EE 0xe0: built by
// lights, bullets, traps, map objects, cameras. The world pairs up bodies
// whose category meets the other's mask, so the names are read from add
// 0x1f6180, not from the collide step.
// layout: host -- EE offsets in the comments.
class CollisionOwner;
struct StageCollisionFace;
struct CollisionBody
{
    // Shape 0 until the owner says; on = 1, the dirty bytes clear.
    CollisionBody(CollisionOwner *bodyOwner, int bodyBucket, u_int bodyCategory,
                  u_int bodyMask);                                       // FS2 0x001f6690
    // Marks it dirty and works out the box.
    void refresh();                                                      // FS2 0x001f6670

    u_char         on;          // FS2 +0x00: 1 from the ctor; a closed trap clears it
    CollisionOwner *owner;      // FS2 +0x04
    int            bucket;      // FS2 +0x08: SceneRecordBuckets' list
    u_int          category;    // FS2 +0x0c
    u_int          mask;        // FS2 +0x10
    int            shape;       // FS2 +0x40: CollisionShape
    u_int          rayMask;     // FS2 +0x44: a segment's, for StageScene::raycast
    u_int          rayFlags;    // FS2 +0x48: the same
    sceVu0FVECTOR  segment[2];  // FS2 +0x60: a segment's ends
    BoundingSphere sphere;      // FS2 +0x80
    sceVu0FVECTOR  capsule[2];  // FS2 +0xa0: a capsule's ends
    float          capsuleRadius; // FS2 +0xc0
};
// CollisionBody::shape (0x1f6590): the box comes from the one sphere, or
// from the list at +0xd4 (unread).
enum CollisionShape
{
    CollisionShape_Camera  = 1, // the main camera's: updateBox gives it no box
    CollisionShape_Segment = 2,
    CollisionShape_Sphere  = 3,
    CollisionShape_Spheres = 4,
    CollisionShape_Capsule = 5,
};
// A body's category, the ones read.
constexpr u_int kCollisionBulletHit = 0x80;     // a bullet's capsule
constexpr u_int kCollisionBulletRay = 0x100;    // a bullet's segment
constexpr u_int kCollisionTrap      = 0x200;
constexpr u_int kCollisionLight     = 0x2000;
// What onCollide is given: the two bodies.
// layout: host -- EE order, 4-byte pointers.
struct CollisionContact
{
    CollisionBody *self;        // FS2 +0x00: the owner's own
    CollisionBody *body;        // FS2 +0x04: the other side's
};
// The base that owns a body (EE +0 a flag, +4 the vptr; +0x200 in the lights
// and the Root objects, +0x240 in the items). Vtable 0x3115a0: six slots,
// five of them empty there. Like the object's bases its slots repeat, after
// the runner's, in the class's own table.
// layout: host
class CollisionOwner
{
public:
    // Slot 0. What it asks is unread: a light answers 0.
    virtual int  collisionHook0() = 0;
    // Slot 1: contact with something; answers 0 on a light and a bullet. On
    // a segment: the sweep's vector, the map's face, how far along it (t,
    // distance from the segment's start).
    virtual int  onCollide(CollisionContact *contact, const float *sweep,
                           StageCollisionFace *face, float t, float distance) = 0;
    // Slot 2: the body follows the owner.
    virtual void syncBody(CollisionBody *body) = 0;
    // Slot 3: after the frame's contacts. Empty on a light.
    virtual void collisionHook3(CollisionBody *body) = 0;
    // Slot 4 (+0x18): the object behind the body; the world compares these.
    virtual StageObject *getCollisionObject() = 0;
    // Slot 5 (+0x1c): an object the body must not meet (add 0x1f6180
    // compares it with the other's); null here.
    // FS2 0x00171ad0
    virtual StageObject *getCollisionParent() { return nullptr; }

    u_char m_enabled = 1;       // FS2 +0x00: 1 from the base's ctor
};
// A keyed vector over frames, EE 0x20: the lights' colour, every value of
// SceneRender. value is the frame's vector; its w the weight (StageLight::
// getWeight). game/vectortrack.cpp, cut out of the run unassigned@00299d90.
// layout: host -- EE offsets in the comments; a key is EE 0x40.
struct VectorKey
{
    // FS2 0x0029a6e0
    VectorKey(const float *from, int keyFrames)
        : ctrlOut{}, ctrlIn{}, frames(keyFrames), prev(nullptr), next(nullptr)
    {
        memcpy(value, from, sizeof(value));
    }
    // FS2 0x0029a700
    explicit VectorKey(int keyFrames)
        : value{}, ctrlOut{}, ctrlIn{}, frames(keyFrames), prev(nullptr), next(nullptr) {}
    // The key moves to time frames from the start, trading frames with its
    // neighbours; below 0 it stays.
    void setTime(int time);                                              // FS2 0x0029a5b0

    sceVu0FVECTOR value;        // FS2 +0x00
    sceVu0FVECTOR ctrlOut;      // FS2 +0x10: the Bezier point after the key
    sceVu0FVECTOR ctrlIn;       // FS2 +0x20: the Bezier point before it
    int           frames;       // FS2 +0x30: to the next key
    VectorKey    *prev;         // FS2 +0x38
    VectorKey    *next;         // FS2 +0x3c
};
// layout: host -- EE 0x20.
struct VectorTrack
{
    // FS2 0x0029a580
    VectorTrack()
        : linear(0), loop(0), done(0), curve(0), frame(0), count(0), index(0), keys(nullptr) {}
    // FS2 0x0029a500
    // callcheck: operator_delete
    ~VectorTrack()
    {
        for (VectorKey *key = keys; key != nullptr;) {
            VectorKey *next = key->next;
            delete key;
            key = next;
        }
    }
    // One frame: index and frame move, value is worked out.
    void step();                                                         // FS2 0x00299d90
    // Back to the start; with two keys or more the first key's value.
    void reset();                                                        // FS2 0x00299f40
    // Chains key last; time places it, computeFrames (1 here) makes the
    // frames from the distance when the key has none.
    void addKey(VectorKey *key, int time, bool computeFrames);           // FS2 0x0029a370
    // The index-th key, null past the last.
    VectorKey *keyAt(int index);                                         // FS2 0x0029a4c0

    sceVu0FVECTOR value;        // FS2 +0x00: w the weight (+0xc)
    u_char     linear;          // FS2 +0x10: 1 linear (SceneRender's), 0 a Bezier
    u_char     loop;            // FS2 +0x11
    u_char     done;            // FS2 +0x12
    u_char     curve;           // FS2 +0x13: 2 wraps the values as angles
    int        frame;           // FS2 +0x14
    short      count;           // FS2 +0x18: keys
    short      index;           // FS2 +0x1a: the key playing
    VectorKey *keys;            // FS2 +0x1c

private:
    void sample(VectorKey *key);                                         // FS2 0x00299f90
    void append(VectorKey *key, bool computeFrames);                     // FS2 0x0029a3b0
    void computeControls();                                              // FS2 0x0029a160
};
// Model parts named bn_<name>_<i>, of which one shows: the rest get
// ModelPartRec::drawMode 2. A light's lit and dark pieces; a character's too.
// layout: host -- EE 0x10.
struct PartSwitch
{
    // FS2 0x00187220
    PartSwitch() : model(nullptr), parts(nullptr), count(0), shown(-1) {}
    // FS2 0x001871c0
    // callcheck: operator_delete[]
    ~PartSwitch() { delete[] parts; }
    // Finds the parts; shown as in show.
    void init(ModelInstance *partsModel, const char *name, int partCount, int firstShown); // FS2 0x00187060
    // Shows part index; no such index hides them all.
    void show(int index);                                                // FS2 0x00186fe0

    ModelInstance  *model;      // FS2 +0x0
    ModelPartRec **parts;      // FS2 +0x4
    int             count;      // FS2 +0x8
    int             shown;      // FS2 +0xc
};
// The model's part called name (length: strlen), null none.
ModelPartRec *modelFindPart(ModelInstance *model, const char *name, size_t length, int arg); // FS2 0x002e4610
// A particle effect of the stage (EE 0x280, kind 0x1d, vtable 0x3149e0; ctors
// 0x190500 / 0x190450, built by StageNodeTree::spawn). A stand-in: what the
// lights call. Effects hang from each other through +0x20c, and both calls
// walk that chain.
// layout: host
class StageParticle : public StageObject
{
public:
    // On or off, down the chain.
    void setEmitting(bool on);                                           // FS2 0x0018c220
    // m_running and +0x21d, down the chain.
    void setRunning(bool on);                                            // FS2 0x0018c2d0
    void setRangeSquared(float rangeSquared);                            // FS2 0x00190040

    u_char m_running = 0;       // FS2 +0x273
};
// Every character with m_viewActive: three steps on its motion (+0x224).
void stageUpdateCharacterMotions();                                      // FS2 0x0017df60

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
    // DemoExecuter's flag: the event is not d0101, or SaveData flag
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
    StageCollision_Slope = 0x4008,  // either bit: the main camera tilts with the floor
    StageCollision_Door = 0x4000000, // doorId is a door's (StageScene::isDoorFace)
    StageCollision_CameraPass = 0x8000000, // the main camera may stand behind it
};
struct StageCollisionFace
{
    // The plane's normal in the world (a moved mesh's turned by its matrix).
    void getNormal(sceVu0FVECTOR out) const;                             // FS2 0x00236030
    // point is on the normal's side of the plane, or on it.
    bool inFront(const sceVu0FVECTOR point) const;                       // FS2 0x00235b00

    sceVu0FVECTOR plane;        // FS2 +0x00: normal, then the plane's d
    u_char _10[0x30];
    u_int  flags;               // FS2 +0x40: StageCollisionFlag
    u_short attr;               // FS2 +0x44: StageScene::getFloorAttr
    short  doorId;              // FS2 +0x46: kDoorFaceBase + what StageScene::findDoorOfFace decodes
    u_char _48[0x4];
    u_char surface;             // FS2 +0x4c: a bullet's effect and sound (getSurfaceSound)
};
static_assert(offsetof(StageCollisionFace, flags) == 0x40, "StageCollisionFace::flags");
static_assert(offsetof(StageCollisionFace, attr) == 0x44, "StageCollisionFace::attr");
static_assert(offsetof(StageCollisionFace, doorId) == 0x46, "StageCollisionFace::doorId");
static_assert(offsetof(StageCollisionFace, surface) == 0x4c, "StageCollisionFace::surface");

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
    // entry joins list; every action step reaches it.
    void add(StageSideBase *entry, int list);                            // FS2 0x001715f0
    // entry leaves whichever of the five lists holds it.
    void remove(StageSideBase *entry);                                   // FS2 0x00171580
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
    // body joins its bucket, and is paired up with what its category meets.
    void add(CollisionBody *body);                                       // FS2 0x001f6180
    void remove(CollisionBody *body);                                    // FS2 0x001f60a0
    void collide();                                                      // FS2 0x001f5250
};

// +0x1c: owns one 0x40-byte object (0x192080, built with this). layout: host (EE 4).
struct StageNodeTree
{
    StageNodeTree();                                                     // FS2 0x00190c20
    ~StageNodeTree();                                                    // FS2 0x00190bc0: deleting
    // A new effect by its definition's name, under parent, at pos / rot
    // (either null: the definition's); null no such name. arg goes to +0x25c.
    StageParticle *spawn(const char *name, StageNodeBase *parent, const float *pos,
                         const float *rot, int arg, int arg2);             // FS2 0x00190890
    // The same with no parent, place or rotation.
    StageParticle *spawnNamed(const char *name, int arg, int arg2);      // FS2 0x001908a0
    // effect stops; null is fine.
    void remove(StageParticle *effect);                                  // FS2 0x001908c0
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
    // The entry of the first table whose name is name, null none (also on a
    // null script: the ROM tests this).
    const void *findEntry(const char *name);                             // FS2 0x001d3d20
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

// A model file (the model keyword) and what ModelFile::createInstance reads
// from it. EE layout: the file itself; relocate (rena/rena_model.cpp) moves
// its pointers in place, so they are all S2dRef.
enum ModelMaterialKind
{
    ModelMaterial_Textured = 0,
    ModelMaterial_Kind1    = 1, // drawSysFindKind1Surface finds it; what it is, is unread
};
enum ModelMaterialFlag
{
    ModelMaterialFlag_Blended = 0x01,   // drawn after the rest, far to near (rena_draw.cpp)
    ModelMaterialFlag_Wave    = 0x08,   // has a ModelTexAnim_Wave (set by ModelSurface)
    ModelMaterialFlag_Foliage = 0x20,   // the texture is a plant's (drawSysAddModel)
};
struct ModelMaterialExt
{
    short        _0;
    short        texture;       // FS2 +0x2: a third texture, into ModelFile::textureNames, or negative
    S2dRef<void> data;          // FS2 +0x4
};
static_assert(sizeof(ModelMaterialExt) == 8, "ModelMaterialExt: EE 8");
// A textured material's animation.
enum ModelTexAnimKind
{
    ModelTexAnim_None   = 0,
    ModelTexAnim_Scroll = 1,    // ModelSurface::scrollU / V
    ModelTexAnim_Wave   = 2,    // ModelSurface::waveValue, a sine
};
struct ModelTexAnim
{
    int kind;                   // FS2 +0x0: ModelTexAnimKind
    union
    {
        struct
        {
            int speedU;         // FS2 +0x4
            int speedV;         // FS2 +0x8
        } scroll;
        struct
        {
            float period;       // FS2 +0x4: in 300ths of the ticks' unit
            float phase;        // FS2 +0x8: the start, 0..period
            float amplitude;    // FS2 +0xc: each swing draws 0.5..1 of it
        } wave;
    };
};
static_assert(sizeof(ModelTexAnim) == 0x10, "ModelTexAnim: 0x10 as read");
struct ModelMaterial
{
    u_char             kind;    // FS2 +0x00: ModelMaterialKind
    u_char             flags;   // FS2 +0x01: ModelMaterialFlag
    short              uvSet;   // FS2 +0x02: which of a skinned prim's UV lists; on ModelMaterial_Kind1 the base material
    S2dRef<const char> name;    // FS2 +0x04
    S2dRef<ModelTexAnim> texAnim; // FS2 +0x08: may be null (renaStepTexAnim)
    short              texture; // FS2 +0x0c: into ModelFile::textureNames, or negative
    short              texture2; // FS2 +0x0e: the same
    float              _10;     // FS2 +0x10: ModelSurface::levelScale
    float              _14;     // FS2 +0x14: picks ModelSurface::level
    S2dRef<ModelMaterialExt> ext; // FS2 +0x18: not on ModelMaterial_Kind1
};
static_assert(offsetof(ModelMaterial, name) == 4, "ModelMaterial::name");
static_assert(offsetof(ModelMaterial, texture) == 0xc, "ModelMaterial::texture");

// A DMA tag of a mesh's packet: its address is a pointer of the file.
struct ModelTag
{
    int          _0;
    S2dRef<void> next;          // FS2 +0x4
    int          _8[2];
};
static_assert(sizeof(ModelTag) == 0x10, "ModelTag: EE 0x10");
// One packet of a ModelPrim: 5 tags, or 7 when ModelPrim::isLong; 0x10
// more unless skinned. Its last two tags' addresses list pointers when
// skinned.
struct ModelBatch
{
    enum
    {
        kShortTags = 5, kLongTags = 7,
        kShortSize = 0x50, kLongSize = 0x70, kPadSize = 0x10,
    };
    ModelTag tags[kLongTags];
};
static_assert(sizeof(ModelBatch) == ModelBatch::kLongSize, "ModelBatch: EE 0x70 at most");
// A mesh material's run of packets (0x10 bytes).
struct ModelPrim
{
    // What a batch's first tag points at (one VU1 quadword). layout: host
    // -- read in place, its size asserted below the class.
    struct Header
    {
        enum { Strip = 0x1, Weighted = 0x2 };
        u_short vertices;       // FS2 +0x0
        u_short end;            // FS2 +0x2: 0x8000 on the GIF tag's EOP
        int     vertexCount;    // FS2 +0x4: the same number
        int     flags;          // FS2 +0x8: Strip, Weighted
        int     weightsLess1;   // FS2 +0xc: bones per vertex - 1
    };
    S2dRef<void>       _0;      // FS2 +0x0
    S2dRef<ModelBatch> batches; // FS2 +0x4: batchCount
    u_short            _8;      // FS2 +0x8
    u_short            batchCount; // FS2 +0xa
    u_char             isLong;  // FS2 +0xc
    u_char             listACount; // FS2 +0xd
    u_char             listBCount; // FS2 +0xe
    u_char             skinned; // FS2 +0xf
};
static_assert(sizeof(ModelPrim) == 0x10, "ModelPrim: EE 0x10");
static_assert(sizeof(ModelPrim::Header) == 0x10, "ModelPrim::Header: one quadword");
// One material of a mesh part (8 bytes).
struct ModelMeshMaterial
{
    u_short           primCount; // FS2 +0x0
    short             material; // FS2 +0x2: into the file's materials, or negative
    S2dRef<ModelPrim> prims;    // FS2 +0x4
};
static_assert(sizeof(ModelMeshMaterial) == 8, "ModelMeshMaterial: EE 8");
// A mesh's part (0x30 bytes).
struct ModelMeshPart
{
    u_char                    _00[0x20];
    int                       materialCount; // FS2 +0x20
    int                       _24;
    S2dRef<ModelMeshMaterial> materials;     // FS2 +0x28
    S2dRef<S2dRef<void> >     lists;         // FS2 +0x2c: ModelMeshParts::boneCount
};
static_assert(sizeof(ModelMeshPart) == 0x30, "ModelMeshPart: EE 0x30");
struct ModelMeshParts
{
    u_short               count; // FS2 +0x0
    u_short               boneCount; // FS2 +0x2: of ModelMesh::boneMap
    S2dRef<ModelMeshPart> parts; // FS2 +0x4
    S2dRef<ModelMaterial> material; // FS2 +0x8: its name tags the mesh for the VCM (vcmtable.cpp)
};
static_assert(offsetof(ModelMeshParts, material) == 8, "ModelMeshParts::material");
// A node's mesh (0x18 bytes).
// ModelMesh::vuFlags. The other bits are unread.
enum ModelMeshFlag
{
    ModelMeshFlag_Unlit = 0x4,  // the VU1 program copies the vertex colours (0x29e) instead of lighting them (0x2a9, 0x2bb)
};
struct ModelMesh
{
    S2dRef<const char>     name;  // FS2 +0x00
    S2dRef<short>          boneMap; // FS2 +0x04: into the node's bones; -1 from the first node's
    S2dRef<ModelMeshParts> parts; // FS2 +0x08: may be null
    u_short                firstChild; // FS2 +0x0c: a mesh index
    short                  bone;  // FS2 +0x0e: the node's bone a rigid mesh hangs on, negative the root
    short                  vuFlags; // FS2 +0x10: goes to VU1 with each part (VU address 15 z): ModelMeshFlag
    u_short                childCount; // FS2 +0x12
    u_char                 _14[4];
};
static_assert(sizeof(ModelMesh) == 0x18, "ModelMesh: EE 0x18");
static_assert(offsetof(ModelMesh, childCount) == 0x12, "ModelMesh::childCount");

struct ModelFile;
// A node's bone (0x10 bytes).
struct ModelNodeBone
{
    S2dRef<const sceVu0FVECTOR> matrix; // FS2 +0x0: 4 rows, the bind pose in the file's space
    S2dRef<const char> name;    // FS2 +0x4
    int                parent;  // FS2 +0x8: bone index, negative at the top
    u_char             _c[4];
};
static_assert(offsetof(ModelNodeBone, parent) == 8, "ModelNodeBone::parent");
static_assert(sizeof(ModelNodeBone) == 0x10, "ModelNodeBone: EE 0x10");
// A node's record in a model file (0x30 bytes).
struct ModelFileNode
{
    // How many surfaces have a running texture animation.
    int countTexAnims() const;                                           // FS2 0x002e5190

    S2dRef<const sceVu0FVECTOR> matrix; // FS2 +0x00: 4 rows, the node's
    S2dRef<const char> name;    // FS2 +0x04: a map model's: the object it becomes
    u_char             _08[8];
    S2dRef<ModelFile>  file;    // FS2 +0x10
    S2dRef<ModelMesh>  meshes;  // FS2 +0x14: meshCount
    S2dRef<ModelNodeBone> bones; // FS2 +0x18: boneCount
    S2dRef<u_short>    textureSlots; // FS2 +0x1c: textureSlotCount of ModelFile::textureNames indices
    S2dRef<u_short>    surfaceMaterials; // FS2 +0x20: surfaceCount of ModelFile::materials indices
    u_short            boneCount;    // FS2 +0x24
    u_short            surfaceCount; // FS2 +0x26
    u_short            textureSlotCount; // FS2 +0x28
    u_short            meshCount;    // FS2 +0x2a
    u_char             _2c[4];
};
static_assert(sizeof(ModelFileNode) == 0x30, "ModelFileNode: EE 0x30");
static_assert(offsetof(ModelFileNode, name) == 4, "ModelFileNode::name");
static_assert(offsetof(ModelFileNode, surfaceCount) == 0x26, "ModelFileNode::surfaceCount");
static_assert(offsetof(ModelFileNode, meshCount) == 0x2a, "ModelFileNode::meshCount");

// A material as a model instance draws it (EE 0x50). layout: host.
struct ModelSurface
{
    ModelSurface(ModelMaterial *material, const ModelFileNode *node);    // FS2 0x002e3e10
    // Two frames of the texture and the blend between them (a zero second
    // frame: the first, faded).
    void setFrames(float blend, int frame0, int frame1);                 // FS2 0x002e3d90

    int            _00[2];      // FS2 +0x00: zero (the draw run reads them: unread)
    float          waveValue;   // FS2 +0x08: amplitude * sin(phase)
    int            _0c;         // FS2 +0x0c: zero
    float          wavePhase;   // FS2 +0x10: -pi..pi
    float          waveAmplitude; // FS2 +0x14
    ModelMaterial *material;    // FS2 +0x18
    short          textureSlot; // FS2 +0x1c: of ModelFileNode::textureSlots, -1 none (material->texture)
    short          extTextureSlot; // FS2 +0x1e: the same, material->ext->texture
    float          blend;       // FS2 +0x20: setFrames
    int            blendFix;    // FS2 +0x24: setFrames, 0..0x80; on a wave the number after WaveComposit
    int            frame0;      // FS2 +0x28: setFrames, 0 on a wave
    int            frame1;      // FS2 +0x2c: setFrames, 1 on a wave
    int            scrollU;     // FS2 +0x30: 0..0xfff
    int            scrollV;     // FS2 +0x34
    short          texture2Slot; // FS2 +0x38: the same, material->texture2
    int            level;       // FS2 +0x3c: 0..4, from material->_14
    float          levelScale;  // FS2 +0x40: material->_10
};

// Bits of ModelInstance::drawMode: the lights set on it.
constexpr u_short kModelLightMask = 0x7; // one bit per light, ModelInstance::setLight
constexpr u_short kModelAmbient   = 0x80; // ModelInstance::setAmbient

// What ModelFile::createInstance makes (vtable 0x31a7f0, 0x100 bytes,
// 0x80-aligned): the model StageModel::data and the draw system's lists
// hold. Everything but the object itself lives in one heap block. layout: host.
struct ModelInstance
{
    // The record whose children range holds rec's index (the group rec is
    // in), null at the top.
    ModelPartRec *findGroup(const ModelPartRec *rec) const;              // FS2 0x002e4100
    ~ModelInstance();                                                    // FS2 0x002e46d0: slot 0 of vtable 0x31a7f0, deleting
    // The name of texture slot i (ModelFileNode::textureSlots).
    const char *getTextureName(int slot) const;                          // FS2 0x002e47d0
    void setTexture(int slot, Tim2Texture *texture);                     // FS2 0x002e4550
    void   setDistant(u_char value);                                     // FS2 0x002e40e0: a slot of vtable 0x31a7f0
    u_char getDistant() const;                                           // FS2 0x002e40f0: the next slot
    void setAmbient(const sceVu0FVECTOR color);                          // FS2 0x002e44e0
    // Light index (0..2): its direction, a column of lightDirs, and colour.
    void setLight(u_int index, const sceVu0FVECTOR direction, const sceVu0FVECTOR color); // FS2 0x002e4500
    // Fills the instance from a node of a relocated file.
    void init(ModelFileNode *fileNode);                                  // FS2 0x002e4800

    u_char         distant = 0;   // FS2 +0x04: set to 1 when farther than the draw distance (drawSysBuildShadows)
    u_char         drawGroup = 0; // FS2 +0x05: DrawSysGroup
    u_char         _06 = 0;       // FS2 +0x06: 1 from StageModel::init (unread)
    u_char         shown = 0;     // FS2 +0x07: the model's own shown flag: while there is a model it replaces StageObject::m_shown
    u_short        drawMode = 0;  // FS2 +0x08: kModelLightMask | kModelAmbient: which of lightColors are set
    u_short        flags = 0;     // FS2 +0x0a: ModelFlag
    ModelFileNode *node = nullptr; // FS2 +0x0c
    ModelBone     *rootBone = nullptr; // FS2 +0x10: its matrix is the owner's (StageModel::init)
    ModelBone     *bones = nullptr; // FS2 +0x14: boneCount of them; [2] carries the shadow
    u_int          boneCount = 0; // FS2 +0x18
    ModelPartRec  *partRecs = nullptr; // FS2 +0x1c: one per node->meshes
    sceVu0FMATRIX *inverseBinds = nullptr; // FS2 +0x20: per bone, the inverses of boneBinds (StageModel::bindBones)
    const sceVu0FVECTOR **bindMatrices = nullptr; // FS2 +0x24: per bone, the file's matrix
    const sceVu0FVECTOR *rootMatrix = nullptr; // FS2 +0x28: the inverse of the node's matrix
    sceVu0FMATRIX *boneBinds = nullptr; // FS2 +0x2c: per bone, its matrix in the node's space
    int            _30 = 0;       // FS2 +0x30: zero (unread)
    float          ghostParam0 = 0.0f; // FS2 +0x34: 191 on ext_ghost (unread)
    float          ghostParam1 = 0.0f; // FS2 +0x38: -52 on ext_ghost (unread)
    ModelSurface  *surfaces = nullptr; // FS2 +0x3c: node->surfaceCount
    ModelSurface **surfaceOfMaterial = nullptr; // FS2 +0x40: by the file's material index, null if unused
    ModelSurface **texAnims = nullptr; // FS2 +0x44: texAnimCount records of renaStepTexAnim
    int            texAnimCount = 0;   // FS2 +0x48
    RenaSpotLight *spot = nullptr; // FS2 +0x4c: the scene's (SceneRender::setup); no host draw reads it yet
    Tim2Texture  **textures = nullptr; // FS2 +0x50: node->textureSlotCount
    char          *block = nullptr; // FS2 +0x54: the one block all those live in
    u_int          blockUsed = 0; // FS2 +0x58
    u_int          blockSize = 0; // FS2 +0x5c
    int            _60 = 0;       // FS2 +0x60: zero (unread)
    sceVu0FMATRIX  lightColors = {}; // FS2 +0x80: [0] ambient, [1..3] the lights; zero for the boss shadow's draw
    sceVu0FMATRIX  lightDirs = {};   // FS2 +0xc0: one column per light

private:
    // The next count T of the block, null for none. The ROM's inline.
    template <class T> T *take(u_int count);
    // recs[index] and its subtree; next: what follows them.
    void buildPartRecs(int index, u_short next);                         // FS2 0x002e4f50
};

// A texture named .._s: a copy of its palette at +0x3c, the colours times
// their alpha, alpha opaque (ModelInstance::setTexture).
void modelTexturePremultiplyPalette(Tim2Texture *texture);                     // FS2 0x002e1700

struct ModelFile
{
    bool isValid();                                                      // FS2 0x002e2cf0: the magic at gp-0x7b48
    void relocate();                                                     // FS2 0x002e2d20
    // Moves every pointer of the file from the base oldBase to the file.
    void relocateFrom(int oldBase);                                      // FS2 0x002e2d80
    int  getNodeCount();                                                 // FS2 0x002e5200
    // A new instance of node index, or of the node called name when name
    // is not null.
    ModelInstance *createInstance(u_int index, const char *name);        // FS2 0x002e5210
    // The two lookups createInstance inlines; each says why when it answers
    // null. The file is relocated first.
    ModelFileNode *findNode(u_int index);
    ModelFileNode *findNode(const char *name);

    u_char  _00[0x18];
    u_short userCount;          // FS2 +0x18: instances of it
    u_short nodeCount;          // FS2 +0x1a
    u_short materialCount;      // FS2 +0x1c
    u_short textureCount;       // FS2 +0x1e
    u_char  _20[4];
    int     meshPartCount;      // FS2 +0x24
    S2dRef<ModelMeshParts> meshParts; // FS2 +0x28: meshPartCount
    S2dRef<void> _2c;           // FS2 +0x2c
    S2dRef<ModelFile> base;     // FS2 +0x30: on the disc an address, then the file itself
    S2dRef<ModelFileNode> nodes; // FS2 +0x34
    S2dRef<S2dRef<ModelMaterial> > materials; // FS2 +0x38: materialCount
    S2dRef<S2dRef<const char> > textureNames; // FS2 +0x3c: textureCount, each a .tm2 of the .tpk without its extension
    S2dRef<void> _40;           // FS2 +0x40
    S2dRef<void> _44;           // FS2 +0x44
};
static_assert(offsetof(ModelFile, materialCount) == 0x1c, "ModelFile::materialCount");
static_assert(offsetof(ModelFile, textureCount) == 0x1e, "ModelFile::textureCount");
static_assert(offsetof(ModelFile, nodes) == 0x34, "ModelFile::nodes");
static_assert(offsetof(ModelFile, textureNames) == 0x3c, "ModelFile::textureNames");
static_assert(offsetof(ModelFile, meshPartCount) == 0x24, "ModelFile::meshPartCount");
static_assert(offsetof(ModelFile, base) == 0x30, "ModelFile::base");

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
// An lpt record (data, _04 of them): the light named name gets the effect.
struct LptRecord
{
    char          name[0x10];   // FS2 +0x00
    char          effect[0x10]; // FS2 +0x10
    sceVu0FVECTOR pos;          // FS2 +0x20
    sceVu0FVECTOR rot;          // FS2 +0x30
};
static_assert(sizeof(LptRecord) == 0x40, "LptRecord: EE 0x40");

// A door's record (0x38 bytes, kDoorRecords of them; bss 0x537d80, read
// from the pack's door sheet by PackTables::parse), found by its map
// object's name. The ROM's five setters 0x172370..0x1723b0 write +0x10..+0x1e.
constexpr int kDoorRecords = 0x87;
struct DoorRecord
{
    char   name[0x10];          // FS2 +0x00: Name
    int    keyKind;             // FS2 +0x10: 鍵属性
    int    startState;          // FS2 +0x14: 初期状態
    int    windowState;         // FS2 +0x18: 窓状態
    short  hp;                  // FS2 +0x1c: ドアHP
    short  seIndex;             // FS2 +0x1e: the number of SEラベル DOOR_%02d
    char   light[0x10];         // FS2 +0x20: light
    u_char hasObject;           // FS2 +0x30: object
    int    value;               // FS2 +0x34: particle; SubSceneDoorObject::m_recordValue
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
    // Slot 123 in the ROM. This family has no host base yet (StageObject).
    int getKind();                                                       // FS2 0x001a45d0

    int m_recordValue;          // FS2 +0x280: DoorRecord::value
};

// What a stage's map file becomes (StageFiles::applyMapTarget). The ROM
// news each with operator new aligned 0x40; the sizes are in the comments.
// All names but ENV_ / YAM_ / P_SPW are the port's. The classes' virtuals
// are read only as far as these calls.


// Named places with their script (vtables 0x3151c0 / 0x3151e4 / 0x3151fc;
// the ctor inline in the ROM; dtor 0x1a4800). layout: host -- EE 0x200.
class StageMarker : public StageObject
{
public:
    // The object list's group 6, then its script by name.
    void addToScene(const char *name);                                   // FS2 0x0017ef10: StageObjectList::add6
    int getKind() override;                                              // FS2 0x001a4880
    // The four slots of the base at +0x18 (vptr +0x1c): empty here; what
    // they are is unread (0x2699f0 of SubSceneModelObject draws something).
    void sideHook0();                                                    // FS2 0x001a48c0
    void sideHook1();                                                    // FS2 0x001a48b0
    void sideHook2();                                                    // FS2 0x001a48a0
    void sideHook3();                                                    // FS2 0x001a4890
};

// vtable 0x311860; used for the map's cameras and by setupCameras.
// layout: host -- EE 0x300.
class StageCamera : public StageObject
{
public:
    // FS2 0x001775b0
    int getKind() override { return StageObject_Camera; }
    // scrZ is the view-screen distance of sceVu0ViewScreenMatrix; farZ
    // only stored here.
    StageCamera(float scrZ, float width, float height, float farZ);      // FS2 0x00177d60
    void addToScene(const char *name);                                   // FS2 0x00177d20: StageObjectList::add0
    // Turns the transform's rotation to look at target.
    void lookAt(const float *target);                                    // FS2 0x00177900
    // The same from eye: only the rotation changes.
    void aimFrom(const float *target, const float *eye);                 // FS2 0x00177860
    // A new view-screen distance (zooms); the screen matrix follows.
    void setScrZ(float scrZ);                                            // FS2 0x00177ac0
    // The far range, what SceneRender::update gives every camera; the same.
    void setRange(float range);                                          // FS2 0x001779a0
    void onViewTop() override;                                           // FS2 0x00177d00
    // The view matrix (+0x240) and view-screen matrix (+0x280) from the
    // object's matrix; each frame for the active camera.
    void calcViewMatrix();                                               // FS2 0x00177810

    sceVu0FMATRIX m_screen;     // FS2 +0x200
    sceVu0FMATRIX m_view;       // FS2 +0x240
    sceVu0FMATRIX m_viewScreen; // FS2 +0x280
    float m_scrZ;               // FS2 +0x2c0
    float m_screenWidth;        // FS2 +0x2c4
    float m_screenHeight;       // FS2 +0x2c8
};
inline const void *StageObject::cameraScreen() const { return static_cast<const StageCamera *>(this)->m_screen; }
inline const void *StageObject::cameraView() const { return static_cast<const StageCamera *>(this)->m_view; }
// The stage's "camera" (kind 9; vtable 0x311fa0). layout: host -- EE 0x3c0.
// The kind 8 class between it and StageCamera (vtable 0x311d30, ctor inline)
// adds only m_linked, which stays a stand-in's field. The camjack derives
// from that class too.
class StageMainCamera : public StageCamera, public CollisionOwner
{
public:
    // FS2 0x001799d0
    int getKind() override { return StageObject_Kind9; }
    StageMainCamera();                                                   // FS2 0x00179700
    void onViewTop() override;                                           // FS2 0x00179490
    void cameraStep() override;                                          // FS2 0x00178160
    void onLinkChanged() override;                                       // FS2 0x00179680

    // FS2 0x00178130
    int  collisionHook0() override { return 0; }
    // FS2 0x00178120
    int  onCollide(CollisionContact *, const float *, StageCollisionFace *, float, float) override { return 0; }
    // FS2 0x00178150
    void syncBody(CollisionBody *) override {}
    // FS2 0x00178140
    void collisionHook3(CollisionBody *) override {}
    // FS2 0x001799e0
    StageObject *getCollisionObject() override { return this; }

    sceVu0FVECTOR m_focusOffset; // FS2 +0x350: the point looked at, in the linked character's space
    sceVu0FVECTOR m_eyeOffset;  // FS2 +0x360: the eye from that point (w 0: only turned)
    sceVu0FVECTOR m_lookTarget; // FS2 +0x370: the point looked at, in the world, eased
    sceVu0FVECTOR m_turn;       // FS2 +0x380: radians per step, eased (the stick's)
    int    m_shoulderSlot;      // FS2 +0x390: the kShoulderEyes entry that worked last
    float  m_zoom;              // FS2 +0x394: added to the view-screen distance, 0..160
    int    m_blockedFrames;     // FS2 +0x398: how long the wanted place has been out of sight
    float  m_headHeight;        // FS2 +0x39c: bn_head above the character; nothing reads it here
    u_char m_nearWall;          // FS2 +0x3a0
    u_char m_snap;              // FS2 +0x3a1: the next step jumps instead of easing
};
// PC: aims the debug camera (game/stagecamera.cpp).
void stageCameraDebugLookAt(const float *eye, const float *target, const float *up);
// "camview", "camview_zoom", "camview_snipe" (kind 10; vtable 0x3185b0; run
// unassigned@00299d90). layout: host -- EE 0x440.
class ViewCamera : public StageCamera
{
public:
    // FS2 0x0029a830
    int getKind() override { return StageObject_Kind10; }
    ViewCamera();                                                        // FS2 0x0029aae0
};
// A sub-scene's "camera_demo" (kind 13; vtable 0x316ed0; run
// unassigned@002681e0). layout: host -- EE 0x340.
class DemoCamera : public StageCamera
{
public:
    // FS2 0x0026c910
    int getKind() override { return StageObject_Kind13; }
    DemoCamera();                                                        // FS2 0x0026ce20
    // The fades on list 9 after the 3D frame.
    void drawOverlay();                                                  // FS2 0x0026c920
};
// The bullet and the items: game/stageitem.h.

// The lights: game/stagelight.h.

// The rest of the light and box sections: scripted volumes (base vtable
// 0x3127f0). "Volume" is a guess: their script hears them.
// layout: host -- EE 0x240.
class StageVolume : public StageObject
{
public:
    // FS2 0x0017ef90
    int getKind() override { return StageObject_Volume; }
    // The object list's group 7, then its script by name.
    void addToScene(const char *name);                                   // FS2 0x0017ecb0: StageObjectList::add7
};
// layout: host
class SphereVolume : public StageVolume
{
public:
    // FS2 0x0017f040
    int getKind() override { return StageObject_SphereVolume; }
    SphereVolume();                                                      // FS2 0x0017ec30
    void setRadius(float radius);                                        // FS2 0x0017ec20
};
// layout: host
class BoxVolume : public StageVolume
{
public:
    // FS2 0x0017e900
    int getKind() override { return StageObject_BoxVolume; }
    BoxVolume();                                                         // FS2 0x0017ea90
    void setSize(float width, float height, float depth);                // FS2 0x0017ea50
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
    // FS2 0x00179a80
    int getKind() override { return StageObject_Character; }
    StageCharacter();                                                    // FS2 0x0017de30
    // Model, textures, the model's two motion lists and mdl.
    void setup(const char *name, ModelInstance *instance, StringMap<Tim2Texture *> *textures,
               StageList<void *> *motions, StageList<void *> *motions2, void *mdl); // FS2 0x0017c700
    // A named event of the character in the scene's +0x78, while +0x30 is
    // set; what takes it is unread (GUNSHOOT).
    void playerEvent(const char *name);                                  // FS2 0x0020e6a0

    sceVu0FVECTOR m_aimPoint = {}; // FS2 +0x2a0: where its bullets steer
    float  m_aimLock = 0.0f;    // FS2 +0x5fc: not 0: they fly straight
    u_char m_bulletFlying = 0;  // FS2 +0x688: set each frame by a bullet of its own
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
    // Slot 123 in the ROM; see StageMarker::getKind.
    int getKind();                                                       // FS2 0x001a4950
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
    // A new set when owner is I_HATUEN, else null.
    static SceneBufferSet *createFor(StageObject *owner, StageNodeTree *tree); // FS2 0x002ce620
    // object joins the list of up to five, if it passes two tests.
    void addObject(StageObject *object);                                 // FS2 0x002ce360
    // The first time, the characters fill the eight blocks.
    void attachCharacters(StageObjectList *objects);                     // FS2 0x002ce3e0
    // Each block whose owner became or stopped being linked: shown or
    // hidden, render->m_snap set.
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
    // What owner builds; null for one that has none (unread).
    static SceneBlock98 *createFor(StageObject *owner, StageNodeTree *tree); // FS2 0x002ce9d0
    // object's frame against the block (unread: a record's type, a
    // distance to the player).
    void updateFor(StageObject *object, StagePlayer *player);            // FS2 0x002ce820
    // A change of on: the node shown or hidden, render->m_snap set.
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
    // A noise of an object into the back list: 0x20 entries of {source,
    // loudness, extra}, more are lost.
    void addNoise(float loudness, StageObject *object, void *extra);     // FS2 0x002430a0
    // The same from a playing source (entry +0xc the loudness); a source
    // with entry +0x1d also takes a free one of the sixteen entries.
    void addSourceNoise(StageObject *object, StageSoundSource *source);  // FS2 0x00242fc0
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

// 44 groups at 0x599830 / 0x599860 reset (a byte zero, 32 words one each):
// a stage's ctor. What the groups are is unread.
void resetStageGroupTable();                                             // FS2 0x002cf510

// What runs a sub-scene's script (StageScene +0x8, from enterSubScene). The
// ROM names the class itself: slot 7 answers "DemoExecuter" (the stand-in
// was SubSceneObject). It overrides nothing else written so far.
// layout: host (unread).
class DemoExecuter : public ScriptRunner
{
public:
    // vtable 0x314d30. flag: +0xb9; "skippable" is a guess.
    explicit DemoExecuter(bool skippable);                               // FS2 0x0026c8c0
    ~DemoExecuter() override;                                            // FS2 0x00195db0: deleting
    const char *getName() override;                                      // FS2 0x0019cf40
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


// The memory-card menu's save manager (sbss 0x31ae94, gp-0x79dc; owned by
// PrgMcCard, game/prgmccard.h, null while there is none): the S2D's
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

// The item definitions (bss 0x574c10, 160 x 0x20; the pack's item sheet,
// PackTables::parse). The names not from a column are guesses from
// findSprite and the inventory scans.
constexpr int kItemDefCount = 160;
struct ItemDef
{
    char   name[0x10];          // FS2 +0x00: the TimM sprite names use it
    int    kind;                // FS2 +0x10: ItemKind (種別, kItemKinds in game/packtables.cpp)
    int    effect;              // FS2 +0x14: アイテム効果
    u_char startCount;          // FS2 +0x18: 入手数: what a placed item holds (StageItem::setupItem)
    u_char stackKind;           // FS2 +0x19: 所持上限 (the carry limit): > 1 shows the count (UiItemSlot::showsCount); StageItem's most
    signed char pickSound;      // FS2 +0x1a: kPickSounds in game/stageitem.cpp; -1 none
    u_char isBlade;             // FS2 +0x1b: 1 cuts (the weapons' hit sounds)
    signed char landSound;      // FS2 +0x1c: kLandSounds in game/stageitem.cpp; -1 none
    u_char _1d;
    short  maxCount;            // FS2 +0x1e: the sheet's No (UiItemSlot::max; the name was a guess, 所持上限 is +0x19)

    // The two sound names of the pack's table; false: no such sound.
    bool setLandSound(const char *soundName);                            // FS2 0x0017fd40
    bool setPickSound(const char *soundName);                            // FS2 0x0017fdf0
    inline bool isFirearm() const;
    inline bool isPistol() const;
    inline bool isRifle() const;
};
static_assert(sizeof(ItemDef) == 0x20, "ItemDef: EE 0x20");
extern ItemDef gItemDefs[kItemDefCount];  /* bss 0x574c10 */

// The character sheet per StageKind (bss 0x539b10: 3 x 0x140 x 0xfc, the
// pack's items 0..2, PackTables::parse), indexed by an object's m_itemDef.
// The field names say what the sheet's column says.
constexpr int kStageObjRecordSets  = 3;
constexpr int kStageObjRecordCount = 0x140;
constexpr int kStageObjRecordMuffles = 0xf;
struct StageObjRecord
{
    char   name[0x10];          // FS2 +0x00: ラベル: a map point's (StageFiles::placeCharacter)
    int    kind;                // FS2 +0x10: ctype (StageSound: 0xf muffles a Kind11 listener)
    int    seType;              // FS2 +0x14: SEtype
    char   items[2][0x10];      // FS2 +0x18: 武器ラベル, 懐中電灯ラベル: items placed with the character; empty none
    u_char lightWeapon : 2;     // FS2 +0x38: 軽
    u_char heavyWeapon : 2;     // FS2 +0x38: 重
    u_char shortGun    : 2;     // FS2 +0x38: 短
    u_char rifle       : 2;     // FS2 +0x38: ラ
    u_int  sides;               // FS2 +0x3c: StageObjSide bits
    int    startMotion;        // FS2 +0x40: 初期M
    int    aiType;             // FS2 +0x44: AIﾀｲﾌﾟ
    int    aiLevel;            // FS2 +0x48: AILV
    int    sight;              // FS2 +0x4c: 視界
    int    hearing;            // FS2 +0x50: 音感
    int    health;             // FS2 +0x54: 体力
    int    endurance;          // FS2 +0x58: 耐久力
    int    leftArm;            // FS2 +0x5c: 左腕
    int    rightArm;           // FS2 +0x60: 右腕
    int    rightLeg;           // FS2 +0x64: 右足
    int    leftLeg;            // FS2 +0x68: 左足
    int    recovery;           // FS2 +0x6c: 回復
    int    lightResist;        // FS2 +0x70: 光耐性
    int    lightDamage;        // FS2 +0x74: 光ﾀﾞﾒｰｼﾞ
    int    lightFlinch;        // FS2 +0x78: 光怯み
    int    unarmedAttack;      // FS2 +0x7c: 素手攻撃
    int    unarmedMsp;         // FS2 +0x80: 素手MSP
    int    unarmedMspDrop;     // FS2 +0x84: 素手MSP減少値
    int    weaponMspScale;     // FS2 +0x88: 武器MSP修正値
    int    meleeAttackScale;   // FS2 +0x8c: 打撃攻撃力修正値
    int    shotAttackScale;    // FS2 +0x90: 射撃攻撃力修正値
    int    bite;               // FS2 +0x94: かみつき
    int    attackCheck;        // FS2 +0x98: 攻撃判定
    int    walkSpeed;          // FS2 +0x9c: 移動速度
    int    runSpeed;           // FS2 +0xa0: 走り速度
    int    turnSpeed;          // FS2 +0xa4: 旋回速度
    int    flashlight;         // FS2 +0xa8: 懐中電灯
    int    reviveTime;         // FS2 +0xac: 復活時間
    int    flinchResist;       // FS2 +0xb0: ひるみ耐性値
    int    decoyResist;        // FS2 +0xb4: 陽動耐性
    int    controlResist;      // FS2 +0xb8: 操り耐性
    int    trapResist;         // FS2 +0xbc: 罠耐性
    int    shot;               // FS2 +0xc0: 射撃
    int    shotInterval;       // FS2 +0xc4: 射撃間隔
    int    attackInterval;     // FS2 +0xc8: 攻撃間隔
    int    biteInterval;       // FS2 +0xcc: 噛みつき間隔
    int    reload;             // FS2 +0xd0: リロード
    int    stepLevel;          // FS2 +0xd4: 段差Lv
    int    ladder;             // FS2 +0xd8: ハシゴ
    int    jumpLevel;          // FS2 +0xdc: ｼﾞｬﾝﾌﾟLv
    int    stepDown;           // FS2 +0xe0: 段差下り
    int    bloodSpray;         // FS2 +0xe4: 血しぶき
    int    unarmedCombo;       // FS2 +0xe8: 素手コンボ
    int    weaponCombo;        // FS2 +0xec: 武器コンボ
    int    drivingSkill;       // FS2 +0xf0: 運転スキル
    int    snipeLerp;          // FS2 +0xf4: 狙撃補間率
    int    snipeShake;         // FS2 +0xf8: 狙撃ブレ率
};
static_assert(sizeof(StageObjRecord) == 0xfc, "StageObjRecord: EE 0xfc");
extern StageObjRecord gStageObjRecords[kStageObjRecordSets][kStageObjRecordCount]; /* bss 0x539b10 */
// StageObjRecord::sides (columns PC, NP, 屍 and 闇).
enum StageObjSide
{
    StageObjSide_Player = 0x1,  // PC
    StageObjSide_Npc    = 0x2,  // NP
    StageObjSide_Corpse = 0x4,  // 屍 (shibito)
    StageObjSide_Dark   = 0x8,  // 闇 (yamibito)
};

// ItemDef::kind: 2..6 are firearms (they show ammo), 7 and up items the
// inventory lists; below 7 the weapons' sprite set.
enum ItemKind
{
    ItemKind_MeleeLight   = 0,
    ItemKind_MeleeHeavy   = 1,
    ItemKind_FirearmFirst = 2,
    ItemKind_PistolLast   = 4,      // 2..4 change magazines one way, 5, 6 the other
    ItemKind_RifleFirst   = 5,
    ItemKind_FirearmLast  = 6,
    ItemKind_ItemFirst    = 7,
    ItemKind_E            = 0xe,    // StageItemE
    ItemKind_F            = 0xf,    // the named special items
};

inline bool ItemDef::isFirearm() const { return kind >= ItemKind_FirearmFirst && kind <= ItemKind_FirearmLast; }
inline bool ItemDef::isPistol() const { return kind >= ItemKind_FirearmFirst && kind <= ItemKind_PistolLast; }
inline bool ItemDef::isRifle() const { return kind >= ItemKind_RifleFirst && kind <= ItemKind_FirearmLast; }
// The two sound tables (game/stageitem.cpp).
constexpr int kItemLandSounds = 5;
constexpr int kItemPickSounds = 0x13;

// getKind's value of a weapon item (StageFiles::placeItem).
constexpr int kStageObjectKindWeapon = 0x15;
// The weapon definitions (bss 0x576010, 0x50 x 0x54): the item sheet's
// rows of a weapon kind (below ItemKind_ItemFirst), in order.
constexpr int kWeaponDefCount = 0x50;
struct WeaponDef
{
    char   name[0x10];          // FS2 +0x00: LABEL
    int    param1;              // FS2 +0x10: parameter 1
    int    param2;              // FS2 +0x14: parameter 2
    int    range;               // FS2 +0x18: parameter 3: a bullet's reach, units
    int    param4;              // FS2 +0x1c: parameter 4
    int    param5;              // FS2 +0x20: parameter 5
    int    magazine;            // FS2 +0x24: parameter 6
    char   ammo[0x10];          // FS2 +0x28: parameter 7: another item's LABEL; empty none
    int    param8;              // FS2 +0x38: parameter 8
    int    param9;              // FS2 +0x3c: parameter 9
    int    param10;             // FS2 +0x40: parameter 10
    int    param11;             // FS2 +0x44: parameter 11
    int    param12;             // FS2 +0x48: parameter 12
    int    ammoParam1;          // FS2 +0x4c: that item's parameter 1
    int    ammoParam12;         // FS2 +0x50: that item's parameter 12
};
static_assert(sizeof(WeaponDef) == 0x54, "WeaponDef: EE 0x54");
static_assert(offsetof(WeaponDef, magazine) == 0x24, "WeaponDef::magazine");
extern WeaponDef gWeaponDefs[kWeaponDefCount];  /* bss 0x576010 */

// ──────────────────────────────────────────────────────────────────────
// The sound players are game/seplayer.h; the one outside a stage is
// getSePlayer (game/menupack.h).

// What the S2D widgets (game/uiwidget.cpp) call of systems without a TU.

// fsm subtitle files (unassigned@002681e0): tracks of timed 16-bit texts,
// relocated in place once.
bool           fsmRelocate(void *file);                                  // FS2 0x0028fd30: false not an fsm
// The text of track whose time range holds seconds, null none.
const u_short *fsmFindText(float seconds, const void *file, int track);  // FS2 0x0028fb80
// The track whose key is key (strcasecmp), -1 none or not an fsm.
int            fsmFindTrack(void *file, const char *key);               // FS2 0x0028fc80
// The track has a non-empty text.
bool           fsmTrackHasText(void *file, int track);                  // FS2 0x0028fa90


// The StDT stream table's header readers stdtIsRelocated / stdtEntryCount:
// game/stagescene.h.

#endif /* _GAME_GAMEHOOKS_H */
