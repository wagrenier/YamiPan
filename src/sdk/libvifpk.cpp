// sdk/libvifpk.cpp -- FS2's libvifpk 0x10ac60..0x10ad90, the three calls the frame chain still uses.
#include "libvifpk.h"

namespace
{
constexpr uintptr_t kQwordMask = 0xf;
constexpr u_long kDmaTagNext = 0x20000000;
constexpr u_int kAddressMask = 0x9fffffff;

void closeOpenTag(sceVif1Packet *packet)
{
    if (packet->pDmaTag == nullptr)
        return;
    auto *const tag = reinterpret_cast<u_int *>(packet->pDmaTag);
    const auto quadwords = static_cast<u_int>(packet->pCurrent - packet->pDmaTag - 1);
    tag[0] = (tag[0] & 0xffff0000) | quadwords;
}
}

void sceVif1PkReset(sceVif1Packet *packet)
{
    packet->pCurrent = packet->pBase;
    packet->pDmaTag = nullptr;
}

void sceVif1PkCloseDmaTag(sceVif1Packet *packet)
{
    // Pad to a qword, then fix the open tag's QWC.
    const auto address = reinterpret_cast<uintptr_t>(packet->pCurrent);
    packet->pCurrent = reinterpret_cast<u_long128 *>((address + kQwordMask) & ~kQwordMask);
    closeOpenTag(packet);
    packet->pDmaTag = nullptr;
}

void sceVif1PkNext(sceVif1Packet *packet, u_int address, int option)
{
    sceVif1PkCloseDmaTag(packet);
    auto *const tag = reinterpret_cast<u_int *>(packet->pCurrent);
    tag[0] = static_cast<u_int>(kDmaTagNext) | static_cast<u_int>(option);
    tag[1] = address & kAddressMask;
    tag[2] = 0;
    tag[3] = 0;
    packet->pDmaTag = packet->pCurrent;
    packet->pCurrent++;
}
