/*
 *  game/playermotion.cpp -- what a character is doing with its body (FS2,
 *  0x1e0000..0x1f0000): the motion a character asks for, what it may become,
 *  and what plays. game/playermotion.h has the outline.
 *
 *  The ids of the sheet (gMotionDefs) are the game's own and stay numbers in
 *  the predicates, whose names say what the code does with them.
 */
#include "game/playermotion.h"

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
// Kinds 0 and 1 while +0x3f4 is set.
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

// The walks, armed or not: what 移動速度 sets the pace of (moveStep).
static bool isWalkMotion(int id)
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
        sceVu0FVECTOR lamp = {};   // PC: getLampAngles is a stub
        if (lampAim)
            m->getLampAngles(lamp);
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
                applyMatrix(point, m->m_lookObject->node->getMatrix(), m->m_lookObject->point);
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
    if (character->m_abilities.runStart != 0 && character->m_3f4 == 0) {
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

    const bool plainPace = character->m_abilities.runStart == 0 || character->m_3f4 != 0;
    const bool walking = wants.moveKind == CharacterMove_Walk || wants.moveKind == CharacterMove_WalkSlow
        || stopAtWall || stopAtEdge || trip || character->m_ground->narrow != 0;
    if (walking) {
        if (stairs != 0) {
            if (character->m_3f4 == 0)
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
        if (character->m_3f4 == 0)
            motion->request(stairs == 1 ? Motion_StairsRunUp : Motion_StairsRunDown);
        else
            motion->request(motion->pickStance(MotionStance_Run));
        return;
    }
    if (plainPace) {
        // A crouch that may not run walks (the ROM's two tests give this).
        if (isCrouchMotion(motion->motionId) && character->m_3f4 != 0)
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
    if (character->m_ground->wallCheck == nullptr)
        return;
    wallFace = motion->probe->faceAhead(wallPoint, true);
    edgeFound = motion->probe->findEdge(wallFace, edgeHit, edgeTarget, &edgeDrop, &edgeFloor, edgePoint,
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
    m_conditions["QuickTurn"] = new QuickTurnCondition(character, this);

    walkLink = new WalkLink(character, this);
    m_links["Walk"] = walkLink;

    // PC: the ROM's 0x212530 sets these from a script's property; nothing
    // written sets them yet, so the sheet's.
    const StageObjRecord *sheet = stageObjRecord(owner->m_itemDef);
    if (sheet->walkSpeed > 0)
        character->m_walkRate = sheet->walkSpeed / 100.0f;
    if (sheet->runSpeed > 0)
        character->m_runRate = sheet->runSpeed / 100.0f;
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

    m_ba4 = 0.0f;
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
        character->m_3f4 = 1;
        character->m_3f8 = 0;
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
            startWalk(m_walkKind, m_hasWalkFrom != 0 ? m_walkFrom : nullptr,
                      m_hasWalkTo != 0 ? m_walkTo : nullptr, m_walkArg != 0, m_walkArg2 != 0);
        else
            startWalkAt(m_walkKind, m_walkTarget, m_walkOffset, m_hasWalkEnd != 0 ? m_walkEnd : nullptr,
                        m_walkArg != 0, m_walkArg2 != 0);
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
        if (ground->wallCheck != nullptr)
            *ground->wallCheck = wall->wallCheck;
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
    if (character->m_3f4 != 0) {
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

// FS2 0x001e3e80
// callcheck: applyMatrix, randNext, vecCross, vecDirection, FUN_001f00a0, FUN_002a2100, FUN_002a49c0, setAimDirection, getNormal, segmentHitsAny, raycastObject, setFootTargets, getFootPositions, raycast, getLowerNames, FUN_001908d0, setWindOn, getDeltaSeconds, requestNow, rollIdleDelay, FUN_00215940, FUN_0020d390, getAltSoundFlag, isPlayer, getTopView, setContact, find, getStageSelect (written: the head; the rest of the function is below in the notes of open-items.md: step sounds, feet, wind, the fidgets)

void PlayerMotion::controlStep()
{
    CharacterGround *ground = static_cast<StageCharacter *>(owner)->m_ground;
    const MotionDef *current = motionDef(motionId);
    if (current != nullptr) {
        const MotionDef *wall = current;
        if (m_queue->motion != Motion_None)
            wall = motionDef(m_queue->motion);
        if (m_walkQueue->motion != Motion_None)
            wall = motionDef(m_walkQueue->motion);
        m_wallCheck = wall->wallCheck;
        if (ground->wallCheck != nullptr)
            *ground->wallCheck = wall->wallCheck;
        ground->groundCheck = current->groundCheck;
        ground->repelLevel = wall->repelLevel;
        m_always->onApply();
        if (activeLink != nullptr)
            activeLink->apply();
        if (activeLink != nullptr)
            activeLink->onRun();
    }
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

void PlayerMotion::startWalkFile(float kind, void *file, const float *from, const float *to, bool arg, bool arg2)
{
    m_walkPending = 1;
    m_walkToTarget = 1;
    memcpy(&m_walkKind, &kind, sizeof(kind));
    m_walkArg = arg;
    m_walkArg2 = arg2;
    m_walkTarget = file;
    memcpy(m_walkOffset, from, sizeof(m_walkOffset));
    m_hasWalkEnd = to != nullptr;
    if (to != nullptr)
        memcpy(m_walkEnd, to, sizeof(m_walkEnd));
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
// callcheck: FUN_001f3930, getDeltaSeconds, FUN_001e0e20, FUN_0020d400, advance (the ground's step comes with game/characterground.cpp)

void StageCharacter::moveStep()
{
    m_motion->applyMovement();
    const int id = m_motion->motionId;
    float rate = 1.0f;
    if (isWalkMotion(id))
        rate = m_walkRate;
    else if (isRunMotion(id) || id == 0x2d || id == 0x24)
        rate = m_runRate;
    // A door's motion takes its rate from the Door link (+0xc0c +0x24): not written.
    const float frames = m_clock->getDeltaSeconds() / 0.033333335f;
    m_model->advance(rate * getMotionRate() * frames, m_xform.position, m_xform.rotation, 0);
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
        // PC: the start hook 0x17a860 and the control hook 0x17a810 are not
        // written, and they run before this in the ROM: this is their order,
        // with the input (0x236b90) and the brain missing.
        memset(&body->m_allows, 0, sizeof(body->m_allows));
        motion->beginFrame();
        memset(&body->m_wants, 0, sizeof(body->m_wants));
        motion->controlStep();
        motion->stepLower(false);
        motion->stepUpper(false);
        motion->stepFace(false);
        // PC: the movement hook runs after this, in actionMovement.
        body->moveStep();
    }
}
