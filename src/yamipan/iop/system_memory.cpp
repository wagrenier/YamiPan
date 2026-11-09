#include "yamipan/iop/system_memory.h"

#include <algorithm>
#include <iterator>

#include "yamipan/core/logger.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace yamipan::iop
{

SystemMemory::SystemMemory()
{
#ifdef _WIN32
    // The first free 2 MB from 256 MB up, below 4 GB. An IOP address is the block's host address.
    for (std::uintptr_t hint = 0x10000000; hint < 0xf0000000 && m_base == nullptr; hint += 0x01000000)
        m_base = static_cast<std::byte *>(
            VirtualAlloc(reinterpret_cast<void *>(hint), kSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#endif
    if (m_base == nullptr)
    {
        log::error("iop: no 2 MB below 4 GB for the IOP's memory; AllocSysMemory will fail");
        return;
    }
    m_free.emplace(0, kSize);
}

SystemMemory::~SystemMemory()
{
#ifdef _WIN32
    if (m_base != nullptr)
        VirtualFree(m_base, 0, MEM_RELEASE);
#endif
}

void *SystemMemory::allocate(std::size_t size, Placement placement)
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
            return m_base + offset;
        }
        return nullptr;
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
        return m_base + offset;
    }
    return nullptr;
}

bool SystemMemory::free(void *block)
{
    if (m_base == nullptr || block == nullptr)
        return false;

    std::lock_guard lock(m_lock);
    const auto used = m_used.find(static_cast<std::size_t>(static_cast<std::byte *>(block) - m_base));
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
