/* ==========================================================================
 *  game/gamehooks.h
 *
 *  Subsystems main and its loop call that have no TU of their own yet:
 *  the rx runtime's game layer and SPK.ROM (game/spkrom.cpp), sound
 *  (sound/sound.cpp), the memory card (0x2a9c80..), the empty debug hooks
 *  0x151270..0x151290 and the scene controller thread (0x231bb0). Each
 *  moves to its TU's header when that TU is written. Minimal.
 * ======================================================================== */
#ifndef _GAME_GAMEHOOKS_H
#define _GAME_GAMEHOOKS_H

// rx boot: thread priorities given absolute, made relative to the caller.
void rxInit(const char *bootPath, int prio0, int prio1, int prio2);      // FS2 0x001527a0
void rxTerm();                                                           // FS2 0x00152750

void soundInit(int isPal);                                               // FS2 0x001486e0
void soundUpdate();                                                      // FS2 0x00148500
void soundShutdown();                                                    // FS2 0x00148540

void memcardInit();                                                      // FS2 0x002a9c80
void memcardLibEnd();                                                    // FS2 0x002acec0

// Empty in retail: a stripped system's init / per-frame / teardown (guess).
void debugSysInit();                                                     // FS2 0x00151290
void debugSysUpdate();                                                   // FS2 0x00151270
void debugSysShutdown();                                                 // FS2 0x00151280

// Thread slot 1: runs the scene table 0x2fabd4 in slot 2, one scene after another.
void gameSceneThread();                                                  // FS2 0x00231bb0

#endif /* _GAME_GAMEHOOKS_H */
