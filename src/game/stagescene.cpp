// game/stagescene.cpp -- the stage scene and the stage's files
// (game/stagescene.h).

#include "game/stagescene.h"

#include <ctype.h>
#include <float.h>
#include <string.h>

#include "game/clock.h"
#include "game/fileloader.h"
#include "game/fontdata.h"
#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/gsutil.h"        /* Tim2Texture */
#include "game/seplayer.h"      /* StreamTable */
#include "game/stageobjlist.h"
#include "game/stageresult.h"   /* getInitItemForLanguage */
#include "game/stagesound.h"
#include "game/stringlist.h"
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

// A stage script's word separators and comment mark.
static const char kScriptSeparators[] = " \t\r\n";    /* data 0x303d40 */
constexpr char    kScriptComment = '#';

// The stage script's keywords, in the order the ROM tries them (table
// 0x31a928).
enum ScriptKeyword
{
    ScriptKeyword_Target,
    ScriptKeyword_End,
    ScriptKeyword_Texture,
    ScriptKeyword_Model,
    ScriptKeyword_Motion,
    ScriptKeyword_Mdl,
    ScriptKeyword_Collision,
    ScriptKeyword_Particle,
    ScriptKeyword_Map,
    ScriptKeyword_Script,
    ScriptKeyword_Rute,
    ScriptKeyword_Sndxv,
    ScriptKeyword_Sndseh,
    ScriptKeyword_Font,
    ScriptKeyword_Areaclip,
    ScriptKeyword_Bbx,
    ScriptKeyword_Lpt,
    ScriptKeyword_Sndsth,
    ScriptKeyword_Message,
    ScriptKeyword_Count,
};
static const char *const kScriptKeywords[ScriptKeyword_Count] = {
    "target", "end", "texture", "model", "motion", "mdl", "collision",
    "particle", "map", "script", "rute", "sndxv", "sndseh", "font",
    "areaclip", "bbx", "lpt", "sndsth", "message",
};
// What a `target` line opens (table 0x31a974).
enum ScriptTarget
{
    ScriptTarget_Script,
    ScriptTarget_Camera,
    ScriptTarget_Character,
    ScriptTarget_Bg,
    ScriptTarget_BgMotion,
    ScriptTarget_Particle,
    ScriptTarget_Item,
    ScriptTarget_Map,
    ScriptTarget_Sound,
    ScriptTarget_Effect,
    ScriptTarget_Count,
};
static const char *const kScriptTargets[ScriptTarget_Count] = {
    "script", "camera", "character", "bg", "bg_motion", "particle", "item",
    "map", "sound", "effect",
};
// The texture the effect target builds SigLamp from.
static const char kSigLampTexture[] = "siglamp";       /* data 0x303ee8 */
// FontPrinter::setTexture: the texture area's block.
constexpr int kBlockAuto = -1;
// Longest line splitLines takes (its stack buffer).
constexpr int kScriptLineSize = 0x410;

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
    delete[] scriptSlots;
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
    if (routePlayer != nullptr)
        routePlayer->destroy();
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
    scripts[0] = scripts[1] = nullptr;
    scriptSlots = nullptr;
    route       = nullptr;
    routePlayer = nullptr;
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

// PC: the ROM's libc strcasecmp (0x127ea0); MSVC has no such name.
static int scriptStrCaseCmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int ca = tolower((u_char)*a);
        int cb = tolower((u_char)*b);
        if (ca != cb || ca == 0)
            return ca - cb;
    }
}

// The index of word in table, count none (the ROM's strcasecmp chains).
static int findWord(const char *word, const char *const *table, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        if (scriptStrCaseCmp(word, table[i]) == 0)
            break;
    }
    return i;
}

// The bytes of the Shift-JIS character starting with c.
static inline int sjisCharSize(u_char c)
{
    bool lead = (c > 0x80 && c < 0xa0) || (c > 0xdf && c < 0xfd);
    return lead ? 2 : 1;
}

// ──────────────────────────────────────────────────────────────────────
// Adds text's lines to lines; line ends are LF, CR, LF CR or CR LF. With
// trim, the blanks around each line go (an empty line stays, empty).
// FS2 0x001a3500
// callcheck: String, operator_delete[] (the String ctor and its inline dtor: entry)

static void splitLines(StringList *lines, const char *text, bool trim)
{
    const char *start = text;
    while (*start != '\0') {
        const char *end = start;
        while (*end != '\0' && *end != '\n' && *end != '\r')
            end += sjisCharSize((u_char)*end);

        const char *stop = end;
        if (trim) {
            while (strchr(kScriptSeparators, *start) != nullptr && start < end)
                start++;
            for (const char *p = start; p != end; p += sjisCharSize((u_char)*p)) {
                if (strchr(kScriptSeparators, *p) == nullptr)
                    stop = p + sjisCharSize((u_char)*p);
            }
        }

        char line[kScriptLineSize];
        memcpy(line, start, stop - start);
        line[stop - start] = '\0';
        String entry(line);
        lines->pushBack(entry);

        start = end;
        if (*end == '\n') {
            start = end + 1;
            if (*start == '\r')
                start++;
        } else if (*end == '\r') {
            start = end + 1;
            if (*start == '\n')
                start++;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001a4270
// callcheck: FUN_001a4960, FUN_001a4ac0, FUN_001a4c20, FUN_001a5910, FUN_001a5980, FUN_001a59f0 (the members' constructors)

StageFiles::StageFiles(FileLoader *fileLoader)
{
    // PC: the ROM leaves target and the rest to parseScript's reset.
    target = nullptr;
    streamTablePath[0] = '\0';
    fsmPath[0] = '\0';
    listPass = 0;
    loader = fileLoader;
    sharedLoader = 1;
    for (int kind = 0; kind < StageData_KindCount; kind++)
        debugTree[kind] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001a3f90
// callcheck: FUN_001a4980, FUN_001a4ae0, FUN_001a4c40, FUN_001a58b0, FUN_001a5920, FUN_001a5990, FUN_001a5a00, operator_delete, operator_delete[] (the members' and the records' destructors)

StageFiles::~StageFiles()
{
    for (StringMap<Tim2Texture *>::Node *node = textures.first; node != nullptr; node = node->next)
        delete node->value;
    for (StringMap<StageList<StageCharacterModel *> *>::Node *node = characters.first;
         node != nullptr; node = node->next) {
        StageList<StageCharacterModel *> *list = node->value;
        for (StageList<StageCharacterModel *>::Node *entry = list->head; entry != nullptr;
             entry = entry->next) {
            delete[] entry->value->altName;
            delete entry->value;
        }
        delete list;
    }
    for (StringMap<StageFileGroup *>::Node *node = groups.first; node != nullptr; node = node->next)
        delete node->value;

    if (!sharedLoader && loader != nullptr)
        delete loader;
}

// ──────────────────────────────────────────────────────────────────────
// new[]'d strings out of a list, and the list emptied.
static void clearStrings(StageList<char *> &list)
{
    for (StageList<char *>::Node *node = list.head; node != nullptr; node = node->next)
        delete[] node->value;
    list.clear();
}

// The same for a list of lists.
static void clearLists(StageList<StageList<void *> *> &lists)
{
    for (StageList<StageList<void *> *>::Node *node = lists.head; node != nullptr; node = node->next)
        delete node->value;
    lists.clear();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001a3dc0
// callcheck: FUN_001a5a00, FUN_001a5e60, FUN_001a5f80, FUN_001a60a0, operator_delete[] (StageList::clear and clearStrings / clearLists)

void StageFiles::reset()
{
    target = nullptr;
    models.clear();
    clearStrings(modelNames);
    clearStrings(modelAltNames);
    clearLists(motionLists);
    clearStrings(motionNames);
    mdls.clear();
    collisions.clear();
    particles.clear();
    maps.clear();
    clearStrings(mapNames);
    scripts.clear();
    rutes.clear();
    sndxvs.clear();
    sndsehs.clear();
    clearLists(sndsehLists);
    streamTablePath[0] = '\0';
    fsmPath[0] = '\0';
    fonts.clear();
    areaclips.clear();
    bbxs.clear();
    lpts.clear();
}

// ──────────────────────────────────────────────────────────────────────
template <class T>
bool StageFiles::takeFile(StageList<T> &list, T *item)
{
    const char *path = strtok(nullptr, kScriptSeparators);
    if (path == nullptr)
        return false;
    loader->setDir(dataDir(path));
    if (listPass) {
        loader->addFile(path);
        *item = T();
        return true;
    }
    *item = (T)loader->getItem(loader->findIndex(path));
    if (*item == T())
        return false;
    list.pushBack(*item);
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The ROM has one chain of compares per kind of scene. A sub-scene skips
// the keywords only a stage's own systems take (the isSubScene tests).
bool StageFiles::parseLine(const char *keyword, bool isSubScene)
{
    void *item;
    switch (findWord(keyword, kScriptKeywords, ScriptKeyword_Count)) {
    case ScriptKeyword_Target:
        return parseTarget();
    case ScriptKeyword_End:
        return parseEnd();
    case ScriptKeyword_Texture:
        return parseTexture();
    case ScriptKeyword_Model:
        return parseModel();
    case ScriptKeyword_Motion:
        return isSubScene || parseMotion();
    case ScriptKeyword_Mdl:
        return isSubScene || takeFile(mdls, &item);
    case ScriptKeyword_Collision:
        if (!takeFile(collisions, &item))
            return false;
        if (item != nullptr) {
            CollisionFile *file = (CollisionFile *)item;
            if (file->magic != kCollisionMagic)
                return false;
            file->relocate();
        }
        return true;
    case ScriptKeyword_Particle:
        return takeFile(particles, &item);
    case ScriptKeyword_Map:
        return parseMap();
    case ScriptKeyword_Script: {
        StageScript *script;
        if (!takeFile(scripts, &script))
            return false;
        if (script != nullptr)
            script->relocate(true);
        return true;
    }
    case ScriptKeyword_Rute:
        return isSubScene || takeFile(rutes, &item);
    case ScriptKeyword_Sndxv:
        return isSubScene || takeFile(sndxvs, &item);
    case ScriptKeyword_Sndseh:
        return isSubScene || parseSndseh();
    case ScriptKeyword_Font:
        if (isSubScene)
            return true;
        if (strtok(nullptr, kScriptSeparators) == nullptr)
            return false;
        // The ROM also builds <path>_<language><ext> here, as parseTexture
        // does, and never reads it: the font is the init pack's. In both
        // passes.
        fonts.pushBack((FontData *)getInitItemForLanguage());
        return true;
    case ScriptKeyword_Areaclip:
        return takeFile(areaclips, &item);
    case ScriptKeyword_Bbx:
        return takeFile(bbxs, &item);
    case ScriptKeyword_Lpt:
        return takeFile(lpts, &item);
    case ScriptKeyword_Sndsth:
        return isSubScene || parseSndsth();
    case ScriptKeyword_Message:
        return isSubScene || parseMessage();
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001a20f0
// callcheck: FUN_001a5a60, FUN_001a5db0, dataDir, getGameOptions, getInitItemForLanguage, getLanguage, getLanguageCode, operator_delete[], operator_new[], parseEnd, parseMap, parseMessage, parseModel, parseMotion, parseSndseh, parseSndsth, parseTarget, parseTexture, relocate, strcasecmp, strcat, strcpy, ~StringList (parseLine and takeFile; the font's unread path; new / delete of text; lines' ctor and dtor)

bool StageFiles::parseScript(int index)
{
    // A 0-terminated copy of the item.
    int size = loader->getItemSize(index);
    char *text = new char[size + 1];
    memset(text, 0, loader->getItemSize(index) + 1);
    strncpy(text, (const char *)loader->getItem(index), loader->getItemSize(index));

    StringList lines;
    splitLines(&lines, text, true);
    delete[] text;

    reset();
    bool ok = true;
    for (StringList::Node *node = lines.head; node != nullptr; node = node->next) {
        char *line = node->value->m_text;
        if (line[0] == '\0' || line[0] == kScriptComment)
            continue;
        const char *keyword = strtok(line, kScriptSeparators);
        ok = parseLine(keyword, gStageScene->isSubScene != 0);
        if (!ok)
            break;
    }
    return ok;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001a1b50
// callcheck: strcasecmp (findWord)

bool StageFiles::parseTarget()
{
    if (target != nullptr)
        return false;
    const char *name = strtok(nullptr, kScriptSeparators);
    if (name == nullptr)
        return false;
    if (findWord(name, kScriptTargets, ScriptTarget_Count) == ScriptTarget_Count)
        return false;
    target = name;
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The first pass's end only closes the block. A stage runs every target; a
// sub-scene only script's bbx, bg, particle and map. An unknown target
// cannot be open (parseTarget).
// FS2 0x001a1820
// callcheck: FUN_00189840, operator_new, strcasecmp (StringMap::find, new SigLamp, findWord)

bool StageFiles::parseEnd()
{
    if (target == nullptr)
        return false;

    bool ok = true;
    if (!listPass) {
        StageScene *scene = gStageScene;
        int kind = findWord(target, kScriptTargets, ScriptTarget_Count);
        if (scene->isSubScene) {
            switch (kind) {
            case ScriptTarget_Script:
                if (bbxs.count > 0)
                    scene->nodeTree->setBoxes(bbxs.head->value);
                break;
            case ScriptTarget_Bg:
                ok = applyBgTarget();
                break;
            case ScriptTarget_Particle:
                ok = applyParticleTarget();
                break;
            case ScriptTarget_Map:
                ok = applyMapTarget();
                break;
            }
        } else {
            switch (kind) {
            case ScriptTarget_Script:
                ok = applyScriptTarget();
                break;
            case ScriptTarget_Camera:
                break;
            case ScriptTarget_Character:
                ok = applyCharacterTarget();
                break;
            case ScriptTarget_Bg:
                ok = applyBgTarget();
                break;
            case ScriptTarget_BgMotion:
                ok = applyBgMotionTarget();
                break;
            case ScriptTarget_Particle:
                ok = applyParticleTarget();
                break;
            case ScriptTarget_Item:
                ok = applyItemTarget();
                break;
            case ScriptTarget_Map:
                ok = applyMapTarget();
                break;
            case ScriptTarget_Sound:
                ok = applySoundTarget();
                break;
            case ScriptTarget_Effect: {
                StringMap<Tim2Texture *>::Node *lamp = textures.find(kSigLampTexture);
                if (lamp != nullptr) {
                    // As the ROM: a second effect block's lamp is dropped
                    // without being deleted.
                    SigLamp *object = new SigLamp(lamp->value);
                    if (scene->owned[0] == nullptr)
                        scene->owned[0] = object;
                }
                break;
            }
            }
        }
    }
    reset();
    return ok;
}

// ──────────────────────────────────────────────────────────────────────
// Two scripts, the rute, the font and the last texture listed are required.
// FS2 0x0019fcd0
// callcheck: FUN_001a4d80 (StringMap::at)

bool StageFiles::applyScriptTarget()
{
    StageScene *scene = gStageScene;
    if (scripts.count < 2)
        return false;
    scene->setScripts(scripts.head->value, scripts.head->next->value);
    if (rutes.count < 1)
        return false;
    scene->setRoute(rutes.head->value);
    if (fonts.count < 1)
        return false;
    scene->printer->setFont(fonts.head->value);

    // PC: at answers null with no texture listed (the ROM reads through it).
    StringMap<Tim2Texture *>::Node *last = textures.at(textures.count - 1);
    if (last == nullptr || last->value == nullptr)
        return false;
    scene->printer->setTexture(last->value, kBlockAuto);

    if (bbxs.count > 0)
        scene->nodeTree->setBoxes(bbxs.head->value);
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// One StageCharacterModel per model line, under the model's name; each
// takes two motion lists. The ROM needs two motion lists per model (it reads
// through a null node otherwise); kept.
// FS2 0x0019fac0
// callcheck: FUN_001a4b70, FUN_001a5250, FUN_001a58a0, FUN_001a59f0, FUN_001a5db0, FUN_001a6140, operator_new, operator_new[] (StringMap::insert / find, StageList::StageList / pushBack, new)

bool StageFiles::applyCharacterTarget()
{
    StageList<char *>::Node *name = modelNames.head;
    StageList<char *>::Node *altName = modelAltNames.head;
    StageList<StageList<void *> *>::Node *motions = motionLists.head;
    StageList<void *>::Node *mdl = mdls.head;

    for (StageList<void *>::Node *model = models.head; model != nullptr; model = model->next) {
        StageList<StageCharacterModel *> *list;
        StringMap<StageList<StageCharacterModel *> *>::Node *found = characters.find(name->value);
        if (found != nullptr) {
            list = found->value;
        } else {
            list = new StageList<StageCharacterModel *>;
            characters.insert(name->value, list);
        }

        StageCharacterModel *record = new StageCharacterModel;
        record->model = model->value;
        const char *alt = altName->value;
        if (alt == nullptr) {
            record->altName = nullptr;
        } else {
            record->altName = new char[strlen(alt) + 1];
            strcpy(record->altName, alt);
        }

        for (StageList<void *>::Node *entry = motions->value->head; entry != nullptr; entry = entry->next)
            record->motions.pushBack(entry->value);
        motions = motions->next;
        for (StageList<void *>::Node *entry = motions->value->head; entry != nullptr; entry = entry->next)
            record->motions2.pushBack(entry->value);

        if (mdl == nullptr) {
            record->mdl = nullptr;
        } else {
            record->mdl = mdl->value;
            mdl = mdl->next;
        }
        list->pushBack(record);

        name = name->next;
        altName = altName->next;
        motions = motions->next;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019b7a0
// callcheck: operator_new[]

void StageScene::setScripts(StageScript *script, StageScript *script2)
{
    scripts[0] = script;
    scripts[1] = script2;
    scriptSlots = new int[script->slotCount];
    for (int i = 0; i < script->slotCount; i++)
        scriptSlots[i] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00196280
// callcheck: operator_new

void StageScene::setRoute(void *rute)
{
    route = rute;
    if (routePlayer != nullptr)
        routePlayer->destroy();
    routePlayer = new RoutePlayer(rute);
    routePlayer->start(0);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001954c0

bool StageScene::isEventActive()
{
    return event != nullptr && !isSubScene && event->state == StageEvent_Active;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019ae90
// callcheck: addId

void StageScene::addArea1(int id)
{
    areas[StageArea_1]->addId(id);
}
