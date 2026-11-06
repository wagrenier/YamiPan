/* ==========================================================================
 *  game/gameoversequence.cpp
 *
 *  GameOverSequence, its camera and SceneDirector (gameoversequence.h).
 * ======================================================================== */
#include <string.h>

#include "game/gameoversequence.h"
#include "game/gamesystem.h"
#include "game/playermotion.h"
#include "game/stagescene.h"
#include "game/vu0math.h"
#include "sdk/libgraph.h"       /* SCE_GS_ALPHA_1, SCE_GS_TEST_1 */
#include "sdk/libpad.h"         /* SCE_PADstart */
#include "yamipan/rendering/yamipan_drawui.h"

constexpr float kGameOverFadeSeconds = 2.0f;
constexpr int   kCulpritFrames       = 100;     // state 3; state 2 has half
constexpr int   kOwnFallFrames       = 200;     // the same when the victim felled itself
constexpr int   kDeathCameraPriority = 3;       // on the view stack

// Who fell in a way the fall's shot makes no sense for: PlayerMotion::m_bfc in
// one of these goes straight to state 3. The same test as CharProp_MotionFlagged.
constexpr int kNoFallShotMotions[] = { Motion_LadderFall, 0x95, 0x94 };

// FS2 0x002b1cb0

GameOverSequence::GameOverSequence(StageObject *victim, StageObject *cause)
    : m_victim(victim), m_cause(cause)
{
}

bool GameOverSequence::isFadeFull()
{
    StageFade *fade = gStageScene->fade;
    return fade->isFullInMode2() || fade->isFullInMode1();
}

// FS2 0x002b1c50
// callcheck: isFullInMode2

int GameOverSequence::getState()
{
    if ((m_state >= GameOverState_Fading || m_skipped) && gStageScene->fade->isFullInMode2())
        return GameOverState_Done;
    return m_state;
}

// The ROM has the fade's tests once per place.
// FS2 0x002b1670
// callcheck: isFullInMode2, isFullInMode1, start, isFullInMode2, hasGear, setState, setState, isFullInMode2, isFullInMode1, start, setState, getLifeState, setState, setState

void GameOverSequence::update()
{
    StageFade *fade = gStageScene->fade;
    if ((gGameSystem->m_pad0.m_pressed & SCE_PADstart) != 0 && !m_skipped && !isFadeFull()) {
        m_skipped = 1;
        fade->start(kGameOverFadeSeconds, 1.0f, 0.0f);
    }
    if (m_skipped)
        return;

    const int state = m_state >= GameOverState_Fading && fade->isFullInMode2()
                    ? (int)GameOverState_Done : m_state;
    StageCharacter *victim = static_cast<StageCharacter *>(m_victim);
    switch (state) {
    case GameOverState_Start: {
        if (victim == nullptr) {
            if (!isFadeFull()) {
                m_skipped = 1;
                fade->start(kGameOverFadeSeconds, 1.0f, 0.0f);
            }
            return;
        }
        bool noFallShot = victim->hasGear();
        if (!noFallShot && victim->m_motion->m_bfc != 0) {
            for (int motion : kNoFallShotMotions)
                noFallShot = noFallShot || victim->m_motion->motionId == motion;
        }
        setState(noFallShot ? GameOverState_Culprit : GameOverState_Fall);
        break;
    }
    case GameOverState_Fall:
        if (m_timer-- > 0)
            return;
        setState(GameOverState_Culprit);
        break;
    case GameOverState_Culprit:
        if (victim->getLifeState() == LifeState_Corpse && m_victim != m_cause) {
            setState(GameOverState_Fading);
            break;
        }
        if (m_timer-- > 0)
            return;
        setState(GameOverState_Fading);
        break;
    default:
        break;
    }
}

// Inline three times in GameOverSequence::setState.

StageObject *GameOverSequence::pickViewTarget()
{
    if (m_victim == nullptr)
        return nullptr;
    if (m_cause == nullptr || m_victim == m_cause)
        return gStageScene->player;
    if (static_cast<StageCharacter *>(m_cause)->canBeJacked() || m_cause->isPlayer())
        return m_cause;
    return nullptr;
}

// The ROM also asks canBeJacked and isPlayer on the way to state 2 and drops
// the answers.
// FS2 0x002b1940
// callcheck: removeView, canBeJacked, isPlayer, start, isRunning, stop, start, startGameOverView, operator_new, DeathCamera, addToScene, pushView (pickViewTarget holds the two questions)

void GameOverSequence::setState(int state)
{
    StageScene *scene = gStageScene;
    const bool ownFall = m_victim != nullptr && m_victim == m_cause;
    const int frames = ownFall ? kOwnFallFrames : kCulpritFrames;

    if (m_state == GameOverState_Fall && pickViewTarget() != nullptr)
        scene->removeView(m_camera);

    switch (state) {
    case GameOverState_Fading:
        scene->fade->start(kGameOverFadeSeconds, 1.0f, 0.0f);
        break;
    case GameOverState_Culprit: {
        StageObject *target = pickViewTarget();
        m_viewTaken = 1;
        if (target != nullptr) {
            if (scene->party->isRunning())
                scene->party->stop();
            scene->party->start(scene->player);
            scene->party->startGameOverView(target);
        }
        m_timer = frames;
        break;
    }
    case GameOverState_Fall: {
        DeathCamera *camera = new DeathCamera;
        camera->addToScene("CAM_DEAD");
        camera->m_linked = m_victim;
        camera->onLinkChanged();
        scene->pushView(camera, kDeathCameraPriority, 1);
        m_camera = camera;
        m_timer = frames / 2;
        break;
    }
    default:
        break;
    }
    m_state = state;
}

// ──────────────────────────────────────────────────────────────────────
// CAM_DEAD.

constexpr float kDeathCameraScrZ   = 450.0f;
constexpr float kDeathCameraWidth  = 512.0f;
constexpr float kDeathCameraHeight = 512.0f;
constexpr float kDeathCameraFar    = 1600.0f;

constexpr u_int kDeathRayMask  = 0xfc2600f;     // as the main camera's
constexpr u_int kDeathRayFlags = 0xc0000;

constexpr float kDeathCameraSwingSteps = 60.0f;
constexpr float kDeathCameraSwingAccel = 0.0034906585f; // 0x3b64c389: half a turn in the steps
constexpr float kDeathCameraHalfTurn   = 3.1415927f;

// FS2 0x002b1fe0
// callcheck: StageCamera, resetShot

DeathCamera::DeathCamera()
    : StageCamera(kDeathCameraScrZ, kDeathCameraWidth, kDeathCameraHeight, kDeathCameraFar)
{
    m_linked = nullptr;
    resetShot();
}

// FS2 0x002b1f50
// callcheck: onViewTop

void DeathCamera::resetShot()
{
    static const sceVu0FVECTOR kEyeOffset   = { 0.0f, -100.0f, 200.0f, 1.0f };
    static const sceVu0FVECTOR kFocusOffset = { 0.0f, 0.0f, 80.0f, 1.0f };

    StageCamera::onViewTop();
    memcpy(m_eyeOffset, kEyeOffset, sizeof(m_eyeOffset));
    memcpy(m_focusOffset, kFocusOffset, sizeof(m_focusOffset));
    m_angle = 0.0f;
    m_angleTotal = kDeathCameraHalfTurn;
    m_swingSpeed = 0.0f;
    m_swingAccel = kDeathCameraSwingAccel;
    m_swingTotal = kDeathCameraSwingSteps;
    m_swingLeft = kDeathCameraSwingSteps;
}

// FS2 0x002b1dc0
// callcheck: identityMatrix, rotateMatrixZ, applyMatrix, getWorldMatrix, applyMatrix, getWorldMatrix, raycast, aimFrom

void DeathCamera::cameraStep()
{
    if (m_linked == nullptr)
        return;

    sceVu0FMATRIX turn;
    identityMatrix(turn);
    rotateMatrixZ(m_angle, turn, turn);
    if (m_swingLeft > 0.0f) {
        m_swingLeft -= 1.0f;
        if (m_swingLeft < m_swingTotal / 2.0f)
            m_swingSpeed -= m_swingAccel;
        else
            m_swingSpeed += m_swingAccel;
        m_angle += m_swingSpeed;
    }

    sceVu0FVECTOR point;
    applyMatrix(point, turn, m_eyeOffset);
    applyMatrix(point, m_linked->getWorldMatrix(), point);
    memcpy(m_xform.position, point, sizeof(point));

    // The eye stays on this side of the map.
    const sceVu0FVECTOR *world = m_linked->getWorldMatrix();
    for (int i = 0; i < 3; i++)
        point[i] = world[3][i] + m_focusOffset[i];
    point[3] = 1.0f;
    sceVu0FVECTOR hit;
    if (gStageScene->raycast(hit, point, m_xform.position, 0, kDeathRayMask, kDeathRayFlags, nullptr) != nullptr)
        memcpy(m_xform.position, hit, sizeof(hit));
    aimFrom(point, m_xform.position);
}

// ──────────────────────────────────────────────────────────────────────
// The letterbox. The fill's numbers are the 512-square frame's: the canvas
// spreads the frame over the display, so the bars keep their share of it and
// reach both edges at any aspect. ALPHA (Cs - Cd) * As + Cd, TEST: depth
// always, no alpha test.

constexpr u_long kLetterboxAlpha = 0x44;
constexpr u_long kLetterboxTest  = 0x30000;
constexpr int    kLetterboxFrame  = 0x200;
constexpr int    kLetterboxTop    = 0x40;
constexpr int    kLetterboxBottom = 0x1c0;

// Inline in SceneDirector::draw and StageScene::draw.
// callcheck: gsFillRect, gsFillRect

void drawLetterbox(sceVif1Packet *packet, u_int rgba)
{
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kLetterboxAlpha);
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kLetterboxTest);
    gsFillRect(packet, 0, 0, kLetterboxFrame, kLetterboxTop, rgba);
    gsFillRect(packet, 0, kLetterboxBottom, kLetterboxFrame, kLetterboxFrame, rgba);
}

// ──────────────────────────────────────────────────────────────────────
// The ramp.

// One byte of the colour, frame frames of frames in.
static u_int rampByte(u_int from, u_int to, int shift, int frame, int frames)
{
    const int start = (from >> shift) & 0xff;
    const int end   = (to >> shift) & 0xff;
    return (u_int)(u_char)(start + frame * (end - start) / frames) << shift;
}

// FS2 0x002b22d0

SceneDirector::SceneDirector()
    : colour(0), m_from(0), m_to(0), m_frame(0), m_frames(0)
{
}

// FS2 0x002b2280
// callcheck: operator_delete (the delete-expression)

SceneDirector::~SceneDirector()
{
}

// FS2 0x002b2270

bool SceneDirector::isRunning()
{
    return m_frames != 0;
}

// FS2 0x002b2250

void SceneDirector::start(int frames, u_int from, u_int to)
{
    m_frames = frames;
    m_frame = 0;
    m_from = from;
    m_to = to;
}

// The two GS register packets are drawLetterbox's YamiPan_DrawUiReg.
// FS2 0x002b20a0
// callcheck: gsFillRect, gsFillRect (drawLetterbox holds them)

void SceneDirector::draw(sceVif1Packet *packet)
{
    if (m_frames != 0) {
        m_frame++;
        if (m_frame < m_frames) {
            colour = 0;
            for (int shift = 0; shift < 32; shift += 8)
                colour |= rampByte(m_from, m_to, shift, m_frame, m_frames);
        } else {
            m_frames = 0;
            m_frame = 0;
            colour = m_to;
        }
    }
    drawLetterbox(packet, colour);
}
