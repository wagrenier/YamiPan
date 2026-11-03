/* ==========================================================================
 *  rx/rxsndstradpcm.h
 *
 *  FS2's rx ADPCM stream format (rx/rxsndstradpcm.c, not written yet; not in
 *  RoR): the RxSndStrFormat table rxSndUtInit registers as "ADPCM stream"
 *  (RxSndStrFormat_Adpcm). Only what written TUs call.
 * ======================================================================== */
#ifndef _RX_RXSNDSTRADPCM_H
#define _RX_RXSNDSTRADPCM_H

#include "rx/rxsndstr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The format's table entries (RxSndStrFormat order). */
void rxSndStrAdpcmInit(void);                                         /* FS2 0x00164c38 */
void rxSndStrAdpcmExit(void);                                         /* FS2 0x00164c80 */
void rxSndStrAdpcmUpdate(void);                                       /* FS2 0x00164d18 */
int  rxSndStrAdpcmOpen(RxSndStrSlot *slot);                           /* FS2 0x001650f8 */
void rxSndStrAdpcmClose(int handle, int async);                       /* FS2 0x001655f8 */
int  rxSndStrAdpcmGetHandle(int handle);                              /* FS2 0x00165680 */
void rxSndStrAdpcmPauseRead(int handle, int pause);                   /* FS2 0x00165698 */
int  rxSndStrAdpcmSyncRead(int handle, int arg);                      /* FS2 0x001656c0 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDSTRADPCM_H */
