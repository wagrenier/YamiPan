// sdk/sif.h -- IOP memory, as FS2 allocates it through the SIF. Addresses stay 32-bit (game code
// keeps them in u_int): the host answers offsets into its model of IOP memory.
#pragma once

#include "scetypes.h"

void *sceSifAllocSysMemory(int mode, u_int size, void *addr);
int sceSifFreeSysMemory(void *addr);
