/* ==========================================================================
 *  rx/rxsndstrmem.h
 *
 *  FS2's rx memory stream sources (rx/rxsndstrmem.c, not written yet; not
 *  in RoR): the type-1 sources of rxsndstrstream, reading from EE or IOP
 *  memory. Only what written TUs call.
 * ======================================================================== */
#ifndef _RX_RXSNDSTRMEM_H
#define _RX_RXSNDSTRMEM_H

#include "sdk/scetypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void rxSndStrMemInit(void);                                             /* FS2 0x001660e0 */
void rxSndStrMemExit(void);                                             /* FS2 0x00166178 */
void rxSndStrMemUpdate(void);                                           /* FS2 0x001661b8 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDSTRMEM_H */
