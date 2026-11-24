// sdk/iop.cpp -- the IOP libraries RXSNDDRV imports. thbase, thevent, intrman, sysmem, loadcore, vblank and
// the IOP side of sifcmd forward to yamipan::iop::Kernel, libsd to its Spu2 (RosePan's sdk/iop_libsd.cpp
// as a class), which makes the sound through Spu2Voices and Spu2Reverb.
// sdk/sif.cpp boots the driver when rxfile loads RXSNDDRV.IRX.
#include "iop_host.h"
#include "iop_voice.h"
#include "libsd.h"
#include "sifcmd.h"

#include <cmath>

#include "yamipan/iop/kernel.h"

namespace
{

yamipan::iop::Kernel &kernel()
{
    return yamipan::iop::Kernel::instance();
}

} // namespace

// ---- thbase, thevent, vblank ----------------------------------------------------------------------

int YamiPan_IopCreateThread(IopThreadParam *param)
{
    if (param == nullptr || param->entry == nullptr)
        return -1;
    return kernel().createThread(reinterpret_cast<void (*)()>(param->entry), param->option, param->initPriority,
                                 param->stackSize);
}

// RXSNDDRV's thread entries take no argument: their work travels in ThreadParam::option.
int YamiPan_IopStartThread(int thid, void *)
{
    return kernel().startThread(thid);
}

int YamiPan_IopGetThreadId()
{
    return kernel().currentThreadId();
}

int YamiPan_IopCreateEventFlag(IopEventFlagParam *param)
{
    return kernel().createEventFlag(param != nullptr ? param->initPattern : 0);
}

int YamiPan_IopSetEventFlag(int evfid, u_int bitpattern)
{
    return kernel().setEventFlag(evfid, bitpattern);
}

int YamiPan_IopWaitEventFlag(int evfid, u_int bitpattern, int waitmode, u_int *resultpat)
{
    return kernel().waitEventFlag(evfid, bitpattern, waitmode, resultpat);
}

int YamiPan_IopWaitVblankEnd()
{
    return kernel().waitVblankEnd();
}

// ---- intrman, loadcore ----------------------------------------------------------------------------

// The big kernel lock already keeps IOP code from interleaving, which is all disabling interrupts did
// for RXSNDDRV; its transfer and SPU2 handlers take the same lock when the host calls them.
int YamiPan_IopCpuSuspendIntr(int *oldstat)
{
    if (oldstat != nullptr)
        *oldstat = 0;
    return 0;
}

int YamiPan_IopCpuResumeIntr(int)
{
    return 0;
}

// One address space, no cache to write back.
void YamiPan_IopFlushDcache() {}

// ---- sysmem ---------------------------------------------------------------------------------------

void *YamiPan_IopAllocSysMemory(int, int size, void *)
{
    auto &memory = kernel().memory();
    return size > 0 ? memory.pointer(memory.allocate(static_cast<std::size_t>(size))) : nullptr;
}

int YamiPan_IopFreeSysMemory(void *ptr)
{
    auto &memory = kernel().memory();
    return memory.free(memory.address(ptr)) ? 0 : -1;
}

void *YamiPan_IopPointer(u_int iopAddr)
{
    return kernel().memory().pointer(iopAddr);
}

u_int YamiPan_IopAddress(const void *ptr)
{
    return kernel().memory().address(ptr);
}

int YamiPan_IopQueryMemSize()
{
    return static_cast<int>(kernel().memory().size());
}

int YamiPan_IopQueryMaxFreeMemSize()
{
    return static_cast<int>(kernel().memory().largestFree());
}

int YamiPan_IopQueryTotalFreeMemSize()
{
    return static_cast<int>(kernel().memory().totalFree());
}

// ---- libsd ----------------------------------------------------------------------------------------
// The SPU2 is yamipan::iop::Spu2 (registers, sound RAM, transfers that complete, voices that play in
// time); these are libsd's calls onto it. Every caller is IOP code, under the big kernel lock.

namespace
{

yamipan::iop::Spu2 &spu2()
{
    return kernel().spu2();
}

} // namespace

int sceSdInit(int)
{
    spu2().reset();
    return 0;
}

void sceSdSetParam(u_short entry, u_short value)
{
    spu2().setParam(entry, value);
}

u_short sceSdGetParam(u_short entry)
{
    return spu2().param(entry);
}

void sceSdSetSwitch(u_short entry, u_int value)
{
    spu2().setSwitch(entry, value);
}

u_int sceSdGetSwitch(u_short entry)
{
    return spu2().switchBits(entry);
}

void sceSdSetAddr(u_short entry, u_int value)
{
    spu2().setAddr(entry, value);
}

u_int sceSdGetAddr(u_short entry)
{
    return spu2().addr(entry);
}

void sceSdSetCoreAttr(u_short entry, u_short value)
{
    spu2().setCoreAttr(entry, value);
}

int sceSdVoiceTrans(short chan, u_int mode, u_char *iopaddr, u_int spuaddr, u_int size)
{
    return spu2().voiceTrans(chan, mode, iopaddr, spuaddr, size);
}

int sceSdBlockTrans(short chan, u_short mode, u_char *iopaddr, u_int size, u_int start_addr)
{
    return spu2().blockTrans(chan, mode, iopaddr, size, start_addr);
}

int sceSdVoiceTransStatus(short chan, short flag)
{
    return spu2().voiceTransStatus(chan, flag);
}

int sceSdBlockTransStatus(short chan, short flag)
{
    return spu2().blockTransStatus(chan, flag);
}

int sceSdSetEffectAttr(int core, void *attr)
{
    const auto *a = static_cast<const sceSdEffectAttrSdk *>(attr);
    if (a == nullptr)
        return -1;
    return spu2().setEffectAttr(core, {a->core, a->mode, a->depth_L, a->depth_R, a->delay, a->feedback});
}

// Each command is the call of the same name; the get forms answer into `returns` when it is given.
// WRITEIOP / WRITEEE / EERETURN / GETCORE: RXSNDDRV's batches (the mixer's and the effect's defaults)
// send none.
int sceSdProcBatch(sceSdBatch *batch, u_int returns[], u_int num)
{
    if (batch == nullptr)
        return 0;
    u_int i = 0;
    for (; i < num; i++)
    {
        const sceSdBatch &b = batch[i];
        u_int ret = 0;
        switch (b.func)
        {
        case SD_BATCH_SETPARAM:
            sceSdSetParam(b.entry, static_cast<u_short>(b.value));
            break;
        case SD_BATCH_SETSWITCH:
            sceSdSetSwitch(b.entry, b.value);
            break;
        case SD_BATCH_SETADDR:
            sceSdSetAddr(b.entry, b.value);
            break;
        case SD_BATCH_SETCORE:
            sceSdSetCoreAttr(b.entry, static_cast<u_short>(b.value));
            break;
        case SD_BATCH_GETPARAM:
            ret = sceSdGetParam(b.entry);
            break;
        case SD_BATCH_GETSWITCH:
            ret = sceSdGetSwitch(b.entry);
            break;
        case SD_BATCH_GETADDR:
            ret = sceSdGetAddr(b.entry);
            break;
        default:
            break;
        }
        if (returns != nullptr)
            returns[i] = ret;
    }
    return static_cast<int>(i);
}

// The SDK computes it from semitone and fine-tune tables; 2^(cents / 1200) is the same curve (RosePan's),
// on the scale where 0x1000 is the centre note. Clamped to the u_short the SDK answers. The tables
// themselves are in the game's own LIBSD.IRX (MODULES), unread.
u_short sceSdNote2Pitch(u_short center_note, u_short center_fine, u_short note, short fine)
{
    const int offset = (static_cast<int>(note) - static_cast<int>(center_note)) * 128 + fine -
                       static_cast<int>(center_fine);
    const double pitch = 4096.0 * std::pow(2.0, offset / (12.0 * 128.0));
    if (pitch > 65535.0)
        return 0xffff;
    return static_cast<u_short>(pitch + 0.5);
}

// The core's work area as it is set now (`channel` is the core libsd borrowed for the clearing transfer).
// RXSNDDRV clears its areas with its own zero-fill DMA; it calls this nowhere, and would retry until 0.
int sceSdClearEffectWorkArea(int core, int, int)
{
    spu2().clearEffectWorkArea(core);
    return 0;
}

void *sceSdSetTransIntrHandler(int core, void *func, void *arg)
{
    return reinterpret_cast<void *>(
        spu2().setTransHandler(core, reinterpret_cast<yamipan::iop::Spu2::Handler>(func), arg));
}

void *sceSdSetSpu2IntrHandler(void *func, void *arg)
{
    return reinterpret_cast<void *>(spu2().setSpu2Handler(reinterpret_cast<yamipan::iop::Spu2::Handler>(func), arg));
}

// ---- sifcmd, the IOP's server side ----------------------------------------------------------------

// The queue only groups servers for sceSifRpcLoop here; nothing waits on it.
void sceSifSetRpcQueue(sceSifQueueData *q, int)
{
    if (q != nullptr)
        *q = sceSifQueueData{};
}

void sceSifRegisterRpc(sceSifServeData *sd, u_int command, sceSifRpcFunc func, void *buff, sceSifRpcFunc cfunc,
                       void *cbuff, sceSifQueueData *q)
{
    if (sd != nullptr)
    {
        *sd = sceSifServeData{};
        sd->command = command;
        sd->func = func;
        sd->buff = buff;
        sd->cfunc = cfunc;
        sd->cbuff = cbuff;
        sd->base = q;
    }
    kernel().rpc().add(command, func, buff, q);
}

// On the IOP this looped for ever taking requests; here a call runs on its caller's thread
// (yamipan/iop/rpc_servers.h), so this only opens the queue's servers and returns.
void sceSifRpcLoop(sceSifQueueData *q)
{
    kernel().rpc().serve(q);
}

void *YamiPan_IopRpcDispatch(u_int rpc_number, u_int command, void *data, int size)
{
    return kernel().rpc().call(kernel(), rpc_number, command, data, size);
}

int YamiPan_IopRpcIsReady(u_int rpc_number)
{
    return kernel().rpc().serving(rpc_number) ? 1 : 0;
}

// ---- the host mixer's volume groups ---------------------------------------------------------------

void YamiPan_VoiceSetGroup(int core, int voice, int group)
{
    spu2().setVoiceGroup(core, voice, group);
}
