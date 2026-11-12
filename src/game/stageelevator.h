/* ========================================================================
 *  game/stageelevator.h -- the elevator (0x2939b0..0x295470).
 *
 *  A map object the model oZS99_01 becomes: createMapObject builds one, and
 *  StageScene::getZs9901 finds it for whoever asks. Its pose, ElevatorPose
 *  (game/vehiclepose.h), holds the queue, the motions and the state; this
 *  class holds the buttons, the five collision meshes that ride with the car
 *  and its doors, the sounds and the pad's rumble, and tells a character
 *  where to stand.
 *
 *  It goes between two landings, B1 and B3. The car's mesh is bn_bx; each
 *  landing has a left and a right door with a mesh of their own.
 *
 *  The file is the port's name: no source string says what the ROM called it.
 * ======================================================================== */
#ifndef GAME_STAGEELEVATOR_H
#define GAME_STAGEELEVATOR_H

#include "game/mapobject.h"
#include "game/vehiclepose.h"

constexpr int kElevatorMeshes = 5;      // the car and the four doors

// The two ends of the shaft, as the route counts them (RouteElevator::m_floor).
enum ElevatorLanding
{
    ElevatorLanding_B1 = 0,
    ElevatorLanding_B3 = 1,
    kElevatorLandings  = 2,
};

// What a character presses: MotionGadgetSpecial's arg, the switch of
// CharacterRoute::useElevator, and 0x1cd's release. A button's bit in
// getCalledButtons is 1 << the button.
enum ElevatorButton
{
    ElevatorButton_CallB1   = 1,    // the landing's, "ElevatorB1"
    ElevatorButton_CallB3   = 2,    // "ElevatorB3"
    ElevatorButton_InsideB1 = 3,    // the car's, to B1, "ElevatorBxB1"
    ElevatorButton_InsideB3 = 4,    // "ElevatorBxB3"
    ElevatorButton_Release  = 5,    // no button: every lamp out, what is queued goes
};

// layout: host -- EE 0x300, offsets in the comments. Vtables 0x318310, 0x318334
// at +0x1c, 0x31834c at +0xf4, 0x31855c at +0x204.
class StageElevator : public StageMapObject
{
public:
    // collision: the file its meshes are in.
    explicit StageElevator(void *collision);                             // FS2 0x00295360
    ~StageElevator() override;                                           // FS2 0x002952b0

    // FS2 0x002939b0
    int  getKind() override { return StageObject_Elevator; }

    // The frame's steps. The first two are empty, unlike the base's first.
    // FS2 0x002939d0
    void sideHook0() override {}
    // FS2 0x002939c0
    void sideHook1() override {}
    void sideHook3() override;                                           // FS2 0x00294560

    int  collisionHook0(CollisionContact *contact, const float *normal, StageCollisionFace *face,
                        float depth) override;                           // FS2 0x00294cb0
    void setMotionList(void *list, int arg, int arg2) override;          // FS2 0x00295010

    // Who stands at position, turned rotation, can use the button; release:
    // the doors stand open.
    bool canCall(const float *position, const float *rotation, int button); // FS2 0x00294140
    void press(int button);                                              // FS2 0x00294280
    // At once, as a script asks: the car at a landing, the doors open or not.
    void placeAt(bool atB3, bool open);                                  // FS2 0x00294f90

    // The pose's step, ElevatorStep; false with no model.
    bool isOpen();                                                       // FS2 0x00294b60
    bool isOpening();                                                    // FS2 0x00294b00
    bool isClosed();                                                     // FS2 0x00294b30
    bool isClosing();                                                    // FS2 0x00294ad0
    // Doors open at that landing.
    bool isOpenAtB1();                                                   // FS2 0x00294be0
    bool isOpenAtB3();                                                   // FS2 0x00294b90
    // There: the side flag turns when a ride begins, so the ride that goes to
    // that landing does not count.
    bool isAtB1();                                                       // FS2 0x00294c70
    bool isAtB3();                                                       // FS2 0x00294c30

    // Where a landing's doors are, on the ground.
    void getDoorPlace(float *out, int landing);                          // FS2 0x00293fd0
    // Where to stand for a button, and the way to face.
    void getSwitchPlace(float *place, float *direction, int button);     // FS2 0x00293c60

    // The buttons lit, a bit each. Inline in the ROM.
    u_int getCalledButtons() const { return m_called; }

private:
    ElevatorPose *pose() const { return static_cast<ElevatorPose *>(m_model->pose); }
    const sceVu0FVECTOR *boneMatrix(const char *bone);
    // The panel's matrix, and the place under it: its x and y, the door's z.
    const sceVu0FVECTOR *panelPlace(sceVu0FVECTOR place, const char *panel, const char *door);
    // The panel of a button and the way it faces.
    void getCallPlace(float *place, float *facing, int button);          // FS2 0x002939e0
    bool isStep(u_int step);
    // A ride began or ended: the sound, the lamps, the rumble of who rides.
    void startRide();
    void endRide(u_int state);
    void findRider();

    void *m_collision;              // FS2 +0x240: the collision file
    // Each kept once worked out; w 1 until then. By ElevatorLanding.
    sceVu0FVECTOR m_callPlace[kElevatorLandings];       // FS2 +0x250
    sceVu0FVECTOR m_callFacing[kElevatorLandings];      // FS2 +0x270
    sceVu0FVECTOR m_switchPlace[kElevatorLandings];     // FS2 +0x290
    sceVu0FVECTOR m_switchDirection[kElevatorLandings]; // FS2 +0x2b0
    sceVu0FVECTOR m_doorPlace[kElevatorLandings];       // FS2 +0x2d0
    StageSoundSource *m_rideSound = nullptr;  // FS2 +0x2f0: ELV_MOVE
    u_int m_lastState = 0;                    // FS2 +0x2f4: the pose's, a frame ago
    StageObject *m_rider = nullptr;           // FS2 +0x2f8: who feels the ride
    u_int m_called = 0;                       // FS2 +0x2fc: 1 << ElevatorButton
};

#endif // GAME_STAGEELEVATOR_H
