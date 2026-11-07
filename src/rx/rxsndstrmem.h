/* ==========================================================================
 *  rx/rxsndstrmem.h
 *
 *  FS2's rx memory stream sources (rx/rxsndstrmem.c, not written yet; not
 *  in RoR): the type-1 sources of rxsndstrstream, reading from EE or IOP
 *  memory: rxFile's ring calls, on a memory stream.
 * ======================================================================== */
#ifndef _RX_RXSNDSTRMEM_H
#define _RX_RXSNDSTRMEM_H

#include "rx/rxfile.h"
#include "sdk/scetypes.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void rxSndStrMemInit(void);                                             /* FS2 0x001660e0 */
void rxSndStrMemExit(void);                                             /* FS2 0x00166178 */
void rxSndStrMemUpdate(void);                                           /* FS2 0x001661b8 */
/* On size bytes at addr: an EE pointer, or an IOP address when iop. Its id, -1 failed. */
int  rxSndStrMemOpen(uintptr_t addr, int size, int iop);                /* FS2 0x001661c0 */
void rxSndStrMemClose(int id);                                          /* FS2 0x001662b0 */
void rxSndStrMemSeek(int id, u_int pos);                                /* FS2 0x00166320 */
int  rxSndStrMemGetReadable(int id);                                    /* FS2 0x00166370 */
void rxSndStrMemConsume(int id, int size);                              /* FS2 0x00166418 */
const u_char *rxSndStrMemGetData(int id);                               /* FS2 0x001665b8 */
int  rxSndStrMemIsEnd(int id);                                          /* FS2 0x00166650 */
void rxSndStrMemSetMarkers(int id, const RxFileMarkerDef *defs, int count, void *arg); /* FS2 0x00166688 */
void rxSndStrMemSetTags(int id, const RxFileTagDef *defs, int count);   /* FS2 0x00166890 */
int  rxSndStrMemGetTags(int id, RxFileTagHit *out, int max, int size);  /* FS2 0x00166a78 */
int  rxSndStrMemGetSize(int id);                                        /* FS2 0x00166b18 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDSTRMEM_H */
