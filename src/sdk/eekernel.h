// sdk/eekernel.h -- the EE kernel as FS2 calls it. Ordinary C++ declarations under SCE's names (the C++
// linkage keeps them apart from Windows' CreateThread at link time); yamipan::ee::Kernel is behind them.
#pragma once

#include "scetypes.h"

// ---- semaphores ------------------------------------------------------------------------------
struct SemaParam
{
    int currentCount;
    int maxCount;
    int initCount;
    int numWaitThreads;
    u_int attr;
    u_int option;
};

int CreateSema(SemaParam *param);
int DeleteSema(int semaId);
int SignalSema(int semaId);
int iSignalSema(int semaId);
int WaitSema(int semaId);
int PollSema(int semaId);
int iPollSema(int semaId);
int ReferSemaStatus(int semaId, SemaParam *info);
int iReferSemaStatus(int semaId, SemaParam *info);

// ---- threads ---------------------------------------------------------------------------------
struct ThreadParam
{
    int status;
    void (*entry)(void *);
    void *stack;
    int stackSize;
    void *gpReg;
    int initPriority;
    int currentPriority;
    u_int attr;
    u_int option;
    int waitType;
    int waitId;
    int wakeupCount;
};

int CreateThread(ThreadParam *param);
int StartThread(int threadId, void *arg);
int TerminateThread(int threadId);
int DeleteThread(int threadId);
int SuspendThread(int threadId);
int ResumeThread(int threadId);
int GetThreadId();
int SleepThread();
int WakeupThread(int threadId);
int iWakeupThread(int threadId);
int ChangeThreadPriority(int threadId, int priority);
int iChangeThreadPriority(int threadId, int priority);
int RotateThreadReadyQueue(int priority);
int iRotateThreadReadyQueue(int priority);
int ReferThreadStatus(int threadId, ThreadParam *info);
int DelayThread(u_int microseconds);
void ExitThread();
void ExitDeleteThread();

// ---- alarms ----------------------------------------------------------------------------------
// handler(id, time, arg) runs once, `time` H-sync ticks from now, in interrupt context.
using AlarmHandler = void (*)(int id, u_short time, void *arg);

int SetAlarm(u_short time, AlarmHandler handler, void *arg);
int iSetAlarm(u_short time, AlarmHandler handler, void *arg);
int ReleaseAlarm(int alarmId);
int iReleaseAlarm(int alarmId);

// ---- caches, exceptions, interrupts ------------------------------------------------------------
constexpr int WRITEBACK_DCACHE = 0;
constexpr int INVALID_DCACHE = 1;
constexpr int INVALIDATE_ICACHE = 2;
constexpr int INVALIDATE_CACHE = INVALID_DCACHE | INVALIDATE_ICACHE;

constexpr int INTC_VBLANK_S = 2;
constexpr int INTC_VBLANK_E = 3;

using YamiPanDebugHandler = void (*)(u_int stat, u_int cause, u_int epc, u_int bva, u_int bpa, u_long128 *gpr);
using DmacHandler = int (*)(int channel);
using IntcHandler = int (*)(int cause);

void FlushCache(int mode);
int SetDebugHandler(int cause, YamiPanDebugHandler handler);
int AddDmacHandler(int channel, DmacHandler handler, int next);
int AddIntcHandler(int cause, IntcHandler handler, int next);
int RemoveIntcHandler(int cause, int handlerId);
int EnableIntc(int cause);
int DisableIntc(int cause);
int iDisableIntc(int cause);
int EnableDmac(int channel);
void ExitHandler();

// Interrupt disable / enable: the ROM always pairs them, so they nest.
int DIntr();
int EIntr();

// ---- devices and the rest ----------------------------------------------------------------------
int sceDevVif0Reset();
int sceDevVu0Reset();
int sceDevVif1Reset();
int sceDevVu1Reset();

int GetMemorySize();
void LoadExecPS2(const char *name, int argc, char **argv);

u_long sceGsGetIMR();
u_long sceGsPutIMR(u_long imr);

int scePrintf(const char *format, ...);

// Pipeline sync and interrupt enable: asm intrinsics on the EE, nothing to do here.
#define SYNC(t) ((void)0)
#define EI() ((void)0)
