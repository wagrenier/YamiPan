// game/teleport.cpp -- PC: the jump to any stage or event (game/teleport.h).

#include "game/teleport.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <mutex>

#include "game/gamehooks.h"
#include "game/progress.h"
#include "game/stageloop.h"
#include "game/scriptrunner.h"
#include "game/stagescene.h"
#include "game/sysdata.h"
#include "yamipan/core/logger.h"

#include "game/teleporttable.inc"

// The natives' fades: black at once for the event, back after it.
constexpr float kFadeBlack     = 1.0f;
constexpr float kFadeClear     = 0.0f;
constexpr float kFadeFromHere  = -1.0f;
constexpr float kFadeNow       = 0.0f;
constexpr float kFadeBackTime  = 0.5f;

// How long the starter's block has to start the event, and how long a black
// screen waits for a script's fade after one (frames).
constexpr int kScriptStartFrames = 90;
constexpr int kFadeCheckFrames   = 60;

// Where the player can stand: CharacterGround's rays. kFloorUp above the
// trigger's floor, down over kFloorRange.
constexpr u_int kFaceFloor     = 0x42400c;
constexpr u_int kFaceFloorSkip = 0x40000;
constexpr float kFloorUp       = 100.0f;
constexpr float kFloorRange    = 300.0f;
constexpr float kBesideObject  = 80.0f;     // from a character or an object, not in it

static const char kRootName[] = "Root";

enum TeleportPhase
{
    TeleportPhase_Idle = 0,
    TeleportPhase_Start,        // the stage's first frame with no event
    TeleportPhase_Script,       // the starter's block is running
    TeleportPhase_After,        // the event has ended, or runs: the black's last check
};

static std::mutex        sLock;
static TeleportRequest   sRequest;              // sLock
static std::atomic<bool> sPending{false};
static char              sPlaying[sizeof(TeleportStatus::event)];   // sLock

// Only the game's threads, which take turns.
static TeleportRequest sTaken;
static int  sPhase;             // TeleportPhase
static int  sPhaseFrames;
static bool sMovieDue;
static bool sSkipIntro;

template <class T, int N>
static constexpr int countOf(const T (&)[N])
{
    return N;
}

int teleportStageCount()
{
    return countOf(kTeleportStages);
}

const TeleportStage &teleportStage(int index)
{
    return kTeleportStages[index];
}

int teleportFilmCount()
{
    return countOf(kTeleportFilms);
}

int teleportFilm(int index)
{
    return kTeleportFilms[index];
}

int teleportEventCount()
{
    return countOf(kTeleportEvents);
}

const TeleportEvent &teleportEvent(int index)
{
    return kTeleportEvents[index];
}

void teleportRequest(const TeleportRequest &request)
{
    std::lock_guard lock(sLock);
    sRequest = request;
    sPending.store(true);
}

TeleportStatus teleportStatus()
{
    TeleportStatus status = {};
    status.pending = sPending.load();
    // Plain reads of ints the game writes: a frame's staleness is fine here.
    const StageSelect *select = getStageSelect();
    status.stage = select->stage;
    status.variant = select->variant;
    status.difficulty = select->kind;
    status.inStage = gStageScene != nullptr;

    std::lock_guard lock(sLock);
    strcpy(status.event, sPlaying);
    return status;
}

static bool sameName(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        if (tolower((u_char)*a) != tolower((u_char)*b))
            return false;
    }
    return *a == *b;
}

// "7" or "7.2": a stage and its part; anything else an event's name.
static bool requestFromText(const char *text, TeleportRequest *request)
{
    *request = {};
    request->difficulty = kTeleportKeepDifficulty;
    request->skipIntro = true;
    request->fromTrigger = true;

    char *end = nullptr;
    int stage = (int)strtol(text, &end, 10);
    if (end != text && (*end == '\0' || *end == '.')) {
        request->kind = Teleport_Stage;
        request->stage = stage;
        request->variant = *end == '.' ? atoi(end + 1) - 1 : 0;
        return true;
    }

    for (int i = 0; i < teleportEventCount(); i++) {
        const TeleportEvent &event = teleportEvent(i);
        if (!sameName(event.name, text))
            continue;
        request->kind = event.kind;
        request->stage = event.stage;
        request->variant = (event.parts & 1) != 0 ? 0 : 1;
        request->branch = event.branch;
        strcpy(request->event, event.name);
        return true;
    }
    return false;
}

void teleportBootRequest()
{
    const char *text = getenv("YAMIPAN_TELEPORT");
    if (text == nullptr || text[0] == '\0')
        return;

    TeleportRequest request;
    if (requestFromText(text, &request))
        teleportRequest(request);
    else
        yamipan::log::warning("YAMIPAN_TELEPORT: no stage or event {}", text);
}

bool teleportPending()
{
    return sPending.load();
}

void teleportTake()
{
    if (!sPending.load())
        return;
    {
        std::lock_guard lock(sLock);
        sTaken = sRequest;
        sPending.store(false);
    }

    StageSelect *select = getStageSelect();
    select->stage = sTaken.stage;
    select->variant = sTaken.variant;
    if (sTaken.difficulty != kTeleportKeepDifficulty)
        select->kind = sTaken.difficulty;
    select->_10 = 0;
    select->exitKind = StageExit_Navi;      // the road the navigator's jump takes

    const bool event = sTaken.kind == Teleport_FileEvent || sTaken.kind == Teleport_SimpleEvent;
    sPhase = event ? TeleportPhase_Start : TeleportPhase_Idle;
    sMovieDue = sTaken.kind == Teleport_Movie;
    sSkipIntro = sTaken.skipIntro && !getNaviTable()->isSinglePart(sTaken.stage);
    yamipan::log::info("Teleport: stage {} part {} {}", sTaken.stage, sTaken.variant + 1, sTaken.event);
}

bool teleportTakeMovie(int *branch)
{
    if (!sMovieDue)
        return false;
    sMovieDue = false;
    *branch = sTaken.branch;
    return true;
}

bool teleportSkipsIntro()
{
    return sSkipIntro;
}

void teleportIntroDone()
{
    sSkipIntro = false;
}

// A sub-scene's scene has the event on its parent.
void teleportNoteFrame()
{
    StageScene *scene = gStageScene->parent != nullptr ? gStageScene->parent : gStageScene;
    const char *name = scene->event != nullptr ? scene->event->name : "";

    std::lock_guard lock(sLock);
    strncpy(sPlaying, name, sizeof(sPlaying) - 1);
}

// The object whose script starts the event, and where: Root only when nothing
// else does, since it is the stage's whole story. An object not active runs no
// script.
static StageObject *findStarter(StageMission *mission, const int **block)
{
    StageObject *root = nullptr;
    const int *rootBlock = nullptr;
    StageObject **end = mission->scriptObjectsEnd();
    for (StageObject **it = mission->scriptObjects; it != end; it++) {
        StageObject *object = *it;
        if (object == nullptr || !object->m_active)
            continue;
        const int *found = object->scriptRunner()->findEventBlock(sTaken.event);
        if (found == nullptr)
            continue;
        if (strcmp(object->m_name, kRootName) != 0) {
            *block = found;
            return object;
        }
        root = object;
        rootBlock = found;
    }
    *block = rootBlock;
    return root;
}

// On a trigger volume's floor, beside anything else; nowhere without a floor.
// Root is the whole map and the player is where they are.
static void placePlayer(StageScene *scene, StageObject *starter)
{
    StageObject *player = scene->player;
    if (player == nullptr || starter == nullptr || starter == player
        || strcmp(starter->m_name, kRootName) == 0)
        return;

    const sceVu0FVECTOR *world = starter->getWorldMatrix();
    sceVu0FVECTOR spot = { world[3][0], world[3][1], world[3][2], 1.0f };
    int kind = starter->getKind();
    if (kind != StageObject_BoxVolume && kind != StageObject_SphereVolume) {
        spot[0] += world[1][0] * kBesideObject;
        spot[1] += world[1][1] * kBesideObject;
    }

    float height = spot[2] + kFloorUp;
    if (scene->findFloorFaceExcept(spot[0], spot[1], kFloorRange, &height, kFaceFloor, kFaceFloorSkip, nullptr) == nullptr) {
        yamipan::log::warning("Teleport: no floor at {}, the player stays", starter->m_name);
        return;
    }
    spot[2] = height;
    player->scriptSetPosition(spot);
}

static void startDirectly(StageScene *scene)
{
    scene->fade->start(kFadeNow, kFadeBlack, kFadeClear);
    if (sTaken.kind == Teleport_FileEvent) {
        scene->sound->setMuted(true);
        scene->startFileEvent(kFadeNow, sTaken.event);
    } else {
        // StageSimpleEvent::begin ends at once, silently, without its entry.
        if (scene->scripts[0]->findEntry(sTaken.event) == nullptr)
            yamipan::log::warning("Teleport: the stage's script has no entry {}", sTaken.event);
        scene->startSimpleEvent(kFadeNow, sTaken.event);
    }
    sPhase = TeleportPhase_After;
    sPhaseFrames = kFadeCheckFrames;
}

// After the frame's scripts, so a block that started the event this frame shows
// here. While any event runs the stage's scripts stop and this is not called:
// a stage's own first event, if its script started one, ends first.
void teleportStageFrame(StageMission *mission)
{
    StageScene *scene = gStageScene;
    if (sPhase == TeleportPhase_Idle)
        return;

    if (scene->event != nullptr) {
        // TeleportPhase_Start: it is the stage's own opening event.
        if (sPhase != TeleportPhase_After && strcmp(scene->event->name, sTaken.event) == 0) {
            yamipan::log::info("Teleport: {} started by its script", sTaken.event);
            sPhase = TeleportPhase_After;
            sPhaseFrames = kFadeCheckFrames;
        }
        return;
    }

    switch (sPhase) {
    case TeleportPhase_Start: {
        mission->pendingEvent[0] = '\0';     // an opening event gives way
        const int *block = nullptr;
        if (sTaken.fromTrigger) {
            StageObject *starter = findStarter(mission, &block);
            placePlayer(scene, starter);
            if (block != nullptr) {
                yamipan::log::info("Teleport: {} from {}", sTaken.event, starter->m_name);
                starter->scriptRunner()->jumpTo(block);
                sPhase = TeleportPhase_Script;
                sPhaseFrames = kScriptStartFrames;
            }
        }
        if (block == nullptr) {
            yamipan::log::info("Teleport: {} starts directly", sTaken.event);
            startDirectly(scene);
        }
        break;
    }
    case TeleportPhase_Script:
        if (--sPhaseFrames <= 0) {
            yamipan::log::warning("Teleport: its script did not start {}", sTaken.event);
            startDirectly(scene);
        }
        break;
    default:
        if (--sPhaseFrames <= 0) {
            yamipan::log::info("Teleport: {} over", sTaken.event);
            if (scene->fade->isFullInMode2())
                scene->fade->start(kFadeBackTime, kFadeClear, kFadeFromHere);
            sPhase = TeleportPhase_Idle;
        }
        break;
    }
}
