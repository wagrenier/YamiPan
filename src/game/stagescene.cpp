// game/stagescene.cpp -- the stage scene and the stage's files
// (game/stagescene.h).

#include "game/stagescene.h"

#include <float.h>
#include <string.h>

#include "game/fileloader.h"
#include "game/gamehooks.h"
#include "game/seplayer.h"      /* StreamTable */
#include "game/stageobjlist.h"
#include "game/stagesound.h"
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
// FS2 0x00195d80

bool StageScene::isSubSceneDone()
{
    return isSubScene && subScene == nullptr;
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
