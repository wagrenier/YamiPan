// rx/rxsndstrpcm.c -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// Functions below are stubs until reconstructed; see tools/reverse/stage.py.

#include "common/yamipan_stub.h"
#include "rx/rxsndstrpcm.h"

// ---- stubs (tools/reverse/stubgen.py) ----

// stub of FS2 0x00164b30
void rxSndStrPcmClose(int handle, int async)
{
    YAMIPAN_STUB("rxSndStrPcmClose", 0x00164b30);
}

// stub of FS2 0x001643e0
void rxSndStrPcmExit(void)
{
    YAMIPAN_STUB("rxSndStrPcmExit", 0x001643e0);
}

// stub of FS2 0x00164bb8
int rxSndStrPcmGetHandle(int handle)
{
    YAMIPAN_STUB("rxSndStrPcmGetHandle", 0x00164bb8);
    return {};
}

// stub of FS2 0x00164398
void rxSndStrPcmInit(void)
{
    YAMIPAN_STUB("rxSndStrPcmInit", 0x00164398);
}

// stub of FS2 0x001646b8
int rxSndStrPcmOpen(RxSndStrSlot *slot)
{
    YAMIPAN_STUB("rxSndStrPcmOpen", 0x001646b8);
    return {};
}

// stub of FS2 0x00164bd8
void rxSndStrPcmPauseRead(int handle, int pause)
{
    YAMIPAN_STUB("rxSndStrPcmPauseRead", 0x00164bd8);
}

// stub of FS2 0x00164c08
int rxSndStrPcmSyncRead(int handle, int arg)
{
    YAMIPAN_STUB("rxSndStrPcmSyncRead", 0x00164c08);
    return {};
}

// stub of FS2 0x00164478
void rxSndStrPcmUpdate(void)
{
    YAMIPAN_STUB("rxSndStrPcmUpdate", 0x00164478);
}
