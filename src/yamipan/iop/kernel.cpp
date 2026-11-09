#include "yamipan/iop/kernel.h"

#include <format>
#include <thread>

#include "yamipan/core/logger.h"
#include "yamipan/ee/interrupts.h"
#include "yamipan/ee/kernel.h"
#include "yamipan/os/thread.h"

namespace yamipan::iop
{

namespace
{

// KE_NOT_DORMANT: StartThread on a thread that is still running.
constexpr int kNotDormant = -414;

// IOP stacks were 2-4 KB; host code under the driver (printf, the C library) needs far more.
constexpr std::size_t kHostStackBytes = 256 * 1024;

// This thread's depth in the big kernel lock, and its IOP thread id (0 on a thread that is not one).
thread_local int t_depth = 0;
thread_local int t_threadId = 0;

} // namespace

struct Kernel::ThreadRecord
{
    int id = 0;
    void (*entry)() = nullptr;
    std::uintptr_t option = 0;
    int priority = 0;
    std::atomic<bool> running{false};
};

Kernel &Kernel::instance()
{
    // A heap object nothing deletes, not a static: a static's destructor would run while the driver's
    // threads still use the kernel (see the header).
    static Kernel *const kernel = new Kernel();
    return *kernel;
}

Kernel::Kernel() = default;
Kernel::~Kernel() = default;

// ---- the big kernel lock --------------------------------------------------------------------------

void Kernel::enter()
{
    if (t_depth++ == 0)
        m_bkl.lock();
}

void Kernel::leave()
{
    if (t_depth > 0 && --t_depth == 0)
        m_bkl.unlock();
}

int Kernel::suspend()
{
    const int depth = t_depth;
    if (depth > 0)
    {
        t_depth = 0;
        m_bkl.unlock();
    }
    return depth;
}

void Kernel::resume(int depth)
{
    if (depth > 0)
    {
        m_bkl.lock();
        t_depth = depth;
    }
}

int Kernel::runModuleStart(int (*start)())
{
    enter();
    const int result = start();
    leave();
    return result;
}

// ---- threads --------------------------------------------------------------------------------------

Kernel::ThreadRecord *Kernel::findThread(int id)
{
    if (id < 1 || id > static_cast<int>(m_threads.size()))
        return nullptr;
    return m_threads[static_cast<std::size_t>(id - 1)].get();
}

int Kernel::createThread(void (*entry)(), std::uintptr_t option, int priority, int)
{
    if (entry == nullptr)
        return -1;

    std::lock_guard lock(m_tableLock);
    auto record = std::make_unique<ThreadRecord>();
    record->id = static_cast<int>(m_threads.size()) + 1;
    record->entry = entry;
    record->option = option;
    record->priority = priority;
    m_threads.push_back(std::move(record));
    return m_threads.back()->id;
}

// The thread holds the lock for its whole body and drops it only inside the blocking calls, so its IOP
// code never interleaves with another IOP context's. A new thread therefore first runs when its creator
// blocks, where the IOP would have run a more urgent one at once: RXSNDDRV's start() does not care.
void Kernel::threadBody(ThreadRecord &record)
{
    t_threadId = record.id;
    enter();
    record.entry();
    record.running = false;
    leave();
}

int Kernel::startThread(int id)
{
    std::lock_guard lock(m_tableLock);
    ThreadRecord *record = findThread(id);
    if (record == nullptr)
        return -1;
    if (record->running.exchange(true))
        return kNotDormant;

    // Detached: the driver's threads loop for ever and the process ends over them.
    os::Thread(std::format("YamiPanIop{}", id), kHostStackBytes, [this, record] { threadBody(*record); })
        .detach();
    return 0;
}

int Kernel::currentThreadId() const
{
    return t_threadId;
}

// ---- event flags ----------------------------------------------------------------------------------

int Kernel::createEventFlag(std::uint32_t initPattern)
{
    std::lock_guard lock(m_tableLock);
    m_flags.push_back(EventFlag{initPattern});
    return static_cast<int>(m_flags.size());
}

int Kernel::setEventFlag(int id, std::uint32_t bits)
{
    {
        std::lock_guard lock(m_tableLock);
        if (id < 1 || id > static_cast<int>(m_flags.size()))
            return -1;
        m_flags[static_cast<std::size_t>(id - 1)].bits |= bits;
    }
    m_flagChanged.notify_all();
    return 0;
}

int Kernel::waitEventFlag(int id, std::uint32_t bits, int mode, std::uint32_t *result)
{
    const int depth = suspend();
    int answer = 0;
    {
        std::unique_lock lock(m_tableLock);
        if (id < 1 || id > static_cast<int>(m_flags.size()))
        {
            answer = -1;
        }
        else
        {
            const auto index = static_cast<std::size_t>(id - 1);
            const auto hit = [&] {
                const std::uint32_t now = m_flags[index].bits;
                return (mode & kWaitOr) != 0 ? (now & bits) != 0 : (now & bits) == bits;
            };
            m_flagChanged.wait(lock, hit);
            if (result != nullptr)
                *result = m_flags[index].bits;
            if ((mode & kWaitClear) != 0)
                m_flags[index].bits &= ~bits;
        }
    }
    resume(depth);
    return answer;
}

// ---- V-blank --------------------------------------------------------------------------------------

int Kernel::waitVblankEnd()
{
    using namespace std::chrono;

    // The rate is read each time: FS2 picks PAL through sceGsResetGraph, possibly after the driver is up.
    const bool pal = ee::Kernel::instance().interrupts().videoStandard() == ee::VideoStandard::Pal;
    const auto period = duration_cast<steady_clock::duration>(duration<double>(pal ? 1.0 / 50.0 : 1001.0 / 60000.0));

    steady_clock::time_point due;
    {
        std::lock_guard lock(m_tableLock);
        const auto now = steady_clock::now();
        if (m_nextVblank == steady_clock::time_point{} || now >= m_nextVblank + period)
            m_nextVblank = now + period; // the first wait, or fell behind: start a new grid
        else
            m_nextVblank += period;
        due = m_nextVblank;

        // Once, so a run can tell the driver's tick runs and at what rate.
        if (++m_vblanks == kVblankReport)
            log::info("iop: {} V-blanks in {:.1f} s", kVblankReport,
                      duration<double>(now - m_firstVblank).count());
        if (m_vblanks == 1)
            m_firstVblank = now;
    }

    const int depth = suspend();
    std::this_thread::sleep_until(due);
    resume(depth);
    return 0;
}

} // namespace yamipan::iop
