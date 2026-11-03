#include "yamipan/ee/kernel.h"

#include <algorithm>
#include <string>

#include "sdk/eekernel.h"
#include "yamipan/core/logger.h"
#include "yamipan/ee/interrupts.h"
#include "yamipan/os/thread.h"

namespace yamipan::ee
{
namespace
{

// A 64-bit host's frames are larger than the EE stacks the game sized, so those are not used.
constexpr std::size_t kHostStackBytes = 8 * 1024 * 1024;

constexpr int kErrorResult = -1;
constexpr int kMainThreadPriority = 1;
constexpr int kLowestPriority = 127;

// ReferThreadStatus's status values.
constexpr int kStatusRun = 0x01;
constexpr int kStatusReady = 0x02;
constexpr int kStatusWait = 0x04;
constexpr int kStatusSuspend = 0x08;
constexpr int kStatusDormant = 0x10;

// Unwinds a thread that ended or was terminated from inside any call chain, back to its host thread's
// body, running destructors on the way.
struct ThreadExit
{
};

} // namespace

struct Kernel::Thread
{
    int id = 0;
    void (*entry)(void *) = nullptr;
    void *arg = nullptr;
    ThreadParam param{};
    int priority = 0;
    State state = State::Dormant;
    Wait wait = Wait::None;
    Semaphore *waitSema = nullptr;
    int wakeupCount = 0;
    int interruptDepth = 0;
    bool suspended = false;
    std::shared_ptr<Run> run;
};

struct Kernel::Semaphore
{
    int count = 0;
    int maxCount = 0;
    int initCount = 0;
    std::deque<Thread *> waiters;
};

// Who is calling: null on the interrupt thread and any other host thread.
struct KernelCaller
{
    static inline thread_local void *thread = nullptr;
    static inline thread_local void *run = nullptr;
};

Kernel &Kernel::instance()
{
    static Kernel kernel;
    return kernel;
}

Kernel::Kernel() : m_interrupts(std::make_unique<InterruptController>(*this)) {}

Kernel::~Kernel() = default;

// ---- the switch ---------------------------------------------------------------------------------

Kernel::Lock Kernel::enter()
{
    Lock lock(m_lock);
    if (KernelCaller::thread != nullptr)
        waitTurn(lock);
    return lock;
}

// Parks the calling EE thread until it holds the CPU. A terminated thread unwinds from here.
void Kernel::waitTurn(Lock &lock)
{
    auto *const self = static_cast<Thread *>(KernelCaller::thread);
    auto *const run = static_cast<Run *>(KernelCaller::run);
    run->turn.wait(lock, [&] { return run->killed || m_current == self; });
    if (run->killed)
    {
        lock.unlock();
        throw ThreadExit{};
    }
}

Kernel::Thread *Kernel::mostUrgentReady() const
{
    for (const std::deque<Thread *> &queue : m_ready)
    {
        if (!queue.empty())
            return queue.front();
    }
    return nullptr;
}

// Gives the CPU to the most urgent ready thread, or to nobody until an interrupt readies one.
void Kernel::dispatch()
{
    m_current = mostUrgentReady();
    if (m_current == nullptr)
    {
        m_interruptGate.notify_all();
        return;
    }
    m_ready[m_current->priority].pop_front();
    m_current->state = State::Running;
    m_current->run->turn.notify_one();
    m_interruptGate.notify_all();
}

void Kernel::block(Lock &lock)
{
    dispatch();
    waitTurn(lock);
}

void Kernel::makeReady(Thread &thread)
{
    thread.state = State::Ready;
    thread.wait = Wait::None;
    thread.waitSema = nullptr;
    m_ready[thread.priority].push_back(&thread);
}

void Kernel::removeFromReady(Thread &thread)
{
    std::erase(m_ready[thread.priority], &thread);
}

// The end of a wait; a thread suspended meanwhile stays out of the ready queue until it is resumed.
void Kernel::wake(Thread &thread)
{
    thread.wait = Wait::None;
    thread.waitSema = nullptr;
    if (thread.suspended)
    {
        thread.state = State::Dormant == thread.state ? State::Dormant : State::Waiting;
        return;
    }
    makeReady(thread);
}

// After anything that may have readied a thread: a more urgent one takes the CPU. The running thread
// stays first in its queue, as on the EE. Called on the interrupt thread, this only moves the token;
// the preempted thread parks at its next kernel call.
void Kernel::preempt(Lock &lock)
{
    Thread *const urgent = mostUrgentReady();
    if (urgent == nullptr)
        return;
    if (m_current == nullptr)
    {
        dispatch();
        return;
    }
    if (urgent->priority >= m_current->priority)
        return;

    Thread *const previous = m_current;
    previous->state = State::Ready;
    m_ready[previous->priority].push_front(previous);
    dispatch();
    if (KernelCaller::thread == previous)
        waitTurn(lock);
}

// ---- threads ------------------------------------------------------------------------------------

Kernel::Thread *Kernel::findThread(int id)
{
    if (id == 0)
        return static_cast<Thread *>(KernelCaller::thread);
    const auto found = m_threads.find(id);
    return found != m_threads.end() ? found->second.get() : nullptr;
}

void Kernel::startMainThread(std::function<void()> body)
{
    Lock lock(m_lock);
    auto thread = std::make_shared<Thread>();
    thread->id = m_nextThreadId++;
    thread->priority = kMainThreadPriority;
    thread->param.initPriority = kMainThreadPriority;
    m_threads.emplace(thread->id, thread);
    makeReady(*thread);
    launch(thread, std::move(body));
    preempt(lock);
    lock.unlock();

    // The console's clock runs from power-on: sceGsSyncV works before the first sceGsResetGraph.
    m_interrupts->start();
}

void Kernel::launch(const std::shared_ptr<Thread> &thread, std::function<void()> body)
{
    thread->run = std::make_shared<Run>();
    const std::string name = thread->id == 1 ? "YamiPanGame" : "YamiPanEe" + std::to_string(thread->id);
    os::Thread(name, kHostStackBytes,
               [this, thread, run = thread->run, body = std::move(body)]() mutable {
                   hostThreadBody(std::move(thread), std::move(run), std::move(body));
               })
        .detach();
}

void Kernel::hostThreadBody(std::shared_ptr<Thread> thread, std::shared_ptr<Run> run, std::function<void()> body)
{
    KernelCaller::thread = thread.get();
    KernelCaller::run = run.get();

    try
    {
        {
            Lock lock(m_lock);
            waitTurn(lock);
        }
        body();

        // Returning from the entry function is ExitThread.
        Lock lock(m_lock);
        waitTurn(lock);
        thread->state = State::Dormant;
        dispatch();
    }
    catch (const ThreadExit &)
    {
        // exitThread or terminateThread already took the thread off the books.
    }
}

int Kernel::createThread(const ThreadParam &param)
{
    Lock lock = enter();
    auto thread = std::make_shared<Thread>();
    thread->id = m_nextThreadId++;
    thread->entry = param.entry;
    thread->param = param;
    thread->priority = std::clamp(param.initPriority, 0, kLowestPriority);
    m_threads.emplace(thread->id, std::move(thread));
    return m_nextThreadId - 1;
}

int Kernel::startThread(int id, void *arg)
{
    Lock lock = enter();
    const auto found = m_threads.find(id);
    if (found == m_threads.end() || found->second->state != State::Dormant)
        return kErrorResult;

    Thread &thread = *found->second;
    thread.arg = arg;
    thread.wakeupCount = 0;
    thread.suspended = false;
    thread.interruptDepth = 0;
    thread.priority = std::clamp(thread.param.initPriority, 0, kLowestPriority);
    makeReady(thread);
    launch(found->second, [entry = thread.entry, arg] { entry(arg); });
    preempt(lock);
    return id;
}

int Kernel::terminateThread(int id)
{
    Lock lock = enter();
    Thread *const thread = findThread(id);
    if (thread == nullptr || thread == KernelCaller::thread || thread->state == State::Dormant)
        return kErrorResult;

    if (thread->state == State::Ready)
        removeFromReady(*thread);
    if (thread->waitSema != nullptr)
        std::erase(thread->waitSema->waiters, thread);
    std::erase(m_vblankWaiters, thread);

    thread->state = State::Dormant;
    thread->wait = Wait::None;
    thread->waitSema = nullptr;
    thread->run->killed = true;
    thread->run->turn.notify_one();
    return id;
}

int Kernel::deleteThread(int id)
{
    Lock lock = enter();
    const auto found = m_threads.find(id);
    if (found == m_threads.end() || found->second->state != State::Dormant)
        return kErrorResult;
    m_threads.erase(found);
    return id;
}

void Kernel::exitThread(bool deleteSelf)
{
    Lock lock = enter();
    auto *const self = static_cast<Thread *>(KernelCaller::thread);
    self->state = State::Dormant;
    self->run->killed = true;
    dispatch();
    if (deleteSelf)
    {
        // The host thread's body holds its own reference, so unwinding stays safe.
        m_threads.erase(self->id);
    }
    lock.unlock();
    throw ThreadExit{};
}

int Kernel::suspendThread(int id)
{
    Lock lock = enter();
    Thread *const thread = findThread(id);
    if (thread == nullptr || thread->state == State::Dormant)
        return kErrorResult;

    thread->suspended = true;
    if (thread == KernelCaller::thread)
    {
        thread->state = State::Waiting;
        block(lock);
    }
    else if (thread->state == State::Ready)
    {
        removeFromReady(*thread);
        thread->state = State::Waiting;
    }
    return id;
}

int Kernel::resumeThread(int id)
{
    Lock lock = enter();
    Thread *const thread = findThread(id);
    if (thread == nullptr || !thread->suspended)
        return kErrorResult;

    thread->suspended = false;
    if (thread->wait == Wait::None)
    {
        makeReady(*thread);
        preempt(lock);
    }
    return id;
}

int Kernel::currentThreadId()
{
    Lock lock(m_lock);
    if (const auto *const self = static_cast<Thread *>(KernelCaller::thread))
        return self->id;
    return m_current != nullptr ? m_current->id : 0;
}

int Kernel::sleepThread()
{
    Lock lock = enter();
    auto *const self = static_cast<Thread *>(KernelCaller::thread);
    if (self->wakeupCount > 0)
    {
        self->wakeupCount--;
        return self->id;
    }
    self->state = State::Waiting;
    self->wait = Wait::Sleep;
    block(lock);
    return self->id;
}

int Kernel::wakeupThread(int id)
{
    Lock lock = enter();
    Thread *const thread = findThread(id);
    if (thread == nullptr || thread->state == State::Dormant)
        return kErrorResult;

    if (thread->wait == Wait::Sleep)
    {
        wake(*thread);
        preempt(lock);
    }
    else
    {
        thread->wakeupCount++;
    }
    return id;
}

int Kernel::changePriority(int id, int priority)
{
    Lock lock = enter();
    Thread *const thread = findThread(id);
    if (thread == nullptr || thread->state == State::Dormant)
        return kErrorResult;

    const int previous = thread->priority;
    priority = std::clamp(priority, 0, kLowestPriority);
    if (thread->state == State::Ready)
    {
        removeFromReady(*thread);
        thread->priority = priority;
        m_ready[priority].push_back(thread);
    }
    else
    {
        thread->priority = priority;
    }
    thread->param.currentPriority = priority;
    preempt(lock);
    return previous;
}

int Kernel::rotateReadyQueue(int priority)
{
    Lock lock = enter();
    priority = std::clamp(priority, 0, kLowestPriority);
    std::deque<Thread *> &queue = m_ready[priority];

    if (m_current != nullptr && m_current->priority == priority)
    {
        if (queue.empty())
            return priority;
        Thread *const previous = m_current;
        previous->state = State::Ready;
        queue.push_back(previous);
        dispatch();
        if (KernelCaller::thread == previous)
            waitTurn(lock);
    }
    else if (queue.size() > 1)
    {
        queue.push_back(queue.front());
        queue.pop_front();
    }
    return priority;
}

int Kernel::referThread(int id, ThreadParam &info)
{
    Lock lock = enter();
    const Thread *const thread = findThread(id);
    if (thread == nullptr)
        return kErrorResult;

    info = thread->param;
    info.currentPriority = thread->priority;
    info.wakeupCount = thread->wakeupCount;
    info.waitType = static_cast<int>(thread->wait);
    switch (thread->state)
    {
    case State::Running:
        info.status = kStatusRun;
        break;
    case State::Ready:
        info.status = kStatusReady;
        break;
    case State::Waiting:
        info.status = thread->suspended ? kStatusSuspend : kStatusWait;
        break;
    case State::Dormant:
        info.status = kStatusDormant;
        break;
    }
    return thread->id;
}

int Kernel::delayThread(std::chrono::microseconds delay)
{
    Lock lock = enter();
    auto *const self = static_cast<Thread *>(KernelCaller::thread);
    self->state = State::Waiting;
    self->wait = Wait::Delay;
    m_interrupts->callAfter(delay, [this, id = self->id] { finishDelay(id); });
    block(lock);
    return 0;
}

void Kernel::finishDelay(int threadId)
{
    Lock lock(m_lock);
    if (Thread *const thread = findThread(threadId); thread != nullptr && thread->wait == Wait::Delay)
    {
        wake(*thread);
        preempt(lock);
    }
}

// ---- semaphores ---------------------------------------------------------------------------------

Kernel::Semaphore *Kernel::findSema(int id)
{
    const auto found = m_semaphores.find(id);
    return found != m_semaphores.end() ? found->second.get() : nullptr;
}

int Kernel::createSema(const SemaParam &param)
{
    Lock lock = enter();
    auto semaphore = std::make_unique<Semaphore>();
    semaphore->count = param.initCount;
    semaphore->initCount = param.initCount;
    semaphore->maxCount = param.maxCount;
    const int id = m_nextSemaId++;
    m_semaphores.emplace(id, std::move(semaphore));
    return id;
}

int Kernel::deleteSema(int id)
{
    Lock lock = enter();
    Semaphore *const semaphore = findSema(id);
    if (semaphore == nullptr)
        return kErrorResult;

    // Whoever waits is released; the EE's WaitSema then answers an error.
    for (Thread *const waiter : semaphore->waiters)
        wake(*waiter);
    m_semaphores.erase(id);
    preempt(lock);
    return id;
}

int Kernel::signalSema(int id)
{
    Lock lock = enter();
    Semaphore *const semaphore = findSema(id);
    if (semaphore == nullptr)
        return kErrorResult;

    if (!semaphore->waiters.empty())
    {
        Thread *const waiter = semaphore->waiters.front();
        semaphore->waiters.pop_front();
        wake(*waiter);
        preempt(lock);
    }
    else if (semaphore->maxCount <= 0 || semaphore->count < semaphore->maxCount)
    {
        semaphore->count++;
    }
    return id;
}

int Kernel::waitSema(int id)
{
    Lock lock = enter();
    Semaphore *const semaphore = findSema(id);
    if (semaphore == nullptr)
        return kErrorResult;

    if (semaphore->count > 0)
    {
        semaphore->count--;
        return id;
    }

    auto *const self = static_cast<Thread *>(KernelCaller::thread);
    semaphore->waiters.push_back(self);
    self->state = State::Waiting;
    self->wait = Wait::Semaphore;
    self->waitSema = semaphore;
    block(lock);
    return id;
}

int Kernel::pollSema(int id)
{
    Lock lock = enter();
    Semaphore *const semaphore = findSema(id);
    if (semaphore == nullptr || semaphore->count <= 0)
        return kErrorResult;
    semaphore->count--;
    return id;
}

int Kernel::referSema(int id, SemaParam &info)
{
    Lock lock = enter();
    const Semaphore *const semaphore = findSema(id);
    if (semaphore == nullptr)
        return kErrorResult;

    info.currentCount = semaphore->count;
    info.maxCount = semaphore->maxCount;
    info.initCount = semaphore->initCount;
    info.numWaitThreads = static_cast<int>(semaphore->waiters.size());
    return id;
}

// ---- interrupts ---------------------------------------------------------------------------------

int Kernel::disableInterrupts()
{
    Lock lock = enter();
    auto *const self = static_cast<Thread *>(KernelCaller::thread);
    if (self == nullptr)
        return 0; // a handler: interrupts are already off

    // A handler still running would be frozen under this thread on the EE: let it finish first.
    m_interruptGate.wait(lock, [this] { return !m_inInterrupt; });
    if (m_current != self)
        waitTurn(lock);
    return self->interruptDepth++ == 0 ? 1 : 0;
}

int Kernel::enableInterrupts()
{
    Lock lock(m_lock);
    auto *const self = static_cast<Thread *>(KernelCaller::thread);
    if (self == nullptr)
        return 0;
    if (self->interruptDepth > 0 && --self->interruptDepth == 0)
        m_interruptGate.notify_all();
    return 1;
}

void Kernel::waitInterruptsEnabled(Lock &lock)
{
    m_interruptGate.wait(lock, [this] { return m_current == nullptr || m_current->interruptDepth == 0; });
}

void Kernel::waitVBlank()
{
    Lock lock = enter();
    auto *const self = static_cast<Thread *>(KernelCaller::thread);
    if (self == nullptr)
        return;
    m_vblankWaiters.push_back(self);
    self->state = State::Waiting;
    self->wait = Wait::VBlank;
    block(lock);
}

void Kernel::pulseVBlank()
{
    Lock lock(m_lock);
    for (Thread *const waiter : m_vblankWaiters)
        wake(*waiter);
    m_vblankWaiters.clear();
    preempt(lock);
}

} // namespace yamipan::ee
