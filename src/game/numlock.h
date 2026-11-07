/* ========================================================================
 *  game/numlock.h -- the number lock (0x2a6ab0..0x2a7b10).
 *
 *  A padlock with four dials of eight numbers each: the map object the
 *  models oMK21_06 and oSJ08_07 become (createMapObject), and NmlPose, the
 *  pose StageModel::setBaseMotion gives a model of kind ModelPose_Nml, which
 *  turns the four dial bones. The code is the stage result's (+0x90), so it
 *  stays the same across retries; a stage without one draws four random
 *  digits once.
 *
 *  Who turns the dials: the player's special motion (SpNumLockLink, game/
 *  playermotion.cpp), the script natives 0x201 / 0x202 / 0x22c and the
 *  number-lock view (the scene's +0x90, game/gadgetview.h).
 *
 *  The file is the port's name: no source string says what the ROM called it.
 * ======================================================================== */
#ifndef GAME_NUMLOCK_H
#define GAME_NUMLOCK_H

#include "game/mapobject.h"
#include "game/modelpose.h"

constexpr int kNumLockDials = 4;        // dials on a lock, the code's digits
constexpr int kNumLockNumbers = 8;      // numbers on a dial, 0..7
constexpr int kNumLockStack = 1;        // pushDials' stack

// The pose of the lock's model: ModelPose with four dial bones whose angles
// ease towards the numbers set on them.
// layout: host -- EE: vptr at +0, subclass of ModelPose (EE 0x3c0), EE 0x410;
// offsets in the comments.
class NmlPose : public ModelPose        // vtable 0x318b10
{
public:
    // The four dials' numbers; they show at once (setDials).
    NmlPose(int dial0, int dial1, int dial2, int dial3);                 // FS2 0x002a7a50
    ~NmlPose() override;                                                 // FS2 0x002a79f0: slot 0, deleting
    // ModelPose::load, then the bones it works on; the dials snap to their
    // numbers. Answers 1, not the bone count.
    int load(void *file, int arg, const char *boneNames, u_int boneCount) override; // FS2 0x002a7890: slot 1
    // ModelPose::update without the walk and the solvers; the dials ease a
    // fifth of the way each frame, or snap after setDials.
    void update(float frames, bool loop) override;                       // FS2 0x002a7420: slot 2

    // FS2 0x002a7800
    int  getDial(int dial) const { return (int)dialNumber[dial]; }
    // FS2 0x002a7820
    void setDial(int dial, int number) { dialNumber[dial] = (float)number; }
    // All four, shown at once on the next update.
    void setDials(int dial0, int dial1, int dial2, int dial3);           // FS2 0x002a7840

    // Each dial's angle a 1/frames step to its number's, the short way; the
    // dial bones take the angles about x.
    int  easeDials(const float *numbers, int frames);                    // FS2 0x002a75d0

    // Bone indices; the ROM's ctor leaves them, load sets the ones it finds.
    int   kanaguBone = 0;               // FS2 +0x3c0: bn_kanagu, the shackle (unread)
    int   keyBone = 0;                  // FS2 +0x3c4: bn_key (unread here)
    int   dialBones[kNumLockDials] = {}; // FS2 +0x3c8: bn_k04, bn_k03, bn_k02, bn_k01
    float dialAngle[kNumLockDials] = {}; // FS2 +0x3e0: radians, -number * 2pi / 8 when still
    float dialNumber[kNumLockDials] = {}; // FS2 +0x3f0: what each dial is set to
    u_char snapDials = 0;               // FS2 +0x400: the next update snaps instead of easing
};

// layout: host -- EE 0x270 in a block of 0x280, offsets in the comments.
// Vtables 0x318870, 0x318894 at +0x1c, 0x3188ac at +0xf4, 0x318abc at +0x204.
class NumLock : public StageMapObject
{
public:
    NumLock();                                                           // FS2 0x002a7340
    ~NumLock() override;                                                 // FS2 0x002a6ab0

    // FS2 0x002a6b40
    int  getKind() override { return StageObject_NumLock; }

    // The frame's steps: all empty but sideHook2, the base's.
    // FS2 0x002a6b60
    void sideHook0() override {}
    // FS2 0x002a71c0
    void sideHook1() override {}
    // FS2 0x002a6b50
    void sideHook3() override {}

    // The base's with the placement moved to bn_top, then the code.
    void setMotionList(void *list, int arg, int arg2) override;          // FS2 0x002a71d0

    // The number-lock view's, as it opens and closes: empty.
    // FS2 0x002a6e30
    void setInView(bool) {}

    // Four new dials, none on the code's digit or where it stood.
    void scramble();                                                     // FS2 0x002a6b70
    // m_active and the dials onto the stack (a negative number keeps its
    // dial); the depth, -1 when full. pop: back, the depth, -1 when empty.
    int  pushDials(int dial0, int dial1, int dial2, int dial3);          // FS2 0x002a6cd0
    int  popDials();                                                     // FS2 0x002a6c40
    // One dial to any number but the code's digit and the one it shows.
    void spinDial(int dial);                                             // FS2 0x002a6e40
    // One dial to the code's digit.
    void setDial(int dial);                                              // FS2 0x002a6ee0
    // One dial a number down / up, round the eight.
    void dialDown(int dial);                                             // FS2 0x002a6f00
    void dialUp(int dial);                                               // FS2 0x002a6f60
    // Every dial on the code's digit.
    bool dialsRight();                                                   // FS2 0x002a6fc0
    // A dial is still turning to its number.
    bool dialsTurning();                                                 // FS2 0x002a7070

    // The model's pose (StageModel +0xc).
    NmlPose *dialPose() const { return static_cast<NmlPose *>(m_model->pose); }

    char   m_code[kNumLockDials] = {};  // FS2 +0x240: the digits, from StageResult::numLockCode
    u_char m_savedActive[kNumLockStack] = {}; // FS2 +0x244
    u_char m_savedDials[kNumLockStack][kNumLockDials] = {}; // FS2 +0x245
    int    m_depth = 0;                 // FS2 +0x24c: of the stack
    // The light the number-lock view hands ModelInstance::setLight.
    sceVu0FVECTOR m_lightDirection = {}; // FS2 +0x250: (0, pi/2, 0, 0)
    sceVu0FVECTOR m_lightColor = {};     // FS2 +0x260: (1, 1, 1, 0)
};

#endif // GAME_NUMLOCK_H
