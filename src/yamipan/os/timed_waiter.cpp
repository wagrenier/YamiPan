#include "yamipan/os/timed_waiter.h"

#include <stdexcept>

#include <SDL3/SDL_timer.h>

#ifdef _WIN32
#include <windows.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x2 // Windows 10 1803
#endif
#endif

namespace yamipan::os
{

#ifdef _WIN32

TimedWaiter::TimedWaiter()
{
    m_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (m_timer == nullptr) // before Windows 10 1803: a plain timer, at the system timer's period
        m_timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (m_timer == nullptr || m_event == nullptr)
        throw std::runtime_error("TimedWaiter: no waitable timer or event");
}

TimedWaiter::~TimedWaiter()
{
    CloseHandle(m_event);
    CloseHandle(m_timer);
}

void TimedWaiter::waitUntil(Clock::time_point deadline)
{
    const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - Clock::now());
    if (remaining.count() <= 0)
    {
        WaitForSingleObject(m_event, 0); // a wake() meant for this wait is spent by it
        return;
    }

    // Negative: relative, in 100 ns units, rounded up so the deadline is never early.
    LARGE_INTEGER due;
    due.QuadPart = -((remaining.count() + 99) / 100);
    const HANDLE handles[] = {m_event, m_timer};
    if (!SetWaitableTimerEx(m_timer, &due, 0, nullptr, nullptr, nullptr, 0))
    {
        WaitForSingleObject(m_event, static_cast<DWORD>((remaining.count() + 999'999) / 1'000'000));
        return;
    }
    WaitForMultipleObjects(2, handles, FALSE, INFINITE);
}

void TimedWaiter::wake()
{
    SetEvent(m_event);
}

#else

TimedWaiter::TimedWaiter() = default;
TimedWaiter::~TimedWaiter() = default;

void TimedWaiter::waitUntil(Clock::time_point deadline)
{
    std::unique_lock lock(m_lock);
    m_changed.wait_until(lock, deadline, [this] { return m_woken; });
    m_woken = false;
}

void TimedWaiter::wake()
{
    {
        std::scoped_lock lock(m_lock);
        m_woken = true;
    }
    m_changed.notify_one();
}

#endif

void TimedWaiter::sleepFor(std::chrono::nanoseconds duration)
{
    // SDL waits on a high-resolution waitable timer on Windows, as above.
    if (duration.count() > 0)
        SDL_DelayNS(static_cast<Uint64>(duration.count()));
}

} // namespace yamipan::os
