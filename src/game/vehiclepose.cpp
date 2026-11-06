/* ========================================================================
 *  game/vehiclepose.cpp -- CarPose and ElevatorPose (game/vehiclepose.h).
 * ======================================================================== */
#include <math.h>
#include <string.h>

#include "game/vehiclepose.h"

namespace {

constexpr float kPi = 3.14159274f;          // 0x40490fdb
constexpr float kHalfPi = 1.57079637f;      // 0x3fc90fdb
constexpr float kTwoPi = 6.28318548f;       // 0x40c90fdb

constexpr float kCarSettle = 0.2f;          // of a bone's solveWeight, a frame
constexpr float kCarLimitTargets = 10.0f;   // setDrive: speed and brake
constexpr float kCarWheelReach = 28.0f;     // an accel bone aims this far over its contact
constexpr float kCarAccelLimit = 0.157079637f;  // an accel bone's turn, a frame (0x3e20d97c)
constexpr float kCarWheelTurnLength = 175.9292f; // ground covered by one turn of a wheel
constexpr float kCarUpLength = 400.0f;      // the body aims at this point over it
constexpr float kCarLean = 0.03f;           // of steer * speed, a frame
constexpr float kCarDecay = 0.8f;           // the root's offset, off the ground

// The steps drive knows by heart: 1 and 1.2 (0x3f999999), with their powers.
constexpr float kRateFull = 1.0f;
constexpr float kRateSlow = 1.19999993f;
constexpr float kRateNear = 0.0001f;

constexpr float kElevatorOpenFrames = 120.0f;   // doors stand open before a queued close
constexpr u_int kElevatorPlay = 0x42;           // PosePlay_: what every motion here plays with

constexpr float kSolveOff = 0.01f;          // ModelPose::update's kWeightOff
constexpr float kTargetDecay = 0.7f;

inline void copyVector(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    memcpy(out, in, sizeof(sceVu0FVECTOR));
}

inline void addVector(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int lane = 0; lane < 4; lane++)
        out[lane] = a[lane] + b[lane];
}

inline void subVector(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int lane = 0; lane < 4; lane++)
        out[lane] = a[lane] - b[lane];
}

// value towards target; the ROM's "+ 0.0" stays, it turns -0 into +0.
inline void easeTo(float &value, float target, float share)
{
    value = (target - value) * share + value + 0.0f;
}

inline void setVector(sceVu0FVECTOR out, float x, float y, float z, float w)
{
    out[0] = x;
    out[1] = y;
    out[2] = z;
    out[3] = w;
}

} // namespace

// ──────────────────────────────────────────────────────────────────────
// CarPose.

// FS2 0x0027ef70
// callcheck: ModelPose, strcpy, sceVu0UnitMatrix

CarPose::CarPose()
{
    strcpy(tag, "car");     // data 0x308f28
    sceVu0UnitMatrix(bodyMatrix);
}

// FS2 0x0027ef10
// callcheck: ~ModelPose, operator_delete

CarPose::~CarPose()
{
}

// The bones the pose works on. FS2 0x0027ee00
// callcheck: load, findBone, findBone, findBone, findBone, findBone, findBone, findBone, findBone, findBone, findBone

int CarPose::load(void *file, int arg, const char *boneNames, u_int boneCount_)
{
    const int count = ModelPose::load(file, arg, boneNames, boneCount_);
    baseBone = &bones[1];
    bodyBone = findBone("bn_body");                         // data 0x308e90
    accelBones[CarWheel_FrontLeft] = findBone("bn_fl_accel");
    accelBones[CarWheel_FrontRight] = findBone("bn_fr_accel");
    accelBones[CarWheel_RearLeft] = findBone("bn_rl_accel");
    accelBones[CarWheel_RearRight] = findBone("bn_rr_accel");
    wheelBones[CarWheel_FrontLeft] = findBone("bn_fl_wheel");
    wheelBones[CarWheel_FrontRight] = findBone("bn_fr_wheel");
    wheelBones[CarWheel_RearLeft] = findBone("bn_rl_wheel");
    wheelBones[CarWheel_RearRight] = findBone("bn_rr_wheel");
    steeringBone = findBone("bn_str_w");
    return count;
}

// ModelPose::update without the walk and the solvers: followGround is the
// solver. FS2 0x0027dc40
// callcheck: advance, sceVu0CopyVector, sceVu0CopyVector, sceVu0AddVector, solve, followGround, interpolate, buildMatrix, sceVu0CopyMatrix

void CarPose::update(float frames, bool loop)
{
    flags &= ~kPoseFlagFresh;
    if (current == nullptr)
        return;

    for (int i = 0; i < boneCount; i++) {
        ModelPoseBone *bone = &bones[i];
        bone->advance(frames);
        copyVector(bone->position[PoseSlot_Solve], bone->position[PoseSlot_Current]);
        copyVector(bone->rotation[PoseSlot_Solve], bone->rotation[PoseSlot_Current]);
        if (i == 1)
            addVector(bone->position[PoseSlot_Solve], bone->position[PoseSlot_Solve], rootOffset);
    }
    solve(PoseSlot_Solve, -1, 1);
    followGround();

    for (int i = 0; i < boneCount; i++) {
        ModelPoseBone *bone = &bones[i];
        if (bone->parent == nullptr) {
            copyVector(bone->position[PoseSlot_Final], bone->position[PoseSlot_Solve]);
            copyVector(bone->rotation[PoseSlot_Final], bone->rotation[PoseSlot_Solve]);
        } else {
            bone->interpolate(bone->solveWeight, PoseSlot_Final, PoseSlot_Current, PoseSlot_Solve);
            bone->buildMatrix(frames, PoseSlot_Final, 1);
        }
        bone->playFlags &= ~PosePlay_Moved;
    }
    // PC: the ROM reads through the null of a model with no bn_body.
    if (bodyBone != nullptr)
        memcpy(bodyMatrix, bodyBone->matrix, sizeof(bodyMatrix));
}

// On the ground: the root rides at the height of the four contacts, the body
// aims up off their plane less the tilt, each accel bone reaches for its
// contact, the wheels roll by how far they moved, the front ones and the
// steering wheel turn by steer, and the whole car leans around the rear axle
// by steer * speed. Off it: every bone lets go. FS2 0x0027de20
// callcheck: settle, settle, settle, settle, settle, settle, settle, settle, settle, settle, sceVu0ScaleVector, getWheelPoints, sceVu0InterVector, sceVu0SubVector, sceVu0SubVector, sceVu0ScaleVector, sceVu0AddVector, solve, getPosition, sceVu0OuterProduct, sceVu0Normalize, sceVu0ScaleVector, sceVu0SubVector, sceVu0AddVector, aimAlongAxis, blendSolve, settleFixed, settleFixed, solve, getPosition, aimAlongAxis, settleFixed, sceVu0SubVector, turnVector, quatFromAxisAngle, quatFromAxisAngle, settleFixed, settleFixed, quatFromAxisAngle, settleFixed, sceVu0AddVector, sceVu0ApplyMatrix, sceVu0UnitMatrix, sceVu0RotMatrix, sceVu0SubVector, sceVu0ApplyMatrix, sceVu0AddVector, sceVu0SubVector, sceVu0AddVector, sceVu0ApplyMatrix, sceVu0ApplyMatrix, sceVu0MulMatrix, sceVu0AddVector, wrapAngleLane (the SDK's vector calls, as lanes here)

void CarPose::followGround()
{
    if (wheelBones[CarWheel_FrontLeft] == nullptr)
        return;

    if (contact[CarWheel_FrontLeft][3] == 0.0f) {
        baseBone->settle(kCarSettle, 1);
        bodyBone->settle(kCarSettle, 1);
        for (int i = 0; i < kCarWheels; i++)
            accelBones[i]->settle(kCarSettle, 1);
        for (int i = 0; i < kCarWheels; i++)
            wheelBones[i]->settle(kCarSettle, 1);
        scaleVec4(kCarDecay, rootOffset, rootOffset);
        return;
    }

    sceVu0FVECTOR points[kCarWheels];
    getWheelPoints(points[CarWheel_FrontLeft], points[CarWheel_FrontRight],
                   points[CarWheel_RearLeft], points[CarWheel_RearRight]);
    for (int i = 0; i < kCarWheels; i++)
        points[i][2] = contact[i][2];

    sceVu0FVECTOR rearMiddle;
    blendVec4(0.5f, rearMiddle, points[CarWheel_RearLeft], points[CarWheel_RearRight]);
    sceVu0FVECTOR across;
    subVector(across, points[CarWheel_FrontRight], points[CarWheel_FrontLeft]);
    sceVu0FVECTOR along;
    subVector(along, rearMiddle, points[CarWheel_FrontLeft]);
    sceVu0FVECTOR centre;
    scaleVec4(0.5f, centre, along);
    addVector(centre, centre, points[CarWheel_FrontLeft]);

    rootOffset[1] = ((centre[2] - position[2]) - (rootOffset[1] + 0.0f)) * kCarSettle
        + rootOffset[1] + 0.0f;
    solve(PoseSlot_Solve, -1, 0);

    sceVu0FVECTOR bodyPosition;
    bodyBone->getPosition(bodyPosition);
    sceVu0FVECTOR up;
    vecCross(up, along, across);
    vecNormalize(up, up);
    scaleVec4(kCarUpLength, up, up);
    subVector(up, up, tilt);
    addVector(up, up, bodyPosition);
    bodyBone->aimAlongAxis(kPi, up, 1);
    bodyBone->blendSolve(kCarSettle, PoseSlot_Final);
    baseBone->settleFixed(kCarSettle, 1);
    bodyBone->settleFixed(kCarSettle, 1);
    solve(PoseSlot_Solve, -1, 0);

    for (int i = 0; i < kCarWheels; i++) {
        ModelPoseBone *wheel = wheelBones[i];
        wheel->getPosition(points[i]);
        points[i][2] = contact[i][2] + kCarWheelReach;
        accelBones[i]->aimAlongAxis(kCarAccelLimit, points[i], 0);
        accelBones[i]->settleFixed(kCarSettle, 1);

        sceVu0FVECTOR moved;
        subVector(moved, wheel->worldPosition, wheel->lastWorldPosition);
        sceVu0FVECTOR axisAngle;
        setVector(axisAngle, 1.0f, 0.0f, 0.0f, 1.0f);
        float travel = sqrtf(moved[0] * moved[0] + moved[1] * moved[1]);
        bodyBone->turnVector(axisAngle, axisAngle);
        if (moved[0] * axisAngle[0] + moved[1] * axisAngle[1] < 0.0f)
            travel *= -1.0f;
        if (i == CarWheel_FrontRight || i == CarWheel_RearRight)
            travel *= -1.0f;

        wheelAngle[i] = (travel / kCarWheelTurnLength) * kTwoPi + wheelAngle[i] + 0.0f;
        while (kPi < wheelAngle[i])
            wheelAngle[i] -= kTwoPi;
        while (wheelAngle[i] < -kPi)
            wheelAngle[i] += kTwoPi;

        setVector(axisAngle, 1.0f, 0.0f, 0.0f, wheelAngle[i]);
        quatFromAxisAngle(wheel->rotation[PoseSlot_Solve], axisAngle);
        if (i == CarWheel_FrontLeft || i == CarWheel_FrontRight) {
            setVector(axisAngle, 0.0f, 1.0f, 0.0f, steer);
            quatFromAxisAngle(wheel->parent->rotation[PoseSlot_Solve], axisAngle);
            wheel->settleFixed(kCarSettle, 2);
        } else {
            wheel->settleFixed(kCarSettle, 1);
        }
    }

    if (steeringBone != nullptr) {
        sceVu0FVECTOR axisAngle;
        setVector(axisAngle, 1.0f, 0.0f, 0.0f, steer * 4.0f);
        quatFromAxisAngle(steeringBone->rotation[PoseSlot_Solve], axisAngle);
        steeringBone->settleFixed(kCarSettle, 2);
    }

    addVector(position, position, drift);

    float lean = steer * speed;
    if (kHalfPi < lean)
        lean = kHalfPi;
    if (lean < -kHalfPi)
        lean = -kHalfPi;
    sceVu0FVECTOR turn;
    setVector(turn, 0.0f, 0.0f, lean * kCarLean, 0.0f);
    sceVu0FVECTOR pivot;
    setVector(pivot, 0.0f, 0.0f, 0.0f, 1.0f);
    applyMatrix(pivot, accelBones[CarWheel_RearLeft]->parent->matrix, pivot);
    sceVu0FMATRIX leanMatrix;
    sceVu0UnitMatrix(leanMatrix);
    sceVu0RotMatrixZ(leanMatrix, leanMatrix, turn[2]);  // the ROM's sceVu0RotMatrix: x and y are 0

    sceVu0FVECTOR offset;
    subVector(offset, position, pivot);
    offset[3] = 1.0f;
    applyMatrix(offset, leanMatrix, offset);
    addVector(offset, pivot, offset);
    subVector(offset, offset, position);
    addVector(position, position, offset);

    applyMatrix(rootBone->rawPosition, leanMatrix, rootBone->rawPosition);
    applyMatrix(rootBone->lastRawPosition, leanMatrix, rootBone->lastRawPosition);
    multiplyMatrix(placement, leanMatrix, placement);
    addVector(rotation, rotation, turn);
    // The ROM calls this and drops the answer, so the lanes stay as they are.
    for (int lane = 0; lane < 3; lane++) {
        if (kPi < fabsf(rotation[lane]))
            (void)wrapAngleLane(rotation[lane]);
    }
}

// FS2 0x0027e650
// callcheck: sceVu0CopyVector, sceVu0CopyVector, sceVu0CopyVector, sceVu0CopyVector, powf, powf, turnVector, sceVu0ScaleVector, sceVu0SubVector, sceVu0ScaleVector, sceVu0CopyVector, powf (the SDK's vector calls, as lanes here)

void CarPose::drive(float rate, const sceVu0FVECTOR frontLeft, const sceVu0FVECTOR frontRight,
                    const sceVu0FVECTOR rearLeft, const sceVu0FVECTOR rearRight)
{
    if (frontLeft == nullptr || frontRight == nullptr || rearLeft == nullptr || rearRight == nullptr) {
        memset(contact, 0, sizeof(contact));
        memset(lastVelocity, 0, sizeof(lastVelocity));
        memset(drift, 0, sizeof(drift));
        memset(tilt, 0, sizeof(tilt));
        speed = speedTarget = 0.0f;
        brake = brakeTarget = 0.0f;
        steer = steerTarget = 0.0f;
        memset(wheelAngle, 0, sizeof(wheelAngle));
        memset(_440, 0, sizeof(_440));
        return;
    }

    copyVector(contact[CarWheel_FrontLeft], frontLeft);
    copyVector(contact[CarWheel_FrontRight], frontRight);
    copyVector(contact[CarWheel_RearLeft], rearLeft);
    copyVector(contact[CarWheel_RearRight], rearRight);
    for (int i = 0; i < kCarWheels; i++)
        contact[i][3] = 1.0f;

    if (fabsf(rate - kRateFull) <= kRateNear) {
        easeTo(speed, speedTarget, 0.02f);
        easeTo(brake, brakeTarget, 0.1f);
        easeTo(steer, steerTarget, 0.1f);
    } else if (fabsf(rate - kRateSlow) <= kRateNear) {
        easeTo(speed, speedTarget, 0.0239517409f);  // 1 - 0.98^1.2
        easeTo(brake, brakeTarget, 0.118766472f);   // 1 - 0.9^1.2
        easeTo(steer, steerTarget, 0.118766472f);
    } else {
        easeTo(speed, speedTarget, 1.0f - powf(0.98f, rate));
        const float share = 1.0f - powf(0.9f, rate);
        easeTo(brake, brakeTarget, share);
        easeTo(steer, steerTarget, share);
    }

    if (fabsf(speed) < 0.3f && fabsf(speedTarget) < 0.1f) {
        speed *= 0.8f;
        speedTarget *= 0.8f;
    }
    speed *= (10.0f - brake) / 10.0f;

    sceVu0FVECTOR velocity;
    setVector(velocity, 1.0f, 0.0f, 0.0f, 1.0f);
    bones[1].turnVector(velocity, velocity);
    scaleVec4(speed * 0.7f, velocity, velocity);
    subVector(tilt, velocity, lastVelocity);
    scaleVec4(100.0f, tilt, tilt);
    for (int lane = 0; lane < 2; lane++) {
        if (30.0f < tilt[lane])
            tilt[lane] = 30.0f;
        if (tilt[lane] < -30.0f)
            tilt[lane] = -30.0f;
    }
    copyVector(lastVelocity, velocity);

    float gain, keep;
    if (fabsf(rate - kRateFull) <= kRateNear) {
        gain = 1.0f;
        keep = 0.8f;
    } else if (fabsf(rate - kRateSlow) <= kRateNear) {
        gain = 1.44000006f;     // 0x3fb851ec
        keep = 0.765082002f;    // 0x3f43dc6a: 0.8^1.2
    } else {
        gain = rate * rate;
        keep = powf(0.8f, rate);
    }
    for (int lane = 0; lane < 3; lane++)
        drift[lane] = keep * (gain * velocity[lane] + drift[lane] + 0.0f);
}

// The corners in the body's space (0x42b76148, 0xbf0cccc0, 0x42640000).
// FS2 0x0027ec80
// callcheck: sceVu0ApplyMatrix, sceVu0ApplyMatrix, sceVu0ApplyMatrix, sceVu0ApplyMatrix (applyMatrix)

void CarPose::getWheelPoints(sceVu0FVECTOR frontLeft, sceVu0FVECTOR frontRight,
                             sceVu0FVECTOR rearLeft, sceVu0FVECTOR rearRight) const
{
    constexpr float kLength = 91.6900024f;
    constexpr float kDrop = -0.549999237f;
    constexpr float kWidth = 57.0f;
    setVector(frontLeft, kLength, kDrop, -kWidth, 1.0f);
    setVector(frontRight, kLength, kDrop, kWidth, 1.0f);
    setVector(rearLeft, -kLength, kDrop, -kWidth, 1.0f);
    setVector(rearRight, -kLength, kDrop, kWidth, 1.0f);
    applyMatrix(frontLeft, bodyMatrix, frontLeft);
    applyMatrix(frontRight, bodyMatrix, frontRight);
    applyMatrix(rearLeft, bodyMatrix, rearLeft);
    applyMatrix(rearRight, bodyMatrix, rearRight);
}

// FS2 0x0027ed70

void CarPose::setDrive(float speed_, float brake_, float steer_)
{
    if (kCarLimitTargets < speed_)
        speed_ = kCarLimitTargets;
    if (kCarLimitTargets < brake_)
        brake_ = kCarLimitTargets;
    if (kHalfPi < steer_)
        steer_ = kHalfPi;
    if (steer_ < -kHalfPi)
        steer_ = -kHalfPi;
    speedTarget = speed_;
    brakeTarget = brake_;
    steerTarget = steer_;
}

// ──────────────────────────────────────────────────────────────────────
// ElevatorPose.

// PC: the ROM's ctor leaves motions and the queue past its second entry to the
// heap; the members' initialisers clear them.
// FS2 0x0027ffb0
// callcheck: ModelPose, strcpy

ElevatorPose::ElevatorPose()
{
    strcpy(tag, "elv");     // data 0x308f70
}

// FS2 0x0027ff50
// callcheck: ~ModelPose, operator_delete

ElevatorPose::~ElevatorPose()
{
}

// FS2 0x0027ff40

int ElevatorPose::load(void *file, int arg, const char *boneNames, u_int boneCount_)
{
    return ModelPose::load(file, arg, boneNames, boneCount_);
}

// ModelPose::update with the queue first, no walk and no solvers but the
// weights. FS2 0x0027f040
// callcheck: stepQueue, advance, sceVu0CopyVector, sceVu0CopyVector, sceVu0AddVector, solve, interpolate, interpolate, buildMatrix

void ElevatorPose::update(float frames, bool loop)
{
    flags &= ~kPoseFlagFresh;
    timer += frames;
    if (current == nullptr)
        return;
    stepQueue();

    for (int i = 0; i < boneCount; i++) {
        ModelPoseBone *bone = &bones[i];
        bone->advance(frames);
        copyVector(bone->position[PoseSlot_Solve], bone->position[PoseSlot_Current]);
        copyVector(bone->rotation[PoseSlot_Solve], bone->rotation[PoseSlot_Current]);
        if (i == 1)
            addVector(bone->position[PoseSlot_Solve], bone->position[PoseSlot_Solve], rootOffset);
    }
    solve(PoseSlot_Solve, -1, 1);

    for (int i = 0; i < boneCount; i++) {
        ModelPoseBone *bone = &bones[i];
        if (bone->parent == nullptr) {
            copyVector(bone->position[PoseSlot_Final], bone->position[PoseSlot_Solve]);
            copyVector(bone->rotation[PoseSlot_Final], bone->rotation[PoseSlot_Solve]);
        } else {
            if (kSolveOff < bone->solveWeight) {
                bone->interpolate(bone->solveWeight, PoseSlot_Final, PoseSlot_Current, PoseSlot_Solve);
            } else {
                copyVector(bone->position[PoseSlot_Final], bone->position[PoseSlot_Current]);
                copyVector(bone->rotation[PoseSlot_Final], bone->rotation[PoseSlot_Current]);
            }
            if (kSolveOff < bone->targetWeight) {
                bone->interpolate(bone->targetWeight, PoseSlot_Final, PoseSlot_Final, PoseSlot_Target);
                bone->targetWeight *= kTargetDecay;
            }
            bone->buildMatrix(frames, PoseSlot_Final, 1);
        }
        bone->playFlags &= ~PosePlay_Moved;
    }
}

// When the motion ends: this side's lamps go out, and the queue goes on. The
// pair open, close waits until the doors have stood open long enough.
// FS2 0x0027f2b0
// callcheck: isBoneEnding, playFileLayered, playFileLayered, playFile

void ElevatorPose::stepQueue()
{
    if (!isBoneEnding(nullptr))
        return;

    u_int step = state & kElevatorStepMask;
    u_int side = state & kElevatorSideMask;
    if (step == ElevatorStep_Opening)
        step = ElevatorStep_Open;
    if (step == ElevatorStep_Closing || step == ElevatorStep_RideToB3 || step == ElevatorStep_RideToB1)
        step = ElevatorStep_Closed;

    // The ROM then tests for the two rides, which the line above has ruled out.
    if (side == kElevatorAtB1) {
        playFileLayered(motions[ElevatorMotion_LampInsideB1Off], kElevatorPlay, 0);
        playFileLayered(motions[ElevatorMotion_LampFloorB1Off], kElevatorPlay, 0);
    } else {
        playFileLayered(motions[ElevatorMotion_LampInsideB3Off], kElevatorPlay, 0);
        playFileLayered(motions[ElevatorMotion_LampFloorB3Off], kElevatorPlay, 0);
    }

    const int at = queueStep;
    if (0 < at && queue[at] == ElevatorStep_Open && queue[at + 1] == ElevatorStep_Closed) {
        if (step == ElevatorStep_Open && kElevatorOpenFrames < timer) {
            playFile(motions[side == kElevatorAtB1 ? ElevatorMotion_CloseB1 : ElevatorMotion_CloseB3],
                     kElevatorPlay, 0);
            step = ElevatorStep_Closing;
            queueStep = at + 1;
        }
    } else {
        if (queue[at] != ElevatorStep_End)
            queueStep++;
        if (queueStep == 0)
            queueStep = 1;
        switch (queue[queueStep]) {
        case ElevatorStep_RideToB3:
            playFile(motions[ElevatorMotion_RideB3], kElevatorPlay, 0);
            step = ElevatorStep_RideToB3;
            side = 0;
            break;
        case ElevatorStep_RideToB1:
            playFile(motions[ElevatorMotion_RideB1], kElevatorPlay, 0);
            step = ElevatorStep_RideToB1;
            side = kElevatorAtB1;
            break;
        case ElevatorStep_Closed:
            playFile(motions[side == kElevatorAtB1 ? ElevatorMotion_CloseB1 : ElevatorMotion_CloseB3],
                     kElevatorPlay, 0);
            step = ElevatorStep_Closing;
            break;
        case ElevatorStep_Open:
            playFile(motions[side == kElevatorAtB1 ? ElevatorMotion_OpenB1 : ElevatorMotion_OpenB3],
                     kElevatorPlay, 0);
            timer = 0.0f;
            step = ElevatorStep_Opening;
            break;
        default:
            break;
        }
    }
    state = side | step;
}

// FS2 0x0027f5a0
// callcheck: playFile, playFile

void ElevatorPose::reopenDoors()
{
    if (state & kElevatorAtB1) {
        playFile(motions[ElevatorMotion_OpenB1], kElevatorPlay, 0);
        state = kElevatorAtB1 | ElevatorStep_Opening;
    } else {
        playFile(motions[ElevatorMotion_OpenB3], kElevatorPlay, 0);
        state = ElevatorStep_Opening;
    }
    timer = 0.0f;

    const int at = queueStep;
    if (0 < at && queue[at - 1] == ElevatorStep_Open && queue[at] == ElevatorStep_Closed) {
        queueStep = at - 1;
    } else {
        for (int i = kElevatorQueue - 2; at < i; i--)
            queue[i + 1] = queue[i];
        queue[at] = ElevatorStep_Open;
        queue[at + 1] = ElevatorStep_Closed;
    }
}

// FS2 0x0027f7d0
// callcheck: playFileLayered, playFileLayered, playFileLayered, playFileLayered

int ElevatorPose::release()
{
    const u_int step = state & kElevatorStepMask;
    int length = 1;
    if (step == ElevatorStep_Opening || step == ElevatorStep_Open) {
        length = 2;
        queue[1] = ElevatorStep_Closed;
        queueStep = 0;
    }
    playFileLayered(motions[ElevatorMotion_LampInsideB1Off], kElevatorPlay, 0);
    playFileLayered(motions[ElevatorMotion_LampFloorB1Off], kElevatorPlay, 0);
    playFileLayered(motions[ElevatorMotion_LampInsideB3Off], kElevatorPlay, 0);
    playFileLayered(motions[ElevatorMotion_LampFloorB3Off], kElevatorPlay, 0);
    return length;
}

// FS2 0x0027f880
// callcheck: playFileLayered

int ElevatorPose::callInsideB3()
{
    const u_int step = state & kElevatorStepMask;
    int length = 1;
    if (!(state & kElevatorAtB1)) {
        if (step == ElevatorStep_Closing || step == ElevatorStep_Closed) {
            length = 3;
            queue[1] = ElevatorStep_Open;
            queue[2] = ElevatorStep_End;
            queueStep = 0;
        }
    } else if (step == ElevatorStep_Closing || step == ElevatorStep_Closed || step == ElevatorStep_Opening
               || step == ElevatorStep_Open || step == ElevatorStep_RideToB1) {
        if (step == ElevatorStep_RideToB1) {
            queue[1] = ElevatorStep_Open;
            length = 2;
        }
        if (step != ElevatorStep_Closing && step != ElevatorStep_Closed)
            queue[length++] = ElevatorStep_Closed;
        queue[length++] = ElevatorStep_RideToB3;
        queue[length++] = ElevatorStep_Open;
        queue[length++] = ElevatorStep_End;
        queueStep = 0;
    }
    playFileLayered(motions[ElevatorMotion_LampInsideB3On], kElevatorPlay, 0);
    return length;
}

// FS2 0x0027f9a0
// callcheck: playFileLayered

int ElevatorPose::callInsideB1()
{
    const u_int step = state & kElevatorStepMask;
    int length = 1;
    if (state & kElevatorAtB1) {
        if (step == ElevatorStep_Closing || step == ElevatorStep_Closed) {
            length = 3;
            queue[1] = ElevatorStep_Open;
            queue[2] = ElevatorStep_End;
            queueStep = 0;
        }
    } else if (step == ElevatorStep_Closing || step == ElevatorStep_Closed || step == ElevatorStep_Opening
               || step == ElevatorStep_Open || step == ElevatorStep_RideToB3) {
        if (step == ElevatorStep_RideToB3) {
            queue[1] = ElevatorStep_Open;
            length = 2;
        }
        if (step != ElevatorStep_Closing && step != ElevatorStep_Closed)
            queue[length++] = ElevatorStep_Closed;
        queue[length++] = ElevatorStep_RideToB1;
        queue[length++] = ElevatorStep_Open;
        queue[length++] = ElevatorStep_End;
        queueStep = 0;
    }
    playFileLayered(motions[ElevatorMotion_LampInsideB1On], kElevatorPlay, 0);
    return length;
}

// FS2 0x0027fac0
// callcheck: reopenDoors, playFileLayered

int ElevatorPose::callFloorB3()
{
    const u_int step = state & kElevatorStepMask;
    int length = 1;
    if (!(state & kElevatorAtB1)) {
        if (step == ElevatorStep_Closing) {
            reopenDoors();
        } else if (step == ElevatorStep_Closed || step == ElevatorStep_RideToB3
                   || step == ElevatorStep_RideToB1) {
            if (step == ElevatorStep_RideToB1) {
                queue[1] = ElevatorStep_Open;
                queue[2] = ElevatorStep_Closed;
                length = 4;
                queue[3] = ElevatorStep_RideToB3;
            }
            queue[length++] = ElevatorStep_Open;
            queue[length++] = ElevatorStep_End;
            queueStep = 0;
        }
    } else {
        switch (step) {
        case ElevatorStep_RideToB1:
            queue[1] = ElevatorStep_Open;
            length = 2;
            // fall through
        case ElevatorStep_Open:
        case ElevatorStep_Opening:
            if (length == 1) {
                length = 2;
                queue[1] = ElevatorStep_Open;
            }
            queue[length++] = ElevatorStep_Closed;
            // fall through
        case ElevatorStep_Closed:
        case ElevatorStep_Closing:
            queue[length++] = ElevatorStep_RideToB3;
            // fall through
        case ElevatorStep_RideToB3:
            queue[length++] = ElevatorStep_Open;
            queue[length++] = ElevatorStep_End;
            queueStep = 0;
            break;
        default:
            break;
        }
    }
    playFileLayered(motions[ElevatorMotion_LampFloorB3On], kElevatorPlay, 0);
    return length;
}

// FS2 0x0027fc40
// callcheck: reopenDoors, playFileLayered

int ElevatorPose::callFloorB1()
{
    const u_int step = state & kElevatorStepMask;
    int length = 1;
    if (state & kElevatorAtB1) {
        if (step == ElevatorStep_Closing) {
            reopenDoors();
        } else if (step == ElevatorStep_Closed || step == ElevatorStep_RideToB1
                   || step == ElevatorStep_RideToB3) {
            if (step == ElevatorStep_RideToB3) {
                queue[1] = ElevatorStep_Open;
                queue[2] = ElevatorStep_Closed;
                length = 4;
                queue[3] = ElevatorStep_RideToB1;
            }
            queue[length++] = ElevatorStep_Open;
            queue[length++] = ElevatorStep_End;
            queueStep = 0;
        }
    } else {
        switch (step) {
        case ElevatorStep_RideToB3:
            queue[1] = ElevatorStep_Open;
            length = 2;
            // fall through
        case ElevatorStep_Open:
        case ElevatorStep_Opening:
            if (length == 1) {
                length = 2;
                queue[1] = ElevatorStep_Open;
            }
            queue[length++] = ElevatorStep_Closed;
            // fall through
        case ElevatorStep_Closed:
        case ElevatorStep_Closing:
            queue[length++] = ElevatorStep_RideToB1;
            // fall through
        case ElevatorStep_RideToB1:
            queue[length++] = ElevatorStep_Open;
            queue[length++] = ElevatorStep_End;
            queueStep = 0;
            break;
        default:
            break;
        }
    }
    playFileLayered(motions[ElevatorMotion_LampFloorB1On], kElevatorPlay, 0);
    return length;
}

// FS2 0x0027fdc0
// callcheck: playFile, playFile, callInsideB1, callInsideB3, playFile, playFile, playFileLayered, playFileLayered, playFileLayered, playFileLayered

int ElevatorPose::setScene(int scene)
{
    switch (scene) {
    case ElevatorScene_ClosedB1:
        playFile(motions[ElevatorMotion_StandClosedB1], kElevatorPlay, 0);
        state = kElevatorAtB1 | ElevatorStep_Closed;
        break;
    case ElevatorScene_OpenB1:
        playFile(motions[ElevatorMotion_StandOpenB1], kElevatorPlay, 0);
        state = kElevatorAtB1 | ElevatorStep_Open;
        break;
    case ElevatorScene_CallB3:
        callInsideB3();
        break;
    case ElevatorScene_CallB1:
        callInsideB1();
        break;
    case ElevatorScene_ClosedB3:
        playFile(motions[ElevatorMotion_StandClosedB3], kElevatorPlay, 0);
        state = ElevatorStep_Closed;
        break;
    case ElevatorScene_OpenB3:
        playFile(motions[ElevatorMotion_StandOpenB3], kElevatorPlay, 0);
        state = ElevatorStep_Open;
        break;
    default:
        break;
    }
    playFileLayered(motions[ElevatorMotion_LampInsideB1Off], kElevatorPlay, 0);
    playFileLayered(motions[ElevatorMotion_LampFloorB1Off], kElevatorPlay, 0);
    playFileLayered(motions[ElevatorMotion_LampInsideB3Off], kElevatorPlay, 0);
    playFileLayered(motions[ElevatorMotion_LampFloorB3Off], kElevatorPlay, 0);
    return scene;
}

// FS2 0x0027ff10

void ElevatorPose::setMotionFile(void *file, u_int motion)
{
    motions[motion] = file;
    if (motion == ElevatorMotion_OpenB1 && file != nullptr)
        state = kElevatorAtB1 | ElevatorStep_Opening;
}
