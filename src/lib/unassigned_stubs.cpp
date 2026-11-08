// lib/unassigned_stubs.cpp -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// Functions below are stubs until reconstructed; see tools/reverse/stage.py.
#include "rena/rena_draw.h"
#include "rena/rena_2d_texture_tim2.h"
#include "game/kunitoris.h"

#include <string.h>

#include "common/yamipan_stub.h"
#include "game/characterbrain.h"
#include "game/characterroute.h"
#include "game/fontdata.h"
#include "game/gadgetview.h"
#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/memcard.h"
#include "game/modelpose.h"
#include "game/playermotion.h"
#include "game/stagenode.h"
#include "game/stageobjlist.h"
#include "game/stageparticle.h"
#include "game/stageitem.h"
#include "game/stagescene.h"
#include "game/stringlist.h"
#include "game/uisequence.h"
#include "game/uiwidget.h"
#include "game/vu0math.h"

// ---- stubs (tools/reverse/stubgen.py) ----

// stub of FS2 address unknown (1 candidates)
void StageObject::onViewTop()
{
    YAMIPAN_STUB("StageObject::onViewTop", 0x00000000);
}

// stub of FS2 address unknown (1 candidates)
void StageObject::cameraStep()
{
    YAMIPAN_STUB("StageObject::cameraStep", 0x00000000);
}

// stub of FS2 address unknown (1 candidates)
void StageObject::onLinkChanged()
{
    YAMIPAN_STUB("StageObject::onLinkChanged", 0x00000000);
}

// stub of FS2 address unknown (1 candidates)
bool StageObject::followsPlayerLight()
{
    YAMIPAN_STUB("StageObject::followsPlayerLight", 0x00000000);
    return {};
}
