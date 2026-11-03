#include "yamipan/core/paths.h"

#include <array>
#include <cstdlib>
#include <system_error>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include "yamipan/core/logger.h"

namespace yamipan
{
namespace fs = std::filesystem;

fs::path pathFromUtf8(std::string_view text)
{
    return fs::path(std::u8string_view(reinterpret_cast<const char8_t *>(text.data()), text.size()));
}

std::string pathToUtf8(const fs::path &path)
{
    const std::u8string text = path.generic_u8string();
    return std::string(text.begin(), text.end());
}

Paths::Paths()
{
    if (const char *base = SDL_GetBasePath())
        m_base = pathFromUtf8(base);

    if (char *const user = SDL_GetPrefPath("Mikompilation", "YamiPan"))
    {
        m_user = pathFromUtf8(user);
        SDL_free(user);
    }
    else
    {
        m_user = m_base;
    }

    std::error_code ignored;
    fs::create_directories(memoryCardDirectory(), ignored);
}

fs::path Paths::resource(std::string_view relative) const
{
    return m_base / pathFromUtf8(relative);
}

bool Paths::isDataFolder(const fs::path &folder)
{
    std::error_code ignored;
    return fs::exists(folder / "SPK.ROM", ignored) && fs::exists(folder / "ROM.000", ignored);
}

bool Paths::setDataFolder(const fs::path &folder)
{
    if (!isDataFolder(folder))
        return false;
    m_data = folder;
    log::info("Data folder: {}", pathToUtf8(m_data));
    return true;
}

bool Paths::discoverDataFolder(std::string_view configured)
{
    if (const char *const environment = std::getenv("YAMIPAN_DATA_DIR");
        environment != nullptr && setDataFolder(pathFromUtf8(environment)))
    {
        return true;
    }
    if (!configured.empty() && setDataFolder(pathFromUtf8(configured)))
        return true;

    std::error_code ignored;
    const fs::path working = fs::current_path(ignored);
    for (const fs::path &candidate : std::array{m_base, m_base / "data", working, working / "data"})
    {
        if (setDataFolder(candidate))
            return true;
    }

    log::warning("No data folder: looked for SPK.ROM and ROM.000");
    return false;
}

} // namespace yamipan
