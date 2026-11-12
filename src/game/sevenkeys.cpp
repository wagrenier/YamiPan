/* ========================================================================
 *  game/sevenkeys.cpp -- the seven keys, their view and pose (game/sevenkeys.h).
 *  0x2b22f0..0x2b3860. The thunks 0x2b2ad0..0x2b2b08 of the side and
 *  ScriptRunner bases are the host's multiple inheritance.
 * ======================================================================== */
#include <math.h>
#include "game/gamerate.h"
#include <string.h>

#include "game/characterinput.h"
#include "game/gamehooks.h"
#include "game/pad.h"
#include "game/sevenkeys.h"
#include "game/stagecollision.h"
#include "game/stageparticle.h"
#include "game/stagescene.h"
#include "game/vu0math.h"

namespace {

constexpr float kPi = 3.14159274f;          // 0x40490fdb
constexpr float kHalfPi = 1.57079637f;      // 0x3fc90fdb
constexpr float kTwoPi = 6.28318548f;       // 0x40c90fdb
constexpr float kDialStill = 0.01f;         // keysTurning: radians from the position
constexpr int   kDialEaseFrames = 5;        // update: a fifth of the way a frame
constexpr int   kDialSnapFrames = 1;

constexpr char kPoseTag[] = "sky";          // data 0x30b3e8
// Dial 0 is bn_k02 (data 0x30b3d8), dial 1 bn_k01 (data 0x30b3e0).
constexpr const char *kDialBoneNames[kSevenKeysDials] = { "bn_k02", "bn_k01" };

// The dials turn about the bone's x (data 0x2fb830).
constexpr float kDialAxis[3] = { 1.0f, 0.0f, 0.0f };

// The view.
constexpr char kTurnSound[] = "EVE_S11_01"; // data 0x30b3c0
constexpr char kSparkleEffect[] = "mon";    // data 0x30b3d0
constexpr char kSparkleBone[] = "bn_top";   // data 0x30b3b8
// Where "mon" is spawned (data 0x2fb800, 0x2fb810), and its place on bn_top
// in view: x moves 20 a dial (data 0x2fb820).
const float kSparkleSpawnPos[4] = { -1.2f, 2.5f, 3.0f, 1.0f };
const float kSparkleSpawnRot[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
const float kSparklePos[4] = { 10.0f, 13.0f, 10.0f, 1.0f };
constexpr float kSparkleDialStep = 20.0f;
constexpr int   kKeyLights = 3;

inline void copyVector(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    memcpy(out, in, sizeof(sceVu0FVECTOR));
}

// The angle a dial stands at for a position, as the ROM rounds it.
inline float dialAngleOf(float position)
{
    return (-position * kPi * 2.0f) / (float)kSevenKeysPositions;
}

// position + step round the four, as the ROM's signed remainder.
inline int wrapPosition(int position, int step)
{
    return (position + step) % kSevenKeysPositions;
}

} // namespace

// ──────────────────────────────────────────────────────────────────────────
// SevenKeys.

// FS2 0x002b2a60
// callcheck: StageMapObject, setDirtyTree

SevenKeys::SevenKeys() : StageMapObject(1)
{
    setDirtyTree(true);
}

// FS2 0x002b22f0
// callcheck: ~StageMapObject, operator_delete

SevenKeys::~SevenKeys()
{
}

// What stands in the header, and the adjustor thunks (the host's compiler
// makes its own):
//   FS2 0x002b2380  SevenKeys::getKind (sevenkeys.h)
//   FS2 0x002b2390  SevenKeys::sideHook3 (sevenkeys.h)
//   FS2 0x002b23a0  SevenKeys::sideHook0 (sevenkeys.h)
//   FS2 0x002b2910  SevenKeys::sideHook1 (sevenkeys.h)
//   FS2 0x002b2ad0  SevenKeys::sideHook3, -0x18
//   FS2 0x002b2ae0  SevenKeys::sideHook1, -0x18
//   FS2 0x002b2af0  SevenKeys::sideHook0, -0x18
//   FS2 0x002b2b00  SevenKeys::~SevenKeys, -0x40

// The property 0x6b of a kind 6 object (StageMapObject::getProperty).
// FS2 0x002b23b0
// callcheck: getDial

int sevenKeysPoseValue(StageObject *object, int sub)
{
    return static_cast<SevenKeys *>(object)->keyValue(sub);
}

// FS2 0x002b23e0

void SevenKeys::setCode(int code0, int code1)
{
    if (code0 >= 0 && code0 < kSevenKeysPositions)
        m_code[0] = (char)code0;
    if (code1 >= 0 && code1 < kSevenKeysPositions)
        m_code[1] = (char)code1;
}

// FS2 0x002b2420
// callcheck: getDial, getDial, setDials

void SevenKeys::setKeys(int dial0, int dial1)
{
    SkyPose *pose = keyPose();
    if (dial0 < 0)
        dial0 = pose->getDial(0);
    if (dial1 < 0)
        dial1 = pose->getDial(1);
    pose->setDials(dial0, dial1);
}

// FS2 0x002b24a0
// callcheck: getDial, setDial, getDial, setDial

void SevenKeys::keyDown(int dial)
{
    SkyPose *pose = keyPose();
    pose->setDial(dial, wrapPosition(pose->getDial(dial), kSevenKeysPositions - 1));
    if (dial == 1)
        pose->setDial(0, wrapPosition(pose->getDial(0), kSevenKeysPositions - 1));
}

// FS2 0x002b2560
// callcheck: getDial, setDial, getDial, setDial

void SevenKeys::keyUp(int dial)
{
    SkyPose *pose = keyPose();
    pose->setDial(dial, wrapPosition(pose->getDial(dial), 1));
    if (dial == 1)
        pose->setDial(0, wrapPosition(pose->getDial(0), 1));
}

// FS2 0x002b2620
// callcheck: keysTurning (inline: getDial), getDial, getDial

bool SevenKeys::keysOpen()
{
    if (keysTurning())
        return false;
    SkyPose *pose = keyPose();
    return m_code[0] == (char)pose->getDial(0) && m_code[1] == (char)pose->getDial(1);
}

// FS2 0x002b27c0
// callcheck: getDial

bool SevenKeys::keysTurning()
{
    SkyPose *pose = keyPose();
    for (int i = 0; i < kSevenKeysDials; i++) {
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

// FS2 0x002b2920
// callcheck: setMotionList, rotateMatrixZ, invertRigidMatrix

void SevenKeys::setMotionList(void *list, int, int arg2)
{
    StageMapObject::setMotionList(list, 1, arg2);
    StageCollision *mesh = collisionAt(0);
    if (mesh != nullptr) {
        // The faces stand where the object does, a quarter turned about z.
        const sceVu0FVECTOR *world = getWorldMatrix();
        for (int row = 0; row < 3; row++)
            copyVector(m_faceMatrix[row], world[row]);
        memset(m_faceMatrix[3], 0, sizeof(sceVu0FVECTOR));
        rotateMatrixZ(kHalfPi, m_faceMatrix, m_faceMatrix);
        copyVector(m_faceMatrix[3], getWorldMatrix()[3]);
        invertRigidMatrix(m_faceInverse, m_faceMatrix);
        mesh->m_matrix = m_faceMatrix;
        mesh->m_inverse = m_faceInverse;
    }
    m_code[0] = 0;
    m_code[1] = 0;
}

// ──────────────────────────────────────────────────────────────────────────
// The view.

// FS2 0x002b2b10
// callcheck: ~GadgetView, operator_delete

SevenKeysView::~SevenKeysView()
{
}

// FS2 0x002b31f0
// callcheck: spawn, setEmitting

void SevenKeysView::prepare()
{
    m_dial = 0;
    m_sparkle = gStageScene->nodeTree->spawn(kSparkleEffect, nullptr, kSparkleSpawnPos,
                                             kSparkleSpawnRot, 0, 0);
    if (m_sparkle != nullptr)
        m_sparkle->setEmitting(false);
}

// The key's own light while in view, the rain stopped, the sparkle on its top.
// FS2 0x002b2dd0
// callcheck: pushCamera, setEmitting, find, erase, erase, pushBack, setRainEmitting, setLight, setAmbient

void SevenKeysView::enterView()
{
    pushCamera();
    if (m_object->getKind() != StageObject_SevenKeys)
        return;
    SevenKeys *keys = static_cast<SevenKeys *>(m_object);
    if (m_sparkle != nullptr) {
        m_sparkle->setEmitting(true);
        // The ROM reads the node without a test. PC: a model without bn_top
        // leaves the sparkle detached.
        auto *top = keys->m_model->boneMap.find(kSparkleBone);
        m_sparkle->setParent(top != nullptr ? top->value : nullptr);
    }
    m_dial = 0;
    gStageScene->effects->setRainEmitting(false);
    sceVu0FVECTOR direction = { 0.0f, kHalfPi, 0.0f, 0.0f };
    sceVu0FVECTOR color = { 1.0f, 1.0f, 1.0f, 0.0f };
    for (int i = 0; i < kKeyLights; i++)
        keys->m_model->data->setLight(i, direction, color);
    keys->m_model->data->setAmbient(color);
}

// FS2 0x002b2d00
// callcheck: popCamera, setEmitting, erase, setRainEmitting

void SevenKeysView::leaveView()
{
    popCamera();
    if (m_object->getKind() != StageObject_SevenKeys)
        return;
    SevenKeys *keys = static_cast<SevenKeys *>(m_object);
    if (m_sparkle != nullptr) {
        m_sparkle->setEmitting(false);
        m_sparkle->detach();
    }
    gStageScene->effects->setRainEmitting(true);
    keys->m_model->data->drawMode = 0;
}

// While no dial turns: Squat or Use leaves, left / right turn the dial
// (EVE_S11_01). Up / down pick it (0..1); the sparkle follows the dial.
// FS2 0x002b2f90
// callcheck: keysTurning, bitMask, bitMask, keysTurning, playNamedHeard2, keyUp, playNamedHeard2, keyDown, setPosition

void SevenKeysView::frame()
{
    StageCharacter *user = static_cast<StageCharacter *>(m_user);
    if (user->m_input == nullptr)
        return;
    CharacterPad *pad = user->m_input->pad();
    if (m_object->getKind() != StageObject_SevenKeys) {
        cancel();
        return;
    }
    SevenKeys *keys = static_cast<SevenKeys *>(m_object);
    if (!keys->keysTurning()
        && (pad->actionPressed(PadAction_Squat) || pad->actionPressed(PadAction_Use))) {
        cancel();
        return;
    }
    if (!keys->keysTurning()) {
        if ((pad->m_pressed & SCE_PADLleft) != 0) {
            keys->playNamedHeard2(kTurnSound, keys->m_xform.position, nullptr);
            keys->keyUp(m_dial);
        }
        if ((pad->m_pressed & SCE_PADLright) != 0) {
            keys->playNamedHeard2(kTurnSound, keys->m_xform.position, nullptr);
            keys->keyDown(m_dial);
        }
    }
    if ((pad->m_pressed & SCE_PADLup) != 0 && m_dial > 0)
        m_dial--;
    if ((pad->m_pressed & SCE_PADLdown) != 0 && m_dial < kSevenKeysDials - 1)
        m_dial++;
    if (m_sparkle != nullptr) {
        const float pos[3] = {
            (kSparklePos[0] + 0.0f) - (float)m_dial * kSparkleDialStep,
            kSparklePos[1], kSparklePos[2],
        };
        m_sparkle->m_xform.setPosition(pos);
    }
}

// ──────────────────────────────────────────────────────────────────────────
// The handle.

// FS2 0x002b2c80
// callcheck: heapAllocAligned, GadgetView

SevenKeysGadget::SevenKeysGadget()
    : SceneOwnedObject(new SevenKeysView())
{
}

// Native 0x205: the object of that name, when it is a key, takes the dials
// (a, b) and the code (c, d).
// FS2 0x002b2b70
// callcheck: findObject, getKind, setKeys, setCode

void SceneOwnedObject::setSevenKeyNumbers(const char *name, int a, int b, int c, int d)
{
    StageObject *object = m_view->findObject(name);
    if (object == nullptr || object->getKind() != StageObject_SevenKeys)
        return;
    SevenKeys *keys = static_cast<SevenKeys *>(object);
    keys->setKeys(a, b);
    keys->setCode(c, d);
}

// FS2 0x002b2c10
// callcheck: findObject, getKind, keysOpen

bool SceneOwnedObject::isSevenKeyOpen(const char *objectName)
{
    StageObject *object = m_view->findObject(objectName);
    if (object == nullptr || object->getKind() != StageObject_SevenKeys)
        return false;
    return static_cast<SevenKeys *>(object)->keysOpen();
}

// ──────────────────────────────────────────────────────────────────────────
// SkyPose.

// FS2 0x002b37d0
// callcheck: ModelPose, strcpy, memset

SkyPose::SkyPose(int dial0, int dial1)
{
    strcpy(tag, kPoseTag);
    dialPosition[0] = (float)dial0;
    dialPosition[1] = (float)dial1;
    snapDials = 1;
    memset(dialAngle, 0, sizeof(dialAngle));
}

// FS2 0x002b3770
// callcheck: ~ModelPose, operator_delete

SkyPose::~SkyPose()
{
}

// FS2 0x002b3680

void SkyPose::setDials(int dial0, int dial1)
{
    dialPosition[0] = (float)dial0;
    dialPosition[1] = (float)dial1;
    snapDials = 1;
}

// FS2 0x002b36b0
// callcheck: load, strcmp, strcmp, easeDials

int SkyPose::load(void *file, int arg, const char *boneNames, u_int boneCount_)
{
    ModelPose::load(file, arg, boneNames, boneCount_);
    for (int i = 0; i < boneCount; i++) {
        const char *name = bones[i].name;
        for (int dial = 0; dial < kSevenKeysDials; dial++) {
            if (strcmp(name, kDialBoneNames[dial]) == 0)
                dialBones[dial] = i;
        }
    }
    easeDials(dialPosition, kDialSnapFrames);
    return 1;
}

// FS2 0x002b32c0
// callcheck: advance, solve, easeDials, easeDials, sceVu0CopyVector, sceVu0CopyVector, sceVu0CopyVector, sceVu0CopyVector, buildMatrix

void SkyPose::update(float frames, bool)
{
    flags &= ~kPoseFlagFresh;
    if (current == nullptr)
        return;

    for (int i = 0; i < boneCount; i++)
        bones[i].advance(frames);
    solve(PoseSlot_Solve, -1, 1);
    if (!snapDials) {
        easeDials(dialPosition, kDialEaseFrames);
    } else {
        easeDials(dialPosition, kDialSnapFrames);
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

// NmlPose::easeDials with two dials of four: the target steps by whole turns
// until it is within half a turn, then the angle moves a 1/frames share of
// the rest ("+ 0.0" kept: it turns -0 into +0). frames < 0 counts as 1.
// FS2 0x002b3470
// callcheck: quatFromAxisAngle, quatFromAxisAngle

int SkyPose::easeDials(const float *positions, int frames)
{
    if (positions == nullptr)
        return 1;
    if (frames < 0)
        frames = 1;
    for (int i = 0; i < kSevenKeysDials; i++) {
        float target = dialAngleOf(positions[i]);
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
    // PC: as NmlPose's, a refused bone file has no bones to turn.
    if (bones == nullptr)
        return 1;
    sceVu0FVECTOR axisAngle = { kDialAxis[0], kDialAxis[1], kDialAxis[2], 0.0f };
    for (int i = 0; i < kSevenKeysDials; i++) {
        axisAngle[3] = dialAngle[i];
        quatFromAxisAngle(bones[dialBones[i]].rotation[PoseSlot_Current], axisAngle);
    }
    return 1;
}
