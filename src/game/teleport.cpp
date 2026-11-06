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

static std::mutex        sLock;
static TeleportRequest   sRequest;              // sLock
static std::atomic<bool> sPending{false};
static char              sPlaying[sizeof(TeleportStatus::event)];   // sLock

// Only the game's threads, which take turns.
static TeleportRequest sTaken;
static bool sEventDue;
static bool sMovieDue;
static bool sSkipIntro;
static bool sFadeBack;

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

    sEventDue = sTaken.kind == Teleport_FileEvent || sTaken.kind == Teleport_SimpleEvent;
    sMovieDue = sTaken.kind == Teleport_Movie;
    sSkipIntro = sTaken.skipIntro && !getNaviTable()->isSinglePart(sTaken.stage);
    sFadeBack = false;
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

// The stage's own first event, if a script started one, ends first; one only
// asked for this frame gives way. A script that starts an event fades back
// itself after it, which no script does here.
void teleportStageFrame(StageMission *mission)
{
    StageScene *scene = gStageScene;
    if (scene->event != nullptr)
        return;

    if (sEventDue) {
        sEventDue = false;
        sFadeBack = true;
        yamipan::log::info("Teleport: event {} starts", sTaken.event);
        mission->pendingEvent[0] = '\0';
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
    } else if (sFadeBack) {
        sFadeBack = false;
        scene->fade->start(kFadeBackTime, kFadeClear, kFadeFromHere);
        yamipan::log::info("Teleport: event {} ended", sTaken.event);
    }
}
