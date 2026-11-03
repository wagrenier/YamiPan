// The game's file descriptors on host files. EE paths ("cdrom0:\\ROM.000;1", "host0:SPK.ROM") are resolved
// against the data folder: the extracted disc is the one device.
#pragma once

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace yamipan::io
{

class HostFiles
{
public:
    enum class Seek
    {
        Set,
        Current,
        End,
    };

    static HostFiles &instance();

    // The host file for an EE path.
    static std::filesystem::path resolve(std::string_view eePath);

    // Negative when the file cannot be opened.
    int open(std::string_view eePath, bool write, bool create, bool truncate);
    int close(int descriptor);
    int read(int descriptor, void *buffer, std::size_t size);
    int write(int descriptor, const void *buffer, std::size_t size);
    int seek(int descriptor, long offset, Seek whence);

private:
    struct FileCloser
    {
        void operator()(std::FILE *file) const { std::fclose(file); }
    };
    using File = std::unique_ptr<std::FILE, FileCloser>;

    std::FILE *find(int descriptor) const;

    mutable std::mutex m_lock;
    std::vector<File> m_files;
};

} // namespace yamipan::io
