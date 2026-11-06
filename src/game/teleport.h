/* ==========================================================================
 *  game/teleport.h
 *
 *  PC: not in the ROM. The jump to any stage or event, for testing: the
 *  debug menu asks (yamipan/ui/teleport_panel.cpp, F2) or YAMIPAN_TELEPORT
 *  does at boot, from any screen, and the game goes there on its own roads:
 *
 *    teleportPending cuts whatever is running. A layout ends the way a cancel
 *    does (runLayout), a stage the way the pause menu's navigator jump does
 *    (stageThread: StageExit_Navi), and menuFlow treats the title as a new
 *    game. menuPlayLoop then calls teleportTake, which writes the play record
 *    as the navigator's answer would, and plays the stage. On the stage's
 *    frame teleportStageFrame starts the event the way a script would.
 *
 *  An event plays on a stage that has just started, not at the moment of the
 *  story that would start it: whatever a script does around it does not run.
 *
 *  The tables come from the disc: tools/reverse/eventscan.py --write.
 * ======================================================================== */
#ifndef _GAME_TELEPORT_H
#define _GAME_TELEPORT_H

#include "sdk/scetypes.h"

struct StageMission;

enum TeleportKind
{
    Teleport_Stage = 0,         // a stage, or a film's navigator node
    Teleport_FileEvent,         // demoEU/s<stage>/<name>.edp (StageFileEvent)
    Teleport_SimpleEvent,       // an entry of the stage's script (StageSimpleEvent)
    Teleport_Movie,             // the movie of a stage's clear (kStageMovies)
};

// A stage's parts as bits: bit n is StageSelect::variant n.
constexpr int kTeleportParts = 2;
constexpr int kTeleportKeepDifficulty = -1;

// layout: host
struct TeleportStage
{
    short stage;
    short parts;                // the parts the disc has a pack for
};

// layout: host
struct TeleportEvent
{
    const char *name;
    short       stage;
    u_char      parts;          // the parts whose scripts start it
    u_char      kind;           // TeleportKind
    u_char      branch;         // Teleport_Movie: kStageMovies' branch
    const char *startedBy;      // the script entries that start it, empty when none does
};

// Written by the host, in any thread. layout: host
struct TeleportRequest
{
    int  kind;                  // TeleportKind
    int  stage;
    int  variant;
    int  difficulty;            // a StageKind, or kTeleportKeepDifficulty
    int  branch;
    bool skipIntro;             // no opening movie, no e / h screens
    char event[0x10];           // kMissionEventName
};

// What the game is doing, for the menu. layout: host
struct TeleportStatus
{
    bool pending;               // a request waits for the game to reach a place it can leave
    bool inStage;
    int  stage;
    int  variant;
    int  difficulty;
    char event[0x10];           // the event playing, empty none
};

int teleportStageCount();
const TeleportStage &teleportStage(int index);
int teleportFilmCount();
int teleportFilm(int index);
int teleportEventCount();
const TeleportEvent &teleportEvent(int index);

// Any thread. A second request replaces one not taken yet.
void teleportRequest(const TeleportRequest &request);
TeleportStatus teleportStatus();

// The game's side (the callers are marked PC).
void teleportBootRequest();                                 // YAMIPAN_TELEPORT
bool teleportPending();
void teleportTake();                                        // into the play record
bool teleportTakeMovie(int *branch);
bool teleportSkipsIntro();
void teleportIntroDone();
void teleportNoteFrame();                                   // each frame of a stage
void teleportStageFrame(StageMission *mission);             // StageMission::afterScripts

#endif /* _GAME_TELEPORT_H */
