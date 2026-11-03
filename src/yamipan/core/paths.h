// Where things live: the user directory (settings, log, memory cards), the install directory (shaders)
// and the data folder, the disc's files, which the port can do nothing without.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace yamipan
{

class Paths
{
public:
    Paths();

    const std::filesystem::path &userDirectory() const { return m_user; }
    std::filesystem::path configFile() const { return m_user / "yamipan.ini"; }
    std::filesystem::path logFile() const { return m_user / "yamipan.log"; }
    std::filesystem::path memoryCardDirectory() const { return m_user / "memcard"; }

    // A file shipped next to the executable (resources/shaders/...).
    std::filesystem::path resource(std::string_view relative) const;

    // The folder the game's files are read from; empty until one is found or chosen.
    const std::filesystem::path &dataFolder() const { return m_data; }
    bool hasDataFolder() const { return !m_data.empty(); }

    // What makes a folder the data folder: the archives the game opens at boot.
    static bool isDataFolder(const std::filesystem::path &folder);

    // Takes `folder` if it holds the game's files. Answers whether it did.
    bool setDataFolder(const std::filesystem::path &folder);

    // YAMIPAN_DATA_DIR, then the configured folder, then beside the executable and the working
    // directory. A folder found is remembered; nothing found leaves hasDataFolder() false.
    bool discoverDataFolder(std::string_view configured);

private:
    std::filesystem::path m_user;
    std::filesystem::path m_base;
    std::filesystem::path m_data;
};

// UTF-8 is what SDL and the settings file speak; std::filesystem must be told.
std::filesystem::path pathFromUtf8(std::string_view text);
std::string pathToUtf8(const std::filesystem::path &path);

} // namespace yamipan
