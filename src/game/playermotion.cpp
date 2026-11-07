/*
 *  game/playermotion.cpp -- what a character is doing with its body (FS2,
 *  0x1e0000..0x1f0000): the motion a character asks for, what it may become,
 *  and what plays. game/playermotion.h has the outline.
 *
 *  The ids of the sheet (gMotionDefs) are the game's own and stay numbers in
 *  the predicates, whose names say what the code does with them.
 */
#include "game/characterbrain.h"
#include "game/characterroute.h"
#include "game/playermotion.h"
#include "game/objectproperty.h"
#include "game/stagedoor.h"
#include "game/stagecar.h"

#include "game/boxgrid.h"

#include "game/characterground.h"
#include "game/characterinput.h"

#include <algorithm>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "game/clock.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/packtables.h"
#include "game/scenepairs.h"
#include "game/stageitem.h"
#include "game/stagenode.h"
#include "game/stageobjlist.h"
#include "game/stageparticle.h"
#include "game/rumblelist.h"
#include "game/stageresult.h"
#include "game/stagescene.h"
#include "game/stagelight.h"
#include "game/stagesound.h"
#include "game/sysdata.h"
#include "game/uiwidget.h"
#include "game/viewcamera.h"
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

// The name's lookup, which the ROM inlines in each link's ctor.

void MotionLink::addCondition(const char *name)
{
    if (auto *found = motion->m_conditions.find(name))
        m_conditions.add(found->value);
}

// The conditions' base: the host keeps the ROM's two calls.

// FS2 0x00276510

MotionCondition::MotionCondition(StageCharacter *owner, PlayerMotion *motion_)
    : character(owner), motion(motion_)
{
}

// FS2 0x002764b0
// callcheck: operator_delete

MotionCondition::~MotionCondition()
{
}

// FS2 0x001eee10

void MotionLink::onUpdate()
{
}

// FS2 0x001eee20

void MotionLink::onApply()
{
}

// The block at +8 is only ever freed: no reader found.
// FS2 0x0027cc50
// callcheck: MotionConditionList, operator_new

MotionLink::MotionLink(StageCharacter *owner, PlayerMotion *motion_)
    : m_motionBlock(new PlayerMotion *(motion_)), character(owner), motion(motion_)
{
}

// FS2 0x0027cbe0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

MotionLink::~MotionLink()
{
    delete m_motionBlock;
}

// ──────────────────────────────────────────────────────────────────────
// The list.

// FS2 0x0027d8d0

MotionConditionList::MotionConditionList()
    : head(nullptr), count(0)
{
}

// FS2 0x0027d8e0
// callcheck: clear, operator_delete

MotionConditionList::~MotionConditionList()
{
    clear();
}

// FS2 0x0027daf0
// callcheck: operator_delete, operator_delete

void MotionConditionList::clear()
{
    Node *next = head;
    while (next != nullptr) {
        Node *node = next;
        next = node->next;
        delete node->value;
        delete node;
    }
    head = nullptr;
    count = 0;
}

// FS2 0x0027da40
// callcheck: last, operator_new, operator_new, Node, addFirst

void MotionConditionList::add(MotionCondition *const &value)
{
    if (head == nullptr) {
        addFirst(value);
        return;
    }
    Node *tail = last();
    Node *node = new Node(new MotionCondition *(value));
    node->previous = tail;
    tail->next = node;
    count++;
}

// FS2 0x0027db60
// callcheck: operator_new, operator_new, Node

void MotionConditionList::addFirst(MotionCondition *const &value)
{
    Node *old = head;
    head = new Node(new MotionCondition *(value));
    if (old != nullptr) {
        head->next = old;
        old->previous = head;
    }
    count++;
}

// FS2 0x0027dbf0

MotionConditionList::Node *MotionConditionList::last() const
{
    Node *tail = head;
    for (Node *node = head; node != nullptr; node = node->next)
        tail = node;
    return tail;
}

// FS2 0x0027d940

MotionConditionList::Node *MotionConditionList::nodeAt(int index) const
{
    Node *node = head;
    if (index < count && 0 < index) {
        for (int i = 0; i < index; i++)
            node = node->next;
    }
    return node;
}

// ──────────────────────────────────────────────────────────────────────
// The links' dtors. Each stores its vtable, runs the base's body inlined and
// frees the object under a flag.

// FS2 0x0027ced0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

BiteLink::~BiteLink()
{
}

// FS2 0x0027cf50
// callcheck: operator_delete, ~MotionConditionList, operator_delete

ThrowLink::~ThrowLink()
{
}

// FS2 0x0027d050
// callcheck: operator_delete, ~MotionConditionList, operator_delete

TrapCancelLink::~TrapCancelLink()
{
}

// FS2 0x002771b0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

ReviveLink::~ReviveLink()
{
}

// FS2 0x0027ccd0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

DamageDeadLink::~DamageDeadLink()
{
}

// FS2 0x0027cd50
// callcheck: operator_delete, ~MotionConditionList, operator_delete

DamageFatalLink::~DamageFatalLink()
{
}

// FS2 0x0027cdd0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

DamageLandLink::~DamageLandLink()
{
}

// FS2 0x0027ce50
// callcheck: operator_delete, ~MotionConditionList, operator_delete

DamageLink::~DamageLink()
{
}

// FS2 0x0027cfd0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

TrapHitLink::~TrapHitLink()
{
}

// FS2 0x0027d150
// callcheck: operator_delete, ~MotionConditionList, operator_delete

DiveLandLink::~DiveLandLink()
{
}

// FS2 0x0027d1d0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

DiveFallLink::~DiveFallLink()
{
}

// FS2 0x0027d0d0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

TrapSetLink::~TrapSetLink()
{
}

// FS2 0x0027d250
// callcheck: operator_delete, ~MotionConditionList, operator_delete

ReloadLink::~ReloadLink()
{
}

// FS2 0x0027d2d0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

GunAttackLink::~GunAttackLink()
{
}

// FS2 0x0027d350
// callcheck: operator_delete, ~MotionConditionList, operator_delete

GunLink::~GunLink()
{
}

// FS2 0x0027d3d0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

BtAttackLink::~BtAttackLink()
{
}

// FS2 0x0027d450
// callcheck: operator_delete, ~MotionConditionList, operator_delete

BattleLink::~BattleLink()
{
}

// FS2 0x0027d4d0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

DoorLink::~DoorLink()
{
}

// FS2 0x0027d550
// callcheck: operator_delete, ~MotionConditionList, operator_delete

LadderLink::~LadderLink()
{
}

// FS2 0x0027d5d0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

JumpLink::~JumpLink()
{
}

// FS2 0x0027d650
// callcheck: operator_delete, ~MotionConditionList, operator_delete

FallLink::~FallLink()
{
}

// FS2 0x0027d6d0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

UpLink::~UpLink()
{
}

// FS2 0x0027d750
// callcheck: operator_delete, ~MotionConditionList, operator_delete

PickupLink::~PickupLink()
{
}

// FS2 0x0027d7d0
// callcheck: operator_delete, ~MotionConditionList, operator_delete

JackLink::~JackLink()
{
}

// FS2 0x0027d850
// callcheck: operator_delete, ~MotionConditionList, operator_delete

WalkLink::~WalkLink()
{
}


// ───────────────────────────────────────
// The special actions: conditions by name in PlayerMotion::m_specials, which
// a script or the brain switches on for a frame (setSpecial). Walk carries
// them after the plain ones. Each frame initSlots14 takes allowed away before
// the links' onUpdate gives it back, and initSlots18 takes enabled away before
// input and brain speak, so a switch lasts the frame it was given.

// Base: vtable 0x317d50 (0x317d50 has no onApply). layout: host -- EE: as
// MotionCondition, then +0x14 allowed, +0x15 enabled.
// Its map is 0x281730's insert; the first of a name stays.
class MotionSpecial : public MotionCondition
{
public:
    MotionSpecial(StageCharacter *owner, PlayerMotion *motion_, const char *name)
        : MotionCondition(owner, motion_)
    {
        motion_->m_specials.insert(name, this);
    }
    void onUpdate() override;
    virtual void clearAllowed();
    void clearEnabled();
    virtual void setEnabled(bool on);
    // What PlayerMotion::specialMotion answers.
    virtual int motionId() const { return Motion_None; }

    u_char allowed = 0;         // FS2 +0x14
    u_char enabled = 0;         // FS2 +0x15
};

// FS2 0x00281540

void MotionSpecial::onUpdate()
{
    allowed = 1;
}

// FS2 0x001df0a0

void MotionSpecial::clearAllowed()
{
    allowed = 0;
}

// FS2 0x001df030

void MotionSpecial::clearEnabled()
{
    enabled = 0;
}

// FS2 0x001da4e0

void MotionSpecial::setEnabled(bool on)
{
    enabled = on;
}

// The row of the sheet whose label is name; -1 none.
// FS2 0x002813e0
// callcheck: strcmp

static int motionIdOfLabel(const char *name)
{
    for (int id = 0; id < kMotionDefCount; id++) {
        const char *label = gMotionDefs[id].labelText;
        if (label != nullptr && strcmp(label, name) == 0)
            return id;
    }
    return -1;
}

// Asks for a motion of the sheet, by its id or its label. Vtable 0x317d30.
// layout: host -- EE 0x1c.
class MotionSpecialAction : public MotionSpecial
{
public:
    // FS2 0x002814a0
    MotionSpecialAction(StageCharacter *owner, PlayerMotion *motion_, const char *name, int id_ = -1)
        : MotionSpecial(owner, motion_, name), id(id_ == -1 ? motionIdOfLabel(name) : id_) {}
    void onApply() override;
    int motionId() const override { return id; }

    int id;                     // FS2 +0x18
};

// FS2 0x00281460

void MotionSpecialAction::onApply()
{
    if (enabled != 0 && allowed != 0)
        motion->request(id);
}

// cr_1, cr_2, cr_3, what the partner answers a call with: the switch stays
// until the condition has seen it, and only a living partner takes it.
// Vtable 0x317d10. layout: host -- EE 0x20.
class MotionPartnerCall : public MotionSpecialAction
{
public:
    // FS2 0x002812e0
    MotionPartnerCall(StageCharacter *owner, PlayerMotion *motion_, const char *name)
        : MotionSpecialAction(owner, motion_, name) {}
    void onUpdate() override;
    void onApply() override;
    void setEnabled(bool on) override;

private:
    u_char pending = 0;         // FS2 +0x1c
};

// FS2 0x002812c0

void MotionPartnerCall::onUpdate()
{
    if (character->m_endurance > 0)
        allowed = 1;
}

// FS2 0x002811c0

void MotionPartnerCall::setEnabled(bool on)
{
    enabled = on;
    pending = on;
}

// In a car the partner has only cr_2 (0xbb for 0xf2) and the voice of cr_3.
// FS2 0x002811d0
// callcheck: isPartner, isMode2, request, request, playNamedHeard2

void MotionPartnerCall::onApply()
{
    constexpr int kCallMotion      = 0xf2;
    constexpr int kCallVoiceMotion = 0xf3;
    constexpr int kCarCallMotion   = 0xbb;

    if (pending == 0 || allowed == 0)
        return;
    if (character->isPartner() && motion->motionId != kCarCallMotion && motion->motionId != kCallMotion) {
        if (!character->isMode2())
            motion->request(id);
        else if (id == kCallMotion)
            motion->request(kCarCallMotion);
        else if (id == kCallVoiceMotion)
            character->playNamedHeard2("PC_END", character->m_xform.position, nullptr);
    }
    pending = 0;
}

// The stage's car, seen from a character. Walls a place beside it must keep
// clear of (the same masks as a walk's, the car itself left out).
constexpr u_int kCarWall       = 0xe8021f3;
constexpr u_int kCarPathSkip   = 0x2040000;
constexpr u_int kCarBodySkip   = 0x40000;
constexpr float kCarPlaceLift  = 50.0f;     // above the ground, for the rays
constexpr float kCarDoorHeight = 50.0f;     // between the car and the character
constexpr float kCarDoorArc    = 0.7853982f; // the character faces the door within this

// A way out of center to place is clear, and a sphere of radius fits there.
static bool carPlaceClear(StageCar *car, const float *center, float radius, const float *place)
{
    sceVu0FVECTOR raised = { place[0], place[1], place[2] + kCarPlaceLift, place[3] };
    return !gStageScene->segmentHitsAny(center, raised, 0, kCarWall, kCarPathSkip, car)
        && !gStageScene->sphereHitsAny(radius, place, kCarWall, kCarBodySkip, car);
}

// The door of car this character can reach, 0 or 1; -1 none. The side the
// car's points take is door == 0. facing: it must look at the door.
// FS2 0x00280fd0
// callcheck: getDoorBone, vecDistance, atan2Octant, addAngle, addAngle, segmentHitsAny

int StageCharacter::findCarDoor(StageCar *car, bool facing)
{
    const float *mine = m_xform.position;
    sceVu0FVECTOR from = { mine[0], mine[1], mine[2] + kCarPlaceLift, mine[3] };

    for (int door = 0; door < 2; door++) {
        sceVu0FVECTOR bone;
        const float reach = car->getDoorBone(bone, door == 0);
        bone[2] += kCarPlaceLift;
        if (vecDistance(from, bone) > reach || fabsf(car->m_xform.position[2] - mine[2]) > kCarDoorHeight)
            continue;
        if (facing) {
            const float bearing = atan2Octant(from[0] - bone[0], -(from[1] - bone[1]));
            const float off = addAngle(addAngle(bearing, -m_xform.rotation[2]), -kMathPi);
            if (fabsf(off) >= kCarDoorArc)
                continue;
        }
        if (!gStageScene->segmentHitsAny(from, bone, 0, kCarWall, kCarBodySkip, car))
            return door;
    }
    return -1;
}

// FS2 0x00215300

bool StageCharacter::isPassenger()
{
    return m_abilities.driving == 2;
}

// The player at a door of the scene's car. Vtable 0x317cf0. layout: host --
// EE 0x1c, +0x18 bringPartner (getCarSpecialFlag; scripts set it).
class MotionGetOnCar : public MotionSpecial
{
public:
    // FS2 0x00280f50
    MotionGetOnCar(StageCharacter *owner, PlayerMotion *motion_, const char *name)
        : MotionSpecial(owner, motion_, name) {}
    void onUpdate() override;
    void onApply() override;

    u_char bringPartner = 0;    // FS2 +0x18
};

// FS2 0x00280ee0
// callcheck: isNotRunning, findCarDoor

void MotionGetOnCar::onUpdate()
{
    StageCar *car = gStageScene->car;
    if (gStageScene->fade->isNotRunning() && car != nullptr && character->findCarDoor(car, true) != -1)
        allowed = 1;
}

// FS2 0x00280ea0
// callcheck: request

void MotionGetOnCar::onApply()
{
    if (enabled != 0 && allowed != 0)
        motion->request(Motion_CarGetIn);
}

// FS2 0x001dee50
// callcheck: find

bool PlayerMotion::getCarSpecialFlag()
{
    auto *found = m_specials.find("GettingOnCar");
    return found != nullptr && static_cast<MotionGetOnCar *>(found->value)->bringPartner != 0;
}

// FS2 0x001dee90
// callcheck: find

void PlayerMotion::setCarSpecialFlag(bool on)
{
    auto *found = m_specials.find("GettingOnCar");
    if (found != nullptr)
        static_cast<MotionGetOnCar *>(found->value)->bringPartner = on;
}

// The character at the wheel, the car all but stopped. Vtable 0x317cd0.
// layout: host -- EE 0x1c, +0x18 side.
class MotionGetOffCar : public MotionSpecial
{
public:
    // FS2 0x00280e20
    MotionGetOffCar(StageCharacter *owner, PlayerMotion *motion_, const char *name)
        : MotionSpecial(owner, motion_, name) {}
    void onUpdate() override;
    void onApply() override;

    u_char side = 1;            // FS2 +0x18: which side was found clear
};

// With a partner aboard the door's place must be clear as well as the sill.
// FS2 0x00280bd0
// callcheck: isNotRunning, isCreeping, isMode2, getCenter, getSillPoint, segmentHitsAny, sphereHitsAny, getDoorPlace, segmentHitsAny, sphereHitsAny

void MotionGetOffCar::onUpdate()
{
    if (!gStageScene->fade->isNotRunning())
        return;
    StageCar *car = static_cast<StageCar *>(character->m_gear);
    if (car == nullptr || !car->isCreeping() || character->m_lookAt != nullptr)
        return;
    StageCharacter *partner = static_cast<StageCharacter *>(gStageScene->partner);
    const bool aboard = partner != nullptr && partner->isMode2();
    if (aboard && partner->m_lookAt != nullptr)
        return;

    sceVu0FVECTOR center;
    car->getCenter(center);
    center[2] += kCarPlaceLift;
    for (int door = 0; door < 2; door++) {
        const bool first = door == 0;
        sceVu0FVECTOR place;
        float radius = car->getSillPoint(place, first);
        if (!carPlaceClear(car, center, radius, place))
            continue;
        if (aboard) {
            radius = car->getDoorPlace(place, first);
            if (!carPlaceClear(car, center, radius, place))
                continue;
        }
        allowed = 1;
        side = first;
        return;
    }
}

// FS2 0x00280b60
// callcheck: resetPlace, request

void MotionGetOffCar::onApply()
{
    if (enabled == 0 || allowed == 0)
        return;
    allowed = 0;
    onUpdate();
    if (allowed != 0) {
        static_cast<StageCar *>(character->m_gear)->resetPlace();
        motion->request(Motion_CarGetOut);
    }
}

// A foe at the door of a car all but stopped, its bite candidate inside.
// The side is the door it stands at. Vtable 0x317cb0. layout: host -- EE 0x20.
class MotionDragOutCar : public MotionGetOffCar
{
public:
    // FS2 0x00280a60
    MotionDragOutCar(StageCharacter *owner, PlayerMotion *motion_, const char *name)
        : MotionGetOffCar(owner, motion_, name) {}
    void onUpdate() override;
    void onApply() override;
    void clearAllowed() override;

    StageCharacter *victim = nullptr; // FS2 +0x1c
};

// FS2 0x00281630

void MotionDragOutCar::clearAllowed()
{
    victim = nullptr;
    allowed = 0;
}

// FS2 0x00280690
// callcheck: isNotRunning, getStageSelect, getBiteTarget, isCrawling, isMode2, findCarDoor, getCenter, getSillPoint, segmentHitsAny, sphereHitsAny, getDoorPlace, segmentHitsAny, sphereHitsAny

void MotionDragOutCar::onUpdate()
{
    if (!gStageScene->fade->isNotRunning() || character->m_abilities.flag3be == 0)
        return;
    if (stageObjRecord(character->m_itemDef)->kind == CharacterKind_Srt)
        return;
    StageObject *candidate = character->getBiteTarget();
    if (candidate == nullptr || candidate->getKind() != StageObject_Character)
        return;
    if (victim == nullptr)
        victim = static_cast<StageCharacter *>(candidate);
    else if (victim != candidate)
        return;
    if (victim->m_biteProof != 0 || victim->getBrainPace() == 0 || victim->m_endurance <= 0)
        return;
    StageCar *car = static_cast<StageCar *>(victim->m_gear);
    if (car == nullptr || !car->isCrawling() || victim->m_lookAt != nullptr)
        return;

    // Whoever sits beside the victim must be free too.
    StageCharacter *partner = static_cast<StageCharacter *>(gStageScene->partner);
    if (victim == partner) {
        if (gStageScene->player->m_lookAt != nullptr)
            return;
    }
    else if (partner != nullptr && partner->isMode2() && partner->m_lookAt != nullptr) {
        return;
    }

    const int door = character->findCarDoor(car, false);
    if (door == -1)
        return;
    const bool first = door == 0;
    sceVu0FVECTOR center;
    car->getCenter(center);
    center[2] += kCarPlaceLift;
    sceVu0FVECTOR place;
    float radius = car->getSillPoint(place, first);
    if (!carPlaceClear(car, center, radius, place))
        return;
    radius = car->getDoorPlace(place, first);
    if (!carPlaceClear(car, center, radius, place))
        return;
    allowed = 1;
    side = first;
}

// FS2 0x00280620
// callcheck: resetPlace, request

void MotionDragOutCar::onApply()
{
    if (enabled == 0 || allowed == 0)
        return;
    allowed = 0;
    onUpdate();
    if (allowed != 0) {
        static_cast<StageCar *>(victim->m_gear)->resetPlace();
        motion->request(Motion_CarDragOut);
    }
}

enum MotionGadget
{
    MotionGadget_Elevator = 0,  // Elevator*: the object zs9901; arg the floor
    MotionGadget_SigLamp  = 1,
    MotionGadget_NumLock  = 2,  // arg 0 the pad, 1 open, 2 close
    MotionGadget_SevenKey = 3,
    MotionGadget_None     = 4,
};

// StageScene::owned.
enum SceneOwnedSlot
{
    SceneOwned_SigLamp  = 0,
    SceneOwned_NumLock  = 1,
    SceneOwned_SevenKey = 2,
};

// Something of the stage a character stands at: the elevator, the signal lamp
// and the two locks. Vtable 0x317c90. layout: host -- EE 0x30.
class MotionGadgetSpecial : public MotionSpecial
{
public:
    MotionGadgetSpecial(StageCharacter *owner, PlayerMotion *motion_, const char *name);
    ~MotionGadgetSpecial() override;
    void onUpdate() override;
    void onApply() override;
    void clearAllowed() override;
    // What a script asks of the gadget.
    void setText(const char *text);
    const char *getText() const { return text; }

private:
    void askOwned(int index);
    void applyOwned(int index, int motionId);

    int    kind = MotionGadget_None; // FS2 +0x18
    int    arg = 0;             // FS2 +0x1c
    char   text[0x10] = {};     // FS2 +0x20
};

// FS2 0x00280410
// callcheck: MotionCondition, insert, strncmp, strcmp, strcmp, strcmp, strcmp, strcmp, strncmp, strcmp, strcmp, strcmp

MotionGadgetSpecial::MotionGadgetSpecial(StageCharacter *owner, PlayerMotion *motion_, const char *name)
    : MotionSpecial(owner, motion_, name)
{
    constexpr size_t kElevatorLength = 8;   // "Elevator"
    constexpr size_t kNumLockLength  = 7;   // "NumLock"

    if (strncmp(name, "Elevator", kElevatorLength) == 0) {
        const char *floor = name + kElevatorLength;
        kind = MotionGadget_Elevator;
        if (strcmp(floor, "B1") == 0)
            arg = 1;
        else if (strcmp(floor, "B3") == 0)
            arg = 2;
        else if (strcmp(floor, "BxB1") == 0)
            arg = 3;
        else if (strcmp(floor, "BxB3") == 0)
            arg = 4;
        else
            kind = MotionGadget_None;
    }
    else if (strcmp(name, "SigLamp") == 0) {
        kind = MotionGadget_SigLamp;
    }
    else if (strncmp(name, "NumLock", kNumLockLength) == 0) {
        const char *action = name + kNumLockLength;
        kind = MotionGadget_NumLock;
        if (*action == '\0')
            arg = 0;
        else if (strcmp(action, "Open") == 0)
            arg = 1;
        else if (strcmp(action, "Close") == 0)
            arg = 2;
        else
            kind = MotionGadget_None;
    }
    else if (strcmp(name, "SevenKey") == 0) {
        kind = MotionGadget_SevenKey;
    }
}

// FS2 0x00280010
// callcheck: ~MotionCondition, operator_delete (the base's body)

MotionGadgetSpecial::~MotionGadgetSpecial()
{
}

// FS2 0x00280400
// callcheck: strcpy

void MotionGadgetSpecial::setText(const char *text_)
{
    strcpy(text, text_);
}

// FS2 0x00280080

void MotionGadgetSpecial::clearAllowed()
{
    text[0] = '\0';
    allowed = 0;
}

// The player, with the gadget there and no party under way: the gadget says.
// PC: one helper for the ROM's three copies.
void MotionGadgetSpecial::askOwned(int index)
{
    SceneOwnedObject *gadget = gStageScene->owned[index];
    if (character->isPlayer() && gadget != nullptr && !gStageScene->party->isRecordBusy()
        && !gStageScene->party->isRunning())
        allowed = gadget->canUse();
}

// FS2 0x00280230
// callcheck: isPlayer, FUN_001fe6b0, isRunning, isPlayer, FUN_001fe6b0, isRunning, isPlayer, FUN_001fe6b0, isRunning, getZs9901, FUN_00294140

void MotionGadgetSpecial::onUpdate()
{
    switch (kind) {
    case MotionGadget_SevenKey:
        askOwned(SceneOwned_SevenKey);
        break;
    case MotionGadget_NumLock:
        if (arg != 0)
            allowed = 1;
        else
            askOwned(SceneOwned_NumLock);
        break;
    case MotionGadget_SigLamp:
        askOwned(SceneOwned_SigLamp);
        break;
    case MotionGadget_Elevator:
        if (StageObject *elevator = gStageScene->getZs9901())
            allowed = elevatorCanCall(elevator, character->m_xform.position, character->m_xform.rotation, arg);
        break;
    default:
        break;
    }
}

// Asks again, and the gadget takes the text or not.
void MotionGadgetSpecial::applyOwned(int index, int motionId)
{
    allowed = 0;
    onUpdate();
    if (allowed != 0 && gStageScene->owned[index]->start(text))
        motion->request(motionId);
}

// FS2 0x00280090
// callcheck: request, request, request, request, getZs9901, FUN_00294280

void MotionGadgetSpecial::onApply()
{
    constexpr int kMotionSevenKey  = 0xfe;
    constexpr int kMotionNumLock   = 0xfd;
    constexpr int kMotionSigLamp   = 0xfc;

    if (enabled == 0 || allowed == 0)
        return;
    switch (kind) {
    case MotionGadget_SevenKey:
        applyOwned(SceneOwned_SevenKey, kMotionSevenKey);
        break;
    case MotionGadget_NumLock:
        if (arg != 0)
            motion->request(Motion_SpLockHand);
        else
            applyOwned(SceneOwned_NumLock, kMotionNumLock);
        break;
    case MotionGadget_SigLamp:
        applyOwned(SceneOwned_SigLamp, kMotionSigLamp);
        break;
    case MotionGadget_Elevator:
        elevatorCall(gStageScene->getZs9901(), arg);
        break;
    default:
        break;
    }
}

void MotionLink::addSpecial(const char *name)
{
    if (auto *found = motion->m_specials.find(name))
        m_conditions.add(found->value);
}

// PC: the ROM reads through the null of a name the character lacks: zero page
// on the EE; the host says no.
// FS2 0x001def60
// callcheck: find

bool PlayerMotion::isSpecialAllowed(const char *name)
{
    auto *found = m_specials.find(name);
    return found != nullptr && found->value->allowed != 0;
}

// FS2 0x001def90
// callcheck: find

void PlayerMotion::setSpecial(const char *name, bool on)
{
    if (auto *found = m_specials.find(name))
        found->value->setEnabled(on);
}

// FS2 0x001deed0
// callcheck: find, find

bool PlayerMotion::isSpecialOnly(const char *name)
{
    if (m_conditions.find(name) != nullptr)
        return false;
    return m_specials.find(name) != nullptr;
}

// FS2 0x001ded30

bool PlayerMotion::setHold(int id)
{
    constexpr int kLastMotion = 0xff;
    if (id < 0 || id > kLastMotion)
        return false;
    m_holds[0] = id;
    return true;
}

// FS2 0x001ded60

bool PlayerMotion::setHoldIfFree(int id)
{
    constexpr int kLastMotion = 0xff;
    if (id < 0 || id > kLastMotion || m_holds[0] != Motion_None)
        return false;
    m_holds[0] = id;
    return true;
}

// The ROM reads +0x18 of whatever special it finds: MotionSpecialAction::id.

int PlayerMotion::specialMotion(const char *name)
{
    auto *found = m_specials.find(name);
    return found != nullptr ? found->value->motionId() : Motion_None;
}

// Inline in the ROM (0x1da430): the class is this file's.
// callcheck: find, setText

bool PlayerMotion::armGadgetSpecial(const char *name, const char *text)
{
    auto *found = m_specials.find(name);
    if (found == nullptr)
        return false;
    MotionGadgetSpecial *gadget = static_cast<MotionGadgetSpecial *>(found->value);
    gadget->setText(text);
    gadget->setEnabled(true);
    return true;
}

namespace {

// rideCar's off: 0 gets in, these get out, any other by the door or, a
// driver, as CarExit_Turned.
constexpr int CarExit_Door = 1;     // at the door's place, the driver as CarExit_Turned
constexpr int CarExit_Turned = 2;   // at the seat, turned round
constexpr int CarExit_Seat = 3;     // at the seat, facing as the car

}  // namespace

// FS2 0x0017af50
// callcheck: isDriver, resetPlace, isMode1, setLights, setChildrenShown, setChildrenActive, setRotationVec, getSeatPoint, addAngle, getSeatPoint, isDriver, getDoorPlace, setPosition, updateMatrix, reset, reset, isViewLinked, getTopView, onViewTop, isMode1, isMode1, find, getWorldMatrix, getStageSelect, isMode1, reset, reset, reset, isViewLinked, getTopView, onViewTop, setChildrenShown, setChildrenActive, isPlayer, find0, getTopView, onLinkChanged, pushView, getTopView, removeView (stageObjRecord)

bool StageCharacter::rideCar(int off, int side)
{
    if (m_abilities.driving == 0)
        return false;

    StageCar *car = gStageScene->car;
    if (isDriver())
        car->resetPlace();

    if (off != 0) {
        if (isMode1()) {
            car->m_driver = nullptr;
            car->setLights(false);
        }
        m_gear = nullptr;
        m_tree->setChildrenShown(true);
        m_tree->setChildrenActive(true);
        m_model->data->flags |= ModelFlag_CastsShadow;
        m_model->pose->parentMatrix = nullptr;
        m_xform.setRotationVec(car->m_xform.rotation);

        sceVu0FVECTOR place;
        if (off == CarExit_Seat) {
            car->getSeatPoint(place, side);
        }
        else if (off == CarExit_Turned || isDriver()) {
            car->getSeatPoint(place, side);
            m_xform.rotation[2] = addAngle(m_xform.rotation[2], 3.1415927f);
        }
        else {
            car->getDoorPlace(place, side);
        }
        m_xform.setPosition(place);
        updateMatrix();
        m_motion->reset(Motion_Stand);
        m_ground->reset();
        if (isViewLinked())
            gStageScene->getTopView()->onViewTop();
    }
    else {
        m_gear = car;
        if (isMode1())
            car->m_driver = this;
        const char *seat = isMode1() ? "bn_dr" : "bn_pa";
        m_model->pose->parentMatrix = car->m_model->boneMap.find(seat)->value->getWorldMatrix();

        const int kind = stageObjRecord(m_itemDef)->kind;
        const bool drivesAtWheel = kind == CharacterKind_4 || kind == CharacterKind_A
            || kind == CharacterKind_8 || kind == CharacterKind_2;
        m_motion->reset(drivesAtWheel && isMode1() ? Motion_CarDrive : Motion_CarSit);
        m_ground->reset();
        if (isViewLinked())
            gStageScene->getTopView()->onViewTop();
        m_model->data->flags &= ~ModelFlag_CastsShadow;
        m_tree->setChildrenShown(false);
        m_tree->setChildrenActive(false);
    }

    if (isPlayer()) {
        StageObject *camera = gStageScene->objects->find0("camview", 0);
        if (off == 0) {
            if (gStageScene->getTopView() != camera) {
                camera->m_linked = this;
                camera->onLinkChanged();
                gStageScene->pushView(camera, 1, 1);
            }
        }
        else if (gStageScene->getTopView() == camera) {
            gStageScene->removeView(camera);
        }
    }
    return true;
}

// FS2 0x0017b3b0

bool StageCharacter::isSpecialAllowed(const char *name)
{
    return m_motion->isSpecialAllowed(name);
}

// FS2 0x0017b3c0

void StageCharacter::setSpecial(bool on, const char *name)
{
    m_motion->setSpecial(name, on);
}

// By the sheet's kind, the stage's gadgets and what the character can do.
// FS2 0x001df0b0
// callcheck: getStageSelect, getStageSelect, getStageSelect, getStageSelect, getZs9901, operator_new, FUN_002812e0, FUN_002814a0

void PlayerMotion::initList()
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    const int kind = stageObjRecord(owner->m_itemDef)->kind;
    constexpr int kSheetLead = 0xf;
    constexpr int kSheetSpawn = 0xb;
    constexpr int kSheetTank = 0xd;
    constexpr int kSheetDropper = 0x10;

    if (kind == kSheetLead || kind == CharacterKind_9 || kind == CharacterKind_1 || kind == CharacterKind_0) {
        new MotionPartnerCall(character, this, "cr_1");
        new MotionPartnerCall(character, this, "cr_2");
        new MotionPartnerCall(character, this, "cr_3");
    }

    static const struct { const char *name; int id; } kPlain[] = {
        { "sp_dig", -1 }, { "sp_hole", -1 }, { "sp_rake", -1 }, { "sp_make", -1 }, { "sp_brkon", -1 },
        { "sp_ron", -1 }, { "sp_lock", -1 }, { "sp_swich", -1 }, { "sp_fear", -1 }, { "sp_robo", -1 },
        { "sp_drop", -1 }, { "sp_pu_l", 0xd4 }, { "sp_shut", -1 },
    };
    for (const auto &entry : kPlain)
        new MotionSpecialAction(character, this, entry.name, entry.id);
    new MotionGadgetSpecial(character, this, "NumLockOpen");
    new MotionGadgetSpecial(character, this, "NumLockClose");
    new MotionSpecialAction(character, this, "sp_uryen");

    if (character->m_abilities.driving == 1) {
        new MotionGetOnCar(character, this, "GettingOnCar");
        new MotionGetOffCar(character, this, "GettingOffCar");
    }
    if (character->m_abilities.flag3be != 0)
        new MotionDragOutCar(character, this, "DragOutCar");

    if (gStageScene->getZs9901() != nullptr) {
        new MotionGadgetSpecial(character, this, "ElevatorB1");
        new MotionGadgetSpecial(character, this, "ElevatorB3");
        new MotionGadgetSpecial(character, this, "ElevatorBxB1");
        new MotionGadgetSpecial(character, this, "ElevatorBxB3");
    }
    if (gStageScene->owned[SceneOwned_SigLamp] != nullptr)
        new MotionGadgetSpecial(character, this, "SigLamp");
    if (gStageScene->owned[SceneOwned_NumLock] != nullptr)
        new MotionGadgetSpecial(character, this, "NumLock");
    if (gStageScene->owned[SceneOwned_SevenKey] != nullptr)
        new MotionGadgetSpecial(character, this, "SevenKey");

    if (kind == kSheetSpawn) {
        static const struct { const char *name; int id; } kSpawn[] = {
            { "sp_at1_1", 0xd8 }, { "sp_at1_2", 0xd9 }, { "sp_spawn_in", 0xda }, { "sp_spawn_idle", 0xdb },
            { "sp_spawn_out", 0xdc }, { "sp_spawn_numb", 0xdd }, { "sp_in", 0xde }, { "sp_idle", 0xdf },
            { "sp_out", 0xe0 }, { "sp_numb1", 0xe1 }, { "sp_land", 0xe3 }, { "sp_fal", 0xe2 },
            { "sp_flop", 0xe4 }, { "sp_ret", 0xe5 }, { "sp_numb0", 0xe6 },
        };
        for (const auto &entry : kSpawn)
            new MotionSpecialAction(character, this, entry.name, entry.id);
    }
    if (kind == kSheetTank) {
        static const struct { const char *name; int id; } kTank[] = {
            { "sp_at1_1", 0xe7 }, { "sp_at1_2", 0xe8 }, { "sp_spawn", 0xe9 }, { "sp_tank", 0xea },
            { "sp_ret", 0xeb }, { "sp_stop", 0xec },
        };
        for (const auto &entry : kTank)
            new MotionSpecialAction(character, this, entry.name, entry.id);
    }
    if (kind == kSheetDropper)
        new MotionSpecialAction(character, this, "sp_t_drp");
    if (kind == CharacterKind_1)
        new MotionSpecialAction(character, this, "sp_jack", 0xd6);
    else if (kind == CharacterKind_A)
        new MotionSpecialAction(character, this, "sp_jack");
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
    ~AlwaysCondition() override {}                                       // FS2 0x00277140
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
        if (motion->scopeOn == 0 && viewSwitch != 0)
            allows.viewSwitch = 1;
        if (motion->scopeOn == 0 && zoom != 0)
            allows.zoom = 1;
    }
    if (character->m_lampItem != nullptr && character->m_lampItem->m_active != 0 && lamp != 0)
        allows.lamp = 1;
    if (isHoldableMotion(id) || isMovingMotion(id))
        allows.standing = 1;
    if (headSway != 0 && character->m_290 == nullptr)
        allows.head = 1;
    if (motion->scopeOn != 0)
        allows.aim = 1;
    else if (upperSway != 0 && character->m_290 == nullptr && !isAttackMotion(id))
        allows.aim = 1;
}

// FS2 0x00275160
// callcheck: isPartner, getStageSelect, FUN_001def60, FUN_001def90 (stageObjRecord)

void AlwaysCondition::onApply()
{
    PlayerMotion *const m = motion;
    if (character->isPartner()) {
        const int kind = stageObjRecord(character->m_itemDef)->kind;
        if ((kind == 0xf || kind == CharacterKind_9 || kind == CharacterKind_1 || kind == CharacterKind_0)
            && gStageScene->gameOver != nullptr && m->isSpecialAllowed("cr_2"))
            m->setSpecial("cr_2", true);
    }
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
        if (m->scopeOn == 0) {
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
    ~QuickTurnCondition() override {}                                    // FS2 0x00276960
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
    ~SquatCondition() override {}                                        // FS2 0x002770d0
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
    ~TurnCondition() override {}                                         // FS2 0x00276f80
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
    ~MoveCondition() override {}                                         // FS2 0x00276f10
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
    return object != nullptr && object->getKind() == StageObject_Car;
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
                     : !gStageScene->sphereHitsKind(ground->radius2, center, StageObject_Car, kFaceWall, 0, nullptr);
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
    ~LevelCondition() override {}                                        // FS2 0x00276ea0
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
    ~DoorCondition() override {}                                         // FS2 0x00276ce0
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
    ~PutItemCondition() override {}                                      // FS2 0x00276c70
    void onUpdate() override;                                            // FS2 0x00271070
    void onApply() override;                                             // FS2 0x00271020
};

// Finds the item in reach and bends for it. EE vtable 0x3175b0.
// layout: host
class PickupItemCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~PickupItemCondition() override {}                                   // FS2 0x00276c00
    void onUpdate() override;                                            // FS2 0x00270e90
    void onApply() override;                                             // FS2 0x00270c50
};

// The partner lifts the character up a ledge (Motion_Pair). EE vtable 0x317650.
// layout: host
class LevelLiftCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~LevelLiftCondition() override {}                                    // FS2 0x00276e30
    void onUpdate() override;                                            // FS2 0x002721a0
    void onApply() override;                                             // FS2 0x00272030
};

// The ladder ahead. EE vtable 0x317630. layout: host -- +0x14 yaw, +0x20
// point, +0x30 height on the base condition.
class LadderCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~LadderCondition() override {}                                       // FS2 0x00276dc0
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
    ~JumpCondition() override {}                                         // FS2 0x00276d50
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
    ~BtOnCondition() override {}                                         // FS2 0x00277060
    void onUpdate() override;                                            // FS2 0x00274810
    void onApply() override;                                             // FS2 0x00274650
};

// Keeps m_290, the target, and the character turned to it. Vtable 0x317470.
// layout: host -- as MotionCondition.
class LockCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~LockCondition() override {}                                         // FS2 0x002767a0
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
    ~BtAttackCondition() override {}                                     // FS2 0x00276730
    void onUpdate() override;                                            // FS2 0x0026e9e0
    void onApply() override;                                             // FS2 0x0026e500
};

// The next blow of a combo. Vtable 0x317430. layout: host.
class BtAtkComboCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~BtAtkComboCondition() override {}                                   // FS2 0x002766c0
    void onUpdate() override;                                            // FS2 0x0026e4e0
    void onApply() override;                                             // FS2 0x0026e280
};

// Steps in the stance, and out of it. Vtable 0x317410. layout: host.
class BtMoveCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~BtMoveCondition() override {}                                       // FS2 0x00276650
    void onUpdate() override;                                            // FS2 0x0026e260
    void onApply() override;                                             // FS2 0x0026df60
};

// The gun up, for who holds a firearm and has room ahead. Vtable 0x3176d0.
// layout: host.
class GunOnCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~GunOnCondition() override {}                                        // FS2 0x00276ff0
    void onUpdate() override;                                            // FS2 0x00274390
    void onApply() override;                                             // FS2 0x002741d0
};

// Steps with the gun up, and out of it. Vtable 0x3173b0. layout: host.
class GunMoveCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    void onUpdate() override;                                            // FS2 0x0026d670
    void onApply() override;                                             // FS2 0x0026d090
};

// The scope: up of the d-pad into it, the right stick zooms, down out of it.
// Vtable 0x3173d0. layout: host.
class SnipeCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~SnipeCondition() override {}                                        // FS2 0x00276570
    void onUpdate() override;                                            // FS2 0x0026d7f0
    void onApply() override;                                             // FS2 0x0026d6a0
};

// A shot, or the butt. Vtable 0x3173f0. layout: host.
class GunAttackCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~GunAttackCondition() override {}                                    // FS2 0x002765e0
    void onUpdate() override;                                            // FS2 0x0026dee0
    void onApply() override;                                             // FS2 0x0026d950
};

// Rounds into the gun: asked, or at the end of two seconds' standing.
// Vtable 0x317510. layout: host.
class ReloadCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~ReloadCondition() override {}                                       // FS2 0x002769d0
    void onUpdate() override;                                            // FS2 0x002704c0
    void onApply() override;                                             // FS2 0x002702e0
};

// The motion call. Vtable 0x317530. layout: host.
class ShowtCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~ShowtCondition() override {}                                        // FS2 0x00276a40
    void onUpdate() override;                                            // FS2 0x002706f0
    void onApply() override;                                             // FS2 0x00270690
};

// A bite on the character it has in mind. Vtable 0x3174b0. layout: host.
class BiteCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~BiteCondition() override {}                                         // FS2 0x00276880
    void onUpdate() override;                                            // FS2 0x0026fd30
    void onApply() override;                                             // FS2 0x0026fb60
};

// The alternate action of a player who is not the lead. Vtable 0x3174d0.
// layout: host.
class JackCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~JackCondition() override {}                                         // FS2 0x002768f0
    void onUpdate() override;                                            // FS2 0x002701c0
    void onApply() override;                                             // FS2 0x00270120
};

// Room ahead of the head to throw. Vtable 0x317550. layout: host.
class ThrowCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~ThrowCondition() override {}                                        // FS2 0x00276ab0
    void onUpdate() override;                                            // FS2 0x00270780
    void onApply() override;                                             // FS2 0x00270710
};

// A trap taken up. Vtable 0x317570. layout: host.
class TrapCancelCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~TrapCancelCondition() override {}                                   // FS2 0x00276b20
    void onUpdate() override;                                            // FS2 0x00270980
    void onApply() override;                                             // FS2 0x002708c0

private:
    StageTrapItem *trap = nullptr; // FS2 +0x14
};

// A trap laid, on a floor 60 ahead. Vtable 0x317590. layout: host.
class TrapSetCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~TrapSetCondition() override {}                                      // FS2 0x00276b90
    void onUpdate() override;                                            // FS2 0x00270a70
    void onApply() override;                                             // FS2 0x00270a00

private:
    sceVu0FVECTOR place = {};      // FS2 +0x20
    StageTrapItem *trap = nullptr; // FS2 +0x30
};

// LadderMove. layout: host -- as MotionCondition.
class LadderMoveCondition : public MotionCondition
{
public:
    using MotionCondition::MotionCondition;
    ~LadderMoveCondition() override {}                                   // FS2 0x00276810
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
    return object != nullptr && object->getKind() == StageObject_Car ? reinterpret_cast<StageCar *>(object) : nullptr;
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
    } else if (character->hasBiter()) {
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
// callcheck: attackSoundName, lockSoundName, unlockSoundName, playNamedHeard2, breakPart, isControlled (three names, one soundName with the format)

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
        door->m_hp -= character->m_unarmedAttack;
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
// callcheck: isInFront, canReachThrough (the ROM has reachesThroughDoor inline here)

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
// callcheck: isControlled, askOpenDoor, FUN_00172530

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
// callcheck: isControlled, askCloseDoor, FUN_00172530

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
// callcheck: isControlled, askDoorKey, FUN_00172530, FUN_00172530

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
        && walk->edgeFloor->getKind() == StageObject_Car) {
        const int kind = standing->getKind();
        ontoCar = kind == StageObject_MapObject || kind == StageObject_Car;
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
// callcheck: find, request, FUN_001e0360, request, request, FUN_001e0360, request, request, request

void LadderMoveCondition::onApply()
{
    constexpr float kOffTop = 185.0f;
    constexpr float kOffFoot = 35.0f;
    const CharacterWants &wants = character->m_wants;
    const int kind = wants.moveKind;
    if (kind == CharacterMove_Back || kind == CharacterMove_Walk || kind == CharacterMove_Run) {
        const LadderLink *ladder = static_cast<const LadderLink *>(motion->m_links.find("Ladder")->value);
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
// callcheck: MotionConditionList, operator_new, find, add (addCondition)

LadderLink::LadderLink(StageCharacter *owner, PlayerMotion *motion_)
    : MotionLink(owner, motion_)
{
    addCondition("LadderMove");
}

// FS2 0x0027b6e0
// callcheck: find

void LadderLink::onRequest()
{
    const LadderCondition *ladder = static_cast<const LadderCondition *>(motion->m_conditions.find("Ladder")->value);
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
// callcheck: MotionConditionList, operator_new, find, add, getStageSelect, getStageSelect, find (addCondition; the list of 0x1df0b0 is not written, so the sp_ names of data 0x2fb3a0 stay out)

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
    static const char *const kSpecials[] = { // data 0x2fb3a0
        "cr_2", "cr_3", "cr_1", "GettingOnCar", "DragOutCar", "ElevatorB1", "ElevatorB3", "ElevatorBxB1",
        "ElevatorBxB3", "SigLamp", "NumLock", "SevenKey", "sp_swich", "sp_dig", "sp_hole", "sp_rake",
        "sp_make", "sp_brkon", "sp_ron", "sp_lock", "sp_fear", "sp_robo", "sp_drop", "sp_pu_l", "sp_shut",
        "NumLockOpen", "NumLockClose", "sp_t_drp", "sp_jack", "sp_uryen",
    };
    for (const char *name : kSpecials)
        addSpecial(name);
    const int kind = stageObjRecord(character->m_itemDef)->kind;
    if (kind == CharacterKind_NoShadow) {
        static const char *const kSpawnFirst[] = { "sp_at1_2", "sp_spawn_in", "sp_in", "sp_fal", "sp_numb0" }; // data 0x2fb420
        for (const char *name : kSpawnFirst)
            addSpecial(name);
    }
    if (kind == CharacterKind_HideD) {
        static const char *const kTankFirst[] = { "sp_at1_1", "sp_at1_2", "sp_spawn", "sp_ret" }; // data 0x2fb440
        for (const char *name : kTankFirst)
            addSpecial(name);
    }
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
// callcheck: find (m_links.find)

void WalkLink::onLeave(MotionLink *next)
{
    if (character->m_abilities.flag3c3 == 0) {
        auto *attack = motion->m_links.find("BtAttack");
        if (attack == nullptr || next != attack->value)
            character->m_290 = nullptr;
    }
    wallFace = nullptr;
    edgeFound = 0;
}

// The stage's own motions: the last links of the ROM's ctor.

// FS2 0x00282d80

void GadgetLink::onRequest()
{
    gadget->takeMotionText(special->getText());
}

// FS2 0x00282d60

void GadgetLink::onStart()
{
    gadget->takeMotionUser(character);
}

// FS2 0x00282d10

void GadgetLink::onRun()
{
    if (!gadget->isBusy())
        finished = 1;
}

// FS2 0x00282cf0

void GadgetLink::onCancel()
{
    gadget->isSlot2c();
}

// FS2 0x00283c50
// callcheck: ~MotionLink, operator_delete

GadgetLink::~GadgetLink()
{
}

// FS2 0x00282b70
// callcheck: getStageSelect (stageObjRecord)

void SpFearLink::onRun()
{
    if (character->m_ground->wasHit != 0) {
        finished = 1;
        character->setEndurance(0);
    }
    else if (finished != 0) {
        motion->request(Motion_Stand);
    }
    else {
        character->setEndurance(stageObjRecord(character->m_itemDef)->endurance);
    }
}

// FS2 0x00282c90

void SpFearLink::onCancel()
{
    constexpr int kMotionBittenBehind = 0xa0;
    constexpr int kMotionBittenFront  = 0xa1;
    if (motion->m_request == kMotionBittenFront || motion->m_request == kMotionBittenBehind)
        character->setEndurance(1);
}

// FS2 0x00283bf0
// callcheck: ~MotionLink, operator_delete

SpFearLink::~SpFearLink()
{
}

// PC: one helper for the ROM's four copies.
void SpRingLink::playRing(const char *suffix, bool loop)
{
    constexpr u_int kRingPlayFlags = PosePlay_NoBlend | PosePlay_Ease;
    if (ring == nullptr || ring->m_model == nullptr)
        return;
    char name[0x20];
    sprintf(name, "%s_%s", ring->m_name, suffix);
    if (ModelMotion *found = ring->m_model->findMotion(0, name))
        ring->m_model->playMotion(found, loop, false, kRingPlayFlags, 0);
}

// FS2 0x00282a50

void SpRingLink::onRequest()
{
    constexpr int   kRingTarget = 5;    // oZS09_04
    constexpr float kRingRate   = 0.35f;
    if (motion->m_request != Motion_SpRingOn)
        return;
    ring = gStageScene->objects->find1(stageMotionTargetName(kRingTarget), 0);
    if (ring != nullptr) {
        const sceVu0FVECTOR angles = { 0.0f, 0.0f, ring->m_xform.rotation[2], 0.0f };
        motion->startWalkFile(kRingRate, motion->requestedFile(), ring->m_xform.position, angles, true, false);
    }
    sound = nullptr;
}

// FS2 0x00282830
// callcheck: sprintf, findMotion, playMotion, sprintf, findMotion, playMotion, sprintf, findMotion, playMotion (playRing)

void SpRingLink::onStart()
{
    if (ring == nullptr)
        return;
    switch (motion->m_request) {
    case Motion_SpRingOff:  playRing("sp_rof", false); break;
    case Motion_SpRingHold: playRing("sp_rs", true); break;
    case Motion_SpRingOn:   playRing("sp_ron", false); break;
    default: break;
    }
}

// The motion's sound mark starts it at P_AC01; the one of 0xca stops it.
// FS2 0x002826e0

void SpRingLink::onRun()
{
    const u_int events = character->m_model->m_motionEvents;
    switch (motion->motionId) {
    case Motion_SpRingOff:
        if ((events & MotionEvent_Sound) != 0 && sound != nullptr) {
            ring->stopSound(sound);
            sound = nullptr;
        }
        break;
    case Motion_SpRingOn:
        if ((events & MotionEvent_Sound) != 0 && ring != nullptr) {
            if (StageObject *mark = gStageScene->objects->find6("P_AC01", 0))
                sound = ring->playNamedHeard2("EVE_S09_04", mark->m_xform.position, nullptr);
        }
        break;
    case Motion_SpRingHold:
        if (counter == 0) {
            if (finished != 0) {
                counter = 1;
                finished = 0;
            }
        }
        else if (character->m_model->pose->isBoneEnding(nullptr)) {
            finished = 1;
        }
        break;
    default:
        break;
    }
}

// FS2 0x00282630
// callcheck: sprintf, findMotion, playMotion (playRing)

void SpRingLink::onLeave(MotionLink *next)
{
    (void)next;
    if (motion->motionId == Motion_SpRingOff)
        playRing("s", true);
}

// FS2 0x002829f0

void SpRingLink::onCancel()
{
    if (motion->m_request == Motion_SpRingOff || motion->m_request == Motion_SpRingHold)
        return;
    if (sound != nullptr)
        ring->stopSound(sound);
    ring = nullptr;
    sound = nullptr;
}

// FS2 0x00283b90
// callcheck: ~MotionLink, operator_delete

SpRingLink::~SpRingLink()
{
}

// PC: one helper for the two onRequest of the ROM.
void SpRoboLink::takeObject(const char *name)
{
    object = gStageScene->objects->find1(name, 0);
    motion->startWalkFile(0.0f, motion->requestedFile(), character->m_xform.position, character->m_xform.rotation,
                          false, true);
}

// FS2 0x00282570
// callcheck: stageMotionTargetName, find1, requestedFile, startWalkFile (takeObject)

void SpRoboLink::onRequest()
{
    constexpr int kRoboTarget = 6;      // oDC07_05
    takeObject(stageMotionTargetName(kRoboTarget));
}

// FS2 0x00282250
// callcheck: stageMotionTargetName, find1, requestedFile, startWalkFile (takeObject)

void SpDropLink::onRequest()
{
    constexpr int kDropTarget = 7;      // oMK21_09
    takeObject(stageMotionTargetName(kDropTarget));
}

// FS2 0x00282460

void SpRoboLink::onStart()
{
    constexpr u_int kObjectPlayFlags = PosePlay_NoBlend | PosePlay_Loop | PosePlay_FromKey;
    if (object == nullptr || object->m_model == nullptr)
        return;
    wasActive = object->m_active != 0;
    object->m_tree->setActive(true);
    object->m_tree->setShown(true);
    memcpy(savedPosition, object->m_xform.position, sizeof(savedPosition));
    object->m_xform.setPosition(character->m_xform.position);
    memcpy(savedRotation, object->m_xform.rotation, sizeof(savedRotation));
    object->m_xform.setRotationVec(character->m_xform.rotation);
    char name[0x20];
    sprintf(name, "%s_sp", object->m_name);
    if (ModelMotion *found = object->m_model->findMotion(0, name))
        object->m_model->pose->play(found, kObjectPlayFlags, 0);
}

// FS2 0x00282380

void SpRoboLink::onLeave(MotionLink *next)
{
    constexpr u_int kObjectBackFlags = PosePlay_NoBlend | PosePlay_FromKey;
    (void)next;
    if (object == nullptr || object->m_model == nullptr)
        return;
    object->m_tree->setActive(wasActive != 0);
    object->m_tree->setShown(wasActive != 0);
    object->m_xform.setPosition(savedPosition);
    object->m_xform.setRotationVec(savedRotation);
    char name[0x20];
    sprintf(name, "%s", object->m_name);
    if (ModelMotion *found = object->m_model->findMotion(0, name))
        object->m_model->playMotion(found, true, false, kObjectBackFlags, 0);
}

// FS2 0x00282560

void SpRoboLink::onCancel()
{
    object = nullptr;
}

// FS2 0x00282320
// callcheck: ~MotionLink, operator_delete

SpRoboLink::~SpRoboLink()
{
}

// FS2 0x00283b20
// callcheck: ~MotionLink, operator_delete

SpDropLink::~SpDropLink()
{
}

// FS2 0x00282200
// callcheck: MotionLink, strcpy

SpPadlockLink::SpPadlockLink(StageCharacter *owner, PlayerMotion *motion_, const char *name)
    : MotionLink(owner, motion_)
{
    strcpy(objectName, name);
}

// FS2 0x00282120

void SpPadlockLink::onRequest()
{
    constexpr float kPadlockRate = 0.35f;
    constexpr float kHalfTurn = -3.1415927f; // 0xc0490fdb
    StageObject *padlock = gStageScene->objects->find1(objectName, 0);
    const sceVu0FVECTOR *matrix = padlock->getWorldMatrix();
    const sceVu0FVECTOR place = { matrix[3][0], matrix[3][1], character->m_xform.position[2], matrix[3][3] };
    sceVu0FVECTOR angles;
    matrixToRotYXZ(angles, padlock->getWorldMatrix());
    angles[2] = addAngle(angles[2], kHalfTurn);
    motion->startWalkFile(kPadlockRate, motion->requestedFile(), place, angles, true, false);
}

// FS2 0x00283ac0
// callcheck: ~MotionLink, operator_delete

SpPadlockLink::~SpPadlockLink()
{
}

// Which lock and which way; the rotation goes back when it is over.
// FS2 0x00281ff0

void SpNumLockLink::onRequest()
{
    if (motion->m_request == Motion_SpLockWait) {
        waited = 0.0f;
    }
    else if (motion->m_request == Motion_SpLockHand) {
        auto *special = static_cast<MotionGadgetSpecial *>(motion->m_specials.find("NumLockOpen")->value);
        opening = special->enabled;
        if (opening == 0)
            special = static_cast<MotionGadgetSpecial *>(motion->m_specials.find("NumLockClose")->value);
        else
            waited = 0.0f;
        lock = gStageScene->objects->find1(special->getText(), 0);
        // PC: the ROM reads the zero page with no such object.
        if (lock != nullptr)
            memcpy(savedRotation, lock->m_xform.rotation, sizeof(savedRotation));
        if (SceneOwnedObject *gadget = gStageScene->owned[SceneOwned_NumLock])
            gadget->setRecordFlag31(true);
    }
}

bool SpNumLockLink::isLock() const
{
    return lock != nullptr && lock->getKind() == StageObject_NumLock;
}

// Opening: each click puts a dial on the code; all right, after 3 s of them
// standing the lock comes off. Closing: the lock shows, each click spins one.
// PC: part of the ROM's onRun.
void SpNumLockLink::runTurn()
{
    constexpr float kSettleSeconds = 3.0f;
    const bool clicked = (character->m_model->m_motionEvents & MotionEvent_Click) != 0;
    if (!isLock()) {
        finished = clicked;
        return;
    }
    if (opening == 0) {
        if (counter == 0) {
            lock->playNamedHeard2("NLOCK_UL", lock->m_xform.position, nullptr);
            lock->m_tree->setActive(true);
            lock->m_tree->setShown(true);
            counter++;
        }
        if (clicked) {
            lock->playNamedHeard2("NLOCK_REV", lock->m_xform.position, nullptr);
            numLockSpinDial(lock, counter - 1);
            counter++;
        }
        return;
    }
    if (character->m_model->pose->isBoneEnding(nullptr)) {
        motion->request(Motion_SpLockWait);
        return;
    }
    if (!clicked)
        return;
    if (!numLockDialsRight(lock)) {
        lock->playNamedHeard2("NLOCK_REV", lock->m_xform.position, nullptr);
        numLockSetDial(lock, counter);
        counter++;
    }
    else if (waited >= kSettleSeconds) {
        if (counter == 0) {
            counter++;
        }
        else {
            lock->playNamedHeard2("NLOCK_UL", lock->m_xform.position, nullptr);
            lock->m_tree->setActive(false);
            lock->m_tree->setShown(false);
            finished = 1;
        }
    }
    else {
        motion->request(Motion_SpLockWait);
    }
}

// PC: part of the ROM's onRun.
void SpNumLockLink::runSettle()
{
    constexpr float kSettleSeconds = 3.0f;
    if (numLockDialsTurning(lock))
        return;
    waited += gStageScene->clock->getDeltaSeconds();
    if (waited >= kSettleSeconds)
        motion->request(Motion_SpLockTurn);
}

// The lock takes the two marks' rotation before the dials go.
// PC: part of the ROM's onRun.
void SpNumLockLink::runReach()
{
    if (!character->m_model->pose->isBoneEnding(nullptr))
        return;
    if (isLock()) {
        StageObject *round = gStageScene->objects->find6("P_NUMROT", 0);
        StageObject *tilt = gStageScene->objects->find6("P_NUMROT_Z", 0);
        lock->m_xform.rotation[0] = round->m_xform.rotation[0];
        lock->m_xform.rotation[1] = round->m_xform.rotation[1];
        lock->m_xform.rotation[2] = tilt->m_xform.rotation[2];
    }
    motion->request(Motion_SpLockTurn);
}

// FS2 0x00281bb0
// callcheck: isBoneEnding, playNamedHeard2, setActive, setShown, numLockSpinDial, isBoneEnding, numLockDialsRight, playNamedHeard2, numLockSetDial, playNamedHeard2, setActive, setShown, request, request, numLockDialsTurning, getDeltaSeconds, request, isBoneEnding, find6, find6, request (runTurn, runSettle, runReach; slot +0x1f4 getKind)

void SpNumLockLink::onRun()
{
    if (finished != 0)
        return;
    switch (motion->motionId) {
    case Motion_SpLockTurn: runTurn(); break;
    case Motion_SpLockWait: runSettle(); break;
    case Motion_SpLockHand: runReach(); break;
    default: break;
    }
}

// FS2 0x00281f50

void SpNumLockLink::onCancel()
{
    if (motion->m_request == Motion_SpLockTurn || motion->m_request == Motion_SpLockWait || lock == nullptr)
        return;
    if (SceneOwnedObject *gadget = gStageScene->owned[SceneOwned_NumLock])
        gadget->setRecordFlag31(false);
    if (isLock())
        memcpy(lock->m_xform.rotation, savedRotation, sizeof(savedRotation));
    lock = nullptr;
}

// FS2 0x00281a40
// callcheck: ~MotionLink, operator_delete

SpNumLockLink::~SpNumLockLink()
{
}

// FS2 0x00281b30
// callcheck: MotionLink, find, add (addSpecial)

BossAttackLink::BossAttackLink(StageCharacter *owner, PlayerMotion *motion_) : MotionLink(owner, motion_)
{
    addSpecial("sp_tank");              // sdata 0x31ac0c
}

// FS2 0x00283a00
// callcheck: ~MotionLink, operator_delete

BossAttackLink::~BossAttackLink()
{
}

// FS2 0x00281aa0
// callcheck: MotionLink, find, add (addSpecial)

BossIdleLink::BossIdleLink(StageCharacter *owner, PlayerMotion *motion_) : MotionLink(owner, motion_)
{
    for (const char *name : { "sp_at1_1", "sp_numb1", "sp_spawn_numb" }) // data 0x2fb470
        addSpecial(name);
}

// FS2 0x00283a60
// callcheck: ~MotionLink, operator_delete

BossIdleLink::~BossIdleLink()
{
}

// FS2 0x001ed410
// callcheck: DamageDeadLink, MotionCondition, MotionLink, memset, find, FUN_00278db0, FUN_0027a0b0, FUN_0027a910, FUN_0027aa90, FUN_0027aea0, FUN_0027b000, FUN_0027b730, BossIdleLink, BossAttackLink, SpNumLockLink, SpPadlockLink, SpDropLink, SpRoboLink, SpRingLink, FUN_00282da0, FUN_00282fa0, FUN_00283560, FUN_00283740, FUN_00283960, getStageSelect, operator_new, LiteralMap, LiteralMap, LiteralMap, FUN_0026c6b0, FUN_00276510, insert, balance, FUN_001df0b0, FUN_0027c520, FUN_0027c140, FUN_0027b640, FUN_0027cc50, FUN_002775d0, insert, FUN_0027c370, balance (the links, the conditions and their maps: game/playermotion.h)

// The car's links. Each of the door ones works behind a fade: to black, the
// change of seat at full black (rideCar's reset leaves the link), the fade back.

constexpr float kCarFadeSeconds = 0.5f;
constexpr float kFadeFromHere   = -1.0f; // StageFade::start: from where it stands

// The pad goes off while the fade runs.
// FS2 0x00283910
// callcheck: setActive, start

void CarDoorLink::onStart()
{
    character->m_input->setActive(false);
    gStageScene->fade->start(kCarFadeSeconds, 1.0f, kFadeFromHere);
}

// FS2 0x00283780
// callcheck: start, setActive

void CarDoorLink::onLeave(MotionLink *)
{
    gStageScene->fade->start(kCarFadeSeconds, 0.0f, kFadeFromHere);
    character->m_input->setActive(true);
}

// FS2 0x002837d0
// callcheck: playNamedHeard2, isFullInMode2, rideCar, requestNow, getCarSpecialFlag, isPassenger, rideCar, playNamedHeard2, playNamedHeard2

void ApproachCarLink::onRun()
{
    StageCar *car = gStageScene->car;
    if (counter == 0) {
        car->playNamedHeard2("CAR_DROP", car->m_xform.position, nullptr);
        counter++;
    }
    if (!gStageScene->fade->isFullInMode2())
        return;
    if (!character->rideCar(0, 0)) {
        motion->requestNow(Motion_Stand);
        return;
    }
    StageCharacter *partner = static_cast<StageCharacter *>(gStageScene->partner);
    if (motion->getCarSpecialFlag() && partner != nullptr && partner->isPassenger())
        partner->rideCar(0, 0);
    StageObject *gear = character->m_gear;
    gear->playNamedHeard2("CAR_IN", gear->m_xform.position, nullptr);
    gear->playNamedHeard2("CAR_DRCL", gear->m_xform.position, nullptr);
}

// FS2 0x00283700
// callcheck: find

void ApproachOutLink::onRequest()
{
    side = static_cast<MotionGetOffCar *>(motion->m_specials.find("GettingOffCar")->value)->side;
}

// FS2 0x00283600
// callcheck: playNamedHeard2, isFullInMode2, rideCar, requestNow, isMode2, rideCar, playNamedHeard2

void ApproachOutLink::onRun()
{
    if (counter == 0) {
        StageObject *gear = character->m_gear;
        gear->playNamedHeard2("CAR_DROP", gear->m_xform.position, nullptr);
        counter++;
    }
    if (!gStageScene->fade->isFullInMode2())
        return;
    if (!character->rideCar(CarExit_Door, side)) {
        motion->requestNow(Motion_CarSit);
        return;
    }
    StageCharacter *partner = static_cast<StageCharacter *>(gStageScene->partner);
    if (partner != nullptr && partner->isMode2())
        partner->rideCar(CarExit_Door, side);
    StageCar *car = gStageScene->car;
    car->playNamedHeard2("CAR_DRCL", car->m_xform.position, nullptr);
}

// who, just put at place, turns its back on the one pulled out, and everything
// that follows a character is told.
// callcheck: setPosition, atan2Octant, addAngle, updateMatrix, applyMotion, reset, isViewLinked, getTopView, onViewTop

static void standBesideCar(StageCharacter *who, const float *place, const StageCharacter *pulled)
{
    who->m_xform.setPosition(place);
    const float *at = who->m_xform.position;
    const float *theirs = pulled->m_xform.position;
    const float bearing = atan2Octant(at[0] - theirs[0], -(at[1] - theirs[1]));
    memcpy(who->m_xform.rotation, pulled->m_xform.rotation, sizeof(sceVu0FVECTOR));
    who->m_xform.rotation[2] = addAngle(bearing, -kMathPi);
    who->updateMatrix();
    who->m_model->applyMotion(who->m_xform.position, who->m_xform.rotation);
    who->m_ground->reset();
    if (who->isViewLinked())
        gStageScene->getTopView()->onViewTop();
}

// FS2 0x00283500
// callcheck: find

void ApproachBiteLink::onRequest()
{
    auto *special = static_cast<MotionDragOutCar *>(motion->m_specials.find("DragOutCar")->value);
    side = special->side;
    prey = special->victim;
    prey->m_lookAt = character;
}

// FS2 0x00283490
// callcheck: start

void ApproachBiteLink::onStart()
{
    gStageScene->fade->start(kCarFadeSeconds, 1.0f, kFadeFromHere);
}

// FS2 0x00283060
// callcheck: getBiteTarget, playNamedHeard2, isMode2, isFullInMode2, rideCar, request, requestNow, rideCar, getCenter, getDoorPoint, segmentHitsAny, sphereHitsAny, getDoorPlace, startBite (standBesideCar's inside)

void ApproachBiteLink::onRun()
{
    constexpr int kExitTries = 5;

    const bool held = prey != nullptr && prey == character->getBiteTarget();
    const bool inCar = held && prey->m_gear != nullptr;
    if (counter == 0 && inCar) {
        character->playNamedHeard2("CAR_PLOUT", character->m_xform.position, nullptr);
        counter++;
    }

    // Whoever else sits in the car comes out with the prey.
    StageCharacter *other = gStageScene->player;
    if (prey == other) {
        other = static_cast<StageCharacter *>(gStageScene->partner);
        if (other != nullptr && !other->isMode2())
            other = nullptr;
    }

    if (!gStageScene->fade->isFullInMode2())
        return;
    if (!inCar) {
        motion->request(Motion_Stand);
        return;
    }
    StageCar *car = static_cast<StageCar *>(prey->m_gear);
    PlayerMotion *preyMotion = prey->m_motion;
    if (!prey->rideCar(CarExit_Turned, side)) {
        motion->request(Motion_Stand);
        preyMotion->requestNow(Motion_CarSit);
        return;
    }

    sceVu0FVECTOR place;
    if (other != nullptr) {
        other->rideCar(CarExit_Seat, side);
        sceVu0FVECTOR center;
        car->getCenter(center);
        center[2] += kCarPlaceLift;
        for (int i = 0; i < kExitTries; i++) {
            const float radius = car->getDoorPoint(place, side, i);
            if (carPlaceClear(car, center, radius, place))
                break;
        }
        standBesideCar(other, place, prey);
    }
    car->getDoorPlace(place, side);
    standBesideCar(character, place, prey);

    character->m_biteTarget = prey;
    character->m_allows.bite = 1;
    prey->m_lookAt = nullptr;
    motion->startBite(prey);
}

// FS2 0x00283030
// callcheck: start

void ApproachBiteLink::onLeave(MotionLink *)
{
    gStageScene->fade->start(kCarFadeSeconds, 0.0f, kFadeFromHere);
}

// Unless it is bitten by now, the prey goes free.
// FS2 0x002834c0

void ApproachBiteLink::onCancel()
{
    constexpr int kMotionBittenBehind = 0xa0;
    constexpr int kMotionBittenFront  = 0xa1;

    if (prey == nullptr)
        return;
    const int request = prey->m_motion->m_request;
    if (request != kMotionBittenFront && request != kMotionBittenBehind)
        prey->m_lookAt = nullptr;
    prey = nullptr;
}

// A driver who sits still at rest and works the pedals and the wheel otherwise.
// FS2 0x00282e30
// callcheck: getStageSelect, isMode1, hasSettled, request, hasSteerRested, request

void DriveLink::onRun()
{
    const int kind = stageObjRecord(character->m_itemDef)->kind;
    if (kind == CharacterKind_4 || kind == CharacterKind_A || kind == CharacterKind_8 || kind == CharacterKind_2)
        return;
    StageCar *car = static_cast<StageCar *>(character->m_gear);
    if (car == nullptr)
        return;
    const int running = motion->motionId;
    if (!character->isMode1())
        return;
    const int wanted = car->hasSettled() && car->hasSteerRested() ? Motion_CarSit : Motion_CarDrive;
    if (running != wanted)
        motion->request(wanted);
}

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

    // The conditions, in the ROM's order.
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    probe = new MotionProbe(character);
    m_always = new AlwaysCondition(character, this);
    m_conditions.insert("Always", m_always);
    m_conditions.insert("Squat", new SquatCondition(character, this));
    m_conditions.insert("BtOn", new BtOnCondition(character, this));
    m_conditions.insert("GunOn", new GunOnCondition(character, this));
    m_conditions.insert("Turn", new TurnCondition(character, this));
    m_conditions.insert("Move", new MoveCondition(character, this));
    m_conditions.insert("Level", new LevelCondition(character, this));
    m_conditions.insert("LevelLift", new LevelLiftCondition(character, this));
    m_conditions.insert("Ladder", new LadderCondition(character, this));
    m_conditions.insert("Jump", new JumpCondition(character, this));
    m_conditions.insert("Door", new DoorCondition(character, this));
    m_conditions.insert("PutItem", new PutItemCondition(character, this));
    m_conditions.insert("PickupItem", new PickupItemCondition(character, this));
    m_conditions.insert("TrapSet", new TrapSetCondition(character, this));
    m_conditions.insert("TrapCancel", new TrapCancelCondition(character, this));
    m_conditions.insert("Throw", new ThrowCondition(character, this));
    m_conditions.insert("Showt", new ShowtCondition(character, this));
    m_conditions.insert("Reload", new ReloadCondition(character, this));
    m_conditions.insert("QuickTurn", new QuickTurnCondition(character, this));
    m_conditions.insert("Jack", new JackCondition(character, this));
    m_conditions.insert("Bite", new BiteCondition(character, this));
    m_conditions.insert("LadderMove", new LadderMoveCondition(character, this));
    m_conditions.insert("Lock", new LockCondition(character, this));
    m_conditions.insert("BtAttack", new BtAttackCondition(character, this));
    m_conditions.insert("BtAtkCombo", new BtAtkComboCondition(character, this));
    m_conditions.insert("BtMove", new BtMoveCondition(character, this));
    m_conditions.insert("GunAttack", new GunAttackCondition(character, this));
    m_conditions.insert("Snipe", new SnipeCondition(character, this));
    m_conditions.insert("GunMove", new GunMoveCondition(character, this));
    m_conditions.balance();
    initList();

    // The links, in the ROM's order (architecture.md "The links of the ROM's ctor").
    damageDeadLink = new DamageDeadLink(character, this);
    walkLink = new WalkLink(character, this);
    m_links.insert("Walk", walkLink);
    m_links.insert("Jack", new JackLink(character, this));
    pickupLink = new PickupLink(character, this);
    m_links.insert("Pickup", pickupLink);
    m_links.insert("Up", new UpLink(character, this));
    m_links.insert("Fall", new FallLink(character, this));
    m_links.insert("Jump", new JumpLink(character, this));
    m_links.insert("Ladder", new LadderLink(character, this));
    doorLink = new DoorLink(character, this);
    m_links.insert("Door", doorLink);
    m_links.insert("Battle", new BattleLink(character, this));
    m_links.insert("BtAttack", new BtAttackLink(character, this));
    m_links.insert("Gun", new GunLink(character, this));
    m_links.insert("GunAttack", new GunAttackLink(character, this));
    m_links.insert("Reload", new ReloadLink(character, this));
    m_links.insert("DiveFall", new DiveFallLink(character, this));
    m_links.insert("DiveLand", new DiveLandLink(character, this));
    trapSetLink = new TrapSetLink(character, this);
    m_links.insert("TrapSet", trapSetLink);
    trapCancelLink = new TrapCancelLink(character, this);
    m_links.insert("TrapCancel", trapCancelLink);
    m_links.insert("TrapHit", new TrapHitLink(character, this));
    throwLink = new ThrowLink(character, this);
    m_links.insert("Throw", throwLink);
    m_links.insert("Bite", new BiteLink(character, this));
    m_links.insert("Bitten", new BittenLink(character, this));
    m_links.insert("Damage", new DamageLink(character, this));
    m_links.insert("DamageLand", new DamageLandLink(character, this));
    m_links.insert("DamageFatal", new DamageFatalLink(character, this));
    m_links.insert("DamageDead", damageDeadLink);
    m_links.insert("Revive", new ReviveLink(character, this));
    if (character->m_abilities.driving == 1) {
        m_links.insert("ApproachCar", new ApproachCarLink(character, this));
        m_links.insert("ApproachOut", new ApproachOutLink(character, this));
    }
    if (character->m_abilities.driving == 1 || character->m_abilities.driving == 2)
        m_links.insert("Drive", new DriveLink(character, this));
    m_links.insert("SpWait", new MotionLink(character, this));
    m_links.insert("NPCReaction", new NpcReactionLink(character, this));
    if (character->m_abilities.flag3be != 0)
        m_links.insert("ApproachBite", new ApproachBiteLink(character, this));
    static const char *const kGadgets[] = { "SigLamp", "NumLock", "SevenKey" }; // SceneOwned order
    for (int i = 0; i < 3; i++) {
        if (SceneOwnedObject *gadget = gStageScene->owned[i]) {
            auto *special = static_cast<MotionGadgetSpecial *>(m_specials.find(kGadgets[i])->value);
            m_links.insert(kGadgets[i], new GadgetLink(character, this, gadget, special));
        }
    }
    m_links.insert("SpFear", new SpFearLink(character, this));
    m_links.insert("SpRing", new SpRingLink(character, this));
    m_links.insert("SpRobo", new SpRoboLink(character, this));
    m_links.insert("SpDrop", new SpDropLink(character, this));
    m_links.insert("SpPadlock", new SpPadlockLink(character, this, "oMT02_30"));
    m_links.insert("SpNumLock", new SpNumLockLink(character, this));
    if (kind == CharacterKind_NoShadow) {
        m_links.insert("BossAttack", new BossAttackLink(character, this));
        m_links.insert("BossIdle", new BossIdleLink(character, this));
    }
    if (kind == CharacterKind_HideD)
        m_links.insert("BossAttack", new BossAttackLink(character, this));
    m_links.balance();
}

// FS2 0x001ed2a0
// callcheck: operator_delete, FUN_0026c650, ~LiteralMap, ~LiteralMap, ~LiteralMap (the host deletes; the maps of the ctor)

PlayerMotion::~PlayerMotion()
{
    delete m_queue;
    delete m_walkQueue;
    for (auto *node = m_links.first; node != nullptr; node = node->next)
        delete node->value;
    for (auto *node = m_conditions.first; node != nullptr; node = node->next)
        delete node->value;
    for (auto *node = m_specials.first; node != nullptr; node = node->next)
        delete node->value;
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
    for (auto *node = m_specials.first; node != nullptr; node = node->next)
        node->value->clearAllowed();
}

// FS2 0x001defd0

void PlayerMotion::initSlots18()
{
    for (auto *node = m_specials.first; node != nullptr; node = node->next)
        node->value->clearEnabled();
}

// FS2 0x001ec7e0
// callcheck: find, getStageSelect, noteMotion, resolveHold, findLink (cancelRequested, stageObjRecord)

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
    m_stanceTime = 0.0f;
    m_hitTime = 0.0f;
    m_soundEvent = 0;
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
    if (scopeOn != 0)
        setScope(false);
    m_bf0 = 0.0f;
    m_sprungTrap = nullptr;
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

// FS2 0x001e5d60

u_char PlayerMotion::setWallCheck(u_char on)
{
    const u_char was = m_wallCheck;
    m_wallCheck = on;
    CollisionBody *body = owner->m_ground->wallBody;
    if (body != nullptr)
        body->on = on;
    return was;
}

// FS2 0x001e5d90

void PlayerMotion::beginFrame()
{
    Clock *clock = owner->m_clock;
    ModelPose *pose = owner->m_model->pose;
    m_motionTime += clock->getDeltaSeconds();
    if (pose->isBoneEnding(nullptr))
        m_soundEvent = 0;
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

// FS2 0x001eb3b0

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
    auto *found = m_links.find(def->linkFunctionText);
    return found != nullptr ? found->value : nullptr;
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
// callcheck: find, getStageSelect, isBoneEnding (stageObjRecord, isEnding)

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
        m_soundEvent = 0;
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
// callcheck: find, isBoneEnding (isEnding)

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
// callcheck: find, isBoneEnding (isEnding)

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
// callcheck: find, noteMotion, resolveHold (cancelRequested)

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

// The eye of a body with no family, in the bone's space.
static const sceVu0FVECTOR kEyeOffsetDefault = { 0.0f, 11.0f, 11.0f, 1.0f }; /* data 0x305040 */

// FS2 0x001e5f70
// callcheck: identityMatrix, rotateByAngles, rotateByAngles, rotateMatrixZ, find, translateMatrix, applyMatrix, applyMatrix (getLampMatrix inline; the two applyMatrix are the two branches)

void PlayerMotion::getEyeMatrix(sceVu0FMATRIX out)
{
    getLampMatrix(out);
    if (m_modelFamily < 0 || m_modelFamily >= kMotionModelCount) {
        applyMatrix(out[3], out, kEyeOffsetDefault);
        return;
    }
    // The family's vector has its lanes the other way round.
    const float *eye = gMotionModels[m_modelFamily].a;
    const sceVu0FVECTOR offset = { eye[2], eye[1], eye[0], 1.0f };
    applyMatrix(out[3], out, offset);
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
constexpr int kSurfaceMoveColumn = 29;  /* data 0x30f870: the ninth family's, one sound */
constexpr int kSurfaceWallColumn = 30;
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

// FS2 0x002cede0
// callcheck: strcmp (findSurfaceEntry: the sound table's find, inline)

const SeEntry *getSurfaceMoveSound(int surface)
{
    if (surface < 0)
        return nullptr;
    return findSurfaceEntry(surface, kSurfaceMoveColumn);
}

// FS2 0x002cef50
// callcheck: strcmp (findSurfaceEntry: the sound table's find, inline)

const SeEntry *getSurfaceWallSound(int surface)
{
    if (surface < 0)
        return nullptr;
    return findSurfaceEntry(surface, kSurfaceWallColumn);
}

// FS2 0x001e0e20
// callcheck: hasGear, isNear (m_gear inline here)

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
        if (StageCar *target = gStageScene->car) {
            const auto within = [&](float range) { return target->isNear(range, character->m_xform.position); };
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

// FS2 0x00212160
// callcheck: getBrainPace, getStageSelect, getStageSelect (stageObjRecord)

bool StageCharacter::canBeJacked()
{
    const int kind = stageObjRecord(m_itemDef)->kind;
    return getBrainPace() != BrainState_Off && kind != CharacterKind_Srt && kind != CharacterKind_Yrt
        && gStageScene->player != this;
}

// FS2 0x00215370
// callcheck: getSaveData, getStageSelect (stageObjRecord; the ROM tests s1 unset off stage 0x12: here no)

bool StageCharacter::isJackBlank()
{
    constexpr int kMotionJackBlank = 0xf8;
    constexpr int kStageJackBlank = 0x12;
    return getStageSelect()->stage == kStageJackBlank && stageObjRecord(m_itemDef)->kind == CharacterKind_1
        && m_motion->motionId == kMotionJackBlank;
}

// FS2 0x00215480
// callcheck: getSaveData, getStageSelect, getStageSelect (stageObjRecord)

bool StageCharacter::canSeeVisions()
{
    constexpr int kStageVisionKind1 = 0x12;
    constexpr int kStageVisionKind0a = 0x15;
    constexpr int kStageVisionKind0b = 7;
    const int stage = getStageSelect()->stage;
    if (stage == kStageVisionKind1)
        return stageObjRecord(m_itemDef)->kind == CharacterKind_1;
    if (stage == kStageVisionKind0a || stage == kStageVisionKind0b)
        return stageObjRecord(m_itemDef)->kind == CharacterKind_0;
    return false;
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
// Kinds 1 and 9 strafe and walk with a gun only as the player. A live
// character's clocks run in seconds of the scene: flinch falls by 10, the
// unarmed MSP climbs back by 10 and endurance by m_enduranceRegen, not while
// bitten. Last, the player on a kyo model carries the point P_URIEN 400
// ahead on the floor, and the script's uryenchu says it is in motion 0xf0.
// FS2 0x0017a860
// callcheck: memset, initSlots14, isPlayer, beginFrame, memset, initSlots18, getLifeState, setFlinch, setEndurance, isPlayer, FUN_00215340, find6, sinCos, sinCos, raycast, vecDirection, scaleVec4, raycast, setPosition, setPosition, setNamed, setNamed, getStageSelect, getDeltaSeconds, getDeltaSeconds, getDeltaSeconds, getDeltaSeconds

void StageCharacter::sideHook0()
{
    constexpr int   kSheetKindCompanion = 1;
    constexpr int   kSheetKindWalker    = 9;
    constexpr int   kLifeStateAlive     = 2;
    constexpr int   kClockStep          = 10;
    constexpr int   kMotionBittenFirst  = 0xa0;
    constexpr int   kMotionBittenSecond = 0xa1;
    constexpr int   kMotionUrien        = 0xf0;
    constexpr float kUrienReach         = 400.0f;
    constexpr float kUrienHeight        = 50.0f;
    constexpr float kUrienBackOff       = 40.0f;
    constexpr float kUrienDrop          = 1000.0f;
    constexpr u_int kFaceUrienStop      = 0xec261ff;

    memset(&m_allows, 0, sizeof(m_allows));
    m_motion->initSlots14();
    const StageObjRecord *sheet = stageObjRecord(m_itemDef);
    if (sheet->kind == kSheetKindWalker || sheet->kind == kSheetKindCompanion) {
        const u_char on = isPlayer() ? 1 : 0;
        m_abilities.strafe = on;
        m_abilities.gunMove = on;
    }
    m_motion->beginFrame();
    memset(&m_wants, 0, sizeof(m_wants));
    m_motion->initSlots18();

    if (getLifeState() == kLifeStateAlive) {
        Clock *clock = gStageScene->clock;
        if (m_flinch > 0) {
            m_flinchClock += clock->getDeltaSeconds();
            if (m_flinchClock >= 1.0f) {
                setFlinch(m_flinch - kClockStep);
                m_flinchClock -= 1.0f;
            }
        }
        if (m_unarmedMsp < sheet->unarmedMsp) {
            m_unarmedClock += clock->getDeltaSeconds();
            if (m_unarmedClock >= 1.0f) {
                int value = m_unarmedMsp + kClockStep;
                if (value < 0)
                    value = 0;
                m_unarmedMsp = value <= sheet->unarmedMsp ? value : sheet->unarmedMsp;
                m_unarmedClock -= 1.0f;
            }
        }
        const int id = m_motion->motionId;
        if (id != kMotionBittenFirst && id != kMotionBittenSecond && m_endurance < sheet->endurance) {
            m_enduranceClock += clock->getDeltaSeconds();
            if (m_enduranceClock >= 1.0f) {
                setEndurance(m_endurance + m_enduranceRegen);
                m_enduranceClock -= 1.0f;
            }
        }
        if (m_lightHurtWait > 0.0f)
            m_lightHurtWait -= clock->getDeltaSeconds();
    }

    if (!isPlayer() || !hasKyoModel())
        return;
    if (StageObject *mark = gStageScene->objects->find6("P_URIEN", 0)) {
        const float *feet = m_xform.position;
        sceVu0FVECTOR from = { feet[0], feet[1], feet[2] + kUrienHeight, feet[3] };
        sceVu0FVECTOR to = { feet[0], feet[1], feet[2] + kUrienHeight, feet[3] };
        const float yaw = -m_xform.rotation[2];
        to[0] = sinCos(yaw, 0) * -kUrienReach + to[0] + 0.0f;
        to[1] = sinCos(yaw, 1) * -kUrienReach + to[1] + 0.0f;
        sceVu0FVECTOR hit;
        if (gStageScene->raycast(hit, from, to, 0, kFaceUrienStop, 0x40000, nullptr) != nullptr) {
            sceVu0FVECTOR back;
            vecDirection(back, from, hit);
            scaleVec4(-kUrienBackOff, back, back);
            for (int i = 0; i < 4; i++)
                to[i] = hit[i] - back[i];
        }
        memcpy(from, to, sizeof(from));
        to[2] -= kUrienDrop;
        if (gStageScene->raycast(hit, from, to, 0, kFaceFloor, 0x40000, nullptr) != nullptr)
            mark->m_xform.setPosition(hit);
        else
            mark->m_xform.setPosition(m_xform.position);
        mark->setDirty(true);
    }
    scriptRunner()->setNamed("uryenchu", m_motion->motionId == kMotionUrien ? 1 : 0);
}

// The model's name holds kyo.
// FS2 0x00215340
// callcheck: strstr

bool StageCharacter::hasKyoModel()
{
    return strstr(m_model->pose->name, "kyo") != nullptr;
}

// 0 off or at the brain's state 0, 2 alive or at its state 6, 3 down, 4 in
// the motions 0xb3, 0xb4, 5 in 0xb1, 0xb2; m_lifeOverride 6, or 7 at 0.
// FS2 0x00212460

int StageCharacter::getLifeState()
{
    constexpr int kMotionDownFirst  = 0xb3;
    constexpr int kMotionDownSecond = 0xb4;
    constexpr int kMotionFatalFirst = 0xb1;
    constexpr int kBrainDown        = 6;
    constexpr int kBrainOff         = 0;
    if (m_active == 0)
        return 0;
    if (m_lifeOverride >= 0)
        return m_lifeOverride != 0 ? LifeState_Leaving : LifeState_Left;
    if (m_brain != nullptr) {
        if (m_brain->getState() == kBrainDown)
            return 2;
        if (m_brain->getState() == kBrainOff)
            return 0;
    }
    const int id = m_motion->motionId;
    if (m_endurance == 0)
        return id == kMotionDownFirst || id == kMotionDownSecond ? 4 : 3;
    const bool fatal = (u_int)(id - kMotionFatalFirst) < 2u || (u_int)(m_motion->m_request - kMotionFatalFirst) < 2u;
    return fatal ? 5 : 2;
}

// FS2 0x0017a810

void StageCharacter::sideHook1()
{
    m_input->update();
    if (m_brain != nullptr)
        m_brain->updateRoute();
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
// callcheck: erase, pushBack, getStageSelect (setParent, detach, stageObjRecord, inline)

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
// callcheck: FUN_00217730, setShadowOwner, setPosition, StageNodeList__erase, erase, FUN_00217840

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
// callcheck: pushBack, erase, erase, find, FUN_00217700, FUN_00217770, getStageSelect, setMoving, setPositionXyz, setRotation, swapWeapons, dropItem, setPosition, attachTo, setShadowOwner, strcmp, setActive, setShown (the trees, setParent and bodyBone inline; the list tail and push_back are pushBack; getStageSelect is in stageObjRecord)

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
        if (static_cast<StageObject *>(item)->m_parent != nullptr || item->m_holder != nullptr || item->m_xform.dirty == 0
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


// FS2 0x0027c370
// callcheck: operator_new, MotionConditionList, find, add (the base's list; addCondition)

JackLink::JackLink(StageCharacter *owner, PlayerMotion *motion_) : MotionLink(owner, motion_)
{
    addCondition("Jack");
}

// FS2 0x0027c300
// callcheck: isRunning, start

void JackLink::onRequest()
{
    if (started == 0 && !gStageScene->party->isRunning()) {
        started = 1;
        gStageScene->party->start(character);
    }
}

// FS2 0x0027c2c0
// callcheck: stop

void JackLink::onCancel()
{
    if (started != 0) {
        gStageScene->party->stop();
        started = 0;
    }
}

// Only the scene's player keeps the view; it is let go when the player changes.
// The partner felled in one of the motions the party cannot be seen through
// ends the stance.
//
// FS2 0x0027c1e0
// callcheck: isPlayer

void JackLink::onRun()
{
    StageScene *scene = gStageScene;
    if (static_cast<StageObject *>(scene->player) == character)
        onRequest();
    else
        onCancel();
    if (character->isPlayer() && scene->partner != nullptr
        && static_cast<StageCharacter *>(scene->partner)->getProperty(CharProp_MotionFlagged, 0) != 0)
        finished = 1;
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

// FS2 0x0026f8d0

StageObject *longWeapon(const StageCharacter *character)
{
    return character->m_weapons[0];
}

// FS2 0x0026f780

bool isFirearmItem(const StageObject *weapon)
{
    return defOf(weapon).isFirearm();
}

bool holdsFirearm(const StageCharacter *character)
{
    return longWeapon(character) != nullptr && isFirearmItem(longWeapon(character));
}

// FS2 0x0026f720

int stageKindOf(const StageSelect *select)
{
    return select->kind;
}

// FS2 0x0026f730

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
    if (stageKindOf(getStageSelect()) == StageKind_Easy && isFirearmItem(weapon))
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
    if (!character->isControlled() || motion->scopeOn != 0)
        return;
    if (character->m_290 == nullptr)
        character->m_allows.lockFind = 1;
    else
        character->m_allows.lockSwitch = 1;
}

// FS2 0x0026ef30
// callcheck: addAngle, isControlled, getLockPoint, longWeapon, isFirearmItem, weaponSheet, findTargetAhead, stageKindOf, getStageSelect, vecDistance, findTargetBeside, isTargetValid, canSeeTarget, faceTarget

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

// ──────────────────────────────────────────────────────────────────────
// Firearms, and what else a hand does: the call, the alternate action, traps
// and throws.

namespace {

// pickGunMotion's slots (table 0x2f7ef0; the labels of the pistol's column).
enum GunSlot
{
    GunSlot_On          = 0,    // gn_on
    GunSlot_Walk        = 1,    // gn_bt_w
    GunSlot_Back        = 2,    // gn_bt_wb
    GunSlot_Right       = 3,    // gn_bt_wr
    GunSlot_Left        = 4,    // gn_bt_wl
    GunSlot_TurnRight   = 5,    // t_r over gn_aim
    GunSlot_TurnLeft    = 6,    // t_l over gn_aim
    GunSlot_Aim         = 7,    // gn_aim
    GunSlot_Fire        = 8,    // gn_fire
    GunSlot_ReloadBegin = 9,    // gn_rl_b: a gun loaded round by round
    GunSlot_ReloadRound = 10,   // gn_rl
    GunSlot_ReloadEnd   = 11,   // gn_rl_e
    GunSlot_ReloadAll   = 12,   // gn_rl_a: the magazine at once
    GunSlot_Butt        = 13,   // gn_at_01
    GunSlot_Off         = 14,   // gn_off
};

constexpr int   kMotionIdle        = 0;
constexpr int   kMotionTrapSet     = 0x98;
constexpr int   kMotionTrapCancel  = 0x99;
constexpr int   kMotionThrow       = 0x9d;
constexpr float kReloadIdleSeconds = 2.0f;  // standing this long loads the gun
constexpr float kGunOnReach        = 75.0f; // room the gun needs ahead to come up
constexpr float kGunKeepReach      = 70.0f; // to stay up
constexpr float kThrowReach        = 100.0f;
constexpr float kTrapReach         = 60.0f; // a trap goes this far ahead
constexpr float kTrapChestHeight   = 50.0f; // the ray that looks for room
constexpr float kTrapFloorDrop     = 70.0f; // .. and from there down to a floor
constexpr float kTrapFloorStep     = 20.0f; // no further than this from the feet
constexpr float kTrapWalkRate      = 0.35f;
constexpr float kThrowDefaultY     = -20.0f; // the default throw
constexpr float kThrowDefaultZ     = 5.0f;
constexpr float kScopeZoomStart    = 0.3f;
constexpr float kHalfPi            = 1.5707964f;
constexpr float kPi                = 3.1415927f;
constexpr float kViewNear          = 350.0f; // scrZ of camview_zoom at its widest

const float *headPoint(StageCharacter *character)
{
    return bodyBone(character, "bn_head")->getMatrix()[3];
}

// A wall one trips on, from the feet's x, y at the head's height to reach
// ahead of it.
// callcheck: find, find, sinCos, sinCos, segmentHitsAny
bool wallAhead(StageCharacter *character, float reach)
{
    sceVu0FVECTOR from, to;
    memcpy(from, character->m_xform.position, sizeof(from));
    memcpy(to, character->m_xform.position, sizeof(to));
    from[2] = headPoint(character)[2];
    to[2] = from[2];
    const float yaw = -character->m_xform.rotation[2];
    to[0] = sinCos(yaw, 0) * -reach + to[0] + 0.0f;
    to[1] = sinCos(yaw, 1) * -reach + to[1] + 0.0f;
    return gStageScene->segmentHitsAny(from, to, 0, kFaceSolid, kFaceFlagsTrip, nullptr);
}

// G_NN38, R_MURATA and G_26NEN load round by round (the ROM's three
// strcmp, inline; StageWeaponItem::loadsByMagazine is the same test).
bool loadsByRound(const StageObject *weapon)
{
    return strcmp(weapon->m_name, "G_NN38") == 0 || strcmp(weapon->m_name, "R_MURATA") == 0
        || strcmp(weapon->m_name, "G_26NEN") == 0;
}

// The two chains of itemDef compares (kind 5 or 6): a rifle.
bool isRifleItem(const StageObject *weapon)
{
    const int kind = defOf(weapon).kind;
    return kind == ItemKind_RifleFirst || kind == ItemKind_FirearmLast;
}

int clampRounds(int value, int high)
{
    if (value < 0)
        value = 0;
    return value < high ? value : high;
}

// Out of the gun: the target goes, the scope, and the pose's aim.
void dropGun(StageCharacter *character, PlayerMotion *motion)
{
    character->m_290 = nullptr;
    if (motion->scopeOn != 0)
        motion->setScope(false);
    character->m_model->pose->lookBone = nullptr;
}

} // namespace

// FS2 0x00216310
// callcheck: FUN_00217700, strcmp

StageObject *StageCharacter::findCarried(const char *name)
{
    for (StageList<StageObject *>::Node *node = m_items.head; node != nullptr; node = node->next) {
        if (strcmp(node->value->m_name, name) == 0)
            return node->value;
    }
    return nullptr;
}

// FS2 0x002a3010
// callcheck: getMagazineSetSound, playNamedHeard2

void StageCharacter::playMagazineSetSound()
{
    playNamedHeard2(static_cast<StageWeaponItem *>(m_weapons[0])->getMagazineSetSound(), m_xform.position, nullptr);
}

// FS2 0x002a3060
// callcheck: getReloadSetSound, playNamedHeard2

void StageCharacter::playReloadSetSound()
{
    playNamedHeard2(static_cast<StageWeaponItem *>(m_weapons[0])->getReloadSetSound(), m_xform.position, nullptr);
}

// FS2 0x002a30b0
// callcheck: getMagazineOutSound, playNamedHeard2

void StageCharacter::playMagazineOutSound()
{
    playNamedHeard2(static_cast<StageWeaponItem *>(m_weapons[0])->getMagazineOutSound(), m_xform.position, nullptr);
}

// FS2 0x002a2ad0
// callcheck: playNamedHeard2

void StageCharacter::playTrapOffSound()
{
    playNamedHeard2("TRAP_OFF", m_xform.position, nullptr);
}

// FS2 0x002a2af0
// callcheck: playNamedHeard2

void StageCharacter::playTrapSetSound()
{
    playNamedHeard2("TRAP_SET", m_xform.position, nullptr);
}

// Up: the zoom starts low, the body hides (the view is from inside it), the
// sight comes on, and camview_snipe takes the top of the view stack, one
// place lower for a character taken over.
// FS2 0x001dfd90
// callcheck: find0, setShown, removeView, copyVec4, setShown, strcmp, isTakenOver, pushView, pushView, setZoom

void PlayerMotion::setScope(bool on)
{
    ViewCamera *view = static_cast<ViewCamera *>(gStageScene->objects->find0("camview_snipe", 0));
    if (!on) {
        scopeOn = 0;
        owner->m_tree->setShown(true);
        if (gStageScene->menuProgs != nullptr)
            gStageScene->menuProgs->state = MenuProgSight_None;
        gStageScene->removeView(view);
        return;
    }
    scopeOn = 1;
    scopeZoom = kScopeZoomStart;
    memcpy(scopeRotation, owner->m_xform.rotation, sizeof(scopeRotation));
    owner->m_290 = nullptr;
    owner->m_tree->setShown(false);
    if (gStageScene->menuProgs != nullptr) {
        const StageObject *weapon = owner->m_weapons[0];
        gStageScene->menuProgs->state = weapon != nullptr && strcmp(weapon->m_name, "R_64C") == 0
            ? MenuProgSight_R64C : MenuProgSight_Scope;
    }
    view->m_linked = owner;
    view->onLinkChanged();
    gStageScene->pushView(view, static_cast<StageCharacter *>(owner)->isTakenOver() ? 2 : 1, 1);
    view->setZoom(ViewCamera::kZoomSpan * scopeZoom + ViewCamera::kZoomLow + 0.0f);
}

// level above the low end, over the span, moves the view's near half.
// FS2 0x0029a840
// callcheck: setProjection

void ViewCamera::setZoom(float level)
{
    constexpr float kScreenSize = 512.0f;
    float over = level - kZoomLow;
    if (over < 0.0f)
        over = 0.0f;
    else if (kZoomSpan < over)
        over = kZoomSpan;
    m_range2 = (over * (m_range - kViewNear)) / kZoomSpan;
    setProjection(m_range2 + kViewNear, kScreenSize, kScreenSize, m_range);
}

// A random line in a cone round from .. to, as wide at the target as the
// spread says. The angles: yaw and pitch of the line, a turn round it, and
// the cone's own, out to rand / 2047 of it.
// FS2 0x00269db0
// callcheck: atan2Octant, randNext, atan2Octant, identityMatrix, rotateMatrixZ, applyMatrix, atan2Octant, randNext, identityMatrix, rotateMatrixX, rotateMatrixY, rotateMatrixX, rotateMatrixZ, applyMatrix

void ShotSpread::scatter(float range, sceVu0FVECTOR out, const sceVu0FVECTOR from,
                         const sceVu0FVECTOR to) const
{
    constexpr float kRandToAngle = 2.9258362e-09f;      // 2 pi / 2^31
    constexpr float kRandEdge = 2047.0f;

    sceVu0FVECTOR way = { to[0] - from[0], to[1] - from[1], to[2] - from[2], 1.0f };
    const float length = sqrtf(way[0] * way[0] + way[1] * way[1] + way[2] * way[2]);
    const float wide = length / 10.0f * amount;
    const float cone = atan2Octant((wide + 0.0f) - (wide / 2.0f) * settled, length);
    const float around = (float)(int)randNext() * kRandToAngle - kPi;

    const float yaw = -atan2Octant(way[0], way[1]);
    sceVu0FMATRIX turn;
    identityMatrix(turn);
    rotateMatrixZ(-yaw, turn, turn);
    sceVu0FVECTOR level;
    applyMatrix(level, turn, way);
    const float pitch = -atan2Octant(-level[2], level[1]);

    const float edge = (float)((int)randNext() >> 20);
    identityMatrix(turn);
    rotateMatrixX((cone / kRandEdge) * edge, turn, turn);
    rotateMatrixY(around, turn, turn);
    rotateMatrixX(pitch, turn, turn);
    rotateMatrixZ(yaw, turn, turn);
    sceVu0FVECTOR line = { 0.0f, range, 0.0f, 0.0f };
    applyMatrix(line, turn, line);
    for (int i = 0; i < 4; i++)
        out[i] = line[i] + from[i];
}

// FS2 0x0026a390
// callcheck: getKind, canSpring

StageTrapItem *MotionProbe::findTrap()
{
    StageObjArray &items = gStageScene->objects->m_lists[StageObjGroup_3];
    for (StageObject **slot = items.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        if ((*slot)->m_active == 0 || (*slot)->getKind() != StageObject_TrapItem)
            continue;
        StageTrapItem *trap = static_cast<StageTrapItem *>(*slot);
        if (trap->canSpring(owner))
            return trap;
    }
    return nullptr;
}

// FS2 0x00274390
// callcheck: isFirearm, isControlled, find, find, sinCos, sinCos, segmentHitsAny (wallAhead)

void GunOnCondition::onUpdate()
{
    if (!holdsFirearm(character))
        return;
    if (character->isControlled() && wallAhead(character, kGunOnReach))
        return;
    character->m_allows.armed = 1;
}

// FS2 0x002741d0
// callcheck: isFirearm, pickGunMotion, request

void GunOnCondition::onApply()
{
    if (character->m_allows.armed == 0 || character->m_wants.ready == 0 || !holdsFirearm(character))
        return;
    motion->request(motion->pickGunMotion(GunSlot_On));
}

// FS2 0x0026d670

void GunMoveCondition::onUpdate()
{
    character->m_allows.armed = 1;
    if (character->m_abilities.gunMove != 0)
        character->m_allows.move = 1;
}

// FS2 0x0026d090
// callcheck: pickGunMotion, request, pickGunMotion, pickGunMotion, find, find, sinCos, sinCos, segmentHitsAny, pickGunMotion, request, pickGunMotion, request, pickGunMotion, request, pickGunMotion, request, pickGunMotion, request, pickGunMotion, request, pickGunMotion, request, pickGunMotion, request, pickGunMotion, request, pickGunMotion, request, pickGunMotion, request, getStageSelect (wallAhead; the switch has one request per case)

void GunMoveCondition::onApply()
{
    if (character->m_allows.armed == 0 || character->m_wants.ready == 0) {
        motion->request(motion->pickGunMotion(GunSlot_Off));
        return;
    }
    if (motion->motionId == motion->pickGunMotion(GunSlot_On))
        return;

    const bool scope = motion->scopeOn != 0;
    if (!scope && motion->motionId != motion->pickGunMotion(GunSlot_Aim)
        && wallAhead(character, kGunKeepReach)) {
        motion->request(motion->pickGunMotion(GunSlot_Off));
    }

    const CharacterAbilities &abilities = character->m_abilities;
    const bool target = character->m_290 != nullptr;
    const int kind = character->m_wants.moveKind;
    if (kind == CharacterMove_TurnLeft) {
        if (!target)
            motion->request(motion->pickGunMotion(GunSlot_TurnLeft));
        else if (abilities.strafe != 0)
            motion->request(motion->pickGunMotion(GunSlot_Left));
    }
    else if (kind == CharacterMove_TurnRight) {
        if (!target)
            motion->request(motion->pickGunMotion(GunSlot_TurnRight));
        else if (abilities.strafe != 0)
            motion->request(motion->pickGunMotion(GunSlot_Right));
    }

    if (character->m_allows.move != 0) {
        switch (kind) {
        case CharacterMove_StrafeLeft:
            if (!scope && abilities.strafe != 0)
                motion->request(motion->pickGunMotion(GunSlot_Left));
            break;
        case CharacterMove_StrafeRight:
            if (!scope && abilities.strafe != 0)
                motion->request(motion->pickGunMotion(GunSlot_Right));
            break;
        case CharacterMove_BackTurn:
            if (!scope && abilities.walkBack != 0)
                motion->request(motion->pickGunMotion(GunSlot_Back));
            break;
        case CharacterMove_Back:
            if (target && abilities.walkBack != 0)
                motion->request(motion->pickGunMotion(GunSlot_Back));
            break;
        case CharacterMove_WalkSlow:
            if (!scope)
                motion->request(motion->pickGunMotion(GunSlot_Walk));
            break;
        case CharacterMove_Walk:
        case CharacterMove_Run:
            if (target)
                motion->request(motion->pickGunMotion(GunSlot_Walk));
            break;
        default:
            break;
        }
    }

    if (motion->m_request == Motion_None)
        motion->request(motion->pickGunMotion(GunSlot_Aim));

    ModelPose *pose = character->m_model->pose;
    if (pose->lookBone == nullptr) {
        // A sheet of ctype 10 holds firearms left (giveItem).
        pose->lookBone = stageObjRecord(character->m_itemDef)->kind == kSheetKindTwoHands
            ? pose->leftTool : pose->rightTool;
    }
}

// FS2 0x0026d7f0
// callcheck: isControlled

void SnipeCondition::onUpdate()
{
    const bool rifle = isRifleItem(character->m_weapons[0]);
    if (!character->isControlled())
        return;
    if (rifle && weaponSheet(character->m_weapons[0]).param4 != 0 && motion->scopeOn == 0)
        character->m_allows.padUp = 1;
    if (motion->scopeOn != 0) {
        character->m_allows.rightStick = 1;
        character->m_allows.padDown = 1;
    }
}

// FS2 0x0026d6a0
// callcheck: setScope, find0, setZoom, setScope

void SnipeCondition::onApply()
{
    CharacterAllows &allows = character->m_allows;
    const CharacterWants &wants = character->m_wants;
    if (allows.armed == 0 || wants.ready == 0)
        return;
    if (allows.padUp != 0 && wants.padUp != 0)
        motion->setScope(true);
    if (allows.rightStick != 0 && wants.rightStickOn != 0) {
        float zoom = motion->scopeZoom - wants.rightStickY / 30.0f;
        if (zoom > 1.0f)
            zoom = 1.0f;
        if (zoom < 0.0f)
            zoom = 0.0f;
        motion->scopeZoom = zoom;
        static_cast<ViewCamera *>(gStageScene->objects->find0("camview_snipe", 0))
            ->setZoom(ViewCamera::kZoomSpan * zoom + ViewCamera::kZoomLow + 0.0f);
    }
    if (allows.padDown != 0 && wants.padDown != 0)
        motion->setScope(false);
}

// FS2 0x0026dee0
// callcheck: pickGunMotion

void GunAttackCondition::onUpdate()
{
    if (motion->m_ba8 > 0.0f)
        return;
    character->m_allows.armed = 1;
    if (motion->motionId != motion->pickGunMotion(GunSlot_On))
        character->m_allows.fire = 1;
}

// Aim settles while the trigger can pull; the shot leaves the gun, from the
// scope's eye through a scope, at the target or ahead, and a magazine that
// has run out only clicks. A wall between head and gun makes a blank.
// FS2 0x0026d950
// callcheck: isControlled, getDeltaSeconds, pickGunMotion, request, getWorldMatrix, playNamedHeard2, getWorldMatrix, find, getMatrix, segmentHitsAny, getLampMatrix, applyMatrix, getWorldMatrix, find0, getWorldMatrix, getWorldMatrix, applyMatrix, isControlled, find, getMatrix, getWorldMatrix, find, getMatrix, segmentHitsAny, getKind, scatter, fire, pickGunMotion, request

void GunAttackCondition::onApply()
{
    const CharacterWants &wants = character->m_wants;
    if (character->m_allows.armed == 0 || wants.ready == 0 || character->m_allows.fire == 0)
        return;

    ShotSpread &spread = motion->probe->spread;
    spread.amount = character->m_shotScatter;
    if (!character->isControlled())
        spread.rate = 1.0f;
    else
        spread.rate = 1.0f / (float)((int)(1.0f / character->m_clock->getDeltaSeconds()) * 3);
    if (spread.settled < 1.0f)
        spread.settled += spread.rate;
    else
        spread.settled = 1.0f;

    if (wants.fire != CharacterFire_Single && wants.fire != CharacterFire_Auto) {
        if ((wants.fire == CharacterFire_Tap || wants.fire == CharacterFire_Hold) && motion->scopeOn == 0) {
            motion->request(motion->pickGunMotion(GunSlot_Butt));
            character->m_model->pose->lookBone = nullptr;
        }
        return;
    }

    StageWeaponItem *weapon = static_cast<StageWeaponItem *>(character->m_weapons[0]);
    if (weapon->m_ammoA < 1) {
        character->playNamedHeard2("NO_BULLET", weapon->getWorldMatrix()[3], nullptr);
        return;
    }

    const float range = (float)weaponSheet(weapon).range;
    StageObject *target = character->m_290;
    sceVu0FVECTOR from, to;
    bool blocked = false;
    if (target == nullptr) {
        if (motion->scopeOn == 0) {
            memcpy(from, weapon->getWorldMatrix()[3], sizeof(from));
            blocked = gStageScene->segmentHitsAny(headPoint(character), from, 0, kFaceSolid, kFaceFlagsTrip, nullptr);
            sceVu0FMATRIX lamp;
            motion->getLampMatrix(lamp);
            sceVu0FVECTOR ahead = { 0.0f, range, 0.0f, 0.0f };
            applyMatrix(ahead, lamp, ahead);
            for (int i = 0; i < 4; i++)
                to[i] = ahead[i] + from[i];
        }
        else {
            StageObject *view = gStageScene->objects->find0("camview_snipe", 0);
            memcpy(from, view->getWorldMatrix()[3], sizeof(from));
            const sceVu0FVECTOR ahead = { 0.0f, range, 0.0f, 1.0f };
            applyMatrix(to, view->getWorldMatrix(), ahead);
        }
    }
    else {
        if (!character->isControlled()) {
            memcpy(from, headPoint(character), sizeof(from));
        }
        else {
            memcpy(from, weapon->getWorldMatrix()[3], sizeof(from));
            blocked = gStageScene->segmentHitsAny(headPoint(character), from, 0, kFaceSolid, kFaceFlagsTrip, nullptr);
        }
        memcpy(to, character->m_aimPoint, sizeof(to));
        if (target->getKind() == StageObject_Character && character->m_shotScatter != 0.0f)
            spread.scatter(range, to, from, to);
    }

    weapon->fire(character, from, to, blocked || character->m_blanks != 0);
    motion->request(motion->pickGunMotion(GunSlot_Fire));
}

// FS2 0x002704c0
// callcheck: isFirearm, strcmp

void ReloadCondition::onUpdate()
{
    if (character->m_abilities.reload == 0 || !holdsFirearm(character))
        return;
    const StageObject *weapon = character->m_weapons[0];
    if (weapon->m_ammoA != weapon->m_ammoB && strcmp(weapon->m_name, "R_MINIMI") != 0)
        character->m_allows.reload = 1;
}

// A player needs rounds to carry.
// FS2 0x002702e0
// callcheck: isPlayer, findCarried, strcmp, strcmp, strcmp, pickGunMotion, request, pickGunMotion, request

void ReloadCondition::onApply()
{
    const CharacterWants &wants = character->m_wants;
    if (character->m_allows.reload == 0)
        return;
    const bool idle = motion->motionId == kMotionIdle && motion->m_motionTime >= kReloadIdleSeconds;
    StageObject *weapon = character->m_weapons[0];
    if ((wants.reload == 0 && !idle) || weapon == nullptr)
        return;
    character->m_reloadRounds = wants.reload != 0 ? wants.reloadRounds : weapon->m_ammoB;
    if (character->isPlayer()) {
        const StageObject *rounds = character->findCarried(weaponSheet(weapon).ammo);
        if (rounds == nullptr || rounds->m_count == 0)
            return;
    }
    motion->request(motion->pickGunMotion(loadsByRound(weapon) ? GunSlot_ReloadBegin : GunSlot_ReloadAll));
}

// FS2 0x002706f0

void ShowtCondition::onUpdate()
{
    if (character->m_abilities.call != 0)
        character->m_allows.call = 1;
}

// FS2 0x00270690
// callcheck: pickStance, request

void ShowtCondition::onApply()
{
    if (character->m_allows.call != 0 && character->m_wants.call != 0)
        motion->request(motion->pickStance(MotionStance_Call));
}

// FS2 0x002701c0
// callcheck: isPlayer, isScenarioLead, isPlayer

void JackCondition::onUpdate()
{
    if (character->m_abilities.jack == 0)
        return;
    if (character->isPlayer() && character->isScenarioLead())
        return;
    if ((character->m_abilities.flag3c3 != 0 || character->m_290 == nullptr) && character->isPlayer())
        character->m_allows.altAction = 1;
}

// FS2 0x00270120
// callcheck: isRunning, pickStance, request, pickStance, request

void JackCondition::onApply()
{
    if (character->m_allows.altAction == 0 || character->m_wants.altAction == 0)
        return;
    motion->request(motion->pickStance(gStageScene->party->isRunning() ? MotionStance_Idle : MotionStance_Jack));
}

// FS2 0x00270780
// callcheck: find, find, sinCos, sinCos, segmentHitsAny

void ThrowCondition::onUpdate()
{
    if (character->m_abilities.throwing == 0)
        return;
    sceVu0FVECTOR from, to;
    memcpy(from, headPoint(character), sizeof(from));
    memcpy(to, from, sizeof(to));
    const float yaw = -character->m_xform.rotation[2];
    to[0] = sinCos(yaw, 0) * -kThrowReach + to[0] + 0.0f;
    to[1] = sinCos(yaw, 1) * -kThrowReach + to[1] + 0.0f;
    if (!gStageScene->segmentHitsAny(from, to, 0, kFaceSolid, kFaceFlagsTrip, nullptr))
        character->m_allows.throwItem = 1;
}

// FS2 0x00270710
// callcheck: setThrow, request

void ThrowCondition::onApply()
{
    if (character->m_allows.throwItem == 0 || character->m_wants.throwItem == 0)
        return;
    motion->throwLink->setThrow(character->m_throwItem, character->m_wants.throwVector);
    motion->request(kMotionThrow);
}

// FS2 0x00270980
// callcheck: findTrap

void TrapCancelCondition::onUpdate()
{
    if (character->m_abilities.traps == 0)
        return;
    if (character->m_abilities.flag3c3 == 0 && character->m_290 != nullptr)
        return;
    trap = motion->probe->findTrap();
    if (trap != nullptr)
        character->m_allows.trapCancel = 1;
}

// FS2 0x002708c0
// callcheck: request, getWorkPlace, FUN_001eac70, FUN_001eac10 (pointBehind, startWalkTo)

void TrapCancelCondition::onApply()
{
    if (character->m_allows.trapCancel == 0 || character->m_wants.trapCancel == 0)
        return;
    motion->request(kMotionTrapCancel);
    motion->trapCancelLink->trap = trap;
    sceVu0FVECTOR pos, rot, lift, stand;
    trap->getWorkPlace(pos, rot, lift, character);
    motion->pointBehind(stand, character->m_xform.position, rot);
    motion->startWalkTo(kTrapWalkRate, nullptr, stand, true, true);
}

// FS2 0x00270a70
// callcheck: findCarried, sinCos, sinCos, segmentHitsAny, raycast

void TrapSetCondition::onUpdate()
{
    if (character->m_abilities.traps == 0)
        return;
    if (character->m_abilities.flag3c3 == 0 && character->m_290 != nullptr)
        return;
    trap = static_cast<StageTrapItem *>(character->findCarried("I_WANA"));
    if (trap == nullptr)
        return;
    const float *feet = character->m_xform.position;
    sceVu0FVECTOR from = { feet[0], feet[1], feet[2] + kTrapChestHeight, feet[3] };
    sceVu0FVECTOR to = { feet[0], feet[1], feet[2] + kTrapChestHeight, feet[3] };
    const float yaw = -character->m_xform.rotation[2];
    to[0] = sinCos(yaw, 0) * -kTrapReach + to[0] + 0.0f;
    to[1] = sinCos(yaw, 1) * -kTrapReach + to[1] + 0.0f;
    if (gStageScene->segmentHitsAny(from, to, 0, kFaceWall, kFaceFlagsTrip, nullptr))
        return;
    memcpy(from, to, sizeof(from));
    to[2] -= kTrapFloorDrop;
    if (gStageScene->raycast(place, from, to, 0, kFaceFloor, kFaceFlagsTrip, nullptr) != nullptr
        && fabsf(feet[2] - place[2]) <= kTrapFloorStep) {
        character->m_allows.trapSet = 1;
    }
}

// FS2 0x00270a00
// callcheck: request

void TrapSetCondition::onApply()
{
    if (character->m_allows.trapSet == 0 || character->m_wants.trapSet == 0)
        return;
    motion->trapSetLink->trap = trap;
    motion->request(kMotionTrapSet);
    memcpy(motion->trapSetLink->place, place, sizeof(place));
}

// FS2 0x0027aa90
// callcheck: MotionConditionList, operator_new, find, add, find, add, find, add, find, add (addCondition)

GunLink::GunLink(StageCharacter *owner, PlayerMotion *motion_)
    : MotionLink(owner, motion_)
{
    addCondition("Lock");
    addCondition("Snipe");
    addCondition("GunAttack");
    addCondition("GunMove");
}

// FS2 0x0027aa70

void GunLink::onStart()
{
    motion->probe->spread.settled = 0.0f;
}

// FS2 0x0027a9e0
// callcheck: find, setScope (m_links.find)

void GunLink::onLeave(MotionLink *next)
{
    if (next != motion->m_links.find("GunAttack")->value)
        dropGun(character, motion);
}

// FS2 0x0027a910
// callcheck: MotionConditionList, operator_new, find, add, find, add (addCondition)

GunAttackLink::GunAttackLink(StageCharacter *owner, PlayerMotion *motion_)
    : MotionLink(owner, motion_)
{
    addCondition("Lock");
    addCondition("GunAttack");
}

// FS2 0x0027a8c0

void GunAttackLink::onRequest()
{
    const int fire = character->m_wants.fire;
    keepsLock = fire == CharacterFire_Auto;
    isShot = fire == CharacterFire_Auto || fire == CharacterFire_Single;
}

// FS2 0x0027a840
// callcheck: nodeAt, nodeAt (m_conditions[])

void GunAttackLink::onStart()
{
    if (keepsLock == 0)
        m_conditions[0]->active = 0;
    m_conditions[1]->active = 0;
    motion->probe->spread.settled = 0.0f;
}

// The butt lands like a blow; after a shot the next may come when the
// sheet's frames have passed: parameter 9, through the scope parameter 4.
// FS2 0x0027a650
// callcheck: findBlowPart, breakPart, nodeAt (m_conditions[])

void GunAttackLink::onRun()
{
    if (isShot == 0) {
        const int part = motion->probe->findBlowPart();
        if (part >= 0)
            character->m_290->breakPart(character->m_290->m_mapParts->ids[part], true);
        if (character->m_ground->bumped != 0 || part >= 0) {
            motion->m_ba8 = kBlowRecovery;
            finished = 1;
        }
    }
    else if (counter == 0) {
        const WeaponDef &sheet = weaponSheet(character->m_weapons[0]);
        const int frames = motion->scopeOn == 0 ? sheet.param9 : sheet.param4;
        if (((float)frames + 0.5f) / 30.0f <= motion->m_motionTime) {
            counter++;
            m_conditions[1]->active = 1;
        }
    }
}

// FS2 0x0027a580
// callcheck: nodeAt, nodeAt, find, setScope (m_conditions[], m_links.find)

void GunAttackLink::onLeave(MotionLink *next)
{
    if (keepsLock == 0)
        m_conditions[0]->active = 1;
    m_conditions[1]->active = 1;
    if (next != motion->m_links.find("Gun")->value)
        dropGun(character, motion);
}

// The click is a round's: all at once, or one by one until the gun is full
// or the rounds asked for are in. A player's come out of what it carries.
// FS2 0x002792c0
// callcheck: isEnding, getStageSelect, request, getStageSelect, request, request, request (stageObjRecord)

void DiveLandLink::onRun()
{
    if (!motion->isEnding())
        return;
    const bool alive = character->m_endurance != 0;
    if (motion->motionId == Motion_DiveLandFront) {
        motion->request(alive ? Motion_Revive : Motion_Dead);
    } else if (motion->motionId == Motion_DiveLandBack) {
        // Yrt has one way of lying.
        const bool sameSide = stageObjRecord(character->m_itemDef)->kind == CharacterKind_Yrt;
        if (alive)
            motion->request(sameSide ? Motion_Revive : Motion_ReviveBack);
        else
            motion->request(sameSide ? Motion_Dead : Motion_DeadBack);
    }
}

// The show flag a deadly fall turns off for its watcher. Only the script's
// property and 0x230400 read it.
static constexpr int kShowFlagNotFalling = 3;

bool DiveFallLink::isDeadly() const
{
    const int id = motion->motionId;
    return motion->m_bfc != 0
        && (id == Motion_LadderFall || id == Motion_DiveBack || id == Motion_DiveFront);
}

// FS2 0x002794e0
// callcheck: getDeltaSeconds, takeFallDamage, getDeltaSeconds, setPosition, lookAt, atan2Octant, atan2Octant, addAngle, sinf, cosf, approachVec3, lookAt

void DiveFallLink::stepCamera()
{
    constexpr float kFrame       = 0.033333335f;
    constexpr float kFrameDone   = 0.01f;
    constexpr float kLookFrom    = 30.0f;   // frames: the look eases over only at the end
    constexpr float kLookMaxStep = 25.0f;

    if (camera == nullptr)
        return;
    if (flyFrames == 0.0f) {
        if (deathFrames != 0.0f) {
            deathFrames -= gStageScene->clock->getDeltaSeconds() / kFrame;
            if (deathFrames < kFrameDone) {
                deathFrames = 0.0f;
                character->takeFallDamage(character->m_endurance);
            }
        }
        return;
    }
    flyFrames -= gStageScene->clock->getDeltaSeconds() / kFrame;
    if (flyFrames < kFrameDone) {
        flyFrames = 0.0f;
        camera->m_xform.setPosition(eye);
        camera->lookAt(target);
        return;
    }
    // Round the point looked at, in the ground plane, the rest of the way.
    const float fromX = camera->m_xform.position[0] - look[0];
    const float fromY = camera->m_xform.position[1] - look[1];
    const float toX = eye[0] - look[0];
    const float toY = eye[1] - look[1];
    const float fromLength = sqrtf(fromX * fromX + fromY * fromY);
    const float toLength = sqrtf(toX * toX + toY * toY);
    const float length = fromLength + (toLength - fromLength) / flyFrames;
    float angle = 0.0f;
    if (toLength != 0.0f) {
        angle = atan2Octant(fromX, fromY);
        angle -= addAngle(angle, -atan2Octant(toX, toY)) / flyFrames;
    }
    camera->m_xform.position[0] = length * sinf(angle) + look[0] + 0.0f;
    camera->m_xform.position[1] = length * cosf(angle) + look[1] + 0.0f;
    camera->m_xform.position[2] += (eye[2] - camera->m_xform.position[2]) / flyFrames;
    if (flyFrames < kLookFrom)
        approachVec3(1.0f / flyFrames, kLookMaxStep, look, look, target);
    camera->lookAt(look);
}

// FS2 0x00279780
// callcheck: isPlayer, getTopView, getTopView, isPartner, find0, faceAhead, vecNormalize, getSwayMatrix, getTopView, setPosition, lookAt, setPosition, lookAt, pushView, canDropWeapon, dropWeapon (the view's kind is its slot +0x1f4)

void DiveFallLink::startCamera()
{
    constexpr float kEyeBack    = 100.0f;
    constexpr float kEyeUp      = 100.0f;
    constexpr float kLookPast   = 60.0f;
    constexpr float kLookBelow  = -30.0f;
    constexpr float kFlightFrames = 60.0f;
    constexpr float kFallFrames = 90.0f;   // from the fall's start to its damage
    constexpr int   kViewPriorityDive = 1;

    bool flies = false;
    bool watched = false;
    if (character->isPlayer()) {
        StageObject *top = gStageScene->getTopView();
        if (top != nullptr && top->getKind() == StageObject_Kind9)
            flies = true;
        watched = true;
    } else if (character->isPartner()) {
        watched = true;
    }
    if (!watched) {
        StageObject *weapon = character->m_weapons[0];
        if (character->m_dropsWeapon != 0 && weapon != nullptr && character->canDropWeapon(weapon))
            character->dropWeapon(weapon, nullptr);
        return;
    }

    camera = static_cast<StageCamera *>(gStageScene->objects->find0("cam_dive", 0));
    if (camera == nullptr)
        return;
    sceVu0FVECTOR edge;
    if (motion->probe->faceAhead(edge, motion->motionId == Motion_DiveFront) == nullptr)
        memcpy(edge, motion->walkLink->wallPoint, sizeof(edge));
    sceVu0FVECTOR away = { edge[0] - character->m_xform.position[0], edge[1] - character->m_xform.position[1], 0.0f, 1.0f };
    vecNormalize(away, away);

    sceVu0FMATRIX sway;
    character->getSwayMatrix(sway);
    const sceVu0FVECTOR eyeStep = { away[0] * -kEyeBack, away[1] * -kEyeBack, kEyeUp, 1.0f };
    const sceVu0FVECTOR lookStep = { away[0] * kLookPast, away[1] * kLookPast, kLookBelow, 1.0f };
    for (int i = 0; i < 4; i++) {
        eye[i] = sway[3][i] + eyeStep[i];
        target[i] = edge[i] + lookStep[i];
    }

    if (flies) {
        flyFrames = kFlightFrames;
        StageMainCamera *top = static_cast<StageMainCamera *>(gStageScene->getTopView());
        camera->m_xform.setPosition(top->m_xform.position);
        memcpy(look, top->m_lookTarget, sizeof(look));
        camera->lookAt(look);
    } else {
        flyFrames = 0.0f;
        camera->m_xform.setPosition(eye);
        camera->lookAt(target);
    }
    deathFrames = kFallFrames - flyFrames;
    gStageScene->pushView(camera, kViewPriorityDive, 1);
}

// FS2 0x00279a60
// callcheck: addAngle, startCamera, isEnding, find, addAngle, sinCos, sinCos, addAngle, raycast, vecDistance, vecDirection, scaleVec4, setWallCheck, stepCamera, request, request, request

void DiveFallLink::onRun()
{
    constexpr float kTwistTurn  = kMathPi / 4.0f;
    constexpr float kTwistStep  = 0.017453292f;     // one degree: less is not worth it
    constexpr float kTwistAfter = 50.0f;            // units fallen
    constexpr float kLandAfter  = 25.0f;
    constexpr u_int kTwistWall  = 0xe8021f3;
    constexpr u_int kTwistSkip  = 0x2040000;

    StageCharacter *const body = character;
    float *const position = body->m_xform.position;
    if (counter == 0) {
        if (headClear == 0)
            twistTo = addAngle(body->m_xform.rotation[2], kTwistTurn);
        if (isDeadly())
            startCamera();
        startHeight = position[2];
        counter++;
    }
    if (counter == 1 && motion->isEnding()) {
        endStep = body->m_model->m_positionStep[2];
        counter++;
    }
    if (counter == 2)
        motion->m_moveStep[2] = endStep;

    const float fallen = startHeight - position[2];
    if (headClear == 0 && kTwistAfter < fallen) {
        BoneMap::Node *node = body->m_model->boneMap.find("bn_pelvis");
        if (node != nullptr && node->value != nullptr) {
            StageNodeBase *pelvis = node->value;
            // Half of what is left, round the pelvis.
            const float turn = addAngle(twistTo, -body->m_xform.rotation[2]) / 2.0f;
            if (kTwistStep < turn || turn < -kTwistStep) {
                const float dx = position[0] - pelvis->getMatrix()[3][0];
                const float dy = position[1] - pelvis->getMatrix()[3][1];
                const float s = sinCos(-turn, 0);
                const float c = sinCos(-turn, 1);
                position[0] = c * dx + s * dy + pelvis->getMatrix()[3][0];
                position[1] = -s * dx + c * dy + pelvis->getMatrix()[3][1];
                body->m_xform.rotation[2] = addAngle(body->m_xform.rotation[2], turn);
            }
            // A wall between the feet and the pelvis: out by as much.
            sceVu0FVECTOR hit;
            if (gStageScene->raycast(hit, position, pelvis->getMatrix()[3], 0, kTwistWall, kTwistSkip, nullptr) != nullptr) {
                const float depth = vecDistance(hit, pelvis->getMatrix()[3]);
                sceVu0FVECTOR direction, step;
                vecDirection(direction, position, pelvis->getMatrix()[3]);
                scaleVec4(depth, step, direction);
                for (int i = 0; i < 4; i++)
                    position[i] += step[i];
            }
        }
        motion->setWallCheck(1);
    }

    stepCamera();

    if (body->m_ground->isGrounded() && kLandAfter < fallen) {
        const int id = motion->motionId;
        if (id == Motion_DiveFront)
            motion->request(Motion_DiveLandFront);
        else if (id == Motion_DiveBack || id == Motion_LadderFall)
            motion->request(Motion_DiveLandBack);
        body->m_ground->groundCheck = 1;
    }
}

// FS2 0x00279ed0
// callcheck: isPlayer, isPartner

void DiveFallLink::onLeave(MotionLink *)
{
    if (isDeadly() && (character->isPlayer() || character->isPartner()))
        gStageScene->showFlags->flag[kShowFlagNotFalling] = 1;
}

// FS2 0x00279f90
// callcheck: find, isPlayer, isPartner

void DiveFallLink::onStart()
{
    const DamageLink *damage = static_cast<const DamageLink *>(motion->m_links.find("Damage")->value);
    if (damage->edgeBlocked != 0 && (character->isPlayer() || character->isPartner()))
        gStageScene->showFlags->flag[kShowFlagNotFalling] = 0;
}

// FS2 0x0027a020
// callcheck: removeView

void DiveFallLink::onCancel()
{
    motion->m_bfc = 0;
    gStageScene->removeView(camera);
    camera = nullptr;
}

// FS2 0x0027a060
// callcheck: find

void DiveFallLink::onRequest()
{
    const DamageLink *damage = static_cast<const DamageLink *>(motion->m_links.find("Damage")->value);
    motion->m_bfc = damage->edgeBlocked;
    headClear = damage->edgeClear;
}

// FS2 0x0027a0b0
// callcheck: MotionConditionList, operator_new

DiveFallLink::DiveFallLink(StageCharacter *owner, PlayerMotion *motion_)
    : MotionLink(owner, motion_)
{
}

// FS2 0x0027a140
// callcheck: pickGunMotion, playMagazineSetSound, isPlayer, findCarried, noteEnding, pickGunMotion, playMagazineOutSound, pickGunMotion, playReloadSetSound, isPlayer, findCarried, pickGunMotion, request, pickGunMotion, request, pickGunMotion, playMagazineSetSound, noteEnding

void ReloadLink::onRun()
{
    if ((character->m_model->m_motionEvents & MotionEvent_Click) == 0)
        return;
    StageObject *weapon = character->m_weapons[0];
    const char *ammo = weaponSheet(weapon).ammo;

    if (motion->motionId == motion->pickGunMotion(GunSlot_ReloadAll)) {
        character->playMagazineSetSound();
        int rounds = weapon->m_ammoB - weapon->m_ammoA;
        if (character->isPlayer()) {
            StageItem *carried = static_cast<StageItem *>(character->findCarried(ammo));
            if (carried->m_count < rounds)
                rounds = carried->m_count;
            carried->m_count = clampRounds(carried->m_count - rounds, carried->m_maxCount);
        }
        motion->noteEnding();
        weapon->m_ammoA = clampRounds(rounds + weapon->m_ammoA, weapon->m_ammoB);
    }
    else if (motion->motionId == motion->pickGunMotion(GunSlot_ReloadBegin)) {
        character->playMagazineOutSound();
    }
    else if (motion->motionId == motion->pickGunMotion(GunSlot_ReloadRound)) {
        if (weapon->m_ammoA < weapon->m_ammoB && character->m_reloadRounds > 0) {
            character->playReloadSetSound();
            StageItem *carried = nullptr;
            int taken = 1;
            if (character->isPlayer()) {
                carried = static_cast<StageItem *>(character->findCarried(ammo));
                taken = carried->m_count != 0 ? 1 : 0;
                carried->m_count = clampRounds(carried->m_count - 1, carried->m_maxCount);
            }
            weapon->m_ammoA = clampRounds(taken + weapon->m_ammoA, weapon->m_ammoB);
            int left = character->m_reloadRounds - 1;
            if (carried != nullptr && carried->m_count < left)
                left = carried->m_count;
            character->m_reloadRounds = left;
        }
        const bool more = weapon->m_ammoA < weapon->m_ammoB && character->m_reloadRounds > 0;
        motion->request(motion->pickGunMotion(more ? GunSlot_ReloadRound : GunSlot_ReloadEnd));
    }
    else if (motion->motionId == motion->pickGunMotion(GunSlot_ReloadEnd)) {
        character->playMagazineSetSound();
        motion->noteEnding();
    }
}

// FS2 0x00279210
// callcheck: dropItem, setPosition, addAngle, setRotation, arm, playTrapSetSound, noteEnding

void TrapSetLink::onRun()
{
    if ((character->m_model->m_motionEvents & MotionEvent_Click) == 0)
        return;
    character->dropItem(trap);
    trap->m_xform.setPosition(place);
    const float *turn = character->m_xform.rotation;
    trap->m_xform.setRotation(turn[0], turn[1], addAngle(turn[2], -kHalfPi));
    trap->arm();
    character->playTrapSetSound();
    motion->noteEnding();
}

// FS2 0x00278f70
// callcheck: startLinger

void TrapHitLink::onLeave(MotionLink *)
{
    // The ROM has no test; a script's own trp_s would have no trap here.
    if (motion->m_sprungTrap != nullptr)
        motion->m_sprungTrap->startLinger();
}

// FS2 0x00278f80
// callcheck: canDropWeapon, dropWeapon, randNext, request, request, getStageSelect (stageObjRecord)

void TrapHitLink::onRun()
{
    constexpr float kThirtieths = 30.0f;
    constexpr int   kChance     = 20;       // percent, added at most
    constexpr float kPercent    = 100.0f;
    constexpr float kShakeWait  = 0.15f;    // the shaken stick's m_bf0
    constexpr float kShakeTakes = 2.0f;     // seconds

    if (counter == 0) {
        StageObject *weapon = character->m_weapons[0];
        if (character->m_dropsWeapon != 0 && weapon != nullptr && character->canDropWeapon(weapon))
            character->dropWeapon(weapon, nullptr);
        heldFor = (float)stageObjRecord(character->m_itemDef)->trapResist / kThirtieths;
        heldFor += heldFor * (float)((int)randNext() % kChance) / kPercent;
        counter++;
        motion->m_bf0 = kShakeWait;
    }
    if (motion->m_bf0 <= 0.0f) {
        heldFor -= kShakeTakes;
        motion->m_bf0 = kShakeWait;
    }
    motion->request(motion->m_motionTime < heldFor ? Motion_TrapHit : Motion_TrapOut);
}

// FS2 0x00279190
// callcheck: disarm, playTrapOffSound, giveItem, playPickSound, noteEnding

void TrapCancelLink::onRun()
{
    if ((character->m_model->m_motionEvents & MotionEvent_Click) == 0)
        return;
    trap->disarm();
    character->playTrapOffSound();
    character->giveItem(trap);
    character->playPickSound(trap);
    motion->noteEnding();
}

// FS2 0x00278ed0
// callcheck: throwItem, noteEnding

void ThrowLink::onRun()
{
    if (counter == 0) {
        if (character->m_weapons[0] != nullptr)
            character->m_weapons[0]->m_model->data->shown = 0;
        counter++;
    }
    if ((character->m_model->m_motionEvents & MotionEvent_Click) != 0) {
        sceVu0FVECTOR push;
        memcpy(push, offset, sizeof(push));
        character->throwItem(item, push);
        motion->noteEnding();
    }
}

// FS2 0x00278ea0

void ThrowLink::onLeave(MotionLink *next)
{
    if (character->m_weapons[0] != nullptr)
        character->m_weapons[0]->m_model->data->shown = 1;
}

// FS2 0x00278e40

void ThrowLink::setThrow(StageThrownItem *item_, const sceVu0FVECTOR vector)
{
    item = item_;
    if (vector[2] + vector[0] + vector[1] == 0.0f) {
        offset[0] = 0.0f;
        offset[1] = kThrowDefaultY;
        offset[2] = kThrowDefaultZ;
        offset[3] = 0.0f;
    }
    else {
        memcpy(offset, vector, sizeof(offset));
        offset[3] = 0.0f;
    }
}

// FS2 0x0027b000
// callcheck: MotionConditionList, operator_new, find, add, find, add, find, add (addCondition)

BattleLink::BattleLink(StageCharacter *owner, PlayerMotion *motion_)
    : MotionLink(owner, motion_)
{
    addCondition("Lock");
    addCondition("BtAttack");
    addCondition("BtMove");
}

// Out of the stance the target goes, but into what keeps it.
// FS2 0x0027af70
// callcheck: find, find (m_links.find)

void BattleLink::onLeave(MotionLink *next)
{
    const char *keeps = character->m_abilities.flag3c3 == 0 ? "Walk" : "BtAttack";
    if (next != motion->m_links.find(keeps)->value)
        character->m_290 = nullptr;
}

// FS2 0x0027aea0
// callcheck: MotionConditionList, operator_new, find, add, find, add (addCondition)

BtAttackLink::BtAttackLink(StageCharacter *owner, PlayerMotion *motion_)
    : MotionLink(owner, motion_)
{
    addCondition("Lock");
    addCondition("BtAtkCombo");
}

// FS2 0x0027adf0
// callcheck: getKind, elevatorIsAtB1, elevatorIsAtB3, nodeAt, nodeAt (m_conditions[]; PC: the two read the pose of a floor object of kind 4, a class the host does not make, so Lock always stops)

void BtAttackLink::onStart()
{
    m_conditions[0]->active = 0;
    m_conditions[1]->active = 0;
}

// FS2 0x0027ac60
// callcheck: nodeAt, findBlowPart, StageObject::breakPart, isEnding, request, request, request (m_conditions[])

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
// callcheck: nodeAt, nodeAt, nodeAt, nodeAt, find (m_conditions[], m_links.find)

void BtAttackLink::onLeave(MotionLink *next)
{
    m_conditions[0]->active = 1;
    m_conditions[1]->active = 1;
    if (next != motion->m_links.find("Battle")->value)
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
// 0x210e50 is a jump here.
// FS2 0x00215f10
// FS2 0x00210e50
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
    if (isOnStageObjectKind(kind) && kind != StageObject_Car && target->m_mapParts != nullptr
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
    if (!isOnStageObjectKind(kind) || kind == StageObject_Car || target->m_mapParts == nullptr
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
    return m_brain == nullptr ? 1 : m_brain->getState();
}

// ───────────────────────────────────────
// The bite, and what a hit does to a character's status.

// Motions of a prey that cannot be bitten: the first group 0x3a..0x40 and
// 0xae, then 0x41, 0x42, then the rest of the ROM's chain.
static bool isBiteProofMotion(int id)
{
    constexpr int kFirstBlock = 0x3a;
    constexpr int kLastBlock  = 0x42;
    constexpr int kLateBlock  = 0xae;
    if ((id >= kFirstBlock && id <= kLastBlock) || id == kLateBlock)
        return true;
    return id == 0x95 || id == 0x94 || id == 0x39 || (id >= 0x2e && id <= 0x35);
}

// Head to head, nothing between.
static bool headsSeeEachOther(StageCharacter *from, StageCharacter *to)
{
    constexpr u_int kFaceSight     = 0xe002003;
    constexpr u_int kFaceSightSkip = 0x40000;
    const sceVu0FVECTOR *theirs = bodyBone(to, "bn_head")->getMatrix();
    const sceVu0FVECTOR *mine = bodyBone(from, "bn_head")->getMatrix();
    return !gStageScene->segmentHitsAny(mine[3], theirs[3], 0, kFaceSight, kFaceSightSkip, nullptr);
}

// Written inline in the headers: the empty slots of a link FS2 0x001eaba0,
// FS2 0x001eabb0, FS2 0x001ec3f0; the link ctors FS2 0x0027b640,
// FS2 0x0027c140, FS2 0x00278db0; the getters FS2 0x0021abb0, FS2 0x00215290.

// The prey stands in front, within reach, at its height, in sight, and
// nothing holds it.
// FS2 0x0026fd30
// callcheck: FUN_0021abb0, FUN_00215290, FUN_0020ce80, vecDistance, atan2Octant, addAngle, addAngle, find, find, segmentHitsAny

void BiteCondition::onUpdate()
{
    constexpr float kMinReach   = 85.0f;
    constexpr float kReachSlack = 10.0f;
    constexpr float kMaxHeight  = 50.0f;
    constexpr float kFrontArc   = 1.0471976f;

    if (character->m_abilities.flag3be == 0)
        return;
    StageObject *candidate = character->getBiteTarget();
    if (candidate == nullptr || candidate->getKind() != StageObject_Character)
        return;
    StageCharacter *prey = static_cast<StageCharacter *>(candidate);
    if (prey->m_biteProof != 0 || prey->getBrainPace() == 0)
        return;
    if (prey->m_lookAt != nullptr || prey->hasGear() || prey->m_abilities.flag3bf == 0 || prey->m_endurance <= 0)
        return;
    if (isBiteProofMotion(prey->m_motion->motionId))
        return;

    float reach = character->getAttackReach() - kReachSlack;
    if (reach < kMinReach)
        reach = kMinReach;
    const float *mine = character->m_xform.position;
    const float *theirs = prey->m_xform.position;
    if (vecDistance(mine, theirs) > reach + kReachSlack || fabsf(mine[2] - theirs[2]) > kMaxHeight)
        return;
    const float bearing = atan2Octant(mine[0] - theirs[0], -(mine[1] - theirs[1]));
    const float off = addAngle(addAngle(bearing, -character->m_xform.rotation[2]), -kMathPi);
    if (fabsf(off) >= kFrontArc)
        return;
    if (headsSeeEachOther(character, prey)) {
        character->m_allows.bite = 1;
        character->m_biteTarget = prey;
    }
}

// FS2 0x0026fb60
// callcheck: FUN_0026fbc0

void BiteCondition::onApply()
{
    if (character->m_allows.bite == 0 || character->m_wants.bite == 0)
        return;
    StageCharacter *prey = character->m_biteTarget;
    if (prey->m_lookAt == nullptr)
        motion->startBite(prey);
}

// The biter comes round to face the prey; the prey's motion is 0xa0 when the
// biter stands at its back, 0xa1 in front, and it turns to match.
// FS2 0x0026fbc0
// callcheck: request, atan2Octant, addAngle, requestedFile, startWalkFile, addAngle, addAngle, requestNow, requestNow, requestedFile, startWalkFile

void PlayerMotion::startBite(StageCharacter *prey)
{
    constexpr int   kMotionBite         = 0x9e;
    constexpr int   kMotionBittenBehind = 0xa0;
    constexpr int   kMotionBittenFront  = 0xa1;
    constexpr float kBiteWalkRate       = 0.35f;
    constexpr float kRightAngle         = 1.5707964f;

    const float *mine = owner->m_xform.position;
    const float *theirs = prey->m_xform.position;
    const float *turn = prey->m_xform.rotation;

    request(kMotionBite);
    float face = atan2Octant(mine[0] - theirs[0], -(mine[1] - theirs[1]));
    face = addAngle(face, -kMathPi);
    sceVu0FVECTOR angles = { turn[0], turn[1], face, turn[3] };
    startWalkFile(kBiteWalkRate, requestedFile(), theirs, angles, false, true);

    PlayerMotion *bitten = prey->m_motion;
    angles[2] = turn[2];
    if (fabsf(addAngle(face, -turn[2])) >= kRightAngle) {
        const float behind = addAngle(face, -kMathPi);
        bitten->requestNow(kMotionBittenBehind);
        angles[2] = behind;
    }
    else {
        bitten->requestNow(kMotionBittenFront);
        angles[2] = face;
    }
    bitten->startWalkFile(kBiteWalkRate, bitten->requestedFile(), theirs, angles, false, true);
}

// ──────────────────────────────────────────────────────────────────────
// Hit, down and up again: the five links of 0x277230..0x278460.

constexpr u_int kWallRayFallen      = 0x200000;  // CollisionBody::rayMask: what a falling body's wall ray meets too
constexpr float kBodyOnFloorSeconds = 2.0f;

// dm_car_f, dm_car_b.
static bool isCarHitMotion(int id)
{
    return id == Motion_CarHitFront || id == Motion_CarHitBack;
}

// What 0x277250 and 0x277670 open with, inline: kinds 0, 1, 9, 0xa lie on the
// floor by the clock, the others on a click of their motion.
static bool isBodyOnFloor(StageCharacter *character, const PlayerMotion *motion)
{
    const int kind = stageObjRecord(character->m_itemDef)->kind;
    if (kind == CharacterKind_A || kind == CharacterKind_9 || kind == CharacterKind_1 || kind == CharacterKind_0)
        return kBodyOnFloorSeconds < motion->m_motionTime;
    return (character->m_model->m_motionEvents & MotionEvent_Click) != 0;
}

// Kinds 5, 6, 0xc end dm_car_f and dm_car_b in the plain dm_rev and dm_ded.
static bool fallsStraightDown(StageCharacter *character)
{
    const int kind = stageObjRecord(character->m_itemDef)->kind;
    return kind == CharacterKind_C || kind == CharacterKind_6 || kind == CharacterKind_5;
}

// FS2 0x00277230

void ReviveLink::onLeave(MotionLink *)
{
    if (motion->m_kindFlags7 != 0)
        motion->m_b54 = 0;
}

// FS2 0x00277250
// callcheck: getStageSelect (stageObjRecord)

void ReviveLink::onRun()
{
    if (isBodyOnFloor(character, motion) && motion->m_kindFlags7 != 0)
        motion->m_b54 = 0;
}

// FS2 0x002773b0

void DamageDeadLink::onLeave(MotionLink *)
{
    if (character->m_model->m_loop != 0)
        character->m_model->m_loop = 0;
}

// FS2 0x002773d0
// callcheck: getDeltaSeconds, request, request, initStatus, isPlayer, isPartner, getStageSelect (stageObjRecord)

void DamageDeadLink::onRun()
{
    constexpr float kPoseFrozenAfter = 2.0f;
    if (counter == 0) {
        reviveFrames = (int)(character->m_reviveTime * (float)(int)(1.0f / character->m_clock->getDeltaSeconds()));
        counter++;
    }
    if (counter == 1) {
        if (0 < reviveFrames)
            reviveFrames--;
        if (reviveFrames < 0) {
            if (motion->motionId == Motion_Dead)
                motion->request(Motion_Revive);
            else if (motion->motionId == Motion_DeadBack)
                motion->request(Motion_ReviveBack);
            character->initStatus();
        }
    }
    if (!character->isPlayer() && !character->isPartner()
        && stageObjRecord(character->m_itemDef)->kind != CharacterKind_Yrt
        && kPoseFrozenAfter < motion->m_motionTime)
        character->m_model->m_loop = 1;
}

// FS2 0x002775c0

void DamageDeadLink::onStart()
{
}

// FS2 0x002775d0
// callcheck: MotionConditionList, operator_new

DamageDeadLink::DamageDeadLink(StageCharacter *owner, PlayerMotion *motion_)
    : MotionLink(owner, motion_)
{
}

// FS2 0x00277660

void DamageFatalLink::onLeave(MotionLink *)
{
}

// FS2 0x00277670
// callcheck: getStageSelect, isEnding, getStageSelect, request, request, request, isEnding, getStageSelect, request, getStageSelect, canDropWeapon, dropWeapon, getStageSelect, find, spawn, getCharacterClass, find, spawn

void DamageFatalLink::onRun()
{
    constexpr float kYrtBodyGoesAfter = 1.2f;
    constexpr u_int kClassEnemy = 8;
    if (isBodyOnFloor(character, motion) && motion->m_kindFlags7 != 0)
        motion->m_b54 = 1;
    if (isCarHitMotion(motion->motionId)) {
        if (0 < character->m_endurance) {
            if (!motion->isEnding())
                return;
            if (fallsStraightDown(character))
                motion->request(Motion_Revive);
            else if (motion->motionId == Motion_CarHitFront)
                motion->request(Motion_ReviveBack);
            else if (motion->motionId == Motion_CarHitBack)
                motion->request(Motion_Revive);
            return;
        }
        if (motion->isEnding() && fallsStraightDown(character))
            motion->request(Motion_Dead);
    }
    if (counter == 1) {
        if (stageObjRecord(character->m_itemDef)->kind == CharacterKind_Yrt && kYrtBodyGoesAfter < motion->m_motionTime) {
            if (CollisionBody *body = character->m_ground->pushBody)     // PC: the ROM writes through a null
                body->on = 0;
            counter++;
        }
    }
    else if (counter == 0) {
        StageObject *weapon = character->m_weapons[0];
        if (character->m_dropsWeapon != 0 && weapon != nullptr && character->canDropWeapon(weapon))
            character->dropWeapon(weapon, nullptr);
        if (stageObjRecord(character->m_itemDef)->kind == CharacterKind_Srt) {
            BoneMap::Node *pelvis = character->m_model->boneMap.find("bn_pelvis");
            gStageScene->nodeTree->spawn("shiryodead", pelvis->value, nullptr, nullptr, 0, 0);
        }
        else if ((character->getCharacterClass() & kClassEnemy) != 0) {
            BoneMap::Node *pelvis = character->m_model->boneMap.find("bn_pelvis");
            gStageScene->nodeTree->spawn("yamidead", pelvis->value, nullptr, nullptr, 0, 0);
        }
        counter++;
    }
}

// FS2 0x00277cc0

void DamageFatalLink::onCancel()
{
    if (CollisionBody *body = character->m_ground->wallBody)             // PC: the ROM writes through a null
        body->rayMask &= ~kWallRayFallen;
}

// FS2 0x00277cf0

void DamageFatalLink::onRequest()
{
    if (isCarHitMotion(motion->motionId) && 0 < character->m_endurance)
        return;
    if (CollisionBody *body = character->m_ground->wallBody)             // PC: the ROM writes through a null
        body->rayMask |= kWallRayFallen;
}

// FS2 0x00277d60

void DamageLandLink::onRun()
{
    if (motion->isEnding())
        motion->request(character->m_endurance == 0 ? Motion_Dead : Motion_Revive);
}

// The head's reach past the edge, a car's floor and what stands on it: the
// middle of 0x277dd0.

void DamageLink::findDive(bool forward)
{
    constexpr float kEdgeRayHeight   = 50.0f;
    constexpr float kHeadMargin      = 20.0f;
    constexpr float kHeadDefault     = 170.0f;
    constexpr float kDiveWalkRate    = 0.35f;
    constexpr u_int kCarFloorSkip    = 0x40140;
    constexpr u_int kPosePlayRootOff = 0x40;

    sceVu0FVECTOR wallPoint, target, point, angles;
    float drop = 0.0f;
    StageObject *floor = nullptr;
    StageCollisionFace *face = motion->probe->faceAhead(wallPoint, forward);
    const bool found = motion->probe->findEdge(face, nullptr, target, &drop, &floor, point, angles, &edgeBlocked);
    if (!(found && drop < 0.0f) && edgeBlocked == 0)
        return;
    edgeFound = 1;
    if (edgeBlocked != 0) {
        edgeClear = 1;
    }
    else {
        sceVu0FVECTOR from, to;
        memcpy(from, target, sizeof(from));
        from[2] += kEdgeRayHeight;
        memcpy(to, from, sizeof(to));
        const float away = addAngle(angles[2], kMathPi);
        float reach = kHeadDefault;
        BoneMap::Node *head = character->m_model->boneMap.find("bn_head");
        if (head != nullptr && head->value != nullptr)
            reach = head->value->getMatrix()[3][2] - character->m_xform.position[2] + kHeadMargin;
        to[0] += reach * sinCos(-away, 0);
        to[1] += reach * sinCos(-away, 1);
        edgeClear = !gStageScene->segmentHitsAny(from, to, 0, kFaceWall, 0, nullptr);

        StageObject *standsOn = character->m_ground->floorObject;
        const bool onCar = standsOn != nullptr && standsOn->getKind() == StageObject_Car;
        const bool edgeOnCar = floor != nullptr && floor->getKind() == StageObject_Car;
        if (edgeClear != 0) {
            if (onCar && !edgeOnCar) {
                memcpy(from, point, sizeof(from));
                from[2] += kEdgeRayHeight;
                to[2] = from[2];
                if (gStageScene->segmentHitsAny(from, to, 0, kFaceWall, kCarFloorSkip, nullptr))
                    edgeFound = 0;
            }
        }
        else if (onCar) {
            if (edgeOnCar)
                edgeClear = 1;
            else
                edgeFound = 0;
        }
    }
    if (edgeFound == 0)
        return;
    ModelMotion *dive = character->m_model->findMotion(0, motion->lowerLabel(forward ? Motion_DiveFront : Motion_DiveBack));
    if (dive == nullptr) {
        edgeFound = 0;
        return;
    }
    if (!forward)
        angles[2] = addAngle(angles[2], kMathPi);
    motion->startWalkFile(kDiveWalkRate, dive->file, point, angles, true, true);
    character->m_model->pose->flags &= ~kPosePlayRootOff;
}

// FS2 0x00277dd0
// callcheck: randNext, randNext, randNext, randNext, canDropWeapon, dropWeapon, isEnding, request, request, setWallCheck, faceAhead, findEdge, addAngle, find, sinCos, sinCos, segmentHitsAny, segmentHitsAny, lowerLabel, findMotion, addAngle, startWalkFile (findDive)

void DamageLink::onRun()
{
    constexpr int   kDropSpreadSide = 50;
    constexpr int   kDropSpreadUp   = 80;
    constexpr float kDropTenths     = 10.0f;
    constexpr float kDropAway       = 15.0f;
    constexpr float kDiveWithin     = 25.0f;   // squared
    constexpr float kDiveSeconds    = 1.0f;
    constexpr int   kRepelInDive    = 2;

    const int id = motion->motionId;
    const bool forward = id != Motion_DamageFront && id != Motion_DamageFrontHeavy && id != Motion_DamageHitFront; // thrown back by the others
    if (counter == 0) {
        edgeFound = 0;
        edgeBlocked = 0;
        edgeClear = 0;
        if (character->m_dropsWeapon != 0 && character->m_armLost != 0) {
            if (StageObject *weapon = character->m_weapons[0]) {
                sceVu0FVECTOR offset;
                offset[0] = (float)((int)randNext() % kDropSpreadSide) / kDropTenths;
                offset[1] = forward ? -kDropAway : kDropAway;
                offset[2] = (float)((int)randNext() % kDropSpreadUp) / kDropTenths;
                offset[3] = 0.0f;
                if (character->canDropWeapon(weapon))
                    character->dropWeapon(weapon, offset);
            }
            character->m_armLost = 0;
        }
        counter++;
    }
    if (character->m_abilities.divesOverEdges == 0)
        return;
    if (edgeFound != 0 && (!(motion->m_walking != 0 && motion->m_walkHeld != 0) || motion->isEnding())) {
        const float *walkTo = character->m_model->pose->walkTarget;
        const float x = character->m_xform.position[0] - walkTo[0];
        const float y = character->m_xform.position[1] - walkTo[1];
        if (kDiveWithin < x * x + y * y)
            edgeFound = 0;
        else
            motion->request(forward ? Motion_DiveFront : Motion_DiveBack);
    }
    if (edgeFound == 0 && motion->m_motionTime < kDiveSeconds)
        findDive(forward);
    if (edgeFound != 0) {
        motion->setWallCheck(0);
        character->m_ground->repelLevel = kRepelInDive;
    }
}

// FS2 0x00278d90

void BiteLink::onRequest()
{
    target = character->m_biteTarget;
    target->m_lookAt = character;
}

// FS2 0x00278d70

void BiteLink::onCancel()
{
    if (target != nullptr) {
        target->m_lookAt = nullptr;
        target = nullptr;
    }
}

// How far a character's pose may stray from where the walk put it.
static bool strayedFromWalk(StageCharacter *who)
{
    constexpr float kStrayFloor = 10.0f;
    constexpr float kStrayShare = 3.0f;
    const float *position = who->m_xform.position;
    const float *place = who->m_model->pose->walkTarget;
    float limit = who->m_ground->nearRadius / kStrayShare;
    if (limit < kStrayFloor)
        limit = kStrayFloor;
    const float dx = position[0] - place[0];
    const float dy = position[1] - place[1];
    return limit * limit < dy * dy + dx * dx;
}

bool BiteLink::isBroken()
{
    if (character->isMotionSlowed() || (!target->isStageDrained() && target->isMotionSlowed()))
        return true;
    if (!headsSeeEachOther(character, target))
        return true;
    const bool ownKindHolds = stageObjRecord(character->m_itemDef)->kind == CharacterKind_Srt;
    if (!ownKindHolds && !character->m_motion->isWalkBusy() && strayedFromWalk(character))
        return true;
    return !target->m_motion->isWalkBusy() && strayedFromWalk(target);
}

// On each click of the biter the prey loses endurance, shared out over the
// presses it has made; a brain-driven prey may slip free by chance, the
// player after enough presses. Kind 3 ends by itself.
// FS2 0x00278640
// callcheck: getStageSelect, isMotionSlowed, getStageSelect, isControlled, FUN_0020eb40, vecDirection, find, FUN_0026a010, playNamedHeard2, playerEvent, FUN_0020eb40, isStageDrained, isMotionSlowed, find, find, segmentHitsAny, getStageSelect, isWalkBusy, isWalkBusy, requestNow, isControlled, randNext

void BiteLink::onRun()
{
    constexpr int   kMotionBittenBehind = 0xa0;
    constexpr int   kMotionBittenFront  = 0xa1;
    constexpr int   kMotionHeldA        = 0xa2;
    constexpr int   kMotionHeldB        = 0xa3;
    constexpr int   kMotionBiteEnd      = 0xa4;
    constexpr int   kHitBite            = 7;
    constexpr int   kHitPartBody        = 1;
    constexpr float kPressWait          = 0.15f;
    constexpr float kBloodSize          = 40.0f;
    constexpr int   kBloodCount         = 4;
    constexpr int   kPlayerPresses      = 4;

    const bool selfEnds = stageObjRecord(character->m_itemDef)->kind == CharacterKind_Srt;
    PlayerMotion *bitten = target->m_motion;
    if (counter == 0) {
        hits = 0;
        bitten->m_bf0 = kPressWait;
        counter++;
    }
    else {
        const int id = bitten->motionId;
        if (id == kMotionHeldB || id == kMotionHeldA) {
            if ((target->m_model->m_motionEvents & MotionEvent_Click) == 0)
                return;
        }
        if (id != kMotionBittenFront && id != kMotionBittenBehind) {
            if (selfEnds)
                character->takeDamage(character, kHitBite, kHitPartBody, character->m_endurance);
            finished = 1;
            return;
        }
    }

    if (isBroken()) {
        if (selfEnds)
            finished = 1;
        else
            motion->requestNow(kMotionBiteEnd);
        return;
    }

    if (target->isControlled() && bitten->m_bf0 <= 0.0f) {
        hits++;
        bitten->m_bf0 = kPressWait;
    }
    if ((character->m_model->m_motionEvents & MotionEvent_Click) == 0)
        return;

    hits++;
    int damage = character->m_biteDamage;
    if (hits > 1)
        damage /= hits + 1;
    if (damage == 0)
        damage = 1;
    target->takeDamage(character, kHitBite, kHitPartBody, damage);
    sceVu0FVECTOR direction;
    vecDirection(direction, target->m_xform.position, character->m_xform.position);
    bitten->probe->spawnBlood(kBloodSize, bodyBone(target, "bn_neck"), direction, kBloodCount, nullptr);
    target->playNamedHeard2("BT_BITE", target->m_xform.position, nullptr);
    target->playerEvent("BITE");

    bool freed;
    if (target->m_biteProof != 0 || target->getBrainPace() == 0) {
        freed = true;
    }
    else if (target->isControlled()) {
        freed = hits > kPlayerPresses;
    }
    else {
        const int roll = (int)(randNext() % 100);
        switch (hits) {
        case 1:  freed = roll < 20; break;
        case 2:  freed = roll < 30; break;
        case 3:  freed = roll < 50; break;
        case 4:  freed = roll < 70; break;
        default: freed = true;      break;
        }
    }
    if (freed)
        bitten->activeLink->finished = 1;
}

// The prey's motion ends and its bare hands come back.
// FS2 0x00278460
// callcheck: requestNow, requestNow, getStageSelect, getStageSelect

void BiteLink::onLeave(MotionLink *)
{
    constexpr int kMotionBittenBehind = 0xa0;
    constexpr int kMotionBittenFront  = 0xa1;
    constexpr int kMotionFreedBehind  = 0xa4;
    constexpr int kMotionFreedFront   = 0xa6;

    if (target == nullptr)      // PC: onCancel came first; the ROM reads through the null
        return;
    PlayerMotion *bitten = target->m_motion;
    if (bitten->motionId == kMotionBittenFront)
        bitten->requestNow(kMotionFreedFront);
    else if (bitten->motionId == kMotionBittenBehind)
        bitten->requestNow(kMotionFreedBehind);
    int value = stageObjRecord(target->m_itemDef)->unarmedMsp;
    if (value < 0)
        value = 0;
    target->m_unarmedMsp = value;
}

// The long weapon's parameter 2, else the sheet's 攻撃判定.
// FS2 0x0020ce80
// callcheck: getStageSelect

float StageCharacter::getAttackReach()
{
    if (m_weapons[0] == nullptr)
        return (float)stageObjRecord(m_itemDef)->attackCheck;
    return (float)weaponSheet(m_weapons[0]).param2;
}

// 2 at the wheel, 9 bare hands; by the kind of the long weapon 3 the firearms
// and the blades, 2 the clubs, 1 the rest.
// FS2 0x00210540

int StageCharacter::getHandClass()
{
    constexpr int kHandWheel = 2;
    constexpr int kHandBare  = 9;
    constexpr int kHandShort = 3;
    constexpr int kHandClub  = 2;
    constexpr int kHandOther = 1;
    constexpr int kItemKindClub = 1;
    constexpr int kItemKindShortFirst = 2;
    constexpr int kItemKindShortLast  = 6;
    if (m_gear != nullptr && m_abilities.driving == 1)
        return kHandWheel;
    if (m_weapons[0] == nullptr)
        return kHandBare;
    const int index = m_weapons[0]->m_itemDef;
    const int kind = gItemDefs[index < 0 || index >= kItemDefCount ? 0 : index].kind;
    if (kind >= kItemKindShortFirst && kind <= kItemKindShortLast)
        return kHandShort;
    return kind == kItemKindClub ? kHandClub : kHandOther;
}

// The sheet's ctype of who hits, as a blow's weight; only "none" is read.
static int blowWeightOf(int sheetKind)
{
    switch (sheetKind) {
    case 2:
    case 3:
    case 8:
    case 10:
    case 13:
        return 4;
    case 4:
    case 5:
    case 6:
    case 7:
    case 11:
    case 12:
        return 8;
    default:
        return 0;
    }
}

// PC: a null weapon is the first row; the ROM reads +0x450 of the zero page.
static const WeaponDef *weaponDefOf(StageObject *weapon)
{
    return weapon != nullptr ? &weaponSheet(weapon) : &gWeaponDefs[0];
}

static int clampToSheet(int value, int limit);
static void stopFleeing(StageCharacter *character);

// The part a blow lands on, the flinch it gives, then the endurance. What the
// lead does to an ally counts a quarter.
// FS2 0x0020eb40
// callcheck: getStageSelect, getBrainPace, getStageSelect, isAlly, setEndurance, getSaveData, getSeconds, isPlayer, isScenarioLead, isRunning, stop, getStageResult, randUnit, playVoice (the inline stage kind reads are stageObjRecord; setRightArm, setFlinch inline)

int StageCharacter::takeDamage(StageObject *fromObject, int kind, int part, int amount)
{
    constexpr int kHitKind4 = 4;
    constexpr int kHitBare = 9;
    constexpr int kMenuRequestClose = 2;
    constexpr int kHitClassCount = 10;
    constexpr int kPartHead = 0;
    constexpr int kPartBody = 1;
    constexpr int kPartLeftArm = 2;
    constexpr int kPartRightArm = 3;
    constexpr int kPartRightLeg = 4;
    constexpr int kPartLeftLeg = 5;
    constexpr int kPartCount = 6;
    constexpr u_int kSheetLeadLast = 1;     // the ctypes above it are the enemies
    constexpr int kBrainPaceAsleep = 5;
    constexpr int kMotionTriple = 0x9b;
    constexpr int kSheetTough1 = 3;
    constexpr int kSheetTough2 = 7;
    constexpr int kHeadScale = 3;
    constexpr int kAllyDivisor = 4;
    constexpr int kPlayerHitMark = 10;
    constexpr int kSheetSeven = 0xb;
    constexpr int kSevenVoiceChance = 4;
    constexpr int kResultHitsTaken = 2;
    static const char kSevenVoice[] = "SEVE1_SP04"; /* data 0x306a08 */

    StageCharacter *from = static_cast<StageCharacter *>(fromObject);
    StageScene *scene = gStageScene;
    StageObject *const onPad = scene->controlled != nullptr
        ? scene->controlled : static_cast<StageObject *>(scene->player);
    const StageObjRecord *sheet = stageObjRecord(m_itemDef);
    const int sheetKind = sheet->kind;
    const bool lead = static_cast<StageObject *>(scene->player) == this || scene->partner == this;
    const bool byLead = from != nullptr
        && (static_cast<StageObject *>(scene->player) == from || scene->partner == from);

    amount = std::max(amount, 0);
    // PC: the ROM has no test of the menu: a sub-scene has none.
    if (onPad == this && scene->menu != nullptr && scene->menu->state != StageMenu_Closed)
        scene->menu->request = kMenuRequestClose;
    if ((u_int)sheetKind > kSheetLeadLast && getBrainPace() != kBrainPaceAsleep) {
        if (from != nullptr && kind != kHitKind4)
            part = kPartBody;
        amount *= 3;
    }
    if (m_motion->motionId == kMotionTriple)
        amount *= 3;

    int lost = amount;
    if (sheetKind != kSheetTough1 && sheetKind != kSheetTough2) {
        if ((u_int)part >= kPartCount) {
            lost = 0;
        } else if (part == kPartHead) {
            if (!lead || from == nullptr || blowWeightOf(stageObjRecord(from->m_itemDef)->kind) == 0)
                lost = amount * kHeadScale;
        } else if (part != kPartBody) {
            lost = amount / 2;
        }
    }
    if (byLead && isAlly(from)) {
        amount /= kAllyDivisor;
        lost /= kAllyDivisor;
    }

    switch (part) {
    case kPartLeftArm:
        m_leftArm = clampToSheet(m_leftArm - amount, sheet->leftArm);
        break;
    case kPartRightArm:
        setRightArm(m_rightArm - amount);
        break;
    case kPartRightLeg:
        m_rightLeg = clampToSheet(m_rightLeg - amount, sheet->rightLeg);
        break;
    case kPartLeftLeg:
        m_leftLeg = clampToSheet(m_leftLeg - amount, sheet->leftLeg);
        break;
    default:
        break;
    }

    if (from != nullptr) {
        const StageObjRecord *fromSheet = stageObjRecord(from->m_itemDef);
        if (from->m_gear != nullptr && from->m_abilities.driving == 1) {
            setFlinch(sheet->flinchResist);
        } else if (kind == kHitBare) {
            setFlinch(m_flinch + from->m_unarmedMsp);
            from->m_unarmedMsp = clampToSheet(from->m_unarmedMsp - fromSheet->unarmedMspDrop, fromSheet->unarmedMsp);
        } else if (kind == kHitKind4) {
            setFlinch(part == kPartHead ? sheet->flinchResist
                                        : m_flinch + weaponDefOf(from->m_weapons[0])->ammoParam12);
        } else if (kind >= 1 && kind <= 3) {
            setFlinch(fromSheet->weaponMspScale + m_flinch + weaponDefOf(from->m_weapons[0])->param12);
        }
    }

    m_lastHitBy = from;
    m_hitThisFrameBy = from;
    setEndurance(m_endurance - lost);
    if (kind >= 0 && kind < kHitClassCount)
        m_62c = kind;
    if (from != nullptr) {
        if (static_cast<StageObject *>(scene->player) == from)
            m_68c = kPlayerHitMark;
        else if (onPad == from && this != from)
            m_foe = from;
    }

    if (onPad == this && static_cast<StageObject *>(scene->player) != onPad) {
        scene->altLatch = 1;
    } else {
        const int stage = getStageSelect()->stage;
        bool endsSlow = false;
        if (stage == 0x1b || stage == 0x19)
            endsSlow = sheetKind == 1;
        else if (stage == 0x17 || stage == 0xf)
            endsSlow = sheetKind == 0;
        if (endsSlow && m_slowStart >= 0.0f
            && m_clock->getSeconds() - m_slowStart < m_slowEnd - m_slowStart)
            m_slowStart = -1.0f;
    }

    stopFleeing(this);
    if (static_cast<StageObject *>(scene->player) == this)
        getStageResult()->counter[kResultHitsTaken]++;
    if (from != nullptr && stageObjRecord(from->m_itemDef)->kind == kSheetSeven && lead
        && (int)(randUnit() * kSevenVoiceChance) == 0)
        from->playVoice(kSevenVoice);
    return lost;
}

// FS2 0x00217c80
// callcheck: strcmp

void StageCharacter::setMotionByLabel(const char *name)
{
    m_placeMotion = -1;
    for (int i = 0; i < kMotionDefCount; i++) {
        if (gMotionDefs[i].labelText != nullptr && strcmp(gMotionDefs[i].labelText, name) == 0) {
            m_placeMotion = i;
            return;
        }
    }
}

// FS2 0x00217d10
// callcheck: getPlayerAltFlag, getCompanion

bool StageCharacter::scriptIsCompanion()
{
    return gStageScene->getPlayerAltFlag() != 0 && gStageScene->party->getCompanion() == this;
}

// The hit of a script: from's hand class, on the body. The motion that takes
// it follows reaction: 0 the character's own, 1, 2 the two falls.
// FS2 0x00217d60
// callcheck: isMode1, takeDamage, setFlinch, getStageSelect, requestNow, requestNow, isPlayer, isPartner, requestNow, requestNow, requestNow (getHandClass inline)

void StageCharacter::scriptHurt(StageObject *fromObject, int amount, int reaction)
{
    constexpr int kPartBody = 1;
    constexpr int kSheetStill = 3;
    constexpr int kReactionOwn = 0;
    constexpr int kReactionHeavy = 1;
    constexpr int kReactionFall = 2;
    constexpr int kMotionHitHeavy = 0xa4;
    constexpr int kMotionHitFall = 0xa5;
    constexpr int kMotionHitCrawl = 0xa8;
    constexpr int kMotionHitLead = 0xb7;

    StageCharacter *from = static_cast<StageCharacter *>(fromObject);
    const int flinch = m_flinch;
    takeDamage(from, from->getHandClass(), kPartBody, amount);
    setFlinch(flinch);
    if (stageObjRecord(m_itemDef)->kind == kSheetStill)
        return;
    const int motionId = m_motion->motionId;
    if (gMotionDefs[motionId < 0 || motionId >= kMotionDefCount ? 0 : motionId].damageStep == 0)
        return;
    if (reaction == kReactionFall) {
        m_motion->requestNow(kMotionHitFall);
    } else if (reaction == kReactionHeavy) {
        m_motion->requestNow(kMotionHitHeavy);
    } else if (reaction == kReactionOwn) {
        if (isPlayer() || isPartner())
            m_motion->requestNow(kMotionHitLead);
        else
            m_motion->requestNow(m_abilities.flag3cc != 0 ? kMotionHitCrawl : kMotionHitHeavy);
    }
}

// FS2 0x00210810
// callcheck: currentMission, currentMission, FUN_001b9a60, isAlly, FUN_002107e0, distanceTo

bool StageCharacter::scriptHasAllyWithin(float range)
{
    StageMission *mission = gStageScene->currentMission();
    for (int i = 0; i < mission->characterCount; i++) {
        StageCharacter *other = static_cast<StageCharacter *>(mission->characters[i]);
        if (other->m_active == 0 || other == this || !isAlly(other))
            continue;
        if (other->getLifeState() == LifeState_Up && distanceTo(other) <= range)
            return true;
    }
    return false;
}

// FS2 0x00218200
// callcheck: isEnabled, askRouteActionKind11

bool StageCharacter::scriptAskRouteKind11()
{
    return m_brain != nullptr && m_brain->isEnabled() && m_brain->askRouteActionKind11();
}

// FS2 0x0021ad40
// callcheck: askSwingAtBite

bool StageCharacter::scriptSwingAtBite()
{
    return m_brain->askSwingAtBite();
}

// FS2 0x0021ad00
// callcheck: askFireAtBite

bool StageCharacter::scriptFireAtBite(const char *flags, int frames30)
{
    return m_brain->askFireAtBite(flags, frames30);
}

// FS2 0x0021ac30
// callcheck: getHandle

int StageCharacter::scriptPickableHandle()
{
    return m_allows.pickup != 0 ? (int)m_pickItem->getHandle() : 0;
}

// FS2 0x00218e00
// callcheck: useItem

void StageCharacter::scriptUseItemAt(const char *name, const float *point)
{
    constexpr int kOnPoint = 1;
    useItem(name, point, kOnPoint);
}

// FS2 0x00219ae0
// callcheck: addNoise, playActionAndRetry

bool StageCharacter::scriptCallOut(float loudness)
{
    constexpr int kNoiseCall = 3;
    gStageScene->noises->addNoise(loudness, this, kNoiseCall);
    return m_brain->playActionAndRetry("cr_3"); /* data 0x307048 */
}

// FS2 0x00218140
// callcheck: isControlled, askNumLock

bool StageCharacter::scriptAskNumLock(int which)
{
    if (isControlled())
        return false;
    return m_brain->askNumLock(which);
}

// FS2 0x002123e0
// callcheck: getKind

bool StageCharacter::isOnElevator()
{
    constexpr int kKindElevator = 4;
    StageObject *floor = m_ground->floorObject;
    return floor != nullptr && floor->getKind() == kKindElevator;
}

// FS2 0x0020d1c0
// callcheck: releaseAction

void StageCharacter::clearSlowChange()
{
    constexpr int kMotionDrained = 0xd6;
    if (m_motion->motionId == kMotionDrained)
        m_motion->releaseAction();
    m_slowStart = -1.0f;
}

// FS2 0x0020d210
// callcheck: getSeconds, getSeconds

void StageCharacter::setSlowChange(int fromPercent, int frames30, int toPercent)
{
    m_slowStart = m_clock->getSeconds();
    m_slowEnd = (float)frames30 / 30.0f + m_clock->getSeconds();
    m_slowFrom = (float)fromPercent / 100.0f;
    m_slowTo = (float)toPercent / 100.0f;
}

// FS2 0x0020d2c0
// callcheck: getSeconds, getSeconds, FUN_001deed0, playActionAndRetry

void StageCharacter::startDrainSlow(int frames30, int rate)
{
    static const char kJack[] = "sp_jack";  /* data 0x306990 */
    m_slowStart = m_clock->getSeconds();
    m_slowEnd = (float)frames30 / 30.0f + m_clock->getSeconds();
    m_slowInDrain = 0;
    m_slowFrom = (float)rate * 1.2f;
    StageObject *player = gStageScene->controlled != nullptr
        ? gStageScene->controlled : gStageScene->player;
    if (player != this && m_motion->isSpecialOnly(kJack))
        m_brain->playActionAndRetry(kJack);
}

// FS2 0x00218610

void StageCharacter::setRespawnPlace(const float *position, const float *rotation)
{
    memcpy(m_respawnPosition, position, sizeof(sceVu0FVECTOR));
    memcpy(m_respawnRotation, rotation, sizeof(sceVu0FVECTOR));
    m_respawnPosition[3] = 1.0f;
}

// FS2 0x00218190
// callcheck: isControlled, askUseSpecial, isSpecialAllowed, setSpecial

bool StageCharacter::scriptUseSpecial(const char *name)
{
    if (!isControlled())
        return m_brain->askUseSpecial(name);
    if (!isSpecialAllowed(name))
        return false;
    setSpecial(true, name);
    return true;
}

// FS2 0x00218260
// callcheck: isEnabled, askDragOutCar

bool StageCharacter::scriptDragOutCar()
{
    return m_brain != nullptr && m_brain->isEnabled() && m_brain->askDragOutCar();
}

// FS2 0x00219a90
// callcheck: addNoise, playActionAndRetry

bool StageCharacter::scriptCrouch(float loudness)
{
    constexpr int kNoiseCrouch = 3;
    static const char kCrouch1[] = "cr_1";      // data 0x307040
    gStageScene->noises->addNoise(loudness, this, kNoiseCrouch);
    return m_brain->playActionAndRetry(kCrouch1);
}

// Clamped to 0..the sheet's ひるみ耐性値. Going straight from 0 to the limit
// is noted.
// FS2 0x00211150
// callcheck: getStageSelect, getStageSelect

void StageCharacter::setFlinch(int value)
{
    const int before = m_flinch;
    const int limit = stageObjRecord(m_itemDef)->flinchResist;
    if (value < 0)
        value = 0;
    m_flinch = value <= limit ? value : limit;
    if (before == 0 && m_flinch == limit)
        m_flinched = 1;
}

// Clamped to 0..the sheet's 耐久力. The lead and the partner keep theirs
// while the input is blocked. At 0 the brain goes to state 7 and, for those
// two, the game is over; above it, one at state 7 comes back.
// FS2 0x00211430
// callcheck: isInputBlocked, getStageSelect, FUN_0021ba10, revive, startGameOver

void StageCharacter::setEndurance(int value)
{
    constexpr int kBrainDownState = 7;
    StageScene *scene = gStageScene;
    const bool lead = scene->player == this || scene->partner == this;
    if (value <= 0 && lead && scene->isInputBlocked())
        return;
    const int limit = stageObjRecord(m_itemDef)->endurance;
    if (value < 0)
        value = 0;
    m_endurance = value <= limit ? value : limit;
    if (m_endurance == 0) {
        setBrainState(kBrainDownState, 0);
        if (lead)
            scene->startGameOver(this, m_lastHitBy);
    }
    else if (getBrainPace() == kBrainDownState && m_brain != nullptr) {
        m_brain->revive();
    }
}

// FS2 0x0021ba10
// callcheck: setShown, setChildrenActive, setState, setShown, setChildrenActive, FUN_002a6380, stopSound, getDeltaSeconds

void StageCharacter::setBrainState(int state, int arg)
{
    constexpr int kBrainStates    = 8;
    constexpr int kBrainDownState = 7;
    if (state < 0 || state >= kBrainStates || state == getBrainPace())
        return;
    if (state == 0) {
        m_tree->setShown(false);
        m_viewActive = 0;
        m_ground->m_enabled = 0;
        m_tree->setChildrenActive(false);
        stopBodySounds();
        if (m_breathSound != nullptr) {
            stopSound(m_breathSound);
            m_breathSound = nullptr;
        }
    }
    else if (getBrainPace() == 0) {
        m_tree->setShown(true);
        m_viewActive = 1;
        m_ground->m_enabled = 1;
        m_tree->setChildrenActive(true);
    }
    if (state == kBrainDownState)
        m_motion->damageDeadLink->reviveFrames = (int)(m_reviveTime * (float)(int)(1.0f / m_clock->getDeltaSeconds()));
    if (m_brain != nullptr)
        m_brain->setState(state, arg);
}

// ──────────────────────────────────────────────────────────────────────
// What the sheet's percent and thirtieths are stored as.
constexpr float kPercent     = 100.0f;
constexpr float kThirtieths  = 30.0f;
constexpr int   kShotBlanks  = 999;     // CharProp_Shot, the sheet's 射撃
constexpr u_int kSideBitsEnd = 0x10;    // the four StageObjSide bits
constexpr int   kLightResistLevels   = 3;
constexpr int   kValue62cLevels      = 10;
constexpr int   kScriptValues        = 11;
constexpr int   kScriptValuesCleared = 10; // the block's loop leaves the last one
constexpr int   kLifeDownInMotion    = 4;  // getLifeState: down, in motions 0xb3 / 0xb4

// FS2 0x00211300
// callcheck: getStageSelect

void StageCharacter::setRightArm(int value)
{
    const int was = m_rightArm;
    const int limit = stageObjRecord(m_itemDef)->rightArm;
    if (value < 0)
        value = 0;
    m_rightArm = value <= limit ? value : limit;
    m_armLost = was >= 1 && m_rightArm == 0;
}

// The sheet's 初期M says what the brain starts as; the sheet's numbers skip one.
static int firstBrainState(int startMotion)
{
    switch (startMotion) {
    case 4:  return 6;
    case 3:  return 5;
    case 2:  return 2;
    case 1:  return 1;
    default: return 0;
    }
}

// The fields initStatus sets too are set again here, as the ROM does. Left
// out, nothing here reads them: the byte and two ints at +0x610.
// FS2 0x0021cc70
// callcheck: getStageSelect, setState

void StageCharacter::initStatusBlock()
{
    const StageObjRecord *sheet = stageObjRecord(m_itemDef);
    m_health = (float)sheet->health;
    m_endurance = sheet->endurance;
    m_leftArm = sheet->leftArm;
    m_rightArm = sheet->rightArm;
    m_rightLeg = sheet->rightLeg;
    m_leftLeg = sheet->leftLeg;
    m_aiLevel = sheet->aiLevel;
    m_sight = (float)sheet->sight;
    m_hearing = (float)sheet->hearing / kPercent;
    m_enduranceRegen = sheet->recovery;
    if (sheet->lightResist >= 0 && sheet->lightResist < kLightResistLevels)
        m_lightResist = sheet->lightResist;
    m_lightDamage = sheet->lightDamage;
    m_lightFlinch = sheet->lightFlinch;
    m_unarmedAttack = sheet->unarmedAttack;
    m_biteDamage = sheet->bite;
    m_walkRate = (float)sheet->walkSpeed / kPercent;
    m_runRate = (float)sheet->runSpeed / kPercent;
    m_turnRate = (float)sheet->turnSpeed / kPercent;
    m_reviveTime = (float)sheet->reviveTime / kThirtieths;
    m_drainRate = sheet->controlResist;
    m_trapResist = (float)sheet->trapResist / kThirtieths;
    setShot(sheet->shot);
    m_shotInterval = (float)sheet->shotInterval / kThirtieths;
    m_attackInterval = (float)sheet->attackInterval / kThirtieths;
    m_biteInterval = (float)sheet->biteInterval / kThirtieths;
    m_reloadRate = (float)sheet->reload / kPercent;
    m_meleeScale = sheet->meleeAttackScale;
    m_shotScale = sheet->shotAttackScale;
    m_flinch = 0;
    m_unarmedMsp = sheet->unarmedMsp;
    m_5b4 = 0;
    m_shrugsHits = 0;
    m_biteProof = 0;
    m_5ba = 0;
    if (m_brain != nullptr)
        m_brain->setState(firstBrainState(sheet->startMotion), 0);
    for (int i = 0; i < kScriptValuesCleared; i++)
        m_scriptValues[i] = 0;
    m_lastHitBy = nullptr;
    m_biteCandidate = nullptr;
    m_nearSearchPoint = nullptr;
    m_spawnPoint = -1;
    memset(m_respawnPosition, 0, sizeof(m_respawnPosition));
    memset(m_respawnRotation, 0, sizeof(m_respawnRotation));
    m_62c = 0;
    m_670 = 0;
    m_bulletFlying = 0;
    m_lifeOverride = -1;
    m_sides = sheet->sides;
    m_6c8 = 0;
    m_6cc = 0;
    m_6d0 = 0;
    m_6d4 = 0;
}

// Back to the state a character has on entering the stage: no point known,
// nobody seen.
// FS2 0x0021e050

void StageCharacter::resetActionState()
{
    m_point700.isSet = 0;
    m_point6e0.isSet = 0;
    static const sceVu0FVECTOR kNoPoint = { 0.0f, 0.0f, 0.0f, 1.0f };   // data 0x2fa7e0
    m_scriptPoint.set(kNoPoint);
    m_point740.isSet = 0;
    m_point760.isSet = 0;
    m_point780.isSet = 0;
    initStatusBlock();
    m_sightOwner = this;
    for (u_int i = 0; i < kSightEntries; i++) {
        m_sightTable[i].seen = 0;
        m_sightTable[i].frame = -1;
    }
    m_armLost = 0;
    m_dropsWeapon = 1;
    m_flinched = 0;
    m_fidgets = 1;
    m_placedThisFrame = 0;
    m_area = nullptr;
    m_placeMotion = 0;
}

// FS2 0x0021cbf0
void StageCharacter::setSides(int sides)
{
    if ((u_int)sides < kSideBitsEnd)
        m_sides = (u_int)sides;
}

// One function in the ROM per bit.
// FS2 0x0021cab0
// FS2 0x0021cb00
// FS2 0x0021cb50
// FS2 0x0021cba0
void StageCharacter::setSideBit(u_int bit, bool on)
{
    const u_int sides = on ? m_sides | bit : m_sides & ~bit;
    if (sides < kSideBitsEnd)
        m_sides = sides;
}

constexpr int kLifeUp = 2;     // getLifeState

// The enemies the character's side lists, or everyone with a foe of its own.
// FS2 0x00210720
// callcheck: getEnemyCandidates, FUN_001b9a60, isEnemyOf, FUN_002107e0, getBiteTarget

bool StageCharacter::hasBiter()
{
    StageMission *mission = gStageScene->currentMission();
    MissionObjectList all = { mission->characters, mission->characterCount };
    const MissionObjectList &list = m_foe == nullptr ? mission->byClass[getSides()] : all;
    for (int i = 0; i < list.count; i++) {
        StageObject *object = list.objects[i];
        if (object->m_active == 0 || !isEnemyOf(object))
            continue;
        StageCharacter *other = static_cast<StageCharacter *>(object);
        if (other->getLifeState() == kLifeUp && other->getBiteTarget() == this)
            return true;
    }
    return false;
}

// FS2 0x00216390
// FS2 0x0021bee0
// callcheck: strcmp, findItem

StageObject *StageCharacter::findItem(const char *name)
{
    for (StageList<StageObject *>::Node *link = m_items.head; link != nullptr; link = link->next) {
        if (strcmp(link->value->m_name, name) == 0)
            return link->value;
    }
    return nullptr;
}

// FS2 0x001dedb0
// callcheck: request, request

void PlayerMotion::releaseAction()
{
    constexpr int kMotionIdle = 0;
    const bool hasNext = motionId >= 0 && motionId < kMotionDefCount
        && gMotionDefs[motionId].nextMotion != Motion_None;
    if (activeLink != nullptr && hasNext)
        activeLink->finished = 1;
    else
        request(kMotionIdle);
}

// layout: host -- what part of the body a bone is, for a shot (data 0x2f8650, 0x19 rows of 8 bytes).
struct BoneHitPart
{
    const char *bone;
    int         part;
};
static const BoneHitPart kBoneHitParts[] = {
    { "bn_pelvis", 1 },       { "bn_spine0", 1 },       { "bn_spine1", 1 },
    { "bn_spine2", 1 },       { "bn_neck", 0 },         { "bn_head", 0 },
    { "bn_l_clavicle", 1 },   { "bn_l_upperarm", 1 },   { "bn_l_foretwist0", 2 },
    { "bn_l_foretwist1", 2 }, { "bn_l_hand", 2 },       { "bn_l_thigh", 4 },
    { "bn_l_calf", 4 },       { "bn_l_foot", 4 },       { "bn_r_clavicle", 1 },
    { "bn_r_upperarm", 1 },   { "bn_r_foretwist0", 3 }, { "bn_r_foretwist1", 3 },
    { "bn_r_hand", 3 },       { "bn_r_thigh", 5 },      { "bn_r_calf", 5 },
    { "bn_r_foot", 5 },       { "bn_l_toe0", 1 },       { "bn_l_toe1", 1 },
    { "bn_l_toe2", 1 },
};

// The part hit by a shot on that bone. PC: the ROM leaves a bone outside the
// table on whatever s3 held; the body is the plain answer.
static int shotPart(const char *bone)
{
    constexpr int kPartBody = 1;
    for (const BoneHitPart &row : kBoneHitParts) {
        if (strcmp(row.bone, bone) == 0)
            return row.part;
    }
    return kPartBody;
}

// The reaction of who has those abilities to a blow of that kind; Motion_None
// a kind with none.
static int blowMotion(int kind, const CharacterAbilities &abilities, bool inBack)
{
    if (abilities.flag3cc != 0) {
        switch (kind) {
        case MotionDamage_Side1:
            return inBack ? Motion_DamageSide2 : Motion_DamageSide1;
        case MotionDamage_Side2:
            return inBack ? Motion_DamageSide1 : Motion_DamageSide2;
        case MotionDamage_HardSide1:
            return inBack ? Motion_DamageSide4 : Motion_DamageSide3;
        case MotionDamage_HardSide2:
            return inBack ? Motion_DamageSide3 : Motion_DamageSide4;
        default:
            break;
        }
    }
    const int plain = inBack ? Motion_DamageRear : Motion_DamageFront;
    const int hard = inBack ? Motion_DamageRearHeavy : Motion_DamageFrontHeavy;
    switch (kind) {
    case MotionDamage_Plain:
    case MotionDamage_Side1:
    case MotionDamage_Side2:
        return plain;
    case MotionDamage_Hard:
    case MotionDamage_HardSide1:
    case MotionDamage_HardSide2:
        return abilities.flag3cd != 0 ? hard : plain;
    default:
        return Motion_None;
    }
}

// How near a trap has to be to catch who plays this motion: a longer stride
// reaches it sooner.
static float trapReach(int id)
{
    constexpr float kReachRun = 50.0f;
    constexpr float kReachDash = 40.0f;
    constexpr float kReachWalk = 25.0f;
    constexpr float kReachStill = 10.0f;
    switch (id) {
    case 9: case 10: case 0x16: case Motion_StairsRunUp: case Motion_StairsRunDown:
        return kReachRun;
    case 0x24: case 0x2d:
        return kReachDash;
    case 3: case 4: case 5: case 6: case 7: case 8:
    case Motion_StairsUp: case Motion_StairsDown: case Motion_CrouchWalk:
    case 0x51: case 0x52: case 0x53: case 0x54:
    case 0x6c: case 0x6d: case 0x6e: case 0x6f:
    case 0x7b: case 0x7c: case 0x7d: case 0x7e:
    case 0x8a: case 0x8b: case 0x8c: case 0x8d:
        return kReachWalk;
    default:
        return kReachStill;
    }
}

// What is asked for, else what plays.
const MotionDef *PlayerMotion::upcomingDef() const
{
    const MotionDef *def = motionDef(m_request);
    return def != nullptr ? def : motionDef(motionId);
}

// The motion that is on takes a hit, for one who shows it.
bool PlayerMotion::reactsToHit(StageCharacter *character) const
{
    constexpr int kSheetStill = 3;
    constexpr int kDamageStepOn = 1;
    const MotionDef *def = upcomingDef();
    return def != nullptr && def->damageStep == kDamageStepOn && character->m_shrugsHits == 0
        && stageObjRecord(character->m_itemDef)->kind != kSheetStill;
}

// A light that hurts it: a hurt every so often, by its sheet.
void PlayerMotion::takeLightHit(StageCharacter *character)
{
    constexpr int kHitHardLight = 5;
    constexpr int kHitSoftLight = 6;
    constexpr int kPartBody = 1;
    constexpr float kFlinchFps = 30.0f;
    constexpr u_int kWaitJitter = 20;       // up to this many percent more
    constexpr float kPercent = 100.0f;
    constexpr int kDamageStepOn = 1;

    StageLight *light = character->m_shadowLight;
    if (character->m_lightDamage <= 0 || light == nullptr || light->m_yamLight != 0
        || character->m_lightHurtWait > 0.0f)
        return;
    character->takeDamage(nullptr, light->getMode() == StageLight_Hard ? kHitHardLight : kHitSoftLight,
                          kPartBody, stageObjRecord(character->m_itemDef)->lightDamage);
    const float wait = (float)character->m_lightFlinch / kFlinchFps;
    character->m_lightHurtWait = wait + wait * (float)(randNext() % kWaitJitter) / kPercent;
    if (character->m_shrugsHits == 0)
        character->m_hitThisFrame = 1;
    const MotionDef *def = motionDef(motionId);
    if (def != nullptr && def->damageStep == kDamageStepOn && character->m_shrugsHits == 0)
        requestNow(Motion_Burn);
}

// The damage and the effect of what reached it. False: it was shrugged off. A
// sleeper of kinds 5, 6, 0xc takes nothing from the front unless the blow's
// owner has the Kyo model; "kikanai" (効かない, it does not work) shows instead
// of blood, and nothing reacts.
bool PlayerMotion::landBlow(StageCharacter *character, CharacterGround *ground, bool &headShot)
{
    constexpr int kSheetSleeperA = 0xc;
    constexpr int kSheetSleeperB = 6;
    constexpr int kSheetSleeperC = 5;
    constexpr int kBrainPaceAsleep = 5;
    constexpr int kHitShot = 4;
    constexpr int kShotBlood = 4;
    constexpr int kShruggedCount = 1;
    constexpr float kRadiusNoBone = 40.0f;
    constexpr float kEffectWait = 0.15f;    /* data 0x31a9d8 */
    static const char kShrugged[] = "kikanai"; /* data 0x3054c0 */

    const int sheetKind = stageObjRecord(character->m_itemDef)->kind;
    StageObject *by = ground->hitBy;
    bool shrugged = false;
    if (ground->hitInBack == 0
        && (sheetKind == kSheetSleeperA || sheetKind == kSheetSleeperB || sheetKind == kSheetSleeperC)
        && character->getBrainPace() == kBrainPaceAsleep) {
        StageCharacter *striker = nullptr;
        if (by->getKind() == StageObject_Bullet)
            striker = static_cast<StageBullet *>(by)->shooter();
        else if (by->getKind() == StageObject_Character)
            striker = static_cast<StageCharacter *>(by);
        shrugged = striker != nullptr && !striker->hasKyoModel();
    }

    const int byKind = by->getKind();
    if (byKind == StageObject_Bullet) {
        StageBullet *bullet = static_cast<StageBullet *>(by);
        if (!shrugged) {
            const int part = shotPart(ground->hitRow->name);
            headShot = part == 0;
            StageCharacter *shooter = bullet->shooter();
            character->takeDamage(shooter, kHitShot, part,
                                  weaponDefOf(bullet->weapon())->ammoParam1 + shooter->m_shotScale);
        }
        if (m_hitTime <= 0.0f) {
            const float radius = ground->hitRow != nullptr ? ground->hitRow->radius : kRadiusNoBone;
            probe->spawnBlood(radius, ground->hitNode, ground->hitDirection,
                              shrugged ? kShruggedCount : kShotBlood, shrugged ? kShrugged : nullptr);
            m_hitTime = kEffectWait;
        }
        character->playHitSound(nullptr);
    } else if (byKind == StageObject_Car) {
        StageCar *car = static_cast<StageCar *>(by);
        if (!shrugged && car->m_driver != nullptr)
            character->takeBlow(car->m_driver);
    } else if (byKind == StageObject_Character) {
        StageCharacter *striker = static_cast<StageCharacter *>(by);
        const MotionDef *def = motionDef(striker->m_motion->motionId);
        if (def != nullptr) {
            if (!shrugged)
                character->takeBlow(striker);
            StageObject *weapon = striker->m_weapons[0];
            if (m_hitTime <= 0.0f) {
                const int count = weapon != nullptr ? weaponSheet(weapon).param11
                                                    : stageObjRecord(striker->m_itemDef)->bloodSpray;
                const float radius = ground->hitRow != nullptr ? ground->hitRow->radius : kRadiusNoBone;
                probe->spawnBlood(radius, ground->hitNode, ground->hitDirection,
                                  shrugged ? kShruggedCount : count, shrugged ? kShrugged : nullptr);
                m_hitTime = kEffectWait;
            }
            if (def->damageKind >= MotionDamage_Plain && def->damageKind <= MotionDamage_HardSide2)
                character->playHitSound(static_cast<StageWeaponItem *>(weapon));
        }
    }
    return !shrugged;
}

// The motion a blow that got through asks for, and the pad.
void PlayerMotion::reactToBlow(StageCharacter *character, CharacterGround *ground, bool headShot)
{
    constexpr float kFarHeight = 200.0f;
    constexpr float kFarDistance = 800.0f;
    constexpr int kSheetDriven = 7;
    constexpr int kSheetBounces = 10;

    const bool inBack = ground->hitInBack != 0;
    const int heavy = inBack ? Motion_DamageRearHeavy : Motion_DamageFrontHeavy;
    const int plain = inBack ? Motion_DamageRear : Motion_DamageFront;
    StageObject *by = ground->hitBy;
    const int byKind = by->getKind();

    if (byKind == StageObject_Character) {
        const MotionDef *def = motionDef(static_cast<StageCharacter *>(by)->m_motion->motionId);
        if (def == nullptr)
            return;
        const int id = blowMotion(def->damageKind, character->m_abilities, inBack);
        if (id != Motion_None)
            requestNow(id);
        character->rumbleHit(false);
    } else if (byKind == StageObject_Bullet) {
        if (character->hasGear()) {
            requestNow(Motion_CarDamage);
        } else {
            // A shot from far off, for who is made to feel the distance
            // (flag3ce), is its own motion.
            StageCharacter *shooter = static_cast<StageBullet *>(by)->shooter();
            const float *theirs = shooter->m_model->boneMap.find("bn_head")->value->getMatrix()[3];
            const float *ours = character->m_model->boneMap.find("bn_head")->value->getMatrix()[3];
            const bool closeBy = character->m_abilities.flag3ce == 0
                || (fabsf(theirs[2] - ours[2]) < kFarHeight && vecDistance(ours, theirs) < kFarDistance);
            if (headShot)
                requestNow(heavy);
            else if (closeBy)
                requestNow(plain);
            else
                requestNow(inBack ? Motion_DamageHitRear : Motion_DamageHitFront);
        }
        character->rumbleHit(headShot);
    } else if (byKind != StageObject_Car) {
        requestNow(plain);
        character->rumbleHit(false);
    } else {
        const int level = character->m_ground->slideLevel;
        const bool lead = character->isPlayer() || character->isPartner();
        const int sheetKind = stageObjRecord(character->m_itemDef)->kind;
        const bool worst = level == GroundSlide_Worst
            || (level == GroundSlide_Heavy && !lead && sheetKind != kSheetDriven && sheetKind != kSheetBounces);
        if (worst) {
            if (lead) {
                requestNow(character->m_endurance == 0 ? Motion_Collapse : heavy);
            } else if (sheetKind == kSheetDriven) {
                requestNow(Motion_Collapse);
            } else {
                requestNow(inBack ? Motion_CarHitBack : Motion_CarHitFront);
            }
        } else if (level == GroundSlide_Heavy || level == GroundSlide_Light) {
            requestNow(heavy);
        }
        character->rumbleHit(true);
    }
}

// What reached it this frame: the damage, then, for who feels it, the
// reaction, and the flinch starts over.
void PlayerMotion::takeBlowHit(StageCharacter *character, CharacterGround *ground)
{
    bool headShot = false;
    const bool gotThrough = landBlow(character, ground, headShot);
    if (!reactsToHit(character) || !gotThrough)
        return;
    const StageObjRecord *sheet = stageObjRecord(character->m_itemDef);
    if (character->m_flinch < sheet->flinchResist && !character->isPlayer() && !character->isPartner())
        return;
    if (isLadderMotion(motionId)) {
        requestNow(Motion_DamageLadder);
        character->rumbleHit(false);
    } else {
        reactToBlow(character, ground, headShot);
    }
    character->setFlinch(0);
    character->m_unarmedMsp = clampToSheet(sheet->unarmedMsp, sheet->unarmedMsp);
}

// A trap in reach of a low-level character catches it.
void PlayerMotion::takeTrapHit(StageCharacter *character, CharacterGround *ground)
{
    constexpr int kTrapAiLevelLast = 1;
    constexpr float kTrapWalkRate = 0.15f;
    if (ground->nearSomeone == 0 || ground->nearObject->getKind() != StageObject_TrapItem)
        return;
    if (stageObjRecord(character->m_itemDef)->aiLevel > kTrapAiLevelLast)
        return;
    StageTrapItem *trap = static_cast<StageTrapItem *>(ground->nearObject);
    if (vecDistance(character->m_xform.position, trap->m_xform.position) > trapReach(motionId))
        return;
    requestNow(Motion_TrapHit);
    character->playNamedHeard2("TRAP_ON", character->m_xform.position, nullptr);
    character->playTrapHitVoice();
    character->rumbleHit(true);
    startWalkFile(kTrapWalkRate, requestedFile(), trap->m_xform.position, character->m_xform.rotation,
                  false, true);
    m_sprungTrap = trap;
    trap->spring();
}

// The first frame on a floor: what the drop cost.
void PlayerMotion::takeLanding(StageCharacter *character, CharacterGround *ground)
{
    constexpr int kSheetDriven = 7;
    constexpr int kSheetStill = 3;
    constexpr float kFallSafe = 305.0f;
    constexpr float kFallBase = 300.0f;
    constexpr float kFallSteady = 395.0f;   // for a stepDown of 1
    constexpr float kDropSafe = 10.0f;
    constexpr float kPerMetre = 100.0f;
    constexpr float kDamagePerMetre = 10.0f;
    constexpr int kFallDamageBase = 10;
    constexpr int kStepDownNone = 0;
    constexpr int kStepDownSteady = 1;

    const StageObjRecord *sheet = stageObjRecord(character->m_itemDef);
    if (sheet->kind == kSheetDriven || sheet->kind == kSheetStill)
        return;
    if (ground->landed == 0 || ground->wasLanded != 0)
        return;
    int damage = 0;
    const float drop = ground->floorHeight - character->m_xform.position[2];
    if (motionId == Motion_LadderLetGo || motionId == Motion_FallLow || motionId == Motion_FallMid
        || motionId == Motion_FallHigh) {
        if (drop > kFallSafe
            && (sheet->stepDown == kStepDownNone
                || (sheet->stepDown == kStepDownSteady && drop >= kFallSteady))) {
            damage = (int)((drop - kFallBase) / kPerMetre * kDamagePerMetre) + kFallDamageBase;
        }
    } else if (motionId == Motion_LadderFall || motionId == Motion_DiveFront || motionId == Motion_DiveBack) {
        if (m_bfc != 0) {
            if (character->isPlayer() || character->isPartner())
                character->playerEvent("DMGLND");
            else
                damage = character->m_endurance;
        } else if (drop > kDropSafe) {
            damage = (int)(drop / kPerMetre * kDamagePerMetre) + kFallDamageBase;
        }
    }
    if (damage > 0) {
        character->takeFallDamage(damage);
        character->playerEvent("DMGLND");
    } else if (motionId == Motion_FallHigh) {
        character->playerEvent("LNDH");
    }
}

// Who climbs over it, and the door that comes at it.
void PlayerMotion::takeShoves(StageCharacter *character, CharacterGround *ground)
{
    constexpr int kHitBare = 9;
    constexpr int kPartBody = 1;
    if (ground->climbedOver != 0 && motionId != Motion_DamageLadder && motionId != Motion_DamageFrontHeavy) {
        character->takeDamage(nullptr, kHitBare, kPartBody, 1);
        if (reactsToHit(character)) {
            requestNow(isLadderMotion(motionId) ? Motion_DamageLadder : Motion_DamageFrontHeavy);
            character->rumbleHit(true);
        }
    }
    if (ground->atDoor != 0 && reactsToHit(character)) {
        requestNow(isLadderMotion(motionId) ? Motion_DamageLadder : Motion_DamageFront);
        character->rumbleHit(false);
    }
}

// The frame's hits. While a deadly fall goes on (m_bfc) nothing counts until
// it lands. One who cannot be hit (m_biteProof, brain pace 0) still falls when
// its endurance is gone.
// FS2 0x001e18c0
// callcheck: requestNow, rumbleHit, spawnBlood, takeBlow, takeFallDamage (in the parts: takeLightHit, takeBlowHit, landBlow, reactToBlow, takeTrapHit, takeLanding, takeShoves; the inline stage kind reads are stageObjRecord; startWalkFile inline)

void PlayerMotion::takeHits()
{
    m_wallCheckNow = m_wallCheck;

    StageCharacter *character = static_cast<StageCharacter *>(owner);
    CharacterGround *ground = character->m_ground;
    if (m_bfc != 0 && (motionId == Motion_LadderFall || motionId == Motion_DiveBack || motionId == Motion_DiveFront)
        && !(ground->landed != 0 && ground->wasLanded == 0))
        return;
    if (ground->wonStruggle != 0)
        requestNow(Motion_LadderLetGo);

    if (character->m_biteProof == 0 && character->getBrainPace() != 0) {
        takeLightHit(character);
        if (ground->wasHit != 0)
            takeBlowHit(character, ground);
        takeTrapHit(character, ground);
        takeLanding(character, ground);
        takeShoves(character, ground);
    }

    const MotionDef *def = upcomingDef();
    if (def != nullptr && def->damageStep > 0 && character->m_endurance == 0) {
        if (isLadderMotion(motionId))
            requestNow(Motion_LadderFall);
        else
            requestNow(character->hasGear() ? Motion_CarCollapse : Motion_Collapse);
    }
    if (m_hitTime > 0.0f)
        m_hitTime -= character->m_clock->getDeltaSeconds();
}

// count effects where a blow along direction enters a body of that radius,
// the rest within 20 across and 15 up of the first.
// FS2 0x0026a010
// callcheck: atan2Octant, getWorldMatrix, scaleVec4, spawn, randNext, randNext, randNext, spawn

void MotionProbe::spawnBlood(float radius, StageNodeBase *node, const float *direction, int count,
                             const char *effect)
{
    constexpr float kHalfTurn = 1.5707964f;
    constexpr float kInset = 10.0f;
    constexpr float kScatterWide = 20.0f;
    constexpr float kScatterTall = 15.0f;
    constexpr float kRandRange = 2147483648.0f; // randNext's
    constexpr int kFallbackCount = 3;
    static const char kFallbackEffect[] = "damage"; /* data 0x3089f8 */

    const sceVu0FVECTOR rotation = { kHalfTurn, 0.0f, atan2Octant(-direction[0], direction[1]), 0.0f };
    sceVu0FVECTOR place;
    sceVu0FVECTOR inset;
    memcpy(place, node->getWorldMatrix()[3], sizeof(place));
    scaleVec4(-(radius - kInset), inset, direction);
    for (int i = 0; i < 4; i++)
        place[i] += inset[i];
    if (effect == nullptr) {
        effect = owner->m_bloodEffect;
        if (count == 0) {
            count = kFallbackCount;
            effect = kFallbackEffect;
        }
    }
    StageNodeTree *tree = gStageScene->nodeTree;
    tree->spawn(effect, nullptr, place, rotation, 0, 0);
    for (int i = 1; i < count; i++) {
        const float x = (float)randNext() * (2.0f * kScatterWide / kRandRange) - kScatterWide;
        const float y = (float)randNext() * (2.0f * kScatterWide / kRandRange) - kScatterWide;
        const float z = (float)randNext() * (2.0f * kScatterTall / kRandRange) - kScatterTall;
        const sceVu0FVECTOR spot = { place[0] + x, place[1] + y, place[2] + z, 0.0f };
        tree->spawn(effect, nullptr, spot, rotation, 0, 0);
    }
}

// The party's stop, for a lead running with it, when it gets hurt.
static void stopFleeing(StageCharacter *character)
{
    constexpr int kMotionFleeA = 0x11;
    constexpr int kMotionFleeB = 0x2a;
    StageScene *scene = gStageScene;
    if (!character->isPlayer() || !character->isScenarioLead())
        return;
    const bool fleeing = static_cast<StageObject *>(scene->player) == character
        ? scene->party->isRunning()
        : character->m_motion->motionId == kMotionFleeA || character->m_motion->motionId == kMotionFleeB;
    if (fleeing) {
        scene->party->stop();
        if (scene->gameOver != nullptr)
            character->m_altAction = 0;
    }
}

// Off a ladder or over an edge it is its own doing: the game over's cause.
// FS2 0x0020e770
// callcheck: getStageSelect, getStageSelect, setEndurance, isPlayer, isScenarioLead, isRunning, stop, getStageResult

int StageCharacter::takeFallDamage(int amount)
{
    constexpr int kFallCause = 8;
    constexpr int kResultHitsTaken = 2;
    constexpr int kLegScale = 2;
    StageScene *scene = gStageScene;
    const int id = m_motion->motionId;
    if (id == Motion_LadderLetGo || id == Motion_FallHigh || id == Motion_FallMid || id == Motion_FallLow)
        m_lastHitBy = this;
    amount = std::max(amount, 0);
    const StageObjRecord *sheet = stageObjRecord(m_itemDef);
    m_rightLeg = clampToSheet(m_rightLeg - amount * kLegScale, sheet->rightLeg);
    m_leftLeg = clampToSheet(m_leftLeg - amount * kLegScale, sheet->leftLeg);
    setEndurance(m_endurance - amount);
    m_62c = kFallCause;
    StageObject *const onPad = scene->controlled != nullptr
        ? scene->controlled : static_cast<StageObject *>(scene->player);
    if (onPad == this && static_cast<StageObject *>(scene->player) != onPad)
        scene->altLatch = 1;
    stopFleeing(this);
    if (static_cast<StageObject *>(scene->player) == this)
        getStageResult()->counter[kResultHitsTaken]++;
    return amount;
}

// The victim's own 打撃攻撃力修正値 goes on a weapon's blow (0x6c0(s4), the
// victim, not from): it is what the ROM reads.
// FS2 0x002103d0
// callcheck: getHandClass, randNext2, at, takeDamage

void StageCharacter::takeBlow(StageCharacter *from)
{
    constexpr u_int kPartPercent = 100;
    constexpr u_int kBodyBelow = 60;
    constexpr u_int kLeftArmBelow = 70;
    constexpr u_int kRightArmBelow = 80;
    constexpr u_int kRightLegBelow = 90;
    constexpr int kPartBody = 1;
    constexpr int kPartLeftArm = 2;
    constexpr int kPartRightArm = 3;
    constexpr int kPartRightLeg = 4;
    constexpr int kPartLeftLeg = 5;
    constexpr int kWorstScale = 4;
    constexpr int kDriving = 1;

    const int kind = from->getHandClass();
    int part = kPartBody;
    int amount;
    if (from->m_gear != nullptr && from->m_abilities.driving == kDriving) {
        const int level = m_ground->slideLevel;
        amount = level == GroundSlide_Worst ? m_endurance * kWorstScale
                                            : *static_cast<StageCar *>(from->m_gear)->runOverDamage(level);
    } else {
        amount = from->m_weapons[0] == nullptr ? from->m_unarmedAttack
                                               : weaponDefOf(from->m_weapons[0])->param1 + m_meleeScale;
        const u_int roll = randNext2() % kPartPercent;
        if (roll >= kRightLegBelow)
            part = kPartLeftLeg;
        else if (roll >= kRightArmBelow)
            part = kPartRightLeg;
        else if (roll >= kLeftArmBelow)
            part = kPartRightArm;
        else if (roll >= kBodyBelow)
            part = kPartLeftArm;
    }
    takeDamage(from, kind, part, amount);
}

// A rumble by name, for the player alone.
// FS2 0x0020e6a0
// callcheck: addNamed

void StageCharacter::playerEvent(const char *name)
{
    StageScene *scene = gStageScene;
    if (scene->rumble != nullptr && static_cast<StageObject *>(scene->player) == this)
        scene->rumble->addNamed(name, this);
}

// FS2 0x0020e6e0
// callcheck: getRumbleSource, addNamed

void StageCharacter::rumbleHit(bool hard)
{
    StageScene *scene = gStageScene;
    if (scene->rumble != nullptr
        && (static_cast<StageObject *>(scene->player) == this || scene->getRumbleSource() == this))
        scene->rumble->addNamed(hard ? "DAMAGE2" : "DAMAGE1", this);
}

// A run's time and distance, which Move reads to trip it on a wall.

void PlayerMotion::stepRunClock(StageCharacter *character)
{
    constexpr float kRunTimeLimit   = 10.0f;
    constexpr float kWindedRecovery = 2.0f;
    if (m_request != Motion_None || !isRunMotion(motionId)) {
        m_runTime = 0.0f;
        m_runDistance = 0.0f;
        return;
    }
    if (m_runTime == 0.0f) {
        memcpy(m_lastPosition, character->m_xform.position, sizeof(m_lastPosition));
    } else {
        const float distance = vecDistance(m_lastPosition, character->m_xform.position);
        if (m_runDistance < distance)
            m_runDistance = distance;
    }
    if (character->m_winded == 0) {
        m_runTime += character->m_clock->getDeltaSeconds();
    } else if (0.0f < m_runTime) {
        if (kRunTimeLimit < m_runTime)
            m_runTime = kRunTimeLimit;
        m_runTime -= character->m_clock->getDeltaSeconds() * kWindedRecovery;
        if (m_runTime < 0.0f) {
            m_runTime = 0.0f;
            m_runDistance = 0.0f;
        }
    }
}

// Inline in the ROM: the motions in which a Yri or Kne keeps its second stance.
static bool isAltStanceMotion(int id)
{
    switch (id) {
    case 0xf8: case 0xf7: case 0xf6: case 0xf5: case 0xf4:
    case 2: case 1: case 0:
    case 0x15: case 0x14:
    case 0x1f: case 0x1e: case 0x1d: case 0x2b:
        return true;
    }
    return false;
}

// A Yri or Kne's second stance comes half a second after a light that is not
// a YAM_ one starts to throw its shadow, and goes half a second after it stops.

void PlayerMotion::stepAltStance(StageCharacter *character)
{
    constexpr float kStanceDelay = 0.5f;
    const StageLight *light = character->m_shadowLight;
    if (light == nullptr || light->m_yamLight != 0) {
        if (!isAltStanceMotion(motionId)) {
            m_stanceFlag = 0;
            m_stanceTime = 0.0f;
        } else if (m_stanceFlag == 0) {
            m_stanceTime = 0.0f;
        } else {
            m_stanceTime += character->m_clock->getDeltaSeconds();
            if (kStanceDelay < m_stanceTime) {
                m_stanceFlag = 0;
                m_stanceTime = 0.0f;
            }
        }
    } else if (m_stanceFlag == 0) {
        m_stanceTime += character->m_clock->getDeltaSeconds();
        if (kStanceDelay < m_stanceTime) {
            m_stanceFlag = 1;
            m_stanceTime = 0.0f;
        }
    } else {
        m_stanceTime = 0.0f;
    }
}

// The motion's sound event: the next of the sheet's S0..S3, whose names are
// kMotionShotKinds' (packtables.cpp: the values below). Kind 0xd falls into
// 0xe in the ROM: JUMP also says AT_NW1.

void PlayerMotion::stepSoundEvents(StageCharacter *character)
{
    const u_int events = character->m_model->m_motionEvents;
    if ((events & MotionEvent_HitSwing) != 0)
        character->playSwingSound(true);
    if ((events & MotionEvent_Sound) == 0)
        return;
    if (motionId < 0 || motionId >= kMotionDefCount)
        return;
    // PC: past S3 the ROM reads the columns that follow as kinds.
    if (m_soundEvent >= kMotionShots) {
        m_soundEvent++;
        return;
    }

    const MotionDef *def = &gMotionDefs[motionId];
    const int kind = def->shotKind[m_soundEvent];
    switch (kind) {
    case 0x00: character->playShout(m_shoutLoudness, 0, character->m_xform.position); break;
    case 0x01: character->playIdleVoice(0); break;
    case 0x02: character->playIdleVoice(1); break;
    case 0x03: character->playDownVoice(); break;
    case 0x04: character->playClashVoice(); break;
    case 0x05: case 0x06: case 0x07: case 0x08: case 0x09:
        character->playBreathUp(kind - 0x05);
        break;
    case 0x0a: case 0x0b: case 0x0c:
        character->playBreathDown(kind - 0x0a);
        break;
    case 0x0d:
        character->playJumpVoice();
        // fall through
    case 0x0e: case 0x10: case 0x12:
        character->playAttackVoice(0, character->hasAlteredVoice());
        break;
    case 0x0f: case 0x11: case 0x13: case 0x14:
        character->playAttackVoice(1, character->hasAlteredVoice());
        break;
    case 0x15: character->playHardDownVoice(); break;
    case 0x16: character->playTrapVoice(); break;
    case 0x17: character->playThrowVoice(); break;
    case 0x18: character->playBiteVoice(); break;
    case 0x19: character->playBittenVoice(); break;
    case 0x1a: character->playUntieVoice(); break;
    case 0x1b: character->playDamageVoice(true); break;
    case 0x1c: character->playDamageVoice(false); break;
    case 0x1d: character->playDieVoice(); break;
    case 0x1e: character->playGetUpVoice(); break;
    case 0x1f: character->playSurpriseVoice(); break;
    case 0x20: character->playPanicVoice(); break;
    case 0x21: character->playEndVoice(); break;
    case 0x22:
        if (character->m_ground->floorFace != nullptr)
            character->playFallDownSound(character->m_ground->floorFace->surface);
        break;
    case 0x23: case 0x24: case 0x25: case 0x26:
        character->playJobSound(kind - 0x22);
        break;
    case 0x27:
        character->playMotionSound(def->shotText[m_soundEvent], character->m_xform.position);
        break;
    }
    m_soundEvent++;
}

// FS2 0x001e06f0
// callcheck: takeHits, vecDistance, getDeltaSeconds, getDeltaSeconds, isYriOrKne, getDeltaSeconds, getDeltaSeconds, playSwingSound, playShout, playIdleVoice, playDownVoice, playClashVoice, playBreathUp, playBreathDown, playJumpVoice, hasAlteredVoice, playAttackVoice, playHardDownVoice, playTrapVoice, playThrowVoice, playBiteVoice, playBittenVoice, playUntieVoice, playDamageVoice, playDieVoice, playGetUpVoice, playSurpriseVoice, playPanicVoice, playEndVoice, playFallDownSound, playJobSound, playMotionSound

void PlayerMotion::endFrame()
{
    StageCharacter *character = static_cast<StageCharacter *>(owner);
    takeHits();
    stepRunClock(character);
    if (character->isYriOrKne())
        stepAltStance(character);
    stepSoundEvents(character);
}

// FS2 0x00179af0

bool StageCharacter::isRunningMotion()
{
    const int id = m_motion->motionId;
    return isRunMotion(id) || isCrouchRunMotion(id);
}

// FS2 0x0020b220

void ScriptPoint::set(const sceVu0FVECTOR value)
{
    point[0] = value[0];
    point[1] = value[1];
    point[2] = value[2];
    point[3] = value[3];
    isSet = 1;
}

// FS2 0x00179ad0

void StageCharacter::setScriptPoint(const sceVu0FVECTOR point)
{
    m_scriptPoint.set(point);
}

// FS2 0x00210e60
// callcheck: getStageSelect, getStageSelect, getStageSelect, getStageSelect

u_int StageCharacter::getWieldSkills() const
{
    const StageObjRecord *sheet = stageObjRecord(m_itemDef);
    u_int skills = sheet->lightWeapon != 0 ? CharacterWieldSkill_Light : 0;
    if (sheet->heavyWeapon != 0)
        skills |= CharacterWieldSkill_Heavy;
    if (sheet->shortGun != 0)
        skills |= CharacterWieldSkill_ShortGun;
    if (sheet->rifle != 0)
        skills |= CharacterWieldSkill_Rifle;
    return skills;
}

// FS2 0x0020cc50
// callcheck: FUN_00210e60, FUN_00210e60

bool StageCharacter::canFireLongWeapon() const
{
    if (m_weapons[0] == nullptr)
        return false;
    const ItemDef *def = weaponDef(m_weapons[0]);
    if (def->isPistol())
        return (getWieldSkills() & CharacterWieldSkill_ShortGun) != 0;
    if (def->isRifle())
        return (getWieldSkills() & CharacterWieldSkill_Rifle) != 0;
    return false;
}

// The long weapon, then the short one.
// FS2 0x0021b280
// FS2 0x0021b1c0

int StageCharacter::getWeaponClass(int slot) const
{
    static const int kClassOfItemKind[] = { 1, 2, 4, 3, 5, 6, 7 };
    if (m_weapons[slot] == nullptr)
        return 0;
    const int kind = weaponDef(m_weapons[slot])->kind;
    return kind >= 0 && kind < (int)(sizeof(kClassOfItemKind) / sizeof(kClassOfItemKind[0])) ? kClassOfItemKind[kind] : 1;
}

// FS2 0x0021b120
int StageCharacter::getWeaponMagazine() const
{
    return m_weapons[0] != nullptr ? m_weapons[0]->m_ammoB : 0;
}

// FS2 0x0021b1a0
int StageCharacter::getWeaponRounds() const
{
    return m_weapons[0] != nullptr ? m_weapons[0]->m_ammoA : 0;
}

// FS2 0x0021b140
void StageCharacter::setWeaponRounds(int rounds)
{
    StageObject *weapon = m_weapons[0];
    if (weapon == nullptr)
        return;
    if (rounds < 0)
        rounds = 0;
    weapon->m_ammoA = rounds <= weapon->m_ammoB ? rounds : weapon->m_ammoB;
}

// FS2 0x0020cf90
// callcheck: strcmp

int StageCharacter::getCarriedAmmo()
{
    constexpr int kOthersAmmo = 100;
    if (m_weapons[0] == nullptr || !weaponDef(m_weapons[0])->isFirearm())
        return 0;
    if (gStageScene->player != this)
        return kOthersAmmo;
    const char *ammo = weaponSheet(m_weapons[0]).ammo;
    int rounds = 0;
    for (StageList<StageObject *>::Node *node = m_items.head; node != nullptr; node = node->next) {
        if (strcmp(node->value->m_name, ammo) == 0)
            rounds += node->value->m_count;
    }
    return rounds;
}

// FS2 0x0021ae80
// callcheck: isPlayer, findCarried, FUN_0021bb90

void StageCharacter::setCarriedAmmo(int rounds)
{
    if (rounds < 0 || !isPlayer())
        return;
    if (m_weapons[0] == nullptr || !weaponDef(m_weapons[0])->isFirearm())
        return;
    const char *ammo = weaponSheet(m_weapons[0]).ammo;
    StageObject *carried = findCarried(ammo);
    if (carried == nullptr) {
        takeNamedItems(ammo, rounds, 0);
        return;
    }
    StageItem *item = static_cast<StageItem *>(carried);
    item->m_count = rounds < item->m_maxCount ? rounds : item->m_maxCount;
}

// The stack's size, within what it may hold.
static int clampStack(int count, int most)
{
    if (count < 0)
        count = 0;
    return count < most ? count : most;
}

// The next free item (m_active clear) from slot on, null at the end.
static StageObject **nextFreeItem(StageObject **slot)
{
    while (slot != nullptr && (*slot)->m_active != 0)
        slot = StageObjArray::next(slot);
    return slot;
}

// A shout asked for: the frame's action takes it up, with its loudness.
// FS2 0x00216270

bool StageCharacter::tryShout(float loudness)
{
    if (m_allows.call == 0)
        return false;
    m_wants.call = 1;
    m_motion->m_shoutLoudness = loudness;
    return true;
}

// FS2 0x00219160
// callcheck: vecDistance, pathLength

float StageCharacter::measurePath(const float *from, const float *to)
{
    constexpr float kShortest = 15.0f;
    float length = 0.0f;
    if (vecDistance(from, to) >= kShortest) {
        // PC: the route's filter is a stub's null until game/characterroute.cpp has it.
        CharacterRoute *route = m_brain != nullptr ? m_brain->getRoute() : nullptr;
        if (route == nullptr || route->filter == nullptr)
            return -1.0f;
        length = route->filter->pathLength(from, to);
        if (length < 0.0f)
            return length;
    }
    m_pathLength = length;
    return length;
}

// FS2 0x00219200
// callcheck: stopIfMoving

bool StageCharacter::stopMoving()
{
    return m_brain->stopIfMoving();
}

// FS2 0x00219c50
// callcheck: strcmp (the name is the weapon's slot +0x21c)

bool StageCharacter::holdsWeapon(const char *name)
{
    return m_weapons[0] != nullptr && strcmp(m_weapons[0]->getObjectName(), name) == 0;
}

// ──────────────────────────────────────────────────────────────────────
// What the scripts ask a character (natives 0x4a.. in game/scriptrunner.cpp).

// How wide a character sees a foe, an item.
constexpr float kScriptSightCone = 1.0471976f;  // 0x3f860a92: 60 degrees

// FS2 0x0020b120
// callcheck: currentMission, getSides (the ROM answers the count; no caller reads it)

void StageCharacter::getEnemyCandidates(StageObject ***first, StageObject ***last)
{
    StageMission *mission = gStageScene->currentMission();
    if (m_foe != nullptr) {
        *first = mission->characters;
        *last = mission->characters + mission->characterCount;
        return;
    }
    const MissionObjectList &list = mission->byClass[getSides()];
    *first = list.objects;
    *last = list.objects + list.count;
}

// FS2 0x0021a090
// callcheck: getEnemyCandidates, isEnemyOf, getLifeState, distanceTo

void StageCharacter::pickNearestFoe()
{
    float nearest = FLT_MAX;
    StageObject *foe = nullptr;
    StageObject **it;
    StageObject **end;
    getEnemyCandidates(&it, &end);
    for (; it != end; ++it) {
        StageCharacter *other = static_cast<StageCharacter *>(*it);
        if (other->m_active == 0 || !isEnemyOf(other) || other->getLifeState() != LifeState_Up)
            continue;
        const float distance = distanceTo(other);
        if (distance < nearest) {
            nearest = distance;
            foe = other;
        }
    }
    m_foeCache[FoeCache_Nearest] = foe;
    m_nearestFoeDistance = foe != nullptr ? (int)nearest : -1;
}

// FS2 0x00219f50

StageObject *StageCharacter::getNearestFoe()
{
    if (m_foeCache[FoeCache_Nearest] == foeNotAsked())
        pickNearestFoe();
    return m_foeCache[FoeCache_Nearest];
}

// FS2 0x00219fa0

int StageCharacter::getNearestFoeDistance()
{
    if (m_foeCache[FoeCache_Nearest] == foeNotAsked())
        pickNearestFoe();
    return m_nearestFoeDistance;
}

// FS2 0x0021a380
// callcheck: getEnemyCandidates, getLifeState, isEnemyOf (getFoeWeight, inline in the ROM)

StageObject *StageCharacter::getBestFoe()
{
    if (m_foeCache[FoeCache_Best] != foeNotAsked())
        return m_foeCache[FoeCache_Best];
    StageObject *best = nullptr;
    int weight = 0;
    StageObject **it;
    StageObject **end;
    getEnemyCandidates(&it, &end);
    for (; it != end; ++it) {
        StageCharacter *other = static_cast<StageCharacter *>(*it);
        if (other->m_active == 0 || other->getLifeState() != LifeState_Up)
            continue;
        const int foeWeight = getFoeWeight(other);
        if (weight < foeWeight) {
            weight = foeWeight;
            best = other;
        }
    }
    m_foeCache[FoeCache_Best] = best;
    return best;
}

// FS2 0x0021a1c0
// callcheck: getEnemyCandidates, isEnemyOf, getLifeState, canSeeThisFrame, getFoeWeight

void StageCharacter::pickSeenEnemies()
{
    float nearestSeen = FLT_MAX;
    float nearestUnnoticed = FLT_MAX;
    StageObject *seen = nullptr;
    StageObject *unnoticed = nullptr;
    StageObject **it;
    StageObject **end;
    getEnemyCandidates(&it, &end);
    for (; it != end; ++it) {
        StageCharacter *other = static_cast<StageCharacter *>(*it);
        if (other->m_active == 0 || !isEnemyOf(other) || other->getLifeState() != LifeState_Up)
            continue;
        float distance;
        if (!canSeeThisFrame(other, &distance))
            continue;
        if (distance < nearestSeen) {
            nearestSeen = distance;
            seen = other;
        }
        if (distance < nearestUnnoticed && getFoeWeight(other) == 0) {
            nearestUnnoticed = distance;
            unnoticed = other;
        }
    }
    m_foeCache[FoeCache_Seen] = seen;
    m_foeCache[FoeCache_Unnoticed] = unnoticed;
}

// FS2 0x0021a040

int StageCharacter::getSeenEnemyId()
{
    if (m_foeCache[FoeCache_Seen] == foeNotAsked())
        pickSeenEnemies();
    return m_foeCache[FoeCache_Seen] != nullptr ? m_foeCache[FoeCache_Seen]->m_id : 0;
}

// FS2 0x00219ff0

StageObject *StageCharacter::getUnnoticedFoe()
{
    if (m_foeCache[FoeCache_Unnoticed] == foeNotAsked())
        pickSeenEnemies();
    return m_foeCache[FoeCache_Unnoticed];
}

// The ROM's is a tail call to the table's 0x2179c0 (SightEntry[0x18] and the
// owner at +0x120 are one object, +0x468), which also tests the line's address
// for null; it never is.
// FS2 0x00210930
// callcheck: FUN_002179c0, canSee (this, on the host)

bool StageCharacter::canSeeThisFrame(StageCharacter *other, float *distance)
{
    SightEntry *line = &m_sightTable[other->m_id - 1];
    *distance = line->distance;
    if (line->frame == SightEntry_NotSeen)
        return false;
    if (line->frame == SightEntry_Seen)
        return true;
    const bool seen = m_sightOwner->canSee(kScriptSightCone, other, distance);
    line->frame = seen ? SightEntry_Seen : SightEntry_NotSeen;
    line->distance = *distance;
    return seen;
}

// FS2 0x00217be0
// callcheck: isEnemyOf, getLifeState

int StageCharacter::getFoeWeight(StageCharacter *other)
{
    if (!m_sightOwner->isEnemyOf(other) || other->getLifeState() != LifeState_Up)
        return 0;
    return m_sightTable[other->m_id - 1].seen;
}

// FS2 0x00217c60

void StageCharacter::setFoeWeight(StageCharacter *other, int weight)
{
    m_sightTable[other->m_id - 1].seen = weight;
}

// FS2 0x0021a710
// callcheck: getEnemyCandidates, isEnemyOf, getLifeState, canSeeThisFrame, getFoeWeight, setFoeWeight

void StageCharacter::addFoeWeight(int amount, int limit, bool inView)
{
    StageObject **it;
    StageObject **end;
    getEnemyCandidates(&it, &end);
    for (; it != end; ++it) {
        StageCharacter *other = static_cast<StageCharacter *>(*it);
        if (other->m_active == 0 || !isEnemyOf(other) || other->getLifeState() != LifeState_Up)
            continue;
        float distance = 0.0f;
        if (canSeeThisFrame(other, &distance) != inView)
            continue;
        int weight = getFoeWeight(other) + amount;
        if (weight < 0)
            weight = 0;
        setFoeWeight(other, weight < limit ? weight : limit);
    }
}

// FS2 0x00219110
// callcheck: isPlayer

bool StageCharacter::askLift()
{
    if (!isPlayer() || m_allows.liftPartner == 0)
        return false;
    m_wants.liftPartner = 1;
    return true;
}

// FS2 0x0021ad60

bool StageCharacter::isBitten()
{
    constexpr int kMotionBittenBehind = 0xa0;
    constexpr int kMotionBittenFront  = 0xa1;
    return m_motion->motionId == kMotionBittenFront || m_motion->motionId == kMotionBittenBehind;
}

// The ROM's inline asks the life state again and has a second arm for
// anything but a corpse (m_reviveTime in frames); the first test leaves it
// dead here.
// FS2 0x002117e0
// callcheck: getLifeState, stageFps, getKind (the dead arm; isNear is inline)

bool StageCharacter::isCorpseGone()
{
    constexpr float kCorpseWatchRange = 250.0f;
    constexpr int   kCorpseWatchedFrames = 5;       // data 0x31aa28
    if (getLifeState() != LifeState_Corpse)
        return false;
    const StageCar *view = gStageScene->car;
    int frames = m_motion->damageDeadLink->reviveFrames;
    if (view != nullptr && view->isNear(kCorpseWatchRange, m_xform.position)) {
        const StageObject *floor = m_ground->floorObject;
        if ((floor == nullptr || const_cast<StageObject *>(floor)->getKind() != StageObject_Car)
            && frames <= kCorpseWatchedFrames)
            frames = kCorpseWatchedFrames;
    }
    return frames == 0;
}

// FS2 0x0021ab10

bool StageCharacter::takeCorpse()
{
    const bool gone = isCorpseGone();
    if (gone)
        m_motion->damageDeadLink->reviveFrames = -1;
    return gone;
}

// FS2 0x002199a0
// callcheck: currentMission, currentMission, isCorpseGone, distanceTo

StageCharacter *StageCharacter::findNearestCorpse()
{
    float nearest = FLT_MAX;
    StageCharacter *corpse = nullptr;
    StageMission *mission = gStageScene->currentMission();
    for (int i = 0; i < mission->characterCount; i++) {
        StageCharacter *other = static_cast<StageCharacter *>(mission->characters[i]);
        if (other->m_active == 0 || !other->isCorpseGone())
            continue;
        const float distance = distanceTo(other);
        if (distance < nearest) {
            nearest = distance;
            corpse = other;
        }
    }
    return corpse;
}

// FS2 0x00212420

bool StageCharacter::isNearViewTarget()
{
    constexpr float kCorpseWatchRange = 250.0f;
    return gStageScene->car != nullptr
        && gStageScene->car->isNear(kCorpseWatchRange, m_xform.position);
}

// One item of list, standing free, in use, in the character's sight. The ROM
// has a copy per list.
static bool seesItemInUse(StageCharacter *who, const MissionObjectList &list)
{
    for (int i = 0; i < list.count; i++) {
        StageItem *item = static_cast<StageItem *>(list.objects[i]);
        if (item->m_active == 0 || item->m_parent != nullptr || item->m_inUse == 0)
            continue;
        float distance = 0.0f;
        if (who->canSee(kScriptSightCone, item, &distance))
            return true;
    }
    return false;
}

// FS2 0x00218e80
// callcheck: currentMission, canSee

bool StageCharacter::seesBurner()
{
    return seesItemInUse(this, gStageScene->currentMission()->burners);
}

// FS2 0x00218f40
// callcheck: currentMission, canSee

bool StageCharacter::seesOpenTrap()
{
    return seesItemInUse(this, gStageScene->currentMission()->trapItems);
}

// FS2 0x002182c0
// callcheck: isMode1, isOwned0Busy, isLampOn, ownsLight

int StageCharacter::getLightReach()
{
    enum { kReachNone = 0, kReachOut = 1, kReachIn = 2 };
    constexpr int kLightOwned = 2;      // StageLight::m_mode, read here only
    constexpr int kLightCar   = 3;
    const StageLight *light = m_shadowLight;
    if (light == nullptr || light->m_yamLight != 0)
        return kReachNone;
    StageObject *object = gStageScene->controlled != nullptr ? gStageScene->controlled : gStageScene->player;
    if (object == nullptr)
        return kReachOut;
    StageCharacter *player = static_cast<StageCharacter *>(object);
    switch (light->getMode()) {
    case kLightCar:
        return player->isMode1() ? kReachIn : kReachOut;
    case kLightOwned:
        return gStageScene->isOwned0Busy() ? kReachIn : kReachOut;
    case StageLight_Hard:
        return player->isLampOn() && player->m_lampItem->ownsLight(m_shadowLight) ? kReachIn : kReachOut;
    default:
        return kReachOut;
    }
}

// FS2 0x0021b340
// callcheck: dropItem

bool StageCharacter::dropLongWeapon()
{
    if (m_weapons[0] == nullptr)
        return false;
    dropItem(m_weapons[0]);
    return true;
}

// FS2 0x002a3100
// callcheck: getPickSound, getPickSound, playNamedHeard2 (the ROM passes 800.0 too; the sound's own loudness counts)

void StageCharacter::playWeaponPickSound()
{
    StageWeaponItem *weapon = static_cast<StageWeaponItem *>(m_weapons[0]);
    if (weapon != nullptr && weapon->getPickSound() != nullptr)
        playNamedHeard2(weapon->getPickSound(), m_xform.position, nullptr);
}

// FS2 0x0021ada0 (thunk 0x0017e730)
// callcheck: askAttackBite

bool StageCharacter::scriptAttackBite()
{
    return m_brain->askAttackBite();
}

// A pistol or a rifle takes all it holds, the bolted three the rounds asked.
// FS2 0x0021b450 (thunk 0x0017e6a0)
// callcheck: isControlled, askGunAction, strcmp, strcmp, strcmp

bool StageCharacter::scriptReload(int rounds)
{
    if (m_allows.reload == 0)
        return false;
    if (!isControlled())
        return m_brain->askGunAction();
    StageObject *weapon = m_weapons[0];
    if (weapon == nullptr || !weaponDef(weapon)->isFirearm())
        return false;
    const bool bolted = strcmp(weapon->m_name, "G_NN38") == 0
        || strcmp(weapon->m_name, "R_MURATA") == 0
        || strcmp(weapon->m_name, "G_26NEN") == 0;
    if (bolted && rounds < 1)
        return false;
    m_wants.reload = 1;
    m_wants.reloadRounds = bolted ? rounds : weapon->m_ammoB;
    return true;
}

// The two heads, as a blow's check; what it carries does not block.
// FS2 0x0021adc0 (thunk 0x0017e690)
// callcheck: find, getMatrix, find, getMatrix, segmentHitsAny

bool StageCharacter::scriptHasLineTo(StageCharacter *other)
{
    static const char kHead[] = "bn_head";      /* data 0x303178 */
    constexpr u_int kFaceLine     = 0x1e42600f;
    constexpr u_int kFaceLineSkip = 0x800;
    StageObject *skip = m_gear != nullptr ? m_gear : other->m_gear;
    const sceVu0FVECTOR *theirs = other->m_model->boneMap.find(kHead)->value->getMatrix();
    const sceVu0FVECTOR *mine = m_model->boneMap.find(kHead)->value->getMatrix();
    return !gStageScene->segmentHitsAny(mine[3], theirs[3], 0, kFaceLine, kFaceLineSkip, skip);
}

// Only from a stand; else the native waits a frame and asks again.
// FS2 0x002183d0 (thunk 0x0017e5e0)
// callcheck: scriptYield, refresh, playWeaponPickSound

void StageCharacter::scriptPutWeaponAway()
{
    constexpr int kNativePutAway = 0x80;
    if (gRunningScript != nullptr)
        gRunningScript->takeYield();
    if (m_allows.standing == 0) {
        scriptYield(kNativePutAway, 0);
        return;
    }
    m_wants.putAway = 1;
    m_motion->refresh();
    playWeaponPickSound();
}

// FS2 0x00218e60 (thunk 0x0017e5a0): slot +0x2f8.
// callcheck: scriptUseNamedItem

void StageCharacter::scriptUseSmoke()
{
    static const char kSmoke[] = "I_HATUEN";    /* data 0x307010 */
    scriptUseNamedItem(kSmoke);
}

// FS2 0x00218e10 (thunk 0x0017e580)
// callcheck: useItem

void StageCharacter::scriptUseNamedItem(const char *name)
{
    sceVu0FVECTOR zero = {};
    useItem(name, zero, 1);
}

// FS2 0x00218b00 (thunk 0x0017e570)
// callcheck: isControlled, askSetTrap

bool StageCharacter::scriptSetTrap()
{
    if (!isControlled())
        return m_brain->askSetTrap();
    if (m_allows.trapSet == 0)
        return false;
    m_wants.trapSet = 1;
    return true;
}

// FS2 0x00218af0 (thunk 0x0017e560)
// callcheck: item218850

void StageCharacter::scriptPutItem()
{
    item218850(1);
}

// FS2 0x00218800 (thunk 0x0017e550)
// callcheck: isControlled, askTakeUpTrap

bool StageCharacter::scriptTakeUpTrap()
{
    if (!isControlled())
        return m_brain->askTakeUpTrap();
    if (m_allows.trapCancel == 0)
        return false;
    m_wants.trapCancel = 1;
    return true;
}

// FS2 0x0021b370 (thunk 0x0017e510)
// callcheck: getKind, isControlled, askTakeItem

StageObject *StageCharacter::scriptTakeItem()
{
    if (m_allows.pickup == 0)
        return nullptr;
    StageObject *item = m_pickItem;
    if (item->getKind() != StageObject_WeaponItem)
        return nullptr;
    if (!isControlled())
        return m_brain->askTakeItem() ? item : nullptr;
    m_wants.pickup = 1;
    return item;
}

// The four script kinds a character takes, into the brain's slots.
// FS2 0x0021b970: the slot's thunk is 0x0017e7c0.
// callcheck: setSlotEntry, setSlotEntry, setSlotEntry, setSlotEntry

void StageCharacter::scriptSetBrainEntry(int kind, const ScriptEntry *entry)
{
    constexpr int kNoId = -1;
    // PC: the ROM has no test.
    if (m_brain == nullptr)
        return;
    switch (kind) {
    case 6: m_brain->setSlotEntry(2, entry, kNoId); break;
    case 5: m_brain->setSlotEntry(6, entry, kNoId); break;
    case 2: m_brain->setSlotEntry(4, entry, kNoId); break;
    case 1: m_brain->setSlotEntry(0, entry, kNoId); break;
    }
}

// FS2 0x0021b890: the slot's thunk is 0x0017e7a0.
// callcheck: getKind, getKind, isPlayer, isEnemyOf, isAlly

int StageCharacter::scriptHeardKind()
{
    enum { Heard_None, Heard_Ally, Heard_Other, Heard_Enemy, Heard_Player };
    if (m_heardFrom == nullptr)
        return Heard_None;
    if (m_heardFrom->getKind() == StageObject_Character) {
        if (m_heardFrom->isPlayer())
            return Heard_Player;
        if (isEnemyOf(m_heardFrom))
            return Heard_Enemy;
        if (isAlly(m_heardFrom))
            return Heard_Ally;
    }
    return Heard_Other;
}

// FS2 0x00179bb0: the slot's thunk is 0x0017e660.
// callcheck: isCrouchMotion (the ROM's own test, the same set)

bool StageCharacter::scriptIsCrouching()
{
    return isCrouchMotion(m_motion->motionId);
}

// FS2 0x00219d70
// callcheck: distanceTo, atan2Octant, wrapAngle, currentMission, currentMission, sinCos, sinCos, atan2Octant, wrapAngle

bool StageCharacter::isInReachOf(StageObject *object)
{
    constexpr float kReach = 180.0f;
    constexpr float kBelow = -20.0f;
    constexpr float kAhead = 10.0f;
    constexpr float kQuarterTurn = 1.5707964f;
    constexpr float kFacing = 0.5235988f;       // 30 degrees

    const float *at = object->m_xform.position;
    if (distanceTo(object) > kReach)
        return false;
    const float rise = at[2] - m_xform.position[2];
    if (rise > kReach || rise < kBelow)
        return false;

    const float x = m_xform.position[0];
    const float y = m_xform.position[1];
    const float yaw = m_xform.rotation[2];
    const float bearing = wrapAngle(yaw + atan2Octant(x - at[0], y - at[1]));
    if (bearing <= -kQuarterTurn || bearing >= kQuarterTurn)
        return false;

    const float aheadX = sinCos(-yaw, 0) * kAhead + x;
    const float aheadY = sinCos(-yaw, 1) * kAhead + y;
    const float facing = wrapAngle(yaw + atan2Octant(aheadX - at[0], aheadY - at[1]));
    return facing > -kFacing && facing < kFacing;
}

// FS2 0x002a6430
// callcheck: isCuePlaying

bool StageCharacter::isVoicePlaying(const char *name)
{
    return gStageScene->sound->isCuePlaying(name, this);
}

// FS2 0x002a6440
// callcheck: findCue, getTopView, playCue, getTopView, pickCueChannel, stopCue, findSource, stopSource, setCue

bool StageCharacter::playVoice(const char *name)
{
    enum { Sound_None = 0, Sound_Source = 1, Sound_Cue = 2 };   // m_soundState
    StageSound *sound = gStageScene->sound;
    SeEntry *cue = sound->findCue(name);
    if (cue == nullptr)
        return false;
    if (cue->priority == 0)
        return sound->playCue(cue, this, gStageScene->getTopView());
    if (m_soundPriority < cue->priority)
        return false;
    const int channel = sound->pickCueChannel(cue, this, gStageScene->getTopView());
    if (channel < 0)
        return false;

    if (m_soundState != Sound_None) {
        if (m_soundState == Sound_Cue) {
            sound->stopCue(this, (const SeEntry *)m_soundHandle);
        } else if (m_soundState == Sound_Source) {
            StageSoundSource *source = sound->findSource((StageSoundSource *)m_soundHandle);
            if (source != nullptr)
                sound->stopSource(source);
        }
        m_soundState = Sound_None;
        m_soundPriority = 0x7fffffff;
        m_soundHandle = nullptr;
    }

    if (!sound->setCue(cue, this, channel))
        return false;
    m_soundPriority = cue->priority;
    m_soundState = Sound_Cue;
    m_soundHandle = cue;
    return true;
}

// Unlike playVoice this is an effect, not a stream: the name is looked up in
// the stage's SE table (inline in the ROM), where the unprefixed names are.
// The volume 800 is playNamedHeard2's.
// FS2 0x002a65c0
// callcheck: strcmp, BoneMap::find, getMatrix, playNamedHeard2, stopCue, findSource, stopSource, getMatrix, playNamedHeard2 (the inline SeTable::find)

void StageCharacter::playVoiceHeard(const char *name)
{
    enum { Sound_None = 0, Sound_Source = 1, Sound_Cue = 2 };   // m_soundState
    static const char kHead[] = "bn_head";  /* data 0x303178 */
    StageSound *sound = gStageScene->sound;
    SeTable *table = sound->m_player->m_table;
    const SeEntry *cue = table != nullptr ? table->find(name) : nullptr;
    if (cue == nullptr)
        return;
    if (cue->priority == 0) {
        playNamedHeard2(name, m_model->boneMap.find(kHead)->value->getMatrix()[3], nullptr);
        return;
    }
    if (m_soundPriority < cue->priority)
        return;
    if (m_soundState != Sound_None) {
        if (m_soundState == Sound_Cue) {
            sound->stopCue(this, (const SeEntry *)m_soundHandle);
        } else if (m_soundState == Sound_Source) {
            StageSoundSource *source = sound->findSource((StageSoundSource *)m_soundHandle);
            if (source != nullptr)
                sound->stopSource(source);
        }
        m_soundState = Sound_None;
        m_soundPriority = 0x7fffffff;
        m_soundHandle = nullptr;
    }
    StageSoundSource *source = playNamedHeard2(name, m_model->boneMap.find(kHead)->value->getMatrix()[3], nullptr);
    if (source != nullptr) {
        m_soundPriority = cue->priority;
        m_soundState = Sound_Source;
        m_soundHandle = source;
    }
}

// FS2 0x0021bb90
// callcheck: findAll3, getKind, dropItem, setActive, setShown, giveItem, setActive, setShown, setActive, setShown, giveItem, setActive, setShown

int StageCharacter::takeNamedItems(const char *name, int count, u_int mode)
{
    mode &= 0xf;
    if (count < 1)
        return 0;
    StageObjArray *items = gStageScene->objects->findAll3(name);
    if (items == nullptr)
        return 0;
    int taken = 0;
    for (StageObject **slot = nextFreeItem(items->begin()); slot != nullptr;
         slot = nextFreeItem(StageObjArray::next(slot))) {
        StageItem *item = static_cast<StageItem *>(*slot);
        if (item->m_parent != nullptr)
            continue;
        if (item->getKind() == kStageObjectKindWeapon) {
            StageObject *held = m_weapons[0];
            if (held != nullptr && m_weapons[1] != nullptr) {
                dropItem(held);
                held->m_tree->setActive(mode == 2);
                held->m_tree->setShown(mode == 2);
            }
            if (giveItem(item)) {
                item->m_tree->setActive(true);
                item->m_tree->setShown(true);
            }
            return 1;
        }
        if (mode != 0)
            return 0;
        const int had = item->m_count;
        const int kind = gItemDefs[item->m_itemDef < 0 || item->m_itemDef >= kItemDefCount ? 0 : item->m_itemDef].kind;
        if (kind >= ItemKind_ItemFirst && kind <= ItemKind_StackLast)
            item->m_count = clampStack(count, item->m_maxCount);
        item->m_tree->setActive(true);
        item->m_tree->setShown(true);
        if (giveItem(item)) {
            taken += item->m_count;
            if (count <= taken)
                return taken;
        }
        else {
            item->m_count = clampStack(had, item->m_maxCount);
            item->m_tree->setActive(false);
            item->m_tree->setShown(false);
        }
    }
    return taken;
}

// FS2 0x0021bf00
int StageCharacter::getScriptValue(int index) const
{
    return index < 0 || index >= kScriptValues ? 0 : m_scriptValues[index];
}

// FS2 0x0021bf40
void StageCharacter::setScriptValue(int index, int value)
{
    if (index >= 0 && index < kScriptValues)
        m_scriptValues[index] = value;
}

// FS2 0x00218500
// callcheck: setPosition, updateMatrix, reset, reset, isViewLinked, getTopView, onViewTop, stopAndRetry
void StageCharacter::setPosition(const float *pos)
{
    m_xform.setPosition(pos);
    updateMatrix();
    m_motion->reset(m_placeMotion);
    m_placeMotion = 0;
    m_ground->reset();
    if (isViewLinked())
        gStageScene->getTopView()->onViewTop();
    if (m_brain != nullptr)
        m_brain->stopAndRetry();
    m_placedThisFrame = 1;
}

// FS2 0x00219010
// callcheck: resetScripts
void StageCharacter::onScriptExit()
{
    if (m_brain != nullptr)
        m_brain->resetScripts();
}

// FS2 0x0021ac60
// callcheck: isControlled, isSpecialAllowed, setSpecial, playActionAndRetry
void StageCharacter::scriptPlayMotion(const char *name)
{
    if (!isControlled()) {
        // PC: the ROM calls through a null brain here.
        if (m_brain != nullptr)
            m_brain->playActionAndRetry(name);
        return;
    }
    if (isSpecialAllowed(name))
        setSpecial(true, name);
}

// FS2 0x0021b3f0
// callcheck: isControlled, tryShout, askShout
bool StageCharacter::shout(float loudness)
{
    if (isControlled())
        return tryShout(loudness);
    return m_brain->askShout(loudness);
}

// FS2 0x0021bf90
// callcheck: walkToAndRetry
void StageCharacter::walkToMarkedPlace(bool run)
{
    if (m_point700.isSet)
        m_brain->walkToAndRetry(m_point700.point, run);
}

// FS2 0x0021bf80
void StageCharacter::forgetMarkedPlace()
{
    m_point700.isSet = 0;
}

// FS2 0x00219be0
// callcheck: scriptYield
void StageCharacter::waitMotionChange()
{
    scriptYieldWhile(m_motion->motionId);
}

// FS2 0x0021abc0
// callcheck: isSwitchedOn (slot +0x26c of the lamp)
bool StageCharacter::isLampOn()
{
    return m_lampItem != nullptr && m_lampItem->isSwitchedOn();
}

// FS2 0x0021ac00
// callcheck: setSwitchedOn (slot +0x268 of the lamp)
void StageCharacter::setLampOn(bool on)
{
    if (m_lampItem != nullptr)
        m_lampItem->setSwitchedOn(on);
}

// 999 in the sheet's 射撃: blanks; the same number is the scatter in hundredths.
// FS2 0x0021cc10
void StageCharacter::setShot(int value)
{
    m_blanks = value == kShotBlanks;
    m_shotScatter = (float)value / kPercent;
}

static int clampToSheet(int value, int limit)
{
    if (value < 0)
        value = 0;
    return value <= limit ? value : limit;
}

// FS2 0x0021cc50

void StageCharacter::setLightResist(int value)
{
    if (value >= 0 && value < kLightResistLevels)
        m_lightResist = value;
}

// The ROM repeats StageObject::setProperty's cases here; they fall to it.
// FS2 0x00212530
// callcheck: getStageSelect, getSaveData, clearBit, markUnlock468, pickVariant3ac, setEndurance, setBrainState, setResultValue, setScriptVar3e, setScriptVar3f, findCharacterById, setPlayer, currentMission, setResultPaused, setResultHold, setClockFrames, setScoreClock, getLifeState, scriptFramesFrom30, setLightResist, setSideBit1, setSideBit2, setSideBit4, setSideBit8 (StageObject::setProperty holds most; setSideBit is one function)

void StageCharacter::setProperty(int id, int sub, int value)
{
    const StageObjRecord *sheet = stageObjRecord(m_itemDef);
    switch (id) {
    case CharProp_Sides:         setSides(value); break;
    case CharProp_BrainState:    setBrainState(value, 1); break;
    case CharProp_AiLevel:       m_aiLevel = value; break;
    case CharProp_Sight:         m_sight = (float)value; break;
    case CharProp_Hearing:       m_hearing = (float)value / kPercent; break;
    case CharProp_Health: {
        const float health = value > 0 ? (float)value : 0.0f;
        m_health = health <= (float)sheet->health ? health : (float)sheet->health;
        break;
    }
    case CharProp_Endurance:     setEndurance(value); break;
    case CharProp_LeftArm:       m_leftArm = clampToSheet(value, sheet->leftArm); break;
    case CharProp_RightArm:      setRightArm(value); break;
    case CharProp_RightLeg:      m_rightLeg = clampToSheet(value, sheet->rightLeg); break;
    case CharProp_LeftLeg:       m_leftLeg = clampToSheet(value, sheet->leftLeg); break;
    case CharProp_Recovery:      m_enduranceRegen = value; break;
    case CharProp_LightResist:   setLightResist(value); break;
    case CharProp_LightDamage:   m_lightDamage = value; break;
    case CharProp_LightFlinch:   m_lightFlinch = value; break;
    case CharProp_UnarmedAttack: m_unarmedAttack = value; break;
    case CharProp_Bite:          m_biteDamage = value; break;
    case CharProp_WalkRate:      m_walkRate = (float)value / kPercent; break;
    case CharProp_RunRate:       m_runRate = (float)value / kPercent; break;
    case CharProp_TurnRate:      m_turnRate = (float)value / kPercent; break;
    case CharProp_ReviveTime:    m_reviveTime = (float)value / kThirtieths; break;
    case CharProp_ControlResist: m_drainRate = value; break;
    case CharProp_TrapResist:    m_trapResist = (float)value / kThirtieths; break;
    case CharProp_Shot:          setShot(value); break;
    case CharProp_ShotInterval:  m_shotInterval = (float)value / kThirtieths; break;
    case CharProp_AttackInterval: m_attackInterval = (float)value / kThirtieths; break;
    case CharProp_BiteInterval:  m_biteInterval = (float)value / kThirtieths; break;
    case CharProp_ReloadRate:    m_reloadRate = (float)value / kPercent; break;
    case CharProp_WeaponShown:
        if (m_weapons[0] != nullptr)
            m_weapons[0]->m_model->data->shown = 1;
        break;
    case CharProp_WeaponRounds:  setWeaponRounds(value); break;
    case CharProp_CarriedAmmo:   setCarriedAmmo(value); break;
    case CharProp_Value5b4:      m_5b4 = value; break;
    case CharProp_Flag5b8:       m_shrugsHits = value != 0; break;
    case CharProp_BiteProof:     m_biteProof = value != 0; break;
    case CharProp_Flag5ba:       m_5ba = value != 0; break;
    case CharProp_Value62c:
        if (value >= 0 && value < kValue62cLevels)
            m_62c = value;
        break;
    case CharProp_SidePlayer:    setSideBit(StageObjSide_Player, value != 0); break;
    case CharProp_SideNpc:       setSideBit(StageObjSide_Npc, value != 0); break;
    case CharProp_SideCorpse:    setSideBit(StageObjSide_Corpse, value != 0); break;
    case CharProp_SideDark:      setSideBit(StageObjSide_Dark, value != 0); break;
    case CharProp_ScriptValue:   setScriptValue(sub, value); break;
    case CharProp_Vector0:       m_scriptPoint.point[0] = (float)value; break;
    case CharProp_Vector1:       m_scriptPoint.point[1] = (float)value; break;
    case CharProp_Vector2:       m_scriptPoint.point[2] = (float)value; break;
    case CharProp_ReviveFrames:
        if (getLifeState() == kLifeDownInMotion)
            m_motion->damageDeadLink->reviveFrames = scriptFramesFrom30(value);
        break;
    case Prop_SwitchedOn:        setLampOn(value != 0); break;
    case CharProp_Flag3d8:       m_dropsWeapon = value != 0; break;
    case CharProp_Fidgets:       m_fidgets = value != 0; break;
    case CharProp_ShotScale:     m_shotScale = value; break;
    case CharProp_MeleeScale:    m_meleeScale = value; break;
    case CharProp_Value6c8:      m_6c8 = value; break;
    case CharProp_Value6cc:      m_6cc = value; break;
    case CharProp_Value6d0:      m_6d0 = value; break;
    case CharProp_Value6d4:      m_6d4 = value; break;
    case CharProp_Flag44c:       m_voiceOn = value != 0; break;
    default:
        StageObject::setProperty(id, sub, value);
        break;
    }
}

// What the weapon's reach is: a firearm the sheet lets it fire, a blow's, or bare hands.
static int attackReachOf(StageCharacter *character)
{
    if (character->m_weapons[0] == nullptr)
        return stageObjRecord(character->m_itemDef)->attackCheck;
    const WeaponDef &sheet = weaponSheet(character->m_weapons[0]);
    return character->canFireLongWeapon() ? sheet.range : sheet.param2;
}

// The frames to its revival, by the DamageDead link's count: down in the
// motion, and near the view target on anything but a car, at least 5.
static int reviveFramesLeft(StageCharacter *character)
{
    constexpr float kNearViewTarget = 250.0f;
    constexpr int   kMinFramesNear  = 5;    /* sdata 0x31aa28 */
    StageScene *scene = gStageScene;
    if (character->getLifeState() != kLifeDownInMotion)
        return scriptFramesTo30((int)(character->m_reviveTime * (float)stageFps()));
    const int frames = character->m_motion->damageDeadLink->reviveFrames;
    const bool near = scene->car != nullptr
                   && scene->car->isNear(kNearViewTarget, character->m_xform.position);
    StageObject *floor = character->m_ground->floorObject;
    if (!near || (floor != nullptr && floor->getKind() == StageObject_Car))
        return scriptFramesTo30(frames);
    return scriptFramesTo30(frames > kMinFramesNear ? frames : kMinFramesNear);
}

// Who it rides with: the party's companion, a character, else itself; 0 when
// it does not ride. The ROM asks getAltSoundFlag's test three times.
static int ridingWithId(StageCharacter *character)
{
    if (character->getAltSoundFlag() == 0)
        return 0;
    StageObject *companion = gStageScene->party->getCompanion();
    if (companion == nullptr || companion->getKind() != StageObject_Character)
        return character->m_id;
    return companion->m_id;
}

// The ROM repeats StageObject::getProperty's cases here; they fall to it.
// FS2 0x002132d0
// callcheck: getSaveData, clearBit, testStageFlags, random, getMissionFrames, stageFps, FUN_00210e60, FUN_00210e50, FUN_0021b280, getLongWeaponClass, FUN_0021b1c0, FUN_0020cc50, getLifeState, FUN_00289eb0, scriptFramesTo30, FUN_0021b1a0, FUN_0021b120, FUN_0020cf90, FUN_0021abb0, isRunning, getCompanion, canFireLongWeapon, currentMission, getClockFrames, getDeltaSeconds, getHandle, getResultHold, getResultPaused, getResultValue, getScoreClock, getScriptVar3e, getScriptVar3f, getSeconds, getShortWeaponClass, getStageSelect, isNear, onRestore (StageObject::getProperty holds the shared ones; ridingWithId and reviveFramesLeft are the ROM's inline code)

int StageCharacter::getProperty(int id, int sub)
{
    const StageObjRecord *sheet = stageObjRecord(m_itemDef);
    switch (id) {
    case CharProp_Id:            return m_id;
    case CharProp_SheetKind:     return sheet->kind;
    case CharProp_SheetSeType:   return sheet->seType;
    case CharProp_WieldSkills:   return getWieldSkills();
    case CharProp_WieldLight:    return (getWieldSkills() & CharacterWieldSkill_Light) != 0;
    case CharProp_WieldHeavy:    return (getWieldSkills() & CharacterWieldSkill_Heavy) != 0;
    case CharProp_WieldShortGun: return (getWieldSkills() & CharacterWieldSkill_ShortGun) != 0;
    case CharProp_WieldRifle:    return (getWieldSkills() & CharacterWieldSkill_Rifle) != 0;
    case CharProp_Sides:         return getSides();
    case CharProp_SidePlayer:    return (getSides() & StageObjSide_Player) != 0;
    case CharProp_SideNpc:       return (getSides() & StageObjSide_Npc) != 0;
    case CharProp_SideCorpse:    return (getSides() & StageObjSide_Corpse) != 0;
    case CharProp_SideDark:      return (getSides() & StageObjSide_Dark) != 0;
    case CharProp_BrainState:    return getBrainPace();
    case CharProp_SheetAiType:   return sheet->aiType;
    case CharProp_AiLevel:       return m_aiLevel;
    case CharProp_LongWeaponKind:  return getWeaponClass(0);
    case CharProp_ShortWeaponKind: return getWeaponClass(1);
    case CharProp_Sight:         return (int)m_sight;
    case CharProp_Hearing:       return (int)(m_hearing * kPercent);
    case CharProp_Health:        return (int)m_health;
    case CharProp_HealthMax:     return sheet->health;
    case CharProp_Endurance:     return m_endurance;
    case CharProp_EnduranceMax:  return sheet->endurance;
    case CharProp_LeftArm:       return m_leftArm;
    case CharProp_LeftArmMax:    return sheet->leftArm;
    case CharProp_RightArm:      return m_rightArm;
    case CharProp_RightArmMax:   return sheet->rightArm;
    case CharProp_RightLeg:      return m_rightLeg;
    case CharProp_RightLegMax:   return sheet->rightLeg;
    case CharProp_LeftLeg:       return m_leftLeg;
    case CharProp_LeftLegMax:    return sheet->leftLeg;
    case CharProp_Recovery:      return m_enduranceRegen;
    case CharProp_LightResist:   return m_lightResist;
    case CharProp_LightDamage:   return m_lightDamage;
    case CharProp_LightFlinch:   return m_lightFlinch;
    case CharProp_UnarmedAttack: return m_unarmedAttack;
    case CharProp_Bite:          return m_biteDamage;
    case CharProp_AttackReach:   return attackReachOf(this);
    case CharProp_WalkRate:      return (int)(m_walkRate * kPercent);
    case CharProp_RunRate:       return (int)(m_runRate * kPercent);
    case CharProp_TurnRate:      return (int)(m_turnRate * kPercent);
    case CharProp_ReviveTime:    return (int)(m_reviveTime * kThirtieths);
    case CharProp_ControlResist: return m_drainRate;
    case CharProp_TrapResist:    return (int)(m_trapResist * kThirtieths);
    case CharProp_Shot:          return m_blanks != 0 ? kShotBlanks : (int)(m_shotScatter * kPercent);
    case CharProp_ShotInterval:  return (int)(m_shotInterval * kThirtieths);
    case CharProp_AttackInterval: return (int)(m_attackInterval * kThirtieths);
    case CharProp_BiteInterval:  return (int)(m_biteInterval * kThirtieths);
    case CharProp_ReloadRate:    return (int)(m_reloadRate * kPercent);
    case CharProp_SheetStepLevel: return sheet->stepLevel;
    case CharProp_SheetLadder:   return sheet->ladder;
    case CharProp_WeaponShown:
        return m_weapons[0] != nullptr && m_weapons[0]->m_model->data->shown != 0;
    case CharProp_WeaponRounds:  return getWeaponRounds();
    case CharProp_WeaponMagazine: return getWeaponMagazine();
    case CharProp_CarriedAmmo:   return getCarriedAmmo();
    case CharProp_BiteTargetId:
        return getBiteTarget() != nullptr ? getBiteTarget()->m_id : 0;
    case CharProp_RidingWithId:  return ridingWithId(this);
    case CharProp_Value5b4:      return m_5b4;
    case CharProp_Flag5b8:       return m_shrugsHits;
    case CharProp_BiteProof:     return m_biteProof;
    case CharProp_Flag5ba:       return m_5ba;
    case CharProp_Value62c:      return m_62c;
    case CharProp_ScriptValue:   return getScriptValue(sub);
    case CharProp_Vector0:       return (int)m_scriptPoint.point[0];
    case CharProp_Vector1:       return (int)m_scriptPoint.point[1];
    case CharProp_Vector2:       return (int)m_scriptPoint.point[2];
    case CharProp_ReviveFrames:  return reviveFramesLeft(this);
    case Prop_SwitchedOn:        return isLampOn();
    case CharProp_Value670:      return m_670;
    case CharProp_Flag3d8:       return m_dropsWeapon;
    case CharProp_Answer3a:
    case CharProp_Answer3b:      return 3;
    case CharProp_SheetDecoyResist: return sheet->decoyResist;
    case CharProp_Fidgets:       return m_fidgets;
    case CharProp_ShotScale:     return m_shotScale;
    case CharProp_MeleeScale:    return m_meleeScale;
    case CharProp_Value6c8:      return m_6c8;
    case CharProp_Value6cc:      return m_6cc;
    case CharProp_Value6d0:      return m_6d0;
    case CharProp_Value6d4:      return m_6d4;
    case CharProp_MotionFlagged: {
        constexpr int kMotionFlaggedA = 0x42;
        constexpr int kMotionFlaggedB = 0x94;
        constexpr int kMotionFlaggedC = 0x95;
        const int motion = m_motion->motionId;
        return m_motion->m_bfc != 0
            && (motion == kMotionFlaggedA || motion == kMotionFlaggedB || motion == kMotionFlaggedC);
    }
    case CharProp_Flag44c:       return m_voiceOn;
    default:
        return StageObject::getProperty(id, sub);
    }
}

// What scripts ask of a character beside the frame's picks (natives 0x2d..0x214).

// FS2 0x0020ca80 (thunk 0x0017e6c0)
// callcheck: getStageSelect, canFireLongWeapon (getAttackReach, weaponSheet)

bool StageCharacter::scriptIsFacing(StageObject *other)
{
    float reach = getAttackReach();
    if (canFireLongWeapon()) {
        const float bullet = m_weapons[0] != nullptr ? (float)weaponSheet(m_weapons[0]).range : 0.0f;
        if (bullet < reach)
            reach = bullet;
    }
    return distanceTo(other) < reach;
}

// FS2 0x00210910 (thunk 0x0017e790)
// callcheck: canSee

bool StageCharacter::scriptCanSee(float viewAngle, StageObject *other)
{
    float distance = 0.0f;
    return canSee(viewAngle, other, &distance);
}

// FS2 0x0021ad20 (thunk 0x0017e710)
// callcheck: askSwingAt

bool StageCharacter::scriptSwingAt(StageObject *target)
{
    // PC: the ROM has no test of the brain.
    return m_brain != nullptr && m_brain->askSwingAt(target);
}

// FS2 0x0021ace0 (thunk 0x0017e6f0)
// callcheck: askFireAt

bool StageCharacter::scriptFireAt(StageObject *target, const char *flags, int frames30)
{
    return m_brain != nullptr && m_brain->askFireAt(target, flags, frames30);
}

// FS2 0x00218470 (thunk 0x0017e640)
// callcheck: askRouteAction12c

int StageCharacter::scriptRouteAction12c()
{
    return m_brain != nullptr && m_brain->askRouteAction12c();
}

// FS2 0x00218450 (thunk 0x0017e630)
// callcheck: askRouteAction130

int StageCharacter::scriptRouteAction130()
{
    return m_brain != nullptr && m_brain->askRouteAction130();
}

// FS2 0x002185a0
// callcheck: missionSpawnPointTaken

bool StageCharacter::canSpawn()
{
    int point = m_spawnPoint;
    return !missionSpawnPointTaken(&point) || m_respawnPosition[3] != 0.0f;
}

// FS2 0x00218630
// callcheck: findEntry, setEntry, missionSpawnPointTaken, currentMission, setPosition, currentMission, setRotationVec, setPosition, setRotationVec, getSaveData, raycast, reset, setActive, setShown, assignSerial (getSystemData)

void StageCharacter::spawn()
{
    constexpr int   kStageFalling   = 0x1b;     // always arrives falling
    constexpr int   kStageHighA     = 0x1a;     // arrive falling when the floor is far under
    constexpr int   kStageHighB     = 0x1d;
    constexpr int   kMotionFall     = 0xed;
    constexpr int   kMotionNone     = 0;
    constexpr float kProbeDrop      = 850.0f;
    constexpr float kFallHeight     = 750.0f;
    constexpr u_int kFaceFloorProbe = 0x42400c;
    setEntry(gStageScene->scripts[0]->findEntry(m_name));
    int point = m_spawnPoint;
    if (!missionSpawnPointTaken(&point)) {
        const MissionSpawnPoint &place = gStageScene->currentMission()->spawnPoints[point];
        m_xform.setPosition(place.position);
        m_xform.setRotationVec(place.rotation);
    } else {
        m_xform.setPosition(m_respawnPosition);
        m_xform.setRotationVec(m_respawnRotation);
        memset(m_respawnPosition, 0, sizeof(m_respawnPosition));
        memset(m_respawnRotation, 0, sizeof(m_respawnRotation));
    }
    const int stage = getSystemData()->select.stage;
    int motion = kMotionNone;
    if (stage == kStageFalling) {
        motion = kMotionFall;
    } else if (stage == kStageHighA || stage == kStageHighB) {
        const sceVu0FVECTOR from = { m_xform.position[0], m_xform.position[1], m_xform.position[2], m_xform.position[3] };
        const sceVu0FVECTOR to = { from[0], from[1], from[2] - kProbeDrop, from[3] };
        sceVu0FVECTOR hit;
        if (gStageScene->raycast(hit, from, to, 0, kFaceFloorProbe, 0, nullptr) != nullptr
            && kFallHeight <= m_xform.position[2] - hit[2])
            motion = kMotionFall;
    }
    reset(motion);
    m_tree->setActive(true);
    m_tree->setShown(true);
    assignSerial();
    m_placedThisFrame = 1;
}

// FS2 0x002115e0
// callcheck: getStageSelect, getStageSelect

bool StageCharacter::isSpawnable()
{
    constexpr int kKindWeak   = 3;
    constexpr int kKindStrong = 7;
    const int kind = stageObjRecord(m_itemDef)->kind;
    if (kind != kKindWeak && kind != kKindStrong)
        return false;
    return m_name[0] == 'F' && m_name[1] == '_' && (m_name[2] == 'Y' || m_name[2] == 'S')
        && m_name[3] == 'R' && m_name[4] == 'T' && m_name[5] == '\0';
}

// FS2 0x002a6980
// callcheck: getStageSelect

int StageCharacter::getSoundKind()
{
    switch (stageObjRecord(m_itemDef)->kind) {
    case 0:   return SoundKind_Pc;
    case 1:   return SoundKind_Npc;
    case 2: case 4: case 5: case 6: case 8: case 10: case 12:
              return SoundKind_Enemy;
    case 3:   return SoundKind_Srt;
    case 7:   return SoundKind_Yrt;
    case 11:  return SoundKind_Boss1;
    case 13:  return SoundKind_Umi;
    case 15:  return SoundKind_Tks;
    default:  return SoundKind_None;
    }
}

// The head of the four leaves: not when it is leaving already; in the two
// motions 0x9e, 0x9f the running script yields and asks again.
static bool mayLeaveNow(StageCharacter *character)
{
    constexpr int kMotionHeldFirst  = 0x9e;
    constexpr int kMotionHeldSecond = 0x9f;
    constexpr int kYieldLeave       = 0x9f; // what the waiting word keeps; not a native's id
    if (character->getLifeState() == LifeState_Leaving || character->getLifeState() == LifeState_Left)
        return false;
    const int id = character->m_motion->motionId;
    if (gRunningScript != nullptr)
        gRunningScript->takeYield();
    if (id == kMotionHeldSecond || id == kMotionHeldFirst) {
        scriptYield(kYieldLeave, 0);
        return false;
    }
    return true;
}

// A shiryo's smoke, and the cry.
static void playLeaveEffects(StageCharacter *character)
{
    constexpr int kKindShiryo = 3;
    if (stageObjRecord(character->m_itemDef)->kind == kKindShiryo) {
        BoneMap::Node *pelvis = character->m_model->boneMap.find("bn_pelvis");
        gStageScene->nodeTree->spawn("shiryoin", pelvis->value, nullptr, nullptr, 0, 0);
    }
    character->playVanishSound();
}

// FS2 0x00219510
// callcheck: getLifeState, getLifeState, scriptYield, getStageSelect, getStageSelect, find, spawn, FUN_002a31b0 (mayLeaveNow, playLeaveEffects)

void StageCharacter::leaveLoud()
{
    if (!mayLeaveNow(this))
        return;
    playLeaveEffects(this);
    m_lifeOverride = 1;
}

// FS2 0x00219420
// callcheck: getLifeState, getLifeState, scriptYield, getLifeState (mayLeaveNow)

void StageCharacter::leaveQuiet()
{
    if (!mayLeaveNow(this))
        return;
    m_lifeOverride = 1;
}

// FS2 0x002196f0
// callcheck: getLifeState, getLifeState, scriptYield, currentMission, currentMission, isCorpseGone, getStageSelect, getStageSelect, find, spawn, FUN_002a31b0 (mayLeaveNow, playLeaveEffects)

void StageCharacter::leaveNoticed()
{
    if (!mayLeaveNow(this))
        return;
    StageMission *mission = gStageScene->currentMission();
    for (StageObject **it = mission->characters; it != mission->charactersEnd(); it++) {
        StageCharacter *corpse = static_cast<StageCharacter *>(*it);
        if (corpse->m_active == 0 || !corpse->isCorpseGone())
            continue;
        const float distance = corpse->distanceTo(this);
        if (distance < corpse->m_noiseRange)
            corpse->m_noiseRange = distance;
    }
    playLeaveEffects(this);
    m_lifeOverride = 1;
}

// FS2 0x00219310
// callcheck: getLifeState, getLifeState, scriptYield, FUN_002115e0, reset, setBrainState (mayLeaveNow; the ROM drops isSpawnable's answer)

void StageCharacter::leaveAtOnce()
{
    if (!mayLeaveNow(this))
        return;
    reset(0);
    setBrainState(0, 0);
}

// FS2 0x00219cb0
// callcheck: dropSlots, setState, dropSlots, setBrainState

bool StageCharacter::wakeBrain()
{
    if (m_brain == nullptr)
        return false;
    const int state = getBrainPace();
    switch (state) {
    case BrainState_Off:
    case 7:
        return false;
    case 1:
        m_brain->dropSlots(false);
        m_brain->setState(1, 1);
        return true;
    case 2: case 3: case 4: case 5: case 6:
        m_brain->dropSlots(false);
        setBrainState(1, 1);
        return true;
    default:
        return true;
    }
}

// FS2 0x0021bfd0 (thunk 0x0017e780)
// callcheck: set

void StageCharacter::scriptMarkPlace()
{
    if (m_point700.isSet == 0)
        m_point700.set(m_xform.position);
}

// FS2 0x00218110
// callcheck: allowPath

void StageCharacter::allowPath(const char *name, bool on)
{
    if (m_brain != nullptr)
        m_brain->allowPath(name, on);
}

// FS2 0x00219040
// callcheck: isPlayer, set

int StageCharacter::getClimbPoint(StageObject *other)
{
    constexpr float kAbove = 100.0f;
    constexpr float kAtIt  = 15.0f;
    if (!isPlayer() || m_xform.position[2] - other->m_xform.position[2] <= kAbove || m_motion->m_bae == 0)
        return 0;
    const float distance = other->distanceToPoint(m_motion->position);
    m_scriptPoint.set(m_motion->position);
    return distance < kAtIt ? 2 : 1;
}

// FS2 0x0021b690
// callcheck: canWield

StageObject *StageCharacter::findLooseWeapon(float range, u_int mask)
{
    StageObject *best = nullptr;
    StageObjArray &items = gStageScene->objects->m_lists[StageObjGroup_3];
    for (StageObject **slot = items.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        if ((*slot)->m_active == 0 || (*slot)->getKind() != StageObject_WeaponItem)
            continue;
        StageWeaponItem *item = static_cast<StageWeaponItem *>(*slot);
        if (static_cast<StageObject *>(item)->m_parent != nullptr || item->m_holder != nullptr || item->m_xform.dirty == 0
            || item->m_inUse != 0 || item->m_sprung != 0)
            continue;
        if (!canWield(item, mask) || item->m_45c != 0)
            continue;
        const float distance = distanceTo(item);
        if (distance < range) {
            range = distance;
            best = item;
        }
    }
    return best;
}

// The search points: group 6's objects called P_SRC<group>_<n>. The ROM
// walks them through two little iterator classes (vtables 0x316080, every
// active object of the group, and 0x316070, those with n 0).
static const char kSearchPointName[] = "P_SRC%02d_%d";  /* data 0x303448 */

// FS2 0x0021ee40
// FS2 0x0021edc0
// FS2 0x0021ede0
// FS2 0x0021ed20
// callcheck: FUN_0017eeb0 (getSearchGroup)

static StageObject *nextSearchGroup(StageObject **&slot)
{
    while (slot != nullptr) {
        StageObject *object = *slot;
        slot = StageObjArray::next(slot);
        int index = 0;
        if (object->m_active != 0 && object->getSearchGroup(&index) >= 0 && index == 0)
            return object;
    }
    return nullptr;
}

// FS2 0x0017eeb0
// callcheck: sscanf

int StageObject::getSearchGroup(int *index)
{
    int group;
    if (sscanf(getObjectName(), kSearchPointName, &group, index) != 2)
        group = -1;
    return group;
}

// FS2 0x0017ed30
// callcheck: sscanf, sscanf, sprintf, find6 (getSearchGroup, findSearchPoint)

int StageObject::getSearchLastIndex()
{
    constexpr int kHighest = 9;
    int index = -1;
    if (getSearchGroup(&index) < 0 || index != 0)
        return 0;
    for (int number = kHighest; number > 0; number--) {
        if (findSearchPoint(number) != nullptr)
            return number;
    }
    return 0;
}

// FS2 0x0017ee30
// callcheck: sscanf, sprintf, find6

StageObject *StageObject::findSearchPoint(int index)
{
    int ignored;
    char name[0x20];
    sprintf(name, kSearchPointName, getSearchGroup(&ignored), index);
    return gStageScene->objects->find6(name, 0);
}

// FS2 0x0021c010
// PC: the ROM's first half is on the status block (a0 + 0xd8), a1 the character.
// callcheck: FUN_0021ee40, FUN_0021edc0 (nextSearchGroup)

StageObject *StageCharacter::getNearSearchPoint()
{
    constexpr float kNear = 50.0f;
    StageObject *point = m_nearSearchPoint;
    if (point != nullptr && kNear < point->distanceTo(this))
        point = nullptr;
    if (point == nullptr) {
        StageObject **slot = gStageScene->objects->m_lists[StageObjGroup_6].begin();
        for (point = nextSearchGroup(slot); point != nullptr; point = nextSearchGroup(slot)) {
            if (point->distanceTo(this) <= kNear)
                break;
        }
    }
    m_nearSearchPoint = point;
    return point;
}

// FS2 0x00219210
// callcheck: FUN_0021ee40, FUN_0021edc0, FUN_0021e120 (nextSearchGroup, std::stable_sort)

StageObject *StageCharacter::pickSearchPoint(const sceVu0FVECTOR point, u_int rank)
{
    // layout: host -- the ROM keeps these on its stack.
    struct Candidate
    {
        StageObject *object;
        float        distance;
    };
    std::vector<Candidate> found;
    StageObject **slot = gStageScene->objects->m_lists[StageObjGroup_6].begin();
    for (StageObject *group = nextSearchGroup(slot); group != nullptr; group = nextSearchGroup(slot))
        found.push_back({ group, group->distanceToPoint(point) });
    if (rank >= found.size())
        return nullptr;
    std::stable_sort(found.begin(), found.end(),
                     [](const Candidate &a, const Candidate &b) { return a.distance < b.distance; });
    return found[rank].object;
}

// Written in headers; tools/reverse reads tags from .cpp files only:
// BittenLink::~BittenLink (game/playermotion.h): FS2 0x001eedb0
// LiteralMap() (game/stagelist.h): FS2 0x001eee30, FS2 0x001eeee0, FS2 0x001ef040
// ~LiteralMap(): FS2 0x001eee50, FS2 0x001eef00, FS2 0x001ef060
// the nodes' destructors: FS2 0x001ef1a0, FS2 0x001ef210, FS2 0x001ef540
// the nodes' constructors: FS2 0x001ef1f0, FS2 0x001ef520, FS2 0x002817e0
// insert: FS2 0x001eef90, FS2 0x001ef0f0, FS2 0x00281730
// insert's searches: FS2 0x001ef2e0, FS2 0x001ef610, FS2 0x00281800
// find: FS2 0x001ef260, FS2 0x001ef590, FS2 0x001d1280
// balance: FS2 0x001ef850, FS2 0x001ef990
// height: FS2 0x001efad0, FS2 0x001efce0
// MotionLink::rewind (game/playermotion.h): FS2 0x0027cab0
// MotionConditionList::Node::Node (game/playermotion.h): FS2 0x0027dc20
