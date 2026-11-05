/* ==========================================================================
 *  game/characterinput.cpp
 *
 *  The pad to the character 0x236b90..0x23a870. CharacterInput::update is
 *  one function of 0x2f00 bytes in the ROM, here cut into its sections, and
 *  the input half of the control hook 0x17a810 (the brain is the other).
 *
 *  What each want does is the running motion's conditions' business
 *  (game/playermotion.cpp): this file asks, m_allows says what may be asked.
 *  Doubts: the file name is the port's, and every want and allow named here
 *  that no condition reads yet (useItem*, ready, fire, the pad extras) is
 *  named from its button only.
 * ======================================================================== */
#include "game/characterinput.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/mathutil.h"
#include "game/packtables.h"
#include "game/playermotion.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
#include "game/stagesound.h"
#include "game/sysdata.h"
#include "game/thread.h"
#include "game/uiwidget.h"



// getActionState's: at the wheel of a car.
constexpr int kActionDriving = 9;
// PlayerMotion::motionId: seated at the wheel.
constexpr int kMotionDriveA = 0xb9;
constexpr int kMotionDriveB = 0xba;
// A grab: the left stick shakes it off (m_bf0).
constexpr int kMotionHeldA = 0xa0;
constexpr int kMotionHeldB = 0xa1;
constexpr int kMotionHeldC = 0x9b;

// The pad words' bits used by name.
constexpr u_int kPadR3    = 1u << PadBit_R3;
constexpr u_int kPadUp    = 1u << PadBit_Up;
constexpr u_int kPadRight = 1u << PadBit_Right;
constexpr u_int kPadDown  = 1u << PadBit_Down;
constexpr u_int kPadLeft  = 1u << PadBit_Left;
constexpr u_int kPadLStick = 1u << PadBit_LStickUp | 1u << PadBit_LStickRight
                           | 1u << PadBit_LStickDown | 1u << PadBit_LStickLeft;

// CharacterPad::update: what is blanked.
enum PadClear
{
    PadClear_None  = 0,
    PadClear_All   = 1,
    PadClear_ButR3 = 2,
};
constexpr int kPadButtons = 16;         // libpad's: the stick bits are left alone

constexpr float kPi         = 3.1415927f;
constexpr float kHalfPi     = 1.5707964f;
constexpr float kFrameTime  = 0.033333335f;     // the rates are per 1/30 s
constexpr float kStickFull  = 0.95f;            // a direction in the history
constexpr float kRunLength  = 0.7f;             // the stick's length that runs
constexpr float kFlickTime  = 0.12f;            // seconds, for a quick turn
constexpr float kLetGo      = 0.1f;             // the stick is back
constexpr float kLookEase   = 0.3f;

// How wide the sideways bands are around the stick's 90 degrees.
constexpr float kBandForward     = 0.17453292f;         // 10 degrees
constexpr float kBandBack        = 0.5235988f;          // 30
constexpr float kBandReady       = 1.3962634f;          // 80
constexpr float kBandReadyStrafe = 0.7853982f;          // 45

constexpr float kHeadRange    = 1.134464f;      // 65 degrees
constexpr float kAimRange     = 1.0471976f;     // 60 degrees
constexpr float kCarHeadPitch = 0.2617994f;     // 15 degrees
constexpr float kCarHeadYaw   = kHalfPi;        // 90 degrees

constexpr u_int kFireHoldFrames = 6;
constexpr float kScopeSpeed     = 0.015f;       // data 0x31aab0
constexpr float kSwayRest       = 0.15f;        // data 0x31aab4
constexpr float kRecoilSway     = 0.11111111f;  // a shot adds to the sway
constexpr float kRecoilKick     = 0.5f;
constexpr float kScopeNear      = 350.0f;
constexpr int   kSwayFrames     = 50;           // a sway's half turn, in frames ..
constexpr int   kSwaySpread     = 40;           // .. and a random part

// The stands and walks of an aim: there the stick aims the upper body. The
// ROM's two chains of compares.
static bool isAimStanceMotion(int id)
{
    switch (id) {
    case 0x90: case 0x81: case 0x72: case 0x50: case 0x4f:
    case 0x8f: case 0x8e: case 0x80: case 0x7f: case 0x71: case 0x70: case 0x58: case 0x57: case 0x56: case 0x55:
        return true;
    }
    return false;
}

// (int)(1 / seconds). PC: at seconds 0 (a paused clock) the EE divides to
// +MAX and cvt.w.s gives INT_MAX; the host's conversion is undefined.
static int framesPerSecond(float seconds)
{
    return seconds > 0.0f ? (int)(1.0f / seconds) : INT_MAX;
}

static float rollSwayStep()
{
    return kPi / ((float)(int)(randNext() % kSwaySpread) + (float)kSwayFrames);
}

// ──────────────────────────────────────────────────────────────────────
// StickHistory: inline in the ROM (CharacterInput's ctor, update's loop).

StickHistory::StickHistory()
{
    count = kStickHistory;
    data = new u_char[count];
    memset(data, 0, count);
}

StickHistory::~StickHistory()
{
    delete[] data;
}

void StickHistory::push(u_char bits)
{
    memmove(data + 1, data, count - 1);
    data[0] = bits;
}

// ──────────────────────────────────────────────────────────────────────

bool CharacterPad::actionHeld(int action) const
{
    return (m_held & bitMask(actionButton(action))) != 0;
}

bool CharacterPad::actionPressed(int action) const
{
    return (m_pressed & bitMask(actionButton(action))) != 0;
}

u_char CharacterPad::actionPressure(int action) const
{
    return m_pressure[actionButton(action)];
}

void CharacterPad::clearButton(int button)
{
    const u_int keep = ~(1u << button);
    m_previous &= keep;
    m_held &= keep;
    m_pressed &= keep;
    m_released &= keep;
    m_repeat &= keep;
    m_pressure[button] = 0;
    m_repeatState[button].count = 0;
}

// FS2 0x00239d50
// callcheck: read, isControlled, getOpenMenu, getPlayerAltFlag, isPlayer, getInputFocus, computeSticks

int CharacterPad::update()
{
    const int got = read();
    if (!m_owner->isControlled())
        return got;

    StageScene *scene = gStageScene;
    const UiShowFlags *flags = scene->showFlags;
    StageMenu *menu = scene->menu;
    bool leftOff = flags->flag[Control_LeftStick] == 0;
    bool rightOff = flags->flag[Control_RightStick] == 0;
    int clear = PadClear_None;
    int result = got;

    if (scene->getOpenMenu() == menu) {
        clear = PadClear_All;
        rightOff = true;
    } else if (scene->gameOver != nullptr) {
        leftOff = true;
        clear = PadClear_All;
        rightOff = true;
    } else if (scene->event != nullptr) {
        clear = PadClear_All;
    } else if (scene->getPlayerAltFlag() == 0) {
        if (scene->getInputFocus() != static_cast<void *>(static_cast<StageObject *>(m_owner))) {
            clearButton(actionButton(PadAction_Use));
            result = 1;
        }
    } else if (!m_owner->isPlayer() || flags->flag[Control_Alt] != 0) {
        if ((m_pressed & kPadR3) != 0) {
            leftOff = true;
            clear = PadClear_ButR3;
            rightOff = true;
        }
    } else {
        leftOff = true;
        clear = PadClear_All;
        rightOff = true;
    }

    if (clear != PadClear_None) {
        for (int button = 0; button < kPadButtons; button++) {
            if (clear != PadClear_ButR3 || button != PadBit_R3)
                clearButton(button);
        }
    }
    if (rightOff) {
        m_analog[0] = 0x80;
        m_analog[1] = 0x80;
        m_stick[0] = 0.0f;
        m_stick[1] = 0.0f;
    }
    if (leftOff) {
        m_analog[2] = 0x80;
        m_analog[3] = 0x80;
        m_stick[2] = 0.0f;
        m_stick[3] = 0.0f;
    }
    if (rightOff || leftOff)
        computeSticks();
    return clear != PadClear_None || rightOff || leftOff || result != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00239c60
// callcheck: operator_new, GamePad, getGameOptions, setButtonMap, operator_new, operator_new[], memset

CharacterInput::CharacterInput(StageObject *owner)
    : m_owner(static_cast<StageCharacter *>(owner))
{
    m_pad = new CharacterPad(m_owner);
    m_pad->setButtonMap(&getGameOptions()->buttonMap);
    m_history = new StickHistory;
}

// FS2 0x00239be0
// callcheck: ~GamePad, operator_delete[], operator_delete, operator_delete

CharacterInput::~CharacterInput()
{
    delete m_pad;
    delete m_history;
}

// FS2 0x00239bc0

void CharacterInput::reset()
{
    memset(m_look, 0, sizeof(m_look));
    m_stickLatch = 0;
    m_scopeWas = 0;
}

// FS2 0x00239bb0
// callcheck: setListening

void CharacterInput::setActive(bool on)
{
    m_active = on;
    m_pad->setListening(on);
}

float CharacterInput::padSway() const
{
    return m_pad->m_dir[2];
}

// FS2 0x00239b90

u_char CharacterInput::hasAutoFire() const
{
    return m_owner->m_weapons[0] != nullptr ? m_owner->m_abilities.flag3c6 : 0;
}

// FS2 0x00239a90
// callcheck: hasAutoFire, bitMask, bitMask, bitMask

int CharacterInput::readFire(int button)
{
    if (hasAutoFire() == 0)
        return (m_pad->m_pressed & bitMask(button)) != 0 ? CharacterFire_Tap : CharacterFire_None;

    if (button != m_fireButton) {
        m_fireButton = button;
        m_fireFrames = 0;
    }
    if ((m_pad->m_released & bitMask(button)) != 0) {
        if (m_fireFrames < kFireHoldFrames)
            return CharacterFire_Tap;
        if (m_fireFrames == kFireHoldFrames)
            return CharacterFire_Hold;
    } else if ((m_pad->m_held & bitMask(button)) != 0) {
        m_fireFrames = m_pad->m_repeatState[button].count;
        if (m_fireFrames == kFireHoldFrames)
            return CharacterFire_Hold;
    }
    return CharacterFire_None;
}

// ──────────────────────────────────────────────────────────────────────
// The stick while walking. Past 90 degrees of the stick it backs, inside the
// bands around 90 it turns on the spot or, with Strafe, strafes; the bands
// widen with Ready. A scope (noFlash) does not turn on the spot.

void CharacterInput::stepMove(float angle, float length)
{
    StageCharacter *owner = m_owner;
    CharacterWants &wants = owner->m_wants;
    const UiShowFlags *flags = gStageScene->showFlags;
    const bool strafing = m_pad->actionHeld(PadAction_Strafe);

    const bool forward = fabsf(angle) <= kHalfPi;
    const float fromSide = fabsf(fabsf(angle) - kHalfPi);
    float bandForward = kBandForward;
    float bandBack = kBandBack;
    if (wants.ready != 0)
        bandForward = bandBack = strafing ? kBandReadyStrafe : kBandReady;

    if (fromSide < (forward ? bandForward : bandBack)) {
        if (strafing) {
            if (angle > 0.0f)
                wants.moveKind = CharacterMove_StrafeLeft;
            else if (angle < 0.0f)
                wants.moveKind = CharacterMove_StrafeRight;
        } else if (owner->m_motion->noFlash == 0) {
            if (angle > 0.0f)
                wants.moveKind = CharacterMove_TurnLeft;
            else if (angle < 0.0f)
                wants.moveKind = CharacterMove_TurnRight;
        }
        return;
    }

    // The ROM tests a list of walking motions here and does the same on both
    // sides of it.
    const float frames = owner->m_clock->getDeltaSeconds() / kFrameTime;
    if (forward) {
        if (strafing) {
            wants.moveKind = CharacterMove_WalkSlow;
            return;
        }
        wants.moveYaw = (angle / kHalfPi) * frames * owner->getTurnRate();
        bool run = length >= kRunLength;
        if (run && owner->isControlled())
            run = flags->flag[Control_Run] != 0 && flags->flag[Control_Walk] != 0;
        wants.moveKind = run ? CharacterMove_Run : CharacterMove_Walk;
    } else if (strafing) {
        wants.moveKind = CharacterMove_BackTurn;
    } else {
        wants.moveKind = CharacterMove_Back;
        wants.moveYaw = -(addAngle(angle, kPi) / kHalfPi) * frames * owner->getTurnRate();
    }
}

// Down flicked and let go inside kFlickTime turns round.

void CharacterInput::stepQuickTurn()
{
    if (m_owner->isControlled() && gStageScene->showFlags->flag[Control_Walk] == 0)
        return;
    if (m_pad->actionHeld(PadAction_Strafe))
        return;
    const u_char *history = m_history->data;
    if (history[0] != 0)
        return;
    int frames = 1;
    while (frames < m_history->count && history[frames] == StickBit_Down)
        frames++;
    frames--;
    if (frames > 0 && (float)frames < kFlickTime / m_owner->m_clock->getDeltaSeconds())
        m_owner->m_wants.quickTurn = 1;
}

// The alternate action; the lead of a scenario has the party on it instead.

void CharacterInput::stepAlt()
{
    StageCharacter *owner = m_owner;
    StageScene *scene = gStageScene;
    if (!m_pad->actionPressed(PadAction_Alt)
        || (owner->isControlled() && scene->showFlags->flag[Control_Alt] == 0))
        return;
    if (!owner->isPlayer() || !owner->isScenarioLead())
        owner->m_wants.altAction = 1;
    else if (owner->getAltSoundFlag() != 0)
        scene->party->stop();
    else
        scene->party->start(owner);
}

// The Use button offers itself to the item list, once per what the motion
// allows.

void CharacterInput::stepItems()
{
    StageCharacter *owner = m_owner;
    if (m_pad->actionPressed(PadAction_Use)) {
        UiItemList *list = gStageScene->itemList;
        if (owner->m_allows.level == 1)
            owner->m_wants.useLevel = list->offer(owner, UiUse_LevelUp);
        else if (owner->m_allows.level == 2)
            owner->m_wants.useLevel = list->offer(owner, UiUse_LevelDown);
        // Both values ask for the same kind in the ROM.
        if (owner->m_allows.ladder == 1 || owner->m_allows.ladder == 2)
            owner->m_wants.useLadder = list->offer(owner, UiUse_Ladder);
        if (owner->m_allows.jump != 0)
            owner->m_wants.useJump = list->offer(owner, UiUse_Jump);
        if (owner->m_allows.useItem5 != 0)
            owner->m_wants.useItem5 = list->offer(owner, UiUse_Gate);
    }
    if (m_pad->actionPressed(PadAction_Squat) && owner->m_allows.squatAction != 0)
        owner->m_wants.squatAction = 1;
}

void CharacterInput::stepFire()
{
    StageCharacter *owner = m_owner;
    if (owner->m_allows.fire == 0)
        return;

    int action = PadAction_Use;
    const StageObject *weapon = owner->m_weapons[0];
    if (weapon != nullptr) {
        // PC: an id outside the table reads as kind 0; the ROM reads low memory.
        const int id = weapon->m_itemDef;
        const int kind = id < 0 || id >= kItemDefCount ? 0 : gItemDefs[id].kind;
        if (ItemKind_FirearmFirst <= kind && kind <= ItemKind_FirearmLast) {
            if ((m_pad->actionHeld(PadAction_Use) && weapon->m_ammoA > 0)
                || m_pad->actionPressed(PadAction_Use)) {
                owner->m_wants.fire = CharacterFire_Auto;
                return;
            }
            action = PadAction_Squat;
        }
    }
    const int fire = readFire(m_pad->actionButton(action));
    if (fire == CharacterFire_Hold || fire == CharacterFire_Tap)
        owner->m_wants.fire = fire;
}

// The d-pad and the right stick's y, for what the motion allows, and a grab
// shaken off by the left stick.

void CharacterInput::stepPadExtras()
{
    StageCharacter *owner = m_owner;
    CharacterPad *pad = m_pad;
    CharacterWants &wants = owner->m_wants;
    const CharacterAllows &allows = owner->m_allows;

    if ((pad->m_pressed & kPadLeft) != 0)
        wants.dpadSide = 1;
    else if ((pad->m_pressed & kPadRight) != 0)
        wants.dpadSide = 2;
    if (allows.padUp != 0 && (pad->m_pressed & kPadUp) != 0)
        wants.padUp = 1;
    if (allows.rightStick != 0) {
        wants.rightStickOn = 1;
        wants.rightStickY = pad->m_stick[1];
    }
    if (allows.padDown != 0 && (pad->m_pressed & kPadDown) != 0)
        wants.padDown = 1;

    PlayerMotion *motion = owner->m_motion;
    const int id = motion->motionId;
    if ((id == kMotionHeldA || id == kMotionHeldB || id == kMotionHeldC) && (pad->m_pressed & kPadLStick) != 0)
        motion->m_bf0 -= owner->m_clock->getDeltaSeconds();
}

// The right stick turns the head and pushes the view.

void CharacterInput::stepHead(sceVu0FVECTOR lookTarget)
{
    StageCharacter *owner = m_owner;
    CharacterWants &wants = owner->m_wants;
    if (owner->m_allows.head == 0 || m_pad->m_length[0] <= 0.0f)
        return;
    const float pitch = -m_pad->m_stick[1];
    const float yaw = -m_pad->m_stick[0];
    wants.headOn = 1;
    wants.headAngles[0] = pitch * kHeadRange;
    wants.headAngles[2] = yaw * kHeadRange;
    lookTarget[0] = pitch;
    lookTarget[2] = yaw;
    const GameOptions *options = getGameOptions();
    if (options->invertY != 0) {
        wants.headAngles[0] *= -1.0f;
        lookTarget[0] *= -1.0f;
    }
    if (options->invertX != 0) {
        wants.headAngles[2] *= -1.0f;
        lookTarget[2] *= -1.0f;
    }
}

// A scope comes up: it starts where the stick and the character are.
// callcheck: randNext, randNext

void CharacterInput::startScope()
{
    m_scopePitch = m_pad->m_stick[3];
    m_scopeYaw = m_owner->m_xform.rotation[2];
    m_scopePitchNow = m_scopePitch;
    m_scopeYawNow = m_scopeYaw;
    m_swayAmount = kSwayRest;
    m_recoil = 0.0f;
    m_swayPhase[0] = 0.0f;
    m_swayPhase[1] = 0.0f;
    m_swayStep[0] = rollSwayStep();
    m_swayStep[1] = rollSwayStep();
}

// The left stick moves the aim, and the sheet's 狙撃ブレ率 and 狙撃補間率
// shake it and ease it.
// callcheck: getStageSelect, getStageSelect, getTopView, getTopView, addAngle, getDeltaSeconds, getDeltaSeconds, randNext, randNext, getDeltaSeconds, getDeltaSeconds, wrapAngle, wrapAngle, wrapAngle, wrapAngle, sinCos, sinCos, wrapAngle (stageObjRecord)

void CharacterInput::stepScope(float length, sceVu0FVECTOR lookTarget)
{
    StageCharacter *owner = m_owner;
    CharacterPad *pad = m_pad;
    PlayerMotion *motion = owner->m_motion;
    const StageObjRecord *sheet = stageObjRecord(owner->m_itemDef);
    const float ease = (float)sheet->snipeLerp / 100.0f;
    const float shake = (float)sheet->snipeShake / 1000.0f;

    if (!pad->actionHeld(PadAction_Strafe) && isAimStanceMotion(motion->motionId)) {
        // Slower with the view zoomed in (the camera's two ranges).
        StageObject *view = gStageScene->getTopView();
        const float speed = (1.0f - view->m_range2 / (view->m_range - kScopeNear)) * kScopeSpeed * 2.0f + kScopeSpeed;
        float pitch = speed * pad->m_stick[3] + m_scopePitch;
        if (pitch <= -1.0f)
            pitch = -1.0f;
        m_scopePitch = pitch < 1.0f ? pitch : 1.0f;
        m_scopeYaw = addAngle(m_scopeYaw, -(speed * pad->m_stick[2]) * kAimRange);
    }

    const bool shooting = isAttackMotion(motion->motionId);
    if (shooting && motion->m_motionTime <= owner->m_clock->getDeltaSeconds() * 3.0f) {
        m_recoil += kRecoilKick;
        m_swayAmount += kRecoilSway;
        if (m_swayAmount >= 1.0f)
            m_swayAmount = 1.0f;
    }

    const int rate = framesPerSecond(owner->m_clock->getDeltaSeconds());
    m_recoil -= 1.0f / (float)(rate / 2);
    if (m_recoil <= 0.0f)
        m_recoil = 0.0f;

    if (length > 0.0f && !shooting) {
        if (m_swayAmount == kSwayRest) {
            m_swayStep[0] = rollSwayStep();
            m_swayStep[1] = rollSwayStep();
        }
        // EE: the shift wraps as a 32-bit int.
        m_swayAmount += 1.0f / (float)(int)((u_int)rate << 1);
        if (m_swayAmount >= 1.0f)
            m_swayAmount = 1.0f;
        if (m_recoil > 0.0f) {
            m_scopePitch += m_recoil * shake;
            m_scopePitchNow += m_recoil * shake;
            m_recoil = 0.0f;
        }
    } else {
        m_swayAmount -= 1.0f / (float)(int)((u_int)rate << 3);
        if (m_swayAmount <= kSwayRest)
            m_swayAmount = kSwayRest;
    }
    m_swayPhase[0] = wrapAngle(m_swayPhase[0] + m_swayStep[0]);
    m_swayPhase[1] = wrapAngle(m_swayPhase[1] + m_swayStep[1]);

    if (shooting) {
        m_scopePitch = m_scopePitchNow;
        m_scopeYaw = m_scopeYawNow;
    } else {
        m_scopePitchNow += ease * (m_scopePitch - m_scopePitchNow);
        m_scopeYawNow = wrapAngle(ease * wrapAngle(m_scopeYaw - m_scopeYawNow) + m_scopeYawNow);
    }

    CharacterWants &wants = owner->m_wants;
    wants.aimAngles[0] = -(shake * (m_swayAmount * sinCos(m_swayPhase[0], SinCos_Sin) + m_recoil) + m_scopePitchNow) * kAimRange;
    wants.aimAngles[2] = -(shake * m_swayAmount * sinCos(m_swayPhase[1], SinCos_Cos)) * kAimRange;
    wants.moveYaw = wrapAngle(m_scopeYawNow - owner->m_xform.rotation[2]);
    lookTarget[0] = -m_scopePitchNow;
}

// The upper body follows the left stick in an aim, unless a scope has it.

void CharacterInput::stepAim(float length, sceVu0FVECTOR lookTarget)
{
    StageCharacter *owner = m_owner;
    PlayerMotion *motion = owner->m_motion;
    if (owner->m_allows.aim == 0) {
        if (owner->m_290 != nullptr)
            m_stickLatch = 1;
        else if (!isArmedMotion(motion->motionId))
            m_stickLatch = 0;
        return;
    }

    owner->m_wants.aimOn = 1;
    if (m_stickLatch != 0 && length < kLetGo)
        m_stickLatch = 0;

    if (motion->noFlash != 0) {
        stepScope(length, lookTarget);
    } else if (!m_pad->actionHeld(PadAction_Strafe) && isAimStanceMotion(motion->motionId) && m_stickLatch == 0) {
        owner->m_wants.aimAngles[0] = -m_pad->m_stick[3] * kAimRange;
        lookTarget[0] = -m_pad->m_stick[3];
    }

    if (getGameOptions()->invertY != 0) {
        owner->m_wants.aimAngles[0] *= -1.0f;
        lookTarget[0] *= -1.0f;
    }
}

// In and out of the view camera: up and down of the d-pad, or R3 from the
// subjective view; a script can force it.
// callcheck: forcedViewChange, getTopView, getTopView

void CharacterInput::stepView()
{
    StageCharacter *owner = m_owner;
    CharacterWants &wants = owner->m_wants;
    if (owner->m_allows.viewSwitch != 0) {
        const int forced = owner->forcedViewChange();
        if (forced != 0) {
            wants.viewChange = forced;
        } else if (owner->m_allows.padUp == 0) {
            if ((m_pad->m_pressed & kPadUp) != 0)
                wants.viewChange = 1;
            if ((m_pad->m_pressed & kPadDown) != 0)
                wants.viewChange = 2;
        } else if ((m_pad->m_pressed & kPadR3) != 0) {
            const int kind = gStageScene->getTopView()->getKind();
            if (kind == StageObject_Kind9)
                wants.viewChange = 1;
            else if (kind == StageObject_Kind10)
                wants.viewChange = 2;
        }
    }
    if (owner->m_allows.zoom != 0 && m_pad->actionHeld(PadAction_Zoom)) {
        wants.zoomOn = 1;
        wants.zoomLevel = m_pad->actionPressure(PadAction_Zoom);
    }
}

// On foot. The party's run on the scenario's lead (getAltSoundFlag) has the
// pad from the lamp on.

void CharacterInput::stepOnFoot(sceVu0FVECTOR lookTarget)
{
    StageCharacter *owner = m_owner;
    CharacterPad *pad = m_pad;
    const UiShowFlags *flags = gStageScene->showFlags;
    const float angle = pad->m_angle[3];
    const float length = pad->m_length[2];

    if (pad->actionPressed(PadAction_Squat) && flags->flag[Control_Squat] != 0)
        owner->m_wants.squat = 1;
    if (pad->actionHeld(PadAction_Ready) && (!owner->isControlled() || flags->flag[Control_Ready] != 0))
        owner->m_wants.ready = 1;
    if (length > 0.0f && (!owner->isControlled() || flags->flag[Control_Walk] != 0))
        stepMove(angle, length);
    stepQuickTurn();
    stepAlt();
    stepItems();
    stepFire();
    stepPadExtras();

    if (owner->isPlayer() && owner->isScenarioLead() && owner->getAltSoundFlag() != 0 && owner->m_altAction == 0)
        return;

    if (owner->m_allows.lamp != 0 && pad->actionPressed(PadAction_Lamp) && flags->flag[Control_Lamp] != 0)
        owner->m_wants.lampSwitch = 1;
    stepHead(lookTarget);

    PlayerMotion *motion = owner->m_motion;
    if (m_scopeWas == 0 && motion->noFlash != 0)
        startScope();
    m_scopeWas = motion->noFlash;
    stepAim(length, lookTarget);
    stepView();
}

// At the wheel. Ready drives on, Zoom back, each by its pressure, the squat
// button brakes and the left stick steers; a look at the car's target
// (m_lookAt, the driver's or the passenger's) takes the head from the stick.
// callcheck: isMode2, setDirection, setDirection, setDirection, setDirection, honk, isLit, setLights, isMode2, atan2Octant, addAngle, addAngle, atan2Octant, addAngle, addAngle, getGameOptions, getGameOptions

void CharacterInput::stepCar()
{
    StageCharacter *owner = m_owner;
    CharacterPad *pad = m_pad;
    StageScene *scene = gStageScene;
    const UiShowFlags *flags = scene->showFlags;
    StageCar *car = static_cast<StageCar *>(owner->m_gear);
    StageCharacter *partner = static_cast<StageCharacter *>(scene->partner);
    const int id = owner->m_motion->motionId;

    bool atWheel = false;
    if ((id == kMotionDriveA || id == kMotionDriveB) && owner->m_lookAt == nullptr)
        atWheel = partner == nullptr || !partner->isMode2() || partner->m_lookAt == nullptr;

    if (atWheel) {
        if (flags->flag[Control_CarDrive] == 0) {
            car->setDirection(1);
            car->m_throttle = 0;
            car->m_steer = 0.0f;
        } else {
            if (pad->actionHeld(PadAction_Ready)) {
                car->setDirection(1);
                car->m_throttle = pad->actionPressure(PadAction_Ready);
            } else {
                car->setDirection(-1);
                car->m_throttle = pad->actionPressure(PadAction_Zoom);
            }
            car->m_steer = pad->m_stick[2];
        }
        car->m_brake = pad->actionPressure(PadAction_Squat);
        if (flags->flag[Control_CarHorn] != 0 && (pad->m_pressed & kPadR3) != 0)
            car->honk();
        if (flags->flag[Control_CarLights] != 0 && pad->actionPressed(PadAction_Lamp))
            car->setLights(!car->m_lights->isLit());
    } else {
        car->setDirection(1);
        car->m_throttle = 0;
        car->m_brake = 0;
        car->m_steer = 0.0f;
    }

    if (owner->m_allows.head == 0)
        return;
    CharacterWants &wants = owner->m_wants;
    wants.headOn = 1;
    if (partner != nullptr && !partner->isMode2())
        partner = nullptr;
    StageObject *target = owner->m_lookAt;
    if (target == nullptr && partner != nullptr)
        target = partner->m_lookAt;

    float pitch;
    float yaw;
    if (target == nullptr) {
        pitch = -pad->m_stick[1] * kCarHeadPitch;
        yaw = -pad->m_stick[0] * kCarHeadYaw;
    } else {
        const float *at = target->m_xform.position;
        pitch = 0.0f;
        if (partner != nullptr && partner->m_allows.head != 0) {
            partner->m_wants.headOn = 1;
            const float *from = partner->m_xform.position;
            float turn = atan2Octant(from[0] - at[0], -(from[1] - at[1]));
            turn = addAngle(turn, -partner->m_xform.rotation[2]);
            partner->m_wants.headAngles[0] = 0.0f;
            partner->m_wants.headAngles[2] = addAngle(turn, -kPi);
        }
        const float *from = owner->m_xform.position;
        float turn = atan2Octant(from[0] - at[0], -(from[1] - at[1]));
        turn = addAngle(turn, -owner->m_xform.rotation[2]);
        yaw = addAngle(turn, -kPi);
    }
    wants.headAngles[0] = pitch;
    wants.headAngles[2] = yaw;
    const GameOptions *options = getGameOptions();
    if (options->invertY != 0)
        wants.headAngles[0] *= -1.0f;
    if (options->invertX != 0)
        wants.headAngles[2] *= -1.0f;
}

// R3 with the party's companion: the player takes the companion over, or it
// answers that it cannot; the lead of a scenario calls them or lets them go.
static const char kSoundTakeOver[] = "JAK_SP2"; /* data 0x307328 */
static const char kSoundRefused[]  = "JAK_SP3"; /* data 0x307330 */
static const char kSoundCall[]     = "JAK_SP1"; /* data 0x307338 */
constexpr int   kStageNoTakeOver = 0x1b;        // StageSelect::stage
constexpr int   kSheetKindB      = 0xb;         // StageObjRecord::kind
constexpr int   kLifeState4      = 4;           // getLifeState
constexpr float kJackVolume      = 1.0f;

// callcheck: isPlayer, isTakenOver, releaseControl, currentMission, isPlayer, isStageDrained, getCompanion, getKind, getKind, getSaveData, canTakeOver, isCompanionWalking, getStageSelect, playNamed, getLifeState, isPartner, playNamed, currentMission, findCharacterById, playNamed, requestControl, isPlayer, isScenarioLead, playNamed, getCompanion, setCompanion, playNamed (stageObjRecord; the ROM asks getKind twice; getStageSelect stands for its getSaveData + 0x4bc)

void CharacterInput::stepCompanion()
{
    StageCharacter *owner = m_owner;
    StageScene *scene = gStageScene;

    if (!owner->isPlayer()) {
        if (owner->isTakenOver()) {
            scene->releaseControl();
            scene->altLatch = 1;
        }
        return;
    }

    scene->currentMission()->_d9 = 1;
    if (owner->isPlayer() && owner->isStageDrained()) {
        StageObject *companionObject = scene->party->getCompanion();
        if (companionObject->getKind() != StageObject_Character)
            return;
        StageCharacter *companion = static_cast<StageCharacter *>(companionObject);
        if (getStageSelect()->stage != kStageNoTakeOver && companion->canTakeOver()) {
            StageObject *found = scene->objects->findCharacterById(companion->m_id);
            scene->sound->playNamed(kSoundTakeOver, kJackVolume);
            scene->requestControl(found);
        } else if (companion->isCompanionWalking()) {
            scene->currentMission()->_d8 = 1;
        } else if (stageObjRecord(companion->m_itemDef)->kind == kSheetKindB
                   || (companion->getLifeState() != kLifeState4 && !companion->isPartner())) {
            scene->sound->playNamed(kSoundRefused, kJackVolume);
            scene->altLatch = 1;
        }
    } else if (owner->isPlayer() && owner->isScenarioLead()) {
        scene->sound->playNamed(kSoundCall, kJackVolume);
        if (owner->m_altAction == 0) {
            scene->party->setCompanion(scene->party->getCompanion());
            owner->m_altAction = 1;
        } else {
            owner->m_altAction = 0;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00236b90
// callcheck: getActionState, lerpVec4, getPlayerAltFlag, getCompanion, addAngle, atan2Octant, bitMask, canTakeOver, currentMission, findCharacterById, forcedViewChange, getAltSoundFlag, getDeltaSeconds, getGameOptions, getLifeState, getSaveData, getStageSelect, getTopView, getTurnRate, honk, isCompanionWalking, isControlled, isLit, isMode2, isPartner, isPlayer, isScenarioLead, isStageDrained, isTakenOver, offer, playNamed, randNext, readFire, releaseControl, requestControl, setCompanion, setDirection, setLights, sinCos, start, stop, wrapAngle (held by the step* sections, one function in the ROM)

void CharacterInput::update()
{
    if (m_active == 0)
        return;

    CharacterPad *pad = m_pad;
    const float stickX = pad->m_stick[2];
    const float stickY = pad->m_stick[3];
    u_char bits = 0;
    if (stickY < -kStickFull)
        bits |= StickBit_Up;
    if (stickY > kStickFull)
        bits |= StickBit_Down;
    if (stickX < -kStickFull)
        bits |= StickBit_Left;
    if (stickX > kStickFull)
        bits |= StickBit_Right;
    m_history->push(bits);

    sceVu0FVECTOR lookTarget = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (m_owner->getActionState() == kActionDriving)
        stepCar();
    else
        stepOnFoot(lookTarget);

    if (!isAttackMotion(m_owner->m_motion->motionId))
        lerpVec4(kLookEase, m_look, m_look, lookTarget);

    StageScene *scene = gStageScene;
    if ((pad->m_pressed & kPadR3) != 0 && scene->showFlags->flag[Control_Companion] != 0
        && scene->getPlayerAltFlag() != 0 && scene->party->getCompanion() != nullptr)
        stepCompanion();
}

// ──────────────────────────────────────────────────────────────────────
// Two small members of the character (0x2162d0 has other callers, not written).

// FS2 0x00214ee0

int StageCharacter::forcedViewChange() const
{
    StageScene *scene = gStageScene;
    const StageObject *onPad = scene->controlled != nullptr
        ? scene->controlled : static_cast<StageObject *>(scene->player);
    if (onPad != this)
        return 0;
    if (scene->showFlags->flag[Control_ForceView] != 0)
        return 1;
    return scene->player == this ? 0 : 1;
}

// FS2 0x002162d0
// callcheck: getDeltaSeconds

float StageCharacter::getTurnRate()
{
    constexpr float kTurnPerSecond = 1.9634955f;
    return m_clock->getDeltaSeconds() * kTurnPerSecond * m_turnRate;
}
