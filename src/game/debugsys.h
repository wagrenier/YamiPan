/*
 *  game/debugsys.h -- the debug system's hooks, empty in retail.
 *
 *  gameInit calls debugSysInit after the game system exists, gameShutdown
 *  debugSysShutdown after the memory card, and the main thread debugSysUpdate
 *  at the top of every frame, before the vsync wait. In the European ROM all
 *  three are a bare "jr ra": the debug build's system was compiled out and
 *  only its entry points stayed (like PerfMeter's drawing, architecture.md).
 *  No source string names the file: the name is the port's.
 */
#ifndef GAME_DEBUGSYS_H
#define GAME_DEBUGSYS_H

void debugSysUpdate();                                                   // FS2 0x00151270
void debugSysShutdown();                                                 // FS2 0x00151280
void debugSysInit();                                                     // FS2 0x00151290

#endif
