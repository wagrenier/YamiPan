/* ========================================================================
 *  game/stagecar.h -- the car (0x289cc0..0x28fa90).
 *
 *  A map object the model oGK99_01 or oDC99_01 becomes: createMapObject
 *  builds one and the StageScene notes it (StageScene::car), so a stage has
 *  one at most. Its body is the model's CarPose, which rides four ground
 *  contacts; this class owns those contacts, the pedals, the 40 spheres it
 *  hits things with, what those hits break and the engine's sounds.
 *
 *  CarParts is what shows the damage: windows, dents and lights. A sub-scene
 *  has one too, on its copy of the car (SubSceneGateObject).
 *
 *  The file is the port's name: no source string says what the ROM called it.
 * ======================================================================== */
#ifndef GAME_STAGECAR_H
#define GAME_STAGECAR_H

#include "game/mapobject.h"
#include "game/vehiclepose.h"

class PointLight;
class SpotLight;
class StageParticle;

constexpr int kCarWindowsPerPart = 3;   // CarParts::windowShown
constexpr int kCarDamageFlags = 7;      // CarParts::damaged
constexpr int kCarEffects = 2;          // the head lamps' and the tail lamps' effects
constexpr int kCarDamageSlots = 10;     // StageCar::m_health
constexpr int kCarSounds = 2;           // StageCar::m_sounds
constexpr int kCarThrottleLog = 10;     // StageCar::m_throttleLog
constexpr int kCarSpheres = 0x28;       // the spheres of m_body
constexpr int kCarRunOverLevels = 5;    // CarRunOver: GroundSlide, 0..GroundSlide_Worst

// CarParts::damaged and the first slots of StageCar::m_health: what shows
// when it is gone. The names are the bones'.
enum CarDamage
{
    CarDamage_SideLeft   = 0,   // bn_dmg_sl
    CarDamage_SideRight  = 1,   // bn_dmg_sr
    CarDamage_FrontLeft  = 2,   // bn_dmg_fl_A in place of _B
    CarDamage_FrontRight = 3,   // bn_dmg_fr_A in place of _B
    CarDamage_FrontMid   = 4,   // bn_dmg_fc_A in place of _B
    CarDamage_Blood1     = 5,   // bn_bld_01
    CarDamage_Blood2     = 6,   // bn_bld_02: no flag sets it, the slot is the first window's
    CarDamage_Window0    = 6,   // windscreen; the slots of the windows are part + 6
    CarDamage_Window1    = 7,
    CarDamage_Window2    = 8,
};

// StageCar::m_mode: a script takes a piece off the model or puts it back.
// Level keeps off at HideWheel (PlayerMotion).
enum CarMode
{
    CarMode_Normal      = 0,
    CarMode_HideWheel   = 1,    // bn_rl_wheel
    CarMode_ShowWheel   = 2,    // and back to Normal
    CarMode_HideBattery = 3,    // bn_battery
    CarMode_ShowBattery = 4,    // and back to Normal
};

// StageCar::getProperty, setProperty.
enum CarProperty
{
    CarProp_OnGround   = 0x5e,
    CarProp_NoiseRange = 0x5f,
    CarProp_Speed      = 100,   // read only
};

// layout: host -- EE 0x40; the EE offsets in the comments.
struct CarParts
{
    // The windows: mapSwitches are the map parts', partIds and partCount their
    // part ids; the interior's BKW%02di are built here. from: as copyFrom.
    CarParts(StageObject *owner, PartSwitch *mapSwitches, int *partIds, int partCount,
             CarParts *from);                                            // FS2 0x0028f890
    ~CarParts();                                                         // FS2 0x0028f820
    // The two lights' priority (SceneRender::update).
    void setLightOrder(int order);                                       // FS2 0x0028ec70
    // The lights and their effects on or off.
    void setEffects(bool on);                                            // FS2 0x0028ecb0
    // Takes from's state; null: the closed, whole one.
    void copyFrom(const CarParts *from);                                 // FS2 0x0028ed60
    // The head lamps are on.
    bool isLit() const;                                                  // FS2 0x0028ec90
    // The windows of the side the view is on, the others hidden.
    void showWindows();                                                  // FS2 0x0028eb90
    // The model's parts for flag.
    void showDamage(int flag);                                           // FS2 0x0028e880

    StageModel *model;          // FS2 +0x00
    PartSwitch *mapSwitches;    // FS2 +0x04: the map parts', the outside's windows
    int        *ids;            // FS2 +0x08
    int         count;          // FS2 +0x0c
    PartSwitch *switches;       // FS2 +0x10: the inside's
    SpotLight  *spot;           // FS2 +0x14: CAR_sl
    PointLight *point;          // FS2 +0x18: CAR_pl
    SpotLight  *spot2;          // FS2 +0x1c: CAR_sl2
    PointLight *point2;         // FS2 +0x20: CAR_pl2
    StageParticle *effects[kCarEffects]; // FS2 +0x24: carlightL, carlightR
    int         windowShown[kCarWindowsPerPart]; // FS2 +0x2c: by part id, the switch's part
    u_char      viewInside;     // FS2 +0x38: the inside's windows show
    u_char      damaged[kCarDamageFlags]; // FS2 +0x39: CarDamage

private:
    // The lights and the effects on the bones, off.
    void buildLights(StageObject *owner);                                // FS2 0x0028ee70
};

// What a character hit by a car loses (0x2103d0, not written), by how hard
// it was hit. The worst stands for any other level.
// EE layout.
struct CarRunOver
{
    int *at(int level);                                                  // FS2 0x0028e850

    int damage[kCarRunOverLevels];      // FS2 +0x0: 0, 0, 20, 80, 1000 from the car's ctor
};
static_assert(sizeof(CarRunOver) == 0x14, "CarRunOver: EE 0x14");

// layout: host -- EE offsets in the comments. Vtables 0x318070, 0x318094 at
// +0x1c, 0x3180ac at +0xf4, 0x3182bc at +0x204.
class StageCar : public StageMapObject
{
public:
    StageCar();                                                          // FS2 0x0028e5a0
    ~StageCar() override;                                                // FS2 0x0028e4b0

    // FS2 0x00289cc0
    int  getKind() override { return StageObject_Car; }
    int  getProperty(int id, int sub) override;                          // FS2 0x00289d40
    void setProperty(int id, int sub, int value) override;               // FS2 0x00289ce0

    // The frame's four steps. The first is empty.
    void sideHook0() override;                                           // FS2 0x00289cd0
    void sideHook1() override;                                           // FS2 0x0028d290
    void sideHook2() override;                                           // FS2 0x0028cda0
    void sideHook3() override;                                           // FS2 0x0028c930

    int  collisionHook0(CollisionContact *contact, const float *normal, StageCollisionFace *face,
                        float depth) override;                           // FS2 0x0028b470
    void syncBody(CollisionBody *body) override;                         // FS2 0x0028c440
    void collisionHook3(CollisionBody *body) override;                   // FS2 0x0028c310

    void setMotionList(void *list, int arg, int arg2) override;          // FS2 0x0028e000

    // On the ground plane, nearer than range to pos.
    bool isNear(float range, const float *pos) const;                    // FS2 0x00289eb0
    // Length of what the body moves each frame.
    float getSpeed();                                                    // FS2 0x00289dd0
    const float *getVelocity();                                          // FS2 0x00289e10
    // The frame step's change, per second squared.
    float getAcceleration();                                             // FS2 0x00289e20
    const float *getFrameStep();                                         // FS2 0x00289ea0
    // The middle of the car, 80 behind its origin.
    void getCenter(float *out);                                          // FS2 0x00289ef0
    // One of the places beside it, by the side and the index (0 the door,
    // 1 behind, 2 ahead, 3 the other seat, else the seat); answers 40.
    float getDoorPoint(sceVu0FVECTOR out, int side, int index);          // FS2 0x00289f70
    // Where who gets in: the nearer seat of the two, or the car's origin
    // from afar. With a driver waiting it takes the way that is clear.
    void getSeatPlace(float *out, StageCharacter *who);                  // FS2 0x0028a250
    // The door's place on the ground; answers 40.
    float getDoorPlace(sceVu0FVECTOR out, int side);                     // FS2 0x0028a890
    // The seat on the ground; answers 40.
    float getSeatPoint(sceVu0FVECTOR out, int side);                     // FS2 0x0028a9a0
    // Where the sill is; answers 64.
    float getSillPoint(sceVu0FVECTOR out, int side);                     // FS2 0x0028aa60
    // The door's bone; answers 120.
    float getDoorBone(float *out, int side);                             // FS2 0x0028ab00
    // The steering wheel has stood for half a second.
    bool hasSteerRested();                                               // FS2 0x0028ab70
    // Under 0.1 for 0.3 s: one may climb on it, and a fall onto it is a fall.
    bool hasSettled();                                                   // FS2 0x0028ab90
    // Under 5 km/h for 0.3 s.
    bool isCrawling();                                                   // FS2 0x0028abb0
    // Under 4 km/h for 0.3 s.
    bool isCreeping();                                                   // FS2 0x0028abd0
    // How hard it hits something at position: GroundSlide.
    int  getHitLevel(const float *position);                             // FS2 0x0028abf0

    // +0x382, 1 or -1; any other value is ignored.
    void setDirection(int direction);                                    // FS2 0x0028d810
    void honk();                                                         // FS2 0x0028d780
    void setLights(bool on);                                             // FS2 0x0028db80
    // Starts from where it stands, by what moved.
    void resetPlace();                                                   // FS2 0x0028dac0
    // Level keeps off at 1; native 0x1c8 sets it.
    int  getMode() { return m_mode; }

    CarParts *m_lights = nullptr;   // FS2 +0x2b0
    StageCharacter *m_driver = nullptr; // FS2 +0x3c0
    u_char m_throttle = 0;          // FS2 +0x381: 0..255
    u_char m_brake = 0;             // FS2 +0x383: 0..255
    float  m_steer = 0.0f;          // FS2 +0x384: -1..1
    float  m_steerRate;             // FS2 +0x394: radians at full lock (RouteMoveCar, native 0x19e)
    int    m_mode = CarMode_Normal; // FS2 +0x39c: CarMode
    u_char m_onGround = 1;          // FS2 +0x3a0: native 0x1ac; off, the car lets go of the ground
    u_char m_honked = 0;            // FS2 +0x3b9: last frame's horn, native 0x154

private:
    // Inline in the ROM.
    CarPose *pose() { return static_cast<CarPose *>(m_model->pose); }
    // The world matrix of bone, which must exist.
    const sceVu0FVECTOR *boneMatrix(const char *bone);
    // getSpeed by the clock, in km/h.
    float speedKmh();
    // The rumble name is not playing.
    bool rumbleIdle(const char *name);
    // Takes amount off slot next frame: the worst of the frame's hits.
    void hurt(int slot, int amount);
    // The hits are over.
    void clearHits();
    // This frame is the last one to go back to.
    void rememberPlace();
    void turnMatrix(sceVu0FMATRIX out, const sceVu0FVECTOR angles);

    // The four ground contacts and the body to the floor under them.
    void snapToGround();                                                 // FS2 0x0028ad50
    // A sphere of body went in: with more of that end in, it turns along face.
    void scrape(CollisionBody *body, int sphere, StageCollisionFace *face); // FS2 0x0028af70
    // A window goes. withEffect: its glass flies.
    void breakWindow(int part, bool withEffect);                         // FS2 0x0028c110
    // The pedals back to nothing, then from where it stands.
    void startOver(bool moved, bool turned);                             // FS2 0x0028d850
    void updateSounds();                                                 // FS2 0x0028dc00

    CarRunOver m_runOver = {{0, 0, 20, 80, 1000}}; // FS2 +0x240
    u_char m_hurt = 0;              // FS2 +0x254: a hit took something this frame
    int    m_damage[kCarDamageSlots] = {}; // FS2 +0x258: the frame's
    int    m_health[kCarDamageSlots];      // FS2 +0x280: 100 from the ctor
    u_char m_hitSide = 0;           // FS2 +0x2a8: a wall along it
    u_char m_hitFront = 0;          // FS2 +0x2a9: a corner of the first pair head on
    u_char m_hitBack = 0;           // FS2 +0x2aa: of the second pair
    u_char m_ranMedium = 0;         // FS2 +0x2ab: over a character
    u_char m_ranLarge = 0;          // FS2 +0x2ac: 5, 6, 0xa, 0xc: nearly stops it
    u_char m_ranSmall = 0;          // FS2 +0x2ad: 7
    u_char m_blocked = 0;           // FS2 +0x2ae: last frame's m_hitFront or m_hitBack
    StageParticle *m_tailLights[kCarEffects] = {}; // FS2 +0x2b4: tailLi, tailRi
    StageNodeBase *m_colBone = nullptr; // FS2 +0x2bc: bn_col, what the spheres stand on
    CollisionBody *m_body = nullptr;    // FS2 +0x2c0: the spheres, on with a driver
    sceVu0FVECTOR  m_contacts[kCarWheels];     // FS2 +0x2d0: CarPose::getWheelPoints's
    sceVu0FVECTOR  m_lastContacts[kCarWheels]; // FS2 +0x310
    sceVu0FVECTOR  m_lastRotation;  // FS2 +0x350
    sceVu0FVECTOR  m_lastPosition;  // FS2 +0x360
    sceVu0FVECTOR  m_push = {};     // FS2 +0x370: what contacts moved it by, this frame
    u_char m_placed = 0;            // FS2 +0x380: just put somewhere
    signed char m_direction = 1;    // FS2 +0x382: 1 ahead, -1 back
    float  m_topSpeed;              // FS2 +0x388: 20
    float  m_topReverse;            // FS2 +0x38c: 6
    float  m_maxBrake;              // FS2 +0x390: 2
    float  m_noiseRange;            // FS2 +0x398: 2400
    float  m_creepTime = 0.0f;      // FS2 +0x3a4: isCreeping
    float  m_crawlTime = 0.0f;      // FS2 +0x3a8: isCrawling
    float  m_settledTime = 0.0f;    // FS2 +0x3ac: hasSettled
    float  m_steerRestTime = 0.0f;  // FS2 +0x3b0: hasSteerRested
    float  m_fastTime = 0.0f;       // FS2 +0x3b4: above 25 km/h
    u_char m_horn = 0;              // FS2 +0x3b8: this frame
    StageObject *m_lastTouchedBy = nullptr; // FS2 +0x3bc
    sceVu0FVECTOR  m_frameStep = {};  // FS2 +0x3d0: moved in the last frame
    sceVu0FVECTOR  m_stepChange = {}; // FS2 +0x3e0
    StageSoundSource *m_sounds[kCarSounds] = {}; // FS2 +0x3f0: the engine, the tyres
    u_char m_throttleLog[kCarThrottleLog] = {}; // FS2 +0x3f8
    u_char m_brakeState = 0;        // FS2 +0x402: 1 the screech is due, 2 it played
    u_char m_pedalStamped = 0;      // FS2 +0x405: the accel note is due
};

#endif // GAME_STAGECAR_H
