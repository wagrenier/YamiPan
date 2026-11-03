/* ==========================================================================
 *  rx/rxthread.c
 *
 *  The rx middleware's thread layer: kernel wrappers, the scheduler
 *  (rxThInit, the dispatch alarm, rxThYield, rxThRequest), the rx thread
 *  slots, the worker's call queue, yield callbacks, and the vblank / HSYNC
 *  counters behind rxThSync. See rxthread.h.
 *
 *  FS2 links Rule of Rose's prebuilt rx: this file is RosePan's
 *  src/rx/rxthread.cpp (every function written there, checked against RoR)
 *  carried over to C. rxcmp.py '^rxTh' finds all 53 identical (jump
 *  targets, lui and gp offsets masked; rxThWorkerMain's one difference is
 *  an unmasked addiu of a data address). FS2 links it in two pieces: the
 *  VBlank tail (rxThVBlankHandler .. rxThSync, 0x159928..0x159bc0, with
 *  its own data 0x2f5b50.. and bss 0x4fdef0..) before rxsnd, the rest at
 *  0x166b50..0x1681c0 (data 0x2f5f58.., bss 0x526a40..).
 *
 *  The ROM masks interrupts by clearing COP0 Status.IE around its kernel
 *  i-calls and saves / restores Status around DIntr ... EIntr; the host's
 *  sections are a recursive mutex (src/sdk) and the i-calls are the plain
 *  ones, so neither is written here. Every spin-wait also prints "DI error"
 *  when it finds interrupts disabled (rxThSpin, empty on the host, as
 *  rxfile's).
 *
 *  PORT: EE threads are host threads, so the rx threads run alongside the
 *  main thread instead of only in its yields, and the priority changes do
 *  nothing. What keeps working is the handshake: the main thread's
 *  rxThYield blocks until the dispatch alarm (a host timer thread) sees
 *  its request answer, and rxThSync counts the vblanks libgraph's vblank
 *  thread raises. Spins that would hold a host core for ever sleep instead
 *  (marked PC:).
 * ======================================================================== */
#include "rx/rxthread.h"

#include "rx/rxsys.h"
#include "sdk/eekernel.h"
#include "sdk/eeregs.h"
#include "sdk/libdma.h"
#include "sdk/libgraph.h"
#include "yamipan/os/yamipan_time.h"

#define RX_TH_INTC_VBLANK_START 2       /* INTC cause: vblank start */

/* Timer 0 mode (T0_MODE): count HBLANKs. */
#define RX_TH_RCNT_CLKS_HBLANK  0x03    /* CLKS: HBLANK                */
#define RX_TH_RCNT_GATS         0x08    /* GATS: gate on VBLANK        */
#define RX_TH_RCNT_CUE          0x80    /* CUE: count up enable        */

/* sceGsGParam.sceGsInterMode */
#define RX_TH_GS_INTERLACE      1

/* ---- data ------------------------------------------------------------- */

static int s_initCount;                         /* data 0x2f5f58 */
static int s_basePriority;                      /* data 0x2f5f5c: the main thread's at rxThInit */
static volatile int s_callWrite;                /* data 0x2f5f60 */
static volatile int s_callRead;                 /* data 0x2f5f64 */
static RxThRequestFunc s_request;               /* data 0x2f5f68 */
static int s_interval = 4;                      /* data 0x2f5f6c: alarm period, HSYNCs */
static int s_prioIndex[3] = { 0, 2, 4 };        /* data 0x2f5f70: main, worker, low */
static int s_vblankHandler = -1;                /* data 0x2f5b50 */
static int s_field;                             /* data 0x2f5b54 */
static int s_vblankUsers;                       /* data 0x2f5b58 */
static volatile unsigned s_vblankCount;         /* data 0x2f5b5c */

static int s_yieldSema;                         /* bss 0x526a40: the main thread waits here */
static int s_dispatchSema;                      /* bss 0x526a44: count = dispatch disables  */
static int s_requestSema;                       /* bss 0x526a48: count = request locks      */
static int s_mainThread;                        /* bss 0x526a4c */
static int s_workerThread;                      /* bss 0x526a50 */
static int s_idleThread;                        /* bss 0x526a54 */
static int s_alarm = -1;                        /* bss 0x526a58 */
static int s_yieldTick;                         /* bss 0x526a5c */
static int s_yielding;                          /* bss 0x526a60 */
static int s_threadCount;                       /* bss 0x526a64 */
static int s_current;                           /* bss 0x526a68: slot of the last turn */
static RxThSlot s_slots[RX_TH_MAX];             /* bss 0x526a70 */
static RxThCall s_calls[RX_TH_CALL_MAX];        /* bss 0x526af0 */
static intptr_t s_requestArgs[3];               /* bss 0x526cf0 */
static RxThCallback s_callbacks[RX_TH_CB_MAX];  /* bss 0x526d00 */
static unsigned char s_workerStack[0x10000];    /* bss 0x526d40 */
static unsigned char s_idleStack[0x400];        /* bss 0x536d40 */
static unsigned s_lastVBlank;                   /* bss 0x4fdef0: vblank count at the last read */
static int s_hsyncCarry;                        /* bss 0x4fdef4 */

// The ROM's spin-wait step: warn ("DI error") when interrupts are off.
// PORT: no Status register on the host; the wait just spins.
static void rxThSpin(void)
{
}

/* ---- kernel wrappers -------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00166b50

int rxThCreateSema(int maxCount, int initCount)
{
    struct SemaParam param;
    param.maxCount = maxCount;
    param.initCount = initCount;
    return CreateSema(&param);
}

// ──────────────────────────────────────────────────────────────────────
// Create and start a thread with the caller's gp.
// FS2 0x00166b78

int rxThStartThread(void (*entry)(void *), void *stack, int stackSize, int priority)
{
    struct ThreadParam param;
    int id;

    param.entry = entry;
    param.stack = stack;
    param.stackSize = stackSize;
    param.gpReg = 0;            // PC: the ROM passes &_gp
    param.initPriority = priority;
    id = CreateThread(&param);
    StartThread(id, 0);
    return id;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00166bd0

void rxThKillThread(int id)
{
    TerminateThread(id);
    DeleteThread(id);
}

// ──────────────────────────────────────────────────────────────────────
// The worker thread: runs the call queue, one entry at a time, for ever.
// It is resumed only while the main thread yields.
// FS2 0x00166bf8

void rxThWorkerMain(void *arg)
{
    (void)arg;
    for (;;)
    {
        int i = s_callRead;
        while (s_callWrite == i)
        {
            rxThSpin();             // callcheck: EIntr, rxSysPrintf (the DI check)
            YamiPan_Sleep(1);       // PC: a host thread would hold a core here
        }
        s_calls[i].func(s_calls[i].args);
        s_callRead = (i + 1) & (RX_TH_CALL_MAX - 1);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The idle thread: the lowest priority, so the kernel always has a thread
// to run; it only does the DI check.
// FS2 0x00166cb8

void rxThIdleMain(void *arg)
{
    (void)arg;
    for (;;)
    {
        rxThSpin();                 // callcheck: EIntr, rxSysPrintf (the DI check)
        SleepThread();              // PC: nobody wakes it; the ROM spins
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00166d10

int rxThIsDispatchDisabled(void)
{
    struct SemaParam st;
    iReferSemaStatus(s_dispatchSema, &st);
    return st.currentCount > 0;
}

/* ---- the worker's call queue ------------------------------------------ */

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00166da8

int rxThGetCallCount(void)
{
    return (s_callWrite - s_callRead) & (RX_TH_CALL_MAX - 1);
}

// ──────────────────────────────────────────────────────────────────────
// Queue func(args) for the worker; 0 when the queue is full (31 entries).
// FS2 0x00166dd0

int rxThPostCall(RxThCallFunc func, intptr_t a, intptr_t b, intptr_t c)
{
    int count = rxThGetCallCount();
    if (count != RX_TH_CALL_MAX - 1)
    {
        // PC: the ROM bumps the write index before it fills the entry (the
        // worker cannot run in between on the EE); the host's worker can, so
        // the entry is filled first.
        int i = s_callWrite;
        s_calls[i].func = func;
        s_calls[i].args[0] = a;
        s_calls[i].args[1] = b;
        s_calls[i].args[2] = c;
        s_callWrite = (i + 1) & (RX_TH_CALL_MAX - 1);
    }
    return count != RX_TH_CALL_MAX - 1;
}

/* ---- init / exit ------------------------------------------------------ */

// ──────────────────────────────────────────────────────────────────────
// First call: the vblank counter, empty slots and callbacks, the three
// semaphores, the idle thread and the (suspended) worker thread, at the
// calling thread's priority + the prio indices.
// FS2 0x00166e68

void rxThInit(void)
{
    struct ThreadParam info;
    int i;

    s_initCount++;
    if (s_initCount != 1)
        return;

    rxThVBlankInit();
    s_yielding = 0;
    for (i = RX_TH_MAX - 1; i >= 0; i--)
        s_slots[i].id = -1;
    for (i = RX_TH_CB_MAX - 1; i >= 0; i--)
        s_callbacks[i] = 0;
    s_current = 0;
    s_threadCount = 0;

    s_mainThread = GetThreadId();
    ReferThreadStatus(s_mainThread, &info);
    s_basePriority = info.currentPriority;

    s_yieldSema = rxThCreateSema(1, 0);
    s_dispatchSema = rxThCreateSema(0x10000, 0);
    s_requestSema = rxThCreateSema(0x10000, 0);
    s_callWrite = 0;
    s_callRead = 0;
    s_idleThread = rxThStartThread(rxThIdleMain, s_idleStack, sizeof(s_idleStack),
                                   s_basePriority + s_prioIndex[2]);
    s_workerThread = rxThStartThread(rxThWorkerMain, s_workerStack, sizeof(s_workerStack),
                                     s_basePriority + s_prioIndex[1]);
    SuspendThread(s_workerThread);
    s_request = 0;
    s_alarm = -1;
}

// ──────────────────────────────────────────────────────────────────────
// Last call: undo rxThInit.
// FS2 0x00167020

void rxThExit(void)
{
    int i;

    if (s_initCount <= 0)
        return;
    s_initCount--;
    if (s_initCount != 0)
        return;

    DIntr();
    if (s_alarm >= 0)
    {
        ReleaseAlarm(s_alarm);
        s_alarm = -1;
    }
    s_threadCount = 0;
    for (i = 0; i < RX_TH_MAX; i++)
    {
        if (s_slots[i].id >= 0)
        {
            rxThKillThread(s_slots[i].id);
            s_slots[i].id = -1;
        }
    }
    EIntr();
    rxThKillThread(s_workerThread);
    rxThKillThread(s_idleThread);
    DeleteSema(s_requestSema);
    DeleteSema(s_dispatchSema);
    DeleteSema(s_yieldSema);
    rxThVBlankExit();
}

/* ---- yield callbacks -------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// yielding: 1 when the main thread enters rxThYield, 0 when it leaves.
// FS2 0x00167180

void rxThRunCallbacks(int yielding)
{
    int i;
    for (i = 0; i < RX_TH_CB_MAX; i++)
    {
        if (s_callbacks[i] != 0)
            s_callbacks[i](yielding);   // callcheck: rxSysThreadCallback
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001671e0

int rxThFindCallback(RxThCallback func)
{
    int i;
    for (i = 0; i < RX_TH_CB_MAX; i++)
    {
        // ROM bug: the match branch's delay slot loads -1 into the result,
        // so this never finds anything (rxThAddCallback does not refuse a
        // duplicate, rxThRemoveCallback removes nothing).
        if (s_callbacks[i] == func)
            return -1;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167220

void rxThAddCallback(RxThCallback func)
{
    int i;

    if (rxThFindCallback(func) >= 0)
        return;
    for (i = 0; i < RX_TH_CB_MAX; i++)
    {
        if (s_callbacks[i] == 0)
        {
            s_callbacks[i] = func;
            return;
        }
    }
    scePrintf("rxthread: rxThAddCallback failed! (%08X) \n", (unsigned)(uintptr_t)func);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001672a8

void rxThRemoveCallback(RxThCallback func)
{
    int i = rxThFindCallback(func);
    if (i >= 0)
        s_callbacks[i] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001672d8

int rxThIsYielding(void)
{
    return s_yielding;
}

/* ---- request lock (worker thread) ------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Hold the main thread in its yield: the alarm only checks the request
// while the request semaphore's count is 0.
// FS2 0x001672e8

void rxThLockRequest(void)
{
    if (GetThreadId() == s_workerThread)
        iSignalSema(s_requestSema);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167390

void rxThUnlockRequest(void)
{
    if (GetThreadId() == s_workerThread)
        iPollSema(s_requestSema);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167438

int rxThIsRequestLocked(void)
{
    struct SemaParam st;
    iReferSemaStatus(s_requestSema, &st);
    return st.currentCount > 0;
}

/* ---- scheduling ------------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Main thread: run the rx threads until the request answers (by default
// until the next vblank). An rx thread: drop back below the main thread.
// The worker: nothing.
// FS2 0x001674d0

void rxThYield(void)
{
    int self;

    rxThSpin();                     // callcheck: EIntr, rxSysPrintf (the DI check)
    self = GetThreadId();
    if (self != s_mainThread)
    {
        if (self == s_workerThread)
            return;
        ChangeThreadPriority(s_mainThread, s_basePriority + s_prioIndex[0]);
        ChangeThreadPriority(self, s_basePriority + s_prioIndex[2]);
        YamiPan_Sleep(1);           // PC: the priority change hands the CPU over on the EE
        return;
    }

    s_yieldTick = rxThSync(RX_TH_SYNC_COUNT);
    if (s_request == 0)
        rxThRequest(rxThVSyncRequest, s_yieldTick, 0, 0);
    rxThRunCallbacks(1);
    s_yielding = 1;
    ResumeThread(s_workerThread);
    WaitSema(s_yieldSema);
    SuspendThread(s_workerThread);
    s_yielding = 0;
    rxThRunCallbacks(0);
}

// ──────────────────────────────────────────────────────────────────────
// The dispatch alarm. Unless dispatch is disabled: drop the last slot's
// thread, find the next dispatched slot; every (priority + 1)-th pass on
// it the thread gets the CPU instead of the main thread. Then wake the
// main thread when its request answers (and it waits, and no lock holds
// it), and re-arm while there are rx threads or a request.
// FS2 0x00167640

void rxThDispatchAlarm(int id, unsigned short time, void *arg)
{
    struct SemaParam st;
    RxThSlot *slot;
    int cur, next, i;

    (void)id;
    (void)time;
    (void)arg;

    s_alarm = -1;
    iReferSemaStatus(s_dispatchSema, &st);
    if (st.currentCount > 0)
        return;                     // ExitHandler (EI)

    cur = s_current;
    if (s_slots[cur].id >= 0)
        iChangeThreadPriority(s_slots[cur].id, s_basePriority + s_prioIndex[2]);
    for (i = 0; i < RX_TH_MAX - 1; i++)
    {
        cur = (cur + 1) & (RX_TH_MAX - 1);
        if (s_slots[cur].id >= 0 && s_slots[cur].state != 0)
            break;
    }
    next = s_mainThread;
    s_current = cur;
    slot = &s_slots[cur];
    if (slot->id >= 0 && slot->state != 0)
    {
        slot->count++;
        if (slot->priority < slot->count)
        {
            slot->count = 0;
            next = slot->id;
            iChangeThreadPriority(s_mainThread, s_basePriority + s_prioIndex[2]);
        }
    }
    iChangeThreadPriority(next, s_basePriority + s_prioIndex[0]);
    iChangeThreadPriority(s_mainThread, s_basePriority + s_prioIndex[0]);

    if (s_request != 0)
    {
        iReferSemaStatus(s_requestSema, &st);
        if (st.currentCount == 0 && s_request(s_requestArgs) != 0)  // callcheck: rxThVSyncRequest
        {
            iReferSemaStatus(s_yieldSema, &st);
            if (st.numWaitThreads != 0)
            {
                iSignalSema(s_yieldSema);
                s_request = 0;
            }
        }
    }
    if (s_threadCount > 0 || s_request != 0)
        s_alarm = iSetAlarm((unsigned short)s_interval, rxThDispatchAlarm, 0);
}                                   // ExitHandler (EI)

// ──────────────────────────────────────────────────────────────────────
// Nesting: each call adds one to the dispatch semaphore.
// FS2 0x00167890

void rxThDisableDispatch(void)
{
    iSignalSema(s_dispatchSema);
}

// ──────────────────────────────────────────────────────────────────────
// Undo one rxThDisableDispatch; the last one re-arms the alarm.
// FS2 0x00167920

void rxThEnableDispatch(void)
{
    struct SemaParam st;
    DIntr();
    iPollSema(s_dispatchSema);
    iReferSemaStatus(s_dispatchSema, &st);
    if (st.currentCount < 1 && s_alarm < 0)
        s_alarm = SetAlarm((unsigned short)s_interval, rxThDispatchAlarm, 0);
    EIntr();
}

/* ---- rx thread slots -------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// The calling thread's slot, -1 when it is not an rx thread.
// FS2 0x00167aa8

int rxThGetIndex(void)
{
    short self = (short)GetThreadId();
    int i;
    for (i = 0; i < RX_TH_MAX; i++)
    {
        if (s_slots[i].id == self)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// Free a slot and end its thread (itself: hand the CPU to the main thread
// and exit).
// FS2 0x00167b08

void rxThDelete(int index)
{
    short id;

    if (index < 0 || s_slots[index].id < 0)
        return;

    rxThDisableDispatch();
    id = s_slots[index].id;
    s_slots[index].id = -1;
    s_threadCount--;
    if (id == (short)GetThreadId())
    {
        ChangeThreadPriority(s_mainThread, s_basePriority + s_prioIndex[0]);
        rxThEnableDispatch();
        ExitDeleteThread();
    }
    else
    {
        rxThKillThread(id);
    }
    rxThEnableDispatch();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167be0

int rxThGetCount(void)
{
    return s_threadCount;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167bf0

void rxThSetPriority(int index, int priority)
{
    if (index >= 0)
        s_slots[index].priority = priority;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167c10

int rxThGetPriority(int index)
{
    int priority = 0;
    if (index >= 0)
        priority = s_slots[index].priority;
    return priority;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167c30

void *rxThGetParam(int index)
{
    void *param = 0;
    if (index >= 0)
        param = s_slots[index].param;
    return param;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167c50

void rxThSetParam(int index, void *param)
{
    if (index >= 0)
        s_slots[index].param = param;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167c70

int rxThGetState(int index)
{
    int state = 0;
    if (index >= 0)
        state = s_slots[index].state;
    return state;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167c90

void rxThSetState(int index, short state)
{
    if (index >= 0)
        s_slots[index].state = state;
}

// ──────────────────────────────────────────────────────────────────────
// Start entry on a free slot at the lowest level, not dispatched yet
// (rxThSetState); -1 when all 8 are taken.
// FS2 0x00167cb0

int rxThCreate(void (*entry)(void), void *stack, int stackSize, int priority, void *param)
{
    RxThSlot *slot;
    int id, index;

    if (s_threadCount >= RX_TH_MAX)
        return -1;

    // PC: entry takes no argument; StartThread passes 0 in a0 either way.
    id = rxThStartThread((void (*)(void *))entry, stack, stackSize,
                         s_basePriority + s_prioIndex[2]);
    index = 0;
    while (index < RX_TH_MAX && s_slots[index].id >= 0)
        index++;
    s_threadCount++;
    slot = &s_slots[index];
    slot->count = 0;
    slot->priority = priority;
    slot->state = 0;
    slot->param = param;
    slot->id = (short)id;

    DIntr();
    if (s_alarm < 0)
        s_alarm = SetAlarm((unsigned short)s_interval, rxThDispatchAlarm, 0);
    EIntr();
    return index;
}

// ──────────────────────────────────────────────────────────────────────
// Main thread only: what its next rxThYield waits for (func(args) != 0).
// FS2 0x00167e28

void rxThRequest(RxThRequestFunc func, intptr_t a, intptr_t b, intptr_t c)
{
    if (GetThreadId() != s_mainThread)
        return;

    s_request = func;
    s_requestArgs[0] = a;
    s_requestArgs[1] = b;
    s_requestArgs[2] = c;
    DIntr();
    if (s_alarm < 0)
        s_alarm = SetAlarm((unsigned short)s_interval, rxThDispatchAlarm, 0);
    EIntr();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167f20

void rxThDummy0(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167f28

void rxThDummy1(void)
{
}

/* ---- waits through a yield -------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Request: the vblank count moved past args[0].
// FS2 0x00167f30

int rxThVSyncRequest(intptr_t *args)
{
    return (int)args[0] != rxThSync(RX_TH_SYNC_COUNT);
}

// ──────────────────────────────────────────────────────────────────────
// mode 0: let the rx threads run until the next vblank. Returns the
// HSYNCs since the last read.
// FS2 0x00167f68

int rxThVSync(int mode)
{
    if (mode == 0)
    {
        rxThRequest(rxThVSyncRequest, rxThSync(RX_TH_SYNC_COUNT), 0, 0);
        rxThYield();
    }
    return rxThSync(RX_TH_SYNC_NOWAIT);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167fb0

int rxThGetInterval(void)
{
    return s_interval;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00167fc0

void rxThSetInterval(int hsyncs)
{
    if (hsyncs < 4)
        hsyncs = 4;
    if (hsyncs > 0x100)
        hsyncs = 0x100;
    scePrintf("rxThread: HSync Interval:%d\n", hsyncs);
    s_interval = hsyncs;
}

// ──────────────────────────────────────────────────────────────────────
// The priority offsets of the main thread, the worker and the rx threads,
// kept strictly increasing (lower priority).
// FS2 0x00168010

void rxThSetPrioIndex(int mainIndex, int workerIndex, int lowIndex)
{
    if (!(mainIndex < workerIndex))
        workerIndex = mainIndex + 1;
    if (!(workerIndex < lowIndex))
        lowIndex = workerIndex + 1;
    s_prioIndex[0] = mainIndex;
    s_prioIndex[1] = workerIndex;
    s_prioIndex[2] = lowIndex;
    scePrintf("rxThread: PrioIndex %d %d %d\n", mainIndex, workerIndex, lowIndex);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00168078

void rxThInitOnce(void)
{
    rxThInit();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00168090

void rxThExitOnce(void)
{
    rxThExit();
}

// ──────────────────────────────────────────────────────────────────────
// Request: path 3 (GIF) is idle.
// FS2 0x001680a8

int rxThGsSyncRequest(intptr_t *args)
{
    (void)args;
    return sceGsSyncPath(1, 0) == 0;
}

// ──────────────────────────────────────────────────────────────────────
// mode 0: let the rx threads run until the GS paths are idle.
// FS2 0x001680d0

int rxThGsSyncPath(int mode)
{
    if (mode != 0)
        return sceGsSyncPath(mode, 0);
    rxThRequest(rxThGsSyncRequest, 0, 0, 0);
    rxThYield();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Request: DMA channel args[0] is idle.
// FS2 0x00168120

int rxThDmaSyncRequest(intptr_t *args)
{
    return sceDmaSync(sceDmaGetChan((int)args[0]), 1, 0) == 0;
}

// ──────────────────────────────────────────────────────────────────────
// mode 0: let the rx threads run until DMA channel chan is idle.
// FS2 0x00168150

int rxThDmaSync(int chan, int mode)
{
    if (mode != 0)
        return sceDmaSync(sceDmaGetChan(chan), mode, 0);
    rxThRequest(rxThDmaSyncRequest, chan, 0, 0);
    rxThYield();
    return 0;
}

/* ---- vblank and HSYNC counters (FS2 0x159928..0x159bc0) --------------- */

// ──────────────────────────────────────────────────────────────────────
// INTC cause 2: count the vblank and latch the field being shown.
// FS2 0x00159928

int rxThVBlankHandler(int cause)
{
    sceGsGParam *gp;

    (void)cause;
    s_vblankCount++;
    gp = sceGsGetGParam();
    if (gp->sceGsInterMode == RX_TH_GS_INTERLACE)
        s_field = (int)(REG_GS_CSR >> 13) & 1;      // CSR.FIELD
    else
        s_field = 1;
    return 0;                       // ExitHandler (EI)
}

// ──────────────────────────────────────────────────────────────────────
// First call: the vblank handler, and timer 0 counting HBLANKs.
// FS2 0x001599a8

void rxThVBlankInit(void)
{
    s_vblankUsers++;
    if (s_vblankUsers != 1)
        return;

    s_vblankCount = 0;
    s_lastVBlank = 0;
    s_hsyncCarry = 0;
    s_vblankHandler = AddIntcHandler(RX_TH_INTC_VBLANK_START, rxThVBlankHandler, -1);
    EnableIntc(RX_TH_INTC_VBLANK_START);
    REG_RCNT0_MODE = RX_TH_RCNT_CUE | RX_TH_RCNT_GATS | RX_TH_RCNT_CLKS_HBLANK;    // 0x8b
    REG_RCNT0_COUNT = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00159a30

void rxThVBlankExit(void)
{
    if (s_vblankUsers <= 0)
        return;
    s_vblankUsers--;
    if (s_vblankUsers != 0)
        return;

    DisableIntc(RX_TH_INTC_VBLANK_START);
    RemoveIntcHandler(RX_TH_INTC_VBLANK_START, s_vblankHandler);
    s_vblankHandler = -1;
}

// ──────────────────────────────────────────────────────────────────────
// HSYNCs since the last read; restarts the count.
// FS2 0x00159a90

int rxThReadHSync(void)
{
    int hsyncs = (int)REG_RCNT0_COUNT;
    s_lastVBlank = s_vblankCount;
    hsyncs += s_hsyncCarry;
    REG_RCNT0_COUNT = 0;
    s_hsyncCarry = 0;
    return hsyncs;
}

// ──────────────────────────────────────────────────────────────────────
// Wait for the next vblank (the kernel's VSync while none was counted).
// FS2 0x00159ac8

void rxThWaitVBlank(void)
{
    unsigned count = s_vblankCount;
    if (count == 0)
    {
        VSync();
        return;
    }
    while (s_vblankCount == count)
        YamiPan_SleepNs(100000);    // PC: the ROM spins
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00159b18

int rxThGetField(void)
{
    return s_field;
}

// ──────────────────────────────────────────────────────────────────────
// mode -1: the vblank count. 0: wait for the next vblank, 1: no wait,
// n > 1: wait until n vblanks have passed since the last read; then the
// HSYNCs since the last read.
// FS2 0x00159b28

int rxThSync(int mode)
{
    if (mode == RX_TH_SYNC_NEXT)
    {
        rxThWaitVBlank();
        return rxThReadHSync();
    }
    if (mode == RX_TH_SYNC_NOWAIT)
        return rxThReadHSync();
    if (mode > 0)
    {
        while (s_vblankCount < (unsigned)(mode + s_lastVBlank))
            YamiPan_SleepNs(100000);    // PC: the ROM spins
        return rxThReadHSync();
    }
    return (int)s_vblankCount;
}
