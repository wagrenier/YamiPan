#include "yamipan/core/crash_report.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <version>

#if defined(__cpp_lib_stacktrace)
#include <stacktrace>
#endif

namespace yamipan::crash
{
namespace
{

constexpr int kMaxFrames = 48;

std::atomic_flag s_reporting; // a fault while reporting one is not reported again

const char *signalName(int signal)
{
    switch (signal)
    {
    case SIGSEGV: return "bad memory access";
    case SIGILL: return "illegal instruction";
    case SIGFPE: return "arithmetic fault";
    case SIGABRT: return "abort";
    default: return "signal";
    }
}

void printStack()
{
#if defined(__cpp_lib_stacktrace)
    int index = 0;
    for (const std::stacktrace_entry &entry : std::stacktrace::current(1, kMaxFrames))
    {
        const std::string description = entry.description();
        const std::string file = entry.source_file();
        if (file.empty())
            std::printf("[crash] frame %2d %s\n", index, description.c_str());
        else
            std::printf("[crash] frame %2d %s %s:%u\n", index, description.c_str(), file.c_str(),
                        static_cast<unsigned>(entry.source_line()));
        index++;
    }
#else
    std::printf("[crash] no stack: this standard library has no <stacktrace>\n");
#endif
}

void onSignal(int signal)
{
    if (!s_reporting.test_and_set())
    {
        std::printf("[crash] %s (signal %d)\n", signalName(signal), signal);
        printStack();
        std::fflush(stdout);
    }
    // The default action from here: a fault runs its instruction again and dies of it as it would have
    // (a debugger or a core dump sees the real one), an abort goes on to terminate.
    std::signal(signal, SIG_DFL);
}

[[noreturn]] void onTerminate()
{
    if (!s_reporting.test_and_set())
    {
        const char *what = "not a std::exception";
        try
        {
            if (const std::exception_ptr exception = std::current_exception())
                std::rethrow_exception(exception);
            what = "no exception (std::terminate called)";
        }
        catch (const std::exception &error)
        {
            std::printf("[crash] uncaught exception: %s\n", error.what());
            what = nullptr;
        }
        catch (...)
        {
        }
        if (what != nullptr)
            std::printf("[crash] uncaught exception: %s\n", what);
        printStack();
        std::fflush(stdout);
    }
    std::abort();
}

} // namespace

void install()
{
    std::set_terminate(onTerminate);
    std::signal(SIGABRT, onSignal);
    installThread();
}

void installThread()
{
    std::signal(SIGSEGV, onSignal);
    std::signal(SIGILL, onSignal);
    std::signal(SIGFPE, onSignal);
}

} // namespace yamipan::crash
