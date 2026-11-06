/* ==========================================================================
 *  game/modelpose.cpp
 *
 *  Skeletons and motion playback: a bone file becomes the bones of a pose, a
 *  motion file's tracks are bound to them, update walks the keys and solve
 *  turns what was played into matrices. On top of that the solvers under
 *  m_solvers bend the played pose towards targets, and the sw bones swing on
 *  springs.
 * ======================================================================== */
#include "game/modelpose.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <new>

#include "game/heap.h"

namespace {

constexpr float kPi     = 3.14159274f;
constexpr float kHalfPi = 1.57079637f;
constexpr float kTwoPi  = 6.28318548f;

constexpr u_int kBoneAlign   = 0x80;
constexpr u_int kTrackAlign  = 0x10;
constexpr float kLowHeight   = 3.0f;   // ModelPoseBone::lowFrames
constexpr float kProbeHeight = 100.0f; // ModelPose::load: how far under a bone it looks
constexpr float kAimScale    = 10000.0f; // ModelPoseBone::aimAxis, before normalizing
constexpr float kWeightOff   = 0.01f;  // ModelPose::update: under it a solver's weight is none
constexpr float kTargetDecay = 0.7f;
constexpr float kFootDecay   = 0.8f;
constexpr float kFootContact = 0.1f;
constexpr float kWeightFull  = 0.99f;  // ModelPoseBone::settleFixed: over it a solver's weight is whole
constexpr float kSwingDrop   = 30.0f;  // how far from a sw bone its spring's rest point lies
constexpr float kSwingLength = 15.0f;  // a sw bone's length: its record holds swingParams instead
constexpr float kWalkTurnDone = 0.0349065848f; // two degrees
constexpr float kWalkTurnLead = 3.0f;  // frames the turn ends before the walk
constexpr float kHeadDrop    = 11.0f;  // ModelPose::setHeadTarget

// Bones by what they belong to, the groups of PoseBoneGroup (data 0x2f5090..;
// the ROM's lists are 16-byte names ended by a '/').
const char *const kLowerBones[] = {     // data 0x2f5470
    "bn_base", "bn_status", "bn_pelvis", "bn_pelvis!", "bn_l_thigh!", "bn_r_thigh!", "bn_l_thigh",
    "bn_r_thigh", "bn_l_calf", "bn_r_calf", "bn_l_foot", "bn_r_foot", "bn_l_toe", "bn_r_toe", nullptr,
};
const char *const kUpperBones[] = {     // data 0x2f5330
    "bn_chin", "bn_face", "bn_spine0", "bn_spine1", "bn_neck", "bn_head", "bn_clavicle!",
    "bn_l_clavicle", "bn_r_clavicle", "bn_l_upperarm!", "bn_l_upperarm", "bn_l_foretwist0",
    "bn_l_foretwist1", "bn_l_hand", "bn_r_upperarm!", "bn_r_upperarm", "bn_r_foretwist0",
    "bn_r_foretwist1", "bn_r_hand", nullptr,
};
const char *const kFaceBones[]  = { "bn_chin", "bn_face", nullptr };   // data 0x2f5090
const char *const kSpineBones[] = { "bn_spine0", "bn_spine1", "bn_neck", "bn_head", nullptr }; // data 0x2f50c0
const char *const kClavicleBones[] = { "bn_clavicle!", "bn_l_clavicle", "bn_r_clavicle", nullptr }; // data 0x2f5110
const char *const kLeftArmBones[] = {   // data 0x2f5150
    "bn_l_upperarm!", "bn_l_upperarm", "bn_l_foretwist0", "bn_l_foretwist1", "bn_l_hand", nullptr,
};
const char *const kRightArmBones[] = {  // data 0x2f51b0
    "bn_r_upperarm!", "bn_r_upperarm", "bn_r_foretwist0", "bn_r_foretwist1", "bn_r_hand", nullptr,
};
const char *const kLeftHandBones[] = {  // data 0x2f5210
    "bn_l_finger00", "bn_l_finger01", "bn_l_finger10", "bn_l_finger11", "bn_l_finger20",
    "bn_l_finger21", "bn_l_tool", nullptr,
};
const char *const kRightHandBones[] = { // data 0x2f5290
    "bn_r_finger00", "bn_r_finger01", "bn_r_finger10", "bn_r_finger11", "bn_r_finger20",
    "bn_r_finger21", "bn_r_tool", nullptr,
};
const char *const kStatusBones[] = { "bn_status", nullptr };           // data 0x2f5310
// The ROM's list has no '/': its scan runs on through the data after it, which
// holds no bone name.
const char *const kTrunkBones[] = {     // data 0x2f5560
    "bn_base", "bn_pelvis", "bn_pelvis!", "bn_spine0", "bn_spine1", "bn_spine2", "bn_neck", "bn_head",
    "bn_l_thigh!", "bn_r_thigh!", "bn_l_thigh", "bn_r_thigh", "bn_l_calf", "bn_r_calf", "bn_l_foot",
    "bn_r_foot", "bn_l_toe", "bn_r_toe", nullptr,
};

// layout: host -- a table of the port, never in the ROM.
struct BoneGroupList
{
    const char *const *names;
    u_short            group;
};
const BoneGroupList kBoneGroups[] = {
    { kLowerBones, PoseGroup_Lower },       { kUpperBones, PoseGroup_Upper },
    { kFaceBones, PoseGroup_Face },         { kSpineBones, PoseGroup_Spine },
    { kClavicleBones, PoseGroup_Clavicle }, { kLeftArmBones, PoseGroup_LeftArm },
    { kRightArmBones, PoseGroup_RightArm }, { kLeftHandBones, PoseGroup_LeftHand },
    { kRightHandBones, PoseGroup_RightHand }, { kStatusBones, PoseGroup_Status },
    { kTrunkBones, PoseGroup_Trunk },
};

// The growth of solve: bones change size along a motion played with
// PosePlay_Grow, x, y and z each by a table of (bone, knot).
constexpr int kGrowthBones = 10;
constexpr int kGrowthKnots = 7;
const char *const kGrowthBoneNames[kGrowthBones] = {    // data 0x2f5690
    "bn_pelvis", "bn_spine0", "bn_spine1", "bn_spine2", "bn_neck",
    "bn_head", "bn_l_thigh", "bn_l_calf", "bn_l_foot", "bn_l_toe",
};
const float kGrowthKnotAt[kGrowthKnots] = { 0.0f, 0.1f, 0.15f, 0.2f, 0.4f, 0.5f, 1.0f }; // data 0x2f56c0
// The three tables, one row per kGrowthBoneNames.
const float kGrowth[3][kGrowthBones][kGrowthKnots] = {
    {   // data 0x2f56e0
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.2f },
        { 1.0f, 1.2f, 1.0f, 1.0f, 1.0f, 0.80000001f, 0.40000001f },
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.4f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.1f, 1.0f, 1.1f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.5f, 1.0f, 1.7f, 1.0f, 1.2f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.60000002f },
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.4f },
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.07f, 0.60000002f },
    },
    {   // data 0x2f5800
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.2f },
        { 1.0f, 1.0f, 0.69999999f, 0.69999999f, 1.0f, 0.60000002f, 0.40000001f },
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.8f, 0.69999999f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 2.7f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 0.80000001f, 1.0f, 1.6f, 1.0f },
        { 1.0f, 0.69999999f, 0.80000001f, 0.89999998f, 1.0f, 1.0f, 0.2f },
        { 1.0f, 1.0f, 2.3f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.1f, 5.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.6f },
    },
    {   // data 0x2f5920
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.2f },
        { 1.0f, 0.60000002f, 0.60000002f, 0.60000002f, 1.0f, 0.80000001f, 0.40000001f },
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 2.5f, 0.5f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.8f, 1.0f, 2.5f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.7f, 1.0f, 1.5f, 1.0f },
        { 1.0f, 0.5f, 0.40000001f, 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 3.0999999f, 0.40000001f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.69999999f },
        { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f },
    },
};

// FS2 0x001498a0
inline void copyVector(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    memcpy(out, in, sizeof(sceVu0FVECTOR));
}

// vsub.xyzw, vadd.xyzw.
inline void subVector(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int lane = 0; lane < 4; lane++)
        out[lane] = a[lane] - b[lane];
}

inline void addVector(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int lane = 0; lane < 4; lane++)
        out[lane] = a[lane] + b[lane];
}

// from half of the way to to, as every solver's target eases.
inline void easeHalfway(sceVu0FVECTOR from, const sceVu0FVECTOR to)
{
    sceVu0FVECTOR step;
    subVector(step, to, from);
    scaleVec4(0.5f, step, step);
    addVector(from, from, step);
}

// (x, y, z, 1).
inline void copyPoint(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    out[3] = 1.0f;
}

inline void setVector(sceVu0FVECTOR out, float x, float y, float z, float w)
{
    out[0] = x;
    out[1] = y;
    out[2] = z;
    out[3] = w;
}

// The cubic the keys ease by.
inline float easeKey(float t)
{
    return t * t * -2.0f * t + t * 3.0f * t;
}

// The rows of an upright matrix for a bone: y and z swap, the new z turns
// over. Null gives the plain turn.
// FS2 0x001498d0
void boneBasis(sceVu0FMATRIX out, const sceVu0FVECTOR *m)
{
    if (m == nullptr) {
        setVector(out[0], 1.0f, 0.0f, 0.0f, 0.0f);
        setVector(out[1], 0.0f, 0.0f, 1.0f, 0.0f);
        setVector(out[2], 0.0f, -1.0f, 0.0f, 0.0f);
        setVector(out[3], 0.0f, 0.0f, 0.0f, 1.0f);
    } else {
        sceVu0FMATRIX copy;
        memcpy(copy, m, sizeof(copy));
        copyVector(out[0], copy[0]);
        copyVector(out[1], copy[2]);
        for (int lane = 0; lane < 4; lane++)
            out[2][lane] = 0.0f - copy[1][lane];
        copyVector(out[3], copy[3]);
    }
}

} // namespace

// ──────────────────────────────────────────────────────────────────────
// The quaternions under the pose.

// FS2 0x00149510
void quatAxisBetween(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    sceVu0FVECTOR from, to;
    vecNormalize(from, a);
    vecNormalize(to, b);
    vecCross(out, from, to);
    out[3] = asinAcos(vecDot(from, to), AsinAcos_Acos);
}

// Spherical, by the short way; nearly parallel: a straight line, normalized.
// FS2 0x00149580
// callcheck: asinAcos, sinCos, sinCos
void quatSlerp(float t, sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    constexpr float kNearlyParallel = 0x1.e6p-1f; // 0x3f730000
    float dot = a[1] * b[1] + a[0] * b[0] + a[2] * b[2] + a[3] * b[3];
    sceVu0FVECTOR to;
    if (dot < 0.0f) {
        for (int lane = 0; lane < 4; lane++)
            to[lane] = 0.0f - b[lane];
        dot = -dot;
    } else {
        copyVector(to, b);
    }
    sceVu0FVECTOR result;
    if (dot < kNearlyParallel) {
        float angle = asinAcos(dot, AsinAcos_Acos);
        float part = t * angle;
        float scaleA = sinCos(angle - part, 0);
        float scaleB = sinCos(part, 0);
        float scale = 1.0f / sqrtf(1.0f - dot * dot);
        for (int lane = 0; lane < 4; lane++)
            result[lane] = (a[lane] * scaleA + to[lane] * scaleB) * scale;
    } else {
        float rest = 1.0f - t;
        float length = 0.0f;
        for (int lane = 0; lane < 4; lane++) {
            result[lane] = to[lane] * t + a[lane] * rest;
            length += result[lane] * result[lane];
        }
        float scale = 1.0f / sqrtf(length);
        for (int lane = 0; lane < 4; lane++)
            result[lane] *= scale;
    }
    copyVector(out, result);
}

// FS2 0x001496f0
void quatMultiply(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    sceVu0FVECTOR result;
    result[0] = a[1] * b[2] - a[2] * b[1] + a[0] * b[3] + b[0] * a[3];
    result[1] = a[2] * b[0] - a[0] * b[2] + a[1] * b[3] + b[1] * a[3];
    result[2] = a[0] * b[1] - a[1] * b[0] + a[2] * b[3] + b[2] * a[3];
    result[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    copyVector(out, result);
}

// FS2 0x00149730
void quatToMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR q)
{
    const float x2 = q[0] + q[0], y2 = q[1] + q[1], z2 = q[2] + q[2];
    const float xx = q[0] * x2, xy = q[0] * y2, xz = q[0] * z2;
    const float yy = q[1] * y2, yz = q[1] * z2, zz = q[2] * z2;
    const float wx = q[3] * x2, wy = q[3] * y2, wz = q[3] * z2;
    setVector(out[0], 1.0f - yy - zz, xy + wz, xz - wy, 0.0f);
    setVector(out[1], xy - wz, 1.0f - xx - zz, yz + wx, 0.0f);
    setVector(out[2], xz + wy, yz - wx, 1.0f - xx - yy, 0.0f);
    setVector(out[3], 0.0f, 0.0f, 0.0f, 1.0f);
}

// FS2 0x001497c0
void quatFromAxisAngle(sceVu0FVECTOR out, const sceVu0FVECTOR axisAngle)
{
    float half = axisAngle[3] * 0.5f;
    setVecLength(sinCos(half, 0), out, axisAngle);
    out[3] = sinCos(half, 1);
}

// FS2 0x00149830
float vecHeading(const sceVu0FVECTOR v)
{
    sceVu0FVECTOR flat;
    copyVector(flat, v);
    flat[2] = 0.0f;
    vecNormalize(flat, flat);
    float sine = asinAcos(flat[1], AsinAcos_Asin);
    float angle = asinAcos(flat[0], AsinAcos_Acos);
    return sine < 0.0f ? -angle : angle;
}

// ──────────────────────────────────────────────────────────────────────
// ModelPoseBone

// The turn of a bone that aims dir at target, given the parent's space: the
// root uses it to face the pose.
// FS2 0x0014ada0
bool ModelPoseBone::aimAxis(float limit, sceVu0FVECTOR out, const sceVu0FVECTOR target,
                            const sceVu0FVECTOR dir) const
{
    bool limited = false;
    sceVu0FMATRIX base;
    if (parent == nullptr)
        identityMatrix(base);
    else
        copyMatrix(base, parent->matrix);

    sceVu0FVECTOR tip = { 1.0f, 0.0f, 0.0f, 1.0f };
    if (dir != nullptr) {
        tip[0] = dir[0];
        tip[1] = dir[1];
        tip[2] = dir[2];
    }
    sceVu0FVECTOR origin;
    copyVector(origin, base[3]);
    applyMatrix(tip, base, tip);
    sceVu0FVECTOR from;
    for (int lane = 0; lane < 4; lane++)
        from[lane] = tip[lane] - origin[lane];
    vecNormalize(from, from);

    copyVector(origin, position[PoseSlot_Final]);
    applyMatrix(origin, base, origin);
    sceVu0FVECTOR to;
    for (int lane = 0; lane < 4; lane++)
        to[lane] = target[lane] - origin[lane];
    vecNormalize(to, to);

    float angle = asinAcos(vecDot(from, to), AsinAcos_Acos);
    vecCross(out, from, to);
    scaleVec4(kAimScale, out, out);
    sceVu0FMATRIX inverse;
    invertRigidMatrix(inverse, base);
    applyMatrix(out, inverse, out);
    vecNormalize(out, out);
    if (limit < angle) {
        limited = true;
        angle = limit;
    } else if (angle < -limit) {
        limited = true;
        angle = -limit;
    }
    out[3] = angle;
    return limited;
}

// FS2 0x0014b5f0
void ModelPoseBone::turnVector(sceVu0FVECTOR out, const sceVu0FVECTOR in) const
{
    float x = in[0], y = in[1], z = in[2];
    for (int lane = 0; lane < 3; lane++)
        out[lane] = matrix[0][lane] * x + matrix[1][lane] * y + matrix[2][lane] * z;
    out[3] = 0.0f;
}

// FS2 0x0014b650
// callcheck: quatToMatrix, quatToMatrix, rotationMatrixXyz, rotationMatrixXyz, identityMatrix, identityMatrix, boneBasis, multiplyMatrix, boneBasis, multiplyMatrix, multiplyMatrix, multiplyMatrix, applyMatrix, applyMatrix, multiplyTurn, multiplyTurn
bool ModelPoseBone::buildMatrix(float frames, int slot, int record)
{
    const u_char bits = tracks;
    if (bits & (PoseTrack_Event | PoseTrack_Value))
        return false;

    if (bits & PoseTrack_Quat) {
        quatToMatrix(rawMatrix, rotation[PoseSlot_Current]);
        quatToMatrix(matrix, rotation[slot]);
    } else if (bits & PoseTrack_Euler) {
        rotationMatrixXyz(rawMatrix, rotation[PoseSlot_Current]);
        rotationMatrixXyz(matrix, rotation[slot]);
    } else {
        identityMatrix(rawMatrix);
        identityMatrix(matrix);
    }
    if (grown != 0) {
        for (int row = 0; row < 3; row++) {
            for (int lane = 0; lane < 3; lane++) {
                matrix[row][lane] *= growth[row];
                rawMatrix[row][lane] *= growth[row];
            }
        }
    }
    copyPoint(rawMatrix[3], position[PoseSlot_Current]);
    copyPoint(matrix[3], position[slot]);

    if (parent == nullptr) {
        sceVu0FMATRIX basis;
        boneBasis(basis, owner->placement);
        multiplyMatrix(rawMatrix, basis, rawMatrix);
        boneBasis(basis, owner->world);
        multiplyMatrix(matrix, basis, matrix);
    } else if (!(bits & PoseTrack_Follow)) {
        multiplyMatrix(rawMatrix, parent->rawMatrix, rawMatrix);
        multiplyMatrix(matrix, parent->matrix, matrix);
    } else {
        ModelPoseBone *root = owner->rootBone;
        applyMatrix(rawMatrix[3], parent->rawMatrix, rawMatrix[3]);
        applyMatrix(matrix[3], parent->matrix, matrix[3]);
        multiplyTurn(rawMatrix, root->rawMatrix, rawMatrix);
        multiplyTurn(matrix, root->matrix, matrix);
    }

    if (record & 1) {
        copyVector(lastWorldPosition, worldPosition);
        copyVector(lastRawPosition, rawPosition);
        copyVector(worldPosition, matrix[3]);
        copyVector(rawPosition, rawMatrix[3]);
        copyVector(lastTurn, turn);
        if (bits & PoseTrack_Euler) {
            copyVector(turn, rotation[slot]);
        } else {
            for (int lane = 0; lane < 4; lane++)
                turn[lane] = matrix[3][lane] + matrix[2][lane] - rawPosition[lane];
        }
        lowFrames = fabsf(rawPosition[2]) < kLowHeight ? lowFrames + frames : 0.0f;
    }
    return true;
}

// a towards b into slot. The ROM counts a key with a span of 0 or less as
// nothing to do.
// FS2 0x0014b9a0
// callcheck: blendVec4, quatSlerp, blendVec4, blendVec4
bool ModelPoseBone::interpolate(float t, int slot, int a, int b)
{
    if (keySpan <= 0.0f)
        return false;
    const u_char bits = tracks;
    if (bits & PoseTrack_Quat) {
        quatSlerp(t, rotation[slot], rotation[a], rotation[b]);
    } else if (bits & PoseTrack_Euler) {
        // By the short way round, lane by lane.
        sceVu0FVECTOR from, to;
        copyVector(from, rotation[a]);
        copyVector(to, rotation[b]);
        for (int lane = 0; lane < 3; lane++) {
            while (kPi < from[lane] - to[lane])
                to[lane] += kTwoPi;
            if (kPi < to[lane] - from[lane]) {
                do {
                    from[lane] += kTwoPi;
                } while (kPi < to[lane] - from[lane]);
            }
        }
        blendVec4(t, rotation[slot], to, from);
    }
    if (bits & PoseTrack_Position) {
        blendVec4(t, position[slot], position[b], position[a]);
        position[slot][3] = 1.0f;
    }
    if (bits & PoseTrack_Value) {
        blendVec4(t, position[slot], position[b], position[a]);
        position[slot][3] = t;
    }
    return true;
}

// FS2 0x0014bbe0
// callcheck: readKey, interpolate, buildMatrix, pushEvent, readKey
bool ModelPoseBone::seek(float frame, int slot)
{
    const float *at = track;
    if (at == nullptr)
        return false;
    while (1.0f < duration && duration < frame)
        frame -= duration;

    float sum = 0.0f;
    int index = -1;
    if (0.0f <= frame) {
        do {
            float length = at[keyWords - 1];
            if (length == 0.0f && (tracks & PoseTrack_Event))
                owner->pushEvent(reinterpret_cast<const char *>(at));
            sum += fabsf(length);
            index++;
            at += keyWords;
            // PC: the ROM has no end test and counts on the motion's length
            // covering frame; the last key stops here instead of running on.
            if (length < 0.0f)
                break;
        } while (sum <= frame);
    }

    const float *found = track + index * keyWords;
    if (slot < 0) {
        readKey(found, nullptr);
        keySpan = sum;
        keyTime = 0.0f;
        time = 0.0f;
        key = found;
        return false;
    }
    const float *previous = index != 0 ? track + (index - 1) * keyWords : nullptr;
    readKey(found, previous);
    keyTime = keySpan - (sum - frame);
    time = frame;
    key = found;
    float t = keyTime / keySpan;
    if (playFlags & PosePlay_Ease)
        t = easeKey(t);
    interpolate(t, slot, PoseSlot_From, PoseSlot_To);
    buildMatrix(1.0f, slot, 1);
    return true;
}

// FS2 0x0014be10
// callcheck: pushEvent
void ModelPoseBone::readKey(const float *at, const float *previous)
{
    if (playFlags & PosePlay_NoBlend)
        previous = nullptr;
    if (previous == nullptr) {
        copyVector(rotation[PoseSlot_From], rotation[PoseSlot_To]);
        copyVector(position[PoseSlot_From], position[PoseSlot_To]);
    }
    int word = 0;
    int previousWord = 0;
    if (tracks & PoseTrack_Position) {
        memcpy(position[PoseSlot_To], at, sizeof(sceVu0FVECTOR));
        if (previous != nullptr)
            memcpy(position[PoseSlot_From], previous, sizeof(sceVu0FVECTOR));
        word = kPoseKeyPosition;
        previousWord = kPoseKeyPosition;
    }
    if (tracks & PoseTrack_Quat) {
        memcpy(rotation[PoseSlot_To], at + word, sizeof(sceVu0FVECTOR));
        if (previous != nullptr)
            memcpy(rotation[PoseSlot_From], previous + previousWord, sizeof(sceVu0FVECTOR));
        word += kPoseKeyQuat;
        previousWord += kPoseKeyQuat;
    }
    if (tracks & PoseTrack_Euler) {
        memcpy(rotation[PoseSlot_To], at + word, sizeof(sceVu0FVECTOR));
        if (previous != nullptr)
            memcpy(rotation[PoseSlot_From], previous + previousWord, sizeof(sceVu0FVECTOR));
        word += kPoseKeyEuler;
        previousWord += kPoseKeyEuler;
    }
    if (tracks & PoseTrack_Event) {
        // The slots hold the event's name here, not numbers.
        char *name = reinterpret_cast<char *>(position[PoseSlot_To]);
        if (0.0f < at[word + kPoseKeyEvent])
            memcpy(name, at + word, sizeof(sceVu0FVECTOR));
        else
            name[0] = 0;
        owner->pushEvent(reinterpret_cast<const char *>(position[PoseSlot_From]));
        word += kPoseKeyEvent;
        previousWord += kPoseKeyEvent;
    }
    if (tracks & PoseTrack_Value) {
        if (0.0f <= at[word])
            setVector(position[PoseSlot_To], at[word], 0.0f, 0.0f, 0.0f);
        if (previous != nullptr && 0.0f <= previous[previousWord])
            setVector(position[PoseSlot_From], previous[previousWord], 0.0f, 0.0f, 0.0f);
        word += kPoseKeyValue;
    }
    keySpan = fabsf(at[word]);
}

// mode 1 starts the carrying over, at the track's first key; mode 0 is a
// frame of it: the object moves by what the root moved since the last.
// FS2 0x0014c0c0
// callcheck: resetPlacement, stepRoot, interpolate, buildMatrix, FUN_00135e20, rotationMatrixXyz, boneBasis, multiplyMatrix
bool ModelPoseBone::stepRoot(float frames, int restart)
{
    ModelPose *pose = owner;
    if (parent != nullptr || !(pose->flags & PosePlay_NoBlend))
        return false;

    if (restart == 1) {
        if (pose->motionFlags & kMotionFileSeek) {
            setVector(rawPosition, track[0], -track[2], track[1], 1.0f);
            memcpy(position[PoseSlot_From], track, sizeof(sceVu0FVECTOR));
        } else {
            setVector(rawPosition, 0.0f, 0.0f, 0.0f, 1.0f);
            setVector(position[PoseSlot_From], 0.0f, 0.0f, 0.0f, 1.0f);
        }
        setVector(turn, 0.0f, 0.0f, 0.0f, 1.0f);
        setVector(rotation[PoseSlot_From], 0.0f, 0.0f, 0.0f, 1.0f);
        pose->resetPlacement(nullptr);
        stepRoot(frames, 0);
        return true;
    }
    if (restart != 0)
        return true;

    float t = keyTime / keySpan;
    if (playFlags & PosePlay_Ease)
        t = easeKey(t);
    interpolate(t, PoseSlot_Current, PoseSlot_From, PoseSlot_To);
    buildMatrix(frames, PoseSlot_Current, 1);
    if (pose->parentMatrix == nullptr) {
        for (int lane = 0; lane < 3; lane++)
            pose->position[lane] += (rawPosition[lane] - lastRawPosition[lane]) * pose->rootScale[lane];
        pose->position[3] += rawPosition[3] - lastRawPosition[3];
        pose->rotation[1] += 0.0f * pose->rootScale[3];
        pose->rotation[2] += -(turn[1] - lastTurn[1]);
    }
    wrapAngleVec(pose->rotation, pose->rotation);
    rotationMatrixXyz(pose->world, pose->rotation);
    copyPoint(pose->world[3], pose->position);
    boneBasis(matrix, nullptr);
    multiplyMatrix(matrix, pose->world, matrix);
    return true;
}

// FS2 0x0014c340
// callcheck: stepRoot, interpolate, advance, readKey, stepRoot, advance, readKey, advance
bool ModelPoseBone::advance(float frames)
{
    if (track == nullptr || (playFlags & PosePlay_Done))
        return false;

    if (1.0f < duration) {
        while (duration * 10.0f < frames)
            frames -= duration * 10.0f;
        while (duration < frames)
            frames -= duration;
    }
    playFlags &= ~PosePlay_Ending;

    const bool lastKey = signbit(key[keyWords - 1]) != 0;
    const float left = keySpan - keyTime;
    if (lastKey) {
        if (left > frames && frames * 1.5f > left) {
            playFlags |= PosePlay_Ending;
            overshoot = left - frames;
        } else if (left == frames) {
            playFlags |= PosePlay_Ending;
            overshoot = 0.0f;
        }
    }

    if (frames <= left) {
        keyTime += frames;
        time += frames;
        if (parent == nullptr && (owner->flags & PosePlay_NoBlend)) {
            stepRoot(frames, 0);
        } else {
            // PC: a key with no length is over. The ROM's 0 / 0 is the largest
            // float, which here would come out of interpolate as a NaN pose.
            float t = keySpan != 0.0f ? keyTime / keySpan : 1.0f;
            if (playFlags & PosePlay_Ease)
                t = easeKey(t);
            interpolate(t, PoseSlot_Current, PoseSlot_From, PoseSlot_To);
        }
        return true;
    }

    playFlags &= ~PosePlay_Ease;
    if (lastKey) {
        advance(left);
        keyTime = 0.0f;
        time = 0.0f;
        readKey(track, key);
        key = track;
        stepRoot(left, 1);
        if (playFlags & PosePlay_Loop) {
            advance(frames - left);
        } else {
            playFlags |= PosePlay_Done;
            overshoot = frames - left;
        }
        playFlags |= PosePlay_Ending;
        return false;
    }
    keyTime = 0.0f;
    const float *next = key + keyWords;
    time += left;
    readKey(next, key);
    key = next;
    return advance(frames - left);
}

// FS2 0x0014c670
const float *ModelPoseBone::keyAt(float frame) const
{
    const float *at = track;
    if (at == nullptr || !(frame < owner->motionFrames))
        return nullptr;
    if (0.0f <= frame) {
        float sum = 0.0f;
        do {
            float length = at[keyWords - 1];
            if (length < 0.0f)
                return nullptr;
            at += keyWords;
            sum += length;
        } while (sum <= frame);
    }
    return at;
}

// FS2 0x0014c710
// callcheck: strcmp, seek, resetPlacement
bool ModelPoseBone::bindTrack(const PoseMotionTrack *record, u_int flags)
{
    if (strcmp(name, record->bone) != 0)
        return false;

    float start = (float)record->phase * phase - 1.0f;
    if (start < 0.0f)
        start = 0.0f;
    track = record->keys();
    key = track;
    duration = owner->motionFrames;
    playFlags = flags;
    tracks = record->tracks;
    startFrame = start;
    keyWords = 1;
    if (tracks & PoseTrack_Position)
        keyWords += kPoseKeyPosition;
    if (tracks & PoseTrack_Quat)
        keyWords += kPoseKeyQuat;
    if (tracks & PoseTrack_Euler)
        keyWords += kPoseKeyEuler;
    if (tracks & PoseTrack_Event)
        keyWords += kPoseKeyEvent;
    if (tracks & PoseTrack_Value)
        keyWords += kPoseKeyValue;
    if (flags & (PosePlay_FromKey | PosePlay_AtStart))
        start = 0.0f;
    seek(start, -1);
    overshoot = 0.0f;

    if (parent == nullptr && (owner->flags & PosePlay_NoBlend)) {
        memset(rawPosition, 0, sizeof(rawPosition));
        memset(lastRawPosition, 0, sizeof(lastRawPosition));
        setVector(position[PoseSlot_From], 0.0f, 0.0f, 0.0f, 1.0f);
        memset(turn, 0, sizeof(turn));
        memset(lastTurn, 0, sizeof(lastTurn));
        setVector(rotation[PoseSlot_From], 0.0f, 0.0f, 0.0f, 1.0f);
        owner->resetPlacement(nullptr);
    } else {
        copyVector(position[PoseSlot_From], position[PoseSlot_Current]);
        copyVector(rotation[PoseSlot_From], rotation[PoseSlot_Current]);
    }
    if (playFlags & PosePlay_FromKey) {
        copyVector(position[PoseSlot_From], position[PoseSlot_To]);
        copyVector(rotation[PoseSlot_From], rotation[PoseSlot_To]);
        playFlags &= ~PosePlay_FromKey;
    }
    playFlags &= ~PosePlay_AtStart;
    playFlags |= PosePlay_Moved;
    return true;
}

// FS2 0x0014c980
void ModelPoseBone::setup(const PoseBoneRecord *record)
{
    strcpy(name, record->name);
    strcpy(parentName, record->parent);
    if (isSwing()) {
        memcpy(swingParams, &record->length, sizeof(swingParams));
        length = kSwingLength;
    } else {
        length = record->length;
    }
    position[PoseSlot_Bind][0] = record->position[0];
    position[PoseSlot_Bind][1] = record->position[1];
    position[PoseSlot_Bind][2] = record->position[2];
    position[PoseSlot_Bind][3] = 1.0f;
    memcpy(rotation[PoseSlot_Bind], record->rotation, sizeof(sceVu0FVECTOR));
    for (int slot = 0; slot < kPoseSlots; slot++) {
        if (slot != PoseSlot_Bind) {
            copyVector(position[slot], position[PoseSlot_Bind]);
            copyVector(rotation[slot], rotation[PoseSlot_Bind]);
        }
    }
    tracks = PoseTrack_Quat;
}

// FS2 0x0014cb10
ModelPoseBone::ModelPoseBone()
{
    identityMatrix(matrix);
    // PC: the ROM leaves it as the heap has it. buildMatrix stops before it on an
    // event or value bone, and their children multiply by it: garbage there
    // overflows to the NaN the EE cannot make.
    identityMatrix(rawMatrix);
}

// ──────────────────────────────────────────────────────────────────────
// ModelPose: the setters, what other code calls between updates.

// FS2 0x0014e0c0
void ModelPose::setFootTargets(float rate, const float *left, const float *right)
{
    if (left == nullptr || right == nullptr || leftToe == nullptr || rightToe == nullptr) {
        flags &= ~kPoseFlagFootSolve;
        _320[3] = 0.0f;   // FS2 +0x32c
        _340[3] = 0.0f;   // FS2 +0x34c
    } else {
        flags |= kPoseFlagFootSolve;
        copyVector(leftFootTarget, left);
        copyVector(rightFootTarget, right);
        _320[3] = 1.0f;
        _340[3] = 1.0f;
    }
    footRate = rate;
}

// FS2 0x0014e150
void ModelPose::setToolTarget(float rate, const float *target)
{
    if (target == nullptr || leftTool == nullptr) {
        toolTo[3] = 0.0f;
    } else {
        if (toolTo[3] == 0.0f)
            copyVector(toolFrom, target);
        copyVector(toolTo, target);
        toolTo[3] = 1.0f;
    }
    toolRate = rate;
}

// FS2 0x0014e1b0
void ModelPose::setLookTarget(float rate, float rate2, const float *target)
{
    if (target == nullptr) {
        lookTo[3] = 0.0f;
    } else {
        if (lookTo[3] == 0.0f)
            copyVector(lookFrom, target);
        copyVector(lookTo, target);
        lookTo[3] = 1.0f;
    }
    lookRate = rate;
    lookRate2 = rate2;
}

// FS2 0x0014e200
// callcheck: vecNormalize, scaleVec4
void ModelPose::setAimDirection(float rate, const float *direction)
{
    if (direction == nullptr)
        setVector(aimTarget, 0.0f, 0.0f, 1.0f, 1.0f);
    else
        vecNormalize(aimTarget, direction);
    scaleVec4(kProbeHeight, aimTarget, aimTarget);
    aimRate = rate;
}

// FS2 0x0014e270
void ModelPose::setHeadTarget(float rate, const float *target)
{
    if (target == nullptr || headBone == nullptr) {
        headTo[3] = 0.0f;
    } else {
        if (headTo[3] == 0.0f) {
            copyVector(headFrom, target);
            headFrom[2] -= kHeadDrop;
        }
        copyVector(headTo, target);
        headTo[2] -= kHeadDrop;
        headTo[3] = 1.0f;
    }
    headRate = rate;
}

// FS2 0x0014e300
int ModelPose::getSurfaceFrames(float *out)
{
    if (faceBone == nullptr)
        return 0;
    out[0] = fabsf(faceBone->position[PoseSlot_From][0]);
    out[1] = fabsf(faceBone->position[PoseSlot_To][0]);
    out[2] = 1.0f - faceBone->position[PoseSlot_Current][3];
    return 1;
}

// FS2 0x0014e3b0
bool ModelPose::pushEvent(const char *event)
{
    if (event[0] == 0)
        return false;
    for (int i = 0; i < kPoseEventQueue; i++) {
        if (events[i][0] == 0) {
            strcpy(events[i], event);
            return true;
        }
    }
    return false;
}

// FS2 0x0014e470
void ModelPose::translate(const float *delta)
{
    for (int i = 0; i < boneCount; i++) {
        for (int lane = 0; lane < 4; lane++)
            bones[i].matrix[3][lane] += delta[lane];
    }
    for (int lane = 0; lane < 4; lane++)
        position[lane] += delta[lane];
}

// FS2 0x0014e4d0
// callcheck: rotationMatrixXyz, blendVec4, aimAxis, quatFromAxisAngle, quatToMatrix, multiplyMatrix, multiplyMatrix, boneBasis, multiplyMatrix, strcmp, buildMatrix
void ModelPose::solve(int slot, int count, int flaggedOnly)
{
    if (count < 0)
        count = boneCount;
    for (int i = 0; i < count; i++) {
        ModelPoseBone *bone = &bones[i];
        if (flaggedOnly != 0 && !(bone->groups & PoseGroup_Trunk))
            continue;

        if (bone->parent == nullptr) {
            rotationMatrixXyz(world, rotation);
            blendVec4(aimRate, aim, aimTarget, aim);
            sceVu0FVECTOR up = { 0.0f, 0.0f, 1.0f, 1.0f };
            memset(rootBone->position[PoseSlot_Final], 0, sizeof(sceVu0FVECTOR));
            sceVu0FVECTOR axisAngle, quat;
            rootBone->aimAxis(kPi, axisAngle, aim, up);
            quatFromAxisAngle(quat, axisAngle);
            quatToMatrix(aimMatrix, quat);
            multiplyMatrix(world, aimMatrix, world);
            copyPoint(world[3], position);
            if (parentMatrix != nullptr)
                multiplyMatrix(world, parentMatrix, world);
            boneBasis(bone->matrix, nullptr);
            multiplyMatrix(bone->matrix, world, bone->matrix);
            continue;
        }

        bone->grown = 0;
        if (flags & PosePlay_Grow) {
            int which = 0;
            while (which < kGrowthBones && strcmp(bone->name, kGrowthBoneNames[which]) != 0)
                which++;
            if (which < kGrowthBones && 0.0f < bone->duration) {
                float t = (bone->playFlags & PosePlay_Loop) ? 1.0f : bone->time / bone->duration;
                for (int knot = 1; knot < kGrowthKnots; knot++) {
                    if (t < kGrowthKnotAt[knot] || knot == kGrowthKnots - 1) {
                        float along = t - kGrowthKnotAt[knot - 1];
                        float span = kGrowthKnotAt[knot] - kGrowthKnotAt[knot - 1];
                        for (int lane = 0; lane < 3; lane++) {
                            float from = kGrowth[lane][which][knot - 1];
                            float to = kGrowth[lane][which][knot];
                            bone->growth[lane] = from + along * (to - from) / span;
                        }
                        bone->grown = 1;
                        break;
                    }
                }
            }
        }
        bone->buildMatrix(1.0f, slot, 0);
    }
}

// FS2 0x0014e8b0
void ModelPose::resetPlacement(const sceVu0FVECTOR *from)
{
    copyMatrix(placement, from != nullptr ? from : world);
    placement[3][0] = 0.0f;
    placement[3][1] = 0.0f;
    placement[3][2] = 0.0f;
}

// FS2 0x0014e900
bool ModelPose::stopWalk()
{
    u_int was = walkFlags;
    walkFlags |= PoseWalk_Stopped;
    walkFlags &= ~PoseWalk_Active;
    return (was & PoseWalk_Active) != 0;
}

// FS2 0x0014e940
// callcheck: scaleVec4, vecNormalize, vecHeading, wrapAngleLane, rotationMatrixXyz, applyMatrix, applyMatrix, multiplyMatrix, wrapAngleVec, solve, rotate (the last six: the ROM has rotate in line, twice)
void ModelPose::stepWalk(float frames)
{
    int finished = 0;
    walkFrames -= frames;
    if (walkFrames < 1.0f) {
        walkFrames = 1.0f;
        finished = 2;
    }

    if (walkFlags & PoseWalk_ToTarget) {
        sceVu0FVECTOR step;
        for (int lane = 0; lane < 4; lane++)
            step[lane] = walkTarget[lane] - position[lane];
        step[2] = 0.0f;
        scaleVec4(1.0f / walkFrames, step, step);
        for (int lane = 0; lane < 4; lane++)
            position[lane] += step[lane];
    } else {
        finished++;
    }

    if (walkFlags & PoseWalk_ToAim) {
        sceVu0FVECTOR direction;
        const float *from = walkTarget[3] == 0.0f ? position : walkTarget;
        for (int lane = 0; lane < 4; lane++)
            direction[lane] = walkAim[lane] - from[lane];
        direction[2] = 0.0f;
        vecNormalize(direction, direction);
        float turn = (vecHeading(direction) - rotation[2]) + kHalfPi;
        if (kPi < fabsf(turn))
            turn = wrapAngleLane(turn);
        if (fabsf(turn) < kWalkTurnDone) {
            finished++;
        } else {
            sceVu0FVECTOR step = { 0.0f, 0.0f, 0.0f, 0.0f };
            if (walkTarget[3] == 0.0f) {
                step[2] = turn / walkFrames;
            } else {
                float left = walkFrames - kWalkTurnLead;
                if (left < 1.0f)
                    left = 1.0f;
                step[2] = turn / left;
            }
            rotate(step);
        }
    } else {
        finished++;
    }

    if (1 < finished) {
        walkFlags |= PoseWalk_Stopped;
        walkFlags &= ~PoseWalk_Active;
    }
}

// FS2 0x0014ed30
// callcheck: turnVector, placeRecord
void ModelPose::getWalkStart(float *out, float *offset, const float *origin, const float *dir)
{
    sceVu0FVECTOR from;
    copyVector(from, origin != nullptr ? origin : position);
    if (dir == nullptr) {
        setVector(offset, 0.0f, 0.0f, -kProbeHeight, 1.0f);
        rootBone->turnVector(offset, offset);
    } else {
        for (int lane = 0; lane < 4; lane++)
            offset[lane] = from[lane] - dir[lane];
    }
    placeRecord(out, nullptr, from, offset);
    for (int lane = 0; lane < 4; lane++)
        offset[lane] = out[lane] - offset[lane];
}

// FS2 0x0014ee00
// callcheck: placeRecord
void ModelPose::getRecordOffset(float *out, float *offset, const PoseBoneRecord *record,
                                const float *origin, const float *dir)
{
    placeRecord(out, record, origin, dir);
    for (int lane = 0; lane < 4; lane++)
        offset[lane] = out[lane] - dir[lane];
}

// FS2 0x0014ee60
// callcheck: turnVector
void ModelPose::startWalk(float frames, const float *target, const float *aimAt)
{
    static const sceVu0FVECTOR kAhead = { 0.0f, 0.0f, kProbeHeight, 1.0f }; // data 0x2f5680
    walkFlags = 0;
    if (target == nullptr) {
        copyVector(walkTarget, position);
    } else {
        copyVector(walkTarget, target);
        walkFlags |= PoseWalk_ToTarget;
    }
    if (aimAt == nullptr) {
        sceVu0FVECTOR ahead;
        rootBone->turnVector(ahead, kAhead);
        for (int lane = 0; lane < 4; lane++)
            walkAim[lane] = walkTarget[lane] + ahead[lane];
    } else {
        copyVector(walkAim, aimAt);
        walkFlags |= PoseWalk_ToAim;
    }
    walkTarget[3] = 1.0f;
    walkAim[2] = walkTarget[2];
    walkAim[3] = walkTarget[3];
    walkFrames = frames;
    walkFlags |= PoseWalk_Active;
}

// FS2 0x0014f010
// callcheck: getPosition, getPosition
void ModelPose::getFootPositions(float *left, float *right) const
{
    if (left != nullptr && leftToe != nullptr)
        leftToe->parent->getPosition(left);
    if (right != nullptr && rightToe != nullptr)
        rightToe->parent->getPosition(right);
}

// FS2 0x0014f080
// callcheck: vecHeading, vecHeading, quatFromAxisAngle, quatToMatrix, applyMatrix
void ModelPose::placeRecord(float *out, const PoseBoneRecord *record, const float *origin,
                            const float *dir)
{
    sceVu0FVECTOR offset = { 0.0f, 0.0f, 0.0f, 1.0f };
    if (record != nullptr) {
        offset[0] = record->position[0];
        offset[1] = -record->position[2];
        offset[2] = record->position[1];
    }
    static const sceVu0FVECTOR kFront = { 0.0f, 1.0f, 0.0f, 1.0f };
    sceVu0FVECTOR axisAngle = { 0.0f, 0.0f, 1.0f, 0.0f };
    axisAngle[3] = vecHeading(dir) - vecHeading(kFront);
    sceVu0FVECTOR quat;
    quatFromAxisAngle(quat, axisAngle);
    sceVu0FMATRIX turn;
    quatToMatrix(turn, quat);
    applyMatrix(offset, turn, offset);
    for (int lane = 0; lane < 4; lane++)
        out[lane] = origin[lane] + offset[lane];
}

// FS2 0x0014f360
// callcheck: strcmp
bool ModelPose::isBoneEnding(const char *boneName) const
{
    if (boneName == nullptr)
        return (bones[0].playFlags & PosePlay_Ending) != 0;
    for (int i = 0; i < boneCount; i++) {
        if (strcmp(bones[i].name, boneName) == 0)
            return (bones[i].playFlags & PosePlay_Ending) != 0;
    }
    return false;
}

// FS2 0x0014f490
void ModelPose::setRootScale(float x, float y, float z, float w)
{
    setVector(rootScale, x, y, z, w);
}

// ──────────────────────────────────────────────────────────────────────
// ModelPose: the frame.

// FS2 0x0014f4b0
// callcheck: stepWalk, advance, solve, solveLook, solveHead, solveTool, settleFixed, settleFixed, settleFixed, settle, settle, settle, scaleVec4, solveFeet, interpolate, interpolate, buildMatrix, stepSwing, solveChestTool
void ModelPose::update(float frames, bool loop)
{
    flags &= ~kPoseFlagFresh;
    if ((walkFlags & PoseWalk_Active) && frames != 0.0f)
        stepWalk(frames);

    if (current == nullptr)
        return;
    if (loop && (bones[0].playFlags & PosePlay_Done))
        return;

    for (int i = 0; i < boneCount; i++) {
        ModelPoseBone *bone = &bones[i];
        bone->advance(frames);
        copyVector(bone->position[PoseSlot_Solve], bone->position[PoseSlot_Current]);
        copyVector(bone->rotation[PoseSlot_Solve], bone->rotation[PoseSlot_Current]);
    }
    // PC: a pose of one bone (the ROM writes past the block).
    if (1 < boneCount) {
        for (int lane = 0; lane < 4; lane++)
            bones[1].position[PoseSlot_Solve][lane] += rootOffset[lane];
    }
    solve(PoseSlot_Solve, -1, 0);

    const u_int solvers = m_solvers & kPoseSolverMask;
    if (solvers != 0) {
        if (m_solvers & PoseSolver_Look)
            solveLook(lookRate2, lookRate, lookBone);
        if (m_solvers & PoseSolver_Head)
            solveHead(headRate);
        if (m_solvers & PoseSolver_Tool)
            solveTool(toolRate);
        if ((m_solvers & PoseSolver_Feet) && leftToe != nullptr && rightToe != nullptr) {
            if (!(flags & kPoseFlagFootSolve)) {
                leftToe->settle(footRate, 4);
                rightToe->settle(footRate, 4);
                pelvisBone->settle(footRate, 1);
                scaleVec4(kFootDecay, rootOffset, rootOffset);
            } else {
                leftToe->settleFixed(footRate, 4);
                rightToe->settleFixed(footRate, 4);
                pelvisBone->settleFixed(footRate, 1);
            }
            if (kFootContact < leftToe->solveWeight)
                solveFeet(leftFootTarget, rightFootTarget);
        }
    }

    for (int i = 0; i < boneCount; i++) {
        ModelPoseBone *bone = &bones[i];
        if (bone->parent == nullptr) {
            copyVector(bone->position[PoseSlot_Final], bone->position[PoseSlot_Solve]);
            copyVector(bone->rotation[PoseSlot_Final], bone->rotation[PoseSlot_Solve]);
        } else {
            if (solvers == 0 || bone->solveWeight <= kWeightOff) {
                copyVector(bone->position[PoseSlot_Final], bone->position[PoseSlot_Current]);
                copyVector(bone->rotation[PoseSlot_Final], bone->rotation[PoseSlot_Current]);
            } else {
                bone->interpolate(bone->solveWeight, PoseSlot_Final, PoseSlot_Current, PoseSlot_Solve);
            }
            if (solvers != 0 && kWeightOff < bone->targetWeight) {
                bone->interpolate(bone->targetWeight, PoseSlot_Final, PoseSlot_Final, PoseSlot_Target);
                bone->targetWeight *= kTargetDecay;
            }
            bone->buildMatrix(frames, PoseSlot_Final, 1);
        }
        bone->playFlags &= ~PosePlay_Moved;
        if (bone->isSwing())
            bone->stepSwing();
    }
    if (hasChestTool != 0)
        solveChestTool(toolRate);
}

// FS2 0x0014f910
// callcheck: bindTracksLayered
int ModelPose::playLayered(ModelMotion *motion, u_int playFlags, int groups)
{
    if (motion->file == nullptr && current == nullptr)
        return 0;
    return motion->bindTracksLayered(playFlags | PosePlay_Layered, (u_short)groups);
}

// FS2 0x0014f950
// callcheck: bindTrack
int ModelPose::playFileLayered(void *file, u_int playFlags, int groups)
{
    const PoseMotionFile *motion = file != nullptr ? static_cast<const PoseMotionFile *>(file) : current;
    if (motion == nullptr)
        return 0;
    const u_short mask = (u_short)groups;
    const int trackCount = motion->trackCount;
    const PoseMotionTrack *records = motion->tracks();
    int bound = 0;
    for (int t = 0; t < trackCount; t++) {
        for (int i = 0; i < boneCount; i++) {
            ModelPoseBone *bone = &bones[i];
            if ((mask == 0 || (mask & bone->groups) != 0)
                && bone->bindTrack(&records[t], playFlags | PosePlay_Layered)) {
                bound++;
                bone->motion = motion;
            }
        }
    }
    return bound;
}

// What play and playFile do first.
// callcheck: strcpy
bool ModelPose::beginMotion(const PoseMotionFile *file, u_int playFlags, u_short groups)
{
    if (playFlags & PosePlay_FromKey) {
        walkFlags |= PoseWalk_Stopped;
        walkFlags &= ~PoseWalk_Active;
    }
    const u_int was = flags;
    if (walkFlags & PoseWalk_Stopped)
        walkFlags = 0;
    if ((playFlags & PosePlay_SameKeep) && current == file)
        return false;

    if (statusBone != nullptr && !(groups & PoseGroup_Upper)) {
        statusBone->track = nullptr;
        statusBone->key = nullptr;
        for (int slot = 0; slot < kPoseSlots; slot++)
            statusBone->position[slot][0] = 0.0f;
    }
    current = file;
    motionKind = file->kind;
    motionTracks = file->trackCount;
    motionFlags = (u_int)(int)file->flags;
    motionFrames = (float)file->frames;
    strcpy(motionName, file->name);
    memcpy(motionParams, file->params, sizeof(motionParams));
    flags = playFlags | (was & kPoseFlagFootSolve);
    return true;
}

// FS2 0x0014fab0
// callcheck: strcpy, bindTracks
int ModelPose::play(ModelMotion *motion, u_int playFlags, int groups)
{
    const PoseMotionFile *file = static_cast<const PoseMotionFile *>(motion->file);
    if (file == nullptr)
        file = current;
    if (file == nullptr || !beginMotion(file, playFlags, (u_short)groups))
        return 0;
    int bound = motion->bindTracks(playFlags, (u_short)groups);
    flags &= ~PosePlay_FromKey;
    return bound;
}

// FS2 0x0014fca0
// callcheck: strcpy, bindTrack, keyAt
int ModelPose::playFile(void *file, u_int playFlags, int groups)
{
    const PoseMotionFile *motion = file != nullptr ? static_cast<const PoseMotionFile *>(file) : current;
    if (motion == nullptr || !beginMotion(motion, playFlags, (u_short)groups))
        return 0;

    const u_short mask = (u_short)groups;
    const PoseMotionTrack *records = current->tracks();
    int bound = 0;
    for (int t = 0; t < motionTracks; t++) {
        for (int i = 0; i < boneCount; i++) {
            ModelPoseBone *bone = &bones[i];
            if ((mask == 0 || (mask & bone->groups) != 0) && bone->bindTrack(&records[t], playFlags)) {
                bone->motion = current;
                bound++;
                if (current->flags & kMotionFileSeek) {
                    const float *at = bone->keyAt(motionParams[3]);
                    if (at != nullptr)
                        bone->track = at;
                }
            }
        }
    }
    flags &= ~PosePlay_FromKey;
    return bound;
}

// FS2 0x0014ff90
// callcheck: strcpy, alloc, ModelPoseBone, setup, strcmp, operator_new[], strcmp, solve, invertRigidMatrix, applyMatrix, vecNormalize, invertRigidMatrix, applyMatrix, vecNormalize
int ModelPose::load(void *file, int arg, const char *boneNames, u_int nameCount)
{
    const PoseBoneFile *source = static_cast<const PoseBoneFile *>(file);
    fileKind = source->kind;
    boneCount = source->boneCount;
    fileFlags = source->flags;
    strcpy(name, source->name);
    if (boneCount > kPoseMaxBones || boneCount < 0)
        return -2;

    bones = static_cast<ModelPoseBone *>(gGlobalHeap.alloc(boneCount * sizeof(ModelPoseBone), kBoneAlign));
    for (int i = 0; i < boneCount; i++)
        new (&bones[i]) ModelPoseBone();
    const PoseBoneRecord *records = source->records();
    for (int i = 0; i < boneCount; i++) {
        bones[i].setup(&records[i]);
        bones[i].owner = this;
    }

    rootBone = bones;
    pelvisBone = leftToe = rightToe = faceBone = statusBone = headBone = nullptr;
    neckBone = spine0Bone = spine1Bone = leftUpperArm = rightUpperArm = nullptr;
    leftTool = rightTool = nullptr;

    // layout: host -- a table of the port, the ROM has the strcmp calls in line.
    struct NamedBone
    {
        const char     *boneName;       // data 0x3015..
        ModelPoseBone **slot;
    };
    const NamedBone named[] = {
        { "bn_base", &rootBone },         { "bn_pelvis", &pelvisBone },
        { "bn_l_toe", &leftToe },         { "bn_r_toe", &rightToe },
        { "bn_face", &faceBone },         { "bn_status", &statusBone },
        { "bn_head", &headBone },         { "bn_neck", &neckBone },
        { "bn_spine0", &spine0Bone },     { "bn_spine1", &spine1Bone },
        { "bn_l_upperarm", &leftUpperArm }, { "bn_r_upperarm", &rightUpperArm },
        { "bn_l_tool", &leftTool },       { "bn_r_tool", &rightTool },
    };

    for (int i = 0; i < boneCount; i++) {
        ModelPoseBone *bone = &bones[i];
        bone->parent = findBone(bone->parentName);
        bone->phase = 1.0f;
        for (const BoneGroupList &list : kBoneGroups) {
            for (const char *const *entry = list.names; *entry != nullptr; entry++) {
                if (strcmp(bone->name, *entry) == 0)
                    bone->groups |= list.group;
            }
        }
        for (const NamedBone &entry : named) {
            if (strcmp(bone->name, entry.boneName) == 0)
                *entry.slot = bone;
        }
        if (bone == statusBone)
            bone->phase = 0.0f;
    }

    if (pelvisBone == nullptr) {
        fileFlags &= ~kPoseFlagHasPelvis;
        // PC: a pose of one bone has no second one to take (the ROM points
        // past the block).
        pelvisBone = boneCount > 1 ? &bones[1] : bones;
    } else {
        fileFlags |= kPoseFlagHasPelvis;
    }

    boneMapCount = (int)nameCount;
    boneMap = new int[boneMapCount];
    for (int i = 0; i < boneMapCount; i++)
        boneMap[i] = -1;
    for (int i = 0; i < boneCount && i < kPoseMaxModelBones; i++) {
        ModelPoseBone *bone = &bones[i];
        bone->modelBone = -1;
        const char *candidate = boneNames;
        for (int j = 0; j < (int)nameCount; j++, candidate += kPoseBoneName) {
            const char letter = bone->name[6];
            if (lowerNames == 0 && 'a' <= letter && letter <= 'z')
                lowerNames = 1;
            if (strcmp(bone->name, candidate) == 0) {
                if (j < kPoseMaxModelBones) {
                    bone->modelBone = j;
                    boneMap[j] = i;
                }
                break;
            }
        }
    }

    solve(PoseSlot_Final, -1, 0);

    if (headBone != nullptr) {
        sceVu0FVECTOR below;
        sceVu0FMATRIX inverse;
        copyVector(below, headBone->matrix[3]);
        below[1] -= kProbeHeight;
        invertRigidMatrix(inverse, headBone->matrix);
        applyMatrix(below, inverse, below);
        vecNormalize(headForward, below);
        headForward[3] = 1.0f;
        ModelPoseBone *above = headBone->parent;
        if (above != nullptr) {
            copyVector(below, above->matrix[3]);
            below[1] -= kProbeHeight;
            invertRigidMatrix(inverse, above->matrix);
            applyMatrix(below, inverse, below);
            vecNormalize(neckForward, below);
            neckForward[3] = 1.0f;
        }
    }
    return boneCount;
}

// ──────────────────────────────────────────────────────────────────────
// Bone files on their own, no pose.

// FS2 0x00150c90
// callcheck: strlen, strcpy, strcpy, strcmp, strlen, atoi
int motionBoneSuffix(void *motion, const char *prefix)
{
    const PoseBoneFile *file = static_cast<const PoseBoneFile *>(motion);
    const PoseBoneRecord *records = file->records();
    const size_t length = strlen(prefix);
    for (int i = 0; i < file->boneCount; i++) {
        if (strncmp(prefix, records[i].name, length) == 0)
            return atoi(records[i].name + length);
    }
    return -1;
}

// FS2 0x00150d80
// callcheck: strcpy, strlen, copyVectorWords, copyVectorWords, quatToMatrix, strcpy, strcpy, strcpy, strcmp, identityMatrix, multiplyMatrix
int motionBoneMatrix(sceVu0FMATRIX out, void *motion, const char *bone, int prefix)
{
    constexpr int kChainMax = 0x40;     // the ROM's 0x1000 bytes of stack
    const PoseBoneFile *file = static_cast<const PoseBoneFile *>(motion);
    const PoseBoneRecord *records = file->records();
    char wanted[kPoseBoneName * 2];
    strcpy(wanted, bone);

    sceVu0FMATRIX chain[kChainMax];
    int depth = 0;
    for (;;) {
        const size_t length = strlen(wanted);
        const PoseBoneRecord *found = nullptr;
        for (int i = 0; i < file->boneCount; i++) {
            char candidate[kPoseBoneName * 2];
            strcpy(candidate, records[i].name);
            if (prefix != 0)
                candidate[length] = 0;
            if (strcmp(wanted, candidate) == 0) {
                found = &records[i];
                break;
            }
        }
        if (found == nullptr)
            break;

        sceVu0FVECTOR quat;
        memcpy(quat, found->rotation, sizeof(quat));
        quatToMatrix(chain[depth], quat);
        memcpy(chain[depth][3], found->position, 3 * sizeof(float));
        chain[depth][3][3] = 1.0f;
        depth++;
        strcpy(wanted, found->parent);
        prefix = 0;
    }

    identityMatrix(out);
    for (int i = depth - 1; i >= 0; i--)
        multiplyMatrix(out, out, chain[i]);
    return depth;
}

// FS2 0x00150f50
// callcheck: ~ModelPoseBone, free, operator_delete[], operator_delete
ModelPose::~ModelPose()
{
    if (bones != nullptr) {
        for (int i = 0; i < boneCount; i++)
            bones[i].~ModelPoseBone();
        gGlobalHeap.free(bones);
    }
    delete[] boneMap;
}

// FS2 0x00151020
// callcheck: strcpy, identityMatrix
ModelPose::ModelPose()
{
    strcpy(tag, "mdl");
    identityMatrix(world);
    position[3] = 1.0f;
    setVector(rootScale, 1.0f, 1.0f, 1.0f, 1.0f);
    setVector(aim, 0.0f, 0.0f, kProbeHeight, 0.0f);
    setVector(aimTarget, 0.0f, 0.0f, kProbeHeight, 0.0f);
    setVector(headForward, 0.0f, 1.0f, 0.0f, 1.0f);
    setVector(neckForward, 0.0f, 1.0f, 0.0f, 1.0f);
    headRate = 0.2f;
    lookRate = 0.2f;
    toolRate = 0.2f;
    aimRate = 0.2f;
    footRate = 0.2f;
    headLimit = headLimitUp = headLimitDown = 0x1.e28c76p-1f; // 54 degrees
    lookRate2 = 0x1.41b2f8p+0f;                   // 72 degrees
    windOn = 1;
    setVector(wind, 1.0f, 1.0f, 0.0f, 0.7f);
}

// ──────────────────────────────────────────────────────────────────────
// ModelMotion

// FS2 0x002d0d10
// callcheck: bindTrack
int ModelMotion::bindTracksLayered(u_int flags, u_short groups)
{
    int bound = 0;
    for (int i = 0; i < pose->boneCount; i++) {
        ModelPoseBone *bone = &pose->bones[i];
        if ((groups == 0 || (bone->groups & groups) != 0) && tracks[i] != nullptr
            && bone->bindTrack(tracks[i], flags)) {
            bound++;
            bone->motion = static_cast<const PoseMotionFile *>(file);
        }
    }
    return bound;
}

// FS2 0x002d0e10
// callcheck: bindTrack, keyAt
int ModelMotion::bindTracks(u_int flags, u_short groups)
{
    const PoseMotionFile *source = static_cast<const PoseMotionFile *>(file);
    int bound = 0;
    for (int i = 0; i < pose->boneCount; i++) {
        ModelPoseBone *bone = &pose->bones[i];
        if ((groups == 0 || (bone->groups & groups) != 0) && tracks[i] != nullptr
            && bone->bindTrack(tracks[i], flags)) {
            bone->motion = source;
            bound++;
            if (source->flags & kMotionFileSeek) {
                const float *at = bone->keyAt(seekFrame);
                if (at != nullptr)
                    bone->track = at;
            }
        }
    }
    return bound;
}

// FS2 0x002d0f30
// callcheck: free, operator_delete
ModelMotion::~ModelMotion()
{
    if (tracks != nullptr)
        gGlobalHeap.free(tracks);
}

// FS2 0x002d0fa0
// callcheck: alloc, memset, strcmp
ModelMotion::ModelMotion(ModelPose *owner, void *motionFile)
{
    const PoseMotionFile *source = static_cast<const PoseMotionFile *>(motionFile);
    pose = owner;
    file = motionFile;
    trackCount = source->trackCount;
    seekFrame = source->params[3];
    if (owner->boneCount == 0) {
        tracks = nullptr;
        return;
    }
    tracks = static_cast<const PoseMotionTrack **>(
        gGlobalHeap.alloc(owner->boneCount * sizeof(*tracks), kTrackAlign));
    memset(tracks, 0, owner->boneCount * sizeof(*tracks));
    const PoseMotionTrack *records = source->tracks();
    for (int t = 0; t < trackCount; t++) {
        for (int i = 0; i < owner->boneCount; i++) {
            if (strcmp(owner->bones[i].name, records[t].bone) == 0)
                tracks[i] = &records[t];
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The swing of the sw bones. A spring runs after a point the bone would reach
// at rest; the bone then turns towards the spring's end or moves to it. A bone
// name carries it: ??sw?KA.., K the kind (upper case no wind, lower case wind,
// S, F, R, T, C), A the one axis it may turn around (x, y or z; upper case
// only by negative angles, lower case only by positive ones).

// The spring's step: swingOut is what comes of it. A cleared swingPos[3]
// (resetSwingBones) starts it where it should end.
// FS2 0x0014a090
void ModelPoseBone::stepSpring(bool windy)
{
    constexpr float kParamStep = 0.1f;
    if (swingPos[3] == 0.0f)
        copyVector(swingPos, swingTarget);

    const float damping = 1.0f - (float)(int)(s_char)swingParams[1] * kParamStep;
    sceVu0FVECTOR pull;
    subVector(pull, swingTarget, swingPos);
    scaleVec4(1.0f - (float)(int)(s_char)swingParams[0] * kParamStep, pull, pull);
    scaleVec4(damping, swingVelocity, swingVelocity);
    addVector(swingVelocity, swingVelocity, pull);
    addVector(swingPos, swingPos, swingVelocity);

    if (!windy || owner->windOn == 0)
        copyVector(swingOut, swingPos);
    else
        addWind(owner->wind);
    swingPos[3] = 1.0f;
    swingOut[3] = 1.0f;
}

// swingOut = the spring's end, blown. wind: a direction, w its strength; time,
// not used by a sw bone otherwise, is the flutter's phase.
// FS2 0x00149ee0
void ModelPoseBone::addWind(const sceVu0FVECTOR wind)
{
    constexpr float kParamStep = 0.1f;
    constexpr float kBlow = 5.0f;
    constexpr float kFlutter = 3.0f;
    constexpr float kPhaseStep = 0.01f;
    constexpr int   kPhases = 7;
    sceVu0FVECTOR blow;
    copyVector(blow, wind);
    const float flutter = 1.0f - (float)(int)(s_char)swingParams[1] * kParamStep;
    const float strength = 1.0f <= blow[3] ? 1.0f : blow[3];
    vecNormalize(blow, blow);
    scaleVec4((1.0f - (float)(int)(s_char)swingParams[0] * kParamStep) * strength * kBlow, blow, blow);

    float phase = time + (float)(modelBone % kPhases) * kPhaseStep + strength + 0.0f;
    time = phase;
    if (kPi < phase)
        time = phase - kTwoPi;
    const float sine = sinCos(time, 0);
    const float cosine = sinCos(time, 1);
    sceVu0FVECTOR side;
    setVector(side, flutter * sine * kFlutter, flutter * cosine * kFlutter, 0.0f, 1.0f);
    addVector(blow, blow, side);
    addVector(swingOut, swingPos, blow);
}

// The axis the name allows, and the side of it.
// FS2 0x00149cc0
void ModelPoseBone::limitSwingAxis(sceVu0FVECTOR axisAngle) const
{
    const char axis = name[7];
    int lane;
    if (axis == 'z' || axis == 'Z')
        lane = 2;
    else if (axis == 'y' || axis == 'Y')
        lane = 1;
    else if (axis == 'x' || axis == 'X')
        lane = 0;
    else
        return;
    const float along = axisAngle[lane];
    axisAngle[0] = 0.0f;
    axisAngle[1] = 0.0f;
    axisAngle[2] = 0.0f;
    axisAngle[lane] = along;
    if (along < 0.0f) {
        axisAngle[lane] = along * -1.0f;
        axisAngle[3] *= -1.0f;
    }
    if (axis < 'a' && 0.0f < axisAngle[3])
        axisAngle[3] = 0.0f;
    if ('a' < axis && axisAngle[3] < 0.0f)
        axisAngle[3] = 0.0f;
}

// Final = Bind turned by axisAngle, and the matrices from it. The tail of
// every branch of the swing in the ROM.
void ModelPoseBone::placeSwing(const sceVu0FVECTOR axisAngle)
{
    sceVu0FVECTOR quat;
    quatFromAxisAngle(quat, axisAngle);
    quatMultiply(rotation[PoseSlot_Final], rotation[PoseSlot_Bind], quat);
    copyVector(position[PoseSlot_Final], position[PoseSlot_Bind]);
    identityMatrix(matrix);
    quatToMatrix(matrix, rotation[PoseSlot_Final]);
    translateMatrix(matrix, matrix, position[PoseSlot_Final]);
    multiplyMatrix(matrix, parent->matrix, matrix);
    copyVector(worldPosition, matrix[3]);
}

// Moved to the spring's end, no turn. The tail of T and C.
void ModelPoseBone::placeSwingAtSpring()
{
    static const sceVu0FVECTOR kNoTurn = { 1.0f, 0.0f, 0.0f, 0.0f }; // data 0x2f5060, 0x2f5080
    sceVu0FMATRIX inverse;
    invertRigidMatrix(inverse, parent->matrix);
    sceVu0FVECTOR local;
    applyMatrix(local, inverse, swingOut);
    sceVu0FVECTOR bind;
    copyVector(bind, position[PoseSlot_Bind]);
    copyVector(position[PoseSlot_Bind], local);
    placeSwing(kNoTurn);
    copyVector(position[PoseSlot_Bind], bind);
}

// Letters F and R, and any other: a point ahead along the bone. 0x00149940 is
// a jump here, the entry F takes in the ROM.
// FS2 0x00149b40
// callcheck: identityMatrix, multiplyMatrix, quatMultiply, quatToMatrix, setVecLength, sinCos, translateMatrix (held by placeSwing and stepSpring)
void ModelPoseBone::swingAhead(bool windy)
{
    const float *bind = position[PoseSlot_Bind];
    setVector(swingTarget, sqrtf(bind[2] * bind[2] + bind[0] * bind[0] + bind[1] * bind[1]) + kSwingDrop,
              0.0f, 0.0f, 1.0f);
    applyMatrix(swingTarget, matrix, swingTarget);
    stepSpring(windy);
    sceVu0FVECTOR axisAngle = { 1.0f, 0.0f, 0.0f, 1.0f }; // data 0x2f5050
    aimOwnAxis(kHalfPi, axisAngle, swingOut, axisAngle);
    limitSwingAxis(axisAngle);
    placeSwing(axisAngle);
}

// Letter C, a skirt's middle: two thirds of the way down to between the calves.
// FS2 0x00149950
// callcheck: applyMatrix, identityMatrix, invertRigidMatrix, blendVec4, multiplyMatrix, quatMultiply, quatToMatrix, setVecLength, sinCos, translateMatrix (held by placeSwingAtSpring)
void ModelPoseBone::swingBetweenCalves(bool windy)
{
    constexpr float kCalfReach = 0.66f;
    if (name[7] != '0')
        return;
    ModelPoseBone *left = owner->findBone("bn_l_calf");
    ModelPoseBone *right = owner->findBone("bn_r_calf");
    if (left == nullptr || right == nullptr)
        return;
    sceVu0FVECTOR middle;
    blendVec4(0.5f, middle, left->worldPosition, right->worldPosition);
    subVector(swingTarget, middle, parent->worldPosition);
    scaleVec4(kCalfReach, swingTarget, swingTarget);
    addVector(swingTarget, swingTarget, parent->worldPosition);
    stepSpring(windy);
    placeSwingAtSpring();
}

// FS2 0x0014a1c0
// callcheck: applyMatrix, invertRigidMatrix, quatMultiply, setVecLength, sinCos (held by placeSwing and placeSwingAtSpring)
void ModelPoseBone::stepSwing()
{
    identityMatrix(matrix);
    quatToMatrix(matrix, rotation[PoseSlot_Bind]);
    translateMatrix(matrix, matrix, position[PoseSlot_Bind]);
    multiplyMatrix(matrix, parent->matrix, matrix);

    const char kind = name[6];
    switch (kind) {
    case 'S':
    case 's': {     // hangs straight down from where it is
        copyVector(swingTarget, matrix[3]);
        swingTarget[2] -= kSwingDrop;
        stepSpring(kind == 's');
        sceVu0FVECTOR axisAngle = { 1.0f, 0.0f, 0.0f, 1.0f }; // data 0x2f5070
        aimAxis(kPi, axisAngle, swingOut, axisAngle);
        limitSwingAxis(axisAngle);
        setVector(rotation[PoseSlot_Bind], 0.0f, 0.0f, 0.0f, 1.0f);
        placeSwing(axisAngle);
        break;
    }
    case 'F':
    case 'R':
        swingAhead(false);
        break;
    case 'f':
    case 'r':
        swingAhead(true);
        break;
    case 'C':
    case 'c':
        swingBetweenCalves(kind == 'c');
        break;
    case 'T':
    case 't':       // the bone's own place, by the spring
        copyVector(swingTarget, matrix[3]);
        stepSpring(kind == 't');
        placeSwingAtSpring();
        break;
    default:
        swingAhead(false);
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// What the solvers bend a bone with. All of them work on PoseSlot_Solve.

// FS2 0x0014a790
void ModelPoseBone::settle(float rate, int count)
{
    for (ModelPoseBone *bone = this; bone != nullptr && count != 0; bone = bone->parent, count--) {
        bone->solveWeight -= rate;
        if (bone->solveWeight < kWeightOff)
            bone->solveWeight = 0.0f;
    }
}

// FS2 0x0014a810
void ModelPoseBone::settleFixed(float rate, int count)
{
    for (ModelPoseBone *bone = this; bone != nullptr && count != 0; bone = bone->parent, count--) {
        bone->solveWeight += rate;
        if (kWeightFull < bone->solveWeight)
            bone->solveWeight = 1.0f;
    }
}

// Solve = slot towards Solve.
// FS2 0x0014a9b0
void ModelPoseBone::blendSolve(float t, int slot)
{
    quatSlerp(t, rotation[PoseSlot_Solve], rotation[slot], rotation[PoseSlot_Solve]);
}

// FS2 0x0014ad10
// callcheck: setVecLength, sinCos (held by quatFromAxisAngle)
bool ModelPoseBone::aimTurn(float limit, const sceVu0FVECTOR target, const sceVu0FVECTOR dir)
{
    sceVu0FVECTOR axisAngle;
    const bool limited = aimAxis(limit, axisAngle, target, dir);
    quatFromAxisAngle(rotation[PoseSlot_Solve], axisAngle);
    return limited;
}

// FS2 0x0014ac40
// callcheck: aimAxis, setVecLength, sinCos (held by aimTurn)
bool ModelPoseBone::aimAlongAxis(float limit, const sceVu0FVECTOR target, int axis)
{
    sceVu0FVECTOR dir = { 0.0f, 0.0f, 0.0f, 1.0f };
    if (axis < 0)
        axis = 0;
    if (3 < axis)
        axis = 2;
    dir[axis] = 1.0f;
    return aimTurn(limit, target, dir);
}

// aimAxis in the bone's own space rather than the parent's. Its origin is the
// matrix's, the place it aims from is Final through that same matrix (the ROM).
// FS2 0x0014aaa0
bool ModelPoseBone::aimOwnAxis(float limit, sceVu0FVECTOR out, const sceVu0FVECTOR target,
                               const sceVu0FVECTOR dir) const
{
    sceVu0FVECTOR tip = { 1.0f, 0.0f, 0.0f, 1.0f };
    if (dir != nullptr) {
        tip[0] = dir[0];
        tip[1] = dir[1];
        tip[2] = dir[2];
    }
    sceVu0FVECTOR origin;
    copyVector(origin, matrix[3]);
    applyMatrix(tip, matrix, tip);
    sceVu0FVECTOR from;
    subVector(from, tip, origin);
    vecNormalize(from, from);

    copyVector(origin, position[PoseSlot_Final]);
    applyMatrix(origin, matrix, origin);
    sceVu0FVECTOR to;
    subVector(to, target, origin);
    vecNormalize(to, to);

    float angle = asinAcos(vecDot(from, to), AsinAcos_Acos);
    vecCross(out, from, to);
    scaleVec4(kAimScale, out, out);
    sceVu0FMATRIX inverse;
    invertRigidMatrix(inverse, matrix);
    applyMatrix(out, inverse, out);
    vecNormalize(out, out);
    const bool over = limit < angle;
    if (over)
        angle = limit;
    const bool under = angle < -limit;
    if (under)
        angle = -limit;
    out[3] = angle;
    return over || under;
}

// Rebuilds the bone, then adds the turn to Solve.
// FS2 0x0014a9d0
// callcheck: setVecLength, sinCos (held by quatFromAxisAngle)
bool ModelPoseBone::aimOwnTurn(float limit, const sceVu0FVECTOR target, const sceVu0FVECTOR dir)
{
    buildMatrix(0.0f, PoseSlot_Solve, 0);
    sceVu0FVECTOR axisAngle;
    const bool limited = aimOwnAxis(limit, axisAngle, target, dir);
    sceVu0FVECTOR quat;
    quatFromAxisAngle(quat, axisAngle);
    quatMultiply(rotation[PoseSlot_Solve], rotation[PoseSlot_Solve], quat);
    return limited;
}

// Turns this bone so that the x axis of held, a bone down the chain, points
// through target: this bone goes around the normal of the plane of held's x
// axis and target. Solve = Current and that turn.
// FS2 0x0014af70
// callcheck: setVecLength, sinCos (held by quatFromAxisAngle)
void ModelPoseBone::aimThrough(float limit, const ModelPoseBone *held, const sceVu0FVECTOR target)
{
    static const sceVu0FVECTOR kX = { 1.0f, 0.0f, 0.0f, 0.0f };
    sceVu0FVECTOR axis;
    applyMatrix(axis, held->matrix, kX);
    sceVu0FVECTOR heldToTarget;
    subVector(heldToTarget, target, held->matrix[3]);
    sceVu0FVECTOR scratch;
    vecCross(scratch, axis, heldToTarget);
    sceVu0FVECTOR plane;
    vecNormalize(plane, scratch);
    plane[3] = -vecDot(plane, held->matrix[3]);

    // This bone on that plane; then the foot of it on held's axis.
    scaleVec4(plane[3] + vecDot(plane, matrix[3]), scratch, plane);
    sceVu0FVECTOR pivot;
    subVector(pivot, matrix[3], scratch);
    subVector(scratch, pivot, held->matrix[3]);
    scaleVec4(vecDot(axis, scratch), scratch, axis);
    sceVu0FVECTOR foot;
    addVector(foot, held->matrix[3], scratch);

    // Where on the axis it stands as far from pivot as target does. PC: the
    // EE's sqrt.s takes the magnitude; a root of a negative stays a number.
    subVector(scratch, target, pivot);
    const float toTargetSq = scratch[0] * scratch[0] + scratch[1] * scratch[1] + scratch[2] * scratch[2];
    subVector(scratch, foot, pivot);
    const float toFootSq = scratch[0] * scratch[0] + scratch[1] * scratch[1] + scratch[2] * scratch[2];
    scaleVec4(sqrtf(fabsf(toTargetSq - toFootSq)), scratch, axis);
    sceVu0FVECTOR reached;
    addVector(reached, foot, scratch);

    subVector(scratch, target, pivot);
    vecNormalize(scratch, scratch);
    sceVu0FVECTOR from;
    subVector(from, reached, pivot);
    vecNormalize(from, from);
    float angle = asinAcos(vecDot(from, scratch), AsinAcos_Acos);
    if (limit < angle)
        angle = limit;
    else if (angle < -limit)
        angle = -limit;

    sceVu0FMATRIX inverse;
    invertRigidMatrix(inverse, parent->matrix);
    setVector(scratch, plane[0], plane[1], plane[2], 0.0f);
    sceVu0FVECTOR axisAngle;
    applyMatrix(axisAngle, inverse, scratch);
    vecNormalize(axisAngle, axisAngle);
    axisAngle[3] = angle;
    sceVu0FVECTOR quat;
    quatFromAxisAngle(quat, axisAngle);
    quatMultiply(rotation[PoseSlot_Solve], rotation[PoseSlot_Current], quat);
}

// The turn (a quaternion, into out) that aims dir at target in two steps, in
// the xy plane by limit, then what is left by upLimit, or downLimit when the
// target is below dir.
// FS2 0x0014b270
// callcheck: setVecLength, sinCos (held by quatFromAxisAngle)
bool ModelPoseBone::aimLimited(float limit, float upLimit, float downLimit, sceVu0FVECTOR out,
                               const sceVu0FVECTOR target, const sceVu0FVECTOR dir) const
{
    bool limited = false;
    sceVu0FMATRIX base;
    if (parent == nullptr)
        identityMatrix(base);
    else
        copyMatrix(base, parent->matrix);
    sceVu0FMATRIX inverse;
    invertRigidMatrix(inverse, base);

    sceVu0FVECTOR tip = { 1.0f, 0.0f, 0.0f, 1.0f };
    if (dir != nullptr) {
        tip[0] = dir[0];
        tip[1] = dir[1];
        tip[2] = dir[2];
    }
    sceVu0FVECTOR origin;
    copyVector(origin, base[3]);
    applyMatrix(tip, base, tip);
    sceVu0FVECTOR from;
    subVector(from, tip, origin);
    vecNormalize(from, from);

    copyVector(origin, position[PoseSlot_Final]);
    applyMatrix(origin, base, origin);
    sceVu0FVECTOR to;
    subVector(to, target, origin);
    vecNormalize(to, to);

    sceVu0FVECTOR flatFrom, flatTo;
    setVector(flatFrom, from[0], from[1], 0.0f, 1.0f);
    vecNormalize(flatFrom, flatFrom);
    setVector(flatTo, to[0], to[1], 0.0f, 1.0f);
    vecNormalize(flatTo, flatTo);
    vecCross(out, flatFrom, flatTo);
    scaleVec4(kAimScale, out, out);
    applyMatrix(out, inverse, out);
    vecNormalize(out, out);
    float angle = asinAcos(vecDot(flatFrom, flatTo), AsinAcos_Acos);
    if (limit < angle) {
        limited = true;
        angle = limit;
    } else if (angle < -limit) {
        limited = true;
        angle = -limit;
    }
    out[3] = angle;
    sceVu0FVECTOR flatTurn;
    quatFromAxisAngle(flatTurn, out);

    quatToMatrix(base, flatTurn);
    sceVu0FVECTOR turned;
    applyMatrix(turned, base, from);
    vecCross(out, turned, to);
    scaleVec4(kAimScale, out, out);
    applyMatrix(out, inverse, out);
    vecNormalize(out, out);
    angle = asinAcos(vecDot(turned, to), AsinAcos_Acos);
    const float pitchLimit = to[2] < from[2] ? downLimit : upLimit;
    if (pitchLimit < angle) {
        limited = true;
        angle = pitchLimit;
    } else if (angle < -pitchLimit) {
        limited = true;
        angle = -pitchLimit;
    }
    out[3] = angle;
    sceVu0FVECTOR pitch;
    quatFromAxisAngle(pitch, out);
    quatMultiply(out, flatTurn, pitch);
    return limited;
}

// The knee of a two-bone leg, this bone the shin and its parent the thigh, by
// the bones' lengths: along, a share of the way from the hip to target; side,
// how far off that line. Past its reach the leg goes straight.
// FS2 0x0014a890
void ModelPoseBone::kneePlacement(float *along, float *side, const sceVu0FVECTOR target) const
{
    const float shin = length;
    const float thigh = parent->length;
    const float thighSq = thigh * thigh;
    const float reachSq = (shin + thigh) * (shin + thigh);
    sceVu0FVECTOR hip = { 0.0f, 0.0f, 0.0f, 1.0f };
    applyMatrix(hip, parent->matrix, hip);
    sceVu0FVECTOR delta;
    subVector(delta, target, hip);
    const float distanceSq = delta[2] * delta[2] + delta[0] * delta[0] + delta[1] * delta[1];
    if (reachSq < distanceSq) {
        *along = 1.0f;
        *side = 0.5f;
    } else {
        const float share = (distanceSq + (thighSq - shin * shin)) / (distanceSq * 2.0f);
        *along = share;
        // PC: the EE's sqrt.s takes the magnitude.
        *side = sqrtf(fabsf((thighSq + 0.0f) - distanceSq * share * share));
    }
}

// ──────────────────────────────────────────────────────────────────────
// The solvers.

// A leg to target: thigh, shin, then the foot flat on ground (its plane's
// normal), or level when it is off the ground. ground may be null.
// FS2 0x0014cc60
bool ModelPose::solveLeg(const sceVu0FVECTOR target, const sceVu0FVECTOR ground, bool left)
{
    constexpr float kFootAhead    = 100.0f;
    constexpr float kFootLift     = 10.0f;  // the toe's raw height where the foot stops following the ground
    constexpr float kFootAimLimit = 0.785398185f;
    constexpr float kFootEaseAir  = 0.1f;
    constexpr float kFootEaseDown = 0.2f;
    constexpr float kCalfTwist    = 0.6f;
    constexpr float kThighTwist   = 0.4f;
    if (leftToe == nullptr || rightToe == nullptr)
        return false;
    ModelPoseBone *toe = left ? leftToe : rightToe;
    ModelPoseBone *foot = toe->parent;
    ModelPoseBone *calf = foot->parent;
    ModelPoseBone *thigh = calf->parent;

    float along, side;
    calf->kneePlacement(&along, &side, target);
    sceVu0FVECTOR hip;
    copyVector(hip, thigh->matrix[3]);
    sceVu0FVECTOR bend = { 0.0f, 0.0f, 1.0f, 1.0f };
    applyMatrix(bend, thigh->matrix, bend);
    subVector(bend, bend, hip);
    sceVu0FVECTOR toTarget;
    subVector(toTarget, target, hip);
    vecCross(bend, bend, toTarget);
    vecNormalize(bend, bend);
    sceVu0FVECTOR straight, out;
    scaleVec4(along, straight, toTarget);
    scaleVec4(side, out, bend);
    sceVu0FVECTOR knee;
    addVector(knee, straight, out);
    addVector(knee, knee, hip);

    // The leg's twist: how far the foot looks away from the hips, flat, taken
    // from whichever of the foot's two axes lies flatter.
    sceVu0FVECTOR hipAhead = { 0.0f, 1.0f, 0.0f, 1.0f };  // data 0x2f5a40
    sceVu0FVECTOR footAhead = { 0.0f, 1.0f, 0.0f, 1.0f }; // data 0x2f5a50
    sceVu0FVECTOR footSide = { -1.0f, 0.0f, 0.0f, 1.0f }; // data 0x2f5a60
    thigh->parent->turnVector(hipAhead, hipAhead);
    foot->turnVector(footAhead, footAhead);
    foot->turnVector(footSide, footSide);
    sceVu0FVECTOR flat;
    copyVector(flat, fabsf(footAhead[2]) < fabsf(footSide[2]) ? footAhead : footSide);
    flat[2] = 0.0f;
    hipAhead[2] = 0.0f;
    sceVu0FVECTOR twist;
    quatAxisBetween(twist, hipAhead, flat);
    sceVu0FVECTOR calfTurn, thighTurn;
    setVector(calfTurn, -twist[2], 0.0f, 0.0f, twist[3] * kCalfTwist);
    quatFromAxisAngle(calfTurn, calfTurn);
    setVector(thighTurn, -twist[2], 0.0f, 0.0f, twist[3] * kThighTwist);
    quatFromAxisAngle(thighTurn, thighTurn);

    thigh->aimAlongAxis(kPi, knee, 0);
    quatMultiply(thigh->rotation[PoseSlot_Solve], thigh->rotation[PoseSlot_Solve], thighTurn);
    thigh->buildMatrix(0.0f, PoseSlot_Solve, 0);
    calf->aimAlongAxis(kPi, target, 0);
    quatMultiply(calf->rotation[PoseSlot_Solve], calf->rotation[PoseSlot_Solve], calfTurn);
    calf->buildMatrix(0.0f, PoseSlot_Solve, 0);

    const float toeHeight = toe->getRawHeight();
    sceVu0FVECTOR at, aimAt;
    if (ground == nullptr || kFootLift <= toeHeight) {
        setVector(aimAt, kFootAhead, 0.0f, 0.0f, 1.0f);    // data 0x2f5a70
        foot->turnVector(aimAt, aimAt);
        foot->getPosition(at);
        addVector(aimAt, aimAt, at);
        foot->aimAlongAxis(kFootAimLimit, aimAt, 0);
        foot->blendSolve(kFootEaseAir, PoseSlot_Final);
    } else {
        foot->getPosition(at);
        scaleVec4(-kFootAhead, aimAt, ground);
        addVector(aimAt, aimAt, at);
        foot->aimAlongAxis(kFootAimLimit, aimAt, 0);
        foot->blendSolve(kFootEaseDown, PoseSlot_Final);
    }
    return true;
}

// The feet on their targets: (normal of the ground, its height) each. The
// pelvis drops to let the higher foot reach, by half the gap each frame, and a
// foot is held at least 30 under the pelvis.
// FS2 0x0014d130
bool ModelPose::solveFeet(const float *left, const float *right)
{
    constexpr float kSteepGround = 0.9f;
    constexpr float kSteepDrop   = 3.0f;
    constexpr float kMinLegRoom  = 30.0f;
    if (leftToe == nullptr || rightToe == nullptr)
        return false;
    if (left == nullptr || right == nullptr)
        return false;

    float pelvisHeight = pelvisBone->getRawHeight();
    sceVu0FVECTOR leftGround, rightGround;
    copyVector(leftGround, left);
    const float pelvisBind = pelvisBone->position[PoseSlot_Bind][1];
    copyVector(rightGround, right);
    if (leftGround[2] < kSteepGround || rightGround[2] < kSteepGround)
        pelvisHeight -= kSteepDrop;

    float pelvisAt;
    if (leftGround[3] < rightGround[3]) {
        pelvisAt = rightGround[3] + pelvisHeight;
        if (pelvisBind < pelvisAt - leftGround[3])
            pelvisAt = leftGround[3] + pelvisBind;
        if (pelvisAt - rightGround[3] < kMinLegRoom)
            rightGround[3] = pelvisAt - kMinLegRoom;
    } else {
        pelvisAt = leftGround[3] + pelvisHeight;
        if (pelvisBind < pelvisAt - rightGround[3])
            pelvisAt = rightGround[3] + pelvisBind;
        if (pelvisAt - leftGround[3] < kMinLegRoom)
            leftGround[3] = pelvisAt - kMinLegRoom;
    }
    rootOffset[1] = (pelvisAt - (rootOffset[1] + pelvisHeight + position[2])) * 0.5f + rootOffset[1] + 0.0f;

    sceVu0FVECTOR ankle;
    leftToe->parent->getPosition(ankle);
    ankle[2] = leftGround[3] + leftToe->parent->getRawHeight();
    solveLeg(ankle, leftGround, true);
    rightToe->parent->getPosition(ankle);
    ankle[2] = rightGround[3] + rightToe->parent->getRawHeight();
    solveLeg(ankle, rightGround, false);
    return true;
}

// The left tool bone hung on the chest (spine1) at a fixed place, for the five
// named characters: what they carry there aims at the tool target, if any.
// FS2 0x0014d370
// callcheck: scaleVec4 (held by easeHalfway)
void ModelPose::solveChestTool(float rate)
{
    if (leftTool == nullptr || spine1Bone == nullptr)
        return;
    leftTool->parent = spine1Bone;
    copyVector(leftTool->position[PoseSlot_Current], chestToolPosition);
    copyVector(leftTool->position[PoseSlot_Solve], chestToolPosition);
    copyVector(leftTool->position[PoseSlot_Final], chestToolPosition);
    sceVu0FVECTOR quat;
    quatFromAxisAngle(quat, chestToolAxis);
    copyVector(leftTool->rotation[PoseSlot_Current], quat);
    copyVector(leftTool->rotation[PoseSlot_Solve], quat);
    copyVector(leftTool->rotation[PoseSlot_Final], quat);
    if (toolTo[3] != 0.0f) {
        easeHalfway(toolFrom, toolTo);
        leftTool->settleFixed(rate, 1);
        static const sceVu0FVECTOR kX = { 1.0f, 0.0f, 0.0f, 1.0f };
        leftTool->aimTurn(kTwoPi, toolFrom, kX);
    }
    leftTool->buildMatrix(0.0f, PoseSlot_Solve, 0);
    copyVector(leftTool->rotation[PoseSlot_Final], leftTool->rotation[PoseSlot_Solve]);
}

// The left arm reaches for the tool target: the elbow is put out by where the
// target lies in the shoulder's axes, the hand ends turned to it.
// FS2 0x0014d4c0
void ModelPose::solveTool(float rate)
{
    constexpr float kAimedWeight = 0.001f;
    constexpr float kNearSq      = 10000.0f;
    constexpr float kElbowDrop   = 100.0f;
    constexpr float kHandTurn    = -2.19911504f; // 0xc00cbe4c
    if (leftTool == nullptr || hasChestTool != 0)
        return;
    ModelPoseBone *hand = leftTool->parent;
    ModelPoseBone *wrist = hand->parent;
    ModelPoseBone *forearm = wrist->parent;
    ModelPoseBone *upperArm = forearm->parent;
    ModelPoseBone *shoulder = upperArm->parent;

    if (toolTo[3] == 0.0f) {
        hand->settle(rate, 4);
        if (hand->solveWeight < kAimedWeight)
            return;
    } else {
        easeHalfway(toolFrom, toolTo);
        hand->settleFixed(rate, 4);
    }

    sceVu0FVECTOR origin = { 0.0f, 0.0f, 0.0f, 1.0f };
    sceVu0FVECTOR axisX = { 1.0f, 0.0f, 0.0f, 1.0f };
    sceVu0FVECTOR axisY = { 0.0f, 1.0f, 0.0f, 1.0f };
    sceVu0FVECTOR axisZ = { 0.0f, 0.0f, 1.0f, 1.0f };
    applyMatrix(origin, shoulder->matrix, origin);
    applyMatrix(axisX, shoulder->matrix, axisX);
    subVector(axisX, axisX, origin);
    applyMatrix(axisY, shoulder->matrix, axisY);
    subVector(axisY, axisY, origin);
    applyMatrix(axisZ, shoulder->matrix, axisZ);
    subVector(axisZ, axisZ, origin);

    sceVu0FVECTOR toTool;
    subVector(toTool, toolFrom, origin);
    const float distanceSq = toTool[2] * toTool[2] + toTool[0] * toTool[0] + toTool[1] * toTool[1];
    vecNormalize(toTool, toTool);
    const float byZ = vecDot(axisZ, toTool);
    const float byX = vecDot(axisX, toTool);

    sceVu0FVECTOR elbowX, elbowZ, nearY, nearX;
    if (byX < 0.0f)
        scaleVec4(byX * 300.0f, elbowX, axisY);
    else
        scaleVec4(byX * 100.0f, elbowX, axisX);
    if (0.0f < byZ)
        scaleVec4(-byZ * 300.0f, elbowZ, axisY);
    else
        scaleVec4(0.0f, elbowZ, axisY);
    if (distanceSq < kNearSq) {
        scaleVec4((kNearSq - distanceSq) * 0.03f, nearY, axisY);
        scaleVec4((kNearSq - distanceSq) * 0.01f + 20.0f, nearX, axisX);
    } else {
        scaleVec4(0.0f, nearY, axisY);
        scaleVec4(20.0f, nearX, axisX);
    }
    sceVu0FVECTOR elbow;
    addVector(elbow, upperArm->matrix[3], elbowX);
    addVector(elbow, elbow, elbowZ);
    addVector(elbow, elbow, nearX);
    addVector(elbow, elbow, nearY);
    elbow[2] -= kElbowDrop;

    upperArm->aimAlongAxis(kHalfPi, elbow, 0);
    upperArm->buildMatrix(0.0f, PoseSlot_Solve, 0);
    forearm->aimAlongAxis(kPi, toolFrom, 0);
    forearm->buildMatrix(0.0f, PoseSlot_Solve, 0);
    wrist->buildMatrix(0.0f, PoseSlot_Solve, 0);

    sceVu0FVECTOR toolAxis = { 1.0f, 0.0f, 0.0f, 1.0f };
    sceVu0FMATRIX toolTurn;
    quatToMatrix(toolTurn, leftTool->rotation[PoseSlot_Current]);
    applyMatrix(toolAxis, toolTurn, toolAxis);
    hand->aimTurn(kPi, toolFrom, toolAxis);
    sceVu0FVECTOR quat = { 1.0f, 0.0f, 0.0f, kHandTurn };
    quatFromAxisAngle(quat, quat);
    quatMultiply(hand->rotation[PoseSlot_Solve], quat, hand->rotation[PoseSlot_Solve]);
}

// The upper body turns to the look target, spine0 by 40% of limit and spine1
// by 60%; given a bone, so that the bone's x axis (a held gun's) points at it.
// FS2 0x0014d990
// callcheck: scaleVec4 (held by easeHalfway)
void ModelPose::solveLook(float limit, float rate, const ModelPoseBone *held)
{
    constexpr float kAimedWeight = 0.001f;
    constexpr float kLowerShare  = 0.4f;
    constexpr float kUpperShare  = 0.6f;
    constexpr float kLookEase    = 0.3f;
    constexpr int   kHeldChain   = 8;       // the bones from the spine down to a tool
    if (spine0Bone == nullptr || spine1Bone == nullptr)
        return;
    if (lookTo[3] == 0.0f) {
        spine0Bone->settle(rate, 1);
        spine1Bone->settle(rate, 1);
        if (spine0Bone->solveWeight < kAimedWeight)
            return;
    } else {
        easeHalfway(lookFrom, lookTo);
        spine0Bone->settleFixed(rate, 1);
        spine1Bone->settleFixed(rate, 1);
    }
    sceVu0FVECTOR target;
    copyVector(target, lookFrom);
    target[3] = 0.0f;

    if (held == nullptr) {
        sceVu0FVECTOR up, dir;
        copyVector(up, rootBone->matrix[2]);
        ModelPoseBone *neck = neckBone;
        if (neck != nullptr)
            target[2] = lookFrom[2] + (spine0Bone->getRawHeight() - neck->getRawHeight());
        spine0Bone->getPosition(dir);
        addVector(dir, dir, up);
        sceVu0FMATRIX inverse;
        invertRigidMatrix(inverse, spine0Bone->matrix);
        applyMatrix(dir, inverse, dir);
        spine0Bone->aimOwnTurn(limit * kLowerShare, target, dir);
        spine0Bone->blendSolve(kLookEase, PoseSlot_Final);
        spine0Bone->buildMatrix(0.0f, PoseSlot_Solve, 0);

        if (neckBone != nullptr)
            target[2] = lookFrom[2] + (spine1Bone->getRawHeight() - neckBone->getRawHeight());
        spine1Bone->getPosition(dir);
        addVector(dir, dir, up);
        invertRigidMatrix(inverse, spine1Bone->matrix);
        applyMatrix(dir, inverse, dir);
        spine1Bone->aimOwnTurn(limit * kUpperShare, target, dir);
        spine1Bone->blendSolve(kLookEase, PoseSlot_Final);
        spine1Bone->buildMatrix(0.0f, PoseSlot_Solve, 0);

        // The arms follow; the left from one bone further up than the right (the ROM).
        if (leftUpperArm != nullptr) {
            leftUpperArm->parent->parent->parent->buildMatrix(0.0f, PoseSlot_Solve, 0);
            leftUpperArm->parent->parent->buildMatrix(0.0f, PoseSlot_Solve, 0);
            leftUpperArm->parent->buildMatrix(0.0f, PoseSlot_Solve, 0);
            leftUpperArm->buildMatrix(0.0f, PoseSlot_Solve, 0);
        }
        if (rightUpperArm != nullptr) {
            rightUpperArm->parent->parent->buildMatrix(0.0f, PoseSlot_Solve, 0);
            rightUpperArm->parent->buildMatrix(0.0f, PoseSlot_Solve, 0);
            rightUpperArm->buildMatrix(0.0f, PoseSlot_Solve, 0);
        }
    } else {
        spine0Bone->aimThrough(limit * kLowerShare, held, target);
        spine0Bone->blendSolve(kLookEase, PoseSlot_Final);
        spine0Bone->buildMatrix(0.0f, PoseSlot_Solve, 0);
        if (held == leftTool || held == rightTool) {
            spine1Bone->buildMatrix(0.0f, PoseSlot_Solve, 0);
            for (int depth = kHeldChain - 1; depth >= 0; depth--) {
                ModelPoseBone *bone = const_cast<ModelPoseBone *>(held);
                for (int up = 0; up < depth; up++)
                    bone = bone->parent;
                bone->buildMatrix(0.0f, PoseSlot_Solve, 0);
            }
        }
        spine1Bone->aimThrough(limit * kUpperShare, held, target);
        spine1Bone->blendSolve(kLookEase, PoseSlot_Final);
        spine1Bone->buildMatrix(0.0f, PoseSlot_Solve, 0);
    }
}

// The neck, then the head, to the head target, each by the head limits.
// FS2 0x0014df50
// callcheck: scaleVec4 (held by easeHalfway)
void ModelPose::solveHead(float rate)
{
    constexpr float kAimedWeight = 0.001f;
    constexpr float kHeadEase    = 0.3f;
    if (headBone == nullptr)
        return;
    if (headTo[3] == 0.0f) {
        headBone->settle(rate, 2);
        if (headBone->solveWeight < kAimedWeight)
            return;
    } else {
        headBone->settleFixed(rate, 2);
    }
    easeHalfway(headFrom, headTo);

    ModelPoseBone *neck = headBone->parent;
    neck->aimLimited(headLimit, headLimitUp, headLimitDown, neck->rotation[PoseSlot_Solve], headFrom, neckForward);
    neck->blendSolve(kHeadEase, PoseSlot_Final);
    neck->buildMatrix(0.0f, PoseSlot_Solve, 0);
    headBone->aimLimited(headLimit, headLimitUp, headLimitDown, headBone->rotation[PoseSlot_Solve], headFrom,
                         headForward);
    headBone->blendSolve(kHeadEase, PoseSlot_Final);
}

// Written in modelpose.h, so not seen by the tools there:
// ModelPoseBone::getPosition FS2 0x0014b620, getRawHeight FS2 0x0014b630,
// getValue FS2 0x0014b970, the implicit ~ModelPoseBone FS2 0x0014cac0,
// ModelPose::getLowerNames FS2 0x0014e0a0, setWindOn FS2 0x0014e0b0,
// popEvent FS2 0x0014e410, isWalking FS2 0x0014e930, clearPlacement
// FS2 0x0014ef60, setPosition FS2 0x0014ef90, getPosition FS2 0x0014f190,
// getPosition2 FS2 0x0014f1b0, rotate FS2 0x0014f1d0, findBone FS2 0x0014f2a0,
// resetSwingBones FS2 0x0014f430, getBoneMatrix FS2 0x0014f8c0. The jump
// FS2 0x00149940 is swingAhead.
