#include "yamipan/iop/system_memory.h"

#include <algorithm>
#include <iterator>

namespace yamipan::iop
{

SystemMemory::SystemMemory()
    : m_ram(std::make_unique<Ram>()) // zeroed, as the old reservation's fresh pages were
{
    // The first granule stays the kernel's, so no block's IOP address is 0.
    m_free.emplace(kGranule, kSize - kGranule);
}

SystemMemory::~SystemMemory() = default;

std::uint32_t SystemMemory::allocate(std::size_t size, Placement placement)
{
    size = size == 0 ? kGranule : (size + kGranule - 1) & ~(kGranule - 1);

    std::lock_guard lock(m_lock);
    if (placement == Placement::High)
    {
        for (auto it = m_free.rbegin(); it != m_free.rend(); ++it)
        {
            if (it->second < size)
                continue;
            const std::size_t offset = it->first + it->second - size;
            it->second -= size;
            if (it->second == 0)
                m_free.erase(std::next(it).base());
            m_used.emplace(offset, size);
            return static_cast<std::uint32_t>(offset);
        }
        return 0;
    }
    for (auto it = m_free.begin(); it != m_free.end(); ++it)
    {
        if (it->second < size)
            continue;
        const std::size_t offset = it->first;
        const std::size_t rest = it->second - size;
        m_free.erase(it);
        if (rest != 0)
            m_free.emplace(offset + size, rest);
        m_used.emplace(offset, size);
        return static_cast<std::uint32_t>(offset);
    }
    return 0;
}

bool SystemMemory::free(std::uint32_t address)
{
    if (address == 0)
        return false;

    std::lock_guard lock(m_lock);
    const auto used = m_used.find(address);
    if (used == m_used.end())
        return false;

    std::size_t offset = used->first;
    std::size_t size = used->second;
    m_used.erase(used);

    // Coalesce with the free blocks on either side.
    auto next = m_free.lower_bound(offset);
    if (next != m_free.end() && next->first == offset + size)
    {
        size += next->second;
        next = m_free.erase(next);
    }
    if (next != m_free.begin())
    {
        auto previous = std::prev(next);
        if (previous->first + previous->second == offset)
        {
            offset = previous->first;
            size += previous->second;
            m_free.erase(previous);
        }
    }
    m_free.emplace(offset, size);
    return true;
}

void *SystemMemory::pointer(std::uint32_t address) const
{
    if (address == 0 || address >= kSize)
        return nullptr;
    return m_ram->bytes + address;
}

std::uint32_t SystemMemory::address(const void *pointer) const
{
    const auto host = reinterpret_cast<std::uintptr_t>(pointer);
    const auto base = reinterpret_cast<std::uintptr_t>(m_ram->bytes);
    if (host < base || host - base >= kSize)
        return 0;
    return static_cast<std::uint32_t>(host - base);
}

std::size_t SystemMemory::largestFree() const
{
    std::lock_guard lock(m_lock);
    std::size_t largest = 0;
    for (const auto &[offset, size] : m_free)
        largest = std::max(largest, size);
    return largest;
}

std::size_t SystemMemory::totalFree() const
{
    std::lock_guard lock(m_lock);
    std::size_t total = 0;
    for (const auto &[offset, size] : m_free)
        total += size;
    return total;
}

} // namespace yamipan::iop
