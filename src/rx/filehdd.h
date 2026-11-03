/* ==========================================================================
 *  rx/filehdd.h
 *
 *  rx's HDD device driver (0x157db8..0x158400: the host0:
 *  driver's shape plus an I/O error hook).
 *  Not written yet (its own TU, rx/filehdd.c): the prototypes rx/rxfile.c
 *  calls, from RosePan's src/rx/rxfile.h. See rxfile.h.
 * ======================================================================== */
#ifndef _RX_FILEHDD_H
#define _RX_FILEHDD_H

#include "rx/rxfile.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HDD driver (same shape as host0:, plus the I/O error handler) */
void   rxFileHddInit(RxFileDriver *drv);                            /* FS2 0x00157db8 */
void   rxFileHddTerm(void);                                         /* FS2 0x00157e40 */
void   rxFileHddOnError(const char *name, int error);               /* FS2 0x00157e48 */
void   rxFileHddMakePath(char *out, const char *name);              /* FS2 0x00157ef8 */
int    rxFileHddControl(int cmd, int arg);                          /* FS2 0x00157f10 */
int    rxFileHddGetStatus(int what);                                /* FS2 0x00157f18 */
int    rxFileHddGetSize(const char *name);                          /* FS2 0x00157f40 */
int    rxFileHddCreate(const char *name, int truncate);             /* FS2 0x00158010 */
int    rxFileHddRead(const char *name, void *buf, int offset, int size); /* FS2 0x001580e0 */
int    rxFileHddWrite(const char *name, int offset, const void *buf, int size); /* FS2 0x00158220 */
void   rxFileHddJoinRoot(char *out, const char *name);              /* FS2 0x00158360 */
char  *rxFileHddGetRoot(void);                                      /* FS2 0x001583a8 */
void   rxFileHddSetRoot(const char *root);                          /* FS2 0x001583b8 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_FILEHDD_H */
