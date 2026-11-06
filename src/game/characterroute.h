/* ========================================================================
 *  game/characterroute.h -- how a brain's commands reach the character.
 *
 *  EE 0x50, vtable 0x316498. It holds three running tasks: the action
 *  (+0xc), the move (+0x10) and the look (+0x14). Each frame update steps
 *  them; they say what the character wants by writing StageCharacter::m_wants,
 *  as the pad does (game/characterinput.h).
 *
 *  The tasks' code runs 0x21efa0..0x22a310, the route's 0x22a310..0x22f270,
 *  the deleting destructors and trivial slots ..0x22fc50, all in vtable order
 *  0x3160a0..0x316498: one file. The classes' and the actions' names are the
 *  port's (open-items.md).
 *
 *  Not written: the actions marked below, and RoutePlayer's tables behind
 *  RouteFilter (game/routefilter.h): until they exist findPath answers -1
 *  and a walk ends at once.
 * ======================================================================== */
#ifndef _GAME_CHARACTERROUTE_H
#define _GAME_CHARACTERROUTE_H

#include "sdk/libvu0.h"
#include "sdk/scetypes.h"

class CharacterBrain;
class CharacterRoute;
class StageCar;
class StageCharacter;
class StageDoor;
class StageObject;
struct RouteFilter;             // game/routefilter.h
struct RouteNode;               // game/routefilter.h

// moveToObject / moveToPoint's flags, the letters of BrainWait::moveTo.
enum RouteMoveFlag
{
    RouteMoveFlag_Straight   = 0x1,    // l, and walking backwards: no path, straight at the goal
    RouteMoveFlag_Exact      = 0x2,    // a: reach may be under 20, and the last steps slide
    RouteMoveFlag_ShutDoors  = 0x4,    // d: a door it went through is shut behind it
    RouteMoveFlag_LockDoors  = 0x8,    // k (with d): and locked
    RouteMoveFlag_StopAtDoor = 0x10,   // the walk ends before a door not open, and no elevator
    RouteMoveFlag_DropLimit  = 0x20,   // 1: down a ledge only as deep as its step level
    RouteMoveFlag_NoDrop     = 0x40,   // 2: not down a ledge
    RouteMoveFlag_NoClimb    = 0x80,   // 3: not up one
    RouteMoveFlag_NoLadder   = 0x100,  // 4
    RouteMoveFlag_HitInWay   = 0x200,  // 5: a character in the way gets a blow first
    RouteMoveFlag_Roll       = 0x400,  // 6: rolls on long straights
    RouteMoveFlag_KeepPath   = 0x800,  // the old goal stands when the new one has no path
    RouteMoveFlag_IgnoreDoor = 0x1000, // 7: does not open doors
};

// One thing the route is doing, in its action slot. vtable 0x3160b0.
// layout: host -- EE: +0 vptr, +4 owner.
class RouteAction
{
public:
    explicit RouteAction(StageCharacter *owner) : m_owner(owner) {}
    virtual ~RouteAction() {}                                            // FS2 0x00224fe0
    // A new action of a level above this takes its place; below 1 any does.
    virtual int priority() = 0;
    // false: done.
    virtual bool update() = 0;

protected:
    StageCharacter *m_owner;    // FS2 +0x04
};

// The move slot. vtable 0x3160d0.
// layout: host -- EE: +0 vptr, +4 owner, +8 route.
class RouteMove
{
public:
    RouteMove(StageCharacter *owner, CharacterRoute *route) : m_owner(owner), m_route(route) {}
    virtual ~RouteMove() {}                                              // FS2 0x00221f20
    virtual bool update() = 0;
    // Where it goes; false: it does not say.
    virtual bool getGoal(float *out) = 0;
    virtual StageObject *getTarget() = 0;
    // The filter's paths changed.
    virtual void pathsChanged() = 0;

protected:
    StageCharacter *m_owner;    // FS2 +0x04
    CharacterRoute *m_route;    // FS2 +0x08
};

// A part of a move: a turn, a detour. vtable 0x3160f0.
// layout: host -- EE: +0 vptr, +4 owner.
class RouteStep
{
public:
    explicit RouteStep(StageCharacter *owner) : m_owner(owner) {}
    virtual ~RouteStep() {}                                              // FS2 0x0021f860
    virtual bool update() = 0;

protected:
    StageCharacter *m_owner;    // FS2 +0x04
};

// The look: where the head turns, relative to the body. vtable 0x316110.
// layout: host -- EE 0x20; the ROM deletes it without a destructor.
class RouteLook
{
public:
    RouteLook(StageCharacter *owner, float yaw, float pitch)
        : m_owner(owner), m_pitch(pitch), m_yaw(yaw) {}
    virtual ~RouteLook() {}
    virtual bool update();                                               // FS2 0x0021f410

protected:
    StageCharacter *m_owner;    // FS2 +0x04
    float m_pitch = 0.0f;       // FS2 +0x10
    float m_yaw = 0.0f;         // FS2 +0x18
};

// layout: host -- EE 0x30, vtable 0x316100.
class RouteLookPoint : public RouteLook
{
public:
    RouteLookPoint(StageCharacter *owner, const float *point);
    bool update() override;                                              // FS2 0x0021f340

protected:
    sceVu0FVECTOR m_point;      // FS2 +0x20
};

// layout: host -- EE 0x40, vtable 0x3160a0.
class RouteLookObject : public RouteLookPoint
{
public:
    // rate > 0: the look holds on what it last saw for that many seconds,
    // and the head goes straight once it is gone.
    RouteLookObject(float rate, StageCharacter *owner, StageObject *object); // FS2 0x0021f1a0
    bool update() override;                                              // FS2 0x0021efa0

private:
    void aim();

    StageObject *m_object;      // FS2 +0x30
    float m_rate;               // FS2 +0x34
    float m_seenAt;             // FS2 +0x38
};

// layout: host -- EE 0x50, vtable 0x316498.
class CharacterRoute
{
public:
    explicit CharacterRoute(CharacterBrain *brain);                      // FS2 0x0022f270
    virtual ~CharacterRoute();                                           // FS2 0x0022f1b0

    // The brain's frame.
    void update();                                                       // FS2 0x0022e740
    // Ends the action, the move and the look.
    bool stop();                                                         // FS2 0x0022d810
    // Neither an action nor a move runs.
    bool isIdle() const { return action == nullptr && move == nullptr; }

    // The filter's paths; the move finds its way again.
    void allowPath(const char *name, bool on);                           // FS2 0x0022a370

    // The look: it stays until stopLook, and a script switch starts it again.
    // force: a look past 65 degrees turns the body, with no move running.
    bool lookAtObject(float rate, StageObject *object, int force);       // FS2 0x0022d900
    bool lookAtPoint(const float *point, int force);                     // FS2 0x0022da50
    bool lookAtYaw(float yaw, float pitch, int force);                   // FS2 0x0022db80
    bool stopLook();                                                     // FS2 0x0022dc60

    // A turn: in the move slot.
    bool turnToObject(float rate, StageObject *object);                  // FS2 0x0022dca0
    bool turnToPoint(float rate, const float *point);                    // FS2 0x0022de30
    bool turnToYaw(float yaw, float rate);                               // FS2 0x0022dfc0

    // A move; mode 1 walks, 2 runs, 3 backs up; 0 keeps the last.
    bool moveToObject(float reach, StageObject *object, int mode, u_int flags); // FS2 0x0022e140
    bool moveToPoint(float reach, const float *point, int mode, u_int flags);   // FS2 0x0022e4a0
    // The move's goal; false without a move.
    bool getMoveGoal(float *out);                                        // FS2 0x0022e110
    bool cancelMove();                                                   // FS2 0x0022e6f0

    // Actions.
    bool playAction(const char *name, StageObject *a, StageObject *b);   // FS2 0x0022cac0: not written
    bool attack(float range, StageObject *target, bool mode);            // FS2 0x0022d350: not written
    bool approach(float distance, const float *point);                   // FS2 0x0022d6d0: not written
    bool useDoor(bool open);                                             // FS2 0x0022a710
    // false locks, true unlocks or forces it.
    bool useDoorKey(bool unlock);                                        // FS2 0x0022a550
    // Rides or calls: 1, 2 the switches outside at B1, B3, 3, 4 inside.
    bool useElevator(int which);                                         // FS2 0x0022a3c0
    bool dragOutCar();                                                   // FS2 0x0022a4f0
    bool takeUpTrap(int arg);                                            // FS2 0x0022a8c0
    bool setTrap(StageObject *trap);                                     // FS2 0x0022a8f0
    // Loads rounds, or the magazine of any gun but G_NN38, R_MURATA, G_26NEN.
    bool gunAction(int rounds = 0);                                      // FS2 0x0022a920
    bool takeItem(StageObject *item);                                    // FS2 0x0022ab50
    bool action22ab90(void *a, void *b);                                 // FS2 0x0022ab90
    // Up from a crouch, and down into one.
    bool action22abf0();                                                 // FS2 0x0022abf0
    bool action22ad60();                                                 // FS2 0x0022ad60
    bool hit(StageObject *target);                                       // FS2 0x0022be10: a jump to shoot
    bool shoot(StageObject *target);                                     // FS2 0x0022aee0
    bool melee(StageCharacter *target, bool endMove);                    // FS2 0x0022b580
    bool hitInTime(float seconds, StageObject *target);                  // FS2 0x0022ba80
    bool hitRunning(float seconds, StageObject *target);                 // FS2 0x0022bc50
    bool swingAt(StageObject *target);                                   // FS2 0x0022be20
    bool attackTask(StageObject *target);                                // FS2 0x0022b970
    bool action22bf60();                                                 // FS2 0x0022bf60
    bool action22c0f0(int arg);                                          // FS2 0x0022c0f0: not written
    bool action22c840(const char *name);                                 // FS2 0x0022c840: not written
    bool action22c960(int arg);                                          // FS2 0x0022c960: not written
    bool action22cbe0();                                                 // FS2 0x0022cbe0: not written
    bool actOnCharacter(StageObject *character);                         // FS2 0x0022ccf0: not written
    bool actLeftRight();                                                 // FS2 0x0022cde0: not written
    bool actLeft();                                                      // FS2 0x0022cf90: not written
    bool actOnFoeRight(StageObject *foe);                                // FS2 0x0022d0f0: not written
    bool actOnFoe(StageObject *foe);                                     // FS2 0x0022d220: not written

    // What the tasks share, inline at every use in the ROM.
    // A turn this frame toward delta radians, no faster than the character turns.
    void steer(float delta);
    // The gun stays up for seconds from now; 0 lets it down.
    void holdReady(float seconds);
    void setMove(RouteMove *task);
    void setAction(RouteAction *task);
    void setLook(RouteLook *task);
    // The action can give way to one of this level.
    bool actionYields(int level);

    StageCharacter *owner = nullptr;    // FS2 +0x04
    RouteFilter    *filter = nullptr;   // FS2 +0x08
    RouteAction    *action = nullptr;   // FS2 +0x0c
    RouteMove      *move = nullptr;     // FS2 +0x10
    RouteLook      *look = nullptr;     // FS2 +0x14
    float           trapSeen = 0.0f;    // FS2 +0x18: 1 from a detour round a trap; no reader found
    u_char          skipUpdate = 0;     // FS2 +0x1c: a request went out: the next update does nothing
    float           readyUntil = 0.0f;  // FS2 +0x20: wants.ready holds until then
    float           shotDelay = 0.0f;   // FS2 +0x24: shoot does nothing until then
    float           lastBlow = 0.0f;    // FS2 +0x28: when melee last fired
    float           lastShot = 0.0f;    // FS2 +0x2c
    int             moveMode = 2;       // FS2 +0x30: the last moveTo's mode
    // FS2 +0x40: where a walk stopped short; w 666 until one has.
    sceVu0FVECTOR   stoppedAt = {0.0f, 0.0f, 0.0f, 666.0f};
};

#endif
