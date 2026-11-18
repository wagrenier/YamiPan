// The IOP's 2 MB of RAM as sysmem hands it out. The driver and the EE keep IOP addresses as 32-bit words
// (the EE passes them back and forth as u_int), so an IOP address is what it was on the IOP: an offset into
// this RAM. pointer() and address() convert it to and from the host pointer where the code dereferences
// one. No block starts below kGranule (the IOP kernel's own memory), so 0 still means "no address".
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

namespace yamipan::iop
{

class SystemMemory
{
public:
    static constexpr std::size_t kSize = 2 * 1024 * 1024;

    SystemMemory();
    ~SystemMemory();

    SystemMemory(const SystemMemory &) = delete;
    SystemMemory &operator=(const SystemMemory &) = delete;

    // sysmem's placements: the lowest block that fits, or the top of the highest one.
    enum class Placement
    {
        Low,
        High,
    };

    // AllocSysMemory: 256-byte granular as sysmem was. The block's IOP address, or 0 when nothing fits.
    std::uint32_t allocate(std::size_t size, Placement placement = Placement::Low);
    bool free(std::uint32_t address);

    // The host pointer an IOP address names: null for 0 and for an address past the RAM's end.
    void *pointer(std::uint32_t address) const;
    // The IOP address of a host pointer into the RAM: 0 for null and for a pointer outside it.
    std::uint32_t address(const void *pointer) const;

    std::size_t size() const { return kSize; }
    std::size_t largestFree() const;
    std::size_t totalFree() const;

private:
    static constexpr std::size_t kGranule = 0x100;

    struct alignas(kGranule) Ram
    {
        std::byte bytes[kSize];
    };

    std::unique_ptr<Ram> m_ram;
    mutable std::mutex m_lock;
    std::map<std::size_t, std::size_t> m_free; // offset -> size, coalesced
    std::map<std::size_t, std::size_t> m_used; // offset -> size
};

} // namespace yamipan::iop
