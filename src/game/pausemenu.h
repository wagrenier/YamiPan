/* ==========================================================================
 *  game/pausemenu.h
 *
 *  The stage's pause menu (start) and map menu (select), FS2 0x20acb0..
 *  0x20b120 (game/pausemenu.cpp): each runs its layout in a thread of
 *  ThreadSlot_Dialog, over the BG movie screen, while the stage loop polls
 *  the answer and StageScene::drawPaused keeps the stage on screen behind.
 *  The file name is the port's (no source string in the range).
 * ======================================================================== */
#ifndef _GAME_PAUSEMENU_H
#define _GAME_PAUSEMENU_H

// What the pause menu answered (pauseMenuResult). The stage screen layout
// answers 0..3 itself; 4 is the pause menu's own, after a navigator pick.
enum PauseMenuChoice
{
    PauseMenu_Running  = -1,
    PauseMenu_Resume   = 0,
    PauseMenu_Restart  = 1,     // the stage loop: StageResult::restart
    PauseMenu_Continue = 2,     // the stage loop: StageResult::addContinue
    PauseMenu_Quit     = 3,     // keeps the BG movie screen up
    PauseMenu_Navi     = 4,     // a stage picked in the navigator
};

// The stage screen layout's other answers: the pause menu runs the screen
// behind each, then the layout again with the cursor where it was.
enum PauseMenuScreen
{
    PauseMenuScreen_Navi    = 4,    // the navigator (runNaviFile)
    PauseMenuScreen_Archive = 5,    // runArchiveScreen
    PauseMenuScreen_StageH  = 6,    // runStageScreenH: the stage's difficulty screen
    PauseMenuScreen_Help    = 7,    // runHelpScreen, page 0
    PauseMenuScreen_Options = 8,    // runOptionScreen
    PauseMenuScreen_MapHelp = 9,    // runHelpScreen, page 7
};

// What the map layout answers to show its help page (runHelpScreen page 7)
// and run again; any other answer closes it.
constexpr int kMapMenuHelp = 1;

// The help pages the two menus open (menuscene.h has the navigator's 8 and
// the archive's 9).
constexpr int kHelpPageStage = 0;
constexpr int kHelpPageMap = 7;

void pauseMenuStart();                                                   // FS2 0x0020acf0
int  pauseMenuResult();                                                  // FS2 0x0020acb0: PauseMenuChoice
void pauseMenuThread(void *arg);                                         // FS2 0x0020ad20
int  runPauseMenu();                                                     // FS2 0x0020ad70: PauseMenuChoice
void mapMenuStart();                                                     // FS2 0x0020afe0
int  mapMenuResult();                                                    // FS2 0x0020afa0: -1 while open, then 0
void mapMenuThread(void *arg);                                           // FS2 0x0020b010

#endif
