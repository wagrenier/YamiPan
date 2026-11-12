// game/thread.cpp -- the cooperative thread system, the frame sync and the
// job worker (game/thread.h).

#include "game/thread.h"

#include <string.h>

#include "sdk/eekernel.h"
#include "sdk/libgraph.h"
#include "sdk/scetypes.h"

// EE thread priorities (lower runs first).
constexpr int kFrameSyncPriority = 0x10;
constexpr int kSlotPriority = 0x30;
constexpr int kSchedulerPriority = 0x40;
constexpr int kJobWorkerPriority = 0x50;
constexpr int kRotatePriority = 0x20;   // the ready queue the scheduler rotates before each switch

constexpr int kSchedulerStackSize = 0x800;
constexpr int kFrameSyncStackSize = 0x800;
constexpr int kJobWorkerStack = ThreadSlot_Count;   // sThreadStacks' ninth row

constexpr int kJobIdMax = 0x6fffffff;   // jobAdd's ids wrap back to 1 past this

// The default stacks, one bss block 0x4af040..0x4c7840 cut by the table
// 0x2f38c0 (the slot stacks, then the job worker's).
alignas(16) static u_int sThreadStackPool[0x18800 / 4];   /* bss 0x4af040 */

static const ThreadStackDef sThreadStacks[ThreadSlot_Count + 1] = {   /* data 0x2f38c0 */
    { 0x1000, sThreadStackPool + 0x00000 / 4 },
    { 0x2000, sThreadStackPool + 0x01000 / 4 },
    { 0x2000, sThreadStackPool + 0x03000 / 4 },
    { 0xc000, sThreadStackPool + 0x05000 / 4 },
    { 0x3000, sThreadStackPool + 0x11000 / 4 },
    { 0x1000, sThreadStackPool + 0x14000 / 4 },
    { 0x1800, sThreadStackPool + 0x15000 / 4 },
    { 0x1000, sThreadStackPool + 0x16800 / 4 },
    { 0x1000, sThreadStackPool + 0x17800 / 4 },
};

static ThreadSlotRecord sThreadSlots[ThreadSlot_Count];   /* bss 0x4c7840 */
alignas(16) static u_char sFrameSyncStack[kFrameSyncStackSize];   /* bss 0x4c7960 */
static FrameSync sFrameSync;                              /* bss 0x4c8160 */
static ThreadDetourStack sDetours;                        /* bss 0x4c8170 */
alignas(16) static u_char sSchedulerStack[kSchedulerStackSize];   /* bss 0x4c81b0 */
static JobRecord sJobs[kJobMax];                          /* bss 0x4c89b0 */
static int sJobWorkerThread;                              /* bss 0x4c8b30 */
static int sJobUnused;                                    /* bss 0x4c8b34: cleared by threadSysInit, read nowhere */
static JobRecord *volatile sJobHead;                      /* bss 0x4c8b38 */
static JobRecord *sJobTail;                               /* bss 0x4c8b3c */

static int sCurSlot = -1;                       /* sdata 0x31a898 */
static int sFrameDoneSema = -1;                 /* sdata 0x31a89c: frameSyncThread -> frameSyncWait */
static int sFrameSyncThread = -1;               /* sdata 0x31a8a0 */
static int sVSyncSema = -1;                     /* sdata 0x31a8a4: threadVSyncHandler -> frameSyncThread */
static int sJobNextId = 1;                      /* sdata 0x31a8a8 */

static int sMainDoneSema;                       /* sbss 0x31adb8: threadRunMain waits on it */
static u_char sQuit;                            /* sbss 0x31adbc: nothing in the ELF sets it */
static FrameReadyFunc sFrameReady;              /* sbss 0x31adc0 */
static int sFrameVsyncs;                        /* sbss 0x31adc4: frameSyncWait's result */
static int sSchedulerThread;                    /* sbss 0x31adc8 */
static ThreadSwitchHook sPreSwitchHook;         /* sbss 0x31adcc: nothing sets it */
static ThreadSwitchHook sPostSwitchHook;        /* sbss 0x31add0 */
static void *sPreSwitchArg;                     /* sbss 0x31add4 */
static void *sPostSwitchArg;                    /* sbss 0x31add8 */

static void threadScheduler(void *arg);
static void jobWorkerThread(void *arg);
static int  threadVSyncHandler(int cause);
static void frameSyncThread(void *arg);

// ──────────────────────────────────────────────────────────────────────
// Inlined helpers.

// A slot whose stack bottom was overwritten hangs the game here.
static inline void threadCheckCanary(ThreadSlotRecord *slot)
{
    if (*slot->stack != kThreadStackCanary) {
        for (;;)
            sceGsSyncV(0);
    }
}

// threadSlotKill's body, as threadSlotCreate and threadSysShutdown inline
// it: a live slot is freed and its thread removed (a start-pending one was
// never started, so not terminated); the cleanup runs either way.
static inline void threadKill(ThreadSlotRecord *slot)
{
    u_int state = slot->state & ~ThreadStateFlag_80;
    if (state != ThreadState_Free) {
        slot->state = ThreadState_Free;
        if (state != ThreadState_Start)
            TerminateThread(slot->tid);
        DeleteThread(slot->tid);
    }
    if (slot->cleanup != nullptr)
        slot->cleanup(slot->arg);
}

static inline JobRecord *jobFind(int id)
{
    JobRecord *job = sJobHead;
    while (job != nullptr && job->id != id)
        job = job->next;
    return job;
}

static inline void jobAppend(JobRecord *job)
{
    if (sJobHead == nullptr) {
        job->prev = nullptr;
        job->next = nullptr;
        sJobHead = job;
    } else {
        job->prev = sJobTail;
        job->next = nullptr;
        sJobTail->next = job;
    }
    sJobTail = job;
}

// (Re)creates the job worker; a running job dies with the old thread.
static inline void jobWorkerStart()
{
    if (sJobWorkerThread >= 0) {
        TerminateThread(sJobWorkerThread);
        DeleteThread(sJobWorkerThread);
        sJobWorkerThread = -1;
    }
    ThreadParam param;
    param.option = 0;
    param.entry = jobWorkerThread;
    param.stack = sThreadStacks[kJobWorkerStack].base;
    param.stackSize = sThreadStacks[kJobWorkerStack].size;
    param.initPriority = kJobWorkerPriority;
    param.gpReg = 0;            // PC: the ROM passes &_gp
    sJobWorkerThread = CreateThread(&param);
    StartThread(sJobWorkerThread, 0);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00130a20

int bitMask(u_int bit)
{
    return 1 << (bit & 0x1f);
}

// ──────────────────────────────────────────────────────────────────────
// The hook the scheduler calls after each switch back to it (main sets the
// perf meter's).
// FS2 0x00130a30

void threadSetSwitchHook(ThreadSwitchHook hook, void *arg)
{
    sPostSwitchHook = hook;
    sPostSwitchArg = arg;
}

// ──────────────────────────────────────────────────────────────────────
// Sets what a slot's threadYield returns; the old value back.
// FS2 0x00130a40

int threadSwapMessage(int slot, int message)
{
    if (slot < 0)
        slot = sCurSlot;
    int old = sThreadSlots[slot].message;
    sThreadSlots[slot].message = message;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00130a70

u_int threadSlotState(int slot)
{
    return sThreadSlots[slot].state;
}

// ──────────────────────────────────────────────────────────────────────
// The running slot ends: freed, the scheduler woken, its cleanup run, and
// the thread deletes itself.
// FS2 0x00130a90
// callcheck: sceGsSyncV (threadCheckCanary inlined)

void threadExit()
{
    ThreadSlotRecord *slot = &sThreadSlots[sCurSlot];
    threadCheckCanary(slot);
    slot->state = ThreadState_Free;
    ChangeThreadPriority(sSchedulerThread, kSchedulerPriority);
    WakeupThread(sSchedulerThread);
    if (slot->cleanup != nullptr)
        slot->cleanup(slot->arg);
    ExitDeleteThread();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00130b30
// callcheck: TerminateThread, DeleteThread (the body is threadKill)

void threadSlotKill(int slot)
{
    threadKill(&sThreadSlots[slot]);
}

// ──────────────────────────────────────────────────────────────────────
// The running slot waits `ticks` turns of the scheduler (1: its next
// turn), then gets its message.
// FS2 0x00130bc0
// callcheck: sceGsSyncV (threadCheckCanary inlined)

int threadYield(int ticks)
{
    FlushCache(0);
    ThreadSlotRecord *slot = &sThreadSlots[sCurSlot];
    threadCheckCanary(slot);
    slot->state = ThreadState_Waiting;
    slot->ticks = ticks;
    ChangeThreadPriority(sSchedulerThread, kSchedulerPriority);
    WakeupThread(sSchedulerThread);
    SleepThread();
    return sThreadSlots[sCurSlot].message;
}

// ──────────────────────────────────────────────────────────────────────
// Puts func(arg) in a slot (killing what ran there): on the slot's default
// stack unless the caller gives one; it starts on the slot's next turn.
// FS2 0x00130c90
// callcheck: TerminateThread, DeleteThread (threadSlotKill inlined)

void threadSlotCreate(ThreadFunc func, int slot, void *arg,
                      ThreadCleanupFunc cleanup, int stackSize, u_int *stack)
{
    ThreadSlotRecord *rec = &sThreadSlots[slot];
    if (rec->state != ThreadState_Free)
        threadKill(rec);

    ThreadParam param;
    param.entry = func;
    if (stackSize == 0) {
        param.stack = sThreadStacks[slot].base;
        param.stackSize = sThreadStacks[slot].size;
        rec->ownStack = 0;
    } else {
        param.stack = stack;
        param.stackSize = stackSize;
        rec->ownStack = 1;
    }
    param.option = slot;
    param.gpReg = 0;            // PC: the ROM passes &_gp
    param.initPriority = kSlotPriority;
    rec->stack = static_cast<u_int *>(param.stack);
    rec->stackSize = param.stackSize;
    rec->tid = CreateThread(&param);
    rec->ticks = 0;
    rec->state = ThreadState_Start;
    rec->arg = arg;
    rec->cleanup = cleanup;
    *rec->stack = kThreadStackCanary;
    rec->message = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Runs the slots with func in slot 0 until the scheduler signals the end.
// FS2 0x00130e00

void threadRunMain(ThreadFunc func)
{
    threadSlotCreate(func, ThreadSlot_Ctrl, 0, 0, 0, 0);
    sCurSlot = -1;              // the scheduler's first step goes to slot 0
    StartThread(sSchedulerThread, 0);
    WaitSema(sMainDoneSema);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00130e50
// callcheck: TerminateThread, DeleteThread (threadSlotKill inlined)

void threadSysShutdown()
{
    sceGsSyncVCallback(0);
    if (sFrameSyncThread >= 0) {
        TerminateThread(sFrameSyncThread);
        DeleteThread(sFrameSyncThread);
        sFrameSyncThread = -1;
    }
    if (sVSyncSema >= 0) {
        DeleteSema(sVSyncSema);
        sVSyncSema = -1;
    }
    TerminateThread(sSchedulerThread);
    DeleteThread(sSchedulerThread);
    DeleteSema(sMainDoneSema);
    DeleteSema(sFrameDoneSema);
    for (int i = 0; i < ThreadSlot_Count; i++)
        threadKill(&sThreadSlots[i]);
}

// ──────────────────────────────────────────────────────────────────────
// Clears the slots, makes the scheduler (started by threadRunMain), starts
// the job worker and the frame sync.
// FS2 0x00130f60
// callcheck: StartThread, TerminateThread, DeleteThread (jobWorkerStart inlined; the ROM drops Terminate / Delete, the worker is -1 here)

void threadSysInit(FrameReadyFunc frameReady, int interval)
{
    memset(sThreadSlots, 0, sizeof(sThreadSlots));
    memset(&sDetours, 0, sizeof(sDetours));

    SemaParam sema = {};
    sema.maxCount = 1;
    sema.initCount = 0;
    sMainDoneSema = CreateSema(&sema);

    ThreadParam param;
    param.entry = threadScheduler;
    param.stack = sSchedulerStack;
    param.stackSize = kSchedulerStackSize;
    param.gpReg = 0;            // PC: the ROM passes &_gp
    param.initPriority = kSchedulerPriority;
    sSchedulerThread = CreateThread(&param);

    sema.maxCount = 1;
    sema.initCount = 0;
    sFrameDoneSema = CreateSema(&sema);

    memset(sJobs, 0, sizeof(sJobs));
    sJobWorkerThread = -1;
    sJobUnused = 0;
    sJobHead = nullptr;
    sJobTail = nullptr;
    jobWorkerStart();

    sQuit = 0;
    frameSyncInit(frameReady, interval);
    sPreSwitchHook = nullptr;
    sPreSwitchArg = nullptr;
    sPostSwitchHook = nullptr;
    sPostSwitchArg = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The main loop's frame wait: the vsyncs since the last frame.
// FS2 0x00131260

int frameSyncWait()
{
    sFrameSync.requested = 1;
    WaitSema(sFrameDoneSema);
    return sFrameVsyncs;
}

// ──────────────────────────────────────────────────────────────────────
// The scheduler thread: steps through the slots (or a detour), starting a
// start-pending slot and counting down a waiting one's ticks, and sleeps
// while the slot it switched to runs. The slots run above its priority, so
// a slot keeps the CPU until it yields; with nothing to run the scheduler
// spins round the slots. Each wrap past slot 7 ends threadRunMain once
// sQuit is set.
// FS2 0x00131290

static void threadScheduler(void *arg)
{
    (void)arg;
    sceGsSyncVCallback(threadVSyncHandler);
    for (;;) {
        sCurSlot++;
        if (sDetours.count != 0) {
            ThreadDetour *detour = &sDetours.entries[sDetours.count - 1];
            if (detour->state == ThreadDetour_Return) {
                sCurSlot = detour->resume;
                detour->state = ThreadDetour_Done;
                sDetours.count--;
            } else if (detour->state == ThreadDetour_Pending) {
                sCurSlot = detour->target;
                detour->state = ThreadDetour_Return;
            } else {
                sDetours.count--;
            }
        }
        if (sCurSlot >= ThreadSlot_Count) {
            sCurSlot = 0;
            if (sQuit)
                SignalSema(sMainDoneSema);
        }

        ThreadSlotRecord *slot = &sThreadSlots[sCurSlot];
        if (slot->state == ThreadState_Start) {
            if (sPreSwitchHook != nullptr)
                sPreSwitchHook(sCurSlot, sPreSwitchArg);
            RotateThreadReadyQueue(kRotatePriority);
            StartThread(sThreadSlots[sCurSlot].tid, sThreadSlots[sCurSlot].arg);
            SleepThread();
            if (sPostSwitchHook != nullptr)
                sPostSwitchHook(sCurSlot, sPostSwitchArg);
        } else if (slot->state == ThreadState_Waiting && --slot->ticks == 0) {
            if (sPreSwitchHook != nullptr)
                sPreSwitchHook(sCurSlot, sPreSwitchArg);
            RotateThreadReadyQueue(kRotatePriority);
            WakeupThread(sThreadSlots[sCurSlot].tid);
            SleepThread();
            if (sPostSwitchHook != nullptr)
                sPostSwitchHook(sCurSlot, sPostSwitchArg);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Takes a job out: a waiting one is just removed, a running one dies with
// the worker, which is started afresh.
// FS2 0x00131480
// callcheck: TerminateThread, DeleteThread, CreateThread, StartThread (jobWorkerStart inlined)

int jobCancel(int id)
{
    JobRecord *job = jobFind(id);
    int result;
    if (job == nullptr)
        result = JobCancel_NotFound;
    else if (job->busy == 0)
        result = JobCancel_Waiting;
    else if (job->busy == 1)
        result = JobCancel_Running;
    else
        result = JobCancel_NotFound;

    if (result == JobCancel_NotFound)
        return JobCancel_NotFound;

    if (result == JobCancel_Running)
        jobWorkerStart();

    job = jobFind(id);
    JobRecord *prev = job->prev;
    JobRecord *next = job->next;
    memset(job, 0, sizeof(*job));
    if (prev == nullptr)
        sJobHead = next;
    else
        prev->next = next;
    if (next == nullptr)
        sJobTail = prev;
    else
        next->prev = prev;
    return result;
}

// ──────────────────────────────────────────────────────────────────────
// Queues fn(arg) for the job worker: its id, 0 when all records are used.
// FS2 0x00131620

int jobAdd(JobFunc fn, void *arg)
{
    int id = sJobNextId;
    JobRecord *job = nullptr;
    for (int i = 0; i < kJobMax; i++) {
        if (sJobs[i].id == 0) {
            job = &sJobs[i];
            break;
        }
    }
    if (job == nullptr)
        return 0;

    memset(job, 0, sizeof(*job));
    jobAppend(job);
    job->fn = fn;
    job->arg = arg;
    job->id = sJobNextId;
    sJobNextId++;
    if (sJobNextId > kJobIdMax)
        sJobNextId = 1;
    return id;
}

// ──────────────────────────────────────────────────────────────────────
// The job worker (the lowest priority: it runs when nothing else can):
// runs the head job, then frees it, or moves it to the tail when it asks
// to run again. It spins while the list is empty.
// PC: the spin is the ROM's; the platform layer must not let it starve the
// other host threads.
// FS2 0x00131720

static void jobWorkerThread(void *arg)
{
    (void)arg;
    for (;;) {
        while (sJobHead == nullptr) {
        }
        DIntr();
        JobRecord *job = sJobHead;
        if (job != nullptr)
            job->busy = 1;
        EIntr();
        if (job == nullptr)
            continue;

        int again = job->fn(job->arg);

        DIntr();
        job->busy = 0;
        sJobHead = job->next;
        if (sJobHead != nullptr)
            sJobHead->prev = nullptr;
        if (again == 0)
            memset(job, 0, sizeof(*job));
        else
            jobAppend(job);
        EIntr();
    }
}

// ──────────────────────────────────────────────────────────────────────
// The vblank callback: leaves exactly one count on the vsync semaphore.
// FS2 0x00131840

static int threadVSyncHandler(int cause)
{
    (void)cause;
    iPollSema(sVSyncSema);
    iSignalSema(sVSyncSema);
    // PC: the ROM ends with sync.l / ei (the interrupt handler epilogue).
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00131870

void frameSyncInit(FrameReadyFunc frameReady, int interval)
{
    memset(&sFrameSync, 0, sizeof(sFrameSync));
    SemaParam sema = {};
    sema.maxCount = 2;
    sema.initCount = 0;
    sema.option = 0;
    sFrameSync.interval = interval;
    sFrameReady = frameReady;
    sVSyncSema = CreateSema(&sema);

    ThreadParam param;
    param.option = 0;
    param.entry = frameSyncThread;
    param.stack = sFrameSyncStack;
    param.stackSize = kFrameSyncStackSize;
    param.initPriority = kFrameSyncPriority;
    param.gpReg = 0;            // PC: the ROM passes &_gp
    sFrameSyncThread = CreateThread(&param);
    StartThread(sFrameSyncThread, 0);
}

// ──────────────────────────────────────────────────────────────────────
// The frame sync thread (above everything): per vsync, counts and asks the
// frame-ready callback (or, without one, whether a waiting main loop's
// interval has passed); a ready frame releases frameSyncWait with the count.
// FS2 0x00131930

static void frameSyncThread(void *arg)
{
    (void)arg;
    for (;;) {
        WaitSema(sVSyncSema);
        sFrameSync.count++;
        char ready = 0;
        if (sFrameSync.requested == 1 && sFrameSync.interval <= sFrameSync.count)
            ready = 1;
        if (sFrameReady != nullptr)
            ready = (char)sFrameReady(&sFrameSync);
        if (ready == 1) {
            sFrameSync.requested = 0;
            sFrameVsyncs = sFrameSync.count;
            sFrameSync.count = 0;
            SignalSema(sFrameDoneSema);
        }
    }
}
