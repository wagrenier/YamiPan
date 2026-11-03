// game/spkrom.cpp -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// Functions below are stubs until reconstructed; see tools/reverse/stage.py.

#include "common/yamipan_stub.h"
#include "game/gamehooks.h"
#include "game/streamring.h"

// ---- stubs (tools/reverse/stubgen.py) ----

// stub of FS2 0x001530d0
int StreamRing::filled()
{
    YAMIPAN_STUB("StreamRing::filled", 0x001530d0);
    return {};
}

// stub of FS2 0x001530e0
int StreamRing::freeSpace()
{
    YAMIPAN_STUB("StreamRing::freeSpace", 0x001530e0);
    return {};
}

// stub of FS2 0x00153100
int StreamRing::readPos()
{
    YAMIPAN_STUB("StreamRing::readPos", 0x00153100);
    return {};
}

// stub of FS2 0x001530f0
void StreamRing::write(const void *src, int size)
{
    YAMIPAN_STUB("StreamRing::write", 0x001530f0);
}

// stub of FS2 0x001527a0
void rxInit(const char *bootPath, int prio0, int prio1, int prio2)
{
    YAMIPAN_STUB("rxInit", 0x001527a0);
}

// stub of FS2 0x00152750
void rxTerm()
{
    YAMIPAN_STUB("rxTerm", 0x00152750);
}
