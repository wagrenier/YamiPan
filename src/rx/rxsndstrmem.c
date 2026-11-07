// rx/rxsndstrmem.c -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// Functions below are stubs until reconstructed; see tools/reverse/stage.py.

#include "common/yamipan_stub.h"
#include "rx/rxsndstrmem.h"

// ---- stubs (tools/reverse/stubgen.py) ----

// stub of FS2 0x00166178
void rxSndStrMemExit(void)
{
    YAMIPAN_STUB("rxSndStrMemExit", 0x00166178);
}

// stub of FS2 0x001660e0
void rxSndStrMemInit(void)
{
    YAMIPAN_STUB("rxSndStrMemInit", 0x001660e0);
}

// stub of FS2 0x001661b8
void rxSndStrMemUpdate(void)
{
    YAMIPAN_STUB("rxSndStrMemUpdate", 0x001661b8);
}

// stub of FS2 0x001662b0
void rxSndStrMemClose(int id)
{
    YAMIPAN_STUB("rxSndStrMemClose", 0x001662b0);
}

// stub of FS2 0x00166418
void rxSndStrMemConsume(int id, int size)
{
    YAMIPAN_STUB("rxSndStrMemConsume", 0x00166418);
}

// stub of FS2 0x001665b8
const u_char * rxSndStrMemGetData(int id)
{
    YAMIPAN_STUB("rxSndStrMemGetData", 0x001665b8);
    return {};
}

// stub of FS2 0x00166370
int rxSndStrMemGetReadable(int id)
{
    YAMIPAN_STUB("rxSndStrMemGetReadable", 0x00166370);
    return {};
}

// stub of FS2 0x00166b18
int rxSndStrMemGetSize(int id)
{
    YAMIPAN_STUB("rxSndStrMemGetSize", 0x00166b18);
    return {};
}

// stub of FS2 0x00166a78
int rxSndStrMemGetTags(int id, RxFileTagHit *out, int max, int size)
{
    YAMIPAN_STUB("rxSndStrMemGetTags", 0x00166a78);
    return {};
}

// stub of FS2 0x00166650
int rxSndStrMemIsEnd(int id)
{
    YAMIPAN_STUB("rxSndStrMemIsEnd", 0x00166650);
    return {};
}

// stub of FS2 0x001661c0
int rxSndStrMemOpen(uintptr_t addr, int size, int iop)
{
    YAMIPAN_STUB("rxSndStrMemOpen", 0x001661c0);
    return {};
}

// stub of FS2 0x00166320
void rxSndStrMemSeek(int id, u_int pos)
{
    YAMIPAN_STUB("rxSndStrMemSeek", 0x00166320);
}

// stub of FS2 0x00166688
void rxSndStrMemSetMarkers(int id, const RxFileMarkerDef *defs, int count, void *arg)
{
    YAMIPAN_STUB("rxSndStrMemSetMarkers", 0x00166688);
}

// stub of FS2 0x00166890
void rxSndStrMemSetTags(int id, const RxFileTagDef *defs, int count)
{
    YAMIPAN_STUB("rxSndStrMemSetTags", 0x00166890);
}
