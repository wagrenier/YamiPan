// game/stagescene.cpp -- the stage scene and the stage's files
// (game/stagescene.h).
#include "common/yamipan_stub.h"

#include "game/characterbrain.h"
#include "game/characterroute.h"
#include "game/playermotion.h"
#include "game/scenerender.h"
#include "game/stageparticle.h"
#include "game/stageparty.h"
#include "game/stagescene.h"

#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "game/clock.h"
#include "game/fileloader.h"
#include "game/fontdata.h"
#include "game/characterinput.h"
#include "game/gamehooks.h"
#include "game/gameoversequence.h"
#include "game/gamepacket.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/gsblockmap.h"
#include "game/perfmeter.h"
#include "game/gsutil.h"        /* Tim2Texture */
#include "game/heap.h"          /* randNext */
#include "game/modelpose.h"
#include "game/pakarchive.h"
#include "game/routeplayer.h"
#include "game/rumblelist.h"
#include "game/seplayer.h"      /* StreamTable */
#include "game/stageitem.h"
#include "game/stagelight.h"
#include "game/stageobjlist.h"
#include "game/stageresult.h"   /* getInitItemForLanguage */
#include "game/stagesound.h"
#include "game/stringlist.h"
#include "game/subsceneobject.h"
#include "game/sysdata.h"       /* getStageSelect */
#include "game/thread.h"        /* threadYield */
#include "game/uiwidget.h"
#include "game/viewcamera.h"
#include "game/vu0math.h"
#include "game/stagecar.h"
#include "sdk/libgraph.h"       /* SCE_GS_ALPHA_1, SCE_GS_TEST_1 */
#include "sdk/libpad.h"         /* SCE_PADstart, SCE_PADselect */
#include "sdk/libvu0.h"
#include "sdk/scetypes.h"
#include "yamipan/rendering/yamipan_drawui.h"

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
            obj->m_dead)
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

    bool sceneFlag = (player != nullptr && player->isMode1()) || targetInView();
    sound->update(listener, playerFlag, sceneFlag);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019ae10
// callcheck: contains

bool StageScene::getSoundAttr(const sceVu0FVECTOR pos)
{
    return areas[StageArea_Reverb]->contains(pos) != nullptr;
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

StageObject *StageScene::getTopView()
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
// callcheck: ~StageObjectList, ~ScenePairTable, ~SceneRecordBuckets, ~StageNodeTree, ~StageSound, ~DemoStream, ~SceneRender, ~RumbleList, ~Clock, ~MenuProgHolder, ~MenuProgSet, ~AreaGrid, ~SceneBufferSet, ~StageParty, ~ScenePool, ~PauseMenuState, ~SceneBoxList, ~SceneNoiseList, ~StageAlert, ~AreaClip, ~StageMission, ~SceneDirector, ~DaylightFilm, operator_delete, operator_delete[] (the delete-expressions; the printer through its virtual dtor)

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
    delete rumble;
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
    delete effects;
    delete noises;
    delete showFlags;
    delete alert;
    if (fade != nullptr)
        delete fade;
    if (routePlayer != nullptr)
        routePlayer->destroy();
    delete[] viewTargets;
    delete areaClip;

    if (isSubScene) {
        if (subScene != nullptr)
            delete subScene;
        operator delete(subSceneBlock);
        subScene      = nullptr;
        subSceneBlock = nullptr;
        if (demoStream != nullptr && demoStream->isPlaying())
            demoStream->stop();
    }

    delete rng;
    delete mission;
    if (event != nullptr)
        delete event;
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
    delete daylight;
}

// ──────────────────────────────────────────────────────────────────────
// A stage builds everything; a sub-scene skips the stage-only members
// (null) and gets a demo voice instead of a sound, plus its own draw
// record. Who / files / the view target and the sequences are set later
// (stageThread, the scene's update).
// FS2 0x0019c180
// callcheck: operator_new, StageObjectList, operator_new, ScenePairTable, operator_new, SceneRecordBuckets, operator_new, StageNodeTree, operator_new, StageSound, resetStageGroupTable, operator_new, DemoStream, setDir, operator_new, SceneRender, operator_new, RumbleList, operator_new, operator_new[], createChild, operator_new, ScreenFill, operator_new, WideFontPrinter, operator_new, SceneBoxList, operator_new, StageFade, operator_new, ScenePool, operator_new, PauseMenuState, operator_new, UiItemList, operator_new, StageMenu, operator_new, SceneNoiseList, operator_new, UiShowFlags, operator_new, StageAlert, operator_new, Rand48, operator_new, StageMission, operator_new, AreaGrid, operator_new, AreaGrid, operator_new, AreaGrid, operator_new, AreaGrid, operator_new, StageParty, drawSysPush

StageScene::StageScene(bool isSubScene, StageSound *sound)
{
    this->isSubScene = isSubScene;
    mission    = nullptr;
    event      = nullptr;
    car = nullptr;
    zs9901Sought = 0;
    zs9901     = nullptr;
    gameOver   = nullptr;
    clear      = nullptr;
    arrays     = nullptr;
    daylight   = nullptr;
    director   = nullptr;   // PC: the ROM leaves +0x120 to whoever made the scene's memory
    camerasReady = 0;

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
    rumble = this->isSubScene ? nullptr : new RumbleList();

    views           = new StageViewStack;
    views->capacity = kViewStackSize;
    views->entries  = new StageViewEntry[views->capacity];
    views->top      = -1;

    clock = gGameSystem->m_clock.createChild();
    frameCount = 0;
    scripts[0] = scripts[1] = nullptr;
    scriptSlots = nullptr;
    route       = nullptr;
    routePlayer = nullptr;
    viewTargetCount = 0;
    viewTargets     = nullptr;
    areaClip = nullptr;
    lpt      = nullptr;
    player = nullptr;
    partner = controlled = nullptr;
    controlRequest = 0;
    owned[0] = owned[1] = owned[2] = nullptr;
    block98 = nullptr;
    buffers = nullptr;

    screenFill = new ScreenFill(kScreenFillRgba, kScreenX, kScreenY, kScreenW, kScreenH);
    menuProg   = nullptr;
    menuProgs  = nullptr;
    printer    = new WideFontPrinter(kScreenX, kScreenY);
    inFrame    = 0;
    itemPicked = 0;
    effects    = new SceneEffectList();
    rootAny = root = nullptr;
    fade = new StageFade(kScreenX, kScreenY, kScreenW, kScreenH, nullptr);

    if (!this->isSubScene) {
        pool       = new ScenePool(kScenePoolCount, kScenePoolSize, kScenePoolSize2);
        pauseState = new PauseMenuState();
        itemList   = new UiItemList(printer, 0);
        menu       = new StageMenu(printer, 0);
        noises    = new SceneNoiseList();
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
        noises    = nullptr;
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
    camerasReady = 0;
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

void StageScene::releaseControl()
{
    controlRequest = 1;
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
        int bankCount = table->bankCount();
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
            else if (isCarModelName(objectName)) {
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

const StageObjRecord *stageObjRecord(int index)
{
    return &stageObjRecords()[index < 0 || index >= kStageObjRecordCount ? 0 : index];
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
    if (clear != nullptr || gameOver != nullptr || getMission()->pendingEvent[0] != '\0')
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

bool actionAllowsMenu(int action)
{
    return action == PlayerAction_20 || action == PlayerAction_AltB || action == PlayerAction_9
        || action == PlayerAction_1 || action == PlayerAction_None;
}

// Inline in wantsMapMenu and wantsPauseMenu, and this copy for 0x230400.
// FS2 0x001963f0
// callcheck: getAltSoundFlag, isBusy, isBusy, isBusy, getActionState

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
    if (!object->m_abilities.flag3c0)
        return false;
    if (!object->m_abilities.flag3c3 && object->m_290 != nullptr)
        return false;
    if (!object->isPlayer())
        return false;
    const WalkLink *walk = object->m_motion->walkLink;
    if (!walk->edgeFound || kLookWindowHigh < walk->edgeDrop || walk->edgeDrop < kLookWindowLow)
        return false;
    return partner != nullptr && partner->m_abilities.flag3c1 != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00195520
// callcheck: currentMission, onRestore, restoreFlags, getShownState, setActive, setShown

void StageScene::restoreObjectFlags()
{
    currentMission()->onRestore();
    StageObject *kept = currentMission()->keptObject;
    if (kept == nullptr) {
        arrays->restoreFlags();
        return;
    }
    bool active = kept->m_active != 0;
    bool shown = kept->getShownState();
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

void StageScene::requestControl(StageObject *object)
{
    controlRequest = (uintptr_t)object;
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
    const float *at = who->m_xform.position;
    sceVu0FVECTOR pos = { at[0], at[1], at[2] + kFloorProbeHeight, at[3] };
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

// Runs the pack's script in a new DemoExecuter under the parent's event,
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
        subScene = new DemoExecuter(parent->event->isSkippable());
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
        // PC: the ROM waits as long as it takes. The host's voice streams are
        // stubs (rxSndStrAdpcmOpen), so no data ever comes: give up after a
        // second. Back to the plain loop with the streams.
        constexpr int kVoiceWaitFrames = 60;
        for (int i = 0; i < kVoiceWaitFrames && demoStream->getDataWritten() < 1; i++)
            threadYield(1);
        demoStream->setPaused(false);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The frame's walks over the object groups.

// The timing marks of the frame (retail's addBar is empty; the colours and
// the labels stay). The label names the stretch that ends at the mark.
constexpr u_int kPerfBlack     = 0x80000000;
constexpr u_int kPerfDarkGreen = 0x80008000;
constexpr u_int kPerfOrange    = 0x80007fff;

static void perfMark(u_int colour, const char *label)
{
    gGameSystem->m_perfMeter.addBar(0.0f, colour, PerfMeterGroup_Detail, label);
}

// The groups whose models have a VCM table to step (updateVcms).
static bool groupHasVcm(int group)
{
    return group == StageObjGroup_1 || group == StageObjGroup_Characters
        || group == StageObjGroup_3 || group == StageObjGroup_8;
}

// Every group but the cameras': each active root node carries its position
// down and takes its matrices, then the model's pose is solved and what
// hangs from its bones follows.
// FS2 0x0019a850
// callcheck: propagate, solve, propagate

void StageScene::calcMatrices()
{
    for (int group = StageObjGroup_1; group < StageObjGroup_Count; group++) {
        StageObjArray &list = objects->m_lists[group];
        for (StageObject **slot = firstActive(list.begin()); slot != nullptr;
             slot = firstActive(StageObjArray::next(slot))) {
            StageObject *obj = *slot;
            StageModel *model = obj->m_model;
            if (obj->m_parent == nullptr) {
                obj->propagate();
                obj->updateMatrix();
            }
            if (model == nullptr || model->pose == nullptr)
                continue;
            if (model->pose->parentMatrix != nullptr)
                model->pose->solve(3, -1, 0);
            for (BoneMap::Node *link = model->boneMap.first; link != nullptr; link = link->next) {
                link->value->propagate();
                link->value->updateMatrix();
            }
        }
    }
}

// calcMatrices' walk with only the positions (setupCameras, before the
// first frame).
// FS2 0x0019aa30
// callcheck: propagate, propagate

void StageScene::propagateNodes()
{
    for (int group = StageObjGroup_1; group < StageObjGroup_Count; group++) {
        StageObjArray &list = objects->m_lists[group];
        for (StageObject **slot = firstActive(list.begin()); slot != nullptr;
             slot = firstActive(StageObjArray::next(slot))) {
            StageObject *obj = *slot;
            StageModel *model = obj->m_model;
            if (obj->m_parent == nullptr)
                obj->propagate();
            if (model == nullptr)
                continue;
            for (BoneMap::Node *link = model->boneMap.first; link != nullptr; link = link->next)
                link->value->propagate();
        }
    }
}

// FS2 0x0019a710
// callcheck: updateVcm

void StageScene::updateVcms()
{
    for (int group = 0; group < StageObjGroup_Count; group++) {
        if (!groupHasVcm(group))
            continue;
        StageObjArray &list = objects->m_lists[group];
        for (StageObject **slot = firstActive(list.begin()); slot != nullptr;
             slot = firstActive(StageObjArray::next(slot))) {
            if ((*slot)->m_model != nullptr)
                (*slot)->m_model->updateVcm();
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019b820
// callcheck: getKind, isPlayer, getCompanion

bool StageScene::isInView(StageCar *target)
{
    if (target == nullptr)
        target = car;
    StageObject *character;
    if (target == nullptr || (character = target->m_driver) == nullptr)
        return false;
    bool camjack = views->topObject()->getKind() == StageObject_Kind11;
    if (character->isPlayer())
        return !camjack;
    return camjack && party->getCompanion() == character;
}

// Inline in the ROM: drawPaused, update, prepareDrawFlags (there on
// gStageScene) and playerOrTargetInView.
bool StageScene::targetInView()
{
    return isInView(car);
}

// FS2 0x0019b900
// callcheck: isMode1, isPlayer, getCompanion, getKind (targetInView holds the last three)

bool StageScene::playerOrTargetInView()
{
    return (player != nullptr && player->isMode1()) || targetInView();
}

// FS2 0x0019bb90

StageObject *StageScene::getViewBottom()
{
    return views->top < 0 ? nullptr : views->entries[0].object;
}

// ──────────────────────────────────────────────────────────────────────
// What takes the buttons: an open menu, then a ready item, then the pause
// state, then the character. During an event only the pause state can.
constexpr u_int kControlToPlayer = 1;   // StageScene::controlRequest

// FS2 0x00195c10
// callcheck: isActive, isReady, isActive

void *StageScene::getInputFocus()
{
    if (focus != nullptr)
        return focus;

    if (event != nullptr) {
        if (pauseState->isActive())
            focus = pauseState;
        return focus;
    }
    if (menu->state == StageMenu_Open) {
        focus = menu;
        return focus;
    }
    UiPickSlot *slot = itemList->frontSlot();
    if (slot->isReady() && slot->object != nullptr && slot->enabled) {
        focus = itemList;
        return focus;
    }
    if (pauseState->isActive())
        focus = pauseState;
    else if (controlled != nullptr)
        focus = controlled;
    else
        focus = player;
    return focus;
}

// ──────────────────────────────────────────────────────────────────────
// Inline three times in update.
// callcheck: setActive, setEnabled

void StageScene::releaseControlled()
{
    StageObject *object = controlled;
    controlled = nullptr;
    if (object == nullptr)
        return;
    object->m_input->setActive(false);
    if (object->m_brain != nullptr)
        object->m_brain->setEnabled(true);
}

// ──────────────────────────────────────────────────────────────────────
// who becomes the player. The main camera follows them, and camview too;
// a view camera that was following the old player goes over at once.
static const char kCameraName[]  = "camera";    /* data 0x303ca8 */
static const char kCamviewName[] = "camview";   /* data 0x303cb8 */

// FS2 0x0019b560
// callcheck: find0, getKind, onLinkChanged, onViewTop, setActive, setEnabled, onLinkChanged, setActive, setEnabled, onLinkChanged, onViewTop, find0, onLinkChanged, onViewTop

void StageScene::setPlayer(StagePlayer *who)
{
    StageObject *whoObject = who != nullptr ? who->character() : nullptr;
    StageObject *camera = objects->find0(kCameraName, 0);

    StageObject *top = views->topObject();
    if (camera != top && top->getKind() == StageObject_Kind10
        && top->m_linked == player->character()) {
        top->m_linked = whoObject;
        top->onLinkChanged();
        top->onViewTop();
    }

    if (player != nullptr) {
        StageObject *old = player->character();
        old->m_input->setActive(false);
        if (old->m_brain != nullptr)
            old->m_brain->setEnabled(true);
        camera->m_linked = nullptr;
        camera->onLinkChanged();
    }

    player = who;

    if (player != nullptr) {
        whoObject->m_input->setActive(true);
        if (whoObject->m_brain != nullptr)
            whoObject->m_brain->setEnabled(false);
        camera->m_linked = whoObject;
        camera->onLinkChanged();
        camera->onViewTop();
    }

    StageObject *camview = objects->find0(kCamviewName, 0);
    if (camview != nullptr) {
        camview->m_linked = whoObject;
        camview->onLinkChanged();
        camview->onViewTop();
    }
}

// ──────────────────────────────────────────────────────────────────────
// The frame's rumble: the tracks of the source give the two
// motors (when the option is on), and a node that fires an alert sounds it.
static const char kRumbleAlert[] = "ALERT2";    /* data 0x303bf8 */
constexpr int   kRumbleFlags  = 8;
constexpr float kRumbleVolume = 1.0f;

// FS2 0x00199250
// callcheck: getAltSoundFlag, getCompanion, getKind, getKind, getKind

StageObject *StageScene::getRumbleSource()
{
    StageObject *who = nullptr;
    if (player != nullptr && player->character()->getAltSoundFlag()) {
        who = party->getCompanion();
    } else {
        StageObject *top = gStageScene->views->topObject();
        if (top != nullptr
            && (top->getKind() == StageObject_Kind9 || top->getKind() == StageObject_Kind10
                || top->getKind() == StageObject_Kind11))
            who = top->m_linked;
    }
    return who != nullptr ? who : player->character();
}

// FS2 0x00199130
// callcheck: removeUnlooped, getRumbleSource, update, getGameOptions, setVibration, setVibration, playNamed

void StageScene::updateRumble(float frames)
{
    if (rumble == nullptr)
        return;
    if (event != nullptr) {
        rumble->removeUnlooped();
        return;
    }

    u_char motorLarge, motorSmall;
    char   flags[kRumbleFlags];
    StageObject *sources[kRumbleFlags];
    rumble->update(frames, &motorLarge, &motorSmall, flags, sources, kRumbleFlags,
                     getRumbleSource());
    // _08: the vibration option (read here for the first time).
    if (getGameOptions()->_08) {
        gGameSystem->m_pad0.setVibration(PadMotor_Small, motorSmall);
        gGameSystem->m_pad0.setVibration(PadMotor_Large, motorLarge);
    }
    for (int i = 0; i < kRumbleFlags; i++) {
        if (flags[i] == 1)
            sound->playNamed(kRumbleAlert, kRumbleVolume);
    }
}

// ──────────────────────────────────────────────────────────────────────
// One frame of the scene, in the order of its timing marks: actions,
// scripts, control, movement, collision, terminate, matrices, the owned
// objects, particles, sound, UI, who has control, the dead objects.
constexpr float kParticleRangeScale = 1.2f;     // of the draw distance
constexpr float kFrameSeconds = 0.033333335f;   // 1/30 s: updateRumble counts in these
constexpr float kDrawSysTicksPerSecond = 300.0f;
constexpr int   kLetterboxFrames = 0xf;
constexpr u_int kBlack = 0x80000000;

// FS2 0x00199400
// callcheck: addBar, sceneRenderGroup, actionStart, addBar, runStageScripts, run, isPlaying, operator_delete, isPlaying, stop, update, update, addBar, getAltSoundFlag, latchAltAction, actionControl, stageUpdateCharacterMotions, addBar, actionMovement, addBar, collide, addBar, actionTerminate, addBar, calcMatrices, cameraStep, calcMatrix, addBar, update, updateBlock98, updateBuffers, getDrawDistance, update, update, getDeltaSeconds, updateRumble, addBar, getAltSoundFlag, isMode1, update, getCueSubtitle, setSlot, update, update, updateLights, update, update, update, getInputFocus, bitMask, pick, update, currentMission, isRunning, ~SceneDirector, start, operator_new, SceneDirector, start, getAltSoundFlag, addBar, updateVcms, getDeltaSeconds, drawSysTick, isPlayer, getCompanion (perfMark, targetInView and releaseControlled carry the rest; the ROM repeats them inline)

void StageScene::update()
{
    focus = nullptr;
    perfMark(kPerfBlack, nullptr);
    render->m_snap = 0;
    render->m_forcedSet = sceneRenderGroup(0);
    if (block98 != nullptr)
        block98->dirty = 1;
    if (buffers != nullptr)
        buffers->used = 0;
    pairs->actionStart();
    perfMark(PerfMeterColour_Blue, "action(start)");

    if (!isSubScene) {
        runStageScripts();
    } else if (subScene != nullptr) {
        subScene->run(0);
        bool going = subScene->isRunning() || demoStream->isPlaying();
        bool skipped = subScene->m_skippable
            && (gGameSystem->m_pad0.m_pressed & SCE_PADstart) != 0;
        if (skipped || !going) {
            delete subScene;
            operator delete(subSceneBlock);
            subScene = nullptr;
            subSceneBlock = nullptr;
            if (demoStream->isPlaying())
                demoStream->stop();
        }
    }
    if (gameOver != nullptr)
        gameOver->update();
    else if (clear != nullptr)
        clear->update();
    perfMark(PerfMeterColour_Yellow, "script");

    inFrame = 1;
    if (altLatch) {
        if (player != nullptr && player->character()->getAltSoundFlag())
            player->latchAltAction();
        else
            altLatch = 0;
    }
    pairs->actionControl();
    stageUpdateCharacterMotions();
    perfMark(PerfMeterColour_Blue, "action(control)");
    pairs->actionMovement();
    perfMark(PerfMeterColour_Cyan, "action(movement(&motion))");
    records->collide();
    perfMark(kPerfDarkGreen, "collision");
    pairs->actionTerminate();
    perfMark(PerfMeterColour_Red, "action(terminate)");

    calcMatrices();
    StageObjArray &cameras = objects->m_lists[StageObjGroup_0];
    for (StageObject **slot = firstActive(cameras.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        if ((*slot)->m_viewActive) {
            (*slot)->cameraStep();
            (*slot)->updateMatrix();
        }
    }
    perfMark(kPerfOrange, "calcmatrix(root)");

    if (!isSubScene) {
        for (int i = 0; i < 3; i++) {
            if (owned[i] != nullptr)
                owned[i]->update();
        }
        party->update();
        updateBlock98();
        updateBuffers();
    }

    float range = getDrawDistance() * kParticleRangeScale;
    nodeTree->update(range, 0, views->topObject(), player);
    effects->update();
    updateRumble(clock->getDeltaSeconds() / kFrameSeconds);
    perfMark(PerfMeterColour_Green, "particle");

    if (!isSubScene) {
        StageObject *listener = views->topObject();
        u_char playerFlag = player == nullptr ? 0 : player->character()->getAltSoundFlag();
        bool sceneFlag = (player != nullptr && player->isMode1()) || targetInView();
        sound->update(listener, playerFlag, sceneFlag);
        pool->setSlot(0, sound->getCueSubtitle());
    } else {
        demoStream->update();
    }

    render->update();
    render->updateLights();

    if (!isSubScene) {
        pauseState->update();
        if (event == nullptr) {
            menu->update();
            itemList->update();
            bool pressed = false;
            if (getInputFocus() == itemList) {
                const GamePad &pad = gGameSystem->m_pad0;
                int button = pad.m_optionsMap == nullptr ? pad.m_buttonMap.button[0]
                                                         : pad.m_optionsMap->button[0];
                pressed = (pad.m_pressed & bitMask(button)) != 0;
            }
            itemPicked = pressed && itemList->pick() >= 0;
            alert->update();
        }

        if (currentMission()->letterbox) {
            if (director == nullptr) {
                director = new SceneDirector;
                director->start(kLetterboxFrames, 0, kBlack);
            }
        } else if (director != nullptr && !director->isRunning()) {
            if (director->colour == 0) {
                delete director;
                director = nullptr;
            } else {
                director->start(kLetterboxFrames, kBlack, 0);
            }
        }
    }

    if (controlRequest == 0) {
        bool alt = player != nullptr && player->character()->getAltSoundFlag();
        if (!alt && controlled != nullptr && controlled != player->character()) {
            releaseControlled();
            setPlayer(player);
        }
    } else if (controlRequest == kControlToPlayer) {
        releaseControlled();
        setPlayer(player);
        controlRequest = 0;
    } else {
        if (player != nullptr && player->character()->getAltSoundFlag()) {
            StageObject *object = (StageObject *)controlRequest;
            party->setCompanion(object);
            releaseControlled();
            setPlayer(player);
            player->character()->m_input->setActive(false);
            object->m_input->setActive(true);
            if (object->m_brain != nullptr)
                object->m_brain->setEnabled(false);
            controlled = object;
            StageObject *camera = objects->find0(kCameraName, 0);
            camera->m_linked = controlled;
            camera->onLinkChanged();
            camera->onViewTop();
        }
        controlRequest = 0;
    }

    // An object's dtor takes it out of the list, so the next one moves into
    // its slot.
    for (int group = 0; group < StageObjGroup_Count; group++) {
        StageObject **slot = objects->m_lists[group].begin();
        while (slot != nullptr) {
            StageObject *obj = *slot;
            if (!obj->m_dead) {
                slot = StageObjArray::next(slot);
                continue;
            }
            bool last = StageObjArray::next(slot) == nullptr;
            obj->destroy();
            if (last)
                slot = nullptr;
        }
    }
    perfMark(PerfMeterColour_Magenta, "others");

    updateVcms();
    drawSysTick((int)(clock->getDeltaSeconds() * kDrawSysTicksPerSecond));
    frameCount++;
}

// ──────────────────────────────────────────────────────────────────────
// Before drawing: all of every character is drawn; the view target's model
// gets the target group while it is in view (the partner's too in its mode
// 2); then what the camera is inside of is hidden: for camview / camjack the
// followed character's head (with the gear of isMode1, the whole body), else
// the whole body of an owned object's target.
constexpr u_char kDrawGroupNormal = 1;
constexpr u_char kDrawGroupTarget = 0x20;

// FS2 0x00198560
// callcheck: isPlayer, getCompanion, getKind, drawSysClearTargetView, drawSysSetTargetView, isMode2, getKind, getKind, hasTarget, getTarget, isMode1 (targetInView holds the first ones)

void StageScene::prepareDrawFlags()
{
    StageObjArray &characters = objects->m_lists[StageObjGroup_Characters];
    for (StageObject **slot = firstActive(characters.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        for (int i = 0; i < 2; i++) {
            if ((*slot)->m_parts[i] != nullptr)
                (*slot)->m_parts[i]->drawMode = kPartShown;
        }
    }

    if (gStageScene->targetInView()) {
        drawSysSetTargetView();
        car->m_model->data->drawGroup = kDrawGroupTarget;
        if (partner != nullptr && partner->isMode2())
            partner->m_model->data->drawGroup = kDrawGroupTarget;
    } else {
        drawSysClearTargetView();
        if (car != nullptr)
            car->m_model->data->drawGroup = kDrawGroupNormal;
        if (partner != nullptr)
            partner->m_model->data->drawGroup = kDrawGroupNormal;
    }

    if (views->topObject()->getKind() == StageObject_Kind10
        || views->topObject()->getKind() == StageObject_Kind11) {
        StageObject *linked = views->topObject()->m_linked;
        if (linked == nullptr)
            return;
        ModelPartRec *part = static_cast<StagePlayer *>(linked)->isMode1()
                                  ? linked->m_parts[1] : linked->m_parts[0];
        if (part != nullptr)
            part->drawMode = kPartHidden;
        return;
    }

    SceneOwnedObject *owner = nullptr;
    for (int i = 0; i < 3 && owner == nullptr; i++) {
        if (owned[i] != nullptr && owned[i]->hasTarget())
            owner = owned[i];
    }
    if (owner == nullptr)
        return;
    StageObject *target = owner->getTarget();
    if (target != nullptr && target->m_parts[1] != nullptr)
        target->m_parts[1]->drawMode = kPartHidden;
}

// ──────────────────────────────────────────────────────────────────────
// The frame filter of camview and camjack, by who the camera follows.
struct ViewFilter
{
    int passes;
    int arg;
};
enum ViewFilterKind
{
    ViewFilter_NoOne,           // the camera follows no one
    ViewFilter_PlayerStage6,    // the player, on stage 6
    ViewFilter_PartnerStage12,  // the partner, on stage 12
    ViewFilter_Snpc,            // F_E24_SNPC2, F_E21_SNPC1
    ViewFilter_Other,
    ViewFilter_Count,
};
static const ViewFilter kViewFilters[ViewFilter_Count] = {     /* data 0x31a8f8 */
    { 0, 0 }, { 3, 1 }, { 3, 1 }, { 1, 1 }, { 0, 0 },
};
static_assert(sizeof(ViewFilter) == 8, "ViewFilter: the table's stride");
constexpr int kFilterStagePlayer  = 6;
constexpr int kFilterStagePartner = 12;
static const char kSnpcE24[] = "F_E24_SNPC2";   /* data 0x303bb8 */
static const char kSnpcE21[] = "F_E21_SNPC1";   /* data 0x303bc8 */
// The 512 x 512 frame the filters and fills work on.
constexpr int kSceneFrameSize = 0x200;
// GsVram area 4's block in the filter's page units (>> 5).
constexpr int kFilterVramArea = 4;
constexpr int kFilterBlockShift = 5;

// FS2 0x001983b0
// callcheck: getKind, getKind, getStageSelect, isPlayer, isPartner, strcmp, strcmp, openList, gfxGetDrawEnv, gfxGetVramBlock, gsFrameFilter, closeList

void StageScene::drawViewFilter(GamePacketManager *packets, StageObject *camera)
{
    if (camera->getKind() != StageObject_Kind10 && camera->getKind() != StageObject_Kind11)
        return;

    ViewFilterKind kind = ViewFilter_NoOne;
    StageObject *linked = camera->m_linked;
    if (linked != nullptr) {
        int stage = getStageSelect()->stage;
        if (linked->isPlayer() && stage == kFilterStagePlayer)
            kind = ViewFilter_PlayerStage6;
        else if (linked->isPartner() && stage == kFilterStagePartner)
            kind = ViewFilter_PartnerStage12;
        else if (strcmp(linked->m_name, kSnpcE24) == 0 || strcmp(linked->m_name, kSnpcE21) == 0)
            kind = ViewFilter_Snpc;
        else
            kind = ViewFilter_Other;
    }

    const ViewFilter &filter = kViewFilters[kind];
    if (filter.passes == 0)
        return;
    sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
    GfxDrawEnv *env = gfxGetDrawEnv();
    u_int block = (u_int)gfxGetVramBlock(kFilterVramArea);
    gsFrameFilter(packet, &env->env, kSceneFrameSize, kSceneFrameSize,
                  (u_short)(block >> kFilterBlockShift), filter.passes, filter.arg);
    packets->closeList();
}

// ──────────────────────────────────────────────────────────────────────
// One frame's draw for camera, layer by layer: the 3D frame, particles,
// the menu program, then the packet lists in the game's order.
static const char kEventD0101[] = "d0101";      /* data 0x303be8 */
// The filter of the event d0101 (below 2 of the demo camera's m_demoCut).
constexpr int kD0101FilterCuts = 2;
constexpr int kD0101FilterPasses = 1;
constexpr int kD0101FilterArg = 5;
// FS2 0x00198970
// callcheck: applyClearColor, prepareDrawFlags, calcViewMatrix, drawSysCurrent, drawSysCurrent, setDrawDistance, update, gfxGetCurrentDrawEnv, drawSysRender, addBar, draw, getDeltaSeconds, draw, drawViewFilter, draw, getKind, strcmp, openList, gfxGetDrawEnv, gfxGetVramBlock, gsFrameFilter, closeList, drawOverlay, openList, getDeltaSeconds, draw, closeList, openList, gfxGetTexArea, getList, closeList, draw, openList, gfxGetTexArea, getList, draw, gfxGetTexArea, getList, closeList, openList, gfxGetTexArea, getList, draw, closeList, openList, drawSubtitles, closeList, openList, gfxGetTexArea, getList, draw, gfxGetTexArea, getList, draw, gfxGetTexArea, getList, gsFillRect, gsFillRect, closeList, addBar (perfMark holds addBar; drawLetterbox holds the two fills and the GS register packets)

void StageScene::draw(StageObject *active)
{
    GamePacketManager *packets = &gGameSystem->m_packets;

    render->applyClearColor();
    prepareDrawFlags();
    static_cast<StageCamera *>(active)->calcViewMatrix();
    drawSysCurrent()->camera->setViewMatrix(active->cameraView());
    drawSysCurrent()->camera->setScreenMatrix(active->cameraScreen());
    setDrawDistance(active->m_range + active->m_range2);
    if (areaClip != nullptr)
        areaClip->update(active);

    GfxDrawEnv *env = gfxGetCurrentDrawEnv();
    drawSysRender(packets, env != nullptr ? &env->env : nullptr, 0);
    perfMark(PerfMeterColour_White, "rena draw");

    nodeTree->draw(packets, active);
    if (menuProg != nullptr)
        menuProg->draw(clock->getDeltaSeconds(), packets, active);

    if (!isSubScene) {
        drawViewFilter(packets, active);
        party->draw();
    } else {
        StageObject *top = views->topObject();
        if (top->getKind() == StageObject_Kind13) {
            if (strcmp(parent->event->name, kEventD0101) == 0
                && top->m_demoCut < kD0101FilterCuts) {
                sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
                GfxDrawEnv *frame = gfxGetDrawEnv();
                u_int block = (u_int)gfxGetVramBlock(kFilterVramArea);
                gsFrameFilter(packet, &frame->env, kSceneFrameSize, kSceneFrameSize,
                              (u_short)(block >> kFilterBlockShift), kD0101FilterPasses,
                              kD0101FilterArg);
                packets->closeList();
            }
            static_cast<DemoCamera *>(top)->drawOverlay();
        }
    }

    if (daylight != nullptr) {
        sceVif1Packet *packet = packets->openList(PacketList_SceneModel);
        daylight->draw(clock->getDeltaSeconds(), packet, active);
        packets->closeList();
    }

    {
        sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
        screenFill->draw(packet, gfxGetTexArea()->getList(PacketList_Scene2d));
        packets->closeList();
    }

    for (int i = 0; i < 3; i++) {
        if (owned[i] != nullptr)
            owned[i]->draw();
    }
    if (menuProgs != nullptr && player != nullptr)
        menuProgs->draw(player->m_motion->scopeZoom);

    if (!isSubScene) {
        if (printer->m_texture != nullptr) {
            if (event == nullptr) {
                sceVif1Packet *packet = packets->openList(PacketList_StageClock);
                itemList->draw(packet, gfxGetTexArea()->getList(PacketList_StageClock));
                menu->draw(packet, gfxGetTexArea()->getList(PacketList_StageClock));
                packets->closeList();
            }
            if (event == nullptr || !event->hidesText()) {
                sceVif1Packet *packet = packets->openList(PacketList_Screen);
                gStageScene->pool->draw(packet, gfxGetTexArea()->getList(PacketList_Screen));
                packets->closeList();
            }
        }
    } else {
        sceVif1Packet *packet = packets->openList(PacketList_Screen);
        demoStream->drawSubtitles(packet);
        packets->closeList();
    }

    sceVif1Packet *packet = packets->openList(PacketList_Overlay);
    if (!isSubScene) {
        alert->draw(packet, gfxGetTexArea()->getList(PacketList_Overlay));
        fade->draw(packet, gfxGetTexArea()->getList(PacketList_Overlay));
        if (director != nullptr)
            director->draw(packet);
    } else {
        fade->draw(packet, gfxGetTexArea()->getList(PacketList_Overlay));
        drawLetterbox(packet, kBlack);
    }
    packets->closeList();
    perfMark(PerfMeterColour_White, "2d draw");
}

// ──────────────────────────────────────────────────────────────────────
// The objects the stage looks at (the view target and the two statues of
// stage 24, where they exist). What reads the array is unread.
static const char *const kSightObjects[2] = {   /* data 0x31a920 */
    "oSJ24_02",                 /* data 0x303b50 */
    "oSJ24_03",                 /* data 0x303b60 */
};

// FS2 0x00197160
// callcheck: find1, operator_new[], find1

void StageScene::collectViewTargets()
{
    if (car != nullptr)
        viewTargetCount++;
    for (u_int i = 0; i < 2; i++) {
        if (objects->find1(kSightObjects[i], 0) != nullptr)
            viewTargetCount++;
    }

    viewTargets = new StageObject *[viewTargetCount];
    int index = 0;
    if (car != nullptr)
        viewTargets[index++] = car;
    for (u_int i = 0; i < 2; i++) {
        StageObject *object = objects->find1(kSightObjects[i], 0);
        if (object != nullptr)
            viewTargets[index++] = object;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Room for every object's saved state but the bullets'. Inline in
// setupCameras.
SceneArrayPair::SceneArrayPair()
{
    count = 0;
    for (int group = 0; group < StageObjGroup_Count; group++) {
        if (group != StageObjGroup_Bullets)
            count += gStageScene->objects->m_lists[group].m_count;
    }
    first  = new u_char[(int)count * 3];
    second = new NodeTransform[(int)count];
}

// ──────────────────────────────────────────────────────────────────────
// Every object's three saved bytes back, group by group, in the walk that
// counted them (SceneArrayPair above). A light with m_keepFlags has no
// record.
// FS2 0x0019c6a0
// callcheck: setActive, setShown, setShown

void SceneArrayPair::restoreFlags()
{
    int index = 0;
    for (int group = 0; group < StageObjGroup_Count; group++) {
        if (group == StageObjGroup_Bullets)
            continue;
        StageObjArray &list = gStageScene->objects->m_lists[group];
        for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
            StageObject *obj = *slot;
            if (group == StageObjGroup_5 && obj->m_keepFlags)
                continue;
            const u_char *saved = &first[index * 3];
            bool active = saved[0] != 0;
            obj->m_tree->setActive(active);
            obj->m_tree->setShown(active);
            obj->m_tree->setShown(saved[1] != 0);
            index++;
            obj->m_viewActive = saved[2] != 0;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0019cdc0
// callcheck: onViewTop

void StageViewStack::push(StageObject *object, int priority, u_char flag)
{
    int last = top;
    for (int i = 0; i <= last; i++) {
        if (entries[i].object == object)
            return;
    }

    top++;
    int index = last;
    for (; index >= 0; index--) {
        if (entries[index].priority <= priority)
            break;
        entries[index + 1] = entries[index];
    }
    index++;
    entries[index].object   = object;
    entries[index].priority = priority;
    entries[index].flag     = flag;

    for (int i = 0; i < top; i++)
        entries[i].object->m_viewActive = 0;

    StageObject *next = topObject();
    if (next != nullptr) {
        if (next == object)
            next->onViewTop();
        next->m_viewActive = 1;
    }
}

// FS2 0x0019cc80
// callcheck: onViewTop

StageObject *StageViewStack::removePriority(int priority)
{
    int index = top;
    while (index >= 0 && entries[index].priority != priority)
        index--;
    if (index < 0)
        return nullptr;

    for (int i = index + 1; i <= top; i++)
        entries[i - 1] = entries[i];
    top--;
    // As the ROM: it reads the entry after the shift, so below the top this
    // is the object that moved into the place, not the one removed.
    StageObject *object = entries[index].object;
    object->m_viewActive = 0;

    StageObject *next = topObject();
    if (next != nullptr) {
        if (!next->m_viewActive)
            next->onViewTop();
        next->m_viewActive = 1;
    }
    return object;
}

// FS2 0x0019bb70
// callcheck: removePriority (a tail jump)

void StageScene::removeViewPriority(int priority)
{
    views->removePriority(priority);
}

// FS2 0x0019bb80
// callcheck: push (a tail jump)

void StageScene::pushView(StageObject *object, int priority, u_char flag)
{
    views->push(object, priority, flag);
}

// ──────────────────────────────────────────────────────────────────────
// Once for a scene: a stage gets its cameras (camera, the three camviews,
// cam_dive), a spare bullet and the flash light, off; a sub-scene its demo
// camera. The Root objects are found, the bg script's entries run (a
// sub-scene's last entry is its own, and entries ending in m are left to
// it), the bones are bound, the first two characters become player and
// partner unless a script said otherwise, and a stage builds what needs the
// whole map.
static const char kCamviewZoomName[]  = "camview_zoom";  /* data 0x303cc0 */
static const char kCamviewSnipeName[] = "camview_snipe"; /* data 0x303cd0 */
static const char kCamDiveName[]      = "cam_dive";      /* data 0x303ce0 */
static const char kFlashName[]        = "flash";         /* data 0x303cf0 */
static const char kRootName[]         = "Root";          /* data 0x303cf8 */
static const char kCameraDemoName[]   = "camera_demo";   /* data 0x303d00 */
constexpr float kCameraScrZ   = 450.0f;
constexpr float kCameraWidth  = 512.0f;
constexpr float kCameraHeight = 512.0f;
constexpr float kCameraFar    = 1600.0f;
constexpr int   kViewPriorityMain = 0;
constexpr int   kViewPriorityDemo = 4;
constexpr char  kScriptSkipMark = 'm';

// FS2 0x00199f00
// callcheck: prepare, operator_new, StageMainCamera, addToScene, push, operator_new, ViewCamera, addToScene, operator_new, ViewCamera, addToScene, operator_new, ViewCamera, addToScene, operator_new, StageCamera, addToScene, operator_new, StageBullet, setActive, setShown, operator_new, PointLight, addToScene, setActive, setShown, currentMission, setup, scriptPrepare, operator_new, DemoCamera, addToScene, push, createCamera, strcmp, getKind, attachToRoot, setup, setup, scriptPrepare, strlen, operator_new, ScriptRunner, setEntry, run, bindBones, placeBones, calcMatrices, propagateNodes, setPlayer, build, build, build, build, GetTimerSystemTime, seed, operator_new, operator_new[], operator_new[], FUN_00104d90, operator_new, getStageSelect, StageDaylight, collectViewTargets, operator_delete, operator_delete (the array's ctor is SceneArrayPair's; m_name stands for the virtual +0x21c)

void StageScene::setupCameras()
{
    if (camerasReady)
        return;

    if (!isSubScene) {
        for (int i = 0; i < 3; i++) {
            if (owned[i] != nullptr)
                owned[i]->prepare();
        }

        StageMainCamera *mainCamera = new StageMainCamera;
        mainCamera->addToScene(kCameraName);
        views->push(mainCamera, kViewPriorityMain, 0);

        static const char *const viewCameras[3] = {
            kCamviewName, kCamviewZoomName, kCamviewSnipeName,
        };
        for (int i = 0; i < 3; i++) {
            ViewCamera *view = new ViewCamera;
            view->addToScene(viewCameras[i]);
            view->m_viewActive = 0;
        }

        StageCamera *dive = new StageCamera(kCameraScrZ, kCameraWidth, kCameraHeight, kCameraFar);
        dive->addToScene(kCamDiveName);
        dive->m_viewActive = 0;

        StageBullet *bullet = new StageBullet(nullptr, nullptr);
        bullet->m_tree->setActive(false);
        bullet->m_tree->setShown(false);

        PointLight *flash = new PointLight;
        flash->addToScene(kFlashName, true);
        flash->m_tree->setActive(false);
        flash->m_tree->setShown(false);

        currentMission()->setup();
        scriptPrepare(scripts[0]);
    } else {
        DemoCamera *demo = new DemoCamera;
        demo->addToScene(kCameraDemoName);
        views->push(demo, kViewPriorityDemo, 0);
    }

    if (party != nullptr)
        party->createCamera();

    if (rootAny == nullptr) {
        for (int group = 0; group < StageObjGroup_Count; group++) {
            StageObjArray &list = objects->m_lists[group];
            for (StageObject **slot = list.begin(); slot != nullptr;
                 slot = StageObjArray::next(slot)) {
                if (strcmp((*slot)->m_name, kRootName) == 0) {
                    rootAny = *slot;
                    if (group == StageObjGroup_1)
                        root = rootAny;
                    break;
                }
            }
        }
    }

    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = roots.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        if ((*slot)->getKind() == StageObject_Door)
            (*slot)->attachToRoot();
    }

    if (areaClip != nullptr)
        areaClip->setup();
    render->setup();

    if (bgScript != nullptr) {
        scriptPrepare(bgScript);
        int count = bgScript->count;
        if (isSubScene && count >= 2)
            count--;
        for (int i = 0; i < count; i++) {
            const char *entry = (const char *)bgScript->entry(i);
            if (isSubScene && entry[strlen(entry) - 1] == kScriptSkipMark)
                continue;
            ScriptRunner *runner = new ScriptRunner;
            runner->setEntry(entry);
            runner->run(0);
            delete runner;
        }
    }

    for (StageObject **slot = roots.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *obj = *slot;
        StageModel *objModel = obj->m_model;
        if (obj != rootAny && objModel != nullptr) {
            objModel->bindBones();
            objModel->placeBones(obj);
        }
    }

    calcMatrices();
    propagateNodes();

    if (scripts[0] == nullptr) {
        StageObjArray &characters = objects->m_lists[StageObjGroup_Characters];
        StageObject **first = firstActive(characters.begin());
        if (player == nullptr && first != nullptr && *first != nullptr)
            setPlayer(static_cast<StagePlayer *>(*first));
        if (partner == nullptr && first != nullptr) {
            StageObject **second = firstActive(StageObjArray::next(first));
            if (second != nullptr && *second != nullptr)
                partner = *second;
        }
    }

    focus = nullptr;
    if (!isSubScene) {
        for (int i = 0; i < StageArea_Count; i++)
            areas[i]->build();
        rng->seed((u_int)GetTimerSystemTime());
        arrays = new SceneArrayPair;
        daylight = new StageDaylight(nodeTree, getStageSelect()->stage);
        collectViewTargets();
    }
    director = nullptr;

    if (gameOver != nullptr) {
        operator delete(gameOver);
        gameOver = nullptr;
    }
    if (clear != nullptr) {
        operator delete(clear);
        clear = nullptr;
    }
    altLatch = 0;
    camerasReady = 1;
}

// ──────────────────────────────────────────────────────────────────────
// The scene's end sequences, script-started.
// FS2 0x00195690
// callcheck: isBusy, operator_new, ClearSequence

void StageScene::startClear()
{
    if (!isBusy())
        clear = new ClearSequence;
}

// FS2 0x00195730
// callcheck: isInputBlocked, operator_new, GameOverSequence

void StageScene::startGameOver(StageObject *victim, StageObject *cause)
{
    if (!isInputBlocked())
        gameOver = new GameOverSequence(victim, cause);
}

// ──────────────────────────────────────────────────────────────────────
// The counterpart of restoreObjectFlags.
// FS2 0x001955e0
// callcheck: saveFlags, currentMission, setRestore

void StageScene::saveObjectFlags(void *arg, StageObject *object)
{
    arrays->saveFlags();
    currentMission()->setRestore(arg, object);
}

// The walk of restoreFlags, writing. The ROM copies the transform by its
// fields; the host's has no others.
// FS2 0x0019c960
// callcheck: getShownState

void SceneArrayPair::saveFlags()
{
    int index = 0;
    for (int group = 0; group < StageObjGroup_Count; group++) {
        if (group == StageObjGroup_Bullets)
            continue;
        StageObjArray &list = gStageScene->objects->m_lists[group];
        for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
            StageObject *obj = *slot;
            if (group == StageObjGroup_5 && obj->m_keepFlags)
                continue;
            u_char *saved = &first[index * 3];
            saved[0] = obj->m_active != 0;
            saved[1] = obj->getShownState();
            saved[2] = obj->m_viewActive;
            second[index] = obj->m_xform;
            index++;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The event that puts the lamps out before its sub-scene.
static const char kEventLampsOff[] = "d1804";          /* data 0x303bb0 */
constexpr int kEventLamps = 4;
static const char *const kEventLampNames[kEventLamps] = {   /* data 0x2f7da0 */
    "ENV_PL03", "ENV_PL04", "ENV_PL05", "ENV_PL06",
};

// FS2 0x00196060
// callcheck: strcmp, findAll5, setTreeScriptFlag

bool StageScene::requestSubScene(const char *name)
{
    if (isSubScene || subSceneRequest != nullptr)
        return false;

    if (strcmp(name, kEventLampsOff) == 0) {
        for (int i = 0; i < kEventLamps; i++) {
            StageObjArray *lamps = objects->findAll5(kEventLampNames[i]);
            if (lamps == nullptr)
                continue;
            for (StageObject **slot = firstActive(lamps->begin()); slot != nullptr;
                 slot = firstActive(StageObjArray::next(slot)))
                (*slot)->setTreeScriptFlag(false);
        }
    }
    subSceneRequest = name;
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001961c0
// callcheck: operator_new, StageFileEvent

StageEvent *StageScene::startFileEvent(float arg, const char *name)
{
    event = new StageFileEvent(arg, name);
    return event;
}

// FS2 0x00196220
// callcheck: operator_new, StageSimpleEvent

StageEvent *StageScene::startSimpleEvent(float arg, const char *name)
{
    event = new StageSimpleEvent(arg, name);
    return event;
}

// ──────────────────────────────────────────────────────────────────────
constexpr float kMissionFramesPerSecond = 30.0f;

// FS2 0x00196340
// callcheck: getMission, getSeconds

int StageScene::getMissionFrames()
{
    float start = getMission()->startTime;
    return (int)((gStageScene->clock->getSeconds() - start) * kMissionFramesPerSecond + 0.5f);
}

// ──────────────────────────────────────────────────────────────────────
// The first answer from the top: FS2 0x001964f0

bool StageScene::ownedHasTarget()
{
    bool has = false;
    if (owned[2] != nullptr)
        has = owned[2]->hasTarget();
    if (!has && owned[1] != nullptr)
        has = owned[1]->hasTarget();
    if (!has && owned[0] != nullptr)
        has = owned[0]->hasTarget();
    return has;
}

// FS2 0x001965a0

bool StageScene::isOwned2Busy()
{
    return owned[2] != nullptr && owned[2]->isBusy();
}

// FS2 0x001965e0

bool StageScene::isOwned1Busy()
{
    return owned[1] != nullptr && owned[1]->isBusy();
}

// FS2 0x00196620

bool StageScene::isOwned0Busy()
{
    return owned[0] != nullptr && owned[0]->isBusy();
}

// FS2 0x00196660
// callcheck: getAltSoundFlag

u_char StageScene::getPlayerAltFlag()
{
    return player != nullptr ? player->character()->getAltSoundFlag() : 0;
}

// ──────────────────────────────────────────────────────────────────────
// The mission's script-set values.
// FS2 0x00196690
// callcheck: getMission

float StageScene::getMissionValue()
{
    return getMission()->value;
}

// FS2 0x001966b0
// callcheck: currentMission

void StageScene::setMissionValue(float value)
{
    currentMission()->value = value;
}

// FS2 0x001966e0
// callcheck: getMission

float StageScene::getMoveScale(int index)
{
    return getMission()->moveScale[index];
}

// FS2 0x00196710
// callcheck: currentMission

void StageScene::setMoveScale(float scale, int index)
{
    currentMission()->moveScale[index] = scale;
}

// FS2 0x00196750
// callcheck: getMission

float StageScene::getDefaultMoveScale()
{
    return getMission()->defaultMoveScale;
}

// FS2 0x00196770
// callcheck: currentMission

void StageScene::setDefaultMoveScale(float scale)
{
    currentMission()->defaultMoveScale = scale;
}

// FS2 0x001967a0
// callcheck: currentMission

void StageScene::setScriptVar3f(int value)
{
    currentMission()->scriptVar3f = value;
}

// FS2 0x001967d0
// callcheck: currentMission

int StageScene::getScriptVar3f()
{
    return currentMission()->scriptVar3f;
}

// FS2 0x001967f0
// callcheck: currentMission

void StageScene::setScriptVar3e(int value)
{
    currentMission()->scriptVar3e = value;
}

// FS2 0x00196820
// callcheck: getMission

int StageScene::getScriptVar3e()
{
    return getMission()->scriptVar3e;
}

// FS2 0x00196a50
// callcheck: currentMission, setResultValue

void StageScene::setResultValue(int index, int value)
{
    currentMission()->setResultValue(index, value);
}

// FS2 0x00196a90
// callcheck: getMission, getResultValue

int StageScene::getResultValue(int index)
{
    return getMission()->getResultValue(index);
}

// ──────────────────────────────────────────────────────────────────────
// What StageScene::getMenuMode answers. Only SceneMenu_Free has been read
// (the fade and the mission's pending event go on then); 4 is never
// answered.
enum SceneMenuMode
{
    SceneMenu_None     = 0,
    SceneMenu_Free     = 1,     // inFrame clear
    SceneMenu_Open     = 2,     // the menu is open
    SceneMenu_Pending  = 3,     // the menu has one pending
    SceneMenu_ItemList = 5,     // the item list has one pending
};

// FS2 0x00196ac0

int StageScene::getMenuMode()
{
    if (!inFrame)
        return SceneMenu_Free;
    if (menu->state == StageMenu_Open)
        return SceneMenu_Open;
    if (menu->pendingOn != 0 && menu->pending != nullptr)
        return SceneMenu_Pending;
    return itemList->pendingCount < 1 ? SceneMenu_None : SceneMenu_ItemList;
}

// ──────────────────────────────────────────────────────────────────────
// A door's collision faces carry its type and number: doorId is
// kDoorFaceBase + (type - 1) * kDoorFacesPerType + (number - 1) * 4.
constexpr int kDoorFaceBase     = 100;
constexpr int kDoorFacesPerType = 180;
constexpr int kDoorFacesPerDoor = 4;

// Inline in findDoorOfFace.
static bool faceNamesDoor(const StageCollisionFace *face)
{
    return (face->flags & StageCollision_Door) != 0 && face->doorId >= kDoorFaceBase;
}

// FS2 0x00196cc0

bool StageScene::isDoorFace(const StageCollisionFace *face)
{
    return faceNamesDoor(face);
}

// FS2 0x00196b20
// callcheck: getKind

StageObject *StageScene::findDoorOfFace(const StageCollisionFace *face)
{
    if (!faceNamesDoor(face))
        return nullptr;

    int id     = face->doorId - kDoorFaceBase;
    int type   = id / kDoorFacesPerType + 1;
    int number = id % kDoorFacesPerType / kDoorFacesPerDoor + 1;

    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = firstActive(roots.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        StageObject *door = *slot;
        if (door->getKind() == StageObject_Door && type == door->m_doorType
            && number == door->m_doorIndex)
            return door;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Can this object be hit at all (the walks below, inline in the ROM)?
static bool isHittable(const StageObject *obj)
{
    return obj->m_active && obj->m_collides && !obj->m_dead;
}

// Any one face will do at both calls of StageCollision::sphereHits.
constexpr int kSphereHitCount = 1;

// FS2 0x00196cf0
// callcheck: getKind, sphereHits

bool StageScene::sphereHitsKind(float radius, const float *pos, int kind, u_int mask, u_int flags,
                                StageObject *skip)
{
    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = firstActive(roots.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        StageObject *obj = *slot;
        if (kind != obj->getKind() || (skip != nullptr && skip == obj) || !isHittable(obj))
            continue;
        for (u_int i = 0; obj->collisionAt(i) != nullptr; i++) {
            StageCollisionFace *out;
            if (obj->collisionAt(i)->sphereHits(radius, kSphereHitCount, &out, mask, flags, pos) != 0)
                return true;
        }
    }
    return false;
}

// FS2 0x00196f00
// callcheck: sphereHits

bool StageScene::sphereHitsAny(float radius, const float *pos, u_int mask, u_int flags,
                               StageObject *skip)
{
    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = firstActive(roots.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        StageObject *obj = *slot;
        if ((skip != nullptr && skip == obj) || !isHittable(obj))
            continue;
        for (u_int i = 0; obj->collisionAt(i) != nullptr; i++) {
            StageCollisionFace *out;
            if (obj->collisionAt(i)->sphereHits(radius, kSphereHitCount, &out, mask, flags, pos) != 0)
                return true;
        }
    }
    return false;
}

// FS2 0x00197a90
// callcheck: segmentHits

bool StageScene::segmentHitsAny(const float *from, const float *to, int arg, u_int mask,
                                u_int flags, StageObject *skip)
{
    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = firstActive(roots.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        StageObject *obj = *slot;
        if ((skip != nullptr && skip == obj) || !isHittable(obj))
            continue;
        for (u_int i = 0; obj->collisionAt(i) != nullptr; i++) {
            if (obj->collisionAt(i)->segmentHits(from, to, mask, flags, arg))
                return true;
        }
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// How far down findFloorFace looks when the range is negative.
constexpr float kFloorRayLength = 10000.0f;

// One mesh's floor below height (inline in both finders): a mesh that does
// not move answers by height, any other by a ray straight down.
static StageCollisionFace *meshFloorFace(StageCollision *mesh, float x, float y, float range,
                                         float height, u_int mask, u_int flags, float *faceHeight)
{
    *faceHeight = height;
    if (mesh->m_matrix == nullptr)
        return mesh->heightAt(x, y, range, faceHeight, mask, flags, nullptr);

    sceVu0FVECTOR hit;
    sceVu0FVECTOR from = { x, y, height, 1.0f };
    sceVu0FVECTOR to   = { x, y, height - (range < 0.0f ? kFloorRayLength : range), 1.0f };
    StageCollisionFace *face = mesh->raycast(hit, from, to, mask, flags, 0);
    *faceHeight = hit[2];
    return face;
}

// FS2 0x00197280
// callcheck: heightAt, raycast (in meshFloorFace)

StageCollisionFace *StageScene::findFloorFace(float x, float y, float range, float *height,
                                              u_int mask, u_int flags, StageObject **skip,
                                              int skipCount)
{
    StageCollisionFace *best = nullptr;
    float bestHeight = -FLT_MAX;

    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = firstActive(roots.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        StageObject *obj = *slot;
        int skipped = 0;
        for (; skipped < skipCount; skipped++) {
            if (skip[skipped] != nullptr && skip[skipped] == obj)
                break;
        }
        if (skipped < skipCount || !isHittable(obj))
            continue;

        for (u_int i = 0; obj->collisionAt(i) != nullptr; i++) {
            float faceHeight;
            StageCollisionFace *face = meshFloorFace(obj->collisionAt(i), x, y, range, *height,
                                                     mask, flags, &faceHeight);
            if (face != nullptr && bestHeight < faceHeight) {
                best = face;
                bestHeight = faceHeight;
            }
        }
    }

    *height = bestHeight;
    return best;
}

// FS2 0x00197580
// callcheck: heightAt, raycast (in meshFloorFace)

StageCollisionFace *StageScene::findFloorFaceExcept(float x, float y, float range, float *height,
                                                    u_int mask, u_int flags, StageObject *skip)
{
    StageCollisionFace *best = nullptr;
    float bestHeight = -FLT_MAX;

    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = firstActive(roots.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        StageObject *obj = *slot;
        if ((skip != nullptr && skip == obj) || !isHittable(obj))
            continue;

        for (u_int i = 0; obj->collisionAt(i) != nullptr; i++) {
            float faceHeight;
            StageCollisionFace *face = meshFloorFace(obj->collisionAt(i), x, y, range, *height,
                                                     mask, flags, &faceHeight);
            if (face != nullptr && bestHeight < faceHeight) {
                best = face;
                bestHeight = faceHeight;
            }
        }
    }

    *height = bestHeight;
    return best;
}

// FS2 0x00197850
// callcheck: getKind, raycast, vecDistance

StageObject *StageScene::findObjectOnSegment(const sceVu0FVECTOR from, const sceVu0FVECTOR to,
                                             int arg, int kind, u_int mask, u_int flags,
                                             StageObject *skip)
{
    StageObject *best = nullptr;
    float bestDistance = FLT_MAX;

    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = firstActive(roots.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        StageObject *obj = *slot;
        if (kind != obj->getKind() || (skip != nullptr && skip == obj) || !isHittable(obj))
            continue;
        for (u_int i = 0; obj->collisionAt(i) != nullptr; i++) {
            sceVu0FVECTOR hit;
            if (obj->collisionAt(i)->raycast(hit, from, to, mask, flags, arg) == nullptr)
                continue;
            float distance = vecDistance(hit, from);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = obj;
            }
        }
    }
    return best;
}

// Both take from and to by value first: hit may be one of them. The best
// hit starts as hit's own contents, as in StageScene::raycast (the ROM
// leaves that local uninitialised).
// FS2 0x00197c80
// callcheck: raycast, vecDistance

StageCollisionFace *StageScene::raycastObjectExcept(StageObject **object, sceVu0FVECTOR hit,
                                                    const sceVu0FVECTOR from,
                                                    const sceVu0FVECTOR to, int arg, u_int mask,
                                                    u_int flags, StageObject **skip, int skipCount)
{
    StageCollisionFace *best = nullptr;
    StageObject *bestObject = nullptr;
    float bestDistance = FLT_MAX;
    sceVu0FVECTOR start, end, bestHit;
    for (int k = 0; k < 4; k++) {
        start[k] = from[k];
        end[k] = to[k];
        bestHit[k] = hit[k];
    }

    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = firstActive(roots.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        StageObject *obj = *slot;
        int skipped = 0;
        for (; skipped < skipCount; skipped++) {
            if (skip[skipped] != nullptr && skip[skipped] == obj)
                break;
        }
        if (skipped < skipCount || !isHittable(obj))
            continue;

        for (u_int i = 0; obj->collisionAt(i) != nullptr; i++) {
            StageCollisionFace *face = obj->collisionAt(i)->raycast(hit, start, end, mask, flags,
                                                                    arg);
            if (face == nullptr)
                continue;
            float distance = vecDistance(hit, start);
            if (distance < bestDistance) {
                for (int k = 0; k < 4; k++)
                    bestHit[k] = hit[k];
                best = face;
                bestDistance = distance;
                bestObject = obj;
            }
        }
    }

    for (int k = 0; k < 4; k++)
        hit[k] = bestHit[k];
    if (object != nullptr)
        *object = bestObject;
    return best;
}

// FS2 0x00197f10
// callcheck: raycast, vecDistance

StageCollisionFace *StageScene::raycastObject(StageObject **object, sceVu0FVECTOR hit,
                                              const sceVu0FVECTOR from, const sceVu0FVECTOR to,
                                              int arg, u_int mask, u_int flags, StageObject *skip)
{
    StageCollisionFace *best = nullptr;
    StageObject *bestObject = nullptr;
    float bestDistance = FLT_MAX;
    sceVu0FVECTOR start, end, bestHit;
    for (int k = 0; k < 4; k++) {
        start[k] = from[k];
        end[k] = to[k];
        bestHit[k] = hit[k];
    }

    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = firstActive(roots.begin()); slot != nullptr;
         slot = firstActive(StageObjArray::next(slot))) {
        StageObject *obj = *slot;
        if ((skip != nullptr && skip == obj) || !isHittable(obj))
            continue;
        for (u_int i = 0; obj->collisionAt(i) != nullptr; i++) {
            StageCollisionFace *face = obj->collisionAt(i)->raycast(hit, start, end, mask, flags,
                                                                    arg);
            if (face == nullptr)
                continue;
            float distance = vecDistance(hit, start);
            if (distance < bestDistance) {
                for (int k = 0; k < 4; k++)
                    bestHit[k] = hit[k];
                best = face;
                bestDistance = distance;
                bestObject = obj;
            }
        }
    }

    for (int k = 0; k < 4; k++)
        hit[k] = bestHit[k];
    if (object != nullptr)
        *object = bestObject;
    return best;
}

// The mask, the probe's start above pos, and what a miss answers.
constexpr u_int   kFloorMask       = 0x42400c;
constexpr float   kFloorProbeAbove = 50.0f;
constexpr float   kFloorProbeRange = -1.0f;
constexpr short   kFloorAttrNone   = -1;

// FS2 0x001970f0
// callcheck: findFloorFace

short StageScene::getFloorAttr(const float *pos)
{
    float height = pos[2] + kFloorProbeAbove;
    StageCollisionFace *face = findFloorFace(pos[0], pos[1], kFloorProbeRange, &height, kFloorMask,
                                             0, viewTargets, viewTargetCount);
    return face != nullptr ? (short)face->attr : kFloorAttrNone;
}

// ──────────────────────────────────────────────────────────────────────
// Also inline in updateBuffers and updateBlock98 in the ROM.
// FS2 0x0019b9f0
// callcheck: getKind

StageObject *viewLinkedObject()
{
    StageObject *top = gStageScene->views->topObject();
    if (top == nullptr)
        return nullptr;
    if (top->getKind() != StageObject_Kind9 && top->getKind() != StageObject_Kind10
        && top->getKind() != StageObject_Kind11)
        return nullptr;
    return top->m_linked;
}

// The eight character blocks and the player's block follow the camera's
// linked object.
// FS2 0x0019afb0
// callcheck: attachCharacters, getKind, getKind, getKind, setActiveFor, currentMission, updateRange, storeActive

void StageScene::updateBuffers()
{
    if (buffers == nullptr)
        return;

    buffers->attachCharacters(objects);
    StageObject *linked = viewLinkedObject();
    buffers->setActiveFor(linked, render);
    buffers->updateRange(currentMission()->blockRange, linked, render);
    buffers->storeActive(linked);
}

// FS2 0x0019b1f0
// callcheck: hasTarget, hasTarget, hasTarget, getKind, getKind, getKind, setActive, updateVisible, attachTo

void StageScene::updateBlock98()
{
    if (block98 == nullptr)
        return;

    bool on = true;
    if ((owned[0] != nullptr && owned[0]->hasTarget())
        || (owned[1] != nullptr && owned[1]->hasTarget())
        || (owned[2] != nullptr && owned[2]->hasTarget())) {
        on = false;
    } else {
        StageObject *linked = viewLinkedObject();
        if (linked == nullptr || linked != player->character())
            on = false;
    }

    block98->setActive(on, render);
    if (on)
        block98->updateVisible(render);
    block98->attachTo(player);
    block98->active = on;
}

// ──────────────────────────────────────────────────────────────────────
// With a stage script and no event: the characters' steps around the
// scripts, the mission's records and its pending event. While an event
// runs only its end is watched. A brain is not null-tested (a character of
// the list has one).
// FS2 0x0019aba0
// callcheck: poll, destroy, pollSoundSources, currentMission, currentMission, beforeScripts, canSpawn, spawn, currentMission, tickPending, currentMission, currentMission, run, currentMission, currentMission, update, currentMission, afterScripts, currentMission, currentMission, afterScripts

void StageScene::runStageScripts()
{
    if (scripts[0] == nullptr)
        return;

    if (event != nullptr) {
        if (event->poll() == kStageEventFinished) {
            delete event;
            event = nullptr;
        }
        return;
    }
    if (isSubScene)
        return;

    noises->pollSoundSources();

    StageObject **it = currentMission()->characters;
    StageObject **end = currentMission()->charactersEnd();
    for (; it != end; it++) {
        StageCharacter *character = static_cast<StageCharacter *>(*it);
        if (character->m_active)
            character->beforeScripts();
        else if (character->canSpawn())
            character->spawn();
    }

    currentMission()->tickPending();

    if (gameOver == nullptr) {
        it  = currentMission()->scriptObjects;
        end = currentMission()->scriptObjectsEnd();
        for (; it != end; it++) {
            if ((*it)->m_active)
                (*it)->scriptRunner()->run(0);
        }

        it  = currentMission()->characters;
        end = currentMission()->charactersEnd();
        for (; it != end; it++) {
            if ((*it)->m_active)
                (*it)->m_brain->update();
        }
    }

    currentMission()->afterScripts();

    it  = currentMission()->characters;
    end = currentMission()->charactersEnd();
    for (; it != end; it++) {
        if ((*it)->m_active)
            static_cast<StageCharacter *>(*it)->afterScripts();
    }
}

// The sections follow the header in the order of the fields.
// FS2 0x00186ee0

void StageMapFile::relocate()
{
    StageMapPoint *pointList = reinterpret_cast<StageMapPoint *>(this + 1);
    points.set(pointList);
    StageMapCamera *cameraList = reinterpret_cast<StageMapCamera *>(pointList + pointCount);
    cameras.set(cameraList);
    StageMapSpot *spotList = reinterpret_cast<StageMapSpot *>(cameraList + cameraCount);
    spots.set(spotList);
    StageMapLight *lightList = reinterpret_cast<StageMapLight *>(spotList + spotCount);
    lights.set(lightList);
    StageMapBox *boxList = reinterpret_cast<StageMapBox *>(lightList + lightCount);
    boxes.set(boxList);
    marks.set(boxList + boxCount);
}

// The motion targets' names, 1..8; UNKNOWN for any other index. kept
// here until the map objects' file 0x175c00.. is cut.
// FS2 0x00175c00

const char *stageMotionTargetName(int index)
{
    static const char *const kNames[] = {
        "CAT", "CROW", "tonbi", "oMT04_04", "oZS09_04", "oDC07_05", "oMK21_09", "oMT04_99", "UNKNOWN",
    };    /* data 0x2f7aa0 */
    if (index < 1 || index > 8)
        index = 9;
    return kNames[index - 1];
}

// ──────────────────────────────────────────────────────────────────────
// What the bgmotion target's names stand for: all doors of a type, all
// objects of a kind, a motion target, or one object by name.
static const char kMotionDoorR[]    = "MODEL_dr_r";    /* data 0x303fb8 */
static const char kMotionDoorT[]    = "MODEL_dr_t";    /* data 0x303fc8 */
static const char kMotionCar[]      = "CAR";           /* data 0x303fd8 */
static const char kMotionSevenKeys[] = "Seven_Keys";   /* data 0x303fe0 */
constexpr int kMotionTargetFirst = 1;
constexpr int kMotionTargetLast  = 8;
// CAT, CROW and tonbi: the shared animals, on the map as oCM99_<index><digit>.
constexpr int kMotionTargetLastAnimal = 3;
constexpr int kAnimalPrefixSize = 7;
constexpr int kAnimalNameSize = 8;

// Without this the object never gets its motions, and so has no pose: a
// script's crow then faults in ModelPose.
// FS2 0x00175c40
// callcheck: sprintf, strncmp, strcmp

bool StageObject::isMotionTarget(int index)
{
    if (index < kMotionTargetFirst || index > kMotionTargetLast)
        return false;
    if (index <= kMotionTargetLastAnimal) {
        char prefix[0x10];
        sprintf(prefix, "oCM99_%d", index);
        return m_name[kAnimalNameSize] == '\0' && isdigit((unsigned char)m_name[kAnimalPrefixSize])
            && strncmp(m_name, prefix, kAnimalPrefixSize) == 0;
    }
    return strcmp(m_name, stageMotionTargetName(index)) == 0;
}

// The doors of type take list (inline twice).
static void setDoorMotion(int type, void *list)
{
    StageObjArray &roots = gStageScene->objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = roots.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *door = *slot;
        if (door->getKind() == StageObject_Door && door->m_doorType == type) {
            door->setMotionList(list, 1, 1);
            door->applyDoorRecord();
        }
    }
}

// The objects of kind take list (inline twice).
static void setKindMotion(int kind, void *list)
{
    StageObjArray &roots = gStageScene->objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = roots.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        if ((*slot)->getKind() == kind)
            (*slot)->setMotionList(list, 1, 1);
    }
}

// The objects of motion target index take list (inline).
static void setTargetMotion(int index, void *list)
{
    StageObjArray &roots = gStageScene->objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = roots.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        if ((*slot)->isMotionTarget(index))
            (*slot)->setMotionList(list, 1, 1);
    }
}

// FS2 0x0019f230
// callcheck: strcmp, getKind, setMotionList, applyDoorRecord, strcmp, stageMotionTargetName, strcmp, isMotionTarget, setMotionList, strcmp, getKind, setMotionList, strcmp, find1, setMotionList (the walks are setDoorMotion, setKindMotion, setTargetMotion)

bool StageFiles::applyBgMotionTarget()
{
    StageList<StageList<void *> *>::Node *list = motionLists.head;
    for (StageList<char *>::Node *node = motionNames.head; node != nullptr;
         node = node->next, list = list->next) {
        const char *name = node->value;

        if (strcmp(name, kMotionDoorR) == 0) {
            setDoorMotion(StageDoorType_R, list->value);
            continue;
        }
        if (strcmp(name, kMotionDoorT) == 0) {
            setDoorMotion(StageDoorType_T, list->value);
            continue;
        }

        int target = kMotionTargetFirst;
        for (; target <= kMotionTargetLast; target++) {
            if (strcmp(name, stageMotionTargetName(target)) == 0)
                break;
        }
        if (target <= kMotionTargetLast) {
            setTargetMotion(target, list->value);
            continue;
        }

        if (strcmp(name, kMotionCar) == 0) {
            setKindMotion(StageObject_Car, list->value);
        } else if (strcmp(name, kMotionSevenKeys) == 0) {
            setKindMotion(StageObject_Kind6, list->value);
        } else {
            StageObject *object = gStageScene->objects->find1(name, 0);
            if (object != nullptr)
                object->setMotionList(list->value, 0, 1);
        }
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The banks of the player's table with a file (inline in the ROM, twice in
// the loop's test); none without a table.
static int usedBankCount(const SePlayer *player)
{
    const SeTable *table = player->m_table;
    if (table == nullptr)
        return 0;
    int count = 0;
    for (int bank = 0; bank < kSeBanks; bank++) {
        if (table->banks[bank].file[0] != '\0')
            count++;
    }
    return count;
}

// The bank's file and second file; null when empty (inline in the ROM).
static const char *bankFile(const SePlayer *player, int bank)
{
    const SeTable *table = player->m_table;
    return table == nullptr || table->banks[bank].file[0] == '\0' ? nullptr
                                                                  : table->banks[bank].file;
}

static const char *bankStreamFile(const SePlayer *player, int bank)
{
    const SeTable *table = player->m_table;
    return table == nullptr || table->banks[bank].streamFile[0] == '\0'
               ? nullptr : table->banks[bank].streamFile;
}

// Once for a stage (StageSound::m_banksApplied): each SEDT file becomes the
// sound's table and its banks are made from what the loader holds, then
// the stream table, its subtitles and the stream banks' headers (read in
// streamEU/), last the loader lets go of what it no longer needs. PC: the
// ROM passes an empty bank's null name to getData, which on the EE reads
// address 0; skipped here.
// FS2 0x0019d9e0
// callcheck: setSeTable, getData, getData, loadBank, getData, getData, getData, getSize, openStreamBank, getData, setStreamTable, setStreamName, getData, setSubtitles, setDir, strcpy, getData, getData, compact

bool StageFiles::applySoundTarget()
{
    if (gStageScene->sound->m_banksApplied != 0)
        return true;
    gStageScene->sound->m_banksApplied = 1;

    for (StageList<void *>::Node *node = sndsehs.head; node != nullptr; node = node->next) {
        StageSound *sound = gStageScene->sound;
        sound->setSeTable((SeTable *)node->value);
        SePlayer *player = sound->m_player;

        for (int bank = 0; bank < usedBankCount(player); bank++) {
            const char *file = bankFile(player, bank);
            if (file == nullptr || loader->getData(file) == nullptr)
                continue;
            if (!player->m_table->banks[bank].isStream) {
                player->loadBank(bank, loader->getData(file));
                continue;
            }
            const char *streamFile = bankStreamFile(player, bank);
            if (streamFile != nullptr && loader->getData(streamFile) != nullptr) {
                player->openStreamBank(bank, loader->getData(file), loader->getData(streamFile),
                                       loader->getSize(streamFile));
            }
        }
    }

    if (streamTablePath[0] != '\0') {
        StageSound *sound = gStageScene->sound;
        void *tableFile = loader->getData(streamTablePath);
        if (tableFile != nullptr) {
            sound->setStreamTable(tableFile);
            if (kStreamDir[0] != '\0')
                sound->setStreamName(kStreamDir);
            if (fsmPath[0] != '\0') {
                void *fsm = loader->getData(fsmPath);
                if (fsm != nullptr)
                    sound->setSubtitles(fsm);
            }
            loader->setDir(kStreamDir);

            StreamTable *table = (StreamTable *)loader->getData(streamTablePath);
            if (table == nullptr)
                return false;
            if (table->magic[0] == 'S' && table->magic[1] == 't' && table->magic[2] == 'D'
                && table->magic[3] == 'T') {
                StBank *banks = table->banks;
                for (int i = 0; i < table->bankCount; i++) {
                    void *header = loader->getData(banks[i].name);
                    if (header != nullptr)
                        banks[i].header.set(header);
                }
            }
        }
    }

    loader->compact();
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The list file's directory and name.
static const char kListDir[]  = "../data/listfile/";   /* data 0x3041f0 */
static const char kListName[] = "s%02dlist.lst";       /* data 0x304208 */
constexpr int kListNameSize = 0x10;
constexpr int kListWordSize = 0x80;
constexpr int kMenuImages   = 6;
constexpr int kPartyImages  = 4;

// The init pack's pieces of a stage's objects first. Then the stage's
// files: the pack of the stage's screen when there is one (a PackFile
// loader takes them all from it); otherwise s%02dlist.lst, each word a
// file to load.
// FS2 0x001a3a60
// callcheck: getInitPackItem, getInitPackItem, operator_new, MenuProgSet, load, getInitPackItem, addImage, getInitPackItem, setImage, getInitPackItem, getInitPackItemSize, setSheet, getInitPackItem, getInitPackItemSize, setData, getInitPackItem, getInitPackItemSize, setImage, setPrinter, getStageSelect, stageScreenPathMp, addFile, startLoad, waitLoaded, isPack, setDir, strcpy, getStageSelect, sprintf, addFile, startLoad, waitLoaded, getItem, TextTokenizer, getItem, getItemSize, setText, next, addFile, startLoad, waitLoaded, ~TextTokenizer, setDir

void StageFiles::loadList()
{
    StageScene *scene = gStageScene;
    if (scene->rumble != nullptr)
        scene->rumble->data = (const RumbleFile *)getInitPackItem(InitItem_Rumble);

    bool missing = false;
    void *images[kMenuImages];
    for (int i = 0; i < kMenuImages; i++) {
        images[i] = getInitPackItem(InitItem_MenuImages + i);
        if (images[i] == nullptr)
            missing = true;
    }
    if (!missing) {
        scene->menuProgs = new MenuProgSet;
        scene->menuProgs->load(images[0], images[1], images[2], images[3], images[4], images[5]);
    }

    for (int i = 0; i < kPartyImages; i++) {
        void *image = getInitPackItem(InitItem_PartyImages + i);
        if (image != nullptr)
            scene->party->addImage(image);
    }
    scene->party->setImage(getInitPackItem(InitItem_PartyImage));

    void *item = getInitPackItem(InitItem_ItemSheet);
    getInitPackItemSize(InitItem_ItemSheet);
    scene->itemList->setSheet(item);
    item = getInitPackItem(InitItem_MenuData);
    getInitPackItemSize(InitItem_MenuData);
    scene->menu->setData(item);
    item = getInitPackItem(InitItem_PoolImage);
    getInitPackItemSize(InitItem_PoolImage);
    scene->pool->setImage(item);
    scene->pool->setPrinter(scene->printer);

    loader->setDir(nullptr);
    const char *pack = stageScreenPathMp(getStageSelect());
    if (pack != nullptr) {
        loader->addFile(pack);
        loader->startLoad();
        loader->waitLoaded(0, false);
    }

    if (loader->isPack())
        return;

    loader->setDir(kListDir);
    char name[kListNameSize];
    sprintf(name, kListName, getStageSelect()->stage);
    int index = loader->addFile(name);
    loader->startLoad();
    loader->waitLoaded(kWaitArg, kWaitShowLoading);

    if (loader->getItem(index) != nullptr) {
        TextTokenizer words(kListWordSize);
        words.setText((const char *)loader->getItem(index), loader->getItemSize(index));
        const char *word;
        while ((word = words.next()) != nullptr)
            loader->addFile(word);
        loader->startLoad();
        loader->waitLoaded(kWaitArg, kWaitShowLoading);
    }
}

// ──────────────────────────────────────────────────────────────────────
// How far above an object's position isInAbArea looks.
constexpr float kAbProbeAbove = 50.0f;

// FS2 0x0019ae50
// callcheck: contains

bool StageScene::isInAbArea(StageObject *object)
{
    const sceVu0FVECTOR &position = object->m_xform.position;
    sceVu0FVECTOR pos = { position[0], position[1], position[2] + kAbProbeAbove, position[3] };
    return areas[StageArea_Ab]->contains(pos) != nullptr;
}

// FS2 0x0019b180
// callcheck: addObject

void StageScene::addBufferObject(StageObject *object)
{
    if (buffers != nullptr)
        buffers->addObject(object);
}

// FS2 0x0019b1b0
// callcheck: createFor

void StageScene::createBuffers(StageObject *owner)
{
    if (buffers == nullptr)
        buffers = SceneBufferSet::createFor(owner, nodeTree);
}

// FS2 0x0019b470
// callcheck: updateFor

void StageScene::updateBlock98For(StageObject *object)
{
    if (block98 != nullptr)
        block98->updateFor(object, player);
}

// FS2 0x0019b4a0
// callcheck: createFor

void StageScene::createBlock98(StageObject *owner)
{
    if (block98 == nullptr)
        block98 = SceneBlock98::createFor(owner, nodeTree);
}

static const char kObjectZs9901[] = "oZS99_01";

// FS2 0x0019b4e0
// callcheck: find1

StageObject *StageScene::getZs9901()
{
    if (!zs9901Sought) {
        zs9901 = objects->find1(kObjectZs9901, 0);
        zs9901Sought = 1;
    }
    return zs9901;
}

// Reads gStageScene, whatever the caller passes.
// FS2 0x0019c7e0
// callcheck: setActive, setShown

void stageHideObjects()
{
    for (int group = 0; group < StageObjGroup_Count; group++) {
        bool stays = group != StageObjGroup_Bullets && group != StageObjGroup_3
                     && group != StageObjGroup_Characters;
        StageObjArray &list = gStageScene->objects->m_lists[group];
        for (StageObject **slot = firstActive(list.begin()); slot != nullptr;
             slot = firstActive(StageObjArray::next(slot))) {
            StageObject *obj = *slot;
            if (!stays) {
                obj->m_tree->setActive(false);
                obj->m_tree->setShown(false);
            }
            if (group == StageObjGroup_1)
                obj->m_viewActive = 0;
        }
    }
}

// FS2 0x0019d4c0
// callcheck: FUN_001a4e80 (StringMap find, game/stagelist.h)

bool StageFiles::hasItem(const char *name)
{
    return groups.find(name) != nullptr;
}

// FS2 0x0019d9c0
// callcheck: FUN_001a5250 (StringMap find, game/stagelist.h)

bool StageFiles::hasCharacter(const char *name)
{
    return characters.find(name) != nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The ROM's name for the class that runs a sub-scene's script.
// FS2 0x0019cf40

const char *DemoExecuter::getName()
{
    return "DemoExecuter";
}

// FS2 0x00195db0
// callcheck: ~ScriptRunner, operator_delete (the base dtor, deleting)

DemoExecuter::~DemoExecuter()
{
}

// ──────────────────────────────────────────────────────────────────────
// StageMarker's kind: slot 123 of its vtable.
// FS2 0x001a4880

int StageMarker::getKind()
{
    return StageObject_Marker;
}

// FS2 0x001a48c0

void StageMarker::sideHook0()
{
}

// FS2 0x001a48b0

void StageMarker::sideHook1()
{
}

// FS2 0x001a48a0

void StageMarker::sideHook2()
{
}

// FS2 0x001a4890

void StageMarker::sideHook3()
{
}

// ──────────────────────────────────────────────────────────────────────
// What the compiler makes on the host: deleting destructors whose body is
// their base's, each a weak copy here because this file builds the object.
//   FS2 0x0019c060  deleting dtor, vtable 0x314ce0 (ctor 0x2b2c80), over 0x2a9be0
//   FS2 0x0019c0c0  deleting dtor, vtable 0x314c90 (ctor 0x2a89b0), over 0x2a9be0
//   FS2 0x0019c120  SigLamp, vtable 0x314c40, over 0x2a9be0
//   FS2 0x001a4550  SubSceneDoorObject, over 0x2bbfb0 (SubSceneMapObject)
//   FS2 0x001a45e0  CbAreaBox / FloorAreaBox, vtable 0x315160, AreaBox's inline
//   FS2 0x001a4650  StageNode, StageNodeBase's inline
//   FS2 0x001a4790  CbAreaBox / FloorAreaBox, vtable 0x315190, AreaBox's inline
//   FS2 0x001a4800  StageMarker, over 0x18b4e0 (StageObject)
//   FS2 0x001a48d0  SubSceneProp, over 0x269c30 (SubSceneModelObject)
// The defaults of ScriptRunner's command slots, inline in game/gamehooks.h
// (CodeWarrior's copy for DemoExecuter's vtable):
//   FS2 0x0019cf50  ScriptRunner::scriptGetPosX
//   FS2 0x0019cf60  ScriptRunner::scriptGetPosY
//   FS2 0x0019cf70  ScriptRunner::scriptGetPosZ
//   FS2 0x0019cf80  ScriptRunner::scriptSetPosition
//   FS2 0x0019cf90  ScriptRunner::scriptSetRotation
//   FS2 0x0019cfa0  ScriptRunner::scriptSetYaw
//   FS2 0x0019cfb0  ScriptRunner::command34
//   FS2 0x0019cfc0  ScriptRunner::getScriptObject
//   FS2 0x0019cfe0  ScriptRunner::command58
//   FS2 0x0019d010  ScriptRunner::command59
//   FS2 0x0019d040  ScriptRunner::scriptAddMenuLine
//   FS2 0x0019d050  ScriptRunner::command80
//   FS2 0x0019d060  ScriptRunner::command87
//   FS2 0x0019d070  ScriptRunner::command88
//   FS2 0x0019d080  ScriptRunner::command99
//   FS2 0x0019d0a0  ScriptRunner::command100
//   FS2 0x0019d0b0  ScriptRunner::command101
// This-adjust thunks of the objects' two other bases: -0x40 the script
// runner, -0x18 the base with four slots.
//   FS2 0x001a67c0  -0x40 to SubSceneProp's dtor
//   FS2 0x001a67d0  -0x18 to StageMarker::sideHook3
//   FS2 0x001a67e0  -0x18 to StageMarker::sideHook2
//   FS2 0x001a67f0  -0x18 to StageMarker::sideHook1
//   FS2 0x001a6800  -0x18 to StageMarker::sideHook0
//   FS2 0x001a6810  -0x40 to StageMarker's dtor
//   FS2 0x001a6820  -0x18 to 0x2699e0 (SubSceneModelObject)
//   FS2 0x001a6830  -0x18 to 0x2699f0
//   FS2 0x001a6840  -0x18 to 0x269a70
//   FS2 0x001a6850  -0x18 to 0x269a80
//   FS2 0x001a6860  -0x40 to 0x268a20 (SubSceneModelObject, slot 78 of the runner)
//   FS2 0x001a6870  -0x40 to 0x268a90 (slot 13)
//   FS2 0x001a6880  -0x40 to SubSceneDoorObject's dtor
//   FS2 0x001a6890  -0x40 to 0x268ae0 (slot 2)
//   FS2 0x001a68a0  -0x40 to 0x269160 (slot 1)

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
