// lib/unassigned_stubs.cpp -- Forbidden Siren 2 module (file from the inference in yamipan_functions.txt).
// Functions below are stubs until reconstructed; see tools/reverse/stage.py.
#include "rena/rena_draw.h"
#include "rena/rena_2d_texture_tim2.h"

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

// stub of FS2 0x002ad070

// stub of FS2 address unknown (1 candidates)
// stub of FS2 0x001d3a40
// stub of FS2 0x00185af0

// stub of FS2 0x002c42c0
int runExtraMode(int arg)
{
    YAMIPAN_STUB("runExtraMode", 0x002c42c0);
    return {};
}

// stub of FS2 0x002acff0

// stub of FS2 address unknown (0 candidates)

// stub of FS2 address unknown (0 candidates)

// stub of FS2 address unknown (0 candidates)

// stub of FS2 0x00185210

// stub of FS2 address unknown (0 candidates)


// stub of FS2 0x001861b0

// stub of FS2 0x00186160

// stub of FS2 0x00185fb0

// stub of FS2 0x00185bb0

// stub of FS2 0x00185c40

// stub of FS2 address unknown (1 candidates)
void StageObject::onViewTop()
{
    YAMIPAN_STUB("StageObject::onViewTop", 0x00000000);
}

// stub of FS2 0x0029f050
// stub of FS2 address unknown (0 candidates)
// stub of FS2 address unknown (0 candidates)

// stub of FS2 0x00239bb0
// stub of FS2 0x002ad030

// stub of FS2 0x00289800
// stub of FS2 0x00289b20
// stub of FS2 address unknown (1 candidates)
// stub of FS2 address unknown (1 candidates)
// stub of FS2 address unknown (1 candidates)
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


// stub of FS2 0x001d3d20
// stub of FS2 address unknown (1 candidates)
// stub of FS2 address unknown (1 candidates)
bool StageObject::followsPlayerLight()
{
    YAMIPAN_STUB("StageObject::followsPlayerLight", 0x00000000);
    return {};
}

// stub of FS2 0x00207440


// stub of FS2 0x00207aa0
// ---- by hand: the character's parts, until their TUs (next in the queue) ----

// stub of FS2 0x00239bc0
// stub of FS2 0x00212020
bool StageCharacter::canTakeOver()
{
    YAMIPAN_STUB("StageCharacter::canTakeOver", 0x00212020);
    return {};
}

// stub of FS2 0x002119d0
bool StageCharacter::isCompanionWalking()
{
    YAMIPAN_STUB("StageCharacter::isCompanionWalking", 0x002119d0);
    return {};
}

// stub of FS2 0x001e05b0
bool PlayerMotion::winsStruggle(StageObject *other)
{
    YAMIPAN_STUB("PlayerMotion::winsStruggle", 0x001e05b0);
    return {};
}

// stub of FS2 0x00294280
void elevatorCall(StageObject *elevator, int floor)
{
    YAMIPAN_STUB("elevatorCall", 0x00294280);
}

// stub of FS2 0x00294140
bool elevatorCanCall(StageObject *elevator, const float *position, const float *rotation, int floor)
{
    YAMIPAN_STUB("elevatorCanCall", 0x00294140);
    return {};
}


// stub of FS2 address unknown (0 candidates)
u_int elevatorCalledFloors(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorCalledFloors", 0x00000000);
    return {};
}

// stub of FS2 0x00294c30
bool elevatorIsAtB1(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsAtB1", 0x00294c30);
    return {};
}

// stub of FS2 0x00294c70
bool elevatorIsAtB3(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsAtB3", 0x00294c70);
    return {};
}

// stub of FS2 0x00294b60
bool elevatorIsPhase1(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsPhase1", 0x00294b60);
    return {};
}

// stub of FS2 0x00294b00
bool elevatorIsPhase2(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsPhase2", 0x00294b00);
    return {};
}

// stub of FS2 0x00294b30
bool elevatorIsPhase3(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsPhase3", 0x00294b30);
    return {};
}

// stub of FS2 0x00294ad0
bool elevatorIsPhase4(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorIsPhase4", 0x00294ad0);
    return {};
}

// stub of FS2 0x00294f90
void elevatorMove(StageObject *elevator, bool toB3, bool fast)
{
    YAMIPAN_STUB("elevatorMove", 0x00294f90);
}

// stub of FS2 0x00293fd0
void elevatorDoorPlace(StageObject *elevator, float *out, int floor)
{
    YAMIPAN_STUB("elevatorDoorPlace", 0x00293fd0);
}

// stub of FS2 0x00294be0
bool elevatorOpenAtB1(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorOpenAtB1", 0x00294be0);
    return {};
}

// stub of FS2 0x00294b90
bool elevatorOpenAtB3(StageObject *elevator)
{
    YAMIPAN_STUB("elevatorOpenAtB3", 0x00294b90);
    return {};
}

// stub of FS2 0x00293c60
void elevatorSwitchPlace(StageObject *elevator, float *place, float *direction, int which)
{
    YAMIPAN_STUB("elevatorSwitchPlace", 0x00293c60);
}


