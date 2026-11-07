/* ==========================================================================
 *  rx/rxsndstrstream.h
 *
 *  FS2's rx stream sources (not in RoR): 64 sources at bss 0x5226e0, an
 *  rxFile ring (type 0) or a memory stream (type 1, rxsndstrmem).
 * ======================================================================== */
#ifndef _RX_RXSNDSTRSTREAM_H
#define _RX_RXSNDSTRSTREAM_H

#include "rx/rxfile.h"
#include "sdk/scetypes.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void rxSndStrStreamInit(void);                                          /* FS2 0x001656e8 */
void rxSndStrStreamExit(void);                                          /* FS2 0x00165738 */
void rxSndStrStreamUpdate(void);                                        /* FS2 0x00165788 */
/* A ring on the file name read through buf. The source id, -1 failed. */
int rxSndStrStreamOpenFile(const char *name, void *buf, int bufSize);   /* FS2 0x00165790 */
/* A memory stream on size bytes at addr: an EE pointer, or an IOP address
 * when iop. The source id, -1 failed. */
int rxSndStrStreamOpenMemory(uintptr_t addr, int size, int iop);        /* FS2 0x001657c0 */
/* Both of the above: type 0 reads through the ring at addr, type 1 the memory at addr. */
int rxSndStrStreamOpen(int type, const char *name, uintptr_t addr, int size, int iop); /* FS2 0x001657f0 */
/* abort: don't wait for a read in flight. */
void rxSndStrStreamClose(int source, int abort);                        /* FS2 0x00165938 */
void rxSndStrStreamSeek(int source, u_int pos);                         /* FS2 0x00165a38 */
/* A source's marks and branches are rxFile's tags and markers, on both types. */
void rxSndStrStreamSetMarkers(int source, const RxFileMarkerDef *defs, int count, void *arg); /* FS2 0x00165ac8 */
void rxSndStrStreamSetTags(int source, const RxFileTagDef *defs, int count); /* FS2 0x00165b38 */
/* The tags in the next size bytes into out (max of them); their count. Null
 * out and max 0 only count. */
int  rxSndStrStreamGetRingTags(int source, RxFileTagHit *out, int max, int size); /* FS2 0x00165ba8 */
/* Of size bytes, how many can be read in one piece. */
int  rxSndStrStreamGetReadable(int source, int size);                   /* FS2 0x00165c20 */
int  rxSndStrStreamGetFilled(int source);                               /* FS2 0x00165df8 */
void rxSndStrStreamConsume(int source, int size);                       /* FS2 0x00165e70 */
const u_char *rxSndStrStreamGetData(int source);                        /* FS2 0x00165ee0 */
int  rxSndStrStreamIsEnd(int source);                                   /* FS2 0x00165f58 */
void rxSndStrStreamPause(int source, int pause);                        /* FS2 0x00166008 */
/* Non-zero while a read is in flight; wait: until none is. */
int  rxSndStrStreamSync(int source, int wait);                          /* FS2 0x00166068 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDSTRSTREAM_H */
