/*
 *  game/playermotion.h -- what a character is doing with its body (FS2,
 *  0x1e0000..0x1f0000, the object at StageCharacter +0x224, EE 0xc20).
 *
 *  Each frame something asks for a motion by its gMotionDefs index
 *  (request), and stepLower decides: the request may wait for a hold, for
 *  the walk or for the pair's other half, and it may need a stand-up or
 *  crouch-down motion first. stepUpper keeps the hands, stepFace the face.
 *  A motion of the sheet may name a link function (MotionLink: Walk, Jack,
 *  Ladder, ...), the code that runs it; the conditions (Always, Squat, ...)
 *  are what the links test.
 *
 *  Not written yet: the links, the conditions and the list 0x1df0b0 fills,
 *  so findLink answers null, which the ROM takes as a motion with no link
 *  function.
 */
#ifndef GAME_PLAYERMOTION_H
#define GAME_PLAYERMOTION_H

#include <string.h>

#include <map>

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
    Motion_CrouchWalk = 0x2c, // sqw_w
    Motion_CrouchIdle = 0x2b, // sq_idle0
    Motion_StandUp    = 0x27, // sq_u: from a crouch, before what was asked (arbitrate)
    Motion_CrouchDown = 0x28, // sq_d
    Motion_Pair       = 0x32, // u_sp: two characters together: both must arrive
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
    MotionStance_Run     = 5,  // run, ka
    MotionStance_WalkRun = 10, // w_run, sq_ka: walk into run
};

// The groups of the sheet, as the code tests them (work out a label with
// the table in architecture.md "Motions").
bool isCrouchMotion(int id);
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

// A motion's own code. Every slot of the base (vtable 0x317c20) is empty.
// layout: host -- EE: +0 the character, +4 the motion, +8 a helper, +0xc a
// condition list, +0x14 finished, +0x18 a counter, +0x1c the vptr.
class MotionLink
{
public:
    virtual ~MotionLink() {}
    virtual void onUpdate() {}                                           // FS2 slot +0xc
    virtual void onRequest() {}                                          // FS2 slot +0x14
    virtual void onStart() {}                                            // FS2 slot +0x18
    virtual void onLeave(MotionLink *next) {}                            // FS2 slot +0x20
    virtual void onCancel() {}                                           // FS2 slot +0x24
    // FS2 0x0027cab0
    void rewind()
    {
        finished = 0;
        counter = 0;
    }
    void update();                                                       // FS2 0x0027cb60

    u_char finished = 0;        // FS2 +0x14
    int    counter = 0;         // FS2 +0x18
};

// A byte-ordered name key, as the MSL maps at +0x8, +0x18 and +0x2c.
// layout: host
struct MotionNameLess
{
    bool operator()(const char *a, const char *b) const { return strcmp(a, b) < 0; }
};

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
    void initSlots14();                                                  // FS2 0x001df040
    void initSlots18();                                                  // FS2 0x001defd0
    // Swaps the weapons; a motion that looks different now starts again.
    void refresh();                                                      // FS2 0x001e66c0
    // The matrix of the lamp the player aims: at bn_head, turned as the
    // player and the motion are.
    void getLampMatrix(sceVu0FMATRIX out);                               // FS2 0x001e60b0

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
    void startRequested();                                               // FS2 0x001ec400
    // Replaces id by the hold it falls to: 1 done, 0 no hold, -1 the holds
    // no longer apply.
    int  resolveHold(int *out, int id) const;                            // FS2 0x001e8320
    // Stand-up, crouch-down and the pair's wait; true: the request moved.
    bool arbitrate();                                                    // FS2 0x001e6d60
    // The end of the motion that is leaving.
    void noteLeaving();                                                  // FS2 0x001ec0a0
    // The list's half of noFlash (the list is not written).
    void clearNoFlash(int arg);                                          // FS2 0x001dfd90
    void startWalk(int kind, const float *from, const float *to, bool arg, bool arg2); // FS2 0x001ead00
    void startWalkAt(int kind, void *target, const float *offset, const float *to, bool arg, bool arg2); // FS2 0x001eaea0

public:
    StageObject *owner;         // FS2 +0x000
    // +0x004: a small object (0x26c6b0) with { 0, 1, 0 }, unread.
    // +0x008: the conditions by name; +0x028 Always, stepped each frame.
    // +0x02c: the list 0x1df0b0 fills.
    std::map<const char *, MotionLink *, MotionNameLess> m_links; // FS2 +0x018
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
    MotionLink *activeLink = nullptr; // FS2 +0x068: the running motion's; the main camera reacts when this is look
    int    m_weaponClass = 0;   // FS2 +0x06c: which of rifleLabelText
    const char *m_faces[kMotionDefCount] = {}; // FS2 +0x070
    u_char m_handsBusy[kMotionDefCount][2] = {}; // FS2 +0x470: left, right: the motion moves the fingers itself
    float  m_endHeight[kMotionDefCount] = {}; // FS2 +0x670: the first track's last key, third value
    u_char m_walking = 0;       // FS2 +0xa70: the pose walks to a point
    u_char m_walkHeld = 0;      // FS2 +0xa71
    u_char m_walkPending = 0;   // FS2 +0xa72: startWalk at the next stepLower
    u_char m_walkToTarget = 0;  // FS2 +0xa73: startWalkAt instead
    int    m_walkKind = 0;      // FS2 +0xa74
    u_char m_walkArg = 0;       // FS2 +0xa78
    u_char m_walkArg2 = 0;      // FS2 +0xa79
    u_char m_hasWalkFrom = 0;   // FS2 +0xa80
    sceVu0FVECTOR m_walkFrom = {}; // FS2 +0xa90
    u_char m_hasWalkTo = 0;     // FS2 +0xaa0
    sceVu0FVECTOR m_walkTo = {}; // FS2 +0xab0
    void  *m_walkTarget = nullptr; // FS2 +0xac0
    sceVu0FVECTOR m_walkOffset = {}; // FS2 +0xad0
    u_char m_hasWalkEnd = 0;    // FS2 +0xae0
    sceVu0FVECTOR m_walkEnd = {}; // FS2 +0xaf0
    float  m_motionTime = 0.0f; // FS2 +0xb00: seconds in this motion
    float  m_b04 = 0.0f;        // FS2 +0xb04
    float  m_b08 = 0.0f;        // FS2 +0xb08
    sceVu0FVECTOR m_lastPosition = {}; // FS2 +0xb10: the character's at reset
    u_char m_stanceFlag = 0;    // FS2 +0xb20: pickStance's third table
    float  m_b24 = 0.0f;        // FS2 +0xb24
    float  m_b28 = 0.0f;        // FS2 +0xb28
    float  m_b2c = 0.0f;        // FS2 +0xb2c: 0 when a motion starts or ends
    u_char m_b30 = 0;           // FS2 +0xb30
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
    u_char m_stepsOn[4] = {};   // FS2 +0xb50: [2] stepUpper, [3] stepFace run; reset sets them
    u_char m_b54 = 0;           // FS2 +0xb54
    int    m_modelFamily = -1;  // FS2 +0xb58: gMotionModels index
    sceVu0FVECTOR m_b60[2] = {}; // FS2 +0xb60
    sceVu0FVECTOR m_lampAngles = {}; // FS2 +0xb80: getLampMatrix's
    sceVu0FVECTOR m_b90 = {};   // FS2 +0xb90
    u_char m_ba0 = 0;           // FS2 +0xba0
    float  m_ba4 = 0.0f;        // FS2 +0xba4
    float  m_ba8 = 0.0f;        // FS2 +0xba8: a countdown
    u_char m_wallCheck = 0;     // FS2 +0xbad: the motion's 壁判定
    u_char m_bae = 0;           // FS2 +0xbae
    sceVu0FVECTOR position = {}; // FS2 +0xbb0
    float  m_bd0 = 0.0f;        // FS2 +0xbd0
    u_char noFlash = 0;         // FS2 +0xbd4: a shot shows no flash (StageWeaponItem::fire)
    PlayerOverlay *overlay = nullptr; // FS2 +0xbd8: StageScene::draw
    float  m_bf0 = 0.0f;        // FS2 +0xbf0
    float  m_bf4 = 0.0f;        // FS2 +0xbf4
    u_char m_bfc = 0;           // FS2 +0xbfc
    PlayerLookState *look;      // FS2 +0xc00
};

#endif // GAME_PLAYERMOTION_H
