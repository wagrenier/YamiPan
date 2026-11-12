// yamipan/yamipan_memory.h -- a seam game code includes (cpp-port.md "Scope"). EE addresses only
// reach the host in rx's exception dump: there is no emulated EE memory behind them.
#pragma once

#include <stdint.h>

void *YamiPan_GetHostPointer(uintptr_t address);
