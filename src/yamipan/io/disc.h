// The game's disc, made from the extracted folder. rx's cdrom driver reads an ISO 9660 file system from raw
// sectors and FS2's archives are addressed by sector, and the game layer stays as the ROM has it, so the host
// answers sector reads: a primary volume descriptor, a path table and a root directory built in memory, and
// the folder's files laid out behind them. FS2's disc is a dual-layer DVD (ROM.000.. on layer 0, ROMD.008.. on
// layer 1), each layer its own volume, as on the real thing.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace yamipan::io
{

class Disc
{
public:
    static constexpr std::size_t kSectorSize = 0x800;

    static Disc &instance();

    // Lays the folder out. Sector numbers stay valid for the process's life.
    void mount(const std::filesystem::path &folder);

    // Whole sectors from `lsn`, the disc's own numbering. Beyond the end reads as zeroes, as a pressed
    // disc's lead-out reads as nothing useful.
    bool read(std::uint32_t lsn, std::uint32_t sectors, std::byte *buffer);

    std::uint32_t layer1Start() const { return m_layer1Start; }

private:
    struct FileCloser
    {
        void operator()(std::FILE *file) const { std::fclose(file); }
    };

    struct Extent
    {
        std::uint32_t firstSector;
        std::uint32_t sectorCount;
        std::filesystem::path path;
        std::unique_ptr<std::FILE, FileCloser> file; // opened on first use
    };

    void buildVolume(std::uint32_t base, std::span<const std::filesystem::path> files, std::uint32_t &nextSector);
    void readExtent(Extent &extent, std::uint32_t sector, std::byte *buffer);

    std::mutex m_lock;
    std::vector<Extent> m_extents; // ordered by firstSector
    std::vector<std::byte> m_metadata;
    std::uint32_t m_metadataSectors = 0;
    std::vector<std::byte> m_layer1Metadata;
    std::uint32_t m_layer1Start = 0;
};

} // namespace yamipan::io
