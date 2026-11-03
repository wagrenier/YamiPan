/* ==========================================================================
 *  rx/rxthread.h
 *
 *  rx's thread layer (rx/rxthread.c, not written yet: RosePan's
 *  src/rx/rxthread.h is the same library). Only what written TUs call so far.
 * ======================================================================== */
#ifndef _RX_RXTHREAD_H
#define _RX_RXTHREAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The arguments are words on the EE; intptr_t so a host pointer fits. */
typedef int  (*RxThRequestFunc)(intptr_t *args);

/* Called with 1 when a BG thread yields, 0 when it comes back. */
typedef void (*RxThCallback)(int yielding);

int  rxThIsDispatchDisabled(void);                                      /* FS2 0x00166d10 */
void rxThAddCallback(RxThCallback func);                                /* FS2 0x00167220 */
void rxThRemoveCallback(RxThCallback func);                             /* FS2 0x001672a8 */
int  rxThIsYielding(void);                                              /* FS2 0x001672d8 */
int  rxThIsRequestLocked(void);                                         /* FS2 0x00167438 */
void rxThYield(void);                                                   /* FS2 0x001674d0 */
void rxThDisableDispatch(void);                                         /* FS2 0x00167890 */
void rxThEnableDispatch(void);                                          /* FS2 0x00167920 */
int  rxThGetIndex(void);                                                /* FS2 0x00167aa8 */
void rxThDelete(int index);                                             /* FS2 0x00167b08 */
void rxThSetPriority(int index, int priority);                          /* FS2 0x00167bf0 */
void *rxThGetParam(int index);                                          /* FS2 0x00167c30 */
void rxThSetState(int index, short state);                              /* FS2 0x00167c90 */
int  rxThCreate(void (*entry)(void), void *stack, int stackSize,
                int priority, void *param);                             /* FS2 0x00167cb0 */
void rxThRequest(RxThRequestFunc func, intptr_t a, intptr_t b, intptr_t c); /* FS2 0x00167e28 */
void rxThInitOnce(void);                                                /* FS2 0x00168078 */
void rxThExitOnce(void);                                                /* FS2 0x00168090 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXTHREAD_H */
