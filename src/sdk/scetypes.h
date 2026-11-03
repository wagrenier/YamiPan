// sdk/scetypes.h -- the EE's base types. Sizes follow the EE ABI (char 1, short 2, int 4, long 8,
// a 128-bit quadword), so the GS / VIF / DMA register structs keep their layout on the host.
#pragma once

#include <stddef.h>
#include <stdint.h>

using u_char = uint8_t;
using u_short = uint16_t;
using u_int = uint32_t;
using u_long = uint64_t;        // the EE's long is 64-bit
using s_char = int8_t;

using vu_char = volatile uint8_t;
using vu_short = volatile uint16_t;
using vu_int = volatile uint32_t;
using vu_long = volatile uint64_t;

__extension__ typedef __int128 long128;
__extension__ typedef unsigned __int128 u_long128;

using uint = unsigned int;
using byte = unsigned char;

// Four ints, 16-aligned as on the EE, so a member of this type lands at its EE offset.
typedef int qword[4] __attribute__((aligned(16)));
static_assert(sizeof(qword) == 16 && alignof(qword) == 16);
