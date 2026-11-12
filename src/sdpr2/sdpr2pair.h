/* ==========================================================================
 *  sdpr2/sdpr2pair.h
 *
 *  SDPR2's two-voice type (registry SDPR2_TYPE_PAIR; sdpr2/sdpr2pair.c
 *  0x16c870..0x16e498, data 0x2f5fa0..0x2f5fb8, bss 0x537c50..0x537cc0).
 *  architecture.md "SDPR2".
 *
 *  A pair handle plays the sound twice: a main voice at the angle and a
 *  back voice at angle + 180, each scaled by its spread table (spread 0:
 *  both at cos 45, no direction; 0x80: all on the main voice). Instead of
 *  turning more than 90 degrees, the pair turns the other way and swaps
 *  the voices' roles (Sdpr2Slot.flip). The second voice starts detuned by
 *  the pair mode's factor for one or two sdpr2PairUpdate calls
 *  (Sdpr2Slot.state), then plays at the slot's pitch.
 * ======================================================================== */
#ifndef _SDPR2_SDPR2PAIR_H
#define _SDPR2_SDPR2PAIR_H

#include "sdpr2/sdpr2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDPR2_PAIR_MODES        (SDPR2_PAIR_MODE_MAX + 1)

/* sdpr2PairCalcPan's voice: angle (+ 180 for the back ones) and spread
 * table (main / back). flip 0 plays MAIN / BACK on voice[0] / voice[1],
 * flip 1 FRONT_BACK / BACK_MAIN. */
enum Sdpr2PairPan
{
    SDPR2_PAIR_PAN_MAIN = 0,        /* angle,       main table */
    SDPR2_PAIR_PAN_BACK,            /* angle + 180, back table */
    SDPR2_PAIR_PAN_FRONT_BACK,      /* angle,       back table */
    SDPR2_PAIR_PAN_BACK_MAIN        /* angle + 180, main table */
};

/* data 0x2f5fa0: the second voice's pitch by pair mode (4096 = 1.0):
 * 3520, 3616, 3520, 3616, 3808, 3856. */
extern const int sdpr2PairDetuneData[SDPR2_PAIR_MODES];                 /* FS2 0x002f5fa0 */

/* Registers SDPR2_TYPE_PAIR; 0, or -1. */
int  sdpr2PairInit(void);                                               /* FS2 0x0016e328 */
int  sdpr2PairExit(void);                                               /* FS2 0x0016e480 */

#ifdef __cplusplus
}
#endif

#endif /* _SDPR2_SDPR2PAIR_H */
