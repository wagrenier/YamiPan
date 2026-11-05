/* ==========================================================================
 *  game/stageresult.h
 *
 *  The stage result (game/stageresult.cpp, 0x2afc20..0x2b0d50): one
 *  object at 0x599700 the stage loop (0x17f160) starts with a clock mode
 *  and ticks every frame. It holds the play's counters (the medal bits come
 *  from them), two clocks the stage scripts set and read, the clock's
 *  on-screen layout, and whether the result counts for the records
 *  (SystemData::recordStageResult). Beside it in the same object file: the
 *  game-pack loader (dataEU/packgameEU.pak, parsed into PackTables once
 *  before the scene loop) and two init-pack lookups by language.
 * ======================================================================== */
#ifndef _GAME_STAGERESULT_H
#define _GAME_STAGERESULT_H

#include <stddef.h>

#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

class TexAreaList;             // game/gsblockmap.h
class UiLayout;

// StageResult::mode: which clock the stage runs. The stage loop picks
// Countdown on kStageHighestWins (29), else CountUp when the play record's
// byte +0x10 is set, else None.
enum StageClockMode
{
    StageClock_None      = 0,   // no clock and no record (recordStageResult)
    StageClock_CountUp   = 1,   // the clock counts up from 0
    StageClock_Countdown = 2,   // from kCountdownStart down to 0, with cues
};

// The countdown's start, its cue spacing and the value the cues start at
// (StageResult::tick: SYS_COUNT1 at 250, 225, .. 25, SYS_COUNT2 at 0).
constexpr u_int kCountdownStart = 7500;
constexpr u_int kCountdownCueEvery = 25;
constexpr u_int kCountdownCueFrom = 250;

// A rising counter stops at this value (the clocks, SaveData::playTime).
constexpr u_int kCounterStopped = 0xffffffff;

// StageResult::getPerfectBits: one bit per counter still at 0, ORed into
// SaveData::medals. Bit 8 is the no-continue medal; the others' counters
// are counted by code not read yet.
enum PerfectBits
{
    PerfectBit_Counter1   = 0x01,   // counter[1] == 0
    PerfectBit_Counter3   = 0x02,   // counter[3] == 0
    PerfectBit_Counter2   = 0x04,   // counter[2] == 0
    PerfectBit_NoContinue = 0x08,   // counter[0] (StageResult::addContinue) == 0
};

// StageClock::state bits as setGameValue's clock kinds set them (the
// stage scripts 0x1a8ca0.. set and test them too: their meaning there is
// unread, the names say what the game values do).
enum StageClockState
{
    StageClockState_Armed     = 0x01,   // target holds an amount
    StageClockState_Cancelled = 0x02,   // the amount counts as none
    StageClockState_Marked    = 0x04,
    StageClockState_Added     = 0x08,   // the amount went into value
    StageClockState_Settled   = 0x10,   // set with Added
};

// One clock, EE 0x10. The stage scripts (0x1a8ca0..0x1a9000) set value,
// state and target, add to value, and test state for 0 or both bits 0x14.
struct StageClock
{
    // Inline in the ROM (StageResult::tick, twice).
    void advance()
    {
        if (step < 0) {
            if (value != 0)
                value += step;
        } else if (value != kCounterStopped) {
            value += step;
        }
    }

    u_int state;                // FS2 +0x0: set to 1 with target by the scripts
    u_int value;                // FS2 +0x4: the clock (or, for clock2 on stage 29, the score)
    int   step;                 // FS2 +0x8: added each tick
    int   target;               // FS2 +0xc: set by the scripts (0x1a8f40 / 0x1a8e00)
};
static_assert(sizeof(StageClock) == 0x10, "StageClock: EE 0x10");

// The stage just played, at 0x599700.
// layout: host -- the layout pointer at +0xb8 (EE 0xc0); the offsets up to
// +0xb5 hold on both.
struct StageResult
{
    // Inline in the static initialiser 0x310f60.
    StageResult() : layout(nullptr), mode(StageClock_None) { begin(StageClock_None); }
    ~StageResult();                                                      // FS2 0x002b0cf0

    bool  beatsRecord() const;                                           // FS2 0x002aff80
    u_int getPerfectBits() const;                                        // FS2 0x002b00d0
    void  update(sceVif1Packet *packet, TexAreaList *texList);           // FS2 0x002b0120
    void  tick();                                                        // FS2 0x002b01a0
    void  addContinue();                                                 // FS2 0x002b0310
    void  restart();                                                     // FS2 0x002b0320
    bool  hasAttemptFlag() const;                                        // FS2 0x002b03b0
    void  releaseLayout();                                               // FS2 0x002b03c0
    void  begin(int newMode);                                            // FS2 0x002b0400

    // Inline in the ROM (restart, begin): the clocks for the mode, the
    // pause bytes and the counters.
    void resetPlay();

    int        counter[4];      // FS2 +0x00: [0] continues, [2] hits on the player (takeDamage); getPerfectBits
    int        attempt[16];     // FS2 +0x10: cleared by restart; 0x17f7c0 reads [15]
    int        stage[16];       // FS2 +0x50: cleared by begin only
    int        _90;             // FS2 +0x90: cleared by begin
    StageClock clock;           // FS2 +0x94: the stage clock (records keep the lowest)
    StageClock clock2;          // FS2 +0xa4: stage 29's record (kept when higher)
    u_char     paused;          // FS2 +0xb4: set by the stage loop around its menus
    u_char     hold;            // FS2 +0xb5: stops the countdown; the scripts clear it
    UiLayout  *layout;          // FS2 +0xb8: the clock on screen (modes 1 and 2)
    int        mode;            // FS2 +0xbc: StageClockMode
};
static_assert(offsetof(StageResult, attempt) == 0x10, "StageResult::attempt");
static_assert(offsetof(StageResult, stage) == 0x50, "StageResult::stage");
static_assert(offsetof(StageResult, _90) == 0x90, "StageResult::_90");
static_assert(offsetof(StageResult, clock) == 0x94, "StageResult::clock");
static_assert(offsetof(StageResult, clock2) == 0xa4, "StageResult::clock2");
static_assert(offsetof(StageResult, paused) == 0xb4, "StageResult::paused");
static_assert(offsetof(StageResult, hold) == 0xb5, "StageResult::hold");

extern StageResult gStageResult;    /* bss 0x599700 */

StageResult *getStageResult();                                           // FS2 0x002b0580

// Init-pack items picked by the language (callers 0x195e10 / 0x1a20f0;
// what the items are is not read yet).
void *getInitItemForLanguage();                                          // FS2 0x002afc20
void *getInitItem21();                                                   // FS2 0x002afcd0

// Loads dataEU/packgameEU.pak in thread slot 3 and waits for it: the pack's
// tables are parsed (PackTables::parse) and the pack is freed again.
void loadGamePack();                                                     // FS2 0x002afd00
void loadGamePackThread(void *arg);                                      // FS2 0x002afd50
void loadGamePackTables();                                               // FS2 0x002afd80

#endif /* _GAME_STAGERESULT_H */
