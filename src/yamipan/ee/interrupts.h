// Everything on the EE that happens by itself: the vblank, alarms, the timers. One host thread keeps the
// time and runs handlers in "interrupt context", which here means: never inside a DIntr section of the
// running thread.
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "yamipan/os/thread.h"
#include "yamipan/os/timed_waiter.h"

namespace yamipan::ee
{

class Kernel;

enum class VideoStandard
{
    Ntsc,
    Pal,
};

class InterruptController
{
public:
    using Clock = std::chrono::steady_clock;
    using Handler = int (*)(int cause);
    using AlarmHandler = void (*)(int id, std::uint16_t ticks, void *arg);
    using VBlankCallback = int (*)(int);

    explicit InterruptController(Kernel &kernel);
    ~InterruptController();

    // The clock starts with the first handler or the first sceGsResetGraph.
    void start();
    void setVideoStandard(VideoStandard standard);
    VideoStandard videoStandard() const { return m_standard.load(); }

    int addHandler(int cause, Handler handler, int next);
    int removeHandler(int cause, int handlerId);
    void setCauseEnabled(int cause, bool enabled);

    // sceGsSyncVCallback: answers the one installed before.
    VBlankCallback setVBlankCallback(VBlankCallback callback);

    int setAlarm(std::uint16_t hsyncTicks, AlarmHandler handler, void *arg);
    int releaseAlarm(int id);

    // Runs `work` on this thread after `delay`, for the kernel's DelayThread.
    void callAfter(std::chrono::microseconds delay, std::function<void()> work);

    // Fields since the start: sceGsSyncV's odd / even answer.
    std::uint64_t fieldCount() const { return m_fields.load(); }

    // The EE timers, T0 and T1. They run from the clock instead of counting.
    std::uint32_t readTimer(int timer) const;
    void writeTimer(int timer, std::uint32_t value);
    void setTimerMode(int timer, std::uint32_t mode);

    // H-sync lines per second of the current standard.
    double hsyncRate() const;

    // Periodic host work on the field clock, for the vsync the pacing and tools hang off.
    void setFieldListener(std::function<void()> listener);

private:
    struct Timed
    {
        Clock::time_point when;
        int id;
        AlarmHandler alarm;
        void *arg;
        std::uint16_t ticks;
        std::function<void()> work;
    };

    struct Registered
    {
        int id;
        Handler handler;
    };

    struct Timer
    {
        Clock::time_point base;
        std::uint32_t mode = 3; // CLKS: H-blanks, what FS2's timers count
        std::uint32_t offset = 0;
    };

    static constexpr int kCauseCount = 16;

    void run();
    void raise(int cause);
    double timerRate(const Timer &timer) const;

    Kernel &m_kernel;
    mutable std::mutex m_lock;
    os::TimedWaiter m_wake; // not a condition variable: its timed waits miss the field by up to 16 ms
    std::array<std::vector<Registered>, kCauseCount> m_handlers;
    std::array<bool, kCauseCount> m_enabled{};
    std::vector<Timed> m_timed;
    std::array<Timer, 2> m_timers;
    std::function<void()> m_fieldListener;
    VBlankCallback m_vblankCallback = nullptr;
    std::atomic<VideoStandard> m_standard{VideoStandard::Ntsc};
    std::atomic<std::uint64_t> m_fields{0};
    int m_nextHandlerId = 1;
    int m_nextAlarmId = 1;
    std::unique_ptr<os::Thread> m_thread;
    bool m_stopping = false;
};

} // namespace yamipan::ee
