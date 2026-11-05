/* ==========================================================================
 *  game/viewcamera.cpp -- the view from a character's eyes
 *  (game/viewcamera.h says what it is). 0x29a730..0x29acb0, less setZoom,
 *  which game/playermotion.cpp keeps beside the scope. The file name is the
 *  port's.
 *
 *  A stage is in this view when its script sets the show flag ForceView:
 *  stage 1's Root does (StageObject::setProperty), and CharacterInput::stepView
 *  asks for it every frame then.
 * ======================================================================== */
#include <math.h>
#include <string.h>

#include "game/modelpose.h"
#include "game/playermotion.h"
#include "game/viewcamera.h"
#include "game/vu0math.h"

constexpr float kViewCameraScrZ   = 450.0f;
constexpr float kViewCameraWidth  = 512.0f;
constexpr float kViewCameraHeight = 512.0f;
constexpr float kViewCameraFar    = 1600.0f;
constexpr float kViewScrZ         = 350.0f; // what the view starts with; setZoom's near end
constexpr float kMountTurn        = 3.1415927f;
// How far the eye moves forward with a full lean (m_leanScale runs from 1).
constexpr float kLeanForward      = 5.0f;

// The ROM's two parent links are the same inline code.
// FS2 0x0029aae0
// callcheck: StageCamera, StageNodeBase, NodeTransform, setScrZ, setRotation, erase, erase, pushBack (setParent is the erases and the push)

ViewCamera::ViewCamera()
    : StageCamera(kViewCameraScrZ, kViewCameraWidth, kViewCameraHeight, kViewCameraFar)
{
    m_linked = nullptr;
    setScrZ(kViewScrZ);
    m_mount.setDirtyTree(false);
    m_mount.m_xform.setRotation(0.0f, 0.0f, kMountTurn);
    setParent(&m_mount);
}

// FS2 0x0029a9c0
// callcheck: onViewTop, erase, erase, pushBack, cameraStep (setParent is the erases and the push)

void ViewCamera::onViewTop()
{
    if (m_linked == nullptr)
        return;
    StageCamera::onViewTop();
    memcpy(m_xform.rotation, m_linked->m_motion->m_lampAngles, sizeof(m_xform.rotation));
    m_mount.setParent(m_linked);
    cameraStep();
}

// The turn is the mount's and the camera's under the character, so the
// character's matrices are rebuilt for it, with the bones first when the pose
// hangs from something; the place is then written over the row: nothing may
// rebuild it before the frame is drawn, so the tree ends clean.
// FS2 0x0029a8c0
// callcheck: setDirtyTree, solve, updateMatrix, setDirtyTree, getEyeMatrix, getWorldMatrix, applyMatrix (virtual slots; the world matrix is m_xform's)

void ViewCamera::cameraStep()
{
    if (m_linked == nullptr)
        return;
    PlayerMotion *motion = m_linked->m_motion;
    memcpy(m_xform.rotation, motion->m_lampAngles, sizeof(m_xform.rotation));
    m_xform.position[1] = (fabsf(motion->m_leanScale[0]) - 1.0f) * kLeanForward;

    m_mount.setDirtyTree(true);
    ModelPose *pose = m_linked->m_model->pose;
    if (pose->parentMatrix != nullptr)
        pose->solve(3, -1, 0);
    m_linked->updateMatrix();
    m_mount.setDirtyTree(false);

    sceVu0FMATRIX eye;
    motion->getEyeMatrix(eye);
    applyMatrix(m_xform.world[3], eye, m_xform.position);
}
