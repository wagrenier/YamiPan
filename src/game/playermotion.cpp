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
#include "game/modelpose.h"
#include "game/packtables.h"
#include "game/scenepairs.h"
#include "game/stageitem.h"
#include "game/stagenode.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
#include "game/stagesound.h"
#include "game/sysdata.h"

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
    memcpy(args, owner->m_360, kArgsSize);
    from = current;
}

void MotionQueue::restore(StageCharacter *owner) const
{
    memcpy(owner->m_360, args, kArgsSize);
}

// FS2 0x0027cb60

void MotionLink::update()
{
    onUpdate();
    // The conditions of its list, slot +0xc: not written.
}

// FS2 0x001ed410
// callcheck: FUN_001d1280, FUN_00278db0, FUN_0027a0b0, FUN_0027a910, FUN_0027aa90, FUN_0027aea0, FUN_0027b000, FUN_0027b730, FUN_00281aa0, FUN_00281b30, FUN_002820e0, FUN_00282200, FUN_002822d0, FUN_002825f0, FUN_00282b30, FUN_00282da0, FUN_00282fa0, FUN_00283560, FUN_00283740, FUN_00283960, getStageSelect, operator_new, FUN_001ef040, FUN_001eeee0, FUN_001eee30, FUN_0026c6b0, FUN_00276510, FUN_001ef0f0, FUN_001ef850, FUN_001df0b0, FUN_0027c520, FUN_0027c140, FUN_0027b640, FUN_0027cc50, FUN_002775d0, FUN_001eef90, FUN_0027c370, FUN_001ef990 (the links, the conditions and their maps: game/playermotion.h)

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
    look = new PlayerLookState;
    memset(look, 0, sizeof(*look));
}

// FS2 0x001ed2a0
// callcheck: operator_delete, FUN_0026c650, FUN_001eee50, FUN_001eef00, FUN_001ef060 (the host deletes; the maps of the ctor)

PlayerMotion::~PlayerMotion()
{
    delete m_queue;
    delete m_walkQueue;
    for (auto &entry : m_links)
        delete entry.second;
    delete look;
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
    m_b04 = 0.0f;
    m_b08 = 0.0f;
    memcpy(m_lastPosition, character->m_xform.position, sizeof(m_lastPosition));
    m_stanceFlag = 0;
    m_b24 = 0.0f;
    m_b28 = 0.0f;
    m_b2c = 0.0f;
    m_b30 = 0;
    m_forceStart = 0;
    memset(m_b60, 0, sizeof(m_b60));
    character->m_xform.rotation[0] = 0.0f;
    character->m_xform.rotation[1] = 0.0f;
    memset(m_lampAngles, 0, sizeof(m_lampAngles));
    memset(m_b90, 0, sizeof(m_b90));
    m_ba0 = 0;

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
    m_bd0 = 0.0f;
    m_stepped = 0;
    m_queue->expire();
    m_queue->open = m_queue->open == 0;
    m_walkQueue->expire();
    m_walkQueue->open = m_walkQueue->open == 0;
    pose->setRootScale(1.0f, 1.0f, 1.0f, 1.0f);
    if (0.0f < m_ba8)
        m_ba8 -= clock->getDeltaSeconds();
    memset(m_b60, 0, sizeof(m_b60));
    // Here the ROM steps the Always condition (slot +0xc): not written.

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
        if (character->m_abilities.flag3aa != 0) {
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

// FS2 0x0017df60

void stageUpdateCharacterMotions()
{
    StageObjArray &characters = gStageScene->objects->m_lists[StageObjGroup_Characters];
    for (StageObject **slot = characters.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *character = *slot;
        if (!character->m_viewActive)
            continue;
        PlayerMotion *motion = character->m_motion;
        // PC: until the start hook 0x17a860 is written.
        motion->beginFrame();
        // PC: nothing asks yet (the player's and the brain's code): the idle.
        motion->request(motion->pickStance(MotionStance_Idle));
        motion->stepLower(false);
        motion->stepUpper(false);
        motion->stepFace(false);
        // PC: the movement hook 0x17a490 runs the model. Until it is written,
        // one 0.033 s step of the ROM per 25 Hz frame.
        const float kFrameStep = 1.2f;
        character->m_model->advance(kFrameStep, character->m_xform.position, character->m_xform.rotation, 0);
    }
}
