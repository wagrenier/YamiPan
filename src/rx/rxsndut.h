/* ==========================================================================
 *  rx/rxsndut.h
 *
 *  FS2's rx sound utility layer (rx/rxsndut.c, not written yet; not in RoR):
 *  the init / update of the rxsndstr layers. Only what written TUs call.
 * ======================================================================== */
#ifndef _RX_RXSNDUT_H
#define _RX_RXSNDUT_H

#ifdef __cplusplus
extern "C" {
#endif

void rxSndUtInit(void);                                                 /* FS2 0x00162178 */
void rxSndUtUpdate(void);                                               /* FS2 0x00162220 */
/* Bit 1 error prints, 2 BG-thread warning, 4 registration print. */
void rxSndUtSetDebugFlags(int flags);                                   /* FS2 0x00162250 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDUT_H */
