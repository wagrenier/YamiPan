/* ==========================================================================
 *  rx/rxsndut.h
 *
 *  FS2's rx sound utility layer (rx/rxsndut.c, not written yet; not in RoR):
 *  the init / update of the rxsndstr layers. Only what written TUs call.
 * ======================================================================== */
#ifndef _RX_RXSNDUT_H
#define _RX_RXSNDUT_H

#include "sdk/scetypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void rxSndUtInit(void);                                                 /* FS2 0x00162178 */
void rxSndUtUpdate(void);                                               /* FS2 0x00162220 */
/* rxSndUtSetDebugFlags bits. */
#define RXSNDUT_DEBUG_ERROR     0x1     /* error prints */
#define RXSNDUT_DEBUG_BGCALL    0x2     /* warn when called from the BG thread */
#define RXSNDUT_DEBUG_REGISTER  0x4     /* print each registered format */

void rxSndUtSetDebugFlags(int flags);                                   /* FS2 0x00162250 */
u_int rxSndUtGetDebugFlags(void);                                       /* FS2 0x00162268 */
/* Warns (RXSNDUT_DEBUG_BGCALL) when the BG thread calls name. */
void rxSndUtCheckBgCall(const char *name);                              /* FS2 0x00162278 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDUT_H */
