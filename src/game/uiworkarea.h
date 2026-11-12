/* ==========================================================================
 *  game/uiworkarea.h
 *
 *  The UI work area (game/uiworkarea.cpp, 0x243350..0x243390): a memory
 *  block the movie widget lends its player. menuPlayMovie sets it to the
 *  stage pack's buffer around a stage's opening movie; null lets the
 *  player allocate its own. The file name is the port's (no source string).
 * ======================================================================== */
#ifndef _GAME_UIWORKAREA_H
#define _GAME_UIWORKAREA_H

void  setUiWorkArea(void *area);                                         // FS2 0x00243370
void *getUiWorkArea();                                                   // FS2 0x00243380

#endif
