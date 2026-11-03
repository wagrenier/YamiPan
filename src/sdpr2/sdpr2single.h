/* ==========================================================================
 *  sdpr2/sdpr2single.h
 *
 *  SDPR2's one-voice type and the pan maths (sdpr2/sdpr2single.c
 *  0x16e498..0x170088, not written yet): only what written TUs call.
 *  architecture.md "SDPR2".
 * ======================================================================== */
#ifndef _SDPR2_SDPR2SINGLE_H
#define _SDPR2_SDPR2SINGLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Registers SDPR2_TYPE_SINGLE; 0, or -1. */
int  sdpr2SingleInit(void);                                             /* FS2 0x0016f808 */

/* The left / right gains (0..0x80) of angle deg; state carries the
 * previous answer between calls (0 to start). The new state. */
int  sdpr2CalcPanDeg(int deg, int state, int *volL, int *volR);         /* FS2 0x0016f9e0 */

#ifdef __cplusplus
}
#endif

#endif /* _SDPR2_SDPR2SINGLE_H */
