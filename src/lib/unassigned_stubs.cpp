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
#include "game/stagenode.h"
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

// stub of FS2 0x0023ae40
void CollisionFile::relocate()
{
    YAMIPAN_STUB("CollisionFile::relocate", 0x0023ae40);
}

// stub of FS2 0x002b1490
DemoStream::DemoStream()
{
    YAMIPAN_STUB("DemoStream::DemoStream", 0x002b1490);
}

// stub of FS2 0x002b1140
bool DemoStream::isPlaying()
{
    YAMIPAN_STUB("DemoStream::isPlaying", 0x002b1140);
    return {};
}

// stub of FS2 0x002b1370
void DemoStream::setDir(const char *dir)
{
    YAMIPAN_STUB("DemoStream::setDir", 0x002b1370);
}

// stub of FS2 0x002b1170
int DemoStream::stop()
{
    YAMIPAN_STUB("DemoStream::stop", 0x002b1170);
    return {};
}

// stub of FS2 0x002b1100
void DemoStream::update()
{
    YAMIPAN_STUB("DemoStream::update", 0x002b1100);
}

// stub of FS2 0x002b13e0
DemoStream::~DemoStream()
{
    YAMIPAN_STUB("DemoStream::~DemoStream", 0x002b13e0);
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

// stub of FS2 0x001e66c0
void PlayerMotion::refresh()
{
    YAMIPAN_STUB("PlayerMotion::refresh", 0x001e66c0);
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

// stub of FS2 0x00171680
ScenePairTable::ScenePairTable()
{
    YAMIPAN_STUB("ScenePairTable::ScenePairTable", 0x00171680);
}

// stub of FS2 0x00171620
ScenePairTable::~ScenePairTable()
{
    YAMIPAN_STUB("ScenePairTable::~ScenePairTable", 0x00171620);
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

// stub of FS2 0x001f6510
SceneRecordBuckets::SceneRecordBuckets()
{
    YAMIPAN_STUB("SceneRecordBuckets::SceneRecordBuckets", 0x001f6510);
}

// stub of FS2 0x001f63f0
SceneRecordBuckets::~SceneRecordBuckets()
{
    YAMIPAN_STUB("SceneRecordBuckets::~SceneRecordBuckets", 0x001f63f0);
}

// stub of FS2 0x001fa4a0
SceneRender::SceneRender()
{
    YAMIPAN_STUB("SceneRender::SceneRender", 0x001fa4a0);
}

// stub of FS2 0x001fa230
SceneRender::~SceneRender()
{
    YAMIPAN_STUB("SceneRender::~SceneRender", 0x001fa230);
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

// stub of FS2 0x00233810
StageCollisionFace * StageCollision::raycast(sceVu0FVECTOR hit, const sceVu0FVECTOR from,
                                const sceVu0FVECTOR to, u_int mask,
                                u_int flags, int arg)
{
    YAMIPAN_STUB("StageCollision::raycast", 0x00233810);
    return {};
}

// stub of FS2 address unknown (1 candidates)
void StageEvent::destroy()
{
    YAMIPAN_STUB("StageEvent::destroy", 0x00000000);
}

// stub of FS2 address unknown (1 candidates)
void * StageEvent::getSubSceneData()
{
    YAMIPAN_STUB("StageEvent::getSubSceneData", 0x00000000);
    return {};
}

// stub of FS2 0x00151600
StageFade::StageFade(int, int, int, int, int)
{
    YAMIPAN_STUB("StageFade::StageFade", 0x00151600);
}

// stub of FS2 address unknown (1 candidates)
void StageFade::destroy()
{
    YAMIPAN_STUB("StageFade::destroy", 0x00000000);
}

// stub of FS2 0x001512a0
bool StageFade::isFullInMode2()
{
    YAMIPAN_STUB("StageFade::isFullInMode2", 0x001512a0);
    return {};
}

// stub of FS2 0x002308a0
StageMenu::StageMenu(WideFontPrinter*, int)
{
    YAMIPAN_STUB("StageMenu::StageMenu", 0x002308a0);
}

// stub of FS2 address unknown (1 candidates)
void StageMenu::destroy()
{
    YAMIPAN_STUB("StageMenu::destroy", 0x00000000);
}

// stub of FS2 0x00287900
StageMission::StageMission()
{
    YAMIPAN_STUB("StageMission::StageMission", 0x00287900);
    // PC: no lists yet: runStageScripts walks them once a stage has scripts.
    memset((void *)this, 0, sizeof(*this));
}

// stub of FS2 0x002870c0
void StageMission::markStart()
{
    YAMIPAN_STUB("StageMission::markStart", 0x002870c0);
}

// stub of FS2 0x00286c30
void StageMission::setMessages(void *file)
{
    YAMIPAN_STUB("StageMission::setMessages", 0x00286c30);
}

// stub of FS2 0x00287820
StageMission::~StageMission()
{
    YAMIPAN_STUB("StageMission::~StageMission", 0x00287820);
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

// stub of FS2 0x00190900
void StageNodeTree::setBoxes(void *bbx)
{
    YAMIPAN_STUB("StageNodeTree::setBoxes", 0x00190900);
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

// stub of FS2 0x00215ef0
bool StageObject::isPlayer()
{
    YAMIPAN_STUB("StageObject::isPlayer", 0x00215ef0);
    return {};
}

// stub of FS2 0x001760c0
void StageObject::setupMapObject(const char *name, ModelInstance *instance,
                        StringMap<Tim2Texture *> *textures, void *collision)
{
    YAMIPAN_STUB("StageObject::setupMapObject", 0x001760c0);
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
bool StagePlayer::isMode1()
{
    YAMIPAN_STUB("StagePlayer::isMode1", 0x002152d0);
    return {};
}

// stub of FS2 0x001d3a40
StageScript * StageScript::relocate(bool withTable2)
{
    YAMIPAN_STUB("StageScript::relocate", 0x001d3a40);
    return {};
}

// stub of FS2 0x002bbdb0
SubSceneGateObject::SubSceneGateObject(int)
{
    YAMIPAN_STUB("SubSceneGateObject::SubSceneGateObject", 0x002bbdb0);
}

// stub of FS2 0x002bbc10
void SubSceneGateObject::setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures, void *collision)
{
    YAMIPAN_STUB("SubSceneGateObject::setup", 0x002bbc10);
}

// stub of FS2 0x002bc040
SubSceneMapObject::SubSceneMapObject(int)
{
    YAMIPAN_STUB("SubSceneMapObject::SubSceneMapObject", 0x002bc040);
}

// stub of FS2 0x002bbed0
void SubSceneMapObject::setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures, void *collision)
{
    YAMIPAN_STUB("SubSceneMapObject::setup", 0x002bbed0);
}

// stub of FS2 0x0023f0b0
UiItemList::UiItemList(WideFontPrinter*, int)
{
    YAMIPAN_STUB("UiItemList::UiItemList", 0x0023f0b0);
}

// stub of FS2 address unknown (1 candidates)
void UiItemList::destroy()
{
    YAMIPAN_STUB("UiItemList::destroy", 0x00000000);
}

// stub of FS2 0x00185af0

// stub of FS2 0x002d4110
WideFontPrinter::WideFontPrinter(short x, short y)
    : FontPrinter(x, y)
{
    YAMIPAN_STUB("WideFontPrinter::WideFontPrinter", 0x002d4110);
}

// stub of FS2 0x002362a0
bool boxContainsPoint(const float m[4][4], const float p[4])
{
    YAMIPAN_STUB("boxContainsPoint", 0x002362a0);
    return {};
}

// stub of FS2 0x001766c0
StageObject * createMapObject(const char *name, void *collision)
{
    YAMIPAN_STUB("createMapObject", 0x001766c0);
    return {};
}

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

// stub of FS2 0x0028fb80
const u_short * fsmFindText(float seconds, const void *file, int track)
{
    YAMIPAN_STUB("fsmFindText", 0x0028fb80);
    return {};
}

// stub of FS2 0x0028fc80
int fsmFindTrack(void *file, const char *key)
{
    YAMIPAN_STUB("fsmFindTrack", 0x0028fc80);
    return {};
}

// stub of FS2 0x0028fd30
bool fsmRelocate(void *file)
{
    YAMIPAN_STUB("fsmRelocate", 0x0028fd30);
    return {};
}

// stub of FS2 0x0028fa90
bool fsmTrackHasText(void *file, int track)
{
    YAMIPAN_STUB("fsmTrackHasText", 0x0028fa90);
    return {};
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

// stub of FS2 0x001769e0
bool isGateModelName(const char *name)
{
    YAMIPAN_STUB("isGateModelName", 0x001769e0);
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

// stub of FS2 0x00136490
void scaleVec4(float s, sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    YAMIPAN_STUB("scaleVec4", 0x00136490);
}

// stub of FS2 0x00135fe0
void setVecLength(float len, sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    YAMIPAN_STUB("setVecLength", 0x00135fe0);
}

// stub of FS2 0x00135f20
float vecDistance(const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    YAMIPAN_STUB("vecDistance", 0x00135f20);
    return {};
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

// stub of FS2 0x002b1270
bool DemoStream::open(const void *header, const char *name)
{
    YAMIPAN_STUB("DemoStream::open", 0x002b1270);
    return {};
}

// stub of FS2 0x002b1240
bool DemoStream::play(int volume)
{
    YAMIPAN_STUB("DemoStream::play", 0x002b1240);
    return {};
}

// stub of FS2 0x002b1010
bool DemoStream::setFont(const void *font, const void *image)
{
    YAMIPAN_STUB("DemoStream::setFont", 0x002b1010);
    return {};
}

// stub of FS2 0x002b11a0
bool DemoStream::setFsm(void *fsm)
{
    YAMIPAN_STUB("DemoStream::setFsm", 0x002b11a0);
    return {};
}

// stub of FS2 0x002b1110
bool DemoStream::setPaused(bool on)
{
    YAMIPAN_STUB("DemoStream::setPaused", 0x002b1110);
    return {};
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

// stub of FS2 0x001f6ad0
void SceneRender::addCbBox(AreaBox *box)
{
    YAMIPAN_STUB("SceneRender::addCbBox", 0x001f6ad0);
}

// stub of FS2 0x001f6ac0
void SceneRender::addSbBox(AreaBox *box)
{
    YAMIPAN_STUB("SceneRender::addSbBox", 0x001f6ac0);
}

// stub of FS2 0x001fa150
void SceneRender::setShadowTexture(Tim2Texture *texture)
{
    YAMIPAN_STUB("SceneRender::setShadowTexture", 0x001fa150);
}

// stub of FS2 0x001fa1c0
void SceneRender::setSpotlightTexture(Tim2Texture *texture)
{
    YAMIPAN_STUB("SceneRender::setSpotlightTexture", 0x001fa1c0);
}

// stub of FS2 0x00151e10
void ScreenFill::setImage(const void *image)
{
    YAMIPAN_STUB("ScreenFill::setImage", 0x00151e10);
}

// stub of FS2 0x001d27e0
void ScriptRunner::run(int arg)
{
    YAMIPAN_STUB("ScriptRunner::run", 0x001d27e0);
}

// stub of FS2 0x001d2f40
void ScriptRunner::setEntry(const void *entry)
{
    YAMIPAN_STUB("ScriptRunner::setEntry", 0x001d2f40);
}

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
StageCamera::StageCamera(float, float, float, float)
{
    YAMIPAN_STUB("StageCamera::StageCamera", 0x00000000);
}

// stub of FS2 0x00177d20
void StageCamera::addToScene(const char *name)
{
    YAMIPAN_STUB("StageCamera::addToScene", 0x00177d20);
}

// stub of FS2 0x00177900
void StageCamera::lookAt(const float *target)
{
    YAMIPAN_STUB("StageCamera::lookAt", 0x00177900);
}

// stub of FS2 address unknown (0 candidates)
StageCharacter::StageCharacter()
{
    YAMIPAN_STUB("StageCharacter::StageCharacter", 0x00000000);
}

// stub of FS2 0x0017c700
void StageCharacter::setup(const char *name, ModelInstance *instance, StringMap<Tim2Texture *> *textures,
               StageList<void *> *motions, StageList<void *> *motions2, void *mdl)
{
    YAMIPAN_STUB("StageCharacter::setup", 0x0017c700);
}

// stub of FS2 0x00287bb0
const char * StageEvent::getVoicePath(char *out)
{
    YAMIPAN_STUB("StageEvent::getVoicePath", 0x00287bb0);
    return {};
}

// stub of FS2 0x00287b70
bool StageEvent::isSkippable()
{
    YAMIPAN_STUB("StageEvent::isSkippable", 0x00287b70);
    return {};
}

// stub of FS2 0x00287af0
bool StageEvent::usesLanguageFont()
{
    YAMIPAN_STUB("StageEvent::usesLanguageFont", 0x00287af0);
    return {};
}

// stub of FS2 0x001512e0
bool StageFade::isFullInMode1()
{
    YAMIPAN_STUB("StageFade::isFullInMode1", 0x001512e0);
    return {};
}

// stub of FS2 0x001861b0

// stub of FS2 0x00186160

// stub of FS2 0x00185fb0

// stub of FS2 0x00185bb0

// stub of FS2 0x00185c40

// stub of FS2 0x002868c0
void StageMission::onRestore()
{
    YAMIPAN_STUB("StageMission::onRestore", 0x002868c0);
}

// stub of FS2 0x002867a0
void StageMission::setSpawnPoint(int id, const float *pos, const float *rot)
{
    YAMIPAN_STUB("StageMission::setSpawnPoint", 0x002867a0);
}

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

// stub of FS2 address unknown (0 candidates)
SubSceneCharacter::SubSceneCharacter(int)
{
    YAMIPAN_STUB("SubSceneCharacter::SubSceneCharacter", 0x00000000);
}

// stub of FS2 0x002bb870
void SubSceneCharacter::setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures)
{
    YAMIPAN_STUB("SubSceneCharacter::setup", 0x002bb870);
}

// stub of FS2 0x0026c880
StageScript * SubSceneData::getScript()
{
    YAMIPAN_STUB("SubSceneData::getScript", 0x0026c880);
    return {};
}

// stub of FS2 address unknown (0 candidates)
SubSceneItem::SubSceneItem(int)
{
    YAMIPAN_STUB("SubSceneItem::SubSceneItem", 0x00000000);
}

// stub of FS2 0x002bb680
void SubSceneItem::setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures)
{
    YAMIPAN_STUB("SubSceneItem::setup", 0x002bb680);
}

// stub of FS2 address unknown (0 candidates)
SubSceneModelObject::SubSceneModelObject(int)
{
    YAMIPAN_STUB("SubSceneModelObject::SubSceneModelObject", 0x00000000);
}

// stub of FS2 address unknown (0 candidates)
DemoExecuter::DemoExecuter(bool)
{
    YAMIPAN_STUB("DemoExecuter::DemoExecuter", 0x0026c8c0);
}

// stub of FS2 0x002bb810
void SubSceneProp::setup(const char *name, ModelInstance *instance,
               StringMap<Tim2Texture *> *textures)
{
    YAMIPAN_STUB("SubSceneProp::setup", 0x002bb810);
}

// stub of FS2 0x0029fb30
int parseSpawnId(const char *name)
{
    YAMIPAN_STUB("parseSpawnId", 0x0029fb30);
    return {};
}

// stub of FS2 0x0029f050
void scriptPrepare(StageScript *script)
{
    YAMIPAN_STUB("scriptPrepare", 0x0029f050);
}

// stub of FS2 address unknown (0 candidates)
ScriptRunner::ScriptRunner()
{
    YAMIPAN_STUB("ScriptRunner::ScriptRunner", 0x00000000);
}

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
void CharacterInput::setActive(bool on)
{
    YAMIPAN_STUB("CharacterInput::setActive", 0x00239bb0);
}

// stub of FS2 0x002b1500
void ClearSequence::update()
{
    YAMIPAN_STUB("ClearSequence::update", 0x002b1500);
}

// stub of FS2 0x0026ce20
DemoCamera::DemoCamera() : StageCamera(450.0f, 512.0f, 512.0f, 1600.0f)
{
    YAMIPAN_STUB("DemoCamera::DemoCamera", 0x0026ce20);
}

// stub of FS2 0x0026c920
void DemoCamera::drawOverlay()
{
    YAMIPAN_STUB("DemoCamera::drawOverlay", 0x0026c920);
}

// stub of FS2 0x002b0d60
void DemoStream::drawSubtitles(sceVif1Packet *packet)
{
    YAMIPAN_STUB("DemoStream::drawSubtitles", 0x002b0d60);
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

// stub of FS2 0x00171440
void ScenePairTable::actionControl()
{
    YAMIPAN_STUB("ScenePairTable::actionControl", 0x00171440);
}

// stub of FS2 0x001713a0
void ScenePairTable::actionMovement()
{
    YAMIPAN_STUB("ScenePairTable::actionMovement", 0x001713a0);
}

// stub of FS2 0x001714e0
void ScenePairTable::actionStart()
{
    YAMIPAN_STUB("ScenePairTable::actionStart", 0x001714e0);
}

// stub of FS2 0x00171300
void ScenePairTable::actionTerminate()
{
    YAMIPAN_STUB("ScenePairTable::actionTerminate", 0x00171300);
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

// stub of FS2 0x001f5250
void SceneRecordBuckets::collide()
{
    YAMIPAN_STUB("SceneRecordBuckets::collide", 0x001f5250);
}

// stub of FS2 0x001f7380
void SceneRender::applyClearColor()
{
    YAMIPAN_STUB("SceneRender::applyClearColor", 0x001f7380);
}

// stub of FS2 0x001f94c0
void SceneRender::setup()
{
    YAMIPAN_STUB("SceneRender::setup", 0x001f94c0);
}

// stub of FS2 0x001f73c0
void SceneRender::update()
{
    YAMIPAN_STUB("SceneRender::update", 0x001f73c0);
}

// stub of FS2 0x001f8620
void SceneRender::updateLights()
{
    YAMIPAN_STUB("SceneRender::updateLights", 0x001f8620);
}

// stub of FS2 address unknown (1 candidates)
void ScreenFill::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    YAMIPAN_STUB("ScreenFill::draw", 0x00000000);
}

// stub of FS2 address unknown (1 candidates)
ScriptRunner::~ScriptRunner()
{
    YAMIPAN_STUB("ScriptRunner::~ScriptRunner", 0x001d30c0);
}

// stub of FS2 0x00177810
void StageCamera::calcViewMatrix()
{
    YAMIPAN_STUB("StageCamera::calcViewMatrix", 0x00177810);
}

// stub of FS2 address unknown (1 candidates)
bool StageEvent::hidesText()
{
    YAMIPAN_STUB("StageEvent::hidesText", 0x00000000);
    return {};
}

// stub of FS2 address unknown (1 candidates)
void StageFade::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    YAMIPAN_STUB("StageFade::draw", 0x00000000);
}

// stub of FS2 0x00179700
StageMainCamera::StageMainCamera() : StageCamera(450.0f, 512.0f, 512.0f, 1600.0f)
{
    YAMIPAN_STUB("StageMainCamera::StageMainCamera", 0x00179700);
}

// stub of FS2 address unknown (1 candidates)
void StageMenu::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    YAMIPAN_STUB("StageMenu::draw", 0x00000000);
}

// stub of FS2 address unknown (1 candidates)
void StageMenu::update()
{
    YAMIPAN_STUB("StageMenu::update", 0x00000000);
}

// stub of FS2 0x002870f0
void StageMission::setup()
{
    YAMIPAN_STUB("StageMission::setup", 0x002870f0);
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

// stub of FS2 0x00173cb0
void StageObject::attachToRoot()
{
    YAMIPAN_STUB("StageObject::attachToRoot", 0x00173cb0);
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

// stub of FS2 0x00215ed0
bool StageObject::isPartner()
{
    YAMIPAN_STUB("StageObject::isPartner", 0x00215ed0);
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
void StagePlayer::latchAltAction()
{
    YAMIPAN_STUB("StagePlayer::latchAltAction", 0x0020b1d0);
}

// stub of FS2 0x0023e770
void UiItemList::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    YAMIPAN_STUB("UiItemList::draw", 0x0023e770);
}

// stub of FS2 0x0023ee90
int UiItemList::pick()
{
    YAMIPAN_STUB("UiItemList::pick", 0x0023ee90);
    return {};
}

// stub of FS2 0x0023e7b0
void UiItemList::update()
{
    YAMIPAN_STUB("UiItemList::update", 0x0023e7b0);
}

// stub of FS2 0x0023e740
bool UiPickSlot::isReady()
{
    YAMIPAN_STUB("UiPickSlot::isReady", 0x0023e740);
    return {};
}

// stub of FS2 0x0029aae0
ViewCamera::ViewCamera() : StageCamera(450.0f, 512.0f, 512.0f, 1600.0f)
{
    YAMIPAN_STUB("ViewCamera::ViewCamera", 0x0029aae0);
}

// stub of FS2 0x002eaeb0
void gsFillRect(sceVif1Packet *packet, int x, int y, int x2, int y2, u_int rgba)
{
    YAMIPAN_STUB("gsFillRect", 0x002eaeb0);
}

// stub of FS2 0x002849c0
void gsFrameFilter(sceVif1Packet *packet, const sceGsDrawEnv1 *env, int w, int h,
                   u_short block, int passes, int arg)
{
    YAMIPAN_STUB("gsFrameFilter", 0x002849c0);
}

// stub of FS2 0x001f9c70
int sceneRenderGroup(int mode)
{
    YAMIPAN_STUB("sceneRenderGroup", 0x001f9c70);
    return {};
}

// stub of FS2 0x0017df60
void stageUpdateCharacterMotions()
{
    YAMIPAN_STUB("stageUpdateCharacterMotions", 0x0017df60);
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

// stub of FS2 0x002342c0
StageCollisionFace * StageCollision::heightAt(float x, float y, float range, float *height, u_int mask,
                                 u_int flags, int arg)
{
    YAMIPAN_STUB("StageCollision::heightAt", 0x002342c0);
    return {};
}

// stub of FS2 0x00233160
bool StageCollision::segmentHits(const float *from, const float *to, u_int mask, u_int flags,
                     int arg)
{
    YAMIPAN_STUB("StageCollision::segmentHits", 0x00233160);
    return {};
}

// stub of FS2 0x00232870
bool StageCollision::sphereHits(float radius, int mode, void *out, u_int mask, u_int flags,
                    const float *pos)
{
    YAMIPAN_STUB("StageCollision::sphereHits", 0x00232870);
    return {};
}

// stub of FS2 address unknown (1 candidates)
int StageEvent::poll()
{
    YAMIPAN_STUB("StageEvent::poll", 0x00000000);
    return {};
}

// stub of FS2 0x002882b0
StageFileEvent::StageFileEvent(float, char const*)
{
    YAMIPAN_STUB("StageFileEvent::StageFileEvent", 0x002882b0);
}

// stub of FS2 0x00230810
void StageMenu::setData(void *data)
{
    YAMIPAN_STUB("StageMenu::setData", 0x00230810);
}

// stub of FS2 0x00286c40
void StageMission::afterScripts()
{
    YAMIPAN_STUB("StageMission::afterScripts", 0x00286c40);
}

// stub of FS2 0x00287090
int StageMission::getResultValue(int index)
{
    YAMIPAN_STUB("StageMission::getResultValue", 0x00287090);
    return {};
}

// stub of FS2 0x00286910
void StageMission::setRestore(void *arg, StageObject *object)
{
    YAMIPAN_STUB("StageMission::setRestore", 0x00286910);
}

// stub of FS2 0x00287050
void StageMission::setResultValue(int index, int value)
{
    YAMIPAN_STUB("StageMission::setResultValue", 0x00287050);
}

// stub of FS2 0x00286ce0
void StageMission::tickPending()
{
    YAMIPAN_STUB("StageMission::tickPending", 0x00286ce0);
}

// stub of FS2 0x0021c310
void StageObject::afterScripts()
{
    YAMIPAN_STUB("StageObject::afterScripts", 0x0021c310);
}

// stub of FS2 0x00173880
void StageObject::applyDoorRecord()
{
    YAMIPAN_STUB("StageObject::applyDoorRecord", 0x00173880);
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

// stub of FS2 address unknown (1 candidates)
void StageObject::setMotionList(void *list, int arg, int arg2)
{
    YAMIPAN_STUB("StageObject::setMotionList", 0x00000000);
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

// stub of FS2 0x00288850
StageSimpleEvent::StageSimpleEvent(float, char const*)
{
    YAMIPAN_STUB("StageSimpleEvent::StageSimpleEvent", 0x00288850);
}

// stub of FS2 0x0023f030
void UiItemList::setSheet(void *sheet)
{
    YAMIPAN_STUB("UiItemList::setSheet", 0x0023f030);
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
int ScriptRunner::defaultCommand()
{
    YAMIPAN_STUB("ScriptRunner::defaultCommand", 0x001cf1e0);
    return {};
}

// stub of FS2 0x001cf1b0
int ScriptRunner::unsupportedCommand()
{
    YAMIPAN_STUB("ScriptRunner::unsupportedCommand", 0x001cf1b0);
    return {};
}

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

// stub of FS2 0x001f6690
CollisionBody::CollisionBody(CollisionOwner*, int, unsigned int, unsigned int)
{
    YAMIPAN_STUB("CollisionBody::CollisionBody", 0x001f6690);
}

// stub of FS2 0x001f6670
void CollisionBody::refresh()
{
    YAMIPAN_STUB("CollisionBody::refresh", 0x001f6670);
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

// stub of FS2 0x001715f0
void ScenePairTable::add(StageSideBase *entry, int list)
{
    YAMIPAN_STUB("ScenePairTable::add", 0x001715f0);
}

// stub of FS2 0x00171580
void ScenePairTable::remove(StageSideBase *entry)
{
    YAMIPAN_STUB("ScenePairTable::remove", 0x00171580);
}

// stub of FS2 0x001f6180
void SceneRecordBuckets::add(CollisionBody *body)
{
    YAMIPAN_STUB("SceneRecordBuckets::add", 0x001f6180);
}

// stub of FS2 0x001f60a0
void SceneRecordBuckets::remove(CollisionBody *body)
{
    YAMIPAN_STUB("SceneRecordBuckets::remove", 0x001f60a0);
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
const void * StageScript::findEntry(const char *name)
{
    YAMIPAN_STUB("StageScript::findEntry", 0x001d3d20);
    return {};
}

// stub of FS2 0x0029a370
void VectorTrack::addKey(VectorKey *key, int time, bool computeFrames)
{
    YAMIPAN_STUB("VectorTrack::addKey", 0x0029a370);
}

// stub of FS2 0x00299d90
void VectorTrack::step()
{
    YAMIPAN_STUB("VectorTrack::step", 0x00299d90);
}

// stub of FS2 0x0023db50
ItemPhysics::ItemPhysics(bool, bool)
{
    YAMIPAN_STUB("ItemPhysics::ItemPhysics", 0x0023db50);
}

// stub of FS2 0x0023c020
int ItemPhysics::collisionHook0()
{
    YAMIPAN_STUB("ItemPhysics::collisionHook0", 0x0023c020);
    return {};
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

// stub of FS2 0x00286c20
const char * StageMission::getItemText(const char *name)
{
    YAMIPAN_STUB("StageMission::getItemText", 0x00286c20);
    return {};
}

// stub of FS2 0x0018c2d0
void StageParticle::setRunning(bool on)
{
    YAMIPAN_STUB("StageParticle::setRunning", 0x0018c2d0);
}

// stub of FS2 0x002cf0b0
const SeEntry * getSurfaceSound(int surface)
{
    YAMIPAN_STUB("getSurfaceSound", 0x002cf0b0);
    return {};
}

// stub of FS2 0x00150d80
int motionBoneMatrix(sceVu0FMATRIX out, void *motion, const char *bone, int frame)
{
    YAMIPAN_STUB("motionBoneMatrix", 0x00150d80);
    return {};
}

// stub of FS2 0x002e86b0
void renaBeginFrame(sceVif1Packet *packet)
{
    YAMIPAN_STUB("renaBeginFrame", 0x002e86b0);
}

// stub of FS2 0x002e5450
void renaDrawModels(sceVif1Packet *packet, DrawSysRecord *record, int mask)
{
    YAMIPAN_STUB("renaDrawModels", 0x002e5450);
}

// stub of FS2 0x002e85e0
void renaEndPass(sceVif1Packet *packet)
{
    YAMIPAN_STUB("renaEndPass", 0x002e85e0);
}

// stub of FS2 0x002e8080
void renaSetLights(sceVif1Packet *packet, DrawSysLights *lights)
{
    YAMIPAN_STUB("renaSetLights", 0x002e8080);
}

// stub of FS2 0x002e8570
void renaSetShadowCount(sceVif1Packet *packet, int count)
{
    YAMIPAN_STUB("renaSetShadowCount", 0x002e8570);
}

// stub of FS2 0x002e8300
void renaSetShadowMatrices(sceVif1Packet *packet, int index, sceVu0FMATRIX projection,
                           sceVu0FMATRIX clip)
{
    YAMIPAN_STUB("renaSetShadowMatrices", 0x002e8300);
}

// stub of FS2 0x002e82a0
void renaSetShadowTexture(sceVif1Packet *packet, Tim2Texture *texture, int block)
{
    YAMIPAN_STUB("renaSetShadowTexture", 0x002e82a0);
}

// stub of FS2 0x002e87f0
void renaSetSpotlight(sceVif1Packet *packet, Tim2Texture *texture, int block)
{
    YAMIPAN_STUB("renaSetSpotlight", 0x002e87f0);
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

// stub of FS2 0x002d0fa0
ModelMotion::ModelMotion(ModelPose*, void*)
{
    YAMIPAN_STUB("ModelMotion::ModelMotion", 0x002d0fa0);
}

// stub of FS2 0x002d0f30
ModelMotion::~ModelMotion()
{
    YAMIPAN_STUB("ModelMotion::~ModelMotion", 0x002d0f30);
}

// stub of FS2 0x00151020
ModelPose::ModelPose()
{
    YAMIPAN_STUB("ModelPose::ModelPose", 0x00151020);
}

// stub of FS2 0x0014e300
int ModelPose::getSurfaceFrames(float *out)
{
    YAMIPAN_STUB("ModelPose::getSurfaceFrames", 0x0014e300);
    return {};
}

// stub of FS2 0x0014fab0
int ModelPose::play(ModelMotion *motion, u_int flags, int arg)
{
    YAMIPAN_STUB("ModelPose::play", 0x0014fab0);
    return {};
}

// stub of FS2 0x0014fca0
int ModelPose::playFile(void *file, u_int flags, int arg)
{
    YAMIPAN_STUB("ModelPose::playFile", 0x0014fca0);
    return {};
}

// stub of FS2 0x0014f950
int ModelPose::playFileLayered(void *file, u_int flags, int arg)
{
    YAMIPAN_STUB("ModelPose::playFileLayered", 0x0014f950);
    return {};
}

// stub of FS2 0x0014f910
int ModelPose::playLayered(ModelMotion *motion, u_int flags, int arg)
{
    YAMIPAN_STUB("ModelPose::playLayered", 0x0014f910);
    return {};
}

// stub of FS2 0x0014e4d0
int ModelPose::solve(int iterations, int count, int flaggedOnly)
{
    YAMIPAN_STUB("ModelPose::solve", 0x0014e4d0);
    return {};
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

// stub of FS2 0x0014ff90
void ModelPose::load(void *file, int arg, const char *boneNames, u_int boneCount)
{
    YAMIPAN_STUB("ModelPose::load", 0x0014ff90);
}

// stub of FS2 0x0014f4b0
void ModelPose::update(float frames, bool loop)
{
    YAMIPAN_STUB("ModelPose::update", 0x0014f4b0);
}

// stub of FS2 0x00150f50
ModelPose::~ModelPose()
{
    YAMIPAN_STUB("ModelPose::~ModelPose", 0x00150f50);
}
