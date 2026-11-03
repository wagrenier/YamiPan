/* ==========================================================================
 *  sdpr2/sdpr2.h
 *
 *  SDPR2 (sdpr2/sdpr2*.c, not written yet; FS2-only middleware): panned
 *  sound effects over rxSndSe, 64 handles of a two-voice "pair" type (0) or
 *  a one-voice type (1). Only what written TUs call; argument names read
 *  from the game's callers (sound/sound.cpp). architecture.md "SDPR2".
 * ======================================================================== */
#ifndef _SDPR2_SDPR2_H
#define _SDPR2_SDPR2_H

#ifdef __cplusplus
extern "C" {
#endif

/* sdpr2PlayPan's types. */
#define SDPR2_TYPE_PAIR     0
#define SDPR2_TYPE_SINGLE   1

/* sdpr2GetStatus: 1 / 2 while it sounds. */
#define SDPR2_STATUS_PLAYING    1
#define SDPR2_STATUS_PAUSED     2

/* config: 8 words handed to the driver and both types. */
int  sdpr2Init(const void *config);                                     /* FS2 0x001683a8 */
void sdpr2SetSurround(int on);                                          /* FS2 0x001681c0 */
void sdpr2SetPairMode(int mode);                                        /* FS2 0x00168260 */
void sdpr2Update(void);                                                 /* FS2 0x0016b698 */
void sdpr2StopAll(int arg);                                             /* FS2 0x0016a1c0 */

/* A handle playing prog / note of the XVAB at angle deg (0..359) and
 * spread (0..0x80); < 0 failed. */
int  sdpr2PlayPan(int type, int xvab, int prog, int note, int volume,
                  int deg, int spread);                                 /* FS2 0x00168708 */
void sdpr2Stop(int handle, int arg);                                    /* FS2 0x00169d20 */
int  sdpr2SetPitchF(int handle, float pitch);                           /* FS2 0x0016a770 */
void sdpr2SetAngle(int handle, int deg);                                /* FS2 0x0016aa08 */
void sdpr2SetVolume(int handle, int volume);                            /* FS2 0x0016acd0 */
void sdpr2SetSpread(int handle, int spread);                            /* FS2 0x0016af38 */
int  sdpr2SetAttr(int handle, int attr);                                /* FS2 0x0016b1a0 */
int  sdpr2GetStatus(int handle);                                        /* FS2 0x0016b5f0: SDPR2_STATUS_* */

/* The left / right gains (0..0x80) of angle deg; state carries the
 * previous answer between calls (0 to start). The new state. */
int  sdpr2CalcPanDeg(int deg, int state, int *volL, int *volR);         /* FS2 0x0016f9e0 */

#ifdef __cplusplus
}
#endif

#endif /* _SDPR2_SDPR2_H */
