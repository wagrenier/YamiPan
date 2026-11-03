// sdk/eekernel.cpp -- SCE's kernel names, forwarding to yamipan::ee::Kernel.
#include "eekernel.h"

#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <utility>

#include "eeregs.h"
#include "yamipan/core/logger.h"
#include "yamipan/ee/interrupts.h"
#include "yamipan/ee/kernel.h"

using yamipan::ee::Kernel;

namespace
{

Kernel &kernel()
{
    return Kernel::instance();
}

constexpr int kRetailMemorySize = 0x2000000; // 32 MB: a devkit's 128 would read as "boot from host0:"

u_long s_gsImr;

} // namespace

int CreateSema(SemaParam *param) { return kernel().createSema(*param); }
int DeleteSema(int semaId) { return kernel().deleteSema(semaId); }
int SignalSema(int semaId) { return kernel().signalSema(semaId); }
int iSignalSema(int semaId) { return kernel().signalSema(semaId); }
int WaitSema(int semaId) { return kernel().waitSema(semaId); }
int PollSema(int semaId) { return kernel().pollSema(semaId); }
int iPollSema(int semaId) { return kernel().pollSema(semaId); }
int ReferSemaStatus(int semaId, SemaParam *info) { return kernel().referSema(semaId, *info); }
int iReferSemaStatus(int semaId, SemaParam *info) { return kernel().referSema(semaId, *info); }

int CreateThread(ThreadParam *param) { return kernel().createThread(*param); }
int StartThread(int threadId, void *arg) { return kernel().startThread(threadId, arg); }
int TerminateThread(int threadId) { return kernel().terminateThread(threadId); }
int DeleteThread(int threadId) { return kernel().deleteThread(threadId); }
int SuspendThread(int threadId) { return kernel().suspendThread(threadId); }
int ResumeThread(int threadId) { return kernel().resumeThread(threadId); }
int GetThreadId() { return kernel().currentThreadId(); }
int SleepThread() { return kernel().sleepThread(); }
int WakeupThread(int threadId) { return kernel().wakeupThread(threadId); }
int iWakeupThread(int threadId) { return kernel().wakeupThread(threadId); }
int ChangeThreadPriority(int threadId, int priority) { return kernel().changePriority(threadId, priority); }
int iChangeThreadPriority(int threadId, int priority) { return kernel().changePriority(threadId, priority); }
int RotateThreadReadyQueue(int priority) { return kernel().rotateReadyQueue(priority); }
int iRotateThreadReadyQueue(int priority) { return kernel().rotateReadyQueue(priority); }
int ReferThreadStatus(int threadId, ThreadParam *info) { return kernel().referThread(threadId, *info); }
int DelayThread(u_int microseconds) { return kernel().delayThread(std::chrono::microseconds(microseconds)); }
void ExitThread() { kernel().exitThread(false); }
void ExitDeleteThread() { kernel().exitThread(true); }

int SetAlarm(u_short time, AlarmHandler handler, void *arg)
{
    return kernel().interrupts().setAlarm(time, handler, arg);
}

int iSetAlarm(u_short time, AlarmHandler handler, void *arg) { return SetAlarm(time, handler, arg); }
int ReleaseAlarm(int alarmId) { return kernel().interrupts().releaseAlarm(alarmId); }
int iReleaseAlarm(int alarmId) { return ReleaseAlarm(alarmId); }

void FlushCache(int) {}
int SetDebugHandler(int, YamiPanDebugHandler) { return 0; }
int AddDmacHandler(int, DmacHandler, int) { return 0; } // no bus errors on the host
int EnableDmac(int) { return 0; }
void ExitHandler() {}

int AddIntcHandler(int cause, IntcHandler handler, int next)
{
    return kernel().interrupts().addHandler(cause, handler, next);
}

int RemoveIntcHandler(int cause, int handlerId) { return kernel().interrupts().removeHandler(cause, handlerId); }

int EnableIntc(int cause)
{
    kernel().interrupts().setCauseEnabled(cause, true);
    return 1;
}

int DisableIntc(int cause)
{
    kernel().interrupts().setCauseEnabled(cause, false);
    return 1;
}

int iDisableIntc(int cause) { return DisableIntc(cause); }

int DIntr() { return kernel().disableInterrupts(); }
int EIntr() { return kernel().enableInterrupts(); }

int sceDevVif0Reset() { return 0; }
int sceDevVu0Reset() { return 0; }
int sceDevVif1Reset() { return 0; }
int sceDevVu1Reset() { return 0; }

int GetMemorySize() { return kRetailMemorySize; }

void LoadExecPS2(const char *name, int, char **)
{
    yamipan::log::error("LoadExecPS2({}): the host cannot run another ELF", name);
}

u_long sceGsGetIMR() { return s_gsImr; }
u_long sceGsPutIMR(u_long imr) { return std::exchange(s_gsImr, imr); }

int scePrintf(const char *format, ...)
{
    std::array<char, 512> text;
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(text.data(), text.size(), format, arguments);
    va_end(arguments);
    yamipan::log::info("{}", text.data());
    return length;
}

EeTimerCount::operator u_int() const
{
    return kernel().interrupts().readTimer(m_timer);
}

u_int EeTimerCount::operator=(u_int value)
{
    kernel().interrupts().writeTimer(m_timer, value);
    return value;
}
