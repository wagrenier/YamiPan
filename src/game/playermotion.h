/*
 *  game/playermotion.h -- what a character is doing with its body (FS2,
 *  0x1e0000..0x1f0000, the object at StageCharacter +0x224, EE 0xc20).
 *
 *  Each frame something asks for a motion by its gMotionDefs index
 *  (request), and stepLower decides: the request may wait for a hold, for
 *  the walk or for the pair's other half, and it may need a stand-up or
 *  crouch-down motion first. stepUpper keeps the hands, stepFace the face.
 *  A motion of the sheet may name a link function (MotionLink: Walk, Jack,
 *  Ladder, ...), the code that runs it. A link holds a list of conditions
 *  (Squat, Move, ...), and those are what ask for the walking motions:
 *  at the start of the frame each says what the running motion allows
 *  (CharacterAllows), then input and brain say what they want
 *  (CharacterWants), then each turns the two into a request, the last one
 *  asking for the idle when nothing else did.
 *
 *  Written so far: the links Walk, Up, Fall, Jump, Ladder, Door, Pickup, Battle,
 *  BtAttack; the conditions Always, QuickTurn, Squat, Turn, Move, Level, LevelLift,
 *  Ladder, Jump, LadderMove, Door, PutItem, PickupItem, Lock, BtOn, BtAttack,
 *  BtAtkCombo, BtMove, and all of controlStep (what a motion's events do, the feet,
 *  the wind, health, the fidgets). A link or a condition of another name answers null, which
 *  the ROM takes as a motion with no link function; addCondition skips it.
 *  The special actions of 0x1df0b0 are written but for the cars' three.
 */
#ifndef GAME_PLAYERMOTION_H
#define GAME_PLAYERMOTION_H

#include <string.h>

#include "game/stagelist.h"

#include "game/gamehooks.h"
#include "game/packtables.h"

// gMotionDefs indices the code names, with the sheet's label. The other
// values stay numbers inside the predicates below.
enum MotionId
{
    Motion_None       = -1,
    Motion_Stand      = 0x00, // s
    Motion_Idle0      = 0x14, // idle0: a fidget
    Motion_Idle1      = 0x15, // idle1
    Motion_QuickTurn  = 0x0f, // t_q
    Motion_StairsUp   = 0x17, // Move, on a stair face
    Motion_StairsDown = 0x18,
    Motion_StairsRunUp = 0x19,
    Motion_StairsRunDown = 0x1a,
    Motion_Ops        = 0x1b, // ops: stops at an edge
    Motion_Wa         = 0x1c, // wa: running into a wall
    Motion_CrouchStand = 0x1d, // sq_s
    Motion_CrouchWalk = 0x2c, // sqw_w
    Motion_CrouchIdle = 0x2b, // sq_idle0
    Motion_StandUp    = 0x27, // sq_u: from a crouch, before what was asked (arbitrate)
    Motion_CrouchDown = 0x28, // sq_d
    Motion_Up060      = 0x2e, // u_060: up a ledge, by the height the sheet's motion was made for
    Motion_Up090      = 0x2f,
    Motion_Up120      = 0x30,
    Motion_Up180      = 0x31,
    Motion_Pair       = 0x32, // u_sp: two characters together: both must arrive
    Motion_FallLow    = 0x33, // fal_l: over an edge
    Motion_FallMid    = 0x34,
    Motion_FallHigh   = 0x35,
    Motion_LandLow    = 0x36, // lnd_l
    Motion_LandMid    = 0x37,
    Motion_LandHigh   = 0x38,
    Motion_Jump       = 0x39, // jp
    Motion_LadderUpOn = 0x3a, // ld_u_b: onto a ladder from its foot
    Motion_LadderUp   = 0x3b, // ld_u
    Motion_LadderUpOff = 0x3c, // ld_u_e: off at the top
    Motion_LadderDownOn = 0x3d, // ld_d_b0: onto it from the top
    Motion_LadderDown = 0x3e, // ld_d
    Motion_LadderRest = 0x3f, // ld_s: the Ladder link's motion
    Motion_LadderDownOff = 0x40, // ld_d_e
    Motion_LadderLetGo = 0x41, // ld_esp
    Motion_LadderFall = 0x42, // ld_fal
    Motion_DoorPullOpen = 0x43, // the door's half is dr_plo
    Motion_DoorPushClose = 0x44, // dr_psc
    Motion_DoorPushOpen = 0x45, // dr_pso
    Motion_DoorPullClose = 0x46, // dr_plc
    Motion_DoorKeyFront = 0x47, // dr_lcr: lock or unlock
    Motion_DoorKeyBack = 0x48,  // dr_lcl
    Motion_DoorSlideA = 0x49,   // dr_op or dr_cl, as the door stands
    Motion_DoorSlideB = 0x4a,
    Motion_DoorReachKey = 0x4b, // through the broken glass
    Motion_DoorForce  = 0x4c,   // spends its hit points
    Motion_Crouched   = 0xf7, // a request, resolved through the holds
    Motion_WalkStand  = 0xf9, // waits while the walk runs (stepLower)
    Motion_WalkCrouch = 0xfa,
    Motion_PairWait   = 0xfb, // waiting for the other half of Motion_Pair
};

// The slots of pickStance, the sheet's labels { standing, crouching }.
enum MotionStanceSlot
{
    MotionStance_Idle    = 0,  // s, sq_s
    MotionStance_Walk    = 1,  // w, sqw
    MotionStance_WalkBack = 2, // w_b
    MotionStance_WalkRight = 3, // w_r
    MotionStance_WalkLeft = 4, // w_l
    MotionStance_Run     = 5,  // run, ka
    MotionStance_TurnRight = 6, // t_r
    MotionStance_TurnLeft = 7, // t_l
    MotionStance_Call    = 8,  // call, sq_call
    MotionStance_Jack    = 9,  // jack, sq_jack
    MotionStance_WalkRun = 10, // w_run, sq_ka: walk into run
};

// The groups of the sheet, as the code tests them (work out a label with
// the table in architecture.md "Motions").
bool isCrouchMotion(int id);
// run, tr_run, w_run and the stairs at a run.
bool isRunMotion(int id);
// The walks, armed or not: what 移動速度 sets the pace of (moveStep).
bool isWalkMotion(int id);
// sq_ka and sqw_ka: spend health like running.
bool isCrouchRunMotion(int id);
// Standing or crouched, doing nothing: health comes back to the player.
bool isRestingMotion(int id);
// sqw, the crouched walks: the quietest steps.
bool isCrouchWalkMotion(int id);
// On a ladder, up or down: its steps have their own sound.
bool isLadderMotion(int id);
// Melee attacks and the guns' shots: Always leaves the lamp alone in them.
bool isAttackMotion(int id);
inline bool isCrouchSide(int id) { return isCrouchMotion(id) || id == Motion_Crouched; }
// Motions with a place in the world to go to, armed or not.
bool isMovingMotion(int id);
// A request that can take the place of a hold (resolveHold).
bool isHoldableMotion(int id);
// What a standing request may interrupt a crouch with (arbitrate).
bool isStandableMotion(int id);
// The motions that hold a gun up (stepUpper), low ones among them.
bool isArmedMotion(int id);
bool isArmedLowMotion(int id);

// What stepLower does with a request it cannot start yet: the request and
// the character's +0x360 block wait here (EE 0x54; +0x5c the motion's, +0x60
// the walk's).
struct MotionQueue
{
    static constexpr int kArgsSize = 0x44;

    void store(int request, const StageCharacter *owner, int current);
    void restore(StageCharacter *owner) const;
    // Empties it. done says the motion really ran.
    void close(bool done)
    {
        finished = done;
        last = motion;
        motion = Motion_None;
        open = 0;
    }
    // A slot open for a frame forgets what it just closed.
    void expire()
    {
        if (open != 0) {
            finished = 0;
            last = Motion_None;
        }
    }

    int    motion = Motion_None; // FS2 +0x00
    int    last = Motion_None;   // FS2 +0x04
    int    from = Motion_None;   // FS2 +0x08: the motion when it was stored
    u_char args[kArgsSize] = {}; // FS2 +0x0c: the character's +0x360
    u_char finished = 0;         // FS2 +0x50
    u_char open = 1;             // FS2 +0x51
};
static_assert(sizeof(MotionQueue) == 0x54, "MotionQueue: EE 0x54");
static_assert(offsetof(MotionQueue, finished) == 0x50, "MotionQueue::finished");

struct PlayerMotion;
class StageTrapItem;

// One thing a motion may lead to. onUpdate runs at the start of the frame
// and says what is allowed; onApply runs after input and brain and asks for
// the motion. Base 0x276510, vtable 0x317750.
// layout: host -- EE 0x14: +0 the character, +4 the motion, +8 a helper,
// +0xc active, +0x10 the vptr.
class MotionCondition
{
public:
    MotionCondition(StageCharacter *owner, PlayerMotion *motion_)        // FS2 0x00276510
        : character(owner), motion(motion_) {}
    virtual ~MotionCondition() {}
    virtual void onUpdate() = 0;                                         // FS2 slot +0xc
    virtual void onApply() = 0;                                          // FS2 slot +0x10

    u_char active = 1;          // FS2 +0xc
protected:
    StageCharacter *character;  // FS2 +0x0
    PlayerMotion   *motion;     // FS2 +0x4
};

// What a link's conditions sit in: the ROM's list (EE 8: +0 head, +4 count). A
// node keeps its value in a block of its own.
// layout: host -- EE 8 bytes, a node 0xc: +0 the value's block, +4 next, +8 previous.
class MotionConditionList
{
public:
    // layout: host -- see above.
    struct Node
    {
        // FS2 0x0027dc20
        explicit Node(MotionCondition **value_) : value(value_) {}
        MotionCondition **value;    // FS2 +0x0
        Node *next = nullptr;       // FS2 +0x4
        Node *previous = nullptr;   // FS2 +0x8
    };
    // layout: host -- a cursor, not in the ROM (it walks the nodes by hand).
    class Iterator
    {
    public:
        explicit Iterator(const Node *node_) : node(node_) {}
        MotionCondition *operator*() const { return *node->value; }
        Iterator &operator++() { node = node->next; return *this; }
        bool operator!=(const Iterator &other) const { return node != other.node; }
    private:
        const Node *node;
    };

    MotionConditionList();                                               // FS2 0x0027d8d0
    ~MotionConditionList();                                              // FS2 0x0027d8e0
    MotionConditionList(const MotionConditionList &) = delete;
    MotionConditionList &operator=(const MotionConditionList &) = delete;

    void clear();                                                        // FS2 0x0027daf0
    // At the end.
    void add(MotionCondition *const &value);                             // FS2 0x0027da40
    void addFirst(MotionCondition *const &value);                        // FS2 0x0027db60
    Node *last() const;                                                  // FS2 0x0027dbf0
    // The node at index; the head for one out of range.
    Node *nodeAt(int index) const;                                       // FS2 0x0027d940

    MotionCondition *operator[](int index) const { return *nodeAt(index)->value; }
    Iterator begin() const { return Iterator(head); }
    Iterator end() const { return Iterator(nullptr); }

private:
    Node *head;                 // FS2 +0x0
    int   count;                // FS2 +0x4
};

// A motion's own code. Every slot of the base (vtable 0x317c20) is empty.
// layout: host -- EE: +0 the character, +4 the motion, +8 a block holding the
// motion again, +0xc a condition list, +0x14 finished, +0x18 a counter, +0x1c
// the vptr.
class MotionLink
{
public:
    MotionLink(StageCharacter *owner, PlayerMotion *motion_);            // FS2 0x0027cc50
    virtual ~MotionLink();                                               // FS2 0x0027cbe0: slot 0, deleting
    virtual void onUpdate() {}                                           // FS2 0x001eee10: slot +0xc
    virtual void onApply() {}                                            // FS2 0x001eee20: slot +0x10
    virtual void onRequest() {}                                          // FS2 0x001ec640: slot +0x14
    // FS2 0x001eaba0
    virtual void onStart() {}
    virtual void onRun() {}                                              // FS2 0x001e5d50: slot +0x1c
    // FS2 0x001eabb0
    virtual void onLeave(MotionLink *next) {}
    // FS2 0x001ec3f0
    virtual void onCancel() {}
    // FS2 0x0027cab0
    void rewind()
    {
        finished = 0;
        counter = 0;
    }
    void update();                                                       // FS2 0x0027cb60
    // Each active condition asks, until one has.
    void apply();                                                        // FS2 0x0027cac0
    // The condition of that name joins the list; the ones not written yet
    // are left out, in their place.
    void addCondition(const char *name);                                 // FS2 0x0027da40
    // The same for a special action (PlayerMotion::m_specials), if it has it.
    void addSpecial(const char *name);

    u_char finished = 0;        // FS2 +0x14
    int    counter = 0;         // FS2 +0x18
protected:
    StageCharacter *character;  // FS2 +0x0
    PlayerMotion   *motion;     // FS2 +0x4
    PlayerMotion  **m_motionBlock; // FS2 +0x8
    MotionConditionList m_conditions; // FS2 +0xc
};

// The prey's side of a bite has no code of its own. Vtable 0x315eb0. layout: host.
class BittenLink : public MotionLink
{
public:
    using MotionLink::MotionLink;
    ~BittenLink() override {}                                            // FS2 0x001eedb0
};

// Gun up: Lock, Snipe, GunAttack, GunMove. Vtable 0x317a10. layout: host.
class GunLink : public MotionLink
{
public:
    ~GunLink() override;                                                 // FS2 0x0027d350: slot 0, deleting
    GunLink(StageCharacter *owner, PlayerMotion *motion_);               // FS2 0x0027aa90
    void onStart() override;                                             // FS2 0x0027aa70
    void onLeave(MotionLink *next) override;                             // FS2 0x0027a9e0
};

// A shot, or the butt of the gun: Lock and GunAttack. A shot may be followed
// by the next when the sheet's frames have gone by. Vtable 0x3179e0.
// layout: host.
class GunAttackLink : public MotionLink
{
public:
    ~GunAttackLink() override;                                           // FS2 0x0027d2d0: slot 0, deleting
    GunAttackLink(StageCharacter *owner, PlayerMotion *motion_);         // FS2 0x0027a910
    void onRequest() override;                                           // FS2 0x0027a8c0
    void onStart() override;                                             // FS2 0x0027a840
    void onRun() override;                                               // FS2 0x0027a650
    void onLeave(MotionLink *next) override;                             // FS2 0x0027a580

private:
    u_char keepsLock = 0;       // FS2 +0x20: CharacterFire_Auto
    u_char isShot = 0;          // FS2 +0x21: else the butt
};

// Rounds move on the motion's clicks. Vtable 0x3179b0. layout: host.
class ReloadLink : public MotionLink
{
public:
    ~ReloadLink() override;                                              // FS2 0x0027d250: slot 0, deleting
    using MotionLink::MotionLink;
    void onRun() override;                                               // FS2 0x0027a140
};

// A trap goes down on the click. Vtable 0x317920, +0xc14 of PlayerMotion.
// layout: host.
class TrapSetLink : public MotionLink
{
public:
    ~TrapSetLink() override;                                             // FS2 0x0027d0d0: slot 0, deleting
    using MotionLink::MotionLink;
    void onRun() override;                                               // FS2 0x00279210

    StageTrapItem *trap = nullptr;    // FS2 +0x20
    sceVu0FVECTOR  place = {};        // FS2 +0x30
};

// A trap comes up on the click. Vtable 0x3178f0, +0xc18 of PlayerMotion.
// layout: host.
class TrapCancelLink : public MotionLink
{
public:
    ~TrapCancelLink() override;                                          // FS2 0x0027d050: slot 0, deleting
    using MotionLink::MotionLink;
    void onRun() override;                                               // FS2 0x00279190

    StageTrapItem *trap = nullptr;    // FS2 +0x20
};

// An item leaves the hand on the click; the long weapon hides meanwhile.
// Vtable 0x317890, +0xc1c of PlayerMotion. layout: host.
class ThrowLink : public MotionLink
{
public:
    ~ThrowLink() override;                                               // FS2 0x0027cf50: slot 0, deleting
    using MotionLink::MotionLink;
    void onRun() override;                                               // FS2 0x00278ed0
    void onLeave(MotionLink *next) override;                             // FS2 0x00278ea0
    // What goes and how; a zero vector: the default throw.
    void setThrow(StageThrownItem *item_, const sceVu0FVECTOR vector);   // FS2 0x00278e40

private:
    StageThrownItem *item = nullptr;  // FS2 +0x20
    sceVu0FVECTOR    offset = {};     // FS2 +0x30
};

// A bite: the biter walks to the prey's front or back and holds on; the
// prey loses endurance on each of the biter's clicks until one of them gets
// away. Vtable 0x317860. layout: host.
class BiteLink : public MotionLink
{
public:
    ~BiteLink() override;                                                // FS2 0x0027ced0: slot 0, deleting
    // FS2 0x00278db0
    BiteLink(StageCharacter *owner, PlayerMotion *motion_) : MotionLink(owner, motion_) {}
    void onRequest() override;                                           // FS2 0x00278d90
    void onRun() override;                                               // FS2 0x00278640
    void onLeave(MotionLink *next) override;                             // FS2 0x00278460
    void onCancel() override;                                            // FS2 0x00278d70

private:
    // The two stand too far from where the motion puts them, or a wall came
    // between them, or a timed change of rate is on.
    bool isBroken();

    StageCharacter *target = nullptr; // FS2 +0x20
    int    hits = 0;                  // FS2 +0x24
};

// A door's motion: the Door condition picks it, and the door's own half
// plays on the door. EE 0x28, vtable 0x317aa0, the field +0xc0c of
// PlayerMotion. layout: host.
class DoorLink : public MotionLink
{
public:
    ~DoorLink() override;                                                // FS2 0x0027d4d0: slot 0, deleting
    // FS2 0x0027b640
    DoorLink(StageCharacter *owner, PlayerMotion *motion_) : MotionLink(owner, motion_) {}
    void onRequest() override;                                           // FS2 0x0027b590
    void onStart() override;                                             // FS2 0x0027b3d0
    void onRun() override;                                               // FS2 0x0027b140
    void onLeave(MotionLink *next) override;                             // FS2 0x0027b0f0
    void onCancel() override;                                            // FS2 0x0027b570

    StageDoor *door = nullptr;  // FS2 +0x20: while it runs
    float  rate = 1.0f;         // FS2 +0x24: of the motion and of the door
};

// Bending for an item: on the motion's click it comes into the hand, or
// leaves it; PickupItem and PutItem share the motion. EE 0x2c, vtable 0x317b90,
// the field +0xc08 of PlayerMotion. layout: host.
class PickupLink : public MotionLink
{
public:
    ~PickupLink() override;                                              // FS2 0x0027d750: slot 0, deleting
    // FS2 0x0027c140
    PickupLink(StageCharacter *owner, PlayerMotion *motion_) : MotionLink(owner, motion_) {}
    void onRequest() override;                                           // FS2 0x0027c100
    void onRun() override;                                               // FS2 0x0027bef0
    void onCancel() override;                                            // FS2 0x0027c0e0

    StageItem *putting = nullptr; // FS2 +0x20: going down
    StageItem *taking = nullptr;  // FS2 +0x24: coming up
    u_char     picks = 1;         // FS2 +0x28: 1 PickupItem asked, 0 PutItem
};

// The sight-jack's stance (jack, sq_jack, for everyone but a scenario's lead,
// who starts it straight from the pad): the scene's player holds the party's
// view open for as long as the motion lasts. Its one condition is Jack, which
// also asks out of it. EE 0x24, vtable 0x317bc0. layout: host.
class JackLink : public MotionLink
{
public:
    ~JackLink() override;                                                // FS2 0x0027d7d0: slot 0, deleting
    JackLink(StageCharacter *owner, PlayerMotion *motion_);              // FS2 0x0027c370
    void onRequest() override;                                           // FS2 0x0027c300
    void onRun() override;                                               // FS2 0x0027c1e0
    void onCancel() override;                                            // FS2 0x0027c2c0

    u_char started = 0;         // FS2 +0x20: this link started the party's view
};

// The fighting stance: bt_on, bt_s, bt_w.., turns, with the conditions Lock,
// BtAttack and BtMove. Vtable 0x317a70. layout: host.
class BattleLink : public MotionLink
{
public:
    ~BattleLink() override;                                              // FS2 0x0027d450: slot 0, deleting
    BattleLink(StageCharacter *owner, PlayerMotion *motion_);            // FS2 0x0027b000
    void onLeave(MotionLink *next) override;                             // FS2 0x0027af70
};

// A blow: Lock and BtAtkCombo, the second from the motion's Cancel event on.
// The blow ends with the motion, at a wall (bumped), or on the part it
// breaks, and those two hold the next blow off for 0.65 s. Vtable 0x317a40.
// layout: host.
class BtAttackLink : public MotionLink
{
public:
    ~BtAttackLink() override;                                            // FS2 0x0027d3d0: slot 0, deleting
    BtAttackLink(StageCharacter *owner, PlayerMotion *motion_);          // FS2 0x0027aea0
    void onStart() override;                                             // FS2 0x0027adf0
    void onRun() override;                                               // FS2 0x0027ac60
    void onLeave(MotionLink *next) override;                             // FS2 0x0027aba0
};

// The motions of the feet: stand, walk, run, turn. Each frame it looks for
// a face ahead and what lies behind it: the edge the player may stop at.
// The main camera rises when the drop is deep, partnerResponds asks the
// same. The field +0xc00 of PlayerMotion, the map's Walk.
// layout: host -- EE 0x94, vtable 0x317bf0.
class WalkLink : public MotionLink
{
public:
    ~WalkLink() override;                                                // FS2 0x0027d850: slot 0, deleting
    WalkLink(StageCharacter *owner, PlayerMotion *motion_);              // FS2 0x0027c520
    void onUpdate() override;                                            // FS2 0x0027c420
    void onLeave(MotionLink *next) override;                             // FS2 0x0027c4b0

    sceVu0FVECTOR wallPoint = {}; // FS2 +0x20: where the ray ended
    StageCollisionFace *wallFace = nullptr; // FS2 +0x30: the face ahead
    StageObject *edgeObject = nullptr; // FS2 +0x40: whose the edge is
    sceVu0FVECTOR edgeTarget = {}; // FS2 +0x50: where to land
    float  edgeDrop = 0.0f;       // FS2 +0x60: how far the floor is, negative below
    StageObject *edgeFloor = nullptr; // FS2 +0x64: whose floor that is
    sceVu0FVECTOR edgePoint = {}; // FS2 +0x70: on the edge
    sceVu0FVECTOR edgeAngles = {}; // FS2 +0x80: facing it
    u_char edgeFound = 0;         // FS2 +0x90
    u_char edgeBlocked = 0;       // FS2 +0x91: there is no way over it
};

// Up: the height of the ledge scales the climb. Vtable 0x317b60.
// layout: host -- +0x20 rise, +0x24 scale on the base link.
class UpLink : public MotionLink
{
public:
    ~UpLink() override;                                                  // FS2 0x0027d6d0: slot 0, deleting
    using MotionLink::MotionLink;
    void onRequest() override;                                           // FS2 0x0027be70
    void onStart() override;                                             // FS2 0x0027bdb0
    void onRun() override;                                               // FS2 0x0027bca0

private:
    float rise = 0.0f;          // FS2 +0x20
    float scale = 1.0f;         // FS2 +0x24
};

// Fall: over an edge, or off a ladder, until the floor; the height fallen
// picks the landing. Vtable 0x317b30.
// layout: host -- on the base link.
class FallLink : public MotionLink
{
public:
    ~FallLink() override;                                                // FS2 0x0027d650: slot 0, deleting
    using MotionLink::MotionLink;
    void onRun() override;                                               // FS2 0x0027b860

private:
    float  startHeight = 0.0f;  // FS2 +0x20
    float  fallStep = 0.0f;     // FS2 +0x24: the motion's last step goes on
    u_char tracking = 0;        // FS2 +0x28: the motion's Click events turn it off and on
};

// Jump: lands the moment there is a floor. Vtable 0x317b00.
// layout: host
class JumpLink : public MotionLink
{
public:
    ~JumpLink() override;                                                // FS2 0x0027d5d0: slot 0, deleting
    using MotionLink::MotionLink;
    void onRun() override;                                               // FS2 0x0027b7e0
};

// Ladder: at rest on one; its one condition is LadderMove. Vtable 0x317ad0.
// layout: host -- on the base link.
class LadderLink : public MotionLink
{
public:
    ~LadderLink() override;                                              // FS2 0x0027d550: slot 0, deleting
    LadderLink(StageCharacter *owner, PlayerMotion *motion_);            // FS2 0x0027b730
    void onRequest() override;                                           // FS2 0x0027b6e0

    float baseHeight = 0.0f;    // FS2 +0x20: where it was got onto
    float height = 0.0f;        // FS2 +0x24: LadderCondition's
};

// What the wall probes share (PlayerMotion +0x4, 0x26c6b0). layout: host --
// EE 8: +0 the character, +4 a small vector { 0, 1, 0 }.
// How far shots stray: a character's m_shotScatter, less as the aim settles
// over three seconds of frames (the controlled one; others at once).
struct ShotSpread
{
    float amount = 0.0f;        // FS2 +0x0: at 10 units of distance
    float rate = 1.0f;          // FS2 +0x4: of settled, a frame
    float settled = 0.0f;       // FS2 +0x8: 0..1
    // out: range along a line strayed from from .. to.
    void scatter(float range, sceVu0FVECTOR out, const sceVu0FVECTOR from,
                 const sceVu0FVECTOR to) const;                          // FS2 0x00269db0
};

// layout: host -- EE: +0 the owner, +4 a pointer to the spread.
struct MotionProbe
{
    explicit MotionProbe(StageCharacter *owner_) : owner(owner_) {}      // FS2 0x0026c6b0
    // A trap this character can spring.
    // Blood on a bone, along direction. Not written.
    void spawnBlood(float size, StageNodeBase *node, const float *direction, int kind, int arg); // FS2 0x0026a010
    StageTrapItem *findTrap();                                           // FS2 0x0026a390
    // The door within reach, facing it, free. A door of type R that stands
    // open must leave room to walk to its place.
    StageDoor *findDoor();                                               // FS2 0x0026a6b0
    // The nearest free item in reach that the head can see.
    StageItem *findItem();                                               // FS2 0x0026a4a0
    // Something to hit: alive, on, and if a character, an enemy.
    bool isTargetValid(StageObject *target);                             // FS2 0x0026b5d0
    // The head sees point, and not more than 60 degrees up or down.
    bool canSeeTarget(StageObject *target, const float *point);          // FS2 0x0026b690
    // The best target but m_290 in a cone along angles (x, z) within range:
    // of group 2 with characters, else of the other groups; nearest, or
    // nearest to the axis. Nothing found: m_290.
    StageObject *findTargetAhead(float cone, float range, bool characters, bool nearest,
                                 const float *angles);                   // FS2 0x0026a8f0
    // The same to one side: side 1 in (0, span) from facing, 0 in (span, 0);
    // nearest, or the farthest round.
    StageObject *findTargetBeside(float facing, float span, float range, bool characters,
                                  bool nearest, int side);               // FS2 0x0026aec0
    // A blow is on and m_290's nearest part is within 140: that part; -1 none.
    int  findBlowPart();                                                 // FS2 0x0026a250
    // A ray from 50 above the feet to just beyond the radius, ahead or
    // behind. out takes the end, back at the feet's height.
    StageCollisionFace *faceAhead(sceVu0FVECTOR out, bool forward);      // FS2 0x0026c4b0
    // What lies behind a face ahead one may climb or drop over: object and
    // point of the edge, angles facing it, the floor beyond it, how far that
    // is and where to land. True when one can get there; blocked: nothing
    // is there and nothing could ever be.
    bool findEdge(StageCollisionFace *face, StageObject **object, sceVu0FVECTOR target, float *drop,
                  StageObject **floor, sceVu0FVECTOR point, sceVu0FVECTOR angles, u_char *blocked); // FS2 0x0026b960
    // The floor at point: above it (up at most), else below (down at most).
    // drop: the height to it, negative below.
    bool findFloor(float up, float down, const sceVu0FVECTOR point, float *drop, StageObject **floor); // FS2 0x0026c180
    // A point 4000 ahead of the head, turned by the angles.
    void pointAhead(float x, float z, sceVu0FVECTOR out);                // FS2 0x0026b860

    StageCharacter *owner;      // FS2 +0x0
    ShotSpread spread;          // FS2 +0x4: a block of its own on the EE
};

class MotionSpecial;

// layout: host -- EE 0xc20; the offsets are the ROM's.
struct PlayerMotion
{
    explicit PlayerMotion(StageObject *owner);                           // FS2 0x001ed410
    ~PlayerMotion();                                                     // FS2 0x001ed2a0: deleting
    // The model's motions: the base from mdl, or the list's first file,
    // the bones, then the rest of the list and the tables per motion.
    void loadMotions(StageList<void *> *motions, StageList<void *> *motions2, void *mdl); // FS2 0x001ecda0
    void reset(int arg);                                                 // FS2 0x001ec7e0
    // Slots +0x14 and +0x18 of each entry of the list at +0x30 (unread):
    // after StageCharacter::setup cleared +0x360 and +0x2c0.
    // The special actions this character has, by its sheet and the stage.
    void initList();                                                     // FS2 0x001df0b0
    // The one of that name may start now. PC: a name it lacks answers false.
    bool isSpecialAllowed(const char *name);                             // FS2 0x001def60
    void setSpecial(const char *name, bool on);                          // FS2 0x001def90
    // Name is a special and no condition (FS2's third argument, a place for
    // the record, is null at the one caller).
    bool isSpecialOnly(const char *name);                                // FS2 0x001deed0
    // m_holds[0] = id when id is 0..0xff; false otherwise.
    bool setHold(int id);                                                // FS2 0x001ded30
    // The same, only when no hold stands.
    bool setHoldIfFree(int id);                                          // FS2 0x001ded60
    // The motion a special plays; -1 no such special, or one that plays none.
    int  specialMotion(const char *name);                                // inline in the ROM (0x225fc0)
    // The byte +0x18 of the special GettingOnCar, a class not written.
    bool getCarSpecialFlag();                                            // FS2 0x001dee50
    void setCarSpecialFlag(bool on);                                     // FS2 0x001dee90
    // The gadget special called name takes text and turns on; false when
    // it has none. Inline in the ROM (CharacterBrain::useNumLock).
    bool armGadgetSpecial(const char *name, const char *text);
    // owner bites prey: both walk to their places.
    void startBite(StageCharacter *prey);                                // FS2 0x0026fbc0
    void initSlots14();                                                  // FS2 0x001df040
    void initSlots18();                                                  // FS2 0x001defd0
    // Swaps the weapons; a motion that looks different now starts again.
    void refresh();                                                      // FS2 0x001e66c0
    // The matrix of the lamp the player aims: at bn_head, turned as the
    // player and the motion are.
    void getLampMatrix(sceVu0FMATRIX out);                               // FS2 0x001e60b0
    // The same at the eye: the model family's offset past bn_head.
    void getEyeMatrix(sceVu0FMATRIX out);                                // FS2 0x001e5f70

    // The frame's start (StageCharacter's start hook).
    void beginFrame();                                                   // FS2 0x001e5d90
    // The three steps of stageUpdateCharacterMotions. immediate: reset's,
    // the motion is applied from its first key.
    void stepLower(bool immediate);                                      // FS2 0x001e9dd0
    void stepUpper(bool immediate);                                      // FS2 0x001e8a30
    void stepFace(bool immediate);                                       // FS2 0x001e8870

    // Asks for id unless a request stands.
    void request(int id);                                                // FS2 0x001ec7a0
    // Asks for id over any request, and stops a walk.
    void requestNow(int id);                                             // FS2 0x001ec650
    // The motion of a locomotion slot, in the character's stance.
    int  pickStance(int slot);                                           // FS2 0x001eb660
    // The motion of a gun slot: by the long gun's class (no rifle, rifle,
    // R_MINIMI), -1 without one.
    int  pickGunMotion(int slot) const;                                  // FS2 0x001eb550
    // The motion of an armed slot, { no long gun, a long gun }.
    int  pickArmedMotion(int slot) const;                                // FS2 0x001eb520
    // The character's control hook: what the ground checks, then the
    // conditions ask.
    void controlStep();                                                  // FS2 0x001e3e80
    // The character's movement hook, before the model runs: how far the
    // walk goes on this floor, the floor's push, m_moveStep and m_turnStep
    // into the character, whether the pose's solvers are worth running.
    void applyMovement();                                                // FS2 0x001e0e20
    // The terminate hook's half: the hits taken, how long it has run, a Yri
    // or Kne's stance, the motion's sound events.
    void endFrame();                                                     // FS2 0x001e06f0
    // The frame's hits: the reaction, the flinch (only the first line is
    // written: the rest needs the hit 0x20eb40 and its enemies).
    void takeHits();                                                     // FS2 0x001e18c0
    // What an event breaks off: the idle, or the link ends where the motion
    // has a next one.
    void releaseAction();                                                // FS2 0x001dedb0
    // Two characters in a struggle: this one comes out on top.
    bool winsStruggle(StageObject *other);                               // FS2 0x001e05b0
    // Turns the body toward angles (limits +-65 degrees); slope: it leans
    // into the floor.
    void leanBody(const sceVu0FVECTOR angles, bool slope, u_char flag);  // FS2 0x001e6400
    void leanBodyAt(float rate, float maxStep, const sceVu0FVECTOR angles, bool slope, u_char flag); // FS2 0x001e6490
    // The three targets of the pose, null lets go.
    void setToolTarget(const float *target);                             // FS2 0x001dec30
    void setLookTarget(float rate, const float *target);                 // FS2 0x001dec50
    // The head, within the model family's limits.
    void setHeadTarget(float rate, const float *target);                 // FS2 0x001dec70
    // Where the lamp's light ends: on the first face along it, or at its
    // radius, never nearer than 200 to the lamp. Unchanged without a lamp.
    void getLampTarget(sceVu0FVECTOR out);                               // FS2 0x001e6160
    void clearEnding() { m_b30 = 0; }
    // Walks from a place to a place (either null: stays) at the next stepLower.
    void startWalkTo(float rate, const float *from, const float *to, bool scaled, bool held); // FS2 0x001eac10
    // The point 100 behind place, as angles face.
    void pointBehind(sceVu0FVECTOR out, const float *place, const float *angles); // FS2 0x001eac70
    // What is about to play: the queues, the request, then the motion.
    int  upcomingMotion() const
    {
        if (m_queue->motion != Motion_None)
            return m_queue->motion;
        if (m_walkQueue->motion != Motion_None)
            return m_walkQueue->motion;
        return m_request != Motion_None ? m_request : motionId;
    }
    // Someone near is already on the way up (up false) or down this ledge.
    bool levelBlocked(bool up) const;                                    // FS2 0x001dff40
    // Someone near holds the ladder.
    bool ladderBlocked() const;                                          // FS2 0x001e0360
    // Where startWalkFile would put the pose, and what it would aim at.
    void walkFileTarget(sceVu0FVECTOR target, sceVu0FVECTOR aimAt, void *file, const float *from,
                        const float *angles);                            // FS2 0x001eb090
    // Walks to where a motion file ends, from a place (a ledge).
    void startWalkFile(float rate, void *file, const float *from, const float *to, bool scaled, bool held); // FS2 0x001eabc0

    // The motion ended, or its link says so.
    bool isEnding() const;                                               // FS2 0x001eb150
    void noteEnding();                                                   // FS2 0x001ec080
    // The file of the requested motion.
    void *requestedFile();                                               // FS2 0x001eb210
    // A request is waiting, or just left, the queue.
    bool isQueued() const;                                               // FS2 0x001e6d00
    bool isWalkBusy() const;                                             // FS2 0x001e6d30

private:
    const char *lowerLabel(int id) const;
    const char *upperLabel(int id) const;
    MotionLink *findLink(const MotionDef *def) const;
    // The def when id would start now: new, or allowed to repeat.
    const MotionDef *startable(int id) const;
    // Cancels what the request would have started.
    void cancelRequested();
    // Replaces id by the hold it falls to: 1 done, 0 no hold, -1 the holds
    // no longer apply.
    int  resolveHold(int *out, int id) const;                            // FS2 0x001e8320
    // Stand-up, crouch-down and the pair's wait; true: the request moved.
    bool arbitrate();                                                    // FS2 0x001e6d60
    // The end of the motion that is leaving.
    void noteLeaving();                                                  // FS2 0x001ec0a0
public:
    void startRequested();                                               // FS2 0x001ec400
    // Into the scope (camview_snipe, the body hidden) or out of it.
    void setScope(bool on);                                              // FS2 0x001dfd90
private:
    // The pose walks to from, turning to aim at to, or where the file puts it.
    // scaled: rate grows with what there is to cross, at least 4 frames' worth.
    void startWalk(float rate, const float *from, const float *to, bool scaled, bool held); // FS2 0x001ead00
    void startWalkAt(float rate, void *target, const float *offset, const float *to, bool scaled, bool held); // FS2 0x001eaea0
    void walkPoseTo(float rate, const float *target, const float *aimAt, bool scaled); // the tail of both, inline in the ROM
    // The parts of endFrame, one function in the ROM.
    void stepRunClock(StageCharacter *character);
    void stepAltStance(StageCharacter *character);
    void stepSoundEvents(StageCharacter *character);
    // The parts of controlStep, one function in the ROM.
    bool stepEvents();
    void stepAim();
    void stepFeet(bool solve);
    void stepWind();
    void stepHealth();
    void stepFidgets(const StageObjRecord *record);

public:
    StageObject *owner;         // FS2 +0x000
    MotionProbe *probe;         // FS2 +0x004
    LiteralMap<MotionCondition *> m_conditions; // FS2 +0x008
    LiteralMap<MotionLink *> m_links; // FS2 +0x018
    MotionCondition *m_always = nullptr; // FS2 +0x028: stepped each frame, whatever runs
    LiteralMap<MotionSpecial *> m_specials; // FS2 +0x02c: initList; scripts and the brain switch them
    int    m_request = Motion_None; // FS2 +0x03c
    int    motionId = Motion_None;  // FS2 +0x040: gMotionDefs index; outside the table none
    const char *m_handNames[2] = {}; // FS2 +0x044: left, right, as stepUpper last played them
    const char *m_faceName = nullptr; // FS2 +0x04c
    u_char m_endNoted = 0;      // FS2 +0x050
    u_char m_canStart = 1;      // FS2 +0x051
    int    m_holds[2] = { Motion_None, Motion_None }; // FS2 +0x054: what resolveHold falls to
    MotionQueue *m_queue;       // FS2 +0x05c
    MotionQueue *m_walkQueue;   // FS2 +0x060
    u_char m_savedWalk = 0;     // FS2 +0x064: m_walkPending while queued
    u_char m_stepped = 0;       // FS2 +0x065: stepLower ran this frame
    u_char m_forceStart = 0;    // FS2 +0x066: refresh: the same id starts again
    MotionLink *activeLink = nullptr; // FS2 +0x068: the running motion's; the main camera reacts when this is walkLink
    int    m_weaponClass = 0;   // FS2 +0x06c: which of rifleLabelText
    const char *m_faces[kMotionDefCount] = {}; // FS2 +0x070
    u_char m_handsBusy[kMotionDefCount][2] = {}; // FS2 +0x470: left, right: the motion moves the fingers itself
    float  m_endHeight[kMotionDefCount] = {}; // FS2 +0x670: the first track's last key, third value
    u_char m_walking = 0;       // FS2 +0xa70: the pose walks to a point
    u_char m_walkHeld = 0;      // FS2 +0xa71
    u_char m_walkPending = 0;   // FS2 +0xa72: startWalk at the next stepLower
    u_char m_walkToTarget = 0;  // FS2 +0xa73: startWalkAt instead
    float  m_walkRate = 0.0f;   // FS2 +0xa74
    u_char m_walkScaled = 0;    // FS2 +0xa78
    u_char m_walkHeldNext = 0;  // FS2 +0xa79: m_walkHeld when it starts
    u_char m_hasWalkFrom = 0;   // FS2 +0xa80
    sceVu0FVECTOR m_walkFrom = {}; // FS2 +0xa90
    u_char m_hasWalkTo = 0;     // FS2 +0xaa0
    sceVu0FVECTOR m_walkTo = {}; // FS2 +0xab0
    void  *m_walkTarget = nullptr; // FS2 +0xac0
    sceVu0FVECTOR m_walkOffset = {}; // FS2 +0xad0
    u_char m_hasWalkEnd = 0;    // FS2 +0xae0
    sceVu0FVECTOR m_walkEnd = {}; // FS2 +0xaf0
    float  m_motionTime = 0.0f; // FS2 +0xb00: seconds in this motion
    float  m_runTime = 0.0f;    // FS2 +0xb04: Move: running that long into a wall trips
    float  m_runDistance = 0.0f; // FS2 +0xb08: the same, in units
    sceVu0FVECTOR m_lastPosition = {}; // FS2 +0xb10: the character's at reset
    u_char m_stanceFlag = 0;    // FS2 +0xb20: pickStance's third table; endFrame sets it half a second after a Yri or Kne's YAM_ light went
    float  m_stanceTime = 0.0f; // FS2 +0xb24: endFrame's half second
    float  m_hitTime = 0.0f;    // FS2 +0xb28: 0x1e18c0: the wait, 0.15 s, between two hits' effects
    int    m_soundEvent = 0;    // FS2 +0xb2c: the next of the sheet's S0..S3; 0 when a motion starts or ends
    u_char m_b30 = 0;           // FS2 +0xb30: a motion event 0x4000 ends an idle: Walk's list runs for it
    // +0xb31..+0xb38: what the character's kind has (the ctor's table).
    u_char m_kindFlags1 = 0;    // FS2 +0xb31
    u_char m_kindFlags2 = 0;    // FS2 +0xb32
    u_char m_kindFlags3 = 0;    // FS2 +0xb33
    u_char m_hasHands = 0;      // FS2 +0xb34: stepUpper runs, m_handsBusy is filled
    u_char m_hasFace = 0;       // FS2 +0xb35: stepFace runs, the second list loads
    u_char m_kindFlags6 = 0;    // FS2 +0xb36
    u_char m_kindFlags7 = 0;    // FS2 +0xb37
    u_char m_kindFlags8 = 0;    // FS2 +0xb38
    sceVu0FVECTOR m_kindVector = {}; // FS2 +0xb40: kinds 6 and 0xf
    u_char m_stepsOn[4] = {};   // FS2 +0xb50: [0] the feet solve, [1] the wind blows, [2] stepUpper, [3] stepFace run; reset sets them
    u_char m_b54 = 0;           // FS2 +0xb54: with m_kindFlags7, the body follows the floor
    int    m_modelFamily = -1;  // FS2 +0xb58: gMotionModels index
    sceVu0FVECTOR m_moveStep = {}; // FS2 +0xb60: this frame's, into the position (applyMovement)
    sceVu0FVECTOR m_turnStep = {}; // FS2 +0xb70: this frame's, into the rotation
    sceVu0FVECTOR m_lampAngles = {}; // FS2 +0xb80: the lean of leanBody; getLampMatrix's
    sceVu0FVECTOR m_leanScale = {}; // FS2 +0xb90: the lean / 65 degrees, y and w 1
    u_char m_leanFlag = 0;      // FS2 +0xba0: leanBody's
    float  m_fallSpeed = 0.0f;  // FS2 +0xba4: grows off the ground, into m_moveStep
    float  m_ba8 = 0.0f;        // FS2 +0xba8: a countdown
    u_char m_wallCheckNow = 0;  // FS2 +0xbac: m_wallCheck as the last frame ended (0x1e18c0); the ground's sync reads it
    u_char m_wallCheck = 0;     // FS2 +0xbad: the motion's 壁判定
    u_char m_bae = 0;           // FS2 +0xbae
    sceVu0FVECTOR position = {}; // FS2 +0xbb0
    sceVu0FVECTOR m_liftAngles = {}; // FS2 +0xbc0: LevelLift: how the partner faces
    StageItem *m_lookObject = nullptr; // FS2 +0xbd0: the item in reach: the head looks at its pick point, cleared each frame
    u_char scopeOn = 0;         // FS2 +0xbd4: camview_snipe; a shot shows no flash (StageWeaponItem::fire)
    float  scopeZoom = 0.0f;    // FS2 +0xbd8: 0..1, the sight's picture too (MenuProgSet::draw)
    sceVu0FVECTOR scopeRotation = {}; // FS2 +0xbe0: the character's as the scope came up
    float  m_bf0 = 0.0f;        // FS2 +0xbf0: bitten: the wait before the next press counts
    int    m_reviveFrames = 0;  // FS2 +0xc10's +0x20: DamageDead's count (the link is not written)
    float  m_bf4 = 0.0f;        // FS2 +0xbf4
    float  m_shoutLoudness = 0.0f; // FS2 +0xbf8: a motion event SHOUT's; 0x216280 sets it
    u_char m_bfc = 0;           // FS2 +0xbfc
    WalkLink *walkLink = nullptr; // FS2 +0xc00: the map's Walk
    PickupLink *pickupLink = nullptr; // FS2 +0xc08: the map's Pickup
    DoorLink *doorLink = nullptr; // FS2 +0xc0c: the map's Door
    TrapSetLink *trapSetLink = nullptr; // FS2 +0xc14: the map's TrapSet
    TrapCancelLink *trapCancelLink = nullptr; // FS2 +0xc18: the map's TrapCancel
    ThrowLink *throwLink = nullptr; // FS2 +0xc1c: the map's Throw
};

#endif // GAME_PLAYERMOTION_H
