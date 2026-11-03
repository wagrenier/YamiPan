#include "yamipan/core/logger.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>

#include <SDL3/SDL_log.h>

#ifndef YAMIPAN_BUILD_COMMIT
#define YAMIPAN_BUILD_COMMIT "unknown"
#endif

namespace yamipan::log
{
namespace
{

namespace fs = std::filesystem;

constexpr int kRotatedLogs = 3;

class Logger
{
public:
    void open(const fs::path &file)
    {
        std::scoped_lock lock(m_lock);
        rotate(file);
        m_file.open(file, std::ios::out | std::ios::trunc);
    }

    void write(Level level, std::string_view text)
    {
        const char *const tag = levelTag(level);
        const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());

        std::scoped_lock lock(m_lock);
        if (level != Level::Debug || m_verbose)
        {
            std::fprintf(stdout, "%s%.*s\n", tag, static_cast<int>(text.size()), text.data());
            std::fflush(stdout);
        }
        if (m_file.is_open())
            m_file << std::format("{:%H:%M:%S} {}{}\n", now, tag, text) << std::flush;
    }

private:
    static const char *levelTag(Level level)
    {
        switch (level)
        {
        case Level::Warning:
            return "[W] ";
        case Level::Error:
            return "[E] ";
        default:
            return "";
        }
    }

    // yamipan.log becomes yamipan.1.log and so on, so a crash's log survives the next launch.
    static void rotate(const fs::path &file)
    {
        const auto numbered = [&file](int index) {
            fs::path path = file;
            path.replace_extension(std::format(".{}.log", index));
            return path;
        };

        std::error_code ignored;
        fs::remove(numbered(kRotatedLogs), ignored);
        for (int index = kRotatedLogs - 1; index > 0; index--)
            fs::rename(numbered(index), numbered(index + 1), ignored);
        fs::rename(file, numbered(1), ignored);
    }

    std::mutex m_lock;
    std::ofstream m_file;
    bool m_verbose = std::getenv("YAMIPAN_LOG_DEBUG") != nullptr;
};

Logger &logger()
{
    static Logger instance;
    return instance;
}

Level levelOf(SDL_LogPriority priority)
{
    switch (priority)
    {
    case SDL_LOG_PRIORITY_CRITICAL:
    case SDL_LOG_PRIORITY_ERROR:
        return Level::Error;
    case SDL_LOG_PRIORITY_WARN:
        return Level::Warning;
    case SDL_LOG_PRIORITY_INFO:
        return Level::Info;
    default:
        return Level::Debug;
    }
}

void SDLCALL onSdlLog(void *, int, SDL_LogPriority priority, const char *message)
{
    logger().write(levelOf(priority), std::format("SDL: {}", message));
}

} // namespace

void open(const fs::path &file)
{
    logger().open(file);
    SDL_SetLogOutputFunction(onSdlLog, nullptr);
    info("YamiPan, build {} ({} {})", YAMIPAN_BUILD_COMMIT, __DATE__, __TIME__);
}

void write(Level level, std::string_view text)
{
    logger().write(level, text);
}

} // namespace yamipan::log
