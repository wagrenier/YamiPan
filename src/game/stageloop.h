/* ==========================================================================
 *  game/stageloop.h
 *
 *  The stage loop (game/stageloop.cpp, 0x17f150..0x17fce0): the thread
 *  that plays one stage. The menu flow (0x2b60b0) hands it the stage's
 *  loader (setStageLoader: the stage PackFile, or a bare FileLoader) and
 *  starts stageThread in ThreadSlot_Game with the stage script's path;
 *  stageThread builds the stage's sound, files and scene (StageScene),
 *  shows the stage's e / h screens, then runs the scene frame by frame
 *  until the stage is cleared or left from the game-over dialog or the
 *  pause menu, rebuilding the scene for a restart and swapping in a
 *  sub-scene when the scene asks for one. How it ended goes to
 *  StageSelect::exitKind.
 *
 *  The file name is the port's (no source string); its globals are the
 *  sbss words gp-0x7a7c..gp-0x7a6c.
 * ======================================================================== */
#ifndef _GAME_STAGELOOP_H
#define _GAME_STAGELOOP_H

#include "sdk/scetypes.h"

class FileLoader;               // game/fileloader.h
class StageFiles;               // game/gamehooks.h
struct StageScene;              // game/gamehooks.h

// The current stage object; null outside a stage (game/uigame.cpp tests it).
extern StageScene *gStageScene;     /* sbss 0x31adf4 (gp-0x7a7c) */

// The stage's files (StageFiles over the stage loader); null outside a
// stage.
StageFiles *getStageFiles();                                             // FS2 0x0017f150

// Thread slot 3 (ThreadSlot_Game): plays the stage; arg is the stage
// script's path (0x5997c0, stageScriptPath). Ends with threadExit.
void stageThread(void *arg);                                             // FS2 0x0017f160

// Deletes the stage loader held so far and keeps loader (null: none).
void setStageLoader(FileLoader *loader);                                 // FS2 0x0017fc90

// Answers 0: no caller, only the handler-table words 0x318044 / 0x318064
// (the 0x2887a0.. tables) point at it.
int stageAnswerZero();                                                   // FS2 0x0017f7b0

#endif /* _GAME_STAGELOOP_H */
