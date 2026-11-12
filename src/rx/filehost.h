/* ==========================================================================
 *  rx/filehost.h
 *
 *  rx's host0: device driver (0x156060..0x1565c8, the string "filehost.c"):
 *  a root prefix joined to every name, plain rxSysOpen / rxSysRead. Written
 *  in rx/filehost.c, from RosePan's src/rx/rxfile.cpp. See rxfile.h.
 * ======================================================================== */
#ifndef _RX_FILEHOST_H
#define _RX_FILEHOST_H

#include "rx/rxfile.h"

#ifdef __cplusplus
extern "C" {
#endif

/* host0: driver */
void   rxFileHostInit(RxFileDriver *drv);                           /* FS2 0x00156060 */
void   rxFileHostTerm(void);                                        /* FS2 0x001560e8 */
void   rxFileHostMakePath(char *out, const char *name);             /* FS2 0x001560f0 */
int    rxFileHostControl(int cmd, int arg);                         /* FS2 0x00156108 */
int    rxFileHostGetStatus(int what);                               /* FS2 0x00156110 */
int    rxFileHostGetSize(const char *name);                         /* FS2 0x00156138 */
int    rxFileHostCreate(const char *name, int truncate);            /* FS2 0x00156208 */
int    rxFileHostRead(const char *name, void *buf, int offset, int size); /* FS2 0x001562c8 */
int    rxFileHostWrite(const char *name, int offset, const void *buf, int size); /* FS2 0x001563f8 */
void   rxFileHostJoinRoot(char *out, const char *name);             /* FS2 0x00156528 */
char  *rxFileHostGetRoot(void);                                     /* FS2 0x00156570 */
void   rxFileHostSetRoot(const char *root);                         /* FS2 0x00156580 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_FILEHOST_H */
