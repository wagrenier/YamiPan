#include "yamipan/io/host_files.h"

#include <algorithm>
#include <string>

#include "yamipan/core/application.h"
#include "yamipan/core/logger.h"

namespace yamipan::io
{
namespace
{

constexpr int kNoSuchFile = -2; // the IOP's -ENOENT

} // namespace

HostFiles &HostFiles::instance()
{
    static HostFiles files;
    return files;
}

std::filesystem::path HostFiles::resolve(std::string_view eePath)
{
    std::string relative(eePath);
    if (const auto colon = relative.find(':'); colon != std::string::npos)
        relative.erase(0, colon + 1);
    if (const auto version = relative.find(';'); version != std::string::npos)
        relative.erase(version);
    std::ranges::replace(relative, '\\', '/');
    while (!relative.empty() && relative.front() == '/')
        relative.erase(0, 1);

    return Application::instance().paths().dataFolder() / pathFromUtf8(relative);
}

std::FILE *HostFiles::find(int descriptor) const
{
    const auto index = static_cast<std::size_t>(descriptor);
    return descriptor >= 0 && index < m_files.size() ? m_files[index].get() : nullptr;
}

int HostFiles::open(std::string_view eePath, bool write, bool create, bool truncate)
{
    const std::filesystem::path path = resolve(eePath);
    const char *const mode = !write ? "rb" : truncate ? "wb" : create ? "ab+" : "rb+";

#ifdef _WIN32
    File file(_wfopen(path.c_str(), std::filesystem::path(mode).c_str()));
#else
    File file(std::fopen(path.c_str(), mode));
#endif
    if (file == nullptr)
        return kNoSuchFile;

    std::scoped_lock lock(m_lock);
    const auto slot = std::ranges::find_if(m_files, [](const File &entry) { return entry == nullptr; });
    if (slot != m_files.end())
    {
        *slot = std::move(file);
        return static_cast<int>(slot - m_files.begin());
    }
    m_files.push_back(std::move(file));
    return static_cast<int>(m_files.size() - 1);
}

int HostFiles::close(int descriptor)
{
    std::scoped_lock lock(m_lock);
    if (find(descriptor) == nullptr)
        return -1;
    m_files[static_cast<std::size_t>(descriptor)].reset();
    return 0;
}

int HostFiles::read(int descriptor, void *buffer, std::size_t size)
{
    std::scoped_lock lock(m_lock);
    std::FILE *const file = find(descriptor);
    return file != nullptr ? static_cast<int>(std::fread(buffer, 1, size, file)) : -1;
}

int HostFiles::write(int descriptor, const void *buffer, std::size_t size)
{
    std::scoped_lock lock(m_lock);
    std::FILE *const file = find(descriptor);
    return file != nullptr ? static_cast<int>(std::fwrite(buffer, 1, size, file)) : -1;
}

int HostFiles::seek(int descriptor, long offset, Seek whence)
{
    std::scoped_lock lock(m_lock);
    std::FILE *const file = find(descriptor);
    if (file == nullptr)
        return -1;

    const int origin = whence == Seek::Set ? SEEK_SET : whence == Seek::Current ? SEEK_CUR : SEEK_END;
    std::fseek(file, offset, origin);
    return static_cast<int>(std::ftell(file));
}

} // namespace yamipan::io
