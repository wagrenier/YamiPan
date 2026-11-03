/* ==========================================================================
 *  game/gamehooks.h
 *
 *  Subsystems main and its loop call that have no TU of their own yet:
 *  the rx runtime's game layer and SPK.ROM (game/spkrom.cpp), sound
 *  (sound/sound.cpp), the empty debug hooks
 *  0x151270..0x151290, the scene controller's callees (game/scene.cpp) and the
 *  stage result (0x2b0580). Each moves to its TU's header when that TU is written.
 *  Minimal.
 * ======================================================================== */
#ifndef _GAME_GAMEHOOKS_H
#define _GAME_GAMEHOOKS_H

#include <stddef.h>

#include "sdk/scetypes.h"

// rx boot: thread priorities given absolute, made relative to the caller.
void rxInit(const char *bootPath, int prio0, int prio1, int prio2);      // FS2 0x001527a0
void rxTerm();                                                           // FS2 0x00152750

void soundInit(int isPal);                                               // FS2 0x001486e0
void soundUpdate();                                                      // FS2 0x00148500
void soundShutdown();                                                    // FS2 0x00148540

// The sound output settings (only what game/sysdata.cpp calls).
// layout: host -- no fields declared yet (EE: the mode at +0x14, the
// output / surround bytes at +0x48 / +0x49).
struct SoundConfig
{
    void setOutputMode(int mode);                                        // FS2 0x00147ee0: SoundOutput
};
extern SoundConfig *gSoundConfig;   /* sbss 0x31addc (gp-0x7a94) */

// The stage just played, at 0x599700 (only what game/sysdata.cpp reads).
struct StageResult
{
    u_int getPerfectBits() const;                                        // FS2 0x002b00d0

    int    counter[4];          // FS2 +0x00: getPerfectBits sets one bit per counter at 0
    u_char _10[0x88];
    u_int  result;              // FS2 +0x98: kept when lower (every stage but 29)
    u_char _9c[0xc];
    u_int  resultHighest;       // FS2 +0xa8: kept when higher (stage 29)
    u_char _ac[0x10];
    int    recorded;            // FS2 +0xbc: nonzero when the result counts for the records
};
static_assert(offsetof(StageResult, result) == 0x98, "StageResult::result");
static_assert(offsetof(StageResult, resultHighest) == 0xa8, "StageResult::resultHighest");
static_assert(offsetof(StageResult, recorded) == 0xbc, "StageResult::recorded");

StageResult *getStageResult();                                           // FS2 0x002b0580

// Empty in retail: a stripped system's init / per-frame / teardown (guess).
void debugSysInit();                                                     // FS2 0x00151290
void debugSysUpdate();                                                   // FS2 0x00151270
void debugSysShutdown();                                                 // FS2 0x00151280

// True when the point (16-aligned xyzw) lies in the box whose world-to-unit-
// cube matrix is m (VU0 transform + vclip against w; game/boxgrid.cpp).
bool boxContainsPoint(const float m[4][4], const float p[4]);            // FS2 0x002362a0

// What game/scene.cpp calls.
// The resident packs: delete the one held and load the file again
// (dataEU/packinitEU.pak at 0x31ae60; dataEU/packmenuEU.pak at 0x31ae98,
// only while not loaded).
void loadInitPack();                                                     // FS2 0x00290840
void loadMenuPack();                                                     // FS2 0x002c3e10
// Runs 0x2afd50 in thread slot 3 and waits for it: packgameEU.pak and an
// object built from it, likely the boot screens (guess).
void runStartupSequence();                                               // FS2 0x002afd00
// Thread slot 6 (when free): draws the overlay objects gp-0x7a28 / -0x7a20
// into packet list 0xb every frame.
void overlayThreadStart();                                               // FS2 0x00267780
// Deletes the overlay object gp-0x7a28 and clears its show flag.
void overlayReleaseScreen();                                             // FS2 0x00267a00
// The scene table's only scene, "Menu (Pack File)": the menu flow 0x2b5770(1).
void menuSceneThread(void *arg);                                         // FS2 0x002b5740

#endif /* _GAME_GAMEHOOKS_H */
