/* ==========================================================================
 *  helpscreen.cpp
 *
 *  The help screen the menus open (FS2 0x2acff0..0x2ad030): one function,
 *  alone between game/memcard.cpp and game/skymist.cpp.
 * ======================================================================== */
#include "game/gamehooks.h"
#include "game/layoutpaths.h"
#include "game/screen.h"

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002acff0
// callcheck: helpLayoutPath, runLayoutFile

int runHelpScreen(int page)
{
    runLayoutFile(helpLayoutPath(), page, false, nullptr, true);
    return 0;
}
