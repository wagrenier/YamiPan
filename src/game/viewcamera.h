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
    // The analog range setZoom spreads over: low, then its width. In the ROM
    // a header's file statics: every file that includes it has its own int
    // (sdata, 30) and float pair (sbss), filled by that file's static
    // initialiser (0x310a20, 0x310a70, 0x310b10, 0x310ec0, 0x310f10); this
    // class's copy is sbss gp-0x7a00 / gp-0x79fc. The EE rounds toward zero,
    // so the ROM holds 76.79999542 / 101.40000916, one ulp from the host's
    // 76.8f / 101.4f (architecture.md "Static initialisers").
    static constexpr int   kZoomPercent = 30;
    static constexpr float kZoomLow  = kZoomPercent * 256.0f / 100.0f;
    static constexpr float kZoomSpan = 255.0f - kZoomLow * 2.0f;

    // Between the character and the camera, half a turn round z: it is what
    // onViewTop moves to the character.
    StageNode m_mount;          // FS2 +0x340 (its transform +0x380)
};

#endif // _GAME_VIEWCAMERA_H
