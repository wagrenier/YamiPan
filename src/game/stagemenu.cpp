/* ==========================================================================
 *  game/stagemenu.cpp -- the stage's own menu (FS2 0x2303c0..0x230980, vtable
 *  0x3164b0): a state machine around a list window of 0xaa8 bytes
 *  (ctor 0x20ac30, the code around 0x208f40..0x20ac30). The menu button
 *  opens it, its first button leaves the picked line's text in `pending`
 *  for one frame, where the scripts read it (native 0x45, getMenuMode 3).
 *  The file name is the port's.
 *
 *  Only the ctor is written. update, draw and destroy wait for the window:
 *  with update written and no window, the button would open a menu nobody
 *  sees that holds the input focus.
 * ======================================================================== */
#include "game/gamehooks.h"

// PC: the list window is not built yet, so `list` stays null, and the members
// start as the ROM's ctor sets them (gamehooks.h). A stub here left them as
// the host heap had them, and the scripts' native 0x45 followed that garbage.
// FS2 0x002308a0
// callcheck: operator_new, FUN_0020ac30 (the window: file header)

StageMenu::StageMenu(WideFontPrinter *, int)
{
}
