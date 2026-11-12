// sdk/iop_host.h -- the IOP kernel's API as RXSNDDRV imports it: thbase, thevent, intrman, sysmem,
// loadcore and vblank. The IOP SDK's own names are macros onto these (thbase.h and the other shims),
// because CreateThread, StartThread and GetThreadId are also the EE kernel's (eekernel.h) and Win32's.
// RosePan's sdk/iop_host.h, cut to what is defined: sdk/iop.cpp until the IOP host exists.
#pragma once

#include "scetypes.h"

#include <stdint.h>

// IOP thread entries take no argument: the work pointer travels in `option`. It was a 32-bit word on
// the IOP; here it must be pointer-wide or a thread starts with a truncated pointer.
typedef struct
{
    u_int attr;
    uintptr_t option;
    void *entry;
    int stackSize;
    int initPriority;
} IopThreadParam;

typedef struct
{
    u_int attr;
    u_int option;
    u_int initPattern;
} IopEventFlagParam;

#define WEF_AND 0x00
#define WEF_OR 0x01
#define WEF_CLEAR 0x10

extern "C" {

int YamiPan_IopCreateThread(IopThreadParam *param);
int YamiPan_IopStartThread(int thid, void *arg);
int YamiPan_IopGetThreadId(void);

int YamiPan_IopCreateEventFlag(IopEventFlagParam *param);
int YamiPan_IopSetEventFlag(int evfid, u_int bitpattern);
int YamiPan_IopWaitEventFlag(int evfid, u_int bitpattern, int waitmode, u_int *resultpat);
int YamiPan_IopWaitVblankEnd(void);

int YamiPan_IopCpuSuspendIntr(int *oldstat);
int YamiPan_IopCpuResumeIntr(int oldstat);
void YamiPan_IopFlushDcache(void);

void *YamiPan_IopAllocSysMemory(int mode, int size, void *ptr);
int YamiPan_IopFreeSysMemory(void *ptr);
int YamiPan_IopQueryMemSize(void);
int YamiPan_IopQueryMaxFreeMemSize(void);
int YamiPan_IopQueryTotalFreeMemSize(void);

} // extern "C"
