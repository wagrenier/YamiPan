// lib/unassigned_stubs.cpp -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// Functions below are stubs until reconstructed; see tools/reverse/stage.py.
#include "rena/rena_draw.h"

#include <string.h>

#include "common/yamipan_stub.h"
#include "game/fontdata.h"
#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/memcard.h"
#include "game/modelpose.h"
#include "game/playermotion.h"
#include "game/stagenode.h"
#include "game/stageobjlist.h"
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

// stub of FS2 0x002b1620
int ClearSequence::getState()
{
    YAMIPAN_STUB("ClearSequence::getState", 0x002b1620);
    return {};
}

// stub of FS2 0x002b1c50
int GameOverSequence::getState()
{
    YAMIPAN_STUB("GameOverSequence::getState", 0x002b1c50);
    return {};
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

// stub of FS2 0x002bc770
KeepList::KeepList()
{
    YAMIPAN_STUB("KeepList::KeepList", 0x002bc770);
}

// stub of FS2 0x002bc5d0
void KeepList::removeUnkept()
{
    YAMIPAN_STUB("KeepList::removeUnkept", 0x002bc5d0);
}

// stub of FS2 0x002bc6f0
KeepList::~KeepList()
{
    YAMIPAN_STUB("KeepList::~KeepList", 0x002bc6f0);
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
PauseMenuState::PauseMenuState()
{
    YAMIPAN_STUB("PauseMenuState::PauseMenuState", 0x00207ef0);
}

// stub of FS2 0x00207ea0
PauseMenuState::~PauseMenuState()
{
    YAMIPAN_STUB("PauseMenuState::~PauseMenuState", 0x00207ea0);
}

// stub of FS2 0x0029e4f0
PrgScaleNavi::PrgScaleNavi(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
    : UiWidget(seq, layout, data, def, arg)
{
    YAMIPAN_STUB("PrgScaleNavi::PrgScaleNavi", 0x0029e4f0);
}

// stub of FS2 0x0029ae30
PrgScaleNaviScroll::PrgScaleNaviScroll(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
    : UiWidget(seq, layout, data, def, arg)
{
    YAMIPAN_STUB("PrgScaleNaviScroll::PrgScaleNaviScroll", 0x0029ae30);
}

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

// stub of FS2 0x00253180
RoutePlayer::RoutePlayer(void*)
{
    YAMIPAN_STUB("RoutePlayer::RoutePlayer", 0x00253180);
}

// stub of FS2 0x00252b10
void RoutePlayer::start(int arg)
{
    YAMIPAN_STUB("RoutePlayer::start", 0x00252b10);
}

// stub of FS2 0x0023ad60
SceneBoxList::SceneBoxList()
{
    YAMIPAN_STUB("SceneBoxList::SceneBoxList", 0x0023ad60);
}

// stub of FS2 0x0023ad10
SceneBoxList::~SceneBoxList()
{
    YAMIPAN_STUB("SceneBoxList::~SceneBoxList", 0x0023ad10);
}

// stub of FS2 0x002ce5a0
SceneBufferSet::~SceneBufferSet()
{
    YAMIPAN_STUB("SceneBufferSet::~SceneBufferSet", 0x002ce5a0);
}

// stub of FS2 0x002b2280
SceneDirector::~SceneDirector()
{
    YAMIPAN_STUB("SceneDirector::~SceneDirector", 0x002b2280);
}

// stub of FS2 0x002cff30
SceneModel::~SceneModel()
{
    YAMIPAN_STUB("SceneModel::~SceneModel", 0x002cff30);
}

// stub of FS2 address unknown (1 candidates)
void SceneOwnedObject::destroy()
{
    YAMIPAN_STUB("SceneOwnedObject::destroy", 0x00000000);
}

// stub of FS2 0x00289be0
ScenePool::ScenePool(unsigned int, int, int)
{
    YAMIPAN_STUB("ScenePool::ScenePool", 0x00289be0);
}

// stub of FS2 0x00289b60
ScenePool::~ScenePool()
{
    YAMIPAN_STUB("ScenePool::~ScenePool", 0x00289b60);
}

// stub of FS2 0x00243150
SceneUiState::SceneUiState()
{
    YAMIPAN_STUB("SceneUiState::SceneUiState", 0x00243150);
}

// stub of FS2 0x00243100
SceneUiState::~SceneUiState()
{
    YAMIPAN_STUB("SceneUiState::~SceneUiState", 0x00243100);
}

// stub of FS2 0x00151f20
ScreenFill::ScreenFill(unsigned int, int, int, int, int)
{
    YAMIPAN_STUB("ScreenFill::ScreenFill", 0x00151f20);
}

// stub of FS2 address unknown (1 candidates)
void ScreenFill::destroy()
{
    YAMIPAN_STUB("ScreenFill::destroy", 0x00000000);
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

// stub of FS2 0x00190c20
StageNodeTree::StageNodeTree()
{
    YAMIPAN_STUB("StageNodeTree::StageNodeTree", 0x00190c20);
}

// stub of FS2 0x00190aa0
void StageNodeTree::addParticleTexture(Tim2Texture *texture)
{
    YAMIPAN_STUB("StageNodeTree::addParticleTexture", 0x00190aa0);
}

// stub of FS2 0x00190b50
void StageNodeTree::setParticleFileA(void *file)
{
    YAMIPAN_STUB("StageNodeTree::setParticleFileA", 0x00190b50);
}

// stub of FS2 0x00190ae0
void StageNodeTree::setParticleFileB(void *file)
{
    YAMIPAN_STUB("StageNodeTree::setParticleFileB", 0x00190ae0);
}

// stub of FS2 0x00190bc0
StageNodeTree::~StageNodeTree()
{
    YAMIPAN_STUB("StageNodeTree::~StageNodeTree", 0x00190bc0);
}

// stub of FS2 0x00214f60
u_char StageObject::getAltSoundFlag()
{
    YAMIPAN_STUB("StageObject::getAltSoundFlag", 0x00214f60);
    return {};
}

// stub of FS2 0x001fecf0
StageParty::StageParty()
{
    YAMIPAN_STUB("StageParty::StageParty", 0x001fecf0);
}

// stub of FS2 0x001fea50
StageObject * StageParty::getCompanion()
{
    YAMIPAN_STUB("StageParty::getCompanion", 0x001fea50);
    return {};
}

// stub of FS2 0x001fe720
void StageParty::resetSaved()
{
    YAMIPAN_STUB("StageParty::resetSaved", 0x001fe720);
}

// stub of FS2 0x001fe6c0
void StageParty::restore()
{
    YAMIPAN_STUB("StageParty::restore", 0x001fe6c0);
}

// stub of FS2 0x001fe6d0
void StageParty::save()
{
    YAMIPAN_STUB("StageParty::save", 0x001fe6d0);
}

// stub of FS2 0x001fec90
StageParty::~StageParty()
{
    YAMIPAN_STUB("StageParty::~StageParty", 0x001fec90);
}

// stub of FS2 0x002152d0
bool StageCharacter::isMode1()
{
    YAMIPAN_STUB("StageCharacter::isMode1", 0x002152d0);
    return {};
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

// stub of FS2 0x0029efe0
NaviMenu * getNaviMenu()
{
    YAMIPAN_STUB("getNaviMenu", 0x0029efe0);
    return {};
}

// stub of FS2 0x0029efd0
SaveExtra * getSaveExtra()
{
    YAMIPAN_STUB("getSaveExtra", 0x0029efd0);
    return {};
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

// stub of FS2 0x0029acb0
int applyNaviAnswer(int answer)
{
    YAMIPAN_STUB("applyNaviAnswer", 0x0029acb0);
    return {};
}

// stub of FS2 0x0029ad40
int currentNaviAnswer()
{
    YAMIPAN_STUB("currentNaviAnswer", 0x0029ad40);
    return {};
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

// stub of FS2 0x00295470
int runNaviFile()
{
    YAMIPAN_STUB("runNaviFile", 0x00295470);
    return {};
}

// stub of FS2 0x0029ef70
void saveExtraInit()
{
    YAMIPAN_STUB("saveExtraInit", 0x0029ef70);
}

// stub of FS2 0x002e0840
void GsRegion::release()
{
    YAMIPAN_STUB("GsRegion::release", 0x002e0840);
}

// stub of FS2 address unknown (0 candidates)
BoxVolume::BoxVolume()
{
    YAMIPAN_STUB("BoxVolume::BoxVolume", 0x00000000);
}

// stub of FS2 0x0017ea50
void BoxVolume::setSize(float width, float height, float depth)
{
    YAMIPAN_STUB("BoxVolume::setSize", 0x0017ea50);
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

// stub of FS2 0x00151e10
void ScreenFill::setImage(const void *image)
{
    YAMIPAN_STUB("ScreenFill::setImage", 0x00151e10);
}

// stub of FS2 0x001d27e0
// stub of FS2 0x001d2f40
// stub of FS2 address unknown (0 candidates)
SphereVolume::SphereVolume()
{
    YAMIPAN_STUB("SphereVolume::SphereVolume", 0x00000000);
}

// stub of FS2 0x0017ec20
void SphereVolume::setRadius(float radius)
{
    YAMIPAN_STUB("SphereVolume::setRadius", 0x0017ec20);
}

// stub of FS2 address unknown (0 candidates)

// stub of FS2 0x00185210

// stub of FS2 address unknown (0 candidates)


// stub of FS2 0x001861b0

// stub of FS2 0x00186160

// stub of FS2 0x00185fb0

// stub of FS2 0x00185bb0

// stub of FS2 0x00185c40

// stub of FS2 0x00214fc0
int StageObject::getActionState()
{
    YAMIPAN_STUB("StageObject::getActionState", 0x00214fc0);
    return {};
}

// stub of FS2 address unknown (1 candidates)
void StageObject::onViewTop()
{
    YAMIPAN_STUB("StageObject::onViewTop", 0x00000000);
}

// stub of FS2 0x001fe9b0
void StageParty::setFillImage(const void *image)
{
    YAMIPAN_STUB("StageParty::setFillImage", 0x001fe9b0);
}

// stub of FS2 0x0029fb30
int parseSpawnId(const char *name)
{
    YAMIPAN_STUB("parseSpawnId", 0x0029fb30);
    return {};
}

// stub of FS2 0x0029f050
// stub of FS2 address unknown (0 candidates)
// stub of FS2 address unknown (0 candidates)

// stub of FS2 0x0017ef10
void StageMarker::addToScene(const char *name)
{
    YAMIPAN_STUB("StageMarker::addToScene", 0x0017ef10);
}

// stub of FS2 0x0017ecb0
void StageVolume::addToScene(const char *name)
{
    YAMIPAN_STUB("StageVolume::addToScene", 0x0017ecb0);
}

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

// stub of FS2 0x001dd3f0
void CharacterBrain::setEnabled(bool on)
{
    YAMIPAN_STUB("CharacterBrain::setEnabled", 0x001dd3f0);
}

// stub of FS2 0x00239bb0
// stub of FS2 0x002b1500
void ClearSequence::update()
{
    YAMIPAN_STUB("ClearSequence::update", 0x002b1500);
}

// stub of FS2 0x002b1670
void GameOverSequence::update()
{
    YAMIPAN_STUB("GameOverSequence::update", 0x002b1670);
}

// stub of FS2 0x002bc100
void KeepList::update(float frames, u_char *motorA, u_char *motorB, char *flags, int *tags,
                int max, StageObject *source)
{
    YAMIPAN_STUB("KeepList::update", 0x002bc100);
}

// stub of FS2 0x002ad030
void MenuProgHolder::draw(float deltaSeconds, GamePacketManager *packets, StageObject *camera)
{
    YAMIPAN_STUB("MenuProgHolder::draw", 0x002ad030);
}

// stub of FS2 0x002078d0
bool PauseMenuState::isActive()
{
    YAMIPAN_STUB("PauseMenuState::isActive", 0x002078d0);
    return {};
}

// stub of FS2 0x00207500
void PauseMenuState::update()
{
    YAMIPAN_STUB("PauseMenuState::update", 0x00207500);
}

// stub of FS2 0x002bb260
void PlayerOverlay::draw()
{
    YAMIPAN_STUB("PlayerOverlay::draw", 0x002bb260);
}

// stub of FS2 0x00268290
void Rand48::seed(u_int value)
{
    YAMIPAN_STUB("Rand48::seed", 0x00268290);
}

// stub of FS2 0x0023aca0
void SceneBoxList::update()
{
    YAMIPAN_STUB("SceneBoxList::update", 0x0023aca0);
}

// stub of FS2 0x002b22d0
SceneDirector::SceneDirector()
{
    YAMIPAN_STUB("SceneDirector::SceneDirector", 0x002b22d0);
}

// stub of FS2 0x002b20a0
void SceneDirector::draw(sceVif1Packet *packet)
{
    YAMIPAN_STUB("SceneDirector::draw", 0x002b20a0);
}

// stub of FS2 0x002b2270
bool SceneDirector::isRunning()
{
    YAMIPAN_STUB("SceneDirector::isRunning", 0x002b2270);
    return {};
}

// stub of FS2 0x002b2250
void SceneDirector::start(int frames, u_int from, u_int to)
{
    YAMIPAN_STUB("SceneDirector::start", 0x002b2250);
}

// stub of FS2 0x002d07a0
SceneModelHolder::SceneModelHolder(StageNodeTree*, int)
{
    YAMIPAN_STUB("SceneModelHolder::SceneModelHolder", 0x002d07a0);
}

// stub of FS2 0x002d0550
void SceneModelHolder::draw(float deltaSeconds, sceVif1Packet *packet, StageObject *camera)
{
    YAMIPAN_STUB("SceneModelHolder::draw", 0x002d0550);
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
void ScenePool::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    YAMIPAN_STUB("ScenePool::draw", 0x00289800);
}

// stub of FS2 0x00289b20
int ScenePool::setSlot(int index, const u_short *text)
{
    YAMIPAN_STUB("ScenePool::setSlot", 0x00289b20);
    return {};
}

// stub of FS2 address unknown (1 candidates)
void ScreenFill::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    YAMIPAN_STUB("ScreenFill::draw", 0x00000000);
}

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

// stub of FS2 0x00190a80
void StageNodeTree::draw(GamePacketManager *packets, StageObject *camera)
{
    YAMIPAN_STUB("StageNodeTree::draw", 0x00190a80);
}

// stub of FS2 0x00190a90
void StageNodeTree::update(float range, int arg, StageObject *view, StagePlayer *player)
{
    YAMIPAN_STUB("StageNodeTree::update", 0x00190a90);
}

// stub of FS2 address unknown (1 candidates)
void StageObject::cameraStep()
{
    YAMIPAN_STUB("StageObject::cameraStep", 0x00000000);
}

// stub of FS2 0x002152a0
bool StageObject::isMode2()
{
    YAMIPAN_STUB("StageObject::isMode2", 0x002152a0);
    return {};
}

// stub of FS2 address unknown (1 candidates)
void StageObject::onLinkChanged()
{
    YAMIPAN_STUB("StageObject::onLinkChanged", 0x00000000);
}

// stub of FS2 0x001febb0
void StageParty::createCamera()
{
    YAMIPAN_STUB("StageParty::createCamera", 0x001febb0);
}

// stub of FS2 0x001feb70
void StageParty::draw()
{
    YAMIPAN_STUB("StageParty::draw", 0x001feb70);
}

// stub of FS2 0x001fe9c0
void StageParty::setCompanion(StageObject *object)
{
    YAMIPAN_STUB("StageParty::setCompanion", 0x001fe9c0);
}

// stub of FS2 0x001feb80
void StageParty::update()
{
    YAMIPAN_STUB("StageParty::update", 0x001feb80);
}

// stub of FS2 0x0020b1d0
void StageCharacter::latchAltAction()
{
    YAMIPAN_STUB("StageCharacter::latchAltAction", 0x0020b1d0);
}

// stub of FS2 0x0029aae0
ViewCamera::ViewCamera() : StageCamera(450.0f, 512.0f, 512.0f, 1600.0f)
{
    YAMIPAN_STUB("ViewCamera::ViewCamera", 0x0029aae0);
}

// stub of FS2 0x002849c0
void gsFrameFilter(sceVif1Packet *packet, const sceGsDrawEnv1 *env, int w, int h,
                   u_short block, int passes, int arg)
{
    YAMIPAN_STUB("gsFrameFilter", 0x002849c0);
}


// stub of FS2 0x001dd330
void CharacterBrain::update()
{
    YAMIPAN_STUB("CharacterBrain::update", 0x001dd330);
}

// stub of FS2 0x002b1660
ClearSequence::ClearSequence()
{
    YAMIPAN_STUB("ClearSequence::ClearSequence", 0x002b1660);
}

// stub of FS2 0x002b1cb0
GameOverSequence::GameOverSequence(int, int)
{
    YAMIPAN_STUB("GameOverSequence::GameOverSequence", 0x002b1cb0);
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

// stub of FS2 0x002894b0
void ScenePool::setImage(const void *image)
{
    YAMIPAN_STUB("ScenePool::setImage", 0x002894b0);
}

// stub of FS2 0x00289b50
void ScenePool::setPrinter(WideFontPrinter *printer)
{
    YAMIPAN_STUB("ScenePool::setPrinter", 0x00289b50);
}

// stub of FS2 0x00242e80
void SceneUiState::pollSoundSources()
{
    YAMIPAN_STUB("SceneUiState::pollSoundSources", 0x00242e80);
}

// stub of FS2 0x00230810
void StageMenu::setData(void *data)
{
    YAMIPAN_STUB("StageMenu::setData", 0x00230810);
}

// stub of FS2 0x0021c310
void StageObject::afterScripts()
{
    YAMIPAN_STUB("StageObject::afterScripts", 0x0021c310);
}

// stub of FS2 0x0021c7c0
void StageObject::beforeScripts()
{
    YAMIPAN_STUB("StageObject::beforeScripts", 0x0021c7c0);
}

// stub of FS2 0x002185a0
bool StageObject::canSpawn()
{
    YAMIPAN_STUB("StageObject::canSpawn", 0x002185a0);
    return {};
}

// stub of FS2 0x00175c40
bool StageObject::isMotionTarget(int index)
{
    YAMIPAN_STUB("StageObject::isMotionTarget", 0x00175c40);
    return {};
}

// stub of FS2 0x00218630
void StageObject::spawn()
{
    YAMIPAN_STUB("StageObject::spawn", 0x00218630);
}

// stub of FS2 0x001fe890
void StageParty::addImage(const void *image)
{
    YAMIPAN_STUB("StageParty::addImage", 0x001fe890);
}

// stub of FS2 0x001fe900
void StageParty::setImage(const void *image)
{
    YAMIPAN_STUB("StageParty::setImage", 0x001fe900);
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

// stub of FS2 0x001cf1e0
// stub of FS2 0x001cf1b0
// stub of FS2 0x002430a0
void SceneUiState::addNoise(float loudness, StageObject *object, void *extra)
{
    YAMIPAN_STUB("SceneUiState::addNoise", 0x002430a0);
}

// stub of FS2 0x00242fc0
void SceneUiState::addSourceNoise(StageObject *object, StageSoundSource *source)
{
    YAMIPAN_STUB("SceneUiState::addSourceNoise", 0x00242fc0);
}

// stub of FS2 0x00187060
void PartSwitch::init(ModelInstance *partsModel, const char *name, int partCount, int firstShown)
{
    YAMIPAN_STUB("PartSwitch::init", 0x00187060);
}

// stub of FS2 0x00186fe0
void PartSwitch::show(int index)
{
    YAMIPAN_STUB("PartSwitch::show", 0x00186fe0);
}

// stub of FS2 0x001908c0
void StageNodeTree::remove(StageParticle *effect)
{
    YAMIPAN_STUB("StageNodeTree::remove", 0x001908c0);
}

// stub of FS2 0x00190890
StageParticle * StageNodeTree::spawn(const char *name, StageNodeBase *parent, const float *pos,
                         const float *rot, int arg, int arg2)
{
    YAMIPAN_STUB("StageNodeTree::spawn", 0x00190890);
    return {};
}

// stub of FS2 0x001908a0
StageParticle * StageNodeTree::spawnNamed(const char *name, int arg, int arg2)
{
    YAMIPAN_STUB("StageNodeTree::spawnNamed", 0x001908a0);
    return {};
}

// stub of FS2 0x0018c220
void StageParticle::setEmitting(bool on)
{
    YAMIPAN_STUB("StageParticle::setEmitting", 0x0018c220);
}

// stub of FS2 0x00190040
void StageParticle::setRangeSquared(float rangeSquared)
{
    YAMIPAN_STUB("StageParticle::setRangeSquared", 0x00190040);
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

// stub of FS2 0x0020e6a0
void StageCharacter::playerEvent(const char *name)
{
    YAMIPAN_STUB("StageCharacter::playerEvent", 0x0020e6a0);
}

// stub of FS2 0x0018c2d0
void StageParticle::setRunning(bool on)
{
    YAMIPAN_STUB("StageParticle::setRunning", 0x0018c2d0);
}

// stub of FS2 0x0027ef70
CarPose::CarPose()
{
    YAMIPAN_STUB("CarPose::CarPose", 0x0027ef70);
}

// stub of FS2 0x0027ffb0
ElevatorPose::ElevatorPose()
{
    YAMIPAN_STUB("ElevatorPose::ElevatorPose", 0x0027ffb0);
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

// stub of FS2 0x00216140
u_int StageObject::getCharacterClass()
{
    YAMIPAN_STUB("StageObject::getCharacterClass", 0x00216140);
    return {};
}

// stub of FS2 0x00215e70
bool StageObject::isYriOrKne()
{
    YAMIPAN_STUB("StageObject::isYriOrKne", 0x00215e70);
    return {};
}

// stub of FS2 0x0028ec70
void StageCharacter::setGearLightOrder(int order)
{
    YAMIPAN_STUB("StageCharacter::setGearLightOrder", 0x0028ec70);
}

// stub of FS2 0x00207440
void PauseMenuState::clearList()
{
    YAMIPAN_STUB("PauseMenuState::clearList", 0x00207440);
}

// stub of FS2 0x00214f20
bool StageCharacter::isHoldingAction()
{
    YAMIPAN_STUB("StageCharacter::isHoldingAction", 0x00214f20);
    return {};
}

// stub of FS2 0x001dedb0
void StageCharacter::releaseAction()
{
    YAMIPAN_STUB("StageCharacter::releaseAction", 0x001dedb0);
}

// stub of FS2 0x002052d0
void demoFilmDestroy(void *film)
{
    YAMIPAN_STUB("demoFilmDestroy", 0x002052d0);
}

// stub of FS2 0x00204550
void demoFilmDraw(float level, void *film, sceVif1Packet *packet)
{
    YAMIPAN_STUB("demoFilmDraw", 0x00204550);
}

// stub of FS2 0x001fea40
void demoFilmTake(StageParty *party, void **film, void **flecks)
{
    YAMIPAN_STUB("demoFilmTake", 0x001fea40);
}

// stub of FS2 0x002a11c0
void demoFleckDraw(float level, void *flecks, sceVif1Packet *packet)
{
    YAMIPAN_STUB("demoFleckDraw", 0x002a11c0);
}

// stub of FS2 0x002a11d0
void demoFleckStep(void *flecks)
{
    YAMIPAN_STUB("demoFleckStep", 0x002a11d0);
}

// stub of FS2 0x0029f810
int missionPickSpawnPoint(const float *place, int mode)
{
    YAMIPAN_STUB("missionPickSpawnPoint", 0x0029f810);
    return {};
}

// stub of FS2 0x0029f2a0
bool missionRestoreCheck()
{
    YAMIPAN_STUB("missionRestoreCheck", 0x0029f2a0);
    return {};
}

// stub of FS2 0x0029fb60
void missionSpawnAt(int *point, int id)
{
    YAMIPAN_STUB("missionSpawnAt", 0x0029fb60);
}

// stub of FS2 0x0029fd10
bool missionSpawnPointTaken(int *point)
{
    YAMIPAN_STUB("missionSpawnPointTaken", 0x0029fd10);
    return {};
}

// stub of FS2 address unknown (0 candidates)
GateParts::GateParts(StageObject*, PartSwitch*, int*, int, GateParts*)
{
    YAMIPAN_STUB("GateParts::GateParts", 0x00000000);
}

// stub of FS2 0x0028ed60
void GateParts::copyFrom(const GateParts *from)
{
    YAMIPAN_STUB("GateParts::copyFrom", 0x0028ed60);
}

// stub of FS2 0x0028ecb0
void GateParts::setEffects(bool on)
{
    YAMIPAN_STUB("GateParts::setEffects", 0x0028ecb0);
}

// stub of FS2 address unknown (0 candidates)
GateParts::~GateParts()
{
    YAMIPAN_STUB("GateParts::~GateParts", 0x00000000);
}


// stub of FS2 0x00207aa0
bool PauseMenuState::addNotice(const u_short *text, int frames, int arg, int level)
{
    YAMIPAN_STUB("PauseMenuState::addNotice", 0x00207aa0);
    return {};
}

// stub of FS2 0x0023c020
int ItemPhysics::collisionHook0(CollisionContact *contact, const float *normal, StageCollisionFace *face,
                        float depth)
{
    YAMIPAN_STUB("ItemPhysics::collisionHook0", 0x0023c020);
    return {};
}

// stub of FS2 0x00175900
void StageObject::breakPart(int part, bool withEffect)
{
    YAMIPAN_STUB("StageObject::breakPart", 0x00175900);
}

// ---- by hand: the character's parts, until their TUs (next in the queue) ----

// stub of FS2 0x00239c60
// stub of FS2 0x00239be0
// stub of FS2 0x001dd680
CharacterBrain::CharacterBrain(StageObject *owner)
{
    YAMIPAN_STUB("CharacterBrain::CharacterBrain", 0x001dd680);
}

// The deleting dtor behind the vptr at +0xb4: its address is unread.
CharacterBrain::~CharacterBrain()
{
    YAMIPAN_STUB("CharacterBrain::~CharacterBrain", 0x00000000);
}

// stub of FS2 0x001dd460
void CharacterBrain::reset()
{
    YAMIPAN_STUB("CharacterBrain::reset", 0x001dd460);
}

// stub of FS2 0x00239bc0
// stub of FS2 0x0023f310
void RouteFilter::allow(const char *name, bool on)
{
    YAMIPAN_STUB("RouteFilter::allow", 0x0023f310);
}

// stub of FS2 0x002a6910
void StageCharacter::initSounds()
{
    YAMIPAN_STUB("StageCharacter::initSounds", 0x002a6910);
}

// stub of FS2 0x0021e050
void StageCharacter::resetActionState()
{
    YAMIPAN_STUB("StageCharacter::resetActionState", 0x0021e050);
}

// stub of FS2 0x00211430
void StageCharacter::setEndurance(int value)
{
    YAMIPAN_STUB("StageCharacter::setEndurance", 0x00211430);
}

// stub of FS2 0x00211150
void StageCharacter::setFlinch(int value)
{
    YAMIPAN_STUB("StageCharacter::setFlinch", 0x00211150);
}

// stub of FS2 0x00211300
void StageCharacter::setRightArm(int value)
{
    YAMIPAN_STUB("StageCharacter::setRightArm", 0x00211300);
}

// stub of FS2 0x002a6830
void StageCharacter::stopSounds()
{
    YAMIPAN_STUB("StageCharacter::stopSounds", 0x002a6830);
}

// stub of FS2 0x0018fec0
void StageParticle::stopChain()
{
    YAMIPAN_STUB("StageParticle::stopChain", 0x0018fec0);
}

// stub of FS2 0x001dfd90
void PlayerMotion::clearNoFlash(int arg)
{
    YAMIPAN_STUB("PlayerMotion::clearNoFlash", 0x001dfd90);
}

// stub of FS2 0x00215620
bool StageObject::isScenarioLead()
{
    YAMIPAN_STUB("StageObject::isScenarioLead", 0x00215620);
    return {};
}

// stub of FS2 0x001fe800
bool StageParty::isRunning()
{
    YAMIPAN_STUB("StageParty::isRunning", 0x001fe800);
    return {};
}

// stub of FS2 0x001feb90
void StageParty::stop()
{
    YAMIPAN_STUB("StageParty::stop", 0x001feb90);
}

// stub of FS2 0x0020c940
void StageCharacter::getSwayMatrix(sceVu0FMATRIX out)
{
    YAMIPAN_STUB("StageCharacter::getSwayMatrix", 0x0020c940);
}

// stub of FS2 0x0029a840
void ViewCamera::setZoom(float level)
{
    YAMIPAN_STUB("ViewCamera::setZoom", 0x0029a840);
}

// stub of FS2 0x0028ec90
bool GateParts::isLit()
{
    YAMIPAN_STUB("GateParts::isLit", 0x0028ec90);
    return {};
}

// stub of FS2 0x0028d780
void StageCar::honk()
{
    YAMIPAN_STUB("StageCar::honk", 0x0028d780);
}

// stub of FS2 0x0028d810
void StageCar::setDirection(int direction)
{
    YAMIPAN_STUB("StageCar::setDirection", 0x0028d810);
}

// stub of FS2 0x0028db80
void StageCar::setLights(bool on)
{
    YAMIPAN_STUB("StageCar::setLights", 0x0028db80);
}

// stub of FS2 0x00212020
bool StageCharacter::canTakeOver()
{
    YAMIPAN_STUB("StageCharacter::canTakeOver", 0x00212020);
    return {};
}

// stub of FS2 0x00212460
int StageCharacter::getLifeState()
{
    YAMIPAN_STUB("StageCharacter::getLifeState", 0x00212460);
    return {};
}

// stub of FS2 0x002119d0
bool StageCharacter::isCompanionWalking()
{
    YAMIPAN_STUB("StageCharacter::isCompanionWalking", 0x002119d0);
    return {};
}

// stub of FS2 0x001feba0
void StageParty::start(StageObject *object)
{
    YAMIPAN_STUB("StageParty::start", 0x001feba0);
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

// stub of FS2 0x00289e20
float StageCar::getAcceleration()
{
    YAMIPAN_STUB("StageCar::getAcceleration", 0x00289e20);
    return {};
}

// stub of FS2 0x00289f70
float StageCar::getDoorPoint(sceVu0FVECTOR out, int side, int index)
{
    YAMIPAN_STUB("StageCar::getDoorPoint", 0x00289f70);
    return {};
}

// stub of FS2 0x00289ea0
const float * StageCar::getFrameStep()
{
    YAMIPAN_STUB("StageCar::getFrameStep", 0x00289ea0);
    return {};
}

// stub of FS2 0x0028abf0
int StageCar::getHitLevel(const float *position)
{
    YAMIPAN_STUB("StageCar::getHitLevel", 0x0028abf0);
    return {};
}

// stub of FS2 0x00289dd0
float StageCar::getSpeed()
{
    YAMIPAN_STUB("StageCar::getSpeed", 0x00289dd0);
    return {};
}

// stub of FS2 0x00289e10
const float * StageCar::getVelocity()
{
    YAMIPAN_STUB("StageCar::getVelocity", 0x00289e10);
    return {};
}

// stub of FS2 0x00210540
int StageCharacter::getHandClass()
{
    YAMIPAN_STUB("StageCharacter::getHandClass", 0x00210540);
    return {};
}

// stub of FS2 0x002a2b10
void StageCharacter::playBumpSound(int surface)
{
    YAMIPAN_STUB("StageCharacter::playBumpSound", 0x002a2b10);
}

// stub of FS2 0x0028ab90
bool StageCar::hasSettled()
{
    YAMIPAN_STUB("StageCar::hasSettled", 0x0028ab90);
    return {};
}

// stub of FS2 0x001db3a0
bool CharacterBrain::askCloseDoor()
{
    YAMIPAN_STUB("CharacterBrain::askCloseDoor", 0x001db3a0);
    return {};
}

// stub of FS2 0x001db2d0
bool CharacterBrain::askDoorKey(int lock)
{
    YAMIPAN_STUB("CharacterBrain::askDoorKey", 0x001db2d0);
    return {};
}

// stub of FS2 0x001db470
bool CharacterBrain::askOpenDoor()
{
    YAMIPAN_STUB("CharacterBrain::askOpenDoor", 0x001db470);
    return {};
}

// stub of FS2 0x00207e20
bool PauseMenuState::hasNotice(int kind)
{
    YAMIPAN_STUB("PauseMenuState::hasNotice", 0x00207e20);
    return {};
}

// stub of FS2 0x00210720
bool StageCharacter::hasFollower()
{
    YAMIPAN_STUB("StageCharacter::hasFollower", 0x00210720);
    return {};
}

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

// stub of FS2 0x00186f80
const char * PartSwitch::partName(int index) const
{
    YAMIPAN_STUB("PartSwitch::partName", 0x00186f80);
    return {};
}
