// The EE as FS2 sees it. The game is written for one CPU: its cooperative thread slots, rx's round-robin
// by priority changes and its job worker all assume that exactly one thread runs at a time, the most
// urgent one that is ready. So each EE thread is a host thread, but only the one holding the CPU runs game
// code; the rest are parked on their own condition variable.
//
// A kernel call is the switch point. What a host cannot do is stop a running thread from outside, so an
// interrupt that wakes a more urgent thread hands the CPU over at once and the preempted thread parks at
// its next kernel call. Until then the two overlap, which game code that was correct on the EE (where an
// interrupt can strike between any two instructions) has to tolerate anyway; sections guarded by DIntr are
// never interrupted, exactly as on the console.
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

struct SemaParam;
struct ThreadParam;

namespace yamipan::ee
{

class InterruptController;

class Kernel
{
public:
    static Kernel &instance();

    Kernel();
    ~Kernel();

    Kernel(const Kernel &) = delete;
    Kernel &operator=(const Kernel &) = delete;

    // Starts `body` as the EE's main thread, on the host thread named YamiPanGame.
    void startMainThread(std::function<void()> body);

    // ---- threads ----------------------------------------------------------------------------
    int createThread(const ThreadParam &param);
    int startThread(int id, void *arg);
    int terminateThread(int id);
    int deleteThread(int id);
    int suspendThread(int id);
    int resumeThread(int id);
    int currentThreadId();
    int sleepThread();
    int wakeupThread(int id);
    int changePriority(int id, int priority);
    int rotateReadyQueue(int priority);
    int referThread(int id, ThreadParam &info);
    int delayThread(std::chrono::microseconds delay);
    [[noreturn]] void exitThread(bool deleteSelf);

    // ---- semaphores -------------------------------------------------------------------------
    int createSema(const SemaParam &param);
    int deleteSema(int id);
    int signalSema(int id);
    int waitSema(int id);
    int pollSema(int id);
    int referSema(int id, SemaParam &info);

    // ---- interrupts -------------------------------------------------------------------------
    // DIntr / EIntr: a depth per thread, as the status register is on the EE, so a thread that blocks
    // inside the pair does not hold the next one's interrupts off.
    int disableInterrupts();
    int enableInterrupts();

    // sceGsSyncV: blocks until the next vblank.
    void waitVBlank();

    InterruptController &interrupts() { return *m_interrupts; }

private:
    friend class InterruptController;

    struct Thread;
    struct Semaphore;

    enum class State
    {
        Dormant,
        Ready,
        Running,
        Waiting,
    };

    enum class Wait
    {
        None,
        Sleep,
        Semaphore,
        Delay,
        VBlank,
    };

    // One start of a thread, so that a thread restarted while its old host thread still unwinds
    // never mixes the two.
    struct Run
    {
        std::condition_variable turn;
        bool killed = false;
    };

    using Lock = std::unique_lock<std::mutex>;

    Lock enter();
    void waitTurn(Lock &lock);
    void dispatch();
    void block(Lock &lock);
    void preempt(Lock &lock);
    void makeReady(Thread &thread);
    void removeFromReady(Thread &thread);
    void wake(Thread &thread);
    Thread *mostUrgentReady() const;
    Thread *findThread(int id);
    Semaphore *findSema(int id);
    void launch(const std::shared_ptr<Thread> &thread, std::function<void()> body);
    void hostThreadBody(std::shared_ptr<Thread> thread, std::shared_ptr<Run> run, std::function<void()> body);

    // For the interrupt thread, under an Interrupts::Scope.
    void waitInterruptsEnabled(Lock &lock);
    void pulseVBlank();
    void finishDelay(int threadId);

    std::mutex m_lock;
    std::condition_variable m_interruptGate;
    std::map<int, std::shared_ptr<Thread>> m_threads;
    std::map<int, std::unique_ptr<Semaphore>> m_semaphores;
    std::array<std::deque<Thread *>, 128> m_ready;
    std::vector<Thread *> m_vblankWaiters;
    Thread *m_current = nullptr; // null: every thread waits, an interrupt will ready one
    bool m_inInterrupt = false;
    int m_nextThreadId = 1;
    int m_nextSemaId = 1;
    std::unique_ptr<InterruptController> m_interrupts;
};

} // namespace yamipan::ee
