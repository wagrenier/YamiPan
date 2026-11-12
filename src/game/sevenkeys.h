/* ========================================================================
 *  game/sevenkeys.h -- the seven keys (0x2b22f0..0x2b3860).
 *
 *  The map models oYC11_21 .. oYC11_27 (createMapObject: "oYC11_2" and a
 *  digit 1..7) become SevenKeys, the object kind 6 that takes the motion
 *  named Seven_Keys (StageScene). Each is a lock of two dials of four
 *  positions; its pose is SkyPose (the tag "sky"), which turns the dial
 *  bones bn_k02 and bn_k01 about x. Turning the second dial turns the first
 *  with it.
 *
 *  The first of them gives the scene its view (StageScene::owned[2], the
 *  seven-key handle, game/gadgetview.h): the left stick's up / down picks a
 *  dial, left / right turns it (EVE_S11_01), Use or Squat leaves; "mon"
 *  sparkles on bn_top while in view. Script native 0x205 sets the dials and
 *  the code (SceneOwnedObject::setSevenKeyNumbers), the mission's restore
 *  check asks whether the dials stand at it (isSevenKeyOpen), and the
 *  property 0x6b reads a dial (sevenKeysPoseValue).
 *
 *  The run is NumLock's twin (game/numlock.h, game/gadgetview.cpp): same
 *  shapes, two dials of four instead of four of eight. The file is the
 *  port's name: no source string says what the ROM called it; "Seven_Keys"
 *  is the motion's name.
 * ======================================================================== */
#ifndef GAME_SEVENKEYS_H
#define GAME_SEVENKEYS_H

#include "game/gadgetview.h"
#include "game/mapobject.h"
#include "game/modelpose.h"

constexpr int kSevenKeysDials = 2;          // dials on a key
constexpr int kSevenKeysPositions = 4;      // positions on a dial, 0..3

// The pose of a key's model: ModelPose with two dial bones whose angles ease
// towards the positions set on them.
// layout: host -- EE: vptr at +0, subclass of ModelPose (EE 0x3c0), EE 0x3e0;
// offsets in the comments.
class SkyPose : public ModelPose        // vtable 0x319260
{
public:
    // The two dials' positions; they show at once (setDials).
    SkyPose(int dial0, int dial1);                                       // FS2 0x002b37d0
    ~SkyPose() override;                                                 // FS2 0x002b3770: slot 0, deleting
    // ModelPose::load, then the dial bones; the dials snap to their
    // positions. Answers 1, not the bone count.
    int load(void *file, int arg, const char *boneNames, u_int boneCount) override; // FS2 0x002b36b0: slot 1
    // ModelPose::update without the walk and the solvers; the dials ease a
    // fifth of the way each frame, or snap after setDials.
    void update(float frames, bool loop) override;                       // FS2 0x002b32c0: slot 2

    // FS2 0x002b3640
    int  getDial(int dial) const { return (int)dialPosition[dial]; }
    // FS2 0x002b3660
    void setDial(int dial, int position) { dialPosition[dial] = (float)position; }
    // Both, shown at once on the next update.
    void setDials(int dial0, int dial1);                                 // FS2 0x002b3680

    // Each dial's angle a 1/frames step to its position's, the short way;
    // the dial bones take the angles about x.
    int  easeDials(const float *positions, int frames);                  // FS2 0x002b3470

    // Bone indices; the ROM's ctor leaves them, load sets the ones it finds.
    int   dialBones[kSevenKeysDials] = {};   // FS2 +0x3c0: bn_k02, bn_k01
    float dialAngle[kSevenKeysDials] = {};   // FS2 +0x3c8: radians, -position * 2pi / 4 when still
    float dialPosition[kSevenKeysDials] = {}; // FS2 +0x3d0: what each dial is set to
    u_char snapDials = 0;                    // FS2 +0x3d8: the next update snaps instead of easing
};

// One of the seven keys.
// layout: host -- EE 0x2d0 in a block of 0x300, offsets in the comments.
// Vtables 0x318f60, 0x318f84 at +0x1c, 0x318f9c at +0xf4, 0x3191ac at +0x204.
class SevenKeys : public StageMapObject
{
public:
    SevenKeys();                                                         // FS2 0x002b2a60
    ~SevenKeys() override;                                               // FS2 0x002b22f0

    // FS2 0x002b2380
    int  getKind() override { return StageObject_SevenKeys; }

    // The frame's steps: all empty but sideHook2, the base's.
    // FS2 0x002b23a0
    void sideHook0() override {}
    // FS2 0x002b2910
    void sideHook1() override {}
    // FS2 0x002b2390
    void sideHook3() override {}

    // The base's with the placement kept, then the faces turned a quarter
    // about z; the code cleared.
    void setMotionList(void *list, int arg, int arg2) override;          // FS2 0x002b2920

    // A dial's position, as the property 0x6b reads it (sevenKeysPoseValue).
    int  keyValue(int dial) const { return (char)keyPose()->getDial(dial); } // FS2 0x002b23b0
    // Both dials (a negative position keeps its dial), shown at once.
    void setKeys(int dial0, int dial1);                                  // FS2 0x002b2420
    // The code; a position out of 0..3 keeps its digit.
    void setCode(int code0, int code1);                                  // FS2 0x002b23e0
    // One dial a position down / up, round the four; dial 1 takes dial 0 along.
    void keyDown(int dial);                                              // FS2 0x002b24a0
    void keyUp(int dial);                                                // FS2 0x002b2560
    // No dial turns and both stand at the code.
    bool keysOpen();                                                     // FS2 0x002b2620
    // A dial is still turning to its position.
    bool keysTurning();                                                  // FS2 0x002b27c0

    // The model's pose (StageModel +0xc).
    SkyPose *keyPose() const { return static_cast<SkyPose *>(m_model->pose); }

    char m_code[kSevenKeysDials] = {};  // FS2 +0x240: the dials' positions that open it
    // The faces' place: the world matrix turned a quarter about z, and its
    // inverse (the first mesh's m_matrix and m_inverse).
    sceVu0FMATRIX m_faceMatrix = {};    // FS2 +0x250
    sceVu0FMATRIX m_faceInverse = {};   // FS2 +0x290
};

// The seven keys' body: the stick's up and down pick a dial, left and right
// turn it, Use or Squat leaves; "mon" sparkles on bn_top while in view.
// layout: host -- EE 0x30 in a block aligned 0x40 (vtable 0x319200).
class SevenKeysView : public GadgetView
{
public:
    SevenKeysView() : GadgetView(7) {}                                   // inline in the handle's ctor
    ~SevenKeysView() override;                                           // FS2 0x002b2b10: deleting

    void prepare() override;                                             // FS2 0x002b31f0
    // FS2 0x002b2cf0
    void draw() override {}

    int     m_dial = 0;                 // FS2 +0x28: 0..1, the one the stick turns
    StageParticle *m_sparkle = nullptr; // FS2 +0x2c: "mon", on bn_top while in view

protected:
    void frame() override;                                               // FS2 0x002b2f90
    void enterView() override;                                           // FS2 0x002b2dd0
    void leaveView() override;                                           // FS2 0x002b2d00
};

// +0x94, built by createMapObject with the first key.
// layout: host -- EE 8 (vtable 0x314ce0).
class SevenKeysGadget : public SceneOwnedObject
{
public:
    SevenKeysGadget();                                                   // FS2 0x002b2c80
};

#endif // GAME_SEVENKEYS_H
