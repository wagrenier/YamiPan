// yamipan/os/yamipan_time.h -- a seam game code includes (cpp-port.md "Scope"): where a spin loop of
// the ROM would hold a host core, rx sleeps instead.
#pragma once

#include <stdint.h>

void YamiPan_Sleep(unsigned int milliseconds);
void YamiPan_SleepNs(uint64_t nanoseconds);
