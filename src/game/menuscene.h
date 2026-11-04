/* ==========================================================================
 *  game/menuscene.h
 *
 *  The menu scene (game/menuscene.cpp, 0x2b5740..0x2b6b90): the scene table's
 *  only scene, "Menu (Pack File)", and with it everything the player walks
 *  through outside a stage. menuSceneThread runs menuFlow in slot 2 and it
 *  never returns:
 *
 *    language screen, card check, sce logo
 *    forever:  title -> new game (difficulty) or load
 *              forever: top menu -> navi screen -> stage -> save dialogs
 *
 *  The stage itself is stageThread in slot 3; menuPlayStage loads it, plays
 *  its movies and records the result. Only menuSceneThread is called from
 *  outside: the rest is file-local, in the ROM's address order. The file
 *  name is the port's (no source string in the range).
 * ======================================================================== */
#ifndef _GAME_MENUSCENE_H
#define _GAME_MENUSCENE_H

#include "sdk/scetypes.h"

// What 2dEU/topMenu%s.d2b answers. Others just show it again.
enum TopMenuAnswer
{
    TopMenuAnswer_Title    = 0,     // back to the title
    TopMenuAnswer_Title3   = 3,     // the same
    TopMenuAnswer_Play     = 4,     // the navi screen
    TopMenuAnswer_Archive  = 5,
    TopMenuAnswer_Options  = 8,
    TopMenuAnswer_Extra    = 10,    // runExtraMode
};

// What 2dEU/newGame%s.d2b answers.
enum NewGameAnswer
{
    NewGameAnswer_Back    = 0,
    NewGameAnswer_Kind0   = 1,      // the record kinds of StageKind, one up
    NewGameAnswer_Kind1   = 2,
    NewGameAnswer_Kind2   = 3,
    NewGameAnswer_Options = 8,
};

// The reasons menuSaveDialog hands mcBase%s.d2b (the load screen is 0).
enum SaveDialogReason
{
    SaveDialog_Load      = 0,   // menuLoadGame, not through menuSaveDialog
    SaveDialog_NaviNode  = 1,   // after a stage whose node has a second part, or a part that did not play
    SaveDialog_StageQuit = 2,   // leaving the navi with something unsaved
    SaveDialog_Archive   = 3,   // the archive changed the save
    SaveDialog_Options   = 4,   // the options changed
    SaveDialog_Extra     = 5,   // runExtraMode changed the save
};

// runHelpScreen's pages.
constexpr int kHelpPageNavi = 8;
constexpr int kHelpPageArchive = 9;

// memAlt%s.d2b's argument, and the answer that stops the card check.
constexpr int kMemAltNoCard = 0;
constexpr int kMemAltNoRoom = 1;
constexpr int kMemAltContinue = 1;

// The stages that end with the tutorial notes (navi_tu%s.d2b, with this arg).
constexpr int kNaviTuStageA = 0x26;     // arg 0
constexpr int kNaviTuStageB = 0x2f;     // arg 1

// A stage pack's buffer on the global heap, unpacked pack and read file alike.
constexpr u_int kStagePackBufferSize = 0x1800000;
constexpr u_int kStagePackAlignMovie = 0x80;    // menuPlayStage
constexpr u_int kStagePackAlignPlain = 0x40;    // menuRunStage
// A stage pack holds three variants (one per StageKind) per language.
constexpr int kStagePackVariantsPerLanguage = 3;

// Slot 2's scene. The argument is unused: "Pack File" is built in.
void menuSceneThread(void *arg);                                         // FS2 0x002b5740

#endif /* _GAME_MENUSCENE_H */
