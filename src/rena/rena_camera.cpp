/* ==========================================================================
 *  rena/rena_camera.cpp
 *
 *  rena's camera base and its box test (rena_camera.h). The base's ctor
 *  0x2e2970 is DrawSysCamera::DrawSysCamera in game/drawsys.cpp.
 * ======================================================================== */
#include "rena/rena_camera.h"

#include <math.h>

namespace {

// How a box lies to one plane: VCLIPw.xyz of (distance, -, -, reach), the
// clip flag's x bits.
enum PlaneSide
{
    PlaneSide_Crossing = 0,     // neither: the plane runs through the box
    PlaneSide_Inner    = 1,     // +x: distance > reach
    PlaneSide_Outer    = 2,     // -x: distance < -reach
};

// The placed box: its centre and its three half axes, each a row of the
// matrix the ROM transposes into vf16..vf19 (pextlw / pextuw / pcpyld /
// pcpyud) so one VMULA chain gives all four dots against a plane.
// layout: host (registers in the ROM).
struct PlacedBox
{
    float center[4];
    float axes[3][4];
};

int planeSide(const PlacedBox &box, const float *plane)
{
    float dots[4];
    const float *rows[4] = { box.center, box.axes[0], box.axes[1], box.axes[2] };
    for (int i = 0; i < 4; i++)
        dots[i] = rows[i][0] * plane[0] + rows[i][1] * plane[1] + rows[i][2] * plane[2] + rows[i][3] * plane[3];
    const float distance = dots[0];
    // vabs.yzw; vadday.w (w + y); vmaddz.w (+ z * vf0.w)
    const float reach = (fabsf(dots[3]) + fabsf(dots[1])) + fabsf(dots[2]);
    if (distance > reach)
        return PlaneSide_Inner;
    if (distance < -reach)
        return PlaneSide_Outer;
    return PlaneSide_Crossing;
}

} // namespace

// The box through the matrix: the centre placed (with its own w), each half
// size along its axis. Then each frame plane: wholly outside one -> Outside;
// the planes it crosses are kept (a stack int[8]) and, when there are any,
// held against the guard band's same planes: outside one -> Outside, crossing
// one -> Crossing, else Inside (the band takes it unclipped).
// FS2 0x002e2a60

int boxClipTest(const sceVu0FVECTOR *box, const sceVu0FVECTOR *matrix, const RenaClipView *view)
{
    PlacedBox placed;
    for (int i = 0; i < 4; i++) {
        placed.center[i] = matrix[0][i] * box[0][0] + matrix[1][i] * box[0][1] + matrix[2][i] * box[0][2]
                         + matrix[3][i] * box[0][3];
        for (int axis = 0; axis < 3; axis++)
            placed.axes[axis][i] = matrix[axis][i] * box[1][axis];
    }

    int crossed[8];
    int count = 0;
    for (int p = 0; p < RenaClipView::kPlaneCount; p++) {
        const int side = planeSide(placed, view->framePlanes[p]);
        if (side == PlaneSide_Outer)
            return BoxClip_Outside;
        if (side == PlaneSide_Crossing)
            crossed[count++] = p;
    }
    if (count == 0)
        return BoxClip_Inside;

    bool clips = false;
    for (int i = 0; i < count; i++) {
        const int side = planeSide(placed, view->guardPlanes[crossed[i]]);
        if (side == PlaneSide_Outer)
            return BoxClip_Outside;
        if (side == PlaneSide_Crossing)
            clips = true;
    }
    return clips ? BoxClip_Crossing : BoxClip_Inside;
}
