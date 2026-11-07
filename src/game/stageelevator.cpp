/* ========================================================================
 *  game/stageelevator.cpp -- the elevator (game/stageelevator.h).
 *  0x2939b0..0x295470. The thunks 0x295420..0x295468 of the side and the
 *  collision bases are the host's multiple inheritance.
 * ======================================================================== */
#include <math.h>
#include <string.h>

#include "game/characterground.h"
#include "game/gamehooks.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/rumblelist.h"
#include "game/stagecollision.h"
#include "game/stageelevator.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
#include "game/vu0math.h"

namespace {

constexpr char kBoneCar[]         = "bn_bx";        // data 0x3097d8
constexpr char kBoneDoorB1Left[]  = "bn_b1_dl";     // data 0x3097e0
constexpr char kBoneDoorB1Right[] = "bn_b1_dr";     // data 0x3097f0
constexpr char kBoneDoorB3Left[]  = "bn_b3_dl";     // data 0x309800
constexpr char kBoneDoorB3Right[] = "bn_b3_dr";     // data 0x309810
constexpr char kBonePanelInB1[]   = "bn_bx_ib1";    // data 0x309950
constexpr char kBonePanelInB3[]   = "bn_bx_ib3";    // data 0x309990
constexpr char kBoneCarRight[]    = "bn_bx_dr";     // data 0x309960
constexpr char kBoneCarLeft[]     = "bn_bx_dl";     // data 0x3099a0
constexpr char kBonePanelB1[]     = "bn_b1_i";      // data 0x309970
constexpr char kBonePanelB3[]     = "bn_b3_i";      // data 0x309978

constexpr char kSoundButton[]     = "ELV_BUTTON";   // data 0x309980
constexpr char kSoundDoorEnd[]    = "ELV_DR_END";   // data 0x3099b0
constexpr char kSoundDoorStart[]  = "ELV_DR_STRT";  // data 0x3099e0
constexpr char kSoundDoorReopen[] = "ELV_DR_REOP";  // data 0x309a50
constexpr char kSoundMove[]       = "ELV_MOVE";     // data 0x3099f0
constexpr char kSoundArrive[]     = "ELV_ARRIVE";   // data 0x309a20

constexpr char kRumbleStop[]      = "ELEVATOR1";    // data 0x309a00: a ride's two ends
constexpr char kRumbleRide[]      = "ELEVATOR2";    // data 0x309a10: while it rides
constexpr char kRumbleCaught[]    = "ELEVATOR3";    // data 0x309a60: a door hit someone

// What lights up with a landing's doors (object group 5).
constexpr char kLightB1[]         = "ENV_SL61";     // data 0x3099c0
constexpr char kLightB3[]         = "ENV_SL62";     // data 0x3099d0

// The meshes and the bones they ride on; the first is the car's.
const char *const kMeshNames[kElevatorMeshes] = {   // data 0x2fb5f0
    "oZS99_01_bx", "oZS99_01_1l", "oZS99_01_1r", "oZS99_01_3l", "oZS99_01_3r",
};
const char *const kMeshBones[kElevatorMeshes] = {   // data 0x2fb610
    kBoneCar, kBoneDoorB1Left, kBoneDoorB1Right, kBoneDoorB3Left, kBoneDoorB3Right,
};
// By ElevatorMotion.
const char *const kMotionNames[kElevatorMotions] = {    // data 0x2fb630
    "oZS99_01_op_b1", "oZS99_01_op_b3", "oZS99_01_cl_b1", "oZS99_01_cl_b3",
    "oZS99_01_mt_b1", "oZS99_01_mt_b3", "oZS99_01_so_b1", "oZS99_01_so_b3",
    "oZS99_01_sc_b1", "oZS99_01_sc_b3", "oZS99_01_fb1_on", "oZS99_01_fb1_of",
    "oZS99_01_fb3_on", "oZS99_01_fb3_of", "oZS99_01_bb1_on", "oZS99_01_bb1_of",
    "oZS99_01_bb3_on", "oZS99_01_bb3_of",
};

constexpr int   kMeshCellSize = 0x96;       // the grid of each mesh
constexpr float kReach        = 150.0f;     // from a panel
constexpr float kReachAngle   = 0.7853982f; // pi / 4, between the way to it and the facing
constexpr float kReachFacing  = 0.577f;     // cos of the panel's cone
constexpr float kSwitchSide   = 32.0f;      // a landing's switch from its panel
constexpr float kSwitchOut    = -50.0f;
constexpr float kInsideSide   = -56.0f;     // the car's
constexpr float kInsideOut    = 72.0f;
constexpr float kDoorWidth    = 35.0f;      // a closing door's edge, from its bone
constexpr float kPiF          = 3.1415927f;

const sceVu0FVECTOR kOnBone = { 0.0f, 0.0f, 0.0f, 0.0f };   // data 0x309940

constexpr float kNotWorkedOut = 1.0f;       // w of a kept place

} // namespace

const sceVu0FVECTOR *StageElevator::boneMatrix(const char *bone)
{
    return m_model->boneMap.find(bone)->value->getWorldMatrix();
}

const sceVu0FVECTOR *StageElevator::panelPlace(sceVu0FVECTOR place, const char *panel, const char *door)
{
    const sceVu0FVECTOR *matrix = boneMatrix(panel);
    place[0] = matrix[3][0];
    place[1] = matrix[3][1];
    place[2] = boneMatrix(door)[3][2];
    place[3] = 0.0f;
    return matrix;
}

// A landing's panel is kept, the car's moves with it. Anything but the two
// landing buttons is the car's, whichever way it goes: bn_bx_ib1 for both.
// FS2 0x002939e0
// callcheck: find, find, find, find, find, find (each with getWorldMatrix, panelPlace is inline)

void StageElevator::getCallPlace(float *place, float *facing, int button)
{
    if (button != ElevatorButton_CallB1 && button != ElevatorButton_CallB3) {
        const sceVu0FVECTOR *panel = panelPlace(place, kBonePanelInB1, kBoneCarRight);
        facing[0] = panel[0][0];
        facing[1] = panel[0][1];
        facing[2] = panel[0][2];
        facing[3] = 0.0f;
        return;
    }
    const bool b3 = button == ElevatorButton_CallB3;
    const int landing = b3 ? ElevatorLanding_B3 : ElevatorLanding_B1;
    if (m_callPlace[landing][3] == kNotWorkedOut) {
        const sceVu0FVECTOR *panel = panelPlace(m_callPlace[landing], b3 ? kBonePanelB3 : kBonePanelB1,
                                                b3 ? kBoneDoorB3Right : kBoneDoorB1Right);
        m_callFacing[landing][0] = -panel[0][0];
        m_callFacing[landing][1] = -panel[0][1];
        m_callFacing[landing][2] = -panel[0][2];
        m_callFacing[landing][3] = 0.0f;
    }
    memcpy(place, m_callPlace[landing], sizeof(sceVu0FVECTOR));
    memcpy(facing, m_callFacing[landing], sizeof(sceVu0FVECTOR));
}

// Where the route brings a character to press the button.
// FS2 0x00293c60
// callcheck: find, find, vecDirection, find, find, vecDirection, find, find, vecDirection (each find with getWorldMatrix)

void StageElevator::getSwitchPlace(float *place, float *direction, int button)
{
    sceVu0FVECTOR base;
    if (button != ElevatorButton_CallB1 && button != ElevatorButton_CallB3) {
        const sceVu0FVECTOR *panel = panelPlace(base, kBonePanelInB1, kBoneCarRight);
        place[0] = panel[1][0] * kInsideSide + panel[0][0] * kInsideOut + base[0];
        place[1] = panel[1][1] * kInsideSide + panel[0][1] * kInsideOut + base[1];
        place[2] = base[2];
        place[3] = 0.0f;
        vecDirection(direction, base, place);
        return;
    }
    const bool b3 = button == ElevatorButton_CallB3;
    const int landing = b3 ? ElevatorLanding_B3 : ElevatorLanding_B1;
    float *kept = m_switchPlace[landing];
    if (kept[3] == kNotWorkedOut) {
        const sceVu0FVECTOR *panel = panelPlace(base, b3 ? kBonePanelB3 : kBonePanelB1,
                                                b3 ? kBoneDoorB3Right : kBoneDoorB1Right);
        kept[0] = panel[1][0] * kSwitchSide + panel[0][0] * kSwitchOut + base[0];
        kept[1] = panel[1][1] * kSwitchSide + panel[0][1] * kSwitchOut + base[1];
        kept[2] = base[2];
        kept[3] = 0.0f;
        vecDirection(m_switchDirection[landing], base, kept);
    }
    memcpy(place, kept, sizeof(sceVu0FVECTOR));
    memcpy(direction, m_switchDirection[landing], sizeof(sceVu0FVECTOR));
}

// The left door's x and z, and the car's y.
// FS2 0x00293fd0
// callcheck: find, find, find, find (each with getWorldMatrix)

void StageElevator::getDoorPlace(float *out, int landing)
{
    const int side = landing == ElevatorLanding_B1 ? ElevatorLanding_B1 : ElevatorLanding_B3;
    float *kept = m_doorPlace[side];
    if (kept[3] == kNotWorkedOut) {
        const sceVu0FVECTOR *door = boneMatrix(side == ElevatorLanding_B1 ? kBoneDoorB1Left : kBoneDoorB3Left);
        kept[0] = door[3][0];
        kept[1] = boneMatrix(kBoneCar)[3][1];
        kept[2] = door[3][2];
        kept[3] = 0.0f;
    }
    memcpy(out, kept, sizeof(sceVu0FVECTOR));
}

// FS2 0x00294140
// callcheck: getCallPlace, vecDistance, atan2Octant, addAngle, addAngle, vecNormalize, vecDot

bool StageElevator::canCall(const float *position, const float *rotation, int button)
{
    if (button == ElevatorButton_Release)
        return isOpen();
    sceVu0FVECTOR place, facing;
    getCallPlace(place, facing, button);
    if (kReach <= vecDistance(position, place))
        return false;
    sceVu0FVECTOR away;
    for (int i = 0; i < 4; i++)
        away[i] = position[i] - place[i];
    float angle = addAngle(atan2Octant(away[0], -away[1]), -rotation[2]);
    angle = addAngle(angle, -kPiF);
    if (kReachAngle <= fabsf(angle))
        return false;
    vecNormalize(away, away);
    return kReachFacing <= vecDot(away, facing);
}

// Each button once: the pose queues what the car must do, the panel clicks
// unless the queue's answer is 1, and the lamp stays lit only for the landing
// the car is not on. The ROM has this four times.
// FS2 0x00294280
// callcheck: release, callInsideB3, find, playNamedHeard2, callInsideB1, find, playNamedHeard2, callFloorB3, find, playNamedHeard2, callFloorB1, find, playNamedHeard2 (each find with getWorldMatrix)

void StageElevator::press(int button)
{
    if (m_model == nullptr)
        return;
    ElevatorPose *elevator = pose();
    if (button == ElevatorButton_Release) {
        elevator->release();
        m_called = 0;
        return;
    }
    if (button < ElevatorButton_CallB1 || button > ElevatorButton_InsideB3)
        return;
    const u_int bit = 1u << button;
    if ((m_called & bit) != 0)
        return;
    int queued;
    const char *panel;
    bool toB3;
    switch (button) {
    case ElevatorButton_InsideB3:
        queued = elevator->callInsideB3();
        panel = kBonePanelInB3;
        toB3 = true;
        break;
    case ElevatorButton_InsideB1:
        queued = elevator->callInsideB1();
        panel = kBonePanelInB1;
        toB3 = false;
        break;
    case ElevatorButton_CallB3:
        queued = elevator->callFloorB3();
        panel = kBonePanelB3;
        toB3 = true;
        break;
    default:
        queued = elevator->callFloorB1();
        panel = kBonePanelB1;
        toB3 = false;
        break;
    }
    if (queued != 1)
        playNamedHeard2(kSoundButton, boneMatrix(panel)[3], nullptr);
    const bool atB1 = (elevator->state & kElevatorAtB1) != 0;
    if (toB3 == atB1)
        m_called |= bit;
}

// Who feels the ride: the player, or else the first character standing on it.
void StageElevator::findRider()
{
    StageObject *player = gStageScene->player;
    m_rider = nullptr;
    if (player != nullptr && player->m_ground->floorObject == this) {   // PC: the ROM reads through a null player
        m_rider = player;
        return;
    }
    StageObject **slot = gStageScene->objects->m_lists[StageObjGroup_Characters].begin();
    for (; slot != nullptr; slot = StageObjArray::next(slot)) {
        if ((*slot)->m_active != 0 && (*slot)->m_ground->floorObject == this) {
            m_rider = *slot;
            return;
        }
    }
}

// A ride began: the sound goes with the car.
void StageElevator::startRide()
{
    stopSound(m_rideSound);
    m_rideSound = playNamedHeard2(kSoundMove, kOnBone, m_model->boneMap.find(kBoneCar)->value);
    RumbleList *rumble = gStageScene->rumble;
    if (rumble == nullptr)
        return;
    findRider();
    if (m_rider != nullptr) {
        rumble->addNamed(kRumbleStop, m_rider);
        rumble->addNamed(kRumbleRide, m_rider);
    }
}

// A ride ended: the lamps of the landing it reached go out.
void StageElevator::endRide(u_int state)
{
    stopSound(m_rideSound);
    m_rideSound = nullptr;
    playNamedHeard2(kSoundArrive, boneMatrix(kBoneCar)[3], nullptr);
    if ((state & kElevatorAtB1) != 0)
        m_called &= ~((1u << ElevatorButton_InsideB1) | (1u << ElevatorButton_CallB1));
    else
        m_called &= ~((1u << ElevatorButton_InsideB3) | (1u << ElevatorButton_CallB3));
    if (m_rider != nullptr) {
        RumbleList *rumble = gStageScene->rumble;
        rumble->removeNamed(kRumbleRide);
        rumble->addNamed(kRumbleStop, m_rider);
        m_rider = nullptr;
    }
}

// What the pose's new state sounds like: the doors' first and last frames, the
// light of the landing (on when its doors open, off when they close), a ride.
// FS2 0x00294560
// callcheck: find, playNamedHeard2, find5, find5, setTreeScriptFlag, find, playNamedHeard2, find, playNamedHeard2, find5, find5, setTreeScriptFlag, find, playNamedHeard2, stopSound, find, playNamedHeard2, addNamed, addNamed, stopSound, find, playNamedHeard2, removeNamed, addNamed

void StageElevator::sideHook3()
{
    if (m_model == nullptr || m_model->pose == nullptr)
        return;     // PC: an oZS99_01 no motion was given to
    const u_int state = pose()->state;
    const u_int step = state & kElevatorStepMask;
    const u_int lastStep = m_lastState & kElevatorStepMask;
    const char *doorSound = nullptr;
    switch (step) {
    case ElevatorStep_Open:
        if (lastStep == ElevatorStep_Opening)
            doorSound = kSoundDoorEnd;
        break;
    case ElevatorStep_Closed:
    case ElevatorStep_RideToB3:
    case ElevatorStep_RideToB1:
        if (lastStep == ElevatorStep_Closing)
            doorSound = kSoundDoorEnd;
        break;
    case ElevatorStep_Opening:
    case ElevatorStep_Closing:
        if (lastStep != step) {
            const bool atB1 = (state & kElevatorAtB1) != 0;
            StageObject *light = gStageScene->objects->find5(atB1 ? kLightB1 : kLightB3, 0);
            if (light != nullptr)
                light->setTreeScriptFlag(step == ElevatorStep_Opening);
            doorSound = kSoundDoorStart;
        }
        break;
    default:
        break;
    }
    if (doorSound != nullptr)
        playNamedHeard2(doorSound, boneMatrix(kBoneCarLeft)[3], nullptr);

    const bool riding = step == ElevatorStep_RideToB3 || step == ElevatorStep_RideToB1;
    const bool wasRiding = lastStep == ElevatorStep_RideToB3 || lastStep == ElevatorStep_RideToB1;
    if (riding && !wasRiding)
        startRide();
    else if (!riding && wasRiding)
        endRide(state);
    m_lastState = pose()->state;
}

// A character in the doors while they close: the mesh it touched tells which
// door, and a line along that door's edge at the sphere's height says whether
// the sphere is really in the way.
// FS2 0x00294cb0
// callcheck: collisionHook0, getCollisionObject, getKind, FUN_001f4c30, find, find, scaleVec4, hitsSegment0, reopenDoors, find, playNamedHeard2, getCollisionObject, addNamed (each find with getWorldMatrix)

int StageElevator::collisionHook0(CollisionContact *contact, const float *normal, StageCollisionFace *face,
                                  float depth)
{
    StageMapObject::collisionHook0(contact, normal, face, depth);
    if (contact->self->category != kCollisionMesh)
        return 0;
    ElevatorPose *elevator = pose();
    if ((elevator->state & kElevatorStepMask) != ElevatorStep_Closing)
        return 0;
    CollisionBody *other = contact->body;
    if (other->owner->getCollisionObject()->getKind() != StageObject_Character)
        return 0;
    if (other->category != kGroundWallBody)
        return 0;

    // The mesh's index: 0, the car, is no door.
    u_int door = 1;
    while (door < m_collisionCount && contact->self->mesh != m_collisions[door])
        door++;
    if (door >= kElevatorMeshes)
        return 0;

    const BoundingSphere *sphere = &other->spheres.at(contact->bodyIndex)->value;
    const sceVu0FVECTOR *matrix = boneMatrix(kMeshBones[door]);
    sceVu0FVECTOR from, to;
    memcpy(from, matrix[3], sizeof(from));
    from[2] = sphere->center[2];
    // The left doors are the odd meshes.
    scaleVec4((door & 1) != 0 ? kDoorWidth : -kDoorWidth, to, matrix[1]);
    for (int i = 0; i < 4; i++)
        to[i] += from[i];
    if (!sphere->hitsSegment0(from, to))
        return 0;

    elevator->reopenDoors();
    playNamedHeard2(kSoundDoorReopen, boneMatrix(kBoneCarLeft)[3], nullptr);
    if (RumbleList *rumble = gStageScene->rumble)
        rumble->addNamed(kRumbleCaught, other->owner->getCollisionObject());
    return 0;
}

// FS2 0x00294f90
// callcheck: setScene, stopSound

void StageElevator::placeAt(bool atB3, bool open)
{
    if (m_model == nullptr)
        return;
    ElevatorPose *elevator = pose();
    elevator->setScene(atB3 ? (open ? ElevatorScene_OpenB3 : ElevatorScene_ClosedB3)
                            : (open ? ElevatorScene_OpenB1 : ElevatorScene_ClosedB1));
    elevator->timer = 0.0f;
    stopSound(m_rideSound);
    m_lastState = elevator->state;
}

bool StageElevator::isStep(u_int step)
{
    return m_model != nullptr && (pose()->state & kElevatorStepMask) == step;
}

// FS2 0x00294b60

bool StageElevator::isOpen()
{
    return isStep(ElevatorStep_Open);
}

// FS2 0x00294b00

bool StageElevator::isOpening()
{
    return isStep(ElevatorStep_Opening);
}

// FS2 0x00294b30

bool StageElevator::isClosed()
{
    return isStep(ElevatorStep_Closed);
}

// FS2 0x00294ad0

bool StageElevator::isClosing()
{
    return isStep(ElevatorStep_Closing);
}

// FS2 0x00294b90

bool StageElevator::isOpenAtB3()
{
    return m_model != nullptr && (pose()->state & kElevatorAtB1) == 0 && isOpen();
}

// FS2 0x00294be0

bool StageElevator::isOpenAtB1()
{
    return m_model != nullptr && (pose()->state & kElevatorAtB1) != 0 && isOpen();
}

// FS2 0x00294c30

bool StageElevator::isAtB3()
{
    if (m_model == nullptr)
        return false;
    const u_int state = pose()->state;
    return (state & kElevatorAtB1) == 0 && (state & kElevatorStepMask) != ElevatorStep_RideToB3;
}

// FS2 0x00294c70

bool StageElevator::isAtB1()
{
    if (m_model == nullptr)
        return false;
    const u_int state = pose()->state;
    return (state & kElevatorAtB1) != 0 && (state & kElevatorStepMask) != ElevatorStep_RideToB1;
}

// The meshes join the world before the base's motions, then ride on their
// bones; the motions go in the pose by ElevatorMotion, and it starts shut at
// B3. The placement always moves from the node: arg is not read.
// FS2 0x00295010
// callcheck: collisionFindRecord, operator_new, StageCollision, getWorldMatrix, operator_new, CollisionBody, add, setMotionList, find, setInverseTree, getWorldMatrix, findMotionFile, setMotionFile, setScene, stopSound (those two inline: placeAt)

void StageElevator::setMotionList(void *list, int, int arg2)
{
    for (u_int i = 0; i < m_collisionCount; i++) {
        CollisionRecord *record = collisionFindRecord(m_collision, kMeshNames[i]);
        if (record == nullptr)
            continue;
        StageCollision *mesh = new StageCollision(record, ((CollisionFile *)m_collision)->surfaceNames,
                                                  kMeshCellSize);
        m_collisions[i] = mesh;
        mesh->m_matrix = getWorldMatrix();
        mesh->m_inverse = m_inverse;
        CollisionBody *body = new CollisionBody(this, 0, kCollisionMesh, 0);
        gStageScene->records->add(body);
        body->shape = CollisionShape_Mesh;
        body->mesh = mesh;
        body->settled = 0;
    }

    StageMapObject::setMotionList(list, 1, arg2);

    for (u_int i = 0; i < m_collisionCount; i++) {
        if (m_collisions[i] == nullptr)
            continue;
        StageNodeBase *bone = m_model->boneMap.find(kMeshBones[i])->value;
        bone->setInverseTree(true);
        m_collisions[i]->m_matrix = bone->getWorldMatrix();
        m_collisions[i]->m_inverse = bone->m_inverse;
    }

    if (m_model != nullptr) {
        ElevatorPose *elevator = pose();
        for (u_int i = 0; i < kElevatorMotions; i++)
            elevator->setMotionFile(m_model->findMotionFile(0, kMotionNames[i]), i);
        placeAt(true, false);
    }
}

// FS2 0x002952b0
// callcheck: stopSound, removeNamed, ~StageMapObject, operator_delete

StageElevator::~StageElevator()
{
    stopSound(m_rideSound);
    if (m_rider != nullptr)
        gStageScene->rumble->removeNamed(kRumbleRide);
}

// The places stay unworked until first asked.
// FS2 0x00295360
// callcheck: StageMapObject, setDirtyTree

StageElevator::StageElevator(void *collision) : StageMapObject(kElevatorMeshes), m_collision(collision)
{
    setDirtyTree(true);
    for (int i = 0; i < kElevatorLandings; i++) {
        m_callPlace[i][3] = kNotWorkedOut;
        m_callFacing[i][3] = kNotWorkedOut;
        m_switchPlace[i][3] = kNotWorkedOut;
        m_switchDirection[i][3] = kNotWorkedOut;
        m_doorPlace[i][3] = kNotWorkedOut;
    }
}

// What stands in the header, and the adjustor thunks that end the file (the
// host's compiler makes its own):
//   FS2 0x002939b0  StageElevator::getKind (stageelevator.h)
//   FS2 0x002939c0  StageElevator::sideHook1 (stageelevator.h)
//   FS2 0x002939d0  StageElevator::sideHook0 (stageelevator.h)
//   FS2 0x00295420  StageElevator::sideHook3, -0x18
//   FS2 0x00295430  StageElevator::sideHook1, -0x18
//   FS2 0x00295440  StageElevator::sideHook0, -0x18
//   FS2 0x00295450  StageElevator::~StageElevator, -0x40
//   FS2 0x00295460  StageElevator::collisionHook0, -0x200
