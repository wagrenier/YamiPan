/* ========================================================================
 *  game/characterbrain.cpp -- what drives a character nobody controls.
 *
 *  0x1a68b0..0x1a7a20 the forwarding runner, 0x1d85b0..0x1dec30 the brain:
 *  its commands, its slots of the runner's table, the ask* family the
 *  character calls, the slots' switch, the wait and the name search.
 *  game/characterbrain.h has the shape; CharacterRoute is not written.
 *
 *  MSL's std::sort over the search's pairs, not written (std::sort here):
 *    FS2 0x001de2f0  the quicksort, its pivots stirred by gp-0x7ea8
 *    FS2 0x001de520  median of three
 *    FS2 0x001de660  partition: below the pivot
 *    FS2 0x001de8d0  partition: not above it
 *    FS2 0x001deb80  selection sort, under 0x15 items
 * ======================================================================== */
#include "game/characterbrain.h"

#include <string.h>

#include <algorithm>
#include <vector>

#include "game/characterroute.h"
#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/stagealert.h"
#include "game/mathutil.h"
#include "game/playermotion.h"
#include "game/stageitem.h"
#include "game/stageobjlist.h"
#include "game/stageresult.h"
#include "game/stagescene.h"
#include "game/sysdata.h"

// The commands: runner call words 0x02000000 | id.
enum BrainCommand
{
    BrainCmd_WalkToName         = 0x19,
    BrainCmd_TurnToAngle        = 0x1a,
    BrainCmd_TurnLeft           = 0x1b,
    BrainCmd_TurnRight          = 0x1c,
    BrainCmd_Wait               = 0x1d,
    BrainCmd_WalkToPoint        = 0x1e,
    BrainCmd_TurnToName         = 0x1f,
    BrainCmd_AttackFoeRange     = 0x20,
    BrainCmd_ActOnName          = 0x21,
    BrainCmd_ActOnFoe           = 0x22,
    BrainCmd_PlayAction         = 0x23,
    BrainCmd_Skip24             = 0x24,
    BrainCmd_CloseDoor          = 0x25,
    BrainCmd_OpenDoor           = 0x26,
    BrainCmd_Action27           = 0x27,
    BrainCmd_SetValue           = 0x28,
    BrainCmd_End                = 0x29,
    BrainCmd_WalkToNameFlags    = 0x2a,
    BrainCmd_WalkToPointFlags   = 0x2b,
    BrainCmd_TurnToNameFlags    = 0x2c,
    BrainCmd_TurnToAngleFlags   = 0x2d,
    BrainCmd_ActFlags           = 0x2e,
    BrainCmd_LookAtNameFlags    = 0x2f,
    BrainCmd_SetState           = 0x30,
    BrainCmd_WalkToNameFor      = 0x31,
    BrainCmd_WalkToPointFor     = 0x32,
    BrainCmd_WalkToNameFlagsFor = 0x33,
    BrainCmd_WalkToPointFlagsFor = 0x34,
    BrainCmd_Action35           = 0x35,
    BrainCmd_Stop               = 0x36,
    BrainCmd_LookAtName         = 0x37,
    BrainCmd_LookAtAngle        = 0x38,
    BrainCmd_Action39           = 0x39,
    BrainCmd_WalkFlags          = 0x3a,
    BrainCmd_WalkFlagsFor       = 0x3b,
    BrainCmd_TurnFlags          = 0x3c,
    BrainCmd_LookAtYaw          = 0x3d,
    BrainCmd_LookAtYawBack      = 0x3e,
    BrainCmd_ApproachName       = 0x3f,
    BrainCmd_AttackFoe          = 0x40,
    BrainCmd_ApproachPoint      = 0x41,
    BrainCmd_Next               = 0x42,
    BrainCmd_WalkFlagsThen      = 0x43,
    BrainCmd_WalkToNameThen     = 0x44,
    BrainCmd_WalkToNameFlagsThen = 0x45,
    BrainCmd_WalkToPointThen    = 0x46,
    BrainCmd_WalkToPointFlagsThen = 0x47,
    BrainCmd_AttackFoeThen      = 0x48,
    BrainCmd_AttackFoeRangeThen = 0x49,
    BrainCmd_PlayActionOn       = 0x4a,
    BrainCmd_LookAtPoint        = 0x4b,
    BrainCmd_ApproachNameWithin = 0x4c,
    BrainCmd_ApproachPointWithin = 0x4d,
    BrainCmd_ActOnId            = 0x4e,
    BrainCmd_AttackFoe2         = 0x4f,
    BrainCmd_AttackFoe2Range    = 0x50,
    BrainCmd_AttackFoe2Then     = 0x51,
    BrainCmd_AttackFoe2RangeThen = 0x52,
    BrainCmd_Action53           = 0x53,
};

constexpr float kBrainDegToRad     = 0x1.1df46ap-6f;    // 0x3c8efa35
constexpr float kBrainViewAngle    = 0x1.0c1524p+0f;    // 0x3f860a92: 60 degrees
constexpr float kApproachDistance  = 500.0f;
constexpr float kFoeAlertSeconds   = 5.0f;
constexpr int   kPropRunnerClass   = 0x40;  // getProperty answers kRunnerClassBrain
constexpr int   kRunnerClassBrain  = 4;
constexpr int   kPropBrainState    = 9;
constexpr u_int kCharacterClassEnemyBits = 0xc; // getCharacterClass: the foe's
constexpr int   kBrainSlotSearch   = 2;
constexpr int   kBrainNoiseCrouch  = 3;
constexpr int   kItemKindKind11    = CharacterKind_NoShadow;
constexpr int   kNumbSeconds   = 10;

static const char kItemTrap[]  = "I_WANA";      // data 0x304f20
static const char kItemSmoke[] = "I_HATUEN";    // data 0x304f28
static const char kCrouch1[]   = "cr_1";        // data 0x304f38
static const char kCrouch3[]   = "cr_3";        // data 0x304f40

// What a stage object must be called for useNumLock: sdata 0x31a9c0 (gp-0x7eb0).
static const char *const kNumLockSpecials[] = { "NumLockOpen", "NumLockClose" };

// ──────────────────────────────────────────────────────────────────────
// ScriptProxy: each slot asks the object or the target and passes on.

// FS2 0x001a68b0
// callcheck: ~ScriptRunner, operator_delete (deleting)

ScriptProxy::~ScriptProxy()
{
}

// FS2 0x001cf130

int ScriptProxy::getProperty(int id, int sub)
{
    if (id == kPropRunnerClass)
        return kRunnerClassBrain;
    return getForwardObject()->getProperty(id, sub);
}

// FS2 0x001cf0b0

void ScriptProxy::setProperty(int id, int sub, int value)
{
    if (id != kPropRunnerClass)
        getForwardObject()->setProperty(id, sub, value);
}

// FS2 0x001a6a90

float ScriptProxy::scriptGetPosX()
{
    return getForwardTarget()->scriptGetPosX();
}

// FS2 0x001a6ad0

float ScriptProxy::scriptGetPosY()
{
    return getForwardTarget()->scriptGetPosY();
}

// FS2 0x001a6b10

float ScriptProxy::scriptGetPosZ()
{
    return getForwardTarget()->scriptGetPosZ();
}

// FS2 0x001a6b90

void ScriptProxy::scriptSetPosition(const float *pos)
{
    getForwardObject()->scriptSetPosition(pos);
}

// FS2 0x001a6bd0

void ScriptProxy::scriptSetRotation(const float *rot)
{
    getForwardObject()->scriptSetRotation(rot);
}

// FS2 0x001a6c10

void ScriptProxy::scriptSetYaw(const float *yaw)
{
    getForwardObject()->scriptSetYaw(yaw);
}

// FS2 0x001a6c50

void ScriptProxy::cmdSetColor(const float *color)
{
    getForwardObject()->cmdSetColor(color);
}

// FS2 0x001a6c90

void ScriptProxy::cmdAddColorKey(int frame, const float *color)
{
    getForwardObject()->cmdAddColorKey(frame, color);
}

// FS2 0x001a6ce0

void ScriptProxy::cmdSetColorLoop(u_char loop)
{
    getForwardObject()->cmdSetColorLoop(loop);
}

// FS2 0x001a6d20

void ScriptProxy::cmdSetRadiusSquared(float radius)
{
    getForwardObject()->cmdSetRadiusSquared(radius);
}

// FS2 0x001a6d60

void ScriptProxy::cmdSetConeAngle(int angle)
{
    getForwardObject()->cmdSetConeAngle(angle);
}

// FS2 0x001a6da0

void ScriptProxy::cmdSetTreeSwitchedOn(bool on)
{
    getForwardObject()->cmdSetTreeSwitchedOn(on);
}

// FS2 0x001a6de0

void ScriptProxy::cmdSetTreeBroken(bool on)
{
    getForwardObject()->cmdSetTreeBroken(on);
}

// FS2 0x001a6e20

void ScriptProxy::cmdSetFadeFrames(int frames)
{
    getForwardObject()->cmdSetFadeFrames(frames);
}

// FS2 0x001a6e60

void ScriptProxy::cmdSetFlag240(bool on)
{
    getForwardObject()->cmdSetFlag240(on);
}

// FS2 0x001a6ea0

void ScriptProxy::cmdSetValue244(int value)
{
    getForwardObject()->cmdSetValue244(value);
}

// FS2 0x001a6ee0

void ScriptProxy::cmdSetFlag248(bool on)
{
    getForwardObject()->cmdSetFlag248(on);
}

// FS2 0x001a6f20

void ScriptProxy::cmdSetColor250(const float *color)
{
    getForwardObject()->cmdSetColor250(color);
}

// FS2 0x001a6f60

void ScriptProxy::cmdSetColor260(const float *color)
{
    getForwardObject()->cmdSetColor260(color);
}

// FS2 0x001a6fa0

void ScriptProxy::cmdSetCastsShadow(bool on)
{
    getForwardObject()->cmdSetCastsShadow(on);
}

// FS2 0x001a6fe0

void ScriptProxy::cmdSetEnabled(bool on)
{
    getForwardObject()->cmdSetEnabled(on);
}

// FS2 0x001a7020

void ScriptProxy::command34(void *arg)
{
    getForwardObject()->command34(arg);
}

// FS2 0x001a6960

StageCharacter *ScriptProxy::getScriptCharacter()
{
    return getForwardObject()->getScriptCharacter();
}

// FS2 0x001a6a10

StageDoor *ScriptProxy::getScriptDoor()
{
    return getForwardObject()->getScriptDoor();
}

// FS2 0x001a6a50

StageObject *ScriptProxy::getScriptObject()
{
    return getForwardObject()->getScriptObject();
}

// FS2 0x001a7270

float ScriptProxy::scriptDistanceTo(StageObject *other)
{
    return getForwardTarget()->scriptDistanceTo(other);
}

// FS2 0x001a6920

StageCharacter *ScriptProxy::getScriptActor()
{
    return getForwardObject()->getScriptActor();
}

// FS2 0x001a69d0
// callcheck: getForwardObject, getScriptMapObject

StageObject *ScriptProxy::getScriptMapObject()
{
    return getForwardObject()->getScriptMapObject();
}

// FS2 0x001a7890
// callcheck: getForwardObject, scriptPickableHandle

int ScriptProxy::scriptPickableHandle()
{
    return getForwardObject()->scriptPickableHandle();
}

// FS2 0x001a71b0
// callcheck: getForwardObject, scriptHasAllyWithin

bool ScriptProxy::scriptHasAllyWithin(float range)
{
    return getForwardObject()->scriptHasAllyWithin(range);
}

// FS2 0x001a7130

void ScriptProxy::scriptMarkPlace()
{
    getForwardObject()->scriptMarkPlace();
}

// FS2 0x001a70e0

bool ScriptProxy::scriptCanSee(float viewAngle, StageObject *other)
{
    return getForwardTarget()->scriptCanSee(viewAngle, other);
}

// FS2 0x001a72b0

float ScriptProxy::command59(void *arg)
{
    return getForwardTarget()->command59(arg);
}

// FS2 0x001a73a0

bool ScriptProxy::scriptIsFacing(StageObject *other)
{
    return getForwardObject()->scriptIsFacing(other);
}

// FS2 0x001a74a0

bool ScriptProxy::scriptHasLineTo(StageCharacter *other)
{
    return getForwardTarget()->scriptHasLineTo(other);
}

// FS2 0x001a7850

void ScriptProxy::scriptPutWeaponAway()
{
    getForwardObject()->scriptPutWeaponAway();
}

// FS2 0x001a76c0

void ScriptProxy::scriptPlayMotion(const char *name)
{
    getForwardObject()->scriptPlayMotion(name);
}

// FS2 0x001a7750

int ScriptProxy::command80()
{
    return getForwardObject()->command80();
}

// FS2 0x001a72f0

void ScriptProxy::scriptAddMenuLine(const u_short *text, int kind, bool plain)
{
    getForwardObject()->scriptAddMenuLine(text, kind, plain);
}

// FS2 0x001a75a0

bool ScriptProxy::scriptIsCrouching()
{
    return getForwardTarget()->scriptIsCrouching();
}

// FS2 0x001a74e0

bool ScriptProxy::scriptIsCompanion()
{
    return getForwardObject()->scriptIsCompanion();
}

// FS2 0x001a70a0

int ScriptProxy::scriptHeardKind()
{
    return getForwardTarget()->scriptHeardKind();
}

// FS2 0x001a78d0

bool ScriptProxy::cmdIsLit()
{
    return getForwardTarget()->cmdIsLit();
}

// FS2 0x001a7910

int ScriptProxy::command87()
{
    return getForwardObject()->command87();
}

// FS2 0x001a7950

void ScriptProxy::command88()
{
    getForwardObject()->command88();
}

// FS2 0x001a7990

bool ScriptProxy::command99()
{
    return getForwardTarget()->command99();
}

// FS2 0x001a79d0

int ScriptProxy::command101(void *arg)
{
    return getForwardObject()->command101(arg);
}

// ──────────────────────────────────────────────────────────────────────
// The sub-script and a slot.

// FS2 0x001dd7f0
// callcheck: ~ScriptRunner, operator_delete (deleting)

BrainSubRunner::~BrainSubRunner()
{
}

// FS2 0x001de290

const char *BrainSubRunner::getName()
{
    return getForwardTarget()->getName();
}

// FS2 0x001dd860
// callcheck: ScriptFrame

BrainFrame::BrainFrame()
{
}

// FS2 0x001dd610
// callcheck: release, operator_delete (deleting)

BrainFrame::~BrainFrame()
{
}

// ──────────────────────────────────────────────────────────────────────
// The wait.

// flags: s the character's point +0x6e0 (nothing to do while it has none),
// t the foe it would bite; else the named object, else the angle.
// FS2 0x001dd8a0
// callcheck: strchr, strchr, getBiteTarget, getBiteTarget, lookAtObject, lookAtPoint, findAny, lookAtObject, lookAtYaw

bool BrainWait::lookAt(const char *name, int angle, const char *flags, int force)
{
    bool done = false;
    if (flags != nullptr) {
        StageCharacter *owner = brain->m_owner;
        if (strchr(flags, 's') != nullptr) {
            if (!owner->m_point6e0.isSet)
                return false;
            route->lookAtPoint(owner->m_point6e0.point, force);
            done = true;
        } else if (strchr(flags, 't') != nullptr) {
            if (brain->m_owner->getBiteTarget() == nullptr)
                return false;
            route->lookAtObject(0.0f, brain->m_owner->getBiteTarget(), force);
            done = true;
        }
    }
    if (done)
        return true;
    if (name != nullptr) {
        StageObject *object = gStageScene->objects->findAny(name, 0);
        if (object == nullptr)
            return false;
        route->lookAtObject(0.0f, object, force);
        return true;
    }
    route->lookAtYaw((float)angle * kBrainDegToRad, 0.0f, force);
    return true;
}

// flags: ? the point +0x6e0, t the foe; else as lookAt.
// FS2 0x001dda10
// callcheck: strchr, strchr, getBiteTarget, getBiteTarget, turnToObject, turnToPoint, findAny, turnToObject, turnToYaw

bool BrainWait::turnTo(const char *name, int angle, const char *flags)
{
    bool done = false;
    if (flags != nullptr) {
        StageCharacter *owner = brain->m_owner;
        if (strchr(flags, '?') != nullptr) {
            if (!owner->m_point6e0.isSet)
                return false;
            route->turnToPoint(0.0f, owner->m_point6e0.point);
            done = true;
        } else if (strchr(flags, 't') != nullptr) {
            if (brain->m_owner->getBiteTarget() == nullptr)
                return false;
            route->turnToObject(0.0f, brain->m_owner->getBiteTarget());
            done = true;
        }
    }
    if (done)
        return true;
    if (name != nullptr) {
        StageObject *object = gStageScene->objects->findAny(name, 0);
        if (object == nullptr)
            return false;
        route->turnToObject(0.0f, object);
        return true;
    }
    route->turnToYaw((float)angle * kBrainDegToRad, 0.0f);
    return true;
}

// Which object of that name, nearest first: nth of them, the last when there
// are fewer; straight measures through walls, not along the route. Characters
// first, by name alone.
// FS2 0x001de050
// callcheck: findCharacter, findAll6, pathLength, distanceTo, FUN_001de2f0 (std::sort)

static StageObject *findNearest(const char *name, StageCharacter *from, u_int nth, bool straight)
{
    // layout: host -- the ROM's pairs of pointer and float on its stack.
    struct Candidate
    {
        StageObject *object;
        float        distance;
    };

    StageObject *object = gStageScene->objects->findCharacter(name, 0);
    if (object != nullptr)
        return object;
    StageObjArray *all = gStageScene->objects->findAll6(name);
    if (all == nullptr)
        return nullptr;

    StageObject **slot = all->begin();
    while (slot != nullptr && !(*slot)->m_active)
        slot = StageObjArray::next(slot);
    if (all->m_count == 1)
        return slot != nullptr ? *slot : nullptr;   // PC: the ROM reads through the null

    std::vector<Candidate> found;
    while (slot != nullptr) {
        StageObject *candidate = *slot;
        float distance;
        if (straight)
            distance = from->distanceTo(candidate);
        else
            distance = from->m_brain->getRoute()->filter->pathLength(from->m_xform.position, candidate->m_xform.position);
        found.push_back({ candidate, distance });
        slot = StageObjArray::next(slot);
        while (slot != nullptr && !(*slot)->m_active)
            slot = StageObjArray::next(slot);
    }
    if (found.empty())
        return nullptr;
    if (found.size() == 1)
        return found[0].object;
    std::sort(found.begin(), found.end(),
              [](const Candidate &a, const Candidate &b) { return a.distance < b.distance; });
    return nth < found.size() ? found[nth].object : found.back().object;
}

// flags: r runs, b the third mode, _ route action 0x22ad60 first; l 1, d 4,
// k 0xc, a 2, 1..6 0x20..0x400, 7 0x1000 are the move's flags; n, o, g choose
// which of the named (findNearest); ? the point +0x6e0, s the same and waits
// for it to come in sight, t the foe. Else the named object, else point.
// FS2 0x001ddb70
// callcheck: strchr, moveToPoint, getBestFoe, moveToObject, findNearest, action22ad60

bool BrainWait::moveTo(const char *name, const int *point, const char *flags)
{
    int   mode = 1;
    u_int moveFlags = 0;
    bool  straight = true;
    u_int nth = 1;

    if (flags != nullptr) {
        int base = strchr(flags, 'r') != nullptr ? 2 : 1;
        mode = strchr(flags, 'b') != nullptr ? 3 : base;
        if (strchr(flags, '_') != nullptr)
            route->action22ad60();
        moveFlags = strchr(flags, 'l') != nullptr ? 1 : 0;
        const bool hasN = strchr(flags, 'n') != nullptr;
        const bool hasO = strchr(flags, 'o') != nullptr;
        const bool hasG = strchr(flags, 'g') != nullptr;
        straight = !hasG && !hasO;
        nth = (hasG || (!hasO && !hasN)) ? 1 : 0;
        if (strchr(flags, 'd') != nullptr) moveFlags |= 0x4;
        if (strchr(flags, 'k') != nullptr) moveFlags |= 0xc;
        if (strchr(flags, 'a') != nullptr) moveFlags |= 0x2;
        if (strchr(flags, '1') != nullptr) moveFlags |= 0x20;
        if (strchr(flags, '2') != nullptr) moveFlags |= 0x40;
        if (strchr(flags, '3') != nullptr) moveFlags |= 0x80;
        if (strchr(flags, '4') != nullptr) moveFlags |= 0x100;
        if (strchr(flags, '5') != nullptr) moveFlags |= 0x200;
        if (strchr(flags, '6') != nullptr) moveFlags |= 0x400;
        if (strchr(flags, '7') != nullptr) moveFlags |= 0x1000;
    }

    bool done = false;
    if (flags != nullptr) {
        StageCharacter *owner = brain->m_owner;
        if (strchr(flags, '?') != nullptr) {
            if (!owner->m_point6e0.isSet)
                return false;
            route->moveToPoint(0.0f, owner->m_point6e0.point, mode, moveFlags);
            done = true;
        } else if (strchr(flags, 's') != nullptr) {
            if (!owner->m_point6e0.isSet)
                return false;
            route->moveToPoint(0.0f, owner->m_point6e0.point, mode, moveFlags);
            done = true;
            kind = BrainWait_InSight;
        } else if (strchr(flags, 't') != nullptr) {
            StageObject *foe = brain->m_owner->getBestFoe();
            if (foe == nullptr)
                return false;
            route->moveToObject(0.0f, foe, mode, moveFlags);
            done = true;
        }
    }
    if (done)
        return true;

    if (name != nullptr) {
        StageObject *object = findNearest(name, brain->m_owner, nth, straight);
        if (object == nullptr)
            return false;
        route->moveToObject(0.0f, object, mode, moveFlags);
        return true;
    }
    if (point == nullptr)
        return false;
    sceVu0FVECTOR target = { (float)point[0], (float)point[1], (float)point[2], 1.0f };
    route->moveToPoint(0.0f, target, mode, moveFlags);
    return true;
}

// FS2 0x001ddf20
// callcheck: getMoveGoal, isPointInView, cancelMove

bool BrainWait::isDone()
{
    StageCharacter *owner = brain->m_owner;
    switch (kind) {
    case BrainWait_RouteIdle:
        return route->isIdle();
    case BrainWait_None:
    case BrainWait_Done4:
    case BrainWait_Motion:
        return true;
    case BrainWait_InSight: {
        alignas(16) float goal[8];
        if (!route->getMoveGoal(goal))
            return true;
        float distance = 0.0f;
        if (owner->isPointInView(kBrainViewAngle, 1.0f, goal, &distance, nullptr)) {
            route->cancelMove();
            return true;
        }
        return false;
    }
    case BrainWait_Frames:
        if (frames == 0)
            return true;
        frames--;
        return false;
    default:
        return false;
    }
}

bool BrainWait::attackFoe(float range, bool mode)
{
    StageObject *foe = brain->m_owner->getBiteTarget();
    return foe != nullptr && route->attack(range, foe, mode);
}

// ──────────────────────────────────────────────────────────────────────
// Inline in the ROM.

bool CharacterBrain::isOverlaySlot(int slot)
{
    return slot == 7 || slot == 5 || slot == 3 || slot == 1;
}

bool CharacterBrain::isKeptSlot(int slot)
{
    return slot == 2 || slot == 0 || isOverlaySlot(slot);
}

void CharacterBrain::saveSlot()
{
    BrainFrame &frame = m_frames[m_slot];
    frame.saveFrom(this);
    frame.look = m_look;
}

void CharacterBrain::loadSlot()
{
    const BrainFrame &frame = m_frames[m_slot];
    frame.restoreTo(this);
    m_look = frame.look;
}

// The look the script asked for, which the route lost in the switch.
void CharacterBrain::restartLook()
{
    switch (m_look.kind) {
    case BrainLook_Yaw:
        m_route->lookAtYaw(m_look.arg.yaw, 0.0f, 0);
        break;
    case BrainLook_Point:
        m_route->lookAtPoint(m_look.arg.point, 1);
        break;
    case BrainLook_Named:
        m_wait->lookAt(m_look.arg.name, 0, m_look.flags);
        break;
    case BrainLook_Angle:
        m_wait->lookAt(nullptr, m_look.arg.angle, m_look.flags);
        break;
    case BrainLook_None:
        m_route->stopLook();
        break;
    }
}

void CharacterBrain::stopRoute()
{
    m_wait->route->stop();
    m_wait->kind = BrainWait_RouteIdle;
    m_wait->frames = 0;
}

void CharacterBrain::retryCommand()
{
    memcpy(m_stack, m_retryStack, sizeof(m_retryStack));
    m_sp = m_retrySp;
    m_pc = m_retryPc;
    m_retryValid = 0;
}

bool CharacterBrain::retryIf(bool started)
{
    if (started) {
        m_subRunner = nullptr;
        if (m_retryValid && m_slot != kBrainNoSlot)
            retryCommand();
    }
    return started;
}

// The running slot empty: down to the next used one. PC: a brain with no slot
// reads the byte before m_slotUsed in the ROM, m_slot's own top byte, 0xff.
void CharacterBrain::fallToUsedSlot()
{
    if (m_slot == kBrainNoSlot || m_slotUsed[m_slot])
        return;
    int slot = m_slot;
    do {
        slot--;
        if (slot < 0)
            return;
    } while (!m_slotUsed[slot]);
    m_slot = slot;
    loadSlot();
    m_retryValid = 0;
}

void CharacterBrain::waitFrames(int frames30)
{
    m_wait->frames = (u_int)((float)frames30 / m_owner->m_clock->getDeltaSeconds() / 30.0f);
    m_wait->kind = BrainWait_Frames;
}

void CharacterBrain::waitIf(bool started)
{
    if (!started)
        m_wait->kind = BrainWait_None;
}

void CharacterBrain::startSubScript(int entry)
{
    m_subRunner = &m_sub;
    m_subRunner->setEntry(gStageScene->scripts[1]->entry(entry));
}

// ──────────────────────────────────────────────────────────────────────
// The slots.

// Saves the running slot and makes slot run. Going up keeps what is under it;
// going down drops the state's slots passed on the way, stopping at an
// overlay still used above slot.
// FS2 0x001dccb0
// callcheck: saveFrom, clear, setEntry, restoreTo, saveFrom, clear, restoreTo

void CharacterBrain::switchSlot(int slot)
{
    if (m_slot == slot) {
        if (m_slot == kBrainNoSlot || m_slotUsed[slot])
            return;
    } else if (m_slot < slot) {
        if (m_slot != kBrainNoSlot) {
            m_subRunner = nullptr;
            if (m_retryValid && m_slot != kBrainNoSlot)
                retryCommand();
            saveSlot();
        }
        if (!isKeptSlot(slot))
            m_frames[slot].clear();
    } else {
        saveSlot();
        int top = slot;
        for (int i = m_slot; i > slot; i--) {
            if (isKeptSlot(i)) {
                if (slot != kBrainNoSlot && isOverlaySlot(i) && m_slotUsed[i] && top < i)
                    top = i;
            } else {
                m_slotUsed[i] = 0;
                m_frames[i].clear();
            }
        }
        if (slot != top) {
            m_slotUsed[slot] = 1;
            slot = top;
        }
        if (m_slot == slot && m_slotUsed[slot])
            return;
    }

    m_retryValid = 0;
    m_slot = slot;
    if (m_slot == kBrainNoSlot) {
        m_locals = nullptr;
        setEntry(nullptr);
    } else {
        m_slotUsed[slot] = 1;
        loadSlot();
    }
    fallToUsedSlot();
}

// State 7: puts the running script aside and remembers where to come back to
// (revive); nothing runs meanwhile.
// FS2 0x001dca60
// callcheck: saveFrom, clear, getBrainPace, getBrainPace, setEntry

void CharacterBrain::suspend()
{
    int resumeSlot = kBrainNoSlot;
    int resumeState = BrainState_Normal;

    if (m_slot != kBrainNoSlot)
        saveSlot();
    for (int i = m_slot; i >= 0; i--) {
        if (!m_slotUsed[i])
            continue;
        if (isKeptSlot(i)) {
            if (resumeSlot == kBrainNoSlot)
                resumeSlot = i;
            if (!isOverlaySlot(i) && i == kBrainSlotSearch)
                resumeState = BrainState_Slot2;
        } else {
            m_slotUsed[i] = 0;
            m_frames[i].clear();
        }
    }
    if (m_owner->getBrainPace() != BrainState_Off && m_owner->getBrainPace() != BrainState_Down
        && resumeSlot == kBrainNoSlot)
        resumeSlot = 0;

    m_resumeSlot = resumeSlot;
    m_resumeState = resumeState;
    m_slot = kBrainNoSlot;
    m_locals = nullptr;
    setEntry(nullptr);
}

// Each state has its slot; state 5 is the moment the foe was seen, and of the
// player or partner bitten gets the alarm once in five seconds: the party's
// call or the screen's flash, by the option.
// FS2 0x001dbca0
// callcheck: switchSlot, sayFound, getCharacterClass, getBiteTarget, getBiteTarget, getBiteTarget, getStageResult, getStageSelect, currentMission, getSeconds, getSeconds, currentMission, getGameOptions, warnAttacker, flash, suspend, stop, lookAtYaw, lookAtPoint, lookAt, stopLook, run

void CharacterBrain::setState(int newState, int runNow)
{
    switch (newState) {
    case BrainState_Off:
        switchSlot(kBrainNoSlot);
        break;
    case BrainState_Normal:
        switchSlot(0);
        break;
    case BrainState_Slot4a:
    case BrainState_Slot4b:
    case BrainState_Slot4c:
        switchSlot(4);
        break;
    case BrainState_Foe: {
        m_owner->sayFound();
        StageObject *player = gStageScene->player;
        StageObject *partner = gStageScene->partner;
        if ((m_owner->getCharacterClass() & kCharacterClassEnemyBits) != 0
            && (m_owner->getBiteTarget() == player || m_owner->getBiteTarget() == partner)) {
            StageCharacter *attacker = m_owner;
            StagePlayer *watched = gStageScene->player;
            if (attacker->getBiteTarget() == watched)
                getStageResult()->counter[1]++;
            if (watched->m_290 != attacker && attacker->m_68c < 1 && !watched->m_motion->scopeOn) {
                const int kind = stageObjRecord(m_owner->m_itemDef)->kind;
                if (kind != CharacterKind_Srt && kind != CharacterKind_Yrt) {
                    auto *mission = gStageScene->currentMission();
                    if (mission->lastFoeAlert < gStageScene->clock->getSeconds() - kFoeAlertSeconds) {
                        gStageScene->currentMission()->lastFoeAlert = gStageScene->clock->getSeconds();
                        GameOptions *options = getGameOptions();
                        if (options->alertMode == GameAlert_Party)
                            gStageScene->party->warnAttacker(attacker);
                        else if (options->alertMode == GameAlert_Flash)
                            gStageScene->alert->flash();
                    }
                }
            }
        }
        switchSlot(6);
        break;
    }
    case BrainState_Slot2:
        switchSlot(kBrainSlotSearch);
        break;
    case BrainState_Down:
        suspend();
        break;
    }
    m_state = newState;

    stopRoute();
    restartLook();
    if (runNow != 0)
        run(0);
}

// FS2 0x001dc060
// callcheck: setSlotEntry

void CharacterBrain::clearAllSlots()
{
    for (int slot = 0; slot < kBrainSlots; slot++)
        setSlotEntry(slot, nullptr, -1);
}

// FS2 0x001d85d0
// callcheck: clearAllSlots

void CharacterBrain::resetScripts()
{
    clearAllSlots();
    if (m_state != BrainState_Down && m_state != BrainState_Off) {
        m_state = BrainState_Normal;
        m_slot = 0;
    }
}

// FS2 0x001dc0d0
// callcheck: prepare, clear, stop, restoreTo, stopLook, lookAtYaw, lookAtPoint, lookAt, restoreTo

void CharacterBrain::setSlotEntry(int slot, const ScriptEntry *entry, int id)
{
    if (slot == kBrainNoSlot)
        return;
    BrainFrame &frame = m_frames[slot];
    frame.prepare(entry);
    frame.overlayId = id;
    frame.look.kind = BrainLook_None;
    frame.look.flags = "";
    frame.clear();
    if (slot == m_slot) {
        stopRoute();
        loadSlot();
        restartLook();
        m_retryValid = 0;
    }
    if (entry == nullptr) {
        m_slotUsed[slot] = 0;
        fallToUsedSlot();
    }
}

// FS2 0x001dc440
// callcheck: setSlotEntry, switchSlot, restartLook, lookAtYaw, lookAtPoint, lookAt, stopLook

void CharacterBrain::endOverlay()
{
    const int top = m_slot;
    if (!isOverlaySlot(top))
        return;
    int below = top;
    do {
        below--;
        if (below < 0) {
            below = top != kBrainNoSlot ? 0 : kBrainNoSlot;
            break;
        }
    } while (!m_slotUsed[below]);
    setSlotEntry(top, nullptr, -1);
    switchSlot(below);
    restartLook();
}

// FS2 0x001dc330
// callcheck: endOverlay, setSlotEntry

void CharacterBrain::endOverlayById(int id)
{
    for (int slot = m_slot; slot >= 0; slot--) {
        if (isOverlaySlot(slot) && m_slotUsed[slot] && m_frames[slot].overlayId == id) {
            if (slot == m_slot)
                endOverlay();
            else
                setSlotEntry(slot, nullptr, -1);
        }
    }
}

// Id 1 replaces what runs, any other first ends its own earlier call. Without
// an overlay running, the state says which slot takes it; none in states 0, 7.
// FS2 0x001dc5f0
// callcheck: endOverlayById, setSlotEntry, switchSlot, stop, lookAtYaw, lookAtPoint, lookAt, stopLook, run

bool CharacterBrain::startOverlay(int id, const ScriptEntry *entry)
{
    constexpr int kOverlayIdReplace = 1;
    const bool had = isOverlaySlot(m_slot);
    if (id != kOverlayIdReplace)
        endOverlayById(id);

    int slot = m_slot;
    if (!isOverlaySlot(slot)) {
        switch (m_state) {
        case BrainState_Normal:
            slot = 1;
            break;
        case BrainState_Slot4a:
        case BrainState_Slot4b:
        case BrainState_Slot4c:
            slot = 5;
            break;
        case BrainState_Foe:
            slot = 7;
            break;
        case BrainState_Slot2:
            slot = 3;
            break;
        default:
            return had;
        }
    }
    setSlotEntry(slot, entry, id);
    switchSlot(slot);
    stopRoute();
    restartLook();
    run(0);
    return had;
}

// FS2 0x001dc840

bool CharacterBrain::hasOverlay(int id) const
{
    for (int slot = m_slot; slot >= 0; slot--) {
        if (isOverlaySlot(slot) && m_slotUsed[slot] && m_frames[slot].overlayId == id)
            return true;
    }
    return false;
}

// FS2 0x001dc900

int CharacterBrain::overlayMark() const
{
    return isOverlaySlot(m_slot) ? 1 : -1;
}

// FS2 0x001dc950
// callcheck: setState, switchSlot, stopLook, lookAtYaw, lookAtPoint, lookAt

void CharacterBrain::revive()
{
    setState(m_resumeState, 0);
    if (m_resumeSlot != m_slot)
        switchSlot(m_resumeSlot);
    restartLook();
    m_resumeSlot = kBrainNoSlot;
    m_resumeState = BrainState_Down;
}

// FS2 0x001dd180
// callcheck: clear, restoreTo

void CharacterBrain::dropSlots(bool all)
{
    int keep = kBrainNoSlot;
    for (int slot = kBrainSlots - 1; slot >= 0; slot--) {
        if (!m_slotUsed[slot])
            continue;
        if (all || !isOverlaySlot(slot)) {
            m_slotUsed[slot] = 0;
            m_frames[slot].clear();
        } else {
            keep = slot;
        }
    }
    if (keep == kBrainNoSlot)
        keep = 0;
    m_state = BrainState_Normal;
    m_slot = keep;
    m_slotUsed[0] = 1;
    m_retryValid = 0;
    loadSlot();
}

// ──────────────────────────────────────────────────────────────────────
// The object.

// callcheck: ScriptRunner, ScriptRunner, operator_new, CharacterRoute, operator_new, __construct_array, prepare, stop
// FS2 0x001dd680

CharacterBrain::CharacterBrain(StageCharacter *owner) : m_owner(owner)
{
    m_route = new CharacterRoute(this);
    m_wait = new BrainWait(this, m_route);
    m_enabled = 1;
    m_sub.setScriptParent(this);
    reset();
}

// FS2 0x001dd510
// callcheck: ~CharacterRoute, operator_delete, __destroy_arr, ~ScriptRunner, ~ScriptRunner, operator_delete (members and bases)

CharacterBrain::~CharacterBrain()
{
    delete m_route;
    delete m_wait;
    m_locals = nullptr;
}

// FS2 0x001dd460
// callcheck: prepare, stop

void CharacterBrain::reset()
{
    m_slot = kBrainNoSlot;
    m_state = BrainState_Off;
    m_retryValid = 0;
    m_look.kind = BrainLook_None;
    for (int slot = 0; slot < kBrainSlots; slot++) {
        m_slotUsed[slot] = 0;
        BrainFrame &frame = m_frames[slot];
        frame.prepare(nullptr);
        frame.overlayId = -1;
        frame.look.kind = BrainLook_None;
        frame.look.flags = "";
    }
    stopRoute();
    m_subRunner = nullptr;
    m_resumeState = BrainState_Down;
    m_resumeSlot = kBrainNoSlot;
}

// Tests the scene first, so the ROM's walk reaches a character without a
// brain on a stage without a route.
// FS2 0x001dd330
// callcheck: isHeld7a0, isDone, run, run

void CharacterBrain::update()
{
    if (gStageScene->route == nullptr || !m_enabled || m_owner->m_placedThisFrame)
        return;
    if (m_wait->isDone()) {
        m_subRunner = nullptr;
        run(0);
    } else if (m_subRunner != nullptr && m_retryValid) {
        m_subRunner->run(0);
    }
}

// FS2 0x001dd2f0
// callcheck: update

void CharacterBrain::updateRoute()
{
    if (gStageScene->route != nullptr && m_enabled)
        m_route->update();
}

// FS2 0x001dd450

const char *CharacterBrain::getName()
{
    return m_owner->m_name;
}

// FS2 0x001daa70

int CharacterBrain::getProperty(int id, int sub)
{
    if (id == kPropRunnerClass)
        return kRunnerClassBrain;
    return m_owner->getProperty(id, sub);
}

// FS2 0x001daa10
// callcheck: setBrainState, setProperty

void CharacterBrain::setProperty(int id, int sub, int value)
{
    if (id == kPropRunnerClass)
        return;
    if (id == kPropBrainState)
        m_owner->setBrainState(value, 0);
    else
        m_owner->setProperty(id, sub, value);
}

// FS2 0x001da9d0

ScriptRunner *CharacterBrain::getForwardObject()
{
    return m_owner;
}

// FS2 0x001da9f0

ScriptRunner *CharacterBrain::getForwardTarget()
{
    return m_owner;
}

// FS2 0x001dd400
// callcheck: getDeltaSeconds

int CharacterBrain::msToFrames(int ms)
{
    return (int)((float)ms / 1000.0f / gStageScene->clock->getDeltaSeconds());
}

// ──────────────────────────────────────────────────────────────────────
// Commands. Each keeps a copy of itself first, so whatever interrupts it can
// run it again (retryCommand); those that take a script's time leave m_wait
// set, and BrainWait_None means carry on at the next word next frame.

// FS2 0x001d8630
// callcheck: getBiteTarget, readNameArg, popArgs, readStringArg, moveTo, turnTo, addAngle, turnToYaw, getDeltaSeconds, attack, findCharacter, followCharacter, findNearestFoe, backFromFoe, playAction, useDoor, quickTurn, setScriptValue, setBrainState, endOverlay, getBrainPace, clear, strchr, fleeFoe, fleeLight, backFromLight, lookAt, speak, stopLook, stop, restIn, lookAtYaw, find6, wanderNear, setEntry, lookAtPoint, findCharacterById, action22bf60

const int *CharacterBrain::runCommand(int id, const int *code)
{
    memcpy(m_retryStack, m_stack, sizeof(m_retryStack));
    m_retrySp = m_sp;
    m_retryPc = code - 1;
    m_retryValid = 1;
    m_wait->kind = BrainWait_RouteIdle;

    int args[4] = {};
    const char *name = nullptr;
    const char *flags = nullptr;

    switch (id) {
    case BrainCmd_WalkToName:
        code = readNameArg(&name, code);
        waitIf(m_wait->moveTo(name, nullptr, nullptr));
        break;
    case BrainCmd_TurnToAngle:
        popArgs(args, 1);
        waitIf(m_wait->turnTo(nullptr, args[0], nullptr));
        break;
    case BrainCmd_TurnLeft:
        popArgs(args, 1);
        waitIf(m_route->turnToYaw(addAngle(m_owner->m_xform.rotation[2], (float)args[0] * kBrainDegToRad), 0.0f));
        break;
    case BrainCmd_TurnRight:
        popArgs(args, 1);
        waitIf(m_route->turnToYaw(addAngle(m_owner->m_xform.rotation[2], (float)-args[0] * kBrainDegToRad), 0.0f));
        break;
    case BrainCmd_Wait:
        popArgs(args, 1);
        waitFrames(args[0]);
        break;
    case BrainCmd_WalkToPoint:
        popArgs(args, 3);
        waitIf(m_wait->moveTo(nullptr, args, nullptr));
        break;
    case BrainCmd_TurnToName:
        code = readNameArg(&name, code);
        waitIf(m_wait->turnTo(name, 0, nullptr));
        break;
    case BrainCmd_AttackFoeRange:
        popArgs(args, 1);
        waitIf(m_wait->attackFoe((float)args[0], false));
        break;
    case BrainCmd_ActOnName:
        code = readNameArg(&name, code);
        waitIf(m_route->followCharacter(gStageScene->objects->findCharacter(name, 0)));
        break;
    case BrainCmd_ActOnFoe:
        waitIf(m_route->backFromFoe(m_owner->findNearestFoe(true, false)));
        break;
    case BrainCmd_PlayAction:
        code = readNameArg(&name, code);
        waitIf(m_route->playAction(name, nullptr, nullptr));
        break;
    case BrainCmd_Skip24:
        popArgs(args, 1);
        break;
    case BrainCmd_CloseDoor:
        waitIf(m_route->useDoor(false));
        break;
    case BrainCmd_OpenDoor:
        waitIf(m_route->useDoor(true));
        break;
    case BrainCmd_Action27:
        waitIf(m_route->quickTurn());
        break;
    case BrainCmd_SetValue:
        popArgs(args, 2);
        m_owner->setScriptValue(args[0] - 1, args[1]);
        m_wait->kind = BrainWait_None;
        break;
    case BrainCmd_End:
        if (!isOverlaySlot(m_slot)) {
            const int was = m_slot;
            m_owner->setBrainState(BrainState_Normal, 1);
            if (was == kBrainSlotSearch) {
                m_slotUsed[kBrainSlotSearch] = 0;
                m_frames[kBrainSlotSearch].clear();
            }
        } else {
            endOverlay();
            m_owner->setBrainState(m_owner->getBrainPace(), 1);
        }
        return nullptr;
    case BrainCmd_WalkToNameFlags:
        code = readNameArg(&name, code);
        code = readStringArg(&flags, code);
        waitIf(m_wait->moveTo(name, nullptr, flags));
        break;
    case BrainCmd_WalkToPointFlags:
        popArgs(args, 3);
        code = readStringArg(&flags, code);
        waitIf(m_wait->moveTo(nullptr, args, flags));
        break;
    case BrainCmd_TurnToNameFlags:
        code = readNameArg(&name, code);
        code = readStringArg(&flags, code);
        waitIf(m_wait->turnTo(name, 0, flags));
        break;
    case BrainCmd_TurnToAngleFlags:
        // The other way round from its neighbours, in the ROM too.
        popArgs(args, 1);
        code = readStringArg(&flags, code);
        if (m_wait->turnTo(nullptr, args[0], flags))
            m_wait->kind = BrainWait_None;
        break;
    case BrainCmd_ActFlags:
        code = readStringArg(&flags, code);
        if (strchr(flags, 'l') == nullptr) {
            StageObject *foe = m_owner->findNearestFoe(true, false);
            if (strchr(flags, 'r') == nullptr)
                waitIf(m_route->backFromFoe(foe));
            else
                waitIf(m_route->fleeFoe(foe));
        } else if (strchr(flags, 'r') == nullptr) {
            waitIf(m_route->backFromLight());
        } else {
            waitIf(m_route->fleeLight());
        }
        break;
    case BrainCmd_LookAtNameFlags:
        code = readNameArg(&name, code);
        code = readStringArg(&flags, code);
        m_look.kind = BrainLook_Named;
        m_look.arg.name = name;
        m_look.flags = flags;
        m_wait->kind = BrainWait_None;
        waitIf(m_wait->lookAt(name, 0, flags));
        break;
    case BrainCmd_SetState: {
        popArgs(args, 1);
        if (isOverlaySlot(m_slot))
            endOverlay();
        const int was = m_slot;
        m_owner->setBrainState(args[0], 0);
        if (was == kBrainSlotSearch) {
            m_slotUsed[kBrainSlotSearch] = 0;
            m_frames[kBrainSlotSearch].clear();
        }
        return nullptr;
    }
    case BrainCmd_WalkToNameFor:
        popArgs(args, 1);
        code = readNameArg(&name, code);
        if (!m_wait->moveTo(name, nullptr, nullptr))
            m_wait->kind = BrainWait_None;
        else
            waitFrames(args[0]);
        break;
    case BrainCmd_WalkToPointFor:
        popArgs(args, 4);
        if (!m_wait->moveTo(nullptr, args, nullptr))
            m_wait->kind = BrainWait_None;
        else
            waitFrames(args[3]);
        break;
    case BrainCmd_WalkToNameFlagsFor:
        popArgs(args, 1);
        code = readNameArg(&name, code);
        code = readStringArg(&flags, code);
        if (!m_wait->moveTo(name, nullptr, flags))
            m_wait->kind = BrainWait_None;
        else
            waitFrames(args[0]);
        break;
    case BrainCmd_WalkToPointFlagsFor:
        popArgs(args, 4);
        code = readStringArg(&flags, code);
        if (!m_wait->moveTo(nullptr, args, flags))
            m_wait->kind = BrainWait_None;
        else
            waitFrames(args[3]);
        break;
    case BrainCmd_Action35:
        code = readNameArg(&name, code);
        m_route->speak(name);
        m_wait->kind = BrainWait_RouteIdle;
        break;
    case BrainCmd_Stop:
        m_look.kind = BrainLook_None;
        m_route->stopLook();
        m_route->stop();
        break;
    case BrainCmd_LookAtName:
        code = readNameArg(&name, code);
        m_look.kind = BrainLook_Named;
        m_look.arg.name = name;
        m_look.flags = nullptr;
        m_wait->kind = BrainWait_None;
        waitIf(m_wait->lookAt(name, 0, nullptr));
        break;
    case BrainCmd_LookAtAngle:
        popArgs(args, 1);
        m_look.kind = BrainLook_Angle;
        m_look.arg.angle = -args[0];
        m_look.flags = nullptr;
        m_wait->kind = BrainWait_None;
        waitIf(m_wait->lookAt(nullptr, -args[0], nullptr));
        break;
    case BrainCmd_Action39:
        popArgs(args, 1);
        waitIf(m_route->restIn(args[0]));
        break;
    case BrainCmd_WalkFlags:
        code = readStringArg(&flags, code);
        waitIf(m_wait->moveTo(nullptr, nullptr, flags));
        break;
    case BrainCmd_WalkFlagsFor:
        popArgs(args, 1);
        code = readStringArg(&flags, code);
        if (!m_wait->moveTo(nullptr, nullptr, flags))
            m_wait->kind = BrainWait_None;
        else
            waitFrames(args[0]);
        break;
    case BrainCmd_TurnFlags:
        // As BrainCmd_TurnToAngleFlags.
        code = readStringArg(&flags, code);
        if (m_wait->turnTo(nullptr, 0, flags))
            m_wait->kind = BrainWait_None;
        break;
    case BrainCmd_LookAtYaw:
    case BrainCmd_LookAtYawBack: {
        popArgs(args, 1);
        const float yaw = id == BrainCmd_LookAtYaw ? (float)args[0] * kBrainDegToRad : -((float)args[0] * kBrainDegToRad);
        m_look.kind = BrainLook_Yaw;
        m_look.arg.yaw = yaw;
        m_wait->kind = BrainWait_None;
        waitIf(m_route->lookAtYaw(yaw, 0.0f, 0));
        break;
    }
    case BrainCmd_ApproachName: {
        code = readNameArg(&name, code);
        StageObject *object = gStageScene->objects->find6(name, 0);
        if (object == nullptr || !m_route->wanderNear(kApproachDistance, object->m_xform.position))
            m_wait->kind = BrainWait_None;
        break;
    }
    case BrainCmd_AttackFoe:
        waitIf(m_wait->attackFoe(0.0f, false));
        break;
    case BrainCmd_ApproachPoint: {
        popArgs(args, 3);
        sceVu0FVECTOR point = { (float)args[0], (float)args[1], (float)args[2], 1.0f };
        waitIf(m_route->wanderNear(kApproachDistance, point));
        break;
    }
    case BrainCmd_Next:
        m_wait->kind = BrainWait_None;
        break;
    case BrainCmd_WalkFlagsThen: {
        code = readStringArg(&flags, code);
        const int entry = *code++;
        if (!m_wait->moveTo(nullptr, nullptr, flags))
            m_wait->kind = BrainWait_None;
        else
            startSubScript(entry);
        break;
    }
    case BrainCmd_WalkToNameThen: {
        code = readNameArg(&name, code);
        const int entry = *code++;
        if (!m_wait->moveTo(name, nullptr, nullptr))
            m_wait->kind = BrainWait_None;
        else
            startSubScript(entry);
        break;
    }
    case BrainCmd_WalkToNameFlagsThen: {
        code = readNameArg(&name, code);
        code = readStringArg(&flags, code);
        const int entry = *code++;
        if (!m_wait->moveTo(name, nullptr, flags))
            m_wait->kind = BrainWait_None;
        else
            startSubScript(entry);
        break;
    }
    case BrainCmd_WalkToPointThen: {
        popArgs(args, 3);
        const int entry = *code++;
        if (!m_wait->moveTo(nullptr, args, nullptr))
            m_wait->kind = BrainWait_None;
        else
            startSubScript(entry);
        break;
    }
    case BrainCmd_WalkToPointFlagsThen: {
        popArgs(args, 3);
        code = readStringArg(&flags, code);
        const int entry = *code++;
        if (!m_wait->moveTo(nullptr, args, flags))
            m_wait->kind = BrainWait_None;
        else
            startSubScript(entry);
        break;
    }
    case BrainCmd_AttackFoeThen: {
        const int entry = *code++;
        if (!m_wait->attackFoe(0.0f, false))
            m_wait->kind = BrainWait_None;
        else
            startSubScript(entry);
        break;
    }
    case BrainCmd_AttackFoeRangeThen: {
        const int entry = *code++;
        popArgs(args, 1);
        if (!m_wait->attackFoe((float)args[0], false))
            m_wait->kind = BrainWait_None;
        else
            startSubScript(entry);
        break;
    }
    case BrainCmd_PlayActionOn: {
        const char *firstName = nullptr;
        const char *secondName = nullptr;
        code = readNameArg(&name, code);
        code = readNameArg(&firstName, code);
        code = readNameArg(&secondName, code);
        auto *objects = gStageScene->objects;
        StageObject *first = objects->findCharacter(firstName, 0);
        if (first == nullptr)
            first = objects->find6(firstName, 0);
        StageObject *second = objects->findCharacter(secondName, 0);
        if (second == nullptr)
            second = objects->find6(secondName, 0);
        waitIf(m_route->playAction(name, first, second));
        break;
    }
    case BrainCmd_LookAtPoint: {
        popArgs(args, 3);
        // PC: the ROM leaves w as the stack had it.
        sceVu0FVECTOR point = { (float)args[0], (float)args[1], (float)args[2], 1.0f };
        m_look.kind = BrainLook_Point;
        memcpy(m_look.arg.point, point, sizeof(sceVu0FVECTOR));
        m_wait->kind = BrainWait_None;
        waitIf(m_route->lookAtPoint(point, 1));
        break;
    }
    case BrainCmd_ApproachNameWithin: {
        popArgs(args, 1);
        code = readNameArg(&name, code);
        StageObject *object = gStageScene->objects->find6(name, 0);
        if (object == nullptr || !m_route->wanderNear((float)args[0], object->m_xform.position))
            m_wait->kind = BrainWait_None;
        break;
    }
    case BrainCmd_ApproachPointWithin: {
        popArgs(args, 4);
        sceVu0FVECTOR point = { (float)args[0], (float)args[1], (float)args[2], 1.0f };
        waitIf(m_route->wanderNear((float)args[3], point));
        break;
    }
    case BrainCmd_ActOnId:
        popArgs(args, 1);
        waitIf(m_route->followCharacter(gStageScene->objects->findCharacterById(args[0])));
        break;
    case BrainCmd_AttackFoe2:
        waitIf(m_wait->attackFoe(0.0f, true));
        break;
    case BrainCmd_AttackFoe2Range:
        popArgs(args, 1);
        waitIf(m_wait->attackFoe((float)args[0], true));
        break;
    case BrainCmd_AttackFoe2Then: {
        const int entry = *code++;
        if (!m_wait->attackFoe(0.0f, true))
            m_wait->kind = BrainWait_None;
        else
            startSubScript(entry);
        break;
    }
    case BrainCmd_AttackFoe2RangeThen: {
        const int entry = *code++;
        popArgs(args, 1);
        if (!m_wait->attackFoe((float)args[0], true))
            m_wait->kind = BrainWait_None;
        else
            startSubScript(entry);
        break;
    }
    case BrainCmd_Action53:
        m_route->action22bf60();
        break;
    }
    return code;
}

// ──────────────────────────────────────────────────────────────────────
// The runner's slots.

// FS2 0x001d9f90

bool CharacterBrain::scriptOpenDoor()
{
    return m_owner->m_allows.doorHand == 1 && m_route->useDoor(true);
}

// FS2 0x001d9f50

bool CharacterBrain::scriptCloseDoor()
{
    return m_owner->m_allows.doorHand == 2 && m_route->useDoor(false);
}

// FS2 0x001d9ee0

bool CharacterBrain::scriptUseDoorKey(int lock)
{
    if (lock == 0) {
        if (m_owner->m_allows.doorKey == 2)
            return m_route->useDoorKey(false);
    } else if (m_owner->m_allows.doorKey == 1) {
        return m_route->useDoorKey(true);
    }
    return false;
}

// FS2 0x001da410

void CharacterBrain::scriptPlayMotion(const char *name)
{
    m_wait->kind = BrainWait_Motion;
    m_route->playAction(name, nullptr, nullptr);
}

// FS2 0x001d9ea0

bool CharacterBrain::routeAction130()
{
    return m_route->action22abf0();
}

// FS2 0x001d9ec0

bool CharacterBrain::routeAction12c()
{
    return m_route->action22ad60();
}

// FS2 0x001d9fc0

void CharacterBrain::walkToPoint700(bool runs)
{
    if (m_owner->m_point700.isSet)
        m_route->moveToPoint(0.0f, m_owner->m_point700.point, runs ? 2 : 1, 0);
}

// FS2 0x001da020

bool CharacterBrain::takeUpTrap()
{
    return m_owner->m_allows.trapCancel != 0 && m_route->takeUpTrap(0);
}

// FS2 0x001da050

bool CharacterBrain::putSmoke()
{
    return m_owner->putSmoke(false);
}

// FS2 0x001da060

bool CharacterBrain::setTrap()
{
    if (!m_owner->m_allows.trapSet)
        return false;
    return m_route->setTrap(m_owner->findCarried(kItemTrap));
}

// FS2 0x001da0b0

bool CharacterBrain::useItemAt(const char *name, const float *point)
{
    return m_owner->throwCarried(name, point, false);
}

// FS2 0x001da0c0

bool CharacterBrain::useNamedItem(const char *name)
{
    sceVu0FVECTOR zero = {};
    return m_owner->throwCarried(name, zero, false);
}

// FS2 0x001da110

bool CharacterBrain::useSmoke()
{
    sceVu0FVECTOR zero = {};
    return m_owner->throwCarried(kItemSmoke, zero, false);
}

// FS2 0x001da170
// callcheck: getMoveGoal, stop

bool CharacterBrain::stopIfMoving()
{
    sceVu0FVECTOR goal;
    return m_route->getMoveGoal(goal) && m_route->stop();
}

// FS2 0x001da1b0
// callcheck: stop, moveToPoint, moveToPoint

void CharacterBrain::walkToAndRetry(const float *point, bool runs)
{
    m_route->stop();
    retryIf(m_route->moveToPoint(0.0f, point, runs ? 2 : 1, 0));
}

// The special, when no action or move is running.
// FS2 0x001da2d0
// callcheck: isSpecialAllowed, setSpecial

bool CharacterBrain::scriptUseSpecial(const char *name)
{
    if (!m_route->isIdle() || !m_owner->isSpecialAllowed(name))
        return false;
    m_owner->setSpecial(true, name);
    return true;
}

// FS2 0x001da370
// callcheck: addNoise, playAction

bool CharacterBrain::scriptCrouch(float loudness)
{
    gStageScene->noises->addNoise(loudness, m_owner, kBrainNoiseCrouch);
    return m_route->playAction(kCrouch1, nullptr, nullptr);
}

// PC: as crouch1.
// FS2 0x001da3c0
// callcheck: addNoise, playAction

void CharacterBrain::crouch3()
{
    gStageScene->noises->addNoise(0.0f, m_owner, kBrainNoiseCrouch);
    m_route->playAction(kCrouch3, nullptr, nullptr);
}

// The object of the special's lock gives the gadget its text, and the special
// turns on.
// FS2 0x001da430
// callcheck: isSpecialAllowed, findNumLockTarget, FUN_001d1280, setText

bool CharacterBrain::useNumLock(int which)
{
    const char *special = kNumLockSpecials[which];
    if (!m_owner->isSpecialAllowed(special))
        return false;
    StageObject *lock = findNumLockTarget();
    // PC: the ROM reads +0x20 of a null lock: the zero page, an empty text.
    return m_owner->m_motion->armGadgetSpecial(special, lock != nullptr ? lock->m_name : "");
}

// FS2 0x001da4f0
// callcheck: getStageSelect, goNumb

bool CharacterBrain::routeActionKind11()
{
    if (stageObjRecord(m_owner->m_itemDef)->kind != kItemKindKind11)
        return false;
    return m_route->goNumb(kNumbSeconds);
}

// FS2 0x001da5d0

bool CharacterBrain::scriptDragOutCar()
{
    return m_route->dragOutCar();
}

// FS2 0x001da5e0
// callcheck: getKind, takeItem

StageObject *CharacterBrain::takeItem()
{
    StageObject *item = m_owner->m_pickItem;
    if (item == nullptr || item->getKind() != kStageObjectKindWeapon || !m_route->takeItem(item))
        return nullptr;
    return item;
}

// FS2 0x001da650

bool CharacterBrain::gunAction()
{
    return m_route->gunAction();
}

// frames30 of the game's thirtieths. flags: r running, b in time, else at once.
// FS2 0x001da660
// callcheck: strchr, strchr, hitInTime, hitRunning, hit

bool CharacterBrain::fireAt(StageObject *target, const char *flags, int frames30)
{
    char how = 'n';
    if (strchr(flags, 'r') != nullptr)
        how = 'r';
    else if (strchr(flags, 'b') != nullptr)
        how = 'b';

    if (how == 'b')
        return m_route->hitInTime((float)frames30 / 30.0f, target);
    if (how == 'r')
        return m_route->hitRunning((float)frames30 / 30.0f, target);
    return m_route->hit(target);
}

// FS2 0x001da7a0
// callcheck: getBiteTarget, strchr, strchr, hitInTime, hitRunning, hit

bool CharacterBrain::fireAtBite(const char *flags, int frames30)
{
    StageObject *target = m_owner->getBiteTarget();
    if (target == nullptr)
        return false;
    return fireAt(target, flags, frames30);
}

// FS2 0x001da8f0

bool CharacterBrain::swingAt(StageObject *target)
{
    return m_route->swingAt(target);
}

// FS2 0x001da910
// callcheck: getBiteTarget, swingAt

bool CharacterBrain::swingAtBite()
{
    StageObject *target = m_owner->getBiteTarget();
    return target != nullptr && m_route->swingAt(target);
}

// FS2 0x001da960
// callcheck: getBiteTarget, attackTask

bool CharacterBrain::attackBite()
{
    StageObject *target = m_owner->getBiteTarget();
    return target != nullptr && m_route->attackTask(target);
}

// FS2 0x001da9b0
// callcheck: tryShout

bool CharacterBrain::shout(float loudness)
{
    return m_owner->tryShout(loudness);
}

// FS2 0x001daab0
// callcheck: allowPath

void CharacterBrain::allowPath(const char *name, bool on)
{
    m_route->allowPath(name, on);
}

// FS2 0x001daac0
// callcheck: stop

void CharacterBrain::stopAndRetry()
{
    m_route->stop();
    retryIf(m_slot != kBrainNoSlot);
}

// FS2 0x001dab80
// callcheck: playAction

bool CharacterBrain::playActionAndRetry(const char *name)
{
    return retryIf(m_route->playAction(name, nullptr, nullptr));
}

// FS2 0x001dac50
// callcheck: getMoveGoal

bool CharacterBrain::getMoveGoal(float *out)
{
    return m_route->getMoveGoal(out);
}

// ──────────────────────────────────────────────────────────────────────
// The ask* family: the same, and a started action retries the waiting command.

// FS2 0x001dac60
// callcheck: useSpecial

bool CharacterBrain::askUseSpecial(const char *name)
{
    return retryIf(scriptUseSpecial(name));
}

// FS2 0x001dad30
// callcheck: useNumLock

bool CharacterBrain::askNumLock(int which)
{
    return retryIf(useNumLock(which));
}

// FS2 0x001dae00
// callcheck: routeActionKind11

bool CharacterBrain::askRouteActionKind11()
{
    return retryIf(routeActionKind11());
}

// FS2 0x001daed0
// callcheck: dragOutCar

bool CharacterBrain::askDragOutCar()
{
    return retryIf(scriptDragOutCar());
}

// FS2 0x001dafa0
// callcheck: takeItem

bool CharacterBrain::askTakeItem()
{
    return retryIf(takeItem() != nullptr);
}

// FS2 0x001db060
// callcheck: gunAction

bool CharacterBrain::askGunAction()
{
    return retryIf(gunAction());
}

// FS2 0x001db130
// callcheck: routeAction130

bool CharacterBrain::askRouteAction130()
{
    return retryIf(routeAction130());
}

// FS2 0x001db200
// callcheck: routeAction12c

bool CharacterBrain::askRouteAction12c()
{
    return retryIf(routeAction12c());
}

// FS2 0x001db2d0
// callcheck: scriptUseDoorKey

bool CharacterBrain::askDoorKey(int lock)
{
    return retryIf(scriptUseDoorKey(lock));
}

// FS2 0x001db3a0
// callcheck: scriptCloseDoor

bool CharacterBrain::askCloseDoor()
{
    return retryIf(scriptCloseDoor());
}

// FS2 0x001db470
// callcheck: scriptOpenDoor

bool CharacterBrain::askOpenDoor()
{
    return retryIf(scriptOpenDoor());
}

// FS2 0x001db540
// callcheck: takeUpTrap

bool CharacterBrain::askTakeUpTrap()
{
    return retryIf(takeUpTrap());
}

// FS2 0x001db610
// callcheck: setTrap

bool CharacterBrain::askSetTrap()
{
    return retryIf(setTrap());
}

// FS2 0x001db6f0
// callcheck: throwAt

bool CharacterBrain::askThrow(void *item, void *vector, bool retry)
{
    const bool started = m_route->throwAt(item, vector);
    if (retry)
        retryIf(started);
    return started;
}

// FS2 0x001db7c0
// callcheck: fireAt

bool CharacterBrain::askFireAt(StageObject *target, const char *flags, int frames30)
{
    return retryIf(fireAt(target, flags, frames30));
}

// FS2 0x001db890
// callcheck: fireAtBite

bool CharacterBrain::askFireAtBite(const char *flags, int frames30)
{
    return retryIf(fireAtBite(flags, frames30));
}

// FS2 0x001db960
// callcheck: swingAt

bool CharacterBrain::askSwingAt(StageObject *target)
{
    return retryIf(swingAt(target));
}

// FS2 0x001dba30
// callcheck: swingAtBite

bool CharacterBrain::askSwingAtBite()
{
    return retryIf(swingAtBite());
}

// FS2 0x001dbb00
// callcheck: attackBite

bool CharacterBrain::askAttackBite()
{
    return retryIf(attackBite());
}

// FS2 0x001dbbd0
// callcheck: shout

bool CharacterBrain::askShout(float loudness)
{
    return retryIf(shout(loudness));
}
