// sdk/sif.cpp -- the EE's side of the IOP. No IOP runs yet: until the sound driver comes with the audio
// milestone, modules "load", memory is handed out as bare IOP addresses, and every RPC answers at once
// with an empty reply. Game code cannot tell, which is the point: the boot goes through.
#include <atomic>

#include "iop/rxsnddrv/rxsnddrv.h"
#include "sif.h"
#include "sifcmd.h"
#include "sifdev.h"
#include "sifman.h"
#include "sifrpc.h"

#include <cstring>

namespace
{

constexpr unsigned kIopMemoryBase = 0x100000;
constexpr unsigned kIopAlignment = 0x40;

std::atomic<unsigned> s_iopTop{kIopMemoryBase};

} // namespace

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

int sceSifLoadModule(const char *, int, const char *)
{
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

void YamiPan_RxSndNoteEeAddress(const void *, size_t) {}
