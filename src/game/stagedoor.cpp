/* ==========================================================================
 *  game/stagedoor.cpp -- a door of the map (game/stagedoor.h says how one
 *  works). The file name is the port's.
 *
 *  Not written: what getProperty and setProperty share with the plain map
 *  object (0x174b00, 0x189f40: position, motions, the mission's clocks): those
 *  ids fall to ScriptRunner's.
 * ======================================================================== */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/clock.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/playermotion.h"
#include "game/stagedoor.h"
#include "game/stagelight.h"
#include "game/stagescene.h"

namespace {

// The motions of the bgmotion targets MODEL_dr_t and MODEL_dr_r.
constexpr char kShutT[] = "MODEL_dr_t_cl";     // data 0x302f68
constexpr char kShutR[] = "MODEL_dr_r_cl";     // data 0x302f58
constexpr char kOpenT[] = "MODEL_dr_t_op";     // data 0x302f88
constexpr char kOpenR[] = "MODEL_dr_r_op";     // data 0x302f78
constexpr u_int kSnapFlags = 0x50;             // playNamed: to the file's end pose

constexpr char kSoundOpenStart[]  = "DR_%02d_OPEN1";   // data 0x302ec8
constexpr char kSoundCloseStart[] = "DR_%02d_CLOSE1";  // data 0x302eb8
constexpr char kSoundOpenEnd[]    = "DR_%02d_OPEN2";   // data 0x302ea8
constexpr char kSoundCloseEnd[]   = "DR_%02d_CLOSE2";  // data 0x302e98

// Faces (kDoorFaceBase is StageScene's): shut, then open, two groups each.
constexpr int kFaceBase = 100;
constexpr int kFacesPerType = 0xb4;
constexpr int kFacesPerDoor = 4;
constexpr int kFacesShut = 2;

constexpr float kReachSquared = 5625.0f;       // 75 units
constexpr float kFacingLimit = 1.0471976f;     // 60 degrees
constexpr float kHalfPi = 1.5707964f;
constexpr float kPiF = 3.1415927f;
constexpr float kMotionFrame = 0.033333335f;   // 1/30 s
constexpr float kLightPace = 0.7f;             // of the motion's length

// In the door's model (data 0x2f7ad0..0x2f7af0).
const sceVu0FVECTOR kPivotLocal = { 47.5f, 0.0f, 0.0f, 1.0f };
const sceVu0FVECTOR kSideALocal = { 47.5f, -35.0f, 0.0f, 1.0f };
const sceVu0FVECTOR kSideBLocal = { 47.5f, 39.0f, 0.0f, 1.0f };

// Script properties (ScriptRunner::getProperty), the door's own.
enum DoorProperty
{
    DoorProperty_SeIndex = 0x06,   // read only
    DoorProperty_Hp      = 0x12,
    DoorProperty_MaxHp   = 0x13,   // read only
    DoorProperty_Mode    = 0x53,
    DoorProperty_Window  = 0x5c,   // read only
};

const DoorRecord *doorRecord(int index)
{
    return index < 0 || index >= kDoorRecords ? (const DoorRecord *)nullptr : &gDoorRecords[index];
}

} // namespace

// FS2 0x00174940
// callcheck: FUN_001765e0

StageDoor::StageDoor(int type, int number)
{
    m_doorType = type;
    m_doorIndex = number;
    m_doorRecord = -1;
    m_doorMode = StageDoorMode_Closed;
    m_doorLight = nullptr;
    m_doorState = StageDoor_Unknown;
}

// FS2 0x00173880
// callcheck: FUN_00173fb0

void StageObject::applyDoorRecord()
{
    StageDoor *door = static_cast<StageDoor *>(this);
    door->m_keyKind = gDoorRecords[m_doorRecord].keyKind;
    door->setMode(gDoorRecords[m_doorRecord].startState);
    door->m_window = gDoorRecords[m_doorRecord].windowState;
    door->m_hp = gDoorRecords[m_doorRecord].hp;
    m_doorState = StageDoor_Unknown;
}

// A door hangs itself from the Root object's lists: which of its faces in the
// Root's mesh count. A broken window takes the glass face away.
// FS2 0x00173cb0
// callcheck: setGroup, setGroup, setGroup, setGroup, setGroup, setGroup, setGroup, setGroup, setGroup

void StageObject::attachToRoot()
{
    StageObject *root = gStageScene->root;
    if (root == nullptr)
        return;
    StageCollision *mesh = root->collisionAt(0);
    const int first = kFaceBase + (m_doorType - 1) * kFacesPerType + (m_doorIndex - 1) * kFacesPerDoor;
    // PC: a door whose motion never came has no parts; the ROM reads low memory.
    PartSwitch *glass = m_mapParts != nullptr ? m_mapParts->switches : nullptr;
    const bool glassGone = glass != nullptr && glass[0].shown == 0;

    switch (m_doorMode) {
    case StageDoorMode_Closed:
    case StageDoorMode_Locked:
    case StageDoorMode_Barred:
        for (int i = 0; i < kFacesPerDoor; i++)
            mesh->setGroup(first + i, i < kFacesShut);
        if (glassGone)
            mesh->setGroup(first + 1, false);
        m_collides = 0;
        break;
    case StageDoorMode_Opening:
    case StageDoorMode_Closing:
        for (int i = 0; i < kFacesPerDoor; i++)
            mesh->setGroup(first + i, false);
        m_collides = 1;
        break;
    case StageDoorMode_Open:
        for (int i = 0; i < kFacesPerDoor; i++)
            mesh->setGroup(first + i, i >= kFacesShut);
        if (glassGone)
            mesh->setGroup(first + 3, false);
        m_collides = 0;
        break;
    }
}

// FS2 0x00173fb0
// callcheck: playNamed, applyMotion, playNamed, applyMotion, attachToRoot

void StageDoor::setMode(int mode)
{
    const char *snapTo = nullptr;
    switch (mode) {
    case StageDoorMode_Closed:
    case StageDoorMode_Locked:
    case StageDoorMode_Barred:
        snapTo = m_doorType == StageDoorType_T ? kShutT : m_doorType == StageDoorType_R ? kShutR : nullptr;
        break;
    case StageDoorMode_Open:
        snapTo = m_doorType == StageDoorType_T ? kOpenT : m_doorType == StageDoorType_R ? kOpenR : nullptr;
        break;
    case StageDoorMode_Opening:
    case StageDoorMode_Closing:
        if (m_doorLight != nullptr) {
            ModelPose *pose = m_model->pose;
            m_lightTime = (pose->motionFrames - pose->rootBone->time) / 30.0f * kLightPace;
            m_lightElapsed = 0.0f;
        }
        break;
    }
    if (mode == StageDoorMode_Closed || mode == StageDoorMode_Locked || mode == StageDoorMode_Barred
        || mode == StageDoorMode_Open) {
        if (snapTo != nullptr)
            m_model->playNamed(0, snapTo, false, false, kSnapFlags, 0);
        m_model->applyMotion(m_xform.position, m_xform.rotation);
        if (m_doorLight != nullptr)
            static_cast<StageLight *>(m_doorLight)->m_brightness = mode == StageDoorMode_Open ? 1.0f : 0.0f;
    }
    m_doorMode = mode;
    attachToRoot();
}

// FS2 0x00172530

void StageDoor::noteUse(bool on)
{
    if (!on)
        m_doorState = StageDoor_Unknown;
    else if (isShut())
        m_doorState = m_doorMode == StageDoorMode_Barred ? StageDoor_Blocked
                    : m_doorMode == StageDoorMode_Locked ? StageDoor_Locked : StageDoor_Open2;
    else
        m_doorState = m_doorMode == StageDoorMode_Closing ? StageDoor_Open2 : StageDoor_Open1;
}

// FS2 0x001725e0

bool StageDoor::isShut() const
{
    return m_doorMode == StageDoorMode_Barred || m_doorMode == StageDoorMode_Locked
        || m_doorMode == StageDoorMode_Closed;
}

// FS2 0x00172480

bool StageDoor::isWithinReach(const float *point) const
{
    for (const sceVu0FVECTOR *side : { &m_sideA, &m_sideB }) {
        const float x = (*side)[0] - point[0], y = (*side)[1] - point[1], z = (*side)[2] - point[2];
        if (x * x + y * y + z * z < kReachSquared)
            return true;
    }
    return false;
}

// FS2 0x00173830

bool StageDoor::canReachThrough() const
{
    return m_keyKind == 2 || m_keyKind == 4 || m_keyKind == kDoorKeyBothSides;
}

// FS2 0x001739b0

bool StageDoor::isFree() const
{
    return m_user == nullptr && m_doorMode != StageDoorMode_Closing && m_doorMode != StageDoorMode_Opening;
}

// Inline in the next three.
float StageDoor::sideOf(const float *point) const
{
    const float toPoint = addAngle(atan2Octant(point[0] - m_pivot[0], -(point[1] - m_pivot[1])), -kHalfPi);
    return addAngle(m_yaw, -toPoint);
}

// FS2 0x001739f0
// callcheck: atan2Octant, addAngle, addAngle (sideOf)

bool StageDoor::keyWorksFrom(const float *point) const
{
    const float side = sideOf(point);
    if (side > 0.0f && (m_keyKind == 1 || m_keyKind == 3 || m_keyKind == kDoorKeyBothSides))
        return true;
    return side <= 0.0f && canReachThrough();
}

// FS2 0x00173c30
// callcheck: atan2Octant, addAngle, addAngle (sideOf)

bool StageDoor::isInFront(const float *point) const
{
    return sideOf(point) > 0.0f;
}

// FS2 0x00173b30
// callcheck: atan2Octant, addAngle, addAngle, addAngle (sideOf)

void StageDoor::getUsePlace(sceVu0FVECTOR place, sceVu0FVECTOR angles, const StageObject *who) const
{
    memcpy(place, m_pivot, sizeof(sceVu0FVECTOR));
    angles[0] = 0.0f;
    angles[1] = 0.0f;
    angles[2] = sideOf(who->m_xform.position) <= 0.0f ? m_yawBack : m_yaw;
    angles[3] = 1.0f;
    angles[2] = addAngle(angles[2], -kPiF);
}

const char *StageDoor::soundName(char *buffer, const char *format) const
{
    sprintf(buffer, format, gDoorRecords[m_doorRecord].seIndex);
    return buffer;
}

// FS2 0x001723c0
// callcheck: isWithinReach, atan2Octant, addAngle, addAngle

bool StageDoor::scriptIsFacing(StageObject *other)
{
    const float *point = other->m_xform.position;
    if (!isWithinReach(point))
        return false;
    float angle = atan2Octant(point[0] - m_pivot[0], -(point[1] - m_pivot[1]));
    angle = addAngle(angle, -other->m_xform.rotation[2]);
    angle = addAngle(angle, -kPiF);
    return fabsf(angle) < kFacingLimit;
}

// The base's, then the mode of the sheet, and the door's points in the world.
// FS2 0x001747b0
// callcheck: FUN_00175d40, FUN_00173fb0, applyMatrix, applyMatrix, applyMatrix, atan2Cw, addAngle, breakPart, getWorldMatrix, getWorldMatrix, getWorldMatrix, getWorldMatrix, getWorldMatrix

void StageDoor::setMotionList(void *list, int arg, int arg2)
{
    StageMapObject::setMotionList(list, arg, arg2);
    // PC: the record is known here; the ROM's range test answers address 0.
    const DoorRecord *record = doorRecord(m_doorRecord);
    if (record == nullptr)
        return;
    setMode(record->startState);
    const sceVu0FVECTOR *world = getWorldMatrix();
    applyMatrix(m_pivot, world, kPivotLocal);
    applyMatrix(m_sideA, world, kSideALocal);
    applyMatrix(m_sideB, world, kSideBLocal);
    m_yaw = atan2Cw(-world[1][0], world[0][0]);
    m_yawBack = addAngle(m_yaw, -kPiF);
    if (record->windowState == StageDoorWindow_Broken)
        breakPart(0, false);
}

// What the frame's motion events do: Begin starts the door with the hand, End
// rests it.
// FS2 0x00174390
// callcheck: FUN_00173fb0, FUN_00173fb0, FUN_00173fb0, FUN_00173fb0, FUN_00173fb0, FUN_00173fb0, isControlled, FUN_00172530, sprintf, sprintf, sprintf, sprintf, playNamedHeard2, FUN_00173fb0, sprintf, FUN_00173fb0, sprintf, playNamedHeard2

void StageDoor::sideHook1()
{
    StageObject *heard = m_user != nullptr ? (StageObject *)m_user : (StageObject *)this;
    char name[0x20];
    if ((m_model->m_motionEvents & MotionEvent_Begin) != 0 && m_user != nullptr) {
        const int id = m_user->m_motion->motionId;
        switch (id) {
        case Motion_DoorPullOpen:
        case Motion_DoorPushOpen:
            setMode(StageDoorMode_Opening);
            break;
        case Motion_DoorPushClose:
        case Motion_DoorPullClose:
            setMode(StageDoorMode_Closing);
            break;
        case Motion_DoorSlideA:
        case Motion_DoorSlideB:
            if (m_doorMode == StageDoorMode_Closed)
                setMode(StageDoorMode_Opening);
            else if (m_doorMode == StageDoorMode_Open)
                setMode(StageDoorMode_Closing);
            break;
        }
        if (m_user->isControlled())
            noteUse(true);

        const char *sound = nullptr;
        switch (id) {
        case Motion_DoorPullOpen:
        case Motion_DoorPushOpen:
            sound = kSoundOpenStart;
            break;
        case Motion_DoorPushClose:
        case Motion_DoorPullClose:
            sound = kSoundCloseStart;
            break;
        case Motion_DoorSlideA:
        case Motion_DoorSlideB:
            sound = m_doorMode == StageDoorMode_Opening ? kSoundOpenStart : kSoundCloseStart;
            break;
        }
        if (sound != nullptr)
            heard->playNamedHeard2(soundName(name, sound), m_xform.position, nullptr);
    }
    if ((m_model->m_motionEvents & MotionEvent_End) != 0) {
        const char *sound = nullptr;
        if (m_doorMode == StageDoorMode_Closing) {
            setMode(StageDoorMode_Closed);
            sound = kSoundCloseEnd;
        } else if (m_doorMode == StageDoorMode_Opening) {
            setMode(StageDoorMode_Open);
            sound = kSoundOpenEnd;
        }
        if (sound != nullptr)
            heard->playNamedHeard2(soundName(name, sound), m_xform.position, nullptr);
    }
}

// FS2 0x001741c0
// callcheck: FUN_00174380, getMotionRate, getDeltaSeconds, advance, getDeltaSeconds (FUN_00174380 answers 1, so the rate)

void StageDoor::sideHook2()
{
    float rate = 1.0f;
    if (m_user != nullptr)
        rate = m_user->m_motion->doorLink->rate * m_user->getMotionRate();
    const float seconds = gStageScene->clock->getDeltaSeconds();
    m_model->advance(rate * (seconds / kMotionFrame), m_xform.position, m_xform.rotation, 0);
    for (BoneMap::Node *node = m_model->boneMap.first; node != nullptr; node = node->next)
        node->value->updateMatrix();
    m_touchedBy = nullptr;

    if (m_doorLight != nullptr) {
        m_lightElapsed += seconds;
        if (m_lightTime < m_lightElapsed)
            m_lightElapsed = m_lightTime;
        StageLight *light = static_cast<StageLight *>(m_doorLight);
        if (m_doorMode == StageDoorMode_Closing)
            light->m_brightness = 1.0f - m_lightElapsed / m_lightTime;
        else if (m_doorMode == StageDoorMode_Opening)
            light->m_brightness = m_lightElapsed / m_lightTime;
    }
}

// The glass went: the next forcing of the door opens it.
// FS2 0x00174180

void StageDoor::sideHook3()
{
    PartSwitch *glass = m_mapParts != nullptr ? m_mapParts->switches : nullptr; // PC: attachToRoot
    if (glass != nullptr && glass[0].shown == 0 && m_window == StageDoorWindow_Whole)
        m_window = StageDoorWindow_Broken;
}

// FS2 0x00172d80
// callcheck: getProperty (the rest is 0x174b00's list: file header)

int StageDoor::getProperty(int id, int sub)
{
    switch (id) {
    case DoorProperty_Window:
        return m_window;
    case DoorProperty_Mode:
        return m_doorMode;
    case DoorProperty_Hp:
        return m_hp;
    case DoorProperty_MaxHp:
        return gDoorRecords[m_doorRecord].hp;
    case DoorProperty_SeIndex:
        return (u_short)gDoorRecords[m_doorRecord].seIndex;
    }
    return StageMapObject::getProperty(id, sub);
}

// FS2 0x00172620
// callcheck: FUN_00173fb0, setProperty (the rest is 0x189f40's list: file header)

void StageDoor::setProperty(int id, int sub, int value)
{
    if (id == DoorProperty_Mode) {
        setMode(value);
    } else if (id == DoorProperty_Hp) {
        const int most = gDoorRecords[m_doorRecord].hp;
        const int hp = value < most ? value : most;
        m_hp = hp > 0 ? hp : 0;
    } else {
        StageMapObject::setProperty(id, sub, value);
    }
}
