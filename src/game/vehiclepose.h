/* ========================================================================
 *  game/vehiclepose.h -- the two poses that carry something: the car's and
 *  the elevator's (0x27dc40..0x280010).
 *
 *  Both are ModelPose with ModelPose::update's shape, made by
 *  StageModel::setBaseMotion for a model of kind ModelPose_Car or
 *  ModelPose_Elevator. The car lays its body and wheels on four ground
 *  contacts each frame; the elevator runs a small queue of door and ride
 *  steps, played as motion files of the model oZS99_01.
 *
 *  The car object is game/stagecar.cpp, the elevator object
 *  game/stageelevator.cpp.
 *
 *  The file is the port's name: no source string says what the ROM called it.
 * ======================================================================== */
#ifndef _GAME_VEHICLEPOSE_H
#define _GAME_VEHICLEPOSE_H

#include "game/modelpose.h"

constexpr int kCarWheels = 4;

// The wheels' order everywhere: front left, front right, rear left, rear right.
enum CarWheel
{
    CarWheel_FrontLeft  = 0,
    CarWheel_FrontRight = 1,
    CarWheel_RearLeft   = 2,
    CarWheel_RearRight  = 3,
};

// layout: host -- EE: vptr at +0, subclass of ModelPose (EE 0x3c0), EE 0x4e0;
// offsets in the comments.
class CarPose : public ModelPose        // vtable 0x317c50
{
public:
    CarPose();                                                           // FS2 0x0027ef70
    ~CarPose() override;                                                 // FS2 0x0027ef10: slot 0, deleting
    int load(void *file, int arg, const char *boneNames, u_int boneCount) override; // FS2 0x0027ee00: slot 1
    void update(float frames, bool loop) override;                       // FS2 0x0027dc40: slot 2

    // One step of the engine: speed, brake and steer ease to their targets,
    // the body takes its drift and tilt. rate is the step in 30ths of a
    // second. Any contact null: the car is off the ground and everything clears.
    void drive(float rate, const sceVu0FVECTOR frontLeft, const sceVu0FVECTOR frontRight,
               const sceVu0FVECTOR rearLeft, const sceVu0FVECTOR rearRight); // FS2 0x0027e650
    // Where the four contacts should be: the fixed corners through bodyMatrix.
    void getWheelPoints(sceVu0FVECTOR frontLeft, sceVu0FVECTOR frontRight,
                        sceVu0FVECTOR rearLeft, sceVu0FVECTOR rearRight) const; // FS2 0x0027ec80
    // The targets drive eases to; speed and brake are limited to 10, steer
    // to a quarter turn.
    void setDrive(float speed, float brake, float steer);                // FS2 0x0027ed70

    sceVu0FVECTOR  contact[kCarWheels] = {}; // FS2 +0x3c0: w 1 while on the ground
    sceVu0FVECTOR  lastVelocity = {};  // FS2 +0x400: forward * speed, the last step's
    sceVu0FVECTOR  drift = {};         // FS2 +0x410: added to the position each frame
    sceVu0FVECTOR  tilt = {};          // FS2 +0x420: the velocity's change, leans the body
    float          wheelAngle[kCarWheels] = {}; // FS2 +0x430: spin, -pi..pi
    float          _440[kCarWheels] = {};       // FS2 +0x440: only cleared
    float          speed = 0.0f;       // FS2 +0x450
    float          speedTarget = 0.0f; // FS2 +0x454
    float          brake = 0.0f;       // FS2 +0x458
    float          brakeTarget = 0.0f; // FS2 +0x45c
    float          steer = 0.0f;       // FS2 +0x460
    float          steerTarget = 0.0f; // FS2 +0x464
    sceVu0FMATRIX  bodyMatrix;         // FS2 +0x470: bn_body's, after the update
    ModelPoseBone *baseBone = nullptr; // FS2 +0x4b0: the second bone
    ModelPoseBone *bodyBone = nullptr; // FS2 +0x4b4: bn_body
    ModelPoseBone *accelBones[kCarWheels] = {}; // FS2 +0x4b8: bn_fl_accel..
    ModelPoseBone *wheelBones[kCarWheels] = {}; // FS2 +0x4c8: bn_fl_wheel..
    ModelPoseBone *steeringBone = nullptr;      // FS2 +0x4d8: bn_str_w, may be missing

private:
    void followGround();                                                 // FS2 0x0027de20
};

// A step of the elevator: the low nibble of ElevatorPose::state and an entry
// of its queue.
enum ElevatorStep
{
    ElevatorStep_End      = 0,  // of the queue
    ElevatorStep_Open     = 1,  // standing with the doors open
    ElevatorStep_Opening  = 2,
    ElevatorStep_Closed   = 3,
    ElevatorStep_Closing  = 4,
    ElevatorStep_RideToB3 = 5,
    ElevatorStep_RideToB1 = 6,
};
constexpr u_int kElevatorStepMask = 0x0f;
constexpr u_int kElevatorSideMask = 0xf0;
constexpr u_int kElevatorAtB1     = 0x10;   // ElevatorPose::state: the car stands at B1

// ElevatorPose::motions: the table 0x2fb630 of the model oZS99_01. 'b' lamps
// are inside the car, 'f' lamps on the landings.
enum ElevatorMotion
{
    ElevatorMotion_OpenB1    = 0,   // oZS99_01_op_b1
    ElevatorMotion_OpenB3    = 1,
    ElevatorMotion_CloseB1   = 2,   // _cl_b1
    ElevatorMotion_CloseB3   = 3,
    ElevatorMotion_RideB1    = 4,   // _mt_b1
    ElevatorMotion_RideB3    = 5,
    ElevatorMotion_StandOpenB1  = 6,  // _so_b1
    ElevatorMotion_StandOpenB3  = 7,
    ElevatorMotion_StandClosedB1 = 8, // _sc_b1
    ElevatorMotion_StandClosedB3 = 9,
    ElevatorMotion_LampFloorB1On  = 10, // _fb1_on
    ElevatorMotion_LampFloorB1Off = 11,
    ElevatorMotion_LampFloorB3On  = 12,
    ElevatorMotion_LampFloorB3Off = 13,
    ElevatorMotion_LampInsideB1On  = 14, // _bb1_on
    ElevatorMotion_LampInsideB1Off = 15,
    ElevatorMotion_LampInsideB3On  = 16,
    ElevatorMotion_LampInsideB3Off = 17,
    kElevatorMotions = 18,
};

// What setScene puts the elevator in: the ROM's script values.
enum ElevatorScene
{
    ElevatorScene_OpenB3    = 2,
    ElevatorScene_ClosedB3  = 4,
    ElevatorScene_CallB1    = 5,
    ElevatorScene_CallB3    = 6,
    ElevatorScene_OpenB1    = 8,
    ElevatorScene_ClosedB1  = 10,
};

constexpr int kElevatorQueue = 16;

// layout: host -- EE: vptr at +0, subclass of ModelPose (EE 0x3c0), EE 0x460;
// offsets in the comments.
class ElevatorPose : public ModelPose   // vtable 0x317c70
{
public:
    ElevatorPose();                                                      // FS2 0x0027ffb0
    ~ElevatorPose() override;                                            // FS2 0x0027ff50: slot 0, deleting
    int load(void *file, int arg, const char *boneNames, u_int boneCount) override; // FS2 0x0027ff40: slot 1
    void update(float frames, bool loop) override;                       // FS2 0x0027f040: slot 2

    // The doors hit something while closing: they open again, then close.
    void reopenDoors();                                                  // FS2 0x0027f5a0
    // The buttons. Each lights its lamp and queues what the car must do from
    // where it is; the answer is the queue's length.
    int release();                                                       // FS2 0x0027f7d0
    int callInsideB3();                                                  // FS2 0x0027f880
    int callInsideB1();                                                  // FS2 0x0027f9a0
    int callFloorB3();                                                   // FS2 0x0027fac0
    int callFloorB1();                                                   // FS2 0x0027fc40
    // At once, as a script or the object's setup asks: an ElevatorScene.
    int setScene(int scene);                                             // FS2 0x0027fdc0
    void setMotionFile(void *file, u_int motion);                        // FS2 0x0027ff10

    void *motions[kElevatorMotions] = {}; // FS2 +0x3c0: ElevatorMotion
    u_int state = 0;                  // FS2 +0x408: ElevatorStep, kElevatorAtB1
    float timer = 600.0f;             // FS2 +0x40c: frames since the doors began to open
    int   queue[kElevatorQueue] = {}; // FS2 +0x410: ElevatorStep; the ROM's ctor clears two
    int   queueStep = 0;              // FS2 +0x450

private:
    void stepQueue();                                                    // FS2 0x0027f2b0
};

#endif /* _GAME_VEHICLEPOSE_H */
