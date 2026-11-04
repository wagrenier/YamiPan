/* ==========================================================================
 *  game/titlescreen.cpp
 *
 *  The title screen and the two loading hooks behind it, 0x231c90..0x231d40.
 * ======================================================================== */
#include "game/titlescreen.h"

#include "game/layoutpaths.h"
#include "game/screen.h"

// ──────────────────────────────────────────────────────────────────────
// Any other answer of the layout, a cancel included, is TitleAnswer_None.
// FS2 0x00231c90

int runTitleScreen(bool withIntro)
{
    const char *path = titleLayoutPath();
    int answer = runLayoutFile(path, withIntro ? TitleLayoutArg_Intro : TitleLayoutArg_NoIntro,
                               false, nullptr, true);
    if (answer == TitleAnswer_LoadGame || answer == TitleAnswer_NewGame)
        return answer;
    return TitleAnswer_None;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00231d20

void loadingTick()
{
    overlayShowLoading();
}

// FS2 0x00231d30

void loadingBegin()
{
    overlayShowLoading();
}
