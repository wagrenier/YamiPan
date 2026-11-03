/* ==========================================================================
 *  rx/rxsndstrstream.h
 *
 *  FS2's rx stream sources (rx/rxsndstrstream.c, not written yet; not in
 *  RoR): 64 sources at bss 0x5226e0, an rxFile ring (type 0) or a memory
 *  stream (type 1, rxsndstrmem). Only what written TUs call.
 * ======================================================================== */
#ifndef _RX_RXSNDSTRSTREAM_H
#define _RX_RXSNDSTRSTREAM_H

#include "sdk/scetypes.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A ring on the file name read through buf. The source id, -1 failed. */
int rxSndStrStreamOpenFile(const char *name, void *buf, int bufSize);   /* FS2 0x00165790 */
/* A memory stream on size bytes at addr: an EE pointer, or an IOP address
 * when iop. The source id, -1 failed. */
int rxSndStrStreamOpenMemory(uintptr_t addr, int size, int iop);        /* FS2 0x001657c0 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDSTRSTREAM_H */
