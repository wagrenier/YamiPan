#include "yamipan/os/timed_waiter.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_timer.h>

namespace yamipan::os
{
namespace
{

using std::chrono::milliseconds;
using std::chrono::nanoseconds;

// The semaphore's timeout can end up to ~1.6 ms late at a 1 ms timer period: it stops this far short of
// the deadline, and precise delays of at most kSlice cover the rest, looking for a wake() between them.
constexpr nanoseconds kMargin = milliseconds(2);
constexpr nanoseconds kSlice = std::chrono::microseconds(250);

} // namespace

TimedWaiter::TimedWaiter()
    : m_wake(SDL_CreateSemaphore(0))
{
    if (m_wake == nullptr)
        throw std::runtime_error("TimedWaiter: no semaphore");
}

TimedWaiter::~TimedWaiter()
{
    SDL_DestroySemaphore(m_wake);
}

void TimedWaiter::waitUntil(Clock::time_point deadline)
{
    for (;;)
    {
        const nanoseconds remaining = deadline - Clock::now();
        if (remaining.count() <= 0)
        {
            SDL_TryWaitSemaphore(m_wake); // a wake() meant for this wait is spent by it
            return;
        }

        const auto bulk = std::chrono::duration_cast<milliseconds>(remaining - kMargin).count();
        if (bulk >= 1)
        {
            if (SDL_WaitSemaphoreTimeout(m_wake, static_cast<Sint32>(std::min<std::int64_t>(bulk, INT32_MAX))))
                return;
            continue;
        }

        if (SDL_TryWaitSemaphore(m_wake))
            return;
        SDL_DelayNS(static_cast<Uint64>(std::min(remaining, kSlice).count()));
    }
}

void TimedWaiter::wake()
{
    // Kept at most once: a second wake() before the wait would only end the next one early for nothing.
    if (SDL_GetSemaphoreValue(m_wake) == 0)
        SDL_SignalSemaphore(m_wake);
}

void TimedWaiter::sleepFor(std::chrono::nanoseconds duration)
{
    // SDL waits on a high-resolution waitable timer on Windows.
    if (duration.count() > 0)
        SDL_DelayNS(static_cast<Uint64>(duration.count()));
}

} // namespace yamipan::os
