/* ==========================================================================
 *  rx/rxsndstradpcm.h
 *
 *  FS2's rx ADPCM stream format (rx/rxsndstradpcm.c; not in RoR): the
 *  RxSndStrFormat table rxSndUtInit registers as "ADPCM stream"
 *  (RxSndStrFormat_Adpcm), which feeds rxSndAdpcm* from a stream source
 *  (rxsndstrstream.c).
 * ======================================================================== */
#ifndef _RX_RXSNDSTRADPCM_H
#define _RX_RXSNDSTRADPCM_H

#include "rx/rxsndstr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RXSNDSTR_ADPCM_STREAM_MAX   64      /* format streams (bss 0x521ed8) */

/* A stream of this format (bss 0x521ed8, 64 x 0x20 on the EE; free / active
 * lists through next). layout: host -- holds pointers. */
typedef struct RxSndStrAdpcmStream RxSndStrAdpcmStream;
struct RxSndStrAdpcmStream
{
    RxSndStrAdpcmStream *next;          // FS2 +0x00
    int                  id;            // FS2 +0x04: the index, the format's handle
    RxSndStrSlot        *slot;          // FS2 +0x08
    int                  source;        // FS2 +0x0c: the stream source, < 0 none
    int                  adpcm;         // FS2 +0x10: the rxSndAdpcm stream, < 0 none
    short                volL;          // FS2 +0x14: the request's
    short                volR;          // FS2 +0x16
    u_char               attr;          // FS2 +0x18: 1 if the request sets RxSndStrRequest::attr
    int                  blockSize;     // FS2 +0x1c: 16 bytes per channel, the unit of a write
};

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
