/* ==========================================================================
 *  sdpr2/sdpr2pair.h
 *
 *  SDPR2's two-voice type (sdpr2/sdpr2pair.c 0x16c870..0x16e498, not
 *  written yet): only what written TUs call. architecture.md "SDPR2".
 * ======================================================================== */
#ifndef _SDPR2_SDPR2PAIR_H
#define _SDPR2_SDPR2PAIR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Registers SDPR2_TYPE_PAIR; 0, or -1. */
int  sdpr2PairInit(void);                                               /* FS2 0x0016e328 */

#ifdef __cplusplus
}
#endif

#endif /* _SDPR2_SDPR2PAIR_H */
