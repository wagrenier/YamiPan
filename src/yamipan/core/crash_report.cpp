#include "yamipan/core/crash_report.h"

#ifdef _WIN32

#include <cstdint>
#include <cstdio>

#include <windows.h>

namespace yamipan::crash
{
namespace
{

constexpr int kMaxFrames = 48;
constexpr int kMaxStackWords = 2048; // the raw scan: an unwind stops at the first frame with no tables
constexpr int kMaxStackHits = 64;

struct ExeImage
{
    std::uintptr_t base = 0;
    std::uintptr_t size = 0;

    bool contains(std::uintptr_t address) const
    {
        return address >= base && address - base < size;
    }
};

ExeImage exeImage()
{
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    return {base, nt->OptionalHeader.SizeOfImage};
}

// A line per address; only offsets into the exe can be symbolised, so the rest stay absolute.
void printAddress(const char *label, int index, std::uintptr_t address, const ExeImage &image)
{
    if (image.contains(address))
        std::printf("[crash] %s %2d exe+0x%llx\n", label, index, static_cast<unsigned long long>(address - image.base));
    else
        std::printf("[crash] %s %2d 0x%llx\n", label, index, static_cast<unsigned long long>(address));
}

void printUnwind(CONTEXT context, const ExeImage &image)
{
    for (int frame = 0; frame < kMaxFrames && context.Rip != 0; frame++)
    {
        printAddress("frame", frame, context.Rip, image);

        DWORD64 moduleBase = 0;
        RUNTIME_FUNCTION *const function = RtlLookupFunctionEntry(context.Rip, &moduleBase, nullptr);
        if (function == nullptr)
            return;
        void *handlerData = nullptr;
        DWORD64 establisher = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, moduleBase, context.Rip, function, &context, &handlerData, &establisher,
                         nullptr);
    }
}

void printStackScan(const CONTEXT &context, const ExeImage &image)
{
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(reinterpret_cast<const void *>(context.Rsp), &region, sizeof(region)) == 0)
        return;
    const auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
    const auto *word = reinterpret_cast<const std::uintptr_t *>(context.Rsp);
    int hits = 0;
    for (int i = 0; i < kMaxStackWords && hits < kMaxStackHits; i++)
    {
        if (reinterpret_cast<std::uintptr_t>(word + i + 1) > end)
            return;
        if (image.contains(word[i]))
            printAddress("stack", hits++, word[i], image);
    }
}

LONG WINAPI report(EXCEPTION_POINTERS *pointers)
{
    const EXCEPTION_RECORD &record = *pointers->ExceptionRecord;
    const CONTEXT &context = *pointers->ContextRecord;
    const ExeImage image = exeImage();

    std::printf("[crash] exception 0x%08lx on thread %lu\n", record.ExceptionCode, GetCurrentThreadId());
    if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
        std::printf("[crash] %s of 0x%llx\n", record.ExceptionInformation[0] == 0 ? "read" : "write",
                    static_cast<unsigned long long>(record.ExceptionInformation[1]));
    std::printf("[crash] rax %llx rcx %llx rdx %llx rbx %llx rsi %llx rdi %llx rsp %llx rbp %llx\n",
                static_cast<unsigned long long>(context.Rax), static_cast<unsigned long long>(context.Rcx),
                static_cast<unsigned long long>(context.Rdx), static_cast<unsigned long long>(context.Rbx),
                static_cast<unsigned long long>(context.Rsi), static_cast<unsigned long long>(context.Rdi),
                static_cast<unsigned long long>(context.Rsp), static_cast<unsigned long long>(context.Rbp));
    printUnwind(context, image);
    printStackScan(context, image);
    std::fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void install()
{
    SetUnhandledExceptionFilter(report);
}

} // namespace yamipan::crash

#else

namespace yamipan::crash
{

void install()
{
}

} // namespace yamipan::crash

#endif
