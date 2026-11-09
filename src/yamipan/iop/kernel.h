// The IOP as RXSNDDRV sees it: one CPU. Exactly one IOP thread ran at a time, and a switch happened only
// where the code itself blocked (WaitEventFlag, WaitVblankEnd) or in an interrupt. The driver's critical
// sections are written against that: against another thread it needed nothing, so CpuSuspendIntr is
// its only guard. Here its threads are real host threads, and the EE's RPC calls arrive on the EE's
// thread on top of that, so the guarantee is gone.
//
// The big kernel lock puts it back. Every IOP context holds it while it runs IOP code and drops it only
// inside the blocking calls, so IOP code is serialised against IOP code exactly where the IOP assumed it
// was, with no change to the reconstruction (RosePan's sdk/iop_host.cpp, whose design this is).
//
// Not modelled: thread priorities (the lock serialises; which waiter runs next is the host's choice)
// and hardware timers (RXSNDDRV uses none).
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "yamipan/iop/rpc_servers.h"
#include "yamipan/iop/spu2.h"
#include "yamipan/iop/system_memory.h"

namespace yamipan::os
{
class Thread;
}

namespace yamipan::iop
{

// WaitEventFlag's modes, as the IOP SDK spells them.
enum WaitMode : int
{
    kWaitAnd = 0x00,
    kWaitOr = 0x01,
    kWaitClear = 0x10,
};

class Kernel
{
public:
    // Created on first use and never destroyed: the driver's threads loop for ever, so the IOP has to
    // outlive static destruction. The process ends over it, as it does over the EE's threads.
    static Kernel &instance();

    Kernel(const Kernel &) = delete;
    Kernel &operator=(const Kernel &) = delete;

    // ---- the big kernel lock ----------------------------------------------------------------------

    void enter();
    void leave();

    // Gives the lock back completely, however deeply this thread holds it, and says how deeply: a
    // blocking call parks without it, then takes the same depth again.
    int suspend();
    void resume(int depth);

    // Runs a module's start() as the IOP's loader did: on the caller's thread, holding the lock.
    int runModuleStart(int (*start)());

    // ---- threads ----------------------------------------------------------------------------------

    // The work pointer travels in `option`, pointer-wide here (it was a 32-bit word on the IOP).
    int createThread(void (*entry)(), std::uintptr_t option, int priority, int stackSize);
    int startThread(int id);
    int currentThreadId() const;

    // ---- event flags ------------------------------------------------------------------------------

    int createEventFlag(std::uint32_t initPattern);
    int setEventFlag(int id, std::uint32_t bits);
    int waitEventFlag(int id, std::uint32_t bits, int mode, std::uint32_t *result);

    // ---- V-blank ----------------------------------------------------------------------------------

    // Parks until the next V-blank at the EE's video standard: 50 Hz for FS2's PAL, 59.94 for NTSC.
    int waitVblankEnd();

    SystemMemory &memory() { return m_memory; }
    RpcServers &rpc() { return m_rpc; }
    Spu2 &spu2() { return m_spu2; }

private:
    struct ThreadRecord;
    struct EventFlag
    {
        std::uint32_t bits = 0;
    };

    Kernel();
    ~Kernel();

    ThreadRecord *findThread(int id);
    void threadBody(ThreadRecord &record);

    // The big kernel lock. Plain, not recursive: this thread's depth is counted beside it.
    std::mutex m_bkl;

    // The tables, and the condition every event flag waits on (few flags, rarely set: one is enough).
    std::mutex m_tableLock;
    std::condition_variable m_flagChanged;
    std::vector<std::unique_ptr<ThreadRecord>> m_threads;
    std::vector<EventFlag> m_flags;

    std::chrono::steady_clock::time_point m_nextVblank;
    static constexpr int kVblankReport = 500;
    int m_vblanks = 0;
    std::chrono::steady_clock::time_point m_firstVblank;

    SystemMemory m_memory;
    RpcServers m_rpc;
    Spu2 m_spu2;
};

} // namespace yamipan::iop
