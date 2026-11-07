// loadcore.h -- the IOP module loader, as RXSNDDRV includes it (macros onto iop_host.h).
#pragma once

#include "iop_host.h"

// start()'s answer: 0 keeps the module resident, 1 unloads it.
#define MODULE_RESIDENT_END 0
#define MODULE_NO_RESIDENT_END 1

#define FlushDcache YamiPan_IopFlushDcache
