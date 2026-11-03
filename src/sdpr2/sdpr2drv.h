/* ==========================================================================
 *  sdpr2/sdpr2drv.h
 *
 *  SDPR2's driver layer (sdpr2/sdpr2drv.c 0x170088..0x171300, not written
 *  yet; the only driver is the SE driver over rxSndSe): only what written
 *  TUs call. architecture.md "SDPR2".
 * ======================================================================== */
#ifndef _SDPR2_SDPR2DRV_H
#define _SDPR2_SDPR2DRV_H

#ifdef __cplusplus
extern "C" {
#endif

/* config: sdpr2Init's 8 words. 0, or -1. */
int  sdpr2DrvInit(const void *config);                                  /* FS2 0x00170088 */

#ifdef __cplusplus
}
#endif

#endif /* _SDPR2_SDPR2DRV_H */
