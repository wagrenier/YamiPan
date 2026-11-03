// rx/rxsndstrstream.c -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// Functions below are stubs until reconstructed; see tools/reverse/stage.py.

#include "common/yamipan_stub.h"
#include "rx/rxsndstrstream.h"

// ---- stubs (tools/reverse/stubgen.py) ----

// stub of FS2 0x00165738
void rxSndStrStreamExit(void)
{
    YAMIPAN_STUB("rxSndStrStreamExit", 0x00165738);
}

// stub of FS2 0x001656e8
void rxSndStrStreamInit(void)
{
    YAMIPAN_STUB("rxSndStrStreamInit", 0x001656e8);
}

// stub of FS2 0x00165790
int rxSndStrStreamOpenFile(const char *name, void *buf, int bufSize)
{
    YAMIPAN_STUB("rxSndStrStreamOpenFile", 0x00165790);
    return {};
}

// stub of FS2 0x001657c0
int rxSndStrStreamOpenMemory(uintptr_t addr, int size, int iop)
{
    YAMIPAN_STUB("rxSndStrStreamOpenMemory", 0x001657c0);
    return {};
}

// stub of FS2 0x00165788
void rxSndStrStreamUpdate(void)
{
    YAMIPAN_STUB("rxSndStrStreamUpdate", 0x00165788);
}
