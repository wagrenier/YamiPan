/* ==========================================================================
 *  game/titlescreen.h
 *
 *  The title screen and the two loading hooks behind it
 *  (game/titlescreen.cpp, 0x231c90..0x231d40). The file name is the port's:
 *  no source string in the range.
 * ======================================================================== */
#ifndef _GAME_TITLESCREEN_H
#define _GAME_TITLESCREEN_H

// runTitleScreen's answer.
enum TitleAnswer
{
    TitleAnswer_None     = 0,   // the title ended unanswered: runAttractDemo, the title again
    TitleAnswer_NewGame  = 1,
    TitleAnswer_LoadGame = 2,
};

// What runTitleScreen hands the layout (UiLayout::setData's arg).
enum TitleLayoutArg
{
    TitleLayoutArg_Intro   = 0,
    TitleLayoutArg_NoIntro = 1,
};

// title<lang>.d2b, without its intro unless withIntro: TitleAnswer.
int  runTitleScreen(bool withIntro);                                     // FS2 0x00231c90

// A load's wait asks for the loading indicator through these: once at its
// start, then once per turn of its loop.
void loadingTick();                                                      // FS2 0x00231d20
void loadingBegin();                                                     // FS2 0x00231d30

#endif /* _GAME_TITLESCREEN_H */
