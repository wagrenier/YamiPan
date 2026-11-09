// sdk/iop.cpp -- the IOP libraries RXSNDDRV imports. thbase, thevent, intrman, sysmem, loadcore, vblank and
// the IOP side of sifcmd forward to yamipan::iop::Kernel. libsd is still a placeholder: each call answers
// as a failed one and warns once, until the host models the SPU2 (RosePan's sdk/iop_libsd.cpp,
// iop_voice.cpp and iop_reverb.cpp, rewritten as yamipan:: classes).
// The driver only runs with YAMIPAN_RXSNDDRV=1 (sdk/sif.cpp boots it); without that nothing here is called.
#include "iop_host.h"
#include "iop_voice.h"
#include "libsd.h"
#include "sifcmd.h"

#include <mutex>

#include "yamipan/core/logger.h"
#include "yamipan/iop/kernel.h"

namespace
{

yamipan::iop::Kernel &kernel()
{
    return yamipan::iop::Kernel::instance();
}

// The host's missing part is the only error these can report.
void notYet(std::once_flag &once, const char *name)
{
    std::call_once(once, [name] { yamipan::log::warning("IOP host not there yet: {} called", name); });
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
    return size > 0 ? kernel().memory().allocate(static_cast<std::size_t>(size)) : nullptr;
}

int YamiPan_IopFreeSysMemory(void *ptr)
{
    return kernel().memory().free(ptr) ? 0 : -1;
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

int sceSdInit(int)
{
    static std::once_flag once;
    notYet(once, "sceSdInit");
    return -1;
}

void sceSdSetParam(u_short, u_short)
{
    static std::once_flag once;
    notYet(once, "sceSdSetParam");
}

u_short sceSdGetParam(u_short)
{
    static std::once_flag once;
    notYet(once, "sceSdGetParam");
    return 0;
}

void sceSdSetSwitch(u_short, u_int)
{
    static std::once_flag once;
    notYet(once, "sceSdSetSwitch");
}

u_int sceSdGetSwitch(u_short)
{
    static std::once_flag once;
    notYet(once, "sceSdGetSwitch");
    return 0;
}

void sceSdSetAddr(u_short, u_int)
{
    static std::once_flag once;
    notYet(once, "sceSdSetAddr");
}

u_int sceSdGetAddr(u_short)
{
    static std::once_flag once;
    notYet(once, "sceSdGetAddr");
    return 0;
}

void sceSdSetCoreAttr(u_short, u_short)
{
    static std::once_flag once;
    notYet(once, "sceSdSetCoreAttr");
}

int sceSdVoiceTrans(short, u_int, u_char *, u_int, u_int)
{
    static std::once_flag once;
    notYet(once, "sceSdVoiceTrans");
    return -1;
}

int sceSdBlockTrans(short, u_short, u_char *, u_int, u_int)
{
    static std::once_flag once;
    notYet(once, "sceSdBlockTrans");
    return -1;
}

int sceSdVoiceTransStatus(short, short)
{
    static std::once_flag once;
    notYet(once, "sceSdVoiceTransStatus");
    return -1;
}

int sceSdBlockTransStatus(short, short)
{
    static std::once_flag once;
    notYet(once, "sceSdBlockTransStatus");
    return 0;
}

int sceSdSetEffectAttr(int, void *)
{
    static std::once_flag once;
    notYet(once, "sceSdSetEffectAttr");
    return -1;
}

int sceSdProcBatch(sceSdBatch *, u_int[], u_int)
{
    static std::once_flag once;
    notYet(once, "sceSdProcBatch");
    return -1;
}

u_short sceSdNote2Pitch(u_short, u_short, u_short, short)
{
    static std::once_flag once;
    notYet(once, "sceSdNote2Pitch");
    return 0;
}

int sceSdClearEffectWorkArea(int, int, int)
{
    static std::once_flag once;
    notYet(once, "sceSdClearEffectWorkArea");
    return -1;
}

void *sceSdSetTransIntrHandler(int, void *, void *)
{
    static std::once_flag once;
    notYet(once, "sceSdSetTransIntrHandler");
    return nullptr;
}

void *sceSdSetSpu2IntrHandler(void *, void *)
{
    static std::once_flag once;
    notYet(once, "sceSdSetSpu2IntrHandler");
    return nullptr;
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

void YamiPan_VoiceSetGroup(int, int, int)
{
    static std::once_flag once;
    notYet(once, "YamiPan_VoiceSetGroup");
}
