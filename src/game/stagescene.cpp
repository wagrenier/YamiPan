// game/stagescene.cpp -- the stage scene and the stage's files
// (game/stagescene.h).
#include "common/yamipan_stub.h"

#include "game/stagescene.h"

#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "game/clock.h"
#include "game/fileloader.h"
#include "game/fontdata.h"
#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/gsutil.h"        /* Tim2Texture */
#include "game/heap.h"          /* randNext */
#include "game/pakarchive.h"
#include "game/seplayer.h"      /* StreamTable */
#include "game/stageobjlist.h"
#include "game/stageresult.h"   /* getInitItemForLanguage */
#include "game/stagesound.h"
#include "game/stringlist.h"
#include "game/sysdata.h"       /* getStageSelect */
#include "game/thread.h"        /* threadYield */
#include "game/uiwidget.h"
#include "game/vu0math.h"
#include "sdk/libpad.h"         /* SCE_PADstart, SCE_PADselect */
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
// The init-pack textures every stage takes (StageFiles::loadCommon).
static const char kSpotlightTexture[] = "spotlight";  /* data 0x3041c8 */
static const char kShadowTexture[]    = "shadow_00";  /* data 0x3041d8 */
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

SubSceneData *StageScene::getSubSceneBlock()
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
    return areas[StageArea_Reverb]->contains(pos) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019ae30
// callcheck: addBox

void StageScene::addReverbBox(AreaBox *box)
{
    areas[StageArea_Reverb]->addBox(box);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019ae40
// callcheck: addBox

void StageScene::addSmokeBox(AreaBox *box)
{
    areas[StageArea_Smoke]->addBox(box);
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
// callcheck: ~StageObjectList, ~ScenePairTable, ~SceneRecordBuckets, ~StageNodeTree, ~StageSound, ~DemoStream, ~SceneRender, ~KeepList, ~Clock, ~MenuProgHolder, ~MenuProgSet, ~AreaGrid, ~SceneBufferSet, ~StageParty, ~ScenePool, ~PauseMenuState, ~SceneBoxList, ~SceneUiState, ~StageAlert, ~AreaClip, ~StageMission, ~SceneDirector, ~SceneModel, operator_delete, operator_delete[] (the delete-expressions; the printer through its virtual dtor)

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
    delete areaClip;

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
    areaClip = nullptr;
    lpt      = nullptr;
    player = nullptr;
    partner = controlled = nullptr;
    flag84 = 0;
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

    bgScript        = nullptr;
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
// callcheck: addBox

void StageScene::addAbBox(AreaBox *box)
{
    areas[StageArea_Ab]->addBox(box);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019af30
// callcheck: addBox

void StageScene::addFloorBox(AreaBox *box)
{
    areas[StageArea_Floor]->addBox(box);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019b540

void StageScene::setFlag84()
{
    flag84 = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019b740

void StageScene::setBgName(const char *name)
{
    strncpy(bgName, name, sizeof(bgName) - 1);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019b750
// callcheck: operator_new

void StageScene::setAreaClip(void *file)
{
    areaClip = new AreaClip(file);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00196840

void StageScene::setMessages(void *file)
{
    currentMission()->setMessages(file);
}

// ──────────────────────────────────────────────────────────────────────
// The keyword handlers: what they cut a path with, and the names they make.

static const char kPathSlash[]        = "/";            /* data 0x3040e8 */
static const char kPathDot[]          = ".";            /* data 0x3040f0 */
static const char kExtMotionPack[]    = ".mpk";         /* data 0x3040f8 */
static const char kExtTexturePack[]   = ".tpk";         /* data 0x304108 */
static const char kExtTexture[]       = ".tm2";         /* data 0x304110 */
static const char kModelNothing[]     = "nothing";      /* data 0x304100 */
static const char kMotionLineEnds[]   = "\r\n";         /* data 0x303d48 */
static const char kMotionPathSep[]    = "\\";           /* data 0x304198 */
static const char kLanguageSep[]      = "_";            /* data 0x304010 */
static const char kLanguageNone[]     = "";             /* data 0x304008 */
static const char kLanguageCodeCt[]   = "Ct";           /* data 0x304000 */
static const char kStreamDir[]        = "streamEU/";    /* data 0x303ef0 */
static const char kFsmDir[]           = "../data/sound/msg/"; /* data 0x304060 */
static const char kStreamName[]       = "Stream_s%02d_SE";    /* data 0x304050 */
static const char kStreamFile[]       = "Stream_s%02d_SE%s.sth"; /* data 0x3040a0 */
static const char kFsmFile[]          = "SC%02d.fsm";   /* data 0x304088 */
static const char kFsmFileLanguage[]  = "SC%02d_%s.fsm"; /* data 0x304078 */
// The targets' names: the particle textures' first; a door's parts.
static const char kParticleFirstTexture[] = "common01"; /* data 0x303fa8 */
static const char kDoorPartA[]        = "_dp";          /* data 0x303ff0 */
static const char kDoorPartB[]        = "_ds";          /* data 0x303ff8 */

// A path's stack buffers in the handlers (0x100 or 0x108 in the ROM,
// unchecked).
constexpr int kScriptPathSize = 0x100;
// The textures the particle target takes from common01 on.
constexpr int kParticleTextures = 0x10;
// The particle files the target needs.
constexpr int kParticleFiles = 2;

// The current language's code for a file name, empty for none. Inline in
// the ROM (parseTexture, parseMessage, parseSndsth, the font keyword).
static const char *languageCode()
{
    int language = getLanguage();
    if (language == Language_Ct)
        return kLanguageCodeCt;
    if (language < 0 || language >= Language_Count)
        return kLanguageNone;
    return getLanguageCode(language);
}

// The index of the last dot after the last slash; -1 none. text is copied
// to out with its 0 (inline in the callers).
static int copyFindExtension(char *out, const char *text)
{
    int dot = -1;
    for (int i = 0;; i++) {
        char c = out[i] = text[i];
        if (c == '/')
            dot = -1;
        else if (c == '.')
            dot = i;
        else if (c == '\0')
            break;
    }
    return dot;
}

// path with _<language> before its extension (in out), path itself with no
// language code. Inline in the ROM, see languageCode.
static const char *languageFile(char *out, const char *path)
{
    const char *code = languageCode();
    if (code == nullptr || code[0] == '\0')
        return path;
    int dot = copyFindExtension(out, path);
    if (dot >= 0)
        out[dot] = '\0';
    strcat(out, kLanguageSep);
    strcat(out, code);
    if (dot >= 0)
        strcat(out, path + dot);
    return out;
}

// ──────────────────────────────────────────────────────────────────────
// path with the options' file suffix before its extension (in out), path
// itself with an empty suffix.
// FS2 0x001a4370
// callcheck: getGameOptions, getFileSuffix

static const char *fileWithSuffix(char *out, const char *path)
{
    const char *suffix = getFileSuffix(getGameOptions()->_04);
    if (suffix[0] == '\0')
        return path;
    char name[kScriptPathSize];
    int dot = copyFindExtension(name, path);
    if (dot < 0) {
        sprintf(out, "%s%s", name, suffix);     /* data 0x304220 */
    }
    else {
        name[dot] = '\0';
        sprintf(out, "%s%s%s", name, suffix, path + dot); /* data 0x304218 */
    }
    return out;
}

// ──────────────────────────────────────────────────────────────────────
// path up to its last slash. PC: with no slash the ROM builds a range ending
// before its start; here it is empty.
// FS2 0x001a4480
// callcheck: rfind_char, string_cstr, string_range (std::string)

static std::string pathDir(const std::string &path)
{
    size_t slash = path.rfind('/');
    if (path.empty() || slash == std::string::npos)
        return std::string();
    return path.substr(0, slash);
}

// The name of a path: its last word, up to the first dot. It lives in parts
// (inline in every handler that names what it loaded).
static const char *cutFileStem(StringList &parts, const char *path)
{
    parts.split(path, kPathSlash);
    parts.splitString(*parts.tail()->value, kPathDot);
    return parts.head->value->m_text;
}

// A new[]'d copy (inline in the handlers).
static char *copyText(const char *text)
{
    char *copy = new char[strlen(text) + 1];
    strcpy(copy, text);
    return copy;
}

// The m_dir this leaves is the temporary's, as in the ROM: nothing reads it
// before the next setDir.
void StageFiles::enterDirOf(const char *path)
{
    std::string dir = loader->m_dir + pathDir(path) + kPathSlash;
    loader->setDir(dir.c_str());
}

// ──────────────────────────────────────────────────────────────────────
// A script/ path in the second pass is a texture of the init pack, named
// after the path plus the language. Any other path the first pass lists,
// and the second makes a Tim2Texture of, once per name.
// FS2 0x001a1430
// callcheck: strtok, strstr, getGameOptions, getLanguage, getLanguageCode, strcat, FUN_001a5a60, split, tail, splitString, operator_new, FUN_001a4cd0, ~StringList, FUN_00189840, strcpy (languageFile, the lists' ctor, new and insert, StringMap::find, setDir)

bool StageFiles::parseTexture()
{
    const char *path = strtok(nullptr, kScriptSeparators);
    if (path == nullptr)
        return false;

    if (strstr(path, kScriptPrefix) == path && !listPass) {
        char file[kScriptPathSize];
        StringList parts;
        const char *name = cutFileStem(parts, languageFile(file, path));
        Tim2Texture *texture = new Tim2Texture(getInitItem21());
        if (textures.insert(name, texture) == nullptr)
            delete texture;
        return true;
    }

    if (listPass) {
        loader->setDir(dataDir(path));
        loader->addFile(path);
        return true;
    }

    void *image = loader->getItem(loader->findIndex(path));
    if (image == nullptr)
        return false;
    StringList parts;
    const char *name = cutFileStem(parts, path);
    if (textures.find(name) == nullptr)
        textures.insert(name, new Tim2Texture(image));
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// model PATH [NAME [NAME2]]: a model and its .tpk beside it, whose .tm2 go
// into textures under the names the model lists; "nothing" keeps its place
// in the list with a null. One name is the model's; with two the first is
// the alternate name and the second the model's.
// FS2 0x001a0e70
// callcheck: strtok, strcmp, FUN_001a5db0, FUN_001a5a60, split, strcpy, strcat, ~StringList, operator_new, FUN_001a4cd0, FUN_00189840, FUN_001a5ed0, tail, splitString, strlen, operator_new[], pathDir, string_cstr, operator_plus_cstr, operator_plus_str, ~string, operator_delete[], PakArchive, ~PakArchive, String (the lists, new and insert, enterDirOf, copyText, String's inline dtor)

bool StageFiles::parseModel()
{
    const char *path = strtok(nullptr, kScriptSeparators);
    if (path == nullptr)
        return false;
    const char *name = strtok(nullptr, kScriptSeparators);
    const char *name2 = name != nullptr ? strtok(nullptr, kScriptSeparators) : nullptr;

    if (strcmp(path, kModelNothing) == 0) {
        if (!listPass)
            models.pushBack(nullptr);
    }
    else {
        char packPath[kScriptPathSize];
        {
            StringList parts;
            parts.split(path, kPathDot);
            strcpy(packPath, parts.head->value->m_text);
            strcat(packPath, kExtTexturePack);
        }
        loader->setDir(dataDir(path));
        if (listPass) {
            loader->addFile(path);
            loader->addFile(packPath);
            return true;
        }

        void *item = loader->getItem(loader->findIndex(path));
        if (item == nullptr)
            return false;
        models.pushBack(item);
        void *pack = loader->getItem(loader->findIndex(packPath));
        if (pack == nullptr)
            return false;
        ModelFile *model = (ModelFile *)item;
        if (!model->isValid())
            return false;
        model->relocate();

        enterDirOf(path);
        PakArchive archive;
        archive.open(pack, 0);
        for (int i = 0; i < model->textureCount; i++) {
            const char *textureName = model->textureNames[i];
            String file(textureName);
            file.append(kExtTexture);
            void *image = archive.getItem(archive.findIndex(file.m_text));
            if (image == nullptr)
                return false;
            if (textures.find(textureName) == nullptr)
                textures.insert(textureName, new Tim2Texture(image));
        }
    }

    if (!listPass) {
        char *modelName;
        char *altName = nullptr;
        if (name == nullptr) {
            StringList parts;
            modelName = copyText(cutFileStem(parts, path));
        }
        else if (name2 == nullptr) {
            modelName = copyText(name);
        }
        else {
            altName = copyText(name);
            modelName = copyText(name2);
        }
        modelAltNames.pushBack(altName);
        modelNames.pushBack(modelName);
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// motion PATH: a text listing motions and the .mpk beside it that holds
// them.
// FS2 0x001a0ae0
// callcheck: strtok, FUN_001a5a60, split, strcpy, strcat, ~StringList, pathDir, string_cstr, operator_plus_cstr, operator_plus_str, ~string, operator_new, FUN_001a59f0, FUN_001a5a00, FUN_001a5ff0, FUN_001a5ed0, tail, splitString, strlen, operator_new[], PakArchive, ~PakArchive (the lists, enterDirOf, cutFileStem, copyText)

bool StageFiles::parseMotion()
{
    const char *path = strtok(nullptr, kScriptSeparators);
    if (path == nullptr)
        return false;

    char packPath[kScriptPathSize];
    {
        StringList parts;
        parts.split(path, kPathDot);
        strcpy(packPath, parts.head->value->m_text);
        strcat(packPath, kExtMotionPack);
    }
    loader->setDir(dataDir(path));
    if (listPass) {
        loader->addFile(path);
        loader->addFile(packPath);
        return true;
    }

    int index = loader->findIndex(path);
    void *text = loader->getItem(index);
    if (text == nullptr)
        return false;
    void *pack = loader->getItem(loader->findIndex(packPath));
    if (pack == nullptr)
        return false;

    enterDirOf(path);
    StageList<void *> *list = new StageList<void *>;
    PakArchive archive;
    archive.open(pack, 0);
    if (!readMotionList((const char *)text, &archive, loader->getItemSize(index), list)) {
        delete list;
        return false;
    }
    motionLists.pushBack(list);

    StringList parts;
    motionNames.pushBack(copyText(cutFileStem(parts, path)));
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001a1df0
// callcheck: operator_new[], memset, strncpy, FUN_001a5a60, split, operator_delete[], FUN_001a5db0, tail, ~StringList (the text copy, the lists)

bool StageFiles::readMotionList(const char *text, PakArchive *pack, int size,
                                StageList<void *> *list)
{
    // A 0-terminated copy of the item.
    char *copy = new char[size + 1];
    memset(copy, 0, size + 1);
    strncpy(copy, text, size);
    StringList lines;
    lines.split(copy, kMotionLineEnds);
    delete[] copy;

    for (StringList::Node *node = lines.head; node != nullptr; node = node->next) {
        const char *line = node->value->m_text;
        if (line[0] == kScriptComment) {
            if (!listPass)
                list->pushBack(nullptr);
            continue;
        }
        StringList parts;
        parts.split(line, kMotionPathSep);
        const char *name = parts.tail()->value->m_text;
        if (listPass) {
            if (pack == nullptr)
                loader->addFile(name);
            continue;
        }
        void *item = pack != nullptr ? pack->getItem(pack->findIndex(name))
                                     : loader->getItem(loader->findIndex(name));
        if (item == nullptr)
            return false;
        list->pushBack(item);
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001a0950
// callcheck: strtok, strcpy, FUN_001a5db0, FUN_001a5a60, split, tail, splitString, strlen, operator_new[], FUN_001a5ed0, ~StringList (setDir, the lists, cutFileStem, copyText)

bool StageFiles::parseMap()
{
    const char *path = strtok(nullptr, kScriptSeparators);
    if (path == nullptr)
        return false;
    loader->setDir(dataDir(path));
    if (listPass) {
        loader->addFile(path);
        return true;
    }

    void *map = loader->getItem(loader->findIndex(path));
    if (map == nullptr)
        return false;
    maps.pushBack(map);
    {
        StringList parts;
        mapNames.pushBack(copyText(cutFileStem(parts, path)));
    }
    ((StageMapFile *)map)->relocate();
    return true;
}

// A table the sound layer takes (the same test as SePlayer::setTable).
static bool isSeTable(const SeTable *table)
{
    return memcmp(table->magic, "SEDT", sizeof(table->magic)) == 0
        && !(table->version < kSeTableVersion);
}

// Lists the file of a bank in the first pass; adds its item to list in the
// second, when it loaded. Inline in the ROM.
void StageFiles::takeBankFile(const char *file, StageList<void *> *list)
{
    const char *name = file[0] != '\0' ? file : nullptr;
    if (listPass) {
        loader->addFile(name);
        return;
    }
    void *item = loader->getItem(loader->findIndex(name));
    if (item != nullptr)
        list->pushBack(item);
}

// ──────────────────────────────────────────────────────────────────────
// sndseh PATH: an effect table and its banks' files. The first pass needs
// the table to list them, so it loads the table here.
// FS2 0x001a0580
// callcheck: strtok, fileWithSuffix, strcpy, startLoad, FUN_001a5db0, pathDir, string_cstr, operator_plus_cstr, operator_plus_str, ~string, operator_new, FUN_001a59f0, FUN_001a5ff0 (setDir, the lists, enterDirOf)

bool StageFiles::parseSndseh()
{
    const char *path = strtok(nullptr, kScriptSeparators);
    if (path == nullptr)
        return false;
    char buffer[kScriptPathSize];
    const char *file = fileWithSuffix(buffer, path);

    loader->setDir(dataDir(file));
    int index;
    if (listPass) {
        index = loader->addFile(file);
        loader->startLoad();
        loader->waitLoaded(kWaitArg, kWaitShowLoading);
    }
    else {
        index = loader->findIndex(file);
    }
    SeTable *table = (SeTable *)loader->getItem(index);
    if (table == nullptr)
        return false;
    sndsehs.pushBack(table);

    enterDirOf(file);
    StageList<void *> *bankFiles = new StageList<void *>;
    if (isSeTable(table)) {
        int bankCount = 0;
        for (int i = 0; i < kSeBanks; i++) {
            if (table->banks[i].file[0] != '\0')
                bankCount++;
        }
        for (int i = 0; i < bankCount; i++) {
            const SeBankInfo &bank = table->banks[i];
            takeBankFile(bank.file, bankFiles);
            if (bank.isStream)
                takeBankFile(bank.streamFile, bankFiles);
        }
    }
    sndsehLists.pushBack(bankFiles);
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// sndsth Stream_sNN_SE: the stage's stream table and its .fsm. The first
// pass also loads the table and lists its banks' files. (The ROM copies each
// path onto itself after building it, nothing else.)
// FS2 0x001a0040
// callcheck: strtok, sscanf, getGameOptions, getFileSuffix, sprintf, strcpy, startLoad, stdtIsRelocated, stdtEntryCount, getLanguage, getLanguageCode (setDir, StreamTable::relocate and languageCode inline)

bool StageFiles::parseSndsth()
{
    const char *name = strtok(nullptr, kScriptSeparators);
    if (name == nullptr)
        return false;
    int stage;
    if (sscanf(name, kStreamName, &stage) != 1)
        return false;
    sprintf(streamTablePath, kStreamFile, stage, getFileSuffix(getGameOptions()->_04));
    loader->setDir(kStreamDir);

    if (listPass) {
        loader->addFile(streamTablePath);
        loader->startLoad();
        loader->waitLoaded(kWaitArg, kWaitShowLoading);
        StreamTable *table = (StreamTable *)loader->getData(streamTablePath);
        if (table == nullptr || memcmp(table->magic, "StDT", sizeof(table->magic)) != 0)
            return false;
        table->relocate();
        for (int i = 0; i < table->bankCount; i++)
            loader->addFile(table->banks[i].name);
    }

    // The second sscanf reads the stage back out of the name just built.
    if (sscanf(streamTablePath, kStreamName, &stage) != 1)
        return false;
    const char *code = languageCode();
    if (code == nullptr || code[0] == '\0')
        sprintf(fsmPath, kFsmFile, stage);
    else
        sprintf(fsmPath, kFsmFileLanguage, stage, code);
    loader->setDir(kFsmDir);
    if (listPass)
        loader->addFile(fsmPath);
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// A stage's only: a sub-scene lists the file and takes nothing.
// FS2 0x0019fdf0
// callcheck: strtok, getGameOptions, getLanguage, getLanguageCode, strcat, strcpy (languageFile, setDir)

bool StageFiles::parseMessage()
{
    const char *path = strtok(nullptr, kScriptSeparators);
    if (path == nullptr)
        return false;
    char buffer[kScriptPathSize];
    const char *file = languageFile(buffer, path);

    loader->setDir(dataDir(file));
    if (listPass) {
        loader->addFile(file);
    }
    else if (!gStageScene->isSubScene) {
        void *messages = loader->getItem(loader->findIndex(file));
        if (messages == nullptr)
            return false;
        gStageScene->setMessages(messages);
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The node tree takes kParticleTextures textures, in the order they were
// added, from common01 on, then the first two particle files. PC: the ROM
// reads through a null node when fewer than that follow common01.
// FS2 0x0019f140
// callcheck: strcmp

bool StageFiles::applyParticleTarget()
{
    StageNodeTree *tree = gStageScene->nodeTree;
    StringMap<Tim2Texture *>::Node *node = textures.first;
    while (node != nullptr && strcmp(node->key, kParticleFirstTexture) != 0)
        node = node->next;
    if (node == nullptr)
        return false;
    for (int i = 0; i < kParticleTextures && node != nullptr; i++, node = node->next)
        tree->addParticleTexture(node->value);
    if (particles.count < kParticleFiles)
        return false;
    StageList<void *>::Node *file = particles.head;
    tree->setParticleFileA(file->value);
    tree->setParticleFileB(file->next->value);
    return true;
}

// The record of the door called name; -1 none (inline in applyBgTarget).
static int findDoorRecord(const char *name)
{
    for (int i = 0; i < kDoorRecords; i++) {
        if (strcmp(gDoorRecords[i].name, name) == 0)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// Every model of the block: its nodes, last first, become the stage's map
// objects (a sub-scene's own, lighter classes), then the scene takes the
// model's name, the script and the areaclip beside it; last the first lpt.
// A collision file per model is required (the ROM reads through a null node
// otherwise; kept).
// FS2 0x0019f680
// callcheck: strcmp, strstr, heapAllocAligned (the door search, the objects' aligned operator new)

bool StageFiles::applyBgTarget()
{
    StageScene *scene = gStageScene;
    StageList<void *>::Node *model = models.head;
    StageList<char *>::Node *name = modelNames.head;
    StageList<void *>::Node *collision = collisions.head;
    StageList<StageScript *>::Node *script = scripts.head;
    StageList<void *>::Node *clip = areaclips.head;

    for (; model != nullptr; model = model->next, collision = collision->next, name = name->next) {
        ModelFile *file = (ModelFile *)model->value;
        void *collisionFile = collision->value;

        for (int i = file->getNodeCount() - 1; i >= 0; i--) {
            ModelInstance *instance = file->createInstance(i, nullptr);
            const char *objectName = instance->node->name;

            if (!scene->isSubScene) {
                StageObject *object = createMapObject(objectName, collisionFile);
                if (object == nullptr)
                    continue;
                if (object->getKind() == StageObject_Door) {
                    int record = findDoorRecord(objectName);
                    object->m_doorRecord = record < 0 ? 0 : record;
                }
                object->setupMapObject(objectName, instance, &textures, collisionFile);
            }
            else if (strstr(objectName, kDoorPartA) != nullptr
                     || strstr(objectName, kDoorPartB) != nullptr) {
                SubSceneDoorObject *object = new SubSceneDoorObject(1);
                object->setup(objectName, instance, &textures, collisionFile);
                int record = findDoorRecord(objectName);
                if (record >= 0)
                    object->m_recordValue = gDoorRecords[record].value;
            }
            else if (isGateModelName(objectName)) {
                SubSceneGateObject *object = new SubSceneGateObject(1);
                object->setup(objectName, instance, &textures, collisionFile);
            }
            else {
                SubSceneMapObject *object = new SubSceneMapObject(1);
                object->setup(objectName, instance, &textures, collisionFile);
            }
        }

        scene->setBgName(name->value);
        if (script != nullptr) {
            scene->bgScript = script->value;
            script = script->next;
        }
        if (clip != nullptr) {
            scene->setAreaClip(clip->value);
            clip = clip->next;
        }
    }

    if (lpts.count > 0) {
        LptFile *lpt = (LptFile *)lpts.head->value;
        if (lpt->magic == kLptMagic) {
            lpt->magic = kLptRelocated;
            lpt->data.set((char *)lpt + lpt->data.rel);
        }
        scene->lpt = lpt;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// A map file's records are told apart by name.
static const char kMapEnv[]      = "ENV";          /* data 0x303f00 */
constexpr int     kMapEnvKindAt  = 10;             // ENV map names: 'd' here is the plain kind
constexpr char    kMapEnvKindPlain = 'd';
static const char kPointFigure[] = "F_";           /* data 0x303f08 */
static const char kPointPlayer[] = "player";       /* data 0x303f10 */
static const char kPointMark[]   = "P_";           /* data 0x303f18 */
static const char kPointBa[]     = "BA_";          /* inline compares */
static const char kEnvPointLight[] = "ENV_PL";     /* data 0x303f20 */
static const char kYamPointLight[] = "YAM_PL";     /* data 0x303f28 */
static const char kEnvBoxCb[]    = "ENV_CB";       /* data 0x303f30 */
static const char kEnvBoxSb[]    = "ENV_SB";       /* data 0x303f38 */
static const char kEnvBoxMb[]    = "ENV_MB";       /* data 0x303f40 */
static const char kEnvBoxMbFormat[] = "ENV_MB_f%02d"; /* data 0x303f48 */
static const char kEnvConeLight[] = "ENV_CL";      /* data 0x303f58 */
static const char kEnvSpotLight[] = "ENV_SL";      /* data 0x303f60 */
static const char kYamConeLight[] = "YAM_CL";      /* data 0x303f68 */
static const char kYamSpotLight[] = "YAM_SL";      /* data 0x303f70 */
static const char kEnvBoxCbFormat[] = "ENV_CB%d";  /* data 0x303f78 */
static const char kEnvBoxAb[]    = "ENV_AB";       /* data 0x303f88 */
static const char kSmokeBox[]    = "SMOKE_A";      /* data 0x303f90 */
static const char kReverbBox[]   = "REVERB_ON";    /* data 0x303f98 */
constexpr int kMapPrefixSize = 6;
constexpr int kSmokePrefixSize = 7;
// A box named after a light: "ENV_CLnn", or "ENV_CLnnc?" with two more.
constexpr int  kLightNameSize       = 8;
constexpr int  kLightNameSizeLong   = 10;
constexpr char kLightNameLongMark   = 'c';
// A point without a rotation (data 0x303e50).
static const float kNoRotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
// The map's cameras.
constexpr float kMapCameraScrZ   = 450.0f;
constexpr float kMapCameraWidth  = 512.0f;
constexpr float kMapCameraHeight = 512.0f;
constexpr float kMapCameraFar    = 1600.0f;
constexpr float kRadToDeg = 57.295776f;

// Items and characters of the map.
static const char kItemTrap[]   = "I_WANA";        /* data 0x303e90 */
static const char *const kItemsThrown[] = {
    "I_HATUEN",                 /* data 0x303e98 */
    "I_AKIKAN",                 /* data 0x303ea8 */
    "I_PET",                    /* data 0x303eb8 */
    "I_NUT",                    /* data 0x303ec0 */
};
static const char kCharacterYrt[] = "F_YRT";       /* data 0x303ec8 */
static const char kCharacterSrt[] = "F_SRT";       /* data 0x303ee0 */
static const char kCharacterStageFormat[] = "%s_S%02d"; /* data 0x303ed0 */
// F_YRT's model part, one at random (table 0x2f7db0).
static const char *const kYrtParts[] = { "ext_eyr01", "ext_eyr02", "ext_eyr03" };
constexpr int kYrtPartCount = 3;
constexpr int kSubSceneCharacterArg = 2;
constexpr int kSubSceneItemArg      = 3;

// A spawn name's point goes to the mission, any other to a new marker. Inline
// in the ROM, with and without a rotation.
static void placeMarker(const char *name, const float *pos, const float *rot)
{
    int id = parseSpawnId(name);
    if (id >= 0) {
        gStageScene->currentMission()->setSpawnPoint(id, pos, rot != nullptr ? rot : kNoRotation);
        return;
    }
    StageMarker *marker = new StageMarker;
    marker->m_xform.setPositionXyz(pos[0], pos[1], pos[2]);
    if (rot != nullptr)
        marker->m_xform.setRotation(rot[0], rot[1], rot[2]);
    marker->addToScene(name);
}

// The set of gStageObjRecords the stage kind reads.
static const StageObjRecord *stageObjRecords()
{
    int set;
    switch (getStageSelect()->kind) {
    case StageKind_Hard: set = 2; break;
    case StageKind_Easy: set = 0; break;
    default:             set = 1; break;
    }
    return gStageObjRecords[set];
}

static int findStageObjRecord(const char *name)
{
    const StageObjRecord *records = stageObjRecords();
    for (int i = 0; i < kStageObjRecordCount; i++) {
        if (strcmp(records[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int findItemDef(const char *name)
{
    for (int i = 0; i < kItemDefCount; i++) {
        if (strcmp(gItemDefs[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int findWeaponDef(const char *name)
{
    for (int i = 0; i < kWeaponDefCount; i++) {
        if (strcmp(gWeaponDefs[i].name, name) == 0)
            return i;
    }
    return -1;
}

// The class of an item, by its definition's kind and its name.
static StageItem *newItem(const char *name, u_int kind)
{
    if (kind == ItemKind_E)
        return new StageItemE;
    if (kind < ItemKind_ItemFirst)
        return new StageWeaponItem;
    if (kind == ItemKind_F) {
        if (strcmp(name, kItemTrap) == 0)
            return new StageTrapItem;
        for (const char *thrown : kItemsThrown) {
            if (strcmp(name, thrown) == 0)
                return new StageThrownItem;
        }
    }
    return new StageItem;
}

// ──────────────────────────────────────────────────────────────────────
// The item's group gives the model; the kind in its definition, or in a
// sub-scene one of two lighter classes, gives the object. An item without a
// group or a definition places nothing.
// FS2 0x0019d0c0
// callcheck: strcmp, heapAllocAligned, FUN_001a4e80, StageItem, StageItemE, StageThrownItem, StageTrapItem, StageWeaponItem, SubSceneModelObject (the definition searches, StringMap::find, the classes through newItem and SubSceneProp)

void StageFiles::placeItem(const char *name, const float *pos, const float *rot)
{
    StringMap<StageFileGroup *>::Node *node = groups.find(name);
    if (node == nullptr)
        return;
    StageFileGroup *group = node->value;
    ModelInstance *instance = ((ModelFile *)group->model)->createInstance(0, nullptr);

    int def = findItemDef(name);
    if (def < 0)
        return;
    u_int kind = gItemDefs[def].kind;

    if (gStageScene->isSubScene) {
        if (kind == ItemKind_E) {
            SubSceneItem *item = new SubSceneItem(kSubSceneItemArg);
            item->setup(name, instance, &textures);
        }
        else {
            SubSceneProp *prop = new SubSceneProp(kSubSceneItemArg);
            prop->setup(name, instance, &textures);
        }
        return;
    }

    StageItem *item = newItem(name, kind);
    item->m_itemDef = def;
    if (pos != nullptr)
        item->m_xform.setPositionXyz(pos[0], pos[1], pos[2]);
    if (rot != nullptr)
        item->m_xform.setRotation(rot[0], rot[1], rot[2]);
    item->setupItem(name, instance, &textures, &group->motions);
    if (item->getKind() == kStageObjectKindWeapon) {
        item->m_weaponDef = findWeaponDef(name);
        item->initWeapon();
    }
}

// ──────────────────────────────────────────────────────────────────────
// The character's last model line. F_YRT and F_SRT have a record per stage
// (name_Snn); the ROM searches for the plain name too when that fails, then
// drops the answer, so they place nothing without one (kept). The record's
// items are placed first, without a position.
// FS2 0x0019d4e0
// callcheck: strcmp, heapAllocAligned, FUN_001a5250, FUN_001a6110, StageCharacter (the record searches, StringMap::find, the tail search written as a loop, the new)

void StageFiles::placeCharacter(const char *name, const float *pos, const float *rot)
{
    StringMap<StageList<StageCharacterModel *> *>::Node *node = characters.find(name);
    if (node == nullptr)
        return;
    StageList<StageCharacterModel *>::Node *line = node->value->head;
    while (line->next != nullptr)
        line = line->next;
    StageCharacterModel *model = line->value;

    const char *part = strcmp(name, kCharacterYrt) == 0
                     ? kYrtParts[randNext() % kYrtPartCount] : model->altName;
    ModelInstance *instance = nullptr;
    if (model->model != nullptr)
        instance = ((ModelFile *)model->model)->createInstance(0, part);

    int record;
    if (strcmp(name, kCharacterSrt) == 0 || strcmp(name, kCharacterYrt) == 0) {
        char stageName[0x20];
        sprintf(stageName, kCharacterStageFormat, name, getStageSelect()->stage);
        record = findStageObjRecord(stageName);
    }
    else {
        record = findStageObjRecord(name);
    }
    if (record < 0)
        return;

    const StageObjRecord *entry = &stageObjRecords()[record];
    for (const char *item : entry->items) {
        if (strlen(item) != 0)
            placeItem(item, nullptr, nullptr);
    }

    if (gStageScene->isSubScene) {
        SubSceneCharacter *character = new SubSceneCharacter(kSubSceneCharacterArg);
        character->setup(name, instance, &textures);
        return;
    }
    StageCharacter *character = new StageCharacter;
    character->m_itemDef = record;
    if (pos != nullptr)
        character->m_xform.setPositionXyz(pos[0], pos[1], pos[2]);
    if (rot != nullptr)
        character->m_xform.setRotation(rot[0], rot[1], rot[2]);
    character->setup(name, instance, &textures, &model->motions, &model->motions2, model->mdl);
}

// ──────────────────────────────────────────────────────────────────────
// Characters (F_, player), marks (P_, BA_) and items (the rest).

void StageFiles::placeMapPoints(const StageMapFile *map)
{
    const StageMapPoint *points = map->points;
    for (int i = 0; i < map->pointCount; i++) {
        const StageMapPoint *point = &points[i];
        const char *name = point->name;
        if (strstr(name, kPointFigure) != nullptr || strstr(name, kPointPlayer) != nullptr)
            placeCharacter(name, point->pos, point->rot);
        else if (strstr(name, kPointMark) != nullptr
                 || strncmp(name, kPointBa, sizeof(kPointBa) - 1) == 0)
            placeMarker(name, point->pos, point->rot);
        else
            placeItem(name, point->pos, point->rot);
    }
}

void StageFiles::placeMapCameras(const StageMapFile *map)
{
    const StageMapCamera *cameras = map->cameras;
    for (int i = 0; i < map->cameraCount; i++) {
        const StageMapCamera *record = &cameras[i];
        StageCamera *camera = new StageCamera(kMapCameraScrZ, kMapCameraWidth, kMapCameraHeight,
                                              kMapCameraFar);
        camera->addToScene(record->name);
        camera->m_viewActive = 0;
        camera->m_xform.setPositionXyz(record->pos[0], record->pos[1], record->pos[2]);
        sceVu0FVECTOR target = { record->pos[0] + record->aim[0], record->pos[1] + record->aim[1],
                                 record->pos[2] + record->aim[2], 0.0f };
        camera->lookAt(target);
    }
}

// A sub-scene's ENV_ light takes what the parent's light of its name shows.
static void copyParentLight(StageLight *light)
{
    if (!gStageScene->isSubScene)
        return;
    StageObject *other = gStageScene->parent->objects->find5(light->m_name, 0);
    light->copyShownFrom(static_cast<StageLight *>(other));
}

// The first pass: the ENV_ lights exist once, whatever the number of maps
// that name them; the YAM_ ones are a stage's, and start off. The second
// pass's YAM_PL test reads the spot section, at the light's index, in the
// ROM (kept): a YAM_PL record ends up a SphereVolume unless the spot record
// of that index happens to be named YAM_PL.

void StageFiles::placeMapLights(const StageMapFile *map)
{
    StageScene *scene = gStageScene;
    const StageMapSpot *spots = map->spots;
    for (int i = 0; i < map->spotCount; i++) {
        const StageMapSpot *record = &spots[i];
        bool env = strncmp(record->name, kEnvConeLight, kMapPrefixSize) == 0
                || strncmp(record->name, kEnvSpotLight, kMapPrefixSize) == 0;
        bool yam = !env && (strncmp(record->name, kYamConeLight, kMapPrefixSize) == 0
                         || strncmp(record->name, kYamSpotLight, kMapPrefixSize) == 0);
        if (env ? scene->objects->find5(record->name, 0) != nullptr
                : !yam || scene->isSubScene)
            continue;

        SpotLight *light = new SpotLight;
        light->m_xform.setPositionXyz(record->pos[0], record->pos[1], record->pos[2]);
        light->m_xform.setRotation(record->rot[0], record->rot[1], record->rot[2]);
        light->setConeAngle((int)(record->angle / 2.0f * kRadToDeg));
        light->setRadiusSquared(record->radius * record->radius);
        if (env) {
            light->setVector(1.0f, 0.0f, 0.0f);
            light->m_envLight = 1;
            light->addToScene(record->name, true);
            copyParentLight(light);
        }
        else {
            light->m_yamLight = 1;
            light->setEnabled(false);
            light->addToScene(record->name, true);
        }
    }

    const StageMapLight *lights = map->lights;
    for (int i = 0; i < map->lightCount; i++) {
        const StageMapLight *record = &lights[i];
        if (strncmp(record->name, kEnvPointLight, kMapPrefixSize) == 0) {
            if (scene->objects->find5(record->name, 0) != nullptr)
                continue;
            PointLight *light = new PointLight;
            light->m_xform.setPositionXyz(record->pos[0], record->pos[1], record->pos[2]);
            light->setRadiusSquared(record->radius * record->radius);
            light->setVector(1.0f, 0.0f, 0.0f);
            light->m_envLight = 1;
            light->addToScene(record->name, true);
            copyParentLight(light);
        }
        else if (strncmp(spots[i].name, kYamPointLight, kMapPrefixSize) == 0) {
            if (scene->isSubScene)
                continue;
            PointLight *light = new PointLight;
            light->m_xform.setPositionXyz(record->pos[0], record->pos[1], record->pos[2]);
            light->setRadiusSquared(record->radius * record->radius);
            light->m_yamLight = 1;
            light->setEnabled(false);
            light->addToScene(record->name, true);
        }
        else if (!scene->isSubScene) {
            SphereVolume *volume = new SphereVolume;
            volume->m_xform.setPositionXyz(record->pos[0], record->pos[1], record->pos[2]);
            volume->setRadius(record->radius);
            volume->addToScene(record->name);
        }
    }
}

// A box of the class, placed (inline for each kind in the ROM).
template <class T>
static T *newMapBox(const StageMapBox *record)
{
    T *box = new T;
    box->m_xform.setPositionXyz(record->pos[0], record->pos[1], record->pos[2]);
    box->m_xform.setRotation(record->rot[0], record->rot[1], record->rot[2]);
    box->setSize(record->size[0], record->size[2], record->size[1]);
    return box;
}

// The ENV_ boxes feed the renderer's and the scene's grids; a box named after
// a light (its first 8 or 10 characters) joins that light; on a stage,
// SMOKE_A and REVERB_ON go to their grids and any other name is a BoxVolume.

void StageFiles::placeMapBoxes(const StageMapFile *map)
{
    StageScene *scene = gStageScene;
    const StageMapBox *boxes = map->boxes;
    for (int i = 0; i < map->boxCount; i++) {
        const StageMapBox *record = &boxes[i];
        const char *name = record->name;
        if (strncmp(name, kEnvBoxCb, kMapPrefixSize) == 0) {
            CbAreaBox *box = newMapBox<CbAreaBox>(record);
            sscanf(name, kEnvBoxCbFormat, &box->m_number);
            box->updateMatrix();
            scene->render->addCbBox(box);
        }
        else if (strncmp(name, kEnvBoxSb, kMapPrefixSize) == 0) {
            AreaBox *box = newMapBox<AreaBox>(record);
            box->updateMatrix();
            scene->render->addSbBox(box);
        }
        else if (strncmp(name, kEnvBoxMb, kMapPrefixSize) == 0) {
            if (scene->isSubScene)
                continue;
            FloorAreaBox *box = newMapBox<FloorAreaBox>(record);
            sscanf(name, kEnvBoxMbFormat, &box->m_floor);
            box->updateMatrix();
            scene->addFloorBox(box);
        }
        else if (strncmp(name, kEnvBoxAb, kMapPrefixSize) == 0) {
            if (scene->isSubScene)
                continue;
            AreaBox *box = newMapBox<AreaBox>(record);
            box->updateMatrix();
            scene->addAbBox(box);
        }
        else if (strncmp(name, kEnvConeLight, kMapPrefixSize) == 0
                 || strncmp(name, kEnvSpotLight, kMapPrefixSize) == 0
                 || strncmp(name, kEnvPointLight, kMapPrefixSize) == 0) {
            char lightName[0x10];
            memset(lightName, 0, sizeof(lightName));
            strncpy(lightName, name,
                    name[kLightNameSize] == kLightNameLongMark ? kLightNameSizeLong : kLightNameSize);
            StageObject *light = scene->objects->find5(lightName, 0);
            if (light != nullptr) {
                AreaBox *box = newMapBox<AreaBox>(record);
                box->updateMatrix();
                static_cast<StageLight *>(light)->m_boxes.pushBack(box);
            }
        }
        else if (!scene->isSubScene) {
            if (strncmp(name, kSmokeBox, kSmokePrefixSize) == 0) {
                AreaBox *box = newMapBox<AreaBox>(record);
                box->updateMatrix();
                scene->addSmokeBox(box);
            }
            else if (strcmp(name, kReverbBox) == 0) {
                AreaBox *box = newMapBox<AreaBox>(record);
                box->updateMatrix();
                scene->addReverbBox(box);
            }
            else {
                BoxVolume *box = new BoxVolume;
                box->m_xform.setPositionXyz(record->pos[0], record->pos[1], record->pos[2]);
                box->m_xform.setRotation(record->rot[0], record->rot[1], record->rot[2]);
                box->setSize(record->size[0], record->size[2], record->size[1]);
                box->addToScene(name);
            }
        }
    }
}

void StageFiles::placeMapMarks(const StageMapFile *map)
{
    const StageMapMark *marks = map->marks;
    for (int i = 0; i < map->markCount; i++)
        placeMarker(marks[i].name, marks[i].pos, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// Every map of the block, one section after the other. A stage takes all the
// sections of every map but a plain ENV map (kind 'd'), which it skips. A
// sub-scene takes the lights of an ENV map of another kind and the boxes of
// any other map, nothing else.
// FS2 0x0019ddf0
// callcheck: strncmp, strcmp, memset, strncpy, sscanf, heapAllocAligned, AreaBox, BoxVolume, FUN_001a5d00, PointLight, SphereVolume, SpotLight, StageCamera, StageObject, addAbBox, addCbBox, addFloorBox, addReverbBox, addSbBox, addSmokeBox, addToScene, copyShownFrom, currentMission, find5, lookAt, parseSpawnId, placeCharacter, placeItem, setConeAngle, setEnabled, setPositionXyz, setRadius, setRadiusSquared, setRotation, setSize, setSpawnPoint, setVector (the sections, inline in the ROM: placeMap*, placeMarker, newMapBox)

bool StageFiles::applyMapTarget()
{
    StageScene *scene = gStageScene;
    StageList<char *>::Node *name = mapNames.head;
    for (StageList<void *>::Node *node = maps.head; node != nullptr;
         node = node->next, name = name->next) {
        const StageMapFile *map = (const StageMapFile *)node->value;

        bool envMap = false;
        if (strstr(name->value, kMapEnv) != nullptr) {
            if (name->value[kMapEnvKindAt] != kMapEnvKindPlain)
                envMap = true;
            else if (!scene->isSubScene)
                continue;
        }

        if (!scene->isSubScene) {
            placeMapPoints(map);
            placeMapCameras(map);
        }
        if (!scene->isSubScene || envMap)
            placeMapLights(map);
        if (!scene->isSubScene || !envMap)
            placeMapBoxes(map);
        if (!scene->isSubScene)
            placeMapMarks(map);
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// Every model of the block whose name is new becomes a group with a copy of
// the motion list beside it.
// FS2 0x0019f020
// callcheck: operator_new, FUN_001a4a10, FUN_001a4e80, FUN_001a59f0, FUN_001a5db0 (the group's new and the templates StringMap::insert / find, StageList ctor / pushBack)

bool StageFiles::applyItemTarget()
{
    StageList<char *>::Node *name = modelNames.head;
    StageList<StageList<void *> *>::Node *motions = motionLists.head;
    for (StageList<void *>::Node *model = models.head; model != nullptr;
         model = model->next, name = name->next) {
        if (groups.find(name->value) != nullptr)
            continue;
        StageFileGroup *group = new StageFileGroup;
        group->model = model->value;
        if (motions != nullptr) {
            for (StageList<void *>::Node *motion = motions->value->head; motion != nullptr;
                 motion = motion->next)
                group->motions.pushBack(motion->value);
            motions = motions->next;
        }
        groups.insert(name->value, group);
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
Tim2Texture *StageFiles::commonTexture(const char *name, const void *image)
{
    StringMap<Tim2Texture *>::Node *node = textures.find(name);
    if (node != nullptr)
        return node->value;
    Tim2Texture *texture = new Tim2Texture(image);
    textures.insert(name, texture);
    return texture;
}

// ──────────────────────────────────────────────────────────────────────
// What every stage has, from the init pack: the renderer's spotlight and
// shadow textures, the party's and the scene's fill image, the menu program.
// FS2 0x001a38e0
// callcheck: getInitPackItem, operator_new, Tim2Texture, FUN_00189840, FUN_001a4cd0, setSpotlightTexture, setShadowTexture (commonTexture: StringMap::find / insert; the render calls are in the same block)

void StageFiles::loadCommon()
{
    StageScene *scene = gStageScene;
    loader->setDir(nullptr);

    const void *image = getInitPackItem(InitItem_Spotlight);
    scene->render->setSpotlightTexture(commonTexture(kSpotlightTexture, image));
    image = getInitPackItem(InitItem_Shadow);
    scene->render->setShadowTexture(commonTexture(kShadowTexture, image));

    image = getInitPackItem(InitItem_Fill);
    if (scene->party != nullptr)
        scene->party->setFillImage(image);
    scene->screenFill->setImage(image);

    scene->createMenuProg(getInitPackItem(InitItem_MenuProg));
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019af40
// callcheck: operator_new, getStageSelect, MenuProgHolder::MenuProgHolder

void StageScene::createMenuProg(const void *data)
{
    menuProg = new MenuProgHolder(data, getStageSelect()->stage);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001957a0
// callcheck: getMission

bool StageScene::isBusy()
{
    if (clear != nullptr || gameOver != nullptr || getMission()->busy)
        return true;
    return event != nullptr && event->runner != gRunningScript;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00195850
// callcheck: isFullInMode1, isFullInMode2, isBusy

bool StageScene::isInputBlocked()
{
    return fade->isFullInMode1() || fade->isFullInMode2() || isBusy();
}

// What StageScene::playerAction answers; which are what is unread but for
// their sources.
enum PlayerAction
{
    PlayerAction_None    = 0,
    PlayerAction_1       = 1,
    PlayerAction_9       = 9,       // StageObject +0x400 set and mode 1
    PlayerAction_Owned2  = 0xa,     // StageScene::owned[2] busy
    PlayerAction_Owned1  = 0xb,
    PlayerAction_Owned0  = 0xc,
    PlayerAction_Alt     = 0x12,    // getAltSoundFlag, m_altAction clear
    PlayerAction_AltB    = 0x13,    // getAltSoundFlag, m_altAction set
    PlayerAction_20      = 0x14,
};

// The UiShowFlags the stage's menus need.
constexpr int kShowFlagPauseMenu = 1;
constexpr int kShowFlagMapMenu   = 2;

static bool actionAllowsMenu(int action)
{
    return action == PlayerAction_20 || action == PlayerAction_AltB || action == PlayerAction_9
        || action == PlayerAction_1 || action == PlayerAction_None;
}

// Inline in wantsMapMenu and wantsPauseMenu.
int StageScene::playerAction()
{
    StageObject *character = player != nullptr ? player->character() : nullptr;
    if (character != nullptr && character->getAltSoundFlag())
        return character->m_altAction == 0 ? PlayerAction_Alt : PlayerAction_AltB;
    if (owned[2] != nullptr && owned[2]->isBusy())
        return PlayerAction_Owned2;
    if (owned[1] != nullptr && owned[1]->isBusy())
        return PlayerAction_Owned1;
    if (owned[0] != nullptr && owned[0]->isBusy())
        return PlayerAction_Owned0;
    StageObject *actor = controlled != nullptr ? controlled : character;
    return actor != nullptr ? actor->getActionState() : PlayerAction_None;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001958b0
// callcheck: isInputBlocked, getAltSoundFlag, getActionState

bool StageScene::wantsMapMenu()
{
    if ((gGameSystem->m_pad0.m_pressed & SCE_PADselect) == 0 || isInputBlocked()
        || isSubScene || !gStageScene->showFlags->flag[kShowFlagMapMenu])
        return false;
    return actionAllowsMenu(playerAction());
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00195a50
// callcheck: isInputBlocked, getAltSoundFlag, getActionState

bool StageScene::wantsPauseMenu()
{
    if ((gGameSystem->m_pad0.m_pressed & SCE_PADstart) == 0 || isInputBlocked())
        return false;
    if (player != nullptr && !player->character()->m_canPause)
        return false;
    if (isSubScene || !gStageScene->showFlags->flag[kShowFlagPauseMenu])
        return false;
    return actionAllowsMenu(playerAction());
}

// ──────────────────────────────────────────────────────────────────────
// The look angle's window (StageScene::partnerResponds).
constexpr float kLookWindowHigh = -160.0f;
constexpr float kLookWindowLow  = -190.0f;

// FS2 0x001953f0
// callcheck: isPlayer

bool StageScene::partnerResponds(StageObject *object)
{
    if (!object->m_3c0)
        return false;
    if (!object->m_3c3 && object->m_290 != nullptr)
        return false;
    if (!object->isPlayer())
        return false;
    PlayerLookState *look = object->m_motion->look;
    if (!look->active || kLookWindowHigh < look->angle || look->angle < kLookWindowLow)
        return false;
    return partner != nullptr && partner->m_3c1 != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00195520
// callcheck: currentMission, onRestore, restoreFlags, stageObjectIsShown, setActive, setShown

void StageScene::restoreObjectFlags()
{
    currentMission()->onRestore();
    StageObject *kept = currentMission()->keptObject;
    if (kept == nullptr) {
        arrays->restoreFlags();
        return;
    }
    bool active = kept->m_active != 0;
    bool shown = stageObjectIsShown(kept);
    u_char viewActive = kept->m_viewActive;
    arrays->restoreFlags();
    kept->m_tree->setActive(active);
    kept->m_tree->setShown(active);
    kept->m_tree->setShown(shown);
    kept->m_viewActive = viewActive;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00196300
// callcheck: getDeltaSeconds

int stageFps()
{
    return (int)(1.0f / gStageScene->clock->getDeltaSeconds());
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019b550

void StageScene::setFlag84To(int value)
{
    flag84 = value;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019bb60: a tail jump.
// callcheck: remove

void StageScene::removeView(StageObject *object)
{
    views->remove(object);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019cb50
// callcheck: onViewTop

void StageViewStack::remove(StageObject *object)
{
    int index = 0;
    while (index <= top && entries[index].object != object)
        index++;
    if (index > top)
        return;
    for (int i = index + 1; i <= top; i++)
        entries[i - 1] = entries[i];
    top--;
    object->m_viewActive = 0;

    StageObject *next = topObject();
    if (next != nullptr) {
        if (!next->m_viewActive)
            next->onViewTop();
        next->m_viewActive = 1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The floor number of the highest ENV_MB box the point 50 units above the
// player's feet stands in; -1 in none.
constexpr float kFloorProbeHeight = 50.0f;
constexpr int   kFloorProbeBoxes  = 5;

// FS2 0x0019aea0
// callcheck: collect

int StageScene::getFloor(StagePlayer *who)
{
    sceVu0FVECTOR pos = { who->posX, who->posZ, who->height + kFloorProbeHeight, who->posW };
    AreaBox *boxes[kFloorProbeBoxes];
    int count = areas[StageArea_Floor]->collect(pos, boxes, kFloorProbeBoxes);
    if (count < 1)
        return -1;
    int floor = 0;
    for (int i = 0; i < count; i++) {
        int number = static_cast<FloorAreaBox *>(boxes[i])->m_floor;
        if (floor < number)
            floor = number;
    }
    return floor;
}

// ──────────────────────────────────────────────────────────────────────
// The demo voice's volume and its path buffer in enterSubScene.
constexpr int kDemoVoiceVolume = 0x80;
constexpr int kDemoVoicePathSize = 0x80;

// Runs the pack's script in a new SubSceneObject under the parent's event,
// builds the cameras, then readies the event's subtitles and voice: the voice
// starts paused and enterSubScene waits for its first data.
// FS2 0x00195e10
// callcheck: FUN_0026c8b0, operator_new, relocate, scriptPrepare, isSkippable, setEntry, run, setupCameras, usesLanguageFont, setFsm, setFont, getInitItemForLanguage, getInitItem21, getVoicePath, open, play, setPaused, getDataWritten, threadYield

void StageScene::enterSubScene(void *data)
{
    subSceneBlock = new SubSceneData(data);
    StageScript *script = subSceneBlock->getScript();
    if (script != nullptr) {
        script->relocate(true);
        scriptPrepare(script);
        if (script->count > 1)
            bgScript = script;
        subScene = new SubSceneObject(parent->event->isSkippable());
        subScene->setEntry(script->entry(script->count - 1));
        subScene->run(0);
    }
    setupCameras();

    StageEvent *ev = parent->event;
    if (ev->usesLanguageFont()) {
        if (ev->fsm != nullptr) {
            demoStream->setFsm(ev->fsm);
            demoStream->setFont(getInitItemForLanguage(), getInitItem21());
        }
    }
    else if (ev->fsm != nullptr && ev->subtitleImage != nullptr && ev->subtitleFont != nullptr) {
        demoStream->setFsm(ev->fsm);
        demoStream->setFont(ev->subtitleFont, ev->subtitleImage);
    }

    if (ev->voice != nullptr) {
        char path[kDemoVoicePathSize];
        demoStream->open(ev->voice, ev->getVoicePath(path));
        demoStream->play(kDemoVoiceVolume);
        demoStream->setPaused(true);
        while (demoStream->getDataWritten() < 1)
            threadYield(1);
        demoStream->setPaused(false);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Template instances of this file, one source in game/stagelist.h (a few
// more use the host's <string>):
//   FS2 0x001a4960  StringMap::StringMap
//   FS2 0x001a4980  StringMap::~StringMap
//   FS2 0x001a4ac0  StringMap::StringMap
//   FS2 0x001a4ae0  StringMap::~StringMap
//   FS2 0x001a4a10  StringMap::insert
//   FS2 0x001a4b70  StringMap::insert
//   FS2 0x001a4c20  StringMap::StringMap
//   FS2 0x001a4c40  StringMap::~StringMap
//   FS2 0x001a4cd0  StringMap::insert
//   FS2 0x001a4d80  StringMap::at
//   FS2 0x001a4db0  StringMap::Node::Node
//   FS2 0x001a4e20  StringMap::Node::~Node
//   FS2 0x001a4e80  StringMap::find
//   FS2 0x001a4f00  StringMap::insert (the search)
//   FS2 0x001a5180  StringMap::insert
//   FS2 0x001a51f0  StringMap::Node::~Node
//   FS2 0x001a5250  StringMap::find
//   FS2 0x001a52d0  StringMap::insert (the search)
//   FS2 0x001a5550  StringMap::insert
//   FS2 0x001a55c0  StringMap::Node::~Node
//   FS2 0x001a5620  StringMap::insert (the search)
//   FS2 0x001a58a0  StageList::StageList
//   FS2 0x001a58b0  StageList::~StageList
//   FS2 0x001a5910  StageList::StageList
//   FS2 0x001a5920  StageList::~StageList
//   FS2 0x001a5980  StageList::StageList
//   FS2 0x001a5990  StageList::~StageList
//   FS2 0x001a59f0  StageList::StageList
//   FS2 0x001a5a00  StageList::~StageList
//   FS2 0x001a5a60  StringList's base constructor (StringList::StringList)
//   FS2 0x001a5a70  operator + (const char *, const std::string &)
//   FS2 0x001a5b70  operator + (const std::string &, const char *)
//   FS2 0x001a5c70  std::string (first, last)
//   FS2 0x001a5d00  StageList::pushBack
//   FS2 0x001a5db0  StageList::pushBack
//   FS2 0x001a5e60  StageList::clear
//   FS2 0x001a5ed0  StageList::pushBack
//   FS2 0x001a5f80  StageList::clear
//   FS2 0x001a5ff0  StageList::pushBack
//   FS2 0x001a60a0  StageList::clear
//   FS2 0x001a6110  StageList::pushBack (the tail search)
//   FS2 0x001a6140  StageList::pushBack
//   FS2 0x001a6210  StageList::clear
//   FS2 0x001a6280  std::string::rfind (char, position)
//   FS2 0x001a6310  std::string (const char *)
//   FS2 0x001a6390  StageList::pushBack (the first node)
//   FS2 0x001a6420  StageList::pushBack (the first node)
//   FS2 0x001a64b0  StageList::pushBack (the tail search)
//   FS2 0x001a64e0  StageList::pushBack (the first node)
//   FS2 0x001a6570  StageList::pushBack (the tail search)
//   FS2 0x001a65a0  StageList::pushBack (the first node)
//   FS2 0x001a6630  StageList::pushBack (the tail search)
//   FS2 0x001a6660  StageList::pushBack (the first node)
//   FS2 0x001a66f0  StageList::Node::Node
//   FS2 0x001a6710  StageList::Node::Node
//   FS2 0x001a6730  StageList::Node::Node
//   FS2 0x001a6750  StageList::Node::Node
//   FS2 0x001a6770  StageList::Node::Node
//   FS2 0x001a6790  StageList::pushBack (the tail search)

// ---- stubs (tools/reverse/stubgen.py) ----

// stub of FS2 0x0019f230
bool StageFiles::applyBgMotionTarget()
{
    YAMIPAN_STUB("StageFiles::applyBgMotionTarget", 0x0019f230);
    return {};
}

// stub of FS2 0x0019d9e0
bool StageFiles::applySoundTarget()
{
    YAMIPAN_STUB("StageFiles::applySoundTarget", 0x0019d9e0);
    return {};
}

// stub of FS2 0x001a3a60
void StageFiles::loadList()
{
    YAMIPAN_STUB("StageFiles::loadList", 0x001a3a60);
}

// stub of FS2 0x00198970
void StageScene::draw(StageObject *active)
{
    YAMIPAN_STUB("StageScene::draw", 0x00198970);
}

// stub of FS2 0x00199f00
void StageScene::setupCameras()
{
    YAMIPAN_STUB("StageScene::setupCameras", 0x00199f00);
}

// stub of FS2 0x00199400
void StageScene::update()
{
    YAMIPAN_STUB("StageScene::update", 0x00199400);
}


// stub of FS2 0x0019c6a0
void SceneArrayPair::restoreFlags()
{
    YAMIPAN_STUB("SceneArrayPair::restoreFlags", 0x0019c6a0);
}
