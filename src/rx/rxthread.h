/* ==========================================================================
 *  rx/rxthread.h
 *
 *  The rx middleware's thread layer (rx/rxthread.c 0x166b50..0x1681c0, and
 *  its VBlank tail 0x159928..0x159bc0, which FS2 links before rxsnd; RoR
 *  0x249b88..0x24b490): a scheduler on top of the EE kernel's priority
 *  threads.
 *
 *  - The main thread (the one that calls rxThInit) runs at the base
 *    priority + prio index 0. Up to 8 rx threads (rxThCreate; the file
 *    thread is one) start at the lowest level, base + prio index 2.
 *  - rxThYield on the main thread hands the CPU over: it leaves a request
 *    (a function the dispatch alarm polls; by default "the vblank count
 *    changed"), resumes the worker thread (which runs the call queue
 *    rxThPostCall fills) and waits on the yield semaphore.
 *  - The dispatch alarm (every 4..256 HSYNCs, rxThSetInterval) gives the rx
 *    threads turns round-robin: a thread runs on every (priority + 1)-th
 *    pass that lands on it, and the alarm wakes the main thread once its
 *    request answers non-zero. rxThDisableDispatch / rxThEnableDispatch
 *    nest (a semaphore used as a counter) and stop the rotation.
 *  - rxThSync reads the vblank counter (an INTC cause-2 handler) and timer
 *    0 counting HBLANKs: the frame pacing the game is built on.
 *
 *  An rx thread is an index into the slot table (bss 0x526a70).
 *
 *  RosePan's src/rx/rxthread.h carried over to C (FS2 links RoR's prebuilt
 *  rx: rxcmp.py finds every function identical, the layouts are RoR's).
 *  FS2's game code also calls the two dispatch calls through jump thunks of
 *  its own (0x151f70 -> rxThEnableDispatch, 0x151f80 -> rxThDisableDispatch,
 *  with the same Ghidra names): the port calls the rx functions.
 * ======================================================================== */
#ifndef _RX_RXTHREAD_H
#define _RX_RXTHREAD_H

#include <assert.h>     /* static_assert in C11 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RX_TH_MAX       8       /* rx thread slots      */
#define RX_TH_CALL_MAX  32      /* call queue entries   */
#define RX_TH_CB_MAX    8       /* yield callbacks      */

/* rxThSync's mode: below 0 the count, else a wait and the HSYNCs. */
#define RX_TH_SYNC_COUNT    (-1)    /* the vblank count, no wait     */
#define RX_TH_SYNC_NEXT     0       /* wait for the next vblank      */
#define RX_TH_SYNC_NOWAIT   1       /* only the HSYNCs since the last read */

typedef struct RxThSlot RxThSlot;
typedef struct RxThCall RxThCall;

/* The arguments are words on the EE; intptr_t so a host pointer fits. */
typedef int  (*RxThRequestFunc)(intptr_t *args);
typedef void (*RxThCallFunc)(intptr_t *args);

/* Called with 1 when a BG thread yields, 0 when it comes back. */
typedef void (*RxThCallback)(int yielding);

/* One rx thread (bss 0x526a70, 8 x EE 0x10).
 * layout: host -- param is a pointer (EE +0xc, 4 bytes). */
struct RxThSlot
{
    short id;           // FS2 +0x0: kernel thread id, -1 when free
    short state;        // FS2 +0x2: 0 = not dispatched (rxThSetState)
    int   count;        // FS2 +0x4: passes since its last turn
    int   priority;     // FS2 +0x8: passes between turns (rxThSetPriority)
    void *param;        // FS2 +0xc: rxThGetParam
};
static_assert(offsetof(RxThSlot, id) == 0x0, "RxThSlot.id");
static_assert(offsetof(RxThSlot, state) == 0x2, "RxThSlot.state");
static_assert(offsetof(RxThSlot, count) == 0x4, "RxThSlot.count");
static_assert(offsetof(RxThSlot, priority) == 0x8, "RxThSlot.priority");
static_assert(offsetof(RxThSlot, param) >= 0xc, "RxThSlot.param");

/* One entry of the worker's call queue (bss 0x526af0, 32 x EE 0x10).
 * layout: host -- func is a pointer (EE +0x0, 4 bytes), the args words. */
struct RxThCall
{
    RxThCallFunc func;      // FS2 +0x0
    intptr_t     args[3];   // FS2 +0x4
};
static_assert(offsetof(RxThCall, func) == 0x0, "RxThCall.func");
static_assert(offsetof(RxThCall, args) >= 0x4, "RxThCall.args");

int  rxThCreateSema(int maxCount, int initCount);                       /* FS2 0x00166b50 */
int  rxThStartThread(void (*entry)(void *), void *stack, int stackSize,
                     int priority);                                     /* FS2 0x00166b78 */
void rxThKillThread(int id);                                            /* FS2 0x00166bd0 */
void rxThWorkerMain(void *arg);                                         /* FS2 0x00166bf8 */
void rxThIdleMain(void *arg);                                           /* FS2 0x00166cb8 */
int  rxThIsDispatchDisabled(void);                                      /* FS2 0x00166d10 */
int  rxThGetCallCount(void);                                            /* FS2 0x00166da8 */
int  rxThPostCall(RxThCallFunc func, intptr_t a, intptr_t b, intptr_t c); /* FS2 0x00166dd0 */
void rxThInit(void);                                                    /* FS2 0x00166e68 */
void rxThExit(void);                                                    /* FS2 0x00167020 */
void rxThRunCallbacks(int yielding);                                    /* FS2 0x00167180 */
int  rxThFindCallback(RxThCallback func);                               /* FS2 0x001671e0 */
void rxThAddCallback(RxThCallback func);                                /* FS2 0x00167220 */
void rxThRemoveCallback(RxThCallback func);                             /* FS2 0x001672a8 */
int  rxThIsYielding(void);                                              /* FS2 0x001672d8 */
void rxThLockRequest(void);                                             /* FS2 0x001672e8 */
void rxThUnlockRequest(void);                                           /* FS2 0x00167390 */
int  rxThIsRequestLocked(void);                                         /* FS2 0x00167438 */
void rxThYield(void);                                                   /* FS2 0x001674d0 */
void rxThDispatchAlarm(int id, unsigned short time, void *arg);         /* FS2 0x00167640 */
void rxThDisableDispatch(void);                                         /* FS2 0x00167890 */
void rxThEnableDispatch(void);                                          /* FS2 0x00167920 */
int  rxThGetIndex(void);                                                /* FS2 0x00167aa8 */
void rxThDelete(int index);                                             /* FS2 0x00167b08 */
int  rxThGetCount(void);                                                /* FS2 0x00167be0 */
void rxThSetPriority(int index, int priority);                          /* FS2 0x00167bf0 */
int  rxThGetPriority(int index);                                        /* FS2 0x00167c10 */
void *rxThGetParam(int index);                                          /* FS2 0x00167c30 */
void rxThSetParam(int index, void *param);                              /* FS2 0x00167c50 */
int  rxThGetState(int index);                                           /* FS2 0x00167c70 */
void rxThSetState(int index, short state);                              /* FS2 0x00167c90 */
int  rxThCreate(void (*entry)(void), void *stack, int stackSize,
                int priority, void *param);                             /* FS2 0x00167cb0 */
void rxThRequest(RxThRequestFunc func, intptr_t a, intptr_t b, intptr_t c); /* FS2 0x00167e28 */
void rxThDummy0(void);                                                  /* FS2 0x00167f20 */
void rxThDummy1(void);                                                  /* FS2 0x00167f28 */
int  rxThVSyncRequest(intptr_t *args);                                  /* FS2 0x00167f30 */
int  rxThVSync(int mode);                                               /* FS2 0x00167f68 */
int  rxThGetInterval(void);                                             /* FS2 0x00167fb0 */
void rxThSetInterval(int hsyncs);                                       /* FS2 0x00167fc0 */
void rxThSetPrioIndex(int mainIndex, int workerIndex, int lowIndex);    /* FS2 0x00168010 */
void rxThInitOnce(void);                                                /* FS2 0x00168078 */
void rxThExitOnce(void);                                                /* FS2 0x00168090 */
int  rxThGsSyncRequest(intptr_t *args);                                 /* FS2 0x001680a8 */
int  rxThGsSyncPath(int mode);                                          /* FS2 0x001680d0 */
int  rxThDmaSyncRequest(intptr_t *args);                                /* FS2 0x00168120 */
int  rxThDmaSync(int chan, int mode);                                   /* FS2 0x00168150 */
int  rxThVBlankHandler(int cause);                                      /* FS2 0x00159928 */
void rxThVBlankInit(void);                                              /* FS2 0x001599a8 */
void rxThVBlankExit(void);                                              /* FS2 0x00159a30 */
int  rxThReadHSync(void);                                               /* FS2 0x00159a90 */
void rxThWaitVBlank(void);                                              /* FS2 0x00159ac8 */
int  rxThGetField(void);                                                /* FS2 0x00159b18 */
int  rxThSync(int mode);                                                /* FS2 0x00159b28 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXTHREAD_H */
