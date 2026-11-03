/* ==========================================================================
 *  rx/rxsndstrpcm.h
 *
 *  FS2's rx PCM stream format (rx/rxsndstrpcm.c, not written yet; not in
 *  RoR): the RxSndStrFormat table rxSndUtInit registers as "PCM stream"
 *  (RxSndStrFormat_Pcm). Only what written TUs call.
 * ======================================================================== */
#ifndef _RX_RXSNDSTRPCM_H
#define _RX_RXSNDSTRPCM_H

#include "rx/rxsndstr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The format's table entries (RxSndStrFormat order). */
void rxSndStrPcmInit(void);                                         /* FS2 0x00164398 */
void rxSndStrPcmExit(void);                                         /* FS2 0x001643e0 */
void rxSndStrPcmUpdate(void);                                       /* FS2 0x00164478 */
int  rxSndStrPcmOpen(RxSndStrSlot *slot);                           /* FS2 0x001646b8 */
void rxSndStrPcmClose(int handle, int async);                       /* FS2 0x00164b30 */
int  rxSndStrPcmGetHandle(int handle);                              /* FS2 0x00164bb8 */
void rxSndStrPcmPauseRead(int handle, int pause);                   /* FS2 0x00164bd8 */
int  rxSndStrPcmSyncRead(int handle, int arg);                      /* FS2 0x00164c08 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDSTRPCM_H */
