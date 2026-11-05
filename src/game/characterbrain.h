/* ========================================================================
 *  game/characterbrain.h -- what drives a character nobody controls.
 *
 *  The brain is a script runner of its own. It runs the character's AI
 *  scripts in eight slots, one set per brain state: an even slot is the
 *  state's script, the odd slot above it a script called over it
 *  (startOverlay). Its commands (runCommand, 0x19..0x53) walk, turn and act
 *  through the character's CharacterRoute and wait on BrainWait; every other
 *  script command falls through ScriptProxy to the character itself.
 *
 *  The names are read from the code: "brain", "slot", "overlay", "retry" and
 *  the state names are the port's, not the game's. FS2 has no symbols here.
 * ======================================================================== */
#ifndef _GAME_CHARACTERBRAIN_H
#define _GAME_CHARACTERBRAIN_H

#include "game/scriptrunner.h"
#include "sdk/libvu0.h"

class CharacterBrain;
class StageCharacter;
class StageObject;
class StageDoor;
class CharacterRoute;           // game/characterroute.h

// A script runner whose commands go to another: vtable 0x315690, the runner's
// table with most slots forwarding. Two slots say to whom: the object (+0x1b8)
// and the target (+0x1bc); both subclasses answer the same for each. Only the
// slots with a host name in ScriptRunner are here; the forwarders 0x1a6920
// (+0x94), 0x1a69d0 (+0x9c), 0x1a6b50 (+0x104), 0x1a7060, 0x1a70a0, 0x1a70e0,
// 0x1a7130, 0x1a7170, 0x1a71b0, 0x1a71f0, 0x1a7230, 0x1a7350, 0x1a73e0,
// 0x1a7420, 0x1a7460, 0x1a74e0, 0x1a7520, 0x1a7560, 0x1a75a0,
// 0x1a75e0, 0x1a7620, 0x1a7660, 0x1a7700, 0x1a77c0, 0x1a7800,
// 0x1a7890 wait for the natives that name their slots.
// layout: host -- EE 0xb8, as ScriptRunner.
class ScriptProxy : public ScriptRunner
{
public:
    ~ScriptProxy() override;                                             // FS2 0x001a68b0: deleting

    // Slots +0x1b8 and +0x1bc, empty here.
    virtual ScriptRunner *getForwardObject() = 0;
    virtual ScriptRunner *getForwardTarget() = 0;

    int  getProperty(int id, int sub) override;                          // FS2 0x001cf130
    void setProperty(int id, int sub, int value) override;               // FS2 0x001cf0b0
    float scriptGetPosX() override;                                      // FS2 0x001a6a90
    float scriptGetPosY() override;                                      // FS2 0x001a6ad0
    float scriptGetPosZ() override;                                      // FS2 0x001a6b10
    void scriptSetPosition(const float *pos) override;                   // FS2 0x001a6b90
    void scriptSetRotation(const float *rot) override;                   // FS2 0x001a6bd0
    void scriptSetYaw(const float *yaw) override;                        // FS2 0x001a6c10
    void cmdSetColor(const float *color) override;                       // FS2 0x001a6c50
    void cmdAddColorKey(int frame, const float *color) override;         // FS2 0x001a6c90
    void cmdSetColorLoop(u_char loop) override;                          // FS2 0x001a6ce0
    void cmdSetRadiusSquared(float radius) override;                     // FS2 0x001a6d20
    void cmdSetConeAngle(int angle) override;                            // FS2 0x001a6d60
    void cmdSetTreeSwitchedOn(bool on) override;                         // FS2 0x001a6da0
    void cmdSetTreeBroken(bool on) override;                             // FS2 0x001a6de0
    void cmdSetFadeFrames(int frames) override;                          // FS2 0x001a6e20
    void cmdSetFlag240(bool on) override;                                // FS2 0x001a6e60
    void cmdSetValue244(int value) override;                             // FS2 0x001a6ea0
    void cmdSetFlag248(bool on) override;                                // FS2 0x001a6ee0
    void cmdSetColor250(const float *color) override;                    // FS2 0x001a6f20
    void cmdSetColor260(const float *color) override;                    // FS2 0x001a6f60
    void cmdSetCastsShadow(bool on) override;                            // FS2 0x001a6fa0
    void cmdSetEnabled(bool on) override;                                // FS2 0x001a6fe0
    void command34(void *arg) override;                                  // FS2 0x001a7020
    StageCharacter *getScriptCharacter() override;                       // FS2 0x001a6960
    StageDoor *getScriptDoor() override;                                 // FS2 0x001a6a10
    StageObject *getScriptObject() override;                             // FS2 0x001a6a50
    float command58(void *arg) override;                                 // FS2 0x001a7270
    float command59(void *arg) override;                                 // FS2 0x001a72b0
    bool scriptIsFacing(StageObject *other) override;                    // FS2 0x001a73a0
    void scriptPlayMotion(const char *name) override;                    // FS2 0x001a76c0
    int  command80() override;                                           // FS2 0x001a7750
    void scriptAddMenuLine(const u_short *text, int kind, bool plain) override; // FS2 0x001a72f0
    bool scriptIsCrouching() override;                                   // FS2 0x001a75a0
    bool scriptHasLineTo(StageCharacter *other) override;                // FS2 0x001a74a0
    void scriptPutWeaponAway() override;                                 // FS2 0x001a7850
    int  scriptHeardKind() override;                                     // FS2 0x001a70a0
    bool cmdIsLit() override;                                            // FS2 0x001a78d0
    int  command87() override;                                           // FS2 0x001a7910
    void command88() override;                                           // FS2 0x001a7950
    bool command99() override;                                           // FS2 0x001a7990
    int  command101(void *arg) override;                                 // FS2 0x001a79d0
};

// The script that runs beside a walk (commands 0x43..0x52): its commands go
// to the brain, the brain's own or on to the character. Vtable 0x315cf0.
// layout: host -- EE 0xb8, the brain's +0xb8.
class BrainSubRunner : public ScriptProxy
{
public:
    ~BrainSubRunner() override;                                          // FS2 0x001dd7f0: deleting

    // FS2 0x001de2e0
    ScriptRunner *getForwardObject() override { return getParent(); }
    // FS2 0x001de2d0
    ScriptRunner *getForwardTarget() override { return getParent(); }
    const char *getName() override;                                      // FS2 0x001de290
};

// What a character was told to look at; a look task ends when the script
// switches, so each script carries its own and the switch starts it again.
enum BrainLookKind
{
    BrainLook_None  = 0,
    BrainLook_Named = 1,        // arg.name, with flags
    BrainLook_Angle = 2,        // arg.angle, in degrees, with flags
    BrainLook_Point = 3,        // arg.point
    BrainLook_Yaw   = 4,        // arg.yaw, in radians
};

// EE 0x30: the brain's +0x1e0 and a frame's +0xb0.
// layout: host -- flags and arg.name are pointers; the offsets hold.
struct BrainLook
{
    int kind = BrainLook_None;          // FS2 +0x00
    union Arg
    {
        sceVu0FVECTOR point;
        const char   *name;
        int           angle;
        float         yaw;
    } arg = {};                         // FS2 +0x10
    const char *flags = "";             // FS2 +0x20: data 0x304e20, the empty string
};
static_assert(offsetof(BrainLook, arg) == 0x10, "BrainLook::arg");
static_assert(offsetof(BrainLook, flags) == 0x20, "BrainLook::flags");

// A slot: the script put aside and its look. EE 0xf0, vtable 0x315cd8.
// layout: host -- ScriptFrame is host; EE offsets beside the fields.
class BrainFrame : public ScriptFrame
{
public:
    BrainFrame();                                                        // FS2 0x001dd860
    ~BrainFrame() override;                                              // FS2 0x001dd610: deleting

    BrainLook look;             // FS2 +0xb0
    int       overlayId = -1;   // FS2 +0xe0: the id it was started with; -1 none
};

// How a command's wait ends (BrainWait::isDone).
enum BrainWaitKind
{
    BrainWait_RouteIdle = 0,    // the route's action and move have ended
    BrainWait_None      = 1,    // at once: nothing started, or nothing to wait for
    BrainWait_InSight   = 2,    // the move's goal comes into view, and the move ends there
    BrainWait_Frames    = 3,    // frames count down
    BrainWait_Done4     = 4,    // at once; no writer found
    BrainWait_Motion    = 5,    // at once (scriptPlayMotion)
};

// EE 0x10, the brain's +0x184: what the running command waits for.
// layout: host -- pointers; the EE offsets beside the fields.
struct BrainWait
{
    BrainWait(CharacterBrain *brain_, CharacterRoute *route_) : brain(brain_), route(route_) {}

    // The command's flags, inline in the ROM's lookAt / turnTo / moveTo.
    bool lookAt(const char *name, int angle, const char *flags, int force = 1); // FS2 0x001dd8a0
    bool turnTo(const char *name, int angle, const char *flags);         // FS2 0x001dda10
    bool moveTo(const char *name, const int *point, const char *flags);  // FS2 0x001ddb70
    bool isDone();                                                       // FS2 0x001ddf20
    // Inline in the ROM (commands 0x20, 0x40, 0x48, 0x49, 0x4f..0x52).
    bool attackFoe(float range, bool mode);

    CharacterBrain *brain;              // FS2 +0x0
    CharacterRoute *route;              // FS2 +0x4
    int             kind = BrainWait_RouteIdle; // FS2 +0x8: BrainWaitKind
    u_int           frames = 0;         // FS2 +0xc: BrainWait_Frames
};

// The brain's state: StageCharacter::setBrainState. The names are guesses
// from getLifeState and what the slots do.
enum BrainState
{
    BrainState_Off    = 0,      // no script; the character is out of the scene
    BrainState_Normal = 1,      // slot 0
    BrainState_Slot4a = 2,      // slot 4
    BrainState_Slot4b = 3,      // slot 4
    BrainState_Slot4c = 4,      // slot 4
    BrainState_Foe    = 5,      // slot 6: found a foe; the alarm of 0x1dbca0
    BrainState_Slot2  = 6,      // slot 2: getLifeState calls it down
    BrainState_Down   = 7,      // put aside by suspend; revive goes back
};

constexpr int kBrainSlots  = 8;
constexpr int kBrainNoSlot = -1;

// EE 0x9b0. vtable 0x315b10, vptr +0xb4.
// layout: host -- ScriptRunner is host; EE offsets beside the fields.
class CharacterBrain : public ScriptProxy
{
    friend struct BrainWait;

public:
    explicit CharacterBrain(StageCharacter *owner);                      // FS2 0x001dd680
    ~CharacterBrain() override;                                          // FS2 0x001dd510: deleting
    void reset();                                                        // FS2 0x001dd460

    bool isEnabled() const { return m_enabled != 0; }                    // FS2 0x001dd3e0
    void setEnabled(bool on) { m_enabled = on; }                         // FS2 0x001dd3f0
    // A frame: the command's wait, then the script; the sub-script meanwhile.
    void update();                                                       // FS2 0x001dd330
    // A frame of the route, before the motion.
    void updateRoute();                                                  // FS2 0x001dd2f0

    // The state, and its script; run runs it at once.
    void setState(int newState, int run);                                // FS2 0x001dbca0
    // Out of state 7: where it stood, what it was doing.
    void revive();                                                       // FS2 0x001dc950

    // The scripts.
    void clearAllSlots();                                                // FS2 0x001dc060
    // The slot's script; the running slot takes it at once.
    void setSlotEntry(int slot, const ScriptEntry *entry, int id);       // FS2 0x001dc0d0
    void endOverlay();                                                   // FS2 0x001dc440
    void endOverlayById(int id);                                         // FS2 0x001dc330
    // Starts entry over the state's script. Answers whether one ran before.
    bool startOverlay(int id, const ScriptEntry *entry);                 // FS2 0x001dc5f0
    bool hasOverlay(int id) const;                                       // FS2 0x001dc840
    // 1 while an overlay runs, else -1.
    int  overlayMark() const;                                            // FS2 0x001dc900
    // Back to slot 0 of the normal state, dropping the slots; all: every one.
    void dropSlots(bool all);                                            // FS2 0x001dd180
    // Clears the slots; a live brain goes back to the normal state's.
    void resetScripts();                                                 // FS2 0x001d85d0

    // ScriptRunner's slots.
    const int *runCommand(int id, const int *code) override;             // FS2 0x001d8630
    int  getProperty(int id, int sub) override;                          // FS2 0x001daa70
    void setProperty(int id, int sub, int value) override;               // FS2 0x001daa10
    const char *getName() override;                                      // FS2 0x001dd450
    StageCharacter *getScriptCharacter() override { return m_owner; }    // FS2 0x001d85b0
    bool scriptOpenDoor() override;                                      // FS2 0x001d9f90
    bool scriptAttackBite() override { return attackBite(); }            // FS2 0x001da960
    bool scriptReload(int rounds) override { (void)rounds; return gunAction(); } // FS2 0x001da650
    void scriptUseSmoke() override { useSmoke(); }                       // FS2 0x001da110
    void scriptUseNamedItem(const char *name) override { useNamedItem(name); } // FS2 0x001da0c0
    void scriptPutItem() override { useItem850(); }                      // FS2 0x001da050
    bool scriptSetTrap() override { return setTrap(); }                  // FS2 0x001da060
    bool scriptTakeUpTrap() override { return takeUpTrap(); }            // FS2 0x001da020
    StageObject *scriptTakeItem() override { return takeItem(); }        // FS2 0x001da5e0
    bool scriptCloseDoor() override;                                     // FS2 0x001d9f50
    bool scriptUseDoorKey(int lock) override;                            // FS2 0x001d9ee0
    void scriptPlayMotion(const char *name) override;                    // FS2 0x001da410
    ScriptRunner *getForwardObject() override;                           // FS2 0x001da9d0
    ScriptRunner *getForwardTarget() override;                           // FS2 0x001da9f0

    // The runner's slots no native names yet: plain members until one does.
    StageCharacter *getOwnerCharacter() { return m_owner; }              // FS2 0x001d85c0
    void walkToPoint700(bool run);                                       // FS2 0x001d9fc0
    bool attackBite();                                                   // FS2 0x001da960
    bool swingAtBite();                                                  // FS2 0x001da910
    bool swingAt(StageObject *target);                                   // FS2 0x001da8f0
    bool fireAtBite(const char *flags, int frames30);                    // FS2 0x001da7a0
    bool fireAt(StageObject *target, const char *flags, int frames30);   // FS2 0x001da660
    bool shout(float loudness);                                          // FS2 0x001da9b0
    bool gunAction();                                                    // FS2 0x001da650
    bool routeAction12c();                                               // FS2 0x001d9ec0
    bool routeAction130();                                               // FS2 0x001d9ea0
    void useSmoke();                                                     // FS2 0x001da110
    void useNamedItem(const char *name);                                 // FS2 0x001da0c0
    void useItemAt(const char *name, const float *point);                // FS2 0x001da0b0
    bool setTrap();                                                      // FS2 0x001da060
    void useItem850();                                                   // FS2 0x001da050
    bool takeUpTrap();                                                   // FS2 0x001da020
    StageObject *takeItem();                                             // FS2 0x001da5e0
    void crouch3();                                                      // FS2 0x001da3c0
    void crouch1();                                                      // FS2 0x001da370
    bool dragOutCar();                                                   // FS2 0x001da5d0
    bool useSpecial(const char *name);                                   // FS2 0x001da2d0
    bool routeActionKind11();                                            // FS2 0x001da4f0
    bool useNumLock(int which);                                          // FS2 0x001da430
    int  msToFrames(int ms);                                             // FS2 0x001dd400

    // The same, and when they start something the command that was waiting
    // runs again. The character's way into each.
    bool askUseSpecial(const char *name);                                // FS2 0x001dac60
    bool askNumLock(int which);                                          // FS2 0x001dad30
    bool askRouteActionKind11();                                         // FS2 0x001dae00
    bool askDragOutCar();                                                // FS2 0x001daed0
    bool askTakeItem();                                                  // FS2 0x001dafa0
    bool askGunAction();                                                 // FS2 0x001db060
    bool askRouteAction130();                                            // FS2 0x001db130
    bool askRouteAction12c();                                            // FS2 0x001db200
    bool askDoorKey(int lock);                                           // FS2 0x001db2d0
    bool askCloseDoor();                                                 // FS2 0x001db3a0
    bool askOpenDoor();                                                  // FS2 0x001db470
    bool askTakeUpTrap();                                                // FS2 0x001db540
    bool askSetTrap();                                                   // FS2 0x001db610
    bool askNothing() { return false; }                                  // FS2 0x001db6e0
    bool askRoute22ab90(void *a, void *b, bool retry);                   // FS2 0x001db6f0
    bool askFireAt(StageObject *target, const char *flags, int frames30); // FS2 0x001db7c0
    bool askFireAtBite(const char *flags, int frames30);                 // FS2 0x001db890
    bool askSwingAt(StageObject *target);                                // FS2 0x001db960
    bool askSwingAtBite();                                               // FS2 0x001dba30
    bool askAttackBite();                                                // FS2 0x001dbb00
    bool askShout(float loudness);                                       // FS2 0x001dbbd0

    // The route, for the character.
    void allowPath(const char *name, bool on);                           // FS2 0x001daab0
    void stopAndRetry();                                                 // FS2 0x001daac0
    void playActionAndRetry(const char *name);                           // FS2 0x001dab80
    bool getMoveGoal(float *out);                                        // FS2 0x001dac50
    bool stopIfMoving();                                                 // FS2 0x001da170
    void walkToAndRetry(const float *point, bool run);                   // FS2 0x001da1b0

    int getState() const { return m_state; }
    CharacterRoute *getRoute() const { return m_route; }

private:
    // An overlay stays until its end; a kept slot stays when the state drops.
    static bool isOverlaySlot(int slot);        // inline in the ROM
    static bool isKeptSlot(int slot);           // inline in the ROM
    void saveSlot();                            // inline in the ROM: the running slot, off the runner
    void loadSlot();                            // inline in the ROM: and back
    void restartLook();                         // inline in the ROM
    void stopRoute();                           // inline in the ROM: the wait's route, its wait cleared
    void retryCommand();                        // inline in the ROM: the waiting command, again
    bool retryIf(bool started);                 // inline in the ROM: the tail of the ask* family
    void suspend();                             // FS2 0x001dca60
    void switchSlot(int slot);                  // FS2 0x001dccb0
    void fallToUsedSlot();                      // inline in the ROM
    void waitFrames(int frames30);              // inline in the ROM
    void waitIf(bool started);                  // inline in the ROM
    void startSubScript(int entry);             // inline in the ROM

    BrainSubRunner  m_sub;                      // FS2 +0x0b8: its parent is this
    ScriptRunner   *m_subRunner = nullptr;      // FS2 +0x170: &m_sub while a command's sub-script runs
    u_char          m_enabled = 1;              // FS2 +0x174: setPlayer turns the player's off
    StageCharacter *m_owner;                    // FS2 +0x178
    int             m_state = BrainState_Off;   // FS2 +0x17c: BrainState
    CharacterRoute *m_route = nullptr;          // FS2 +0x180
    BrainWait      *m_wait = nullptr;           // FS2 +0x184
    u_char          m_retryValid = 0;           // FS2 +0x188: runCommand's copy below is good
    int             m_retrySp = 0;              // FS2 +0x18c
    int             m_retryStack[kScriptStackSize] = {}; // FS2 +0x190
    const int      *m_retryPc = nullptr;        // FS2 +0x1d0: the command's own word
    BrainLook       m_look;                     // FS2 +0x1e0
    int             m_slot = kBrainNoSlot;      // FS2 +0x210
    u_char          m_slotUsed[kBrainSlots] = {}; // FS2 +0x214
    BrainFrame      m_frames[kBrainSlots];      // FS2 +0x220
    int             m_resumeState = BrainState_Down; // FS2 +0x9a0: revive's
    int             m_resumeSlot = kBrainNoSlot;     // FS2 +0x9a4
};

#endif
