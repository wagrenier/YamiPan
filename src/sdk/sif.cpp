// sdk/sif.cpp -- the EE's side of the IOP. No IOP runs yet: the sound driver is in src/iop/rxsnddrv but
// nothing boots it until the IOP host comes, so modules "load", memory is handed out as bare IOP
// addresses, and every RPC answers at once with an empty reply. Game code cannot tell, which is the
// point: the boot goes through.
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <string_view>

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

constexpr unsigned kIopMemoryBase = 0x100000;
constexpr unsigned kIopAlignment = 0x40;

std::atomic<unsigned> s_iopTop{kIopMemoryBase};

// Whether a module path names RXSNDDRV: rxfile loads it as 'cdrom0:\MODULES\RXSNDDRV.IRX;1'.
bool namesSoundDriver(const char *path)
{
    std::string_view name(path);
    constexpr std::string_view kDriver = "rxsnddrv.irx";
    return std::search(name.begin(), name.end(), kDriver.begin(), kDriver.end(), [](char a, char b) {
               return std::tolower(static_cast<unsigned char>(a)) == b;
           }) != name.end();
}

// Until the host serves the driver's RPC and models the SPU2, it runs only on request: its threads start
// and tick, the EE does not talk to it yet.
bool driverRequested()
{
    static const bool requested = [] {
        const char *value = std::getenv("YAMIPAN_RXSNDDRV");
        return value != nullptr && value[0] == '1';
    }();
    return requested;
}

} // namespace

void YamiPan_IopBoot()
{
    static std::once_flag once;
    std::call_once(once, [] {
        yamipan::log::info("iop: starting RXSNDDRV (YAMIPAN_RXSNDDRV=1)");
        yamipan::iop::Kernel::instance().runModuleStart(&RxSndDrvStart);
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
    if (path != nullptr && driverRequested() && namesSoundDriver(path))
        YamiPan_IopBoot();

    static std::atomic<int> s_nextModule{1};
    return s_nextModule.fetch_add(1);
}

int sceSifBindRpc(sceSifClientData *bd, unsigned int, unsigned int)
{
    // Callers wait for serve to be set; nothing reads through it.
    bd->serve = reinterpret_cast<_sif_serve_data *>(bd);
    return 0;
}

int sceSifCheckStatRpc(sceSifRpcData *)
{
    return 0;
}

int sceSifCallRpc(sceSifClientData *, unsigned int, unsigned int, void *, int, void *receive, int rsize,
                  sceSifEndFunc func, void *para)
{
    if (receive != nullptr && rsize > 0)
        std::memset(receive, 0, static_cast<std::size_t>(rsize));
    if (func != nullptr)
        reinterpret_cast<void (*)(void *)>(func)(para);
    return 0;
}

int sceSifGetOtherData(sceSifReceiveData *, void *, void *dest, int size, unsigned int)
{
    if (dest != nullptr && size > 0)
        std::memset(dest, 0, static_cast<std::size_t>(size));
    return 0;
}

int sceSifDmaStat(u_int)
{
    return -1;
}

u_int sceSifSetDma(sceSifDmaData *, int)
{
    return 1;
}

void *sceSifAllocSysMemory(int, u_int size, void *)
{
    const unsigned aligned = (size + kIopAlignment - 1) & ~(kIopAlignment - 1);
    return reinterpret_cast<void *>(static_cast<uintptr_t>(s_iopTop.fetch_add(aligned)));
}

int sceSifFreeSysMemory(void *)
{
    return 0;
}
