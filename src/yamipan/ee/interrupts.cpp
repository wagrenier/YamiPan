#include "yamipan/ee/interrupts.h"

#include <algorithm>
#include <utility>

#include "yamipan/ee/kernel.h"
#include "yamipan/os/yamipan_time.h"

namespace yamipan::ee
{
namespace
{

constexpr std::size_t kInterruptStackBytes = 1024 * 1024;

constexpr int kCauseVBlankStart = 2;
constexpr int kCauseVBlankEnd = 3;

constexpr double kBusClock = 147'456'000.0;
constexpr double kNtscFieldRate = 59.94;
constexpr double kPalFieldRate = 50.0;
constexpr double kGameRate60FieldRate = 60.0;
constexpr double kNtscHsyncRate = 15734.0;
constexpr double kPalHsyncRate = 15625.0;

constexpr std::uint32_t kTimerMask = 0xffff;
constexpr std::uint32_t kClockSourceMask = 3;

InterruptController::Clock::duration seconds(double value)
{
    return std::chrono::duration_cast<InterruptController::Clock::duration>(std::chrono::duration<double>(value));
}

} // namespace

InterruptController::InterruptController(Kernel &kernel) : m_kernel(kernel)
{
    const auto now = Clock::now();
    for (Timer &timer : m_timers)
        timer.base = now;
}

InterruptController::~InterruptController()
{
    {
        std::scoped_lock lock(m_lock);
        m_stopping = true;
    }
    m_wake.wake();
}

void InterruptController::start()
{
    std::scoped_lock lock(m_lock);
    if (m_thread == nullptr)
        m_thread = std::make_unique<os::Thread>("YamiPanIntc", kInterruptStackBytes, [this] { run(); });
}

void InterruptController::setVideoStandard(VideoStandard standard)
{
    m_standard.store(standard);
    start();
}

int InterruptController::addHandler(int cause, Handler handler, int next)
{
    if (cause < 0 || cause >= kCauseCount)
        return -1;

    std::unique_lock lock(m_lock);
    const int id = m_nextHandlerId++;
    std::vector<Registered> &handlers = m_handlers[cause];
    if (next < 0)
        handlers.insert(handlers.begin(), {id, handler});
    else
        handlers.push_back({id, handler});
    lock.unlock();

    start();
    return id;
}

int InterruptController::removeHandler(int cause, int handlerId)
{
    if (cause < 0 || cause >= kCauseCount)
        return -1;

    std::scoped_lock lock(m_lock);
    std::erase_if(m_handlers[cause], [handlerId](const Registered &entry) { return entry.id == handlerId; });
    return 0;
}

void InterruptController::setCauseEnabled(int cause, bool enabled)
{
    if (cause < 0 || cause >= kCauseCount)
        return;
    std::scoped_lock lock(m_lock);
    m_enabled[cause] = enabled;
}

InterruptController::VBlankCallback InterruptController::setVBlankCallback(VBlankCallback callback)
{
    std::scoped_lock lock(m_lock);
    return std::exchange(m_vblankCallback, callback);
}

int InterruptController::setAlarm(std::uint16_t hsyncTicks, AlarmHandler handler, void *arg)
{
    std::unique_lock lock(m_lock);
    const int id = m_nextAlarmId++;
    m_timed.push_back({Clock::now() + seconds(hsyncTicks / hsyncRate()), id, handler, arg, hsyncTicks, {}});
    lock.unlock();

    m_wake.wake();
    start();
    return id;
}

int InterruptController::releaseAlarm(int id)
{
    std::scoped_lock lock(m_lock);
    return std::erase_if(m_timed, [id](const Timed &entry) { return entry.id == id; }) != 0 ? id : -1;
}

void InterruptController::callAfter(std::chrono::microseconds delay, std::function<void()> work)
{
    std::unique_lock lock(m_lock);
    m_timed.push_back({Clock::now() + delay, 0, nullptr, nullptr, 0, std::move(work)});
    lock.unlock();

    m_wake.wake();
    start();
}

void InterruptController::setFieldListener(std::function<void()> listener)
{
    std::scoped_lock lock(m_lock);
    m_fieldListener = std::move(listener);
}

double InterruptController::hsyncRate() const
{
    return m_standard.load() == VideoStandard::Pal ? kPalHsyncRate : kNtscHsyncRate;
}

double InterruptController::timerRate(const Timer &timer) const
{
    switch (timer.mode & kClockSourceMask)
    {
    case 0:
        return kBusClock;
    case 1:
        return kBusClock / 16.0;
    case 2:
        return kBusClock / 256.0;
    default:
        return hsyncRate();
    }
}

std::uint32_t InterruptController::readTimer(int timer) const
{
    std::scoped_lock lock(m_lock);
    const Timer &entry = m_timers[timer & 1];
    const std::chrono::duration<double> elapsed = Clock::now() - entry.base;
    return (entry.offset + static_cast<std::uint32_t>(elapsed.count() * timerRate(entry))) & kTimerMask;
}

void InterruptController::writeTimer(int timer, std::uint32_t value)
{
    std::scoped_lock lock(m_lock);
    Timer &entry = m_timers[timer & 1];
    entry.base = Clock::now();
    entry.offset = value & kTimerMask;
}

void InterruptController::setTimerMode(int timer, std::uint32_t mode)
{
    std::scoped_lock lock(m_lock);
    m_timers[timer & 1].mode = mode;
}

void InterruptController::raise(int cause)
{
    std::unique_lock lock(m_lock);
    if (!m_enabled[cause])
        return;
    const std::vector<Registered> handlers = m_handlers[cause];
    lock.unlock();

    for (const Registered &entry : handlers)
    {
        if (entry.handler(cause) == 0)
            break;
    }
}

void InterruptController::run()
{
    Clock::time_point nextField = Clock::now();

    for (;;)
    {
        Clock::time_point wakeAt;
        {
            std::scoped_lock lock(m_lock);
            if (m_stopping)
                return;
            const auto earliest = std::ranges::min_element(m_timed, {}, &Timed::when);
            wakeAt = earliest != m_timed.end() ? std::min(earliest->when, nextField) : nextField;
        }
        // An alarm set meanwhile wakes this early; the loop then looks again.
        m_wake.waitUntil(wakeAt);

        std::unique_lock lock(m_lock);
        if (m_stopping)
            return;

        const Clock::time_point now = Clock::now();
        std::vector<Timed> due;
        for (auto entry = m_timed.begin(); entry != m_timed.end();)
        {
            if (entry->when <= now)
            {
                due.push_back(std::move(*entry));
                entry = m_timed.erase(entry);
            }
            else
            {
                ++entry;
            }
        }
        std::ranges::sort(due, {}, &Timed::when);

        const bool fieldDue = now >= nextField;
        const VBlankCallback vblankCallback = m_vblankCallback;
        const std::function<void()> fieldListener = m_fieldListener;
        lock.unlock();

        if (due.empty() && !fieldDue)
            continue;

        // Handlers run only while the running thread has interrupts on, and hold further DIntr calls
        // off for as long as they take: a frozen thread, as far as anybody can tell.
        {
            std::unique_lock kernelLock(m_kernel.m_lock);
            m_kernel.waitInterruptsEnabled(kernelLock);
            m_kernel.m_inInterrupt = true;
        }

        for (Timed &entry : due)
        {
            if (entry.alarm != nullptr)
                entry.alarm(entry.id, entry.ticks, entry.arg);
            else
                entry.work();
        }

        if (fieldDue)
        {
            // game_rate 60 (roadmap-framerate.md "Native 60 Hz") runs the EE's fields at 60 Hz whatever the game chose;
            // the IOP keeps its own V-blank (iop/kernel.cpp), so the sound driver's ticks stay the video standard's.
            const double rate = YamiPan_GameRate() == 60 ? kGameRate60FieldRate
                                : m_standard.load() == VideoStandard::Pal ? kPalFieldRate : kNtscFieldRate;
            m_lastFieldTime.store(nextField.time_since_epoch().count());
            m_fields.fetch_add(1);
            raise(kCauseVBlankStart);
            if (vblankCallback != nullptr)
                vblankCallback(0);
            m_kernel.pulseVBlank();
            raise(kCauseVBlankEnd);
            if (fieldListener)
                fieldListener();

            // Never bursts to catch up: a stall loses fields, as a stopped console would.
            nextField += seconds(1.0 / rate);
            if (nextField < now)
                nextField = now + seconds(1.0 / rate);
        }

        {
            std::scoped_lock kernelLock(m_kernel.m_lock);
            m_kernel.m_inInterrupt = false;
        }
        m_kernel.m_interruptGate.notify_all();
    }
}

} // namespace yamipan::ee
