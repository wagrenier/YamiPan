// The log: one file in the user directory (yamipan.log, older runs rotated away) plus stdout, where the
// tools read the [W] and [E] lines. Every record is flushed as it is written, because the interesting log
// is always the one of a process that died badly, and SDL's own messages are captured into the same
// stream. "Send me yamipan.log" is the whole bug-report instruction.
#pragma once

#include <filesystem>
#include <format>
#include <string_view>
#include <utility>

namespace yamipan::log
{

enum class Level
{
    Debug,
    Info,
    Warning,
    Error,
};

// Opens the file and installs the SDL hook. Records written earlier reach stdout only.
void open(const std::filesystem::path &file);

void write(Level level, std::string_view text);

template <class... Args>
void debug(std::format_string<Args...> format, Args &&...args)
{
    write(Level::Debug, std::format(format, std::forward<Args>(args)...));
}

template <class... Args>
void info(std::format_string<Args...> format, Args &&...args)
{
    write(Level::Info, std::format(format, std::forward<Args>(args)...));
}

template <class... Args>
void warning(std::format_string<Args...> format, Args &&...args)
{
    write(Level::Warning, std::format(format, std::forward<Args>(args)...));
}

template <class... Args>
void error(std::format_string<Args...> format, Args &&...args)
{
    write(Level::Error, std::format(format, std::forward<Args>(args)...));
}

} // namespace yamipan::log
