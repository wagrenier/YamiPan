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

// stub of FS2 0x00284560
void AreaGrid::addId(int id)
{
    YAMIPAN_STUB("AreaGrid::addId", 0x00284560);
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
    return {};
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
    return {};
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

// stub of FS2 0x002b5740
void menuSceneThread(void *arg)
{
    YAMIPAN_STUB("menuSceneThread", 0x002b5740);
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
