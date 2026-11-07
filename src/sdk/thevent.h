// thevent.h -- IOP event flags, as RXSNDDRV includes them (macros onto iop_host.h).
#pragma once

#include "iop_host.h"

#define EventFlagParam IopEventFlagParam
#define CreateEventFlag YamiPan_IopCreateEventFlag
#define SetEventFlag YamiPan_IopSetEventFlag
// The interrupt-context form: the host has no separate one.
#define iSetEventFlag YamiPan_IopSetEventFlag
#define WaitEventFlag YamiPan_IopWaitEventFlag
