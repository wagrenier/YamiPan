/* ==========================================================================
 *  game/characterinput.h
 *
 *  The pad to the character (0x236b90..0x23a870): the character's own pad
 *  (CharacterPad, a GamePad that blanks what the character may not use) and
 *  CharacterInput (StageCharacter +0x21c), which turns that pad into the
 *  frame's CharacterWants: stick to walk / turn / strafe, the action buttons
 *  through GameOptions::buttonMap, the aim, the view, the car.
 *
 *  Only the character the scene has put on the pad is active
 *  (StageScene::setPlayer, setActive): the others keep empty wants and their
 *  brain fills them.
 * ======================================================================== */
#ifndef _GAME_CHARACTERINPUT_H
#define _GAME_CHARACTERINPUT_H

#include "game/gamesystem.h"
#include "sdk/libvu0.h"
#include "sdk/scetypes.h"

class StageCharacter;
class StageObject;

// The actions of GameOptions::buttonMap (ButtonMap::button's index), named
// for what CharacterInput does with them on foot, the default button after
// each. A car reads Ready, Zoom and Squat as well (stepCar).
enum PadAction
{
    PadAction_Use    = 0,       // Cross: the item list, the first trigger
    PadAction_Squat  = 1,       // Circle: also a firearm's second trigger
    PadAction_Unused = 2,       // Triangle: nothing here
    PadAction_Lamp   = 3,       // Square
    PadAction_Ready  = 4,       // R1: held, the stick's bands widen (wants.ready)
    PadAction_Zoom   = 5,       // R2: held, with its pressure
    PadAction_Strafe = 6,       // L1: held, the stick strafes and does not turn
    PadAction_Alt    = 7,       // L2: the alternate action, or the party
};

// UiShowFlags::flag: what a script lets the pad do, on foot and in a car.
// 1 from the constructor but ForceView. Pause and map menu are the bytes 1
// and 2 (game/stagescene.cpp).
enum ControlFlag
{
    Control_Walk       = 0,
    Control_Run        = 5,
    Control_Ready      = 6,
    Control_Lamp       = 7,
    Control_Squat      = 8,
    Control_Alt        = 9,     // also lets R3 through to a party member
    Control_ForceView  = 10,    // 0 from the constructor: forcedViewChange
    Control_CarDrive   = 0xb,
    Control_CarLights  = 0xc,
    Control_CarHorn    = 0xd,
    Control_Companion  = 0xe,   // R3 and the companion
    Control_LeftStick  = 0xf,
    Control_RightStick = 0x10,
};

// CharacterWants::fire: what readFire says of a trigger.
enum CharacterFire
{
    CharacterFire_None = 0,
    CharacterFire_Tap  = 1,     // let go early
    CharacterFire_Hold = 2,     // held kFireHoldFrames
    CharacterFire_Auto = 4,     // a firearm's first trigger
};

// The last kStickHistory frames of the left stick, newest first: a bit per
// direction past kStickFull. The quick turn reads it.
constexpr int kStickHistory = 0x20;
enum StickBit
{
    StickBit_Up    = 1,
    StickBit_Down  = 2,
    StickBit_Left  = 4,
    StickBit_Right = 8,
};
// layout: host -- EE 8: a new[] block and its size.
struct StickHistory
{
    StickHistory();
    ~StickHistory();
    void push(u_char bits);

    u_char *data;               // FS2 +0x0
    int     count;              // FS2 +0x4
};

// vtable 0x3164f8 (one slot, EE vptr at +0x120 as GamePad's). The EE adds
// the owner at +0x178, size 0x17c.
// layout: host
class CharacterPad : public GamePad
{
public:
    explicit CharacterPad(StageCharacter *owner) : m_owner(owner) {}     // inline in CharacterInput's ctor

    // A read, then what the owner may not use is cleared: everything under
    // a menu, a game over or an event, the Use button while something else
    // has the focus. Not 0 when anything was read or cleared.
    int update() override;                                               // FS2 0x00239d50

    // Through the options' map: inline at every use in the ROM.
    bool actionHeld(int action) const;
    bool actionPressed(int action) const;
    u_char actionPressure(int action) const;

private:
    void clearButton(int button);       // inline in update, 16 times over

    StageCharacter *m_owner;    // FS2 +0x178
};

// What drives a character from the pad (StageCharacter +0x21c).
// layout: host -- pointers; EE 0x60, offsets in the comments.
struct CharacterInput
{
    explicit CharacterInput(StageObject *owner);                         // FS2 0x00239c60
    ~CharacterInput();                                                   // FS2 0x00239be0: deleting
    void reset();                                                        // FS2 0x00239bc0
    void setActive(bool on);                                             // FS2 0x00239bb0
    // The control hook's first step: the wants from the pad.
    void update();                                                       // FS2 0x00236b90

    // The left stick's x squared to the circle: the main camera leans by it
    // while a swaying motion plays.
    float padSway() const;                                               // FS2 +0x8 -> +0x12c
    // The push on the view, -1..1, from the right stick (the left one's y
    // in an aim); the camera reads them. Their signs say which way they turn it
    // (StageMainCamera::cameraStep).
    float lookPitch() const { return m_look[0]; }                        // FS2 +0x10
    float lookYaw() const { return m_look[2]; }                          // FS2 +0x18

private:
    // The sections of update, one function in the ROM.
    void stepCar();
    void stepOnFoot(sceVu0FVECTOR lookTarget);
    void stepMove(float angle, float length);
    void stepQuickTurn();
    void stepAlt();
    void stepItems();
    void stepFire();
    void stepPadExtras();
    void stepHead(sceVu0FVECTOR lookTarget);
    void startScope();
    void stepAim(float length, sceVu0FVECTOR lookTarget);
    void stepScope(float length, sceVu0FVECTOR lookTarget);
    void stepView();
    void stepCompanion();

    // 0 nothing, else a CharacterFire of button: a tap or a hold with an
    // automatic weapon, a press without one.
    int  readFire(int button);                                           // FS2 0x00239a90
    // The first weapon fires while held (the abilities' +0x3c6).
    u_char hasAutoFire() const;                                          // FS2 0x00239b90

    u_char          m_active = 0;       // FS2 +0x00
    StageCharacter *m_owner;            // FS2 +0x04
    CharacterPad   *m_pad;              // FS2 +0x08
    StickHistory   *m_history;          // FS2 +0x0c
    sceVu0FVECTOR   m_look = {};        // FS2 +0x10: eases to the frame's target; [0] pitch, [2] yaw
    u_char m_stickLatch = 0;            // FS2 +0x20: an aim ended: the stick aims again once let go
    // The scope (PlayerMotion::noFlash): not set by the ctor in the ROM.
    float  m_scopePitch = 0.0f;         // FS2 +0x24: where the stick has it, -1..1
    float  m_scopeYaw = 0.0f;           // FS2 +0x28
    float  m_scopePitchNow = 0.0f;      // FS2 +0x2c: eased after them by the sheet's 狙撃補間率
    float  m_scopeYawNow = 0.0f;        // FS2 +0x30
    float  m_swayAmount = 0.0f;         // FS2 +0x34: kSwayRest..1
    float  m_recoil = 0.0f;             // FS2 +0x38: a shot's kick, wearing off
    float  m_swayPhase[2] = {};         // FS2 +0x3c: pitch, yaw
    float  m_swayStep[2] = {};          // FS2 +0x44: a frame's, rolled on each start
    u_char m_scopeWas = 0;              // FS2 +0x4c: last frame's noFlash
    int    m_fireButton = -1;           // FS2 +0x50: the button readFire counts
    u_int  m_fireFrames = 0;            // FS2 +0x54: held so far
};

#endif /* _GAME_CHARACTERINPUT_H */
