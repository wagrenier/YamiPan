/* ==========================================================================
 *  game/modelpose.h
 *
 *  The motion player a StageModel drives (+0xc): the run unassigned@00149510,
 *  0x14e0a0..0x151020 and its subclasses, a class of its own, not written
 *  yet. The file name and the class names are the port's; the ROM's classes
 *  only carry a three-letter tag at +0x14 ("mdl", "car", "elv", "nml",
 *  "sky"), which gave the subclasses their names.
 *
 *  What is here: the fields StageModel reads, the prototypes it calls and, as
 *  bodies, the short accessors its loops cannot run without (applyMotion
 *  spins on getPosition, advance turns the object by what rotate left).
 *  load and update are still stubs: a pose has no bones yet, and models with
 *  motions stand in their bind pose.
 * ======================================================================== */
#ifndef _GAME_MODELPOSE_H
#define _GAME_MODELPOSE_H

#include <stddef.h>
#include <string.h>

#include "game/mathutil.h"
#include "game/vu0math.h"
#include "sdk/libvu0.h"
#include "sdk/scetypes.h"

constexpr int kPoseBoneName    = 0x10;  // the longest bone name, its nul included
constexpr int kPoseEventName   = 0x10;
constexpr int kPoseEventQueue  = 8;

// What the motion's flags start from (StageModel::playMotion).
enum MotionPlayFlag
{
    MotionPlay_Base = 0x02,     // always set here (unread)
    MotionPlay_Arg0 = 0x20,     // the callers' first bool (unread)
};

// The kind of pose StageModel::setBaseMotion builds.
enum ModelPoseKind
{
    ModelPose_Model    = 0,     // ModelPose
    ModelPose_Car      = 1,     // CarPose
    ModelPose_Elevator = 2,     // ElevatorPose
    ModelPose_Nml      = 3,     // NmlPose
    ModelPose_Sky      = 4,     // SkyPose
};

// One bone of a pose (EE 0x300, the record array at +0x15c).
constexpr u_char kPoseBoneHasValue = 0x20;
struct ModelPoseBone
{
    // Its keyed value, what a lens bone holds; 0 without one.
    // FS2 0x0014b970
    float getValue() const { return (flags & kPoseBoneHasValue) ? value : 0.0f; }

    u_char        flags;        // FS2 +0x00
    u_char        _001[0x37];
    char          name[kPoseBoneName]; // FS2 +0x38
    u_char        _048[0x38];
    sceVu0FMATRIX matrix;       // FS2 +0x80: a StageMatrixNode's matrix
    u_char        _0c0[0x60];
    float         value;        // FS2 +0x120
    u_char        _124[0xfc];
    sceVu0FVECTOR _220;         // FS2 +0x220: on the root, turned by ModelPose::rotate
    sceVu0FVECTOR _230;         // FS2 +0x230: the same
    u_char        _240[0x3c];
    float         swing;        // FS2 +0x27c: cleared on the sw bones (resetSwingBones)
    u_char        _280[0x80];
};
static_assert(offsetof(ModelPoseBone, name) == 0x38, "ModelPoseBone::name");
static_assert(offsetof(ModelPoseBone, matrix) == 0x80, "ModelPoseBone::matrix");
static_assert(offsetof(ModelPoseBone, swing) == 0x27c, "ModelPoseBone::swing");
static_assert(sizeof(ModelPoseBone) == 0x300, "ModelPoseBone: EE 0x300");

class ModelPose;

// A motion file as one pose plays it (EE 0x14, the list values of
// StageModel::motions).
// layout: host -- EE offsets in the comments.
struct ModelMotion
{
    ModelMotion(ModelPose *owner, void *motionFile);                     // FS2 0x002d0fa0
    ~ModelMotion();                                                      // FS2 0x002d0f30: deleting

    ModelPose *pose = nullptr;  // FS2 +0x00
    void      *file = nullptr;  // FS2 +0x04: the name at +0x10
    const void **tracks = nullptr; // FS2 +0x08: a heap block, per pose bone its track, or null
    int        trackCount = 0;  // FS2 +0x0c: the file's +1
    int        _10 = 0;         // FS2 +0x10: the file's +0x2c
};

// layout: host -- EE: the vptr at +0, offsets in the comments. EE 0x3c0, a
// subclass 0x3e0..0x4e0.
class ModelPose
{
public:
    ModelPose();                                                         // FS2 0x00151020
    virtual ~ModelPose();                                                // FS2 0x00150f50: slot 0 (vtable 0x311230 +8), deleting
    // Slot 1: reads the file, its bones matched to boneCount names of
    // kPoseBoneName chars each (null, 0: its own).
    virtual void load(void *file, int arg, const char *boneNames, u_int boneCount); // FS2 0x0014ff90
    // Slot 2: frames of the game's 30 forward.
    virtual void update(float frames, bool loop);                        // FS2 0x0014f4b0

    // iterations 3, count -1 (all), flaggedOnly 0 at the one caller here.
    int solve(int iterations, int count, int flaggedOnly);               // FS2 0x0014e4d0
    int play(ModelMotion *motion, u_int flags, int arg);                 // FS2 0x0014fab0
    // The same on the layer over the base (flag 0x20000).
    int playLayered(ModelMotion *motion, u_int flags, int arg);          // FS2 0x0014f910
    // play and playLayered on a file with no ModelMotion; null: the current one.
    int playFile(void *file, u_int flags, int arg);                      // FS2 0x0014fca0
    int playFileLayered(void *file, u_int flags, int arg);               // FS2 0x0014f950
    // out: two frames and the blend of the surface animation; 0 without one.
    int getSurfaceFrames(float *out);                                    // FS2 0x0014e300

    // FS2 0x0014ef60
    void clearPlacement()
    {
        memset(position, 0, sizeof(position));
        memset(rotation, 0, sizeof(rotation));
    }
    // Under a parent: into the parent's space, when a motion plays.
    // FS2 0x0014ef90
    // callcheck: invertRigidMatrix, applyMatrix
    void setPosition(const float *pos)
    {
        if (parentMatrix != nullptr && current != nullptr) {
            sceVu0FMATRIX inverse;
            invertRigidMatrix(inverse, parentMatrix);
            applyMatrix(position, inverse, pos);
        } else {
            memcpy(position, pos, sizeof(position));
        }
        position[3] = 1.0f;
    }
    // FS2 0x0014f190
    void getPosition(float *out) const { memcpy(out, position, 3 * sizeof(float)); }
    // The ROM's second copy, the one StageModel::advance calls.
    // FS2 0x0014f1b0
    void getPosition2(float *out) const { memcpy(out, position, 3 * sizeof(float)); }
    // Turns the pose by delta, then solves the flagged bones.
    // PC: the guard is for the stub load, which leaves no root.
    // FS2 0x0014f1d0
    // callcheck: rotationMatrixXyz, applyMatrix, applyMatrix, multiplyMatrix, FUN_00135e20, solve (FUN_00135e20: addAngles wraps the same)
    void rotate(const float *delta)
    {
        if (delta[0] == 0.0f && delta[1] == 0.0f && delta[2] == 0.0f)
            return;
        sceVu0FVECTOR step;
        memcpy(step, delta, sizeof(step));
        sceVu0FMATRIX turn;
        rotationMatrixXyz(turn, step);
        if (rootBone != nullptr) {
            applyMatrix(rootBone->_220, turn, rootBone->_220);
            applyMatrix(rootBone->_230, turn, rootBone->_230);
        }
        multiplyMatrix(placement, turn, placement);
        addAngles(rotation, rotation, step);
        solve(3, -1, 1);
    }
    // The bones named ??sw.. stop swinging.
    // FS2 0x0014f430
    void resetSwingBones()
    {
        for (int i = 0; i < boneCount; i++) {
            ModelPoseBone *bone = &bones[i];
            if (bone->name[3] == 's' && bone->name[4] == 'w')
                bone->swing = 0.0f;
        }
    }
    // The matrix of the pose's bone for the model's bone index, null if the
    // pose has none: the model's own bones then.
    // FS2 0x0014f8c0
    sceVu0FVECTOR *getBoneMatrix(int modelBone) const
    {
        int index = modelBone < boneMapCount ? boneMap[modelBone] : -1;
        return index < 0 ? nullptr : bones[index].matrix;
    }
    // The pose's bone of that name; null none.
    // FS2 0x0014f2a0
    ModelPoseBone *findBone(const char *boneName) const
    {
        for (int i = 0; i < boneCount; i++) {
            if (strcmp(boneName, bones[i].name) == 0)
                return &bones[i];
        }
        return nullptr;
    }
    // The oldest queued event of the motion into out; false when none.
    // FS2 0x0014e410
    bool popEvent(char *out)
    {
        memcpy(out, events[0], kPoseEventName);
        int i = 0;
        for (; i < kPoseEventQueue - 1 && events[i][0] != 0; i++)
            memcpy(events[i], events[i + 1], kPoseEventName);
        events[i][0] = 0;
        return i != 0;
    }

    char           name[kPoseBoneName] = {}; // FS2 +0x004: the model's (kyo.., ryh_ky..)
    char           tag[8] = {};      // FS2 +0x014: "mdl" and the subclasses'
    u_int          flags = 0x1000;   // FS2 +0x01c
    sceVu0FVECTOR  rotation = {};    // FS2 +0x030
    sceVu0FVECTOR  position = {};    // FS2 +0x040
    sceVu0FMATRIX  _50 = {};         // FS2 +0x050: identity from the ctor
    sceVu0FMATRIX  placement = {};   // FS2 +0x090: turned by rotate
    const sceVu0FVECTOR *parentMatrix = nullptr; // FS2 +0x150: the pose hangs in this matrix, the object follows it
    int            boneCount = 0;    // FS2 +0x158
    ModelPoseBone *bones = nullptr;  // FS2 +0x15c
    int           *boneMap = nullptr; // FS2 +0x160: model bone to pose bone, or -1
    int            boneMapCount = 0; // FS2 +0x164
    ModelPoseBone *rootBone = nullptr; // FS2 +0x168
    ModelPoseBone *neckBone = nullptr; // FS2 +0x184: bn_d_neck on the ryh_ky / ekm_ky models
    void          *current = nullptr; // FS2 +0x1a4: the motion file playing
    char           events[kPoseEventQueue][kPoseEventName] = {}; // FS2 +0x1e0
    sceVu0FVECTOR  neckLimits = {};  // FS2 +0x350: by setBaseMotion on the five named characters (unread)
    sceVu0FVECTOR  neckAxis = {};    // FS2 +0x360: the same; (0, 0, 1, pi/2) (unread)
    u_char         hasNeckLimits = 0; // FS2 +0x3b0
};

// The subclasses, in the runs that use them (a kind of setBaseMotion).
// layout: host.
class CarPose : public ModelPose        // vtable 0x317c50, EE 0x4e0
{
public:
    CarPose();                                                           // FS2 0x0027ef70
};
// layout: host.
class ElevatorPose : public ModelPose   // vtable 0x317c70, EE 0x460
{
public:
    ElevatorPose();                                                      // FS2 0x0027ffb0
};
// layout: host.
class NmlPose : public ModelPose        // vtable 0x318b10, EE 0x410
{
public:
    NmlPose(int a, int b, int c, int d);                                 // FS2 0x002a7a50
};
// layout: host.
class SkyPose : public ModelPose        // vtable 0x319260, EE 0x3e0
{
public:
    SkyPose(int a, int b);                                               // FS2 0x002b37d0
};

#endif /* _GAME_MODELPOSE_H */
