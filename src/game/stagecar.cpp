/* ========================================================================
 *  game/stagecar.cpp -- the car and what shows its damage (game/stagecar.h).
 *  0x289cc0..0x28fa90. The thunks 0x28f9f0..0x28fa88 of the side and the
 *  collision bases are the host's multiple inheritance.
 * ======================================================================== */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/characterground.h"
#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/objectproperty.h"
#include "game/rumblelist.h"
#include "game/stagecar.h"
#include "game/stagecollision.h"
#include "game/stageitem.h"
#include "game/stagelight.h"
#include "game/stageparticle.h"
#include "game/stagescene.h"
#include "game/stagesound.h"
#include "game/vu0math.h"

namespace {

constexpr char kBoneSeatRight[]  = "bn_dmg_sr";     // data 0x309320
constexpr char kBoneSeatLeft[]   = "bn_dmg_sl";     // data 0x309330
constexpr char kBoneColumn[]     = "bn_col";        // data 0x309478
constexpr char kBoneLampLeft[]   = "bn_Lt_l";       // data 0x309490
constexpr char kBoneLampRight[]  = "bn_Lt_r";       // data 0x3094a0
constexpr char kBoneTop[]        = "bn_top";        // data 0x309550
constexpr char kBoneRearWheel[]  = "bn_rl_wheel";   // data 0x3093f8
constexpr char kBoneBattery[]    = "bn_battery";    // data 0x309408
constexpr char kBoneDamageLeftB[]  = "bn_dmg_fl_B"; // data 0x3094b0
constexpr char kBoneDamageLeftA[]  = "bn_dmg_fl_A"; // data 0x3094c0
constexpr char kBoneDamageRightB[] = "bn_dmg_fr_B"; // data 0x3094d0
constexpr char kBoneDamageRightA[] = "bn_dmg_fr_A"; // data 0x3094e0
constexpr char kBoneDamageMidB[]   = "bn_dmg_fc_B"; // data 0x3094f0
constexpr char kBoneDamageMidA[]   = "bn_dmg_fc_A"; // data 0x309500
constexpr char kBoneBlood1[]     = "bn_bld_01";     // data 0x309510
constexpr char kBoneBlood2[]     = "bn_bld_02";     // data 0x309520
constexpr char kInsideWindow[]   = "BKW%02di";      // data 0x309480

constexpr char kLightSpot[]   = "CAR_sl";           // data 0x309558
constexpr char kLightPoint[]  = "CAR_pl";           // data 0x309560
constexpr char kLightSpot2[]  = "CAR_sl2";          // data 0x309568
constexpr char kLightPoint2[] = "CAR_pl2";          // data 0x309570
constexpr char kEffectLampLeft[]  = "carlightL";    // data 0x309578
constexpr char kEffectLampRight[] = "carlightR";    // data 0x309588
constexpr char kEffectTailLeft[]  = "tailLi";       // data 0x309498
constexpr char kEffectTailRight[] = "tailRi";       // data 0x3094a8
constexpr char kEffectGlass[]     = "carglass0";    // data 0x3093e8

constexpr char kSoundHit[]    = "CAR_HIT";          // data 0x309340
constexpr char kSoundClash1[] = "CAR_CLSH1";        // data 0x3093a8
constexpr char kSoundClash2[] = "CAR_CLSH2";        // data 0x3093b8
constexpr char kSoundClash3[] = "CAR_CLSH3";        // data 0x3093c8
constexpr char kSoundHorn[]   = "CAR_HORN";         // data 0x309418
constexpr char kSoundIdle[]   = "CAR_EG_ST";        // data 0x309428
constexpr char kSoundDrive[]  = "CAR_EG_DV";        // data 0x309438
constexpr char kSoundTyres[]  = "CAR_TURN";         // data 0x309448
constexpr char kSoundAccel[]  = "CAR_ACCEL";        // data 0x309458
constexpr char kSoundBrake[]  = "CAR_BRAKE";        // data 0x309468

// What the driver feels: playerEvent's names.
constexpr char kEventRanLight[]  = "CARCLASH5";     // data 0x309348
constexpr char kEventRanMedium[] = "CARCLASH6";     // data 0x309358
constexpr char kEventRanHeavy[]  = "CARCLASH7";     // data 0x309368
constexpr char kEventClash1[]    = "CARCLASH1";     // data 0x309378
constexpr char kEventClash2[]    = "CARCLASH2";     // data 0x309388
constexpr char kEventClash3[]    = "CARCLASH3";     // data 0x309398
constexpr char kEventClash4[]    = "CARCLASH4";     // data 0x3093d8

constexpr sceVu0FVECTOR kOrigin = { 0.0f, 0.0f, 0.0f, 1.0f };       // data 0x309310
constexpr sceVu0FVECTOR kLampPlace = { 2.9f, 0.0f, 6.9f, 1.0f };    // data 0x3092c0
constexpr sceVu0FVECTOR kLampTurn = { 0.0f, kMathPi / 2.0f, -kMathPi / 2.0f, 1.0f }; // data 0x2fb500
constexpr sceVu0FVECTOR kTailPlace = { 2.8f, 310.5f, -36.0f, 1.0f }; // data 0x2fb510

// Where the spheres go, on bn_col's axes.
constexpr float kSphereCorner    = 40.0f;
constexpr float kSphereSeat      = 20.0f;
constexpr float kSphereBody      = 35.0f;
constexpr float kSpheresAlong[]  = { -105.0f, -70.0f, -35.0f, 35.0f, 35.0f, 70.0f, 105.0f }; // data 0x3092f0
constexpr float kSpheresAcross[] = { 0.0f, 18.5f, -18.5f, 37.0f, -37.0f };                   // data 0x3092d0

// The spheres of m_body by place: the corners, the seats, then a row along
// each side, and four rows across, of five.
enum CarSphere
{
    CarSphere_Corners   = 0,
    CarSphere_SeatLeft  = 4,
    CarSphere_SeatRight = 5,
    CarSphere_RowLeft   = 6,
    CarSphere_RowRight  = 13,
    CarSphere_RearLow   = 20,
    CarSphere_FrontLow  = 25,
    CarSphere_RearHigh  = 30,
    CarSphere_FrontHigh = 35,
    CarSphere_End       = 40,
};

// The world's masks: the map and the objects in the way, the ground.
constexpr u_int kCarBodyMask  = 0xe9021f3;
constexpr u_int kCarSeatMask  = 0xe8021f3;
constexpr u_int kCarRayFlags  = 0x40000;
constexpr u_int kCarSeatFlags = 0x2040000;
constexpr u_int kCarGroundMask = 0x42400c;

constexpr float kGroundRayUp   = 50.0f;
constexpr float kGroundRayDown = 500.0f;

constexpr float kToKmh       = 0.036f;      // units a second
constexpr float kSeatReach   = 600.0f;
constexpr float kSeatOut     = -38.0f;      // along the bone's x
constexpr float kDoorAlong   = 84.0f;
constexpr float kSillOut     = -14.0f;
constexpr float kCenterBack  = -80.0f;
constexpr float kAhead       = 200.0f;
constexpr float kBehind      = -210.0f;
constexpr float kSeatClear   = 50.0f;       // the ray to a seat runs above the floor
constexpr float kPlaceAnswer = 40.0f;       // what the door places answer
constexpr float kSillAnswer  = 64.0f;
constexpr float kBoneAnswer  = 120.0f;

constexpr float kCreepKmh = 4.0f;           // data 0x31ac1c
constexpr float kCrawlKmh = 5.0f;           // data 0x31ac20
constexpr float kFastKmh  = 25.0f;
constexpr float kStillSpeed = 0.1f;
constexpr float kRestSeconds = 0.3f;        // data 0x31ac24
constexpr float kSteerRestSeconds = 0.5f;   // data 0x31ac28
constexpr float kSteerRest = 0.001f;
constexpr float kFrameSeconds = 0.033333335f;
constexpr int   kSettleSteps = 0x10;        // the body finds the ground after a warp

// The switch's parts.
constexpr int kNoPart = -1;
constexpr int kCarLightOrder = 1;
constexpr int kCarBodyBucket = 1;
constexpr int kCharacterHeavyHit = 2;       // a slide level above this makes a noise

void addVector(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int i = 0; i < 4; i++)
        out[i] = a[i] + b[i];
}

void subVector(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int i = 0; i < 4; i++)
        out[i] = a[i] - b[i];
}

// out = from + dir * scale, the ROM's scaleVec4 and vadd.
void offsetBy(sceVu0FVECTOR out, const sceVu0FVECTOR from, const sceVu0FVECTOR dir, float scale)
{
    sceVu0FVECTOR step;
    scaleVec4(scale, step, dir);
    addVector(out, from, step);
}

void copyVector(sceVu0FVECTOR out, const sceVu0FVECTOR from)
{
    memcpy(out, from, sizeof(sceVu0FVECTOR));
}

const char *seatBone(int side)
{
    return side == 0 ? kBoneSeatLeft : kBoneSeatRight;
}

void setPartMode(ModelInstance *model, const char *name, u_char mode)
{
    ModelPartRec *part = modelFindPart(model, name, strlen(name), 0);
    if (part != nullptr)
        part->drawMode = mode;
}

// A light on the bone and, in the object tree, under owner.
void hangLight(StageLight *light, StageNodeBase *bone, StageObject *owner)
{
    light->setParent(bone);
    light->m_tree->setParent(owner->m_tree);
}

}

// ──────────────────────────────────────────────────────────────────────
// Inline in the ROM (characters' calls and setMotionList's).
const sceVu0FVECTOR *StageCar::boneMatrix(const char *bone)
{
    return m_model->boneMap.find(bone)->value->getWorldMatrix();
}

float StageCar::speedKmh()
{
    const float length = getSpeed();
    const float seconds = gStageScene->clock->getDeltaSeconds();
    return seconds != 0.0f ? length * (kToKmh / seconds) : 0.0f;
}

bool StageCar::rumbleIdle(const char *name)
{
    return !gStageScene->rumble->isPlaying(name);
}

void StageCar::hurt(int slot, int amount)
{
    m_hurt = 1;
    if (m_health[slot] != 0 && m_damage[slot] < amount)
        m_damage[slot] = amount;
}

void StageCar::clearHits()
{
    m_blocked = m_hitFront != 0 || m_hitBack != 0;
    m_hurt = 0;
    memset(m_damage, 0, sizeof(m_damage));
    m_hitSide = 0;
    m_hitFront = 0;
    m_hitBack = 0;
    m_ranMedium = 0;
    m_ranLarge = 0;
    m_ranSmall = 0;
}

void StageCar::rememberPlace()
{
    copyVector(m_lastRotation, m_xform.rotation);
    copyVector(m_lastPosition, m_xform.position);
    memcpy(m_lastContacts, m_contacts, sizeof(m_contacts));
}

void StageCar::turnMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR angles)
{
    identityMatrix(out);
    if (m_xform.rotationXyz == 1) {
        rotateByAngles(out, out, angles);
    } else {
        rotateMatrixY(angles[1], out, out);
        rotateMatrixX(angles[0], out, out);
        rotateMatrixZ(angles[2], out, out);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00289ce0
// callcheck: setProperty

void StageCar::setProperty(int id, int sub, int value)
{
    if (id == CarProp_NoiseRange)
        m_noiseRange = (float)value;
    else if (id == CarProp_OnGround)
        m_onGround = value != 0;
    else
        StageObject::setProperty(id, sub, value);
}

// FS2 0x00289d40
// callcheck: getProperty

int StageCar::getProperty(int id, int sub)
{
    if (id == CarProp_Speed)
        return (int)getSpeed();
    if (id == CarProp_NoiseRange)
        return (int)m_noiseRange;
    if (id == CarProp_OnGround)
        return m_onGround;
    return StageMapObject::getProperty(id, sub);
}

// FS2 0x00289cd0

void StageCar::sideHook0()
{
}

// FS2 0x00289dd0

float StageCar::getSpeed()
{
    const float *drift = pose()->drift;
    return sqrtf(drift[2] * drift[2] + drift[0] * drift[0] + drift[1] * drift[1]);
}

// FS2 0x00289e10

const float *StageCar::getVelocity()
{
    return pose()->drift;
}

// FS2 0x00289e20
// callcheck: getDeltaSeconds

float StageCar::getAcceleration()
{
    const float z = m_stepChange[2];
    const float x = m_stepChange[0];
    const float y = m_stepChange[1];
    const float seconds = gStageScene->clock->getDeltaSeconds();
    if (seconds == 0.0f)
        return 0.0f;
    return sqrtf(z * z + x * x + y * y) * (1.0f / (seconds * seconds));
}

// FS2 0x00289ea0

const float *StageCar::getFrameStep()
{
    return m_frameStep;
}

// FS2 0x00289eb0

bool StageCar::isNear(float range, const float *pos) const
{
    const float dy = m_xform.position[1] - pos[1];
    const float dx = m_xform.position[0] - pos[0];
    return dx * dx + dy * dy < range * range;
}

// FS2 0x00289ef0
// callcheck: getMatrix, vecNormalize, scaleVec4 (offsetBy)

void StageCar::getCenter(float *out)
{
    sceVu0FVECTOR ahead;
    copyVector(ahead, getMatrix()[1]);
    vecNormalize(ahead, ahead);
    offsetBy(out, m_xform.position, ahead, kCenterBack);
}

// FS2 0x00289f70
// callcheck: find, getWorldMatrix, scaleVec4, find, getWorldMatrix, scaleVec4, getMatrix, vecNormalize, scaleVec4, getMatrix, vecNormalize, scaleVec4, find, getWorldMatrix, scaleVec4, scaleVec4 (boneMatrix, offsetBy)

float StageCar::getDoorPoint(sceVu0FVECTOR out, int side, int index)
{
    if (index < 0 || index > 3)
        index = 4;
    if (index == 4 || index == 3) {
        const sceVu0FVECTOR *bone = boneMatrix(seatBone(index == 4 ? side : !side));
        offsetBy(out, bone[3], bone[0], kSeatOut);
        out[2] = m_xform.position[2];
    } else if (index == 2 || index == 1) {
        sceVu0FVECTOR ahead;
        copyVector(ahead, getMatrix()[1]);
        vecNormalize(ahead, ahead);
        offsetBy(out, m_xform.position, ahead, index == 2 ? kAhead : kBehind);
    } else {
        const sceVu0FVECTOR *bone = boneMatrix(seatBone(side));
        offsetBy(out, bone[3], bone[0], kSeatOut);
        offsetBy(out, out, bone[1], side == 0 ? kDoorAlong : -kDoorAlong);
        out[2] = m_xform.position[2];
    }
    return kPlaceAnswer;
}

// FS2 0x0028a250
// callcheck: find, getWorldMatrix, scaleVec4, find, getWorldMatrix, scaleVec4, vecDistance, vecDistance, isNotRunning, getStageSelect, getBrainPace, isMode2, getMatrix, vecNormalize, scaleVec4, find, getWorldMatrix, scaleVec4, segmentHitsAny, sphereHitsAny, find, getWorldMatrix, scaleVec4, scaleVec4, segmentHitsAny, sphereHitsAny (boneMatrix, offsetBy, stageObjRecord, getCenter inline)

void StageCar::getSeatPlace(float *out, StageCharacter *who)
{
    sceVu0FVECTOR from;
    copyVector(from, who->m_xform.position);

    sceVu0FVECTOR seatRight;
    sceVu0FVECTOR seatLeft;
    const sceVu0FVECTOR *bone = boneMatrix(kBoneSeatRight);
    offsetBy(seatRight, bone[3], bone[0], kSeatOut);
    seatRight[2] = m_xform.position[2];
    bone = boneMatrix(kBoneSeatLeft);
    offsetBy(seatLeft, bone[3], bone[0], kSeatOut);
    seatLeft[2] = m_xform.position[2];

    const float toRight = vecDistance(from, seatRight);
    const float toLeft = vecDistance(from, seatLeft);
    if (kSeatReach < toRight && kSeatReach < toLeft) {
        copyVector(out, m_xform.position);
        return;
    }
    copyVector(out, toRight < toLeft ? seatRight : seatLeft);

    if (!gStageScene->fade->isNotRunning() || who->m_abilities.flag3be == 0)
        return;
    if (stageObjRecord(who->m_itemDef)->kind == kStageObjRecordCarProof)
        return;
    StageCharacter *driver = m_driver;
    if (driver == nullptr)
        return;
    if (driver->m_biteProof != 0 || driver->getBrainPace() == 0)
        return;
    if (driver->m_endurance < 1 || m_crawlTime < kRestSeconds || driver->m_lookAt != nullptr)
        return;
    StageCharacter *partner = static_cast<StageCharacter *>(gStageScene->partner);
    StageObject *held;
    if (driver == partner) {
        held = gStageScene->player->m_lookAt;
    } else {
        held = nullptr;
        if (partner != nullptr && partner->isMode2())
            held = partner->m_lookAt;
    }
    if (held != nullptr)
        return;

    // The way from the middle of the car to the seat, and on to the door,
    // above the floor: blocked, the other side.
    bool right = toRight < toLeft;
    sceVu0FVECTOR ahead;
    copyVector(ahead, getMatrix()[1]);
    vecNormalize(ahead, ahead);
    sceVu0FVECTOR start;
    offsetBy(start, m_xform.position, ahead, kCenterBack);
    start[2] += kSeatClear;

    for (int tries = 0; tries < 2; tries++) {
        const char *name = right ? kBoneSeatRight : kBoneSeatLeft;
        bone = boneMatrix(name);
        sceVu0FVECTOR point;
        offsetBy(point, bone[3], bone[0], kSillOut);
        sceVu0FVECTOR end;
        copyVector(end, point);
        end[2] += kSeatClear;
        if (!gStageScene->segmentHitsAny(start, end, 0, kCarSeatMask, kCarSeatFlags, this)
            && !gStageScene->sphereHitsAny(kSillAnswer, point, kCarSeatMask, kCarRayFlags, this)) {
            bone = boneMatrix(name);
            offsetBy(point, bone[3], bone[0], kSeatOut);
            offsetBy(point, point, bone[1], right ? kDoorAlong : -kDoorAlong);
            point[2] = m_xform.position[2];
            copyVector(end, point);
            end[2] = m_xform.position[2] + kSeatClear;
            if (!gStageScene->segmentHitsAny(start, end, 0, kCarSeatMask, kCarSeatFlags, this)
                && !gStageScene->sphereHitsAny(kPlaceAnswer, point, kCarSeatMask, kCarRayFlags, this))
                return;
        }
        right = !right;
        copyVector(out, right ? seatRight : seatLeft);
    }
}

// FS2 0x0028a890
// callcheck: find, getWorldMatrix, scaleVec4, scaleVec4 (boneMatrix, offsetBy)

float StageCar::getDoorPlace(sceVu0FVECTOR out, int side)
{
    const sceVu0FVECTOR *bone = boneMatrix(seatBone(side));
    offsetBy(out, bone[3], bone[0], kSeatOut);
    offsetBy(out, out, bone[1], side == 0 ? -kDoorAlong : kDoorAlong);
    out[2] = m_xform.position[2];
    return kPlaceAnswer;
}

// FS2 0x0028a9a0
// callcheck: find, getWorldMatrix, scaleVec4 (boneMatrix, offsetBy)

float StageCar::getSeatPoint(sceVu0FVECTOR out, int side)
{
    const sceVu0FVECTOR *bone = boneMatrix(seatBone(side));
    offsetBy(out, bone[3], bone[0], kSeatOut);
    out[2] = m_xform.position[2];
    return kPlaceAnswer;
}

// FS2 0x0028aa60
// callcheck: find, getWorldMatrix, scaleVec4 (boneMatrix, offsetBy)

float StageCar::getSillPoint(sceVu0FVECTOR out, int side)
{
    const sceVu0FVECTOR *bone = boneMatrix(seatBone(side));
    offsetBy(out, bone[3], bone[0], kSillOut);
    return kSillAnswer;
}

// FS2 0x0028ab00
// callcheck: find, getWorldMatrix (boneMatrix)

float StageCar::getDoorBone(float *out, int side)
{
    copyVector(out, boneMatrix(seatBone(side))[3]);
    return kBoneAnswer;
}

// FS2 0x0028ab70

bool StageCar::hasSteerRested()
{
    return kSteerRestSeconds <= m_steerRestTime;
}

// FS2 0x0028ab90

bool StageCar::hasSettled()
{
    return kRestSeconds <= m_settledTime;
}

// FS2 0x0028abb0

bool StageCar::isCrawling()
{
    return kRestSeconds <= m_crawlTime;
}

// FS2 0x0028abd0

bool StageCar::isCreeping()
{
    return kRestSeconds <= m_creepTime;
}

// The speed towards position, in km/h, in four steps.
// FS2 0x0028abf0
// callcheck: vecDirection, vecDot, scaleVec4, getDeltaSeconds

int StageCar::getHitLevel(const float *position)
{
    if (getSpeed() == 0.0f)
        return GroundSlide_None;
    sceVu0FVECTOR way;
    vecDirection(way, position, m_xform.position);
    scaleVec4(vecDot(way, pose()->drift), way, way);
    const float seconds = gStageScene->clock->getDeltaSeconds();
    float kmh = 0.0f;
    if (seconds != 0.0f)
        kmh = sqrtf(way[2] * way[2] + way[0] * way[0] + way[1] * way[1]) * (kToKmh / seconds);
    if (kmh < 4.0f)
        return 1;
    if (kmh < 12.0f)
        return 2;
    return kmh < 20.0f ? 3 : 4;
}

// FS2 0x0028ad50
// callcheck: raycast, vecDistance, vecDirection, scaleVec4, raycast, vecDistance, vecDirection, scaleVec4

void StageCar::snapToGround()
{
    if (m_onGround == 0 || m_model == nullptr || m_model->pose == nullptr
        || m_model->pose->current == nullptr)
        return;
    const auto drop = [this](sceVu0FVECTOR point) {
        sceVu0FVECTOR from;
        sceVu0FVECTOR to;
        copyVector(from, point);
        copyVector(to, point);
        from[2] = point[2] + kGroundRayUp;
        to[2] = point[2] - kGroundRayDown;
        sceVu0FVECTOR hit;
        if (gStageScene->raycast(hit, from, to, 0, kCarGroundMask, kCarRayFlags, this) == nullptr)
            return;
        const float left = vecDistance(hit, to);
        sceVu0FVECTOR way;
        vecDirection(way, from, to);
        offsetBy(point, point, way, left - kGroundRayDown);
    };
    for (int i = 0; i < kCarWheels; i++)
        drop(m_contacts[i]);
    drop(m_xform.position);
}

// Which spheres to try: the row or the end next to the one that went in.
// FS2 0x0028af70
// callcheck: FUN_001f4c30, sphereHitsAny, getNormal, vecCross, vecNormalize, vecNormalize, vecDot, asinAcos, vecCross, addAngle, identityMatrix, rotateByAngles, rotateMatrixY, rotateMatrixX, rotateMatrixZ, applyMatrix, subAngles, setPosition, rotate, solve, updateMatrix, syncBody (FUN_001f4c30 is StageList::at)

void StageCar::scrape(CollisionBody *body, int sphere, StageCollisionFace *face)
{
    int first[2] = { 0, 0 };
    int last[2] = { 0, 0 };
    if (sphere >= CarSphere_RowLeft && sphere < CarSphere_RowRight) {
        first[0] = CarSphere_RowRight;
        last[0] = CarSphere_RearLow;
    } else if (sphere >= CarSphere_RowRight && sphere < CarSphere_RearLow) {
        first[0] = CarSphere_RowLeft;
        last[0] = CarSphere_RowRight;
    } else if ((sphere >= CarSphere_RearLow && sphere < CarSphere_FrontLow)
               || (sphere >= CarSphere_RearHigh && sphere < CarSphere_FrontHigh)) {
        first[0] = CarSphere_FrontLow;
        last[0] = CarSphere_RearHigh;
        first[1] = CarSphere_FrontHigh;
        last[1] = CarSphere_End;
    } else if ((sphere >= CarSphere_FrontLow && sphere < CarSphere_RearHigh)
               || (sphere >= CarSphere_FrontHigh && sphere < CarSphere_End)) {
        first[0] = CarSphere_RearLow;
        last[0] = CarSphere_FrontLow;
        first[1] = CarSphere_RearHigh;
        last[1] = CarSphere_FrontHigh;
    }

    for (int range = 0; range < 2; range++) {
        for (int index = first[range]; index < last[range]; index++) {
            StageList<BoundingSphere>::Node *node = body->spheres.at(index);
            if (node == nullptr)
                continue;
            const BoundingSphere &other = node->value;
            if (!gStageScene->sphereHitsAny(other.radius * 0.9f, other.center, kCarBodyMask,
                                            kCarRayFlags, this))
                continue;

            // How far the way it goes is off the wall, a 64th of it a time.
            sceVu0FVECTOR normal;
            face->getNormal(normal);
            const sceVu0FVECTOR up = { 0.0f, 0.0f, 1.0f, 0.0f };
            sceVu0FVECTOR along;
            vecCross(along, up, normal);
            vecNormalize(along, along);
            sceVu0FVECTOR going;
            vecNormalize(going, pose()->drift);
            going[2] = 0.0f;
            along[2] = 0.0f;
            float cosine = vecDot(going, along);
            if (cosine < 0.0f) {
                cosine = -cosine;
                going[0] = -going[0];
                going[1] = -going[1];
            }
            float turn = asinAcos(cosine, 1) / 64.0f;
            vecCross(along, going, along);
            if (along[2] < 0.0f)
                turn = -turn;

            m_xform.rotation[0] = m_lastRotation[0];
            m_xform.rotation[1] = m_lastRotation[1];
            m_xform.rotation[2] = addAngle(m_lastRotation[2], turn);
            copyVector(m_xform.position, m_lastPosition);
            memcpy(m_contacts, m_lastContacts, sizeof(m_contacts));

            const sceVu0FVECTOR angles = { 0.0f, 0.0f, turn, 0.0f };
            sceVu0FMATRIX matrix;
            turnMatrix(matrix, angles);
            for (int i = 0; i < kCarWheels; i++) {
                sceVu0FVECTOR point;
                subVector(point, m_contacts[i], m_xform.position);
                applyMatrix(point, matrix, point);
                addVector(m_contacts[i], point, m_xform.position);
            }

            CarPose *carPose = pose();
            sceVu0FVECTOR delta;
            subAngles(delta, m_xform.rotation, carPose->rotation);
            carPose->setPosition(m_xform.position);
            carPose->rotate(delta);
            carPose->solve(PoseSlot_Final, -1, 0);
            for (BoneMap::Node *bone = m_model->boneMap.first; bone != nullptr; bone = bone->next)
                bone->value->updateMatrix();
            syncBody(body);
            return;
        }
    }
}

// What touches it: a blade or a bullet breaks the window of the face, the
// spheres take the map (a corner stops it, a seat dents, the rest scrapes),
// and the mesh takes characters.
// FS2 0x0028b470
// callcheck: getCollisionObject, isPlayer, breakWindow, getCollisionObject, getKind, isPlayer, breakWindow, getDeltaSeconds, isPlayer, vecNormalize, vecDot, isPlaying, isPlaying, isPlaying, playerEvent, playNamedHeard2, isPlaying, isPlaying, playerEvent, playNamedHeard2, isPlaying, playerEvent, playNamedHeard2, isPlaying, playerEvent, playNamedHeard2, isPlaying, playerEvent, isPlaying, playerEvent, breakWindow, scaleVec4, scrape, getCollisionObject, getKind, getCollisionObject, getStageSelect, getStageSelect, playNamedHeard2, playerEvent, playerEvent, playerEvent (stageObjRecord, speedKmh, rumbleIdle)

int StageCar::collisionHook0(CollisionContact *contact, const float *normal, StageCollisionFace *face,
                             float depth)
{
    CollisionBody *other = contact->body;
    if (other->category == kCollisionMelee) {
        StageObject *object = other->owner->getCollisionObject();
        if (object->m_weapons[0] != nullptr) {
            if (m_touchedBy == nullptr || object->isPlayer())
                m_touchedBy = object;
            if (face->doorId > 0)
                breakWindow(face->doorId - 1, true);
        }
    } else if (other->category == kCollisionBulletRay) {
        StageObject *object = other->owner->getCollisionObject();
        if (object->getKind() == StageObject_Bullet) {
            StageObject *shooter = static_cast<StageBullet *>(object)->shooter();
            if (m_touchedBy == nullptr || shooter->isPlayer())
                m_touchedBy = shooter;
        }
        if (face->doorId > 0)
            breakWindow(face->doorId - 1, true);
    }

    CollisionBody *self = contact->self;
    if (self->category == kCollisionCarSpheres) {
        if (m_colBone == nullptr)
            return 0;
        const float kmh = speedKmh();
        const int sphere = contact->selfIndex;
        if (sphere < CarSphere_SeatLeft) {
            if (m_driver != nullptr && !m_driver->isPlayer())
                return 0;
            bool headOn = false;
            if (getSpeed() == 0.0f && m_blocked != 0) {
                headOn = true;
            } else {
                sceVu0FVECTOR going;
                vecNormalize(going, pose()->drift);
                if (0.9f < fabsf(vecDot(going, normal)))
                    headOn = true;
            }
            RumbleList *rumble = gStageScene->rumble;
            if (headOn) {
                bool away;
                if (sphere < 2) {
                    away = m_direction < 1;
                    m_hitFront = 1;
                } else {
                    away = -1 < m_direction;
                    m_hitBack = 1;
                }
                if (!away && rumble != nullptr && 1.0f <= kmh) {
                    if (kmh < 4.0f) {
                        if (rumbleIdle(kEventClash1) && rumbleIdle(kEventClash2) && rumbleIdle(kEventClash3)) {
                            m_driver->playerEvent(kEventClash1);
                            playNamedHeard2(kSoundClash1, m_xform.position, nullptr);
                        }
                    } else if (kmh < 12.0f) {
                        if (rumbleIdle(kEventClash2) && rumbleIdle(kEventClash3)) {
                            m_driver->playerEvent(kEventClash2);
                            playNamedHeard2(kSoundClash2, m_xform.position, nullptr);
                        }
                    } else if (rumbleIdle(kEventClash3)) {
                        m_driver->playerEvent(kEventClash3);
                        playNamedHeard2(kSoundClash3, m_xform.position, nullptr);
                    }
                }
            } else {
                const bool into = sphere < 2 ? 0 < m_direction : m_direction < 0;
                m_hitSide = 1;
                if (into && rumble != nullptr && 12.0f <= kmh && rumbleIdle(kEventClash4)) {
                    m_driver->playerEvent(kEventClash4);
                    playNamedHeard2(kSoundClash1, m_xform.position, nullptr);
                }
            }
        } else if (sphere == CarSphere_SeatLeft || sphere == CarSphere_SeatRight) {
            if (3.0f < kmh) {
                hurt(sphere - CarSphere_SeatLeft, (int)kmh / 2);
                if (gStageScene->rumble != nullptr && m_driver != nullptr && rumbleIdle(kEventClash4))
                    m_driver->playerEvent(kEventClash4);
            }
        } else {
            if (kmh != 0.0f) {
                int slot = 4;
                const int row = sphere - CarSphere_RearLow;
                if (row >= 0 && row < 3) {
                    if (3.0f < m_fastTime) {
                        breakWindow(0, true);
                        m_fastTime = 0.0f;
                    }
                } else if (row == 3) {
                    slot = m_health[CarDamage_FrontLeft] == 0 ? CarDamage_FrontMid : CarDamage_FrontLeft;
                } else if (row == 4) {
                    slot = m_health[CarDamage_FrontRight] == 0 ? CarDamage_FrontMid : CarDamage_FrontRight;
                } else {
                    slot = -1;
                }
                if (slot != -1 && 4.0f <= kmh) {
                    if (kmh < 10.0f) {
                        if (50 < m_health[slot])
                            hurt(slot, 2);
                    } else {
                        hurt(slot, (int)kmh / 2);
                    }
                }
            }
            sceVu0FVECTOR push;
            scaleVec4(depth, push, normal);
            push[2] = 0.0f;
            push[3] = 0.0f;
            addVector(m_xform.position, m_xform.position, push);
            for (StageList<BoundingSphere>::Node *node = self->spheres.head; node != nullptr; node = node->next)
                addVector(node->value.center, node->value.center, push);
            for (int i = 0; i < kCarWheels; i++)
                addVector(m_contacts[i], m_contacts[i], push);
            addVector(m_push, m_push, push);
            scrape(self, sphere, face);
        }
    } else if (self->category == kCollisionMesh) {
        StageObject *object = other->owner->getCollisionObject();
        if (object->getKind() != StageObject_Character)
            return 0;
        StageCharacter *character = static_cast<StageCharacter *>(object);
        CharacterGround *ground = character->m_ground;
        const int level = ground->slideLevel;
        if (ground->carHit == 0 || level < 2)
            return 0;
        if (stageObjRecord(character->m_itemDef)->kind == kStageObjRecordCarProof)
            return 0;
        if (m_health[CarDamage_Blood1] == 0) {
            hurt(CarDamage_Window0, level * 10);
        } else {
            hurt(CarDamage_Blood1, level * 10);
            if (m_health[CarDamage_Blood1] <= level * 10)
                hurt(CarDamage_FrontMid, m_health[CarDamage_FrontMid]);
        }
        const int kind = stageObjRecord(character->m_itemDef)->kind;
        if (kind == 10 || kind == 0xc || kind == 6 || kind == 5)
            m_ranLarge = 1;
        else if (kind == 7)
            m_ranSmall = 1;
        else
            m_ranMedium = 1;
        if (level > kCharacterHeavyHit)
            playNamedHeard2(kSoundHit, character->m_xform.position, nullptr);
        if (m_driver != nullptr) {
            if (level == 4 || level == 3)
                m_driver->playerEvent(kEventRanHeavy);
            else if (level == 2)
                m_driver->playerEvent(kEventRanMedium);
            else if (level == 1)
                m_driver->playerEvent(kEventRanLight);
        }
    }
    return 0;
}

// FS2 0x0028c110
// callcheck: partName, find, spawn (hurt)

void StageCar::breakWindow(int part, bool withEffect)
{
    MapObjectParts *parts = m_mapParts;
    int index = 0;
    while (index < parts->count && parts->ids[index] != part)
        index++;
    if (index >= parts->count)
        return;
    const int slot = part + CarDamage_Window0;
    const bool known = slot >= 0 && slot < kCarDamageSlots;
    const int health = known ? m_health[slot] : 0;
    const int taken = known ? m_damage[slot] : 0;
    if (health <= 0 || taken != 0)
        return;
    if (part == 0) {
        if (m_touchedBy != m_lastTouchedBy)
            hurt(slot, health == 1 ? 1 : health - 1);
    } else {
        hurt(slot, health);
    }
    if (withEffect) {
        BoneMap::Node *bone = m_model->boneMap.find(parts->switches[index].partName(0));
        if (bone != nullptr)
            gStageScene->nodeTree->spawn(kEffectGlass, bone->value, nullptr, nullptr, 0, 0);
    }
}

// The frame's end: the last step and the change of it, the place to come
// back to, and the model where the contacts moved it.
// FS2 0x0028c310
// callcheck: snapToGround, translate, updateMatrix

void StageCar::collisionHook3(CollisionBody *body)
{
    if (body->category != kCollisionCarSpheres)
        return;
    snapToGround();
    sceVu0FVECTOR step;
    subVector(step, m_xform.position, m_lastPosition);
    subVector(m_stepChange, step, m_frameStep);
    copyVector(m_frameStep, step);
    rememberPlace();
    if (getSpeed() != 0.0f || m_placed != 0) {
        pose()->translate(m_push);
        for (BoneMap::Node *bone = m_model->boneMap.first; bone != nullptr; bone = bone->next)
            bone->value->updateMatrix();
    }
}

// The spheres on bn_col: their places are CarSphere's.
// FS2 0x0028c440
// callcheck: getWorldMatrix, scaleVec4, find, getWorldMatrix, scaleVec4, find, getWorldMatrix, scaleVec4, scaleVec4, scaleVec4 (boneMatrix, offsetBy)

void StageCar::syncBody(CollisionBody *body)
{
    if (body->category != kCollisionCarSpheres || m_colBone == nullptr)
        return;
    const sceVu0FVECTOR *axes = m_colBone->getWorldMatrix();
    int index = 0;
    for (StageList<BoundingSphere>::Node *node = body->spheres.head; node != nullptr; node = node->next, index++) {
        float *center = node->value.center;
        if (index == CarSphere_SeatLeft || index == CarSphere_SeatRight) {
            offsetBy(center, boneMatrix(index == CarSphere_SeatLeft ? kBoneSeatLeft : kBoneSeatRight)[3],
                     axes[1], -30.0f);
            continue;
        }
        float across;
        float along;
        float up;
        if (index >= CarSphere_RowLeft && index < CarSphere_RowRight) {
            across = 35.0f;
            along = kSpheresAlong[index - CarSphere_RowLeft];
            up = 35.0f;
        } else if (index >= CarSphere_RowRight && index < CarSphere_RearLow) {
            across = -35.0f;
            along = kSpheresAlong[index - CarSphere_RowRight];
            up = 35.0f;
        } else if (index >= CarSphere_RearLow && index < CarSphere_FrontLow) {
            across = kSpheresAcross[index - CarSphere_RearLow];
            along = -136.0f;
            up = 35.0f;
        } else if (index >= CarSphere_FrontLow && index < CarSphere_RearHigh) {
            across = kSpheresAcross[index - CarSphere_FrontLow];
            along = 119.0f;
            up = 35.0f;
        } else if (index >= CarSphere_RearHigh && index < CarSphere_FrontHigh) {
            across = kSpheresAcross[index - CarSphere_RearHigh];
            along = -110.0f;
            up = 145.0f;
        } else if (index >= CarSphere_FrontHigh && index < CarSphere_End) {
            across = kSpheresAcross[index - CarSphere_FrontHigh];
            along = 119.0f;
            up = 60.0f;
        } else {
            across = (index & 1) == 0 ? 35.0f : -35.0f;
            along = index < 2 ? -136.0f : 119.0f;
            up = 40.0f;
        }
        sceVu0FVECTOR offset;
        sceVu0FVECTOR height;
        scaleVec4(along, offset, axes[1]);
        scaleVec4(up, height, axes[2]);
        addVector(offset, offset, height);
        offsetBy(offset, offset, axes[0], across);
        addVector(center, m_xform.position, offset);
    }
}

// The script's pieces, the frame's damage, then what shows of it.
// FS2 0x0028c930
// callcheck: strlen, modelFindPart, strlen, modelFindPart, strlen, modelFindPart, strlen, modelFindPart, setGroup, showDamage, isInView, showWindows, isPlayer, updateSounds (setPartMode; collisionAt)

void StageCar::sideHook3()
{
    CarParts *parts = m_lights;
    ModelInstance *model = parts->model->data;
    if (m_mode == CarMode_ShowBattery) {
        setPartMode(model, kBoneBattery, kPartShown);
        m_mode = CarMode_Normal;
    } else if (m_mode == CarMode_HideBattery) {
        setPartMode(model, kBoneBattery, kPartHidden);
    } else if (m_mode == CarMode_ShowWheel) {
        setPartMode(model, kBoneRearWheel, kPartShown);
        m_mode = CarMode_Normal;
    } else if (m_mode == CarMode_HideWheel) {
        setPartMode(model, kBoneRearWheel, kPartHidden);
    }

    if (m_hurt != 0) {
        for (int i = 0; i < kCarDamageSlots; i++) {
            m_health[i] -= m_damage[i];
            if (m_health[i] < 0)
                m_health[i] = 0;
        }
        for (int i = 0; i < kCarDamageSlots; i++) {
            if (m_health[i] != 0 || m_damage[i] == 0)
                continue;
            if (i >= CarDamage_Window0 && i <= CarDamage_Window2) {
                collisionAt(0)->setGroup(i - CarDamage_Window0 + 1, false);
            } else if (i < kCarDamageFlags) {
                // PC: the last slot has no flag; the ROM's byte lands past the object.
                parts->damaged[i] = 1;
                parts->showDamage(i);
            }
        }
    }

    for (int i = 0; i < m_mapParts->count; i++) {
        const int id = m_mapParts->ids[i];
        if (id == 2 || id == 1) {
            parts->windowShown[id] = m_health[id + CarDamage_Window0] != 0;
        } else if (id == 0) {
            const int health = m_health[CarDamage_Window0];
            parts->windowShown[0] = health == 1 ? 2 : health != 0;
        }
    }

    parts->viewInside = gStageScene->isInView(this);
    parts->showWindows();
    m_placed = 0;

    // callcheck: the ROM's lights as CarParts::setLightOrder writes them
    if ((parts->spot == nullptr ? kCarLightOrder : parts->spot->m_priority) != kCarLightOrder)
        parts->setLightOrder(kCarLightOrder);
    parts->spot->m_shadowOwner = m_driver;
    parts->spot2->m_shadowOwner = m_driver;
    parts->point->m_shadowOwner = m_driver;
    parts->point2->m_shadowOwner = m_driver;
    if (m_driver != nullptr) {
        const bool player = m_driver->isPlayer();
        parts->spot->m_castsShadow = player;
        parts->spot2->m_castsShadow = player;
    }
    updateSounds();
    m_lastTouchedBy = m_touchedBy;
}

// The body rides its contacts; a warp lets it find them first, and the
// timers tell how long it has been slow.
// FS2 0x0028cda0
// callcheck: startOver, getDeltaSeconds, drive, drive, advance, advanceMotion, getWheelPoints, getDeltaSeconds, collisionHook3 (speedKmh)

void StageCar::sideHook2()
{
    m_body->on = m_driver != nullptr;
    if (CarPose *carPose = pose()) {
        const bool moved = m_lastPosition[0] != m_xform.position[0] || m_lastPosition[1] != m_xform.position[1]
                        || m_lastPosition[2] != m_xform.position[2];
        const bool turned = m_lastRotation[0] != m_xform.rotation[0] || m_lastRotation[1] != m_xform.rotation[1]
                         || m_lastRotation[2] != m_xform.rotation[2];
        if (moved || turned)
            startOver(moved, turned);
        const float seconds = gStageScene->clock->getDeltaSeconds();
        if (m_onGround == 0)
            carPose->drive(1.0f, nullptr, nullptr, nullptr, nullptr);
        else
            carPose->drive(seconds / kFrameSeconds, m_contacts[0], m_contacts[1], m_contacts[2], m_contacts[3]);
        if (moved || turned || m_placed != 0) {
            for (int i = 0; i < kSettleSteps; i++)
                m_model->advance(1.0f, m_xform.position, m_xform.rotation, 0);
        }
        advanceMotion(1.0f);
        carPose->getWheelPoints(m_contacts[0], m_contacts[1], m_contacts[2], m_contacts[3]);

        const float kmh = speedKmh();
        if (kmh <= kCreepKmh) {
            if (m_creepTime < kRestSeconds)
                m_creepTime += seconds;
        } else {
            m_creepTime = 0.0f;
        }
        if (kmh <= kCrawlKmh) {
            if (m_crawlTime < kRestSeconds)
                m_crawlTime += seconds;
        } else {
            m_crawlTime = 0.0f;
        }
        if (kmh <= kStillSpeed) {
            if (carPose->speedTarget == 0.0f) {
                carPose->speed = 0.0f;
                memset(carPose->drift, 0, sizeof(carPose->drift));
            }
            if (m_settledTime < kRestSeconds) {
                m_settledTime += seconds;
            } else if (m_placed == 0) {
                // At rest it does not creep: x and y stay as they were.
                for (int i = 0; i < 2; i++) {
                    m_xform.rotation[i] = m_lastRotation[i];
                    m_xform.position[i] = m_lastPosition[i];
                    for (int wheel = 0; wheel < kCarWheels; wheel++)
                        m_contacts[wheel][i] = m_lastContacts[wheel][i];
                }
            }
        } else {
            m_settledTime = 0.0f;
        }
        if (fabsf(m_steer) < kSteerRest) {
            if (m_steerRestTime < kSteerRestSeconds)
                m_steerRestTime += seconds;
        } else {
            m_steerRestTime = 0.0f;
        }
        if (kFastKmh < kmh)
            m_fastTime += seconds;
        else
            m_fastTime = 0.0f;
    }
    clearHits();
    memset(m_push, 0, sizeof(m_push));
    if (m_body->on == 0)
        collisionHook3(m_body);
}

// The pedals: what they ask of the pose, less what the frame's hits take.
// FS2 0x0028d290
// callcheck: getDeltaSeconds, sceVu0ScaleVector, setDrive (speedKmh; scaleVec4 is the ROM's sceVu0ScaleVector)

void StageCar::sideHook1()
{
    CarPose *carPose = pose();
    if (carPose == nullptr)
        return;
    float brake = ((float)m_brake / 255.0f) * m_maxBrake;
    if (10.0f < speedKmh()) {
        if (m_maxBrake * 0.8f < brake) {
            if (m_brakeState == 0)
                m_brakeState = 1;
        } else if (m_brakeState != 0 && brake < m_maxBrake * 0.4f) {
            m_brakeState = 0;
        }
    }

    float drive;
    if (m_direction == -1) {
        drive = ((float)m_throttle / -255.0f) * m_topReverse;
        if (drive != 0.0f && 0.0f < carPose->speed)
            brake = m_maxBrake;
    } else {
        drive = ((float)m_throttle / 255.0f) * m_topSpeed;
        if (drive != 0.0f && carPose->speed < 0.0f)
            brake = m_maxBrake;
    }

    if ((float)m_throttleLog[kCarThrottleLog - 1] + 102.0f < (float)m_throttle)
        m_pedalStamped = 1;
    // The ROM's slip: it goes up the log, so every entry takes the first, and
    // the last entry is simply last frame's pedal.
    for (int i = 0; i < kCarThrottleLog - 1; i++)
        m_throttleLog[i + 1] = m_throttleLog[i];
    m_throttleLog[0] = m_throttle;

    float drag = 1.0f;
    if (m_ranMedium != 0)
        drag = 0.6f;
    else if (m_ranSmall != 0)
        drag = 0.9f;
    else if (m_ranLarge != 0)
        drag = 0.1f;
    if (m_hitFront != 0 || m_hitBack != 0)
        drag = 0.0f;
    else if (m_hitSide != 0)
        drag = 0.9f;

    if (drag != 1.0f) {
        bool held = false;
        if (m_hitFront != 0)
            held = 0.0f <= carPose->speed && 0.0f <= drive;
        else if (m_hitBack != 0)
            held = carPose->speed <= 0.0f && drive <= 0.0f;
        else
            held = (0.0f < carPose->speed && 0.0f <= drive) || (carPose->speed < 0.0f && drive <= 0.0f);
        if (held) {
            drive *= drag;
            carPose->speed *= drag;
            scaleVec4(drag, carPose->drift, carPose->drift);
        }
    }
    carPose->setDrive(drive, brake, -m_steer * m_steerRate);
    m_honked = m_horn;
    m_horn = 0;
}

// FS2 0x0028d780
// callcheck: playNamed, isPlayer, addNoise, addNoise

void StageCar::honk()
{
    if (m_driver == nullptr)
        return;
    playNamed(kSoundHorn, m_xform.position, nullptr);
    m_horn = 1;
    gStageScene->noises->addNoise(m_noiseRange, m_driver, m_driver->isPlayer() ? 1 : 2);
}

// FS2 0x0028d810

void StageCar::setDirection(int direction)
{
    if (direction == 1 || direction == -1)
        m_direction = (signed char)direction;
}

// FS2 0x0028d850
// callcheck: memset, drive, addAngle, identityMatrix, rotateByAngles, rotateMatrixY, rotateMatrixX, rotateMatrixZ, applyMatrix, snapToGround

void StageCar::startOver(bool moved, bool turned)
{
    m_brakeState = 0;
    m_pedalStamped = 0;
    memset(m_throttleLog, 0, sizeof(m_throttleLog));
    m_throttle = 0;
    m_direction = 1;
    m_brake = 0;
    m_steer = 0.0f;
    m_creepTime = kRestSeconds;
    m_crawlTime = kRestSeconds;
    m_settledTime = kRestSeconds;
    m_steerRestTime = kSteerRestSeconds;
    m_fastTime = 0.0f;
    if (CarPose *carPose = pose())
        carPose->drive(1.0f, nullptr, nullptr, nullptr, nullptr);

    if (moved) {
        sceVu0FVECTOR step;
        subVector(step, m_xform.position, m_lastPosition);
        for (int i = 0; i < kCarWheels; i++)
            addVector(m_contacts[i], m_contacts[i], step);
    }
    if (turned) {
        sceVu0FVECTOR angles;
        copyVector(angles, m_xform.rotation);
        for (int i = 0; i < 3; i++)
            angles[i] = addAngle(angles[i], -m_lastRotation[i]);
        sceVu0FMATRIX matrix;
        turnMatrix(matrix, angles);
        for (int i = 0; i < kCarWheels; i++) {
            sceVu0FVECTOR point;
            subVector(point, m_contacts[i], m_xform.position);
            applyMatrix(point, matrix, point);
            addVector(m_contacts[i], point, m_xform.position);
        }
    }
    snapToGround();
    rememberPlace();
    m_placed = 1;
    memset(m_frameStep, 0, sizeof(m_frameStep));
    memset(m_stepChange, 0, sizeof(m_stepChange));
}

// FS2 0x0028dac0
// callcheck: startOver

void StageCar::resetPlace()
{
    const bool moved = m_lastPosition[0] != m_xform.position[0] || m_lastPosition[1] != m_xform.position[1]
                    || m_lastPosition[2] != m_xform.position[2];
    const bool turned = m_lastRotation[0] != m_xform.rotation[0] || m_lastRotation[1] != m_xform.rotation[1]
                     || m_lastRotation[2] != m_xform.rotation[2];
    startOver(moved, turned);
}

// FS2 0x0028db80
// callcheck: setEffects, setEmitting, setEmitting

void StageCar::setLights(bool on)
{
    m_lights->setEffects(on);
    for (int i = 0; i < kCarEffects; i++) {
        if (m_tailLights[i] != nullptr)
            m_tailLights[i]->setEmitting(on);
    }
}

// The engine runs with someone at the wheel: idling, or by the speed, and
// the tyres in a hard turn.
// FS2 0x0028dc00
// callcheck: getBrainPace, stopSound, findSource, strcmp, stopSound, playNamedHeard2, findSource, strcmp, stopSound, playNamedHeard2, getDeltaSeconds, setSourcePitch, playNamedHeard2, playNamedHeard2, getDeltaSeconds, findSource, playNamedHeard2, stopSound (speedKmh)

void StageCar::updateSounds()
{
    if (m_driver == nullptr || m_driver->getBrainPace() == 0) {
        for (int i = 0; i < kCarSounds; i++) {
            stopSound(m_sounds[i]);
            m_sounds[i] = nullptr;
        }
        return;
    }

    StageSound *sound = gStageScene->sound;
    const auto engine = [this, sound](const char *name) {
        StageSoundSource *source = sound->findSource(m_sounds[0]);
        if (source != nullptr && strcmp(source->m_entry->name, name) != 0) {
            stopSound(source);
            source = nullptr;
        }
        if (source == nullptr)
            m_sounds[0] = playNamedHeard2(name, kOrigin, this);
    };
    if (kRestSeconds <= m_creepTime) {
        engine(kSoundIdle);
    } else {
        engine(kSoundDrive);
        const float kmh = speedKmh();
        const float rise = kmh < 20.0f ? kmh * 0.05f : 1.0f;
        sound->setSourcePitch(m_sounds[0], rise + 1.0f);
    }

    if (m_pedalStamped == 1) {
        playNamedHeard2(kSoundAccel, m_xform.position, nullptr);
        m_pedalStamped = 0;
    }
    if (m_brakeState == 1) {
        playNamedHeard2(kSoundBrake, m_xform.position, nullptr);
        m_brakeState = 2;
    }

    bool hardTurn = false;
    if (kFastKmh <= speedKmh()) {
        const float limit = kMathPi / 2.0f < m_steerRate ? kMathPi / 2.0f : m_steerRate;
        hardTurn = limit * 0.95f < fabsf(pose()->steerTarget);
    }
    StageSoundSource *tyres = sound->findSource(m_sounds[1]);
    if (hardTurn) {
        if (tyres == nullptr)
            m_sounds[1] = playNamedHeard2(kSoundTyres, kOrigin, this);
    } else if (tyres != nullptr) {
        stopSound(tyres);
        m_sounds[1] = nullptr;
    }
}

// The parts come after the map object's: its faces follow bn_col, and the
// lights hang on the model.
// FS2 0x0028e000
// callcheck: setMotionList, find, setInverseTree, getWorldMatrix, startOver, operator_new, operator_new[], FUN_00104d90, sprintf, init, buildLights, copyFrom, find, spawn, setEmitting, erase, erase, pushBack, find, spawn, setEmitting, erase, erase, pushBack (new CarParts is inline in the ROM; collisionAt)

void StageCar::setMotionList(void *list, int arg, int arg2)
{
    StageMapObject::setMotionList(list, arg, arg2);

    StageCollision *mesh = collisionAt(0);
    if (mesh != nullptr) {
        BoneMap::Node *column = m_model->boneMap.find(kBoneColumn);
        if (column != nullptr) {
            m_colBone = column->value;
            m_colBone->setInverseTree(true);
            mesh->m_matrix = m_colBone->getWorldMatrix();
            mesh->m_inverse = m_colBone->m_inverse;
        }
    }

    for (int i = 0; i < kCarWheels; i++)
        copyVector(m_contacts[i], m_xform.position);
    startOver(false, false);
    m_lights = new CarParts(this, m_mapParts->switches, m_mapParts->ids, m_mapParts->count, nullptr);

    const auto tail = [this](const char *name, const char *bone, const float *place) {
        StageParticle *effect = gStageScene->nodeTree->spawn(name, m_model->boneMap.find(bone)->value, place,
                                                             nullptr, 0, 0);
        if (effect != nullptr) {
            effect->setEmitting(false);
            effect->m_tree->setParent(m_lights->spot->m_tree);
        }
        return effect;
    };
    sceVu0FVECTOR place;
    copyVector(place, kTailPlace);
    m_tailLights[0] = tail(kEffectTailLeft, kBoneLampLeft, place);
    place[0] = -place[0];
    m_tailLights[1] = tail(kEffectTailRight, kBoneLampRight, place);
}

// FS2 0x0028e4b0
// callcheck: stopSound, __destroy_new_array, operator_delete, ~StageMapObject, operator_delete

StageCar::~StageCar()
{
    for (int i = 0; i < kCarSounds; i++)
        stopSound(m_sounds[i]);
    delete m_lights;
}

// The spheres go in the world now; bn_col places them. The scene keeps it as
// its car.
// FS2 0x0028e5a0
// callcheck: StageMapObject, operator_new, CollisionBody, add, BoundingSphere, FUN_001f4d30, setDirtyTree

StageCar::StageCar() : StageMapObject(1)
{
    clearHits();
    for (int i = 0; i < kCarDamageSlots; i++)
        m_health[i] = 100;
    m_blocked = 0;
    gStageScene->car = this;

    m_body = new CollisionBody(this, kCarBodyBucket, kCollisionCarSpheres, kCollisionMesh);
    gStageScene->records->add(m_body);
    m_body->shape = CollisionShape_Spheres;
    m_body->rayMask = kCarBodyMask;
    m_body->rayFlags = kCarRayFlags;
    BoundingSphere sphere;
    for (int i = 0; i < kCarSpheres; i++) {
        if (i < CarSphere_SeatLeft)
            sphere.radius = kSphereCorner;
        else if (i < CarSphere_RowLeft)
            sphere.radius = kSphereSeat;
        else
            sphere.radius = kSphereBody;
        sphere.radiusSq = sphere.radius * sphere.radius;
        m_body->spheres.pushBack(sphere);
    }
    setDirtyTree(true);

    m_topSpeed = 20.0f;
    m_topReverse = 6.0f;
    m_maxBrake = 2.0f;
    m_steerRate = 0.62831855f;
    m_noiseRange = 2400.0f;
}

// FS2 0x0028e850

int *CarRunOver::at(int level)
{
    if (level < 0 || level > GroundSlide_Worst)
        level = GroundSlide_Worst;
    return &damage[level];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0028e880
// callcheck: strlen, modelFindPart, strlen, modelFindPart, strlen, modelFindPart, strlen, modelFindPart, strlen, modelFindPart, strlen, modelFindPart, strlen, modelFindPart, strlen, modelFindPart, strlen, modelFindPart, strlen, modelFindPart (setPartMode, the ROM's jump table 0x309530)

void CarParts::showDamage(int flag)
{
    if (flag >= kCarDamageFlags)
        return;
    const bool hit = damaged[flag] != 0;
    const u_char dent = hit ? kPartShown : kPartHidden;
    const u_char whole = hit ? kPartHidden : kPartShown;
    ModelInstance *data = model->data;
    switch (flag) {
    case CarDamage_SideLeft:
        setPartMode(data, kBoneSeatLeft, dent);
        break;
    case CarDamage_SideRight:
        setPartMode(data, kBoneSeatRight, dent);
        break;
    case CarDamage_FrontLeft:
        setPartMode(data, kBoneDamageLeftB, whole);
        setPartMode(data, kBoneDamageLeftA, dent);
        break;
    case CarDamage_FrontRight:
        setPartMode(data, kBoneDamageRightB, whole);
        setPartMode(data, kBoneDamageRightA, dent);
        break;
    case CarDamage_FrontMid:
        setPartMode(data, kBoneDamageMidB, whole);
        setPartMode(data, kBoneDamageMidA, dent);
        break;
    case CarDamage_Blood1:
        setPartMode(data, kBoneBlood1, dent);
        break;
    case CarDamage_Blood2:
        setPartMode(data, kBoneBlood2, dent);
        break;
    }
}

// FS2 0x0028eb90
// callcheck: show, show

void CarParts::showWindows()
{
    PartSwitch *hidden = viewInside == 0 ? switches : mapSwitches;
    PartSwitch *shown = viewInside == 0 ? mapSwitches : switches;
    for (int i = 0; i < count; i++)
        hidden[i].show(kNoPart);
    for (int i = 0; i < count; i++)
        shown[i].show(windowShown[ids[i]]);
}

// FS2 0x0028ec70

void CarParts::setLightOrder(int order)
{
    if (spot != nullptr) {
        spot->m_priority = order;
        point2->m_priority = order;
    }
}

// FS2 0x0028ec90

bool CarParts::isLit() const
{
    return spot != nullptr && spot->m_switchedOn != 0;
}

// FS2 0x0028ecb0
// callcheck: setTreeScriptFlag, setTreeScriptFlag, setTreeScriptFlag, setTreeScriptFlag, setEmitting, setEmitting

void CarParts::setEffects(bool on)
{
    if (spot == nullptr)
        return;
    spot->setTreeScriptFlag(on);
    point->setTreeScriptFlag(on);
    spot2->setTreeScriptFlag(on);
    point2->setTreeScriptFlag(on);
    for (int i = 0; i < kCarEffects; i++) {
        if (effects[i] != nullptr)
            effects[i]->setEmitting(on);
    }
}

// FS2 0x0028ed60
// callcheck: setEffects, setEffects, showWindows, showDamage (isLit is inline in the ROM)

void CarParts::copyFrom(const CarParts *from)
{
    if (from == nullptr) {
        setEffects(false);
        for (int i = 0; i < kCarWindowsPerPart; i++)
            windowShown[i] = 1;
        memset(damaged, 0, sizeof(damaged));
    } else {
        setEffects(from->isLit());
        memcpy(windowShown, from->windowShown, sizeof(windowShown));
        memcpy(damaged, from->damaged, sizeof(damaged));
    }
    viewInside = 0;
    showWindows();
    for (int i = 0; i < kCarDamageFlags; i++)
        showDamage(i);
}

// Two pairs of a spot and a point on bn_top, and the lamps' effects under
// the first spot.
// FS2 0x0028ee70
// callcheck: find, operator_new, SpotLight, addToScene, erase, erase, pushBack, erase, erase, pushBack, operator_new, PointLight, addToScene, erase, erase, pushBack, erase, erase, pushBack, operator_new, SpotLight, addToScene, erase, erase, pushBack, erase, erase, pushBack, operator_new, PointLight, addToScene, erase, erase, pushBack, erase, erase, pushBack, find, spawn, erase, erase, pushBack, find, spawn, erase, erase, pushBack, setEffects (setParent is inline in the ROM)

void CarParts::buildLights(StageObject *owner)
{
    StageNodeBase *top = model->boneMap.find(kBoneTop)->value;

    spot = new SpotLight();
    spot->addToScene(kLightSpot, true);
    hangLight(spot, top, owner);
    spot->m_limitByMap = 0;
    spot->m_priority = kCarLightOrder;
    spot->m_mode = StageLight_Car;

    point = new PointLight();
    point->m_castsShadow = 0;
    point->addToScene(kLightPoint, true);
    hangLight(point, top, owner);
    point->m_castsShadow = 0;
    point->m_mode = StageLight_Car;

    spot2 = new SpotLight();
    spot2->addToScene(kLightSpot2, false);
    hangLight(spot2, top, owner);
    spot2->m_mode = StageLight_Car;

    point2 = new PointLight();
    point2->addToScene(kLightPoint2, true);
    hangLight(point2, top, owner);
    point2->m_castsShadow = 0;
    point2->m_usable = 0;
    point2->m_priority = kCarLightOrder;
    point2->m_mode = StageLight_Car;

    const auto lamp = [this](const char *name, const char *bone, const float *place) {
        StageParticle *effect = gStageScene->nodeTree->spawn(name, model->boneMap.find(bone)->value, place,
                                                             kLampTurn, 0, 0);
        if (effect != nullptr)
            effect->m_tree->setParent(spot->m_tree);
        return effect;
    };
    effects[0] = lamp(kEffectLampLeft, kBoneLampLeft, kLampPlace);
    sceVu0FVECTOR place;
    copyVector(place, kLampPlace);
    place[0] = -place[0];
    effects[1] = lamp(kEffectLampRight, kBoneLampRight, place);
    setEffects(false);
}

// FS2 0x0028f820
// callcheck: __destroy_new_array, operator_delete

CarParts::~CarParts()
{
    delete[] switches;
}

// FS2 0x0028f890
// callcheck: operator_new[], FUN_00104d90, sprintf, init, buildLights, copyFrom

CarParts::CarParts(StageObject *owner, PartSwitch *mapParts, int *partIds, int partCount, CarParts *from)
    : model(owner->m_model), mapSwitches(mapParts), ids(partIds), count(partCount),
      spot(nullptr), point(nullptr), spot2(nullptr), point2(nullptr), effects{ nullptr, nullptr }
{
    switches = new PartSwitch[count];
    for (int i = 0; i < count; i++) {
        const int number = ids[i] + 1;
        char name[0x80];
        sprintf(name, kInsideWindow, number);
        switches[i].init(model->data, name, number == 1 ? 3 : 2, kNoPart);
    }
    buildLights(owner);
    copyFrom(from);
}

// What stands in the header, and the adjustor thunks that end the file (the
// host's compiler makes its own):
//   FS2 0x00289cc0  StageCar::getKind (stagecar.h)
//   FS2 0x0028f9f0  StageCar::sideHook3, -0x18
//   FS2 0x0028fa00  StageCar::sideHook2, -0x18
//   FS2 0x0028fa10  StageCar::sideHook1, -0x18
//   FS2 0x0028fa20  StageCar::sideHook0, -0x18
//   FS2 0x0028fa30  StageCar::~StageCar, -0x40
//   FS2 0x0028fa40  StageCar::setProperty, -0x40
//   FS2 0x0028fa50  StageCar::getProperty, -0x40
//   FS2 0x0028fa60  StageCar::collisionHook3, -0x200
//   FS2 0x0028fa70  StageCar::syncBody, -0x200
//   FS2 0x0028fa80  StageCar::collisionHook0, -0x200
