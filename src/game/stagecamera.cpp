/* ==========================================================================
 *  game/stagecamera.cpp
 *
 *  The stage's cameras: the base (vtable 0x311860, 0x177600..0x177d60) and the
 *  main camera "camera" (0x311fa0), which follows the object it is linked to
 *  (kind 8's m_linked: 0x178160..0x179900). The file name is the port's.
 *  The camjack (0x311ac0), ViewCamera and DemoCamera have their own files
 *  to come; their slots are stubs.
 *
 *  A camera without a link stays where its object was placed: the world
 *  origin, which on stage 1 is empty space far from the map: native 0x70 links
 *  it to the player.
 * ======================================================================== */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "game/characterground.h"
#include "game/characterinput.h"
#include "game/gamehooks.h"
#include "game/playermotion.h"
#include "game/mathutil.h"
#include "game/packtables.h"
#include "game/scenerender.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
#include "game/sysdata.h"
#include "game/vu0math.h"

constexpr float kCameraScreenCenter = 2048.0f;
constexpr float kCameraZMin = 1.0f;
constexpr float kCameraZMax = 16777215.0f;
constexpr float kCameraNear = 1.0f;
constexpr float kHalfPi = 1.5707964f;       // 0x3fc90fdb
constexpr float kCameraFarMargin = 1000.0f; // 0x447a0000: the screen's far is this past the range

// FS2 0x00177d60
// callcheck: StageObject, identityMatrix, rotateMatrixX, sceVu0ViewScreenMatrix, sceVu0MulMatrix

StageCamera::StageCamera(float scrZ, float width, float height, float farZ)
{
    sceVu0FMATRIX axes;
    identityMatrix(axes);
    rotateMatrixX(kHalfPi, axes, axes);

    m_scrZ = scrZ;
    m_screenWidth = width;
    m_screenHeight = height;
    m_range = farZ;
    m_range2 = 0.0f;    // PC: the ROM clears it after the matrix, which reads whatever the heap held
    sceVu0ViewScreenMatrix(m_screen, m_scrZ, 1.0f, m_screenHeight / ((m_screenWidth * 3.0f) / 4.0f),
                           kCameraScreenCenter, kCameraScreenCenter, kCameraZMin, kCameraZMax, kCameraNear,
                           m_range + m_range2 + kCameraFarMargin);
    sceVu0MulMatrix(m_screen, m_screen, axes);
}

// FS2 0x00177d20
// callcheck: setName, add0

void StageCamera::addToScene(const char *name)
{
    setName(name);
    gStageScene->objects->add0(this);
}

// FS2 0x00177d00
// callcheck: none (a byte of the render record)

void StageCamera::onViewTop()
{
    gStageScene->render->m_snap = 1;
}

// FS2 0x001779a0
// callcheck: identityMatrix, rotateMatrixX, sceVu0ViewScreenMatrix, sceVu0MulMatrix

void StageCamera::setRange(float range)
{
    sceVu0FMATRIX axes;
    identityMatrix(axes);
    rotateMatrixX(kHalfPi, axes, axes);

    m_range = range;
    sceVu0ViewScreenMatrix(m_screen, m_scrZ, 1.0f, m_screenHeight / ((m_screenWidth * 3.0f) / 4.0f),
                           kCameraScreenCenter, kCameraScreenCenter, kCameraZMin, kCameraZMax, kCameraNear,
                           m_range + m_range2 + kCameraFarMargin);
    sceVu0MulMatrix(m_screen, m_screen, axes);
}

// FS2 0x00177be0
// callcheck: identityMatrix, rotateMatrixX, sceVu0ViewScreenMatrix, sceVu0MulMatrix

void StageCamera::setProjection(float scrZ, float width, float height, float range)
{
    sceVu0FMATRIX axes;
    identityMatrix(axes);
    rotateMatrixX(kHalfPi, axes, axes);

    m_scrZ = scrZ;
    m_screenWidth = width;
    m_screenHeight = height;
    m_range = range;
    sceVu0ViewScreenMatrix(m_screen, m_scrZ, 1.0f, m_screenHeight / ((m_screenWidth * 3.0f) / 4.0f),
                           kCameraScreenCenter, kCameraScreenCenter, kCameraZMin, kCameraZMax, kCameraNear,
                           m_range + m_range2 + kCameraFarMargin);
    sceVu0MulMatrix(m_screen, m_screen, axes);
}

// FS2 0x00177ac0
// callcheck: identityMatrix, rotateMatrixX, sceVu0ViewScreenMatrix, sceVu0MulMatrix

void StageCamera::setScrZ(float scrZ)
{
    sceVu0FMATRIX axes;
    identityMatrix(axes);
    rotateMatrixX(kHalfPi, axes, axes);

    m_scrZ = scrZ;
    sceVu0ViewScreenMatrix(m_screen, m_scrZ, 1.0f, m_screenHeight / ((m_screenWidth * 3.0f) / 4.0f),
                           kCameraScreenCenter, kCameraScreenCenter, kCameraZMin, kCameraZMax, kCameraNear,
                           m_range + m_range2 + kCameraFarMargin);
    sceVu0MulMatrix(m_screen, m_screen, axes);
}

// FS2 0x00177860
// callcheck: atan2Octant, atan2Octant, setRotation

void StageCamera::aimFrom(const float *target, const float *eye)
{
    sceVu0FVECTOR d, sq;
    for (int i = 0; i < 4; i++) {
        d[i] = target[i] - eye[i];
        sq[i] = d[i] * d[i];
    }
    const float yaw = atan2Octant(d[0], d[1]);
    const float pitch = atan2Octant(d[2], sqrtf(sq[0] + sq[1]));
    m_xform.setRotation(pitch, 0.0f, -yaw);
}

// FS2 0x00177900
// callcheck: atan2Octant, atan2Octant, setRotation (aimFrom is the same code)

void StageCamera::lookAt(const float *target)
{
    aimFrom(target, m_xform.position);
}

// FS2 0x00177810
// callcheck: invertRigidMatrix, multiplyMatrix

void StageCamera::calcViewMatrix()
{
    invertRigidMatrix(m_view, getMatrix());
    multiplyMatrix(m_viewScreen, m_screen, m_view);
}

// ──────────────────────────────────────────────────────────────────────
// A character's numbers for the camera.

// What the model's name says the body is (table 0x2f7ba0, read by
// StageCharacter::setup), its height.
static const float kCameraBodyHeights[4] = { 180.0f, 170.0f, 160.0f, 110.0f }; /* data 0x2f7c10 */

// The record of the character's motion; null outside the table. Inline in
// the ROM.
static const MotionDef *linkedMotion(const StageObject *character)
{
    const int id = character->m_motion->motionId;
    return id < 0 || id >= kMotionDefCount ? nullptr : &gMotionDefs[id];
}

// FS2 0x0017bb10

float StageObject::cameraBodyHeight() const
{
    return kCameraBodyHeights[m_bodySize];
}

// FS2 0x0017bb80

const float *StageObject::cameraEyeOffset(int set) const
{
    return linkedMotion(this)->cameraEye[set][m_bodySize];
}

// FS2 0x0017bbf0

const float *StageObject::cameraFocusOffset(int set) const
{
    return linkedMotion(this)->cameraFocus[set][m_bodySize];
}

// PC: these two belong to the collision run (unassigned@00231c90), the
// main camera is their first caller.
// FS2 0x00235b00
// callcheck: vecDot

bool StageCollisionFace::inFront(const sceVu0FVECTOR point) const
{
    return plane[3] + vecDot(plane, point) >= 0.0f;
}

// The AABB min..max, each side moved out by margin, holds point.
// FS2 0x00236b20

static bool boxHolds(const CollisionBody *box, const sceVu0FVECTOR point, const sceVu0FVECTOR margin)
{
    for (int i = 0; i < 3; i++) {
        if (point[i] < box->box[0][i] - margin[i] || box->box[1][i] + margin[i] < point[i])
            return false;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The main camera.

constexpr float kMainCameraScrZ = 450.0f;
constexpr float kMainCameraWidth = 512.0f;
constexpr float kMainCameraHeight = 512.0f;
constexpr float kMainCameraFar = 1600.0f;

constexpr int   kMainCameraBodyBucket = 2;
constexpr u_int kMainCameraBodyCategory = 0x4000;
constexpr u_int kMainCameraBodyMask = 2;

// What its rays hit.
constexpr u_int kCameraRayMask = 0xfc2600f;
constexpr u_int kCameraRayFlags = 0xc0000;
constexpr u_int kShoulderEyeRayMask = 0x1e42600f;
constexpr u_int kShoulderEyeRayFlags = 0x800;

constexpr float kFollowDivisor = 150.0f;        // the eye's ease, slower is greater
constexpr float kFollowDivisorAbArea = 300.0f;
constexpr float kOffsetEaseScale = 0.66f;       // the offsets ease this much faster than the eye
constexpr float kFollowMaxStep = 25.0f;
constexpr float kFacingEaseMax = 1000.0f;       // eye moving towards what it looks at: slower
constexpr float kLookEaseRate = 0.005f;         // 0x3ba3d70a
constexpr float kLookMaxStep = 20.0f;
constexpr float kTurnEaseRate = 0.8f;           // 0x3f4ccccd
constexpr float kTurnMaxStep = 0.314159f;       // 0x3ea0d97c

constexpr float kZoomStep = 20.0f;
constexpr float kZoomShoulder = 160.0f;         // scrZ 610 over the shoulder

constexpr float kShiftFocus = -15.0f;           // the motion's cameraShift with GameOptions::_4c
constexpr float kShiftEye = -30.0f;
constexpr float kSwayScale = 30.0f;

constexpr float kFloorTiltScale = 0.65f;
constexpr float kYawStickEyeRange = 1.134464f;  // 65 degrees
constexpr float kPitchUpLimit = 0.8853982f;
constexpr float kPitchDownLimit = 1.3707963f;   // pi/2 - 0.2
constexpr float kPitchStickRise = 40.0f;
constexpr float kPitchStickPull = 0.45f;
constexpr float kBodyHeightRoom = 60.0f;
constexpr float kYawStickPull = 5.0f;
constexpr float kYawStickFocusSide = 60.0f;

constexpr int   kBlockedForever = 30;           // frames before the eye gives up on the wanted place
constexpr float kBackFaceFarSq = 100.0f;
constexpr float kBackFaceFarSqEye = 10.0f;
constexpr float kWallProbeOut = 6.0f;           // 0x40c00000
constexpr float kWallProbeIn = 1.0f;

constexpr float kPartnerFarSq = 22500.0f;       // 150

constexpr int   kShoulderSlots = 7;
constexpr float kShoulderEyeRise = 80.0f;       // the second ray starts this far above the character

// Where the eye goes in the character's space over the shoulder (0x2f7b10),
// tried from the one that worked last.
static const sceVu0FVECTOR kShoulderEyes[kShoulderSlots] = {
    { -80.0f, 80.0f, 120.0f, 1.0f },
    { -60.0f, 30.0f, 120.0f, 1.0f },
    { 80.0f, 80.0f, 120.0f, 1.0f },
    { 60.0f, 30.0f, 120.0f, 1.0f },
    { 40.0f, 20.0f, 160.0f, 1.0f },
    { -40.0f, 20.0f, 160.0f, 1.0f },
    { 0.0f, 20.0f, 180.0f, 1.0f },
};
// The floor that tilts the view is looked for this far ahead.
static const sceVu0FVECTOR kFloorProbe = { 0.0f, -60.0f, 0.0f, 0.0f }; /* data 0x2f7b80 */
// A character's GroundVolume is entered this far early.
static const sceVu0FVECTOR kVolumeMargin = { 0.0f, 0.0f, 15.0f, 0.0f }; /* data 0x2f7b90 */

static const char kHeadBone[] = "bn_head";      /* data 0x303178 */

// The motions the camera leans with (their labels are unread).
static const int kSwayMotions[] = {
    0x2c, 0x18, 0x17, 5, 4, 3, 0x8a, 0x7b, 0x6c, 0x51, 0x20, 0x1a, 0x19, 0x16, 10, 9, 0x2d, 0x24,
};

static bool motionSways(int id)
{
    for (int sway : kSwayMotions) {
        if (sway == id)
            return true;
    }
    return false;
}

// The first active object from slot on. Inline in the ROM.
static StageObject **nextActive(StageObject **slot)
{
    while (slot != nullptr && !(*slot)->m_active)
        slot = StageObjArray::next(slot);
    return slot;
}

static inline void vecCopy(sceVu0FVECTOR out, const float *in)
{
    for (int i = 0; i < 4; i++)
        out[i] = in[i];
}

// FS2 0x00179700
// callcheck: StageCamera, operator_new, CollisionBody, add

StageMainCamera::StageMainCamera()
    : StageCamera(kMainCameraScrZ, kMainCameraWidth, kMainCameraHeight, kMainCameraFar)
{
    m_linked = nullptr;
    m_focusOffset[0] = 0.0f;
    m_focusOffset[1] = 0.0f;
    m_focusOffset[2] = 135.0f;
    m_focusOffset[3] = 1.0f;
    m_eyeOffset[0] = 0.0f;
    m_eyeOffset[1] = 250.0f;
    m_eyeOffset[2] = 40.0f;
    m_eyeOffset[3] = 0.0f;
    for (int i = 0; i < 4; i++) {
        m_lookTarget[i] = 0.0f;
        m_turn[i] = 0.0f;
    }
    m_shoulderSlot = 0;
    m_zoom = 0.0f;
    m_blockedFrames = 0;
    m_headHeight = 0.0f;
    m_nearWall = 0;
    m_snap = 1;

    CollisionBody *body = new CollisionBody(this, kMainCameraBodyBucket, kMainCameraBodyCategory,
                                            kMainCameraBodyMask);
    body->shape = CollisionShape_Camera;
    gStageScene->records->add(body);
}

// FS2 0x00179680
// callcheck: find, getWorldMatrix, onViewTop

void StageMainCamera::onLinkChanged()
{
    StageObject *linked = m_linked;
    if (linked != nullptr) {
        StageNodeBase *head = linked->m_model->boneMap.find(kHeadBone)->value;
        m_headHeight = head->getWorldMatrix()[3][2] - linked->m_xform.position[2];
    }
    onViewTop();
}

// FS2 0x00179490
// callcheck: onViewTop, isInAbArea, cameraFocusOffset, cameraEyeOffset, getGameOptions, getMatrix, getMatrix, cameraStep, updateMatrix (applyMatrix: vu0math.h)

void StageMainCamera::onViewTop()
{
    m_snap = 1;
    m_nearWall = 0;
    m_blockedFrames = kBlockedForever;
    if (m_linked == nullptr)
        return;

    StageCamera::onViewTop();
    if (linkedMotion(m_linked) != nullptr) {
        const int set = gStageScene->isInAbArea(m_linked) ? 1 : 0;
        vecCopy(m_focusOffset, m_linked->cameraFocusOffset(set));
        vecCopy(m_eyeOffset, m_linked->cameraEyeOffset(set));
        if (getGameOptions()->_4c != 0 && linkedMotion(m_linked)->cameraShift[set] != 0) {
            m_focusOffset[0] += kShiftFocus;
            m_eyeOffset[0] += kShiftEye;
        }
    }
    m_focusOffset[3] = 1.0f;
    m_eyeOffset[3] = 0.0f;

    applyMatrix(m_lookTarget, m_linked->getMatrix(), m_focusOffset);
    applyMatrix(m_xform.position, m_linked->getMatrix(), m_eyeOffset);
    for (int i = 0; i < 4; i++) {
        m_xform.position[i] += m_lookTarget[i];
        m_turn[i] = 0.0f;
    }
    cameraStep();
    updateMatrix();
}

// Each frame while it is a view (StageScene::update), and from onViewTop. It
// works out where it wants to stand, a point of the character's plus an
// offset that the character's motion and the right stick move; pulls that
// place out of the walls the rays find; eases the eye there; and turns.
// FS2 0x00178160
// callcheck: __extendsfdf2, __muldf3, __subdf3, __truncdfsf2, isInAbArea, getKind, getKind, setScrZ, cameraFocusOffset, cameraEyeOffset, getGameOptions, approachVec3, approachVec3, getMatrix, applyMatrix, faceNear, getNormal, vecDot, asinAcos, vecNormalize, sinCos, sinCos, vecDot, identityMatrix, rotateMatrixX, applyMatrix, partnerResponds, vecDistanceSq, cameraBodyHeight, getMatrix, applyMatrix, getMatrix, applyMatrix, aimFrom, setScrZ, getMatrix, applyMatrix, segmentHitsAny, segmentHitsAny, aimFrom, raycast, raycast, inFront, vecDistanceSq, segmentHitsAny, raycast, vecNormalize, scaleVec4, scaleVec4, raycast, vecDot, scaleVec4, scaleVec4, raycast, vecDot, scaleVec4, boxHolds, boxHolds, vecDirection, vecDirection, vecDot, approachVec3, boxHolds, raycast, segmentHitsAny, raycast, inFront, vecDistanceSq, approachVec3, aimFrom, approachVec3, addAngles

void StageMainCamera::cameraStep()
{
    StageObject *linked = m_linked;
    if (linked == nullptr)
        return;

    StageScene *scene = gStageScene;
    float yawStick = 0.0f;
    float pitchStick = 0.0f;
    bool moveEye = true;
    bool stickTurns = true;
    const int set = scene->isInAbArea(linked) ? 1 : 0;
    float followDivisor = set == 0 ? kFollowDivisor : kFollowDivisorAbArea;

    // The rays leave out what the character is aiming at, unless that is the
    // map. A kind 1..6 only.
    StageObject *skip = nullptr;
    if (linked->m_290 != nullptr) {
        const int kind = linked->m_290->getKind();
        if (kind > 0 && kind < 7)
            skip = linked->m_290;
    }
    if (skip == scene->root)
        skip = nullptr;

    sceVu0FVECTOR anchor;       // the rays start here
    sceVu0FVECTOR focusPoint;
    sceVu0FVECTOR wanted;       // where the eye wants to be

    if (linked->m_290 == nullptr) {
        if (m_zoom > 0.0f) {
            m_zoom -= kZoomStep;
            if (m_zoom < 0.0f)
                m_zoom = 0.0f;
            setScrZ(m_zoom + kMainCameraScrZ);
        }
        m_shoulderSlot = 0;

        sceVu0FVECTOR focus;
        sceVu0FVECTOR eye;
        float focusShift = 0.0f;
        float eyeShift = 0.0f;
        const MotionDef *motion = linkedMotion(linked);
        if (motion != nullptr) {
            vecCopy(focus, linked->cameraFocusOffset(set));
            vecCopy(eye, linked->cameraEyeOffset(set));
            if (getGameOptions()->_4c != 0 && motion->cameraShift[set] != 0) {
                focusShift = kShiftFocus;
                eyeShift = kShiftEye;
                focus[0] += kShiftFocus;
                eye[0] += kShiftEye;
            }
            if (motionSways(linked->m_motion->motionId)) {
                // The ROM does this one in double (the soft-float calls).
                const double side = linked->m_input->padSway() * kSwayScale;
                eye[0] += (float)(side * (1.0 - fabsf(linked->m_input->lookYaw())));
            }
            const float rate = 1.0f / (followDivisor * kOffsetEaseScale);
            approachVec3(rate, kFollowMaxStep, m_focusOffset, m_focusOffset, focus);
            approachVec3(rate, kFollowMaxStep, m_eyeOffset, m_eyeOffset, eye);
        }
        vecCopy(focus, m_focusOffset);
        vecCopy(eye, m_eyeOffset);
        applyMatrix(focusPoint, linked->getMatrix(), focus);
        vecCopy(anchor, focusPoint);

        // The view tilts with a slope the character walks up or down.
        const StageCollisionFace *floor = linked->m_ground->faceNear(kFloorProbe);
        if (floor != nullptr && (floor->flags & StageCollision_Slope) != 0) {
            static const sceVu0FVECTOR kUp = { 0.0f, 0.0f, 1.0f, 0.0f };
            sceVu0FVECTOR normal;
            floor->getNormal(normal);
            const float slope = asinAcos(vecDot(normal, kUp), AsinAcos_Acos);
            sceVu0FVECTOR downhill = { normal[0], normal[1], 0.0f, 0.0f };
            vecNormalize(downhill, downhill);
            sceVu0FVECTOR ahead;
            ahead[0] = -sinCos(-m_xform.rotation[2], SinCos_Sin);
            ahead[1] = -sinCos(-m_xform.rotation[2], SinCos_Cos);
            ahead[2] = 0.0f;
            ahead[3] = 0.0f;
            const float along = vecDot(downhill, ahead);
            sceVu0FMATRIX tilt;
            identityMatrix(tilt);
            rotateMatrixX(-(slope * along * kFloorTiltScale), tilt, tilt);
            applyMatrix(eye, tilt, eye);
        }

        // At an edge with a deep drop: over the character's head.
        PlayerMotion *playerMotion = linked->m_motion;
        const WalkLink *walk = playerMotion->walkLink;
        if (playerMotion->activeLink == walk && walk->edgeFound) {
            const float drop = walk->edgeDrop;
            if (drop < -170.0f) {
                bool partnerFar = false;
                if (scene->partnerResponds(linked)
                    && vecDistanceSq(playerMotion->position, scene->partner->m_xform.position) > kPartnerFarSq)
                    partnerFar = true;
                if (partnerFar) {
                    focus[1] = -30.0f;
                    eye[2] += 60.0f;
                    eye[1] = 20.0f;
                }
                else {
                    eye[2] += 50.0f;
                    eye[1] *= 0.6f;
                }
            }
            else if (drop >= -110.0f || drop <= -130.0f) {
                if (drop < -80.0f && drop > -100.0f) {
                    eye[2] += 30.0f;
                    eye[1] *= 0.7f;
                }
            }
            else {
                eye[2] += 40.0f;
                eye[1] *= 0.65f;
            }
        }

        // The stick pulls the eye round the character.
        yawStick = linked->m_input->lookYaw();
        pitchStick = linked->m_input->lookPitch();
        const float yawSize = fabsf(yawStick);
        const float stickSize = fabsf(pitchStick) <= yawSize ? yawSize : fabsf(pitchStick);
        float rise;
        float pull;
        if (pitchStick > 0.0f) {
            rise = pitchStick * kPitchStickRise;
            pull = pitchStick * eye[1] * kPitchStickPull;
        }
        else {
            pull = -pitchStick * eye[1];
            rise = pitchStick * ((linked->cameraBodyHeight() + kBodyHeightRoom - focus[2]) - eye[2]);
        }
        const float yawPull = yawSize * (eye[1] - kYawStickPull);
        if (pull < yawPull)
            pull = yawPull;
        eye[0] = (eye[0] + 0.0f) - eyeShift * stickSize;
        focus[0] = ((focus[0] + 0.0f) - focusShift * stickSize) - yawStick * (kYawStickFocusSide - eyeShift);
        focus[1] -= pull;
        focus[2] -= rise;

        applyMatrix(focusPoint, linked->getMatrix(), focus);
        applyMatrix(wanted, linked->getMatrix(), eye);
        for (int i = 0; i < 4; i++)
            wanted[i] += focusPoint[i];
        aimFrom(focusPoint, wanted);
    }
    else {
        // Over the shoulder: zoomed in, looking at the character's aim from the
        // first of the seven places both rays reach.
        if (m_zoom < kZoomShoulder) {
            m_zoom += kZoomStep;
            if (m_zoom > kZoomShoulder)
                m_zoom = kZoomShoulder;
            setScrZ(m_zoom + kMainCameraScrZ);
        }

        const StageCharacter *character = static_cast<const StageCharacter *>(linked);
        vecCopy(anchor, character->m_aimPoint);
        vecCopy(focusPoint, anchor);
        int tried = 0;
        for (; tried < kShoulderSlots; tried++) {
            const int slot = (tried + m_shoulderSlot) % kShoulderSlots;
            applyMatrix(wanted, linked->getMatrix(), kShoulderEyes[slot]);
            if (scene->segmentHitsAny(focusPoint, wanted, 0, kShoulderEyeRayMask, kShoulderEyeRayFlags, skip))
                continue;
            sceVu0FVECTOR above;
            vecCopy(above, linked->m_xform.position);
            above[2] += kShoulderEyeRise;
            if (scene->segmentHitsAny(above, wanted, 0, kCameraRayMask, kCameraRayFlags, skip))
                continue;
            aimFrom(focusPoint, wanted);
            vecCopy(anchor, above);
            m_shoulderSlot = slot;
            break;
        }
        moveEye = tried != kShoulderSlots;
        stickTurns = false;
    }

    // Is the wanted place in sight of the anchor? Faces with
    // StageCollision_CameraPass on both sides may lie between; a place behind
    // anything else is left after kBlockedForever frames at most.
    sceVu0FVECTOR hit;
    StageCollisionFace *front = scene->raycast(hit, anchor, wanted, 0, kCameraRayMask, kCameraRayFlags, skip);
    if (front == nullptr) {
        m_blockedFrames = 0;
    }
    else {
        sceVu0FVECTOR hitBack;
        const StageCollisionFace *back = scene->raycast(hitBack, wanted, hit, 0, kCameraRayMask, kCameraRayFlags, skip);
        bool passes = false;
        if (back == nullptr || !back->inFront(wanted)) {
            m_blockedFrames = kBlockedForever;
        }
        else if (vecDistanceSq(hitBack, wanted) > kBackFaceFarSq) {
            passes = (front->flags & StageCollision_CameraPass) != 0 && (back->flags & StageCollision_CameraPass) != 0;
        }
        else {
            m_blockedFrames = kBlockedForever;
        }
        if (passes) {
            m_blockedFrames = 0;
        }
        else if (m_blockedFrames < kBlockedForever
                 && !scene->segmentHitsAny(wanted, m_xform.position, 0, kCameraRayMask, kCameraRayFlags, skip)) {
            m_blockedFrames++;
        }
        else {
            wanted[0] = hit[0];
            wanted[1] = hit[1];
            if (scene->raycast(hit, anchor, wanted, 0, kCameraRayMask, kCameraRayFlags, skip) != nullptr) {
                vecCopy(wanted, hit);
                m_blockedFrames = kBlockedForever;
            }
        }
    }

    // Keep the eye off walls to its sides: across the view, a short way out
    // each way, stepping back by the same.
    sceVu0FVECTOR side = { focusPoint[1] - wanted[1], wanted[0] - focusPoint[0], 0.0f, 0.0f };
    vecNormalize(side, side);
    sceVu0FVECTOR probeTo;
    sceVu0FVECTOR probeFrom;
    scaleVec4(kWallProbeOut, probeTo, side);
    scaleVec4(-kWallProbeIn, probeFrom, side);
    for (int i = 0; i < 4; i++) {
        probeTo[i] += wanted[i];
        probeFrom[i] += wanted[i];
    }
    bool nearWall = false;
    sceVu0FVECTOR wallHit;
    const StageCollisionFace *wall = scene->raycast(wallHit, probeFrom, probeTo, 0, kCameraRayMask, kCameraRayFlags, skip);
    if (wall != nullptr && vecDot(side, wall->plane) < 0.0f) {
        scaleVec4(-kWallProbeOut, probeTo, side);
        for (int i = 0; i < 4; i++)
            wanted[i] = wallHit[i] + probeTo[i];
        nearWall = true;
    }
    scaleVec4(-kWallProbeOut, probeTo, side);
    for (int i = 0; i < 4; i++) {
        probeTo[i] += wanted[i];
        probeFrom[i] = wanted[i] + side[i];
    }
    wall = scene->raycast(wallHit, probeFrom, probeTo, 0, kCameraRayMask, kCameraRayFlags, skip);
    if (wall != nullptr && vecDot(side, wall->plane) > 0.0f) {
        scaleVec4(kWallProbeOut, probeTo, side);
        for (int i = 0; i < 4; i++)
            wanted[i] = wallHit[i] + probeTo[i];
        nearWall = true;
    }
    if (nearWall) {
        if (m_nearWall == 0)
            m_nearWall = 1;
    }
    else {
        m_nearWall = 0;
    }

    // Any character's volume the eye would enter: stay above it, at once.
    bool aboveVolume = false;
    StageObjArray &characters = scene->objects->m_lists[StageObjGroup_Characters];
    for (StageObject **slot = nextActive(characters.begin()); slot != nullptr;
         slot = nextActive(StageObjArray::next(slot))) {
        const CollisionBody *volume = (*slot)->m_ground->volume;
        if (volume != nullptr && volume->boxed != 0 && boxHolds(volume, wanted, kVolumeMargin)) {
            aboveVolume = true;
            wanted[2] = kVolumeMargin[2] + volume->box[1][2];
        }
    }

    if (moveEye) {
        sceVu0FVECTOR oldPosition;
        vecCopy(oldPosition, m_xform.position);
        if (m_snap != 0 || aboveVolume) {
            vecCopy(m_xform.position, wanted);
        }
        else {
            sceVu0FVECTOR looking;
            sceVu0FVECTOR going;
            vecDirection(looking, m_lookTarget, m_xform.position);
            vecDirection(going, wanted, m_xform.position);
            const float towards = vecDot(looking, going);
            if (towards > 0.0f)
                followDivisor = towards * kFacingEaseMax + followDivisor + 0.0f;
            approachVec3(1.0f / followDivisor, kFollowMaxStep, m_xform.position, m_xform.position, wanted);
        }

        const CollisionBody *volume = linked->m_ground->volume;
        if (volume != nullptr && volume->boxed != 0 && boxHolds(volume, m_xform.position, kVolumeMargin))
            m_xform.position[2] = kVolumeMargin[2] + volume->box[1][2];

        // The eased eye may have gone behind something the wanted place is
        // not: then it stops at what it hit, unless that may be passed.
        sceVu0FVECTOR stopped;
        const StageCollisionFace *face = scene->raycast(stopped, anchor, m_xform.position, 0, kCameraRayMask,
                                                        kCameraRayFlags, skip);
        if (face != nullptr
            && (m_blockedFrames >= kBlockedForever
                || scene->segmentHitsAny(oldPosition, wanted, 0, kCameraRayMask, kCameraRayFlags, skip))) {
            bool passes = false;
            if ((face->flags & StageCollision_CameraPass) != 0) {
                sceVu0FVECTOR hitBack;
                const StageCollisionFace *back = scene->raycast(hitBack, wanted, stopped, 0, kCameraRayMask,
                                                                kCameraRayFlags, skip);
                passes = back != nullptr && back->inFront(wanted)
                      && vecDistanceSq(hitBack, wanted) > kBackFaceFarSqEye
                      && (back->flags & StageCollision_CameraPass) != 0;
            }
            if (!passes)
                vecCopy(m_xform.position, stopped);
        }

        if (m_snap == 0)
            approachVec3(kLookEaseRate, kLookMaxStep, m_lookTarget, m_lookTarget, focusPoint);
        else
            vecCopy(m_lookTarget, focusPoint);
        aimFrom(m_lookTarget, m_xform.position);
    }

    if (linked->m_290 == nullptr) {
        sceVu0FVECTOR turn = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (stickTurns) {
            turn[2] = yawStick * kYawStickEyeRange;
            if (pitchStick > 0.0f)
                turn[0] = pitchStick * (kPitchUpLimit - m_xform.rotation[0]);
            else
                turn[0] = pitchStick * (m_xform.rotation[0] + kPitchDownLimit);
        }
        if (m_snap == 0) {
            approachVec3(kTurnEaseRate, kTurnMaxStep, m_turn, m_turn, turn);
        }
        else {
            m_turn[0] = turn[0];
            m_turn[1] = 0.0f;
            m_turn[2] = turn[2];
            m_turn[3] = 0.0f;
        }
        addAngles(m_xform.rotation, m_xform.rotation, m_turn);
    }
    m_snap = 0;
}

// FS2 0x001776d0
// callcheck: acosf (vecDot)

bool StageCamera::seesPoint(const sceVu0FVECTOR point)
{
    sceVu0FVECTOR ahead = { 0.0f, 1.0f, 0.0f, 1.0f };   /* data 0x2f7b00 */
    applyMatrix(ahead, getMatrix(), ahead);
    vecDirection(ahead, ahead, getMatrix()[3]);
    sceVu0FVECTOR toPoint;
    vecDirection(toPoint, point, getMatrix()[3]);
    const float angle = eeAcosf(vecDot(toPoint, ahead));
    const float side = m_screenWidth < m_screenHeight ? m_screenHeight : m_screenWidth;
    return angle <= atan2Octant(m_scrZ, side / 2.0f);
}

// Written in headers or left to the host; tools/reverse reads tags from .cpp files only:
//   FS2 0x001775b0  StageCamera::getKind (gamehooks.h)
//   FS2 0x001775c0  ScriptRunner::getScriptMapObject (scriptrunner.h), the base's answer
//   FS2 0x001775e0  StageCamera::sideHook3, empty as the base's
//   FS2 0x001775f0  StageCamera::sideHook2, empty as the base's
//   FS2 0x00177600  StageCamera::sideHook1, empty as the base's
//   FS2 0x00177610  StageCamera::sideHook0, empty as the base's
//   FS2 0x00177620  StageObject::setYaw (gamehooks.h)
//   FS2 0x00177630  StageObject::setRotation (gamehooks.h)
//   FS2 0x00177640  StageObject::setPosition (gamehooks.h)
//   FS2 0x00177650  StageCamera's dtor: the compiler's
