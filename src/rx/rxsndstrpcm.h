/* ==========================================================================
 *  rx/rxsndstrpcm.h
 *
 *  FS2's rx PCM stream format (rx/rxsndstrpcm.c; not in RoR): the
 *  RxSndStrFormat table rxSndUtInit registers as "PCM stream"
 *  (RxSndStrFormat_Pcm), which feeds rxSndPcm* from a stream source
 *  (rxsndstrstream.c). The twin of rxsndstradpcm.h without tracks, pitch
 *  or attribute.
 * ======================================================================== */
#ifndef _RX_RXSNDSTRPCM_H
#define _RX_RXSNDSTRPCM_H

#include "rx/rxsndstr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RXSNDSTR_PCM_STREAM_MAX     64      /* format streams (bss 0x5217d0) */

/* A stream of this format (bss 0x5217d0, 64 x 0x1c on the EE; free / active
 * lists through next, heads bss 0x521ed0 / 0x521ed4). layout: host -- holds
 * pointers. */
typedef struct RxSndStrPcmStream RxSndStrPcmStream;
struct RxSndStrPcmStream
{
    RxSndStrPcmStream *next;            // FS2 +0x00
    int                id;              // FS2 +0x04: the index, the format's handle
    RxSndStrSlot      *slot;            // FS2 +0x08
    int                source;          // FS2 +0x0c: the stream source, < 0 none
    int                pcm;             // FS2 +0x10: the rxSndPcm stream, < 0 none
    short              volL;            // FS2 +0x14: the request's
    short              volR;            // FS2 +0x16
    u_char             attr;            // FS2 +0x18: 1 if the request sets RxSndStrRequest::attr; unread by this format
};

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
