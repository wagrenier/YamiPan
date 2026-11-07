// sdpr2/sdpr2drv.c -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// Functions below are stubs until reconstructed; see tools/reverse/stage.py.

#include "common/yamipan_stub.h"
#include "sdpr2/sdpr2drv.h"

// ---- stubs (tools/reverse/stubgen.py) ----

// stub of FS2 0x00170088
int sdpr2DrvInit(const void *config)
{
    YAMIPAN_STUB("sdpr2DrvInit", 0x00170088);
    return {};
}

// stub of FS2 0x00170f90
int sdpr2DrvGetConfig(int *out)
{
    YAMIPAN_STUB("sdpr2DrvGetConfig", 0x00170f90);
    return {};
}

// stub of FS2 0x00170bc8
int sdpr2DrvGetStatus(int voice)
{
    YAMIPAN_STUB("sdpr2DrvGetStatus", 0x00170bc8);
    return {};
}

// stub of FS2 0x00170508
int sdpr2DrvPause(int voice, int on)
{
    YAMIPAN_STUB("sdpr2DrvPause", 0x00170508);
    return {};
}

// stub of FS2 0x00170120
int sdpr2DrvPlay(int xvab, int prog, int note, int volL, int volR,
                  int signL, int signR)
{
    YAMIPAN_STUB("sdpr2DrvPlay", 0x00170120);
    return {};
}

// stub of FS2 0x00170280
int sdpr2DrvPlayEx(int xvab, int prog, int note, int volL, int volR,
                    int signL, int signR, int preset)
{
    YAMIPAN_STUB("sdpr2DrvPlayEx", 0x00170280);
    return {};
}

// stub of FS2 0x00170998
int sdpr2DrvSetAttr(int voice, int attr)
{
    YAMIPAN_STUB("sdpr2DrvSetAttr", 0x00170998);
    return {};
}

// stub of FS2 0x00170620
int sdpr2DrvSetPitch(int voice, int pitch)
{
    YAMIPAN_STUB("sdpr2DrvSetPitch", 0x00170620);
    return {};
}

// stub of FS2 0x00170738
int sdpr2DrvSetPitchF(int voice, float pitch)
{
    YAMIPAN_STUB("sdpr2DrvSetPitchF", 0x00170738);
    return {};
}

// stub of FS2 0x00170850
int sdpr2DrvSetVolume(int voice, int volL, int volR, int signL,
                       int signR)
{
    YAMIPAN_STUB("sdpr2DrvSetVolume", 0x00170850);
    return {};
}

// stub of FS2 0x001703f0
int sdpr2DrvStop(int voice, int mode)
{
    YAMIPAN_STUB("sdpr2DrvStop", 0x001703f0);
    return {};
}
