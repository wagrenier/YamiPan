// A wait until a deadline that another thread can cut short, precise to about a millisecond.
//
// std::condition_variable's timed waits and std::this_thread::sleep_for do not do: under MinGW's libstdc++
// they land on the 15.6 ms system tick whatever timer period the process asks for (measured 2026-11-07: a
// 1 ms sleep_for takes 15.5 ms, a wait_until for a deadline 20 ms ahead wakes 8 ms late on average and up to
// 16 ms, with SDL's timeBeginPeriod(1) in force; Win32's Sleep(1) in the same process takes 1.6 ms). The EE's
// clock cannot live with that: the field clock would deliver its 50 Hz unevenly, so a frame that needs two
// fields sometimes gets 25 ms instead of 40. On Windows this waits on a high-resolution waitable timer.
#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace yamipan::os
{

class TimedWaiter
{
public:
    using Clock = std::chrono::steady_clock;

    TimedWaiter();
    ~TimedWaiter();

    TimedWaiter(const TimedWaiter &) = delete;
    TimedWaiter &operator=(const TimedWaiter &) = delete;

    // Blocks until `deadline` or a wake(), whichever is first. A wake() with nobody waiting is kept for the
    // next wait, so a deadline computed just before one moved earlier is never slept through.
    void waitUntil(Clock::time_point deadline);
    void wake();

    // A plain precise sleep, for the calling thread.
    static void sleepFor(std::chrono::nanoseconds duration);

private:
#ifdef _WIN32
    void *m_timer = nullptr; // HANDLE
    void *m_event = nullptr; // HANDLE, auto-reset: one wake() releases one wait
#else
    std::mutex m_lock;
    std::condition_variable m_changed;
    bool m_woken = false;
#endif
};

} // namespace yamipan::os
