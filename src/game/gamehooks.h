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
#include "game/routefilter.h"
#include "game/scenepairs.h"
#include "game/scriptrunner.h"
#include "game/framefilter.h"
#include "game/seplayer.h"
#include "game/stagecollision.h"
#include "game/stageloop.h"
#include "game/stageobjlist.h"
#include "game/stagelist.h"
#include "game/stagenode.h"
#include "game/stagesound.h"
#include "game/streamring.h"
#include "game/uilayout.h"
#include "game/uiworkarea.h"
#include "game/vectortrack.h"
#include "game/vu0math.h"
#include "rx/rxfile.h"
#include "sdk/libgraph.h"     /* sceGsDrawEnv1 */
#include "sdk/libvifpk.h"
#include "sdk/libvu0.h"     /* sceVu0FVECTOR */
#include "sdk/scetypes.h"
#include "sound/sound.h"

class GamePacketManager;        // game/gamepacket.h
class GsTexture;                // game/gsutil.h
class TexAreaList;              // game/gsblockmap.h
class DemoFleck;                // game/demofleck.h

struct StageSelect;             // game/sysdata.h

// rx boot: thread priorities given absolute, made relative to the caller.
// The rx file hooks (a file's size: 0 empty, < 0 missing; reading it whole
// into buf) are rx/rxfile.h's rxFileCallSizeHook 0x1556d0 /
// rxFileCallLoadHook 0x155690; the game reaches them through jump thunks of
// the same names beside rxTerm (0x152740 / 0x152730).

// The debug system's empty hooks are game/debugsys.h.

// True when the point (16-aligned xyzw) lies in the box whose world-to-unit-
// cube matrix is m (VU0 transform + vclip against w; game/boxgrid.cpp).
bool boxContainsPoint(const float m[4][4], const float p[4]);            // FS2 0x002362a0

// What game/scene.cpp calls.
// The resident menu pack is game/menupack.h; loadInitPack game/gamepaths.h.

// What game/menuscene.cpp calls.
// help<lang>.d2b at a page; always 0 (game/helpscreen.cpp).
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
    InitItem_SkyMist   = 6,     // StageScene::createSkyMist: the mist's sheet
    InitItem_MenuImages = 7,    // 7..0xc: StageFiles::loadList, MenuProgSet::load
    InitItem_PartyImages = 0xd, // 0xd..0x10: StageParty::addImage
    InitItem_ItemSheet = 0x11,  // UiItemList::setSheet
    InitItem_MenuData  = 0x12,  // StageMenu::setData
    InitItem_PoolImage = 0x13,  // ScenePool::setImage
    InitItem_Rumble    = 0x14,  // RumbleList's +0
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
    StageObject_Car    = 3,     // StageCar: getKind 0x289cc0; takes the motion named CAR (StageFiles::applyBgMotionTarget)
    StageObject_Elevator = 4,   // StageElevator: the model oZS99_01; takes the base motion ElevatorPose
    StageObject_NumLock = 5,    // the number lock, vtable 0x318870, NumLock (game/numlock.h)
    StageObject_SevenKeys = 6, // one of the seven keys oYC11_21..27 (SevenKeys, game/sevenkeys.h); takes the motion named Seven_Keys
    StageObject_Camera = 7,     // StageCamera
    StageObject_Kind8  = 8,     // a camera, vtable 0x311d30 (no host class yet)
    StageObject_Kind9  = 9,     // the main camera "camera" (StageMainCamera): hears from its m_linked's head
    StageObject_Kind10 = 0xa,   // the camera "camview" and its two variants (ViewCamera)
    StageObject_Kind11 = 0xb,   // the party's "camjack" camera: its m_linked record may muffle
    StageObject_Kind12 = 0xc,   // "CAM_DEAD" (DeathCamera)
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
    StageObject_Particle = 0x1d, // StageParticle
    StageObject_CritterNest = 0x1e, // game/stageparticle.h
    StageObject_SubSceneModel = 0x1f, // game/subsceneobject.h, as the next six
    StageObject_SubSceneMap  = 0x20,
    StageObject_SubSceneDoor = 0x21,
    StageObject_SubSceneGate = 0x22,
    StageObject_SubSceneCharacter = 0x23,
    StageObject_SubSceneProp = 0x24,
    StageObject_SubSceneItem = 0x25,
};

class StageNode;
class StageItem;                 // game/stageitem.h
class StageItemE;                // game/stageitem.h
class StageThrownItem;           // game/stageitem.h
class StageWeaponItem;           // game/stageitem.h
class StageParticle;
struct StageNodeTree;
struct MapObjectParts;
struct CharacterHidePart;
class StageLight;               // game/stagelight.h
class StageCar;                 // game/stagecar.h
struct CarParts;
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
    ModelFlag_TakesSpot = 0x1,  // a map model's: VU1 0x2eb draws the scene's spot over it
    ModelFlag_Item   = 0x2,     // every item's, from setupItem
    ModelFlag_CastsShadow = 0x8,  // drawSysBuildShadows takes it (cleared on oXX27_05)
    ModelFlag_TakesShadow = 0x10, // a map model's: VU1 0x393 lays the frame's shadows over it
    ModelFlag_Distant     = 0x40, // past a quarter of the squared draw distance, when StageModel::m_mayGoDistant is set
    ModelFlag_TakesPoint  = 0x80, // a map model's without its own lights: VU1 0x545 adds the scene's point light
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
// Both keep the caller's keys (the node ctors 0x189310, 0x188fe0 store the
// pointer): the pose's bone names, the motion file's own.
typedef LiteralMap<StageNodeBase *> BoneMap;
typedef LiteralMap<ModelMotion *> MotionMap;
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
    StageNodeBase *m_lightProxy = nullptr; // FS2 +0xb8: the lights are worked out at this node's place, not the owner's: a character's bn_pelvis
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
    StageDoor_None     = 5,     // native 0xb3's script state 3
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
class StageObject;
class StageCharacter;
class StageDoor;           // game/stagedoor.h
// The scene's player is a character: one class in the ROM (vtable 0x312240).
typedef StageCharacter StagePlayer;
struct CharacterInput;          // game/characterinput.h
class CharacterGround;          // game/characterground.h
// What drives a character otherwise (+0x220, null without one):
// game/characterbrain.h, game/characterroute.h.
class CharacterBrain;
class CharacterRoute;
// The model instance's record of each mesh (vtable 0x31a7d0, 0x10 on the EE,
// parallel to ModelFileNode::meshes): the mesh tree, threaded through next,
// and what the game switches per part. modelFindPart hands them out: a
// character's +0x240 / +0x244, a light's PartSwitch, VcmTable, AreaClip.
// layout: host.
struct ModelMesh;
constexpr u_char kPartShown = 0;        // ModelPartRec::drawMode
constexpr u_char kPartHidden = 2;       // with all under it (drawTree)
// What AreaClip hides (vtable 0x31a700: its dtor, the two slots empty): a
// model, a mesh of one, a particle effect.
// layout: host -- the vptr, EE +0.
class ClipTarget
{
public:
    // FS2 0x002db030: deleting
    virtual ~ClipTarget() {}
    virtual void   setClipped(u_char clipped) = 0;
    virtual u_char getClipped() const = 0;
};
// layout: host (EE 0x10).
struct ModelPartRec : public ClipTarget
{
    ModelPartRec(const ModelMesh *mesh, u_short next);                   // FS2 0x002e3ba0
    void   setClipped(u_char value) override;                            // FS2 0x002e3b50: slot +0xc of vtable 0x31a7d0
    u_char getClipped() const override;                                  // FS2 0x002e3b60: slot +0x10

    u_short firstChild;         // FS2 +0x4: a mesh index
    u_short next;               // FS2 +0x6: the next sibling, or the parent's; 0 at the end
    u_short childCount;         // FS2 +0x8: 0 for a part grouping none
    u_char  vcmAlpha;           // FS2 +0xa: 0..0x80 (VcmTable::update)
    u_char  vcmFadeBits;        // FS2 +0xb
    u_char  vcmOffBits;         // FS2 +0xc
    u_char  drawMode;           // FS2 +0xd: 0 drawn, 2 hidden with all under it (drawTree); PartSwitch, StageScene::prepareDrawFlags
    u_char  clipped;            // FS2 +0xe: AreaClip hides it, with all under it (drawTree)
    bool    hasMesh;            // FS2 +0xf: ModelMesh::parts is set
};
// The object's second base (EE +0x18): the view flag, then a vtable of four
// slots (empty in the base's table): an entry of ScenePairTable, whose four
// action steps call them. StageMarker's are empty; a light's third is its
// frame (StageLight::sideHook2), a spot's fourth trims it against the map; a
// map object's third advances its model (mapobject.cpp).
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
// What a character may do, by its sheet: 0x2c bytes at +0x3a4, every byte 1
// but what StageCharacter::initAbilities clears. Only the ones named are
// understood; the sheet's columns give the three that come from it.
// EE layout.
struct CharacterAbilities
{
    u_char quickTurn;           // FS2 +0x3a4: QuickTurn
    u_char jack;                // FS2 +0x3a5: Jack: the alternate action, others than the lead
    u_char flag3a6;             // FS2 +0x3a6
    u_char moveCrouched;        // FS2 +0x3a7: Move and Turn in a crouch
    u_char strafe;              // FS2 +0x3a8: Move's w_l, w_r
    u_char walkBack;            // FS2 +0x3a9: Move's w_b
    u_char runStart;            // FS2 +0x3aa: w_run, sq_ka, sqw_w between the walks
    u_char stairs;              // FS2 +0x3ab: Move's stair motions
    u_char gunMove;             // FS2 +0x3ac: GunMove: it walks with the gun up
    u_char flag3ad;             // FS2 +0x3ad
    u_char _3ae[2];
    int    stepLevel;           // FS2 +0x3b0: 0..4, the sheet's 段差Lv
    u_char ladder;              // FS2 +0x3b4: the sheet's ハシゴ
    u_char jump;                // FS2 +0x3b5: the sheet's ｼﾞｬﾝﾌﾟLv
    u_char divesOverEdges;      // FS2 +0x3b6: DamageLink looks for an edge to go over
    u_char flag3b7;             // FS2 +0x3b7
    u_char flag3b8;             // FS2 +0x3b8
    u_char flag3b9;             // FS2 +0x3b9
    u_char traps;               // FS2 +0x3ba: TrapSet, TrapCancel; the near body (CharacterGround)
    u_char throwing;            // FS2 +0x3bb: Throw
    u_char call;                // FS2 +0x3bc: Showt
    u_char reload;              // FS2 +0x3bd: Reload
    u_char flag3be;             // FS2 +0x3be
    u_char flag3bf;             // FS2 +0x3bf
    u_char flag3c0;             // FS2 +0x3c0: StageScene::partnerResponds
    u_char flag3c1;             // FS2 +0x3c1: the same, of the partner
    u_char flag3c2;             // FS2 +0x3c2
    u_char flag3c3;             // FS2 +0x3c3: the same
    u_char flag3c4;             // FS2 +0x3c4
    u_char flag3c5;             // FS2 +0x3c5
    u_char flag3c6;             // FS2 +0x3c6
    u_char _3c7;
    int    driving;             // FS2 +0x3c8: 0..2, the sheet's 運転スキル; isMode1 / isMode2 read it
    u_char flag3cc;             // FS2 +0x3cc
    u_char flag3cd;             // FS2 +0x3cd
    u_char flag3ce;             // FS2 +0x3ce
    u_char _3cf;
};
static_assert(sizeof(CharacterAbilities) == 0x2c, "CharacterAbilities: EE 0x2c");
static_assert(offsetof(CharacterAbilities, stepLevel) == 0xc, "CharacterAbilities::stepLevel");
static_assert(offsetof(CharacterAbilities, flag3c0) == 0x1c, "CharacterAbilities::flag3c0");
static_assert(offsetof(CharacterAbilities, driving) == 0x24, "CharacterAbilities::driving");
static_assert(offsetof(CharacterAbilities, flag3ce) == 0x2a, "CharacterAbilities::flag3ce");

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

    // The object's own slots (the ROM's +0x1f4..+0x248).

    // StageObjectKind. Pure in the ROM.
    virtual int getKind() = 0;
    // FS2 0x00172340
    virtual float getPosX() { return m_xform.position[0]; }
    // The runner's slots +0x30..+0x38 (thunks 0x177510, 0x177500, 0x1774f0).
    float scriptGetPosX() override { return getPosX(); }
    float scriptGetPosY() override { return getPosY(); }
    float scriptGetPosZ() override { return getPosZ(); }
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
    // A script's store to a property: the game's global ones, which a class
    // with properties of its own passes on to.
    void setProperty(int id, int sub, int value) override;               // FS2 0x00189f40
    // The reads, the same way.
    int  getProperty(int id, int sub) override;                          // FS2 0x0018a5b0
    // The runner's slots: thunks to the object's (-0x40). FS2 0x00177520,
    // 0x00177f10, 0x00177f00, 0x00177480 are those thunks.
    const char *getName() override { return getObjectName(); }
    void scriptSetPosition(const float *pos) override { setPosition(pos); }
    void scriptSetRotation(const float *rot) override { setRotation(rot); }
    StageObject *getScriptObject() override { return this; }
    // Thunk 0x00177470 to distanceTo.
    float scriptDistanceTo(StageObject *other) override { return distanceTo(other); }
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
    void scriptAddMenuLine(const u_short *text, int kind, bool plain) override; // FS2 0x00189ce0 (thunk 0x00177450)
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
    // The flags of a map object's model; a gate's and a kind 3's are lit by
    // themselves. Inline in setupMapObject.
    void applyMapModelFlags();                                           // FS2 0x001763e0
    // The same of a character's, by its record's kind.
    void applyCharacterModelFlags(int kind);                             // FS2 0x0017dcb0
    // The body's links, its shadow box from gMotionModels, and for a model
    // with no data the four effects of kBodyEffectCount: keep answers them
    // in a new block, else they are not kept.
    StageParticle **setupBody(bool keep);                                // FS2 0x0017d970
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
    // Breaks part (a face group less 1) of a map object or a door: its faces
    // go, the piece hides, withEffect plays the glass.
    void breakPart(int part, bool withEffect);                           // FS2 0x00175900
    // Of the parts still shown, the one nearest from's head; -1 none.
    int  nearestPart(StageObject *from);                                 // FS2 0x00175700
    // P_SRC<group>_<n>, the map's search points. The object's group, -1 for
    // any other name; index answers n. The highest n of its group (9..1), 0
    // unless this is n 0; the point n of its group.
    int  getSearchGroup(int *index);                                     // FS2 0x0017eeb0
    int  getSearchLastIndex();                                           // FS2 0x0017ed30
    StageObject *findSearchPoint(int index);                             // FS2 0x0017ee30
    // Slot +0x270 of the map objects (doors, CAR, Seven_Keys): a motion
    // list of the bgmotion target (a StageList<void *>); empty here.
    virtual void setMotionList(void *list, int arg, int arg2);           // FS2 vtable +0x270
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
    // By the save's scenario (+0x4bc), whether this character leads it.
    bool isScenarioLead();                                               // FS2 0x00215620
    // +0x5cc above 0: such a character's shadow may come from a light that
    // follows the player (SceneRender::updateLights). A plain stub until its TU.
    bool followsPlayerLight();                                           // FS2 +0x5cc
    // A character's action as the menus see it (StageScene::playerAction):
    // CharacterAction.
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
    CharacterHidePart *m_hidePart; // FS2 +0x204: a character's (StageCharacter)
    union {
        int    m_id;            // FS2 +0x208: a character's number (StageObjectList::addCharacter)
        u_int  m_collisionCount; // FS2 +0x208: a Root object's mesh count
    };
    union {
        int    m_count;         // FS2 +0x20c: an item's count
        int    m_bodySize;      // FS2 +0x20c: a character's: 0..3 from its model's name (_lh_ _th_, _mh_ _ms_, _sh_ _ss_, _ch_), default 1 (StageCharacter::setup)
        StageCollision **m_collisions; // FS2 +0x20c: a Root object's meshes
    };
    u_char _210[0x4];
    MapObjectParts *m_mapParts; // FS2 +0x214: a Root object's: read by the sub-scene's copy
    u_char _218[0x4];
    CharacterInput *m_input;    // FS2 +0x21c: a character's
    CharacterBrain *m_brain;    // FS2 +0x220: a character's; null none
    PlayerMotion *m_motion;     // FS2 +0x224: a character's
    CharacterGround *m_ground;  // FS2 +0x228: a character's
    u_char _22c[0x8];
    StageItemE *m_lampItem;     // FS2 +0x234: a character's: the lamp it holds (SceneRender::update)
    StageObject *m_weapons[2];  // FS2 +0x238: a character's: long (rW00), short (rW01), null none; StageWeaponItem::fire's report comes from the first
    union {
        ModelPartRec *m_parts[2]; // FS2 +0x240: a character's head, whole body; StagePlayer::isMode1 picks which one a view camera hides
        struct {
            int m_doorType;     // FS2 +0x240: a door's StageDoorType
            int m_doorIndex;    // FS2 +0x244: a door's number from 1 (StageScene::findDoorOfFace)
        };
    };
    // +0x288 StageDoor::m_user, +0x29c.. its record's copies (game/stagedoor.h).
    int    m_doorMode;          // FS2 +0x248: a door's StageDoorMode (game/stagedoor.h)
    u_char _24c[0x40];
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
    u_char _3a1;                // FS2 +0x3a1: a character's m_allows.standing (StageScene::wantsPauseMenu)
    u_char _3a2[0x2];
    CharacterAbilities m_abilities; // FS2 +0x3a4: a character's
    StageLight *m_shadowLight;  // FS2 +0x3d0: a character's: what throws its shadow (SceneRender::updateLights); null none
    float  m_shadowWeight;      // FS2 +0x3d4: how strongly
    u_char _3d8[0x28];
    StageObject *m_gear;        // FS2 +0x400: what the player carries; left out of updateLights' head ray
    u_char _404[0x4c];
    union {
        int    m_weaponDef;     // FS2 +0x450: a weapon's gWeaponDefs index (StageFiles::placeItem)
        StageObject *m_foe;     // FS2 +0x450: a character's: who it may hit whatever its side, while on the pad (isEnemyOf): who took it over and hit it (takeDamage); null from initStatus
    };
    union {
        int    m_ammoB;         // FS2 +0x454: a firearm's (findSprite's second number)
        u_char m_altAction;     // FS2 +0x454: the player's: which alternate action (StageScene::playerAction)
    };
    int    m_ammoA;             // FS2 +0x458: a firearm's (the first)
};

// StageObject::getActionState. The values between are unread (other callers).
enum CharacterAction
{
    CharacterAction_None    = 0,
    CharacterAction_Attack  = 2,    // a blow or a shot under way
    CharacterAction_Scope   = 7,    // looking through camview_zoom
    CharacterAction_Driving = 9,
    CharacterAction_Ladder  = 0xd,
    CharacterAction_Held    = 0x10, // a grab
};

// The player's +0x224 object (its +0 the player). Guess: the motion set,
// picked by the weapon carried.
// layout: host -- EE: +0xc00 the Walk link.
// StageObject::m_doorType.
enum StageDoorType
{
    StageDoorType_T = 1,        // takes the motion named MODEL_dr_t
    StageDoorType_R = 2,        // MODEL_dr_r
};
// The name of motion target index 1..8 (CAT, CROW, tonbi, oMT04_04,
// oZS09_04, oDC07_05, oMK21_09, oMT04_99); UNKNOWN for any other.
const char *stageMotionTargetName(int index);                            // FS2 0x00175c00

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
    // The name of part index, null none.
    const char *partName(int index) const;                               // FS2 0x00186f80

    ModelInstance  *model;      // FS2 +0x0
    ModelPartRec **parts;      // FS2 +0x4
    int             count;      // FS2 +0x8
    int             shown;      // FS2 +0xc
};
// The model's part called name (length: strlen), null none.
ModelPartRec *modelFindPart(ModelInstance *model, const char *name, size_t length, int arg); // FS2 0x002e4610
// StageParticle: game/stageparticle.h.
// The first object of group 1 the number lock's handle (StageScene +0x90)
// has a pair for; null none.
StageObject *findNumLockTarget();                                        // FS2 0x0029f440
// Its dials: NumLock, game/numlock.h.
// Every character with m_viewActive: three steps on its motion (+0x224).
void stageUpdateCharacterMotions();                                      // FS2 0x0017df60

// layout: host
struct StageParty
{
    StageParty();                                                        // FS2 0x001fecf0: +0 = new 0x148-byte record (0x203d70)
    ~StageParty();                                                       // FS2 0x001fec90: deleting
    void resetSaved();                                                   // FS2 0x001fe720: the four ids to -1
    // Its record's first byte; stop ends what it runs.
    bool isRunning();                                                    // FS2 0x001fe800
    // Its record's +0x2c.
    bool isRecordBusy();                                                 // FS2 0x001fe6b0
    void stop();                                                         // FS2 0x001feb90
    // The game over's view: the record runs in mode 4 on who.
    void startGameOverView(StageObject *who);                            // FS2 0x001fe7c0
    // The foe's warning: the view on who it is, for 0.3 seconds.
    void warnAttacker(StageCharacter *attacker);                         // FS2 0x001feab0
    void save();                                                         // FS2 0x001fe6d0
    void restore();                                                      // FS2 0x001fe6c0
    // The record's mode (+0xb8) is 3 or 4.
    bool isCompanionMode();                                              // FS2 0x001fe9f0
    // What the sound KAKOSI opens: the record's +0xe4 and mode word +0xdc 8,
    // null also empties the two lists; addVisionA, addVisionB fill them
    // (+0xe8, +0x110); frames +0xe0. Past-sight is a guess from the sound.
    void setVisionTarget(StageObject *object);                           // FS2 0x001fe7a0
    void addVisionA(StageObject *object);                                // FS2 0x001fe770
    void addVisionB(StageObject *object);                                // FS2 0x001fe740
    void setVisionFrames(int frames);                                    // FS2 0x001fe7b0
    // Of the +0 record, by its mode (+0xb8): +0x18 (modes 3, 4), +0x14
    // (mode 1), else null. The name is a guess (StageScene::drawPaused
    // compares it with the view target's character).
    StageObject *getCompanion();                                         // FS2 0x001fea50
    // ScreenFill::setImage on the record's +0x3c.
    void setFillImage(const void *image);                                // FS2 0x001fe9b0
    // The film overlay's data: two parts of the record.
    DemoFleck *getFlecks();                                              // FS2 0x001fea30
    struct JackFilm *getFilm();                                          // FS2 0x001fea40
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
    // The record's +4 and +8: how far the sight-jack reaches and its angle in
    // radians, neither below 0 (natives 0x13c, 0x13d).
    void setJackRange(float range);                                      // FS2 0x001fe850
    void setJackAngle(float angle);                                      // FS2 0x001fe810
    // The record runs, seen through object ("camjack": the sight-jack).
    void start(StageObject *object);                                     // FS2 0x001feba0
    class SightJack *m_jack;    // FS2 +0x0: game/stageparty.h
};

// The stage object's +0xec: what a sub-scene starts from (the 0x287xxx
// classes, vtables 0x318020..; their own TU). Only what stageThread calls.
// layout: host -- EE: vptr at +0x2c.
enum StageEventState
{
    StageEvent_Starting = 0,    // the first poll
    StageEvent_Fading   = 1,    // waits for the fade to black
    StageEvent_Active   = 2,    // StageScene::isEventActive
    StageEvent_Finished = 3,    // StageScene::runStageScripts deletes it
};
constexpr int kStageEventFinished = StageEvent_Finished;
class ScriptRunner;
class FileLoader;               // game/fileloader.h
// layout: host (game/demoevent.cpp)
class StageEvent
{
public:
    StageEvent(float fadeSeconds, const char *name);                     // inline in the ROM (vtable 0x318050)
    virtual ~StageEvent();                                               // FS2 0x002888a0: slot 0, deleting
    // The first subtitle font for a language of the set {1, 5..9}, the
    // event's own otherwise (the argument is unused; the language's
    // meaning is unread).
    bool usesLanguageFont();                                             // FS2 0x00287af0
    // DemoExecuter's flag: the event is not d0101, or SaveData flag
    // (1, 0x15) is set. "Skippable" is a guess.
    bool isSkippable();                                                  // FS2 0x00287b70
    // out = the voice stream's file, .xwb; null with no voice.
    const char *getVoicePath(char *out);                                 // FS2 0x00287bb0

    // Slot 1 (vtable +0xc): set, StageScene::draw leaves the pool's text
    // out, and the end of the event leaves the pause state's list alone.
    virtual bool hidesText() { return false; }                           // FS2 0x00288920 (StageSimpleEvent)
    // Slot 2 (vtable +0x10): one frame; answers the state.
    virtual int poll() = 0;
    // Slot 3 (vtable +0x14): what StageScene::enterSubScene builds the
    // sub-scene from.
    virtual void *getSubSceneData() { return nullptr; }                  // FS2 0x0017f7b0 (stageAnswerZero)

    char   name[0x20];          // FS2 +0x00: the event's id (d0101); the fade seconds follow it
    float  fadeSeconds;         // FS2 +0x20: 0: no fade first
    int    state;               // FS2 +0x24: StageEventState
    ScriptRunner *runner;       // FS2 +0x28: StageScene::isBusy: its script may go on while the event runs
    // A StageFileEvent's (here for enterSubScene, whose event is one).
    void       *pack = nullptr;          // FS2 +0x30: the .edp
    const void *voice = nullptr;         // FS2 +0x34: the voice stream's header (DemoStream::open); null none
    void       *fsm = nullptr;           // FS2 +0x38: subtitles (DemoStream::setFsm)
    const void *subtitleImage = nullptr; // FS2 +0x3c: their font's image
    const void *subtitleFont = nullptr;  // FS2 +0x40: their font

protected:
    // state = Finished; the pause state's list closes unless hidesText.
    void finish();                                                       // inline in the ROM
};

// The event a sub-scene runs from (vtable 0x318010): reads
// demoEU/s<stage>/<name>.edp and its voice and subtitles, then asks the stage
// loop for a sub-scene. The name is a guess.
// layout: host -- EE 0xf4.
class StageFileEvent : public StageEvent
{
public:
    StageFileEvent(float fadeSeconds, const char *name);                 // FS2 0x002882b0
    ~StageFileEvent() override;                                          // FS2 0x00288210
    bool hidesText() override { return true; }                           // FS2 0x00287ad0
    int  poll() override;                                                // FS2 0x00287c00
    void *getSubSceneData() override { return pack; }                    // FS2 0x00287ae0

private:
    // The sound dies down, then the files are queued.
    void queueFiles();                                                   // FS2 0x00287f20
    // Waits for them; the sub-scene, or the end with no pack.
    void takeFiles();                                                    // FS2 0x00287db0

    FileLoader *m_loader;       // FS2 +0x44: the object itself on the EE
    int m_packIndex = -1;       // FS2 +0xe0
    int m_voiceIndex = -1;      // FS2 +0xe4
    int m_fsmIndex = -1;        // FS2 +0xe8
    int m_imageIndex = -1;      // FS2 +0xec
    int m_fontIndex = -1;       // FS2 +0xf0
};
// An event played by the stage's own script: the entry of its name, in a
// runner of its own, with the objects' flags kept (vtable 0x318030). The name
// is a guess.
// layout: host -- EE 0x34.
class StageSimpleEvent : public StageEvent
{
public:
    StageSimpleEvent(float fadeSeconds, const char *name);               // FS2 0x00288850
    ~StageSimpleEvent() override;                                        // FS2 0x002887a0
    int poll() override;                                                 // FS2 0x00288340

private:
    // Runner and entry; no entry ends the event at once.
    void begin();                                                        // inline in the ROM (twice)

    void *m_30 = nullptr;       // FS2 +0x30: delete[] by the dtor; nothing here sets it
};

class Clock;                    // game/clock.h

// A sub-scene's demo voice (StageScene +0x28; the run unassigned@002b0d50):
// a SoundStream with a directory (streamEU/demo/) and what looks like a
// subtitle colour and position. Only what StageScene calls.
// layout: host -- EE 0x80: SoundStream's 0x60, +0x60..+0x70 zero (+0x64
// the directory, +0x68 a string, +0x70 an object with the vptr at +0x10),
// +0x74 0x80606060, +0x78 0x100, +0x7c 0x1bc.
class WideFontPrinter;          // game/fontdata.h

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

private:
    void       *m_fsm = nullptr;        // FS2 +0x60
    char       *m_dir = nullptr;        // FS2 +0x64
    char       *m_path = nullptr;       // FS2 +0x68
    const void *m_header = nullptr;     // FS2 +0x6c
    WideFontPrinter *m_printer = nullptr; // FS2 +0x70: setFont's
    u_int       m_colour = 0x80606060;  // FS2 +0x74: the subtitles'
    int         m_textX = 0x100;        // FS2 +0x78
    int         m_textY = 0x1bc;        // FS2 +0x7c
};

// The scene's fade (StageScene +0xfc; 0x1512a0 / 0x1512e0 in the run
// unassigned@00148db0). Class and mode meaning are guesses.
// layout: host -- EE offsets in the comments, 0x3c.
enum StageFadeMode
{
    StageFade_Idle    = 0,
    StageFade_Running = 1,
    StageFade_Done    = 2,      // holds its target
};
// layout: host
class StageFade
{
public:
    // (x, y, w, h, clock): StageScene passes 0x700, 0x700, 0x200, 0x200, null
    // (GS window units: the 512-pixel screen at the 1792 offset); null: a
    // clock of its own under the game's.
    StageFade(int x, int y, int w, int h, Clock *clock);                 // FS2 0x00151600
    ~StageFade();                                                        // FS2 0x00151550: deleting
    // Goes to target in seconds, from from (negative: from where it is).
    void start(float seconds, float target, float from);                 // FS2 0x00151330
    // The fade's colour, 24 bits.
    // FS2 0x00151380
    void setColour(u_int colour) { m_colour = colour & 0xffffff; }
    // Anything but mode 1.
    // FS2 0x00151320
    bool isNotRunning() const { return m_mode != StageFade_Running; }
    // Full in mode 2 (StageSound does not resume a cue stream then).
    bool isFullInMode2();                                                // FS2 0x001512a0
    // The same in mode 1.
    bool isFullInMode1();                                                // FS2 0x001512e0
    // Slot 2 (vtable +0xc, vptr +4): steps it and fills the rect. The list is
    // unused.
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x001513a0

private:
    int    m_x;                 // FS2 +0x08
    int    m_y;                 // FS2 +0x0c
    int    m_w;                 // FS2 +0x10
    int    m_h;                 // FS2 +0x14
    u_int  m_colour = 0;        // FS2 +0x18: 24 bits
    float  m_drawn = 0.0f;      // FS2 +0x1c: the level clamped
    bool   m_clockShared = false; // FS2 +0x20: not ours to delete
    float  m_target = 0.0f;     // FS2 +0x24
    float  m_from = 0.0f;       // FS2 +0x28
    float  m_level = 0.0f;      // FS2 +0x2c
    float  m_seconds = 0.0f;    // FS2 +0x30
    int    m_mode = StageFade_Idle; // FS2 +0x34: StageFadeMode
    Clock *m_clock = nullptr;   // FS2 +0x38
};

// -- The stage scene's members (StageScene's ctor 0x19c180 / dtor 0x19bbe0;
// batch 080). Each lives in a run not read yet: only the ctor / dtor and
// what game/stagescene.cpp calls are declared. Class names are the port's
// guesses from where they sit and what they allocate (the batch's why).
// The ones the ROM deletes through a vtable get a plain `destroy()` stub
// (the deleting dtor's slot) until their TU, like StageObject's.

// +0x14: ScenePairTable, game/scenepairs.h.

// +0x1c: StageNodeTree, game/stageparticle.h.

// +0x8c..+0x94: the gadgets (SigLamp, the number lock, the seven-key pad),
// game/gadgetview.h.
class SceneOwnedObject;

class RoutePlayer;              // game/routeplayer.h: +0x4c, the stage's rute file

class Tim2Texture;              // game/gsutil.h



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
    ModelMaterialFlag_NoShadow = 0x02,  // takes no shadow, like a blended one (VU1 0x39d); nothing else reads it
    ModelMaterialFlag_Wave    = 0x08,   // has a ModelTexAnim_Wave (set by ModelSurface)
    ModelMaterialFlag_PointFlat = 0x10, // the point light ignores the way a vertex faces (VU1 0x59a)
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
// One packet of a ModelPrim: 5 tags, or 7 with a palette (tags 3, 4: bone
// ids and weights); 0x10 more unless listFlags. Its last two tags' addresses
// list pointers when listFlags.
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
        int     weightsLess1;   // FS2 +0xc: bones per vertex - 1 (VU1 0x07f picks the loop)
    };
    S2dRef<u_short>    paletteBones; // FS2 +0x0: paletteCount indices into ModelMesh::boneMap
    S2dRef<ModelBatch> batches; // FS2 +0x4: batchCount
    u_short            _8;      // FS2 +0x8
    u_short            batchCount; // FS2 +0xa
    u_char             paletteCount; // FS2 +0xc: bones of the palette; non-zero makes 7 tags
    u_char             listACount; // FS2 +0xd
    u_char             listBCount; // FS2 +0xe
    u_char             listFlags; // FS2 +0xf: non-zero: colours and UVs by lists; 2: the fade path 0x2e9220 / 0x2e96a0
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
    int                       paletted;      // FS2 +0x24: its prims carry bones (0x2e43e0 hands it to 0x2e83b0)
    S2dRef<ModelMeshMaterial> materials;     // FS2 +0x28
    S2dRef<S2dRef<float> >    lists;         // FS2 +0x2c: ModelMeshParts::boneCount; [0]: per bone its box, centre then half size, 2 vectors each
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
struct ModelInstance : public ClipTarget
{
    // The record whose children range holds rec's index (the group rec is
    // in), null at the top.
    ModelPartRec *findGroup(const ModelPartRec *rec) const;              // FS2 0x002e4100
    ~ModelInstance() override;                                           // FS2 0x002e46d0: slot +8 of vtable 0x31a7f0, deleting
    // The name of texture slot i (ModelFileNode::textureSlots).
    const char *getTextureName(int slot) const;                          // FS2 0x002e47d0
    void setTexture(int slot, Tim2Texture *texture);                     // FS2 0x002e4550
    void   setClipped(u_char value) override;                            // FS2 0x002e40e0: slot +0xc of vtable 0x31a7f0
    u_char getClipped() const override;                                  // FS2 0x002e40f0: slot +0x10
    void setAmbient(const sceVu0FVECTOR color);                          // FS2 0x002e44e0
    // Light index (0..2): its direction, a column of lightDirs, and colour.
    void setLight(u_int index, const sceVu0FVECTOR direction, const sceVu0FVECTOR color); // FS2 0x002e4500
    // Fills the instance from a node of a relocated file.
    void init(ModelFileNode *fileNode);                                  // FS2 0x002e4800

    u_char         clipped = 0;   // FS2 +0x04: not drawn: AreaClip, and farther than the draw distance (drawSysBuildShadows)
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
    RenaSpotLight *spot = nullptr; // FS2 +0x4c: the scene's (SceneRender::setup)
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
// object's name.
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

    // The sheet's columns. Each answers 1, which parse does not read.
    // FS2 0x001723b0
    bool setKeyKind(int kind) { keyKind = kind; return true; }
    // FS2 0x001723a0
    bool setStartState(int state) { startState = state; return true; }
    // FS2 0x00172390
    bool setWindowState(int state) { windowState = state; return true; }
    // FS2 0x00172380
    bool setHp(short value_) { hp = value_; return true; }
    // FS2 0x00172370
    bool setSeIndex(short index) { seIndex = index; return true; }
};
static_assert(sizeof(DoorRecord) == 0x38, "DoorRecord: EE 0x38");
extern DoorRecord gDoorRecords[kDoorRecords];   /* bss 0x537d80 */

// The stage's map object for a model's node called name; null none. The
// class comes from the name.
StageObject *createMapObject(const char *name, void *collision);         // FS2 0x001766c0
// oGK99_01 and oDC99_01: createMapObject gives them the car (0x28e5a0, kind 3).
bool isCarModelName(const char *name);                                   // FS2 0x001769e0
// A value of a kind 6 object's pose (+0x3d0, sub floats, as an int). Its class,
// 0x2b2a60, is not written.
int sevenKeysPoseValue(StageObject *object, int sub);                    // FS2 0x002b23b0

// The sub-scene's copies of these: game/subsceneobject.h.

// What of a map object switches with its faces (EE 0x14; a stage's Root and
// every sub-scene map object): the part ids its collision record names, a
// PartSwitch for each, and effects with withEffects. Written with the
// collision file: its record's faces are not relocated yet.
// layout: host -- EE offsets in the comments.
struct MapObjectParts
{
    MapObjectParts(StageObject *owner, CollisionRecord *collisionRecord, bool withEffects); // FS2 0x00176bc0
    ~MapObjectParts();                                                   // FS2 0x00176b30
    // Each part as from shows it, and each effect as from's emits.
    void copyShown(const MapObjectParts *from);                          // FS2 0x00176a40

    int             count = 0;      // FS2 +0x00
    int            *ids = nullptr;  // FS2 +0x04
    PartSwitch     *switches = nullptr; // FS2 +0x08
    StageParticle **effects = nullptr;  // FS2 +0x0c
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
    // All four at once; the screen matrix follows.
    void setProjection(float scrZ, float width, float height, float range); // FS2 0x00177be0
    // The far range, what SceneRender::update gives every camera; the same.
    void setRange(float range);                                          // FS2 0x001779a0
    void onViewTop() override;                                           // FS2 0x00177d00
    // The view matrix (+0x240) and view-screen matrix (+0x280) from the
    // object's matrix; each frame for the active camera.
    void calcViewMatrix();                                               // FS2 0x00177810
    // The point is inside the cone of the view.
    bool seesPoint(const sceVu0FVECTOR point);                           // FS2 0x001776d0

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
    // The step after the action's (ScenePairTable's fourth): the ROM's thunk
    // 0x1799f0 falls into cameraStep, as ViewCamera's does.
    void sideHook3() override { cameraStep(); }

    // FS2 0x00178130
    int  collisionHook0(CollisionContact *, const float *, StageCollisionFace *, float) override { return 0; }
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
// ViewCamera, "camview" and its two variants: game/viewcamera.h.
// What the event code calls of TUs to come.
// The generator of the film and the flecks: the next seed; its bits 16..30 are the draw.
// The flecks over a texture, and their end (vtable +8).
// The mission's script helpers beside scriptPrepare.
// After a lock gadget's screen (StageMission::restoreKind): LockRestoreBits.
int  missionRestoreCheck();                                              // FS2 0x0029f2a0
// A kept spawn point out of the view: mode 0.. the mode-th nearest place, or
// the nearest such around it; below 0 at random; -1 none.
bool missionObjectInSight(StageObject *object);                          // FS2 0x0029f200
int  missionPickSpawnPoint(const float *place, int mode);                // FS2 0x0029f810
// The pick said none. The name is older than the reading: -1, nothing else.
bool missionSpawnPointTaken(int *point);                                 // FS2 0x0029fd10
// The first character of its class waiting off stage gets the point: strong
// says which class. false: none waits.
bool missionSpawnAt(int *point, int strong);                             // FS2 0x0029fb60
// The rank-th nearest of group 6's objects called name, from who: by the
// straight line, or by the route. lastInstead: past the end, the farthest.
StageObject *scriptPickNamed(const char *name, StageCharacter *who, u_int rank, bool straight, bool lastInstead); // FS2 0x0029f5c0

// A DemoCamera's overlay (+0x300): what drawOverlay adds after the 3D frame.
enum DemoOverlay
{
    DemoOverlay_None  = 0,
    DemoOverlay_Film  = 1,      // the two effects 0x2a11c0 / 0x204550
    DemoOverlay_FilmDark = 2,   // the same under a half-dark fill
};
// A sub-scene's "camera_demo" (kind 13; vtable 0x316ed0; run
// unassigned@002681e0). layout: host -- EE 0x340.
class DemoCamera : public StageCamera
{
public:
    // FS2 0x0026c910
    int getKind() override { return StageObject_Kind13; }
    DemoCamera();                                                        // FS2 0x0026ce20
    ~DemoCamera() override;                                              // FS2 0x0026cd30
    // The frame: the cut's motion forward; the bones bn_fovy (or bn_length)
    // and bn_top are the lens and the view.
    void cameraStep() override;                                          // FS2 0x0026cb90
    // A cut: the motion file on the camera's model.
    void playCut(void *motionFile);                                      // FS2 0x0026cb70: vtable +0x260
    // The same by name in the event's pack; the view jumps to it at once.
    void playCutNamed(const char *name);                                 // FS2 0x0026ca40: vtable +0x264
    // Native 0x148 on the camera.
    // FS2 0x0026d000
    // callcheck: playCutNamed
    void scriptPlayMotion(const char *name) override { playCutNamed(name); }
    // Sets the overlay; the first one takes the pack's film data.
    void setOverlay(int overlay);                                        // FS2 0x0026cab0
    // The fades on list 9 after the 3D frame.
    void drawOverlay();                                                  // FS2 0x0026c920

    int   m_overlay = DemoOverlay_None;   // FS2 +0x300
    bool  m_ownsFilm = false;   // FS2 +0x304: the dtor deletes m_film (never set here)
    struct JackFilm *m_film = nullptr; // FS2 +0x308
    DemoFleck *m_flecks = nullptr; // FS2 +0x30c
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
    // FS2 0x0017eb20
    // callcheck: operator_delete, ~StageObject (the compiler's)
    ~StageVolume() override {}
    // The object list's group 7, then its script by name.
    void addToScene(const char *name);                                   // FS2 0x0017ecb0: StageObjectList::add7
    // Slot +0x24c: point is inside. Empty in the base's table.
    virtual bool contains(const float *point) = 0;                       // FS2 vtable +0x24c
    // The object's world position is inside. Slot +0x260 of the box's table
    // (data 0x3127e0); the sphere's has no such slot.
    bool containsObject(StageObject *object);                            // FS2 0x0017e950
};
// layout: host
class SphereVolume : public StageVolume
{
public:
    // FS2 0x0017f040
    int getKind() override { return StageObject_SphereVolume; }
    SphereVolume();                                                      // FS2 0x0017ec30
    ~SphereVolume() override;                                            // FS2 0x0017efa0: deleting
    void setRadius(float radius);                                        // FS2 0x0017ec20
    bool contains(const float *point) override;                          // FS2 0x0017eba0
private:
    float m_radius = 0.0f;      // FS2 +0x200
};
// layout: host
class BoxVolume : public StageVolume
{
public:
    // FS2 0x0017e900
    int getKind() override { return StageObject_BoxVolume; }
    BoxVolume();                                                         // FS2 0x0017ea90
    ~BoxVolume() override;                                               // FS2 0x0017e860: deleting
    // The frame's four steps pass a box by.
    void sideHook0() override {}                                         // FS2 0x0017e940
    void sideHook1() override {}                                         // FS2 0x0017e930
    void sideHook2() override {}                                         // FS2 0x0017e920
    void sideHook3() override {}                                         // FS2 0x0017e910
    void setSize(float width, float height, float depth);                // FS2 0x0017ea50
    bool contains(const float *point) override;                          // FS2 0x0017e9a0
    // Runner slot +0xfc (table 0x3125bc): native 0x44 on a box asks whether
    // other stands in it, which is how an AREA_ script hears the player. A
    // sphere keeps the base's no (table 0x312a7c).
    bool scriptIsFacing(StageObject *other) override { return containsObject(other); } // FS2 0x0017f130
private:
    sceVu0FVECTOR m_min = {};   // FS2 +0x200: in the box's own space, z from 0
    sceVu0FVECTOR m_max = {};   // FS2 +0x210
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

// A place with a flag saying it is known (EE 0x20).
struct ScriptPoint
{
    u_char isSet = 0;                                   // FS2 +0x00
    sceVu0FVECTOR point = { 0.0f, 0.0f, 0.0f, 1.0f };   // FS2 +0x10

    void set(const sceVu0FVECTOR value);                // FS2 0x0020b220
};
static_assert(sizeof(ScriptPoint) == 0x20);
static_assert(offsetof(ScriptPoint, point) == 0x10);

// One line of the table of who a character has seen (EE 0xc). A script adds
// to seen (natives 0x97..0x9c, 0xf3, 0xc1), the best foe is the highest;
// the sight's answer is kept for the frame.
struct SightEntry
{
    int   seen = 0;     // FS2 +0x0: how much the foe counts
    int   frame = -1;   // FS2 +0x4: SightEntryAsked: -1 each frame (ageSightTable)
    float distance = 0.0f; // FS2 +0x8: what canSee measured; not touched on reset
};
static_assert(sizeof(SightEntry) == 0xc);
constexpr u_int kSightEntries = 0x18;
// SightEntry::frame: what canSee said this frame.
enum SightEntryAsked
{
    SightEntry_NotSeen  = 0,
    SightEntry_Seen     = 1,
    SightEntry_NotAsked = -1,   // any other value asks
};
// StageCharacter::getLifeState.
enum CharacterLifeState
{
    LifeState_Inactive = 0,
    LifeState_Up       = 2,
    LifeState_Down     = 3,
    LifeState_Corpse   = 4,     // down in the motions 0xb3, 0xb4
    LifeState_Fatal    = 5,     // up, in the motions 0xb1, 0xb2
    LifeState_Leaving  = 6,     // m_lifeOverride above 0: a script's natives 0x13b, 0x17e
    LifeState_Left     = 7,     // m_lifeOverride 0
};
// StageCharacter::m_foeCache: each foe is picked once a frame, on demand.
enum CharacterFoeCache
{
    FoeCache_Best,              // +0x7a4: the highest SightEntry::seen
    FoeCache_Seen,              // +0x7a8: the nearest it sees
    FoeCache_Unnoticed,         // +0x7ac: the nearest it sees whose line is still 0
    FoeCache_Nearest,           // +0x7b0: the nearest, seen or not
    FoeCache_Count,
};

// What StageCharacter::getSoundKind answers: who a cue's name belongs to, in
// the order of the ROM's name table 0x30a058 (the names say which is which).
enum SoundKind
{
    SoundKind_None  = 0,
    SoundKind_Pc    = 1,        // sheet kind 0
    SoundKind_Npc   = 2,        // sheet kind 1
    SoundKind_Enemy = 3,        // E%02d, the sheet's SEtype; kinds 2, 4, 5, 6, 8, 0xa, 0xc
    SoundKind_Umi   = 4,        // sheet kind 0xd
    SoundKind_Srt   = 5,        // sheet kind 3
    SoundKind_Yrt   = 6,        // sheet kind 7
    SoundKind_Ryo   = 7,        // has names; getSoundKind never answers it
    SoundKind_Tks   = 8,        // sheet kind 0xf
    SoundKind_Boss1 = 9,        // sheet kind 0xb
};

// StageObjRecord::kind: the sheet's ctype. What the kinds are is read from
// what the code gives them; a name is a guess unless a comment says more.
enum CharacterKind
{
    CharacterKind_0     = 0x0,  // with 1, 9, 0xe: walks, two feet, motion set 0
    CharacterKind_1     = 0x1,
    CharacterKind_2     = 0x2,  // with 4, 8, 0xa: two feet, motion set 1
    CharacterKind_Srt   = 0x3,  // F_SRT shows
    CharacterKind_4     = 0x4,
    CharacterKind_5     = 0x5,  // with 0xc: one foot effect, the glow at bn_spine_base
    CharacterKind_6     = 0x6,
    CharacterKind_Yrt   = 0x7,  // F_YRT shows
    CharacterKind_8     = 0x8,
    CharacterKind_9     = 0x9,
    CharacterKind_A     = 0xa,
    CharacterKind_NoShadow = 0xb, // bthamon for a splash
    CharacterKind_C     = 0xc,
    CharacterKind_HideD = 0xd,  // otblood for blood; a hide part, as 0xb
    CharacterKind_E     = 0xe,
    CharacterKind_F     = 0xf,  // muffles a Kind11 listener (StageSound)
    CharacterKind_None  = 0x10, // no abilities at all
};
// The last four motions a character started (+0x7c8, 0x38; noteMotion), a
// ring.
constexpr int kMotionHistory = 4;
struct MotionHistoryEntry
{
    int id;                     // FS2 +0x0
    u_int events;               // FS2 +0x4: bit 0 started, then 1 << noteMotion's event
    int frame;                  // FS2 +0x8: StageScene::getMissionFrames
};
constexpr u_int kMotionEventsOver = 0x18;   // MotionHistoryEntry::events: noteMotion's 3 and 4, it ended or gave way
constexpr u_int kMotionEventEnding = 1u << 2; // MotionHistoryEntry::events: noteEnding: it did what it was for
static_assert(sizeof(MotionHistoryEntry) == 0xc, "MotionHistoryEntry: EE 0xc");
struct CharacterBlock7c8
{
    // FS2 0x0021a8c0
    void clear() { next = 0; count = 0; }
    // The newest entry of the motion; null none.
    // FS2 0x0021a8d0
    const MotionHistoryEntry *find(int id) const
    {
        for (u_int i = 0; i < count; i++) {
            const MotionHistoryEntry &entry = entries[(next - i + kMotionHistory - 1) & (kMotionHistory - 1)];
            if (entry.id == id)
                return &entry;
        }
        return nullptr;
    }

    // The motion started last; null none. Inline in the ROM.
    const MotionHistoryEntry *newest() const
    {
        return count != 0 ? &entries[(next + kMotionHistory - 1) & (kMotionHistory - 1)] : nullptr;
    }

    MotionHistoryEntry entries[kMotionHistory]; // FS2 +0x00
    int    next;                // FS2 +0x30
    u_int  count;               // FS2 +0x34
};
static_assert(offsetof(CharacterBlock7c8, count) == 0x34, "CharacterBlock7c8::count");

constexpr int kBodyEffectCount = 4;     // setupBody's block (+0x414)
constexpr int kCharacterFeet   = 2;     // +0x41c / +0x424

// CharacterWants::moveKind: what the stick says, as Move and Turn read it.
enum CharacterMoveKind
{
    CharacterMove_None        = 0,
    CharacterMove_StrafeLeft  = 1, // w_l
    CharacterMove_TurnLeft    = 2, // t_l, standing
    CharacterMove_StrafeRight = 3, // w_r
    CharacterMove_TurnRight   = 4, // t_r, standing
    CharacterMove_Back        = 5, // w_b, moveYaw turns it
    CharacterMove_BackTurn    = 6, // the same
    CharacterMove_Walk        = 7, // w, moveYaw turns it
    CharacterMove_Run         = 8, // run, or w_run into it
    CharacterMove_WalkSlow    = 9, // w
};

// The classes of weapon StageCharacter::canWield is asked about.
enum WieldClass
{
    WieldClass_Light = 1,
    WieldClass_Heavy = 2,
    WieldClass_Short = 4,
    WieldClass_Rifle = 8,
    WieldClass_Any   = 0xffff,
};

// What the input or the brain asks of the character this frame: +0x2c0,
// which the start hook clears. The conditions of the running motion read it
// and turn it into motions. EE layout.
struct CharacterWants
{
    int    moveKind;            // FS2 +0x2c0: CharacterMoveKind
    float  moveYaw;             // FS2 +0x2c4: radians this frame
    u_char squat;               // FS2 +0x2c8
    u_char quickTurn;           // FS2 +0x2c9
    u_char altAction;           // FS2 +0x2ca: latchAltAction
    u_char useLevel;            // FS2 +0x2cb: the item list gave the Use button to Level
    u_char squatAction;         // FS2 +0x2cc: the squat button, on a ladder: let go
    u_char useLadder;           // FS2 +0x2cd: the same for Ladder
    u_char useJump;             // FS2 +0x2ce: for Jump
    u_char useDoor;             // FS2 +0x2cf: Door: open or close, as allows.doorHand
    u_char useDoorKey;          // FS2 +0x2d0: Door: as allows.doorKey
    u_char pickup;              // FS2 +0x2d1: the item list gave the Use button to PickupItem
    u_char putItem;             // FS2 +0x2d2: put down m_putItem (putSmoke)
    u_char _2d3;
    int    putItemEe;           // FS2 +0x2d4: its size on the EE; StageCharacter::m_putItem holds the pointer
    u_char trapSet;             // FS2 +0x2d8: lay the trap (no writer read yet)
    u_char trapCancel;          // FS2 +0x2d9: take it up
    u_char throwItem;           // FS2 +0x2da: throw m_throwItem
    u_char _2db;
    int    throwItemEe;         // FS2 +0x2dc: its size on the EE; StageCharacter::m_throwItem holds the pointer
    sceVu0FVECTOR throwVector;  // FS2 +0x2e0: zero: the default throw
    u_char call;                // FS2 +0x2f0: Showt
    u_char ready;               // FS2 +0x2f1: PadAction_Ready held
    u_char _2f2[2];
    int    fire;                // FS2 +0x2f4: CharacterFire
    u_char reload;              // FS2 +0x2f8: reloadRounds into the gun (no writer read yet)
    u_char _2f9[3];
    int    reloadRounds;        // FS2 +0x2fc
    u_char headOn;              // FS2 +0x300: the head follows headAngles
    u_char _301[0xf];
    sceVu0FVECTOR headAngles;   // FS2 +0x310
    u_char aimOn;               // FS2 +0x320: the upper body follows aimAngles
    u_char _321[0xf];
    sceVu0FVECTOR aimAngles;    // FS2 +0x330
    u_char bite;                // FS2 +0x340: Bite; the input never sets it: the brain's
    u_char liftPartner;         // FS2 +0x341: LevelLift; the input never sets it: the brain?
    u_char _342[2];
    int    dpadSide;            // FS2 +0x344: 1 left, 2 right
    int    viewChange;          // FS2 +0x348: 1 into camview, 2 out of it
    u_char zoomOn;              // FS2 +0x34c: into camview_zoom, or out of it
    u_char _34d[3];
    int    zoomLevel;           // FS2 +0x350
    u_char lampSwitch;          // FS2 +0x354: the lamp changes over
    u_char padUp;               // FS2 +0x355: up of the d-pad, allows.padUp
    u_char rightStickOn;        // FS2 +0x356: allows.rightStick
    u_char _357;
    float  rightStickY;         // FS2 +0x358
    u_char padDown;             // FS2 +0x35c: down of the d-pad, allows.padDown
    u_char putAway;             // FS2 +0x35d: native 0x80, which needs allows.standing
    u_char _35e[2];
};
static_assert(sizeof(CharacterWants) == 0xa0, "CharacterWants: EE 0xa0");
static_assert(offsetof(CharacterWants, headOn) == 0x40, "CharacterWants::headOn");
static_assert(offsetof(CharacterWants, aimAngles) == 0x70, "CharacterWants::aimAngles");
static_assert(offsetof(CharacterWants, lampSwitch) == 0x94, "CharacterWants::lampSwitch");
static_assert(offsetof(CharacterWants, fire) == 0x34, "CharacterWants::fire");
static_assert(offsetof(CharacterWants, throwVector) == 0x20, "CharacterWants::throwVector");
static_assert(offsetof(CharacterWants, reloadRounds) == 0x3c, "CharacterWants::reloadRounds");
static_assert(offsetof(CharacterWants, ready) == 0x31, "CharacterWants::ready");
static_assert(offsetof(CharacterWants, putItemEe) == 0x14, "CharacterWants::putItemEe");
static_assert(offsetof(CharacterWants, dpadSide) == 0x84, "CharacterWants::dpadSide");
static_assert(offsetof(CharacterWants, liftPartner) == 0x81, "CharacterWants::liftPartner");
static_assert(offsetof(CharacterWants, rightStickY) == 0x98, "CharacterWants::rightStickY");

// What the running motion's conditions allow this frame: +0x360, which the
// start hook clears. Each byte is a condition's yes. A queued motion keeps
// this block (MotionQueue). EE layout.
struct CharacterAllows
{
    u_char squat;               // FS2 +0x360
    u_char turn;                // FS2 +0x361
    u_char move;                // FS2 +0x362
    u_char quickTurn;           // FS2 +0x363
    u_char altAction;           // FS2 +0x364: latchAltAction
    u_char _365[3];
    int    level;               // FS2 +0x368: Level: 1 up a ledge, 2 down it (the Use button, UiUse_Level*)
    int    ladder;              // FS2 +0x36c: Ladder: 1 from its foot, 2 from its top
    u_char squatAction;         // FS2 +0x370: LadderMove, at rest on a ladder: the squat button lets go
    u_char jump;                // FS2 +0x371: Jump
    u_char _372[2];
    int    doorHand;            // FS2 +0x374: Door: 1 it can open, 2 close (natives 0x87, 0x88)
    int    doorKey;             // FS2 +0x378: Door: 1 it can lock, 2 unlock, 3 only force (native 0x89)
    int    doorEe;              // FS2 +0x37c: its size on the EE; door below holds the pointer
    u_char pickup;              // FS2 +0x380: PickupItem: m_pickItem can be taken
    u_char _381[3];
    int    pickItemEe;          // FS2 +0x384: its size on the EE; StageCharacter::m_pickItem holds the pointer
    u_char putItem;             // FS2 +0x388: PutItem: nothing stands ahead
    u_char trapSet;             // FS2 +0x389: TrapSet: a trap in hand and floor ahead
    u_char trapCancel;          // FS2 +0x38a: TrapCancel: a trap here to take up
    u_char throwItem;           // FS2 +0x38b: Throw: room ahead
    u_char call;                // FS2 +0x38c: Showt
    u_char armed;               // FS2 +0x38d: Lock, BtOn, BtAttack, BtMove: the stance may stand; wants.ready holds it
    u_char fire;                // FS2 +0x38e
    u_char reload;              // FS2 +0x38f: Reload: the magazine is not full
    u_char head;                // FS2 +0x390: the sheet's 頭振り
    u_char aim;                 // FS2 +0x391: the sheet's 上半身振り
    u_char bite;                // FS2 +0x392: Bite: m_biteTarget is in reach
    u_char _393;
    int    biteTargetEe;        // FS2 +0x394: its size on the EE; StageCharacter::m_biteTarget holds the pointer
    u_char liftPartner;         // FS2 +0x398: LevelLift: the partner stands where it can lift
    u_char lockFind;            // FS2 +0x399: Lock: controlled, no target: it looks for one
    u_char lockSwitch;          // FS2 +0x39a: Lock: controlled, a target: the d-pad's sides change it
    u_char viewSwitch;          // FS2 +0x39b: カメラモード切替
    u_char zoom;                // FS2 +0x39c: 主観ズーム
    u_char lamp;                // FS2 +0x39d: 懐中電灯ON/OFF
    u_char padUp;               // FS2 +0x39e: also R3 for the view (CharacterInput::stepView)
    u_char rightStick;          // FS2 +0x39f
    u_char padDown;             // FS2 +0x3a0
    u_char standing;            // FS2 +0x3a1: a hold or a walk (AlwaysCondition); the player's clear: no pause menu (StageScene::wantsPauseMenu)
    u_char _3a2[2];
};
static_assert(sizeof(CharacterAllows) == 0x44, "CharacterAllows: EE 0x44");
static_assert(offsetof(CharacterAllows, head) == 0x30, "CharacterAllows::head");
static_assert(offsetof(CharacterAllows, level) == 0x8, "CharacterAllows::level");
static_assert(offsetof(CharacterAllows, liftPartner) == 0x38, "CharacterAllows::liftPartner");
static_assert(offsetof(CharacterAllows, fire) == 0x2e, "CharacterAllows::fire");
static_assert(offsetof(CharacterAllows, armed) == 0x2d, "CharacterAllows::armed");
static_assert(offsetof(CharacterAllows, lockSwitch) == 0x3a, "CharacterAllows::lockSwitch");
static_assert(offsetof(CharacterAllows, putItem) == 0x28, "CharacterAllows::putItem");
static_assert(offsetof(CharacterAllows, standing) == 0x41, "CharacterAllows::standing");

// A character's side bits (StageObjSide, StageCharacter +0x590): one word
// with an inline reader, out of line once in the ROM (StageCharacter::getSides).
struct StageSideMask
{
    u_int bits = 0;             // FS2 +0x0
    u_int get() const;                                                   // FS2 0x002160a0
};
static_assert(sizeof(StageSideMask) == 4, "StageSideMask");

// A character of the map (the F_ and player points), the player and the
// partner among them. vtable 0x312240 (0x312264, 0x31227c), game/stagecharacter.cpp.
// layout: host -- EE 0x800.
class StageCharacter : public StageObject
{
public:
    // FS2 0x00179a80
    int getKind() override { return StageObject_Character; }
    // The cameras' slot: the brain's runner starts over and re-enters its
    // state. Only group 0 (the cameras) is stepped, so no ROM path reaches it.
    void cameraStep() override;                                          // FS2 0x00179a90
    StageCharacter();                                                    // FS2 0x0017de30
    ~StageCharacter() override;                                          // FS2 0x0017dd10: deleting
    // Model, textures, the model's two motion lists and mdl.
    void setup(const char *name, ModelInstance *instance, StringMap<Tim2Texture *> *textures,
               StageList<void *> *motions, StageList<void *> *motions2, void *mdl); // FS2 0x0017c700
    // Back to the sheet's values, as when it first stood there. arg goes to
    // the motion.
    void reset(int arg);                                                 // FS2 0x0017c620
    // m_abilities from the sheet.
    void initAbilities();                                                // FS2 0x0017b3e0
    // The status from the sheet.
    void initStatus();                                                   // FS2 0x0017bd10
    // The rest of what a character starts with: the block +0x590, from the
    // sheet, and it tells the brain the sheet's first state.
    void initStatusBlock();                                              // FS2 0x0021cc70
    // A script's reads and stores of its properties (CharacterProperty); the
    // rest go to StageObject's.
    int  getProperty(int id, int sub) override;                          // FS2 0x002132d0
    void setProperty(int id, int sub, int value) override;               // FS2 0x00212530
    // m_sides, for 0..15; the second form sets or clears one bit.
    void setSides(int sides);                                            // FS2 0x0021cbf0
    // The sheet's 射撃: m_blanks and m_shotScatter.
    void setShot(int value);                                             // FS2 0x0021cc10
    void setSideBit(u_int bit, bool on);                                 // FS2 0x0021cab0..0x0021cba0
    void setLightResist(int value);                                      // FS2 0x0021cc50
    // The sheet's weapon columns as CharacterWieldSkill bits.
    u_int getWieldSkills() const;                                        // FS2 0x00210e60
    // Its long weapon is a firearm and the sheet lets it use one.
    bool  canFireLongWeapon() const;                                     // FS2 0x0020cc50
    // The script's number of the weapon in a slot, 0 for an empty one.
    int   getWeaponClass(int slot) const;                                // FS2 0x0021b280, 0x0021b1c0
    // The long weapon's rounds, and what it holds.
    void  setWeaponRounds(int rounds);                                   // FS2 0x0021b140
    int   getWeaponRounds() const;                                       // FS2 0x0021b1a0
    int   getWeaponMagazine() const;                                     // FS2 0x0021b120
    // The rounds it carries for the long firearm; another character says 100.
    int   getCarriedAmmo();                                              // FS2 0x0020cf90
    void  setCarriedAmmo(int rounds);                                    // FS2 0x0021ae80
    // Takes items named so from the stage, free ones, up to count of them.
    // mode 0 takes stacks too; a weapon goes on the first found, and with
    // both hands full the long one is put down, in view for mode 2.
    int   takeNamedItems(const char *name, int count, u_int mode);       // FS2 0x0021bb90
    // +0x630: a script's own numbers.
    int   getScriptValue(int index) const;                               // FS2 0x0021bf00
    void  setScriptValue(int index, int value);                          // FS2 0x0021bf40
    // The lamp it holds. Slots +0x264, +0x260.
    bool  isLampOn();                                                    // FS2 0x0021abc0
    void  setLampOn(bool on);                                            // FS2 0x0021ac00
    // 10..20, from the RNG.
    void rollIdleDelay();                                                // FS2 0x0017bb30
    // The chains of the character's own effects stop (+0x408.., +0x414).
    void stopBodyEffects();                                              // FS2 0x0017bc60
    // The camera on top of the view stack follows this character.
    bool isViewLinked();                                                 // FS2 0x002176d0
    // The door its door motion plays on; null when none runs.
    StageDoor *getMotionDoor();                                          // FS2 0x002179b0
    // The last four motions (+0x7c8) and what happened to each. event 0
    // opens an entry; another sets bit event on the newest entry of id.
    void noteMotion(int id, int event);                                  // FS2 0x0021a960
    // The long and short weapon change places.
    void swapWeapons();                                                  // FS2 0x00216400
    // The sound fields' first values, and a stop of all of them.
    void initSounds();                                                   // FS2 0x002a6910
    void stopSounds();                                                   // FS2 0x002a6830
    // The action state: the fields past +0x3d8. Clears them (stageplayer).
    void resetActionState();                                             // FS2 0x0021e050
    // Hands item over: lamp, weapon, or the list; false: it has an owner.
    bool giveItem(StageObject *item);                                    // FS2 0x00216aa0
    // Takes item off the character: it stands where the character stands,
    // on its own again. false: it did not carry it.
    bool dropItem(StageObject *item);                                    // FS2 0x00216920
    // dropItem, and the item falls; offset: a push, in the character's space.
    void dropWeapon(StageObject *item, sceVu0FVECTOR offset);            // FS2 0x002166d0
    // The same for an item that flies: it goes along the character's position.
    void throwItem(StageThrownItem *item, sceVu0FVECTOR offset);         // FS2 0x00216630
    // There is room for a weapon to drop from where it hangs.
    bool canDropWeapon(StageObject *weapon);                             // FS2 0x00216760
    // The sheet lets the character use a weapon of item's kind; mask: the
    // WieldClass bits asked about.
    bool canWield(StageObject *item, u_int mask);                        // FS2 0x0020b240
    void playPickSound(StageItem *item);                                 // FS2 0x002a3160
    // The sound names of the long weapon (0x2a30b0 opens a gun, 0x2a3060 a
    // round, 0x2a3010 closes it), and of a trap.
    void playMagazineSetSound();                                         // FS2 0x002a3010
    void playReloadSetSound();                                           // FS2 0x002a3060
    void playMagazineOutSound();                                         // FS2 0x002a30b0
    void playTrapOffSound();                                             // FS2 0x002a2ad0
    void playTrapSetSound();                                             // FS2 0x002a2af0
    // What it carries of that name.
    StageObject *findCarried(const char *name);                          // FS2 0x00216310
    // The mission's notice with the item's name.
    void noticeItem(StageItem *item);                                    // FS2 0x002184b0
    // The three status setters of the player's code: each clamps to the
    // sheet, and what going to zero does is theirs.
    void setEndurance(int value);                                        // FS2 0x00211430
    void setRightArm(int value);                                         // FS2 0x00211300
    // The carried item called name; null none.
    StageObject *findItem(const char *name);                             // FS2 0x00216390, 0x0021bee0
    // Its motion is a run, standing or crouched.
    bool isRunningMotion();                                              // FS2 0x00179af0
    // The script point (+0x720) is point: native 0x94.
    void setScriptPoint(const sceVu0FVECTOR point);                      // FS2 0x00179ad0, 0x0020b220
    void setFlinch(int value);                                           // FS2 0x00211150
    // state 0 takes the character out of the scene, another brings it back;
    // 7 starts the count of DamageDead. The brain follows. arg goes to it.
    void setBrainState(int state, int arg);                              // FS2 0x0021ba10
    // from's blow: the parts, the flinch, the endurance; what the endurance
    // lost. kind: getHandClass's, or 4 a shot.
    int  takeDamage(StageObject *from, int kind, int part, int amount);  // FS2 0x0020eb40
    // A blow of from's hand or weapon on a part drawn at random, or the car
    // from drives.
    void takeBlow(StageCharacter *from);                                 // FS2 0x002103d0
    // A fall: twice the amount off each leg, the amount off the endurance.
    // What it lost.
    int  takeFallDamage(int amount);                                     // FS2 0x0020e770
    // The pad shakes when this is who it feels for.
    void rumbleHit(bool hard);                                           // FS2 0x0020e6e0
    // A script's hit: takeDamage on the body, the flinch left as it was, and
    // the motion the reaction asks for (natives 0x1d1, 0x1d2).
    void scriptHurt(StageObject *from, int amount, int reaction);        // FS2 0x00217d60
    // The motion of the sheet labelled name, -1 none; m_placeMotion (native 0x21a).
    void setMotionByLabel(const char *name);                             // FS2 0x00217c80
    // On the party's companion while the player's alternate action is on.
    bool scriptIsCompanion() override;                                   // FS2 0x00217d10
    bool scriptCrouch(float loudness) override;                          // FS2 0x00219a90
    bool scriptDragOutCar() override;                                    // FS2 0x00218260
    bool scriptUseSpecial(const char *name) override;                    // FS2 0x00218190
    // Sound state of a character leaving the scene (unread).
    void stopBodySounds();                                               // FS2 0x002a6380
    // What it would bite: the writer of +0x624 was not found.
    StageObject *getBiteTarget() const { return m_biteCandidate; }       // FS2 0x0021abb0
    // The foe the character knows best (+0x7a4 caches it; +0x468 by id).
    StageObject *getBestFoe();                                           // FS2 0x0021a380
    // The cells of m_foeCache before the frame's first question.
    static StageObject *foeNotAsked() { return reinterpret_cast<StageObject *>(intptr_t(1)); }
    // Whom it can be set on: the characters of its sides' list, every
    // character while m_foe is set.
    void getEnemyCandidates(StageObject ***first, StageObject ***last);  // FS2 0x0020b120
    // The nearest foe, seen or not; the distance, -1 without one.
    void pickNearestFoe();                                               // FS2 0x0021a090
    StageObject *getNearestFoe();                                        // FS2 0x00219f50
    int  getNearestFoeDistance();                                        // FS2 0x00219fa0
    // FoeCache_Seen and FoeCache_Unnoticed.
    void pickSeenEnemies();                                              // FS2 0x0021a1c0
    int  getSeenEnemyId();                                               // FS2 0x0021a040
    StageObject *getUnnoticedFoe();                                      // FS2 0x00219ff0
    // canSee through the frame's SightEntry of other.
    bool canSeeThisFrame(StageCharacter *other, float *distance);        // FS2 0x00210930
    // The foe's SightEntry::seen; 0 unless it is an enemy and up.
    int  getFoeWeight(StageCharacter *other);                            // FS2 0x00217be0
    void setFoeWeight(StageCharacter *other, int weight);                // FS2 0x00217c60
    // Adds to every enemy up, seen or not as inView says, held under limit.
    void addFoeWeight(int amount, int limit, bool inView);               // FS2 0x0021a710
    // The script's lift of the partner, when the player can (allows).
    bool askLift();                                                      // FS2 0x00219110
    // It is in one of the two motions of being bitten.
    bool isBitten();                                                     // FS2 0x0021ad60
    // Down for good: its count is over, or it is out of the view target's
    // reach. takeCorpse ends the count as well.
    bool isCorpseGone();                                                 // FS2 0x002117e0
    bool takeCorpse();                                                   // FS2 0x0021ab10
    // The nearest such corpse; null none.
    StageCharacter *findNearestCorpse();                                 // FS2 0x002199a0
    // Near the scene's view target (250); false without one.
    bool isNearViewTarget();                                             // FS2 0x00212420
    // It sees a burner or an open trap (natives 0x10a, 0x10e).
    bool seesBurner();                                                   // FS2 0x00218e80
    bool seesOpenTrap();                                                 // FS2 0x00218f40
    // 0 no light to work; 1 the one lighting it is out of the player's
    // reach, 2 within it (native 0x96).
    int  getLightReach();                                                // FS2 0x002182c0
    // Lets go of the long weapon.
    bool dropLongWeapon();                                               // FS2 0x0021b340
    // The cue of the long weapon, heard around (native 0x80).
    void playWeaponPickSound();                                          // FS2 0x002a3100
    // Off stage, it comes back: canSpawn asks (no while m_spawnPoint is none
    // and the respawn record has no w), spawn takes the script entry and the
    // place, and starts it; on stages 0x1a, 0x1b, 0x1d in the motion 0xed.
    bool canSpawn();                                                     // FS2 0x002185a0
    void spawn();                                                        // FS2 0x00218630
    // The spawn point missionSpawnAt gave it.
    // FS2 0x002187f0
    void setSpawnPoint(const int *point) { m_spawnPoint = *point; } // FS2 0x002187f0
    // Its name is F_YRT or F_SRT and its sheet kind 3 or 7: it may be sent.
    bool isSpawnable();                                                  // FS2 0x002115e0
    // The sheet kind as the sound tables have it: SoundKind.
    int  getSoundKind();                                                 // FS2 0x002a6980
    // The YRT_VANISH / SRT_VANISH cry, heard around.
    void playVanishSound();                                              // FS2 0x002a31b0
    // The weapon's hit sound, a bare hand's with none; heard around.
    void playHitSound(StageWeaponItem *weapon);                          // FS2 0x002a3220
    // What it cries as a trap takes it (NPC_TRP_I, a shibito's heavy damage).
    void playTrapHitVoice();                                             // FS2 0x002a36e0
    // E%02d_Q or RYO_Q: StageAlert::cry.
    void playQuestionVoice();                                            // FS2 0x002a4340
    // The cue of an attack, in buffer (0x20) when it has to be built; null none.
    const char *getAttackVoice(char *buffer, int second, bool altered);  // FS2 0x002a25b0
    // A script ends it: its life state says leaving, with the effect of
    // sheet kind 3 and the cry; the quiet form without. In the two motions
    // of 0x9e, 0x9f they wait, the running script yields.
    void leaveLoud();                                                    // FS2 0x00219510
    void leaveQuiet();                                                   // FS2 0x00219420
    // leaveLoud, and the corpses around note how far it was.
    void leaveNoticed();                                                 // FS2 0x002196f0
    // Off with no fuss: reset, and the brain off.
    void leaveAtOnce();                                                  // FS2 0x00219310
    // The brain back at its first state; false with no brain or brain off.
    bool wakeBrain();                                                    // FS2 0x00219cb0
    void allowPath(const char *name, bool on);                           // FS2 0x00218110
    // The motion's ledge point, for the player above other: 0 none, 1 far,
    // 2 at it; the script point goes there.
    int  getClimbPoint(StageObject *other);                              // FS2 0x00219040
    // The nearest weapon lying free that it can wield, under range.
    StageObject *findLooseWeapon(float range, u_int mask);               // FS2 0x0021b690
    // The first point of the nearest search group within 50, kept.
    StageObject *getNearSearchPoint();                                   // FS2 0x0021c010
    // The rank-th nearest search group from point; null past the end.
    StageObject *pickSearchPoint(const sceVu0FVECTOR point, u_int rank); // FS2 0x00219210
    // foes: the nearest enemy up among getEnemyCandidates; foes false: the
    // nearest ally up among the mission's characters (not itself). sight
    // keeps only what canSeeThisFrame sees, by the distance it answers;
    // else by distanceTo. Null without one.
    StageObject *findNearestFoe(bool foes, bool sight);                  // FS2 0x00214a80
    // point lies inside its view: within viewAngle (the whole cone; a half
    // circle up to kCloseViewRange), nearer than scale * m_sight, and no
    // face between; skip goes to the ray. distance answers how far.
    bool isPointInView(float viewAngle, float scale, const float *point, float *distance, StageObject *skip); // FS2 0x00210b30
    // isPointInView on target: a character's head, by how visible it is;
    // anything else's position at full range.
    bool canSee(float viewAngle, StageObject *target, float *distance);  // FS2 0x00210940
    // Neither is the other's enemy.
    bool isAlly(StageObject *other);                                     // FS2 0x00214cf0
    // The mission's list runs these around the frame's scripts
    // (StageScene::runStageScripts): before, what it heard, saw and how
    // visible it is; after, a brain gone off takes it out of the scene.
    void beforeScripts();                                                // FS2 0x0021c7c0
    void afterScripts();                                                 // FS2 0x0021c310
    // The status block's frame: what it hears of the scene's noises
    // (m_scriptValues[kScriptValueHeard], m_heardFrom, m_point6e0), and its
    // two countdowns.
    void stepStatus();                                                   // FS2 0x0021c3d0
    // m_sightTable forgets the lines of characters no longer worth seeing.
    void ageSightTable();                                                // FS2 0x00217a80
    // m_visibility from the VISIBILITY_ volume it stands in, its posture,
    // its lamp and the light on it.
    float updateVisibility();                                            // FS2 0x0020d8c0
    // The thrown items it sees (m_seenThrown, m_seenFlying, m_followedFlying).
    void watchThrownItems();                                             // inline in 0x0021c7c0
    // The voice of a found foe (RYO_FIND or the sheet's).
    void sayFound();                                                     // FS2 0x002a46d0
    // +0x38c allows it: the motion's SHOUT at this loudness.
    bool tryShout(float loudness);                                       // FS2 0x00216270
    // A cue from the character, as voice: a cue with a priority takes the
    // place of what plays now only if it counts for at least as much.
    bool playVoice(const char *name);                                    // FS2 0x002a6440
    // The cue plays, from this character.
    bool isVoicePlaying(const char *name);                               // FS2 0x002a6430
    // How far its long weapon reaches; 0 without one.
    inline float getWeaponRange() const;                                 // FS2 0x0020ce10
    // object stands within 180, above by -20..180, in the front half, and in
    // 30 degrees of a point 10 ahead.
    bool isInReachOf(StageObject *object);                               // FS2 0x00219d70
    // The route's length between two points, kept in m_pathLength; under 15
    // it counts as 0, below 0 no way.
    float measurePath(const float *from, const float *to);               // FS2 0x00219160
    bool stopMoving();                                                   // FS2 0x00219200
    // The long weapon is the object called name.
    bool holdsWeapon(const char *name);                                  // FS2 0x00219c50
    void scriptSetBrainEntry(int kind, const ScriptEntry *entry) override; // FS2 0x0021b970 (thunk 0x0017e7c0)
    int  scriptHeardKind() override;                                     // FS2 0x0021b890 (thunk 0x0017e7a0)
    bool scriptIsCrouching() override;                                   // FS2 0x00179bb0 (thunk 0x0017e660)
    // FS2 0x00179d20 (thunk 0x0017e7e0)
    StageCharacter *getScriptActor() override { return this; }
    // The lamp it holds (thunk 0x0017e7f0).
    // callcheck: setLampOn
    void cmdSetTreeSwitchedOn(bool on) override { setLampOn(on); }
    // It has other in reach of its weapon: the blow's, and a bullet's when
    // that is shorter.
    bool scriptIsFacing(StageObject *other) override;                    // FS2 0x0020ca80 (thunk 0x0017e6c0)
    bool scriptCanSee(float viewAngle, StageObject *other) override;     // FS2 0x00210910 (thunk 0x0017e790)
    bool scriptSwingAt(StageObject *target) override;                    // FS2 0x0021ad20 (thunk 0x0017e710)
    bool scriptFireAt(StageObject *target, const char *flags, int frames30) override; // FS2 0x0021ace0 (thunk 0x0017e6f0)
    int  scriptRouteAction12c() override;                                // FS2 0x00218470 (thunk 0x0017e640)
    int  scriptRouteAction130() override;                                // FS2 0x00218450 (thunk 0x0017e630)
    void scriptMarkPlace() override;                                     // FS2 0x0021bfd0 (thunk 0x0017e780)
    bool scriptAttackBite() override;                                    // FS2 0x0021ada0 (thunk 0x0017e730)
    bool scriptReload(int rounds) override;                              // FS2 0x0021b450 (thunk 0x0017e6a0)
    bool scriptHasLineTo(StageCharacter *other) override;                // FS2 0x0021adc0 (thunk 0x0017e690)
    void scriptPutWeaponAway() override;                                 // FS2 0x002183d0 (thunk 0x0017e5e0)
    bool scriptUseSmoke() override;                                      // FS2 0x00218e60 (thunk 0x0017e5a0)
    bool scriptUseNamedItem(const char *name) override;                  // FS2 0x00218e10 (thunk 0x0017e580)
    bool scriptSetTrap() override;                                       // FS2 0x00218b00 (thunk 0x0017e570)
    bool scriptPutItem() override;                                       // FS2 0x00218af0 (thunk 0x0017e560)
    bool scriptTakeUpTrap() override;                                    // FS2 0x00218800 (thunk 0x0017e550)
    StageObject *scriptTakeItem() override;                              // FS2 0x0021b370 (thunk 0x0017e510)
    // A script's wait: the carried item called name is thrown with point as
    // its vector (zero: the default throw). True when the throw has ended;
    // false with nothing to throw, or the script yields until it is known.
    bool throwCarried(const char *name, const float *point, bool retry); // FS2 0x00218b50
    // The same for putting the smoke down. The brain never asks for this.
    bool putSmoke(bool retry);                                           // FS2 0x00218850
    // Sets what the Throw condition reads.
    void wantThrow(bool on, StageObject *item, const float *vector);     // FS2 0x0020b200
    // FS2 0x00219000
    bool canTakeUpTrap() const { return m_allows.trapCancel != 0; }
    // FS2 0x00219980
    bool heardWithin(float range) const { return m_lastNoiseRange <= range; }
    // +0x400 holds something.
    bool hasGear() const { return m_gear != nullptr; }                   // FS2 0x00215290
    // How far its blow reaches: the long weapon's, else the sheet's 攻撃判定.
    float getAttackReach();                                              // FS2 0x0020ce80
    // A special action of the motion (PlayerMotion::m_specials).
    bool isSpecialAllowed(const char *name);                             // FS2 0x0017b3b0
    // Into the scene's car (off 0), or out of it (the other values: the ways
    // out, playermotion.cpp). side picks the door. One who cannot drive at
    // all answers false and nothing happens.
    bool rideCar(int off, int side);                                     // FS2 0x0017af50
    // It stands on the elevator: the floor's object is of kind 4.
    bool isOnElevator();                                                 // FS2 0x002123e0
    // The motion rate's timed change: off (letting go of the drained
    // motion), from one rate to another, a stage's drain.
    void clearSlowChange();                                              // FS2 0x0020d1c0
    void setSlowChange(int fromPercent, int frames30, int toPercent);    // FS2 0x0020d210
    void startDrainSlow(int frames30, int rate);                         // FS2 0x0020d2c0
    // As playVoice, from its head and heard; the source is what stopSounds
    // stops.
    void playVoiceHeard(const char *name);                               // FS2 0x002a65c0
    // Where it comes back (w 1).
    void setRespawnPlace(const float *position, const float *rotation);  // FS2 0x00218610
    bool scriptHasAllyWithin(float range) override;                      // FS2 0x00210810
    bool scriptAskRouteKind11() override;                                // FS2 0x00218200
    bool scriptSwingAtBite() override;                                   // FS2 0x0021ad40
    bool scriptFireAtBite(const char *flags, int frames30) override;     // FS2 0x0021ad00
    int  scriptPickableHandle() override;                                // FS2 0x0021ac30
    bool scriptUseItemAt(const char *name, const float *point) override; // FS2 0x00218e00
    bool scriptCallOut(float loudness) override;                         // FS2 0x00219ae0
    bool scriptAskNumLock(int which) override;                           // FS2 0x00218140
    void setSpecial(bool on, const char *name);                          // FS2 0x0017b3c0

    // The player's.
    // Its action is 0x11 or 0x2a (+0x224's +0x40), what an event breaks off.
    bool isHoldingAction();                                              // FS2 0x00214f20
    // +0x400 set and +0x3c8 == 1: StageScene::drawPaused / update then
    // pass StageSound::update sceneFlag 1. What mode 1 is is unread.
    bool isMode1();                                                      // FS2 0x002152d0
    // The sheet's 運転スキル makes it a driver, in a car or not.
    bool isDriver();                                                     // FS2 0x00215310
    // The sheet's 運転スキル is 2: rides beside the driver.
    bool isPassenger();                                                  // FS2 0x00215300
    // The door of car in reach, 0 or 1, or -1. facing: looking at it.
    int  findCarDoor(StageCar *car, bool facing);                        // FS2 0x00280fd0
    // m_allows.altAction set: m_wants.altAction = 1 (it is never cleared here).
    void latchAltAction();                                               // FS2 0x0020b1d0
    // The object it is: the callers that hold it as the scene's player.
    StageObject *character() { return this; }
    // A named event of the character in the scene's +0x78, while +0x30 is
    // set; what takes it is unread (GUNSHOOT).
    void playerEvent(const char *name);                                  // FS2 0x0020e6a0
    // Who the pad moves is this one (the scene's controlled, else player).
    bool isControlled() const;                                           // FS2 0x00215270
    void playLandSound();                                                // FS2 0x002a2fc0
    // Free to answer the Use button: no walk, queue or fade under way.
    // The door the Door condition found, as CharacterDoorAction bits.
    int  doorActions() const;                                            // FS2 0x00219b30
    // A door's locked side: the key works, or the glass is out and one can
    // reach through from the front. Its other answers keep to their ROM names.
    bool canUnlockDoor(StageDoor *door) const;                           // FS2 0x0020d730
    // Locked, and only force would do.
    bool canForceDoor(StageDoor *door) const;                            // FS2 0x0020d650
    bool canLockDoor(StageDoor *door) const;                             // FS2 0x0020d820
    // Reaches through the broken glass.
    bool reachesThroughDoor(StageDoor *door) const;                      // FS2 0x0020d6d0
    // Someone follows it close by, so the door goes faster. Slot +0x2ac.
    // A living enemy holds it in its bite.
    bool hasBiter();                                                     // FS2 0x00210720
    // The door's pace of a character with a brain. Slot +0x278.
    int  getBrainPace();                                                 // FS2 0x0021bb70
    StageCharacter *getScriptCharacter() override { return this; }       // FS2 0x00179d10
    // A character put somewhere starts over there: its motion, the ground
    // and the brain, and the view when it looks through this character.
    // Runner slot +0x3c, natives 0x26, 0x47, 0x144.
    void setPosition(const float *pos) override;                         // FS2 0x00218500
    // Runner slot +0x14: its script ended, so the brain's start over.
    void onScriptExit() override;                                        // FS2 0x00219010
    // Runner slot +0x140, native 0x68: a special when it is controlled, the
    // brain's action otherwise.
    void scriptPlayMotion(const char *name) override;                    // FS2 0x0021ac60
    // Runner slot +0x160: Prop_SwitchedOn on a character is its lamp.
    bool cmdIsLit() override { return isLampOn(); }
    // Empty, as the base's.
    void noiseHook() override {}                                         // FS2 0x00179ae0
    // Runner slots no script reaches: natives 0x2e, 0x36, 0x37, 0x63, 0x6c
    // and 0x84 are on no script of the disc (sssall.py), and slot +0xe4 of a
    // character has no caller (the brain answers its own). Thunks -0x40:
    // stagecharacter.cpp.
    int  scriptBrainPace() override { return getBrainPace(); }
    void scriptWalkToMarkedPlace(bool run) override { walkToMarkedPlace(run); }
    void scriptForgetMarkedPlace() override { forgetMarkedPlace(); }
    bool scriptAnswerNo() override { return answerNo() != 0; }
    int  scriptShout(float loudness) override { return shout(loudness); }
    bool scriptHasBiter() override { return hasBiter(); }
    void scriptDoNothing() override { doNothing(); }
    bool scriptCarriesItem(const char *name) override { return findItem(name) != nullptr; }
    bool scriptIsBulletFlying() override { return isBulletFlying(); }
    bool scriptIsRunningMotion() override { return isRunningMotion(); }
    void scriptWaitMotionChange() override { waitMotionChange(); }
    int  scriptGetValue(int index) override { return getScriptValue(index); }
    void scriptSetValue(int index, int value) override { setScriptValue(index, value); }
    bool shout(float loudness);                                          // FS2 0x0021b3f0
    void walkToMarkedPlace(bool run);                                    // FS2 0x0021bf90
    void forgetMarkedPlace();                                            // FS2 0x0021bf80
    // Holds the script in its native until the motion changes.
    void waitMotionChange();                                             // FS2 0x00219be0
    // FS2 0x0021b960
    int  answerNo() { return 0; }
    // FS2 0x00218490
    void doNothing() {}
    // FS2 0x00179d00
    bool isBulletFlying() const { return m_bulletFlying != 0; }
    void sideHook0() override;                                           // FS2 0x0017a860
    void sideHook1() override;                                           // FS2 0x0017a810
    // Natives 0x87, 0x88, 0x89: the controlled character, by what it may do;
    // the brain's for the rest. Slots +0x30c, +0x310, +0x314.
    bool scriptOpenDoor() override;                                      // FS2 0x0021c230
    bool scriptCloseDoor() override;                                     // FS2 0x0021c1d0
    bool scriptUseDoorKey(int lock) override;                            // FS2 0x0021c130
    bool canTakePrompt();                                                // FS2 0x0020c0e0
    // The same, and in a motion a script's prompt may interrupt.
    bool canUsePrompt();                                                 // FS2 0x0020b590
    // Stage 1 for the sheet's kind 0, stage 2 for its kind 1: no deep drops.
    bool isOpeningLead();                                                // FS2 0x00215ce0
    // On a ladder, who gives way: 0 kind 1, 1 kind 0, 2 anyone else.
    int  getLadderRank();                                                // FS2 0x00271f50
    // It acts for the player (controlled, not the player itself).
    bool isTakenOver() const;                                            // FS2 0x00211970
    // The player, when this one acts for it; else null.
    StagePlayer *getTakenOverPlayer() const;                             // FS2 0x00211910
    // The sides it fights: the player's while it acts for the player.
    u_int getSides() const;                                              // FS2 0x00215f10
    // Its own side bits (m_sides), whoever it acts for.
    const StageSideMask &getSideMask() const;                            // FS2 0x002160b0
    // target's sheet kind belongs to a side it fights, or target is m_foe.
    bool isEnemyOf(StageObject *target);                                 // FS2 0x00214d40
    // One of its blows' bodies is on now.
    bool isBlowActive() const;                                           // FS2 0x002162a0
    // Where it aims at m_290, and at target: the head of a character, or from
    // past 300 with a firearm its chest when that can be seen (plain: the
    // head anyway); kind 0xb its nearest of head, pelvis and foot; a map
    // object's nearest part. False: no such point.
    bool getLockPoint(float *out, bool plain);                           // FS2 0x0020c1d0
    bool getTargetPoint(StageObject *target, float *out, bool plain);    // FS2 0x0020c1f0
    // How the view changes by itself: 1 into the view camera, 0 not. For
    // the one on the pad, when a script says so or it acts for the player.
    int  forcedViewChange() const;                                       // FS2 0x00214ee0
    // Radians a 1/30 s frame turns at full stick, by the clock.
    float getTurnRate();                                                 // FS2 0x002162d0
    // 0 inactive; 2 up, 3 down, 4 down in motions 0xb3 / 0xb4, 5 in 0xb1 /
    // 0xb2, 6 and 7 by +0x690. The brain's state 6 says 2, its state 0 says 0.
    int  getLifeState();                                                 // FS2 0x00212460
    bool hasKyoModel();                                                  // FS2 0x00215340
    // A brain not at 0, the sheet's kind not 3 or 7, not the scene's player:
    // the party's view may look through its eyes. The name is a guess.
    bool canBeJacked();                                                  // FS2 0x00212160
    // The sight-jack shows it as it shows a corpse (stage 0x12's; unread).
    bool isJackBlank();                                                  // FS2 0x00215370
    // The sight-jack may show this viewer a vision (read in part).
    bool canSeeVisions();                                                // FS2 0x00215480
    // Not the partner, the sheet's kind not 0xb, slot +0x278 (the brain's
    // state, 1 without one) none of 2..5, 7: the player may become it.
    bool canTakeOver();                                                  // FS2 0x00212020
    // Kind 0xb in a walking motion (the list is unread past its first half).
    bool isCompanionWalking();                                           // FS2 0x002119d0
    // The motion rate, 1 outside a timed change.
    float getMotionRate();                                               // FS2 0x0020d400
    // The world matrix the head's sway of kind 2 turns to.
    void getSwayMatrix(sceVu0FMATRIX out);                               // FS2 0x0020c940
    // Inside the timed change of the motion rate.
    bool isMotionSlowed();                                               // FS2 0x0020d390
    // The stages where a character of one kind loses m_health by itself
    // (0x19, 0x1b: kind 1; 0xf, 0x17: kind 0): the same test as getMotionRate.
    bool isStageDrained() const;                                         // FS2 0x00215940
    void playLadderStep();                                               // FS2 0x002a2100
    // A step on surface at pace (0 quiet .. 3 running; see kStepColumns).
    void playStepSound(int surface, int pace, const float *pos);         // FS2 0x002a49c0
    const SeEntry *findStepSound(int surface, int pace) const;           // FS2 0x002cf210
    // The movement hook: the motion's step, then the model.
    void moveStep();                                                     // FS2 0x0017a490
    // The terminate hook: the ground's and the motion's end of frame, then
    // what follows the character: its effects, its lamp, its sounds, the end
    // of a timed change of the motion rate, the light on what it carries.
    void sideHook3() override;                                           // FS2 0x00179d30
    // The parts of the hook, one function in the ROM.
    void stepSmoke();
    void stepSplash();
    void stepFootEffects();
    void stepLampOwner();
    void stepSlowChange();
    void lightCarriedItems();
    // Each frame: a finished voice lets go, then its breath, how it moves and
    // what it says by itself.
    void updateSounds();                                                 // FS2 0x002a4a40
    // What a motion event asks for (MotionEvent_HitSwing, MotionEvent_Sound
    // through the sheet's S0..S3); the 0x2a2070..0x2a4800 family, game/charactersound.cpp.
    void playSwingSound(bool light);                                     // FS2 0x002a3280
    void playShout(float loudness, int unused, const float *pos);        // FS2 0x002a4800
    void playIdleVoice(int which);                                       // FS2 0x002a2bc0
    void playDownVoice();                                                // FS2 0x002a4050
    void playClashVoice();                                               // FS2 0x002a32e0
    void playBreathUp(int level);                                        // FS2 0x002a3e20
    void playBreathDown(int level);                                      // FS2 0x002a3cf0
    void playJumpVoice();                                                // FS2 0x002a3bf0
    void playAttackVoice(int second, bool altered);                      // FS2 0x002a4680
    void playHardDownVoice();                                            // FS2 0x002a3f50
    void playTrapVoice();                                                // FS2 0x002a35e0
    void playThrowVoice();                                               // FS2 0x002a4150
    void playBiteVoice();                                                // FS2 0x002a4250
    void playBittenVoice();                                              // FS2 0x002a3af0
    void playUntieVoice();                                               // FS2 0x002a39f0
    void playDamageVoice(bool light);                                    // FS2 0x002a4450
    void playDieVoice();                                                 // FS2 0x002a2e30
    void playGetUpVoice();                                               // FS2 0x002a2d00
    void playSurpriseVoice();                                            // FS2 0x002a34e0
    void playPanicVoice();                                               // FS2 0x002a2c80
    void playEndVoice();                                                 // FS2 0x002a33e0
    void playFallDownSound(int surface);                                 // FS2 0x002a29f0
    void playJobSound(int job);                                          // FS2 0x002a2b60
    void playMotionSound(const char *name, const float *pos);            // FS2 0x002a2070
    // The attack voices' other set: F_E.._TKA on stages 0x1c and 0x18, the
    // sheet's kind 0 on 0x11 and 0xe, kind 1 on 5.
    bool hasAlteredVoice();                                              // FS2 0x00215af0
    // 2 at the wheel, 3 the short weapons, 9 bare hands; the rest of the
    // table is unread.
    int  getHandClass();                                                 // FS2 0x00210540
    // A blow on a wall: the surface's sound, for one with a long weapon.
    void playBumpSound(int surface);                                     // FS2 0x002a2b10

    StageList<StageObject *> m_items; // FS2 +0x22c: what it carries (giveItem)
    StageObject *m_294 = nullptr; // FS2 +0x294: a target given to a character off the pad: Lock takes it (writers unread)
    sceVu0FVECTOR m_aimPoint = {}; // FS2 +0x2a0: where its bullets steer
    StageObject *m_lookAt = nullptr; // FS2 +0x2b0: who holds it: a biter (BiteLink); in a car the heads turn to it
    CharacterWants  m_wants = {};   // FS2 +0x2c0: the start hook clears it, input and brain fill it
    // CharacterAllows +0x37c: the door in reach. PC: a pointer does not fit
    // the EE's int there, so it stands beside; the same clear covers both.
    StageDoor *m_door = nullptr;
    // CharacterAllows +0x384 and CharacterWants +0x2d4, the same way.
    StageCharacter *m_biteTarget = nullptr; // beside CharacterAllows +0x394, the same way
    StageItem *m_pickItem = nullptr;
    StageItem *m_putItem = nullptr;
    StageThrownItem *m_throwItem = nullptr; // beside wants.throwItemEe, as m_putItem
    int    m_reloadRounds = 0;  // FS2 +0x3dc: rounds still to load, one by one
    CharacterAllows m_allows = {};  // FS2 +0x360: the start hook clears it, the conditions fill it
    u_char m_dropsWeapon = 1;   // FS2 +0x3d8: 1 from resetActionState; Damage and DamageFatal let the long weapon go
    u_char m_armLost = 0;       // FS2 +0x3d9: the right arm just reached 0 (setRightArm)
    u_char m_fidgets = 1;       // FS2 +0x3e0: resetActionState sets it, a script's property may clear it: stepFidgets' idle0 / idle1
    u_char m_flinched = 0;      // FS2 +0x3e1: flinch went from 0 straight to the sheet's limit (setFlinch; readers unread)
    SightEntry m_sightTable[kSightEntries] = {}; // FS2 +0x468: one per character id - 1 (0x21a1c0); the brain fills it
    StageCharacter *m_sightOwner = nullptr;      // FS2 +0x588: this, from resetActionState
    float  m_flinchClock = 0.0f; // FS2 +0x3e4: 0 from initStatus; sideHook0's seconds
    float  m_unarmedClock = 0.0f; // FS2 +0x3e8
    float  m_enduranceClock = 0.0f; // FS2 +0x3ec
    float  m_lightHurtWait = 0.0f; // FS2 +0x3f0: seconds until a light can hurt it again (PlayerMotion::takeLightHit)
    u_char m_winded = 0;        // FS2 +0x3f4: m_health ran out, until it is back at 40%: the stance tables' second row, no running
    float  m_idleTime = 0.0f;   // FS2 +0x3f8: seconds standing, for the next fidget
    float  m_idleDelay = 0.0f;  // FS2 +0x3fc: the wait for a fidget (rollIdleDelay)
    const char *m_bloodEffect = nullptr; // FS2 +0x404: the effect its blood is
    StageParticle *m_smoke[2] = {};      // FS2 +0x408: yamismoke1, yamismoke2 (stages 0x11..0x15)
    StageParticle *m_glow = nullptr;     // FS2 +0x410: koh, at bn_spine_base
    StageParticle **m_bodyEffects = nullptr; // FS2 +0x414: setupBody's block, kBodyEffectCount
    StageParticle *m_splash = nullptr;   // FS2 +0x418: bthamon or L_SPLASH
    StageParticle *m_footEffects[kCharacterFeet] = {}; // FS2 +0x41c: F_HAMON
    StageNodeBase *m_footNodes[kCharacterFeet] = {};   // FS2 +0x424: where they stand: the feet, or the character
    u_char m_42c = 0;           // FS2 +0x42c: 0 from the ctor (unread)
    int    m_soundState = 0;    // FS2 +0x430: 0 none, 1 a source, 2 a cue (stopSounds)
    const void *m_soundHandle = nullptr; // FS2 +0x434: the source or the cue
    int    m_soundPriority = 0x7fffffff; // FS2 +0x438: what plays counts for this; initSounds
    StageSoundSource *m_breathSound = nullptr; // FS2 +0x43c: its breath at the head; updateSounds, stopped when it leaves the scene
    int    m_moveSoundState = 0;   // FS2 +0x440: what m_moveSound is, by the sheet's kind; above 4 a motion's own sound, the motion id + 5
    StageSoundSource *m_moveSound = nullptr; // FS2 +0x444: the sound of how it moves, or of its motion (playMotionSound)
    int    m_voiceDelay = 0;    // FS2 +0x448: frames to its next word (updateSounds)
    u_char m_voiceOn = 0;       // FS2 +0x44c: a script's property; 1 from initSounds: it says things by itself
    float  m_slowStart = 0.0f;  // FS2 +0x458: -1 none; a timed change of the motion rate
    float  m_slowEnd = 0.0f;    // FS2 +0x45c
    float  m_slowFrom = 0.0f;   // FS2 +0x460: the rate at the start
    float  m_slowTo = 0.0f;     // FS2 +0x464: at the end
    // FS2 +0x464: the terminate hook keeps "it is in motion 0xd6" in the low
    // byte of m_slowTo; PC: a byte of its own.
    u_char m_slowInDrain = 0;
    StageSideMask m_sides;      // FS2 +0x590: the sheet's StageObjSide bits: the sides it fights (getSides)
    float  m_health = 0.0f;     // FS2 +0x594: the sheet's 体力: running spends a point a frame at 30 Hz, 5% of the sheet's a second comes back (PlayerMotion::stepHealth)
    int    m_endurance = 0;     // FS2 +0x598: the sheet's 耐久力; setEndurance
    int    m_leftArm = 0;       // FS2 +0x59c: the sheet's 左腕
    int    m_rightArm = 0;      // FS2 +0x5a0: the sheet's 右腕; setRightArm
    int    m_rightLeg = 0;      // FS2 +0x5a4: the sheet's 右足
    int    m_leftLeg = 0;       // FS2 +0x5a8: the sheet's 左足
    int    m_flinch = 0;        // FS2 +0x5ac: setFlinch, to the sheet's ひるみ耐性値
    int    m_unarmedMsp = 0;    // FS2 +0x5b0: the sheet's 素手MSP
    int    m_5b4 = 0;           // FS2 +0x5b4: only a script's
    u_char m_shrugsHits = 0;    // FS2 +0x5b8: a script's: hits take endurance and it plays no reaction
    u_char m_biteProof = 0;     // FS2 +0x5b9: it is not bitten and nothing hits it, whatever its brain
    u_char m_5ba = 0;           // FS2 +0x5ba: a script's; 0x226270 reads it
    int    m_aiLevel = 0;       // FS2 +0x5bc: the sheet's AILV
    float  m_sight = 0.0f;      // FS2 +0x5c0: 視界
    float  m_hearing = 0.0f;    // FS2 +0x5c4: 音感 / 100
    int    m_enduranceRegen = 0; // FS2 +0x5c8: 回復: a second, sideHook0
    int    m_lightResist = 0;   // FS2 +0x5cc: 光耐性, 0..2
    int    m_lightDamage = 0;   // FS2 +0x5d0: 光ﾀﾞﾒｰｼﾞ
    int    m_lightFlinch = 0;   // FS2 +0x5d4: 光怯み
    int    m_lifeOverride = -1; // FS2 +0x690: getLifeState: >= 0 says 6, or 0 says 7 (writer unread)
    int    m_unarmedAttack = 0;      // FS2 +0x5d8: 素手攻撃: a blow, and what a forcing takes off a door
    int    m_biteDamage = 0;    // FS2 +0x5dc: かみつき: a bite's
    float  m_walkRate = 1.0f;   // FS2 +0x5e0: 移動速度 / 100
    float  m_runRate = 1.0f;    // FS2 +0x5e4: 走り速度 / 100
    float  m_turnRate = 1.0f;   // FS2 +0x5e8: 旋回速度 / 100
    float  m_reviveTime = 0.0f; // FS2 +0x5ec: seconds of DamageDead
    int    m_drainRate = 0;     // FS2 +0x5f0: 操り耐性: what a drained character loses a frame at 30 Hz, read from whoever the player controls
    float  m_trapResist = 0.0f; // FS2 +0x5f4: 罠耐性 / 30
    u_char m_blanks = 0;        // FS2 +0x5f8: 射撃 is 999: its shots carry no bullet
    float  m_shotScatter = 0.0f; // FS2 +0x5fc: 射撃 / 100: ShotSpread::amount; 0: a bullet steers after the aim, else straight
    float  m_shotInterval = 0.0f;   // FS2 +0x600: 射撃間隔 / 30
    float  m_attackInterval = 0.0f; // FS2 +0x604: 攻撃間隔 / 30
    float  m_biteInterval = 0.0f;   // FS2 +0x608: 噛みつき間隔 / 30
    float  m_reloadRate = 0.0f;     // FS2 +0x60c: リロード / 100
    u_char m_610 = 0;           // FS2 +0x610: cleared by afterScripts; no reader found
    int    m_lastDoorMode = 0;  // FS2 +0x614: afterScripts: StageDoor::m_doorMode of m_doorInUse; no reader found
    StageDoor *m_doorInUse = nullptr; // FS2 +0x618: afterScripts: m_door while its m_user is this; no reader found
    StageObject *m_hitThisFrameBy = nullptr; // FS2 +0x61c: cleared by afterScripts, 0x20eb40 writes it; native 0x90 reads its number
    StageObject *m_lastHitBy = nullptr; // FS2 +0x620: who hit it last (0x20e770, 0x20eb40); the game over's cause
    StageObject *m_biteCandidate = nullptr; // FS2 +0x624: natives 0xbc, 0xbd write it
    StageObject *m_628 = nullptr; // FS2 +0x628: native 0x207 asks its number; cleared by afterScripts, the route's tasks write it (0x228670, 0x229f60, 0x22aee0, 0x22b580)
    int    m_62c = 0;           // FS2 +0x62c: 0..9 from a script; the hits 0x20e770, 0x20eb40 write it, 0x1ce3f0 reads it
    int    m_scriptValues[11] = {}; // FS2 +0x630: getScriptValue
    StageObject *m_heardFrom = nullptr; // FS2 +0x65c: whose noise it heard this frame; read by 0x21b890
    float  m_lastNoiseRange = 0.0f; // FS2 +0x660: m_noiseRange of the frame before; read by 0x219980
    float  m_noiseRange = 0.0f; // FS2 +0x664: FLT_MAX each frame; leaveNoticed lowers it on the corpses around
    StageObject *m_nearSearchPoint = nullptr; // FS2 +0x668: getNearSearchPoint's last
    int    m_spawnPoint = -1;   // FS2 +0x66c: missionSpawnAt's, -1 from the status block
    int    m_670 = 0;           // FS2 +0x670: 0 from the block; only the property reads it here
    float  m_pathLength = 0.0f; // FS2 +0x678: measurePath's last
    StageThrownItem *m_seenFlying = nullptr;     // FS2 +0x67c: the flying thrown item in view
    StageThrownItem *m_followedFlying = nullptr; // FS2 +0x680: the one m_point740 / m_point760 are about
    StageThrownItem *m_seenThrown = nullptr;     // FS2 +0x684: the last one in view lying still
    u_char m_bulletFlying = 0;  // FS2 +0x688: set each frame by a bullet of its own
    u_char m_hitThisFrame = 0;  // FS2 +0x689: PlayerMotion::takeHits sets it; afterScripts clears it
    int    m_68c = 0;           // FS2 +0x68c: 10 when the player hits it (takeDamage); 0 or less, the brain's state 5 may sound the alarm
    sceVu0FVECTOR m_respawnPosition = {}; // FS2 +0x6a0: 0x218610 sets it with w 1; spawn clears it
    sceVu0FVECTOR m_respawnRotation = {}; // FS2 +0x6b0
    int    m_meleeScale = 0;    // FS2 +0x6c0: 打撃攻撃力修正値
    int    m_shotScale = 0;     // FS2 +0x6c4: 射撃攻撃力修正値
    int    m_6c8 = 0;           // FS2 +0x6c8: 0 from the block, then a script's
    int    m_6cc = 0;           // FS2 +0x6cc: the same; 0x228670 reads it
    int    m_6d0 = 0;           // FS2 +0x6d0
    int    m_6d4 = 0;           // FS2 +0x6d4
    ScriptPoint m_point6e0;     // FS2 +0x6e0: stepStatus: where the noise it heard came from
    ScriptPoint m_point700;     // FS2 +0x700: where 0x21bfd0 found it, once
    ScriptPoint m_scriptPoint;  // FS2 +0x720: a script's (properties, native 0x94) or the brain's (0x21ab60, 0x219040)
    ScriptPoint m_point740;     // FS2 +0x740: beforeScripts: the flying item's throw vector (+0x450)
    ScriptPoint m_point760;     // FS2 +0x760: beforeScripts: where the followed item came down
    ScriptPoint m_point780;     // FS2 +0x780: beforeScripts: where the item lying still was
    u_char m_placedThisFrame = 0; // FS2 +0x7a0: setPosition and the placing set it, beforeScripts and resetActionState clear it; the brain waits a frame (0x2184a0)
    // PC: "not asked yet" is the ROM's 1 as a pointer (foeNotAsked).
    StageObject *m_foeCache[FoeCache_Count] = {}; // FS2 +0x7a4: 1 each frame, "not asked yet": getBestFoe and the other sight answers, unwritten
    int    m_nearestFoeDistance = -1; // FS2 +0x7b4: -1 each frame or with no foe (pickNearestFoe)
    float  m_visibility = 0.0f; // FS2 +0x7b8: 0 unseen .. 1 plain to see: scales what canSee reaches
    float  m_areaDark = 0.0f;   // FS2 +0x7bc: the darkness that gave it; 0x1adaa0 reads it (a native, unread)
    StageVolume *m_area = nullptr; // FS2 +0x7c0: the VISIBILITY_ volume it stands in
    int    m_placeMotion = 0;   // FS2 +0x7c4: the motion setPosition starts it in, once; native 0x21a (setMotionByLabel)
    CharacterBlock7c8 m_block7c8 = {}; // FS2 +0x7c8
};

// What a character's kind 0xb / 0xd hides: the model's part "hide" (EE 4;
// StageCharacter +0x204, SubSceneCharacter +0x240).
// layout: host
struct CharacterHidePart
{
    CharacterHidePart(StageObject *owner, int kind);                     // FS2 0x0017e0b0
    ~CharacterHidePart();                                                // FS2 0x0017e050
    // Shows what other shows.
    void copyFrom(const CharacterHidePart *other);                       // FS2 0x0017e010

    PartSwitch *m_switch;       // FS2 +0x0
};

// The script's number of a player spawn name (P_SPW%02d); -1 not one.
int parseSpawnId(const char *name);                                      // FS2 0x0029fb30

class StageObjectList;          // game/stageobjlist.h
struct SceneRender;
// +0xa0: the grain over a frame: the init pack's image as a grid of 64-pixel
// tiles, each from a random place and turn of it (vtable 0x311280 over
// StageFade's base 0x311260, vptr +4). The party's fill is one too.
// layout: host (EE 0x24).
class ScreenFill
{
public:
    ScreenFill(u_int rgba, int x, int y, int w, int h);                  // FS2 0x00151f20
    virtual ~ScreenFill();                                               // FS2 0x00151e90
    // A new Tim2Texture of image at +8, the old one deleted.
    void setImage(const void *image);                                    // FS2 0x00151e10
    void destroy();                                                      // FS2 vtable +0x8 (vptr +4)
    virtual void draw(sceVif1Packet *packet, TexAreaList *texList);      // FS2 0x00151de0: slot +0xc
    // The tiles, in rgba (0x80 in the top byte is opaque); nothing until an
    // image is set.
    void drawGrain(sceVif1Packet *packet, TexAreaList *texList, u_int rgba); // FS2 0x001518a0

protected:
    Tim2Texture *m_texture = nullptr; // FS2 +0x8
    u_int  m_rgba;              // FS2 +0xc
    int    m_x;                 // FS2 +0x10
    int    m_y;                 // FS2 +0x14
    int    m_w;                 // FS2 +0x18
    int    m_h;                 // FS2 +0x1c
    u_char m_hasImage = 0;      // FS2 +0x20
};

// +0xa4: SkyMistHolder is game/skymist.h.

// gsFrameFilter 0x2849c0, the frame filter of the view cameras and event
// d0101: game/framefilter.h.
// A flat sprite from (x, y) to (x2, y2) of the 512 x 512 frame.
void gsFillRect(sceVif1Packet *packet, int x, int y, int x2, int y2, u_int rgba); // FS2 0x002eaeb0

// +0xc0: (count 4, 0x800, 0x878). layout: host (EE 0x2c).
class WideFontPrinter;          // game/fontdata.h
struct ScenePool
{
    static constexpr int kAllSlots = -1;

    ScenePool(u_int count, int size, int size2);                         // FS2 0x00289be0
    ~ScenePool();                                                        // FS2 0x00289b60: deleting
    // Text for slot index; -1 past the last.
    int  setSlot(int index, const u_short *text);                        // FS2 0x00289b20
    // The colour of a slot, of all with kAllSlots.
    void setColour(u_int rgba, int index);                               // FS2 0x002896a0
    void setImage(const void *image);                                    // FS2 0x002894b0
    void setPrinter(WideFontPrinter *printer);                           // FS2 0x00289b50
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x00289800
    void drawStrip(sceVif1Packet *packet);                               // FS2 0x00288c00

    int              m_count;    // FS2 +0x14
    int              m_originX;  // FS2 +0x20: where the printer's lines start, GS window
    int              m_originY;  // FS2 +0x24
    GsTexture       *m_image;    // FS2 +0x10: the strip's cells
    int             *m_widths;   // FS2 +0x00: each line of the frame, pixels
    WideFontPrinter *m_printer;  // FS2 +0x18
    const u_short  **m_slots;    // FS2 +0x1c
    u_int           *m_colours;  // FS2 +0x28
};

constexpr int kNoticeLines    = 2;
constexpr int kNoticeLineSize = 0x40;

// A notice waiting for its kind's entry; free with a null text.
// layout: host (EE 0x18)
struct PauseNotice
{
    int            kind = 0;     // FS2 +0x00: PauseMenuState::NoticeKind
    u_int          serial = 0;   // FS2 +0x04: the oldest shows first
    int            frames = 0;   // FS2 +0x08: how long it shows, of the game's 30 a second
    int            delay = 0;    // FS2 +0x0c: before it shows
    int            hold = 0;     // FS2 +0x10: not dismissed before this
    const u_short *text = nullptr; // FS2 +0x14
};

enum NoticeState
{
    NoticeState_Waiting = 0,     // before its start
    NoticeState_Showing = 1,
    NoticeState_Over    = 2,     // free
};

// What a kind shows now.
// layout: host (EE 0x114)
struct NoticeEntry
{
    // Takes the notice from now, its text cut at the control characters.
    void show(float now, const PauseNotice &notice);                     // FS2 0x00207110
    // Past hold, with an end to come after it.
    bool canDismiss(float now) const
    {
        return state == NoticeState_Showing && end > hold && now > hold;
    }
    int stateAt(float now) const
    {
        if (now < start)
            return NoticeState_Waiting;
        return dismissed != 0 || end < now ? NoticeState_Over : NoticeState_Showing;
    }

    u_short lines[kNoticeLines][kNoticeLineSize] = {}; // FS2 +0x000
    float   start = 0.0f;        // FS2 +0x100: seconds of the scene's clock
    float   end = 0.0f;          // FS2 +0x104
    float   hold = 0.0f;         // FS2 +0x108
    u_char  dismissed = 0;       // FS2 +0x10c
    u_char  pinned = 0;          // FS2 +0x10d: stays until a script clears it (native 0x1aa)
    int     state = NoticeState_Over; // FS2 +0x110
};

// +0xc4: the stage's notices (game/pausemenustate.cpp). layout: host (EE 0x678).
struct PauseMenuState
{
    enum NoticeKind
    {
        NoticeKind_Purpose = 0,  // shown, it becomes the mission's purpose
        NoticeKind_Message = 1,
        NoticeKind_Hint    = 2,
        NoticeKind_Event   = 3,  // Message while an event runs; the only one shown then
    };
    static constexpr int kNoticeKinds = 5;
    static constexpr int kNoticeSlots = 11;

    PauseMenuState();                                                    // FS2 0x00207ef0
    ~PauseMenuState();                                                   // FS2 0x00207ea0: deleting
    void update();                                                       // FS2 0x00207500
    // An entry can be dismissed: the notice holds the input.
    bool isActive();                                                     // FS2 0x002078d0
    // The event's entry off.
    void clearList();                                                    // FS2 0x00207440
    bool clearPinned();                                                  // FS2 0x00207490
    // A notice for the list: true when a free entry of the 11 took it and
    // no number was negative.
    bool addNotice(const u_short *text, int frames, int arg, int level); // FS2 0x00207aa0
    // The same by kind, frames 0 for ever.
    bool addPurpose(const u_short *text, int frames, int delay);         // FS2 0x00207970
    bool addMessage(const u_short *text, int frames, int delay);         // FS2 0x00207bc0
    bool addHint(const u_short *text, int frames, int delay);            // FS2 0x00207cf0
    // A notice of that kind waits, or that kind's entry shows.
    bool hasNotice(int kind);                                            // FS2 0x00207e20

    bool         m_showing = false;  // FS2 +0x000
    u_int        m_serial = 0;       // FS2 +0x004
    PauseNotice  m_notices[kNoticeSlots]; // FS2 +0x008
    NoticeEntry  m_entries[kNoticeKinds]; // FS2 +0x110
    u_int        m_colour = 0x80606060;   // FS2 +0x674

private:
    PauseNotice *freeNotice();
    bool post(int kind, const u_short *text, int frames, int delay, int hold);
};

class WideFontPrinter;          // game/fontdata.h

// +0xcc: a list view over the scene's printer (vtable 0x316530 at +0x88).
// layout: host (EE 0x8c).
// The prompt of the Use button: a label over the scene's printer, on a bar
// from the init pack's sheet. layout: host -- EE 0x38.
class UiPrompt
{
public:
    UiPrompt(WideFontPrinter *printer, int arg);                         // FS2 0x00208c70
    ~UiPrompt();                                                         // FS2 0x00208c00
    // The label to show next; false while a press is playing.
    bool setLabel(const void *text, int value);                          // FS2 0x00208bb0
    // Plays the press; answers the label pressed.
    const void *press();                                                 // FS2 0x00208330
    void setSheet(void *sheet);                                          // FS2 0x00208380
    void endFrame();                                                     // FS2 0x00208be0
    // A press that has played lets the prompt go.
    void stepPress();                                                    // FS2 0x00208cc0
    // The fade of the moment, the button's picture and the label, centred
    // at the top.
    void draw(sceVif1Packet *packet);                                    // FS2 0x00208490
    void drawButton(sceVif1Packet *packet, const struct TimSheetSprite *sprite, u_int rgba, int x, int y);

    enum State { State_Idle = 0, State_Pressed = 1 };

    int    state = State_Idle;  // FS2 +0x00
    WideFontPrinter *printer;   // FS2 +0x04
    int    arg;                 // FS2 +0x08
    const void *label = nullptr; // FS2 +0x0c
    const void *shown = nullptr; // FS2 +0x10
    int    value = -1;          // FS2 +0x14
    int    lastValue = -1;      // FS2 +0x18
    void  *sheet = nullptr;     // FS2 +0x1c
    Tim2Texture *texture = nullptr; // FS2 +0x20: the picture of the sheet's first sprite
    struct TimSheet *buttons = nullptr; // FS2 +0x24: sheet, when it is a TimM sheet
    const struct UiPromptFade *fade = nullptr; // FS2 +0x28: 0x2fa7b0 open, 0x2fa7c0 press, 0x2fa7c8 close
    int    animFrame = 0;       // FS2 +0x2c
    int    animKind = 0;        // FS2 +0x30: UiPromptAnim
    u_char canPress = 1;        // FS2 +0x34: the scene's input is on the prompt
};

// What the Use button does at the player's feet (CharacterAllows); kinds
// from 7 up are the scripts' own (UiItemList::post).
enum UiUseKind
{
    UiUse_LevelUp   = 0,
    UiUse_LevelDown = 1,
    UiUse_Ladder    = 2,
    UiUse_Pickup    = 5,        // CharacterAllows::pickup
    UiUse_Jump      = 6,
    UiUse_Script    = 7,
};

// One of UiItemList's two slots (front and back), rebuilt each frame.
// layout: host -- EE 0x14.
struct UiPickSlot
{
    // object set and timer past 6.
    bool isReady();                                                      // FS2 0x0023e740
    void clear();

    void  *object;              // FS2 +0x00: who the prompt is for
    int    kind;                // FS2 +0x04: UiUseKind
    int    priority;            // FS2 +0x08
    const void *label;          // FS2 +0x0c
    u_char enabled;             // FS2 +0x10
};
// +0xcc: the Use button's list (vtable 0x316530 at +0x88). layout: host (EE 0x8c).
class UiItemList
{
public:
    static constexpr int kKinds = 7;
    static constexpr int kNoPriority = (int)0x80000000;

    UiItemList(WideFontPrinter *printer, int arg);                       // FS2 0x0023f0b0
    void destroy();                                                      // FS2 vtable +0x8 (vptr +0x88)
    void update();                                                       // FS2 0x0023e7b0
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x0023e770
    // The front slot is used; negative when nothing could be.
    int  pick();                                                         // FS2 0x0023ee90
    // Whether a press of Use goes to who with the slot of this kind (the
    // slot's user is told when it does).
    u_char offer(StageCharacter *who, int kind);                         // FS2 0x0023ed10
    // A script's entry: scriptKind 0 is UiUse_Script. False: one with a
    // higher priority stands this frame.
    bool post(void *object, int scriptKind, const void *label, int priority, bool enabled); // FS2 0x0023ef40
    // The natives: a kind on or off, its label and priority.
    void enableKind(int kind, bool enabled);                             // FS2 0x0023efb0
    void setKind(int kind, const void *label, int priority);             // FS2 0x0023eff0
    // The init pack's sheet (the ROM asks its size too, and drops it).
    void setSheet(void *sheet);                                          // FS2 0x0023f030

    // The slot in front (flip's bit 0 picks it; update fills the other).
    UiPickSlot *frontSlot() { return &slots[flip & 1]; }

    // layout: host -- EE 0xc.
    struct KindRow
    {
        u_char      enabled;    // FS2 +0x0
        int         priority;   // FS2 +0x4
        const void *label;      // FS2 +0x8
    };
private:
    void takeKind(UiPickSlot *slot, StageCharacter *who, int kind);
public:
    UiPrompt  *prompt;          // FS2 +0x00
    KindRow    kinds[kKinds];   // FS2 +0x04 (0xc each)
    UiPickSlot slots[2];        // FS2 +0x58 (0x14 each)
    u_int      flip;            // FS2 +0x80
    int        pendingCount;    // FS2 +0x84: the script's index just picked, StageScene::getMenuMode
};

// +0xd0: a menu over the scene's printer (vtable 0x3164b0 at +8; a 0xaa8
// object at +0xc). StageScene::getOpenMenu reads its state.
// layout: host -- EE 0x18: +0 state.
enum StageMenuState
{
    StageMenu_Closed  = 0,
    StageMenu_Opening = 1,
    StageMenu_Open    = 2,      // getOpenMenu's test
    StageMenu_Closing = 3,
};
// StageMenu::request.
enum StageMenuRequest
{
    StageMenuRequest_None  = 0,
    StageMenuRequest_Open  = 1,
    StageMenuRequest_Close = 2,
};
class StageMenuList;            // game/stagemenu.h
// The stage's own menu: a state machine around its list window (game/stagemenu.cpp).
// layout: host (EE 0x18; vptr at +8).
class StageMenu
{
public:
    StageMenu(WideFontPrinter *printer, int arg);                        // FS2 0x002308a0
    void destroy();                                                      // FS2 0x00230820: slot +0x8, deleting
    void update();                                                       // FS2 0x00230400: slot +0xc
    void draw(sceVif1Packet *packet, TexAreaList *texList);              // FS2 0x002303c0: slot +0x10
    // A line of the list window (0x209d90): the line with that text again,
    // else a free one. Lower orders are higher up.
    void addLine(const u_short *text, int order, bool enabled);          // FS2 0x00230800
    void setData(void *data);                                            // FS2 0x00230810

    int    state = StageMenu_Closed;    // FS2 +0x0: StageMenuState
    int    request = StageMenuRequest_None; // FS2 +0x4: read by update
    StageMenuList *list = nullptr;      // FS2 +0xc: game/stagemenu.h
    const u_short *pending = nullptr;   // FS2 +0x10: the picked line's text (isMenuPick, getMenuMode)
    u_char pendingOn = 0;               // FS2 +0x14: for one frame, update clears it
};

// +0xdc: the effects scripts started, by a handle of their own; an entry frees
// itself when the tree has let its effect go. layout: host (EE 0xc4).
struct SceneEffectList
{
    SceneEffectList();                                                   // FS2 0x0023ad60
    ~SceneEffectList();                                                  // FS2 0x0023ad10: deleting
    void update();                                                       // FS2 0x0023aca0
    // A new effect under parent, at pos / rot (either null: the definition's);
    // its handle, -1 with no free entry or no such name.
    int  spawn(const char *name, StageNodeBase *parent, const float *pos, const float *rot); // FS2 0x0023aad0
    // The effect behind a handle, null when it is gone.
    StageParticle *find(int handle);                                     // FS2 0x0023a910
    // Stops it on the next frame; false when it is gone.
    bool stop(int handle);                                               // FS2 0x0023a9f0
    // setEmitting on the stage's rain (a name of s??rain).
    void setRainEmitting(bool on);                                       // FS2 0x0023a870

    // layout: host
    struct Entry
    {
        u_char         isRain = 0;       // FS2 +0x0
        int            handle = 0;       // FS2 +0x4
        StageParticle *effect = nullptr; // FS2 +0x8: null free
    };
    static constexpr int kEntries = 16;

    int   m_lastHandle = 0;        // FS2 +0x0
    Entry m_entries[kEntries];     // FS2 +0x4
};

// What a noise says of who may hear it. A hearer's stepStatus turns it into
// a bit of m_scriptValues[kScriptValueHeard].
enum SceneNoiseKind
{
    SceneNoise_Sound = 0,       // a played sound: heard by chance, the loudest's place kept
    SceneNoise_Any   = 1,       // by everyone in reach
    SceneNoise_Ally  = 2,       // by its allies
    SceneNoise_Foe   = 3,       // by the others
};
// m_scriptValues[10] holds the frame's hearing bits, m_scriptValues being
// the scripts' window on +0x630..+0x65c. Natives 0xe2, 0x93, 0xa9 read them.
constexpr int kScriptValueHeard = 10;
constexpr int kHeardSound = 2;
constexpr int kHeardAny   = 4;
constexpr int kHeardAlly  = 8;
constexpr int kHeardFoe   = 0x10;
constexpr u_int kSceneNoises     = 0x20;    // more in a frame are lost
constexpr u_int kSceneSoundWatch = 0x10;
constexpr int   kSceneWatchDelay = 0xf;     // frames between a watched sound's noises

// layout: host -- EE 0xc.
struct SceneNoise
{
    StageObject *source;        // FS2 +0x0
    float        loudness;      // FS2 +0x4: how far it reaches
    int          kind;          // FS2 +0x8: SceneNoiseKind

    // Where it comes from: a character's head, or the object's position.
    const float *place() const;                                          // FS2 0x002431e0
    bool isFromAlly(StageObject *hearer) const;                          // FS2 0x002432a0
};
// A sound that stays audible while it plays (SeEntry::resumable).
// layout: host -- EE 0xc.
struct SceneSoundWatch
{
    StageObject      *object;   // FS2 +0x0: null a free entry
    StageSoundSource *source;   // FS2 +0x4
    int               delay;    // FS2 +0x8: frames to its next noise
};

// +0xe0: the frame's noises, in two lists: stepStatus reads the front one,
// what is made this frame goes to the back one. layout: host (EE 0x3cc).
struct SceneNoiseList
{
    SceneNoiseList();                                                    // FS2 0x00243150
    ~SceneNoiseList();                                                   // FS2 0x00243100: deleting
    // The watched sounds make their noises; then the lists flip.
    void pollSoundSources();                                             // FS2 0x00242e80
    // A noise of an object into the back list.
    void addNoise(float loudness, StageObject *object, int kind);        // FS2 0x002430a0
    // The same from a playing source; a resumable one is watched as well.
    void addSourceNoise(StageObject *object, StageSoundSource *source);  // FS2 0x00242fc0

    // layout: host -- EE 0x184.
    struct List
    {
        SceneNoise noises[kSceneNoises]; // FS2 +0x0
        u_int      count;                // FS2 +0x180
    };
    const List &front() const { return m_lists[m_front]; }

private:
    List &back() { return m_lists[(m_front + 1) & 1]; }

    List            m_lists[2];          // FS2 +0x0
    u_int           m_front;             // FS2 +0x308
    SceneSoundWatch m_watches[kSceneSoundWatch]; // FS2 +0x30c
};

// +0xe4: Rand48, game/rand48.h.

// +0xf4: StageAlert, game/stagealert.h.

// +0x11c: the stage-clear sequence (+0x118, the game over: game/gameoversequence.h).
// Its state reads as done once the scene's fade is full in mode 2.
enum
{
    kClearSequenceStart   = 0,
    kClearSequenceFading  = 1,
    kClearSequenceDone    = 2,  // StageScene::isCleared
};
// The stage's end, from native 0x52 (game/demoevent.cpp). layout: host.
struct ClearSequence
{
    ClearSequence();                                                     // FS2 0x002b1660
    int getState();                                                      // FS2 0x002b1620
    void update();                                                       // FS2 0x002b1500

    int m_state;                // FS2 +0x0
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

// What runs a sub-scene's script (StageScene +0x8, from enterSubScene). The
// ROM names the class itself: slot 7 answers "DemoExecuter" (the stand-in
// was SubSceneObject). It overrides nothing else written so far.
// layout: host (unread).
class DemoExecuter : public ScriptRunner
{
public:
    // vtable 0x314d30. flag: +0xb9; "skippable" is a guess.
    explicit DemoExecuter(bool skippable);                               // FS2 0x0026c8c0
    bool m_b8 = false;          // FS2 +0xb8 (writers unread)
    bool m_skippable;           // FS2 +0xb9
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
    // A file of the pack: name + .mdl, name + .mtn; null none.
    void *findModel(const char *name);                                  // FS2 0x0026c700
    void *findMotion(const char *name);                                  // FS2 0x0026c7c0

    void *pack;                 // FS2 +0x0
};
// Goes over a relocated script's text tables (magic LBL, STR, SST); read
// only by its shape.

// StageScene (the current stage object) and StageFiles (the stage's
// files): game/stagescene.h.
#include "game/stagescene.h"

// The navigator (game/navimenu.h). A step of the route: the node, and which
// of its links back it was reached by (-1 neither). layout: EE 8 bytes, also
// in SaveExtra.
struct NaviStep
{
    int node;                   // FS2 +0x0
    int side;                   // FS2 +0x4
};
static_assert(sizeof(NaviStep) == 8, "NaviStep: EE 8");

constexpr int kNaviNodes = 0x45;
constexpr int kNaviRows = 0x34;
constexpr int kNaviRouteSteps = 0x20;
constexpr int kNaviPending = 4;

// NaviMenu::m_nodeFlags.
enum NaviNodeFlag
{
    NaviNode_Open          = 0x01,
    NaviNode_FirstCleared  = 0x02,
    NaviNode_SecondCleared = 0x04,
    NaviNode_FirstGoal     = 0x08,
    NaviNode_SecondGoal    = 0x10,
    NaviNode_HereFirst     = 0x20,  // the stage running now, its first part
    NaviNode_HereSecond    = 0x40,
    NaviNode_SinglePart    = 0x80,
};

// NaviMenu::m_bits.
enum NaviBit
{
    NaviBit_Link0  = 0x01,
    NaviBit_Link1  = 0x02,
    NaviBit_Link2  = 0x04,
    NaviBit_Link3  = 0x08,
    NaviBit_Shown0 = 0x10,      // ... and the layout's tab shows it
    NaviBit_Shown1 = 0x20,
    NaviBit_Shown2 = 0x40,
    NaviBit_Shown3 = 0x80,
};

// The navi screen's data, the part of PrgScaleNavi a layout reads by index
// (getGameValue). layout: EE, the widget's +0x2a8..+0x628; the offsets noted
// are the widget's.
struct NaviMenu
{
    int stage() const { return m_node; }

    int      m_nodeKind[kNaviNodes];   // FS2 +0x2a8: GameValue_NaviValue: PrgScaleNavi::nodeKindOf
    u_short  m_nodeFlags[kNaviNodes];  // FS2 +0x3bc: NaviNodeFlag
    u_char   _446[2];
    int      m_cursorX;         // FS2 +0x448
    int      m_cursorY;         // FS2 +0x44c
    int      m_linkX;           // FS2 +0x450
    int      m_linkY;           // FS2 +0x454
    int      m_linkNode;        // FS2 +0x458
    int      m_linkSecond;      // FS2 +0x45c
    int      m_savedX;          // FS2 +0x460: where the overview was opened
    int      m_savedY;          // FS2 +0x464
    int      m_savedNode;       // FS2 +0x468: -1 while it is not open
    int      m_node;            // FS2 +0x46c: under the cursor, -1 none
    float    m_markA[2];        // FS2 +0x470
    float    m_markB[2];        // FS2 +0x478
    int      m_routeCount;      // FS2 +0x480
    NaviStep m_route[kNaviRouteSteps]; // FS2 +0x484
    u_char   m_bits[kNaviNodes];       // FS2 +0x584: GameValue_NaviBits: NaviBit
    u_char   _5c9[3];
    int      m_pendingCount;    // FS2 +0x5cc
    int      m_pending[kNaviPending];  // FS2 +0x5d0: the nodes a link would open
    u_char   m_columnShown[0x10];      // FS2 +0x5e0: GameValue_NaviFlagE0
    u_char   m_rowShown[kNaviRows];    // FS2 +0x5f0: GameValue_NaviFlagF0
    int      m_lastTab;         // FS2 +0x624
};
static_assert(offsetof(NaviMenu, m_nodeFlags) == 0x3bc - 0x2a8, "NaviMenu::m_nodeFlags");
static_assert(offsetof(NaviMenu, m_node) == 0x46c - 0x2a8, "NaviMenu::m_node");
static_assert(offsetof(NaviMenu, m_bits) == 0x584 - 0x2a8, "NaviMenu::m_bits");
static_assert(offsetof(NaviMenu, m_columnShown) == 0x5e0 - 0x2a8, "NaviMenu::m_columnShown");
static_assert(offsetof(NaviMenu, m_rowShown) == 0x5f0 - 0x2a8, "NaviMenu::m_rowShown");
static_assert(sizeof(NaviMenu) == 0x628 - 0x2a8, "NaviMenu: to EE 0x628");
NaviMenu *getNaviMenu();                                                 // FS2 0x0029efe0


// The memory-card menu's save manager (sbss 0x31ae94, gp-0x79dc; owned by
// PrgMcCard, game/prgmccard.h, null while there is none): the S2D's
// rMcTotalTime0..3 show its saves' play times. Data the build must define.
class Memcard;                  // game/memcard.h
extern Memcard *gMenuMemcard;

// The stage's pause menu and map menu are game/pausemenu.h.

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
constexpr int kStageObjRecordCarProof = 3;  // a car neither hurts it nor is hurt by it
constexpr int kStageObjRecordStartMotion = 1; // 初期M before the sheet's column is read (init)
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
    int    startMotion;        // FS2 +0x40: 初期M: the brain's first state (initStatusBlock)
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

    // The one default a row gets before its columns: startMotion.
    void init();                                                         // FS2 0x00214a70
};
static_assert(sizeof(StageObjRecord) == 0xfc, "StageObjRecord: EE 0xfc");
extern StageObjRecord gStageObjRecords[kStageObjRecordSets][kStageObjRecordCount]; /* bss 0x539b10 */
// The record at index of the stage kind's set; the first with a bad index.
const StageObjRecord *stageObjRecord(int index);                         // inline in the ROM (game/stagescene.cpp)
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
    ItemKind_StackLast    = 10,     // 7..10 stack by name (StageCharacter::giveItem)
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
inline float StageCharacter::getWeaponRange() const
{
    if (m_weapons[0] == nullptr)
        return 0.0f;
    const int index = m_weapons[0]->m_weaponDef;
    return (float)gWeaponDefs[index < 0 || index >= kWeaponDefCount ? 0 : index].range;
}

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
