// game/stagealert.cpp -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// Functions below are stubs until reconstructed; see tools/reverse/stage.py.

#include "common/yamipan_stub.h"
#include "game/gamehooks.h"

// ---- stubs (tools/reverse/stubgen.py) ----

// stub of FS2 0x00264070
StageAlert::StageAlert()
{
    YAMIPAN_STUB("StageAlert::StageAlert", 0x00264070);
}

// stub of FS2 0x00264000
StageAlert::~StageAlert()
{
    YAMIPAN_STUB("StageAlert::~StageAlert", 0x00264000);
}

// stub of FS2 0x00263ba0
void StageAlert::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    YAMIPAN_STUB("StageAlert::draw", 0x00263ba0);
}

// stub of FS2 0x00263c10
u_long StageAlert::update()
{
    YAMIPAN_STUB("StageAlert::update", 0x00263c10);
    return {};
}

// stub of FS2 0x00263ed0
bool StageAlert::flash()
{
    YAMIPAN_STUB("StageAlert::flash", 0x00263ed0);
    return {};
}
