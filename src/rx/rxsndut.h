/* ==========================================================================
 *  rx/rxsndut.h
 *
 *  FS2's rx sound utility layer (rx/rxsndut.c, FS2 0x162178..0x1622f8; not
 *  in RoR): brings up, steps and takes down the rxsndstr layers (memory and
 *  file stream sources, banks, the stream core), registers the two stream
 *  formats and keeps the layer's debug flags.
 * ======================================================================== */
#ifndef _RX_RXSNDUT_H
#define _RX_RXSNDUT_H

#include "sdk/scetypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* rxSndUtSetDebugFlags bits (data 0x2f5f10, 7 = all three at boot). */
#define RXSNDUT_DEBUG_ERROR     0x1     /* error prints */
#define RXSNDUT_DEBUG_BGCALL    0x2     /* warn when called from the BG thread */
#define RXSNDUT_DEBUG_REGISTER  0x4     /* print each registered format */
#define RXSNDUT_DEBUG_DEFAULT   (RXSNDUT_DEBUG_ERROR | RXSNDUT_DEBUG_BGCALL | RXSNDUT_DEBUG_REGISTER)

/* Inits the stream layers bottom up and registers the PCM and ADPCM formats
 * (with rxsnd's SPU heap trace turned off). */
void  rxSndUtInit(void);                                                 /* FS2 0x00162178 */
/* Takes the layers down (streams, banks, the core, then memory sources). */
void  rxSndUtExit(void);                                                 /* FS2 0x001621f0 */
/* Once a frame: steps every layer, the stream core last. */
void  rxSndUtUpdate(void);                                               /* FS2 0x00162220 */
/* Stores the flags; the old ones back. */
u_int rxSndUtSetDebugFlags(u_int flags);                                 /* FS2 0x00162250 */
u_int rxSndUtGetDebugFlags(void);                                        /* FS2 0x00162268 */
/* Warns (RXSNDUT_DEBUG_BGCALL) when the BG thread calls name. */
void  rxSndUtCheckBgCall(const char *name);                              /* FS2 0x00162278 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDUT_H */
