/* ==========================================================================
 *  rx/rxthread.h
 *
 *  rx's thread layer (rx/rxthread.c, not written yet: RosePan's
 *  src/rx/rxthread.h is the same library). Only what written TUs call so far.
 * ======================================================================== */
#ifndef _RX_RXTHREAD_H
#define _RX_RXTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Called with 1 when a BG thread yields, 0 when it comes back. */
typedef void (*RxThCallback)(int yielding);

int  rxThIsDispatchDisabled(void);                                      /* FS2 0x00166d10 */
void rxThAddCallback(RxThCallback func);                                /* FS2 0x00167220 */
void rxThRemoveCallback(RxThCallback func);                             /* FS2 0x001672a8 */
int  rxThIsYielding(void);                                              /* FS2 0x001672d8 */
int  rxThIsRequestLocked(void);                                         /* FS2 0x00167438 */
void rxThDisableDispatch(void);                                         /* FS2 0x00167890 */
void rxThEnableDispatch(void);                                          /* FS2 0x00167920 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXTHREAD_H */
