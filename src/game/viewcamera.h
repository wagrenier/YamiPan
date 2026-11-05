/* ==========================================================================
 *  game/viewcamera.h
 *
 *  The view from a character's eyes: "camview", "camview_zoom" and
 *  "camview_snipe" (kind 10, vtable 0x3185b0, EE 0x440; setupCameras makes
 *  the three). The file name is the port's. It hangs from the character it
 *  is linked to through a node of its own and turns with the character's
 *  lean; its place is the eye, past bn_head (PlayerMotion::getEyeMatrix).
 *  It does not hide the body: StageScene::prepareDrawFlags hides the head.
 * ======================================================================== */
#ifndef _GAME_VIEWCAMERA_H
#define _GAME_VIEWCAMERA_H

#include "game/gamehooks.h"
#include "game/stagenode.h"

// layout: host -- EE 0x440; the kind 8 class under it adds m_linked (+0x300).
class ViewCamera : public StageCamera
{
public:
    // FS2 0x0029a830
    int getKind() override { return StageObject_Kind10; }
    ViewCamera();                                                        // FS2 0x0029aae0
    // FS2 0x0029a730: deleting; 0x0029aca0 the runner's thunk. The mount goes
    // first, and clears the camera's link to it.
    ~ViewCamera() override = default;
    // Takes the character's turn, hangs from it and steps.
    void onViewTop() override;                                           // FS2 0x0029a9c0
    // The character's matrices with the camera under them, then the eye.
    void cameraStep() override;                                          // FS2 0x0029a8c0
    // The step after the action's, again before the frame's camera step (the
    // ROM's thunk falls into cameraStep).
    // FS2 0x0029ac90
    void sideHook3() override { cameraStep(); }

    // The zoom of camview_zoom, from the character's zoomLevel.
    void setZoom(float level);                                           // FS2 0x0029a840
    // The analog range setZoom spreads over: low, then its width.
    static constexpr float kZoomLow  = 76.8f;                            // sbss gp-0x7a00: 256 * 30 / 100
    static constexpr float kZoomSpan = 101.4f;                           // sbss gp-0x79fc: 255 - 2 * low

    // Between the character and the camera, half a turn round z: it is what
    // onViewTop moves to the character.
    StageNode m_mount;          // FS2 +0x340 (its transform +0x380)
};

#endif // _GAME_VIEWCAMERA_H
