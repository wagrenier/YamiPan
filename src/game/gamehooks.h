/* ==========================================================================
 *  game/gamehooks.h
 *
 *  Subsystems main and its loop call that have no TU of their own yet:
 *  the rx runtime's game layer and SPK.ROM (game/spkrom.cpp), sound
 *  (sound/sound.cpp), the memory card (0x2a9c80..), the empty debug hooks
 *  0x151270..0x151290, the scene controller thread (0x231bb0), the RNG
 *  (0x104720), the stage records' defaults (0x296d00) and the stage result
 *  (0x2b0580). Each moves to its TU's header when that TU is written.
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

void memcardInit();                                                      // FS2 0x002a9c80
void memcardLibEnd();                                                    // FS2 0x002acec0
// The memcard module's byte gp-0x79f0; SaveData::setDefaults seeds +0x468 from it.
u_char memcardGetState();                                                // FS2 0x002ac610

// A float in [0, 1) (shuffle table over an LCG).
float randUnit();                                                        // FS2 0x00104720

// The record a fresh save holds for (kind, variant, stage).
u_int stageDefaultRecord(int kind, int variant, int stage);              // FS2 0x00296d00

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

// Thread slot 1: runs the scene table 0x2fabd4 in slot 2, one scene after another.
void gameSceneThread();                                                  // FS2 0x00231bb0

#endif /* _GAME_GAMEHOOKS_H */
