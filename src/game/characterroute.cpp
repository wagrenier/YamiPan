/* ========================================================================
 *  game/characterroute.cpp -- how a brain's commands reach the character.
 *
 *  game/characterroute.h has the shape. The look 0x21efa0.., the steps of a
 *  move 0x21f610.., the moves 0x221e70.. (a turn, a car, the walk 0x223080),
 *  the actions 0x224da0.., the route 0x22a310..0x22f270 and the deleting
 *  destructors and trivial slots ..0x22fc50. The ROM's inlined comparisons
 *  of the motion id are the predicates of game/playermotion.h here.
 *
 *  The actions with a task of their own run 0x2251f0..0x229ec0, the commands
 *  that start them 0x22c0f0..0x22d810. Which class is what is read from the
 *  code: Yri, Kne, Srt and Yrt keep out of the light, the rest is
 *  open-items.md.
 * ======================================================================== */
#include "game/characterroute.h"

#include <math.h>
#include <string.h>

#include "game/characterbrain.h"
#include "game/characterground.h"
#include "game/characterinput.h"
#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/playermotion.h"
#include "game/routeplayer.h"
#include "game/stagecollision.h"
#include "game/stagedoor.h"
#include "game/stageitem.h"
#include "game/stagelight.h"
#include "game/stageobjlist.h"
#include "game/stageparticle.h"
#include "game/stagescene.h"
#include "game/sysdata.h"
#include "game/vu0math.h"
#include "game/stagecar.h"

namespace {

// Angles the tasks compare with.
constexpr float kOneDegree     = 0.017453292f;
constexpr float kFifteenDeg    = 0.2617994f;
constexpr float kTwentyDeg     = 0.34906584f;
constexpr float kThirtyDeg     = 0.5235988f;
constexpr float kFortyFiveDeg  = 0.7853982f;
constexpr float kSixtyDeg      = 1.0471976f;
constexpr float kSixtyFiveDeg  = 1.134464f;     // how far the head turns
constexpr float kEightyDeg     = 1.3962634f;
constexpr float kNinetyDeg     = 1.5707964f;
constexpr float kNinetyFiveDeg = 1.6580628f;
constexpr float kOneThirtyFiveDeg = 2.3561945f;

constexpr float kFrameSeconds  = 0.033333335f;  // a 30 Hz frame
constexpr float kMinReach      = 20.0f;         // a move ends this near without RouteMoveFlag_Exact
constexpr float kCarExtraReach = 50.0f;
constexpr float kSightAngle    = 1.0471976f;    // what the shooter and the look see in
constexpr float kFlyingHeight  = 880.0f;        // CharacterKind_NoShadow's while it moves

// The two motions RouteHoldAction waits in: isHoldingAction's.
constexpr int kMotionHoldA = 0x11;
constexpr int kMotionHoldB = 0x2a;
// The motion a roll ends in.
constexpr int kMotionRoll = 0xe7;
constexpr int kMotionQuickTurn = 0xf;
constexpr int kMotionRestFirst = 0xf4;      // what CharacterRoute::restIn counts from
constexpr int kMotionStunned = 0xdf;        // sp_numb1 goes on from it
constexpr int kMotionSpawning = 0xdb;       // sp_spawn_numb
// A target being bitten: a second bite does not come.
constexpr int kMotionBittenA = 0xa0;
constexpr int kMotionBittenB = 0xa1;

constexpr float kLookRate = 3.0f;           // the rate the tasks look at their target
constexpr float kShootFrom = 0.9f;          // of the weapon's range
constexpr float kBurstSeconds = 0.5f;       // at least
const char kEndAction[] = "NULL";           /* data 0x307118 */
const char kCower[] = "cr_3";               /* data 0x307120 */

// The numb motions, the three RouteNumbAction waits for.
inline bool isNumbMotion(int id) { return id == 0xe6 || id == 0xe1 || id == 0xdd; }
inline bool isBittenMotion(int id) { return id == kMotionBittenA || id == kMotionBittenB; }
// Resting or on the way: a special may take over.
inline bool isFreeMotion(int id) { return isHoldableMotion(id) || isMovingMotion(id); }

inline bool isHoldMotion(int id) { return id == kMotionHoldA || id == kMotionHoldB; }

// A turn on the spot in the sheet's motions.
bool isTurningMotion(int id)
{
    switch (id) {
    case 0xf: case 0xe: case 0xd: case 0xc: case 0xb:
    case 0x8f: case 0x8e: case 0x80: case 0x7f: case 0x71: case 0x70: case 0x58: case 0x57: case 0x56: case 0x55:
    case 0x26: case 0x25:
        return true;
    }
    return false;
}

inline void copyVector(float *out, const float *in)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    out[3] = in[3];
}

inline void subVector(float *out, const float *a, const float *b)
{
    for (int i = 0; i < 4; i++)
        out[i] = a[i] - b[i];
}

// The object's item sheet row is a firearm's; the ROM reads address 0 for a bad index.
bool isFirearm(const StageObject *weapon)
{
    const int index = weapon->m_itemDef;
    return gItemDefs[index < 0 || index >= kItemDefCount ? 0 : index].isFirearm();
}

inline bool isRifle(const StageObject *weapon)
{
    const int index = weapon->m_itemDef;
    return gItemDefs[index < 0 || index >= kItemDefCount ? 0 : index].isRifle();
}

inline const WeaponDef &weaponDefOf(const StageObject *weapon)
{
    const int index = weapon->m_weaponDef;
    return gWeaponDefs[index < 0 || index >= kWeaponDefCount ? 0 : index];
}

// 0 .. 1, from randNext's low 15 bits.
inline float randomUnit()
{
    return (float)(randNext() & 0x7fff) * 3.0517578e-05f;
}

// The room between two characters' bodies.
inline float gapBetween(StageCharacter *a, StageCharacter *b)
{
    return vecDistance(a->m_xform.position, b->m_xform.position)
         - (a->m_ground->nearRadius + b->m_ground->nearRadius);
}

// What lights the character, a YAM_ light not counted.
inline StageLight *shiningLight(StageCharacter *who)
{
    StageLight *light = who->m_shadowLight;
    return light == nullptr || light->m_yamLight != 0 ? nullptr : light;
}

inline const float *headOf(StageCharacter *who)
{
    return who->m_model->boneMap.find("bn_head")->value->getMatrix()[3];
}

inline bool cowerSpecial(StageCharacter *who)
{
    const bool allowed = who->isSpecialAllowed(kCower);
    if (allowed)
        who->setSpecial(true, kCower);
    return allowed;
}

// The yaw from who to the point, and what that is from where it faces.
float headingTo(const StageObject *who, const float *point)
{
    const float dx = point[0] - who->m_xform.position[0];
    const float dy = point[1] - who->m_xform.position[1];
    return addAngle(-atan2Octant(dx, dy), kMathPi);
}

float turnTo(const StageObject *who, const float *point)
{
    return addAngle(headingTo(who, point), -who->m_xform.rotation[2]);
}

// Where an object is, for who is going to it: a character in a car by the car.
void objectPlace(StageObject *object, float *out, StageCharacter *who)
{
    if (object->getKind() == StageObject_Character) {
        if (object->m_gear == nullptr)
            copyVector(out, object->m_xform.position);
        else
            static_cast<StageCar *>(object->m_gear)->getSeatPlace(out, who);
    } else {
        copyVector(out, object->getMatrix()[3]);
    }
}

// The goal is closer than reach, in a car over the ground.
bool hasReached(StageCharacter *who, const float *goal, float reach)
{
    if (!who->isMode1())
        return vecDistance(goal, who->m_xform.position) < reach;
    float center[4];
    static_cast<StageCar *>(who->m_gear)->getCenter(center);
    const float dx = goal[0] - center[0];
    const float dy = goal[1] - center[1];
    return sqrtf(dx * dx + dy * dy) < reach + kCarExtraReach;
}

inline void wantMove(StageCharacter *who, int kind)
{
    if (who->m_allows.move != 0)
        who->m_wants.moveKind = kind;
}

// Past something on the side: for edge until the goal clears it, then the
// goal. false: it is facing the goal.
bool steerPast(CharacterRoute *route, bool obstacleLeft, float edge, float goal)
{
    if (obstacleLeft ? edge < goal : goal < edge) {
        route->steer(goal);
        if (fabsf(goal) < kOneDegree)
            return false;
    } else {
        route->steer(edge);
    }
    return true;
}

inline StageObject **skipInactive(StageObject **slot)
{
    while (slot != nullptr && !(*slot)->m_active)
        slot = StageObjArray::next(slot);
    return slot;
}

inline StageObject **firstActive(StageObjGroup group)
{
    return skipInactive(gStageScene->objects->m_lists[group].begin());
}

inline StageObject **nextActive(StageObject **slot)
{
    return skipInactive(StageObjArray::next(slot));
}

// An ally of who stands within 80 of the gun's line short of the aim point.
// FS2 0x0022b340
// callcheck: isAlly, getLifeState, find (getMatrix, vecDot, firstActive)
bool allyInLine(StageObject *weapon, StageCharacter *who, const float *aim)
{
    constexpr float kNearest = 1.0f;
    constexpr float kLineWidthSq = 6400.0f;

    const float *origin = weapon->getMatrix()[3];
    sceVu0FVECTOR toAim;
    subVector(toAim, aim, origin);
    const float aimSq = vecDot(toAim, toAim);
    for (StageObject **slot = firstActive(StageObjGroup_Characters); slot != nullptr; slot = nextActive(slot)) {
        StageCharacter *other = static_cast<StageCharacter *>(*slot);
        if (!who->isAlly(other) || other == who || other->getLifeState() != 2)
            continue;
        sceVu0FVECTOR toHead;
        subVector(toHead, other->m_model->boneMap.find("bn_head")->value->getMatrix()[3], weapon->getMatrix()[3]);
        const float headSq = vecDot(toHead, toHead);
        if (headSq < aimSq && kNearest < headSq) {
            const float along = vecDot(toHead, weapon->getMatrix()[0]);
            if (0.0f < along && (headSq + 0.0f) - along * along < kLineWidthSq)
                return true;
        }
    }
    return false;
}

} // namespace

// ──────────────────────────────────────────────────────────────────────
// The look.

// FS2 0x0021f410
// callcheck: getStageSelect (stageObjRecord)
bool RouteLook::update()
{
    if (m_pitch < -kSixtyFiveDeg)
        m_pitch = -kSixtyFiveDeg;
    else if (kSixtyFiveDeg < m_pitch)
        m_pitch = kSixtyFiveDeg;

    if (stageObjRecord(m_owner->m_itemDef)->kind == CharacterKind_NoShadow) {
        // It flies on its side: the range is about its quarter turn.
        float yaw = m_yaw + kNinetyDeg;
        if (yaw < -kSixtyFiveDeg)
            yaw = -kSixtyFiveDeg;
        else if (kSixtyFiveDeg < yaw)
            yaw = kSixtyFiveDeg;
        m_yaw = yaw - kNinetyDeg;
    } else {
        if (m_yaw < -kSixtyFiveDeg)
            m_yaw = -kSixtyFiveDeg;
        else if (kSixtyFiveDeg < m_yaw)
            m_yaw = kSixtyFiveDeg;
    }

    m_owner->m_wants.headAngles[0] = m_pitch;
    m_owner->m_wants.headAngles[1] = 0.0f;
    m_owner->m_wants.headAngles[2] = m_yaw;
    m_owner->m_wants.headAngles[3] = 0.0f;
    m_owner->m_wants.headOn = 1;
    return true;
}

RouteLookPoint::RouteLookPoint(StageCharacter *owner, const float *point) : RouteLook(owner, 0.0f, 0.0f)
{
    copyVector(m_point, point);
}

// FS2 0x0021f340
// callcheck: getSwayMatrix, atan2Octant, atan2Octant, addAngle, addAngle, update
bool RouteLookPoint::update()
{
    sceVu0FMATRIX sway;
    m_owner->getSwayMatrix(sway);
    sceVu0FVECTOR delta;
    subVector(delta, m_point, sway[3]);
    m_pitch = atan2Octant(delta[2], sqrtf(delta[0] * delta[0] + delta[1] * delta[1]));
    m_yaw = -atan2Octant(delta[0], delta[1]);
    m_yaw = addAngle(m_yaw, kMathPi);
    m_yaw = addAngle(m_yaw, -m_owner->m_xform.rotation[2]);
    return RouteLook::update();
}

// The point of the object: a character's head, by its sway.
void RouteLookObject::aim()
{
    sceVu0FMATRIX sway;
    m_owner->getSwayMatrix(sway);
    if (m_object->getKind() == StageObject_Character) {
        if (stageObjRecord(m_owner->m_itemDef)->kind == CharacterKind_F) {
            copyVector(m_point, m_object->m_xform.position);
            m_point[2] += 100.0f;
        } else {
            sceVu0FMATRIX other;
            static_cast<StageCharacter *>(m_object)->getSwayMatrix(other);
            copyVector(m_point, other[3]);
        }
    } else {
        copyVector(m_point, m_object->m_xform.position);
    }
}

// FS2 0x0021f1a0
// callcheck: getSwayMatrix, getSwayMatrix, getStageSelect, getSeconds (aim; stageObjRecord)
RouteLookObject::RouteLookObject(float rate, StageCharacter *owner, StageObject *object)
    : RouteLookPoint(owner, owner->m_xform.position), m_object(object), m_rate(rate)
{
    aim();
    m_seenAt = m_owner->m_clock->getSeconds();
}

// FS2 0x0021efa0
// callcheck: canSee, getSwayMatrix, getSwayMatrix, getStageSelect, getSeconds, getSeconds, update (aim; stageObjRecord)
bool RouteLookObject::update()
{
    float distance = 0.0f;
    if (m_rate <= 0.0f || m_owner->canSee(kSightAngle, m_object, &distance)) {
        aim();
        m_seenAt = m_owner->m_clock->getSeconds();
    }
    if (m_rate <= 0.0f || m_owner->m_clock->getSeconds() <= m_seenAt + m_rate)
        return RouteLookPoint::update();
    m_owner->m_wants.headOn = 0;
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The steps of a move. Each answers false when it is over.

namespace {

// A turn to a yaw. layout: host -- EE 0x10, vtable 0x3161d0.
class RouteTurn : public RouteStep
{
public:
    RouteTurn(StageCharacter *owner, float yaw) : RouteStep(owner), m_yaw(yaw) {}
    bool update() override;                                              // FS2 0x002219d0

protected:
    float m_yaw;                // FS2 +0x08
    float m_side = 0.0f;        // FS2 +0x0c: -1 or 1, once it has chosen
};

// layout: host -- EE 0x20, vtable 0x3161c0.
class RouteTurnPoint : public RouteTurn
{
public:
    RouteTurnPoint(StageCharacter *owner, const float *point) : RouteTurn(owner, 0.0f) { copyVector(m_point, point); }
    bool update() override;                                              // FS2 0x00221960

protected:
    sceVu0FVECTOR m_point;      // FS2 +0x10
};

// layout: host -- EE 0x30, vtable 0x3161b0.
class RouteTurnObject : public RouteTurnPoint
{
public:
    RouteTurnObject(StageCharacter *owner, StageObject *object)
        : RouteTurnPoint(owner, owner->m_xform.position), m_object(object) {}
    bool update() override;                                              // FS2 0x00221880

private:
    StageObject *m_object;      // FS2 +0x20
};

// A roll along a long straight. layout: host -- EE 0x30, vtable 0x316120.
class RouteRoll : public RouteStep
{
public:
    RouteRoll(StageCharacter *owner, const float *goal);
    ~RouteRoll() override;                                               // FS2 0x0021f7b0
    bool update() override;                                              // FS2 0x0021f610

private:
    sceVu0FVECTOR m_goal;       // FS2 +0x10
    float m_until;              // FS2 +0x20
    StageParticle *m_effect = nullptr; // FS2 +0x24
    int   m_phase = 0;          // FS2 +0x28: 0 asks for the special, 1 it runs, 2 waits for its end
};

// To the elevator and through it. layout: host -- EE 0x1c, vtable 0x316130.
class RouteElevator : public RouteStep
{
public:
    RouteElevator(StageCharacter *owner, CharacterRoute *route, int floor, bool goingDown);
    ~RouteElevator() override { delete m_turn; }                         // FS2 0x0021ffc0
    bool update() override;                                              // FS2 0x0021f8b0

private:
    void useSwitch(StageObject *elevator, int which);
    void setTurn(RouteStep *turn);

    CharacterRoute *m_route;    // FS2 +0x08
    int   m_floor;              // FS2 +0x0c: 0 B1, 1 B3: the door it is near
    int   m_goingDown;          // FS2 +0x10
    float m_until;              // FS2 +0x14
    RouteStep *m_turn = nullptr; // FS2 +0x18
};

// Turns back to a door it came through and shuts it.
// layout: host -- EE 0x1c, vtable 0x316140.
class RouteDoorStep : public RouteStep
{
public:
    RouteDoorStep(StageCharacter *owner, CharacterRoute *route, StageDoor *door, bool lock);
    ~RouteDoorStep() override { delete m_turn; }                         // FS2 0x00220610
    bool update() override;                                              // FS2 0x00220050

private:
    CharacterRoute *m_route;    // FS2 +0x08
    StageDoor *m_door;          // FS2 +0x0c
    u_char m_lock;              // FS2 +0x10
    float  m_until;             // FS2 +0x14
    RouteStep *m_turn = nullptr; // FS2 +0x18
};

// A detour round a character. layout: host -- EE 0x30, vtable 0x316150.
class RouteAvoidCharacter : public RouteStep
{
public:
    RouteAvoidCharacter(StageCharacter *owner, CharacterRoute *route, StageCharacter *other,
                        const float *goal, float reach, float seconds);
    bool update() override;                                              // FS2 0x002206a0

private:
    CharacterRoute *m_route;    // FS2 +0x08
    StageCharacter *m_other;    // FS2 +0x0c
    sceVu0FVECTOR m_goal;       // FS2 +0x10
    float m_reach;              // FS2 +0x20
    float m_until;              // FS2 +0x24
};

// Along a wall. layout: host -- EE 0x30, vtable 0x316160.
class RouteAvoidWall : public RouteStep
{
public:
    RouteAvoidWall(StageCharacter *owner, CharacterRoute *route, float yaw, const float *goal, float reach);
    bool update() override;                                              // FS2 0x00220ae0

private:
    CharacterRoute *m_route;    // FS2 +0x08
    float m_yaw;                // FS2 +0x0c: of the wall
    sceVu0FVECTOR m_goal;       // FS2 +0x10
    float m_reach;              // FS2 +0x20
    float m_until;              // FS2 +0x24
};

// Past an open door's leaf. layout: host -- EE 0x30, vtable 0x316170.
class RouteAvoidDoor : public RouteStep
{
public:
    RouteAvoidDoor(StageCharacter *owner, CharacterRoute *route, StageObject *door, const float *goal);
    bool update() override;                                              // FS2 0x00220c90

private:
    CharacterRoute *m_route;    // FS2 +0x08
    StageObject *m_door;        // FS2 +0x0c
    sceVu0FVECTOR m_goal;       // FS2 +0x10
    float m_until;              // FS2 +0x20
};

// Round an open trap. layout: host -- EE 0x30, vtable 0x316180.
class RouteAvoidTrap : public RouteStep
{
public:
    RouteAvoidTrap(StageCharacter *owner, CharacterRoute *route, StageObject *trap, const float *goal);
    bool update() override;                                              // FS2 0x002210f0

private:
    CharacterRoute *m_route;    // FS2 +0x08
    StageObject *m_trap;        // FS2 +0x0c
    sceVu0FVECTOR m_goal;       // FS2 +0x10
    float m_until;              // FS2 +0x20
};

// layout: host -- EE 0x14, vtable 0x316190.
class RouteLadderStep : public RouteStep
{
public:
    RouteLadderStep(StageCharacter *owner, bool up);
    bool update() override;                                              // FS2 0x00221510

private:
    u_char m_up;                // FS2 +0x08
    float  m_until;             // FS2 +0x0c
    int    m_phase = 0;         // FS2 +0x10: 0 asks, 1 climbs
};

// To a corner of the path at a walk. layout: host -- EE 0x30, vtable 0x3161a0.
class RouteFollowNode : public RouteStep
{
public:
    RouteFollowNode(StageCharacter *owner, CharacterRoute *route, const float *point);
    bool update() override;                                              // FS2 0x00221720

private:
    CharacterRoute *m_route;    // FS2 +0x08
    sceVu0FVECTOR m_point;      // FS2 +0x10
    float m_until;              // FS2 +0x20
};

} // namespace

// FS2 0x002219d0
// callcheck: addAngle
bool RouteTurn::update()
{
    const float delta = addAngle(m_yaw, -m_owner->m_xform.rotation[2]);
    if (m_side == 0.0f) {
        if (delta < 0.0f)
            m_side = -1.0f;
        if (0.0f < delta)
            m_side = 1.0f;
    }

    const int id = m_owner->m_motion->motionId;
    if (isLadderMotion(id)) {
        if (m_owner->m_allows.squatAction != 0)
            m_owner->m_wants.squatAction = 1;
        return true;
    }
    if (kOneThirtyFiveDeg < fabsf(delta) && m_owner->m_abilities.quickTurn != 0 && !isCrouchMotion(id)) {
        if (m_owner->m_allows.quickTurn != 0)
            m_owner->m_wants.quickTurn = 1;
        return true;
    }

    if (m_side < 0.0f && (delta < -kOneDegree || kNinetyDeg < delta)) {
        if (m_owner->m_allows.turn != 0)
            m_owner->m_wants.moveKind = CharacterMove_TurnRight;
    } else if (0.0f < m_side && (kOneDegree < delta || delta < -kNinetyDeg)) {
        if (m_owner->m_allows.turn != 0)
            m_owner->m_wants.moveKind = CharacterMove_TurnLeft;
    } else if (isHoldableMotion(id)) {
        m_owner->m_wants.moveYaw = delta;
        return false;
    }
    return true;
}

// FS2 0x00221960
// callcheck: atan2Octant, addAngle, update
bool RouteTurnPoint::update()
{
    m_yaw = headingTo(m_owner, m_point);
    return RouteTurn::update();
}

// FS2 0x00221880
// callcheck: atan2Octant, addAngle, update
bool RouteTurnObject::update()
{
    copyVector(m_point, m_object->m_xform.position);
    m_yaw = headingTo(m_owner, m_point);
    return RouteTurn::update();
}

RouteRoll::RouteRoll(StageCharacter *owner, const float *goal) : RouteStep(owner)
{
    copyVector(m_goal, goal);
    m_until = m_owner->m_clock->getSeconds() + 5.0f;
}

// FS2 0x0021f7b0
// callcheck: releaseAction, remove, operator_delete (deleting)
RouteRoll::~RouteRoll()
{
    if (m_owner->m_motion->motionId == kMotionRoll)
        m_owner->m_motion->releaseAction();
    if (m_effect != nullptr)
        gStageScene->nodeTree->remove(m_effect);
}

// FS2 0x0021f610
// callcheck: getSeconds, isBoneEnding, atan2Octant, addAngle, addAngle, isSpecialAllowed, setSpecial, find, spawn
bool RouteRoll::update()
{
    static const char kSpecial[] = "sp_at1_1";

    const float now = m_owner->m_clock->getSeconds();
    if (m_phase == 2) {
        if (m_owner->m_model->pose->isBoneEnding(nullptr))
            return false;
    } else if (m_phase == 1) {
        turnTo(m_owner, m_goal);        // nothing reads it
        if (m_until < now)
            m_phase = 2;
    } else if (m_phase == 0) {
        if (m_owner->isSpecialAllowed(kSpecial)) {
            m_owner->setSpecial(true, kSpecial);
            const sceVu0FVECTOR pos = {0.0f, 0.0f, 0.0f, 0.0f};
            const sceVu0FVECTOR rot = {0.0f, 0.0f, 0.0f, 0.0f};
            StageNodeBase *pelvis = m_owner->m_model->boneMap.find("bn_pelvis")->value;
            m_effect = gStageScene->nodeTree->spawn("rolling", pelvis, pos, rot, 0, 0);
            m_phase = 1;
        } else if (m_until < now) {
            return false;
        }
    }
    return true;
}

RouteElevator::RouteElevator(StageCharacter *owner, CharacterRoute *route, int floor, bool goingDown)
    : RouteStep(owner), m_route(route), m_floor(floor), m_goingDown(goingDown)
{
    m_until = m_owner->m_clock->getSeconds() + 30.0f;
}

void RouteElevator::setTurn(RouteStep *turn)
{
    delete m_turn;
    m_turn = turn;
}

// To the switch and at it; inline twice in update, with elevator's door on the way.
void RouteElevator::useSwitch(StageObject *elevator, int which)
{
    sceVu0FVECTOR place;
    sceVu0FVECTOR direction;
    elevatorSwitchPlace(elevator, place, direction, which);
    if (vecDistance(place, m_owner->m_xform.position) < kMinReach) {
        setTurn(new RouteTurn(m_owner, atan2Octant(direction[1], direction[0]) + kNinetyDeg));
    } else if (kFifteenDeg < fabsf(turnTo(m_owner, place))) {
        setTurn(new RouteTurnPoint(m_owner, place));
    } else {
        wantMove(m_owner, CharacterMove_Walk);
    }
}

// FS2 0x0021f8b0
// callcheck: getSeconds, getZs9901, isOnElevator, elevatorOpenAtB1, elevatorOpenAtB3, isOnElevator, elevatorDoorPlace, useElevator, elevatorSwitchPlace, atan2Octant, addAngle, operator_new (useSwitch, turnTo, the ROM's two inlined copies)
bool RouteElevator::update()
{
    if (m_until < m_owner->m_clock->getSeconds())
        return false;
    StageObject *elevator = gStageScene->getZs9901();
    if (elevator == nullptr)
        return false;

    if (m_owner->isOnElevator()
        && ((m_goingDown == 0 && elevatorOpenAtB1(elevator)) || (m_goingDown == 1 && elevatorOpenAtB3(elevator))))
        return false;

    if (m_turn != nullptr) {
        if (m_turn->update())
            return true;
        setTurn(nullptr);
    }

    const float radius = m_owner->m_ground->nearRadius;
    if (!m_owner->isOnElevator()) {
        if ((m_floor == 0 && elevatorOpenAtB1(elevator)) || (m_floor == 1 && elevatorOpenAtB3(elevator))) {
            if (kFifteenDeg < fabsf(turnTo(m_owner, elevator->m_xform.position)))
                setTurn(new RouteTurnPoint(m_owner, elevator->m_xform.position));
            else
                wantMove(m_owner, CharacterMove_Walk);
        } else {
            const int which = m_floor != 0 ? 2 : 1;
            if (!m_route->useElevator(which))
                useSwitch(elevator, which);
        }
    } else {
        sceVu0FVECTOR door;
        elevatorDoorPlace(elevator, door, m_floor);
        const float distance = vecDistance(door, m_owner->m_xform.position);
        if (fabsf(turnTo(m_owner, door)) <= kNinetyDeg || radius + 20.0f <= distance) {
            if (!m_route->useElevator(m_goingDown != 0 ? 4 : 3))
                useSwitch(elevator, m_goingDown != 0 ? 3 : 4);
        } else {
            wantMove(m_owner, CharacterMove_Walk);
        }
    }
    return true;
}

RouteDoorStep::RouteDoorStep(StageCharacter *owner, CharacterRoute *route, StageDoor *door, bool lock)
    : RouteStep(owner), m_route(route), m_door(door), m_lock(lock)
{
    m_until = m_owner->m_clock->getSeconds() + 5.0f;
}

// FS2 0x00220050
// callcheck: getSeconds, isShut, canLockDoor, atan2Octant, addAngle, addAngle, operator_new (useDoor, useDoorKey inline in the ROM; turnTo)
bool RouteDoorStep::update()
{
    if (m_until < m_owner->m_clock->getSeconds())
        return false;
    if (m_door->isShut()
        && (m_lock == 0 || !m_owner->canLockDoor(m_door) || m_door->m_doorMode == StageDoorMode_Locked))
        return false;

    if (m_turn != nullptr) {
        if (m_turn->update())
            return true;
        delete m_turn;
        m_turn = nullptr;
    }

    const float distance = vecDistance(m_owner->m_xform.position, m_door->getPivot());
    const float reach = m_owner->m_ground->nearRadius + 20.0f;
    const float yaw = turnTo(m_owner, m_door->getPivot());
    if (kNinetyDeg < fabsf(yaw) && distance < reach) {
        wantMove(m_owner, CharacterMove_Walk);
        return true;
    }
    if (kFifteenDeg < fabsf(yaw)) {
        delete m_turn;
        m_turn = new RouteTurnPoint(m_owner, m_door->getPivot());
        return true;
    }

    if (distance < reach) {
        if (m_owner->m_abilities.walkBack != 0)
            wantMove(m_owner, CharacterMove_Back);
    } else {
        wantMove(m_owner, CharacterMove_Walk);
    }
    if (m_owner->m_allows.doorHand == 2)
        m_route->useDoor(false);
    if (m_lock != 0 && m_owner->m_allows.doorKey == 1)
        m_route->useDoorKey(false);
    return true;
}

RouteAvoidCharacter::RouteAvoidCharacter(StageCharacter *owner, CharacterRoute *route, StageCharacter *other,
                                         const float *goal, float reach, float seconds)
    : RouteStep(owner), m_route(route), m_other(other), m_reach(reach)
{
    copyVector(m_goal, goal);
    m_until = m_owner->m_clock->getSeconds() + seconds;
}

// FS2 0x002206a0
// callcheck: getSeconds, getLifeState, atan2Octant, addAngle, getDeltaSeconds, getTurnRate (steerPast, turnTo, steer)
bool RouteAvoidCharacter::update()
{
    if (m_until < m_owner->m_clock->getSeconds() || m_other->getLifeState() != 2)
        return false;
    if (vecDistance(m_owner->m_xform.position, m_goal) < m_reach)
        return false;

    const float goal = turnTo(m_owner, m_goal);
    const float other = turnTo(m_owner, m_other->m_xform.position);
    if (m_owner->m_abilities.strafe == 0) {
        if (!steerPast(m_route, other < 0.0f, other + (other < 0.0f ? kEightyDeg : -kEightyDeg), goal))
            return false;
        wantMove(m_owner, CharacterMove_Walk);
    } else {
        if (kThirtyDeg < fabsf(other))
            return false;
        wantMove(m_owner, other < 0.0f ? CharacterMove_StrafeLeft : CharacterMove_StrafeRight);
    }
    return true;
}

RouteAvoidWall::RouteAvoidWall(StageCharacter *owner, CharacterRoute *route, float yaw, const float *goal, float reach)
    : RouteStep(owner), m_route(route), m_yaw(yaw), m_reach(reach)
{
    copyVector(m_goal, goal);
    m_until = m_owner->m_clock->getSeconds() + (m_owner->m_abilities.strafe != 0 ? 1.0f : 0.5f);
}

// FS2 0x00220ae0
// callcheck: getSeconds, addAngle, getDeltaSeconds, getTurnRate (steer)
bool RouteAvoidWall::update()
{
    if (m_until < m_owner->m_clock->getSeconds())
        return false;
    if (vecDistance(m_owner->m_xform.position, m_goal) < m_reach)
        return false;

    const float wall = addAngle(m_yaw, -m_owner->m_xform.rotation[2]);
    if (m_owner->m_abilities.strafe == 0) {
        m_route->steer(wall < 0.0f ? wall + kNinetyDeg : wall - kNinetyDeg);
        wantMove(m_owner, CharacterMove_Walk);
    } else {
        wantMove(m_owner, wall < 0.0f ? CharacterMove_StrafeLeft : CharacterMove_StrafeRight);
    }
    return true;
}

RouteAvoidDoor::RouteAvoidDoor(StageCharacter *owner, CharacterRoute *route, StageObject *door, const float *goal)
    : RouteStep(owner), m_route(route), m_door(door)
{
    copyVector(m_goal, goal);
    m_until = m_owner->m_clock->getSeconds() + 3.0f;
}

// FS2 0x00220c90
// callcheck: getSeconds, applyMatrix, atan2Octant, addAngle, getDeltaSeconds, getTurnRate (steerPast, turnTo, steer)
bool RouteAvoidDoor::update()
{
    static const sceVu0FVECTOR kLeafEnd = {0.0f, -95.0f, 0.0f, 1.0f};   // 0x2fa830

    if (m_until < m_owner->m_clock->getSeconds())
        return false;
    if (m_door->m_doorMode != StageDoorMode_Open)
        return false;

    sceVu0FVECTOR end;
    applyMatrix(end, m_door->getWorldMatrix(), kLeafEnd);
    const float hinge = turnTo(m_owner, m_door->getWorldMatrix()[3]);
    const float goal = turnTo(m_owner, m_goal);
    const float leaf = turnTo(m_owner, end);
    if (!steerPast(m_route, hinge < 0.0f, leaf + (hinge < 0.0f ? kNinetyDeg : -kNinetyDeg), goal))
        return false;
    wantMove(m_owner, CharacterMove_Walk);
    return true;
}

RouteAvoidTrap::RouteAvoidTrap(StageCharacter *owner, CharacterRoute *route, StageObject *trap, const float *goal)
    : RouteStep(owner), m_route(route), m_trap(trap)
{
    copyVector(m_goal, goal);
    m_until = m_owner->m_clock->getSeconds() + 3.0f;
    if (1 < m_owner->m_aiLevel)
        m_route->trapSeen = 1.0f;
}

// FS2 0x002210f0
// callcheck: getSeconds, vecDistance, atan2Octant, addAngle, getDeltaSeconds, getTurnRate (steerPast, turnTo, steer)
bool RouteAvoidTrap::update()
{
    if (m_until < m_owner->m_clock->getSeconds() || m_owner->m_aiLevel == 0)
        return false;

    const float trap = turnTo(m_owner, m_trap->m_xform.position);
    const float goal = turnTo(m_owner, m_goal);
    if (!steerPast(m_route, trap < 0.0f, trap + (trap < 0.0f ? kNinetyFiveDeg : -kNinetyFiveDeg), goal))
        return false;
    wantMove(m_owner, CharacterMove_Walk);
    return true;
}

RouteLadderStep::RouteLadderStep(StageCharacter *owner, bool up) : RouteStep(owner), m_up(up)
{
    m_until = m_owner->m_clock->getSeconds() + 5.0f;
}

// FS2 0x00221510
// callcheck: getSeconds
bool RouteLadderStep::update()
{
    const int id = m_owner->m_motion->motionId;
    if (m_phase == 1) {
        if (!isLadderMotion(id))
            return false;
        wantMove(m_owner, m_up != 0 ? CharacterMove_Walk : CharacterMove_Back);
    } else if (m_phase == 0) {
        if (m_until < m_owner->m_clock->getSeconds())
            return false;
        if (isLadderMotion(id))
            m_phase = 1;
        else if (m_owner->m_allows.ladder != 0)
            m_owner->m_wants.useLadder = 1;
    }
    return true;
}

RouteFollowNode::RouteFollowNode(StageCharacter *owner, CharacterRoute *route, const float *point)
    : RouteStep(owner), m_route(route)
{
    copyVector(m_point, point);
    m_until = m_owner->m_clock->getSeconds() + 1.0f;
}

// FS2 0x00221720
// callcheck: getSeconds, atan2Octant, addAngle, addAngle, getDeltaSeconds, getTurnRate
bool RouteFollowNode::update()
{
    if (m_until < m_owner->m_clock->getSeconds())
        return false;
    const float yaw = turnTo(m_owner, m_point);
    if (kThirtyDeg < fabsf(yaw))
        return false;
    m_route->steer(yaw);
    wantMove(m_owner, CharacterMove_Walk);
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The moves.

namespace {

// A turn in the move slot. layout: host -- EE 0x10, vtable 0x3161e0.
class RouteMoveTurn : public RouteMove
{
public:
    RouteMoveTurn(StageCharacter *owner, CharacterRoute *route, RouteStep *step) : RouteMove(owner, route), m_step(step)
    {
        m_route->holdReady(0.0f);
    }
    ~RouteMoveTurn() override { delete m_step; }                         // FS2 0x00221e90
    bool update() override { return m_step->update(); }                  // FS2 0x00221e70
    bool getGoal(float *out) override { (void)out; return false; }       // FS2 0x0022f580
    StageObject *getTarget() override { return nullptr; }                // FS2 0x0022f590
    void pathsChanged() override {}                                      // FS2 0x0022f5a0

private:
    RouteStep *m_step;          // FS2 +0x0c
};

// Driving. layout: host -- EE 0x30, vtable 0x316200.
class RouteMoveCar : public RouteMove
{
public:
    RouteMoveCar(StageCharacter *owner, CharacterRoute *route, const float *point, StageObject *object, float reach);
    bool update() override;                                              // FS2 0x00222040
    bool getGoal(float *out) override;                                   // FS2 0x00221f70
    StageObject *getTarget() override { return m_object; }               // FS2 0x0022f610
    void pathsChanged() override {}                                      // FS2 0x0022f620

private:
    StageCar *m_car;            // FS2 +0x0c
    sceVu0FVECTOR m_point = {}; // FS2 +0x10
    StageObject *m_object;      // FS2 +0x20
    float m_reach;              // FS2 +0x24
};

// On foot. layout: host -- EE 0x60, vtable 0x316220.
class RouteWalk : public RouteMove
{
public:
    RouteWalk(float reach, StageCharacter *owner, CharacterRoute *route, RouteFilter *filter,
              StageObject *object, u_int flags);                         // FS2 0x00224ba0
    RouteWalk(float reach, StageCharacter *owner, CharacterRoute *route, RouteFilter *filter,
              const float *point, u_int flags);                          // FS2 0x00224ce0
    ~RouteWalk() override { delete m_step; }                             // FS2 0x00224b10
    bool update() override;                                              // FS2 0x00223080
    bool getGoal(float *out) override;                                   // FS2 0x00222fb0
    StageObject *getTarget() override { return m_object; }               // FS2 0x0022f630
    void pathsChanged() override { m_repath = 1; }                       // FS2 0x0022f640

private:
    void start();
    void setStep(RouteStep *step);
    // The stops before it walks on.
    bool updatePath(float now, const float *goal);
    bool useLedge(const sceVu0FVECTOR goal, float rise, bool *done);
    bool useDoors(const sceVu0FVECTOR goal, bool *done);
    bool useElevator(float rise);
    bool avoid(const float *goal, bool noPath);                          // FS2 0x00222e60
    bool avoidTrap(const float *goal);                                   // FS2 0x00222b80
    bool avoidDoor(StageObject *door, const float *goal);                // FS2 0x002229a0
    bool avoidWall(const float *goal, bool noPath);                      // FS2 0x00222830
    bool avoidCharacter(const float *goal, bool noPath);                 // FS2 0x00222310

    RouteFilter *m_filter;      // FS2 +0x0c
    sceVu0FVECTOR m_point = {}; // FS2 +0x10
    StageObject *m_object = nullptr; // FS2 +0x20
    float  m_reach;             // FS2 +0x24
    u_int  m_flags;             // FS2 +0x28: RouteMoveFlag
    StageDoor *m_doorBehind = nullptr; // FS2 +0x2c: the door it is going through
    int    m_targetArea = -1;   // FS2 +0x30: the target's floor attr
    sceVu0FVECTOR m_pathGoal = {}; // FS2 +0x40
    u_char m_repath = 1;        // FS2 +0x50
    RouteStep *m_step = nullptr; // FS2 +0x54
    float  m_repathAt = 0.0f;   // FS2 +0x58
};

} // namespace

RouteMoveCar::RouteMoveCar(StageCharacter *owner, CharacterRoute *route, const float *point, StageObject *object, float reach)
    : RouteMove(owner, route), m_object(object), m_reach(reach)
{
    m_car = static_cast<StageCar *>(m_owner->m_gear);
    if (point != nullptr)
        copyVector(m_point, point);
}

// FS2 0x00221f70
// callcheck: getSeatPlace (objectPlace)
bool RouteMoveCar::getGoal(float *out)
{
    if (m_object == nullptr)
        copyVector(out, m_point);
    else
        objectPlace(m_object, out, m_owner);
    return true;
}

// FS2 0x00222040
// callcheck: getCenter, atan2Octant, addAngle, addAngle, setDirection, setDirection, getDeltaSeconds, getSpeed, setDirection, setDirection (getGoal)
bool RouteMoveCar::update()
{
    constexpr float kFullBrake = 250.0f;
    constexpr u_char kPedalDown = 0xff;

    float goal[4];
    getGoal(goal);
    float center[4];
    m_car->getCenter(center);
    const float dx = goal[0] - center[0];
    const float dy = goal[1] - center[1];
    const float yaw = addAngle(addAngle(-atan2Octant(dx, dy), kMathPi), -m_car->m_xform.rotation[2]);
    const float rate = m_car->m_steerRate;
    const float distance = sqrtf(dx * dx + dy * dy);

    float steer = yaw / (rate * 2.0f);
    if (steer < -1.0f)
        steer = 1.0f;
    else
        steer = -(steer <= 1.0f ? steer : 1.0f);
    const float brake = fabsf(steer) * kFullBrake;

    if (kNinetyDeg < fabsf(yaw)) {
        m_car->m_steer = -steer;
        m_car->m_throttle = kPedalDown;
        m_car->m_brake = 0;
        m_car->setDirection(m_route->moveMode == 3 ? 1 : -1);
        return true;
    }

    const float frame = rate * m_owner->m_clock->getDeltaSeconds();
    float allowed = frame;
    const float speed = m_car->getSpeed();
    if (0.0f < speed)
        allowed = (frame * distance) / speed;
    if (allowed < frame)
        allowed = frame;
    m_car->m_steer = steer;
    m_car->m_throttle = kPedalDown;
    m_car->m_brake = allowed < fabsf(yaw) ? (u_char)(int)brake : 0;
    if (m_route->moveMode == 3) {
        m_car->setDirection(-1);
    } else {
        m_car->setDirection(1);
        if (distance < m_reach + kCarExtraReach)
            return false;
    }
    return true;
}

// The ctors' common end.
void RouteWalk::start()
{
    m_repathAt = m_owner->m_clock->getSeconds() + 10.0f;
    m_filter->clearPath();
    m_route->holdReady(0.0f);
}

// FS2 0x00224ba0
// callcheck: getSeatPlace, getSeconds, clearPath, getSeconds (objectPlace, start)
RouteWalk::RouteWalk(float reach, StageCharacter *owner, CharacterRoute *route, RouteFilter *filter,
                     StageObject *object, u_int flags)
    : RouteMove(owner, route), m_filter(filter), m_object(object), m_reach(reach), m_flags(flags)
{
    objectPlace(m_object, m_pathGoal, m_owner);
    start();
}

// FS2 0x00224ce0
// callcheck: getSeconds, clearPath, getSeconds (start)
RouteWalk::RouteWalk(float reach, StageCharacter *owner, CharacterRoute *route, RouteFilter *filter,
                     const float *point, u_int flags)
    : RouteMove(owner, route), m_filter(filter), m_reach(reach), m_flags(flags)
{
    copyVector(m_point, point);
    copyVector(m_pathGoal, point);
    start();
}

void RouteWalk::setStep(RouteStep *step)
{
    delete m_step;
    m_step = step;
}

// FS2 0x00222fb0
// callcheck: getSeatPlace (objectPlace)
bool RouteWalk::getGoal(float *out)
{
    if (m_object == nullptr)
        copyVector(out, m_point);
    else
        objectPlace(m_object, out, m_owner);
    return true;
}

// FS2 0x00222b80
// callcheck: atan2Octant, addAngle, addAngle, operator_new, getSeconds (firstActive; turnTo)
bool RouteWalk::avoidTrap(const float *goal)
{
    if (m_owner->m_aiLevel == 0)
        return false;

    constexpr float kTrapRange = 80.0f;
    float nearest = 100.0f;
    StageObject *trap = nullptr;
    for (StageObject **slot = firstActive(StageObjGroup_3); slot != nullptr; slot = nextActive(slot)) {
        StageObject *object = *slot;
        if (object->getKind() != StageObject_TrapItem || static_cast<StageItem *>(object)->m_inUse == 0)
            continue;
        const float distance = vecDistance(m_owner->m_xform.position, object->m_xform.position);
        if (kTrapRange < distance)
            continue;
        if (fabsf(turnTo(m_owner, object->m_xform.position)) <= kSixtyDeg
            && kTrapRange <= vecDistance(object->m_xform.position, goal) && distance < nearest) {
            nearest = distance;
            trap = object;
        }
    }
    if (trap == nullptr)
        return false;
    setStep(new RouteAvoidTrap(m_owner, m_route, trap, goal));
    return true;
}

// FS2 0x002229a0
// callcheck: getStageSelect, operator_new, getSeconds (stageObjRecord)
bool RouteWalk::avoidDoor(StageObject *door, const float *goal)
{
    if (door == nullptr)
        return false;
    if (stageObjRecord(m_owner->m_itemDef)->kind == CharacterKind_Srt)
        return false;
    if (door->m_doorType != StageDoorType_R || door->m_doorMode != StageDoorMode_Open)
        return false;
    if (m_owner->m_ground->nearRadius + 50.0f < vecDistance(m_owner->m_xform.position, door->m_xform.position))
        return false;
    setStep(new RouteAvoidDoor(m_owner, m_route, door, goal));
    return true;
}

// FS2 0x00222830
// callcheck: addAngle, operator_new, getSeconds
bool RouteWalk::avoidWall(const float *goal, bool noPath)
{
    CharacterGround *ground = m_owner->m_ground;
    if (ground->wallCount < 1 || kSixtyDeg <= fabsf(ground->wallAngles[0]))
        return false;
    const float yaw = addAngle(ground->wallAngles[0], m_owner->m_xform.rotation[2]);
    setStep(new RouteAvoidWall(m_owner, m_route, yaw, goal, noPath ? m_reach : 0.0f));
    return true;
}

// FS2 0x00222310
// callcheck: getStageSelect, getLifeState, getStageSelect, getStageSelect, atan2Octant, addAngle, addAngle, melee, operator_new, getSeconds, operator_new, getSeconds (firstActive; turnTo; stageObjRecord)
bool RouteWalk::avoidCharacter(const float *goal, bool noPath)
{
    if (stageObjRecord(m_owner->m_itemDef)->kind == CharacterKind_Srt)
        return false;

    for (StageObject **slot = firstActive(StageObjGroup_Characters); slot != nullptr; slot = nextActive(slot)) {
        StageCharacter *other = static_cast<StageCharacter *>(*slot);
        if (other == m_owner || other->getLifeState() != 2 || other == m_object)
            continue;
        const int kind = stageObjRecord(other->m_itemDef)->kind;
        if (kind == CharacterKind_Yrt || kind == CharacterKind_Srt)
            continue;
        const float sum = m_owner->m_ground->nearRadius + other->m_ground->nearRadius;
        if (sum < vecDistance(m_owner->m_xform.position, other->m_xform.position))
            continue;
        if (kSixtyDeg < fabsf(turnTo(m_owner, other->m_xform.position)))
            continue;
        const float reach = noPath ? m_reach : 0.0f;
        if ((m_flags & RouteMoveFlag_HitInWay) == 0) {
            if (other->m_ground->nearRadius <= vecDistance(other->m_xform.position, goal)) {
                setStep(new RouteAvoidCharacter(m_owner, m_route, other, goal, reach, 3.0f));
                return true;
            }
        } else if (m_route->melee(other, false)) {
            setStep(new RouteAvoidCharacter(m_owner, m_route, other, goal, reach, 6.0f));
            return true;
        }
    }
    return false;
}

// FS2 0x00222e60
// callcheck: avoidTrap, findObjectOnSegment, raycast, findDoorOfFace, avoidDoor, avoidWall, avoidCharacter
bool RouteWalk::avoid(const float *goal, bool noPath)
{
    constexpr float kBodyHeight = 50.0f;

    if (avoidTrap(goal))
        return true;

    sceVu0FVECTOR from;
    sceVu0FVECTOR to;
    copyVector(from, m_owner->m_xform.position);
    copyVector(to, goal);
    from[2] += kBodyHeight;
    to[2] += kBodyHeight;
    StageObject *door = gStageScene->findObjectOnSegment(from, to, 0, StageObject_Door, 1, 0, nullptr);
    if (door == nullptr) {
        sceVu0FVECTOR hit;
        const StageCollisionFace *face = gStageScene->raycast(hit, from, to, 0, 0x4000000, 0, nullptr);
        if (face != nullptr)
            door = gStageScene->findDoorOfFace(face);
    }
    if (avoidDoor(door, goal))
        return true;
    if (door == nullptr && avoidWall(goal, noPath))
        return true;
    return avoidCharacter(goal, noPath);
}

// The path: a target on another floor area, or a stale one, finds it again.
// false: there is no way, the walk ends.
// inline in update
bool RouteWalk::updatePath(float now, const float *goal)
{
    constexpr float kTargetMoved = 100.0f;
    constexpr float kRepathSeconds = 10.0f;

    if (m_object != nullptr && m_object->getKind() == StageObject_Character
        && 0 <= m_owner->m_ground->floorAttr) {
        const int area = m_object->m_ground->floorAttr;
        if (0 <= area) {
            const bool listed = gStageScene->routePlayer->isListedArea((short)area);
            const float moved = vecDistance(m_pathGoal, goal);
            if (m_targetArea != area || (!listed && kTargetMoved < moved)) {
                if ((m_flags & RouteMoveFlag_KeepPath) == 0
                    || 0.0f < m_filter->pathLength(m_owner->m_xform.position, goal)) {
                    m_targetArea = area;
                    copyVector(m_pathGoal, goal);
                    m_repath = 1;
                } else {
                    copyVector(m_point, m_pathGoal);
                    m_object = nullptr;
                }
            }
        }
    }
    if (m_repathAt < now)
        m_repath = 1;
    if (m_repath != 0) {
        if (m_owner->m_ground->floorAttr < 0)
            return true;
        if (m_filter->findPath(m_pathGoal) <= 0.0f)
            return false;
        m_repath = 0;
        m_repathAt = now + kRepathSeconds;
    }
    m_filter->advance();
    return true;
}

// The ledge ahead, a jump or a ladder. true: it stands in for the frame's
// walk, *done the move's answer. inline in update
bool RouteWalk::useLedge(const sceVu0FVECTOR goal, float rise, bool *done)
{
    // What a walker with each step level drops: 段差Lv 0..3, 0x2fa820.
    static const float kDrops[4] = {-5.0f, -75.0f, -105.0f, -150.0f};
    constexpr float kLadderRise = 45.0f;

    *done = false;
    if (m_owner->m_allows.level == 1) {
        if ((m_flags & RouteMoveFlag_NoClimb) == 0) {
            m_owner->m_wants.useLevel = 1;
            return true;
        }
    } else if (m_owner->m_allows.level == 2 && (m_flags & RouteMoveFlag_NoDrop) == 0) {
        if ((m_flags & RouteMoveFlag_DropLimit) != 0) {
            const int level = stageObjRecord(m_owner->m_itemDef)->stepLevel;
            if (level < 4 && m_owner->m_motion->walkLink->edgeDrop < kDrops[level]) {
                m_owner->m_wants.moveKind = CharacterMove_None;
                copyVector(m_route->stoppedAt, goal);
                *done = true;
                return true;
            }
        }
        m_owner->m_wants.useLevel = 1;
        return true;
    }

    if ((m_flags & RouteMoveFlag_NoLadder) == 0) {
        if (m_owner->m_allows.ladder == 1) {
            if (kLadderRise < rise) {
                m_owner->m_wants.useLadder = 1;
                setStep(new RouteLadderStep(m_owner, true));
                return true;
            }
        } else if (m_owner->m_allows.ladder == 2 && rise < -kLadderRise) {
            m_owner->m_wants.useLadder = 1;
            setStep(new RouteLadderStep(m_owner, false));
            return true;
        }
    }
    return false;
}

// The door in reach, and the one it came through. inline in update
bool RouteWalk::useDoors(const sceVu0FVECTOR goal, bool *done)
{
    constexpr float kBehindNear = 10.0f;
    constexpr float kBehindFar = 30.0f;

    *done = false;
    StageDoor *door = m_owner->m_door;
    if (door != nullptr && (m_flags & RouteMoveFlag_IgnoreDoor) == 0
        && fabsf(turnTo(m_owner, door->getPivot())) < kFortyFiveDeg) {
        if ((m_flags & RouteMoveFlag_StopAtDoor) != 0 && door->m_doorMode != StageDoorMode_Open) {
            m_owner->m_wants.moveKind = CharacterMove_None;
            copyVector(m_route->stoppedAt, goal);
            *done = true;
            return true;
        }
        if (m_owner->m_allows.doorHand == 1) {
            m_owner->m_wants.useDoor = 1;
            return true;
        }
        if (m_owner->m_allows.doorKey == 2 || m_owner->m_allows.doorKey == 3) {
            m_owner->m_wants.useDoorKey = 1;
            return true;
        }
    }

    if ((m_flags & RouteMoveFlag_ShutDoors) != 0) {
        if (m_owner->m_allows.doorHand == 2) {
            m_doorBehind = door;
        } else if (m_doorBehind != nullptr) {
            const float distance = vecDistance(m_doorBehind->getPivot(), m_owner->m_xform.position);
            if (kBehindNear < distance && distance < kBehindFar
                && kNinetyDeg < fabsf(turnTo(m_owner, m_doorBehind->getPivot()))) {
                setStep(new RouteDoorStep(m_owner, m_route, m_doorBehind, (m_flags & RouteMoveFlag_LockDoors) != 0));
                return true;
            }
        }
    }
    return false;
}

// The elevator, when the way goes through it. inline in update
bool RouteWalk::useElevator(float rise)
{
    constexpr float kFloorRise = 30.0f;

    StageObject *elevator = gStageScene->getZs9901();
    if (elevator == nullptr)
        return false;
    if (stageObjRecord(m_owner->m_itemDef)->kind == CharacterKind_Srt || (m_flags & RouteMoveFlag_StopAtDoor) != 0)
        return false;

    if (!m_owner->isOnElevator()) {
        const float reach = m_owner->m_ground->nearRadius + 20.0f;
        sceVu0FVECTOR door;
        elevatorDoorPlace(elevator, door, 0);
        if (vecDistance(door, m_owner->m_xform.position) < reach
            && fabsf(turnTo(m_owner, door)) < kThirtyDeg && !elevatorOpenAtB1(elevator)) {
            setStep(new RouteElevator(m_owner, m_route, 0, rise < -kFloorRise));
            return true;
        }
        elevatorDoorPlace(elevator, door, 1);
        if (vecDistance(door, m_owner->m_xform.position) < reach
            && fabsf(turnTo(m_owner, door)) < kThirtyDeg && !elevatorOpenAtB3(elevator)) {
            setStep(new RouteElevator(m_owner, m_route, 1, rise <= kFloorRise));
            return true;
        }
    } else if (rise < -kFloorRise && !elevatorOpenAtB3(elevator)) {
        setStep(new RouteElevator(m_owner, m_route, 0, true));
        return true;
    } else if (kFloorRise < rise && !elevatorOpenAtB1(elevator)) {
        setStep(new RouteElevator(m_owner, m_route, 1, false));
        return true;
    }
    return false;
}

// One frame: what stands in the way first (a crouch, a step), then a turn to
// the path's next node, the wants of the walk, and what the place asks for:
// a ledge, a ladder, a door, the elevator, a detour.
// FS2 0x00223080
// callcheck: getSeconds, vecDistance, isListedArea, pathLength, findPath, advance, operator_new, getSeconds, junctionNode, junctionNode, operator_new, getSeconds, currentNode, vecDot, clearPath, atan2Octant, addAngle, addAngle, addAngle, startWalkTo, getDeltaSeconds, getTurnRate, operator_new, getSeconds, getStageSelect, constructAt, elevatorDoorPlace, elevatorOpenAtB1, elevatorOpenAtB3, getZs9901, isOnElevator (the rest in updatePath, useLedge, useDoors, useElevator, avoid; constructAt wraps a short)
bool RouteWalk::update()
{
    constexpr float kCutCornerNear = 30.0f;
    constexpr float kCutCornerCos2 = 0.7569f;   // 0.87 squared: about 30 degrees
    constexpr float kExactStop = 20.0f;
    constexpr float kExactRate = 0.35f;
    constexpr float kRollLength = 400.0f;
    constexpr float kSharpLength = 300.0f;
    constexpr float kJumpDrop = -45.0f;

    float now = m_owner->m_clock->getSeconds();
    PlayerMotion *motion = m_owner->m_motion;
    if (isCrouchMotion(motion->motionId) && m_owner->m_abilities.moveCrouched == 0) {
        m_route->action22abf0();
        return true;
    }
    if (m_owner->m_wants.ready != 0 && m_owner->m_abilities.gunMove == 0)
        return true;

    sceVu0FVECTOR goal;
    getGoal(goal);
    const float toGoal = vecDistance(m_owner->m_xform.position, goal);
    if ((m_flags & RouteMoveFlag_Straight) == 0 && !updatePath(now, goal))
        return false;

    bool stepEnded = false;
    if (m_step != nullptr) {
        if (m_step->update())
            return true;
        setStep(nullptr);
        stepEnded = true;
    }

    if (isLadderMotion(motion->motionId)) {
        setStep(new RouteLadderStep(m_owner, false));
        return true;
    }
    if (m_filter->junctionNode() != nullptr && m_route->moveMode == 1) {
        setStep(new RouteFollowNode(m_owner, m_route, m_filter->junctionNode()->position));
        return true;
    }

    sceVu0FVECTOR delta;
    subVector(delta, goal, m_owner->m_xform.position);
    RouteNode *node = m_filter->currentNode();
    if (node != nullptr) {
        sceVu0FVECTOR place = {node->position[0], node->position[1], node->position[2], 1.0f};
        const float nodeToGoal = vecDistance(place, goal);
        sceVu0FVECTOR toNode;
        subVector(toNode, place, m_owner->m_xform.position);
        const float nodeSq = vecDot(toNode, toNode);
        const float goalSq = vecDot(delta, delta);
        const float dot = vecDot(delta, toNode);
        if (kCutCornerNear <= nodeToGoal || dot <= 0.0f || dot * dot <= goalSq * nodeSq * kCutCornerCos2) {
            copyVector(goal, place);
            subVector(delta, place, m_owner->m_xform.position);
        } else {
            m_filter->clearPath();
        }
    }

    float heading = headingTo(m_owner, goal);
    if (m_route->moveMode == 3)
        heading = addAngle(heading, kMathPi);
    const float yaw = addAngle(heading, -m_owner->m_xform.rotation[2]);
    const float length = vecDistance(m_owner->m_xform.position, goal);

    if ((m_flags & RouteMoveFlag_Exact) != 0 && toGoal < kExactStop) {
        if (!isHoldableMotion(motion->motionId))
            return true;
        motion->startWalkTo(kExactRate, goal, nullptr, true, true);
        return false;
    }

    const bool sharpNode = node != nullptr && (node->flags & RouteNodeFlag_Open) == 0;
    const float turnLimit = sharpNode ? kSixtyDeg : kEightyDeg;
    if ((turnLimit <= fabsf(yaw) || isTurningMotion(motion->motionId)) && !stepEnded) {
        setStep(new RouteTurn(m_owner, heading));
        return true;
    }

    m_route->steer(yaw);
    if (fabsf(yaw) < kOneDegree && (m_flags & RouteMoveFlag_Roll) != 0 && kRollLength < length)
        setStep(new RouteRoll(m_owner, goal));

    int kind;
    if (m_route->moveMode == 3)
        kind = CharacterMove_Back;
    else if ((sharpNode && length < kSharpLength && kThirtyDeg <= fabsf(yaw)) || m_owner->m_winded != 0)
        kind = CharacterMove_Walk;
    else if (m_route->moveMode == 2)
        kind = CharacterMove_Run;
    else
        kind = CharacterMove_Walk;
    wantMove(m_owner, kind);

    if (toGoal < m_reach)
        return false;
    if (kThirtyDeg < fabsf(yaw))
        return true;

    const float rise = delta[2];
    if (m_owner->m_allows.jump != 0 && kJumpDrop <= rise) {
        m_owner->m_wants.useJump = 1;
        return true;
    }

    bool done = false;
    if (useLedge(goal, rise, &done))
        return !done;
    if (useDoors(goal, &done))
        return !done;
    if (useElevator(rise))
        return true;
    avoid(goal, node == nullptr);
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The actions.

namespace {

// Hands on something: health falls. layout: host -- EE 0x18, vtable 0x316240.
class RouteHoldAction : public RouteAction
{
public:
    explicit RouteHoldAction(StageCharacter *owner, CharacterRoute *route);
    ~RouteHoldAction() override;                                         // FS2 0x00224f30
    int priority() override { return 2; }                                // FS2 0x0022f650
    bool update() override;                                              // FS2 0x00224da0

private:
    float m_stopAt;             // FS2 +0x0c
    float m_health;             // FS2 +0x10
    // FS2 +0x14: the ROM keeps the motion in a block of its own.
    PlayerMotion *m_motion;
};

// layout: host -- EE 0xc, vtable 0x316440.
class RouteBiteAction : public RouteAction
{
public:
    explicit RouteBiteAction(StageCharacter *owner);
    int priority() override { return 1; }                                // FS2 0x0022fb60
    bool update() override;                                              // FS2 0x00229f60

private:
    float m_at;                 // FS2 +0x08
};

// Shots until the time, on a target or straight ahead.
// layout: host -- EE 0x1c, vtable 0x316460.
class RouteShootAction : public RouteAction
{
public:
    RouteShootAction(float seconds, StageCharacter *owner, CharacterRoute *route, StageObject *target);
    int priority() override { return 1; }                                // FS2 0x0022fbd0
    bool update() override;                                              // FS2 0x00229fe0

private:
    CharacterRoute *m_route;    // FS2 +0x08
    StageObject *m_target;      // FS2 +0x0c
    float m_until;              // FS2 +0x10
    float m_interval;           // FS2 +0x14
    float m_nextShot;           // FS2 +0x18
};

// layout: host -- EE 0xc, vtable 0x316480.
class RouteSwingAction : public RouteAction
{
public:
    RouteSwingAction(StageCharacter *owner, CharacterRoute *route) : RouteAction(owner), m_route(route) {}
    int priority() override { return 1; }                                // FS2 0x0022fc40
    bool update() override;                                              // FS2 0x0022a310

private:
    CharacterRoute *m_route;    // FS2 +0x08
};

// A flyer numb for a while: sp_numb0 down, sp_numb1 or sp_spawn_numb when it
// is in the motion before.
// layout: host -- EE 0x18, vtable 0x316260.
class RouteNumbAction : public RouteAction
{
public:
    RouteNumbAction(float seconds, StageCharacter *owner, CharacterRoute *route); // FS2 0x002251f0
    ~RouteNumbAction() override;                                         // FS2 0x00225130
    int priority() override { return 2; }                                // FS2 0x0022f660
    bool update() override;                                              // FS2 0x00225030

private:
    float m_until;              // FS2 +0x0c
    // FS2 +0x10: the ROM keeps the motion in a block of its own.
    PlayerMotion *m_motion;
    bool m_numb = false;        // FS2 +0x14: the numb motion has been reached, or the time is out
};

// While a voice cue plays. layout: host -- EE 0x18, vtable 0x316280.
class RouteVoiceAction : public RouteAction
{
public:
    RouteVoiceAction(StageCharacter *owner, const char *name);
    int priority() override { return 0; }                                // FS2 0x0022f6d0
    bool update() override;                                              // FS2 0x002257b0

private:
    char m_name[0x10];          // FS2 +0x08
};

// layout: host -- EE 0x2c, vtable 0x3162a0.
class RoutePlayAction : public RouteAction
{
public:
    RoutePlayAction(StageCharacter *owner, CharacterRoute *route, const char *name,
                    StageObject *from, StageObject *to);                 // FS2 0x00225fc0
    int priority() override { return 0; }                                // FS2 0x0022f740
    bool update() override;                                              // FS2 0x002257c0

private:
    enum State
    {
        State_Place = 0,        // to from, facing to
        State_Start = 1,        // the pose walks there; the special goes when it may
        State_Play  = 2,
        State_End   = 3,        // only until it rests
    };

    CharacterRoute *m_route;    // FS2 +0x08
    char m_name[0x10];          // FS2 +0x0c
    StageObject *m_from = nullptr; // FS2 +0x1c
    StageObject *m_to = nullptr;   // FS2 +0x20
    int m_motionId;             // FS2 +0x24: the special's; -1 none
    int m_state;                // FS2 +0x28
};

// layout: host -- EE 0xc, vtable 0x3162c0.
class RouteQuickTurnAction : public RouteAction
{
public:
    explicit RouteQuickTurnAction(StageCharacter *owner) : RouteAction(owner) {}
    int priority() override { return 0; }                                // FS2 0x0022f7b0
    bool update() override;                                              // FS2 0x002260f0

private:
    int m_state = 0;            // FS2 +0x08: 1 once it is asked for
};

// What flees: ends with its move or after a time. vtable 0x316340; its last
// two slots are empty there. layout: host -- EE 0x14.
class RouteFleeAction : public RouteAction
{
public:
    RouteFleeAction(StageCharacter *owner, CharacterRoute *route);
    ~RouteFleeAction() override;                                         // FS2 0x002272f0
    int priority() override { return 0; }                                // FS2 0x0022f890
    bool update() override;                                              // FS2 0x00227170
    // What it flees still stands.
    virtual bool threatStands() = 0;
    // No way to go: false when it cannot do that either.
    virtual bool cower() = 0;

protected:
    CharacterRoute *m_route;    // FS2 +0x08
    float m_until;              // FS2 +0x0c
    int m_state = 0;            // FS2 +0x10: 0 fleeing, 1 cowering, 2 a last second of it
};

// Out of a light, turning 95 degrees off it. layout: host -- EE 0x18, vtable 0x316300.
class RouteFleeLightAction : public RouteFleeAction
{
public:
    RouteFleeLightAction(StageCharacter *owner, CharacterRoute *route, StageLight *light);
    bool update() override;                                              // FS2 0x00226da0
    bool threatStands() override;                                        // FS2 0x00226fd0
    bool cower() override { return true; }                               // FS2 0x0022f880

private:
    StageLight *m_light;        // FS2 +0x14
};

// layout: host -- EE 0x14, vtable 0x316320.
class RouteFleeFoeAction : public RouteFleeAction
{
public:
    RouteFleeFoeAction(StageCharacter *owner, CharacterRoute *route, StageObject *foe); // FS2 0x00227050
    bool threatStands() override { return true; }                        // FS2 0x0022f950
    bool cower() override;                                               // FS2 0x00226ff0
};

// Walks backwards from a point, facing it, until a wall. vtable 0x3163a0, its
// last two slots empty. layout: host -- EE 0x10.
class RouteBackAction : public RouteAction
{
public:
    RouteBackAction(StageCharacter *owner, CharacterRoute *route);
    ~RouteBackAction() override;                                         // FS2 0x00227790
    int priority() override { return 0; }                                // FS2 0x0022fa10
    bool update() override;                                              // FS2 0x00227510
    // Where what it backs from is; false: gone.
    virtual bool getThreat(float *out) = 0;
    virtual bool cower() = 0;

protected:
    CharacterRoute *m_route;    // FS2 +0x08
    int m_state = 0;            // FS2 +0x0c: 1 once it cowers
};

// layout: host -- EE 0x10, vtable 0x316360.
class RouteBackFromLightAction : public RouteBackAction
{
public:
    RouteBackFromLightAction(StageCharacter *owner, CharacterRoute *route);
    bool getThreat(float *out) override;                                 // FS2 0x00227390
    bool cower() override { return true; }                               // FS2 0x0022fa00
};

// layout: host -- EE 0x14, vtable 0x316380.
class RouteBackFromFoeAction : public RouteBackAction
{
public:
    RouteBackFromFoeAction(StageCharacter *owner, CharacterRoute *route, StageObject *foe); // FS2 0x00227470
    bool getThreat(float *out) override;                                 // FS2 0x00227450
    bool cower() override;                                               // FS2 0x002273f0

private:
    StageObject *m_foe;         // FS2 +0x10
};

// layout: host -- EE 0x20, vtable 0x3162e0.
class RouteFollowAction : public RouteAction
{
public:
    RouteFollowAction(StageCharacter *owner, CharacterRoute *route, StageCharacter *target); // FS2 0x00226d40
    ~RouteFollowAction() override;                                       // FS2 0x00226c60
    int priority() override { return 0; }                                // FS2 0x0022f7c0
    bool update() override;                                              // FS2 0x00226270

private:
    enum State
    {
        State_Stand  = 0,
        State_Walk   = 1,
        State_Close  = 2,       // too near: it backs off
        State_Step   = 3,       // walks off it
        State_Watch  = 4,       // turned to someone coming at them
    };

    CharacterRoute *m_route;    // FS2 +0x08
    StageCharacter *m_target;   // FS2 +0x0c
    float m_inLight = 0.0f;     // FS2 +0x10: seconds a Yri or Kne has stood in the light
    float m_watchUntil = 0.0f;  // FS2 +0x14
    int m_state = State_Stand;  // FS2 +0x18
    RouteAction *m_flee = nullptr; // FS2 +0x1c: it comes first
};

// A flyer's run at the target: past it, away, and back. layout: host -- EE 0x1c, vtable 0x3163c0.
class RouteSwoopAction : public RouteAction
{
public:
    RouteSwoopAction(StageCharacter *owner, CharacterRoute *route, StageObject *target, float range)
        : RouteAction(owner), m_route(route), m_target(target), m_range(range) {}
    ~RouteSwoopAction() override;                                        // FS2 0x00227ac0
    int priority() override { return 0; }                                // FS2 0x0022fac0
    bool update() override;                                              // FS2 0x00227820

private:
    enum State
    {
        State_Start  = 0,
        State_Charge = 1,
        State_Away   = 2,
    };

    CharacterRoute *m_route;    // FS2 +0x08
    StageObject *m_target;      // FS2 +0x0c
    float m_range;              // FS2 +0x10
    int m_state = State_Start;  // FS2 +0x14
    RouteStep *m_turn = nullptr; // FS2 +0x18
};

// Holds the distance from the target, in sight of it. layout: host -- EE 0x18, vtable 0x3163e0.
class RouteStalkAction : public RouteAction
{
public:
    RouteStalkAction(float range, StageCharacter *owner, CharacterRoute *route, StageObject *target); // FS2 0x00227ed0
    ~RouteStalkAction() override;                                        // FS2 0x00227e10
    int priority() override { return 0; }                                // FS2 0x0022fad0
    bool update() override;                                              // FS2 0x00227b80

private:
    enum State
    {
        State_Hold  = 0,
        State_Close = 1,
        State_Leave = 2,
    };

    CharacterRoute *m_route;    // FS2 +0x08
    StageObject *m_target;      // FS2 +0x0c
    float m_range;              // FS2 +0x10
    int m_state = State_Hold;   // FS2 +0x14
};

// After the target, and at it. layout: host -- EE 0x20, vtable 0x316400.
class RouteHuntAction : public RouteAction
{
public:
    RouteHuntAction(StageCharacter *owner, CharacterRoute *route, StageObject *target, bool fights); // FS2 0x002299b0
    ~RouteHuntAction() override;                                         // FS2 0x002298d0
    int priority() override { return 0; }                                // FS2 0x0022fae0
    bool update() override;                                              // FS2 0x00228670

private:
    bool runFrom();
    bool tryBite();
    void goAfter(StageObject *weapon, bool seen);

    CharacterRoute *m_route;    // FS2 +0x08
    StageCharacter *m_target;   // FS2 +0x0c
    bool m_fights;              // FS2 +0x10: without it, it only comes after the target
    bool m_wantsBite = false;   // FS2 +0x11
    float m_nextThought;        // FS2 +0x14: when it may set its mind on a bite
    RouteAction *m_task = nullptr; // FS2 +0x18: a bite, a burst, a flight from the light
    float m_biteUntil = 0.0f;   // FS2 +0x1c
};

// layout: host -- EE 0x30, vtable 0x316420.
class RouteWanderAction : public RouteAction
{
public:
    RouteWanderAction(float distance, StageCharacter *owner, CharacterRoute *route, const float *point);
    ~RouteWanderAction() override;                                       // FS2 0x00229ec0
    int priority() override { return 0; }                                // FS2 0x0022faf0
    bool update() override;                                              // FS2 0x00229c20

private:
    enum State
    {
        State_Going = 0,        // a stroll, ended at the time
        State_Pause = 1,        // until the next one
    };

    CharacterRoute *m_route;    // FS2 +0x08
    sceVu0FVECTOR m_point;      // FS2 +0x10
    float m_distance;           // FS2 +0x20
    float m_until;              // FS2 +0x24
    int m_state = State_Going;  // FS2 +0x28
};

bool findWayOut(float *out, StageCharacter *who, const float *threat, const float *last);

} // namespace

RouteHoldAction::RouteHoldAction(StageCharacter *owner, CharacterRoute *route) : RouteAction(owner)
{
    constexpr float kHealthCost = 750.0f;

    m_health = m_owner->m_health;
    m_stopAt = m_health - kHealthCost;
    if (m_stopAt < 0.0f)
        m_stopAt = 0.0f;
    m_motion = m_owner->m_motion;
    route->cancelMove();
}

// FS2 0x00224f30
// callcheck: releaseAction, operator_delete, operator_delete (deleting; the block)
RouteHoldAction::~RouteHoldAction()
{
    if (isHoldMotion(m_motion->motionId))
        m_owner->m_motion->releaseAction();
}

// FS2 0x00224da0
// callcheck: getDeltaSeconds, getStageSelect (stageObjRecord)
bool RouteHoldAction::update()
{
    m_health -= (m_owner->m_clock->getDeltaSeconds() / kFrameSeconds) * 5.0f;
    const float most = (float)stageObjRecord(m_owner->m_itemDef)->health;
    float health = 0.0f < m_health ? m_health : 0.0f;
    if (most < health)
        health = most;
    m_owner->m_health = health;
    return m_stopAt < m_health && isHoldMotion(m_motion->motionId);
}

RouteBiteAction::RouteBiteAction(StageCharacter *owner) : RouteAction(owner)
{
    m_at = m_owner->m_clock->getSeconds() + m_owner->m_biteInterval;
}

// FS2 0x00225030
// callcheck: getSeconds
bool RouteNumbAction::update()
{
    const float now = m_owner->m_clock->getSeconds();
    if (m_numb)
        return isNumbMotion(m_motion->motionId) && now <= m_until;
    if (isNumbMotion(m_motion->motionId) || m_until < now)
        m_numb = true;
    return true;
}

// FS2 0x00225130
// callcheck: operator_delete, operator_delete, releaseAction
RouteNumbAction::~RouteNumbAction()
{
    if (isNumbMotion(m_motion->motionId))
        m_motion->releaseAction();
}

// FS2 0x002251f0
// callcheck: getSeconds, operator_new, setSpecial, setSpecial, setSpecial
RouteNumbAction::RouteNumbAction(float seconds, StageCharacter *owner, CharacterRoute *route) : RouteAction(owner)
{
    m_until = seconds + m_owner->m_clock->getSeconds();
    m_motion = m_owner->m_motion;
    route->cancelMove();
    if (m_motion->motionId == kMotionStunned)
        m_owner->setSpecial(true, "sp_numb1");
    else if (m_motion->motionId == kMotionSpawning)
        m_owner->setSpecial(true, "sp_spawn_numb");
    else if (isFreeMotion(m_motion->motionId))
        m_owner->setSpecial(true, "sp_numb0");
}

RouteVoiceAction::RouteVoiceAction(StageCharacter *owner, const char *name) : RouteAction(owner)
{
    strcpy(m_name, name);
}

// FS2 0x002257b0
// callcheck: isVoicePlaying
bool RouteVoiceAction::update()
{
    return m_owner->isVoicePlaying(m_name);
}

// FS2 0x002257c0
// callcheck: vecDistance, atan2Octant, addAngle, addAngle, startWalkTo, turnToObject, moveToObject, isWalkBusy, isSpecialAllowed, setSpecial, find
bool RoutePlayAction::update()
{
    constexpr float kOnThePlace = 20.0f;
    constexpr float kWalkRate = 0.35f;

    PlayerMotion *motion = m_owner->m_motion;
    switch (m_state) {
    case State_Place: {
        const float distance = m_from != nullptr
            ? vecDistance(m_owner->m_xform.position, m_from->getMatrix()[3]) : 0.0f;
        const float yaw = m_to != nullptr ? turnTo(m_owner, m_to->getMatrix()[3]) : 0.0f;
        if (kOnThePlace <= distance) {
            if (m_route->move == nullptr)
                m_route->moveToObject(0.0f, m_from, 1, 0);
            return true;
        }
        if (kOneDegree <= yaw) {
            if (m_route->move == nullptr)
                m_route->turnToObject(0.0f, m_to);
            return true;
        }
        if (isFreeMotion(motion->motionId)) {
            motion->startWalkTo(kWalkRate, m_from != nullptr ? m_from->getMatrix()[3] : nullptr,
                                m_to != nullptr ? m_to->getMatrix()[3] : nullptr, true, false);
            m_state = State_Start;
        }
        return true;
    }
    case State_Start:
        if (!motion->isWalkBusy() && m_owner->isSpecialAllowed(m_name)) {
            m_owner->setSpecial(true, m_name);
            m_state = State_Play;
        }
        return true;
    case State_Play:
        if (0 <= m_motionId) {
            const MotionHistoryEntry *entry = m_owner->m_block7c8.find(m_motionId);
            if (entry != nullptr && (entry->events & kMotionEventsOver) != 0)
                return false;
        }
        // fall through
    case State_End:
        return !isHoldableMotion(motion->motionId);
    }
    return true;
}

// FS2 0x00225fc0
// callcheck: strcmp, strcpy, FUN_001d1280, releaseAction
RoutePlayAction::RoutePlayAction(StageCharacter *owner, CharacterRoute *route, const char *name,
                                 StageObject *from, StageObject *to)
    : RouteAction(owner), m_route(route)
{
    if (strcmp(name, kEndAction) == 0) {
        m_owner->m_motion->releaseAction();
        m_motionId = Motion_None;
        m_state = State_End;
    } else {
        strcpy(m_name, name);
        m_from = from;
        m_to = to;
        m_motionId = m_owner->m_motion->specialMotion(m_name);
        m_state = m_from == nullptr && m_to == nullptr ? State_Start : State_Place;
    }
    m_route->cancelMove();
}

// FS2 0x002260f0
// callcheck: find
bool RouteQuickTurnAction::update()
{
    if (m_state == 1) {
        const MotionHistoryEntry *entry = m_owner->m_block7c8.find(kMotionQuickTurn);
        if (isHoldableMotion(m_owner->m_motion->motionId)
            || (entry != nullptr && (entry->events & kMotionEventsOver) != 0))
            return false;
    } else if (m_state == 0 && m_owner->m_allows.quickTurn != 0) {
        m_owner->m_wants.quickTurn = 1;
        m_state = 1;
    }
    return true;
}

// FS2 0x00226270
// callcheck: lookAtObject, getSeconds, canSee, vecDistance, atan2Octant, addAngle, addAngle, moveToObject, turnToObject, isYriOrKne, find, vecDistance, getDeltaSeconds, operator_new, getSeconds, operator_delete, getSeconds, getViewBottom, getTopView, turnToObject, turnToObject, findWayOut, moveToPoint, getDeltaSeconds, getTurnRate, turnToObject, turnToObject, lookAtObject, getLifeState, getBiteTarget, getBiteTarget, turnToObject, lookAtObject
bool RouteFollowAction::update()
{
    constexpr float kFollowAt = 100.0f;     // nearer it need not walk
    constexpr float kRunAt = 200.0f;
    constexpr float kTooClose = 20.0f;
    constexpr float kRoomEnough = 50.0f;
    constexpr float kViewBesideRange = 100.0f;
    constexpr float kViewAboveRange = 200.0f;
    constexpr float kLightReach = 200.0f;
    constexpr float kLightSeconds = 1.0f;
    constexpr float kAttackerRange = 300.0f;
    constexpr float kWatchSeconds = 3.0f;
    constexpr u_int kStepAwayFlags = RouteMoveFlag_StopAtDoor | RouteMoveFlag_DropLimit;

    if (m_flee != nullptr) {
        if (m_flee->update())
            return true;
        delete m_flee;
        m_flee = nullptr;
        m_route->lookAtObject(kLookRate, m_target, 0);
    }

    const float now = m_owner->m_clock->getSeconds();
    float sightDistance = 0.0f;
    const bool unseen = !m_owner->canSee(kSightAngle, m_target, &sightDistance);
    const float gap = gapBetween(m_owner, m_target);
    const float yaw = turnTo(m_owner, m_target->m_xform.position);
    const int mode = kRunAt < gap || unseen ? 2 : 1;

    switch (m_state) {
    case State_Watch:
        if (m_watchUntil < now) {
            m_route->cancelMove();
            m_route->turnToObject(0.0f, m_target);
            m_route->lookAtObject(kLookRate, m_target, 0);
            m_state = State_Stand;
        }
        break;
    case State_Step:
        if (m_route->move == nullptr || kRoomEnough < gap) {
            m_route->cancelMove();
            m_route->turnToObject(0.0f, m_target);
            m_state = State_Stand;
        }
        break;
    case State_Close:
        if (kRoomEnough < gap) {
            m_state = State_Stand;
        } else {
            const bool canBack = (m_owner->m_ground->wallCount < 1 || 0.0f <= gap)
                              && m_owner->m_abilities.walkBack != 0;
            sceVu0FVECTOR way = {};
            if (canBack || !findWayOut(way, m_owner, m_target->m_xform.position, m_route->stoppedAt)) {
                wantMove(m_owner, CharacterMove_Back);
                m_route->steer(yaw);
            } else {
                m_route->moveToPoint(0.0f, way, 2, kStepAwayFlags);
                m_state = State_Step;
            }
        }
        break;
    case State_Walk: {
        StageObject *view = gStageScene->getViewBottom();
        if (view != nullptr && view == gStageScene->getTopView()) {
            sceVu0FVECTOR off;
            subVector(off, view->m_xform.position, m_owner->m_xform.position);
            if (sqrtf(off[0] * off[0] + off[1] * off[1]) < kViewBesideRange && fabsf(off[2]) < kViewAboveRange) {
                m_route->cancelMove();
                m_route->turnToObject(0.0f, m_target);
                break;
            }
        }
        if (m_route->move == nullptr || (gap < kRoomEnough && !unseen)) {
            m_route->cancelMove();
            m_route->turnToObject(0.0f, m_target);
            m_state = State_Stand;
        } else if (mode == 2) {
            m_route->moveMode = 2;
        }
        break;
    }
    case State_Stand: {
        bool walk = kFollowAt < gap;
        if (!walk && unseen && m_route->move == nullptr) {
            const int id = m_owner->m_motion->motionId;
            walk = id != Motion_Idle1 && id != Motion_Idle0 && id != Motion_CrouchIdle;
        }
        if (walk) {
            m_route->moveToObject(0.0f, m_target, mode, RouteMoveFlag_KeepPath);
            if (m_route->move == nullptr)
                m_route->turnToObject(0.0f, m_target);
            else
                m_state = State_Walk;
        } else if (m_owner->m_5ba == 0) {
            if (gap < kTooClose) {
                m_route->cancelMove();
                m_state = State_Close;
            } else if (m_owner->isYriOrKne()) {
                StageLight *light = shiningLight(m_owner);
                if (light == nullptr || kLightReach <= vecDistance(headOf(m_owner), light->getMatrix()[3])) {
                    m_inLight = 0.0f;
                } else {
                    m_inLight += m_owner->m_clock->getDeltaSeconds();
                    if (kLightSeconds < m_inLight) {
                        m_inLight = 0.0f;
                        m_flee = new RouteFleeLightAction(m_owner, m_route, light);
                        return true;
                    }
                }
            }
        }
        break;
    }
    }

    // Someone coming at the two of them comes before everything.
    if ((u_int)m_state < State_Close) {
        for (StageObject **slot = firstActive(StageObjGroup_Characters); slot != nullptr; slot = nextActive(slot)) {
            StageCharacter *other = static_cast<StageCharacter *>(*slot);
            if (other != m_owner && other != m_target && other->getLifeState() == LifeState_Up
                && (other->getBiteTarget() == m_owner || other->getBiteTarget() == m_target)
                && m_owner->distanceTo(other) < kAttackerRange && m_target->distanceTo(other) < kAttackerRange) {
                m_route->cancelMove();
                m_route->turnToObject(0.0f, other);
                m_route->lookAtObject(kLookRate, other, 0);
                m_state = State_Watch;
                m_watchUntil = now + kWatchSeconds;
                return true;
            }
        }
    }
    return true;
}

// FS2 0x00226c60
// callcheck: operator_delete, operator_delete
RouteFollowAction::~RouteFollowAction()
{
    delete m_flee;
    m_route->cancelMove();
    m_route->stopLook();
}

// FS2 0x00226d40
// callcheck: lookAtObject
RouteFollowAction::RouteFollowAction(StageCharacter *owner, CharacterRoute *route, StageCharacter *target)
    : RouteAction(owner), m_route(route), m_target(target)
{
    m_route->lookAtObject(kLookRate, m_target, 0);
}

RouteFleeAction::RouteFleeAction(StageCharacter *owner, CharacterRoute *route)
    : RouteAction(owner), m_route(route)
{
    constexpr float kFleeSeconds = 10.0f;

    m_until = m_owner->m_clock->getSeconds() + kFleeSeconds;
    m_route->stopLook();
}

RouteFleeLightAction::RouteFleeLightAction(StageCharacter *owner, CharacterRoute *route, StageLight *light)
    : RouteFleeAction(owner, route), m_light(light)
{
    constexpr float kFleeSeconds = 5.0f;

    m_until = m_owner->m_clock->getSeconds() + kFleeSeconds;
    m_route->cancelMove();
}

// FS2 0x00226da0
// callcheck: getSeconds, applyMatrix, atan2Octant, addAngle, addAngle, getDeltaSeconds, getTurnRate, isYriOrKne
bool RouteFleeLightAction::update()
{
    constexpr float kLastSecond = 1.0f;
    constexpr int kStateLeaving = 2;
    static const sceVu0FVECTOR kInTheBeam = {0.0f, 30.0f, 0.0f, 1.0f};  /* data 0x2fa810 */

    const float now = m_owner->m_clock->getSeconds();
    if (m_until < now)
        return false;
    if (m_state == 0 && !threatStands()) {
        m_until = now + kLastSecond;
        m_state = kStateLeaving;
    }

    sceVu0FVECTOR beam;
    applyMatrix(beam, m_light->getMatrix(), kInTheBeam);
    float yaw = turnTo(m_owner, beam);
    if (yaw < 0.0f)
        yaw += kNinetyFiveDeg;
    else
        yaw -= kNinetyFiveDeg;
    m_route->steer(yaw);
    if (m_owner->m_allows.move != 0)
        m_owner->m_wants.moveKind = m_owner->isYriOrKne() ? CharacterMove_Walk : CharacterMove_Run;
    return true;
}

// FS2 0x00226fd0
bool RouteFleeLightAction::threatStands()
{
    return m_owner->m_shadowLight == m_light;
}

// FS2 0x00226ff0
// callcheck: isSpecialAllowed, setSpecial
bool RouteFleeFoeAction::cower()
{
    return cowerSpecial(m_owner);
}

// FS2 0x00227050
// callcheck: getSeconds, operator_delete, findWayOut, moveToPoint
RouteFleeFoeAction::RouteFleeFoeAction(StageCharacter *owner, CharacterRoute *route, StageObject *foe)
    : RouteFleeAction(owner, route)
{
    sceVu0FVECTOR way = {};
    if (findWayOut(way, m_owner, foe->m_xform.position, m_route->stoppedAt)) {
        m_route->moveToPoint(0.0f, way, 2, RouteMoveFlag_DropLimit);
        m_state = 0;
    } else {
        m_route->cancelMove();
        cower();
        m_state = 1;
    }
}

// FS2 0x00227170
// callcheck: getSeconds
bool RouteFleeAction::update()
{
    const float now = m_owner->m_clock->getSeconds();
    if (m_state == 1)
        return !isHoldableMotion(m_owner->m_motion->motionId);
    if (m_state != 0)
        return true;
    return m_route->move != nullptr && now <= m_until;
}

// FS2 0x002272f0
// callcheck: operator_delete
RouteFleeAction::~RouteFleeAction()
{
    m_route->cancelMove();
}

RouteBackAction::RouteBackAction(StageCharacter *owner, CharacterRoute *route)
    : RouteAction(owner), m_route(route)
{
    m_route->cancelMove();
}

RouteBackFromLightAction::RouteBackFromLightAction(StageCharacter *owner, CharacterRoute *route)
    : RouteBackAction(owner, route)
{
    m_route->stopLook();
}

// FS2 0x00227390
bool RouteBackFromLightAction::getThreat(float *out)
{
    StageLight *light = shiningLight(m_owner);
    if (light == nullptr)
        return false;
    copyVector(out, light->getMatrix()[3]);
    return true;
}

// FS2 0x002273f0
// callcheck: isSpecialAllowed, setSpecial
bool RouteBackFromFoeAction::cower()
{
    return cowerSpecial(m_owner);
}

// FS2 0x00227450
bool RouteBackFromFoeAction::getThreat(float *out)
{
    copyVector(out, m_foe->m_xform.position);
    return true;
}

// FS2 0x00227470
// callcheck: lookAtObject
RouteBackFromFoeAction::RouteBackFromFoeAction(StageCharacter *owner, CharacterRoute *route, StageObject *foe)
    : RouteBackAction(owner, route), m_foe(foe)
{
    m_route->lookAtObject(0.0f, m_foe, 0);
}

// FS2 0x00227510
// callcheck: atan2Octant, addAngle, addAngle, getDeltaSeconds, getTurnRate
bool RouteBackAction::update()
{
    sceVu0FVECTOR threat;
    if (!getThreat(threat))
        return false;
    if (m_state == 1) {
        if (isHoldableMotion(m_owner->m_motion->motionId))
            return false;
    } else if (m_state == 0) {
        const float yaw = turnTo(m_owner, threat);
        if (m_owner->m_ground->wallCount < 1 && m_owner->m_abilities.walkBack != 0) {
            wantMove(m_owner, CharacterMove_Back);
            m_route->steer(yaw);
        } else if (cower()) {
            m_state = 1;
        }
    }
    return true;
}

// FS2 0x00227790
// callcheck: operator_delete, operator_delete
RouteBackAction::~RouteBackAction()
{
    m_route->stopLook();
}

// FS2 0x00227820
// callcheck: getBiteTarget, operator_new, moveToObject, addAngle
bool RouteSwoopAction::update()
{
    constexpr float kFlyOutBy = 1000.0f;

    if (m_target != m_owner->getBiteTarget())
        return false;
    if (m_turn != nullptr) {
        if (m_turn->update())
            return true;
        delete m_turn;
        m_turn = nullptr;
    }

    const float dx = m_target->m_xform.position[0] - m_owner->m_xform.position[0];
    const float dy = m_target->m_xform.position[1] - m_owner->m_xform.position[1];
    const float distance = sqrtf(dx * dx + dy * dy);
    switch (m_state) {
    case State_Away:
        if (m_range + kFlyOutBy < distance) {
            m_route->cancelMove();
            m_state = State_Start;
        } else {
            wantMove(m_owner, CharacterMove_Run);
        }
        break;
    case State_Charge:
        if (m_route->move == nullptr || distance < m_range) {
            m_route->cancelMove();
            m_state = State_Start;
        }
        break;
    case State_Start:
        if (distance < m_range) {
            m_route->cancelMove();
            m_turn = new RouteTurn(m_owner, addAngle(kMathPi, m_owner->m_xform.rotation[2]));
            m_state = State_Away;
        } else {
            m_route->moveToObject(0.0f, m_target, 2, RouteMoveFlag_Straight);
            m_state = State_Charge;
        }
        break;
    }
    return true;
}

// FS2 0x00227ac0
// callcheck: operator_delete
RouteSwoopAction::~RouteSwoopAction()
{
    delete m_turn;
    m_route->cancelMove();
}

// FS2 0x00227b80
// callcheck: getBiteTarget, canSee, vecDistance, findWayOut, moveToPoint, moveToObject, turnToObject, canSee, turnToObject, turnToObject
bool RouteStalkAction::update()
{
    constexpr float kTooFar = 1.2f;
    constexpr float kTooNear = 0.8f;

    if (m_target != m_owner->getBiteTarget())
        return false;
    StageCharacter *target = static_cast<StageCharacter *>(m_target);
    float sightDistance = 0.0f;
    const bool sees = m_owner->canSee(kSightAngle, target, &sightDistance);
    const float distance = vecDistance(m_owner->m_xform.position, target->m_xform.position);
    switch (m_state) {
    case State_Leave:
        if (m_route->move == nullptr || m_range < distance) {
            m_route->cancelMove();
            m_route->turnToObject(0.0f, m_target);
            m_state = State_Hold;
        }
        break;
    case State_Close:
        if (m_route->move == nullptr || (distance < m_range && sees)) {
            m_route->cancelMove();
            m_route->turnToObject(0.0f, m_target);
            m_state = State_Hold;
        }
        break;
    case State_Hold:
        if (m_range * kTooFar < distance || (!sees && m_route->move == nullptr)) {
            m_route->moveToObject(0.0f, m_target, 2, 0);
            if (m_route->move == nullptr)
                m_route->turnToObject(0.0f, m_target);
            else
                m_state = State_Close;
        } else {
            if (!(distance < m_range * kTooNear)) {
                float seenFrom = 0.0f;
                if (!target->canSee(kSightAngle, m_owner, &seenFrom))
                    return true;
            }
            sceVu0FVECTOR way = {};
            if (findWayOut(way, m_owner, target->m_xform.position, m_route->stoppedAt)) {
                m_route->moveToPoint(0.0f, way, 2, 0);
                m_state = State_Leave;
            }
        }
        break;
    }
    return true;
}

// FS2 0x00227e10
// callcheck: operator_delete, operator_delete
RouteStalkAction::~RouteStalkAction()
{
    m_route->cancelMove();
    m_route->stopLook();
}

// FS2 0x00227ed0
// callcheck: lookAtObject
RouteStalkAction::RouteStalkAction(float range, StageCharacter *owner, CharacterRoute *route, StageObject *target)
    : RouteAction(owner), m_route(route), m_target(target), m_range(range)
{
    m_route->lookAtObject(kLookRate, m_target, 0);
}

// The way out of the area whose end lies most against the point, not the one
// it last stopped at; last is CharacterRoute::stoppedAt. A stored path ending
// in a chain: the ROM asks for the node one past the last, which reads what
// follows the list; the host takes the last one (open-items.md).
// FS2 0x00227f30
// callcheck: ~vector, constructAt, areaList, constructAt, crossingPath, ~vector, ~vector, erase, vecNormalize, constructAt, crossingPath, ~vector, link, gate, node, node, node, chainNodeCount, chainNode, ~vector, ~vector, ~vector, vecNormalize, ~vector
namespace {

bool findWayOut(float *out, StageCharacter *who, const float *threat, const float *last)
{
    constexpr float kNothingYet = 6.2831855f;

    RoutePlayer *routes = gStageScene->routePlayer;
    const int area = who->m_ground->floorAttr;
    if (area < 0)
        return false;

    std::vector<short> ends;
    routes->areaList(ends, (short)area);
    for (auto end = ends.begin(); end != ends.end();) {
        RoutePath path;
        if (routes->crossingPath(path, (short)area, *end, *end, 0) < 0.0f)
            end = ends.erase(end);
        else
            ++end;
    }
    if (ends.empty())
        return false;

    sceVu0FVECTOR toThreat = {threat[0] - who->m_xform.position[0], threat[1] - who->m_xform.position[1], 0.0f, 1.0f};
    vecNormalize(toThreat, toThreat);

    float best = kNothingYet;
    float place[3] = {};        // a leg of none of the four kinds keeps the one before
    for (short end : ends) {
        RoutePath path;
        if (routes->crossingPath(path, (short)area, end, end, 0) <= 0.0f || path.steps.empty())
            continue;
        const RouteLeg &leg = path.steps.back();
        if (leg.kind == RouteLeg_Chain) {
            const RouteNode *node = routes->chainNode(leg.index, routes->chainNodeCount(leg.index) - 1);
            place[0] = node->position[0];
            place[1] = node->position[1];
            place[2] = node->position[2];
        } else if (leg.kind == RouteLeg_Node) {
            const RouteNode *node = routes->node(leg.index);
            place[0] = node->position[0];
            place[1] = node->position[1];
            place[2] = node->position[2];
        } else if (leg.kind == RouteLeg_Gate) {
            const RouteSpan *gate = routes->gate(leg.index);
            place[0] = (gate->first->position[0] + gate->second->position[0]) / 2.0f;
            place[1] = (gate->first->position[1] + gate->second->position[1]) / 2.0f;
            place[2] = (gate->first->position[2] + gate->second->position[2]) / 2.0f;
        } else if (leg.kind == RouteLeg_Link) {
            const RouteSpan *link = routes->link(leg.index);
            const RouteNode *node = (leg.index & kRouteLinkReversed) == 0 ? link->second : link->first;
            place[0] = node->position[0];
            place[1] = node->position[1];
            place[2] = node->position[2];
        }
        if (place[0] == last[0] && place[1] == last[1] && place[2] == last[2])
            continue;
        sceVu0FVECTOR toPlace = {place[0] - who->m_xform.position[0], place[1] - who->m_xform.position[1], 0.0f, 1.0f};
        vecNormalize(toPlace, toPlace);
        const float along = toThreat[0] * toPlace[0] + toThreat[1] * toPlace[1];
        if (along < best) {
            out[0] = place[0];
            out[1] = place[1];
            out[2] = place[2];
            best = along;
        }
    }
    return best <= kMathPi;
}

} // namespace

// Inline in the ROM: the character's own bite, in 0x228670.
bool RouteHuntAction::tryBite()
{
    if (m_owner->m_abilities.flag3be == 0 || m_owner->m_allows.bite == 0)
        return false;
    m_owner->m_wants.bite = 1;
    if (StageObject *target = m_owner->getBiteTarget())
        m_owner->m_628 = target;
    m_route->skipUpdate = 1;
    return true;
}

// Inline in the ROM: Srt and Yrt keep out of a light near their head.
bool RouteHuntAction::runFrom()
{
    constexpr float kLightReach = 300.0f;

    const int kind = stageObjRecord(m_owner->m_itemDef)->kind;
    if (kind != CharacterKind_Srt && kind != CharacterKind_Yrt)
        return false;
    StageLight *light = shiningLight(m_owner);
    if (light == nullptr || kLightReach <= vecDistance(headOf(m_owner), light->getMatrix()[3]))
        return false;
    m_task = new RouteFleeLightAction(m_owner, m_route, light);
    return true;
}

// The end of 0x228670, and the same in the ctor without the test of ready.
// callcheck: getWeaponRange, moveToObject, turnToObject
void RouteHuntAction::goAfter(StageObject *weapon, bool seen)
{
    float reach = 0.0f;
    if (weapon != nullptr && isFirearm(weapon) && seen)
        reach = m_owner->getWeaponRange() * kShootFrom;
    if (m_owner->m_6d4 != 0)
        m_route->moveToObject(reach, m_target, 2, 0);
    if (m_route->move == nullptr)
        m_route->turnToObject(0.0f, m_target);
}

// FS2 0x00228670
// callcheck: lookAtObject, getLifeState, getLifeState, getBiteTarget, canSee, getSeconds, getStageSelect, find, vecDistance, operator_new, vecDistance, getStageSelect, getStageSelect, getStageSelect, getStageSelect, operator_new, shoot, randNext, operator_new, melee, getStageSelect, getSeconds, operator_delete, getSeconds, getBiteTarget, getSeconds, getSeconds, getSeconds, getWeaponRange, shoot, melee, isPlayer, randNext, randNext, isPartner, operator_new, getSeconds, getWeaponRange, turnToObject, moveToObject
bool RouteHuntAction::update()
{
    constexpr float kBiteRange = 10.0f;
    constexpr float kBlowRange = 15.0f;
    constexpr float kYrtExtraRange = 45.0f;
    constexpr float kSrtExtraRange = 15.0f;
    constexpr float kOnASlope = 10.0f;      // what a tilted floor takes off the blow's reach
    constexpr float kTackleRange = 50.0f;
    constexpr float kFriendsRange = 500.0f;
    constexpr int kBiteChanceWithFriends = 80;
    constexpr int kBiteChanceAlone = 40;
    constexpr int kBurstMagazine = 0x13;    // a magazine above this fires bursts
    constexpr float kBiteMindSeconds = 10.0f;
    constexpr float kThoughtSeconds = 10.0f;
    constexpr float kThoughtSpread = 20.0f;
    constexpr int kScriptBite = 1;          // m_6c8
    constexpr int kScriptBurst = 2;

    if (m_task != nullptr) {
        if (m_task->update())
            return true;
        delete m_task;
        m_task = nullptr;
        m_route->lookAtObject(kLookRate, m_target, 0);
    }

    const int life = m_target->getLifeState();
    if (!(1 < life && life < 6))
        return false;
    if (m_target != m_owner->getBiteTarget())
        return false;

    StageObject *weapon = m_owner->m_weapons[0];
    float sightDistance = 0.0f;
    const bool seen = m_owner->canSee(kSightAngle, m_target, &sightDistance);
    const float now = m_owner->m_clock->getSeconds();

    if (m_fights) {
        if (runFrom())
            return true;

        const float gap = gapBetween(m_owner, m_target);
        const int kind = stageObjRecord(m_owner->m_itemDef)->kind;
        const StageCollisionFace *floor = m_owner->m_ground->floorFace;
        const float slope = kind != CharacterKind_Yrt && floor != nullptr
            && (floor->flags & StageCollision_Slope) != 0 ? kOnASlope : 0.0f;
        float blowRange = kBlowRange;
        if (kind == CharacterKind_Yrt)
            blowRange += kYrtExtraRange;
        if (stageObjRecord(m_target->m_itemDef)->kind == CharacterKind_Yrt && m_target->getBiteTarget() == m_owner)
            blowRange += kYrtExtraRange;
        if (kind == CharacterKind_Srt)
            blowRange += kSrtExtraRange;

        const int targetMotion = m_target->m_motion->motionId;
        const bool tackles = m_owner->m_abilities.flag3be != 0 && m_owner->m_6cc == 0 && m_owner->m_6d0 == 0
                          && gap < kTackleRange
                          && (isRunMotion(targetMotion) || isCrouchRunMotion(targetMotion))
                          && (weapon == nullptr || !isRifle(weapon));
        if (tackles) {
            if (tryBite())
                return true;
        } else if (m_owner->m_6d0 == 0
                   && (m_owner->m_6c8 == kScriptBite || (weapon == nullptr && m_owner->m_abilities.flag3c4 == 0))) {
            if (gap < kBiteRange && m_owner->m_allows.bite != 0) {
                m_task = new RouteBiteAction(m_owner);
                return true;
            }
        } else if (m_owner->m_6c8 == kScriptBurst && weapon != nullptr
                   && kBurstMagazine < weaponDefOf(weapon).magazine) {
            if (m_route->shoot(m_target)) {
                const float seconds = randomUnit() + kBurstSeconds;
                m_task = new RouteShootAction(seconds, m_owner, m_route, m_target);
                return true;
            }
        } else if (weapon != nullptr && isFirearm(weapon)) {
            if (blowRange - slope <= gap || m_owner->m_aiLevel < 1) {
                if (gap < m_owner->getWeaponRange() * kShootFrom && 0 < weapon->m_ammoA && m_route->shoot(m_target))
                    return true;
            } else if (m_route->melee(m_target, true)) {
                return true;
            }
        } else if (!m_wantsBite && gap < blowRange - slope && m_route->melee(m_target, true)) {
            return true;
        }

        // A foe with bare hands sets its mind on the player's throat now and then.
        if (m_owner->m_6d0 == 0 && !(weapon != nullptr && isFirearm(weapon))
            && (m_target->isPlayer() || m_target->isPartner())) {
            if (!m_wantsBite) {
                if (m_nextThought < now) {
                    const int chance = m_owner->scriptHasAllyWithin(kFriendsRange) ? kBiteChanceWithFriends
                                                                                   : kBiteChanceAlone;
                    if ((int)randNext() % 100 < chance) {
                        m_wantsBite = true;
                        m_biteUntil = now + kBiteMindSeconds;
                    }
                    m_nextThought = randomUnit() * kThoughtSpread + now + kThoughtSeconds;
                }
            } else if (m_biteUntil < now || isBittenMotion(targetMotion)) {
                m_wantsBite = false;
            } else if (gap < kBiteRange) {
                if (m_owner->m_allows.bite != 0) {
                    m_task = new RouteBiteAction(m_owner);
                    return true;
                }
                m_wantsBite = false;
            }
        }
    }

    if (m_route->move == nullptr && m_owner->m_wants.ready == 0)
        goAfter(weapon, seen);
    return true;
}

// FS2 0x002298d0
// callcheck: operator_delete, operator_delete
RouteHuntAction::~RouteHuntAction()
{
    delete m_task;
    m_route->cancelMove();
    m_route->stopLook();
}

// FS2 0x002299b0
// callcheck: getSeconds, canSee, getWeaponRange, lookAtObject, moveToObject, turnToObject
RouteHuntAction::RouteHuntAction(StageCharacter *owner, CharacterRoute *route, StageObject *target, bool fights)
    : RouteAction(owner), m_route(route), m_target(static_cast<StageCharacter *>(target)), m_fights(fights)
{
    m_nextThought = m_owner->m_clock->getSeconds();
    StageObject *weapon = m_owner->m_weapons[0];
    bool seen = false;
    if (weapon != nullptr && isFirearm(weapon)) {
        float sightDistance = 0.0f;
        seen = m_owner->canSee(kSightAngle, m_target, &sightDistance);
    }
    goAfter(weapon, seen);
    m_route->lookAtObject(kLookRate, m_target, 0);
}

// FS2 0x00229c20
// callcheck: getSeconds, randNext, randNext, randNext, turnToYaw, getSeconds, randNext, sinCos, sinCos, findFloorFaceExcept, moveToPoint
bool RouteWanderAction::update()
{
    constexpr float kTwoPi = 6.2831855f;
    constexpr u_int kTurnChance = 0x5999;   // of 0x8000: it turns; else it strolls
    constexpr float kWalkHeight = 50.0f;
    constexpr float kFloorRange = -1.0f;
    constexpr u_int kFaceFloor = 0x42400c;
    constexpr u_int kFaceFloorSkip = 0x40000;
    constexpr float kStrollSeconds = 10.0f;
    constexpr float kPauseSeconds = 3.0f;

    const float now = m_owner->m_clock->getSeconds();
    if (m_state == State_Pause) {
        if (m_until < now) {
            const float yaw = randomUnit() * kTwoPi - kMathPi;
            if ((randNext() & 0x7fff) < kTurnChance) {
                m_route->turnToYaw(yaw, 0.0f);
            } else {
                const float half = m_distance * 0.5f;
                const float reach = half * randomUnit() + half + 0.0f;
                sceVu0FVECTOR goal;
                copyVector(goal, m_point);
                goal[0] = reach * sinCos(yaw, SinCos_Sin) + goal[0] + 0.0f;
                goal[1] = reach * sinCos(yaw, SinCos_Cos) + goal[1] + 0.0f;
                goal[2] += kWalkHeight;
                const StageCollisionFace *face = gStageScene->findFloorFaceExcept(
                    goal[0], goal[1], kFloorRange, &goal[2], kFaceFloor, kFaceFloorSkip,
                    gStageScene->car);
                if (face == nullptr || (short)face->attr < 0)
                    return true;
                m_route->moveToPoint(0.0f, goal, 1, 0);
            }
            m_until = m_owner->m_clock->getSeconds() + kStrollSeconds;
            m_state = State_Going;
        }
    } else if (m_state == State_Going) {
        if (m_until < now)
            m_route->cancelMove();
        if (m_route->move == nullptr) {
            m_until = randomUnit() * kPauseSeconds + now + 0.0f + kPauseSeconds;
            m_state = State_Pause;
        }
    }
    return true;
}

// FS2 0x00229ec0
// callcheck: operator_delete
RouteWanderAction::~RouteWanderAction()
{
    m_route->cancelMove();
}

RouteWanderAction::RouteWanderAction(float distance, StageCharacter *owner, CharacterRoute *route, const float *point)
    : RouteAction(owner), m_route(route), m_distance(distance)
{
    constexpr float kFirstStrollSeconds = 10.0f;

    copyVector(m_point, point);
    m_until = m_owner->m_clock->getSeconds() + kFirstStrollSeconds;
    m_route->stopLook();
}

// FS2 0x00229f60
// callcheck: getSeconds, getBiteTarget
bool RouteBiteAction::update()
{
    if (!(m_at < m_owner->m_clock->getSeconds()))
        return true;
    if (m_owner->m_allows.bite != 0) {
        m_owner->m_wants.bite = 1;
        StageObject *target = m_owner->getBiteTarget();
        if (target != nullptr)
            m_owner->m_628 = target;
    }
    return false;
}

RouteShootAction::RouteShootAction(float seconds, StageCharacter *owner, CharacterRoute *route, StageObject *target)
    : RouteAction(owner), m_route(route), m_target(target)
{
    constexpr float kFramesPerSecond = 30.0f;

    m_until = seconds + m_owner->m_clock->getSeconds();
    m_interval = 0.0f;
    if (StageObject *weapon = m_owner->m_weapons[0]) {
        const int index = weapon->m_weaponDef;
        const WeaponDef &def = gWeaponDefs[index < 0 || index >= kWeaponDefCount ? 0 : index];
        m_interval = ((float)def.param9 + 0.5f) / kFramesPerSecond;
    }
    m_nextShot = m_interval + m_owner->m_clock->getSeconds();
}

// FS2 0x00229fe0
// callcheck: getSeconds, atan2Octant, addAngle, addAngle, getDeltaSeconds, getTurnRate, getSeconds, getSeconds
bool RouteShootAction::update()
{
    const float now = m_owner->m_clock->getSeconds();
    StageObject *weapon = m_owner->m_weapons[0];
    if (weapon == nullptr || !isFirearm(weapon) || weapon->m_ammoA <= 0)
        return false;

    if (m_target != nullptr)
        m_route->steer(turnTo(m_owner, m_target->m_xform.position));
    m_owner->m_294 = m_target;
    m_owner->m_wants.ready = 1;
    m_route->holdReady(3.0f);
    if (m_until < now)
        return false;
    if (m_nextShot < now && m_owner->m_allows.fire != 0) {
        m_owner->m_wants.fire = m_target == nullptr ? CharacterFire_Single : CharacterFire_Auto;
        m_route->lastShot = m_owner->m_clock->getSeconds();
        m_nextShot = now + m_interval;
    }
    return true;
}

// FS2 0x0022a310
// callcheck: getSeconds
bool RouteSwingAction::update()
{
    m_owner->m_wants.ready = 1;
    const bool waits = m_owner->m_allows.fire == 0;
    if (!waits) {
        m_owner->m_wants.fire = CharacterFire_Tap;
        m_route->lastBlow = m_owner->m_clock->getSeconds();
    }
    return waits;
}

// ──────────────────────────────────────────────────────────────────────
// The route.

// FS2 0x0022f270
// callcheck: operator_new, RouteFilter
CharacterRoute::CharacterRoute(CharacterBrain *brain)
{
    owner = brain->getOwnerCharacter();
    filter = new RouteFilter(gStageScene->routePlayer, owner);
}

// FS2 0x0022f1b0
// callcheck: ~RouteFilter, operator_delete (deleting)
CharacterRoute::~CharacterRoute()
{
    delete filter;
    delete action;
    delete move;
    delete look;
}

void CharacterRoute::steer(float delta)
{
    const float limit = (owner->m_clock->getDeltaSeconds() / kFrameSeconds) * owner->getTurnRate();
    if (delta < -limit)
        delta = -limit;
    else if (limit < delta)
        delta = limit;
    owner->m_wants.moveYaw = delta;
}

void CharacterRoute::holdReady(float seconds)
{
    readyUntil = owner->m_clock->getSeconds() + seconds;
}

void CharacterRoute::setMove(RouteMove *task)
{
    delete move;
    move = task;
}

void CharacterRoute::setAction(RouteAction *task)
{
    delete action;
    action = task;
}

void CharacterRoute::setLook(RouteLook *task)
{
    delete look;
    look = task;
}

bool CharacterRoute::actionYields(int level)
{
    return action == nullptr || action->priority() < 1 || action->priority() < level;
}

// FS2 0x0022a370
// callcheck: allow
void CharacterRoute::allowPath(const char *name, bool on)
{
    filter->allow(name, on);
    if (move != nullptr)
        move->pathsChanged();
}

// FS2 0x0022a3c0
// callcheck: isSpecialAllowed, setSpecial
bool CharacterRoute::useElevator(int which)
{
    const char *name;
    switch (which) {
    case 1: name = "ElevatorB1"; break;
    case 2: name = "ElevatorB3"; break;
    case 3: name = "ElevatorBxB1"; break;
    case 4: name = "ElevatorBxB3"; break;
    default: return false;
    }
    if (!owner->isSpecialAllowed(name))
        return false;
    owner->setSpecial(true, name);
    skipUpdate = 1;
    return true;
}

// FS2 0x0022a4f0
// callcheck: isSpecialAllowed, setSpecial
bool CharacterRoute::dragOutCar()
{
    static const char kName[] = "DragOutCar";

    const bool allowed = owner->isSpecialAllowed(kName);
    if (allowed) {
        owner->setSpecial(true, kName);
        skipUpdate = 1;
    }
    return allowed;
}

// FS2 0x0022a550
bool CharacterRoute::useDoorKey(bool unlock)
{
    if (isCrouchMotion(owner->m_motion->motionId))
        return false;
    const int key = owner->m_allows.doorKey;
    if (key == 1 ? unlock : !(key == 2 || key == 3) || !unlock)
        return false;
    owner->m_wants.useDoorKey = 1;
    skipUpdate = 1;
    return true;
}

// FS2 0x0022a710
bool CharacterRoute::useDoor(bool open)
{
    if (isCrouchMotion(owner->m_motion->motionId))
        return false;
    const int hand = owner->m_allows.doorHand;
    if (hand == 2 ? open : hand != 1 || !open)
        return false;
    owner->m_wants.useDoor = 1;
    skipUpdate = 1;
    return true;
}

// FS2 0x0022a8c0
bool CharacterRoute::takeUpTrap(int arg)
{
    (void)arg;
    const bool allowed = owner->m_allows.trapCancel != 0;
    if (allowed) {
        owner->m_wants.trapCancel = 1;
        skipUpdate = 1;
    }
    return allowed;
}

// FS2 0x0022a8f0
bool CharacterRoute::setTrap(StageObject *trap)
{
    (void)trap;
    const bool allowed = owner->m_allows.trapSet != 0;
    if (allowed) {
        owner->m_wants.trapSet = 1;
        skipUpdate = 1;
    }
    return allowed;
}

// FS2 0x0022a920
// callcheck: strcmp, strcmp, strcmp
bool CharacterRoute::gunAction(int rounds)
{
    if (owner->m_abilities.reload == 0 || owner->m_allows.reload == 0)
        return false;
    StageObject *weapon = owner->m_weapons[0];
    if (weapon == nullptr || !isFirearm(weapon))
        return false;
    if (strcmp(weapon->m_name, "G_NN38") != 0 && strcmp(weapon->m_name, "R_MURATA") != 0
        && strcmp(weapon->m_name, "G_26NEN") != 0)
        rounds = weapon->m_ammoB;
    owner->m_wants.reload = 1;
    owner->m_wants.reloadRounds = rounds;
    skipUpdate = 1;
    return true;
}

// FS2 0x0022ab50
bool CharacterRoute::takeItem(StageObject *item)
{
    (void)item;
    if (owner->m_abilities.flag3b9 == 0 || owner->m_allows.pickup == 0)
        return false;
    owner->m_wants.pickup = 1;
    skipUpdate = 1;
    return true;
}

// FS2 0x0022ab90
// callcheck: FUN_0020b200 (the setter inline: throwItem, its vector)
bool CharacterRoute::action22ab90(void *a, void *b)
{
    if (owner->m_abilities.throwing == 0 || owner->m_allows.throwItem == 0)
        return false;
    owner->m_wants.throwItem = 1;
    owner->m_throwItem = static_cast<StageThrownItem *>(a);
    copyVector(owner->m_wants.throwVector, static_cast<const float *>(b));
    skipUpdate = 1;
    return true;
}

// FS2 0x0022abf0
bool CharacterRoute::action22abf0()
{
    const bool crouched = isCrouchMotion(owner->m_motion->motionId);
    if (crouched) {
        owner->m_wants.squat = 1;
        skipUpdate = 1;
    }
    return crouched;
}

// FS2 0x0022ad60
bool CharacterRoute::action22ad60()
{
    if (isCrouchMotion(owner->m_motion->motionId) || owner->m_allows.squat == 0)
        return false;
    owner->m_wants.squat = 1;
    skipUpdate = 1;
    return true;
}

// FS2 0x0022aee0
// callcheck: getSeconds, canSee, getSeconds, getSeconds, getSeconds, isOpeningLead, takeDamage, getSeconds, operator_delete
bool CharacterRoute::shoot(StageObject *target)
{
    constexpr float kReadySeconds = 3.0f;
    constexpr float kLostSeconds = 1.5f;
    constexpr int kStoryHitKind = 9;
    constexpr int kStoryHitDamage = 0x3fffffff;

    const float now = owner->m_clock->getSeconds();
    if (now < shotDelay)
        return false;
    StageObject *weapon = owner->m_weapons[0];
    if (weapon == nullptr || !isFirearm(weapon) || weapon->m_ammoA < 1)
        return false;

    owner->m_294 = target;
    if (target != nullptr && target->getKind() == StageObject_Character) {
        float distance = 0.0f;
        const bool seen = owner->canSee(kSightAngle, target, &distance);
        if (!seen && owner->m_6d4 != 0 && readyUntil < now)
            return false;
        owner->m_wants.ready = 1;
        holdReady(kReadySeconds);
        if (now < lastShot + owner->m_shotInterval || owner->m_allows.fire == 0)
            return false;
        if (!seen) {
            if (owner->m_6d4 != 0) {
                holdReady(0.0f);
                shotDelay = owner->m_clock->getSeconds() + kLostSeconds;
            }
            return false;
        }
        if (owner->m_290 == nullptr)
            return false;
        sceVu0FVECTOR aim;
        copyVector(aim, owner->m_aimPoint);
        if (allyInLine(weapon, owner, aim))
            return false;
        StageCharacter *victim = static_cast<StageCharacter *>(target);
        if (victim->isOpeningLead())
            victim->takeDamage(owner, kStoryHitKind, 1, kStoryHitDamage);
        else
            owner->m_wants.fire = CharacterFire_Auto;
        owner->m_628 = target;
    } else {
        owner->m_wants.ready = 1;
        holdReady(kReadySeconds);
        if (now < lastShot + owner->m_shotInterval || owner->m_allows.fire == 0)
            return false;
        owner->m_wants.fire = target == nullptr ? CharacterFire_Single : CharacterFire_Auto;
    }
    skipUpdate = 1;
    lastShot = owner->m_clock->getSeconds();
    cancelMove();
    return true;
}

// FS2 0x0022be10
// callcheck: shoot
bool CharacterRoute::hit(StageObject *target)
{
    return shoot(target);
}

// FS2 0x0022b580
// callcheck: getSeconds, hasGear, atan2Octant, addAngle, addAngle, findObjectOnSegment, raycast, findDoorOfFace, getStageSelect, isSpecialAllowed, setSpecial, isOpeningLead, randNext, takeDamage, getSeconds, operator_delete
bool CharacterRoute::melee(StageCharacter *target, bool endMove)
{
    constexpr float kBodyHeight = 50.0f;
    constexpr float kHighBlow = 500.0f;
    constexpr int kStoryHitKind = 9;
    constexpr int kStoryHitDamage = 0x3fffffff;
    static const char *const kBlows[2] = {"sp_at1_1", "sp_at1_2"};      // 0x31aa68

    const float now = owner->m_clock->getSeconds();
    if (target->hasGear())
        return false;
    owner->m_294 = target;
    if (kTwentyDeg <= fabsf(turnTo(owner, target->m_xform.position)))
        return false;

    sceVu0FVECTOR from;
    sceVu0FVECTOR to;
    copyVector(from, owner->m_xform.position);
    copyVector(to, target->m_xform.position);
    from[2] += kBodyHeight;
    to[2] += kBodyHeight;
    StageObject *door = gStageScene->findObjectOnSegment(from, to, 0, StageObject_Door, 1, 0, nullptr);
    if (door == nullptr) {
        sceVu0FVECTOR hit;
        const StageCollisionFace *face = gStageScene->raycast(hit, from, to, 0, 0x4000000, 0, nullptr);
        if (face != nullptr)
            door = gStageScene->findDoorOfFace(face);
    }
    if (door != nullptr) {
        if (owner->m_allows.doorHand == 1)
            owner->m_wants.useDoor = 1;
        else if (owner->m_allows.doorKey == 2 || owner->m_allows.doorKey == 3)
            owner->m_wants.useDoorKey = 1;
        return false;
    }

    owner->m_wants.ready = 1;
    if (now < lastBlow + owner->m_attackInterval)
        return false;
    if (stageObjRecord(owner->m_itemDef)->kind == CharacterKind_NoShadow) {
        const char *blow = kBlows[owner->m_xform.position[2] - target->m_xform.position[2] < kHighBlow ? 0 : 1];
        if (!owner->isSpecialAllowed(blow))
            return false;
        owner->setSpecial(true, blow);
    } else {
        if (owner->m_allows.fire == 0)
            return false;
        if (target->isOpeningLead())
            target->takeDamage(owner, kStoryHitKind, 1, kStoryHitDamage);
        else
            owner->m_wants.fire = (int)randNext() % 2 == 0 ? CharacterFire_Tap : CharacterFire_Hold;
    }
    owner->m_628 = target;
    skipUpdate = 1;
    lastBlow = owner->m_clock->getSeconds();
    if (endMove)
        cancelMove();
    return true;
}

// FS2 0x0022b970
// callcheck: operator_new, getSeconds, operator_delete
bool CharacterRoute::attackTask(StageObject *target)
{
    (void)target;
    if (owner->m_allows.bite == 0 || !actionYields(1))
        return false;
    setAction(new RouteBiteAction(owner));
    return true;
}

// FS2 0x0022ba80
// callcheck: shoot, operator_new, getSeconds, getSeconds, operator_delete
bool CharacterRoute::hitInTime(float seconds, StageObject *target)
{
    if (!actionYields(1) || !shoot(target))
        return false;
    setAction(new RouteShootAction(seconds, owner, this, target));
    skipUpdate = 0;
    return true;
}

// FS2 0x0022bc50
// callcheck: shoot, operator_new, getSeconds, getSeconds, operator_delete
bool CharacterRoute::hitRunning(float seconds, StageObject *target)
{
    if (!actionYields(1) || !shoot(target))
        return false;
    setAction(new RouteShootAction(seconds, owner, this, nullptr));
    skipUpdate = 0;
    return true;
}

// FS2 0x0022be20
// callcheck: melee, operator_new, operator_delete
bool CharacterRoute::swingAt(StageObject *target)
{
    if (target != nullptr && target->getKind() == StageObject_Character)
        return melee(static_cast<StageCharacter *>(target), true);
    if (!actionYields(1))
        return false;
    setAction(new RouteSwingAction(owner, this));
    return true;
}

// FS2 0x0022bf60
// callcheck: operator_new, operator_new, operator_delete, operator_delete
bool CharacterRoute::action22bf60()
{
    if (!actionYields(2) || !isHoldMotion(owner->m_motion->motionId))
        return false;
    setAction(new RouteHoldAction(owner, this));
    return true;
}

// FS2 0x0022c0f0
// callcheck: getStageSelect, operator_new, operator_delete (stageObjRecord, RouteNumbAction)
bool CharacterRoute::goNumb(int seconds)
{
    if (seconds == 0) {
        setAction(nullptr);
        return true;
    }
    if (!actionYields(2) || stageObjRecord(owner->m_itemDef)->kind != CharacterKind_NoShadow)
        return false;
    const int id = owner->m_motion->motionId;
    if (id != kMotionStunned && id != kMotionSpawning && !isFreeMotion(id))
        return false;
    setAction(new RouteNumbAction((float)seconds, owner, this));
    return true;
}

// FS2 0x0022c840
// callcheck: isVoicePlaying, playVoice, operator_new, strcpy, operator_delete
bool CharacterRoute::speak(const char *name)
{
    if (!actionYields(0))
        return false;
    if (!owner->isVoicePlaying(name) && !owner->playVoice(name))
        return false;
    setAction(new RouteVoiceAction(owner, name));
    return true;
}

// FS2 0x0022c960
// callcheck: operator_delete, setHoldIfFree
bool CharacterRoute::restIn(int variant)
{
    if (!actionYields(0))
        return false;
    setAction(nullptr);
    setMove(nullptr);
    setLook(nullptr);
    owner->m_motion->setHoldIfFree(variant + kMotionRestFirst);
    skipUpdate = 1;
    return true;
}

// FS2 0x0022cac0
// callcheck: strcmp, operator_new, operator_delete
bool CharacterRoute::playAction(const char *name, StageObject *a, StageObject *b)
{
    if (!actionYields(0) && strcmp(name, kEndAction) != 0)
        return false;
    setAction(new RoutePlayAction(owner, this, name, a, b));
    return true;
}

// FS2 0x0022cbe0
// callcheck: operator_new, operator_delete
bool CharacterRoute::quickTurn()
{
    if (!actionYields(0) || owner->m_abilities.quickTurn == 0)
        return false;
    setMove(nullptr);
    setAction(new RouteQuickTurnAction(owner));
    return true;
}

// FS2 0x0022ccf0
// callcheck: operator_new, operator_delete
bool CharacterRoute::followCharacter(StageObject *character)
{
    if (!actionYields(0) || character == nullptr)
        return false;
    setAction(new RouteFollowAction(owner, this, static_cast<StageCharacter *>(character)));
    return true;
}

// FS2 0x0022cde0
// callcheck: operator_new, getSeconds, operator_delete, getSeconds
bool CharacterRoute::fleeLight()
{
    if (!actionYields(0))
        return false;
    StageLight *light = shiningLight(owner);
    if (light == nullptr)
        return false;
    setAction(new RouteFleeLightAction(owner, this, light));
    return true;
}

// FS2 0x0022cf90
// callcheck: operator_new, operator_delete
bool CharacterRoute::backFromLight()
{
    if (!actionYields(0) || shiningLight(owner) == nullptr)
        return false;
    setAction(new RouteBackFromLightAction(owner, this));
    return true;
}

// FS2 0x0022d0f0
// callcheck: operator_new, operator_delete
bool CharacterRoute::fleeFoe(StageObject *foe)
{
    if (!actionYields(0) || foe == nullptr || foe->getKind() != StageObject_Character)
        return false;
    setAction(new RouteFleeFoeAction(owner, this, foe));
    return true;
}

// FS2 0x0022d220
// callcheck: operator_new, operator_delete
bool CharacterRoute::backFromFoe(StageObject *foe)
{
    if (!actionYields(0) || foe == nullptr || foe->getKind() != StageObject_Character)
        return false;
    setAction(new RouteBackFromFoeAction(owner, this, foe));
    return true;
}

// FS2 0x0022d350
// callcheck: getStageSelect, operator_new, operator_new, operator_new, operator_delete
bool CharacterRoute::attack(float range, StageObject *target, bool fights)
{
    if (target == nullptr || target->getKind() != StageObject_Character || !actionYields(0))
        return false;
    if (range <= 0.0f)
        setAction(new RouteHuntAction(owner, this, target, fights));
    else if (stageObjRecord(owner->m_itemDef)->kind == CharacterKind_NoShadow)
        setAction(new RouteSwoopAction(owner, this, target, range));
    else
        setAction(new RouteStalkAction(range, owner, this, target));
    return true;
}

// FS2 0x0022d6d0
// callcheck: operator_new, getSeconds, operator_delete
bool CharacterRoute::wanderNear(float distance, const float *point)
{
    if (!actionYields(0))
        return false;
    setAction(new RouteWanderAction(distance, owner, this, point));
    return true;
}

// FS2 0x0022d810
// callcheck: operator_delete
bool CharacterRoute::stop()
{
    if (actionYields(0))
        setAction(nullptr);
    setMove(nullptr);
    setLook(nullptr);
    return true;
}

// FS2 0x0022d900
// callcheck: atan2Octant, addAngle, addAngle, turnToObject, operator_new, RouteLookObject, operator_delete
bool CharacterRoute::lookAtObject(float rate, StageObject *object, int force)
{
    if (object == nullptr) {
        setLook(nullptr);
        return true;
    }
    if (force != 0 && move == nullptr && kSixtyFiveDeg < fabsf(turnTo(owner, object->m_xform.position)))
        turnToObject(0.0f, object);
    setLook(new RouteLookObject(rate, owner, object));
    return true;
}

// FS2 0x0022da50
// callcheck: atan2Octant, addAngle, addAngle, turnToPoint, operator_new, operator_delete
bool CharacterRoute::lookAtPoint(const float *point, int force)
{
    if (force != 0 && move == nullptr && kSixtyFiveDeg < fabsf(turnTo(owner, point)))
        turnToPoint(0.0f, point);
    setLook(new RouteLookPoint(owner, point));
    return true;
}

// FS2 0x0022db80
// callcheck: addAngle, turnToYaw, operator_new, operator_delete
bool CharacterRoute::lookAtYaw(float yaw, float pitch, int force)
{
    if (force != 0 && move == nullptr && kSixtyFiveDeg < fabsf(yaw)) {
        turnToYaw(addAngle(yaw, owner->m_xform.rotation[2]), 0.0f);
        yaw = 0.0f;
    }
    setLook(new RouteLook(owner, yaw, pitch));
    return true;
}

// FS2 0x0022dc60
// callcheck: operator_delete
bool CharacterRoute::stopLook()
{
    setLook(nullptr);
    return true;
}

// FS2 0x0022dca0
// callcheck: isMode1, atan2Octant, addAngle, addAngle, operator_new, operator_new, getSeconds, operator_delete
bool CharacterRoute::turnToObject(float rate, StageObject *object)
{
    if (object == nullptr || owner->isMode1())
        return false;
    const float yaw = turnTo(owner, object->m_xform.position);
    if (rate < kOneDegree)
        rate = kOneDegree;
    if (rate <= fabsf(yaw))
        setMove(new RouteMoveTurn(owner, this, new RouteTurnObject(owner, object)));
    return true;
}

// FS2 0x0022de30
// callcheck: isMode1, atan2Octant, addAngle, addAngle, operator_new, operator_new, getSeconds, operator_delete
bool CharacterRoute::turnToPoint(float rate, const float *point)
{
    if (owner->isMode1())
        return false;
    const float yaw = turnTo(owner, point);
    if (rate < kOneDegree)
        rate = kOneDegree;
    if (rate <= fabsf(yaw))
        setMove(new RouteMoveTurn(owner, this, new RouteTurnPoint(owner, point)));
    return true;
}

// FS2 0x0022dfc0
// callcheck: isMode1, addAngle, operator_new, operator_new, getSeconds, operator_delete
bool CharacterRoute::turnToYaw(float yaw, float rate)
{
    if (owner->isMode1())
        return false;
    const float delta = addAngle(yaw, -owner->m_xform.rotation[2]);
    if (rate < kOneDegree)
        rate = kOneDegree;
    if (rate <= fabsf(delta))
        setMove(new RouteMoveTurn(owner, this, new RouteTurn(owner, yaw)));
    return true;
}

// FS2 0x0022e110
bool CharacterRoute::getMoveGoal(float *out)
{
    return move != nullptr && move->getGoal(out);
}

// FS2 0x0022e140
// callcheck: getSeatPlace, isMode1, vecDistance, getCenter, isMode1, operator_new, RouteWalk, operator_delete, operator_new, operator_delete (objectPlace, hasReached)
bool CharacterRoute::moveToObject(float reach, StageObject *object, int mode, u_int flags)
{
    if (object == nullptr)
        return false;
    if (object->getKind() == StageObject_Character && object->m_gear == nullptr) {
        const float touching = owner->m_ground->nearRadius + object->m_ground->nearRadius + 1.0f;
        if (reach < touching)
            reach = touching;
    }
    if (reach < kMinReach && (flags & RouteMoveFlag_Exact) == 0)
        reach = kMinReach;

    sceVu0FVECTOR goal;
    objectPlace(object, goal, owner);
    if (hasReached(owner, goal, reach)) {
        cancelMove();
        return true;
    }
    if (mode != 0)
        moveMode = mode;
    if (owner->isMode1()) {
        setMove(new RouteMoveCar(owner, this, nullptr, object, reach));
    } else {
        if (moveMode == 3)
            flags |= RouteMoveFlag_Straight;
        setMove(new RouteWalk(reach, owner, this, filter, object, flags));
    }
    return true;
}

// FS2 0x0022e4a0
// callcheck: isMode1, vecDistance, getCenter, isMode1, operator_new, RouteWalk, operator_delete, operator_new, operator_delete (hasReached)
bool CharacterRoute::moveToPoint(float reach, const float *point, int mode, u_int flags)
{
    if (reach < kMinReach && (flags & RouteMoveFlag_Exact) == 0)
        reach = kMinReach;
    if (hasReached(owner, point, reach)) {
        cancelMove();
        return true;
    }
    if (mode != 0)
        moveMode = mode;
    if (owner->isMode1()) {
        setMove(new RouteMoveCar(owner, this, point, nullptr, reach));
    } else {
        if (moveMode == 3)
            flags |= RouteMoveFlag_Straight;
        setMove(new RouteWalk(reach, owner, this, filter, point, flags));
    }
    return true;
}

// FS2 0x0022e6f0
// callcheck: operator_delete
bool CharacterRoute::cancelMove()
{
    setMove(nullptr);
    return true;
}

// FS2 0x0022e740
// callcheck: getLifeState, getLifeState, getSeconds, getStageSelect, getBiteTarget, vecDistance, refresh, playWeaponPickSound, refresh, playWeaponPickSound, getStageSelect, setPositionXyz, operator_delete, isMode1, operator_delete, operator_delete (stageObjRecord)
void CharacterRoute::update()
{
    constexpr float kWeaponSwapRange = 200.0f;

    if (skipUpdate != 0) {
        skipUpdate = 0;
        return;
    }

    const int life = owner->getLifeState();
    if (!(1 < life && life < 6))
        return;

    if (owner->m_clock->getSeconds() < readyUntil)
        owner->m_wants.ready = 1;

    const int kind = stageObjRecord(owner->m_itemDef)->kind;
    if (kind == CharacterKind_A) {
        // Far off the gun, close the short weapon, with a second one in hand.
        if (owner->m_allows.standing != 0) {
            StageObject *weapon = owner->m_weapons[0];
            StageObject *target = owner->getBiteTarget();
            if (target != nullptr) {
                const bool far = kWeaponSwapRange <= vecDistance(owner->m_xform.position, target->m_xform.position);
                const bool gun = weapon != nullptr && isFirearm(weapon);
                if (far ? !gun : weapon == nullptr || gun) {
                    if (owner->m_weapons[1] != nullptr) {
                        owner->m_wants.putAway = 1;
                        owner->m_motion->refresh();
                        owner->playWeaponPickSound();
                    }
                }
            }
        }
    } else if (kind == CharacterKind_NoShadow) {
        if (isMovingMotion(owner->m_motion->motionId))
            owner->m_xform.setPositionXyz(owner->m_xform.position[0], owner->m_xform.position[1], kFlyingHeight);
    }

    if (action != nullptr && !action->update())
        setAction(nullptr);

    if (move == nullptr) {
        if (owner->isMode1()) {
            StageCar *car = static_cast<StageCar *>(owner->m_gear);
            car->m_throttle = 0;
            car->m_brake = 0xff;
        }
    } else if (!move->update()) {
        setMove(nullptr);
    }

    if (look != nullptr && !look->update())
        setLook(nullptr);
}

// The MSL std::vector instances of the file, out of line in the ROM after the
// code above; the host <vector> provides them.
//   FS2 0x0022fc50  vector<short>::constructAt (one element, placement)
//   FS2 0x0022fc60  vector<short>::erase (pos)
//   FS2 0x0022fda0  vector<short>::~vector
//   FS2 0x0022fe10  vector<RouteLeg>::~vector
//   FS2 0x0022fe80  vector<short>::clear
//   FS2 0x0022fe90  vector<RouteLeg>::clear
