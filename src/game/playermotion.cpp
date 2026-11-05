/*
 *  game/playermotion.cpp -- what a character is doing with its body (FS2,
 *  0x1e0000..0x1f0000): the motion a character asks for, what it may become,
 *  and what plays. game/playermotion.h has the outline.
 *
 *  The ids of the sheet (gMotionDefs) are the game's own and stay numbers in
 *  the predicates, whose names say what the code does with them.
 */
#include "game/playermotion.h"
#include "game/stagedoor.h"

#include "game/boxgrid.h"

#include "game/characterground.h"
#include "game/characterinput.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "game/clock.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/packtables.h"
#include "game/scenepairs.h"
#include "game/stageitem.h"
#include "game/stagenode.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
#include "game/stagelight.h"
#include "game/stagesound.h"
#include "game/sysdata.h"
#include "game/vu0math.h"

constexpr int kMotionStanceSlots = 12;
constexpr int kMotionStances = 2;
typedef int MotionStanceTable[kMotionStanceSlots][kMotionStances];

// The motion of each slot, { standing, crouching }. Slot 0 is the idle.
static const MotionStanceTable kStanceTable = {     /* data 0x2f7dd0 */
    { 0, 29 }, { 3, 32 }, { 6, 33 }, { 7, 34 }, { 8, 35 }, { 9, 36 },
    { 11, 37 }, { 12, 38 }, { 16, 41 }, { 17, 42 }, { 22, 45 }, { 0, 0 },
};
// Kinds 0 and 1 while m_winded is set.
static const MotionStanceTable kStanceTableDown = { /* data 0x2f7e30 */
    { 1, 30 }, { 4, 32 }, { 6, 33 }, { 7, 34 }, { 8, 35 }, { 10, 36 },
    { 11, 37 }, { 12, 38 }, { 16, 41 }, { 17, 42 }, { 22, 45 }, { 0, 0 },
};
// Yri and Kne while m_stanceFlag is set.
static const MotionStanceTable kStanceTableAlt = {  /* data 0x2f7e90 */
    { 2, 31 }, { 5, 32 }, { 6, 33 }, { 7, 34 }, { 8, 35 }, { 9, 36 },
    { 13, 37 }, { 14, 38 }, { 16, 41 }, { 17, 42 }, { 22, 45 }, { 0, 0 },
};

// A kind's weapon class (+0x6c) and the flags at +0xb31..+0xb38: a switch in
// the ctor. layout: host
struct MotionKindRow
{
    int    weaponClass;
    u_char flags1, flags2, flags3, hands, face, flags6, flags7, flags8;
};

static const MotionKindRow *motionKindRow(int kind)
{
    static const MotionKindRow kHuman  = { 0, 1, 1, 1, 1, 1, 0, 1, 0 };
    static const MotionKindRow kArmed  = { 1, 1, 1, 1, 1, 1, 0, 1, 0 };
    static const MotionKindRow kBare   = { 2, 0, 0, 0, 0, 0, 0, 0, 0 };
    static const MotionKindRow kPair   = { 2, 1, 1, 0, 1, 1, 0, 1, 0 };
    static const MotionKindRow kHead6  = { 2, 1, 0, 0, 0, 1, 1, 0, 1 };
    static const MotionKindRow kKind7  = { 2, 0, 0, 0, 0, 0, 1, 0, 0 };
    static const MotionKindRow kFaceB  = { 2, 0, 0, 0, 0, 1, 0, 0, 0 }; // the ROM leaves +0xb37 alone
    static const MotionKindRow kFaceD  = { 2, 0, 0, 0, 0, 1, 1, 0, 0 };
    static const MotionKindRow kHeadF  = { 2, 1, 0, 0, 0, 0, 1, 0, 1 };
    switch (kind) {
    case 0: case 1: case 9: case 0xe: return &kHuman;
    case 2: case 4: case 8: case 0xa: return &kArmed;
    case 3: case 0x10:                return &kBare;
    case 5: case 0xc:                 return &kPair;
    case 6:                           return &kHead6;
    case 7:                           return &kKind7;
    case 0xb:                         return &kFaceB;
    case 0xd:                         return &kFaceD;
    case 0xf:                         return &kHeadF;
    }
    return nullptr;
}

static const MotionDef *motionDef(int id)
{
    return id < 0 || id >= kMotionDefCount ? nullptr : &gMotionDefs[id];
}

static const ItemDef *weaponDef(const StageObject *weapon)
{
    const int index = weapon->m_itemDef;
    return &gItemDefs[index < 0 || index >= kItemDefCount ? 0 : index];
}

// Which hands the file moves itself: bit 1 left, bit 0 right (a track named
// bn_?_finger.. with keys).
static u_int motionFileHands(const void *file)                           // FS2 0x002938d0
{
    if (file == nullptr)
        return 0;
    const PoseMotionFile *motion = static_cast<const PoseMotionFile *>(file);
    if ((motion->flags & 0x100) == 0)
        return 0;
    u_int hands = 0;
    const PoseMotionTrack *tracks = motion->tracks();
    for (int i = 0; i < motion->trackCount; i++) {
        if (strncmp(tracks[i].bone + 5, "finger", 6) == 0 && tracks[i].tracks != 0) {
            if (tracks[i].bone[3] == 'r')
                hands |= 1;
            else if (tracks[i].bone[3] == 'l')
                hands |= 2;
        }
    }
    return hands;
}

// The position in a track's last key.
static bool motionFileLastPosition(float *x, float *y, float *z, int index, const void *file) // FS2 0x002937e0
{
    if (file == nullptr)
        return false;
    const PoseMotionFile *motion = static_cast<const PoseMotionFile *>(file);
    if (index >= motion->trackCount)
        return false;
    const PoseMotionTrack *track = &motion->tracks()[index];
    const u_char bits = track->tracks;
    const float *keys = track->keys();
    int word = 0;
    float time;
    do {
        if (bits & 1) {
            *x = keys[word];
            *y = keys[word + 1];
            *z = keys[word + 2];
            word += 3;
        }
        if (bits & 2)
            word += 4;
        if (bits & 4)
            word += 3;
        if (bits & 8)
            word += 4;
        if (bits & 0x20)
            word += 1;
        time = keys[word++];
    } while (time >= 0.0f);
    return true;
}

bool isCrouchMotion(int id)
{
    switch (id) {
    case 0x1d: case 0x1e: case 0x1f: case 0x2b:
    case 0x20: case 0x21: case 0x22: case 0x23:
    case 0x24: case 0x2d: case 0x25: case 0x26:
    case 0x29: case 0x2a:
        return true;
    }
    return false;
}

bool isRunMotion(int id)
{
    return id == 0x1a || id == 0x19 || id == 0x16 || id == 10 || id == 9;
}

bool isCrouchRunMotion(int id)
{
    return id == 0x2d || id == 0x24;
}

bool isRestingMotion(int id)
{
    return id == 0xf8 || id == 0xf7 || id == 0xf6 || id == 0xf5 || id == 0xf4 || id == 2 || id == 1 || id == 0
        || id == Motion_Idle1 || id == Motion_Idle0
        || id == 0x1f || id == 0x1e || id == Motion_CrouchStand || id == Motion_CrouchIdle;
}

bool isCrouchWalkMotion(int id)
{
    return id == 0x20 || id == 0x21 || id == 0x23 || id == 0x22;
}

bool isLadderMotion(int id)
{
    return id == 0xae || (0x3a <= id && id <= 0x42);
}

bool isAttackMotion(int id)
{
    switch (id) {
    case 0x91: case 0x92: case 0x82: case 0x87: case 0x73: case 0x78:
    case 0x67: case 0x66: case 0x65: case 100: case 99: case 0x62: case 0x61: case 0x60: case 0x5f:
    case 0x5e: case 0x5d: case 0x5c: case 0x5b: case 0x5a: case 0x59:
        return true;
    }
    return false;
}

bool isWalkMotion(int id)
{
    switch (id) {
    case 0x2c: case 0x18: case 0x17: case 5: case 4: case 3:
    case 0x8a: case 0x7b: case 0x6c: case 0x51:
    case 6: case 0x8b: case 0x7c: case 0x6d: case 0x52:
    case 8: case 7: case 0x8d: case 0x8c: case 0x7e: case 0x7d: case 0x6f: case 0x6e: case 0x54: case 0x53:
    case 0x20: case 0x21: case 0x23: case 0x22:
        return true;
    }
    return false;
}

bool isMovingMotion(int id)
{
    switch (id) {
    case 0x2c: case 0x18: case 0x17: case 5: case 4: case 3:
    case 0x8a: case 0x7b: case 0x6c: case 0x51:
    case 6: case 0x8b: case 0x7c: case 0x6d: case 0x52:
    case 8: case 7: case 0x8d: case 0x8c: case 0x7e: case 0x7d: case 0x6f: case 0x6e: case 0x54: case 0x53:
    case 0x1a: case 0x19: case 0x16: case 10: case 9:
    case 0xf: case 0xe: case 0xd: case 0xc: case 0xb:
    case 0x8f: case 0x8e: case 0x80: case 0x7f: case 0x71: case 0x70: case 0x58: case 0x57: case 0x56: case 0x55:
    case 0x20: case 0x21: case 0x23: case 0x22: case 0x2d: case 0x24: case 0x26: case 0x25:
        return true;
    }
    return false;
}

bool isHoldableMotion(int id)
{
    switch (id) {
    case 0xf8: case 0xf7: case 0xf6: case 0xf5: case 0xf4: case 2: case 1: case 0:
    case 0x15: case 0x14:
    case 0x1f: case 0x1e: case 0x1d: case 0x2b:
        return true;
    }
    return false;
}

bool isStandableMotion(int id)
{
    switch (id) {
    case 0xf8: case 0xf6: case 0xf5: case 0xf4: case 0xbc: case 0x9d:
    case 0x4c: case 0x4b: case 0x4a: case 0x49: case 0x48: case 0x47: case 0x46: case 0x45: case 0x44: case 0x43:
    case 0x3d: case 0x3a: case 0x39: case 0x38: case 0x37: case 0x36: case 0x35: case 0x34: case 0x33: case 0x32:
    case 0x31: case 0x30: case 0x2f: case 0x2e:
    case 0x13: case 0x12: case 0x15: case 0x14: case 0x10:
    case 0xc: case 0xb: case 10: case 9: case 8: case 7: case 6: case 4: case 3: case 1: case 0:
        return true;
    }
    return false;
}

bool isArmedLowMotion(int id)
{
    return id == 0x93 || id == 0x88 || id == 0x79 || id == 0x69 || id == 0x68;
}

bool isArmedMotion(int id)
{
    switch (id) {
    case 0x90: case 0x81: case 0x72: case 0x50: case 0x4f:
    case 0x8a: case 0x7b: case 0x6c: case 0x51:
    case 0x8b: case 0x7c: case 0x6d: case 0x52:
    case 0x8d: case 0x8c: case 0x7e: case 0x7d: case 0x6f: case 0x6e: case 0x54: case 0x53:
    case 0x8f: case 0x8e: case 0x80: case 0x7f: case 0x71: case 0x70: case 0x58: case 0x57: case 0x56: case 0x55:
    case 0x91: case 0x92: case 0x82: case 0x87: case 0x73: case 0x78:
    case 0x67: case 0x66: case 0x65: case 100: case 99: case 0x62: case 0x61: case 0x60: case 0x5f:
    case 0x5e: case 0x5d: case 0x5c: case 0x5b: case 0x5a: case 0x59:
        return true;
    }
    return isArmedLowMotion(id);
}

// FS2 0x001e81f0 is the args' copy, field by field in the ROM.
void MotionQueue::store(int request, const StageCharacter *owner, int current)
{
    motion = request;
    memcpy(args, &owner->m_allows, kArgsSize);
    from = current;
}

void MotionQueue::restore(StageCharacter *owner) const
{
    memcpy(&owner->m_allows, args, kArgsSize);
}

// FS2 0x0027cb60

void MotionLink::update()
{
    onUpdate();
    for (MotionCondition *condition : m_conditions) {
        if (condition->active != 0)
            condition->onUpdate();
    }
}

// FS2 0x0027cac0

void MotionLink::apply()
{
    onApply();
    for (MotionCondition *condition : m_conditions) {
        if (motion->m_request != Motion_None)
            break;
        if (condition->active != 0)
            condition->onApply();
    }
}

// FS2 0x0027da40
// callcheck: FUN_0027db60, FUN_0027dbf0, operator_new, FUN_0027dc20 (a std::vector here; FUN_001ef590 is the map's find)

void MotionLink::addCondition(const char *name)
{
    auto found = motion->m_conditions.find(name);
    if (found != motion->m_conditions.end())
        m_conditions.push_back(found->second);
}

constexpr u_int kMotionEventEndIdle = 0x4000;

// layout: host -- as MotionCondition.
// What runs whatever the motion is: the view, the zoom and the lamp, the
// head and the upper body, by the running motion's row of the sheet.
// EE vtable 0x317730.
class AlwaysCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x00275b60
    void onApply() override;                                             // FS2 0x00275160
};

// FS2 0x00275b60

void AlwaysCondition::onUpdate()
{
    u_char headSway = 0, upperSway = 0, viewSwitch = 0, zoom = 0, lamp = 0;
    const int id = motion->motionId;
    if (const MotionDef *def = motionDef(id)) {
        if (motion->m_b30 == 0) {
            bool mayEnd = character->isPlayer();
            if (!mayEnd && character->isPartner()
                && (id == Motion_CrouchIdle || id == Motion_Idle1 || id == Motion_Idle0))
                mayEnd = true;
            if (mayEnd && (character->m_model->m_motionEvents & kMotionEventEndIdle) != 0)
                motion->m_b30 = 1;
        }
        if (motion->m_b30 != 0)
            motion->walkLink->update();
        headSway = def->headSway;
        upperSway = def->upperSway;
        viewSwitch = def->cameraModeSwitch;
        zoom = def->firstPersonZoom;
        lamp = def->flashlightToggle;
    }
    CharacterAllows &allows = character->m_allows;
    if (character->isPlayer()) {
        if (motion->noFlash == 0 && viewSwitch != 0)
            allows.viewSwitch = 1;
        if (motion->noFlash == 0 && zoom != 0)
            allows.zoom = 1;
    }
    if (character->m_lampItem != nullptr && character->m_lampItem->m_active != 0 && lamp != 0)
        allows.lamp = 1;
    if (isHoldableMotion(id) || isMovingMotion(id))
        allows.standing = 1;
    if (headSway != 0 && character->m_290 == nullptr)
        allows.head = 1;
    if (motion->noFlash != 0)
        allows.aim = 1;
    else if (upperSway != 0 && character->m_290 == nullptr && !isAttackMotion(id))
        allows.aim = 1;
}

// FS2 0x00275160
// callcheck: isPartner, getStageSelect, FUN_001def60, FUN_001def90 (stageObjRecord; the partner's cr_2 needs the list of 0x1df0b0)

void AlwaysCondition::onApply()
{
    PlayerMotion *const m = motion;
    if (m->m_b30 != 0 && m->m_request == Motion_None) {
        m->m_canStart = 0;
        m->walkLink->apply();
        m->m_canStart = 1;
        if (m->m_request == m->pickStance(MotionStance_Idle))
            m->m_request = Motion_None;
        else
            m->startRequested();
    }

    const CharacterWants &wants = character->m_wants;
    const CharacterAllows &allows = character->m_allows;
    if (allows.viewSwitch != 0) {
        StageObject *camera = gStageScene->objects->find0("camview", 0);
        if (wants.viewChange == 1) {
            camera->m_linked = character;
            camera->onLinkChanged();
            gStageScene->pushView(camera, 0, 1);
        }
        else if (wants.viewChange == 2) {
            gStageScene->removeView(camera);
        }
    }
    if (allows.zoom != 0) {
        StageObject *camera = gStageScene->objects->find0("camview_zoom", 0);
        if (wants.zoomOn != 0) {
            camera->m_linked = character;
            camera->onLinkChanged();
            gStageScene->pushView(camera, 1, 1);
            static_cast<ViewCamera *>(camera)->setZoom((float)wants.zoomLevel);
        }
        else {
            gStageScene->removeView(camera);
        }
    }
    StageItemE *lampItem = character->m_lampItem;
    if (allows.lamp != 0 && wants.lampSwitch != 0)
        lampItem->m_lamp->setSwitchedOn(!lampItem->m_lamp->m_spot->isSwitchedOn());

    bool faceForward = false, swayToMatrix = false, boneLink = false, lampAim = false;
    if (const MotionDef *def = motionDef(m->motionId)) {
        if (def->headSway == 0 && def->upperSway == 0)
            faceForward = true;
        boneLink = def->headSwayBoneLink != 0;
        swayToMatrix = def->headSway2 != 0;
        if (stageObjRecord(character->m_itemDef)->kind == 0xb) {
            faceForward = true;
            swayToMatrix = true;
        }
        if (lampItem != nullptr && lampItem->m_active != 0 && lampItem->m_lamp->m_spot->isLit()
            && (lampItem->m_isTorch2 == 0 || character->isControlled()))
            lampAim = lampItem->m_isTorch2 != 0 || def->flashlightControl != 0;
    }

    if (character->m_290 == nullptr || isAttackMotion(m->motionId)) {
        sceVu0FVECTOR lamp = {};   // PC: getLampTarget leaves it without a lamp
        if (lampAim)
            m->getLampTarget(lamp);
        m->setToolTarget(lampAim ? lamp : nullptr);
    }

    if (faceForward) {
        sceVu0FVECTOR angles = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (swayToMatrix) {
            sceVu0FMATRIX sway;
            identityMatrix(sway);      // PC: getSwayMatrix is a stub
            character->getSwayMatrix(sway);
            matrixToRotYXZ(angles, sway);
            angles[0] = wrapAngle(angles[0] - character->m_xform.rotation[0]);
            angles[1] = 0.0f;
            angles[2] = wrapAngle(angles[2] - wrapAngle(character->m_xform.rotation[2] + 3.1415927f));
            angles[3] = 1.0f;
        }
        if (m->m_kindFlags1 != 0 || m->m_kindFlags2 != 0) {
            m->setHeadTarget(0.15f, nullptr);
            m->setLookTarget(0.15f, nullptr);
        }
        m->leanBody(angles, false, swayToMatrix);
    }

    if (allows.head != 0) {
        m->leanBody(wants.headAngles, character->isControlled() || wants.headOn == 0, 0);
        if (m->m_kindFlags1 != 0) {
            sceVu0FVECTOR point;
            const float *target;
            if (m->m_lookObject == nullptr) {
                m->probe->pointAhead(m->m_lampAngles[0], m->m_lampAngles[2], point);
                target = wants.headOn != 0 && boneLink ? point : nullptr;
            }
            else {
                applyMatrix(point, m->m_lookObject->getMatrix(), m->m_lookObject->m_pickPoint);
                target = boneLink ? point : nullptr;
            }
            m->setHeadTarget(0.15f, target);
            m->setLookTarget(0.15f, nullptr);
        }
    }

    if (allows.aim != 0) {
        const bool controlled = character->isControlled();
        const bool slope = !controlled && wants.aimOn == 0;
        if (m->noFlash == 0) {
            m->leanBody(wants.aimAngles, slope, 0);
        }
        else {
            m->leanBodyAt(1.0f, 3.1415927f, wants.aimAngles, slope, 0);
            m->m_turnStep[2] = addAngle(m->m_turnStep[2], wants.moveYaw);
        }
        if (m->m_kindFlags2 != 0) {
            sceVu0FVECTOR point;
            m->probe->pointAhead(m->m_lampAngles[0], m->m_lampAngles[2], point);
            m->setHeadTarget(0.15f, wants.aimOn != 0 && boneLink ? point : nullptr);
            m->setLookTarget(0.15f, wants.aimOn != 0 ? point : nullptr);
        }
    }
}

// layout: host -- as MotionCondition.
// QuickTurn: t_q. EE vtable 0x3174f0.
class QuickTurnCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x002702a0
    void onApply() override;                                             // FS2 0x00270260
};

// FS2 0x002702a0

void QuickTurnCondition::onUpdate()
{
    if (character->m_abilities.quickTurn != 0
        && (character->m_abilities.flag3c3 != 0 || character->m_290 == nullptr))
        character->m_allows.quickTurn = 1;
}

// FS2 0x00270260

void QuickTurnCondition::onApply()
{
    if (character->m_allows.quickTurn != 0 && character->m_wants.quickTurn != 0)
        motion->request(Motion_QuickTurn);
}

// layout: host -- as MotionCondition.
// Squat: down, up, or from a run into the other stance's. EE vtable 0x317710.
class SquatCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x002750f0
    void onApply() override;                                             // FS2 0x00274990
};

// FS2 0x002750f0

void SquatCondition::onUpdate()
{
    if (character->m_abilities.flag3a6 != 0
        && (character->m_abilities.flag3c3 != 0 || character->m_290 == nullptr)
        && !character->isTakenOver())
        character->m_allows.squat = 1;
}

// FS2 0x00274990

void SquatCondition::onApply()
{
    if (character->m_allows.squat == 0 || character->m_wants.squat == 0)
        return;
    const bool crouched = isCrouchMotion(motion->motionId);
    if (character->m_abilities.runStart != 0 && character->m_winded == 0) {
        if (motion->motionId == motion->pickStance(MotionStance_Run)
            || motion->motionId == motion->pickStance(MotionStance_WalkRun)) {
            motion->request(crouched ? 9 : 0x24);
            return;
        }
        if (motion->motionId == 0x20) {         // sqw
            motion->request(Motion_CrouchWalk);
            return;
        }
    }
    motion->request(crouched ? Motion_StandUp : Motion_CrouchDown);
}

// layout: host -- as MotionCondition.
// Turn: on the spot. EE vtable 0x3176b0.
class TurnCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x00273fa0
    void onApply() override;                                             // FS2 0x00273f10
};

// FS2 0x00273fa0

void TurnCondition::onUpdate()
{
    if (character->m_abilities.moveCrouched == 0) {
        StageObject *weapon = character->m_weapons[0];
        const bool rifle = weapon != nullptr && weaponDef(weapon)->isRifle();
        if (!rifle && isCrouchMotion(motion->motionId))
            return;
    }
    if (character->m_abilities.flag3c3 != 0 || character->m_290 == nullptr)
        character->m_allows.turn = 1;
}

// FS2 0x00273f10

void TurnCondition::onApply()
{
    if (character->m_allows.turn == 0)
        return;
    if (character->m_wants.moveKind == CharacterMove_TurnLeft)
        motion->request(motion->pickStance(MotionStance_TurnLeft));
    else if (character->m_wants.moveKind == CharacterMove_TurnRight)
        motion->request(motion->pickStance(MotionStance_TurnRight));
}

// layout: host -- as MotionCondition.
// Move: every way of walking, and last of all the idle: the end of the
// list, so this is who asks for it when nothing else did. EE vtable 0x317690.
class MoveCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x00273da0
    void onApply() override;                                             // FS2 0x00272d50
private:
    void applyForward();
    // The body's distance to the wall ahead is within reach.
    bool nearWall() const;
};

constexpr u_int kFaceWall     = 0xe8021f3;   // what walking into stops at
constexpr u_int kFaceLedge    = 0x140;       // a wall one can look over
constexpr u_int kFaceSolid    = 0xe002003;   // a wall one trips on
constexpr u_int kFaceStairs   = 8;
constexpr u_int kFaceFlagsTrip = 0x40000;

static u_int faceFlags(const StageCollisionFace *face)
{
    return face->flags;
}

// FS2 0x00273da0

void MoveCondition::onUpdate()
{
    if (character->m_abilities.moveCrouched == 0 && isCrouchMotion(motion->motionId))
        return;
    character->m_allows.move = 1;
}

bool MoveCondition::nearWall() const
{
    const float *wall = motion->walkLink->wallPoint;
    const float *position = character->m_xform.position;
    const float x = wall[0] - position[0];
    const float y = wall[1] - position[1];
    return sqrtf(x * x + y * y) < character->m_ground->radius + 12.5f;
}

void MoveCondition::applyForward()
{
    const CharacterWants &wants = character->m_wants;
    const float *const position = character->m_xform.position;
    const float yaw = character->m_xform.rotation[2];

    bool stopAtWall = false;    // bVar2
    bool stopAtEdge = false;    // bVar3
    bool trip = false;          // bVar4
    StageCollisionFace *wall = motion->walkLink->wallFace;
    if (wall != nullptr && (faceFlags(wall) & kFaceWall) != 0) {
        sceVu0FVECTOR normal;
        wall->getNormal(normal);
        const float facing = wrapAngle(atan2Octant(-normal[0], normal[1]) - wrapAngle(yaw + 3.1415927f));
        stopAtWall = 2.0943952f < fabsf(facing);
        const int kind = stageObjRecord(character->m_itemDef)->kind;
        if (stopAtWall && character->isPlayer() && (kind == 0 || kind == 1)
            && isRunMotion(motion->motionId)) {
            if ((faceFlags(wall) & kFaceLedge) == 0) {
                if ((faceFlags(wall) & kFaceSolid) != 0 && 5.0f < motion->m_runTime
                    && 1000.0f < motion->m_runDistance) {
                    sceVu0FVECTOR from = { position[0], position[1], position[2] + 125.0f, 1.0f };
                    BoneMap::Node *shoulder = character->m_model->boneMap.find("bn_clavicle!");
                    if (shoulder != nullptr && shoulder->value != nullptr)
                        from[2] = shoulder->value->getWorldMatrix()[3][2];
                    sceVu0FVECTOR to = { from[0], from[1], from[2], 1.0f };
                    const float reach = -(character->m_ground->radius + 25.0f);
                    to[0] += reach * sinCos(-yaw, 0);
                    to[1] += reach * sinCos(-yaw, 1);
                    if (gStageScene->segmentHitsAny(from, to, 0, kFaceSolid, kFaceFlagsTrip, nullptr)) {
                        stopAtWall = false;
                        trip = nearWall();
                    }
                }
            }
            else if (1.0f < motion->m_runTime && 200.0f < motion->m_runDistance) {
                const WalkLink *walk = motion->walkLink;
                bool blocked = walk->edgeBlocked != 0;
                if (!blocked && walk->edgeFound != 0) {
                    const int stepDown = stageObjRecord(character->m_itemDef)->stepDown;
                    if (stepDown == 1)
                        blocked = walk->edgeDrop <= -395.0f;
                    else if (stepDown == 0)
                        blocked = walk->edgeDrop <= -295.0f;
                }
                if (blocked) {
                    stopAtWall = false;
                    stopAtEdge = nearWall();
                }
            }
        }
    }

    int stairs = 0;             // 1 up, 2 down
    if (character->m_abilities.stairs != 0 && !isCrouchMotion(motion->motionId)) {
        StageCollisionFace *floor = character->m_ground->floorFace;
        if (floor != nullptr && (faceFlags(floor) & kFaceStairs) != 0) {
            sceVu0FVECTOR normal;
            floor->getNormal(normal);
            sceVu0FVECTOR downhill = { normal[0], normal[1], 0.0f, 0.0f };
            vecNormalize(downhill, downhill);
            sceVu0FVECTOR ahead = { -sinCos(-yaw, 0), -sinCos(-yaw, 1), 0.0f, 0.0f };
            stairs = 0.0f < vecDot(downhill, ahead) ? 2 : 1;
        }
    }

    const bool plainPace = character->m_abilities.runStart == 0 || character->m_winded != 0;
    const bool walking = wants.moveKind == CharacterMove_Walk || wants.moveKind == CharacterMove_WalkSlow
        || stopAtWall || stopAtEdge || trip || character->m_ground->narrow != 0;
    if (walking) {
        if (stairs != 0) {
            if (character->m_winded == 0)
                motion->request(stairs == 1 ? Motion_StairsUp : Motion_StairsDown);
            else
                motion->request(motion->pickStance(MotionStance_Walk));
        }
        else if (stopAtEdge) {
            motion->request(Motion_Ops);
            WalkLink *walk = motion->walkLink;
            motion->startWalkFile(0.35f, motion->requestedFile(), walk->edgePoint, walk->edgeAngles, true, true);
        }
        else if (trip) {
            motion->request(Motion_Wa);
            character->playerEvent("COLLISIONHIT");
        }
        else if (plainPace) {
            motion->request(motion->pickStance(MotionStance_Walk));
        }
        else if (motion->motionId == Motion_CrouchWalk && !motion->isEnding()) {
            motion->request(Motion_CrouchWalk);
        }
        else {
            motion->request(motion->pickStance(MotionStance_Walk));
        }
        return;
    }

    if (stairs != 0) {
        if (character->m_winded == 0)
            motion->request(stairs == 1 ? Motion_StairsRunUp : Motion_StairsRunDown);
        else
            motion->request(motion->pickStance(MotionStance_Run));
        return;
    }
    if (plainPace) {
        // A crouch that may not run walks (the ROM's two tests give this).
        if (isCrouchMotion(motion->motionId) && character->m_winded != 0)
            motion->request(motion->pickStance(MotionStance_Walk));
        else
            motion->request(motion->pickStance(MotionStance_Run));
        return;
    }
    const int walkRun = motion->pickStance(MotionStance_WalkRun);
    if (motion->motionId == walkRun) {
        motion->request(motion->pickStance(motion->isEnding() ? MotionStance_Run : MotionStance_WalkRun));
        return;
    }
    const int id = motion->motionId;
    if (isRunMotion(id) || id == 0x2d || id == 0x24)
        motion->request(motion->pickStance(MotionStance_Run));
    else
        motion->request(motion->pickStance(MotionStance_WalkRun));
}

// FS2 0x00272d50
// callcheck: getStageSelect, getStageSelect, addAngle, addAngle, atan2Octant, find, getNormal, isEnding, isPlayer, playerEvent, requestedFile, segmentHitsAny, sinCos, startWalkFile, vecDot, vecNormalize, wrapAngle (stageObjRecord; turnBy; the walking half is applyForward here)

void MoveCondition::onApply()
{
    const CharacterWants &wants = character->m_wants;
    const auto turnBy = [&] {
        motion->m_turnStep[2] = addAngle(motion->m_turnStep[2], wants.moveYaw);
    };
    bool asked = false;
    if (character->m_allows.move != 0) {
        switch (wants.moveKind) {
        case CharacterMove_StrafeLeft:
            if (character->m_abilities.strafe != 0) {
                motion->request(motion->pickStance(MotionStance_WalkLeft));
                asked = true;
            }
            break;
        case CharacterMove_StrafeRight:
            if (character->m_abilities.strafe != 0)
                motion->request(motion->pickStance(MotionStance_WalkRight));
            break;
        case CharacterMove_Back:
        case CharacterMove_BackTurn:
            if (character->m_abilities.walkBack != 0) {
                turnBy();
                motion->request(motion->pickStance(MotionStance_WalkBack));
            }
            break;
        case CharacterMove_Walk:
        case CharacterMove_Run:
        case CharacterMove_WalkSlow:
            turnBy();
            applyForward();
            break;
        }
    }
    (void)asked;
    if (motion->m_request == Motion_None) {
        if (!character->isControlled())
            turnBy();
        motion->request(motion->pickStance(MotionStance_Idle));
    }
}

// FS2 0x0026c4b0

StageCollisionFace *MotionProbe::faceAhead(sceVu0FVECTOR out, bool forward)
{
    const float *position = owner->m_xform.position;
    sceVu0FVECTOR from = { position[0], position[1], position[2] + 50.0f, position[3] };
    sceVu0FVECTOR to = { position[0], position[1], position[2] + 50.0f, position[3] };
    const float yaw = forward ? owner->m_xform.rotation[2] : addAngle(owner->m_xform.rotation[2], 3.1415927f);
    const float reach = -(owner->m_ground->radius + 25.0f);
    to[0] += reach * sinCos(-yaw, 0);
    to[1] += reach * sinCos(-yaw, 1);
    StageCollisionFace *face = gStageScene->raycast(out, from, to, 0, kFaceWall, 0x40000, nullptr);
    out[2] -= 50.0f;
    return face;
}

// A car's roof and the like: an edge on one is not a dead end.
static bool isVehicle(StageObject *object)
{
    return object != nullptr && object->getKind() == StageObject_Kind3;
}

// Neither side of the place is against a wall.
// FS2 0x0026c2d0
// callcheck: segmentHitsAny, segmentHitsAny, addAngle, addAngle, sinCos, sinCos, addAngle, addAngle, sinCos, sinCos

static bool sidesAreClear(const sceVu0FVECTOR place, float yaw, float reach)
{
    constexpr float kQuarterTurn = 1.5707964f;
    const sceVu0FVECTOR from = { place[0], place[1], place[2] + 50.0f, place[3] };
    for (const float side : { kQuarterTurn, -kQuarterTurn }) {
        const float angle = -addAngle(yaw, side);
        const sceVu0FVECTOR to = { from[0] + reach * sinCos(angle, 0), from[1] + reach * sinCos(angle, 1),
                                   from[2], from[3] };
        if (gStageScene->segmentHitsAny(from, to, 0, kFaceWall, 0, nullptr))
            return false;
    }
    return true;
}

// FS2 0x0026b960
// callcheck: BoundingSphere, addAngle, addAngle, addAngle, addAngle, addAngle, addAngle, addAngle, atan2Octant, atan2Octant, findFloor, getKind, getKind, getKind, getNormal, getNormal, raycastObject, segmentHitsAny, segmentHitsAny, segmentHitsAny, segmentHitsAny, sinCos, sinCos, sinCos, sinCos, sinCos, sinCos, sinCos, sinCos, sinCos, sinCos, sinCos, sinCos, sinCos, sinCos, sphereHitsAny, sphereHitsKind (the side tests twice over in the ROM: sidesAreClear; the sphere's two fields are its arguments here)

bool MotionProbe::findEdge(StageCollisionFace *face, StageObject **object, sceVu0FVECTOR target, float *drop,
                           StageObject **floor, sceVu0FVECTOR point, sceVu0FVECTOR angles, u_char *blocked)
{
    constexpr float kPi = 3.1415927f;
    constexpr float kEyeHeight = 50.0f;
    constexpr float kLookUp = 205.0f;
    constexpr float kLookDown = 1000.0f;
    constexpr float kOverEdge = 15.0f;
    constexpr u_int kFaceEdge = 0x142;
    constexpr u_int kFaceEdgeFinal = 2;     // an edge that ends the world for a walker
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    CharacterGround *ground = character->m_ground;
    if (blocked != nullptr)
        *blocked = 0;
    if (character->m_abilities.stepLevel == 0 || face == nullptr || (face->flags & kFaceEdge) == 0)
        return false;

    const float *position = owner->m_xform.position;
    const sceVu0FVECTOR from = { position[0], position[1], position[2] + kEyeHeight, position[3] };
    sceVu0FVECTOR normal;
    face->getNormal(normal);
    float yaw = atan2Octant(normal[0], -normal[1]);
    const float ahead = ground->radius + 25.0f;
    const sceVu0FVECTOR to = { from[0] + ahead * sinCos(-yaw, 0), from[1] + ahead * sinCos(-yaw, 1), from[2], from[3] };
    sceVu0FVECTOR hit;
    StageCollisionFace *edge = gStageScene->raycastObject(object, hit, from, to, 0, kFaceEdge, 0, nullptr);
    if (edge == nullptr)
        return false;
    hit[2] -= kEyeHeight;

    edge->getNormal(normal);
    yaw = atan2Octant(normal[0], -normal[1]);
    const float back = -yaw;
    sceVu0FVECTOR over = { hit[0] + ground->radius2 * sinCos(back, 0), hit[1] + ground->radius2 * sinCos(back, 1),
                           hit[2], hit[3] };
    memcpy(point, hit, sizeof(hit));
    angles[0] = 0.0f;
    angles[1] = 0.0f;
    angles[2] = addAngle(yaw, kPi);
    angles[3] = 0.0f;

    StageObject *floorObject = nullptr;     // PC: the ROM's is uninitialised without a hit
    const bool found = findFloor(kLookUp, kLookDown, over, drop, &floorObject);
    if (floor != nullptr)
        *floor = floorObject;
    if (!found) {
        if (blocked != nullptr && (edge->flags & kFaceEdgeFinal) == 0 && !isVehicle(ground->floorObject))
            *blocked = 1;
        return false;
    }

    memcpy(target, over, sizeof(over));
    target[2] += *drop;
    if (character->m_42c == 0)
        return sidesAreClear(target, yaw, ground->radius2);

    const bool onVehicle = isVehicle(ground->floorObject);
    if ((!onVehicle || !isVehicle(floorObject)) && !sidesAreClear(target, yaw, ground->radius2))
        return false;
    target[0] += sinCos(back, 0) * kOverEdge;
    target[1] += sinCos(back, 1) * kOverEdge;
    const sceVu0FVECTOR center = { target[0], target[1], target[2] + kEyeHeight, target[3] };
    return onVehicle ? !gStageScene->sphereHitsAny(ground->radius2, center, kFaceWall, 0, nullptr)
                     : !gStageScene->sphereHitsKind(ground->radius2, center, StageObject_Kind3, kFaceWall, 0, nullptr);
}

// FS2 0x0026c180
// callcheck: raycastObject, raycastObject

bool MotionProbe::findFloor(float up, float down, const sceVu0FVECTOR point, float *drop, StageObject **floor)
{
    constexpr u_int kFaceNoFloor = 0x20000;
    constexpr u_int kFloorFlags = 0x40000;
    const u_int mask = 0x42400c;
    sceVu0FVECTOR hit;
    const sceVu0FVECTOR above = { point[0], point[1], point[2] + up, point[3] };
    StageCollisionFace *face = gStageScene->raycastObject(floor, hit, above, point, 0, mask, kFloorFlags, nullptr);
    if (face != nullptr && (face->flags & kFaceNoFloor) == 0) {
        *drop = fabsf(hit[2] - point[2]);
        return true;
    }
    const sceVu0FVECTOR below = { point[0], point[1], point[2] - down, point[3] };
    face = gStageScene->raycastObject(floor, hit, point, below, 0, mask, kFloorFlags, nullptr);
    if (face == nullptr || (face->flags & kFaceNoFloor) != 0)
        return false;
    *drop = -fabsf(hit[2] - point[2]);
    return true;
}


constexpr u_int kFaceGap    = 0x100;   // a ledge one jumps at a run and climbs from standing
constexpr u_int kFaceJump   = 0x180;   // what Jump goes over
constexpr u_int kFaceLadder = 0x2020;
// The sheet's kind whose ledges nobody waits for.
constexpr int   kKindNoBlock = 3;

constexpr float kDoorWalkRate = 0.35f;       // to its place (0x3eb33333)
constexpr float kDoorPathHeight = 50.0f;     // the ray of findDoor, above the feet
constexpr u_int kDoorPathFlags = 0x2040000;
constexpr float kDoorFastRate = 1.2f;        // 0x3f99999a
constexpr int   kBrainPaceFast = 5;
constexpr int   kKindForcesDoors = 6;        // StageObjRecord::kind
constexpr int   kDoorMotionGroup = 3;
constexpr u_int kDoorPoseFlags = 0x42;

// The ledge ahead: the Use button climbs or drops over it. EE vtable 0x317670.
// layout: host
class LevelCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x00272630
    void onApply() override;                                             // FS2 0x00272360

private:
    bool pathBlocked(int id, float reach, const float *from, u_int flags, StageObject *skip);
};

// The door within reach: what it allows; the door's script reads that, shows
// it on the Use button, and answers through wants. EE vtable 0x3175f0.
// layout: host
class DoorCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x00271630
    void onApply() override;                                             // FS2 0x002710a0

private:
    int forceOr(int id) const;
};

// Puts down what the wants name, unless a wall stands ahead. EE vtable 0x3175d0.
// layout: host
class PutItemCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x00271070
    void onApply() override;                                             // FS2 0x00271020
};

// Finds the item in reach and bends for it. EE vtable 0x3175b0.
// layout: host
class PickupItemCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x00270e90
    void onApply() override;                                             // FS2 0x00270c50
};

// The partner lifts the character up a ledge (Motion_Pair). EE vtable 0x317650.
// layout: host
class LevelLiftCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x002721a0
    void onApply() override;                                             // FS2 0x00272030
};

// The ladder ahead. EE vtable 0x317630. layout: host -- +0x14 yaw, +0x20
// point, +0x30 height on the base condition.
class LadderCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x00271d70
    void onApply() override;                                             // FS2 0x00271c60

    float yaw = 0.0f;           // FS2 +0x14: facing away from it
    sceVu0FVECTOR point = {};   // FS2 +0x20: its middle, at the feet's height
    float height = 0.0f;        // FS2 +0x30: to the floor behind it: above, one is at its foot
};

// A gap ahead. EE vtable 0x317610. layout: host -- +0x14 yaw, +0x20 point.
class JumpCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x00271850
    void onApply() override;                                             // FS2 0x00271790

private:
    float yaw = 0.0f;           // FS2 +0x14
    sceVu0FVECTOR point = {};   // FS2 +0x20
};

// On a ladder: the stick climbs, the squat button lets go. EE vtable 0x317490.
// layout: host
// Into the stance, for those who have one. Vtable 0x3176f0. layout: host.
class BtOnCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x00274810
    void onApply() override;                                             // FS2 0x00274650
};

// Keeps m_290, the target, and the character turned to it. Vtable 0x317470.
// layout: host -- as MotionCondition.
class LockCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x0026f8e0
    void onApply() override;                                             // FS2 0x0026ef30

private:
    void faceTarget();                                                   // FS2 0x0026ebe0
};

// The first blow, by the hand. Vtable 0x317450. layout: host.
class BtAttackCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x0026e9e0
    void onApply() override;                                             // FS2 0x0026e500
};

// The next blow of a combo. Vtable 0x317430. layout: host.
class BtAtkComboCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x0026e4e0
    void onApply() override;                                             // FS2 0x0026e280
};

// Steps in the stance, and out of it. Vtable 0x317410. layout: host.
class BtMoveCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x0026e260
    void onApply() override;                                             // FS2 0x0026df60
};

// LadderMove. layout: host -- as MotionCondition.
class LadderMoveCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x0026fb20
    void onApply() override;                                             // FS2 0x0026f970
};

// FS2 0x0026b860

void MotionProbe::pointAhead(float x, float z, sceVu0FVECTOR out)
{
    sceVu0FMATRIX turn;
    identityMatrix(turn);
    rotateMatrixX(x, turn, turn);
    rotateMatrixZ(z, turn, turn);
    rotateMatrixZ(addAngle(owner->m_xform.rotation[2], 3.1415927f), turn, turn);
    const sceVu0FVECTOR ahead = { 0.0f, 4000.0f, 0.0f, 1.0f };
    applyMatrix(out, turn, ahead);
    const float *head = owner->m_model->boneMap.find("bn_head")->value->getMatrix()[3];
    for (int i = 0; i < 4; i++)
        out[i] += head[i];
}

// FS2 0x00215ce0
// callcheck: getSaveData, getStageSelect, getStageSelect (stageObjRecord)

bool StageCharacter::isOpeningLead()
{
    const int stage = getStageSelect()->stage;
    if (stage == 2)
        return stageObjRecord(m_itemDef)->kind == 1;
    if (stage == 1)
        return stageObjRecord(m_itemDef)->kind == 0;
    return false;
}

// FS2 0x00271f50
// callcheck: getStageSelect (stageObjRecord)

int StageCharacter::getLadderRank()
{
    const int kind = stageObjRecord(m_itemDef)->kind;
    if (kind == 1)
        return 0;
    return kind == 0 ? 1 : 2;
}

// The heights of a ledge, what the sheet's up and fall motions are made for.
static constexpr float kDropFar = -150.0f;
static constexpr float kDropMid = -75.0f;
static constexpr float kRiseLow = 75.0f;
static constexpr float kRiseMid = 105.0f;
static constexpr float kRiseHigh = 150.0f;
static constexpr float kLadderWalkRate = 0.35f;

// Walking or running ahead: the way to meet a gap (Jump) and not a ledge (Level).
static bool isForwardMotion(int id)
{
    switch (id) {
    case Motion_CrouchWalk: case Motion_StairsDown: case Motion_StairsUp: case 5: case 4: case 3:
    case 0x8a: case 0x7b: case 0x6c: case 0x51:
    case 0x20:
        return true;
    }
    return isRunMotion(id) || isCrouchRunMotion(id);
}

// The up or fall motion for a height.
static int levelMotion(float drop)
{
    if (drop < kDropFar)
        return Motion_FallHigh;
    if (drop < kDropMid)
        return Motion_FallMid;
    if (drop < 0.0f)
        return Motion_FallLow;
    if (drop < kRiseLow)
        return Motion_Up060;
    if (drop < kRiseMid)
        return Motion_Up090;
    return drop < kRiseHigh ? Motion_Up120 : Motion_Up180;
}

// The floor object is a car: the ROM's kind 3. No such object exists yet.
static StageCar *asCar(StageObject *object)
{
    return object != nullptr && object->getKind() == StageObject_Kind3 ? reinterpret_cast<StageCar *>(object) : nullptr;
}

// FS2 0x001dff40
// callcheck: getStageSelect, getStageSelect (stageObjRecord; upcomingMotion; the loop as stageUpdateCharacterMotions')

bool PlayerMotion::levelBlocked(bool up) const
{
    constexpr float kNearSquared = 10000.0f;
    if (stageObjRecord(owner->m_itemDef)->kind == kKindNoBlock)
        return false;
    StageObjArray &characters = gStageScene->objects->m_lists[StageObjGroup_Characters];
    for (StageObject **slot = characters.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *other = *slot;
        if (!other->m_viewActive || other == owner || stageObjRecord(other->m_itemDef)->kind == kKindNoBlock)
            continue;
        const float height = fabsf(other->m_xform.position[2] - owner->m_xform.position[2]);
        if (200.0f < height || height < 20.0f)
            continue;
        const float dy = other->m_xform.position[1] - owner->m_xform.position[1];
        const float dx = other->m_xform.position[0] - owner->m_xform.position[0];
        if (kNearSquared < dx * dx + dy * dy)
            continue;
        const int id = other->m_motion->upcomingMotion();
        if (!up) {
            if (id == Motion_Pair || id == Motion_Up180 || id == Motion_Up120 || id == Motion_Up090 || id == Motion_Up060)
                return true;
        }
        else if (id == Motion_LadderLetGo || id == Motion_FallHigh || id == Motion_FallMid || id == Motion_FallLow
                 || id == Motion_LadderFall || id == 0x95 || id == 0x94) {
            return true;
        }
    }
    return false;
}

// FS2 0x001e0360
// callcheck: FUN_00271f50, FUN_00271f50

bool PlayerMotion::ladderBlocked() const
{
    constexpr float kNearSquared = 10000.0f;
    StageCharacter *self = static_cast<StageCharacter *>(owner);
    const int rank = self->getLadderRank();
    StageObjArray &characters = gStageScene->objects->m_lists[StageObjGroup_Characters];
    for (StageObject **slot = characters.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *other = *slot;
        if (!other->m_viewActive || other == owner)
            continue;
        const int id = other->m_motion->upcomingMotion();
        if (id == 0xae || id == Motion_LadderRest || id == Motion_LadderDown || id == Motion_LadderUp) {
            if (static_cast<StageCharacter *>(other)->getLadderRank() < rank)
                continue;
        }
        else if (id != Motion_LadderDownOff && id != Motion_LadderDownOn && id != Motion_LadderUpOff
                 && id != Motion_LadderUpOn) {
            continue;
        }
        const float dy = other->m_xform.position[1] - owner->m_xform.position[1];
        const float dx = other->m_xform.position[0] - owner->m_xform.position[0];
        if (dx * dx + dy * dy <= kNearSquared) {
            const float height = other->m_xform.position[2] - owner->m_xform.position[2];
            if (height <= 200.0f && -300.0f <= height)
                return true;
        }
    }
    return false;
}

// FS2 0x001eb090
// callcheck: identityMatrix, rotateMatrixZ, applyMatrix, getRecordOffset

void PlayerMotion::walkFileTarget(sceVu0FVECTOR target, sceVu0FVECTOR aimAt, void *file, const float *from,
                                  const float *angles)
{
    sceVu0FVECTOR ahead = { 0.0f, 100.0f, 0.0f, 0.0f };
    if (angles != nullptr) {
        sceVu0FMATRIX turn;
        identityMatrix(turn);
        rotateMatrixZ(angles[2], turn, turn);
        applyMatrix(ahead, turn, ahead);
    }
    owner->m_model->pose->getRecordOffset(target, aimAt, static_cast<const PoseBoneRecord *>(file), from, ahead);
}

// FS2 0x00271630
// callcheck: FUN_002179b0, FUN_0026a6b0, FUN_0020d730, FUN_0020d650, FUN_0020d820

void DoorCondition::onUpdate()
{
    if (character->m_abilities.flag3b7 == 0 || motion->doorLink->door != nullptr
        || (character->m_abilities.flag3c3 == 0 && character->m_290 != nullptr))
        return;
    StageDoor *door = motion->probe->findDoor();
    if (door == nullptr)
        return;
    CharacterAllows &allows = character->m_allows;
    character->m_door = door;
    allows.doorHand = DoorHand_None;
    allows.doorKey = DoorKey_None;
    if (door->m_doorMode == StageDoorMode_Locked) {
        if (character->canUnlockDoor(door))
            allows.doorKey = DoorKey_Unlock;
        else if (character->canForceDoor(door) && character->m_abilities.flag3b8 != 0)
            allows.doorKey = DoorKey_Force;
    } else if (door->m_doorMode == StageDoorMode_Closed) {
        allows.doorHand = DoorHand_Open;
        if (character->canLockDoor(door))
            allows.doorKey = DoorKey_Lock;
    } else if (door->m_doorMode == StageDoorMode_Open) {
        allows.doorHand = DoorHand_Close;
    }
}

// A character of the sheet's kind 6 has no key to turn: it forces the door.
int DoorCondition::forceOr(int id) const
{
    return stageObjRecord(character->m_itemDef)->kind == kKindForcesDoors ? (int)Motion_DoorForce : id;
}

// FS2 0x002710a0
// callcheck: request, request, FUN_0020d6d0, FUN_00173c30, request, getStageSelect, request, request, FUN_00173c30, request, getStageSelect, request, request, getStageSelect, request, request, FUN_00173c30, request, request, FUN_00173c30, request, request, FUN_00173c30, request, request, FUN_00173c30, request, request, FUN_00173b30, requestedFile, startWalkFile (the sheet's kind is forceOr; the two door types each pick their pair)

void DoorCondition::onApply()
{
    StageDoor *door = character->m_door;
    const CharacterAllows &allows = character->m_allows;
    const CharacterWants &wants = character->m_wants;
    if (door == nullptr || door->m_user != nullptr
        || (allows.doorHand == DoorHand_None && allows.doorKey == DoorKey_None)
        || (wants.useDoor == 0 && wants.useDoorKey == 0))
        return;

    const float *position = character->m_xform.position;
    const bool front = door->isInFront(position);
    const bool typeT = door->m_doorType == StageDoorType_T;
    if (wants.useDoor == 0) {
        if (allows.doorKey == DoorKey_Force) {
            motion->request(Motion_DoorForce);
        } else if (allows.doorKey == DoorKey_Unlock || allows.doorKey == DoorKey_Lock) {
            if (character->reachesThroughDoor(door))
                motion->request(forceOr(Motion_DoorReachKey));
            else if (door->m_doorType == StageDoorType_T || door->m_doorType == StageDoorType_R)
                motion->request(front ? forceOr(Motion_DoorKeyFront) : (int)Motion_DoorKeyBack);
        }
    } else if (allows.doorHand == DoorHand_Close) {
        if (typeT)
            motion->request(front ? Motion_DoorSlideB : Motion_DoorSlideA);
        else if (door->m_doorType == StageDoorType_R)
            motion->request(front ? Motion_DoorPushClose : Motion_DoorPullClose);
    } else if (allows.doorHand == DoorHand_Open) {
        if (typeT)
            motion->request(front ? Motion_DoorSlideA : Motion_DoorSlideB);
        else if (door->m_doorType == StageDoorType_R)
            motion->request(front ? Motion_DoorPullOpen : Motion_DoorPushOpen);
    }

    sceVu0FVECTOR place, angles;
    door->getUsePlace(place, angles, character);
    motion->startWalkFile(kDoorWalkRate, motion->requestedFile(), place, angles, true, false);
}

// FS2 0x0026a6b0
// callcheck: getKind, FUN_001739b0, FUN_001723c0, FUN_001725e0, FUN_00173b30, segmentHits (the ROM calls the two middle ones by slot)

StageDoor *MotionProbe::findDoor()
{
    StageObjArray &roots = gStageScene->objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = roots.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        if ((*slot)->m_active == 0 || (*slot)->getKind() != StageObject_Door)
            continue;
        StageDoor *door = static_cast<StageDoor *>(*slot);
        if (!door->isFree() || !door->scriptIsFacing(owner))
            continue;
        if (door->m_doorType == StageDoorType_R && !door->isShut()) {
            sceVu0FVECTOR from, angles, to;
            door->getUsePlace(from, angles, owner);
            from[2] += kDoorPathHeight;
            memcpy(to, owner->m_xform.position, sizeof(sceVu0FVECTOR));
            to[2] += kDoorPathHeight;
            for (u_int i = 0; door->collisionAt(i) != nullptr; i++) {
                if (door->collisionAt(i)->segmentHits(from, to, 1, kDoorPathFlags, 1))
                    return nullptr;
            }
        }
        return door;
    }
    return nullptr;
}

// FS2 0x0027b590
// callcheck: isPlayer, isPartner, FUN_00210720 (slots +0x278 and +0x2ac are getBrainPace and hasFollower)

void DoorLink::onRequest()
{
    door = character->m_door;
    door->m_user = character;
    if (!character->isPlayer() && !character->isPartner()) {
        if (character->getBrainPace() == kBrainPaceFast)
            rate = kDoorFastRate;
    } else if (character->hasFollower()) {
        rate = kDoorFastRate;
    }
}

// The door plays its half of the motion on its own pose. The third group of
// the character's motions holds them, by these names.
// FS2 0x0027b3d0
// callcheck: findMotion, findMotion, findMotion, findMotion, findMotion, findMotion, findMotion, findMotion, playFile

void DoorLink::onStart()
{
    const char *name = nullptr;
    switch (motion->m_request) {
    case Motion_DoorPullOpen:  name = "dr_plo"; break;  // data 0x308d58
    case Motion_DoorPushClose: name = "dr_psc"; break;  // data 0x308d60
    case Motion_DoorPushOpen:  name = "dr_pso"; break;  // data 0x308d68
    case Motion_DoorPullClose: name = "dr_plc"; break;  // data 0x308d70
    case Motion_DoorKeyFront:  name = "dr_lcr"; break;  // data 0x308d78
    case Motion_DoorKeyBack:   name = "dr_lcl"; break;  // data 0x308d80
    case Motion_DoorSlideA:
    case Motion_DoorSlideB:
        if (door->m_doorMode == StageDoorMode_Closed)
            name = "dr_op";                             // data 0x308d90
        else if (door->m_doorMode == StageDoorMode_Open)
            name = "dr_cl";                             // data 0x308d88
        break;
    }
    if (name == nullptr)
        return;
    ModelMotion *found = character->m_model->findMotion(kDoorMotionGroup, name);
    if (found != nullptr)
        door->m_model->pose->playFile(found->file, kDoorPoseFlags, 0);
}

// The motion's Click: the key turns, or the blow lands.
// FS2 0x0027b140
// callcheck: FUN_001736e0, FUN_00173750, FUN_001737c0, playNamedHeard2, breakPart, FUN_00173fb0, FUN_00173fb0, FUN_00173fb0, FUN_00173fb0, FUN_00173fb0, FUN_00173fb0, FUN_00173fb0, isControlled, FUN_00172530

void DoorLink::onRun()
{
    if ((character->m_model->m_motionEvents & MotionEvent_Click) == 0)
        return;
    const int id = motion->motionId;
    const bool turnsKey = id == Motion_DoorReachKey || id == Motion_DoorKeyBack || id == Motion_DoorKeyFront;
    char buffer[0x20];
    const char *sound = nullptr;
    if (id == Motion_DoorForce)
        sound = door->soundName(buffer, "DR_%02d_AT");           // data 0x302e68
    else if (turnsKey && door->m_doorMode == StageDoorMode_Closed)
        sound = door->soundName(buffer, "DR_%02d_LOCK");         // data 0x302e78
    else if (turnsKey && door->m_doorMode == StageDoorMode_Locked)
        sound = door->soundName(buffer, "DR_%02d_ULOCK");        // the third of the ROM's formats
    if (sound != nullptr)
        door->playNamedHeard2(sound, door->m_xform.position, nullptr);

    if (id == Motion_DoorForce) {
        door->m_hp -= character->m_doorBlow;
        if (door->m_hp < 0)
            door->m_hp = 0;
        if (door->m_hp == 0) {
            if (door->m_window == StageDoorWindow_Whole)
                door->breakPart(0, false);
            else
                door->setMode(StageDoorMode_Closed);
            finished = 1;
        }
    } else if (turnsKey) {
        if (door->m_doorMode == StageDoorMode_Closed)
            door->setMode(StageDoorMode_Locked);
        else if (door->m_doorMode == StageDoorMode_Locked)
            door->setMode(StageDoorMode_Closed);
    }
    if (character->isControlled())
        door->noteUse(true);
}

// FS2 0x0027b0f0
// callcheck: FUN_00173fb0

void DoorLink::onLeave(MotionLink *)
{
    if (door->m_doorMode == StageDoorMode_Open || door->m_doorMode == StageDoorMode_Closed)
        door->setMode(door->m_doorMode);
}

// FS2 0x0027b570

void DoorLink::onCancel()
{
    if (door != nullptr) {
        door->m_user = nullptr;
        door = nullptr;
    }
    rate = 1.0f;
}

// FS2 0x00219b30

int StageCharacter::doorActions() const
{
    if (m_door == nullptr)
        return 0;
    const int hand = m_allows.doorHand, key = m_allows.doorKey;
    int actions = hand == DoorHand_Close ? CharacterDoor_Close : hand == DoorHand_Open ? CharacterDoor_Open : 0;
    if (key == DoorKey_Unlock)
        actions |= CharacterDoor_Unlock;
    else if (key == DoorKey_Lock)
        actions |= CharacterDoor_Lock;
    if (hand == DoorHand_None && key == DoorKey_None)
        actions |= CharacterDoor_Nothing;
    return actions;
}

// FS2 0x0020d6d0
// callcheck: FUN_00173c30, FUN_00173830

bool StageCharacter::reachesThroughDoor(StageDoor *door) const
{
    return door->isInFront(m_xform.position) && door->m_window == StageDoorWindow_Broken && door->canReachThrough();
}

// FS2 0x0020d730
// callcheck: FUN_001739f0, FUN_00173c30, FUN_00173830, FUN_001739f0

bool StageCharacter::canUnlockDoor(StageDoor *door) const
{
    if (door->m_doorMode != StageDoorMode_Locked)
        return false;
    switch (door->m_keyKind) {
    case 1:
    case 2:
        return door->keyWorksFrom(m_xform.position) || reachesThroughDoor(door);
    case 3:
    case 4:
    case kDoorKeyBothSides:
        return door->keyWorksFrom(m_xform.position);
    }
    return false;
}

// FS2 0x0020d650
// callcheck: FUN_001739f0

bool StageCharacter::canForceDoor(StageDoor *door) const
{
    if (door->m_doorMode != StageDoorMode_Locked || (door->m_keyKind != 1 && door->m_keyKind != 2))
        return false;
    return !door->keyWorksFrom(m_xform.position);
}

// FS2 0x0020d820
// callcheck: FUN_001739f0, FUN_001739f0

bool StageCharacter::canLockDoor(StageDoor *door) const
{
    return door->m_doorMode == StageDoorMode_Closed && door->m_keyKind != 0
        && door->keyWorksFrom(m_xform.position);
}

// FS2 0x0021c230
// callcheck: isControlled, FUN_001db470, FUN_00172530

bool StageCharacter::scriptOpenDoor()
{
    if (!isControlled())
        return m_brain->askOpenDoor();
    if (m_allows.doorHand != DoorHand_Open)
        return false;
    m_door->noteUse(true);
    m_wants.useDoor = 1;
    return true;
}

// FS2 0x0021c1d0
// callcheck: isControlled, FUN_001db3a0, FUN_00172530

bool StageCharacter::scriptCloseDoor()
{
    if (!isControlled())
        return m_brain->askCloseDoor();
    if (m_allows.doorHand != DoorHand_Close)
        return false;
    m_door->noteUse(true);
    m_wants.useDoor = 1;
    return true;
}

// FS2 0x0021c130
// callcheck: isControlled, FUN_001db2d0, FUN_00172530, FUN_00172530

bool StageCharacter::scriptUseDoorKey(int lock)
{
    if (!isControlled())
        return m_brain->askDoorKey(lock);
    if (m_allows.doorKey != (lock == 0 ? DoorKey_Unlock : DoorKey_Lock))
        return false;
    m_door->noteUse(true);
    m_wants.useDoorKey = 1;
    return true;
}

// FS2 0x00272630
// callcheck: isControlled, getKind, FUN_0028ab90, getKind, FUN_0028ab90, findMotionFile, walkFileTarget, walkFileTarget, sinCos, sinCos, segmentHitsAny, FUN_00215ce0, findMotionFile, sinCos, sinCos, segmentHitsAny, FUN_001dff40 (the two segment tests are pathBlocked here)

void LevelCondition::onUpdate()
{
    WalkLink *walk = motion->walkLink;
    const StageCollisionFace *face = walk->wallFace;
    if (face != nullptr && (faceFlags(face) & kFaceGap) != 0 && character->isControlled()
        && isForwardMotion(motion->motionId))
        return;
    if (walk->edgeFound == 0)
        return;
    if (StageCar *car = asCar(walk->edgeFloor)) {
        if (!car->hasSettled() || car->getMode() == 1)
            return;
    }
    else {
        StageObject *floor = character->m_ground->floorObject;
        if (floor == nullptr)
            return;
        StageCar *standing = asCar(floor);
        if (standing != nullptr && !standing->hasSettled())
            return;
    }

    const float drop = walk->edgeDrop;
    const int id = levelMotion(drop);
    if (0.0f <= drop) {
        const int needed = id == Motion_Up060 ? 1 : id == Motion_Up090 ? 2 : id == Motion_Up120 ? 3 : 4;
        if (character->m_abilities.stepLevel < needed)
            return;
        if (character->m_42c != 0 && pathBlocked(id, 5.0f, walk->edgePoint, 0x40000, walk->edgeObject))
            return;
    }
    else {
        if (character->isOpeningLead() && walk->edgeDrop < -125.0f)
            return;
        // A fall ends where the motion's height says; 5 past that is what must be free.
        const float reach = -(motion->m_endHeight[id] + 5.0f);
        if (pathBlocked(id, reach, nullptr, 0x40140, nullptr))
            return;
    }
    if ((character->m_abilities.flag3c3 != 0 || character->m_290 == nullptr) && !motion->levelBlocked(0.0f < drop))
        character->m_allows.level = drop < 0.0f ? 2 : 1;
}

// The way from the edge to where motion id ends, 50 over the floor. from:
// null starts at that end.

bool LevelCondition::pathBlocked(int id, float reach, const float *from, u_int flags, StageObject *skip)
{
    WalkLink *walk = motion->walkLink;
    sceVu0FVECTOR target, aimAt;
    void *file = character->m_model->findMotionFile(0, gMotionDefs[id].labelText);
    motion->walkFileTarget(target, aimAt, file, walk->edgePoint, walk->edgeAngles);
    sceVu0FVECTOR start, end;
    memcpy(start, from != nullptr ? from : target, sizeof(start));
    memcpy(end, target, sizeof(end));
    const float yaw = -walk->edgeAngles[2];
    end[0] += reach * sinCos(yaw, 0);
    end[1] += reach * sinCos(yaw, 1);
    start[2] += 50.0f;
    end[2] += 50.0f;
    return gStageScene->segmentHitsAny(start, end, 0, kFaceWall, flags, skip);
}

// FS2 0x00272360
// callcheck: FUN_001dff40, getKind, getKind, getKind, getKind, request, requestedFile, startWalkFile (the six other requests: levelMotion)

void LevelCondition::onApply()
{
    WalkLink *walk = motion->walkLink;
    if (character->m_allows.level == 0 || character->m_wants.useLevel == 0
        || motion->levelBlocked(0.0f < walk->edgeDrop))
        return;
    // Onto a car, by whatever height, is the one motion.
    StageObject *standing = character->m_ground->floorObject;
    bool ontoCar = false;
    if (0.0f < walk->edgeDrop && standing != nullptr && walk->edgeFloor != nullptr
        && walk->edgeFloor->getKind() == StageObject_Kind3) {
        const int kind = standing->getKind();
        ontoCar = kind == StageObject_MapObject || kind == StageObject_Kind3;
    }
    motion->request(ontoCar ? (int)Motion_Up090 : levelMotion(walk->edgeDrop));
    motion->startWalkFile(kLadderWalkRate, motion->requestedFile(), walk->edgePoint, walk->edgeAngles, true, false);
}

// FS2 0x002721a0
// callcheck: partnerResponds, addAngle, vecDistance

void LevelLiftCondition::onUpdate()
{
    if (!gStageScene->partnerResponds(character))
        return;
    const WalkLink *walk = motion->walkLink;
    motion->m_bae = 1;
    memcpy(motion->position, walk->edgeTarget, sizeof(motion->position));
    memcpy(motion->m_liftAngles, walk->edgeAngles, sizeof(motion->m_liftAngles));
    motion->m_liftAngles[2] = addAngle(walk->edgeAngles[2], 3.1415927f);
    StageObject *partner = gStageScene->partner;
    if (isRestingMotion(partner->m_motion->motionId)
        && vecDistance(motion->position, partner->m_xform.position) <= 30.0f)
        character->m_allows.liftPartner = 1;
}

// FS2 0x00272030
// callcheck: request, requestedFile, startWalkFile, requestNow, sinCos, sinCos, requestedFile, startWalkFile

void LevelLiftCondition::onApply()
{
    if (character->m_allows.liftPartner == 0 || character->m_wants.liftPartner == 0)
        return;
    WalkLink *walk = motion->walkLink;
    motion->request(Motion_Pair);
    motion->startWalkFile(kLadderWalkRate, motion->requestedFile(), walk->edgePoint, walk->edgeAngles, false, false);

    PlayerMotion *other = gStageScene->partner->m_motion;
    other->requestNow(Motion_Pair);
    sceVu0FVECTOR place;
    memcpy(place, motion->position, sizeof(place));
    const float back = character->m_ground->radius2;
    place[0] -= back * sinCos(-motion->m_liftAngles[2], 0);
    place[1] -= back * sinCos(-motion->m_liftAngles[2], 1);
    other->startWalkFile(kLadderWalkRate, other->requestedFile(), place, motion->m_liftAngles, false, false);
}

// FS2 0x00271d70
// callcheck: getBox, getNormal, atan2Octant, sinCos, sinCos, findFloor, FUN_001e0360

void LadderCondition::onUpdate()
{
    if (character->m_abilities.ladder == 0
        || (character->m_abilities.flag3c3 == 0 && character->m_290 != nullptr))
        return;
    const StageCollisionFace *face = motion->walkLink->wallFace;
    if (face == nullptr || (faceFlags(face) & kFaceLadder) == 0)
        return;
    sceVu0FVECTOR box[2];
    face->getBox(box);
    point[0] = (box[0][0] + box[1][0]) / 2.0f;
    point[1] = (box[0][1] + box[1][1]) / 2.0f;
    point[2] = character->m_xform.position[2];
    sceVu0FVECTOR beyond;
    memcpy(beyond, point, sizeof(beyond));
    sceVu0FVECTOR normal;
    face->getNormal(normal);
    yaw = atan2Octant(normal[0], -normal[1]);
    const float radius = character->m_ground->radius2;
    beyond[0] += radius * sinCos(-yaw, 0);
    beyond[1] += radius * sinCos(-yaw, 1);
    beyond[2] += 50.0f;
    if (motion->probe->findFloor(1000.0f, 1000.0f, beyond, &height, nullptr) && !motion->ladderBlocked()) {
        height += 50.0f;
        character->m_allows.ladder = height < 0.0f ? 2 : 1;
    }
}

// FS2 0x00271c60
// callcheck: FUN_001e0360, request, request, addAngle, requestedFile, startWalkFile

void LadderCondition::onApply()
{
    const int side = character->m_allows.ladder;
    if (side == 0 || character->m_wants.useLadder == 0 || motion->ladderBlocked())
        return;
    if (side == 1)
        motion->request(Motion_LadderUpOn);
    else if (side == 2)
        motion->request(Motion_LadderDownOn);
    const sceVu0FVECTOR angles = { 0.0f, 0.0f, addAngle(yaw, 3.1415927f), 0.0f };
    motion->startWalkFile(kLadderWalkRate, motion->requestedFile(), point, angles, true, false);
}

// FS2 0x00271850
// callcheck: isControlled, getNormal, atan2Octant, sinCos, sinCos, raycast, getNormal, atan2Octant, sinCos, sinCos, FUN_0026c2d0

void JumpCondition::onUpdate()
{
    const StageCollisionFace *face = motion->walkLink->wallFace;
    if (face == nullptr || (faceFlags(face) & kFaceJump) == 0)
        return;
    if ((faceFlags(face) & kFaceGap) != 0 && character->isControlled() && !isForwardMotion(motion->motionId))
        return;
    if (character->m_abilities.jump == 0
        || (character->m_abilities.flag3c3 == 0 && character->m_290 != nullptr))
        return;
    sceVu0FVECTOR from, to;
    memcpy(from, character->m_xform.position, sizeof(from));
    memcpy(to, character->m_xform.position, sizeof(to));
    from[2] += 50.0f;
    to[2] += 50.0f;
    sceVu0FVECTOR normal;
    face->getNormal(normal);
    yaw = atan2Octant(normal[0], -normal[1]);
    const float reach = character->m_ground->radius + 25.0f;
    to[0] += reach * sinCos(-yaw, 0);
    to[1] += reach * sinCos(-yaw, 1);
    StageCollisionFace *hit = gStageScene->raycast(point, from, to, 0, kFaceJump, 0, nullptr);
    point[2] -= 50.0f;
    if (hit == nullptr)
        return;
    sceVu0FVECTOR place;
    memcpy(place, point, sizeof(place));
    hit->getNormal(normal);
    yaw = atan2Octant(normal[0], -normal[1]);
    const float radius = character->m_ground->radius2;
    place[0] -= radius * sinCos(-yaw, 0);
    place[1] -= radius * sinCos(-yaw, 1);
    if (sidesAreClear(place, yaw, radius))
        character->m_allows.jump = 1;
}

// FS2 0x00271790
// callcheck: request, addAngle, requestedFile, startWalkFile

void JumpCondition::onApply()
{
    if (character->m_allows.jump == 0 || character->m_wants.useJump == 0)
        return;
    motion->request(Motion_Jump);
    const sceVu0FVECTOR angles = { 0.0f, 0.0f, addAngle(yaw, 3.1415927f), 0.0f };
    motion->startWalkFile(kLadderWalkRate, motion->requestedFile(), point, angles, true, false);
}

// FS2 0x0026fb20

void LadderMoveCondition::onUpdate()
{
    character->m_allows.move = 1;
    if (motion->motionId == Motion_LadderRest)
        character->m_allows.squatAction = 1;
}

// FS2 0x0026f970
// callcheck: FUN_001ef260, request, FUN_001e0360, request, request, FUN_001e0360, request, request, request

void LadderMoveCondition::onApply()
{
    constexpr float kOffTop = 185.0f;
    constexpr float kOffFoot = 35.0f;
    const CharacterWants &wants = character->m_wants;
    const int kind = wants.moveKind;
    if (kind == CharacterMove_Back || kind == CharacterMove_Walk || kind == CharacterMove_Run) {
        const LadderLink *ladder = static_cast<const LadderLink *>(motion->m_links.find("Ladder")->second);
        float above = ladder->baseHeight - character->m_xform.position[2];
        if (0.0f < ladder->height)
            above += ladder->height;
        if (kind == CharacterMove_Back) {
            if (fabsf(ladder->height) - kOffFoot < above)
                motion->request(motion->ladderBlocked() ? Motion_LadderRest : Motion_LadderDownOff);
            else
                motion->request(Motion_LadderDown);
        }
        else {
            if (above < kOffTop)
                motion->request(motion->ladderBlocked() ? Motion_LadderRest : Motion_LadderUpOff);
            else
                motion->request(Motion_LadderUp);
        }
    }
    else if (character->m_allows.squatAction == 0 || wants.squatAction == 0) {
        motion->request(Motion_LadderRest);
    }
    else {
        motion->request(Motion_LadderLetGo);
    }
}

// FS2 0x0027b730
// callcheck: FUN_0027d8d0, operator_new, FUN_001ef590, MotionConditionList::add (addCondition)

LadderLink::LadderLink(StageCharacter *owner, PlayerMotion *motion_)
    : MotionLink(owner, motion_)
{
    addCondition("LadderMove");
}

// FS2 0x0027b6e0
// callcheck: FUN_001ef590

void LadderLink::onRequest()
{
    const LadderCondition *ladder = static_cast<const LadderCondition *>(motion->m_conditions.find("Ladder")->second);
    baseHeight = ladder->point[2];
    height = ladder->height;
}

// FS2 0x0027be70
// callcheck: isPlayer

void UpLink::onRequest()
{
    if (motion->m_request != Motion_Pair)
        rise = motion->walkLink->edgeDrop;
    else if (character->isPlayer())
        rise = 0.0f;
    else
        rise = -gStageScene->player->m_motion->walkLink->edgeDrop;
}

// FS2 0x0027bdb0
// callcheck: setRootScale

void UpLink::onStart()
{
    // How high the sheet's motion was made for.
    float made = 1.0f;
    switch (motion->m_request) {
    case Motion_Pair:
    case Motion_Up180:
        made = 180.0f;
        break;
    case Motion_Up120:
        made = 120.0f;
        break;
    case Motion_Up090:
        made = 90.0f;
        break;
    case Motion_Up060:
        made = 60.0f;
        break;
    }
    scale = rise / made;
    character->m_model->pose->setRootScale(1.0f, 1.0f, scale, 1.0f);
}

// FS2 0x0027bca0
// callcheck: setRootScale, isPlayer, requestNow

void UpLink::onRun()
{
    constexpr int kFallBack = 0xa4;
    character->m_model->pose->setRootScale(1.0f, 1.0f, scale, 1.0f);
    if (motion->motionId != Motion_Pair)
        return;
    StageObject *player = gStageScene->player;
    StageObject *partner = gStageScene->partner;
    if (!(10.0f < fabsf(player->m_xform.position[2] - partner->m_xform.position[2])))
        return;
    if (!character->isPlayer()) {
        const PlayerMotion *other = player->m_motion;
        if (other->motionId != Motion_Pair && other->m_request != Motion_Pair)
            motion->requestNow(kFallBack);
    }
    else {
        const PlayerMotion *other = partner->m_motion;
        if (other->motionId != Motion_Pair && other->m_request != Motion_Pair)
            finished = 1;
    }
}

// FS2 0x0027b860
// callcheck: isEnding, getKind, FUN_0028ab90, getStageSelect, getStageSelect, request, request, request, request, request, request, request, request, request, getStageSelect, getStageSelect, request, request, FUN_002a2fc0 (stageObjRecord: landingMotion)

void FallLink::onRun()
{
    constexpr int kLandFatal = 0xaf;
    constexpr int kFallWeapon = 0xed;
    constexpr int kFallWeaponLand = 0xee;
    constexpr float kFootDrop = 25.0f;
    constexpr float kLowDrop = 75.0f;
    constexpr float kMidDrop = 150.0f;
    constexpr float kSafeDrop = 295.0f;
    constexpr float kSturdyDrop = 395.0f;

    if (counter == 0) {
        tracking = 1;
        startHeight = character->m_xform.position[2];
        counter++;
    }
    if (counter == 1 && motion->isEnding()) {
        fallStep = character->m_model->m_positionStep[2];
        counter++;
    }
    if (counter == 2)
        motion->m_moveStep[2] = fallStep;

    if ((character->m_model->m_motionEvents & MotionEvent_Click) != 0)
        tracking = tracking == 0;
    if (tracking == 0)
        return;
    CharacterGround *ground = character->m_ground;
    if (ground->landed == 0 && ground->hasFloor != 0)
        return;

    const float depth = startHeight - character->m_xform.position[2];
    const int sturdiness = stageObjRecord(character->m_itemDef)->stepDown;
    StageCar *car = asCar(ground->floorObject);
    if (car != nullptr && !car->hasSettled()) {
        motion->request(sturdiness == 2 ? (int)Motion_LandHigh : kLandFatal);
        ground->groundCheck = 1;
        return;
    }
    if (depth < kFootDrop)
        return;
    if (motion->motionId == kFallWeapon)
        motion->request(kFallWeaponLand);
    else if (depth < kLowDrop)
        motion->request(Motion_LandLow);
    else if (depth < kMidDrop)
        motion->request(Motion_LandMid);
    else if (depth < kSafeDrop)
        motion->request(Motion_LandHigh);
    else if (sturdiness == 2)
        motion->request(Motion_LandHigh);
    else if (sturdiness == 1)
        motion->request(depth < kSturdyDrop ? (int)Motion_LandHigh : kLandFatal);
    else if (sturdiness == 0)
        motion->request(kLandFatal);
    character->playLandSound();
    ground->groundCheck = 1;
}

// FS2 0x0027b7e0
// callcheck: request, FUN_002a2fc0

void JumpLink::onRun()
{
    CharacterGround *ground = character->m_ground;
    if (ground->landed != 0 && ground->wasLanded == 0) {
        motion->request(Motion_LandHigh);
        character->playLandSound();
        ground->groundCheck = 1;
    }
}

// FS2 0x002a2fc0
// callcheck: findStepSound, playEntryHeard

void StageCharacter::playLandSound()
{
    constexpr int kPaceLanding = 4;
    // PC: with no floor (hasFloor 0) the ROM reads through null.
    if (m_ground->floorFace == nullptr)
        return;
    if (const SeEntry *entry = findStepSound(m_ground->floorFace->surface, kPaceLanding))
        playEntryHeard(entry, m_xform.position, nullptr);
}

// FS2 0x0027c520
// callcheck: FUN_0027d8d0, operator_new, FUN_001ef590, FUN_0027da40, getStageSelect, getStageSelect, FUN_001d1280 (addCondition; the list of 0x1df0b0 is not written, so the sp_ names of data 0x2fb3a0 stay out)

WalkLink::WalkLink(StageCharacter *owner, PlayerMotion *motion_)
    : MotionLink(owner, motion_)
{
    addCondition("QuickTurn");
    addCondition("Squat");
    if (character->m_abilities.flag3c2 != 0) {
        if (character->m_abilities.flag3c3 == 0) {
            addCondition("Lock");
            addCondition("BtAttack");
        }
        else {
            addCondition("BtOn");
        }
    }
    static const char *const kOrder[] = {
        "GunOn", "Level", "LevelLift", "Ladder", "Jump", "Door", "PutItem", "PickupItem",
        "TrapSet", "TrapCancel", "Throw", "Showt", "Reload", "Bite", "Jack",
    };
    for (const char *name : kOrder)
        addCondition(name);
    addCondition("Turn");
    addCondition("Move");
}

// FS2 0x0027c420

void WalkLink::onUpdate()
{
    if (character->m_ground->wallBody == nullptr)
        return;
    wallFace = motion->probe->faceAhead(wallPoint, true);
    edgeFound = motion->probe->findEdge(wallFace, &edgeObject, edgeTarget, &edgeDrop, &edgeFloor, edgePoint,
                                        edgeAngles, &edgeBlocked);
}

// FS2 0x0027c4b0
// callcheck: FUN_001ef260 (m_links.find)

void WalkLink::onLeave(MotionLink *next)
{
    if (character->m_abilities.flag3c3 == 0) {
        auto attack = motion->m_links.find("BtAttack");
        if (attack == motion->m_links.end() || next != attack->second)
            character->m_290 = nullptr;
    }
    wallFace = nullptr;
    edgeFound = 0;
}

// FS2 0x001ed410
// callcheck: DamageDeadLink, MotionCondition, MotionLink, memset, FUN_001d1280, FUN_00278db0, FUN_0027a0b0, FUN_0027a910, FUN_0027aa90, FUN_0027aea0, FUN_0027b000, FUN_0027b730, FUN_00281aa0, FUN_00281b30, FUN_002820e0, FUN_00282200, FUN_002822d0, FUN_002825f0, FUN_00282b30, FUN_00282da0, FUN_00282fa0, FUN_00283560, FUN_00283740, FUN_00283960, getStageSelect, operator_new, FUN_001ef040, FUN_001eeee0, FUN_001eee30, FUN_0026c6b0, FUN_00276510, FUN_001ef0f0, FUN_001ef850, FUN_001df0b0, FUN_0027c520, FUN_0027c140, FUN_0027b640, FUN_0027cc50, FUN_002775d0, FUN_001eef90, FUN_0027c370, FUN_001ef990 (the links, the conditions and their maps: game/playermotion.h)

PlayerMotion::PlayerMotion(StageObject *owner_)
    : owner(owner_)
{
    const int kind = stageObjRecord(owner->m_itemDef)->kind;
    if (const MotionKindRow *row = motionKindRow(kind)) {
        m_weaponClass = row->weaponClass;
        m_kindFlags1 = row->flags1;
        m_kindFlags2 = row->flags2;
        m_kindFlags3 = row->flags3;
        m_hasHands = row->hands;
        m_hasFace = row->face;
        m_kindFlags6 = row->flags6;
        m_kindFlags7 = row->flags7;
        m_kindFlags8 = row->flags8;
    }
    if (kind == 6 || kind == 0xf) {
        m_kindVector[0] = 0.0f;
        m_kindVector[1] = kind == 6 ? -55.0f : -50.0f;
        m_kindVector[2] = 0.0f;
        m_kindVector[3] = 1.0f;
    }

    m_queue = new MotionQueue;
    m_walkQueue = new MotionQueue;

    // The conditions: the ROM's order is the 29 of architecture.md "Motions";
    // here the ones written.
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    probe = new MotionProbe(character);
    m_always = new AlwaysCondition(character, this);
    m_conditions["Always"] = m_always;
    m_conditions["Squat"] = new SquatCondition(character, this);
    m_conditions["Turn"] = new TurnCondition(character, this);
    m_conditions["Move"] = new MoveCondition(character, this);
    m_conditions["Level"] = new LevelCondition(character, this);
    m_conditions["LevelLift"] = new LevelLiftCondition(character, this);
    m_conditions["Ladder"] = new LadderCondition(character, this);
    m_conditions["Jump"] = new JumpCondition(character, this);
    m_conditions["QuickTurn"] = new QuickTurnCondition(character, this);
    m_conditions["LadderMove"] = new LadderMoveCondition(character, this);
    m_conditions["Door"] = new DoorCondition(character, this);
    m_conditions["PutItem"] = new PutItemCondition(character, this);
    m_conditions["PickupItem"] = new PickupItemCondition(character, this);
    m_conditions["BtOn"] = new BtOnCondition(character, this);
    m_conditions["Lock"] = new LockCondition(character, this);
    m_conditions["BtAttack"] = new BtAttackCondition(character, this);
    m_conditions["BtAtkCombo"] = new BtAtkComboCondition(character, this);
    m_conditions["BtMove"] = new BtMoveCondition(character, this);

    walkLink = new WalkLink(character, this);
    m_links["Walk"] = walkLink;
    m_links["Up"] = new UpLink(character, this);
    m_links["Fall"] = new FallLink(character, this);
    m_links["Jump"] = new JumpLink(character, this);
    m_links["Ladder"] = new LadderLink(character, this);
    doorLink = new DoorLink(character, this);
    m_links["Door"] = doorLink;
    pickupLink = new PickupLink(character, this);
    m_links["Pickup"] = pickupLink;
    m_links["Battle"] = new BattleLink(character, this);
    m_links["BtAttack"] = new BtAttackLink(character, this);

    // PC: the ROM's 0x212530 sets these from a script's property; nothing
    // written sets them yet, so the sheet's.
    const StageObjRecord *sheet = stageObjRecord(owner->m_itemDef);
    if (sheet->walkSpeed > 0)
        character->m_walkRate = sheet->walkSpeed / 100.0f;
    if (sheet->runSpeed > 0)
        character->m_runRate = sheet->runSpeed / 100.0f;
    if (sheet->turnSpeed > 0)
        character->m_turnRate = sheet->turnSpeed / 100.0f;
}

// FS2 0x001ed2a0
// callcheck: operator_delete, FUN_0026c650, FUN_001eee50, FUN_001eef00, FUN_001ef060 (the host deletes; the maps of the ctor)

PlayerMotion::~PlayerMotion()
{
    delete m_queue;
    delete m_walkQueue;
    for (auto &entry : m_links)
        delete entry.second;
    for (auto &entry : m_conditions)
        delete entry.second;
    delete probe;
}

// FS2 0x001ecda0

void PlayerMotion::loadMotions(StageList<void *> *motions, StageList<void *> *motions2, void *mdl)
{
    StageModel *model = owner->m_model;
    StageList<void *>::Node *node = motions->head;
    model->setBaseMotion(mdl != nullptr ? mdl : node->value, 0);
    model->bindBones();
    model->placeBones(owner);

    const auto indexOf = [](const char *file) {
        switch (file[0x13]) {
        case 'd': return 3;
        case 'f': return 2;
        case 'h': return 1;
        case '_': return 0;
        }
        return 4;
    };
    StageList<void *>::Node *second = motions2->head;
    for (node = node->next; node != nullptr && second != nullptr; node = node->next, second = second->next) {
        char *file = static_cast<char *>(node->value);
        model->addMotion(file, indexOf(file), file + 0x17);
        if (m_hasFace == 0)
            continue;

        // The other list's file is the same motion's face.
        char *face = static_cast<char *>(second->value);
        const char *faceKey = "s";
        if (face != nullptr) {
            faceKey = face + 0x17;
            if (model->findMotion(2, faceKey) == nullptr)
                model->addMotion(face, indexOf(face), faceKey);
        }
        for (int i = 0; i < kMotionDefCount; i++) {
            if (m_faces[i] == nullptr && gMotionDefs[i].labelText != nullptr
                && strcmp(gMotionDefs[i].labelText, file + 0x17) == 0)
                m_faces[i] = faceKey;
        }
    }
    model->balanceMotions();

    for (int i = 0; i < kMotionDefCount; i++) {
        if (gMotionDefs[i].labelText == nullptr)
            continue;
        ModelMotion *motion = model->findMotion(0, gMotionDefs[i].labelText);
        if (motion == nullptr)
            continue;
        if (m_hasHands != 0) {
            const u_int hands = motionFileHands(motion->file);
            m_handsBusy[i][0] = (hands & 2) != 0;
            m_handsBusy[i][1] = (hands & 1) != 0;
        }
        float x = 0.0f, y = 0.0f, z = 0.0f;
        motionFileLastPosition(&x, &y, &z, 0, motion->file);
        m_endHeight[i] = z;
    }

    m_modelFamily = -1;
    for (int i = 0; i < kMotionModelCount; i++) {
        if (gMotionModels[i].tag != nullptr && strstr(model->pose->name, gMotionModels[i].tag) != nullptr) {
            m_modelFamily = i;
            break;
        }
    }

    model->pose->m_solvers = 0x1f;
    if (m_kindFlags1 == 0 && m_kindFlags2 == 0 && m_kindFlags3 == 0)
        model->pose->m_solvers &= ~0xfu;
}

// FS2 0x001df040

void PlayerMotion::initSlots14()
{
    // Each entry of the list at +0x30, slot +0x14: the list is not written.
}

// FS2 0x001defd0

void PlayerMotion::initSlots18()
{
    // Each entry of the list at +0x30, slot +0x18: the list is not written.
}

// FS2 0x001ec7e0
// callcheck: FUN_001ef260, getStageSelect, noteMotion, resolveHold, findLink (cancelRequested, stageObjRecord)

void PlayerMotion::reset(int arg)
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    ModelPose *pose = character->m_model->pose;

    cancelRequested();
    m_request = arg;
    m_queue->close(true);
    m_walkPending = 0;
    if (m_walking != 0) {
        pose->stopWalk();
        m_walking = 0;
        m_walkQueue->close(true);
    }
    if (motionId != Motion_None) {
        if (activeLink != nullptr)
            activeLink->onLeave(nullptr);
        noteLeaving();
    }
    m_queue->open = 1;
    m_queue->expire();
    m_queue->open = 0;
    m_walkQueue->open = 1;
    m_walkQueue->expire();
    m_walkQueue->open = 0;
    m_stepsOn[0] = 1;
    m_stepsOn[1] = 1;
    m_stepsOn[2] = 1;
    m_stepsOn[3] = 1;
    m_b54 = 0;
    motionId = Motion_None;
    activeLink = nullptr;
    startRequested();

    m_handNames[0] = nullptr;
    m_handNames[1] = nullptr;
    m_faceName = nullptr;
    m_motionTime = 0.0f;
    m_runTime = 0.0f;
    m_runDistance = 0.0f;
    memcpy(m_lastPosition, character->m_xform.position, sizeof(m_lastPosition));
    m_stanceFlag = 0;
    m_b24 = 0.0f;
    m_b28 = 0.0f;
    m_b2c = 0.0f;
    m_b30 = 0;
    m_forceStart = 0;
    memset(m_moveStep, 0, sizeof(m_moveStep));
    memset(m_turnStep, 0, sizeof(m_turnStep));
    character->m_xform.rotation[0] = 0.0f;
    character->m_xform.rotation[1] = 0.0f;
    memset(m_lampAngles, 0, sizeof(m_lampAngles));
    memset(m_leanScale, 0, sizeof(m_leanScale));
    m_leanFlag = 0;

    if (m_modelFamily >= 0 && m_modelFamily < kMotionModelCount) {
        const MotionModel *family = &gMotionModels[m_modelFamily];
        pose->headLimit = family->limits[0] * 0.5f;
        pose->headLimitUp = family->limits[1] * 0.5f;
        pose->headLimitDown = family->limits[2] * 0.5f;
    }
    pose->setHeadTarget(0.15f, nullptr);
    pose->setLookTarget(0.15f, 1.2566371f, nullptr);
    pose->setToolTarget(0.1f, nullptr);
    sceVu0FVECTOR ground = { 0.0f, 0.0f, 1.0f, 0.0f };
    if (pose->parentMatrix == nullptr)
        ground[3] = character->m_xform.position[2];
    pose->setFootTargets(1.0f, ground, ground);
    memset(pose->rootOffset, 0, sizeof(pose->rootOffset));

    m_fallSpeed = 0.0f;
    m_ba8 = 0.0f;
    if (noFlash != 0)
        clearNoFlash(0);
    m_bf0 = 0.0f;
    m_bf4 = 0.0f;
    if (character->getAltSoundFlag() != 0 && gStageScene->party->isRunning())
        gStageScene->party->stop();
    character->m_altAction = 0;

    stepLower(true);
    stepUpper(true);
    stepFace(true);
    character->m_model->applyMotion(character->m_xform.position, character->m_xform.rotation);

    if (0 < stageObjRecord(character->m_itemDef)->health && character->m_health <= 0.0f) {
        character->m_winded = 1;
        character->m_idleTime = 0.0f;
    }
    character->stopBodyEffects();
}

// FS2 0x001e5d90

void PlayerMotion::beginFrame()
{
    Clock *clock = owner->m_clock;
    ModelPose *pose = owner->m_model->pose;
    m_motionTime += clock->getDeltaSeconds();
    if (pose->isBoneEnding(nullptr))
        m_b2c = 0.0f;
    m_bae = 0;
    m_lookObject = nullptr;
    m_stepped = 0;
    m_queue->expire();
    m_queue->open = m_queue->open == 0;
    m_walkQueue->expire();
    m_walkQueue->open = m_walkQueue->open == 0;
    pose->setRootScale(1.0f, 1.0f, 1.0f, 1.0f);
    if (0.0f < m_ba8)
        m_ba8 -= clock->getDeltaSeconds();
    memset(m_moveStep, 0, sizeof(m_moveStep));
    memset(m_turnStep, 0, sizeof(m_turnStep));
    m_always->onUpdate();

    const bool scenarioLead = owner->isPlayer() && owner->isScenarioLead();
    if (scenarioLead && owner->getAltSoundFlag() != 0 && owner->m_altAction == 0)
        return;
    if (motionId != Motion_None && activeLink != nullptr)
        activeLink->update();
}

// FS2 0x001e66c0
// callcheck: strcmp, strcmp

void PlayerMotion::refresh()
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    const char *lower = lowerLabel(motionId);
    const char *upper = upperLabel(motionId);
    character->swapWeapons();
    const char *newLower = lowerLabel(motionId);
    const char *newUpper = upperLabel(motionId);
    if (lower == nullptr || newLower == nullptr)        // PC: no motion yet
        return;
    if (strcmp(lower, newLower) == 0
        && strcmp(upper != nullptr ? upper : "", newUpper != nullptr ? newUpper : "") == 0)
        return;
    if (motionId != pickStance(MotionStance_WalkRun))
        m_forceStart = 1;
}

// FS2 0x001e6d00

bool PlayerMotion::isQueued() const
{
    return m_queue->motion != Motion_None || m_queue->last != Motion_None;
}

// FS2 0x001e6d30

bool PlayerMotion::isWalkBusy() const
{
    return m_walking != 0 || m_walkQueue->last != Motion_None;
}

// FS2 0x001e6d60

bool PlayerMotion::arbitrate()
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    const auto storeRequest = [&] {
        m_queue->store(m_request, character, motionId);
        m_savedWalk = m_walkPending;
        m_walkPending = 0;
    };
    const auto takeQueued = [&] {
        m_request = m_queue->motion;
        m_queue->restore(character);
        m_walkPending = m_savedWalk;
        m_queue->close(false);
    };
    // The pair's other half, when wanted is the pair.
    const auto otherHalf = [&](int wanted) -> StageObject * {
        if (wanted != Motion_Pair)
            return nullptr;
        if (character->isPlayer())
            return gStageScene->partner;
        return gStageScene->player;
    };

    if (m_request == Motion_None || motionId == m_request) {
        if (motionId != Motion_PairWait)
            return false;
        const int wanted = m_queue->motion == Motion_Pair ? Motion_Pair : Motion_None;
        StageObject *other = otherHalf(wanted);
        if (other == nullptr) {
            m_queue->close(true);
            requestNow(Motion_Stand);
            return true;
        }
        const PlayerMotion *theirs = other->m_motion;
        if (theirs->motionId == wanted || theirs->m_walkQueue->motion == wanted
            || (theirs->m_stepped == 0 && theirs->motionId == Motion_PairWait)) {
            takeQueued();
            return true;
        }
        if (theirs->m_queue->motion == Motion_None) {
            m_queue->close(true);
            requestNow(Motion_Stand);
            return true;
        }
        return false;
    }

    if (m_queue->motion != Motion_None) {
        bool go = false;
        if (motionId == Motion_CrouchDown) {
            if (m_request == Motion_Crouched || m_request == 0x1e || m_request == 0x1d)
                go = isCrouchSide(m_queue->motion);
        }
        else if (motionId == Motion_StandUp) {
            switch (m_request) {
            case 0xf8: case 0xf7: case 0xf6: case 0xf5: case 0xf4: case 1: case 0:
                go = !isCrouchSide(m_queue->motion);
                break;
            }
        }
        if (!go)
            m_queue->close(true);
        else if (m_queue->motion == Motion_Pair)
            m_request = Motion_PairWait;
        else
            takeQueued();
        return go;
    }

    bool moved = false;
    if (character->m_abilities.flag3a6 != 0) {
        const bool wantCrouch = isCrouchSide(m_request);
        const bool crouched = isCrouchSide(motionId);
        if (wantCrouch != crouched && !(isMovingMotion(m_request) && isMovingMotion(motionId))) {
            if (!wantCrouch) {
                if (isStandableMotion(m_request)) {
                    moved = true;
                    storeRequest();
                    m_request = Motion_StandUp;
                }
            }
            else if (m_request == Motion_Crouched) {
                moved = true;
                storeRequest();
                m_request = Motion_CrouchDown;
            }
        }
    }
    if (!moved) {
        const int wanted = m_request == Motion_Pair ? Motion_Pair : Motion_None;
        if (StageObject *other = otherHalf(wanted)) {
            const PlayerMotion *theirs = other->m_motion;
            bool go = false;
            if (theirs->m_stepped == 0) {
                if (theirs->m_request == wanted && theirs->m_request == Motion_Pair)
                    go = isCrouchSide(theirs->motionId);
            }
            else if (theirs->m_queue->motion != Motion_None) {
                go = true;
            }
            if (go) {
                storeRequest();
                moved = true;
                m_request = Motion_PairWait;
            }
        }
    }
    return moved;
}

// FS2 0x001e8320

int PlayerMotion::resolveHold(int *out, int id) const
{
    *out = id;
    if (m_holds[0] == Motion_None)
        return 0;
    if (m_holds[0] == id)
        return 1;
    if (m_holds[1] == id) {
        *out = m_holds[1];
        return 1;
    }
    if (!isHoldableMotion(id))
        return -1;
    const bool crouch = isCrouchSide(id);
    *out = m_holds[0];
    if (crouch != isCrouchSide(m_holds[0]) && m_holds[1] != Motion_None && crouch == isCrouchSide(m_holds[1]))
        *out = m_holds[1];
    return 1;
}

// FS2 0x001e8870

void PlayerMotion::stepFace(bool immediate)
{
    if (m_hasFace == 0 || m_stepsOn[3] == 0)
        return;
    if (motionId == Motion_None)       // PC: the ROM reads the word before the table
        return;

    const bool plainMotion = motionId == 0xb0 || motionId == Motion_Idle1 || motionId == Motion_Idle0 || motionId == Motion_CrouchIdle;
    const char *face;
    if (!plainMotion && gStageScene->sound->hasCue(owner))
        face = m_faces[0xff];
    else if (owner->isPlayer() && owner->isScenarioLead() && owner->getAltSoundFlag() != 0 && owner->m_altAction == 0)
        face = m_faces[0x11];
    else
        face = m_faces[motionId];

    if (face == nullptr || (m_faceName != nullptr && strcmp(m_faceName, face) == 0))
        return;
    if (ModelMotion *motion = owner->m_model->findMotion(2, face))
        owner->m_model->playMotion(motion, true, true, immediate ? PosePlay_FromKey : 0, 0);
    m_faceName = face;
}

// FS2 0x001e8a30
// callcheck: getStageSelect (stageObjRecord)

void PlayerMotion::stepUpper(bool immediate)
{
    if (m_hasHands == 0 || m_stepsOn[2] == 0)
        return;
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    const u_char *busy = motionId >= 0 && motionId < kMotionDefCount ? m_handsBusy[motionId] : nullptr;
    if (busy != nullptr) {
        if (busy[0] != 0)
            m_handNames[0] = nullptr;
        if (busy[1] != 0)
            m_handNames[1] = nullptr;
    }

    const StageObjRecord *sheet = stageObjRecord(character->m_itemDef);
    const char *names[2];
    if (sheet->kind == 0xa) {
        names[0] = "l_gun";
        names[1] = "r_weapon";
    }
    else {
        StageObject *weapon = character->m_weapons[0];
        const ItemDef *def = weapon != nullptr ? weaponDef(weapon) : nullptr;
        const bool melee = def != nullptr && (def->kind == ItemKind_MeleeLight || def->kind == ItemKind_MeleeHeavy);
        const bool pistol = def != nullptr && def->isPistol();
        const bool rifle = def != nullptr && def->isRifle();
        const bool aiming = isArmedMotion(motionId) && !isArmedLowMotion(motionId);

        if (character->m_lampItem != nullptr && character->m_lampItem->m_isTorch2 == 0)
            names[0] = "l_torch";
        else if (busy != nullptr && busy[0] != 0)
            names[0] = nullptr;
        else if (melee)
            names[0] = "l_def";
        else if (pistol && aiming)
            names[0] = "l_gun";
        else if (rifle)
            names[0] = "l_rifle";
        else if (sheet->unarmedCombo != 0 && aiming)
            names[0] = "l_fist";
        else
            names[0] = "l_def";

        if (melee)
            names[1] = "r_weapon";
        else if (pistol)
            names[1] = "r_gun";
        else if (rifle)
            names[1] = "r_rifle";
        else if (busy != nullptr && busy[1] != 0)
            names[1] = nullptr;
        else if (sheet->unarmedCombo != 0 && aiming)
            names[1] = "r_fist";
        else
            names[1] = "r_def";
    }

    for (int i = 0; i < 2; i++) {
        if (names[i] == nullptr || (m_handNames[i] != nullptr && strcmp(m_handNames[i], names[i]) == 0))
            continue;
        if (ModelMotion *motion = character->m_model->findMotion(1, names[i]))
            character->m_model->playMotion(motion, true, true, immediate ? PosePlay_FromKey : 0, 0);
        m_handNames[i] = names[i];
    }
}

const char *PlayerMotion::lowerLabel(int id) const
{
    const MotionDef *def = motionDef(id);
    StageObject *weapon = owner->m_weapons[0];
    if (weapon != nullptr && def != nullptr && weaponDef(weapon)->isRifle()) {
        const char *label = def->rifleLabelText[m_weaponClass];
        if (label != nullptr && label[0] != '*')
            return label;
    }
    return def != nullptr ? def->labelText : nullptr;
}

const char *PlayerMotion::upperLabel(int id) const
{
    const MotionDef *def = motionDef(id);
    StageObject *weapon = owner->m_weapons[0];
    if (weapon != nullptr && def != nullptr && weaponDef(weapon)->isRifle()) {
        const char *label = def->rifleLabelText[m_weaponClass];
        if (label != nullptr && label[0] == '*')
            return label + 1;
    }
    return def != nullptr ? def->upperLabelText : nullptr;
}

MotionLink *PlayerMotion::findLink(const MotionDef *def) const
{
    if (def == nullptr || def->linkFunctionText == nullptr)
        return nullptr;
    auto found = m_links.find(def->linkFunctionText);
    return found != m_links.end() ? found->second : nullptr;
}

const MotionDef *PlayerMotion::startable(int id) const
{
    const MotionDef *def = motionDef(id);
    if (def == nullptr || (def->sameRequest == 0 && m_forceStart == 0 && motionId == id))
        return nullptr;
    return def;
}

void PlayerMotion::cancelRequested()
{
    int id;
    resolveHold(&id, m_request);
    if (const MotionDef *def = startable(id)) {
        if (MotionLink *link = findLink(def))
            link->onCancel();
        static_cast<StageCharacter *>(owner)->noteMotion(id, 4);
    }
}

// FS2 0x001e9dd0
// callcheck: FUN_001ef260, getStageSelect, isBoneEnding (stageObjRecord, isEnding)

void PlayerMotion::stepLower(bool immediate)
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    StageModel *model = character->m_model;

    const MotionDef *current = motionDef(motionId);
    if (current != nullptr && current->nextMotion != -1 && isEnding())
        request(current->nextMotion);

    int resolved;
    const int held = resolveHold(&resolved, m_request);
    if (held == 1) {
        m_request = resolved;
    }
    else if (held == -1) {
        m_holds[0] = Motion_None;
        m_holds[1] = Motion_None;
    }

    if (!immediate)
        arbitrate();

    if (m_walkPending != 0) {
        if (m_walkToTarget == 0)
            startWalk(m_walkRate, m_hasWalkFrom != 0 ? m_walkFrom : nullptr,
                      m_hasWalkTo != 0 ? m_walkTo : nullptr, m_walkScaled != 0, m_walkHeldNext != 0);
        else
            startWalkAt(m_walkRate, m_walkTarget, m_walkOffset, m_hasWalkEnd != 0 ? m_walkEnd : nullptr,
                        m_walkScaled != 0, m_walkHeldNext != 0);
        if (m_walking != 0 && m_walkHeld == 0) {
            m_walkQueue->store(m_request, character, motionId);
            m_request = isCrouchMotion(motionId) ? Motion_WalkCrouch : Motion_WalkStand;
        }
        m_walkPending = 0;
    }
    if (m_walking != 0 && !model->pose->isWalking()) {
        m_walking = 0;
        if (m_walkHeld == 0 && m_request == Motion_None) {
            m_request = m_walkQueue->motion;
            m_walkQueue->restore(character);
            m_walkQueue->close(false);
        }
    }

    if (const MotionDef *def = startable(m_request)) {
        MotionLink *link = findLink(def);
        if (motionId != Motion_None) {
            if (activeLink != nullptr && activeLink != link)
                activeLink->onLeave(link);
            character->m_ground->setContact(0);
        }
        noteLeaving();
        m_endNoted = 0;
        if ((m_walking == 0 || m_walkHeld != 0) && m_queue->motion == Motion_None) {
            const int last = m_walkQueue->last;
            if (last == Motion_None || m_walkQueue->from != last)
                character->noteMotion(m_request, 1);
        }
        if (link != nullptr)
            link->onStart();

        u_int flags = PosePlay_NoBlend;
        if (immediate)
            flags |= PosePlay_FromKey;
        if (character->m_abilities.runStart != 0) {
            if ((motionId == pickStance(MotionStance_WalkRun) && m_request == pickStance(MotionStance_Run))
                || (motionId == Motion_CrouchWalk && m_request == pickStance(MotionStance_Walk)))
                flags |= PosePlay_AtStart;
        }
        const StageObjRecord *sheet = stageObjRecord(character->m_itemDef);
        if (sheet->kind == 7 && character->m_endurance < 1)
            flags |= PosePlay_Grow;

        bool loop = def->waitForEnd == 0;
        if ((m_request == 0xb3 || m_request == 0xb4) && !character->isPlayer() && !character->isPartner()
            && sheet->kind != 7)
            loop = false;

        if (ModelMotion *motion = model->findMotion(0, lowerLabel(m_request)))
            model->playMotion(motion, loop, false, flags, 0);
        const char *upper = upperLabel(m_request);
        if (upper != nullptr) {
            if (ModelMotion *motion = model->findMotion(0, upper))
                model->playMotion(motion, loop, false, flags, 0x200);
        }

        motionId = m_request;
        m_motionTime = 0.0f;
        m_b2c = 0.0f;
        activeLink = link;
        if (activeLink != nullptr)
            activeLink->rewind();
        m_b30 = 0;
        m_forceStart = 0;

        // What the ground checks: the motion's; a queued motion's wall.
        const MotionDef *shown = motionDef(motionId);
        const MotionDef *wall = shown;
        if (m_queue->motion != Motion_None)
            wall = motionDef(m_queue->motion);
        if (m_walkQueue->motion != Motion_None)
            wall = motionDef(m_walkQueue->motion);
        m_wallCheck = wall->wallCheck;
        CharacterGround *ground = character->m_ground;
        if (ground->wallBody != nullptr)
            ground->wallBody->on = wall->wallCheck;
        ground->groundCheck = shown->groundCheck;
        ground->repelLevel = wall->repelLevel;
    }

    m_request = Motion_None;
    m_queue->expire();
    m_queue->open = 1;
    m_walkQueue->expire();
    m_walkQueue->open = 1;
    m_stepped = 1;
}

// FS2 0x001ec0a0
// callcheck: FUN_001ef260, isBoneEnding (isEnding)

void PlayerMotion::noteLeaving()
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    if (m_request == Motion_PairWait && motionId != m_queue->from)
        return;

    if (isEnding()) {
        int id = m_queue->last;
        if (id == Motion_None) {
            id = m_walkQueue->last;
            if (id == Motion_None)
                id = motionId;
            else if (m_walkQueue->finished == 0)
                return;
        }
        else if (m_queue->finished == 0) {
            return;
        }
        if (id == m_walkQueue->motion)
            return;
        if (m_endNoted == 0 && id == motionId)
            noteEnding();
        if (MotionLink *link = findLink(motionDef(id)))
            link->onCancel();
        character->noteMotion(id, id == motionId ? 3 : 4);
        return;
    }

    if (motionId == Motion_None)
        return;
    int id = motionId;
    bool done = true;
    if (m_queue->last != Motion_None) {
        id = m_queue->last;
        done = m_queue->finished != 0;
    }
    else if (m_walkQueue->last != Motion_None) {
        id = m_walkQueue->last;
        done = m_walkQueue->finished != 0;
    }
    else if (motionId == m_walkQueue->motion) {
        return;
    }
    if (!done)
        return;
    if (MotionLink *link = findLink(motionDef(id)))
        link->onCancel();
    character->noteMotion(id, 4);
}

// FS2 0x001ec080

void PlayerMotion::noteEnding()
{
    m_endNoted = 1;
    static_cast<StageCharacter *>(owner)->noteMotion(motionId, 2);
}

// FS2 0x001ec400
// callcheck: FUN_001ef260, isBoneEnding (isEnding)

void PlayerMotion::startRequested()
{
    if (m_canStart == 0)
        return;
    if (((m_walking != 0 && m_walkHeld == 0) || m_queue->motion != Motion_None) && isEnding()) {
        const MotionDef *current = motionDef(motionId);
        if (current != nullptr && m_request == current->nextMotion)
            return;
    }
    int id;
    resolveHold(&id, m_request);
    if (const MotionDef *def = startable(id)) {
        if (MotionLink *link = findLink(def))
            link->onRequest();
        static_cast<StageCharacter *>(owner)->noteMotion(id, 0);
    }
}

// FS2 0x001ec650
// callcheck: FUN_001ef260, noteMotion, resolveHold (cancelRequested)

void PlayerMotion::requestNow(int id)
{
    cancelRequested();
    m_request = id;
    startRequested();
    m_walkPending = 0;
    if (m_walking != 0) {
        owner->m_model->pose->stopWalk();
        m_walking = 0;
        m_walkQueue->close(true);
    }
}

// FS2 0x001ec7a0

void PlayerMotion::request(int id)
{
    if (m_request == Motion_None) {
        m_request = id;
        startRequested();
    }
}

// FS2 0x001eb150

bool PlayerMotion::isEnding() const
{
    if (activeLink != nullptr && activeLink->finished != 0)
        return true;
    const MotionDef *def = motionDef(motionId);
    return def != nullptr && def->waitForEnd != 0 && owner->m_model->pose->isBoneEnding(nullptr);
}

// FS2 0x001eb210

void *PlayerMotion::requestedFile()
{
    ModelMotion *motion = owner->m_model->findMotion(0, lowerLabel(m_request));
    return motion != nullptr ? motion->file : nullptr;
}

// FS2 0x001eb660
// callcheck: getStageSelect (stageObjRecord)

int PlayerMotion::pickStance(int slot)
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    const int stance = isCrouchMotion(motionId) ? 1 : 0;
    if (character->m_winded != 0) {
        const int kind = stageObjRecord(character->m_itemDef)->kind;
        if (kind == 0 || kind == 1)
            return kStanceTableDown[slot][stance];
        return kStanceTable[slot][stance];
    }
    if (character->isYriOrKne() && m_stanceFlag != 0)
        return kStanceTableAlt[slot][stance];
    return kStanceTable[slot][stance];
}

// FS2 0x0021a960

void StageCharacter::noteMotion(int id, int event)
{
    CharacterBlock7c8 &history = m_block7c8;
    if (event == 0) {
        MotionHistoryEntry &entry = history.entries[history.next];
        entry.id = id;
        entry.events = 1;
        entry.frame = gStageScene->getMissionFrames();
        history.next = (history.next + 1) & (kMotionHistory - 1);
        if (history.count < kMotionHistory)
            history.count++;
        return;
    }
    for (u_int i = 0; i < history.count; i++) {
        MotionHistoryEntry &entry = history.entries[(history.next - i + kMotionHistory) & (kMotionHistory - 1)];
        if (entry.id == id) {
            entry.events |= 1u << event;
            return;
        }
    }
}

// Constants of what follows; the masks are collision flag sets the ROM
// spells out.
constexpr float kAimRate = 0.07f;
constexpr float kFootRate = 0.2f;
constexpr float kFrame30Hz = 0.033333335f;
constexpr u_int kFaceFloor     = 0x42400c;  // what a foot stands on
constexpr u_int kFaceBodyFloor = 0x242400c; // what the body leans along
constexpr u_int kFaceNoLean    = 0x2000000;
constexpr u_int kMotionHitLimbs = MotionEvent_HitRightHand | MotionEvent_HitLeftHand | MotionEvent_HitRightLeg
    | MotionEvent_HitLeftLeg | MotionEvent_HitHead | MotionEvent_HitPelvis;
constexpr int kStageObjKindLone = 0xf;      // a record's kind that fidgets with no health

// The named bone, or the model's root.
static StageNodeBase *findFoot(StageModel *model, const char *name)
{
    BoneMap::Node *node = model->boneMap.find(name);
    if (node == nullptr)
        node = model->boneMap.find("bn_base");
    return node->value;
}

// in = in * R(angles), row by row. FS2 0x00135990
// callcheck: FUN_00135a90 (rotationMatrixXyz)
static void rotateByAngles(sceVu0FMATRIX out, const sceVu0FVECTOR *in, const sceVu0FVECTOR angles)
{
    sceVu0FMATRIX turn;
    rotationMatrixXyz(turn, angles);
    multiplyMatrix(out, in, turn);
}

// FS2 0x001e3e80
// callcheck: applyMatrix, randNext, vecCross, vecDirection, setHitBone, playLadderStep, playStepSound, setAimDirection, getNormal, segmentHitsAny, raycastObject, setFootTargets, getFootPositions, raycast, getLowerNames, contains, setWindOn, getDeltaSeconds, requestNow, rollIdleDelay, isStageDrained, isMotionSlowed, getAltSoundFlag, isPlayer, getTopView, setContact, find, getStageSelect (stepEvents, stepAim, stepFeet, stepWind, stepHealth, stepFidgets: one function in the ROM)

void PlayerMotion::controlStep()
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    CharacterGround *ground = character->m_ground;
    const MotionDef *current = motionDef(motionId);
    if (current != nullptr) {
        const MotionDef *wall = current;
        if (m_queue->motion != Motion_None)
            wall = motionDef(m_queue->motion);
        if (m_walkQueue->motion != Motion_None)
            wall = motionDef(m_walkQueue->motion);
        m_wallCheck = wall->wallCheck;
        if (ground->wallBody != nullptr)
            ground->wallBody->on = wall->wallCheck;
        ground->groundCheck = current->groundCheck;
        ground->repelLevel = wall->repelLevel;
        m_always->onApply();
        if (activeLink != nullptr)
            activeLink->apply();
        if (activeLink != nullptr)
            activeLink->onRun();
    }

    const bool footSolve = stepEvents();

    // Off the ground a motion that checks it falls.
    constexpr float kFallPerFrame = 120.0f;
    if (ground->isGrounded()) {
        m_fallSpeed = 0.0f;
    } else if (ground->groundCheck == 1) {
        const float seconds = character->m_clock->getDeltaSeconds();
        m_fallSpeed += kFallPerFrame / (float)(int)(1.0f / seconds);
        m_moveStep[2] = -m_fallSpeed;
    }

    stepAim();
    stepFeet(footSolve);
    stepWind();
    stepHealth();
}

// What a motion's events do: a hit's bone and contact, a foot's sound.
// Answers the motion's IK flag: stepFeet needs it. controlStep, in the ROM.

bool PlayerMotion::stepEvents()
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    CharacterGround *ground = character->m_ground;
    StageModel *model = character->m_model;
    const MotionDef *current = motionDef(motionId);
    if (current == nullptr)
        return false;

    if (activeLink != nullptr) {
        const u_int events = model->m_motionEvents;
        if ((events & kMotionHitLimbs) != 0) {
            if ((events & MotionEvent_HitRightHand) != 0)
                ground->setHitBone("bn_r_hand");
            else if ((events & MotionEvent_HitLeftHand) != 0)
                ground->setHitBone("bn_l_hand");
            else if ((events & MotionEvent_HitRightLeg) != 0)
                ground->setHitBone("bn_r_foot");
            else if ((events & MotionEvent_HitLeftLeg) != 0)
                ground->setHitBone("bn_l_foot");
            else if ((events & MotionEvent_HitHead) != 0)
                ground->setHitBone("bn_head");
            else if ((events & MotionEvent_HitPelvis) != 0)
                ground->setHitBone("bn_pelvis");
            ground->setContact(1);
        } else if ((events & MotionEvent_HitEnd) != 0) {
            ground->setContact(0);
        }
    }

    StageNodeBase *foot = nullptr;
    if ((model->m_motionEvents & MotionEvent_StepLeft) != 0)
        foot = findFoot(model, "bn_l_foot");
    if ((model->m_motionEvents & MotionEvent_StepRight) != 0)
        foot = findFoot(model, "bn_r_foot");
    if (foot != nullptr) {
        if (isLadderMotion(motionId)) {
            character->playLadderStep();
        } else if (StageCollisionFace *floor = ground->floorFace) {
            const int id = motionId;
            int pace = 1;
            if (isCrouchWalkMotion(id))
                pace = 0;
            else if (isRunMotion(id))
                pace = 3;
            else if (isCrouchRunMotion(id))
                pace = 2;
            character->playStepSound((signed char)floor->surface, pace, foot->getWorldMatrix()[3]);
        }
    }
    return current->ik != 0;
}

// The pelvis leans to the floor. A character that walks on an object looks
// along it from a point of its own instead of along the face it stands on.
// controlStep, in the ROM.

void PlayerMotion::stepAim()
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    CharacterGround *ground = character->m_ground;
    ModelPose *pose = character->m_model->pose;
    StageCollisionFace *floor = ground->floorFace;
    if ((m_kindFlags6 == 0 && (m_kindFlags7 == 0 || m_b54 == 0)) || floor == nullptr) {
        pose->setAimDirection(kAimRate, nullptr);
        return;
    }

    sceVu0FVECTOR normal;
    floor->getNormal(normal);
    if (m_kindFlags8 != 0 || m_kindFlags7 != 0) {
        sceVu0FVECTOR point;
        if (m_kindFlags8 == 0)
            memcpy(point, pose->neckBone->matrix[3], sizeof(point));
        else
            applyMatrix(point, character->getWorldMatrix(), m_kindVector);
        const float *position = character->m_xform.position;
        const bool blocked = m_kindFlags7 != 0
            && gStageScene->segmentHitsAny(position, point, 0, kFaceWall, kFaceFlagsTrip, nullptr);
        if (!blocked) {
            const sceVu0FVECTOR above = { point[0], point[1], position[2] + 150.0f, point[3] };
            const sceVu0FVECTOR below = { point[0], point[1], position[2] - 150.0f, point[3] };
            StageObject *object = nullptr;
            sceVu0FVECTOR hit;
            StageCollisionFace *face = gStageScene->raycastObject(&object, hit, above, below, 0, kFaceBodyFloor,
                                                                  kFaceFlagsTrip, nullptr);
            if (face != nullptr && ground->floorObject == object && (face->flags & kFaceNoLean) == 0) {
                const sceVu0FVECTOR up = { 0.0f, 0.0f, 1.0f, 0.0f };   /* data 0x2f8640 */
                sceVu0FVECTOR along, side;
                vecDirection(along, hit, position);
                vecCross(side, along, up);
                vecCross(normal, side, along);
            }
        }
    }
    pose->setAimDirection(kAimRate, normal);
}

// Each foot takes the plane under it, or lets go. controlStep, in the ROM.

void PlayerMotion::stepFeet(bool solve)
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    ModelPose *pose = character->m_model->pose;
    if (m_kindFlags3 == 0 || m_stepsOn[0] == 0 || !solve || !character->m_ground->isGrounded()) {
        pose->setFootTargets(kFootRate, nullptr, nullptr);
        return;
    }

    constexpr float kFootReach = 50.0f;
    const float height = character->m_xform.position[2];
    sceVu0FVECTOR feet[2], planes[2];
    pose->getFootPositions(feet[0], feet[1]);
    for (int i = 0; i < 2; i++) {
        planes[i][0] = 0.0f;
        planes[i][1] = 0.0f;
        planes[i][2] = 1.0f;
        planes[i][3] = height;
        const sceVu0FVECTOR from = { feet[i][0], feet[i][1], height + kFootReach, feet[i][3] };
        const sceVu0FVECTOR to = { feet[i][0], feet[i][1], height - kFootReach, feet[i][3] };
        sceVu0FVECTOR hit;
        StageCollisionFace *face = gStageScene->raycast(hit, from, to, 0, kFaceFloor, kFaceFlagsTrip, nullptr);
        if (face != nullptr) {
            if ((face->flags & kFaceStairs) == 0)
                face->getNormal(planes[i]);
            planes[i][3] = hit[2];
        }
    }
    pose->setFootTargets(kFootRate, planes[0], planes[1]);
}

// Outside the stage's boxes the wind blows the clothes. controlStep, in the ROM.

void PlayerMotion::stepWind()
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    ModelPose *pose = character->m_model->pose;
    if (!pose->getLowerNames())
        return;
    if (m_stepsOn[1] == 0) {
        pose->setWindOn(0);
        return;
    }
    const float *position = character->m_xform.position;
    const sceVu0FVECTOR point = { position[0], position[1], position[2] + 50.0f, 1.0f };
    pose->setWindOn(gStageScene->nodeTree->contains(point) ? 0 : 1);
}

// The ROM clamps through 0 (sdata 0x31a9cc) and the sheet's value.
static float clampHealth(float value, float limit)
{
    return value < 0.0f ? 0.0f : (limit < value ? limit : value);
}

// What the frame gives back; a winded character jumps to full from 40%.
static void restoreHealth(StageCharacter *character, float gain, float limit)
{
    constexpr float kWindedBack = 0.4f;
    character->m_health = clampHealth(character->m_health + gain, limit);
    if (character->m_winded != 0 && limit * kWindedBack <= character->m_health)
        character->m_health = limit;
}

// 体力: how long a character can run. Kinds 0 and 1 get fidgets once full, a
// character the sheet gives no health to only kind 0xf.
// controlStep, in the ROM; the sheet's value is re-read at each use there.

void PlayerMotion::stepHealth()
{
    constexpr float kRecoveryPerSecond = 0.05f; // of the sheet's value
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    const StageObjRecord *record = stageObjRecord(character->m_itemDef);
    const float limit = (float)record->health;
    if (limit <= 0.0f) {
        if (record->kind != kStageObjKindLone || character->m_fidgets == 0)
            return;
        if (motionId == Motion_Stand && character->m_wants.headOn == 0)
            character->m_idleTime += character->m_clock->getDeltaSeconds();
        else
            character->m_idleTime = 0.0f;
        if (character->m_idleTime < character->m_idleDelay)
            return;
        if (m_request == Motion_Stand) {
            requestNow(Motion_Idle0);
            character->m_idleTime = 0.0f;
            character->rollIdleDelay();
        }
        return;
    }

    const float seconds = character->m_clock->getDeltaSeconds();
    const float frames = seconds / kFrame30Hz;
    const float gain = seconds * limit * kRecoveryPerSecond;
    // 0 by itself, 1 drained by whom the player controls, 2 by a timed change.
    int drain = 0;
    StageObject *controlled = gStageScene->controlled != nullptr
        ? gStageScene->controlled : static_cast<StageObject *>(gStageScene->player);
    if (character->isStageDrained()) {
        if (gStageScene->player != controlled)
            drain = 1;
        else if (character->isMotionSlowed())
            drain = 2;
    }

    if (drain == 2) {
        character->m_health = clampHealth(character->m_health - character->m_slowFrom, limit);
    } else if (drain == 1) {
        const float left = character->m_health
            - (float)static_cast<StageCharacter *>(controlled)->m_drainRate * frames;
        character->m_health = clampHealth(left, limit);
        if (left <= 0.0f)
            gStageScene->altLatch = 1;
    } else {
        const int id = motionId;
        if (isRunMotion(id) || isCrouchRunMotion(id)) {
            character->m_health = clampHealth(character->m_health - frames, limit);
        } else if (record->kind != 0 || isRestingMotion(id) || character->m_gear != nullptr
                   || (character->getAltSoundFlag() != 0 && character->m_altAction == 0)) {
            restoreHealth(character, gain, limit);
        }
    }

    if (character->m_health <= 0.0f) {
        character->m_winded = 1;
        character->m_idleTime = 0.0f;
        return;
    }
    if (character->m_health < limit)
        return;
    if (record->kind == 0 || record->kind == 1)
        stepFidgets(record);
    character->m_winded = 0;
}

// At full health, standing for m_idleDelay: idle0 or idle1, or the crouched
// one. Not with gear, nor for the player seen through the view camera.
// controlStep, in the ROM.

void PlayerMotion::stepFidgets(const StageObjRecord *record)
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    if (character->m_fidgets == 0 || character->m_gear != nullptr)
        return;
    if (character->isPlayer()) {
        StageObject *top = gStageScene->getTopView();
        if (top->getKind() == StageObject_Kind10 && top->m_linked == character)
            return;
    }
    if (character->m_winded != 0)
        return;

    if ((motionId == Motion_CrouchStand || motionId == Motion_Stand) && character->m_wants.headOn == 0)
        character->m_idleTime += character->m_clock->getDeltaSeconds();
    else
        character->m_idleTime = 0.0f;
    if (character->m_idleTime < character->m_idleDelay)
        return;
    if (m_request == Motion_CrouchStand) {
        requestNow(Motion_CrouchIdle);
    } else if (m_request == Motion_Stand) {
        requestNow((u_char)randNext() < 0x7f ? Motion_Idle0 : Motion_Idle1);
    } else {
        return;
    }
    character->m_idleTime = 0.0f;
    character->rollIdleDelay();
}

// FS2 0x001e60b0
// callcheck: identityMatrix, rotateByAngles, rotateByAngles, rotateMatrixZ, find, translateMatrix

void PlayerMotion::getLampMatrix(sceVu0FMATRIX out)
{
    constexpr float kPi = 3.1415927f;
    identityMatrix(out);
    rotateByAngles(out, out, m_lampAngles);
    rotateByAngles(out, out, owner->m_xform.rotation);
    rotateMatrixZ(kPi, out, out);
    translateMatrix(out, out, owner->m_model->boneMap.find("bn_head")->value->getMatrix()[3]);
}

// FS2 0x001e6160
// callcheck: applyMatrix, applyMatrix, find, find, getTopView, getTopView, getTopView, getTopView, identityMatrix, rotateByAngles, rotateByAngles, isViewLinked, raycast, rotateMatrixZ, scaleVec4, translateMatrix, vecDirection, vecDistance (getLampMatrix inline)

void PlayerMotion::getLampTarget(sceVu0FVECTOR out)
{
    constexpr float kNearest = 200.0f;
    constexpr u_int kFaceLamp = 0xe42600f;
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    if (character->m_lampItem == nullptr)
        return;

    const sceVu0FVECTOR reach = { 0.0f, character->m_lampItem->m_lamp->m_spot->getRadius(), 0.0f, 1.0f };
    sceVu0FVECTOR from, to;
    if (character->isViewLinked()) {
        StageObject *top = gStageScene->getTopView();
        const int kind = top->getKind();
        if (kind == StageObject_Kind10 || kind == StageObject_Kind11)
            memcpy(from, top->getWorldMatrix()[3], sizeof(from));
        else
            memcpy(from, character->m_model->boneMap.find("bn_head")->value->getMatrix()[3], sizeof(from));
        applyMatrix(to, top->getWorldMatrix(), reach);
    } else {
        sceVu0FMATRIX lamp;
        getLampMatrix(lamp);
        memcpy(from, character->m_model->boneMap.find("bn_head")->value->getMatrix()[3], sizeof(from));
        applyMatrix(to, lamp, reach);
    }

    if (gStageScene->raycast(out, from, to, 0, kFaceLamp, kFaceFlagsTrip, nullptr) == nullptr)
        memcpy(out, to, sizeof(to));
    if (vecDistance(from, out) < kNearest) {
        sceVu0FVECTOR direction;
        vecDirection(direction, out, from);
        scaleVec4(kNearest, direction, direction);
        for (int i = 0; i < 4; i++)
            out[i] = from[i] + direction[i];
    }
}

// The tail of startWalk and startWalkAt.
// callcheck: atan2Octant, addAngle, vecDistance, getDeltaSeconds, getDeltaSeconds, getDeltaSeconds, startWalk

void PlayerMotion::walkPoseTo(float rate, const float *target, const float *aimAt, bool scaled)
{
    constexpr float kTurnPerRate = 0.61086524f;  // 35 degrees
    constexpr float kWalkPerRate = 50.0f;
    constexpr float kFewestFrames = 4.0f;
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    const float seconds = character->m_clock->getDeltaSeconds();
    if (scaled) {
        const float yaw = atan2Octant(aimAt[0] - target[0], -(aimAt[1] - target[1]));
        const float walk = vecDistance(target, character->m_xform.position) / kWalkPerRate;
        const float turn = fabsf(addAngle(yaw, -character->m_xform.rotation[2])) / kTurnPerRate;
        rate *= turn <= walk ? walk : turn;
        if (rate < seconds * kFewestFrames)
            rate = seconds * kFewestFrames;
    }
    character->m_model->pose->startWalk(rate / seconds, target, aimAt);
}

// FS2 0x001ead00
// callcheck: getWalkStart, atan2Octant, addAngle, vecDistance, getDeltaSeconds, getDeltaSeconds, getDeltaSeconds, startWalk (walkPoseTo)

void PlayerMotion::startWalk(float rate, const float *from, const float *to, bool scaled, bool held)
{
    m_walking = 1;
    m_walkHeld = held;
    sceVu0FVECTOR target, aimAt;
    owner->m_model->pose->getWalkStart(target, aimAt, from, to);
    walkPoseTo(rate, target, aimAt, scaled);
}

// FS2 0x001eaea0
// callcheck: identityMatrix, rotateMatrixZ, applyMatrix, getRecordOffset, atan2Octant, addAngle, vecDistance, getDeltaSeconds, getDeltaSeconds, getDeltaSeconds, startWalk (walkPoseTo)

void PlayerMotion::startWalkAt(float rate, void *target, const float *offset, const float *to, bool scaled, bool held)
{
    m_walking = 1;
    m_walkHeld = held;
    sceVu0FVECTOR ahead = { 0.0f, 100.0f, 0.0f, 0.0f };
    if (to != nullptr) {
        sceVu0FMATRIX turn;
        identityMatrix(turn);
        rotateMatrixZ(to[2], turn, turn);
        applyMatrix(ahead, turn, ahead);
    }
    sceVu0FVECTOR walkTarget, aimAt;
    owner->m_model->pose->getRecordOffset(walkTarget, aimAt, static_cast<const PoseBoneRecord *>(target), offset, ahead);
    walkPoseTo(rate, walkTarget, aimAt, scaled);
}

// FS2 0x001eb550
// callcheck: strcmp (weaponDef)

int PlayerMotion::pickGunMotion(int slot) const
{
    static const int kGunMotions[][3] = {       /* data 0x2f7ef0 */
        { 106, 122, 137 }, { 108, 123, 138 }, { 109, 124, 139 }, { 110, 125, 140 },
        { 111, 126, 141 }, { 112, 127, 142 }, { 113, 128, 143 }, { 114, 129, 144 },
        { 115, 130, 145 }, { 116, 131, 131 }, { 117, 132, 132 }, { 118, 133, 133 },
        { 119, 134, 134 }, { 120, 135, 146 }, { 121, 136, 147 }, { 0, 0, 0 },
    };
    const StageObject *gun = owner->m_weapons[0];
    if (gun == nullptr)
        return Motion_None;
    int gunClass = 0;
    const int kind = weaponDef(gun)->kind;
    if (kind == ItemKind_RifleFirst || kind == ItemKind_FirearmLast)
        gunClass = strcmp(gun->m_name, "R_MINIMI") == 0 ? 2 : 1;
    return kGunMotions[slot][gunClass];
}

// FS2 0x001eb520

int PlayerMotion::pickArmedMotion(int slot) const
{
    static const int kArmedMotions[][2] = {     /* data 0x2f7fb0 */
        { 77, 78 }, { 79, 80 }, { 81, 81 }, { 82, 82 }, { 83, 83 },
        { 84, 84 }, { 85, 87 }, { 86, 88 }, { 104, 105 },
    };
    return kArmedMotions[slot][owner->m_weapons[0] != nullptr ? 1 : 0];
}

// FS2 0x00215940
// callcheck: getSaveData, getStageSelect (stageObjRecord; getSaveData's +0x4bc is getStageSelect's record)

bool StageCharacter::isStageDrained() const
{
    const int stage = getStageSelect()->stage;
    if (stage == 0x1b || stage == 0x19)
        return stageObjRecord(m_itemDef)->kind == 1;
    if (stage == 0x17 || stage == 0xf)
        return stageObjRecord(m_itemDef)->kind == 0;
    return false;
}

// FS2 0x0020d390
// callcheck: getSeconds

bool StageCharacter::isMotionSlowed()
{
    if (m_slowStart < 0.0f)
        return false;
    return m_clock->getSeconds() - m_slowStart < m_slowEnd - m_slowStart;
}

// FS2 0x002a2100
// callcheck: getStageSelect, isPartner, playNamed, playNamedHeard2 (stageObjRecord)

void StageCharacter::playLadderStep()
{
    const char *name = nullptr;
    switch (stageObjRecord(m_itemDef)->kind) {
    case 0:
        name = "PC_LADDER";
        break;
    case 1:
    case 9:
    case 10:
        name = "NPC_LADDER";
        break;
    case 2:
        name = "SBT_LADDER";
        break;
    case 4:
        name = "YBZ_LADDER";
        break;
    case 8:
        name = "RYO_LADDER";
        break;
    }
    if (name == nullptr)
        return;
    if (isPartner())
        playNamed(name, m_xform.position, nullptr);
    else
        playNamedHeard2(name, m_xform.position, nullptr);
}

// FS2 0x002a49c0
// callcheck: findStepSound, isPartner, playEntry, playEntryHeard

void StageCharacter::playStepSound(int surface, int pace, const float *pos)
{
    const SeEntry *entry = findStepSound(surface, pace);
    if (entry == nullptr)
        return;
    if (isPartner())
        playEntry(entry, pos, nullptr);
    else
        playEntryHeard(entry, pos, nullptr);
}

// The sound names by surface: 0 the surface's own, 1..29 the steps, 30 and
// 31 what hits it.
constexpr int kStepSoundColumns = 32;
constexpr int kSurfaceHitColumn = 31;
static const char *const kStepSounds[][kStepSoundColumns] = {   /* data 0x2fc3b0 */
#include "game/stepsounds.inc"
};
constexpr int kStepSoundRows = sizeof(kStepSounds) / sizeof(kStepSounds[0]);

// PC: the ROM keeps what it found in a table (bss 0x599860, 1 not looked
// up); a search per step costs nothing here and cannot go stale.
static const SeEntry *findSurfaceEntry(int surface, int column)
{
    if (surface >= kStepSoundRows)              // PC: the ROM has no bound
        return nullptr;
    const char *name = kStepSounds[surface][column];
    SeTable *table = gStageScene->sound->m_player->m_table;
    if (name == nullptr || table == nullptr)
        return nullptr;
    return table->find(name);
}

// FS2 0x002cf210
// callcheck: getStageSelect, strcmp (stageObjRecord; findSurfaceEntry: the sound table's find, inline)

const SeEntry *StageCharacter::findStepSound(int surface, int pace) const
{
    // The columns of each family: the first, and whether it has all five
    // paces (H W S R L) or three (W R L). layout: host
    struct StepColumn
    {
        int  first;
        bool fivePaces;
    };
    static const StepColumn kStepColumns[] = {  /* data 0x30f830 */
        { 1, true }, { 6, true }, { 11, false }, { 14, false },
        { 17, false }, { 20, false }, { 23, false }, { 26, false },
    };
    if (surface < 0)
        return nullptr;
    int family;
    switch (stageObjRecord(m_itemDef)->kind) {
    case 0:
        family = 0;
        break;
    case 1:
    case 9:
    case 10:
        family = 1;
        break;
    case 2:
        family = 2;
        break;
    case 4:
        family = 3;
        break;
    case 5:
    case 0xc:
        family = 4;
        break;
    case 6:
        family = 5;
        break;
    case 8:
        family = 6;
        break;
    case 0xf:
        family = 7;
        break;
    default:
        return nullptr;
    }
    if (!kStepColumns[family].fivePaces)
        pace = pace == 4 ? 2 : (pace == 3 || pace == 2 ? 1 : 0);
    return findSurfaceEntry(surface, kStepColumns[family].first + pace);
}

// FS2 0x002cf0b0
// callcheck: strcmp (findSurfaceEntry: the sound table's find, inline)

const SeEntry *getSurfaceSound(int surface)
{
    if (surface < 0)
        return nullptr;
    return findSurfaceEntry(surface, kSurfaceHitColumn);
}

// FS2 0x001e0e20
// callcheck: FUN_00215290, FUN_00289eb0 (m_gear and within, inline here)

void PlayerMotion::applyMovement()
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    CharacterGround *ground = character->m_ground;
    StageModel *model = character->m_model;
    ModelPose *pose = model->pose;

    if (isMovingMotion(motionId)) {
        float slopeScale = 1.0f;
        StageCollisionFace *floor = ground->floorFace;
        if (floor != nullptr && (floor->flags & 0x4008) != 0) {
            const int id = motionId;
            if ((floor->flags & kFaceStairs) != 0 && id != 0x1a && id != 0x19 && id != 0x18 && id != 0x17)
                slopeScale = 0.6f;
            sceVu0FVECTOR normal;
            floor->getNormal(normal);
            const sceVu0FVECTOR up = { 0.0f, 0.0f, 1.0f, 0.0f };
            const float flat = vecDot(normal, up);
            sceVu0FVECTOR downhill = { normal[0], normal[1], 0.0f, 0.0f };
            vecNormalize(downhill, downhill);
            // Sideways steps measure the slope across the body.
            const bool sideways = id == 8 || id == 7 || id == 0x8d || id == 0x8c || id == 0x7e || id == 0x7d
                || id == 0x6f || id == 0x6e || id == 0x54 || id == 0x53 || id == 0x23 || id == 0x22;
            const float yaw = sideways ? addAngle(character->m_xform.rotation[2], 1.5707964f)
                                       : character->m_xform.rotation[2];
            sceVu0FVECTOR ahead = { -sinCos(-yaw, 0), -sinCos(-yaw, 1), 0.0f, 0.0f };
            const float along = vecDot(downhill, ahead);
            slopeScale = (1.0f - (1.0f - fabsf(flat)) * fabsf(along)) * slopeScale;
        }

        // A run starts slowly.
        float paceScale = 1.0f;
        const bool running = isRunMotion(motionId) || motionId == 0x2d || motionId == 0x24;
        if (running) {
            const bool ramp = character->m_abilities.runStart == 0
                || !(motionId == 0x24 || motionId == 0x2d || motionId == 9 || motionId == 0x16);
            model->_188 = ramp;
            if (ramp)
                paceScale = m_motionTime * 0.8f + 0.4f;
            if (1.0f < paceScale)
                paceScale = 1.0f;
        }
        pose->setRootScale(slopeScale * paceScale, slopeScale * paceScale, 1.0f, 1.0f);
    }

    if (m_walking == 0 && m_walkQueue->last == Motion_None && 0.0f < ground->slideSpeed) {
        sceVu0FVECTOR slide;
        scaleVec4(ground->slideSpeed * owner->m_clock->getDeltaSeconds() / 0.036f, slide, ground->slideDirection);
        for (int i = 0; i < 4; i++)
            m_moveStep[i] += slide[i];
    }
    for (int i = 0; i < 4; i++)
        character->m_xform.position[i] += m_moveStep[i];
    if (model->m_loop != 0)
        pose->translate(m_moveStep);
    addAngles(character->m_xform.rotation, character->m_xform.rotation, m_turnStep);

    // Off the view a pose runs no solvers.
    bool shown = character->isViewLinked();
    if (!shown) {
        StageObject *top = gStageScene->getTopView();
        const float distance = vecDistance(top->getWorldMatrix()[3], character->getWorldMatrix()[3]);
        StageCamera *camera = static_cast<StageCamera *>(top);
        shown = camera->seesPoint(character->m_xform.position) && distance < camera->m_range + camera->m_range2;
    }
    m_stepsOn[0] = shown;
    m_stepsOn[1] = shown;
    if (shown) {
        pose->m_solvers |= 0x11;
    }
    else {
        pose->m_solvers &= ~0x11u;
        if (pose->leftToe != nullptr)
            pose->leftToe->solveWeight = 0.0f;
    }

    bool near250 = false;
    u_char near300 = 0;
    if (character->m_gear == nullptr) {
        if (StageViewTarget *target = gStageScene->viewTarget) {
            const float *where = target->position;
            const auto within = [&](float range) {
                const float x = where[0] - character->m_xform.position[0];
                const float y = where[1] - character->m_xform.position[1];
                return x * x + y * y < range * range;
            };
            near250 = within(250.0f) || motionId == 0xb6 || motionId == 0xb5;
            near300 = within(300.0f);
        }
    }
    ground->nearTarget = near250;
    character->m_42c = near300;
}

// FS2 0x001e6400
// callcheck: FUN_00215270, FUN_001e6490 (isControlled, leanBodyAt)

void PlayerMotion::leanBody(const sceVu0FVECTOR angles, bool slope, u_char flag)
{
    leanBodyAt(0.3f, static_cast<StageCharacter *>(owner)->isControlled() ? 0.31415927f : 0.05235988f,
               angles, slope, flag);
}

// FS2 0x001e6490

void PlayerMotion::leanBodyAt(float rate, float maxStep, const sceVu0FVECTOR angles, bool slope, u_char flag)
{
    constexpr float kLeanLimit = 1.134464f;     // 65 degrees
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    sceVu0FVECTOR target;
    memcpy(target, angles, sizeof(target));
    for (int i = 0; i <= 2; i += 2) {
        if (kLeanLimit <= target[i])
            target[i] = kLeanLimit;
        else if (target[i] < -kLeanLimit)
            target[i] = -kLeanLimit;
    }

    StageCollisionFace *floor = character->m_ground->floorFace;
    if (slope && floor != nullptr && (floor->flags & 0x4008) != 0) {
        const sceVu0FVECTOR up = { 0.0f, 0.0f, 1.0f, 0.0f };
        sceVu0FVECTOR normal;
        floor->getNormal(normal);
        const float steepness = asinAcos(vecDot(normal, up), 1);
        sceVu0FVECTOR downhill = { normal[0], normal[1], 0.0f, 0.0f };
        vecNormalize(downhill, downhill);
        const float yaw = character->m_xform.rotation[2];
        sceVu0FVECTOR ahead = { -sinCos(-yaw, 0), -sinCos(-yaw, 1), 0.0f, 0.0f };
        target[0] = wrapAngle(target[0] - steepness * vecDot(downhill, ahead)
                              * (1.0f - fabsf(target[0] / kLeanLimit)));
    }
    approachAnglesXZ(rate, maxStep, m_lampAngles, m_lampAngles, target);
    m_leanScale[0] = m_lampAngles[0] / kLeanLimit;
    m_leanScale[1] = 1.0f;
    m_leanScale[2] = m_lampAngles[2] / kLeanLimit;
    m_leanScale[3] = 1.0f;
    m_leanFlag = flag;
}

// FS2 0x001dec30

void PlayerMotion::setToolTarget(const float *target)
{
    owner->m_model->pose->setToolTarget(0.1f, target);
}

// FS2 0x001dec50

void PlayerMotion::setLookTarget(float rate, const float *target)
{
    owner->m_model->pose->setLookTarget(rate, 1.2566371f, target);
}

// FS2 0x001dec70

void PlayerMotion::setHeadTarget(float rate, const float *target)
{
    ModelPose *pose = owner->m_model->pose;
    if (m_modelFamily >= 0 && m_modelFamily < kMotionModelCount) {
        const MotionModel *family = &gMotionModels[m_modelFamily];
        pose->headLimit = family->limits[0] * 0.5f;
        pose->headLimitUp = family->limits[1] * 0.5f;
        pose->headLimitDown = family->limits[2] * 0.5f;
    }
    pose->setHeadTarget(rate, target);
}

// FS2 0x001eabc0

void PlayerMotion::startWalkFile(float rate, void *file, const float *from, const float *to, bool scaled, bool held)
{
    m_walkPending = 1;
    m_walkToTarget = 1;
    m_walkRate = rate;
    m_walkScaled = scaled;
    m_walkHeldNext = held;
    m_walkTarget = file;
    memcpy(m_walkOffset, from, sizeof(m_walkOffset));
    m_hasWalkEnd = to != nullptr;
    if (to != nullptr)
        memcpy(m_walkEnd, to, sizeof(m_walkEnd));
}

// FS2 0x001eac10

void PlayerMotion::startWalkTo(float rate, const float *from, const float *to, bool scaled, bool held)
{
    m_walkPending = 1;
    m_walkToTarget = 0;
    m_walkRate = rate;
    m_walkScaled = scaled;
    m_walkHeldNext = held;
    m_hasWalkFrom = from != nullptr;
    if (from != nullptr)
        memcpy(m_walkFrom, from, sizeof(m_walkFrom));
    m_hasWalkTo = to != nullptr;
    if (to != nullptr)
        memcpy(m_walkTo, to, sizeof(m_walkTo));
}

// FS2 0x001eac70
// callcheck: identityMatrix, rotateMatrixZ, applyMatrix

void PlayerMotion::pointBehind(sceVu0FVECTOR out, const float *place, const float *angles)
{
    sceVu0FMATRIX turn;
    identityMatrix(turn);
    rotateMatrixZ(angles[2], turn, turn);
    const sceVu0FVECTOR behind = { 0.0f, -100.0f, 0.0f, 0.0f };
    applyMatrix(out, turn, behind);
    for (int i = 0; i < 4; i++)
        out[i] += place[i];
}

// FS2 0x00190900
// callcheck: FUN_001933a0, operator_delete, operator_new, BoxGrid (the ROM's inner object is not written: the tree holds the grid)

void StageNodeTree::setBoxes(void *bbx)
{
    delete m_boxes;
    m_boxes = new BoxGrid(static_cast<const BoxSet *>(bbx), 8);
}

// FS2 0x001908d0
// callcheck: contains

bool StageNodeTree::contains(const sceVu0FVECTOR point)
{
    return m_boxes != nullptr && m_boxes->contains(point);
}

// FS2 0x00215270

bool StageCharacter::isControlled() const
{
    const StageObject *driver = gStageScene->controlled != nullptr ? gStageScene->controlled : gStageScene->player;
    return driver == this;
}

// FS2 0x00211970

bool StageCharacter::isTakenOver() const
{
    return gStageScene->controlled == this && gStageScene->controlled != gStageScene->player;
}

// FS2 0x0020d400
// callcheck: getSeconds, getSeconds, getSaveData, getStageSelect, getStageSelect (stageObjRecord)

float StageCharacter::getMotionRate()
{
    if (m_slowStart < 0.0f || !(m_clock->getSeconds() - m_slowStart < m_slowEnd - m_slowStart))
        return 1.0f;
    const int stage = getStageSelect()->stage;
    const int kind = stageObjRecord(m_itemDef)->kind;
    if (((stage == 0x1b || stage == 0x19) && kind == 1) || ((stage == 0x17 || stage == 0xf) && kind == 0))
        return 1.0f;
    return m_slowFrom - (m_slowFrom - m_slowTo)
        * ((m_clock->getSeconds() - m_slowStart) / (m_slowEnd - m_slowStart));
}

// FS2 0x0017a490
// callcheck: FUN_001f3930, getDeltaSeconds, FUN_001e0e20, FUN_0020d400, advance

void StageCharacter::moveStep()
{
    m_motion->applyMovement();
    const int id = m_motion->motionId;
    float rate = 1.0f;
    if (isWalkMotion(id))
        rate = m_walkRate;
    else if (isRunMotion(id) || id == 0x2d || id == 0x24)
        rate = m_runRate;
    else if (id >= Motion_DoorPullOpen && id <= Motion_DoorForce)
        rate = m_motion->doorLink->rate;
    const float frames = m_clock->getDeltaSeconds() / 0.033333335f;
    m_model->advance(rate * getMotionRate() * frames, m_xform.position, m_xform.rotation, 0);
    m_ground->beginFrame();
}

// The frame's start, before the scripts run: what the character may do is
// found again, and what it wants starts empty, so a script's request (a door's
// native 0x87) stands until the control hook below.
// Not written: the rest of the ROM's hook, the kinds 1 and 9's two flags at
// +0x3a8 and +0x3ac, the clocks of a live character (flinch, health, endurance)
// and the player's look-ahead point.
// FS2 0x0017a860

void StageCharacter::sideHook0()
{
    memset(&m_allows, 0, sizeof(m_allows));
    m_motion->initSlots14();
    m_motion->beginFrame();
    memset(&m_wants, 0, sizeof(m_wants));
    m_motion->initSlots18();
}

// The brain (0x1dd2f0) is not written.
// FS2 0x0017a810

void StageCharacter::sideHook1()
{
    m_input->update();
    m_motion->controlStep();
}

// FS2 0x0017df60

void stageUpdateCharacterMotions()
{
    StageObjArray &characters = gStageScene->objects->m_lists[StageObjGroup_Characters];
    for (StageObject **slot = characters.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *character = *slot;
        if (!character->m_viewActive)
            continue;
        PlayerMotion *motion = character->m_motion;
        StageCharacter *body = static_cast<StageCharacter *>(character);
        motion->stepLower(false);
        motion->stepUpper(false);
        motion->stepFace(false);
        // PC: the movement hook runs after this, in actionMovement.
        body->moveStep();
    }
}

// ───────────────────────────────────────
// What a character carries.

namespace {

// Where the hand-held things hang. data 0x306fd0, 0x306ff0, 0x306970.
const char kLeftTool[]  = "bn_l_tool";
const char kRightTool[] = "bn_r_tool";
const char kChest[]     = "bn_spine1";
const char kChestLamp[] = "T_TORCH2";   /* data 0x306fe0: the lamp worn on the chest */

constexpr float kFootHeight = 50.0f;    // the probes' height over a position
constexpr float kFootDepth  = 10.0f;    // and how far under it
constexpr float kDropDepth  = 1000.0f;  // a thing dropped must have a floor this close
constexpr u_int kFaceDropWalls = 0xe8021f3;
constexpr u_int kFaceDropSolid = 0x42400c;
constexpr u_int kFaceDropFloor = 0x400000;
constexpr u_int kFaceSkipBodies = 0x40000;
constexpr u_int kFaceItemSight = 0xe002003;

constexpr float kPickHighAbove = 65.0f; // over this the higher bend
constexpr float kPickWalkFar   = 30.0f; // further than this it turns to the item as it bends
constexpr float kPickWalkRate  = 0.35f;
constexpr int   kMotionPickLow  = 0x12;
constexpr int   kMotionPickHigh = 0x13;

// A sheet of this ctype holds firearms left and keeps its second weapon shown.
constexpr int kSheetKindTwoHands = 10;

StageNodeBase *bodyBone(StageObject *owner, const char *name)
{
    BoneMap::Node *bone = owner->m_model->boneMap.find(name);
    return bone != nullptr ? bone->value : owner;
}

void showCarried(StageObject *item, bool shown)
{
    if (item->m_model != nullptr && item->m_model->data != nullptr)
        item->m_model->data->shown = shown;
}

const ItemDef &defOf(const StageObject *item)
{
    const int id = item->m_itemDef;
    return gItemDefs[id < 0 || id >= kItemDefCount ? 0 : id];
}

} // namespace

// FS2 0x00216400
// callcheck: FUN_0017e260, FUN_0017e1b0, getStageSelect (setParent, detach, stageObjRecord, inline)

void StageCharacter::swapWeapons()
{
    StageObject *held = m_weapons[0];
    m_weapons[0] = m_weapons[1];
    if (m_weapons[0] != nullptr) {
        showCarried(m_weapons[0], true);
        m_weapons[0]->m_tree->setParent(m_tree);
    }
    m_weapons[1] = held;
    if (held != nullptr && stageObjRecord(m_itemDef)->kind != kSheetKindTwoHands) {
        showCarried(held, false);
        held->m_tree->detach();
    }
}

// FS2 0x00216920
// callcheck: FUN_00217730, setShadowOwner, setPosition, StageNodeList__erase, FUN_0017e260, FUN_00217840

bool StageCharacter::dropItem(StageObject *item)
{
    StageList<StageObject *>::Node *node = m_items.head;
    while (node != nullptr && node->value != item)
        node = node->next;
    if (node == nullptr)
        return false;
    if (m_lampItem == item) {
        m_lampItem->setShadowOwner(nullptr);
        m_lampItem = nullptr;
    }
    if (m_weapons[0] == item)
        m_weapons[0] = nullptr;
    if (m_weapons[1] == item)
        m_weapons[1] = nullptr;
    showCarried(item, true);
    item->m_xform.setPosition(m_xform.position);
    item->detach();
    item->m_tree->detach();
    item->m_model->m_lit = 1;
    item->m_model->m_lightSet = -1;
    m_items.erase(node);
    return true;
}

// FS2 0x00216630
// callcheck: dropItem, setMoving, getWorldMatrix, applyMatrix, push, setThrowVector

void StageCharacter::throwItem(StageThrownItem *item, sceVu0FVECTOR offset)
{
    dropItem(item);
    ItemPhysics *physics = item;
    physics->setMoving(true, false);
    if (offset != nullptr) {
        applyMatrix(offset, getWorldMatrix(), offset);
        physics->push(offset, -1);
    }
    physics->m_parent = this;
    item->m_inUse = 1;
    item->setThrowVector(m_xform.position);
}

// FS2 0x002166d0
// callcheck: dropItem, setMoving, getWorldMatrix, applyMatrix, push

void StageCharacter::dropWeapon(StageObject *item, sceVu0FVECTOR offset)
{
    dropItem(item);
    ItemPhysics *physics = static_cast<StageWeaponItem *>(item);
    physics->setMoving(true, false);
    if (offset != nullptr) {
        applyMatrix(offset, getWorldMatrix(), offset);
        physics->push(offset, 0);
    }
    physics->m_parent = this;
}

// FS2 0x00216760
// callcheck: segmentHitsAny, getWorldMatrix, segmentHitsAny, getWorldMatrix, getWorldMatrix, segmentHitsAny, getWorldMatrix, getWorldMatrix, segmentHitsAny

bool StageCharacter::canDropWeapon(StageObject *weapon)
{
    sceVu0FVECTOR from, to;
    memcpy(from, m_xform.position, sizeof(from));
    memcpy(to, weapon->getWorldMatrix()[3], sizeof(to));
    if (gStageScene->segmentHitsAny(from, to, 0, kFaceDropWalls, kFaceSkipBodies, nullptr))
        return false;
    from[2] += kFootHeight;
    to[2] = from[2];
    if (gStageScene->segmentHitsAny(from, to, 0, kFaceDropWalls, kFaceSkipBodies, nullptr))
        return false;
    memcpy(from, weapon->getWorldMatrix()[3], sizeof(from));
    memcpy(to, from, sizeof(to));
    from[2] += kFootHeight;
    if (gStageScene->segmentHitsAny(from, to, 0, kFaceDropSolid, kFaceSkipBodies, nullptr))
        return false;
    memcpy(from, weapon->getWorldMatrix()[3], sizeof(from));
    memcpy(to, from, sizeof(to));
    from[2] -= kDropDepth;
    return !gStageScene->segmentHitsAny(from, to, 0, kFaceDropFloor, 0, nullptr);
}

// FS2 0x0020b240
// callcheck: getStageSelect, getStageSelect, getStageSelect, getStageSelect (stageObjRecord, inline)

bool StageCharacter::canWield(StageObject *item, u_int mask)
{
    const StageObjRecord *sheet = stageObjRecord(m_itemDef);
    const ItemDef &def = defOf(item);
    switch (def.kind) {
    case ItemKind_MeleeLight:
        return sheet->lightWeapon != 0 && (mask & WieldClass_Light) != 0;
    case ItemKind_MeleeHeavy:
        return sheet->heavyWeapon != 0 && (mask & WieldClass_Heavy) != 0;
    default:
        if (def.isPistol())
            return sheet->shortGun != 0 && (mask & WieldClass_Short) != 0;
        if (def.isRifle())
            return sheet->rifle != 0 && (mask & WieldClass_Rifle) != 0;
        return false;
    }
}

// FS2 0x00216aa0
// callcheck: FUN_0017e1b0, FUN_0017e260, erase (the trees, setParent and detach inline), setMoving, setPositionXyz, setRotation, find, swapWeapons, dropItem, setPosition, setRotation, attachTo, setShadowOwner, strcmp, strcmp, setActive, setShown, FUN_00217700, FUN_00217770, getStageSelect

bool StageCharacter::giveItem(StageObject *item)
{
    if (item->m_parent != nullptr)
        return false;
    const int kind = item->getKind();
    if (kind == StageObject_WeaponItem && static_cast<StageWeaponItem *>(item)->m_moving != 0)
        static_cast<StageWeaponItem *>(item)->setMoving(false, true);
    item->setParent(this);
    item->m_xform.setPositionXyz(0.0f, 0.0f, 0.0f);
    item->m_xform.setRotation(0.0f, 0.0f, 0.0f);
    showCarried(item, false);
    item->m_model->m_lit = 0;

    if (kind == StageObject_ItemE) {
        m_items.pushBack(item);
        m_lampItem = static_cast<StageItemE *>(item);
        showCarried(item, true);
        item->m_tree->setParent(m_tree);
        if (strcmp(item->m_name, kChestLamp) == 0) {
            item->setParent(bodyBone(this, kChest));
            const ModelPose *pose = m_model->pose;
            item->m_xform.setPosition(pose->chestToolPosition);
            item->m_xform.setRotation(0.0f, 0.0f, pose->chestToolAxis[3]);
            m_lampItem->lamp()->attachTo(bodyBone(this, kLeftTool));
        }
        else {
            item->setParent(bodyBone(this, kLeftTool));
        }
        m_lampItem->setShadowOwner(this);
    }
    else if (kind == StageObject_WeaponItem) {
        m_items.pushBack(item);
        if (m_weapons[1] == nullptr)
            swapWeapons();
        else if (m_weapons[0] != nullptr)
            dropItem(m_weapons[0]);
        m_weapons[0] = item;
        showCarried(item, true);
        item->m_tree->setParent(m_tree);
        const bool left = stageObjRecord(m_itemDef)->kind == kSheetKindTwoHands && defOf(item).isFirearm();
        item->setParent(bodyBone(this, left ? kLeftTool : kRightTool));
    }
    else if (kind == StageObject_ThrownItem) {
        m_items.pushBack(item);
        item->setParent(bodyBone(this, kRightTool));
    }
    else if (defOf(item).kind < ItemKind_ItemFirst || defOf(item).kind > ItemKind_StackLast) {
        m_items.pushBack(item);
    }
    else {
        // What stacks goes into the count of the same name, and the new one
        // is gone. The ROM walks the list from its tail.
        StageObject *same = nullptr;
        for (StageList<StageObject *>::Node *node = m_items.head; node != nullptr && same == nullptr;
             node = node->next) {
            if (strcmp(node->value->m_name, item->m_name) == 0)
                same = node->value;
        }
        if (same == nullptr) {
            m_items.pushBack(item);
        }
        else {
            StageItem *stack = static_cast<StageItem *>(same);
            int total = stack->m_count + item->m_count;
            if (total < 0)      // sdata 0x31aa10, 0: bytes the ELF does not hold
                total = 0;
            stack->m_count = total < stack->m_maxCount ? total : stack->m_maxCount;
            item->m_tree->setActive(false);
            item->m_tree->setShown(false);
            item->detach();
            item->m_tree->detach();
        }
    }
    return true;
}

// FS2 0x002a3160
// callcheck: getPickSound, playNamedHeard2

void StageCharacter::playPickSound(StageItem *item)
{
    // PC: the host lookup takes no null name; the ROM's finds nothing.
    if (const char *sound = item->getPickSound())
        playNamedHeard2(sound, m_xform.position, nullptr);
}

// FS2 0x002184b0
// callcheck: currentMission, getDisplayName, showNotice

void StageCharacter::noticeItem(StageItem *item)
{
    gStageScene->currentMission()->showNotice(0, reinterpret_cast<const u_short *>(item->getDisplayName()));
}

// FS2 0x0026a4a0
// callcheck: canPickFar, getMatrix, applyMatrix, find, getMatrix, segmentHitsAny, vecDistance

StageItem *MotionProbe::findItem()
{
    StageItem *best = nullptr;
    float bestDistance = 0.0f;
    StageObjArray &items = gStageScene->objects->m_lists[StageObjGroup_3];
    for (StageObject **slot = items.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        if ((*slot)->m_active == 0)
            continue;
        StageItem *item = static_cast<StageItem *>(*slot);
        if (item->m_parent != nullptr || item->m_holder != nullptr || item->m_xform.dirty == 0
            || item->m_inUse != 0 || item->m_sprung != 0 || !item->canPickFar(owner))
            continue;
        sceVu0FVECTOR point;
        applyMatrix(point, item->getMatrix(), item->m_pickPoint);
        const sceVu0FVECTOR *head = bodyBone(owner, "bn_head")->getMatrix();
        if (gStageScene->segmentHitsAny(head[3], point, 0, kFaceItemSight, kFaceSkipBodies, nullptr))
            continue;
        const float distance = vecDistance(owner->m_xform.position, point);
        if (best == nullptr || distance < bestDistance) {
            bestDistance = distance;
            best = item;
        }
    }
    return best;
}

// FS2 0x00271070

void PutItemCondition::onUpdate()
{
    if (motion->walkLink->wallFace == nullptr)
        character->m_allows.putItem = 1;
}

// FS2 0x00271020
// callcheck: request

void PutItemCondition::onApply()
{
    if (character->m_allows.putItem == 0 || character->m_wants.putItem == 0)
        return;
    motion->pickupLink->picks = 0;
    motion->request(kMotionPickLow);
}

// FS2 0x00270e90
// callcheck: isPlayer, isPartner, isTakenOver, findItem, getKind, canWield, getKind, getKind, canDropWeapon, canPick

void PickupItemCondition::onUpdate()
{
    if (character->m_abilities.flag3b9 == 0)
        return;
    if (character->m_abilities.flag3c3 == 0 && character->m_290 != nullptr)
        return;
    if (!character->isPlayer() && !character->isPartner() && character->m_weapons[0] != nullptr)
        return;
    if (character->isTakenOver())
        return;
    StageItem *item = motion->probe->findItem();
    motion->m_lookObject = item;
    if (item == nullptr)
        return;
    if (item->getKind() == StageObject_WeaponItem) {
        if (!character->canWield(item, WieldClass_Any))
            return;
        if (static_cast<StageWeaponItem *>(item)->m_moving != 0)
            return;
        if (character->m_weapons[0] != nullptr && character->m_weapons[1] != nullptr
            && !character->canDropWeapon(character->m_weapons[0]))
            return;
    }
    if (!item->canPick(character))
        return;
    character->m_allows.pickup = 1;
    character->m_pickItem = item;
}

// FS2 0x00270c50
// callcheck: getKind, dropWeapon, getMatrix, applyMatrix, request, request, atan2Octant, addAngle, requestedFile, startWalkFile, requestedFile, startWalkFile

void PickupItemCondition::onApply()
{
    if (character->m_allows.pickup == 0 || character->m_wants.pickup == 0)
        return;
    StageItem *item = character->m_pickItem;
    if (item->m_holder != nullptr)
        return;
    // Both hands full: the held weapon falls for the new one.
    if (item->getKind() == StageObject_WeaponItem && character->m_weapons[0] != nullptr
        && character->m_weapons[1] != nullptr)
        character->dropWeapon(character->m_weapons[0], nullptr);
    motion->pickupLink->picks = 1;

    sceVu0FVECTOR point;
    applyMatrix(point, item->getMatrix(), item->m_pickPoint);
    const float *position = character->m_xform.position;
    motion->request(point[2] - position[2] > kPickHighAbove ? kMotionPickHigh : kMotionPickLow);

    const float dx = position[0] - point[0];
    const float dy = position[1] - point[1];
    if (sqrtf(dx * dx + dy * dy) > kPickWalkFar) {
        constexpr float kPi = 3.1415927f;
        sceVu0FVECTOR angles = { 0.0f, 0.0f, 0.0f, 0.0f };
        angles[2] = addAngle(atan2Octant(dx, -dy), -kPi);
        motion->startWalkFile(kPickWalkRate, motion->requestedFile(), point, angles, true, false);
    }
    else {
        motion->startWalkFile(kPickWalkRate, motion->requestedFile(), point, character->m_xform.rotation, true, false);
    }
}

// FS2 0x0027c100

void PickupLink::onRequest()
{
    if (picks == 0) {
        putting = character->m_putItem;
        taking = nullptr;
    }
    else {
        putting = nullptr;
        taking = character->m_pickItem;
        taking->m_holder = character;
    }
}

// The item leaves the hand: a flying one is thrown, the rest stays where it
// is and counts as used. Twice inline in 0x27bef0.

static void letGoOf(StageCharacter *character, StageItem *item)
{
    if (item->getKind() == StageObject_ThrownItem) {
        character->throwItem(static_cast<StageThrownItem *>(item), nullptr);
    }
    else {
        character->dropItem(item);
        item->m_inUse = 1;
    }
}

// FS2 0x0027bef0
// callcheck: getWorldMatrix, getWorldMatrix, segmentHitsAny, getKind, throwItem, dropItem, noteEnding, getKind, throwItem, dropItem, noteEnding, giveItem, playPickSound, isControlled, noticeItem, noteEnding

void PickupLink::onRun()
{
    if (putting != nullptr) {
        // Early, when the hand is down at something solid.
        sceVu0FVECTOR from, to;
        memcpy(from, putting->getWorldMatrix()[3], sizeof(from));
        memcpy(to, from, sizeof(to));
        from[2] += kFootHeight;
        to[2] -= kFootDepth;
        if (gStageScene->segmentHitsAny(from, to, 0, kFaceDropSolid, kFaceSkipBodies, nullptr)) {
            letGoOf(character, putting);
            motion->noteEnding();
            putting = nullptr;
        }
    }
    if ((character->m_model->m_motionEvents & MotionEvent_Click) == 0)
        return;
    if (taking != nullptr) {
        character->giveItem(taking);
        character->playPickSound(taking);
        if (character->isControlled())
            character->noticeItem(taking);
        motion->noteEnding();
    }
    else if (putting != nullptr) {
        letGoOf(character, putting);
        motion->noteEnding();
    }
    onCancel();
}

// FS2 0x0027c0e0

void PickupLink::onCancel()
{
    if (taking != nullptr) {
        taking->m_holder = nullptr;
        taking = nullptr;
    }
    putting = nullptr;
}


// ── The fight: the stance, the blows, the target ───────────────────────────

namespace {

constexpr float kLockCone60 = 1.0471976f;   // 0x3f860a92
constexpr float kLockCone30 = 0.5235988f;   // 0x3f060a92
constexpr float kLockCone15 = 0.2617994f;   // 0x3e860a92
constexpr float kLockReachDefault = 500.0f;
constexpr float kLockReachFar = 15000.0f;
constexpr float kLockTurnLimit = 0.10471976f; // 6 degrees a frame
constexpr float kLockLookRate = 0.15f;      // 0x3e19999a
constexpr float kBlowRecovery = 0.65f;      // 0x3f266666
constexpr float kBlowPartReach = 140.0f;
constexpr float kChestShotDistance = 300.0f;
constexpr float kPartPointForward = 10.0f;  // a part's aim point, towards the head's far side
constexpr float kNoHeadPartHeight = 35.0f;  // oMT18_30 aims above its origin
constexpr u_int kFaceLockSight = 0x1e42600f;
constexpr u_int kFaceLockSkip = 0x800;
constexpr u_int kFaceChestSkip = 0x48000;
constexpr int   kKindWithThreePoints = 0xb; // a sheet kind aimed at by its nearest of three bones

// The slots of pickArmedMotion.
enum ArmedSlot
{
    ArmedSlot_On = 0,        // bt_on
    ArmedSlot_Stand = 1,     // bt_s
    ArmedSlot_Walk = 2,      // bt_w
    ArmedSlot_Back = 3,      // bt_w_b
    ArmedSlot_Right = 4,     // bt_w_r
    ArmedSlot_Left = 5,      // bt_w_l
    ArmedSlot_TurnRight = 6, // t_r with bt_s
    ArmedSlot_TurnLeft = 7,  // t_l with bt_s
    ArmedSlot_Off = 8,       // bt_off
};

// The blows, gMotionDefs indices with the sheet's labels. 素手コンボ and 武器コンボ
// on the sheet choose the second row, which chains by BtAtkCombo.
enum BlowMotion
{
    Blow_Unarmed1 = 0x59,    // bt_at0_1
    Blow_Unarmed2 = 0x5a,    // bt_at0_2
    Blow_Light1 = 0x5b,      // bt_at1_1
    Blow_Light2 = 0x5c,      // bt_at1_2
    Blow_LightHeavy = 0x5d,  // bt_at1_3
    Blow_Heavy1 = 0x5e,      // bt_at2_1
    Blow_Heavy2 = 0x5f,      // bt_at2_2
    Blow_HeavyHeavy = 0x60,  // bt_at2_3
    Blow_UnarmedChain1 = 0x61, // bt_at0_1
    Blow_UnarmedChain2 = 0x62, // bt_at0_2
    Blow_UnarmedChain3 = 0x63, // bt_at0_3
    Blow_LightChain1 = 0x64, // bt_at1_1
    Blow_LightChain2 = 0x65, // bt_at1_2
    Blow_HeavyChain1 = 0x66, // bt_at2_1
    Blow_HeavyChain2 = 0x67, // bt_at2_2
    Blow_StanceStand = 0x4f, // bt_s: where a blow ends
    Blow_StanceStandArmed = 0x50, // bt_s_1
};

// wants.dpadSide.
enum DpadSide
{
    DpadSide_None = 0,
    DpadSide_Left = 1,
    DpadSide_Right = 2,
};

bool holdsFirearm(const StageCharacter *character)
{
    return character->m_weapons[0] != nullptr && defOf(character->m_weapons[0]).isFirearm();
}

const WeaponDef &weaponSheet(const StageObject *weapon)
{
    const int index = weapon->m_weaponDef;
    return gWeaponDefs[index < 0 || index >= kWeaponDefCount ? 0 : index];
}

// How far the first look reaches: a melee weapon's own, 500 otherwise.
float lockReach(const StageCharacter *character)
{
    const StageObject *weapon = character->m_weapons[0];
    if (weapon == nullptr || defOf(weapon).isFirearm())
        return kLockReachDefault;
    return (float)weaponSheet(weapon).param5;
}

// How far a target stays or changes: the weapon's own, a firearm's three
// times that on Easy.
// callcheck: getStageSelect
float lockKeepReach(const StageCharacter *character)
{
    const StageObject *weapon = character->m_weapons[0];
    if (weapon == nullptr)
        return kLockReachDefault;
    const float reach = (float)weaponSheet(weapon).param5;
    if (getStageSelect()->kind == StageKind_Easy && defOf(weapon).isFirearm())
        return reach * 3.0f;
    return reach;
}

bool isOnStageObjectKind(int kind)
{
    return kind > 0 && kind < 7;
}

} // namespace

// FS2 0x00274810
// callcheck: isFirearm

void BtOnCondition::onUpdate()
{
    if (holdsFirearm(character))
        return;
    if (character->m_abilities.flag3c3 != 0)
        character->m_allows.armed = 1;
}

// FS2 0x00274650
// callcheck: pickArmedMotion, request

void BtOnCondition::onApply()
{
    if (character->m_allows.armed == 0 || character->m_wants.ready == 0 || holdsFirearm(character))
        return;
    motion->request(motion->pickArmedMotion(ArmedSlot_On));
}

// FS2 0x0026f8e0
// callcheck: isControlled

void LockCondition::onUpdate()
{
    if (character->m_abilities.flag3c3 == 0)
        character->m_allows.armed = 1;
    if (!character->isControlled() || motion->noFlash != 0)
        return;
    if (character->m_290 == nullptr)
        character->m_allows.lockFind = 1;
    else
        character->m_allows.lockSwitch = 1;
}

// FS2 0x0026ef30
// callcheck: addAngle, isControlled, getLockPoint, FUN_0026f8d0, FUN_0026f780, FUN_0026f730, findTargetAhead, FUN_0026f720, getStageSelect, vecDistance, findTargetBeside, isTargetValid, canSeeTarget, faceTarget (0x26f8d0 m_weapons[0], 0x26f720 StageSelect::kind, 0x26f730 weaponSheet, 0x26f780 isFirearm)

void LockCondition::onApply()
{
    if (character->m_abilities.flag3c3 == 0
        && (character->m_allows.armed == 0 || character->m_wants.ready == 0)) {
        character->m_290 = nullptr;
        return;
    }

    sceVu0FVECTOR angles = { 0.0f, 0.0f, 0.0f, 0.0f };
    angles[2] = addAngle(character->m_xform.rotation[2], kMathPi);
    angles[0] = motion->m_lampAngles[0];

    if (!character->isControlled() && character->m_294 != nullptr) {
        character->m_290 = character->m_294;
        character->getLockPoint(character->m_aimPoint, false);
        faceTarget();
        return;
    }

    if (character->m_allows.lockFind != 0) {
        MotionProbe *probe = motion->probe;
        character->m_290 = probe->findTargetAhead(kLockCone60, lockReach(character), true, true, angles);
        if (character->m_290 == nullptr) {
            StageObject *far = probe->findTargetAhead(kLockCone15, kLockReachFar, true, false, angles);
            if (far == nullptr) {
                character->m_290 = probe->findTargetAhead(kLockCone30, kLockReachDefault, false, true, angles);
            }
            else if (lockReach(character) < lockKeepReach(character)) {
                const float keep = lockKeepReach(character);
                if (vecDistance(far->getMatrix()[3], character->m_xform.position) < keep)
                    character->m_290 = far;
            }
        }
    }

    const int side = character->m_wants.dpadSide;
    // PC: the ROM reads the target's slot without a test; lockSwitch was
    // set with one, but a blow's onLeave may have cleared it since.
    if (character->m_allows.lockSwitch != 0 && side != DpadSide_None && character->m_290 != nullptr) {
        MotionProbe *probe = motion->probe;
        const float reach = lockKeepReach(character);
        if (side == DpadSide_Right)
            character->m_290 = probe->findTargetBeside(angles[2], -kLockCone60, reach, true, false, 0);
        else if (side == DpadSide_Left)
            character->m_290 = probe->findTargetBeside(angles[2], kLockCone60, reach, true, false, 1);
        if (character->m_290->getKind() != StageObject_Character) {
            if (side == DpadSide_Right)
                character->m_290 = probe->findTargetBeside(angles[2], -kLockCone30, reach, false, false, 0);
            else if (side == DpadSide_Left)
                character->m_290 = probe->findTargetBeside(angles[2], kLockCone30, reach, false, false, 1);
        }
    }

    if (character->m_290 != nullptr) {
        if (!motion->probe->isTargetValid(character->m_290))
            character->m_290 = nullptr;
        if (character->m_290 != nullptr) {
            character->getLockPoint(character->m_aimPoint, false);
            if (!motion->probe->canSeeTarget(character->m_290, character->m_aimPoint))
                character->m_290 = nullptr;
        }
    }
    faceTarget();
}

// A sixth of a right angle a frame towards the target, the body leaning to
// it; head, lamp and upper body look there as the motion's sheet row says.
// FS2 0x0026ebe0
// callcheck: atan2Octant, addAngle, addAngle, addAngle, find, getMatrix, atan2Octant, atan2Octant, addAngle, subAngles, leanBody, isLit, isControlled, setToolTarget, setHeadTarget, setLookTarget

void LockCondition::faceTarget()
{
    if (character->m_290 == nullptr)
        return;
    sceVu0FVECTOR point;
    memcpy(point, character->m_aimPoint, sizeof(point));
    const float *position = character->m_xform.position;

    const float bearing = atan2Octant(position[0] - point[0], point[1] - position[1]);
    const float facing = addAngle(character->m_xform.rotation[2], kMathPi);
    float turn = addAngle(bearing, -facing) / 4.0f;
    if (turn > kLockTurnLimit)
        turn = kLockTurnLimit;
    else if (turn < -kLockTurnLimit)
        turn = -kLockTurnLimit;
    motion->m_turnStep[2] = addAngle(motion->m_turnStep[2], turn);

    const sceVu0FVECTOR *head = bodyBone(character, "bn_head")->getMatrix();
    const float dx = point[0] - head[3][0];
    const float dy = point[1] - head[3][1];
    const float dz = point[2] - head[3][2];
    sceVu0FVECTOR lean = { 0.0f, 0.0f, 0.0f, 0.0f };
    lean[0] = atan2Octant(dz, sqrtf(dx * dx + dy * dy));
    lean[2] = addAngle(-atan2Octant(dx, dy), kMathPi);
    subAngles(lean, lean, character->m_xform.rotation);
    motion->leanBody(lean, false, 0);

    bool toolFollows = false;
    bool headFollows = false;
    if (const MotionDef *def = motionDef(motion->motionId)) {
        headFollows = def->headSwayBoneLink != 0;
        StageItemE *lamp = character->m_lampItem;
        if (lamp != nullptr && lamp->m_active != 0 && lamp->m_lamp->m_spot->isLit()
            && (lamp->m_isTorch2 == 0 || character->isControlled()))
            toolFollows = lamp->m_isTorch2 != 0 || def->flashlightControl != 0;
    }
    motion->setToolTarget(toolFollows ? point : nullptr);
    if (motion->m_kindFlags2 != 0) {
        motion->setHeadTarget(kLockLookRate, headFollows ? point : nullptr);
        motion->setLookTarget(kLockLookRate, point);
    }
    else if (motion->m_kindFlags1 != 0) {
        motion->setHeadTarget(kLockLookRate, headFollows ? point : nullptr);
        motion->setLookTarget(kLockLookRate, nullptr);
    }
}

// FS2 0x0026e9e0
// callcheck: pickArmedMotion

void BtAttackCondition::onUpdate()
{
    if (holdsFirearm(character))
        return;
    if ((character->m_weapons[0] == nullptr && character->m_abilities.flag3c4 == 0) || motion->m_ba8 > 0.0f)
        return;
    character->m_allows.armed = 1;
    if (motion->motionId != motion->pickArmedMotion(ArmedSlot_On))
        character->m_allows.fire = 1;
}

// FS2 0x0026e500
// callcheck: getStageSelect, randNext, request, getStageSelect, randNext, request, getStageSelect, randNext, request

void BtAttackCondition::onApply()
{
    const CharacterWants &wants = character->m_wants;
    if (character->m_allows.armed == 0 || wants.ready == 0 || character->m_allows.fire == 0)
        return;
    const StageObjRecord *sheet = stageObjRecord(character->m_itemDef);
    const StageObject *weapon = character->m_weapons[0];
    const bool hit = wants.fire == CharacterFire_Tap || wants.fire == CharacterFire_Hold;

    if (weapon == nullptr) {
        if (!hit)
            return;
        if (sheet->unarmedCombo != 0)
            motion->request(Blow_UnarmedChain1);
        else if (character->m_abilities.flag3c5 != 0 && (int)randNext() % 2 == 0)
            motion->request(Blow_Unarmed2);
        else
            motion->request(Blow_Unarmed1);
        return;
    }

    const int kind = defOf(weapon).kind;
    if (kind != ItemKind_MeleeLight && kind != ItemKind_MeleeHeavy)
        return;
    const bool light = kind == ItemKind_MeleeLight;
    if (wants.fire == CharacterFire_Tap
        || (wants.fire == CharacterFire_Hold && character->m_abilities.flag3c6 == 0)) {
        if (sheet->weaponCombo != 0)
            motion->request(light ? Blow_LightChain1 : Blow_HeavyChain1);
        else if ((int)randNext() % 2 == 0)
            motion->request(light ? Blow_Light2 : Blow_Heavy2);
        else
            motion->request(light ? Blow_Light1 : Blow_Heavy1);
    }
    else if (wants.fire == CharacterFire_Hold) {
        motion->request(light ? Blow_LightHeavy : Blow_HeavyHeavy);
    }
}

// FS2 0x0026e4e0

void BtAtkComboCondition::onUpdate()
{
    character->m_allows.armed = 1;
    character->m_allows.fire = 1;
}

// FS2 0x0026e280
// callcheck: getStageSelect, request, request, getStageSelect, request, request, request, request

void BtAtkComboCondition::onApply()
{
    const CharacterWants &wants = character->m_wants;
    if (character->m_allows.armed == 0 || wants.ready == 0 || character->m_allows.fire == 0)
        return;
    if (wants.fire != CharacterFire_Tap && wants.fire != CharacterFire_Hold)
        return;
    const StageObjRecord *sheet = stageObjRecord(character->m_itemDef);
    if (sheet->unarmedCombo != 0) {
        if (motion->motionId == Blow_UnarmedChain2)
            motion->request(Blow_UnarmedChain3);
        else if (motion->motionId == Blow_UnarmedChain1)
            motion->request(Blow_UnarmedChain2);
    }
    if (sheet->weaponCombo != 0) {
        switch (motion->motionId) {
        case Blow_HeavyChain2: motion->request(Blow_HeavyHeavy); break;
        case Blow_HeavyChain1: motion->request(Blow_HeavyChain2); break;
        case Blow_LightChain2: motion->request(Blow_LightHeavy); break;
        case Blow_LightChain1: motion->request(Blow_LightChain2); break;
        }
    }
}

// FS2 0x0026e260

void BtMoveCondition::onUpdate()
{
    character->m_allows.armed = 1;
    character->m_allows.move = 1;
}

// With a target the walks close on it, without one the stick turns.
// FS2 0x0026df60
// callcheck: pickArmedMotion, request, pickArmedMotion, pickArmedMotion, request, pickArmedMotion, request

void BtMoveCondition::onApply()
{
    const CharacterWants &wants = character->m_wants;
    if (character->m_allows.armed == 0 || wants.ready == 0) {
        motion->request(motion->pickArmedMotion(ArmedSlot_Off));
        return;
    }
    if (motion->motionId == motion->pickArmedMotion(ArmedSlot_On))
        return;

    int slot = -1;
    if (character->m_allows.move != 0) {
        const CharacterAbilities &abilities = character->m_abilities;
        const bool locked = character->m_290 != nullptr;
        switch (wants.moveKind) {
        case CharacterMove_StrafeLeft:
            if (abilities.strafe != 0)
                slot = ArmedSlot_Left;
            break;
        case CharacterMove_TurnLeft:
            if (!locked)
                slot = ArmedSlot_TurnLeft;
            else if (abilities.strafe != 0)
                slot = ArmedSlot_Left;
            break;
        case CharacterMove_StrafeRight:
            if (abilities.strafe != 0)
                slot = ArmedSlot_Right;
            break;
        case CharacterMove_TurnRight:
            if (!locked)
                slot = ArmedSlot_TurnRight;
            else if (abilities.strafe != 0)
                slot = ArmedSlot_Right;
            break;
        case CharacterMove_BackTurn:
            if (abilities.walkBack != 0)
                slot = ArmedSlot_Back;
            break;
        case CharacterMove_Back:
            if (locked && abilities.walkBack != 0)
                slot = ArmedSlot_Back;
            break;
        case CharacterMove_WalkSlow:
            slot = ArmedSlot_Walk;
            break;
        case CharacterMove_Walk:
        case CharacterMove_Run:
            if (locked)
                slot = ArmedSlot_Walk;
            break;
        }
    }
    if (slot >= 0)
        motion->request(motion->pickArmedMotion(slot));
    if (motion->m_request == Motion_None)
        motion->request(motion->pickArmedMotion(ArmedSlot_Stand));
}

// FS2 0x0027b000
// callcheck: FUN_0027d8d0, operator_new, FUN_001ef590, MotionConditionList::add, FUN_001ef590, MotionConditionList::add, FUN_001ef590, MotionConditionList::add (addCondition)

BattleLink::BattleLink(StageCharacter *owner, PlayerMotion *motion_)
    : MotionLink(owner, motion_)
{
    addCondition("Lock");
    addCondition("BtAttack");
    addCondition("BtMove");
}

// Out of the stance the target goes, but into what keeps it.
// FS2 0x0027af70
// callcheck: FUN_001ef260, FUN_001ef260 (m_links.find)

void BattleLink::onLeave(MotionLink *next)
{
    const char *keeps = character->m_abilities.flag3c3 == 0 ? "Walk" : "BtAttack";
    if (next != motion->m_links.find(keeps)->second)
        character->m_290 = nullptr;
}

// FS2 0x0027aea0
// callcheck: FUN_0027d8d0, operator_new, FUN_001ef590, MotionConditionList::add, FUN_001ef590, MotionConditionList::add (addCondition)

BtAttackLink::BtAttackLink(StageCharacter *owner, PlayerMotion *motion_)
    : MotionLink(owner, motion_)
{
    addCondition("Lock");
    addCondition("BtAtkCombo");
}

// FS2 0x0027adf0
// callcheck: getKind, FUN_00294c70, FUN_00294c30, FUN_0027d940, FUN_0027d940 (m_conditions[]; PC: the two read the pose of a floor object of kind 4, a class the host does not make, so Lock always stops)

void BtAttackLink::onStart()
{
    m_conditions[0]->active = 0;
    m_conditions[1]->active = 0;
}

// FS2 0x0027ac60
// callcheck: FUN_0027d940, findBlowPart, StageObject::breakPart, isEnding, request, request, request (m_conditions[])

void BtAttackLink::onRun()
{
    if (counter == 0 && (character->m_model->m_motionEvents & MotionEvent_Cancel) != 0) {
        counter = 1;
        m_conditions[1]->active = 1;
    }
    const int part = motion->probe->findBlowPart();
    const bool broke = part >= 0;
    if (broke)
        character->m_290->breakPart(character->m_290->m_mapParts->ids[part], true);
    const bool bumped = character->m_ground->bumped != 0;
    if (!motion->isEnding() && !bumped && !broke)
        return;
    if (character->m_abilities.flag3c3 == 0)
        motion->request(Motion_Stand);
    else
        motion->request(character->m_weapons[0] == nullptr ? Blow_StanceStand : Blow_StanceStandArmed);
    if (bumped || broke)
        motion->m_ba8 = kBlowRecovery;
}

// FS2 0x0027aba0
// callcheck: FUN_0027d940, FUN_0027d940, FUN_0027d940, FUN_0027d940, FUN_001ef260 (m_conditions[], m_links.find)

void BtAttackLink::onLeave(MotionLink *next)
{
    m_conditions[0]->active = 1;
    m_conditions[1]->active = 1;
    if (next != motion->m_links.find("Battle")->second)
        character->m_290 = nullptr;
}

// FS2 0x00211910
// callcheck: isTakenOver (inline)

StagePlayer *StageCharacter::getTakenOverPlayer() const
{
    return isTakenOver() ? gStageScene->player : nullptr;
}

// The ROM inlines itself here three levels deep, with the scene's getters
// 0x216120, 0x216110, 0x2160f0, 0x2160d0 and 0x2160c0 and the side's 0x2160b0, 0x2160a0.
// FS2 0x00215f10
// callcheck: FUN_00216120, FUN_00216110, FUN_002160f0, isControlled, FUN_002160d0, isTakenOver, FUN_002160c0, getTakenOverPlayer, FUN_002160b0, FUN_002160a0, getSides

u_int StageCharacter::getSides() const
{
    const StagePlayer *player = getTakenOverPlayer();
    return player != nullptr ? player->getSides() : m_sides;
}

// FS2 0x00214d40
// callcheck: getStageSelect, getSides

bool StageCharacter::isEnemyOf(StageObject *target)
{
    if (target == m_foe) {
        const StageObject *driver = gStageScene->controlled != nullptr
                                        ? gStageScene->controlled : gStageScene->player;
        if (driver == target)
            return true;
        m_foe = nullptr;
    }
    if (target == this)
        return false;
    // The side a sheet kind belongs to: StageObjSide, none for 9 and 0xe.
    static const u_int kSideOfKind[] = {
        StageObjSide_Player, StageObjSide_Npc, StageObjSide_Corpse, StageObjSide_Corpse,
        StageObjSide_Dark, StageObjSide_Dark, StageObjSide_Dark, StageObjSide_Dark,
        StageObjSide_Corpse, 0, StageObjSide_Corpse, StageObjSide_Dark,
        StageObjSide_Dark, StageObjSide_Corpse, 0, StageObjSide_Npc,
    };
    const u_int kind = (u_int)stageObjRecord(target->m_itemDef)->kind;
    const u_int side = kind < sizeof(kSideOfKind) / sizeof(kSideOfKind[0]) ? kSideOfKind[kind] : 0;
    return (side & getSides()) != 0;
}

// FS2 0x002162a0

bool StageCharacter::isBlowActive() const
{
    const int bone = m_ground->hitBone;
    return bone >= 0 && m_ground->blowBodies[bone]->on != 0;
}

// FS2 0x0020c1d0
// callcheck: getTargetPoint

bool StageCharacter::getLockPoint(float *out, bool plain)
{
    return getTargetPoint(m_290, out, plain);
}

// FS2 0x0020c1f0
// callcheck: getKind, getStageSelect, find, getMatrix, find, getWorldMatrix, find, getWorldMatrix, find, getMatrix, vecDistance, find, getMatrix, vecDistance, vecDistance, find, getWorldMatrix, getWorldMatrix, find, getMatrix, segmentHitsAny, find, getMatrix, getKind, getBodyPosition, nearestPart, partName, find, getWorldMatrix, getWorldMatrix, find, getMatrix, vecDirection, scaleVec4, strcmp, getMatrix, getMatrix

bool StageCharacter::getTargetPoint(StageObject *target, float *out, bool plain)
{
    const int kind = target->getKind();
    if (kind == StageObject_Character) {
        const StageCharacter *other = static_cast<StageCharacter *>(target);
        if (stageObjRecord(target->m_itemDef)->kind == kKindWithThreePoints) {
            const float *points[3] = {
                bodyBone(target, "bn_head")->getMatrix()[3],
                bodyBone(target, "bn_pelvis")->getWorldMatrix()[3],
                bodyBone(target, "bn_l_foot")->getWorldMatrix()[3],
            };
            const float *head = bodyBone(this, "bn_head")->getMatrix()[3];
            int best = 0;
            float bestDistance = vecDistance(head, points[0]);
            for (int i = 1; i < 3; i++) {
                const float distance = vecDistance(head, points[i]);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    best = i;
                }
            }
            memcpy(out, points[best], sizeof(sceVu0FVECTOR));
            return true;
        }
        if (!plain && other->m_gear == nullptr && holdsFirearm(this)
            && kChestShotDistance < vecDistance(target->m_xform.position, m_xform.position)) {
            BoneMap::Node *spine = target->m_model->boneMap.find("bn_spine1");
            if (spine != nullptr) {
                const float *chest = spine->value->getWorldMatrix()[3];
                const StageObject *driver = gStageScene->controlled != nullptr
                                                ? gStageScene->controlled : gStageScene->player;
                if (driver == this
                    || !gStageScene->segmentHitsAny(bodyBone(this, "bn_head")->getMatrix()[3], chest, 0,
                                                    kFaceLockSight, kFaceChestSkip, nullptr)) {
                    memcpy(out, chest, sizeof(sceVu0FVECTOR));
                    return true;
                }
            }
        }
        memcpy(out, bodyBone(target, "bn_head")->getMatrix()[3], sizeof(sceVu0FVECTOR));
        return true;
    }

    if (kind >= StageObject_Light && kind < StageObject_Item) {
        memcpy(out, static_cast<StageLight *>(target)->getBodyPosition(), sizeof(sceVu0FVECTOR));
        return true;
    }

    // PC: only a map object has m_mapParts; the ROM's objects of these kinds all do.
    if (isOnStageObjectKind(kind) && kind != StageObject_Kind3 && target->m_mapParts != nullptr
        && target->m_mapParts->switches != nullptr) {
        if (m_weapons[0] == nullptr)
            return false;
        const int part = target->nearestPart(this);
        if (part < 0)
            return false;
        const MapObjectParts *parts = target->m_mapParts;
        if (parts->effects[part] != nullptr) {
            memcpy(out, parts->effects[part]->getWorldMatrix()[3], sizeof(sceVu0FVECTOR));
        }
        else {
            const char *name = parts->switches[part].partName(0);
            BoneMap::Node *bone = name != nullptr ? target->m_model->boneMap.find(name) : nullptr;
            if (bone == nullptr)
                return false;
            memcpy(out, bone->value->getWorldMatrix()[3], sizeof(sceVu0FVECTOR));
        }
        sceVu0FVECTOR direction;
        vecDirection(direction, bodyBone(this, "bn_head")->getMatrix()[3], out);
        scaleVec4(kPartPointForward, direction, direction);
        out[0] += direction[0];
        out[1] += direction[1];
        out[2] += direction[2];
        return true;
    }

    memcpy(out, target->getMatrix()[3], sizeof(sceVu0FVECTOR));
    if (isOnStageObjectKind(kind) && strcmp(target->m_name, "oMT18_30") == 0)
        out[2] += kNoHeadPartHeight;
    return true;
}

// FS2 0x00175700
// callcheck: partName, find, find, getMatrix, getWorldMatrix, vecDistance, find, getMatrix, getWorldMatrix, vecDistance

int StageObject::nearestPart(StageObject *from)
{
    const MapObjectParts *parts = m_mapParts;
    if (parts->switches == nullptr || parts->effects == nullptr)
        return -1;
    int best = -1;
    float bestDistance = FLT_MAX;
    for (int i = 0; i < parts->count; i++) {
        if (parts->switches[i].shown == 0)
            continue;
        const float *point = nullptr;
        if (parts->effects[i] != nullptr) {
            point = parts->effects[i]->getWorldMatrix()[3];
        }
        else {
            const char *name = parts->switches[i].partName(0);
            BoneMap::Node *bone = name != nullptr ? m_model->boneMap.find(name) : nullptr;
            if (bone == nullptr || bone->value == nullptr)
                continue;
            point = bone->value->getWorldMatrix()[3];
        }
        const float distance = vecDistance(bodyBone(from, "bn_head")->getMatrix()[3], point);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

// FS2 0x0026a250
// callcheck: getKind, getKind, getKind, isBlowActive, nearestPart, getWorldMatrix, distanceToPoint

int MotionProbe::findBlowPart()
{
    StageObject *target = owner->m_290;
    if (target == nullptr)
        return -1;
    const int kind = target->getKind();
    // PC: m_mapParts as in getTargetPoint.
    if (!isOnStageObjectKind(kind) || kind == StageObject_Kind3 || target->m_mapParts == nullptr
        || target->m_mapParts->switches == nullptr || !owner->isBlowActive())
        return -1;
    const int part = target->nearestPart(owner);
    if (part < 0)
        return -1;
    // PC: the ROM calls the effect without a test; nearestPart's way round
    // a missing one is a bone, which has no distance here.
    StageParticle *effect = target->m_mapParts->effects[part];
    if (effect == nullptr)
        return -1;
    sceVu0FVECTOR point;
    memcpy(point, effect->getWorldMatrix()[3], sizeof(point));
    point[2] = owner->m_xform.position[2];
    return owner->distanceToPoint(point) < kBlowPartReach ? part : -1;
}

// FS2 0x0026b5d0
// callcheck: getKind, isEnemyOf, getBrainPace

bool MotionProbe::isTargetValid(StageObject *target)
{
    if (target->m_lightOn == 0 || target->m_active == 0)
        return false;
    if (target->getKind() != StageObject_Character)
        return true;
    if (target == owner || !owner->isEnemyOf(target))
        return false;
    StageCharacter *other = static_cast<StageCharacter *>(target);
    return other->getBrainPace() != 0 && other->m_endurance > 0;
}

// FS2 0x0026b690
// callcheck: getKind, getKind, getKind, find, getMatrix, segmentHitsAny, find, getMatrix, atan2Octant

bool MotionProbe::canSeeTarget(StageObject *target, const float *point)
{
    const int kind = target->getKind();
    StageObject *skip = isOnStageObjectKind(kind) && kind != StageObject_Door && target != gStageScene->root
                            ? target : nullptr;
    const float *head = bodyBone(owner, "bn_head")->getMatrix()[3];
    if (gStageScene->segmentHitsAny(head, point, 0, kFaceLockSight, kFaceLockSkip, skip))
        return false;
    const float dx = point[0] - head[0];
    const float dy = point[1] - head[1];
    const float pitch = atan2Octant(point[2] - head[2], sqrtf(dx * dx + dy * dy));
    return fabsf(pitch) <= kLockCone60;
}

// FS2 0x0026a8f0
// callcheck: identityMatrix, rotateMatrixX, rotateMatrixZ, applyMatrix, find, getMatrix, getKind, isEnemyOf, getBrainPace, getTargetPoint, vecDot, asinAcos, vecDistance, getKind, getKind, getKind, find, getMatrix, segmentHitsAny, find, getMatrix, atan2Octant (0x26b5d0 and 0x26b690 inline, as isTargetValid and canSeeTarget)

StageObject *MotionProbe::findTargetAhead(float cone, float range, bool characters, bool nearest,
                                          const float *angles)
{
    sceVu0FMATRIX matrix;
    identityMatrix(matrix);
    rotateMatrixX(angles[0], matrix, matrix);
    rotateMatrixZ(angles[2], matrix, matrix);
    const sceVu0FVECTOR axisLocal = { 0.0f, range, 0.0f, 1.0f };
    sceVu0FVECTOR axis;
    applyMatrix(axis, matrix, axisLocal);
    sceVu0FVECTOR head;
    memcpy(head, bodyBone(owner, "bn_head")->getMatrix()[3], sizeof(head));

    StageObject *best = nullptr;
    float bestDistance = 0.0f;
    float bestAngle = 0.0f;
    for (int group = 0; group < StageObjGroup_Count; group++) {
        if ((group == StageObjGroup_Characters) != characters)
            continue;
        StageObjArray &list = gStageScene->objects->m_lists[group];
        for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
            StageObject *object = *slot;
            if (object->m_active == 0 || object == owner->m_290 || !isTargetValid(object))
                continue;
            sceVu0FVECTOR point;
            if (!owner->getTargetPoint(object, point, false))
                continue;
            const sceVu0FVECTOR toPoint = { point[0] - head[0], point[1] - head[1], point[2] - head[2], 0.0f };
            const float axisLength = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
            const float pointLength = sqrtf(toPoint[0] * toPoint[0] + toPoint[1] * toPoint[1] + toPoint[2] * toPoint[2]);
            const float angle = asinAcos(vecDot(axis, toPoint) / (axisLength * pointLength), AsinAcos_Acos);
            const float distance = vecDistance(point, owner->m_xform.position);
            if (angle >= cone || distance >= range || !canSeeTarget(object, point))
                continue;
            if (best == nullptr) {
                bestDistance = distance;
                bestAngle = fabsf(angle);
                best = object;
            }
            else if (nearest) {
                if (distance < bestDistance) {
                    bestDistance = distance;
                    best = object;
                }
            }
            else if (fabsf(angle) < bestAngle) {
                bestAngle = fabsf(angle);
                best = object;
            }
        }
    }
    return best != nullptr ? best : owner->m_290;
}

// FS2 0x0026aec0
// callcheck: getKind, isEnemyOf, getBrainPace, getTargetPoint, atan2Octant, wrapAngle, vecDistance, getKind, getKind, getKind, find, getMatrix, segmentHitsAny, find, getMatrix, atan2Octant, vecDistance, getKind, getKind, getKind, find, getMatrix, segmentHitsAny, find, getMatrix, atan2Octant (0x26b5d0 and 0x26b690 inline)

StageObject *MotionProbe::findTargetBeside(float facing, float span, float range, bool characters,
                                           bool nearest, int side)
{
    StageObject *best = nullptr;
    float bestDistance = 0.0f;
    float bestAngle = 0.0f;
    for (int group = 0; group < StageObjGroup_Count; group++) {
        if ((group == StageObjGroup_Characters) != characters)
            continue;
        StageObjArray &list = gStageScene->objects->m_lists[group];
        for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
            StageObject *object = *slot;
            if (object->m_active == 0 || object == owner->m_290 || !isTargetValid(object))
                continue;
            sceVu0FVECTOR point;
            if (!owner->getTargetPoint(object, point, false))
                continue;
            const float *position = owner->m_xform.position;
            const float angle = wrapAngle(atan2Octant(position[0] - point[0], point[1] - position[1]) - facing);
            const bool inSide = side == 0 ? span < angle && angle < 0.0f
                                          : side == 1 && 0.0f < angle && angle < span;
            if (!inSide)
                continue;
            const float distance = vecDistance(point, position);
            if (distance >= range || !canSeeTarget(object, point))
                continue;
            if (best == nullptr) {
                bestDistance = distance;
                bestAngle = angle;
                best = object;
            }
            else if (nearest) {
                if (distance < bestDistance) {
                    bestDistance = distance;
                    best = object;
                }
            }
            else if (bestAngle < angle) {
                bestAngle = angle;
                best = object;
            }
        }
    }
    return best != nullptr ? best : owner->m_290;
}

// FS2 0x0021bb70

int StageCharacter::getBrainPace()
{
    return m_brain == nullptr ? 1 : m_brain->state;
}
