/* ==========================================================================
 *  game/thread.h
 *
 *  The game's thread system (game/thread.cpp, not written yet): eight
 *  slots run cooperatively on EE threads, paced by the frame sync. Slot 0
 *  is the main loop (gameMainThread), slot 1 the scene controller
 *  (gameSceneThread), which runs each scene in slot 2. Minimal: only what
 *  game/gamesystem.cpp touches.
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
};

// The record threadSysInit hands the frame-ready callback each vsync
// (EE size not known: at least 0xc).
struct FrameSync
{
    u_char  requested;          // FS2 +0x0: 1 while the main loop waits in frameSyncWait
    int     count;              // FS2 +0x4: vsyncs since the last frame
    int     interval;           // FS2 +0x8: vsyncs per frame (threadSysInit's interval)
};
static_assert(offsetof(FrameSync, count) == 0x4, "FrameSync::count");
static_assert(offsetof(FrameSync, interval) == 0x8, "FrameSync::interval");

typedef int  (*FrameReadyFunc)(FrameSync *sync);   // nonzero: flip now
typedef void (*ThreadSwitchHook)(u_int slot, void *arg);
typedef void (*ThreadFunc)();

void threadSysInit(FrameReadyFunc frameReady, int interval);             // FS2 0x00130f60
void threadSysShutdown();                                                // FS2 0x00130e50
void threadSetSwitchHook(ThreadSwitchHook hook, void *arg);              // FS2 0x00130a30
void threadRunMain(ThreadFunc func);                                     // FS2 0x00130e00
void threadSlotCreate(ThreadFunc func, int slot, int a2, int a3,
                      int stackSize, void *stack);                       // FS2 0x00130c90
void threadYield(int frames);                                            // FS2 0x00130bc0
int  frameSyncWait();                                                    // FS2 0x00131260

#endif /* _GAME_THREAD_H */
