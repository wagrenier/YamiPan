/* ========================================================================
 *  game/characterroute.cpp -- how a brain's commands reach the character.
 *
 *  game/characterroute.h has the shape. The look 0x21efa0.., the steps of a
 *  move 0x21f610.., the moves 0x221e70.. (a turn, a car, the walk 0x223080),
 *  the actions 0x224da0.., the route 0x22a310..0x22f270 and the deleting
 *  destructors and trivial slots ..0x22fc50. The ROM's inlined comparisons
 *  of the motion id are the predicates of game/playermotion.h here.
 *
 *  Left in src/lib/unassigned_stubs.cpp: the actions with their own task
 *  classes 0x2251f0..0x229ec0 (the header marks them), and the path search
 *  behind RouteFilter.
 * ======================================================================== */
#include "game/characterroute.h"

#include <math.h>
#include <string.h>

#include "game/characterbrain.h"
#include "game/characterground.h"
#include "game/characterinput.h"
#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/playermotion.h"
#include "game/routeplayer.h"
#include "game/stagedoor.h"
#include "game/stageitem.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
#include "game/sysdata.h"
#include "game/vu0math.h"

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
// callcheck: getSeconds, vecDistance, isListedArea, pathLength, findPath, advance, operator_new, getSeconds, junctionNode, junctionNode, operator_new, getSeconds, currentNode, vecDot, clearPath, atan2Octant, addAngle, addAngle, addAngle, startWalkTo, getDeltaSeconds, getTurnRate, operator_new, getSeconds, getStageSelect, FUN_0022fc50, elevatorDoorPlace, elevatorOpenAtB1, elevatorOpenAtB3, getZs9901, isOnElevator (the rest in updatePath, useLedge, useDoors, useElevator, avoid; FUN_0022fc50 wraps a short)
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
