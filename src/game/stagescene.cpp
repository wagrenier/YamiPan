// game/stagescene.cpp -- the stage scene and the stage's files
// (game/stagescene.h).

#include "game/stagescene.h"

#include <float.h>
#include <string.h>

#include "game/clock.h"
#include "game/fileloader.h"
#include "game/fontdata.h"
#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/seplayer.h"      /* StreamTable */
#include "game/stageobjlist.h"
#include "game/stagesound.h"
#include "game/uiwidget.h"
#include "game/vu0math.h"
#include "sdk/libvu0.h"
#include "sdk/scetypes.h"

// The data trees a stage script's paths are read from (StageFiles::dataDir).
static const char kDataDir[]      = "../data/";        /* data 0x304188 */
static const char kDataDirDebug[] = "../data/debug/";  /* data 0x304150 */
static const char kDataDirProof[] = "../data/proof/";  /* data 0x304160 */
// The path prefixes that have a debug / proof tree, by StageDataKind.
static const char *const kKindPrefixes[StageData_KindCount] = {
    "bg/",                      /* data 0x304148 */
    "chara/",                   /* data 0x304170 */
    "motion/",                  /* data 0x304178 */
    "item/",                    /* data 0x304180 */
};
static const char kScriptPrefix[] = "script/";         /* data 0x304118 */

// The loader's waitLoaded arguments in StageFiles::load (arg unused, the
// loading indicator shown).
constexpr int  kWaitArg         = 1;
constexpr bool kWaitShowLoading = true;

// The view stack's depth (the ctor's new[] of 8 entries).
constexpr int kViewStackSize = 8;
// The sub-scene demo voice's directory.
static const char kDemoDir[] = "streamEU/demo/";      /* data 0x303d20 */
// The screen rect the fill, the fade and the printers are built with: GS
// window units, the 512-pixel square at the 1792 offset (0x700).
constexpr int kScreenX = 0x700;
constexpr int kScreenY = 0x700;
constexpr int kScreenW = 0x200;
constexpr int kScreenH = 0x200;
// The fill's colour (RGBA bytes; alpha 4).
constexpr u_int kScreenFillRgba = 0x04808080;
// ScenePool's arguments.
constexpr u_int kScenePoolCount = 4;
constexpr int   kScenePoolSize  = 0x800;
constexpr int   kScenePoolSize2 = 0x878;

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00195640
// callcheck: getState

bool StageScene::isCleared()
{
    return clear != nullptr && clear->getState() == kClearSequenceDone;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001956e0
// callcheck: getState

bool StageScene::isGameOver()
{
    return gameOver != nullptr && gameOver->getState() == kGameOverSequenceDone;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00195d40
// callcheck: next

int StageScene::random(int n)
{
    return rng->next(n);
}

// ──────────────────────────────────────────────────────────────────────
// The objective text: the mission state's, from this scene or the nearest
// parent that has one.
// FS2 0x00195d50
// callcheck: getMission

const u_short *StageScene::getPurposeText()
{
    return getMission()->purposeText;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00195d70

void *StageScene::getSubSceneBlock()
{
    return subSceneBlock;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00195d80

bool StageScene::isSubSceneDone()
{
    return isSubScene && subScene == nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00196050

void StageScene::clearSubSceneRequest()
{
    subSceneRequest = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001963a0
// callcheck: currentMission, markStart

void StageScene::beginPlay()
{
    currentMission()->markStart();
}

// ──────────────────────────────────────────────────────────────────────
// PC: a sub-scene has no menu (+0xd0 null); the ROM reads through it
// anyway. Here a missing menu answers null.
// FS2 0x001963d0

StageMenu *StageScene::getOpenMenu()
{
    if (menu == nullptr || menu->state != StageMenu_Open)
        return nullptr;
    return menu;
}

// ──────────────────────────────────────────────────────────────────────
// The ROM unrolls the recursion eight levels deep before it calls itself.
// FS2 0x00196870
// callcheck: getParent, getMission

StageMission *StageScene::getMission()
{
    if (mission != nullptr)
        return mission;
    return getParent()->getMission();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00196950

StageScene *StageScene::getParent()
{
    return parent;
}

// ──────────────────────────────────────────────────────────────────────
// The same code as getMission (also unrolled eight levels deep).
// FS2 0x00196960
// callcheck: parentScene, currentMission

StageMission *StageScene::currentMission()
{
    if (mission != nullptr)
        return mission;
    return parentScene()->currentMission();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00196a40

StageScene *StageScene::parentScene()
{
    return parent;
}

// ──────────────────────────────────────────────────────────────────────
// The first active object from slot on (slot itself included); null past
// the last. Inline in the ROM (every walk over a group's list).
static StageObject **firstActive(StageObject **slot)
{
    while (slot != nullptr && !(*slot)->m_active)
        slot = StageObjArray::next(slot);
    return slot;
}

// Every mesh of every active Root object that has collision and is not
// hidden (skip left out) is tested; the face nearest to from wins, its hit
// point is written back.
//
// The ROM keeps the best hit point in an uninitialised local and copies it
// to hit even when nothing was hit; here that local starts as hit's own
// contents (StageSound reads hit only after a face). The ROM also calls
// the mesh accessor twice per mesh (the loop test and the argument).
// FS2 0x00198170
// callcheck: raycast, vecDistance

StageCollisionFace *StageScene::raycast(sceVu0FVECTOR hit, const sceVu0FVECTOR from,
                                        const sceVu0FVECTOR to, int arg, u_int mask,
                                        u_int flags, StageObject *skip)
{
    StageCollisionFace *best = nullptr;
    float bestDistance = FLT_MAX;
    sceVu0FVECTOR bestHit;
    for (int k = 0; k < 4; k++)
        bestHit[k] = hit[k];

    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = firstActive(roots.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        StageObject *obj = *slot;
        if ((skip != nullptr && skip == obj) || !obj->m_active || !obj->m_collides ||
            obj->m_hidden)
            continue;
        for (u_int i = 0; obj->collisionAt(i) != nullptr; i++) {
            StageCollisionFace *face = obj->collisionAt(i)->raycast(hit, from, to, mask,
                                                                    flags, arg);
            if (face == nullptr)
                continue;
            float distance = vecDistance(hit, from);
            if (distance < bestDistance) {
                for (int k = 0; k < 4; k++)
                    bestHit[k] = hit[k];
                best = face;
                bestDistance = distance;
            }
        }
    }

    for (int k = 0; k < 4; k++)
        hit[k] = bestHit[k];
    return best;
}

// ──────────────────────────────────────────────────────────────────────
// A stage hears through the top of the view stack: the player's alternate
// flag picks the X_J / X_D entries, and the scene flag is set while the
// player is in mode 1, or -- with a view target -- when the target is the
// player and the listener is not a Kind11 object, or when the listener is
// a Kind11 object and the target is the party's companion. A sub-scene
// only keeps its demo voice going.
// FS2 0x00198fc0
// callcheck: getAltSoundFlag, isMode1, getKind, isPlayer, getCompanion, update, update

void StageScene::drawPaused()
{
    if (isSubScene) {
        demoStream->update();
        return;
    }

    StageObject *listener = views->topObject();
    u_char playerFlag = player == nullptr ? 0 : player->character()->getAltSoundFlag();

    bool sceneFlag;
    if (player != nullptr && player->isMode1()) {
        sceneFlag = true;
    } else {
        sceneFlag = false;
        StageObject *target;
        if (viewTarget != nullptr && (target = viewTarget->character) != nullptr) {
            bool kind11 = views->topObject()->getKind() == StageObject_Kind11;
            if (target->isPlayer())
                sceneFlag = !kind11;
            else if (kind11)
                sceneFlag = party->getCompanion() == target;
        }
    }
    sound->update(listener, playerFlag, sceneFlag);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019ae10
// callcheck: contains

bool StageScene::getSoundAttr(const sceVu0FVECTOR pos)
{
    return areas[StageArea_Sound]->contains(pos) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019ae30
// callcheck: addId

void StageScene::addSoundArea(int id)
{
    areas[StageArea_Sound]->addId(id);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019ae40
// callcheck: addId

void StageScene::addArea2(int id)
{
    areas[StageArea_2]->addId(id);
}

// ──────────────────────────────────────────────────────────────────────
// The top of the view stack.
// FS2 0x0019bbb0

StageObject *StageScene::getActiveCharacter()
{
    return views->topObject();
}

// ──────────────────────────────────────────────────────────────────────
// The members in the ROM's order. A stage's sound is reset rather than
// deleted when it was handed in (stageLoad keeps one across reloads).
//
// PC: the ROM deletes a sub-scene's demo stream early and near the end
// asks the deleted stream isPlaying / stop; here the pointer is cleared
// at the delete and the late stop is skipped (the stream's dtor ends it).
// The ROM's +0x124 second array is freed from 0x10 bytes before it
// (CodeWarrior's array header); the host frees what it allocated.
// FS2 0x0019bbe0
// callcheck: ~StageObjectList, ~ScenePairTable, ~SceneRecordBuckets, ~StageNodeTree, ~StageSound, ~DemoStream, ~SceneRender, ~KeepList, ~Clock, ~MenuProgHolder, ~MenuProgSet, ~AreaGrid, ~SceneBufferSet, ~StageParty, ~ScenePool, ~PauseMenuState, ~SceneBoxList, ~SceneUiState, ~StageAlert, ~DrawLayerList, ~StageMission, ~SceneDirector, ~SceneModel, operator_delete, operator_delete[] (the delete-expressions; the printer through its virtual dtor)

StageScene::~StageScene()
{
    if (isSubScene)
        drawSysPop();

    delete objects;
    delete pairs;
    delete records;
    delete nodeTree;
    if (views != nullptr) {
        delete[] views->entries;
        delete views;
    }

    if (!isSubScene) {
        if (ownsSound)
            delete sound;
        else
            sound->reset();
    } else {
        delete demoStream;
        demoStream = nullptr;
    }
    delete render;
    delete keepList;
    delete clock;
    if (screenFill != nullptr)
        screenFill->destroy();
    delete menuProg;
    delete menuProgs;
    for (int i = 0; i < StageArea_Count; i++)
        delete areas[i];
    for (int i = 0; i < 3; i++) {
        if (owned[i] != nullptr)
            owned[i]->destroy();
    }
    if (block98 != nullptr) {
        operator delete(block98);
        block98 = nullptr;
    }
    if (buffers != nullptr) {
        delete buffers;
        buffers = nullptr;
    }
    delete party;
    delete printer;
    delete pool;
    delete pauseState;
    delete[] array44;
    if (itemList != nullptr)
        itemList->destroy();
    if (menu != nullptr)
        menu->destroy();
    delete boxes;
    delete uiState;
    delete showFlags;
    delete alert;
    if (fade != nullptr)
        fade->destroy();
    if (object4c != nullptr)
        object4c->destroy();
    delete[] array54;
    delete drawLayers;

    if (isSubScene) {
        if (subScene != nullptr)
            subScene->destroy();
        operator delete(subSceneBlock);
        subScene      = nullptr;
        subSceneBlock = nullptr;
        if (demoStream != nullptr && demoStream->isPlaying())
            demoStream->stop();
    }

    delete rng;
    delete mission;
    if (event != nullptr)
        event->destroy();
    if (gameOver != nullptr) {
        operator delete(gameOver);
        gameOver = nullptr;
    }
    if (clear != nullptr) {
        operator delete(clear);
        clear = nullptr;
    }
    if (director != nullptr) {
        delete director;
        director = nullptr;
    }
    if (arrays != nullptr) {
        delete[] arrays->first;
        delete[] arrays->second;
        delete arrays;
    }
    delete model;
}

// ──────────────────────────────────────────────────────────────────────
// A stage builds everything; a sub-scene skips the stage-only members
// (null) and gets a demo voice instead of a sound, plus its own draw
// record. Who / files / the view target and the sequences are set later
// (stageThread, the scene's update).
// FS2 0x0019c180
// callcheck: operator_new, StageObjectList, operator_new, ScenePairTable, operator_new, SceneRecordBuckets, operator_new, StageNodeTree, operator_new, StageSound, resetStageGroupTable, operator_new, DemoStream, setDir, operator_new, SceneRender, operator_new, KeepList, operator_new, operator_new[], createChild, operator_new, ScreenFill, operator_new, WideFontPrinter, operator_new, SceneBoxList, operator_new, StageFade, operator_new, ScenePool, operator_new, PauseMenuState, operator_new, UiItemList, operator_new, StageMenu, operator_new, SceneUiState, operator_new, UiShowFlags, operator_new, StageAlert, operator_new, Rand48, operator_new, StageMission, operator_new, AreaGrid, operator_new, AreaGrid, operator_new, AreaGrid, operator_new, AreaGrid, operator_new, StageParty, drawSysPush

StageScene::StageScene(bool isSubScene, StageSound *sound)
{
    this->isSubScene = isSubScene;
    mission    = nullptr;
    event      = nullptr;
    viewTarget = nullptr;
    _104       = 0;
    _108       = 0;
    gameOver   = nullptr;
    clear      = nullptr;
    arrays     = nullptr;
    model      = nullptr;
    director   = nullptr;   // PC: the ROM leaves +0x120 to whoever made the scene's memory
    _114       = 0;

    objects  = new StageObjectList();
    pairs    = new ScenePairTable();
    records  = new SceneRecordBuckets();
    nodeTree = new StageNodeTree();

    ownsSound = 0;
    if (!this->isSubScene) {
        if (sound == nullptr) {
            ownsSound   = 1;
            this->sound = new StageSound();
        } else {
            ownsSound   = 0;
            this->sound = sound;
        }
        demoStream = nullptr;
        resetStageGroupTable();
    } else {
        this->sound = nullptr;
        demoStream  = new DemoStream();
        demoStream->setDir(kDemoDir);
    }

    render   = new SceneRender();
    keepList = this->isSubScene ? nullptr : new KeepList();

    views           = new StageViewStack;
    views->capacity = kViewStackSize;
    views->entries  = new StageViewEntry[views->capacity];
    views->top      = -1;

    clock = gGameSystem->m_clock.createChild();
    _64   = 0;
    _3c = _40 = 0;
    array44    = nullptr;
    _48        = 0;
    object4c   = nullptr;
    _50        = 0;
    array54    = nullptr;
    drawLayers = nullptr;
    _5c        = 0;
    player = nullptr;
    _7c = _80 = _84 = 0;
    owned[0] = owned[1] = owned[2] = nullptr;
    block98 = nullptr;
    buffers = nullptr;

    screenFill = new ScreenFill(kScreenFillRgba, kScreenX, kScreenY, kScreenW, kScreenH);
    menuProg   = nullptr;
    menuProgs  = nullptr;
    printer    = new WideFontPrinter(kScreenX, kScreenY);
    _c8        = 0;
    boxes      = new SceneBoxList();
    _d4 = _d8 = 0;
    fade = new StageFade(kScreenX, kScreenY, kScreenW, kScreenH, 0);

    if (!this->isSubScene) {
        pool       = new ScenePool(kScenePoolCount, kScenePoolSize, kScenePoolSize2);
        pauseState = new PauseMenuState();
        itemList   = new UiItemList(printer, 0);
        menu       = new StageMenu(printer, 0);
        uiState    = new SceneUiState();
        showFlags  = new UiShowFlags();
        alert      = new StageAlert();
        rng        = new Rand48();
        mission    = new StageMission();
        for (int i = 0; i < StageArea_Count; i++)
            areas[i] = new AreaGrid();
        party = new StageParty();
    } else {
        pool       = nullptr;
        pauseState = nullptr;
        itemList   = nullptr;
        menu       = nullptr;
        uiState    = nullptr;
        showFlags  = nullptr;
        alert      = nullptr;
        rng        = nullptr;
        mission    = nullptr;
        for (int i = 0; i < StageArea_Count; i++)
            areas[i] = nullptr;
        party = nullptr;
    }

    _38             = 0;
    subSceneRequest = 0;
    subScene        = nullptr;
    subSceneBlock   = nullptr;
    event           = nullptr;
    if (this->isSubScene)
        drawSysPush();
    parent = nullptr;
    files  = nullptr;
    _114   = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001a0560

int stdtEntryCount(const StreamTable *table)
{
    return table->count;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001a0570

bool stdtIsRelocated(const StreamTable *table)
{
    return table->relocated;
}

// ──────────────────────────────────────────────────────────────────────
// The ROM ends with strstr(path, "script/") and drops the answer (a
// script/ path, like any other, reads from ../data/).
// FS2 0x001a1ca0
// callcheck: strstr, strstr, strstr, strstr, strstr

const char *StageFiles::dataDir(const char *path)
{
    for (int kind = 0; kind < StageData_KindCount; kind++) {
        if (strstr(path, kKindPrefixes[kind]) == path)
            return debugTree[kind] ? kDataDirDebug : kDataDirProof;
    }
    strstr(path, kScriptPrefix);
    return kDataDir;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001a37e0
// callcheck: dataDir, strcpy, addFile, startLoad, waitLoaded, getItem, parseScript, startLoad, waitLoaded, parseScript (the inline setDir; the loader's virtuals)

int StageFiles::load(const char *path)
{
    loader->setDir(dataDir(path));
    int index = loader->addFile(path);
    loader->startLoad();
    loader->waitLoaded(kWaitArg, kWaitShowLoading);
    if (loader->getItem(index) == nullptr)
        return 0;

    // First pass: the lines list their files; the loader loads them all.
    listPass = 1;
    if (!parseScript(index))
        return 0;
    loader->startLoad();
    loader->waitLoaded(kWaitArg, kWaitShowLoading);

    // Second pass: the lines take what was loaded.
    listPass = 0;
    return parseScript(index);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001a3db0

void StageFiles::loadSubSceneList()
{
}
