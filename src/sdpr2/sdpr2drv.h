/* ==========================================================================
 *  sdpr2/sdpr2drv.h
 *
 *  SDPR2's driver layer (sdpr2/sdpr2drv.c 0x170088..0x171300, not written
 *  yet; the only driver is the SE driver over rxSndSe): only what written
 *  TUs call. architecture.md "SDPR2".
 * ======================================================================== */
#ifndef _SDPR2_SDPR2DRV_H
#define _SDPR2_SDPR2DRV_H

#ifdef __cplusplus
extern "C" {
#endif

/* config: sdpr2Init's 8 words. 0, or -1. */
int  sdpr2DrvInit(const void *config);                                  /* FS2 0x00170088 */

/* A voice: the SE driver's id, or -1. volL / volR 0..0x80 (the pan's
 * gains, sdpr2CalcPanVolumes), signL / signR the phase signs (+1 / -1). */
int  sdpr2DrvPlay(int xvab, int prog, int note, int volL, int volR,
                  int signL, int signR);                                /* FS2 0x00170120 */
/* preset: 0..SDPR2_PRESET_MAX, the SE parameter set. */
int  sdpr2DrvPlayEx(int xvab, int prog, int note, int volL, int volR,
                    int signL, int signR, int preset);                  /* FS2 0x00170280 */
int  sdpr2DrvStop(int voice, int mode);                                 /* FS2 0x001703f0 */
int  sdpr2DrvPause(int voice, int on);                                  /* FS2 0x00170508 */
int  sdpr2DrvSetPitch(int voice, int pitch);                            /* FS2 0x00170620 */
int  sdpr2DrvSetPitchF(int voice, float pitch);                         /* FS2 0x00170738 */
int  sdpr2DrvSetVolume(int voice, int volL, int volR, int signL,
                       int signR);                                      /* FS2 0x00170850 */
int  sdpr2DrvSetAttr(int voice, int attr);                              /* FS2 0x00170998 */
int  sdpr2DrvGetStatus(int voice);                                      /* FS2 0x00170bc8: SDPR2_STATUS_*, 0 ended */
/* The 8 config words sdpr2DrvInit kept, into out; -1 when out is null. */
int  sdpr2DrvGetConfig(int *out);                                       /* FS2 0x00170f90 */

#ifdef __cplusplus
}
#endif

#endif /* _SDPR2_SDPR2DRV_H */
