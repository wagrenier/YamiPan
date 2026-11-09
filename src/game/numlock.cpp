/* ========================================================================
 *  game/numlock.cpp -- the number lock and its pose (game/numlock.h).
 *  0x2a6ab0..0x2a7b10. The thunks 0x2a73e0..0x2a7418 of the side and
 *  ScriptRunner bases are the host's multiple inheritance.
 * ======================================================================== */
#include <math.h>
#include "game/gamerate.h"
#include <string.h>

#include "game/heap.h"
#include "game/numlock.h"
#include "game/stageresult.h"

namespace {

constexpr float kPi = 3.14159274f;          // 0x40490fdb
constexpr float kHalfPi = 1.57079637f;      // 0x3fc90fdb
constexpr float kTwoPi = 6.28318548f;       // 0x40c90fdb
constexpr float kDialStill = 0.01f;         // dialsTurning: radians from the number
constexpr int   kDialEaseFrames = 5;        // update: a fifth of the way a frame
constexpr int   kDialSnapFrames = 1;

constexpr char kPoseTag[] = "nml";          // data 0x30a670
constexpr char kBoneKanagu[] = "bn_kanagu"; // data 0x30a638
constexpr char kBoneKey[] = "bn_key";       // data 0x30a648
// Dial 0 is bn_k04 (data 0x30a650), dial 3 bn_k01 (data 0x30a668).
constexpr const char *kDialBoneNames[kNumLockDials] = { "bn_k04", "bn_k03", "bn_k02", "bn_k01" };

// The dials turn about the bone's x (data 0x2fb720).
constexpr float kDialAxis[3] = { 1.0f, 0.0f, 0.0f };

inline void copyVector(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    memcpy(out, in, sizeof(sceVu0FVECTOR));
}

// The angle a dial stands at for number, as the ROM rounds it.
inline float dialAngleOf(float number)
{
    return (-number * kPi * 2.0f) / (float)kNumLockNumbers;
}

// A random number of a dial.
inline int randomDialNumber()
{
    return (int)(randUnit() * (float)kNumLockNumbers);
}

} // namespace

// ──────────────────────────────────────────────────────────────────────────
// NumLock.

// FS2 0x002a7340
// callcheck: StageMapObject, setDirtyTree

NumLock::NumLock() : StageMapObject(1)
{
    setDirtyTree(true);
    m_depth = 0;
    m_lightDirection[0] = 0.0f;
    m_lightDirection[1] = kHalfPi;
    m_lightDirection[2] = 0.0f;
    m_lightDirection[3] = 0.0f;
    m_lightColor[0] = 1.0f;
    m_lightColor[1] = 1.0f;
    m_lightColor[2] = 1.0f;
    m_lightColor[3] = 0.0f;
}

// FS2 0x002a6ab0
// callcheck: ~StageMapObject, operator_delete

NumLock::~NumLock()
{
}

// What stands in the header, and the adjustor thunks (the host's compiler
// makes its own):
//   FS2 0x002a6b40  NumLock::getKind (numlock.h)
//   FS2 0x002a6b50  NumLock::sideHook3 (numlock.h)
//   FS2 0x002a6b60  NumLock::sideHook0 (numlock.h)
//   FS2 0x002a6e30  NumLock::setInView (numlock.h)
//   FS2 0x002a71c0  NumLock::sideHook1 (numlock.h)
//   FS2 0x002a73e0  NumLock::sideHook3, -0x18
//   FS2 0x002a73f0  NumLock::sideHook1, -0x18
//   FS2 0x002a7400  NumLock::sideHook0, -0x18
//   FS2 0x002a7410  NumLock::~NumLock, -0x40

// FS2 0x002a6b70
// callcheck: randUnit, getDial, setDials

void NumLock::scramble()
{
    NmlPose *pose = dialPose();
    int numbers[kNumLockDials];
    for (int i = 0; i < kNumLockDials; i++) {
        do {
            do {
                numbers[i] = randomDialNumber();
            } while (m_code[i] == numbers[i]);
        } while (numbers[i] == pose->getDial(i));
    }
    pose->setDials(numbers[0], numbers[1], numbers[2], numbers[3]);
}

// FS2 0x002a6c40
// callcheck: setActive, setShown, setDials

int NumLock::popDials()
{
    if (m_depth < 1)
        return -1;
    m_depth--;
    const bool active = m_savedActive[m_depth] != 0;
    m_tree->setActive(active);
    m_tree->setShown(active);
    const u_char *dials = m_savedDials[m_depth];
    dialPose()->setDials(dials[0], dials[1], dials[2], dials[3]);
    return m_depth;
}

// FS2 0x002a6cd0
// callcheck: getDial, getDial, getDial, getDial, getDial, setDials

int NumLock::pushDials(int dial0, int dial1, int dial2, int dial3)
{
    if (m_depth >= kNumLockStack)
        return -1;
    m_savedActive[m_depth] = m_active != 0;
    NmlPose *pose = dialPose();
    for (int i = 0; i < kNumLockDials; i++)
        m_savedDials[m_depth][i] = (u_char)pose->getDial(i);

    if (dial0 < 0)
        dial0 = pose->getDial(0);
    if (dial1 < 0)
        dial1 = pose->getDial(1);
    if (dial2 < 0)
        dial2 = pose->getDial(2);
    if (dial3 < 0)
        dial3 = pose->getDial(3);
    pose->setDials(dial0, dial1, dial2, dial3);
    m_depth++;
    return m_depth;
}

// FS2 0x002a6e40
// callcheck: randUnit, getDial, setDial

void NumLock::spinDial(int dial)
{
    NmlPose *pose = dialPose();
    int number;
    do {
        do {
            number = randomDialNumber();
        } while (number == m_code[dial]);
    } while (number == pose->getDial(dial));
    pose->setDial(dial, number);
}

// FS2 0x002a6ee0
// callcheck: setDial

void NumLock::setDial(int dial)
{
    dialPose()->setDial(dial, m_code[dial]);
}

// FS2 0x002a6f00
// callcheck: getDial, setDial

void NumLock::dialDown(int dial)
{
    NmlPose *pose = dialPose();
    pose->setDial(dial, (pose->getDial(dial) + kNumLockNumbers - 1) % kNumLockNumbers);
}

// FS2 0x002a6f60
// callcheck: getDial, setDial

void NumLock::dialUp(int dial)
{
    NmlPose *pose = dialPose();
    pose->setDial(dial, (pose->getDial(dial) + 1) % kNumLockNumbers);
}

// FS2 0x002a6fc0
// callcheck: getDial, getDial, getDial, getDial

bool NumLock::dialsRight()
{
    NmlPose *pose = dialPose();
    for (int i = 0; i < kNumLockDials; i++) {
        if (m_code[i] != (char)pose->getDial(i))
            return false;
    }
    return true;
}

// FS2 0x002a7070
// callcheck: getDial

bool NumLock::dialsTurning()
{
    NmlPose *pose = dialPose();
    for (int i = 0; i < kNumLockDials; i++) {
        float delta = dialAngleOf((float)pose->getDial(i)) - pose->dialAngle[i];
        while (kPi < delta)
            delta -= kTwoPi;
        while (delta < -kPi)
            delta += kTwoPi;
        if (fabsf(delta) >= kDialStill)
            return true;
    }
    return false;
}

// FS2 0x002a71d0
// callcheck: setMotionList, getStageResult, randUnit, randUnit, randUnit, randUnit, getStageResult

void NumLock::setMotionList(void *list, int, int arg2)
{
    StageMapObject::setMotionList(list, 1, arg2);
    for (int i = 0; i < kNumLockDials; i++)
        m_code[i] = getStageResult()->numLockCode[i];
    if (m_code[0] != 0 || m_code[1] != 0 || m_code[2] != 0 || m_code[3] != 0)
        return;

    // No code yet: four digits, not all zero, kept for the retries.
    do {
        for (int i = 0; i < kNumLockDials; i++)
            m_code[i] = (char)randomDialNumber();
    } while (m_code[0] == 0 && m_code[1] == 0 && m_code[2] == 0 && m_code[3] == 0);
    for (int i = 0; i < kNumLockDials; i++)
        getStageResult()->numLockCode[i] = m_code[i];
}

// ──────────────────────────────────────────────────────────────────────────
// NmlPose.

// FS2 0x002a7a50
// callcheck: ModelPose, strcpy, memset

NmlPose::NmlPose(int dial0, int dial1, int dial2, int dial3)
{
    strcpy(tag, kPoseTag);
    dialNumber[0] = (float)dial0;
    dialNumber[1] = (float)dial1;
    dialNumber[2] = (float)dial2;
    dialNumber[3] = (float)dial3;
    snapDials = 1;
    memset(dialAngle, 0, sizeof(dialAngle));
}

// FS2 0x002a79f0
// callcheck: ~ModelPose, operator_delete

NmlPose::~NmlPose()
{
}

// FS2 0x002a7840

void NmlPose::setDials(int dial0, int dial1, int dial2, int dial3)
{
    dialNumber[0] = (float)dial0;
    dialNumber[1] = (float)dial1;
    dialNumber[2] = (float)dial2;
    dialNumber[3] = (float)dial3;
    snapDials = 1;
}

// FS2 0x002a7890
// callcheck: load, strcmp, strcmp, strcmp, strcmp, strcmp, strcmp, easeDials

int NmlPose::load(void *file, int arg, const char *boneNames, u_int boneCount_)
{
    ModelPose::load(file, arg, boneNames, boneCount_);
    for (int i = 0; i < boneCount; i++) {
        const char *name = bones[i].name;
        if (strcmp(name, kBoneKanagu) == 0)
            kanaguBone = i;
        if (strcmp(name, kBoneKey) == 0)
            keyBone = i;
        for (int dial = 0; dial < kNumLockDials; dial++) {
            if (strcmp(name, kDialBoneNames[dial]) == 0)
                dialBones[dial] = i;
        }
    }
    easeDials(dialNumber, kDialSnapFrames);
    return 1;
}

// FS2 0x002a7420
// callcheck: advance, solve, easeDials, easeDials, sceVu0CopyVector, sceVu0CopyVector, sceVu0CopyVector, sceVu0CopyVector, buildMatrix

void NmlPose::update(float frames, bool)
{
    flags &= ~kPoseFlagFresh;
    if (current == nullptr)
        return;

    for (int i = 0; i < boneCount; i++)
        bones[i].advance(frames);
    solve(PoseSlot_Solve, -1, 1);
    if (!snapDials) {
        easeDials(dialNumber, kDialEaseFrames);
    } else {
        easeDials(dialNumber, kDialSnapFrames);
        snapDials = 0;
    }

    for (int i = 0; i < boneCount; i++) {
        ModelPoseBone *bone = &bones[i];
        if (bone->parent == nullptr) {
            copyVector(bone->position[PoseSlot_Final], bone->position[PoseSlot_Solve]);
            copyVector(bone->rotation[PoseSlot_Final], bone->rotation[PoseSlot_Solve]);
        } else {
            copyVector(bone->position[PoseSlot_Final], bone->position[PoseSlot_Current]);
            copyVector(bone->rotation[PoseSlot_Final], bone->rotation[PoseSlot_Current]);
            bone->buildMatrix(frames, PoseSlot_Final, 1);
        }
        bone->playFlags &= ~PosePlay_Moved;
    }
}

// The ROM steps the target by whole turns until it is within half a turn of
// the angle, then moves a 1/frames share of the rest (its "+ 0.0" stays: it
// turns -0 into +0). frames < 0 counts as 1. Always 1.
// FS2 0x002a75d0
// callcheck: quatFromAxisAngle, quatFromAxisAngle, quatFromAxisAngle, quatFromAxisAngle

int NmlPose::easeDials(const float *numbers, int frames)
{
    if (numbers == nullptr)
        return 1;
    if (frames < 0)
        frames = 1;
    for (int i = 0; i < kNumLockDials; i++) {
        float target = dialAngleOf(numbers[i]);
        float delta;
        while ((delta = target - dialAngle[i]) > kPi)
            target -= kTwoPi;
        while (delta < -kPi) {
            target += kTwoPi;
            delta = target - dialAngle[i];
        }
        // PC: game_rate (game/gamerate.h): 1 / frames of the way a 25 Hz frame, as this frame's share.
        dialAngle[i] = palEase(1.0f / (float)frames) * delta + dialAngle[i] + 0.0f;
    }
    // PC: a model without the dial bones keeps them at bone 0, as the host
    // clears the indices the ROM's ctor leaves; a refused bone file has none.
    if (bones == nullptr)
        return 1;
    sceVu0FVECTOR axisAngle = { kDialAxis[0], kDialAxis[1], kDialAxis[2], 0.0f };
    for (int i = 0; i < kNumLockDials; i++) {
        axisAngle[3] = dialAngle[i];
        quatFromAxisAngle(bones[dialBones[i]].rotation[PoseSlot_Current], axisAngle);
    }
    return 1;
}
