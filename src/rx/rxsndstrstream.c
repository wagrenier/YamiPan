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

// stub of FS2 0x00165938
void rxSndStrStreamClose(int source, int abort)
{
    YAMIPAN_STUB("rxSndStrStreamClose", 0x00165938);
}

// stub of FS2 0x00165e70
void rxSndStrStreamConsume(int source, int size)
{
    YAMIPAN_STUB("rxSndStrStreamConsume", 0x00165e70);
}

// stub of FS2 0x00165ee0
const u_char * rxSndStrStreamGetData(int source)
{
    YAMIPAN_STUB("rxSndStrStreamGetData", 0x00165ee0);
    return {};
}

// stub of FS2 0x00165df8
int rxSndStrStreamGetFilled(int source)
{
    YAMIPAN_STUB("rxSndStrStreamGetFilled", 0x00165df8);
    return {};
}

// stub of FS2 0x00165c20
int rxSndStrStreamGetReadable(int source, int size)
{
    YAMIPAN_STUB("rxSndStrStreamGetReadable", 0x00165c20);
    return {};
}

// stub of FS2 0x00165ba8
int rxSndStrStreamGetRingTags(int source, RxFileTagHit *out, int max, int size)
{
    YAMIPAN_STUB("rxSndStrStreamGetRingTags", 0x00165ba8);
    return {};
}

// stub of FS2 0x00165f58
int rxSndStrStreamIsEnd(int source)
{
    YAMIPAN_STUB("rxSndStrStreamIsEnd", 0x00165f58);
    return {};
}

// stub of FS2 0x00166008
void rxSndStrStreamPause(int source, int pause)
{
    YAMIPAN_STUB("rxSndStrStreamPause", 0x00166008);
}

// stub of FS2 0x00165a38
void rxSndStrStreamSeek(int source, u_int pos)
{
    YAMIPAN_STUB("rxSndStrStreamSeek", 0x00165a38);
}

// stub of FS2 0x00165ac8
void rxSndStrStreamSetMarkers(int source, const RxFileMarkerDef *defs, int count, void *arg)
{
    YAMIPAN_STUB("rxSndStrStreamSetMarkers", 0x00165ac8);
}

// stub of FS2 0x00165b38
void rxSndStrStreamSetTags(int source, const RxFileTagDef *defs, int count)
{
    YAMIPAN_STUB("rxSndStrStreamSetTags", 0x00165b38);
}

// stub of FS2 0x00166068
int rxSndStrStreamSync(int source, int arg)
{
    YAMIPAN_STUB("rxSndStrStreamSync", 0x00166068);
    return {};
}
