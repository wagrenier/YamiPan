// The IOP's 2 MB of RAM as sysmem hands it out. The driver keeps IOP addresses as 32-bit words (the
// EE passes them back and forth as u_int) and dereferences them, so every block must lie below 4 GB:
// the arena is reserved at a low address, and an IOP address is the block's host address.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
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

    // AllocSysMemory: first fit from the bottom, 256-byte granular as sysmem was. Null when full, or
    // when no arena could be placed below 4 GB.
    // sysmem's placements: the lowest block that fits, or the top of the highest one.
    enum class Placement
    {
        Low,
        High,
    };

    void *allocate(std::size_t size, Placement placement = Placement::Low);
    bool free(void *block);

    std::size_t size() const { return m_base != nullptr ? kSize : 0; }
    std::size_t largestFree() const;
    std::size_t totalFree() const;

private:
    static constexpr std::size_t kGranule = 0x100;

    std::byte *m_base = nullptr;
    mutable std::mutex m_lock;
    std::map<std::size_t, std::size_t> m_free; // offset -> size, coalesced
    std::map<std::size_t, std::size_t> m_used; // offset -> size
};

} // namespace yamipan::iop
