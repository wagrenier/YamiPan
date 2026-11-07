// sysmem.h -- IOP system memory, as RXSNDDRV includes it (macros onto iop_host.h).
#pragma once

#include "iop_host.h"

#define AllocSysMemory YamiPan_IopAllocSysMemory
#define FreeSysMemory YamiPan_IopFreeSysMemory
#define QueryMemSize YamiPan_IopQueryMemSize
#define QueryMaxFreeMemSize YamiPan_IopQueryMaxFreeMemSize
#define QueryTotalFreeMemSize YamiPan_IopQueryTotalFreeMemSize
