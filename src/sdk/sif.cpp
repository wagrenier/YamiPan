// sdk/sif.cpp -- the EE's side of the IOP.
//
// An IOP runs (yamipan/iop/): loading RXSNDDRV.IRX boots the sound driver, IOP memory is
// iop::SystemMemory (an IOP address is a host address below 4 GB), SIF DMA and sceSifGetOtherData copy,
// and calls to the driver's RPC 0xd000 go through its own dispatcher, on the caller's thread, before
// sceSifCallRpc returns (so its end function runs at once). The other modules only "load", and a call
// to a server nobody registered answers at once with an empty reply.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <mutex>
#include <string_view>
#include <thread>

#include "sif.h"
#include "sifcmd.h"
#include "sifdev.h"
#include "sifman.h"
#include "sifrpc.h"

#include <cstring>

#include "iop/rxsnddrv/rxsnddrv.h"
#include "yamipan/core/logger.h"
#include "yamipan/iop/kernel.h"

namespace
{

// Whether a module path names RXSNDDRV: rxfile loads it as 'cdrom0:\MODULES\RXSNDDRV.IRX;1'.
bool namesSoundDriver(const char *path)
{
    std::string_view name(path);
    constexpr std::string_view kDriver = "rxsnddrv.irx";
    return std::search(name.begin(), name.end(), kDriver.begin(), kDriver.end(), [](char a, char b) {
               return std::tolower(static_cast<unsigned char>(a)) == b;
           }) != name.end();
}

// RXSNDDRV is the rx sound library's server.
constexpr unsigned kRxSndRpc = 0xd000;

} // namespace

void YamiPan_IopBoot()
{
    static std::once_flag once;
    std::call_once(once, [] {
        yamipan::log::info("iop: starting RXSNDDRV");
        yamipan::iop::Kernel::instance().runModuleStart(&RxSndDrvStart);

        // A boot check: once its RPC thread has registered, command 0x12 (the version, no side effect)
        // goes through the driver's own dispatcher. The EE binds its client only after this.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (YamiPan_IopRpcIsReady(kRxSndRpc) == 0 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        u_int none[4] = {};
        const auto *reply = static_cast<const u_int *>(YamiPan_IopRpcDispatch(kRxSndRpc, 0x12, none, sizeof none));
        if (reply != nullptr)
            yamipan::log::info("iop: RXSNDDRV serves RPC {:#x}; command 0x12 answers version {:#06x}", kRxSndRpc,
                               reply[0]);
        else
            yamipan::log::warning("iop: RXSNDDRV did not register RPC {:#x}", kRxSndRpc);
    });
}

void sceSifInitRpc(unsigned int) {}
void sceSifExitCmd() {}
void sceSifInitIopHeap() {}
void sceFsReset() {}

int sceSifRebootIop(const char *)
{
    return 1;
}

int sceSifSyncIop()
{
    return 1;
}

int sceSifLoadModule(const char *path, int, const char *)
{
    // The IOP ran a module's start() as it loaded it; the sound driver is the one with code here.
    if (path != nullptr && namesSoundDriver(path))
        YamiPan_IopBoot();

    static std::atomic<int> s_nextModule{1};
    return s_nextModule.fetch_add(1);
}

int sceSifBindRpc(sceSifClientData *bd, unsigned int request, unsigned int)
{
    // Callers wait for serve to be set; nothing reads through it. The number picks the server a call
    // goes to.
    bd->command = request;
    bd->serve = reinterpret_cast<_sif_serve_data *>(bd);
    return 0;
}

int YamiPan_IopSoundDriverRuns()
{
    return YamiPan_IopRpcIsReady(kRxSndRpc) != 0 ? 1 : 0;
}

int sceSifCheckStatRpc(sceSifRpcData *)
{
    return 0;
}

// The IOP answered `rsize` bytes from the buffer its handler returned, whatever their meaning; so does this.
// A call nothing serves (every other client: rxsys's tty and its own RPCs) answers zeros.
int sceSifCallRpc(sceSifClientData *bd, unsigned int fno, unsigned int, void *send, int ssize, void *receive,
                  int rsize, sceSifEndFunc func, void *para)
{
    const void *reply = nullptr;
    if (bd != nullptr && YamiPan_IopRpcIsReady(bd->command) != 0)
        reply = YamiPan_IopRpcDispatch(bd->command, fno, send, ssize);
    if (receive != nullptr && rsize > 0)
    {
        if (reply != nullptr)
            std::memcpy(receive, reply, static_cast<std::size_t>(rsize));
        else
            std::memset(receive, 0, static_cast<std::size_t>(rsize));
    }
    if (func != nullptr)
        reinterpret_cast<void (*)(void *)>(func)(para);
    return 0;
}

// Both sides call it: the EE reading IOP memory (rxSndIopRead), the driver reading EE memory (src is the
// pointer RxSndDrvEePointer widened). Either way both are host pointers.
int sceSifGetOtherData(sceSifReceiveData *, void *src, void *dest, int size, unsigned int)
{
    if (dest == nullptr || size <= 0)
        return 0;
    if (src != nullptr)
        std::memcpy(dest, src, static_cast<std::size_t>(size));
    else
        std::memset(dest, 0, static_cast<std::size_t>(size));
    return 0;
}

int sceSifDmaStat(u_int)
{
    return -1;
}

// Both sides call it: the EE writing IOP memory (rxSndIopWrite), the driver writing EE memory
// (RxSndDrvSendToEe). The transfer is done when this returns; sceSifDmaStat says so.
u_int sceSifSetDma(sceSifDmaData *sdd, int len)
{
    if (sdd != nullptr)
    {
        for (int i = 0; i < len; i++)
        {
            if (sdd[i].addr != nullptr && sdd[i].data != nullptr && sdd[i].size > 0)
                std::memcpy(sdd[i].addr, sdd[i].data, static_cast<std::size_t>(sdd[i].size));
        }
    }
    return 1;
}

// Mode 1 is sysmem's high placement (the sound library's "from the top"), 0 its low one; mode 2 ("at
// `addr`") no caller uses, so it is placed low.
void *sceSifAllocSysMemory(int mode, u_int size, void *)
{
    using Placement = yamipan::iop::SystemMemory::Placement;
    return yamipan::iop::Kernel::instance().memory().allocate(size, mode == 1 ? Placement::High : Placement::Low);
}

int sceSifFreeSysMemory(void *addr)
{
    return yamipan::iop::Kernel::instance().memory().free(addr) ? 0 : -1;
}
