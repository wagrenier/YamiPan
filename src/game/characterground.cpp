/* ==========================================================================
 *  game/characterground.cpp
 *
 *  The character's ground, 0x1eff00..0x1f4bb0 (CharacterGround, vtable
 *  0x315ee0). What it does in a frame, in the ROM's order:
 *  - movement hook: StageCharacter::moveStep ends in beginFrame;
 *  - SceneRecordBuckets::collide: syncBody puts every body at its bones,
 *    collisionHook0 / onCollide answer what touches, collisionHook3 of the
 *    wall body sweeps the way walked and looks for the floor;
 *  - terminate hook: endFrame.
 *  The bone tables are in game/groundbones.inc.
 * ======================================================================== */
#include "game/characterground.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/playermotion.h"
#include "game/scriptrunner.h"
#include "game/stagecollision.h"
#include "game/stageitem.h"
#include "game/stagenode.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
#include "game/vu0math.h"

// layout: host -- a bone a body hangs from. EE 0x20: +0 name, +4 second bone, +8 radius, +0x10
// an offset, zero in every row of the ROM (left out; the ROM still turns it).
struct GroundBone
{
    const char *name;           // FS2 +0x0
    const char *second;         // FS2 +0x4: blows only: the bone the arm or leg starts from
    float       radius;         // FS2 +0x8
};
// layout: host -- a row of a table with its count (the ROM keeps the counts apart).
struct GroundBoneTable
{
    const GroundBone *rows;
    int               count;
};

#include "game/groundbones.inc"

namespace {

// What rays and spheres of the ground stop at.
constexpr u_int kFaceFloor     = 0x42400c;   // what a foot stands on
constexpr u_int kFaceFloorSkip = 0x40000;
constexpr u_int kFaceWall      = 0xe8021f3;  // what a wall sphere stops at
constexpr u_int kFaceWallGhost = 0x2e8021f3; // kinds 5, 0xc, 0xd: with bit 0x20000000
constexpr u_int kFaceWallSkip  = 0x2040000;
constexpr u_int kFaceSight     = 0xe002003;  // what a blow or the side rays stop at
constexpr u_int kFaceSightSkip = 0x40000;
constexpr u_int kFaceMeleeSkip = 0x8401f0;
constexpr u_int kFaceCarInside = 0xe42600f;  // a car's own mesh, seen from inside
constexpr u_int kFaceCarEdge   = 0x40;       // a face of a car one may step off

constexpr float kWallEye       = 50.0f;      // the sweep runs this far above the feet
constexpr float kSweepMargin   = 10.0f;
constexpr float kFloorProbeUp  = 100.0f;
constexpr float kFloorStick    = 50.0f;      // how far a landed character's feet find the floor
constexpr float kFloorReach    = 10.0f;      // the same in the air
constexpr float kMaxWallPush   = 12.0f;      // a frame's
constexpr float kMaxPushedBack = 12.0f;
constexpr float kCarProbe      = 400.0f;
constexpr float kCarThrowSpeed = 150.0f;

constexpr float kFacePi        = 3.1415927f;
constexpr float kBumpAngle     = 2.0943952f; // a blow counts on a wall it faces, within 60 degrees

// The motion ids of being down: getLifeState's 4; the label is unread.
constexpr int kMotionDownA = 0xb3;
constexpr int kMotionDownB = 0xb4;
constexpr int kMotionThrown = 0xc2;
constexpr int kItemThrownKnife = 5;          // the two ItemKinds a blow reaches backwards with (a guess)
constexpr int kItemThrownBlade = 6;
constexpr int kDoorModeA = 1;                // what m_doorMode says for atDoor; unread
constexpr int kDoorModeB = 3;

constexpr float kFrameSeconds = 0.033333335f;

constexpr int kHandClassDriving = 2;         // StageCharacter::getHandClass

bool isDownMotion(int id)
{
    return id == kMotionDownA || id == kMotionDownB;
}

bool isDown(const StageObject *character)
{
    return isDownMotion(character->m_motion->motionId);
}

// The walking, running, turning motions: a character in one yields to
// another (meetCharacter).
bool isTurnMotion(int id)
{
    switch (id) {
    case 0xb: case 0xc: case 0xd: case 0xe: case 0xf:
    case 0x55: case 0x56: case 0x57: case 0x58:
    case 0x70: case 0x71: case 0x7f: case 0x80: case 0x8e: case 0x8f:
        return true;
    default:
        return false;
    }
}

bool isYieldingMotion(int id)
{
    return isWalkMotion(id) || isRunMotion(id) || isTurnMotion(id)
        || id == 0x2d || id == 0x24 || id == 0x26 || id == 0x25;
}

// The motions a car can throw a character out of.
bool isThrowableMotion(int id)
{
    switch (id) {
    case 0xf8: case 0xf7: case 0xf6: case 0xf5: case 0xf4:
    case 2: case 1: case 0:
    case 0x15: case 0x14:
    case 0x1f: case 0x1e: case 0x1d: case 0x2b:
        return true;
    default:
        return false;
    }
}

// The climbing motions; the labels are unread.
bool isClimbMotion(int id)
{
    return id == 0x41 || id == 0x35 || id == 0x34 || id == 0x33 || id == 0x42 || id == 0x95
        || id == 0x94;
}

// The named bone; the ROM takes it with no test.
// PC: a model with no such bone falls back on the character, whose matrix is
// always good: the EE would read address 0 and carry on.
StageNodeBase *groundBone(StageObject *owner, const char *name)
{
    BoneMap::Node *bone = owner->m_model->boneMap.find(name);
    return bone != nullptr ? bone->value : owner;
}

void addTo(sceVu0FVECTOR out, const sceVu0FVECTOR step)
{
    for (int i = 0; i < 4; i++)
        out[i] += step[i];
}

void copyTo(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    memcpy(out, in, sizeof(sceVu0FVECTOR));
}

void subtract(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int i = 0; i < 4; i++)
        out[i] = a[i] - b[i];
}

float length3(const sceVu0FVECTOR v)
{
    return sqrtf(v[2] * v[2] + v[0] * v[0] + v[1] * v[1]);
}

// A sphere's place at a bone: the row's offset, zero in the ROM, is left out.
void bonePosition(sceVu0FVECTOR out, StageNodeBase *bone)
{
    copyTo(out, bone->getWorldMatrix()[3]);
}

// The view target as the object it is. PC: it is not a StageObject on the
// host yet (StageViewTarget), and nothing sets it.
StageCar *viewCar()
{
    return reinterpret_cast<StageCar *>(gStageScene->viewTarget);
}

// FS2 0x001f22a0: qsort's order, the shorter vectors first.
int compareLength(const void *a, const void *b)
{
    const float *one = static_cast<const float *>(a);
    const float *two = static_cast<const float *>(b);
    const float first = one[2] * one[2] + one[0] * one[0] + one[1] * one[1];
    const float second = two[2] * two[2] + two[0] * two[0] + two[1] * two[1];
    if (first < second)
        return -1;
    return second < first ? 1 : 0;
}

} // namespace

// ──────────────────────────────────────────────────────────────────────
// Built

// The bone table's row, from the sheet's kind; the flags say which bodies.
// FS2 0x001f3d20; the bodies by family. The bit doors are a list only when
// they let the character through.
// callcheck: FUN_001f4c20, FUN_001f4bb0, FUN_001f4bb0, FUN_001f4bb0, FUN_001f4bb0, FUN_001f4bb0, getStageSelect, getStageSelect, setContact, operator_new[], strcmp, operator_new, CollisionBody, add, operator_new, CollisionBody, add, find, alloc, FUN_001f4f20, find, FUN_001f4f20, operator_new, CollisionBody, add, find, FUN_001f4f20, BoundingSphere, FUN_001f4d30, FUN_001f4f20, operator_new, CollisionBody, add, operator_new, CollisionBody, add, find, FUN_001f4f20, BoundingSphere, FUN_001f4d30, motionBoneMatrix, operator_new, CollisionBody, find, FUN_001f4f20, BoundingSphere, FUN_001f4d30, FUN_001f4d30, motionBoneMatrix, add, getStageSelect (StageList's members stand for the instances; the sheet is stageObjRecord)

CharacterGround::CharacterGround(StageObject *ownerObject)
{
    owner = ownerObject;
    groundCheck = 1;
    sweepsWalls = 1;
    nearTarget = 0;

    const int kind = stageObjRecord(owner->m_itemDef)->kind;
    u_char floor = 1, push = 1, wall = 1, doors = 1, hits = 1;
    switch (kind) {
    case 0: case 1: case 2: case 8: case 9: case 0xa: case 0xe:
        if (owner->m_bodySize == 3)
            family = 2;
        else if (owner->m_bodySize == 2)
            family = 1;
        else if (owner->m_bodySize == 1 || owner->m_bodySize == 0)
            family = 0;
        if (kind == 0xe)
            push = wall = doors = hits = 0;
        break;
    case 3:
        family = 6;
        push = 0;
        doors = 0;
        break;
    case 4:
        family = 0;
        break;
    case 5: case 0xc:
        family = 3;
        break;
    case 6:
        family = 4;
        break;
    case 7:
        family = 7;
        break;
    case 0xb:
        family = 8;
        floor = 0;
        wall = 0;
        doors = 0;
        break;
    case 0xd:
        family = 9;
        break;
    case 0xf:
        family = 5;
        break;
    case 0x10:
        floor = push = wall = doors = hits = 0;
        break;
    }
    hasFloor = floor;
    hasPush = push;
    hasWall = wall;
    doorsBlock = doors;
    hasHits = hits;

    if (hasHits) {
        volume = new CollisionBody(this, 2, kGroundHitBody, 0);
        gStageScene->records->add(volume);
        volume->shape = CollisionShape_Spheres;
        const GroundBoneTable &bodyTable = kBodySpheres[family];
        for (int i = 0; i < bodyTable.count; i++) {
            bodyBones.pushBack(groundBone(owner, bodyTable.rows[i].name));
            BoundingSphere sphere;
            sphere.radius = bodyTable.rows[i].radius;
            sphere.radiusSq = sphere.radius * sphere.radius;
            volume->spheres.pushBack(sphere);
        }
        const GroundBoneTable &blowTable = kHitBones[family];
        for (int i = 0; i < blowTable.count; i++) {
            blowBodies[i] = new CollisionBody(this, 2, kGroundBlowBody, kGroundHitBody);
            gStageScene->records->add(blowBodies[i]);
            blowBodies[i]->shape = CollisionShape_Capsule;
            blowBodies[i]->capsuleRadius = blowTable.rows[i].radius;
            StageNodeBase *bone = groundBone(owner, blowTable.rows[i].name);
            // PC: the ROM allocates whatever the node holds; the fallback
            // of groundBone already has one.
            if (bone->m_lastPosition == nullptr)
                bone->m_lastPosition = static_cast<float *>(gGlobalHeap.alloc(0x10, 0x10));
            blowBones.pushBack(bone);
            blowRoots.pushBack(blowTable.rows[i].second == nullptr
                                   ? nullptr : groundBone(owner, blowTable.rows[i].second));
        }
        meleeBody = new CollisionBody(this, 1, kCollisionMelee, kCollisionMesh);
        gStageScene->records->add(meleeBody);
        meleeBody->shape = CollisionShape_Segment;
        meleeBody->rayMask = kFaceSight;
        meleeBody->rayFlags = kFaceMeleeSkip;
    }

    if (hasPush == 0) {
        nearRadius = 0.0f;
    } else {
        pushBody = new CollisionBody(this, 0, kGroundPushBody, kGroundPushBody);
        gStageScene->records->add(pushBody);
        pushBody->shape = CollisionShape_Spheres;
        nearRadius = 30.0f;
        const GroundBoneTable &table = kPushSpheres[family];
        for (int i = 0; i < table.count; i++) {
            pushBones.pushBack(groundBone(owner, table.rows[i].name));
            BoundingSphere sphere;
            sphere.radius = table.rows[i].radius;
            sphere.radiusSq = sphere.radius * sphere.radius;
            pushBody->spheres.pushBack(sphere);
            float height = 0.0f;
            sceVu0FMATRIX bind;
            if (motionBoneMatrix(bind, owner->m_model->m_motionFile, table.rows[i].name, 0) > 0)
                height = fabsf(bind[3][2]);
            if (nearRadius < height + table.rows[i].radius)
                nearRadius = height + table.rows[i].radius;
        }
        if (owner->m_abilities.flag3ba != 0) {
            nearBody = new CollisionBody(this, 0, kGroundNearBody, 0);
            gStageScene->records->add(nearBody);
            nearBody->shape = CollisionShape_Sphere;
            nearBody->sphere.radius = nearRadius;
            nearBody->sphere.radiusSq = nearRadius * nearRadius;
        }
    }

    if (hasWall == 0) {
        radius = 0.0f;
        radius2 = 0.0f;
    } else {
        wallBody = new CollisionBody(this, 1, kGroundWallBody, kCollisionMesh);
        gStageScene->records->add(wallBody);
        wallBody->shape = CollisionShape_Spheres;
        wallBody->rayMask = kind == 0xd || kind == 0xc || kind == 5 ? kFaceWallGhost : kFaceWall;
        wallBody->rayFlags = kFaceWallSkip;
        radius = 0.0f;
        radius2 = 0.0f;
        const GroundBoneTable &table = kWallSpheres[family];
        // PC: 0 before the first bone found; the ROM's stack is uninitialised.
        float reachX = 0.0f, reachZ = 0.0f;
        for (int i = 0; i < table.count; i++) {
            wallBones.pushBack(groundBone(owner, table.rows[i].name));
            BoundingSphere sphere;
            sphere.radius = table.rows[i].radius;
            sphere.radiusSq = sphere.radius * sphere.radius;
            wallBody->spheres.pushBack(sphere);
            lastSpheres.pushBack(sphere);
            sceVu0FMATRIX bind;
            if (motionBoneMatrix(bind, owner->m_model->m_motionFile, table.rows[i].name, 0) > 0) {
                reachZ = -bind[3][2];
                reachX = bind[3][0];
            }
            const float reach = table.rows[i].radius + sqrtf(reachX * reachX + reachZ * reachZ);
            if (radius < reach)
                radius = reach;
            if (radius2 < table.rows[i].radius)
                radius2 = table.rows[i].radius;
        }
    }

    setContact(0);

    // The doors go on the list only where they do not block.
    StageObjArray &roots = gStageScene->objects->m_lists[StageObjGroup_1];
    auto nextActive = [](StageObject **slot) {
        while (slot != nullptr && !(*slot)->m_active)
            slot = StageObjArray::next(slot);
        return slot;
    };
    exceptCount = 0;
    if (doorsBlock == 0) {
        for (StageObject **slot = nextActive(roots.begin()); slot != nullptr;
             slot = nextActive(StageObjArray::next(slot))) {
            if ((*slot)->getKind() == StageObject_Door)
                exceptCount++;
        }
    }
    exceptCount++;
    exceptList = new StageObject *[exceptCount]();
    if (doorsBlock == 0) {
        int index = 0;
        for (StageObject **slot = nextActive(roots.begin()); slot != nullptr;
             slot = nextActive(StageObjArray::next(slot))) {
            if ((*slot)->getKind() == StageObject_Door)
                exceptList[index++] = *slot;
        }
    }

    fixPositionName = strcmp(owner->m_name, "F_E10_SB1") == 0 ? 1 : 0; /* data 0x305a98 */
}

// FS2 0x001f3c70
// callcheck: operator_delete[], FUN_001f4bc0, FUN_001f4bc0, FUN_001f4bc0, FUN_001f4bc0, FUN_001f4bc0, FUN_00184260, operator_delete (the lists' dtors run by themselves)

CharacterGround::~CharacterGround()
{
    delete[] exceptList;
}

// FS2 0x001efef0

StageObject *CharacterGround::getCollisionObject()
{
    return owner;
}

// FS2 0x001f3b30
// callcheck: FUN_001f35b0, getWorldMatrix, applyMatrix, getWorldMatrix, FUN_001f3930

void CharacterGround::reset()
{
    checkFloor();
    copyTo(lastPosition, owner->m_xform.position);
    if (wallBody != nullptr) {
        StageList<StageNodeBase *>::Node *bone = wallBones.head;
        for (StageList<BoundingSphere>::Node *node = wallBody->spheres.head;
             bone != nullptr && node != nullptr; bone = bone->next, node = node->next) {
            bonePosition(node->value.center, bone->value);
            node->value.center[2] = owner->m_xform.position[2] + node->value.radius + 25.0f;
        }
    }
    slideTime = 0.0f;
    slideSpeed = 0.0f;
    memset(slideDirection, 0, sizeof(slideDirection));
    slideLevel = GroundSlide_None;
    pushedBack = 0.0f;
    beginFrame();
}

// ──────────────────────────────────────────────────────────────────────
// Asked from outside

// FS2 0x001eff00
// callcheck: getWorldMatrix, applyMatrix, findFloorFaceExcept

StageCollisionFace *CharacterGround::faceNear(const float *localPoint)
{
    sceVu0FVECTOR world;
    applyMatrix(world, owner->getWorldMatrix(), localPoint);
    float height = world[2] + 70.0f;
    return gStageScene->findFloorFaceExcept(world[0], world[1], 170.0f, &height, kFaceFloor,
                                            kFaceFloorSkip, nullptr);
}

// FS2 0x001eff80
// callcheck: isPlayer, FUN_001f4e20 (StageList::at)

void CharacterGround::setContact(int value)
{
    if (hasHits == 0)
        return;
    if (value == 0) {
        for (int i = 0; i < kHitBones[family].count; i++)
            blowBodies[i]->on = 0;
        hitterCount = 0;
    } else if (hitBone >= 0) {
        blowBodies[hitBone]->on = static_cast<u_char>(value);
    }
    if (!owner->isPlayer()) {
        meleeBody->on = 0;
        return;
    }
    StageList<StageNodeBase *>::Node *root = blowRoots.at(hitBone);
    meleeBody->on = root == nullptr || root->value == nullptr ? 0 : static_cast<u_char>(value);
}

// FS2 0x001f00a0
// callcheck: strcmp

void CharacterGround::setHitBone(const char *name)
{
    hitBone = -1;
    const GroundBoneTable &table = kHitBones[family];
    for (int i = 0; i < table.count; i++) {
        if (strcmp(table.rows[i].name, name) == 0) {
            hitBone = i;
            return;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The frame

void CharacterGround::moveBy(const sceVu0FVECTOR step)
{
    addTo(owner->m_xform.position, step);
    owner->m_model->pose->translate(step);
    if (wallBody == nullptr)            // PC: the ROM's callers all have one
        return;
    for (StageList<BoundingSphere>::Node *node = wallBody->spheres.head; node != nullptr;
         node = node->next)
        addTo(node->value.center, step);
}

// FS2 0x001f3930
// callcheck: getDeltaSeconds, powf

void CharacterGround::beginFrame()
{
    wasLanded = landed;
    wallCount = 0;
    wasHit = 0;
    blowLanded = 0;
    nearSomeone = 0;
    narrow = 0;
    wonStruggle = 0;
    climbedOver = 0;
    pushedThisFrame = 0;
    atDoor = 0;
    bumped = 0;
    carHit = 0;
    if (slideTime <= 0.0f)
        return;
    if (isDown(owner)) {
        slideTime = 0.0f;
    } else {
        const float seconds = owner->m_clock->getDeltaSeconds();
        slideTime -= seconds;
        if (slideTime < 0.0f)
            slideTime = 0.0f;
        const float frames = seconds / kFrameSeconds;
        if (fabsf(frames - 1.0f) <= 0.0001f)
            slideSpeed *= 0.8f;
        else if (fabsf(frames - 1.2f) <= 0.0001f)
            slideSpeed *= 0.765082f;    // 0.8 ^ 1.2
        else
            slideSpeed *= powf(0.8f, frames);
    }
    if (slideTime == 0.0f) {
        slideLevel = GroundSlide_None;
        slideSpeed = 0.0f;
    } else if (slideSpeed < 10.0f) {
        slideSpeed = 0.0f;
    }
}

// FS2 0x001f38d0
// callcheck: FUN_001f35b0

void CharacterGround::endFrame()
{
    if (wallBody == nullptr || wallBody->on == 0)
        checkFloor();
    copyTo(lastPosition, owner->m_xform.position);
}

// FS2 0x001f35b0
// callcheck: getKind, getFloorAttr, raycastObject, vecDistance, vecDirection, scaleVec4, translate, raycastObject, vecDistance, vecDirection, scaleVec4, translate

void CharacterGround::checkFloor()
{
    // A character down on a car's floor stands on it; otherwise the view
    // target is not a floor.
    StageObject *skip = nullptr;
    if (isDown(owner) && (floorObject == nullptr || floorObject->getKind() != StageObject_Kind3))
        skip = reinterpret_cast<StageObject *>(gStageScene->viewTarget);
    landed = 0;
    floorFace = nullptr;
    floorObject = nullptr;
    floorAttr = gStageScene->getFloorAttr(owner->m_xform.position);
    if (hasFloor == 0 || owner->m_gear != nullptr)
        return;

    float *position = owner->m_xform.position;
    copyTo(rayFrom, position);
    copyTo(rayTo, position);
    rayFrom[2] += kFloorProbeUp;
    StageObject *object = nullptr;
    sceVu0FVECTOR hit, way, step;
    if (groundCheck == 1 && wasLanded != 0) {
        rayTo[2] -= kFloorStick;
        StageCollisionFace *face = gStageScene->raycastObject(&object, hit, rayFrom, rayTo, 0,
                                                              kFaceFloor, kFaceFloorSkip, skip);
        if (face != nullptr) {
            const float distance = vecDistance(hit, rayTo);
            vecDirection(way, rayFrom, rayTo);
            scaleVec4(distance - kFloorStick, step, way);
            addTo(position, step);
            owner->m_model->pose->translate(step);
            floorFace = face;
            floorObject = object;
            landed = 1;
            floorHeight = position[2];
        }
        return;
    }
    rayTo[2] -= kFloorReach;
    if (groundCheck == 0)
        floorHeight = position[2];
    StageCollisionFace *face = gStageScene->raycastObject(&object, hit, rayFrom, rayTo, 0,
                                                          kFaceFloor, kFaceFloorSkip, skip);
    if (face == nullptr)
        return;
    floorFace = face;
    floorObject = object;
    landed = 1;
    if (groundCheck != 0 && wasLanded == 0) {
        const float distance = vecDistance(hit, rayTo);
        vecDirection(way, rayFrom, rayTo);
        scaleVec4(distance - kFloorReach, step, way);
        addTo(position, step);
        owner->m_model->pose->translate(step);
    }
}

// FS2 0x001f2b20
// callcheck: FUN_001f1c40, FUN_001f35b0

void CharacterGround::collisionHook3(CollisionBody *body)
{
    if (body->category == kGroundWallBody) {
        sweepWalls();
        checkFloor();
    } else if (body->category == kGroundPushBody && pushedThisFrame == 0) {
        pushedBack -= 0.25f;
        if (pushedBack < 0.0f)
            pushedBack = 0.0f;
    }
}

// FS2 0x001f1c40
// callcheck: FUN_00135fe0, raycastObjectExcept, getKind, isDoorFace, translate, getKind, raycast, FUN_00289dd0, findEdge, translate, FUN_001f2990, FUN_00289f70, FUN_00289f70, FUN_00126588, sphereHitsAny, FUN_00135fe0, segmentHitsAny, translate (0x135fe0 setVecLength; moveBy holds the translates)

void CharacterGround::sweepWalls()
{
    if (sweepsWalls == 0 && nearTarget == 0)
        return;
    StageCar *car = viewCar();
    exceptList[exceptCount - 1] = car;
    sceVu0FVECTOR from, to, sweep, hit, step;
    copyTo(from, lastPosition);
    from[2] += kWallEye;
    copyTo(to, owner->m_xform.position);
    to[2] += kWallEye;
    subtract(sweep, to, from);
    setVecLength(length3(sweep) + kSweepMargin, sweep, sweep);
    for (int i = 0; i < 4; i++)
        to[i] = sweep[i] + from[i];

    StageObject *object = nullptr;
    StageCollisionFace *face = gStageScene->raycastObjectExcept(&object, hit, from, to, 1,
                                                                wallBody->rayMask, kFaceWallSkip,
                                                                exceptList, exceptCount);
    if (face == nullptr) {
        // PC: the ROM reads the view target with no test.
        if (car != nullptr && floorObject != nullptr && floorObject->getKind() == StageObject_Kind3) {
            StageCollisionFace *wall = car->collisionAt(0)->raycast(hit, from, to, kFaceWall,
                                                                    kFaceWallSkip, FaceSide_Front);
            if (wall != nullptr) {
                bool blocked = true;
                float drop = 0.0f;
                sceVu0FVECTOR target, point, angles;
                if ((wall->flags & kFaceCarEdge) != 0 && car->getSpeed() > 0.0f
                    && owner->m_motion->probe->findEdge(wall, nullptr, target, &drop, nullptr, point,
                                                        angles, nullptr)
                    && drop < 0.0f)
                    blocked = false;
                if (blocked) {
                    subtract(step, hit, to);
                    moveBy(step);
                }
            }
        }
    } else if (doorsBlock != 0
               || (object->getKind() != StageObject_Door && !gStageScene->isDoorFace(face))) {
        subtract(step, hit, to);
        moveBy(step);
    }

    if (nearTarget == 0 || isDown(owner) || !isInsideCar())
        return;
    // Inside the car's mesh: the nearest way out that has room.
    float *position = owner->m_xform.position;
    sceVu0FVECTOR center;
    copyTo(center, position);
    center[2] += kWallEye;
    constexpr int kWays = 6;
    sceVu0FVECTOR ways[kWays];
    for (int i = 0; i < kWays; i++) {
        if (i < kWays - 1)
            car->getDoorPoint(ways[i], 1, i);
        else
            car->getDoorPoint(ways[i], 0, 0);
        ways[i][2] = position[2] + kWallEye;
        subtract(ways[i], ways[i], center);
    }
    qsort(ways, kWays, sizeof(sceVu0FVECTOR), compareLength);
    copyTo(from, center);
    for (int i = 0; i < kWays; i++) {
        sceVu0FVECTOR there;
        for (int lane = 0; lane < 4; lane++)
            there[lane] = ways[i][lane] + center[lane];
        if (gStageScene->sphereHitsAny(nearRadius * 1.1f, there, kFaceWall, kFaceWallSkip, car))
            continue;
        subtract(sweep, there, from);
        setVecLength(kSweepMargin + length3(sweep), sweep, sweep);
        for (int lane = 0; lane < 4; lane++)
            to[lane] = sweep[lane] + from[lane];
        if (!gStageScene->segmentHitsAny(from, to, 1, kFaceWall, kFaceWallSkip, car)) {
            moveBy(ways[i]);
            return;
        }
    }
}

// FS2 0x001f2990
// callcheck: segmentHits

bool CharacterGround::isInsideCar()
{
    if (nearTarget == 0)
        return false;
    StageCar *car = viewCar();
    StageCollision *mesh = car != nullptr ? car->collisionAt(0) : nullptr; // PC: the test of car
    if (mesh == nullptr)
        return false;
    static const float kWays[4][2] = { /* built on the stack */
        { 0.0f, kCarProbe }, { 0.0f, -kCarProbe }, { kCarProbe, 0.0f }, { -kCarProbe, 0.0f },
    };
    sceVu0FVECTOR center;
    copyTo(center, owner->m_xform.position);
    center[2] += kWallEye;
    for (int i = 0; i < 4; i++) {
        sceVu0FVECTOR end;
        copyTo(end, center);
        end[0] += kWays[i][0];
        end[1] += kWays[i][1];
        if (!mesh->segmentHits(end, center, kFaceCarInside, 0, FaceSide_Any))
            return false;
    }
    return true;
}

// FS2 0x001f2670
// callcheck: FUN_001f2990, FUN_00289dd0, randNext, randNext, randNext, FUN_00289e10, setVecLength, raycast, vecDirection, vecDistance, scaleVec4, translate

StageCar *CharacterGround::leaveCar()
{
    StageCar *car = viewCar();
    if (isDown(owner))
        return car;
    if (!isInsideCar())
        return nullptr;
    sceVu0FVECTOR start, way;
    copyTo(start, owner->m_xform.position);
    start[2] += kWallEye;
    if (car->getSpeed() == 0.0f) {
        subtract(way, lastPosition, owner->m_xform.position);
        way[2] = 0.0f;
        if (way[0] == 0.0f && way[1] == 0.0f) {
            // It stood still in a car that stands: any way, from the car's middle.
            constexpr int kRandHalf = 0x3fffffff;
            way[0] = static_cast<float>(static_cast<int>(randNext()) - kRandHalf);
            way[1] = static_cast<float>(static_cast<int>(randNext()) - kRandHalf);
            way[2] = 0.0f;
            way[3] = 0.0f;
            if (way[0] == 0.0f) {
                while (way[1] == 0.0f)
                    way[1] = static_cast<float>(static_cast<int>(randNext()) - kRandHalf);
            }
            copyTo(start, car->m_xform.position);
            start[2] += kWallEye;
        }
    } else {
        copyTo(way, car->getVelocity());
    }
    setVecLength(kCarProbe, way, way);
    sceVu0FVECTOR end, hit, step;
    for (int i = 0; i < 4; i++)
        end[i] = way[i] + start[i];
    StageCollisionFace *face = car->collisionAt(0)->raycast(hit, end, start, kFaceCarInside,
                                                            kFaceWallSkip, FaceSide_Any);
    if (face == nullptr) {
        copyTo(step, way);
    } else {
        sceVu0FVECTOR direction;
        vecDirection(direction, end, start);
        scaleVec4(nearRadius + vecDistance(hit, start), step, direction);
    }
    moveBy(step);
    return car;
}

// FS2 0x001f2300
// callcheck: raycastObjectExcept, vecCross, setVecLength, raycastObjectExcept, raycastObjectExcept, getKind, isDoorFace, vecDistance, vecDirection, scaleVec4, translate

void CharacterGround::keepInside(int level, StageCar *car)
{
    if (level == 0)
        return;
    exceptList[exceptCount - 1] = car;
    const u_int mask = wallBody->rayMask;
    StageList<BoundingSphere>::Node *last = lastSpheres.head;
    for (StageList<BoundingSphere>::Node *node = wallBody->spheres.head;
         node != nullptr && last != nullptr; node = node->next, last = last->next) {
        sceVu0FVECTOR from, to, hit;
        copyTo(from, last->value.center);
        copyTo(to, node->value.center);
        StageObject *object = nullptr;
        float extra = 0.0f;
        StageCollisionFace *face = gStageScene->raycastObjectExcept(&object, hit, from, to, 1, mask,
                                                                    kFaceWallSkip, exceptList,
                                                                    exceptCount);
        if (face != nullptr) {
            extra = node->value.radius / 2.0f;
        } else if (level > 1) {
            static const sceVu0FVECTOR kUp = { 0.0f, 0.0f, 50.0f, 0.0f };
            sceVu0FVECTOR way, side;
            subtract(way, to, from);
            vecCross(side, kUp, way);
            setVecLength(0.9f * node->value.radius, side, side);
            for (int i = 0; i < 4; i++) {
                from[i] = last->value.center[i] + side[i];
                to[i] = node->value.center[i] + side[i];
            }
            face = gStageScene->raycastObjectExcept(&object, hit, from, to, 1, kFaceSight,
                                                    kFaceWallSkip, exceptList, exceptCount);
            if (face == nullptr) {
                for (int i = 0; i < 4; i++) {
                    from[i] = last->value.center[i] - side[i];
                    to[i] = node->value.center[i] - side[i];
                }
                face = gStageScene->raycastObjectExcept(&object, hit, from, to, 1, kFaceSight,
                                                        kFaceWallSkip, exceptList, exceptCount);
            }
        }
        if (face != nullptr
            && (doorsBlock != 0
                || (object->getKind() != StageObject_Door && !gStageScene->isDoorFace(face)))) {
            extra += vecDistance(hit, to);
            sceVu0FVECTOR direction, step;
            vecDirection(direction, from, to);
            scaleVec4(extra, step, direction);
            moveBy(step);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The bodies follow the bones

void CharacterGround::syncWall(CollisionBody *body)
{
    const int kind = stageObjRecord(owner->m_itemDef)->kind;
    int level = kind == 0xc || kind == 5 || nearTarget != 0 ? 2 : 1;
    if (owner->m_motion->m_wallCheckNow == 0)
        level = 0;
    if (level != 0) {
        StageList<BoundingSphere>::Node *last = lastSpheres.head;
        for (StageList<BoundingSphere>::Node *node = body->spheres.head;
             node != nullptr && last != nullptr; node = node->next, last = last->next)
            copyTo(last->value.center, node->value.center);
    }
    StageList<StageNodeBase *>::Node *bone = wallBones.head;
    for (StageList<BoundingSphere>::Node *node = wallBody->spheres.head;
         node != nullptr && bone != nullptr; node = node->next, bone = bone->next) {
        bonePosition(node->value.center, bone->value);
        node->value.center[2] = owner->m_xform.position[2] + node->value.radius + 25.0f;
    }
    if (kind == 6) {
        StageList<BoundingSphere>::Node *first = wallBody->spheres.at(0);
        StageList<BoundingSphere>::Node *second = wallBody->spheres.at(1);
        doorsBlock = !gStageScene->segmentHitsAny(first->value.center, second->value.center, 0,
                                                  StageCollision_Door, 0, nullptr);
    }
    if (floorObject != nullptr && floorObject->getKind() == StageObject_Kind3 && owner->m_gear == nullptr) {
        StageCar *car = static_cast<StageCar *>(floorObject);
        StageCharacter *character = static_cast<StageCharacter *>(owner);
        if (car->getSpeed() > 0.0f && character->m_endurance > 0) {
            if (kCarThrowSpeed <= car->getAcceleration()) {
                if (isThrowableMotion(owner->m_motion->motionId) && kind != 3)
                    owner->m_motion->request(kMotionThrown);
            } else {
                moveBy(car->getFrameStep());
            }
        }
    }
    StageCar *car = leaveCar();
    keepInside(level, car);
}

// FS2 0x001f2be0
// callcheck: FUN_001f4e20, getWorldMatrix, FUN_001f4e20, getWorldMatrix, getWorldMatrix, applyMatrix, getWorldMatrix, applyMatrix, getStageSelect, getStageSelect, getWorldMatrix, applyMatrix, FUN_001f4c30, FUN_001f4c30, segmentHitsAny, FUN_00289dd0, FUN_00289e20, FUN_00289ea0, translate, getStageSelect, request, FUN_001f2670, FUN_001f2300, FUN_001f4e20, sinCos, sinCos, applyMatrix (the sheet is stageObjRecord, the rows' zero offsets are left out)

void CharacterGround::syncBody(CollisionBody *body)
{
    switch (body->category) {
    case kCollisionMelee: {
        // PC: the ROM has no test: this body is off without a second bone,
        // and a body that is off is still synced here.
        StageList<StageNodeBase *>::Node *root = blowRoots.at(hitBone);
        StageList<StageNodeBase *>::Node *bone = blowBones.at(hitBone);
        if (root == nullptr || root->value == nullptr || bone == nullptr)
            return;
        bonePosition(body->segment[0], root->value);
        bonePosition(body->segment[1], bone->value);
        StageObject *weapon = owner->m_weapons[0];
        if (weapon != nullptr) {
            const int weaponIndex = weapon->m_itemDef;
            const int itemKind = weaponIndex < 0 || weaponIndex >= kItemDefCount
                                     ? gItemDefs[0].kind : gItemDefs[weaponIndex].kind;
            sceVu0FVECTOR tip = { itemKind == kItemThrownKnife || itemKind == kItemThrownBlade
                                      ? -60.0f : 60.0f, 0.0f, 0.0f, 0.0f };
            applyMatrix(tip, weapon->getWorldMatrix(), tip);
            addTo(body->segment[1], tip);
        }
        return;
    }
    case kGroundWallBody:
        syncWall(body);
        return;
    case kGroundPushBody: {
        StageList<BoundingSphere>::Node *sphere = pushBody->spheres.head;
        for (StageList<StageNodeBase *>::Node *bone = pushBones.head;
             bone != nullptr && sphere != nullptr; bone = bone->next, sphere = sphere->next)
            bonePosition(sphere->value.center, bone->value);
        return;
    }
    case kGroundNearBody:
        copyTo(body->sphere.center, owner->m_xform.position);
        return;
    case kGroundBlowBody: {
        if (hitBone < 0)
            return;
        StageList<StageNodeBase *>::Node *bone = blowBones.at(hitBone);
        copyTo(body->capsule[0], bone->value->m_lastPosition);
        bonePosition(body->capsule[1], bone->value);
        if (owner->m_weapons[0] != nullptr) {
            const float yaw = -owner->m_xform.rotation[2];
            sceVu0FVECTOR reach = { sinCos(yaw, 0) * -40.0f + 0.0f, sinCos(yaw, 1) * -40.0f + 0.0f,
                                    0.0f, 0.0f };
            addTo(body->capsule[0], reach);
            addTo(body->capsule[1], reach);
        }
        return;
    }
    case kGroundHitBody: {
        StageList<BoundingSphere>::Node *sphere = body->spheres.head;
        for (StageList<StageNodeBase *>::Node *bone = bodyBones.head;
             bone != nullptr && sphere != nullptr; bone = bone->next, sphere = sphere->next)
            bonePosition(sphere->value.center, bone->value);
        return;
    }
    default:
        return;
    }
}

// ──────────────────────────────────────────────────────────────────────
// What touches

void CharacterGround::noteHitBone(int index, StageObject *by)
{
    hitNode = bodyBones.at(index)->value;
    hitRow = &kBodySpheres[family].rows[index];
    hitBy = by;
}

// The way it faces against the way the hit came.
void CharacterGround::updateInBack()
{
    const float yaw = owner->m_xform.rotation[2];
    const sceVu0FVECTOR facing = { -sinCos(-yaw, 0), -sinCos(yaw, 1), 0.0f, 0.0f };
    hitInBack = vecDot(hitDirection, facing) > 0.0f ? 1 : 0;
}

// FS2 0x001f0170
// callcheck: getKind, getKind, getKind, FUN_00289dd0, findEdge, isPlayer, isPartner, FUN_0028abf0, getStageSelect, FUN_001f4e20, FUN_00289e10, vecCross, vecNormalize, vecDot, scaleVec4, vecNormalize, sinCos, sinCos, vecDot

bool CharacterGround::wallCounts(CollisionContact *contact, StageCollisionFace *face)
{
    StageObject *object = contact->body->owner->getCollisionObject();
    if (object->getKind() != StageObject_Kind3)
        return true;
    if (owner->m_gear != nullptr)
        return false;
    StageCar *car = static_cast<StageCar *>(object);
    StageCharacter *character = static_cast<StageCharacter *>(owner);

    if (floorObject != nullptr && floorObject->getKind() == StageObject_Kind3) {
        // On the car: only the edge one can step off does not count.
        sceVu0FVECTOR target, point, angles;
        float drop = 0.0f;
        return (face->flags & kFaceCarEdge) == 0 || car->getSpeed() <= 0.0f
            || !owner->m_motion->probe->findEdge(face, nullptr, target, &drop, nullptr, point,
                                                 angles, nullptr)
            || drop >= 0.0f;
    }
    if (isDown(owner) && !owner->isPlayer() && !owner->isPartner())
        return false;
    if (character->m_endurance == 0)
        return true;
    if (wasHit != 0 && hitBy == object)
        return true;

    int level = car->getHitLevel(owner->m_xform.position);
    const int kind = stageObjRecord(owner->m_itemDef)->kind;
    if (kind == 3) {
        if (level > 1)
            level = GroundSlide_Worst;
    } else if ((kind == 10 || kind == 0xc || kind == 6 || kind == 5) && level == GroundSlide_Worst) {
        level = GroundSlide_Heavy;
    }
    if (level < GroundSlide_Light || level <= slideLevel)
        return true;

    carHit = 1;
    wasHit = 1;
    noteHitBone(contact->selfIndex, object);
    sceVu0FVECTOR way;
    copyTo(way, car->getVelocity());
    static const sceVu0FVECTOR kUp = { 0.0f, 0.0f, 1.0f, 0.0f };
    sceVu0FVECTOR side, offset;
    vecCross(side, kUp, way);
    vecNormalize(side, side);
    subtract(offset, owner->m_xform.position, hitBy->m_xform.position);
    scaleVec4(vecDot(offset, side), side, side);
    addTo(way, side);
    vecNormalize(hitDirection, way);
    updateInBack();
    slideLevel = level;
    if (slideLevel == GroundSlide_Heavy) {
        slideTime = 3.0f;
        slideSpeed = 50.0f;
    } else if (slideLevel == GroundSlide_Light) {
        slideTime = 1.5f;
        slideSpeed = 25.0f;
    } else {
        slideTime = 5.0f;
        slideSpeed = 75.0f;
    }
    copyTo(slideDirection, hitDirection);
    return true;
}

// A blow's bone against the map.
int CharacterGround::bumpWall(StageCollisionFace *face)
{
    if (bumped != 0)
        return 0;
    sceVu0FVECTOR normal;
    face->getNormal(normal);
    const float angle = atan2Octant(-normal[0], normal[1]);
    const float back = wrapAngle(owner->m_xform.rotation[2] + kFacePi);
    if (fabsf(wrapAngle(angle - back)) > kBumpAngle) {
        StageCharacter *character = static_cast<StageCharacter *>(owner);
        character->playerEvent("COLLISIONHIT"); /* data 0x305a88 */
        character->playBumpSound(face->surface);
        bumpFace = face;
        bumped = 1;
    }
    return 0;
}

// A wall sphere against the map: out along the face, and along a corner
// when the spheres find a second face there.
int CharacterGround::meetWall(CollisionContact *contact, const float *normal,
                              StageCollisionFace *face, float depth)
{
    if (fixPositionName != 0 && owner->scriptRunner()->getNamed("fix_position") != 0) /* data 0x305a78 */
        return 0;
    if (!wallCounts(contact, face))
        return 0;
    StageObject *object = contact->body->owner->getCollisionObject();
    if (doorsBlock == 0 && object->getKind() == StageObject_Door)
        return 0;
    if (doorsBlock == 0 && gStageScene->isDoorFace(face))
        return 0;
    if (object->getKind() == StageObject_Door
        && (object->m_doorMode == kDoorModeB || object->m_doorMode == kDoorModeA))
        atDoor = 1;

    sceVu0FVECTOR push;
    scaleVec4(depth, push, normal);
    push[2] = 0.0f;

    if (sweepsWalls != 0 || nearTarget != 0) {
        StageObjArray &roots = gStageScene->objects->m_lists[StageObjGroup_1];
        for (StageList<BoundingSphere>::Node *node = contact->self->spheres.head; node != nullptr;
             node = node->next) {
            sceVu0FVECTOR there;
            for (int i = 0; i < 4; i++)
                there[i] = node->value.center[i] + push[i];
            for (StageObject **slot = roots.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
                StageObject *root = *slot;
                if (root->m_active == 0)
                    continue;
                if ((doorsBlock == 0 && root->getKind() == StageObject_Door) || root->m_collides == 0
                    || root->m_dead != 0)
                    continue;
                for (u_int i = 0; root->collisionAt(i) != nullptr; i++) {
                    StageCollisionFace *other = nullptr;
                    if (root->collisionAt(i)->sphereHits(node->value.radius, 1, &other,
                                                         wallBody->rayMask, kFaceWallSkip, there) == 0
                        || face->meshId == other->meshId)
                        continue;
                    sceVu0FVECTOR first, second;
                    face->getNormal(first);
                    other->getNormal(second);
                    const float dot = vecDot(first, second);
                    if (dot >= -0.1f)
                        continue;
                    // The two faces meet nearly head on: slide off the second.
                    static const sceVu0FVECTOR kUp = { 0.0f, 0.0f, 1.0f, 0.0f };
                    sceVu0FVECTOR side;
                    vecCross(side, kUp, first);
                    vecNormalize(side, side);
                    if (vecDot(side, second) < 0.0f) {
                        side[0] = -side[0];
                        side[1] = -side[1];
                        side[2] = -side[2];
                    }
                    const float sine = sqrtf((1.0f + 0.0f) - dot * dot);
                    const float limit = node->value.radius / 8.0f;
                    float amount;
                    if (sine < 0.2f) {
                        amount = limit * 0.1f;
                    } else {
                        sceVu0FVECTOR corner;
                        const sceVu0FVECTOR *matrix = other->mesh()->m_matrix;
                        if (matrix == nullptr)
                            copyTo(corner, other->vertex[0]);
                        else
                            applyMatrix(corner, matrix, other->vertex[0]);
                        const sceVu0FVECTOR offset = { there[0] - corner[0], there[1] - corner[1],
                                                       there[2] - corner[2], 0.0f };
                        const float inward = -vecDot(offset, second) + node->value.radius;
                        amount = inward / sine;
                        if (limit < amount)
                            amount = limit < inward ? limit : inward;
                        else if (amount < 0.0f)
                            amount = 0.0f;
                    }
                    scaleVec4(amount, side, side);
                    addTo(push, side);
                    break;
                }
            }
        }
    }

    const float length = length3(push);
    if (length > kMaxWallPush) {
        const float scale = kMaxWallPush / length;
        push[0] *= scale;
        push[1] *= scale;
        push[2] *= scale;
    }
    moveBy(push);

    const float angle = atan2Octant(normal[0], normal[1]);
    wallAngles[wallCount] = addAngle(-angle, -owner->m_xform.rotation[2]);
    wallCount++;
    if (wallCount >= kGroundWallAngles)
        wallCount = kGroundWallAngles - 1;
    return 0;
}

// Character against character: who yields, by the motions' キャラ反発LV.
int CharacterGround::meetCharacter(CollisionContact *contact, const float *normal, float depth)
{
    StageObject *other = contact->body->owner->getCollisionObject();
    if (isClimbMotion(other->m_motion->motionId)
        && other->m_ground->floorHeight - other->m_xform.position[2] > 100.0f)
        climbedOver = 1;

    float amount = depth;
    if (!owner->isPlayer()) {
        if (pushedThisFrame == 0 && isYieldingMotion(owner->m_motion->motionId)) {
            pushedThisFrame = 1;
            pushedBack += 0.1f;
            if (pushedBack > kMaxPushedBack)
                pushedBack = kMaxPushedBack;
        }
        const float sphereRadius = kPushSpheres[family].rows[contact->selfIndex].radius;
        float yields = sphereRadius / 2.0f;
        if (yields <= sphereRadius - pushedBack)
            yields = pushedBack;
        amount = fabsf(depth) - yields;
        if (amount < 0.0f)
            amount = 0.0f;
        if (depth <= 0.0f)
            amount = -amount;
    }

    const int mine = owner->isPlayer() && repelLevel < 2 ? repelLevel - 1 : repelLevel;
    const int theirRepel = other->m_ground->repelLevel;
    const int theirs = other->isPlayer() && theirRepel < 2 ? theirRepel - 1 : theirRepel;
    if (mine == theirs) {
        if (mine == 2) {
            wonStruggle = owner->m_motion->winsStruggle(other) ? 1 : 0;
            if (wonStruggle != 0)
                climbedOver = 0;
            amount = 0.0f;
        } else {
            amount /= 2.0f;
        }
    } else if (mine < theirs) {
        if (isDown(other)) {
            amount = 0.0f;
            narrow = 1;
        }
    } else {
        amount = 0.0f;
    }
    if (amount == 0.0f)
        return 0;

    sceVu0FVECTOR step;
    scaleVec4(amount, step, normal);
    step[2] = 0.0f;
    addTo(owner->m_xform.position, step);
    owner->m_model->pose->translate(step);
    for (StageList<BoundingSphere>::Node *node = pushBody->spheres.head; node != nullptr;
         node = node->next)
        addTo(node->value.center, step);
    return 0;
}

// What reaches the spheres it is hit on.
int CharacterGround::meetHit(CollisionContact *contact)
{
    if (wasHit != 0)
        return 0;
    StageObject *other = contact->body->owner->getCollisionObject();
    noteHitBone(contact->selfIndex, other);
    if (other->getKind() == StageObject_ThrownItem) {
        StageThrownItem *item = static_cast<StageThrownItem *>(other);
        sceVu0FVECTOR velocity;
        if (static_cast<ItemPhysics *>(item)->m_parent == contact->self->owner->getCollisionObject()) {
            // Its own throw reaches it only once it flies.
            item->getMeanVelocity(velocity);
            if (velocity[0] != 0.0f || velocity[1] != 0.0f || velocity[2] != 0.0f)
                wasHit = 1;
        } else {
            wasHit = 1;
        }
        if (wasHit != 0) {
            item->getMeanVelocity(hitDirection);
            vecNormalize(hitDirection, hitDirection);
        }
    }
    if (wasHit != 0)
        updateInBack();
    return 0;
}

// FS2 0x001f0a20
// callcheck: FUN_001f4e20, getKind, getCollisionObject, FUN_0023ce00, FUN_0023ce00, vecNormalize, sinCos, sinCos, vecDot, isPlayer, isPlayer, isPlayer, FUN_001e05b0, scaleVec4, translate, getNamed, FUN_001f0170, isDoorFace, scaleVec4, sphereHits, getNormal, getNormal, vecDot, vecCross, vecNormalize, vecDot, applyMatrix, vecDot, scaleVec4, translate, atan2Octant, addAngle, getNormal, atan2Octant, wrapAngle, wrapAngle, playerEvent, FUN_002a2b10

int CharacterGround::collisionHook0(CollisionContact *contact, const float *normal,
                                    StageCollisionFace *face, float depth)
{
    switch (contact->self->category) {
    case kCollisionMelee:
        return bumpWall(face);
    case kGroundWallBody:
        return meetWall(contact, normal, face, depth);
    case kGroundPushBody:
        return meetCharacter(contact, normal, depth);
    case kGroundNearBody:
        if (nearSomeone == 0)
            nearObject = contact->body->owner->getCollisionObject();
        nearSomeone = 1;
        return 0;
    case kGroundHitBody:
        return meetHit(contact);
    default:
        return 0;
    }
}

// FS2 0x001f06b0
// callcheck: FUN_001f4e20, FUN_001f4e20, find, find, getMatrix, getMatrix, segmentHitsAny, sinCos, sinCos, FUN_00210540, playerEvent, vecNormalize, sinCos, sinCos, vecDot, FUN_00210540, setContact, getKind

int CharacterGround::onCollide(CollisionContact *contact, const float *sweep,
                               StageCollisionFace *face, float t, float distance)
{
    (void)sweep;
    (void)face;
    (void)t;
    (void)distance;
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    if (contact->self->category == kGroundBlowBody) {
        if (blowLanded == 0 && character->getHandClass() != kHandClassDriving)
            setContact(0);
        blowLanded = 1;
        return 0;
    }
    if (contact->self->category != kGroundHitBody || wasHit != 0)
        return 0;
    StageObject *other = contact->body->owner->getCollisionObject();
    noteHitBone(contact->selfIndex, other);
    if (other->getKind() == StageObject_Character) {
        static const char kHead[] = "bn_head";  /* data 0x303178 */
        const sceVu0FVECTOR *mine = groundBone(owner, kHead)->getMatrix();
        const sceVu0FVECTOR *theirs = groundBone(other, kHead)->getMatrix();
        if (!gStageScene->segmentHitsAny(theirs[3], mine[3], 0, kFaceSight, kFaceSightSkip, nullptr)) {
            CharacterGround *ground = other->m_ground;
            int i = 0;
            while (i < ground->hitterCount && ground->hitters[i] != owner)
                i++;
            if (i == ground->hitterCount && ground->hitterCount < kGroundMaxHitters) {
                ground->hitters[ground->hitterCount] = owner;
                ground->hitterCount++;
                wasHit = 1;
                const float yaw = -other->m_xform.rotation[2];
                hitDirection[0] = -sinCos(yaw, 0);
                hitDirection[1] = -sinCos(yaw, 1);
                hitDirection[2] = 0.0f;
                hitDirection[3] = 0.0f;
                static_cast<StageCharacter *>(other)->playerEvent(
                    character->getHandClass() == kHandClassDriving ? "ATTACKHIT2" : "ATTACKHIT1"); /* data 0x305a58, 0x305a68 */
            }
        }
    } else if (other->getKind() == StageObject_Bullet && other->m_dead != 0) {
        wasHit = 1;
        copyTo(hitDirection, static_cast<StageBullet *>(other)->m_velocity);
        vecNormalize(hitDirection, hitDirection);
    }
    if (wasHit != 0)
        updateInBack();
    return 0;
}
