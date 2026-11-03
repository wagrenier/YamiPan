/* ==========================================================================
 *  game/screen.h
 *
 *  The 2D screens around the game (game/screen.cpp, 0x2676e0..0x2681e0):
 *
 *  - the overlay thread (slot 6) that draws two layouts over everything
 *    else in packet list 11: the BG movie screen (2dEU/BGMOVIEEU.d2b from
 *    the init pack, shown until overlayReleaseScreen) and the loading
 *    indicator (language pack item 0, shown only on frames that ask for it:
 *    FileLoader::waitLoaded asks while it waits);
 *  - the modal layout runner (runLayout / runLayoutFile): a layout drawn in
 *    packet list 12 until it ends, answering its result, or -1 on cross when
 *    cancelable;
 *  - the screens built on it: a stage's e / h screens, the archive and the
 *    options, and the game-over and notice dialogs, which run in slot 4 so
 *    the stage loop keeps drawing behind them.
 *
 *  The file name is the port's (no source string in the range).
 * ======================================================================== */
#ifndef _GAME_SCREEN_H
#define _GAME_SCREEN_H

#include "sdk/scetypes.h"

// runLayout's answer when the player cancels (and runLayoutFile's when the
// file is empty).
constexpr int kLayoutCancelled = -1;
// What runLayoutFile / runLayout pass UiLayout::setData when the caller has
// nothing to pass.
constexpr int kLayoutNoArg = -1;

// gameOverDialogResult: what the player picked.
enum GameOverChoice
{
    GameOverChoice_Running  = -1,
    GameOverChoice_Restart  = 0,    // the stage loop: StageResult::restart
    GameOverChoice_Quit     = 1,
    GameOverChoice_Continue = 2,    // the stage loop: StageResult::addContinue
};

// runArchiveScreen's answer: bit 0 the layout answered 1, bit 1 the save
// data changed.
enum ArchiveScreenResult
{
    ArchiveScreen_Answered1   = 1,
    ArchiveScreen_SaveChanged = 2,
};

// The overlay thread (slot 6, started when the slot is free).
void overlayThreadStart();                                               // FS2 0x00267780
void overlayThread(void *arg);                                           // FS2 0x00267830
void overlayThreadCleanup(void *arg);                                    // FS2 0x002677e0
// Shows the BG movie screen layout until overlayReleaseScreen.
void overlayShowScreen();                                                // FS2 0x00267a30
// Deletes the BG movie screen layout and clears its show flag.
void overlayReleaseScreen();                                             // FS2 0x00267a00
// Shows the loading indicator for this frame.
void overlayShowLoading();                                               // FS2 0x00267950

// The modal layout runner.
int runLayout(void *item, int arg, bool cancelable, const char *baseDir); // FS2 0x00267ae0
int runLayoutFile(const char *path, int arg, bool cancelable,
                  const char *baseDir, bool yieldAfter);                 // FS2 0x00267c00

// The screens.
void runStageScreenE();                                                  // FS2 0x002676e0
void runStageScreenH();                                                  // FS2 0x00267730
int  runArchiveScreen(int arg);                                          // FS2 0x00267f80
// Answers 1 when the options or the play record's kind changed.
int  runOptionScreen(int arg);                                           // FS2 0x00268050

// The dialogs (slot 4): start, then poll each frame.
void gameOverDialogStart();                                              // FS2 0x00267de0
void gameOverDialogThread(void *item);                                   // FS2 0x00267e50
int  gameOverDialogResult();                                             // FS2 0x00267d80: GameOverChoice
void noticeDialogStart();                                                // FS2 0x00267ee0
void noticeDialogThread(void *item);                                     // FS2 0x00267f30
// -1 while the notice is up, then 0.
int  noticeDialogState();                                                // FS2 0x00267ea0

#endif /* _GAME_SCREEN_H */
