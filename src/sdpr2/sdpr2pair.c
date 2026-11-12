/* ==========================================================================
 *  sdpr2/sdpr2pair.c
 *
 *  SDPR2's two-voice type (registry SDPR2_TYPE_PAIR; FS2 0x16c870..
 *  0x16e498, data 0x2f5fa0..0x2f5fb8, bss 0x537c50..0x537cc0). See
 *  sdpr2pair.h.
 *
 *  A handle holds two SE voices (Sdpr2Slot.voice[0] / [1]) and plays as
 *  long as one of them does: every voice call is made on both, and fails
 *  only when both fail. The slot keeps:
 *  - angle: the last angle asked; levelAngle: the angle of the last turn
 *    (spread 0 does not turn: no direction to turn);
 *  - panAngle: the unwrapped pan angle (0..719) of the voices;
 *  - flip: the voices' roles swapped (sdpr2PairTurn);
 *  - state: sdpr2PairUpdate calls left with the second voice detuned.
 *
 *  FS2-only middleware, named by reading (batch 011). The handle checks
 *  are sdpr2check.h's, with this file's own message copies in the ROM
 *  (0x302920..); SDPR2_TRY's shape makes a failed call twice for its
 *  answer, so a failed pan or play is done twice: kept.
 * ======================================================================== */
#include "sdpr2/sdpr2pair.h"

#include <stddef.h>

#include "sdpr2/sdpr2.h"
#include "sdpr2/sdpr2check.h"
#include "sdpr2/sdpr2drv.h"
#include "sdpr2/sdpr2single.h"

/* sdpr2DrvGetConfig's 8 words: [0] 1 = sdpr2PairUpdate stops a pair
 * that ended (the game passes 0). */
#define SDPR2_CONFIG_WORDS      8
#define SDPR2_CONFIG_STOP_ENDED 0

#define SDPR2_STOP_ENDED_MODE   1   /* sdpr2PairStop's mode for that stop */
#define SDPR2_STOP_PLAY_FAILED  0   /* sdpr2DrvStop's mode when the second play fails */

#define SDPR2_DETUNE_ONE        4096    /* sdpr2PairDetuneData's 1.0 */

/* Sdpr2Slot.state after a play: the updates the second voice stays
 * detuned (modes 0..1 two, 2..5 one). */
#define SDPR2_PAIR_DETUNE_LONG_MODES    2
#define SDPR2_PAIR_STATE_DETUNE_LONG    2
#define SDPR2_PAIR_STATE_DETUNE_SHORT   1
#define SDPR2_PAIR_STATE_TUNED          0

/* ---- data ------------------------------------------------------------- */

const int sdpr2PairDetuneData[SDPR2_PAIR_MODES] =       /* data 0x2f5fa0 */
{
    3520, 3616, 3520, 3616, 3808, 3856
};

/* ---- bss -------------------------------------------------------------- */

static Sdpr2Type s_type;                        /* bss 0x537c50: filled by sdpr2PairInit */
static int s_config[SDPR2_CONFIG_WORDS];        /* bss 0x537ca0 */

// ──────────────────────────────────────────────────────────────────────
// The pair's new pan angle for deg (0..359) from prev (the last turn's
// angle): the step deg - prev taken the short way round; a step of more
// than 90 degrees turns 180 less the other way and swaps the voices
// (*flip ^= 1), so the pair never turns more than a quarter.
// FS2 0x0016c870

static void sdpr2PairTurn(int deg, int prev, int panAngle, int *outPan,
                          int *flip)
{
    int delta, size;

    delta = deg - prev;
    size = delta < 0 ? -delta : delta;
    if (size > SDPR2_ANGLE_RANGE / 2)
    {
        if (delta > 0)
            delta -= SDPR2_ANGLE_RANGE;
        else if (delta < 0)
            delta += SDPR2_ANGLE_RANGE;
        size = delta < 0 ? -delta : delta;
    }
    if (size > SDPR2_ANGLE_RANGE / 4)
    {
        if (delta > 0)
            delta -= SDPR2_ANGLE_RANGE / 2;
        else if (delta < 0)
            delta += SDPR2_ANGLE_RANGE / 2;
        *flip ^= 1;
    }
    *outPan = (panAngle + delta + SDPR2_PAN_TABLE_SIZE) % SDPR2_PAN_TABLE_SIZE;
}

// ──────────────────────────────────────────────────────────────────────
// One voice's gains (Sdpr2PairPan which): volume panned to angle (+ 180
// for the back voices), scaled by the spread table's gain at spread. A
// which out of 0..3 is panned but not scaled (no caller passes one).
// FS2 0x0016c908

static void sdpr2PairCalcPan(int volume, int angle, int spread, int which,
                             int *volL, int *volR, int *signL, int *signR)
{
    int gain;

    if (which == SDPR2_PAIR_PAN_BACK || which == SDPR2_PAIR_PAN_BACK_MAIN)
        angle = (angle + SDPR2_ANGLE_RANGE / 2) % SDPR2_PAN_TABLE_SIZE;
    sdpr2CalcPanVolumes(volume, angle, volL, volR, signL, signR);
    if (which == SDPR2_PAIR_PAN_MAIN || which == SDPR2_PAIR_PAN_BACK_MAIN)
    {
        gain = sdpr2SpreadMainTable[spread];
        *volL = *volL * gain;
        *volR = *volR * gain;
    }
    else if (which == SDPR2_PAIR_PAN_BACK || which == SDPR2_PAIR_PAN_FRONT_BACK)
    {
        gain = sdpr2SpreadBackTable[spread];
        *volL = *volL * gain;
        *volR = *volR * gain;
    }
    *volL = *volL / SDPR2_GAIN_ONE;
    *volR = *volR / SDPR2_GAIN_ONE;
}

// ──────────────────────────────────────────────────────────────────────
// Both voices panned (both signs +1 without surround); -1 only when both
// failed. A flip other than 0 / 1 leaves the ROM's results unset (stale
// registers); flip only ever holds 0 or 1: 0 here.
// FS2 0x0016ca40

static int sdpr2PairSetPan(int voice0, int voice1, int volume, int panAngle,
                           int spread, int flip)
{
    int volL, volR, signL, signR;
    int result0 = 0, result1 = 0;

    if (flip == 0)
    {
        sdpr2PairCalcPan(volume, panAngle, spread, SDPR2_PAIR_PAN_MAIN,
                         &volL, &volR, &signL, &signR);
        if (sdpr2GetSurround() == 0)
        {
            signL = 1;
            signR = 1;
        }
        result0 = sdpr2DrvSetVolume(voice0, volL, volR, signL, signR);
        sdpr2PairCalcPan(volume, panAngle, spread, SDPR2_PAIR_PAN_BACK,
                         &volL, &volR, &signL, &signR);
        if (sdpr2GetSurround() == 0)
        {
            signL = 1;
            signR = 1;
        }
        result1 = sdpr2DrvSetVolume(voice1, volL, volR, signL, signR);
    }
    else if (flip == 1)
    {
        sdpr2PairCalcPan(volume, panAngle, spread, SDPR2_PAIR_PAN_FRONT_BACK,
                         &volL, &volR, &signL, &signR);
        if (sdpr2GetSurround() == 0)
        {
            signL = 1;
            signR = 1;
        }
        result0 = sdpr2DrvSetVolume(voice0, volL, volR, signL, signR);
        sdpr2PairCalcPan(volume, panAngle, spread, SDPR2_PAIR_PAN_BACK_MAIN,
                         &volL, &volR, &signL, &signR);
        if (sdpr2GetSurround() == 0)
        {
            signL = 1;
            signR = 1;
        }
        result1 = sdpr2DrvSetVolume(voice1, volL, volR, signL, signR);
    }
    if (result0 == SDPR2_ERR && result1 == SDPR2_ERR)
        return -1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// A free slot, the main voice played at volume, deg (0..359, also the
// first unwrapped angle) and spread, then the back voice, detuned by the
// pair mode (a failed back voice stops the main one); the slot's fields
// set; its index.
// FS2 0x0016cc40

static int sdpr2PairPlayPan(int xvab, int prog, int note, int volume,
                            int deg, int spread)
{
    Sdpr2Slot *slot;
    int handle, voice0, voice1;
    int volL, volR, signL, signR;

    handle = sdpr2FindFreeSlot();
    if (handle == SDPR2_ERR)
        return sdpr2FindFreeSlot();
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    sdpr2PairCalcPan(volume, deg, spread, SDPR2_PAIR_PAN_MAIN,
                     &volL, &volR, &signL, &signR);
    if (sdpr2GetSurround() == 0)
    {
        signL = 1;
        signR = 1;
    }
    voice0 = sdpr2DrvPlay(xvab, prog, note, volL, volR, signL, signR);
    if (voice0 == SDPR2_ERR)
        return sdpr2DrvPlay(xvab, prog, note, volL, volR, signL, signR);
    sdpr2PairCalcPan(volume, deg, spread, SDPR2_PAIR_PAN_BACK,
                     &volL, &volR, &signL, &signR);
    if (sdpr2GetSurround() == 0)
    {
        signL = 1;
        signR = 1;
    }
    voice1 = sdpr2DrvPlay(xvab, prog, note, volL, volR, signL, signR);
    if (voice1 == SDPR2_ERR)
    {
        sdpr2DrvStop(voice0, SDPR2_STOP_PLAY_FAILED);
        return -1;
    }
    sdpr2DrvSetPitch(voice1,
                     (int)((float)sdpr2PairDetuneData[sdpr2GetPairMode()]
                           * (float)SDPR2_PITCH_ONE
                           * (1.0f / (float)SDPR2_DETUNE_ONE)));
    slot->voice[0] = voice0;
    slot->voice[1] = voice1;
    slot->panAngle = deg;
    slot->volume = volume;
    slot->pitch = SDPR2_PITCH_ONE;
    slot->spread = spread;
    slot->flip = 0;
    slot->levelAngle = deg;
    slot->angle = deg;
    slot->state = (sdpr2GetPairMode() < SDPR2_PAIR_DETUNE_LONG_MODES)
                      ? SDPR2_PAIR_STATE_DETUNE_LONG
                      : SDPR2_PAIR_STATE_DETUNE_SHORT;
    return handle;
}

// ──────────────────────────────────────────────────────────────────────
// Full volume, ahead, full spread.
// FS2 0x0016ce90

static int sdpr2PairPlay(int xvab, int prog, int note)
{
    return sdpr2PairPlayPan(xvab, prog, note, SDPR2_VOLUME_MAX, 0,
                            SDPR2_SPREAD_MAX);
}

// ──────────────────────────────────────────────────────────────────────
// sdpr2PairPlayPan with an SE parameter set (sdpr2DrvPlayEx).
// FS2 0x0016ceb0

static int sdpr2PairPlayPanEx(int xvab, int prog, int note, int volume,
                              int deg, int spread, int preset)
{
    Sdpr2Slot *slot;
    int handle, voice0, voice1;
    int volL, volR, signL, signR;

    handle = sdpr2FindFreeSlot();
    if (handle == SDPR2_ERR)
        return sdpr2FindFreeSlot();
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    sdpr2PairCalcPan(volume, deg, spread, SDPR2_PAIR_PAN_MAIN,
                     &volL, &volR, &signL, &signR);
    if (sdpr2GetSurround() == 0)
    {
        signL = 1;
        signR = 1;
    }
    voice0 = sdpr2DrvPlayEx(xvab, prog, note, volL, volR, signL, signR, preset);
    if (voice0 == SDPR2_ERR)
        return sdpr2DrvPlayEx(xvab, prog, note, volL, volR, signL, signR,
                              preset);
    sdpr2PairCalcPan(volume, deg, spread, SDPR2_PAIR_PAN_BACK,
                     &volL, &volR, &signL, &signR);
    if (sdpr2GetSurround() == 0)
    {
        signL = 1;
        signR = 1;
    }
    voice1 = sdpr2DrvPlayEx(xvab, prog, note, volL, volR, signL, signR, preset);
    if (voice1 == SDPR2_ERR)
    {
        sdpr2DrvStop(voice0, SDPR2_STOP_PLAY_FAILED);
        return -1;
    }
    sdpr2DrvSetPitch(voice1,
                     (int)((float)sdpr2PairDetuneData[sdpr2GetPairMode()]
                           * (float)SDPR2_PITCH_ONE
                           * (1.0f / (float)SDPR2_DETUNE_ONE)));
    slot->voice[0] = voice0;
    slot->voice[1] = voice1;
    slot->panAngle = deg;
    slot->volume = volume;
    slot->pitch = SDPR2_PITCH_ONE;
    slot->spread = spread;
    slot->flip = 0;
    slot->levelAngle = deg;
    slot->angle = deg;
    slot->state = (sdpr2GetPairMode() < SDPR2_PAIR_DETUNE_LONG_MODES)
                      ? SDPR2_PAIR_STATE_DETUNE_LONG
                      : SDPR2_PAIR_STATE_DETUNE_SHORT;
    return handle;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016d108

static int sdpr2PairPlayEx(int xvab, int prog, int note, int preset)
{
    return sdpr2PairPlayPanEx(xvab, prog, note, SDPR2_VOLUME_MAX, 0,
                              SDPR2_SPREAD_MAX, preset);
}

// ──────────────────────────────────────────────────────────────────────
// Both voices stopped and the slot freed (unless both stops failed).
// FS2 0x0016d130

static int sdpr2PairStop(int handle, int mode)
{
    Sdpr2Slot *slot;
    int result0, result1;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE && slot->voice[1] == SDPR2_VOICE_FREE)
        return -1;
    result0 = sdpr2DrvStop(slot->voice[0], mode);
    result1 = sdpr2DrvStop(slot->voice[1], mode);
    if (result0 == SDPR2_ERR && result1 == SDPR2_ERR)
        return -1;
    slot->voice[1] = SDPR2_VOICE_FREE;
    slot->state = SDPR2_PAIR_STATE_TUNED;
    slot->voice[0] = SDPR2_VOICE_FREE;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Every playing slot of this type; a slot whose two stops both failed
// ends the walk.
// FS2 0x0016d2c0

static int sdpr2PairStopAll(int mode)
{
    Sdpr2Slot *slot;
    int i, result0, result1;

    i = 0;
    do
    {
        slot = sdpr2GetSlot(i);
        if (slot == NULL)
            return -1;
        if (slot->type == &s_type
            && (slot->voice[0] != SDPR2_VOICE_FREE
                || slot->voice[1] != SDPR2_VOICE_FREE))
        {
            result0 = sdpr2DrvStop(slot->voice[0], mode);
            result1 = sdpr2DrvStop(slot->voice[1], mode);
            if (result0 == SDPR2_ERR && result1 == SDPR2_ERR)
                return -1;
            slot->state = SDPR2_PAIR_STATE_TUNED;
            slot->voice[0] = SDPR2_VOICE_FREE;
            slot->voice[1] = SDPR2_VOICE_FREE;
        }
        i++;
    } while (i < SDPR2_SLOT_MAX);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The ROM calls sdpr2DrvStop here, not sdpr2DrvPause (jal 0x1703f0 twice,
// on as the stop's mode), and keeps the slot: a paused pair is stopped
// but still looks taken until sdpr2PairUpdate frees it. Kept.
// FS2 0x0016d3a8

static int sdpr2PairPause(int handle, int on)
{
    Sdpr2Slot *slot;
    int result0, result1;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE && slot->voice[1] == SDPR2_VOICE_FREE)
        return -1;
    result0 = sdpr2DrvStop(slot->voice[0], on);
    result1 = sdpr2DrvStop(slot->voice[1], on);
    if (result0 == SDPR2_ERR && result1 == SDPR2_ERR)
        return -1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// pitch: SDPR2_PITCH_ONE = 1.0; the second voice gets it detuned while
// the slot's state says so.
// FS2 0x0016d528

static int sdpr2PairSetPitch(int handle, int pitch)
{
    Sdpr2Slot *slot;
    int result0, result1, pitch1;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE && slot->voice[1] == SDPR2_VOICE_FREE)
        return -1;
    result0 = sdpr2DrvSetPitch(slot->voice[0], pitch);
    pitch1 = pitch;
    if (slot->state > SDPR2_PAIR_STATE_TUNED)
        pitch1 = pitch * sdpr2PairDetuneData[sdpr2GetPairMode()]
                 / SDPR2_DETUNE_ONE;
    result1 = sdpr2DrvSetPitch(slot->voice[1], pitch1);
    if (result0 == SDPR2_ERR && result1 == SDPR2_ERR)
        return -1;
    slot->pitch = pitch;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The slot keeps the pitch as fixed point (truncated).
// FS2 0x0016d6e0

static int sdpr2PairSetPitchF(int handle, float pitch)
{
    Sdpr2Slot *slot;
    int result0, result1;
    float pitch1;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE && slot->voice[1] == SDPR2_VOICE_FREE)
        return -1;
    result0 = sdpr2DrvSetPitchF(slot->voice[0], pitch);
    pitch1 = pitch;
    if (slot->state > SDPR2_PAIR_STATE_TUNED)
        pitch1 = pitch * (float)sdpr2PairDetuneData[sdpr2GetPairMode()]
                 * (1.0f / (float)SDPR2_DETUNE_ONE);
    result1 = sdpr2DrvSetPitchF(slot->voice[1], pitch1);
    if (result0 == SDPR2_ERR && result1 == SDPR2_ERR)
        return -1;
    slot->pitch = (int)(pitch * (float)SDPR2_PITCH_ONE);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The pair turns from the last turn's angle to deg (sdpr2PairTurn); with
// spread 0 both voices sound alike, so it does not turn and the turn
// angle stays.
// FS2 0x0016d8a8

static int sdpr2PairSetAngle(int handle, int deg)
{
    Sdpr2Slot *slot;
    int panAngle, flip;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE && slot->voice[1] == SDPR2_VOICE_FREE)
        return -1;
    flip = slot->flip;
    if (slot->spread == 0)
        panAngle = slot->panAngle;
    else
        sdpr2PairTurn(deg, slot->levelAngle, slot->panAngle, &panAngle, &flip);
    SDPR2_TRY(sdpr2PairSetPan(slot->voice[0], slot->voice[1], slot->volume,
                              panAngle, slot->spread, flip));
    slot->flip = flip;
    if (slot->spread != 0)
        slot->levelAngle = deg;
    slot->angle = deg;
    slot->panAngle = panAngle;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016daa8

static int sdpr2PairSetVolume(int handle, int volume)
{
    Sdpr2Slot *slot;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE && slot->voice[1] == SDPR2_VOICE_FREE)
        return -1;
    SDPR2_TRY(sdpr2PairSetPan(slot->voice[0], slot->voice[1], volume,
                              slot->panAngle, slot->spread, slot->flip));
    slot->volume = volume;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016dc58

static int sdpr2PairSetSpread(int handle, int spread)
{
    Sdpr2Slot *slot;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE && slot->voice[1] == SDPR2_VOICE_FREE)
        return -1;
    SDPR2_TRY(sdpr2PairSetPan(slot->voice[0], slot->voice[1], slot->volume,
                              slot->panAngle, spread, slot->flip));
    slot->spread = spread;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016de08

static int sdpr2PairSetAttr(int handle, int attr)
{
    Sdpr2Slot *slot;
    int result0, result1;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE && slot->voice[1] == SDPR2_VOICE_FREE)
        return -1;
    result0 = sdpr2DrvSetAttr(slot->voice[0], attr);
    result1 = sdpr2DrvSetAttr(slot->voice[1], attr);
    if (result0 == SDPR2_ERR && result1 == SDPR2_ERR)
        return -1;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// levels[SDPR2_LEVEL_MAX]: the main voice's speakers, then the back
// voice's (at the turn angle + 180; a speaker both write keeps the back
// voice's level).
// FS2 0x0016df88

static int sdpr2PairGetLevels(int handle, int *levels)
{
    Sdpr2Slot *slot;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE && slot->voice[1] == SDPR2_VOICE_FREE)
        return -1;
    levels[SDPR2_LEVEL_CENTER] = 0;
    levels[SDPR2_LEVEL_LEFT] = 0;
    levels[SDPR2_LEVEL_RIGHT] = 0;
    levels[SDPR2_LEVEL_REAR_LEFT] = 0;
    levels[SDPR2_LEVEL_REAR_RIGHT] = 0;
    sdpr2CalcLevels(slot->volume, slot->levelAngle, slot->spread,
                    SDPR2_LEVELS_MAIN, levels);
    sdpr2CalcLevels(slot->volume, slot->levelAngle, slot->spread,
                    SDPR2_LEVELS_BACK, levels);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The first voice's status unless it is 0, else the second's; -1 when
// both fail, 0 for a free slot. The handle check prints nothing.
// FS2 0x0016e120

static int sdpr2PairGetStatus(int handle)
{
    Sdpr2Slot *slot;
    int status0, status1;

    if (handle < 0 || handle >= SDPR2_SLOT_MAX)
        return -1;
    slot = sdpr2GetSlot(handle);
    if (slot == NULL)
        return -1;
    if (slot->voice[0] == SDPR2_VOICE_FREE && slot->voice[1] == SDPR2_VOICE_FREE)
        return 0;
    status0 = sdpr2DrvGetStatus(slot->voice[0]);
    status1 = sdpr2DrvGetStatus(slot->voice[1]);
    if (status0 == SDPR2_ERR && status1 == SDPR2_ERR)
        return -1;
    if (status0 != 0)
        return status0;
    return status1;
}

// ──────────────────────────────────────────────────────────────────────
// Counts down the second voice's detune (back to the slot's pitch on the
// last step) and frees the slots whose voices ended (stopping them first
// when the config says so).
// FS2 0x0016e1f8

static int sdpr2PairUpdate(void)
{
    Sdpr2Slot *slot;
    int i;

    i = 0;
    do
    {
        slot = sdpr2GetSlot(i);
        if (slot == NULL)
            return -1;
        if (slot->type == &s_type
            && (slot->voice[0] != SDPR2_VOICE_FREE
                || slot->voice[1] != SDPR2_VOICE_FREE))
        {
            if (slot->state == SDPR2_PAIR_STATE_DETUNE_LONG)
                slot->state = SDPR2_PAIR_STATE_DETUNE_SHORT;
            else if (slot->state == SDPR2_PAIR_STATE_DETUNE_SHORT)
            {
                sdpr2DrvSetPitch(slot->voice[1], slot->pitch);
                slot->state = SDPR2_PAIR_STATE_TUNED;
            }
            if (sdpr2PairGetStatus(i) == 0)
            {
                if (s_config[SDPR2_CONFIG_STOP_ENDED] == 1)
                    SDPR2_TRY(sdpr2PairStop(i, SDPR2_STOP_ENDED_MODE));
                slot->state = SDPR2_PAIR_STATE_TUNED;
                slot->voice[0] = SDPR2_VOICE_FREE;
                slot->voice[1] = SDPR2_VOICE_FREE;
            }
        }
        i++;
    } while (i < SDPR2_SLOT_MAX);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The table (no extensions), the driver's config, then the registry.
// FS2 0x0016e328

int sdpr2PairInit(void)
{
    s_type.play = sdpr2PairPlay;
    s_type.playPanEx = sdpr2PairPlayPanEx;
    s_type.playPan = sdpr2PairPlayPan;
    s_type.playEx = sdpr2PairPlayEx;
    s_type.stop = sdpr2PairStop;
    s_type.stopAll = sdpr2PairStopAll;
    s_type.pause = sdpr2PairPause;
    s_type.setPitch = sdpr2PairSetPitch;
    s_type.setPitchF = sdpr2PairSetPitchF;
    s_type.setAngle = sdpr2PairSetAngle;
    s_type.setVolume = sdpr2PairSetVolume;
    s_type.setSpread = sdpr2PairSetSpread;
    s_type.setAttr = sdpr2PairSetAttr;
    s_type.getLevels = sdpr2PairGetLevels;
    s_type.getStatus = sdpr2PairGetStatus;
    s_type.update = sdpr2PairUpdate;
    s_type.ext40 = NULL;
    s_type.ext44 = NULL;
    s_type.ext48 = NULL;
    SDPR2_TRY(sdpr2DrvGetConfig(s_config));
    return sdpr2RegisterType(SDPR2_TYPE_PAIR, &s_type);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016e480

int sdpr2PairExit(void)
{
    return sdpr2UnregisterType(SDPR2_TYPE_PAIR);
}
