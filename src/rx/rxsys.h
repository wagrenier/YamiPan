/* ==========================================================================
 *  rx/rxsys.h
 *
 *  rx's system layer (rx/rxsys.c, not written yet: RosePan's
 *  src/rx/rxsys.h is the same library). Only what written TUs call so far.
 * ======================================================================== */
#ifndef _RX_RXSYS_H
#define _RX_RXSYS_H

#ifdef __cplusplus
extern "C" {
#endif

/* The printf every rx and rena module reports through. */
int rxSysPrintf(const char *fmt, ...);                                  /* FS2 0x00102328 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSYS_H */
