#include "yamipan/io/disc.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#include <system_error>

#include "yamipan/core/logger.h"
#include "yamipan/core/paths.h"

namespace yamipan::io
{
namespace
{

namespace fs = std::filesystem;

// Where ISO 9660 puts things. The first 16 sectors are the system area; the volume needs only these.
constexpr std::uint32_t kDescriptorSector = 16;
constexpr std::uint32_t kPathTableSector = 18;
constexpr std::uint32_t kRootSector = 19;
constexpr std::uint32_t kRootSectors = 8;
constexpr std::uint32_t kFirstFileSector = kRootSector + kRootSectors;

// Offsets in the descriptor sector.
constexpr std::size_t kPathTableSizeField = 132;
constexpr std::size_t kPathTableLsnField = 140;
constexpr std::size_t kRootRecordField = 156;

// Offsets in a directory record.
constexpr std::size_t kRecordExtent = 2;
constexpr std::size_t kRecordSize = 10;
constexpr std::size_t kRecordFlags = 25;
constexpr std::size_t kRecordNameLength = 32;
constexpr std::size_t kRecordName = 33;
constexpr std::uint8_t kFlagDirectory = 2;
constexpr std::size_t kDotRecordLength = 34;

constexpr std::uint64_t kSector = Disc::kSectorSize;

void put32(std::byte *out, std::uint32_t value)
{
    for (int index = 0; index < 4; index++)
        out[index] = static_cast<std::byte>(value >> (index * 8));
}

// ISO 9660 stores sizes both ways round.
void putBoth32(std::byte *out, std::uint32_t value)
{
    put32(out, value);
    for (int index = 0; index < 4; index++)
        out[4 + index] = static_cast<std::byte>(value >> ((3 - index) * 8));
}

std::size_t putRecord(std::byte *out, std::string_view name, std::uint32_t extent, std::uint32_t size, std::uint8_t flags)
{
    const std::size_t length = (kRecordName + name.size() + 1) & ~std::size_t{1};
    out[0] = static_cast<std::byte>(length);
    putBoth32(out + kRecordExtent, extent);
    putBoth32(out + kRecordSize, size);
    out[kRecordFlags] = static_cast<std::byte>(flags);
    out[kRecordNameLength] = static_cast<std::byte>(name.size());
    std::memcpy(out + kRecordName, name.data(), name.size());
    return length;
}

// ROM.000 -> ROM.000;1, as the disc spells it.
std::string isoName(const fs::path &file)
{
    std::string name = pathToUtf8(file.filename());
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return name + ";1";
}

} // namespace

Disc &Disc::instance()
{
    static Disc disc;
    return disc;
}

// One volume's metadata, with its files' sector numbers relative to the volume, as an ISO records them:
// the driver adds the layer's start itself.
void Disc::buildVolume(std::uint32_t base, std::span<const fs::path> files, std::uint32_t &nextSector)
{
    std::vector<std::byte> &metadata = base == 0 ? m_metadata : m_layer1Metadata;
    metadata.assign(kFirstFileSector * kSector, std::byte{0});

    std::byte *const descriptor = metadata.data() + kDescriptorSector * kSector;
    descriptor[0] = std::byte{1};
    std::memcpy(descriptor + 1, "CD001", 5);

    // The path table holds only the root: everything on FS2's disc that the game opens sits there.
    constexpr std::size_t kPathRecordSize = 10;
    std::byte *const pathTable = metadata.data() + kPathTableSector * kSector;
    pathTable[0] = std::byte{1};
    put32(pathTable + 2, kRootSector);
    pathTable[6] = std::byte{1};
    putBoth32(descriptor + kPathTableSizeField, kPathRecordSize);
    put32(descriptor + kPathTableLsnField, kPathTableSector);

    const auto rootBytes = static_cast<std::uint32_t>(kRootSectors * kSector);
    putRecord(descriptor + kRootRecordField, std::string_view("\0", 1), kRootSector, rootBytes, kFlagDirectory);

    std::byte *const root = metadata.data() + kRootSector * kSector;
    std::size_t at = 0;
    at += putRecord(root + at, std::string_view("\0", 1), kRootSector, rootBytes, kFlagDirectory);
    at += putRecord(root + at, std::string_view("\1", 1), kRootSector, rootBytes, kFlagDirectory);

    std::uint32_t sector = kFirstFileSector;
    for (const fs::path &file : files)
    {
        std::error_code error;
        const std::uint64_t bytes = fs::file_size(file, error);
        if (error)
            continue;
        const auto sectors = static_cast<std::uint32_t>((bytes + kSector - 1) / kSector);

        // Records do not cross a sector.
        const std::string name = isoName(file);
        const std::size_t length = (kRecordName + name.size() + 1) & ~std::size_t{1};
        if (at % kSector + length > kSector)
            at = (at / kSector + 1) * kSector;
        at += putRecord(root + at, name, sector, static_cast<std::uint32_t>(bytes), 0);

        m_extents.push_back({base + sector, sectors, file, nullptr});
        sector += sectors;
    }
    nextSector = sector;
}

void Disc::mount(const fs::path &folder)
{
    std::scoped_lock lock(m_lock);

    std::vector<fs::path> layer0;
    std::vector<fs::path> layer1;
    std::error_code error;
    for (const fs::directory_entry &entry : fs::directory_iterator(folder, error))
    {
        if (!entry.is_regular_file(error))
            continue;
        (entry.path().filename().string().starts_with("ROMD.") ? layer1 : layer0).push_back(entry.path());
    }
    std::ranges::sort(layer0);
    std::ranges::sort(layer1);

    m_extents.clear();
    std::uint32_t layer0End = 0;
    buildVolume(0, layer0, layer0End);

    m_layer1Start = layer0End;
    std::uint32_t layer1End = 0;
    buildVolume(m_layer1Start, layer1, layer1End);

    log::info("Disc: {} + {} files, layer 1 at sector {}", layer0.size(), layer1.size(), m_layer1Start);
}

void Disc::readExtent(Extent &extent, std::uint32_t sector, std::byte *buffer)
{
    if (extent.file == nullptr)
    {
#ifdef _WIN32
        extent.file.reset(_wfopen(extent.path.c_str(), L"rb"));
#else
        extent.file.reset(std::fopen(extent.path.c_str(), "rb"));
#endif
    }
    std::memset(buffer, 0, kSectorSize);
    if (extent.file == nullptr)
        return;

    const std::uint64_t offset = static_cast<std::uint64_t>(sector - extent.firstSector) * kSector;
    _fseeki64(extent.file.get(), static_cast<long long>(offset), SEEK_SET);
    std::fread(buffer, 1, kSectorSize, extent.file.get());
}

bool Disc::read(std::uint32_t lsn, std::uint32_t sectors, std::byte *buffer)
{
    std::scoped_lock lock(m_lock);

    for (std::uint32_t index = 0; index < sectors; index++)
    {
        const std::uint32_t sector = lsn + index;
        std::byte *const out = buffer + index * kSector;

        if (sector < kFirstFileSector)
        {
            std::memcpy(out, m_metadata.data() + sector * kSector, kSector);
            continue;
        }
        if (sector >= m_layer1Start && sector - m_layer1Start < kFirstFileSector)
        {
            std::memcpy(out, m_layer1Metadata.data() + (sector - m_layer1Start) * kSector, kSector);
            continue;
        }

        const auto extent = std::ranges::upper_bound(m_extents, sector, {}, &Extent::firstSector);
        if (extent == m_extents.begin() || sector - std::prev(extent)->firstSector >= std::prev(extent)->sectorCount)
        {
            std::memset(out, 0, kSector);
            continue;
        }
        readExtent(*std::prev(extent), sector, out);
    }
    return true;
}

} // namespace yamipan::io
