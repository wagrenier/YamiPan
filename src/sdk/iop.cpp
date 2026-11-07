// sdk/iop.cpp -- the IOP libraries RXSNDDRV imports (thbase, thevent, intrman, sysmem, loadcore, vblank,
// libsd and the IOP side of sifcmd), until the IOP host exists.
//
// The driver (src/iop/rxsnddrv) is compiled and linked, but nothing boots it: sif.cpp still answers the
// EE's RPCs itself. So nothing here is called. Each function answers as a failed call and warns once,
// so a caller that turns up is seen at once. The IOP host replaces this file: RosePan's sdk/iop_host.cpp,
// iop_sif.cpp, iop_libsd.cpp, iop_voice.cpp and iop_reverb.cpp, rewritten as yamipan:: classes
// (cpp-port.md), with sif.cpp's sceSifCallRpc dispatching to the driver's server.
#include "iop_host.h"
#include "iop_voice.h"
#include "libsd.h"
#include "sifcmd.h"

#include <mutex>

#include "yamipan/core/logger.h"

namespace
{

// The host's absence is the only error these can report.
void notYet(std::once_flag &once, const char *name)
{
    std::call_once(once, [name] { yamipan::log::warning("IOP host not there yet: {} called", name); });
}

} // namespace

// ---- thbase, thevent, vblank ----------------------------------------------------------------------

int YamiPan_IopCreateThread(IopThreadParam *)
{
    static std::once_flag once;
    notYet(once, "CreateThread");
    return -1;
}

int YamiPan_IopStartThread(int, void *)
{
    static std::once_flag once;
    notYet(once, "StartThread");
    return -1;
}

int YamiPan_IopGetThreadId()
{
    static std::once_flag once;
    notYet(once, "GetThreadId");
    return -1;
}

int YamiPan_IopCreateEventFlag(IopEventFlagParam *)
{
    static std::once_flag once;
    notYet(once, "CreateEventFlag");
    return -1;
}

int YamiPan_IopSetEventFlag(int, u_int)
{
    static std::once_flag once;
    notYet(once, "SetEventFlag");
    return -1;
}

int YamiPan_IopWaitEventFlag(int, u_int, int, u_int *)
{
    static std::once_flag once;
    notYet(once, "WaitEventFlag");
    return -1;
}

int YamiPan_IopWaitVblankEnd()
{
    static std::once_flag once;
    notYet(once, "WaitVblankEnd");
    return -1;
}

// ---- intrman, loadcore, sysmem --------------------------------------------------------------------

int YamiPan_IopCpuSuspendIntr(int *oldstat)
{
    static std::once_flag once;
    notYet(once, "CpuSuspendIntr");
    if (oldstat != nullptr)
        *oldstat = 0;
    return -1;
}

int YamiPan_IopCpuResumeIntr(int)
{
    static std::once_flag once;
    notYet(once, "CpuResumeIntr");
    return -1;
}

void YamiPan_IopFlushDcache()
{
    static std::once_flag once;
    notYet(once, "FlushDcache");
}

void *YamiPan_IopAllocSysMemory(int, int, void *)
{
    static std::once_flag once;
    notYet(once, "AllocSysMemory");
    return nullptr;
}

int YamiPan_IopFreeSysMemory(void *)
{
    static std::once_flag once;
    notYet(once, "FreeSysMemory");
    return -1;
}

int YamiPan_IopQueryMemSize()
{
    static std::once_flag once;
    notYet(once, "QueryMemSize");
    return 0;
}

int YamiPan_IopQueryMaxFreeMemSize()
{
    static std::once_flag once;
    notYet(once, "QueryMaxFreeMemSize");
    return 0;
}

int YamiPan_IopQueryTotalFreeMemSize()
{
    static std::once_flag once;
    notYet(once, "QueryTotalFreeMemSize");
    return 0;
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

void sceSifSetRpcQueue(sceSifQueueData *, int)
{
    static std::once_flag once;
    notYet(once, "sceSifSetRpcQueue");
}

void sceSifRegisterRpc(sceSifServeData *, u_int, sceSifRpcFunc, void *, sceSifRpcFunc, void *,
                       sceSifQueueData *)
{
    static std::once_flag once;
    notYet(once, "sceSifRegisterRpc");
}

// On the IOP this loops for ever serving requests; with no host to serve, it returns.
void sceSifRpcLoop(sceSifQueueData *)
{
    static std::once_flag once;
    notYet(once, "sceSifRpcLoop");
}

// ---- the host mixer's volume groups ---------------------------------------------------------------

void YamiPan_VoiceSetGroup(int, int, int)
{
    static std::once_flag once;
    notYet(once, "YamiPan_VoiceSetGroup");
}
