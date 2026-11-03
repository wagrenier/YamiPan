/* ==========================================================================
 *  game/thread.h
 *
 *  The game's thread system (game/thread.cpp, 0x130a20..0x1319f0): eight
 *  slots, each an EE thread at priority 0x30, run cooperatively by a
 *  scheduler thread (priority 0x40) that the vblank wakes: each tick it
 *  steps the slots round-robin, starting slots whose start is pending and
 *  waking those whose wait has run out; a slot runs until it yields. Slot 0
 *  is the main loop (gameMainThread), slot 1 the scene controller
 *  (gameSceneThread), which runs each scene in slot 2. Beside it: the frame
 *  sync (a thread at priority 0x10 counting vblanks and asking the game's
 *  frame-ready callback when a frame is due) and a job worker (priority
 *  0x50, the idle time) running queued function calls one after another.
 * ======================================================================== */
#ifndef _GAME_THREAD_H
#define _GAME_THREAD_H

#include <stddef.h>

#include "sdk/scetypes.h"

// The slots, by the perf meter's names for them (0x2f3810: "Th Ctrl" ..).
enum ThreadSlot
{
    ThreadSlot_Ctrl   = 0,      // gameMainThread
    ThreadSlot_Main   = 1,      // gameSceneThread
    ThreadSlot_02     = 2,      // the current scene (gameSceneThread)
    ThreadSlot_Game   = 3,
    ThreadSlot_04     = 4,
    ThreadSlot_Extra  = 5,
    ThreadSlot_06     = 6,
    ThreadSlot_Effect = 7,
    ThreadSlot_Count  = 8,
    ThreadSlot_Current = -1,    // threadSwapMessage: the running slot
};

// ThreadSlotRecord::state. Bit 7 is masked off wherever a slot is torn down
// (threadSlotKill), never set in this TU.
enum ThreadState
{
    ThreadState_Free    = 0,    // no thread (ended, killed or never made)
    ThreadState_Waiting = 1,    // yielded: runs again when ticks reaches 0
    ThreadState_Start   = 2,    // created, StartThread on its next turn
};
constexpr u_int ThreadStateFlag_80 = 0x80;

// The word at the bottom of every slot stack, checked on each yield and
// exit (a mismatch hangs the game in sceGsSyncV).
constexpr u_int kThreadStackCanary = 0x572a8b4c;

typedef void (*ThreadFunc)(void *arg);
typedef void (*ThreadCleanupFunc)(void *arg);
typedef void (*ThreadSwitchHook)(u_int slot, void *arg);

// One slot, EE 0x24 (array 0x4c7840).
// layout: host -- pointers; the EE offsets are in the comments.
struct ThreadSlotRecord
{
    int               tid;          // FS2 +0x00: the EE thread
    u_int             state;        // FS2 +0x04: ThreadState
    int               ticks;        // FS2 +0x08: scheduler ticks left to wait
    void             *arg;          // FS2 +0x0c: the entry's argument, the cleanup's too
    ThreadCleanupFunc cleanup;      // FS2 +0x10: called when the slot ends or is killed
    int               message;      // FS2 +0x14: what threadYield returns (threadSwapMessage)
    int               stackSize;    // FS2 +0x18
    u_int            *stack;        // FS2 +0x1c: lowest word: kThreadStackCanary
    u_char            ownStack;     // FS2 +0x20: 1 when the caller gave the stack
};
static_assert(offsetof(ThreadSlotRecord, tid) == 0x0, "ThreadSlotRecord::tid");

// A slot's default stack (table 0x2f38c0, one per slot and a ninth for the
// job worker; all in one bss block 0x4af040..0x4c7840).
// layout: host -- pointers; EE 0x8: size +0x0, base +0x4.
struct ThreadStackDef
{
    int    size;                // FS2 +0x0
    u_int *base;                // FS2 +0x4
};

// The scheduler's detour stack (0x4c8170: count, then 4 entries): the top
// entry makes the next turn go to `target`, the turn after back to
// `resume`, then it is popped. Only threadSysInit (clearing) and
// threadScheduler (reading) touch it; nothing in the ELF pushes an entry.
struct ThreadDetour
{
    int resume;                 // FS2 +0x0: slot after the detour
    int target;                 // FS2 +0x4: slot the detour runs
    int state;                  // FS2 +0x8: ThreadDetourState
};
static_assert(sizeof(ThreadDetour) == 0xc, "ThreadDetour");
static_assert(offsetof(ThreadDetour, state) == 0x8, "ThreadDetour::state");

enum ThreadDetourState
{
    ThreadDetour_Done    = 0,   // popped without a switch
    ThreadDetour_Return  = 1,   // next turn: back to resume, then pop
    ThreadDetour_Pending = 2,   // next turn: to target
};

constexpr int kThreadDetourMax = 4;

struct ThreadDetourStack
{
    int          count;                     // FS2 +0x0
    ThreadDetour entries[kThreadDetourMax]; // FS2 +0x4
};
static_assert(sizeof(ThreadDetourStack) == 0x34, "ThreadDetourStack");

// The record the frame sync hands the frame-ready callback each vsync
// (0x4c8160, 0xc bytes cleared by frameSyncInit).
struct FrameSync
{
    u_char  requested;          // FS2 +0x0: 1 while the main loop waits in frameSyncWait
    int     count;              // FS2 +0x4: vsyncs since the last frame
    int     interval;           // FS2 +0x8: vsyncs per frame (threadSysInit's interval)
};
static_assert(offsetof(FrameSync, count) == 0x4, "FrameSync::count");
static_assert(offsetof(FrameSync, interval) == 0x8, "FrameSync::interval");
static_assert(sizeof(FrameSync) == 0xc, "FrameSync");

typedef int  (*FrameReadyFunc)(FrameSync *sync);   // 1: the frame is ready

// A queued call (16 records 0x4c89b0, EE 0x18): the job worker calls
// fn(arg); a nonzero return queues it again at the tail.
// layout: host -- pointers; the EE offsets are in the comments.
typedef int (*JobFunc)(void *arg);

struct JobRecord
{
    int        busy;            // FS2 +0x00: 1 while the worker runs it
    int        id;              // FS2 +0x04: jobAdd's id; 0 marks a free record
    JobFunc    fn;              // FS2 +0x08
    void      *arg;             // FS2 +0x0c
    JobRecord *prev;            // FS2 +0x10
    JobRecord *next;            // FS2 +0x14
};
static_assert(offsetof(JobRecord, busy) == 0x0, "JobRecord::busy");

constexpr int kJobMax = 16;

// jobCancel's result.
enum JobCancelResult
{
    JobCancel_NotFound = 0,
    JobCancel_Waiting  = 1,     // removed before it ran
    JobCancel_Running  = 2,     // the worker was restarted under it
};

int  bitMask(u_int bit);                                                 // FS2 0x00130a20
void threadSetSwitchHook(ThreadSwitchHook hook, void *arg);              // FS2 0x00130a30
int  threadSwapMessage(int slot, int message);                           // FS2 0x00130a40
u_int threadSlotState(int slot);                                         // FS2 0x00130a70
void threadExit();                                                       // FS2 0x00130a90
void threadSlotKill(int slot);                                           // FS2 0x00130b30
int  threadYield(int ticks);                                             // FS2 0x00130bc0
void threadSlotCreate(ThreadFunc func, int slot, void *arg,
                      ThreadCleanupFunc cleanup, int stackSize,
                      u_int *stack);                                     // FS2 0x00130c90
void threadRunMain(ThreadFunc func);                                     // FS2 0x00130e00
void threadSysShutdown();                                                // FS2 0x00130e50
void threadSysInit(FrameReadyFunc frameReady, int interval);             // FS2 0x00130f60
int  frameSyncWait();                                                    // FS2 0x00131260
int  jobCancel(int id);                                                  // FS2 0x00131480
int  jobAdd(JobFunc fn, void *arg);                                      // FS2 0x00131620
void frameSyncInit(FrameReadyFunc frameReady, int interval);             // FS2 0x00131870

#endif /* _GAME_THREAD_H */
