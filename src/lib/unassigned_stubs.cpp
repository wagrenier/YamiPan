// lib/unassigned_stubs.cpp -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// Functions below are stubs until reconstructed; see tools/reverse/stage.py.

#include "common/yamipan_stub.h"
#include "game/fontdata.h"
#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/memcard.h"
#include "game/stagenode.h"
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

// stub of FS2 0x00284640
AreaGrid::AreaGrid()
{
    YAMIPAN_STUB("AreaGrid::AreaGrid", 0x00284640);
}

// stub of FS2 0x00284190
int AreaGrid::contains(const sceVu0FVECTOR pos)
{
    YAMIPAN_STUB("AreaGrid::contains", 0x00284190);
    return {};
}

// stub of FS2 0x00284580
AreaGrid::~AreaGrid()
{
    YAMIPAN_STUB("AreaGrid::~AreaGrid", 0x00284580);
}

// stub of FS2 0x00177270
BoneNode * BoneNode::find(BoneNode *node, const char *key)
{
    YAMIPAN_STUB("BoneNode::find", 0x00177270);
    return {};
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

// stub of FS2 0x002e5210
ModelInstance * ModelFile::createInstance(u_int index, const char *name)
{
    YAMIPAN_STUB("ModelFile::createInstance", 0x002e5210);
    return {};
}

// stub of FS2 0x002e5200
int ModelFile::getNodeCount()
{
    YAMIPAN_STUB("ModelFile::getNodeCount", 0x002e5200);
    return {};
}

// stub of FS2 0x002e2cf0
bool ModelFile::isValid()
{
    YAMIPAN_STUB("ModelFile::isValid", 0x002e2cf0);
    return {};
}

// stub of FS2 0x002e2d20
void ModelFile::relocate()
{
    YAMIPAN_STUB("ModelFile::relocate", 0x002e2d20);
}

// stub of FS2 0x001d7f20
NodeTransform::NodeTransform()
{
    YAMIPAN_STUB("NodeTransform::NodeTransform", 0x001d7f20);
}

// stub of FS2 0x001d7ee0
void NodeTransform::setPosition(const float *pos)
{
    YAMIPAN_STUB("NodeTransform::setPosition", 0x001d7ee0);
}

// stub of FS2 0x002c0dc0
PackTables::PackTables()
{
    YAMIPAN_STUB("PackTables::PackTables", 0x002c0dc0);
}

// stub of FS2 0x002bcf10
void PackTables::parse(PackFile *pack)
{
    YAMIPAN_STUB("PackTables::parse", 0x002bcf10);
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

// stub of FS2 0x002b9d60
PrgMcCard::PrgMcCard(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
    : UiWidget(seq, layout, data, def, arg)
{
    YAMIPAN_STUB("PrgMcCard::PrgMcCard", 0x002b9d60);
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

// stub of FS2 0x002e4100
ModelPartRec * RenaModel::findGroup(const ModelPartRec *rec) const
{
    YAMIPAN_STUB("RenaModel::findGroup", 0x002e4100);
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

// stub of FS2 0x00186ee0
void StageMapFile::relocate()
{
    YAMIPAN_STUB("StageMapFile::relocate", 0x00186ee0);
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

// stub of FS2 address unknown (1 candidates)
const sceVu0FVECTOR * StageNode::getMatrix()
{
    YAMIPAN_STUB("StageNode::getMatrix", 0x00000000);
    return {};
}

// stub of FS2 address unknown (1 candidates)
const sceVu0FVECTOR * StageNode::getWorldMatrix()
{
    YAMIPAN_STUB("StageNode::getWorldMatrix", 0x00000000);
    return {};
}

// stub of FS2 address unknown (1 candidates)
void StageNode::updateMatrix()
{
    YAMIPAN_STUB("StageNode::updateMatrix", 0x00000000);
}

// stub of FS2 0x001d8540
StageNodeBase::StageNodeBase()
{
    YAMIPAN_STUB("StageNodeBase::StageNodeBase", 0x001d8540);
}

// stub of FS2 0x001865c0
StageNodeBase::~StageNodeBase()
{
    YAMIPAN_STUB("StageNodeBase::~StageNodeBase", 0x001865c0);
}

// stub of FS2 0x00184370
void StageNodeList::erase(StageNodeLink *link)
{
    YAMIPAN_STUB("StageNodeList::erase", 0x00184370);
}

// stub of FS2 0x001842c0
void StageNodeList::pushBack(StageNodeBase **item)
{
    YAMIPAN_STUB("StageNodeList::pushBack", 0x001842c0);
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

// stub of FS2 address unknown (1 candidates)
void StageObject::destroy()
{
    YAMIPAN_STUB("StageObject::destroy", 0x00000000);
}

// stub of FS2 0x00214f60
u_char StageObject::getAltSoundFlag()
{
    YAMIPAN_STUB("StageObject::getAltSoundFlag", 0x00214f60);
    return {};
}

// stub of FS2 address unknown (1 candidates)
int StageObject::getKind()
{
    YAMIPAN_STUB("StageObject::getKind", 0x00000000);
    return {};
}

// stub of FS2 address unknown (1 candidates)
const sceVu0FVECTOR * StageObject::getMatrix()
{
    YAMIPAN_STUB("StageObject::getMatrix", 0x00000000);
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

// stub of FS2 0x00149040
String::String(char const*)
{
    YAMIPAN_STUB("String::String", 0x00149040);
}

// stub of FS2 0x00148ec0
String & String::append(const char *text)
{
    YAMIPAN_STUB("String::append", 0x00148ec0);
    return *this;
}

// stub of FS2 0x00148db0
String::~String()
{
    YAMIPAN_STUB("String::~String", 0x00148db0);
}

// stub of FS2 0x001492b0
void StringList::pushBack(const String &text)
{
    YAMIPAN_STUB("StringList::pushBack", 0x001492b0);
}

// stub of FS2 0x00149100
int StringList::split(const char *text, const char *delimiters)
{
    YAMIPAN_STUB("StringList::split", 0x00149100);
    return {};
}

// stub of FS2 0x00149230
int StringList::splitString(const String &text, const char *delimiters)
{
    YAMIPAN_STUB("StringList::splitString", 0x00149230);
    return {};
}

// stub of FS2 0x001494c0
StringList::Node * StringList::tail()
{
    YAMIPAN_STUB("StringList::tail", 0x001494c0);
    return {};
}

// stub of FS2 0x00149250
StringList::~StringList()
{
    YAMIPAN_STUB("StringList::~StringList", 0x00149250);
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

// stub of FS2 address unknown (1 candidates)
void SubSceneObject::destroy()
{
    YAMIPAN_STUB("SubSceneObject::destroy", 0x00000000);
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

// stub of FS2 0x00243350
void UiTrack::draw(UiDrawer *drawer)
{
    YAMIPAN_STUB("UiTrack::draw", 0x00243350);
}

// stub of FS2 0x00243360
const float * UiTrack::getAnchor()
{
    YAMIPAN_STUB("UiTrack::getAnchor", 0x00243360);
    return {};
}

// stub of FS2 0x00185af0
float VcmController::getWeight() const
{
    YAMIPAN_STUB("VcmController::getWeight", 0x00185af0);
    return {};
}

// stub of FS2 0x002d4110
WideFontPrinter::WideFontPrinter(short x, short y)
    : FontPrinter(x, y)
{
    YAMIPAN_STUB("WideFontPrinter::WideFontPrinter", 0x002d4110);
}

// stub of FS2 0x002ec7f0
const char * archiveLayoutPath()
{
    YAMIPAN_STUB("archiveLayoutPath", 0x002ec7f0);
    return {};
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

// stub of FS2 0x002d7cf0
void drawSysInit(int originX, int originY, int width, int height,
                 GsRegionAlloc *alloc)
{
    YAMIPAN_STUB("drawSysInit", 0x002d7cf0);
}

// stub of FS2 0x002d7c00
void drawSysPop()
{
    YAMIPAN_STUB("drawSysPop", 0x002d7c00);
}

// stub of FS2 0x002d7c70
void drawSysPush()
{
    YAMIPAN_STUB("drawSysPush", 0x002d7c70);
}

// stub of FS2 0x002ec520
const char * eventMoviePath0102()
{
    YAMIPAN_STUB("eventMoviePath0102", 0x002ec520);
    return {};
}

// stub of FS2 0x002ec4d0
const char * eventMoviePath0202()
{
    YAMIPAN_STUB("eventMoviePath0202", 0x002ec4d0);
    return {};
}

// stub of FS2 0x002ec480
const char * eventMoviePath1206()
{
    YAMIPAN_STUB("eventMoviePath1206", 0x002ec480);
    return {};
}

// stub of FS2 0x002ec430
const char * eventMoviePath1404()
{
    YAMIPAN_STUB("eventMoviePath1404", 0x002ec430);
    return {};
}

// stub of FS2 0x002ec3e0
const char * eventMoviePath1405()
{
    YAMIPAN_STUB("eventMoviePath1405", 0x002ec3e0);
    return {};
}

// stub of FS2 0x002ec390
const char * eventMoviePath1505()
{
    YAMIPAN_STUB("eventMoviePath1505", 0x002ec390);
    return {};
}

// stub of FS2 0x002ec340
const char * eventMoviePath1506()
{
    YAMIPAN_STUB("eventMoviePath1506", 0x002ec340);
    return {};
}

// stub of FS2 0x002ec2f0
const char * eventMoviePath1608()
{
    YAMIPAN_STUB("eventMoviePath1608", 0x002ec2f0);
    return {};
}

// stub of FS2 0x002ec2a0
const char * eventMoviePath2103()
{
    YAMIPAN_STUB("eventMoviePath2103", 0x002ec2a0);
    return {};
}

// stub of FS2 0x002ec250
const char * eventMoviePath2205()
{
    YAMIPAN_STUB("eventMoviePath2205", 0x002ec250);
    return {};
}

// stub of FS2 0x002ec200
const char * eventMoviePath2301()
{
    YAMIPAN_STUB("eventMoviePath2301", 0x002ec200);
    return {};
}

// stub of FS2 0x002ec1b0
const char * eventMoviePath2502()
{
    YAMIPAN_STUB("eventMoviePath2502", 0x002ec1b0);
    return {};
}

// stub of FS2 0x002ec160
const char * eventMoviePath2506()
{
    YAMIPAN_STUB("eventMoviePath2506", 0x002ec160);
    return {};
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

// stub of FS2 0x002c3d70
void * getSaveResource(int id)
{
    YAMIPAN_STUB("getSaveResource", 0x002c3d70);
    return {};
}

// stub of FS2 0x002c3d30
void * getSaveResourceAux(int id)
{
    YAMIPAN_STUB("getSaveResourceAux", 0x002c3d30);
    return {};
}

// stub of FS2 0x002c4140
SePlayer * getSePlayer()
{
    YAMIPAN_STUB("getSePlayer", 0x002c4140);
    return {};
}

// stub of FS2 0x00243380
void * getUiWorkArea()
{
    YAMIPAN_STUB("getUiWorkArea", 0x00243380);
    return {};
}

// stub of FS2 0x001769e0
bool isGateModelName(const char *name)
{
    YAMIPAN_STUB("isGateModelName", 0x001769e0);
    return {};
}

// stub of FS2 0x002c3e10
void loadMenuPack()
{
    YAMIPAN_STUB("loadMenuPack", 0x002c3e10);
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

// stub of FS2 0x002ec660
const char * optionLayoutPath()
{
    YAMIPAN_STUB("optionLayoutPath", 0x002ec660);
    return {};
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

// stub of FS2 0x00283df0
int seDistanceVolume(float ratio, int falloff, int volume)
{
    YAMIPAN_STUB("seDistanceVolume", 0x00283df0);
    return {};
}

// stub of FS2 0x00135fe0
void setVecLength(float len, sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    YAMIPAN_STUB("setVecLength", 0x00135fe0);
}

// stub of FS2 0x002d0a10
int sldiDecompress(const void *src, void *dst)
{
    YAMIPAN_STUB("sldiDecompress", 0x002d0a10);
    return {};
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

// stub of FS2 0x002ec610
const char * mcBaseLayoutPath()
{
    YAMIPAN_STUB("mcBaseLayoutPath", 0x002ec610);
    return {};
}

// stub of FS2 0x002ec6b0
const char * memAltLayoutPath()
{
    YAMIPAN_STUB("memAltLayoutPath", 0x002ec6b0);
    return {};
}

// stub of FS2 0x002ec5c0
const char * naviTuLayoutPath()
{
    YAMIPAN_STUB("naviTuLayoutPath", 0x002ec5c0);
    return {};
}

// stub of FS2 0x002ec700
const char * newGameLayoutPath()
{
    YAMIPAN_STUB("newGameLayoutPath", 0x002ec700);
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

// stub of FS2 0x00231c90
int runTitleScreen(bool withIntro)
{
    YAMIPAN_STUB("runTitleScreen", 0x00231c90);
    return {};
}

// stub of FS2 0x0029ef70
void saveExtraInit()
{
    YAMIPAN_STUB("saveExtraInit", 0x0029ef70);
}

// stub of FS2 0x002ec750
const char * sceLogoLayoutPath()
{
    YAMIPAN_STUB("sceLogoLayoutPath", 0x002ec750);
    return {};
}

// stub of FS2 0x00243370
void setUiWorkArea(void *area)
{
    YAMIPAN_STUB("setUiWorkArea", 0x00243370);
}

// stub of FS2 0x002ec7a0
const char * topMenuLayoutPath()
{
    YAMIPAN_STUB("topMenuLayoutPath", 0x002ec7a0);
    return {};
}

// stub of FS2 0x002c3db0
void unloadMenuPack()
{
    YAMIPAN_STUB("unloadMenuPack", 0x002c3db0);
}

// stub of FS2 0x002e0840
void GsRegion::release()
{
    YAMIPAN_STUB("GsRegion::release", 0x002e0840);
}

// stub of FS2 address unknown (0 candidates)
AreaBox::AreaBox()
{
    YAMIPAN_STUB("AreaBox::AreaBox", 0x00000000);
}

// stub of FS2 0x002848d0
void AreaBox::setSize(float width, float height, float depth)
{
    YAMIPAN_STUB("AreaBox::setSize", 0x002848d0);
}

// stub of FS2 0x00284560
void AreaGrid::addBox(AreaBox *box)
{
    YAMIPAN_STUB("AreaGrid::addBox", 0x00284560);
}

// stub of FS2 0x00283ff0
int AreaGrid::collect(const sceVu0FVECTOR pos, AreaBox **out, int max)
{
    YAMIPAN_STUB("AreaGrid::collect", 0x00283ff0);
    return {};
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

// stub of FS2 0x001d7ec0
void NodeTransform::setRotation(float x, float y, float z)
{
    YAMIPAN_STUB("NodeTransform::setRotation", 0x001d7ec0);
}

// stub of FS2 address unknown (0 candidates)
PointLight::PointLight()
{
    YAMIPAN_STUB("PointLight::PointLight", 0x00000000);
}

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
SpotLight::SpotLight()
{
    YAMIPAN_STUB("SpotLight::SpotLight", 0x00000000);
}

// stub of FS2 0x00185210
void SpotLight::setConeAngle(int degrees)
{
    YAMIPAN_STUB("SpotLight::setConeAngle", 0x00185210);
}

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

// stub of FS2 address unknown (0 candidates)
StageItem::StageItem()
{
    YAMIPAN_STUB("StageItem::StageItem", 0x00000000);
}

// stub of FS2 0x001823f0
void StageItem::initWeapon()
{
    YAMIPAN_STUB("StageItem::initWeapon", 0x001823f0);
}

// stub of FS2 address unknown (1 candidates)
void StageItem::setupItem(const char *name, ModelInstance *instance,
                   StringMap<Tim2Texture *> *textures, StageList<void *> *motions)
{
    YAMIPAN_STUB("StageItem::setupItem", 0x00000000);
}

// stub of FS2 address unknown (0 candidates)
StageItemE::StageItemE()
{
    YAMIPAN_STUB("StageItemE::StageItemE", 0x00000000);
}

// stub of FS2 0x001861b0
void StageLight::addToScene(const char *name, bool withShade)
{
    YAMIPAN_STUB("StageLight::addToScene", 0x001861b0);
}

// stub of FS2 0x00186160
void StageLight::copyShownFrom(StageLight *other)
{
    YAMIPAN_STUB("StageLight::copyShownFrom", 0x00186160);
}

// stub of FS2 0x00185fb0
void StageLight::setEnabled(bool on)
{
    YAMIPAN_STUB("StageLight::setEnabled", 0x00185fb0);
}

// stub of FS2 0x00185bb0
void StageLight::setRadiusSquared(float radiusSquared)
{
    YAMIPAN_STUB("StageLight::setRadiusSquared", 0x00185bb0);
}

// stub of FS2 0x00185c40
void StageLight::setVector(float x, float y, float z)
{
    YAMIPAN_STUB("StageLight::setVector", 0x00185c40);
}

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

// stub of FS2 address unknown (0 candidates)
StageObject::StageObject()
{
    YAMIPAN_STUB("StageObject::StageObject", 0x00000000);
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

// stub of FS2 0x0018bdc0
void StageObjectTree::setActive(bool on)
{
    YAMIPAN_STUB("StageObjectTree::setActive", 0x0018bdc0);
}

// stub of FS2 0x0018bbe0
void StageObjectTree::setShown(bool on)
{
    YAMIPAN_STUB("StageObjectTree::setShown", 0x0018bbe0);
}

// stub of FS2 0x001fe9b0
void StageParty::setFillImage(const void *image)
{
    YAMIPAN_STUB("StageParty::setFillImage", 0x001fe9b0);
}

// stub of FS2 address unknown (0 candidates)
StageThrownItem::StageThrownItem()
{
    YAMIPAN_STUB("StageThrownItem::StageThrownItem", 0x00000000);
}

// stub of FS2 address unknown (0 candidates)
StageTrapItem::StageTrapItem()
{
    YAMIPAN_STUB("StageTrapItem::StageTrapItem", 0x00000000);
}

// stub of FS2 address unknown (0 candidates)
StageWeaponItem::StageWeaponItem()
{
    YAMIPAN_STUB("StageWeaponItem::StageWeaponItem", 0x00000000);
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
SubSceneObject::SubSceneObject(bool)
{
    YAMIPAN_STUB("SubSceneObject::SubSceneObject", 0x00000000);
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

// stub of FS2 0x0018b290
bool stageObjectIsShown(StageObject *object)
{
    YAMIPAN_STUB("stageObjectIsShown", 0x0018b290);
    return {};
}

// stub of FS2 address unknown (0 candidates)
ScriptRunner::ScriptRunner()
{
    YAMIPAN_STUB("ScriptRunner::ScriptRunner", 0x00000000);
}

// stub of FS2 address unknown (0 candidates)
StageLight::StageLight()
{
    YAMIPAN_STUB("StageLight::StageLight", 0x00000000);
}

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

// stub of FS2 0x001d7f00
void NodeTransform::setPositionXyz(float x, float y, float z)
{
    YAMIPAN_STUB("NodeTransform::setPositionXyz", 0x001d7f00);
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

// stub of FS2 0x002842f0
void AreaGrid::build()
{
    YAMIPAN_STUB("AreaGrid::build", 0x002842f0);
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

// stub of FS2 address unknown (1 candidates)
void DrawSysCamera::setScreenMatrix(const void *matrix)
{
    YAMIPAN_STUB("DrawSysCamera::setScreenMatrix", 0x00000000);
}

// stub of FS2 address unknown (1 candidates)
void DrawSysCamera::setViewMatrix(const void *matrix)
{
    YAMIPAN_STUB("DrawSysCamera::setViewMatrix", 0x00000000);
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

// stub of FS2 0x0014e4d0
int ModelPose::solve(int iterations, int count, int flaggedOnly)
{
    YAMIPAN_STUB("ModelPose::solve", 0x0014e4d0);
    return {};
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
void ScriptRunner::destroy()
{
    YAMIPAN_STUB("ScriptRunner::destroy", 0x00000000);
}

// stub of FS2 0x001803e0
StageBullet::StageBullet(int, int)
{
    YAMIPAN_STUB("StageBullet::StageBullet", 0x001803e0);
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

// stub of FS2 0x00187d10
void StageModel::bindBones()
{
    YAMIPAN_STUB("StageModel::bindBones", 0x00187d10);
}

// stub of FS2 0x00187b70
void StageModel::placeBones(StageObject *owner)
{
    YAMIPAN_STUB("StageModel::placeBones", 0x00187b70);
}

// stub of FS2 0x00187230
void StageModel::updateVcm()
{
    YAMIPAN_STUB("StageModel::updateVcm", 0x00187230);
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
void StageObject::calcMatrix()
{
    YAMIPAN_STUB("StageObject::calcMatrix", 0x00000000);
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

// stub of FS2 0x001d8260
void StageObject::propagate()
{
    YAMIPAN_STUB("StageObject::propagate", 0x001d8260);
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

// stub of FS2 0x002d4780
void drawSysClearTargetView()
{
    YAMIPAN_STUB("drawSysClearTargetView", 0x002d4780);
}

// stub of FS2 0x002d4fd0
DrawSysRecord * drawSysCurrent()
{
    YAMIPAN_STUB("drawSysCurrent", 0x002d4fd0);
    return {};
}

// stub of FS2 0x002d5560
void drawSysRender(GamePacketManager *packets, const sceGsDrawEnv1 *env, int flags)
{
    YAMIPAN_STUB("drawSysRender", 0x002d5560);
}

// stub of FS2 0x002d4790
void drawSysSetTargetView()
{
    YAMIPAN_STUB("drawSysSetTargetView", 0x002d4790);
}

// stub of FS2 0x002d4ef0
void drawSysTick(int ticks)
{
    YAMIPAN_STUB("drawSysTick", 0x002d4ef0);
}

// stub of FS2 0x002d9aa0
float getDrawDistance()
{
    YAMIPAN_STUB("getDrawDistance", 0x002d9aa0);
    return {};
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

// stub of FS2 0x002d9ab0
void setDrawDistance(float distance)
{
    YAMIPAN_STUB("setDrawDistance", 0x002d9ab0);
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

// stub of FS2 0x00185fc0
void StageObject::setTreeScriptFlag(bool on)
{
    YAMIPAN_STUB("StageObject::setTreeScriptFlag", 0x00185fc0);
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

// stub of FS2 0x00175c00
const char * stageMotionTargetName(int index)
{
    YAMIPAN_STUB("stageMotionTargetName", 0x00175c00);
    return {};
}
