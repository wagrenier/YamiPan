/* ==========================================================================
 *  game/modelpose.h
 *
 *  The motion player a StageModel drives (+0xc): game/modelpose.cpp, the run
 *  0x149510..0x151270, with ModelMotion 0x2d0d10..0x2d10f0. The file name and
 *  the class names are the port's; the ROM's classes only carry a three-letter
 *  tag at +0x14 ("mdl", "car", "elv", "nml", "sky"), which gave the subclasses
 *  their names.
 *
 *  A pose is a skeleton read from a bone file and played by motion files, both
 *  used in place as they came off the disc (the record types below). The
 *  solvers bend a played pose towards targets (feet, tools, head); they run
 *  under m_solvers, whose writer is not found yet. The bones named ??sw.. swing
 *  on springs.
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
constexpr int kPoseMaxBones    = 0x40;  // ModelPose::load refuses more
constexpr int kPoseMaxModelBones = 0x80;

// What the motion's flags start from (StageModel::playMotion).
enum MotionPlayFlag
{
    MotionPlay_Base = 0x02,     // always set here: PosePlay_Ease
    MotionPlay_Arg0 = 0x20,     // the callers' first bool: PosePlay_Loop
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

// What a bone file holds (the file given to ModelPose::load): this header, then
// boneCount records. Read in place, EE layout.
struct PoseBoneRecord
{
    char  name[kPoseBoneName];   // FS2 +0x00
    char  parent[kPoseBoneName]; // FS2 +0x10
    float position[3];           // FS2 +0x20
    float length;                // FS2 +0x2c: to the next bone; for sw bones four bytes instead (swingParams)
    float rotation[4];           // FS2 +0x30: a quaternion
};
static_assert(sizeof(PoseBoneRecord) == 0x40, "PoseBoneRecord: EE 0x40");
static_assert(offsetof(PoseBoneRecord, position) == 0x20, "PoseBoneRecord::position");
static_assert(offsetof(PoseBoneRecord, length) == 0x2c, "PoseBoneRecord::length");
static_assert(offsetof(PoseBoneRecord, rotation) == 0x30, "PoseBoneRecord::rotation");

struct PoseBoneFile
{
    u_char kind;                 // FS2 +0x00
    s_char boneCount;            // FS2 +0x01
    short  flags;                // FS2 +0x02
    u_char _04[0xc];
    char   name[kPoseBoneName];  // FS2 +0x10

    const PoseBoneRecord *records() const { return reinterpret_cast<const PoseBoneRecord *>(this + 1); }
};
static_assert(sizeof(PoseBoneFile) == 0x20, "PoseBoneFile: EE 0x20");
static_assert(offsetof(PoseBoneFile, flags) == 2, "PoseBoneFile::flags");
static_assert(offsetof(PoseBoneFile, name) == 0x10, "PoseBoneFile::name");

// What a motion file holds: this header, then trackCount tracks, then each
// track's keys. A key is, in this order and as the track's bits say, the
// values of PoseTrack, and last its length in frames, negative on the track's
// last key.
struct PoseMotionTrack
{
    char   bone[kPoseBoneName];  // FS2 +0x00
    int    keysOffset;           // FS2 +0x10: from this record
    u_char tracks;               // FS2 +0x14: PoseTrack bits
    s_char phase;                // FS2 +0x15: the frame this bone starts from, with ModelPoseBone::phase
    u_char _16[0xa];

    const float *keys() const
    {
        return reinterpret_cast<const float *>(reinterpret_cast<const char *>(this) + keysOffset);
    }
};
static_assert(sizeof(PoseMotionTrack) == 0x20, "PoseMotionTrack: EE 0x20");
static_assert(offsetof(PoseMotionTrack, keysOffset) == 0x10, "PoseMotionTrack::keysOffset");
static_assert(offsetof(PoseMotionTrack, tracks) == 0x14, "PoseMotionTrack::tracks");
static_assert(offsetof(PoseMotionTrack, phase) == 0x15, "PoseMotionTrack::phase");

constexpr u_int kMotionFileSeek = 0x1000; // PoseMotionFile::flags: playing starts at params[3]

struct PoseMotionFile
{
    u_char kind;                 // FS2 +0x00
    s_char trackCount;           // FS2 +0x01
    short  flags;                // FS2 +0x02
    short  frames;               // FS2 +0x04
    u_char _06[0xa];
    char   name[kPoseBoneName];  // FS2 +0x10
    float  params[4];            // FS2 +0x20: [3] the frame play seeks to

    const PoseMotionTrack *tracks() const { return reinterpret_cast<const PoseMotionTrack *>(this + 1); }
};
static_assert(sizeof(PoseMotionFile) == 0x30, "PoseMotionFile: EE 0x30");
static_assert(offsetof(PoseMotionFile, trackCount) == 1, "PoseMotionFile::trackCount");
static_assert(offsetof(PoseMotionFile, flags) == 2, "PoseMotionFile::flags");
static_assert(offsetof(PoseMotionFile, frames) == 4, "PoseMotionFile::frames");
static_assert(offsetof(PoseMotionFile, name) == 0x10, "PoseMotionFile::name");
static_assert(offsetof(PoseMotionFile, params) == 0x20, "PoseMotionFile::params");

// What a key of a track holds (PoseMotionTrack::tracks, ModelPoseBone::tracks).
enum PoseTrack
{
    PoseTrack_Position = 0x01,  // three words
    PoseTrack_Quat     = 0x02,  // four
    PoseTrack_Euler    = 0x04,  // three
    PoseTrack_Event    = 0x08,  // four: a name, queued as the key is reached
    PoseTrack_Value    = 0x20,  // one: a plain number (a lens bone's)
    PoseTrack_Follow   = 0x40,  // none: placed by the parent, turned by the root
};
constexpr int kPoseKeyPosition = 3;
constexpr int kPoseKeyQuat     = 4;
constexpr int kPoseKeyEuler    = 3;
constexpr int kPoseKeyEvent    = 4;
constexpr int kPoseKeyValue    = 1;

// ModelPoseBone::playFlags; the flags given to play, so ModelPose::flags too.
enum PosePlayFlag : u_int
{
    PosePlay_Moved    = 0x00001, // bound since the last update
    PosePlay_Ease     = 0x00002, // eased between keys; dropped with the first key passed
    PosePlay_Done     = 0x00004, // a track run out, not looping
    PosePlay_Ending   = 0x00008, // at its last frames
    PosePlay_FromKey  = 0x00010, // the blend starts at the first key, not where the bone is
    PosePlay_Loop     = 0x00020,
    PosePlay_NoBlend  = 0x00040, // readKey ignores the previous key; on ModelPose::flags
                                 // the root carries the object instead
    PosePlay_AtStart  = 0x00080, // from frame 0 whatever the bone's phase
    PosePlay_Grow     = 0x00100, // ModelPose::flags: bones grow by the tables of solve
    PosePlay_SameKeep = 0x02000, // the motion already playing is left alone
    PosePlay_Layered  = 0x20000, // over the base motion
};
constexpr u_int kPoseFlagFootSolve = 0x200000; // ModelPose::flags: feet follow setFootTargets; survives play
constexpr u_int kPoseFlagFresh     = 0x001000; // ModelPose::flags: not updated yet
constexpr u_int kPoseFlagHasPelvis = 0x000100; // ModelPose::fileFlags

// ModelPose::m_solvers. No writer found yet.
enum PoseSolver
{
    PoseSolver_Feet = 0x1,
    PoseSolver_Head = 0x2,
    PoseSolver_Tool = 0x4,
    PoseSolver_Look = 0x8,
    kPoseSolverMask = 0xf,
};

// ModelPose::walkFlags.
enum PoseWalkFlag
{
    PoseWalk_Active   = 0x01,
    PoseWalk_ToAim    = 0x04,   // walkAim was given
    PoseWalk_ToTarget = 0x08,   // walkTarget was given
    PoseWalk_Stopped  = 0x10,
};

// Slots of ModelPoseBone::position and ::rotation.
enum PoseSlot
{
    PoseSlot_From    = 0,       // the key behind
    PoseSlot_To      = 1,       // the key ahead
    PoseSlot_Current = 2,       // played
    PoseSlot_Final   = 3,       // after the solvers
    PoseSlot_Bind    = 4,       // the bone file's
    PoseSlot_Solve   = 5,       // what the solvers read; a copy of Current
    PoseSlot_Target  = 6,       // what a solver asks for
    kPoseSlots       = 7,
};

// Bone groups, ModelPoseBone::groups, the mask play takes.
enum PoseBoneGroup : u_short
{
    PoseGroup_Face      = 0x001,
    PoseGroup_Spine     = 0x002,
    PoseGroup_Clavicle  = 0x004,
    PoseGroup_LeftArm   = 0x008,
    PoseGroup_RightArm  = 0x010,
    PoseGroup_LeftHand  = 0x020,
    PoseGroup_RightHand = 0x040,
    PoseGroup_Lower     = 0x080,
    PoseGroup_Status    = 0x100,
    PoseGroup_Upper     = 0x200,
    PoseGroup_Trunk     = 0x400,    // the bones solve(.., flaggedOnly) walks
};

class ModelPose;

// One bone of a pose (EE 0x300, the record array at +0x15c).
// layout: host -- EE: the pointers at +0x08..+0x18 are 4 bytes, so what follows
// them sits further on here; EE offsets in the comments.
struct ModelPoseBone
{
    ModelPoseBone();                                                     // FS2 0x0014cb10

    void setup(const PoseBoneRecord *record);                            // FS2 0x0014c980
    // Takes the track if it is this bone's; false otherwise.
    bool bindTrack(const PoseMotionTrack *record, u_int flags);          // FS2 0x0014c710
    // Puts the bone at a frame of its track; slot < 0 only finds the key.
    bool seek(float frame, int slot);                                    // FS2 0x0014bbe0
    // The keys around the cursor into PoseSlot_From and _To.
    void readKey(const float *at, const float *previous);                // FS2 0x0014be10
    // slot = a towards b.
    bool interpolate(float t, int slot, int a, int b);                   // FS2 0x0014b9a0
    // The matrices from a slot and the parent's. record also keeps the history.
    bool buildMatrix(float frames, int slot, int record);                // FS2 0x0014b650
    // Forward along the track.
    bool advance(float frames);                                          // FS2 0x0014c340
    // The root carrying the object (PosePlay_NoBlend on the pose).
    bool stepRoot(float frames, int restart);                            // FS2 0x0014c0c0
    // The key at a frame of the track, null past the last.
    const float *keyAt(float frame) const;                               // FS2 0x0014c670
    // The axis (xyz) and angle (w, limited) that turn dir, in the parent's
    // space, to the target.
    bool aimAxis(float limit, sceVu0FVECTOR out, const sceVu0FVECTOR target,
                 const sceVu0FVECTOR dir) const;                         // FS2 0x0014ada0
    // out = in through the bone's turn.
    void turnVector(sceVu0FVECTOR out, const sceVu0FVECTOR in) const;    // FS2 0x0014b5f0
    // FS2 0x0014b620
    void getPosition(sceVu0FVECTOR out) const { memcpy(out, matrix[3], sizeof(sceVu0FVECTOR)); }
    // FS2 0x0014b630
    float getRawHeight() const { return rawMatrix[3][2]; }
    // Its keyed value, what a lens bone holds; 0 without one.
    // FS2 0x0014b970
    float getValue() const { return (tracks & PoseTrack_Value) ? position[PoseSlot_Current][0] : 0.0f; }
    // The swing of a sw bone, after the bones it hangs from.
    void stepSwing();                                                    // FS2 0x0014a1c0
    void stepSpring(bool windy);                                         // FS2 0x0014a090
    void addWind(const sceVu0FVECTOR wind);                              // FS2 0x00149ee0
    void limitSwingAxis(sceVu0FVECTOR axisAngle) const;                  // FS2 0x00149cc0
    void placeSwing(const sceVu0FVECTOR axisAngle);
    void placeSwingAtSpring();
    void swingAhead(bool windy);                                         // FS2 0x00149b40
    void swingBetweenCalves(bool windy);                                 // FS2 0x00149950
    // solveWeight down and up, count bones from this one to the root.
    void settle(float rate, int count);                                  // FS2 0x0014a790
    void settleFixed(float rate, int count);                             // FS2 0x0014a810
    // Solve = slot towards Solve.
    void blendSolve(float t, int slot);                                  // FS2 0x0014a9b0
    // Solve = the turn that aims dir at target; aimAlongAxis takes a lane of
    // the bone for dir. True when limit cut it short.
    bool aimTurn(float limit, const sceVu0FVECTOR target, const sceVu0FVECTOR dir); // FS2 0x0014ad10
    bool aimAlongAxis(float limit, const sceVu0FVECTOR target, int axis); // FS2 0x0014ac40
    // aimAxis in the bone's own space; the same added to Solve.
    bool aimOwnAxis(float limit, sceVu0FVECTOR out, const sceVu0FVECTOR target,
                    const sceVu0FVECTOR dir) const;                      // FS2 0x0014aaa0
    bool aimOwnTurn(float limit, const sceVu0FVECTOR target, const sceVu0FVECTOR dir); // FS2 0x0014a9d0
    void aimThrough(float limit, const ModelPoseBone *held, const sceVu0FVECTOR target); // FS2 0x0014af70
    bool aimLimited(float limit, float upLimit, float downLimit, sceVu0FVECTOR out,
                    const sceVu0FVECTOR target, const sceVu0FVECTOR dir) const; // FS2 0x0014b270
    void kneePlacement(float *along, float *side, const sceVu0FVECTOR target) const; // FS2 0x0014a890

    // The bones named ??sw.. swing.
    bool isSwing() const { return name[3] == 's' && name[4] == 'w'; }

    u_char         tracks = 0;   // FS2 +0x00: PoseTrack bits of the bound track
    u_char         grown = 0;    // FS2 +0x01: growth applies
    u_short        groups = 0;   // FS2 +0x02: PoseBoneGroup
    u_int          playFlags = PosePlay_Done | PosePlay_Ending; // FS2 +0x04
    ModelPose     *owner = nullptr;  // FS2 +0x08
    ModelPoseBone *parent = nullptr; // FS2 +0x0c
    const PoseMotionFile *motion = nullptr; // FS2 +0x10
    const float   *track = nullptr;  // FS2 +0x14: the first key
    const float   *key = nullptr;    // FS2 +0x18: the cursor
    int            keyWords = 0;     // FS2 +0x1c: floats a key takes
    float          duration = 0.0f;  // FS2 +0x20: frames of the motion
    float          keyTime = 0.0f;   // FS2 +0x24: into this key
    float          keySpan = 0.0f;   // FS2 +0x28: this key's length
    float          time = 0.0f;      // FS2 +0x2c: into the motion
    float          overshoot = 0.0f; // FS2 +0x30: frames left at the end
    float          lowFrames = 0.0f; // FS2 +0x34: frames spent with the raw height under 3 (a foot's contact)
    char           name[kPoseBoneName] = {};       // FS2 +0x38
    char           parentName[kPoseBoneName] = {}; // FS2 +0x48
    sceVu0FVECTOR  growth = { 0.0f, 0.0f, 0.0f, 1.0f }; // FS2 +0x60
    int            modelBone = 0;    // FS2 +0x70: its index in the model, or -1
    float          phase = 1.0f;     // FS2 +0x74
    float          startFrame = 0.0f; // FS2 +0x78
    sceVu0FMATRIX  matrix;           // FS2 +0x80: a StageMatrixNode's matrix
    sceVu0FMATRIX  rawMatrix;        // FS2 +0xc0: the played pose, no solvers
    sceVu0FVECTOR  position[kPoseSlots]; // FS2 +0x100: PoseSlot
    float          targetWeight = 0.0f;  // FS2 +0x170: how far Final moves to Target
    float          solveWeight = 0.0f;   // FS2 +0x174: how far Final moves to Solve
    sceVu0FVECTOR  rotation[kPoseSlots]; // FS2 +0x180: PoseSlot
    float          length = 0.0f;    // FS2 +0x1f0: to the next bone (kneePlacement); 15 on a sw bone
    sceVu0FVECTOR  worldPosition = { 0.0f, 0.0f, 0.0f, 1.0f };     // FS2 +0x200
    sceVu0FVECTOR  lastWorldPosition = { 0.0f, 0.0f, 0.0f, 1.0f }; // FS2 +0x210
    sceVu0FVECTOR  rawPosition = { 0.0f, 0.0f, 0.0f, 1.0f };       // FS2 +0x220: on the root, turned by ModelPose::rotate
    sceVu0FVECTOR  lastRawPosition = { 0.0f, 0.0f, 0.0f, 1.0f };   // FS2 +0x230: the same
    sceVu0FVECTOR  turn = { 0.0f, 0.0f, 0.0f, 1.0f };              // FS2 +0x240
    sceVu0FVECTOR  lastTurn = { 0.0f, 0.0f, 0.0f, 1.0f };          // FS2 +0x250
    sceVu0FVECTOR  swingTarget = {}; // FS2 +0x260: the spring's rest point
    sceVu0FVECTOR  swingPos = {};    // FS2 +0x270: where it is; w 0: starts from swingTarget (resetSwingBones)
    sceVu0FVECTOR  swingOut = {};    // FS2 +0x280: the same with the wind
    sceVu0FVECTOR  swingVelocity = {}; // FS2 +0x290
    u_char         swingParams[4] = {}; // FS2 +0x2a0: [0] loose, [1] undamped, tenths; read signed
};
static_assert(offsetof(ModelPoseBone, owner) == 8, "ModelPoseBone::owner");
static_assert(sizeof(ModelPoseBone) >= 0x2a4, "ModelPoseBone: EE 0x300");

// A motion file as one pose plays it (EE 0x14, the list values of
// StageModel::motions).
// layout: host -- EE offsets in the comments.
struct ModelMotion
{
    ModelMotion(ModelPose *owner, void *motionFile);                     // FS2 0x002d0fa0
    ~ModelMotion();                                                      // FS2 0x002d0f30: deleting

    // The same as ModelPose::playFile's loop with the tracks found already.
    int bindTracks(u_int flags, u_short groups);                         // FS2 0x002d0e10
    int bindTracksLayered(u_int flags, u_short groups);                  // FS2 0x002d0d10

    ModelPose *pose = nullptr;  // FS2 +0x00
    void      *file = nullptr;  // FS2 +0x04: a PoseMotionFile
    const PoseMotionTrack **tracks = nullptr; // FS2 +0x08: a heap block, per pose bone its track, or null
    int        trackCount = 0;  // FS2 +0x0c: the file's
    float      seekFrame = 0.0f; // FS2 +0x10: the file's params[3]
};

// layout: host -- EE: the vptr at +0, offsets in the comments. EE 0x3c0, a
// subclass 0x3e0..0x4e0.
class ModelPose
{
public:
    ModelPose();                                                         // FS2 0x00151020
    virtual ~ModelPose();                                                // FS2 0x00150f50: slot 0 (vtable 0x311230 +8), deleting
    // Slot 1: reads the bone file, its bones matched to boneCount names of
    // kPoseBoneName chars each (null, 0: its own). The bone count, -2 if refused.
    virtual int load(void *file, int arg, const char *boneNames, u_int boneCount); // FS2 0x0014ff90
    // Slot 2: frames of the game's 30 forward.
    virtual void update(float frames, bool loop);                        // FS2 0x0014f4b0

    // Rebuilds the bones from a PoseSlot; count -1 all, flaggedOnly the
    // PoseGroup_Trunk ones.
    void solve(int slot, int count, int flaggedOnly);                    // FS2 0x0014e4d0
    int play(ModelMotion *motion, u_int flags, int groups);              // FS2 0x0014fab0
    // The same on the layer over the base (flag 0x20000).
    int playLayered(ModelMotion *motion, u_int flags, int groups);       // FS2 0x0014f910
    // play and playLayered on a file with no ModelMotion; null: the current one.
    int playFile(void *file, u_int flags, int groups);                   // FS2 0x0014fca0
    int playFileLayered(void *file, u_int flags, int groups);            // FS2 0x0014f950
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
    // PC: the guard is for the subclasses, still stubs with no root.
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
            applyMatrix(rootBone->rawPosition, turn, rootBone->rawPosition);
            applyMatrix(rootBone->lastRawPosition, turn, rootBone->lastRawPosition);
        }
        multiplyMatrix(placement, turn, placement);
        addAngles(rotation, rotation, step);
        solve(PoseSlot_Final, -1, 1);
    }
    // The bones named ??sw.. stop swinging.
    // FS2 0x0014f430
    void resetSwingBones()
    {
        for (int i = 0; i < boneCount; i++) {
            if (bones[i].isSwing())
                bones[i].swingPos[3] = 0.0f;
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

    // Into the event queue; false when full or the name is empty.
    bool pushEvent(const char *event);                                   // FS2 0x0014e3b0
    // The world matrix from another one (null: the pose's own), no turn left.
    void resetPlacement(const sceVu0FVECTOR *from);                      // FS2 0x0014e8b0
    // One frame of the walk startWalk asked for.
    void stepWalk(float frames);                                         // FS2 0x0014e940
    // Moves the pose towards target over frames: from and aim may be null.
    void startWalk(float frames, const float *target, const float *aimAt); // FS2 0x0014ee60
    bool stopWalk();                                                     // FS2 0x0014e900
    bool isWalking() const { return (walkFlags & PoseWalk_Active) != 0; } // FS2 0x0014e930
    // Where a bone of a file's record would be, at origin facing dir; out, offset.
    void getWalkStart(float *out, float *offset, const float *origin, const float *dir);  // FS2 0x0014ed30
    void getRecordOffset(float *out, float *offset, const PoseBoneRecord *record,
                         const float *origin, const float *dir);         // FS2 0x0014ee00
    void placeRecord(float *out, const PoseBoneRecord *record, const float *origin,
                     const float *dir);                                  // FS2 0x0014f080
    // Moves every bone and the pose by delta.
    void translate(const float *delta);                                  // FS2 0x0014e470
    // A bone's PosePlay_Ending by name.
    bool isBoneEnding(const char *boneName) const;                       // FS2 0x0014f360
    void setRootScale(float x, float y, float z, float w);               // FS2 0x0014f490
    bool getLowerNames() const { return lowerNames != 0; }               // FS2 0x0014e0a0
    void setWindOn(u_char value) { windOn = value; }                     // FS2 0x0014e0b0
    // The world positions of the toes, into whichever is not null.
    void getFootPositions(float *left, float *right) const;              // FS2 0x0014f010
    void setFootTargets(float rate, const float *left, const float *right); // FS2 0x0014e0c0
    void setToolTarget(float rate, const float *target);                 // FS2 0x0014e150
    void setLookTarget(float rate, float rate2, const float *target);    // FS2 0x0014e1b0
    void setAimDirection(float rate, const float *direction);            // FS2 0x0014e200
    void setHeadTarget(float rate, const float *target);                 // FS2 0x0014e270

    // The solvers, under m_solvers (solveChestTool under hasChestTool).
    void solveLook(float limit, float rate, const ModelPoseBone *held);  // FS2 0x0014d990
    void solveHead(float rate);                                          // FS2 0x0014df50
    void solveTool(float rate);                                          // FS2 0x0014d4c0
    void solveChestTool(float rate);                                     // FS2 0x0014d370
    bool solveFeet(const float *left, const float *right);               // FS2 0x0014d130
    bool solveLeg(const sceVu0FVECTOR target, const sceVu0FVECTOR ground, bool left); // FS2 0x0014cc60

    char           name[kPoseBoneName] = {}; // FS2 +0x004: the model's (kyo.., ryh_ky..)
    char           tag[8] = {};      // FS2 +0x014: "mdl" and the subclasses'
    int            fileFlags = 0;    // FS2 +0x018: the bone file's; kPoseFlagHasPelvis
    u_int          flags = kPoseFlagFresh; // FS2 +0x01c: the play flags
    u_int          m_solvers = 0;    // FS2 +0x020: PoseSolver
    sceVu0FVECTOR  rotation = {};    // FS2 +0x030
    sceVu0FVECTOR  position = {};    // FS2 +0x040
    sceVu0FMATRIX  world = {};       // FS2 +0x050: identity from the ctor; row 3 the position
    sceVu0FMATRIX  placement = {};   // FS2 +0x090: turned by rotate
    sceVu0FVECTOR  rootOffset = {};  // FS2 +0x0d0: added to the second bone each update
    sceVu0FVECTOR  rootScale = {};   // FS2 +0x0e0: of what the root carries
    sceVu0FVECTOR  aim = {};         // FS2 +0x0f0: where the root faces, easing to aimTarget
    sceVu0FVECTOR  aimTarget = {};   // FS2 +0x100
    sceVu0FMATRIX  aimMatrix = {};   // FS2 +0x110
    const sceVu0FVECTOR *parentMatrix = nullptr; // FS2 +0x150: the pose hangs in this matrix, the object follows it
    u_char         fileKind = 0;     // FS2 +0x154
    int            boneCount = 0;    // FS2 +0x158
    ModelPoseBone *bones = nullptr;  // FS2 +0x15c
    int           *boneMap = nullptr; // FS2 +0x160: model bone to pose bone, or -1
    int            boneMapCount = 0; // FS2 +0x164
    ModelPoseBone *rootBone = nullptr;   // FS2 +0x168: bn_base, else the first
    ModelPoseBone *pelvisBone = nullptr; // FS2 +0x16c: bn_pelvis, else the second
    ModelPoseBone *leftToe = nullptr;    // FS2 +0x170
    ModelPoseBone *rightToe = nullptr;   // FS2 +0x174
    ModelPoseBone *faceBone = nullptr;   // FS2 +0x178
    ModelPoseBone *statusBone = nullptr; // FS2 +0x17c
    ModelPoseBone *headBone = nullptr;   // FS2 +0x180
    ModelPoseBone *neckBone = nullptr;   // FS2 +0x184: bn_neck; bn_d_neck on the ryh_ky / ekm_ky models
    ModelPoseBone *spine0Bone = nullptr; // FS2 +0x188
    ModelPoseBone *spine1Bone = nullptr; // FS2 +0x18c
    ModelPoseBone *leftUpperArm = nullptr;  // FS2 +0x190
    ModelPoseBone *rightUpperArm = nullptr; // FS2 +0x194
    ModelPoseBone *leftTool = nullptr;   // FS2 +0x198
    ModelPoseBone *rightTool = nullptr;  // FS2 +0x19c
    ModelPoseBone *lookBone = nullptr; // FS2 +0x1a0: given to solveLook; no writer found yet
    const PoseMotionFile *current = nullptr; // FS2 +0x1a4: the motion file playing
    u_char         motionKind = 0;   // FS2 +0x1a8
    char           motionName[0x13] = {}; // FS2 +0x1a9
    int            motionTracks = 0; // FS2 +0x1bc
    u_int          motionFlags = 0;  // FS2 +0x1c0: the file's; kMotionFileSeek
    float          motionFrames = 0.0f; // FS2 +0x1c4
    float          motionParams[4] = {}; // FS2 +0x1d0
    char           events[kPoseEventQueue][kPoseEventName] = {}; // FS2 +0x1e0
    sceVu0FVECTOR  walkTarget = {};  // FS2 +0x260
    sceVu0FVECTOR  walkAim = {};     // FS2 +0x270
    float          walkFrames = 0.0f; // FS2 +0x280
    u_int          walkFlags = 0;    // FS2 +0x284: PoseWalkFlag
    sceVu0FVECTOR  headForward = {}; // FS2 +0x290: down the head, in its space (load)
    sceVu0FVECTOR  neckForward = {}; // FS2 +0x2a0: the same for its parent
    sceVu0FVECTOR  headFrom = {};    // FS2 +0x2b0
    sceVu0FVECTOR  headTo = {};      // FS2 +0x2c0
    sceVu0FVECTOR  lookFrom = {};    // FS2 +0x2d0
    sceVu0FVECTOR  lookTo = {};      // FS2 +0x2e0
    sceVu0FVECTOR  toolFrom = {};    // FS2 +0x2f0
    sceVu0FVECTOR  toolTo = {};      // FS2 +0x300
    sceVu0FVECTOR  leftFootTarget = {};  // FS2 +0x310
    sceVu0FVECTOR  _320 = {};            // FS2 +0x320
    sceVu0FVECTOR  rightFootTarget = {}; // FS2 +0x330
    sceVu0FVECTOR  _340 = {};            // FS2 +0x340
    sceVu0FVECTOR  chestToolPosition = {}; // FS2 +0x350: by setBaseMotion on the five named characters, on spine1
    sceVu0FVECTOR  chestToolAxis = {};     // FS2 +0x360: the same; (0, 0, 1, pi/2)
    float          headRate = 0.0f;  // FS2 +0x370
    float          headLimit = 0.0f; // FS2 +0x374: aimLimited, across
    float          headLimitUp = 0.0f;   // FS2 +0x378
    float          headLimitDown = 0.0f; // FS2 +0x37c
    float          lookRate = 0.0f;  // FS2 +0x380
    float          lookRate2 = 0.0f; // FS2 +0x384
    float          toolRate = 0.0f;  // FS2 +0x388
    float          aimRate = 0.0f;   // FS2 +0x38c: aim to aimTarget
    float          footRate = 0.0f;  // FS2 +0x390
    u_char         lowerNames = 0;   // FS2 +0x394: a bone name's seventh letter is lower case
    u_char         windOn = 0;       // FS2 +0x395: the lower-case sw bones feel wind
    sceVu0FVECTOR  wind = {};        // FS2 +0x3a0: direction, w its strength
    u_char         hasChestTool = 0; // FS2 +0x3b0: solveChestTool runs, solveTool does not

private:
    // The head play and playFile share (the ROM has it twice): false when the
    // motion is kept as it is.
    bool beginMotion(const PoseMotionFile *file, u_int playFlags, u_short groups);
};

// The subclasses, in the runs that use them (a kind of setBaseMotion). CarPose
// and ElevatorPose are in game/vehiclepose.h, NmlPose in game/numlock.h,
// SkyPose in game/sevenkeys.h.

// A bone's matrix in the bind pose of a bone file, through its parents; the
// number of bones walked. prefix: the name need only start with bone.
int motionBoneMatrix(sceVu0FMATRIX out, void *motion, const char *bone, int prefix); // FS2 0x00150d80
// The number after the name of the first bone starting with prefix; -1 none.
int motionBoneSuffix(void *motion, const char *prefix);                  // FS2 0x00150c90

// quat math under the pose (the run's head).
// out = the axis from a to b (the cross of the two, normalized first), w the angle.
void quatAxisBetween(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b); // FS2 0x00149510
void quatSlerp(float t, sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b); // FS2 0x00149580
// out = a b, Hamilton's product.
void quatMultiply(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b);    // FS2 0x001496f0
void quatToMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR q);             // FS2 0x00149730
// out = the quaternion of an axis (xyz) and an angle (w).
void quatFromAxisAngle(sceVu0FVECTOR out, const sceVu0FVECTOR axisAngle); // FS2 0x001497c0
// The heading of v in the xy plane.
float vecHeading(const sceVu0FVECTOR v);                                 // FS2 0x00149830

#endif /* _GAME_MODELPOSE_H */
