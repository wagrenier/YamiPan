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
 *    as the navigator's answer would, and plays the stage.
 *
 *  An event goes on the stage's first frame. The object whose script starts
 *  it is found in the script as loaded, the player is put on it, and that
 *  script runs from the block that starts the event: its conditions are
 *  skipped, not what it does after the event (the story's next state, who
 *  stands where, the fade back). Without such an object the event just starts.
 *  What the stage's earlier steps would have set stays unset.
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
    bool fromTrigger;           // through its script, the player on its trigger
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
