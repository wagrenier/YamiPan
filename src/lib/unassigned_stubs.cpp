// lib/unassigned_stubs.cpp -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// Functions below are stubs until reconstructed; see tools/reverse/stage.py.
#include "rena/rena_draw.h"

#include <string.h>

#include "common/yamipan_stub.h"
#include "game/characterbrain.h"
#include "game/characterroute.h"
#include "game/fontdata.h"
#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/memcard.h"
#include "game/modelpose.h"
#include "game/playermotion.h"
#include "game/stagenode.h"
#include "game/stageobjlist.h"
#include "game/stageparticle.h"
#include "game/stageitem.h"
#include "game/stagescene.h"
#include "game/stringlist.h"
#include "game/uisequence.h"
#include "game/uiwidget.h"
#include "game/vu0math.h"

// ---- stubs (tools/reverse/stubgen.py) ----

// stub of FS2 0x002dccf0
AreaClip::AreaClip(void*)
{
    YAMIPAN_STUB("AreaClip::AreaClip", 0x002dccf0);
}

// stub of FS2 0x002dcab0
AreaClip::~AreaClip()
{
    YAMIPAN_STUB("AreaClip::~AreaClip", 0x002dcab0);
}

// stub of FS2 0x0012cce0
void GameSystem::beginStage()
{
    YAMIPAN_STUB("GameSystem::beginStage", 0x0012cce0);
}

// stub of FS2 0x002e14d0
GsRegionAlloc::GsRegionAlloc(int, int)
{
    YAMIPAN_STUB("GsRegionAlloc::GsRegionAlloc", 0x002e14d0);
}

// stub of FS2 0x002ad070
MenuProgHolder::~MenuProgHolder()
{
    YAMIPAN_STUB("MenuProgHolder::~MenuProgHolder", 0x002ad070);
}

// stub of FS2 0x002bb3e0
MenuProgSet::~MenuProgSet()
{
    YAMIPAN_STUB("MenuProgSet::~MenuProgSet", 0x002bb3e0);
}

// stub of FS2 0x00207ef0
// stub of FS2 0x00207ea0
// stub of FS2 0x002682e0
Rand48::Rand48()
{
    YAMIPAN_STUB("Rand48::Rand48", 0x002682e0);
}

// stub of FS2 0x002681e0
int Rand48::next(int n)
{
    YAMIPAN_STUB("Rand48::next", 0x002681e0);
    return {};
}

// stub of FS2 0x002ce5a0
SceneBufferSet::~SceneBufferSet()
{
    YAMIPAN_STUB("SceneBufferSet::~SceneBufferSet", 0x002ce5a0);
}

// stub of FS2 address unknown (1 candidates)
void SceneOwnedObject::destroy()
{
    YAMIPAN_STUB("SceneOwnedObject::destroy", 0x00000000);
}

// stub of FS2 0x002a7bf0
SigLamp::SigLamp(Tim2Texture*)
{
    YAMIPAN_STUB("SigLamp::SigLamp", 0x002a7bf0);
}

// stub of FS2 address unknown (1 candidates)
void StageMenu::destroy()
{
    YAMIPAN_STUB("StageMenu::destroy", 0x00230820);
}

// stub of FS2 0x001d3a40
// stub of FS2 0x00185af0

// stub of FS2 0x00151290
void debugSysInit()
{
    YAMIPAN_STUB("debugSysInit", 0x00151290);
}

// stub of FS2 0x00151280
void debugSysShutdown()
{
    YAMIPAN_STUB("debugSysShutdown", 0x00151280);
}

// stub of FS2 0x00151270
void debugSysUpdate()
{
    YAMIPAN_STUB("debugSysUpdate", 0x00151270);
}

// stub of FS2 0x0020afa0
int mapMenuResult()
{
    YAMIPAN_STUB("mapMenuResult", 0x0020afa0);
    return {};
}

// stub of FS2 0x0020afe0
void mapMenuStart()
{
    YAMIPAN_STUB("mapMenuStart", 0x0020afe0);
}

// stub of FS2 0x0020acb0
int pauseMenuResult()
{
    YAMIPAN_STUB("pauseMenuResult", 0x0020acb0);
    return {};
}

// stub of FS2 0x0020acf0
void pauseMenuStart()
{
    YAMIPAN_STUB("pauseMenuStart", 0x0020acf0);
}

// stub of FS2 0x002cf510
void resetStageGroupTable()
{
    YAMIPAN_STUB("resetStageGroupTable", 0x002cf510);
}

// stub of FS2 0x002e1440
GsRegionAlloc::~GsRegionAlloc()
{
    YAMIPAN_STUB("GsRegionAlloc::~GsRegionAlloc", 0x002e1440);
}

// stub of FS2 0x002c42c0
int runExtraMode(int arg)
{
    YAMIPAN_STUB("runExtraMode", 0x002c42c0);
    return {};
}

// stub of FS2 0x002acff0
int runHelpScreen(int page)
{
    YAMIPAN_STUB("runHelpScreen", 0x002acff0);
    return {};
}

// stub of FS2 0x002e0840
void GsRegion::release()
{
    YAMIPAN_STUB("GsRegion::release", 0x002e0840);
}

// stub of FS2 address unknown (0 candidates)
MenuProgHolder::MenuProgHolder(void const*, int)
{
    YAMIPAN_STUB("MenuProgHolder::MenuProgHolder", 0x00000000);
}

// stub of FS2 address unknown (0 candidates)

// stub of FS2 address unknown (1 candidates)
bool SceneOwnedObject::isBusy()
{
    YAMIPAN_STUB("SceneOwnedObject::isBusy", 0x00000000);
    return {};
}

// stub of FS2 address unknown (0 candidates)

// stub of FS2 0x00185210

// stub of FS2 address unknown (0 candidates)


// stub of FS2 0x001861b0

// stub of FS2 0x00186160

// stub of FS2 0x00185fb0

// stub of FS2 0x00185bb0

// stub of FS2 0x00185c40

// stub of FS2 address unknown (1 candidates)
void StageObject::onViewTop()
{
    YAMIPAN_STUB("StageObject::onViewTop", 0x00000000);
}

// stub of FS2 0x0029f050
// stub of FS2 address unknown (0 candidates)
// stub of FS2 address unknown (0 candidates)

// stub of FS2 0x002dcc30
void AreaClip::setup()
{
    YAMIPAN_STUB("AreaClip::setup", 0x002dcc30);
}

// stub of FS2 0x002dbd10
void AreaClip::update(StageObject *camera)
{
    YAMIPAN_STUB("AreaClip::update", 0x002dbd10);
}

// stub of FS2 0x00239bb0
// stub of FS2 0x002ad030
void MenuProgHolder::draw(float deltaSeconds, GamePacketManager *packets, StageObject *camera)
{
    YAMIPAN_STUB("MenuProgHolder::draw", 0x002ad030);
}

// stub of FS2 0x002078d0
// stub of FS2 0x00207500
// stub of FS2 0x002bb260
void MenuProgSet::draw(float zoom)
{
    YAMIPAN_STUB("MenuProgSet::draw", 0x002bb260);
}

// stub of FS2 0x00268290
void Rand48::seed(u_int value)
{
    YAMIPAN_STUB("Rand48::seed", 0x00268290);
}

// stub of FS2 address unknown (1 candidates)
void SceneOwnedObject::draw()
{
    YAMIPAN_STUB("SceneOwnedObject::draw", 0x00000000);
}

// stub of FS2 address unknown (1 candidates)
StageObject * SceneOwnedObject::getTarget()
{
    YAMIPAN_STUB("SceneOwnedObject::getTarget", 0x00000000);
    return {};
}

// stub of FS2 address unknown (1 candidates)
bool SceneOwnedObject::hasTarget()
{
    YAMIPAN_STUB("SceneOwnedObject::hasTarget", 0x00000000);
    return {};
}

// stub of FS2 address unknown (1 candidates)
void SceneOwnedObject::prepare()
{
    YAMIPAN_STUB("SceneOwnedObject::prepare", 0x00000000);
}

// stub of FS2 address unknown (1 candidates)
void SceneOwnedObject::update()
{
    YAMIPAN_STUB("SceneOwnedObject::update", 0x00000000);
}

// stub of FS2 0x00289800
// stub of FS2 0x00289b20
// stub of FS2 address unknown (1 candidates)
// stub of FS2 address unknown (1 candidates)
void StageMenu::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    YAMIPAN_STUB("StageMenu::draw", 0x002303c0);
}

// stub of FS2 address unknown (1 candidates)
void StageMenu::update()
{
    YAMIPAN_STUB("StageMenu::update", 0x00230400);
}

// stub of FS2 address unknown (1 candidates)
void StageObject::cameraStep()
{
    YAMIPAN_STUB("StageObject::cameraStep", 0x00000000);
}

// stub of FS2 address unknown (1 candidates)
void StageObject::onLinkChanged()
{
    YAMIPAN_STUB("StageObject::onLinkChanged", 0x00000000);
}

// stub of FS2 0x0020b1d0
void StageCharacter::latchAltAction()
{
    YAMIPAN_STUB("StageCharacter::latchAltAction", 0x0020b1d0);
}

// stub of FS2 0x002849c0
void gsFrameFilter(sceVif1Packet *packet, const sceGsDrawEnv1 *env, int w, int h,
                   u_short block, int passes, int arg)
{
    YAMIPAN_STUB("gsFrameFilter", 0x002849c0);
}


// stub of FS2 0x002bb4e0
MenuProgSet::MenuProgSet()
{
    YAMIPAN_STUB("MenuProgSet::MenuProgSet", 0x002bb4e0);
}

// stub of FS2 0x002bb2b0
void MenuProgSet::load(void *image0, void *image1, void *image2, void *image3, void *image4,
              void *image5)
{
    YAMIPAN_STUB("MenuProgSet::load", 0x002bb2b0);
}

// stub of FS2 0x002cebd0
void SceneBlock98::attachTo(StagePlayer *player)
{
    YAMIPAN_STUB("SceneBlock98::attachTo", 0x002cebd0);
}

// stub of FS2 0x002ceb70
void SceneBlock98::setActive(bool on, SceneRender *render)
{
    YAMIPAN_STUB("SceneBlock98::setActive", 0x002ceb70);
}

// stub of FS2 0x002ce790
void SceneBlock98::updateVisible(SceneRender *render)
{
    YAMIPAN_STUB("SceneBlock98::updateVisible", 0x002ce790);
}

// stub of FS2 0x002ce3e0
void SceneBufferSet::attachCharacters(StageObjectList *objects)
{
    YAMIPAN_STUB("SceneBufferSet::attachCharacters", 0x002ce3e0);
}

// stub of FS2 0x002ce0c0
void SceneBufferSet::setActiveFor(StageObject *linked, SceneRender *render)
{
    YAMIPAN_STUB("SceneBufferSet::setActiveFor", 0x002ce0c0);
}

// stub of FS2 0x002ce180
void SceneBufferSet::storeActive(StageObject *linked)
{
    YAMIPAN_STUB("SceneBufferSet::storeActive", 0x002ce180);
}

// stub of FS2 0x002ce1c0
void SceneBufferSet::updateRange(float range, StageObject *linked, SceneRender *render)
{
    YAMIPAN_STUB("SceneBufferSet::updateRange", 0x002ce1c0);
}

// stub of FS2 0x00230810
void StageMenu::setData(void *data)
{
    YAMIPAN_STUB("StageMenu::setData", 0x00230810);
}

// stub of FS2 0x002ce9d0
SceneBlock98 * SceneBlock98::createFor(StageObject *owner, StageNodeTree *tree)
{
    YAMIPAN_STUB("SceneBlock98::createFor", 0x002ce9d0);
    return {};
}

// stub of FS2 0x002ce820
void SceneBlock98::updateFor(StageObject *object, StagePlayer *player)
{
    YAMIPAN_STUB("SceneBlock98::updateFor", 0x002ce820);
}

// stub of FS2 0x002ce360
void SceneBufferSet::addObject(StageObject *object)
{
    YAMIPAN_STUB("SceneBufferSet::addObject", 0x002ce360);
}

// stub of FS2 0x002ce620
SceneBufferSet * SceneBufferSet::createFor(StageObject *owner, StageNodeTree *tree)
{
    YAMIPAN_STUB("SceneBufferSet::createFor", 0x002ce620);
    return {};
}

// stub of FS2 0x001d3d20
// stub of FS2 0x0023db50
ItemPhysics::ItemPhysics(bool, bool)
{
    YAMIPAN_STUB("ItemPhysics::ItemPhysics", 0x0023db50);
}

// stub of FS2 0x0023c2f0
void ItemPhysics::collisionHook3(CollisionBody *body)
{
    YAMIPAN_STUB("ItemPhysics::collisionHook3", 0x0023c2f0);
}

// stub of FS2 0x0023c910
void ItemPhysics::draw()
{
    YAMIPAN_STUB("ItemPhysics::draw", 0x0023c910);
}

// stub of FS2 0x0023d600
void ItemPhysics::setup(StageItem *owner, void *motion)
{
    YAMIPAN_STUB("ItemPhysics::setup", 0x0023d600);
}

// stub of FS2 0x0023c300
void ItemPhysics::syncBody(CollisionBody *body)
{
    YAMIPAN_STUB("ItemPhysics::syncBody", 0x0023c300);
}

// stub of FS2 0x0023ca50
void ItemPhysics::update()
{
    YAMIPAN_STUB("ItemPhysics::update", 0x0023ca50);
}

// stub of FS2 0x0023db00
ItemPhysics::~ItemPhysics()
{
    YAMIPAN_STUB("ItemPhysics::~ItemPhysics", 0x0023db00);
}

// stub of FS2 0x002a7a50
NmlPose::NmlPose(int, int, int, int)
{
    YAMIPAN_STUB("NmlPose::NmlPose", 0x002a7a50);
}

// stub of FS2 0x002b37d0
SkyPose::SkyPose(int, int)
{
    YAMIPAN_STUB("SkyPose::SkyPose", 0x002b37d0);
}

// stub of FS2 address unknown (1 candidates)
// stub of FS2 address unknown (1 candidates)
bool StageObject::followsPlayerLight()
{
    YAMIPAN_STUB("StageObject::followsPlayerLight", 0x00000000);
    return {};
}

// stub of FS2 0x00207440
// stub of FS2 0x0029f2a0
bool missionRestoreCheck()
{
    YAMIPAN_STUB("missionRestoreCheck", 0x0029f2a0);
    return {};
}


// stub of FS2 0x00207aa0
// stub of FS2 0x0023c020
int ItemPhysics::collisionHook0(CollisionContact *contact, const float *normal, StageCollisionFace *face,
                        float depth)
{
    YAMIPAN_STUB("ItemPhysics::collisionHook0", 0x0023c020);
    return {};
}

// ---- by hand: the character's parts, until their TUs (next in the queue) ----

// stub of FS2 0x00239bc0
// stub of FS2 0x0020c940
void StageCharacter::getSwayMatrix(sceVu0FMATRIX out)
{
    YAMIPAN_STUB("StageCharacter::getSwayMatrix", 0x0020c940);
}

// stub of FS2 0x00212020
bool StageCharacter::canTakeOver()
{
    YAMIPAN_STUB("StageCharacter::canTakeOver", 0x00212020);
    return {};
}

// stub of FS2 0x002119d0
bool StageCharacter::isCompanionWalking()
{
    YAMIPAN_STUB("StageCharacter::isCompanionWalking", 0x002119d0);
    return {};
}

// stub of FS2 0x0023ce00
void ItemPhysics::getMeanVelocity(sceVu0FVECTOR out)
{
    YAMIPAN_STUB("ItemPhysics::getMeanVelocity", 0x0023ce00);
}

// stub of FS2 0x001e05b0
bool PlayerMotion::winsStruggle(StageObject *other)
{
    YAMIPAN_STUB("PlayerMotion::winsStruggle", 0x001e05b0);
    return {};
}

// stub of FS2 0x00207e20
// stub of FS2 0x00210720
// stub of FS2 0x0023cf50
void ItemPhysics::push(const sceVu0FVECTOR velocity, int bone)
{
    YAMIPAN_STUB("ItemPhysics::push", 0x0023cf50);
}

// stub of FS2 0x0023cfc0
void ItemPhysics::setMoving(bool moving, bool awake)
{
    YAMIPAN_STUB("ItemPhysics::setMoving", 0x0023cfc0);
}

// stub of FS2 address unknown (1 candidates)
bool SceneOwnedObject::canUse()
{
    YAMIPAN_STUB("SceneOwnedObject::canUse", 0x00000000);
    return {};
}

// stub of FS2 address unknown (1 candidates)
bool SceneOwnedObject::start(const char *text)
{
    YAMIPAN_STUB("SceneOwnedObject::start", 0x00000000);
    return {};
}

// stub of FS2 0x00294280
void elevatorCall(StageObject *elevator, int floor)
{
    YAMIPAN_STUB("elevatorCall", 0x00294280);
}

// stub of FS2 0x00294140
bool elevatorCanCall(StageObject *elevator, const float *position, const float *rotation, int floor)
{
    YAMIPAN_STUB("elevatorCanCall", 0x00294140);
    return {};
}

// stub of FS2 0x00214a80
StageObject * StageCharacter::findNearestFoe(bool foes, bool sight)
{
    YAMIPAN_STUB("StageCharacter::findNearestFoe", 0x00214a80);
    return {};
}

// stub of FS2 0x00218850
void StageCharacter::item218850(int arg)
{
    YAMIPAN_STUB("StageCharacter::item218850", 0x00218850);
}

// stub of FS2 0x00218b50
void StageCharacter::useItem(const char *name, const float *point, int arg)
{
    YAMIPAN_STUB("StageCharacter::useItem", 0x00218b50);
}

// stub of FS2 0x0029f440
StageObject * findNumLockTarget()
{
    YAMIPAN_STUB("findNumLockTarget", 0x0029f440);
    return {};
}

// stub of FS2 0x001dee50
// stub of FS2 0x001dee90
// stub of FS2 0x002a8920
bool SceneOwnedObject::getRecordFlag31()
{
    YAMIPAN_STUB("SceneOwnedObject::getRecordFlag31", 0x002a8920);
    return {};
}

// stub of FS2 address unknown (1 candidates)
bool SceneOwnedObject::isSlot2c()
{
    YAMIPAN_STUB("SceneOwnedObject::isSlot2c", 0x00000000);
    return {};
}

// stub of FS2 address unknown (1 candidates)
bool SceneOwnedObject::isSlot44()
{
    YAMIPAN_STUB("SceneOwnedObject::isSlot44", 0x00000000);
    return {};
}

// stub of FS2 address unknown (1 candidates)
void SceneOwnedObject::setNames(const char *first, const char *second)
{
    YAMIPAN_STUB("SceneOwnedObject::setNames", 0x00000000);
}

// stub of FS2 address unknown (0 candidates)
u_int elevatorCalledFloors(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorCalledFloors", 0x00000000);
    return {};
}

// stub of FS2 0x00294c30
bool elevatorIsAtB1(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsAtB1", 0x00294c30);
    return {};
}

// stub of FS2 0x00294c70
bool elevatorIsAtB3(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsAtB3", 0x00294c70);
    return {};
}

// stub of FS2 0x00294b60
bool elevatorIsPhase1(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsPhase1", 0x00294b60);
    return {};
}

// stub of FS2 0x00294b00
bool elevatorIsPhase2(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsPhase2", 0x00294b00);
    return {};
}

// stub of FS2 0x00294b30
bool elevatorIsPhase3(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsPhase3", 0x00294b30);
    return {};
}

// stub of FS2 0x00294ad0
bool elevatorIsPhase4(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsPhase4", 0x00294ad0);
    return {};
}

// stub of FS2 0x00294f90
void elevatorMove(StageObject *elevator, bool toB3, bool fast)
{
    YAMIPAN_STUB("elevatorMove", 0x00294f90);
}

// stub of FS2 0x002a6c40
int numLockPopDials(StageObject *lock)
{
    YAMIPAN_STUB("numLockPopDials", 0x002a6c40);
    return {};
}

// stub of FS2 0x002a6cd0
int numLockPushDials(StageObject *lock, int a, int b, int c, int d)
{
    YAMIPAN_STUB("numLockPushDials", 0x002a6cd0);
    return {};
}

// stub of FS2 0x002a6b70
void numLockScramble(StageObject *lock)
{
    YAMIPAN_STUB("numLockScramble", 0x002a6b70);
}

// stub of FS2 0x002b2b70
void SceneOwnedObject::setSevenKeyNumbers(const char *name, int a, int b, int c, int d)
{
    YAMIPAN_STUB("SceneOwnedObject::setSevenKeyNumbers", 0x002b2b70);
}

// stub of FS2 0x00293fd0
void elevatorDoorPlace(StageObject *elevator, float *out, int floor)
{
    YAMIPAN_STUB("elevatorDoorPlace", 0x00293fd0);
}

// stub of FS2 0x00294be0
bool elevatorOpenAtB1(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorOpenAtB1", 0x00294be0);
    return {};
}

// stub of FS2 0x00294b90
bool elevatorOpenAtB3(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorOpenAtB3", 0x00294b90);
    return {};
}

// stub of FS2 0x00293c60
void elevatorSwitchPlace(StageObject *elevator, float *place, float *direction, int which)
{
    YAMIPAN_STUB("elevatorSwitchPlace", 0x00293c60);
}


// stub of FS2 0x002b23b0
int sevenKeysPoseValue(StageObject *object, int sub)
{
    YAMIPAN_STUB("sevenKeysPoseValue", 0x002b23b0);
    return {};
}

// stub of FS2 0x002a8910
void SceneOwnedObject::setRecordFlag31(bool on)
{
    YAMIPAN_STUB("SceneOwnedObject::setRecordFlag31", 0x002a8910);
}

// stub of FS2 address unknown (1 candidates)
void SceneOwnedObject::takeMotionText(const char *text)
{
    YAMIPAN_STUB("SceneOwnedObject::takeMotionText", 0x00000000);
}

// stub of FS2 address unknown (1 candidates)
void SceneOwnedObject::takeMotionUser(StageObject *user)
{
    YAMIPAN_STUB("SceneOwnedObject::takeMotionUser", 0x00000000);
}

// stub of FS2 0x002a6fc0
bool numLockDialsRight(StageObject *lock)
{
    YAMIPAN_STUB("numLockDialsRight", 0x002a6fc0);
    return {};
}

// stub of FS2 0x002a7070
bool numLockDialsTurning(StageObject *lock)
{
    YAMIPAN_STUB("numLockDialsTurning", 0x002a7070);
    return {};
}

// stub of FS2 0x002a6ee0
void numLockSetDial(StageObject *lock, int dial)
{
    YAMIPAN_STUB("numLockSetDial", 0x002a6ee0);
}

// stub of FS2 0x002a6e40
void numLockSpinDial(StageObject *lock, int dial)
{
    YAMIPAN_STUB("numLockSpinDial", 0x002a6e40);
}
