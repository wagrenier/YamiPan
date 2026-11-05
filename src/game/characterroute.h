/* ========================================================================
 *  game/characterroute.h -- how a brain's commands reach the character.
 *
 *  EE 0x50, vtable 0x316498, code 0x22a310..0x22f270 and its task classes
 *  (0x224ba0.. and 0x22f320..). It holds three running tasks: the action
 *  (+0xc), the move (+0x10) and the look (+0x14). Not written yet: only the
 *  brain's calls are declared, and they stay stubs until the route's TU, so
 *  nobody walks by themselves. Names are read from the callers, the file's
 *  and the action names are guesses (open-items.md).
 * ======================================================================== */
#ifndef _GAME_CHARACTERROUTE_H
#define _GAME_CHARACTERROUTE_H

#include "sdk/scetypes.h"

class CharacterBrain;
class StageCharacter;
class StageObject;
struct RouteFilter;             // game/gamehooks.h
struct RouteTask;

// layout: host -- EE 0x50.
class CharacterRoute
{
public:
    explicit CharacterRoute(CharacterBrain *brain);                      // FS2 0x0022f270
    ~CharacterRoute();                                                   // FS2 0x0022f1b0

    // The brain's frame.
    void update();                                                       // FS2 0x0022e740
    // Ends the action, the move and the look.
    bool stop();                                                         // FS2 0x0022d810
    // Neither an action nor a move runs.
    bool isIdle() const { return action == nullptr && move == nullptr; }

    // The filter's paths; the move finds its way again.
    void allowPath(const char *name, bool on);                           // FS2 0x0022a370

    // The look: it stays until stopLook, and a script switch starts it again.
    bool lookAtObject(float rate, StageObject *object, int force);       // FS2 0x0022d900
    bool lookAtPoint(const float *point, int force);                     // FS2 0x0022da50
    bool lookAtYaw(float yaw, float rate, int force);                    // FS2 0x0022db80
    bool stopLook();                                                     // FS2 0x0022dc60

    // A turn: the action.
    bool turnToObject(float rate, StageObject *object);                  // FS2 0x0022dca0
    bool turnToPoint(float rate, const float *point);                    // FS2 0x0022de30
    bool turnToYaw(float yaw, float rate);                               // FS2 0x0022dfc0

    // A move; mode 1 walks, 2 runs, 3 by BrainWait::moveTo's flags.
    bool moveToObject(float reach, StageObject *object, int mode, u_int flags); // FS2 0x0022e140
    bool moveToPoint(float reach, const float *point, int mode, u_int flags);   // FS2 0x0022e4a0
    // The move's goal; false without a move.
    bool getMoveGoal(float *out);                                        // FS2 0x0022e110
    bool cancelMove();                                                   // FS2 0x0022e6f0

    // Actions.
    bool playAction(const char *name, StageObject *a, StageObject *b);   // FS2 0x0022cac0
    bool attack(float range, StageObject *target, bool mode);            // FS2 0x0022d350
    bool approach(float distance, const float *point);                   // FS2 0x0022d6d0
    bool useDoor(bool open);                                             // FS2 0x0022a710
    bool useDoorKey(bool lock);                                          // FS2 0x0022a550
    bool dragOutCar();                                                   // FS2 0x0022a4f0
    bool takeUpTrap(int arg);                                            // FS2 0x0022a8c0
    bool setTrap(StageObject *trap);                                     // FS2 0x0022a8f0
    bool gunAction();                                                    // FS2 0x0022a920
    bool takeItem(StageObject *item);                                    // FS2 0x0022ab50
    bool action22ab90(void *a, void *b);                                 // FS2 0x0022ab90
    bool action22abf0();                                                 // FS2 0x0022abf0
    bool action22ad60();                                                 // FS2 0x0022ad60
    bool hit(StageObject *target);                                       // FS2 0x0022be10
    bool hitInTime(float seconds, StageObject *target);                  // FS2 0x0022ba80
    bool hitRunning(float seconds, StageObject *target);                 // FS2 0x0022bc50
    bool swingAt(StageObject *target);                                   // FS2 0x0022be20
    bool attackTask(StageObject *target);                                // FS2 0x0022b970
    bool action22bf60();                                                 // FS2 0x0022bf60
    bool action22c0f0(int arg);                                          // FS2 0x0022c0f0
    bool action22c840(const char *name);                                 // FS2 0x0022c840
    bool action22c960(int arg);                                          // FS2 0x0022c960
    bool action22cbe0();                                                 // FS2 0x0022cbe0
    bool actOnCharacter(StageObject *character);                         // FS2 0x0022ccf0
    bool actLeftRight();                                                 // FS2 0x0022cde0
    bool actLeft();                                                      // FS2 0x0022cf90
    bool actOnFoeRight(StageObject *foe);                                // FS2 0x0022d0f0
    bool actOnFoe(StageObject *foe);                                     // FS2 0x0022d220

    StageCharacter *owner = nullptr;    // FS2 +0x04
    // The filter of the rute's paths; null until the route's TU writes the
    // ctor, and RouteFilter::allow is a stub until then.
    RouteFilter    *filter = nullptr;   // FS2 +0x08
    RouteTask      *action = nullptr;   // FS2 +0x0c
    RouteTask      *move = nullptr;     // FS2 +0x10
    RouteTask      *look = nullptr;     // FS2 +0x14
};

#endif
