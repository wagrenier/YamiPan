// game/areagrid.cpp -- area boxes and their grids, the distance volume curve
// (game/areagrid.h).
#include "game/areagrid.h"

#include <math.h>
#include <string.h>

#include "game/mathutil.h"
#include "game/seplayer.h"  /* SeFalloff */
#include "game/vu0math.h"

// ──────────────────────────────────────────────────────────────────────
// The cell of a point offset from the grid's origin; the ROM inlines this
// in collect, contains and build (a truncating cast, then at most 7).
// PC: a flat grid has cells of size 0. On the EE x / 0 is +MAX and the cast
// saturates, so the cell is the last one; on x64 the cast answers INT_MIN.

static int areaCell(float offset, float cellSize)
{
    float cell = offset / cellSize;
    return cell < kAreaGridSide ? (int)cell : kAreaGridSide - 1;
}

// ──────────────────────────────────────────────────────────────────────
// Scales volume down the falloff curve; past ratio 1 nothing is heard. Any
// falloff but the two curves is linear.
// FS2 0x00283df0

u_int seDistanceVolume(float ratio, int falloff, u_int volume)
{
    if (!(ratio <= 1.0f))
        return 0;
    if (ratio < 0.0f)
        ratio = 0.0f;

    switch (falloff) {
    case SeFalloff_Sin:
        return (u_int)((float)volume * (1.0f - sinf(ratio * kMathHalfPi)));
    case SeFalloff_Cos:
        return (u_int)((float)volume * cosf(ratio * kMathHalfPi));
    default:
        return (u_int)((float)volume * (1.0f - ratio));
    }
}

// ──────────────────────────────────────────────────────────────────────
// Inline in the ROM's collect and contains: the bounds test on xyz, then
// the cell on xy.

StageList<AreaBox *>::Node *AreaGrid::cellAt(const sceVu0FVECTOR pos)
{
    if (m_boxes.count == 0)
        return nullptr;
    for (int i = 0; i < 3; i++) {
        if (pos[i] - m_min[i] < 0.0f || m_max[i] - pos[i] < 0.0f)
            return nullptr;
    }
    int x = areaCell(pos[0] - m_min[0], m_cellSizeX);
    int y = areaCell(pos[1] - m_min[1], m_cellSizeY);
    return m_cells[x + y * kAreaGridSide].head;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00283ff0

int AreaGrid::collect(const sceVu0FVECTOR pos, AreaBox **out, int max)
{
    if (max < 1)
        return 0;
    int found = 0;
    for (StageList<AreaBox *>::Node *node = cellAt(pos); node != nullptr; node = node->next) {
        AreaBox *box = node->value;
        if (box->contains(pos)) {
            *out++ = box;
            if (++found == max)
                break;
        }
    }
    return found;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00284190

AreaBox *AreaGrid::contains(const sceVu0FVECTOR pos)
{
    for (StageList<AreaBox *>::Node *node = cellAt(pos); node != nullptr; node = node->next) {
        AreaBox *box = node->value;
        if (box->contains(pos))
            return box;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The bounds grow box by box (getBounds over the old bounds and the box),
// the cells divide them, and each box joins every cell its bounds touch. A
// point on the far edge falls in the last cell.
// callcheck: FUN_001a5d00 (StageList::pushBack, an instance)
// FS2 0x002842f0

void AreaGrid::build()
{
    if (m_boxes.count == 0)
        return;

    m_boxes.head->value->getWorldBounds(m_min, m_max);
    for (StageList<AreaBox *>::Node *node = m_boxes.head; node != nullptr; node = node->next) {
        sceVu0FVECTOR points[4];
        memcpy(points[0], m_min, sizeof(sceVu0FVECTOR));
        memcpy(points[1], m_max, sizeof(sceVu0FVECTOR));
        node->value->getWorldBounds(points[2], points[3]);
        getBounds(points, 4, m_min, m_max);
    }

    m_cellSizeX = (m_max[0] - m_min[0]) / (float)kAreaGridSide;
    m_cellSizeY = (m_max[1] - m_min[1]) / (float)kAreaGridSide;

    for (StageList<AreaBox *>::Node *node = m_boxes.head; node != nullptr; node = node->next) {
        AreaBox *box = node->value;
        sceVu0FVECTOR boxMin, boxMax;
        box->getWorldBounds(boxMin, boxMax);

        int left   = areaCell(boxMin[0] - m_min[0], m_cellSizeX);
        int right  = areaCell(boxMax[0] - m_min[0], m_cellSizeX);
        int top    = areaCell(boxMin[1] - m_min[1], m_cellSizeY);
        int bottom = areaCell(boxMax[1] - m_min[1], m_cellSizeY);
        for (int y = top; y <= bottom; y++) {
            for (int x = left; x <= right; x++)
                m_cells[x + y * kAreaGridSide].pushBack(box);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// callcheck: FUN_001a5d00 (StageList::pushBack, an instance)
// FS2 0x00284560

void AreaGrid::addBox(AreaBox *box)
{
    m_boxes.pushBack(box);
}

// ──────────────────────────────────────────────────────────────────────
// The grid owns its boxes; the member dtors then free the cells and the list.
// callcheck: __destroy_arr, FUN_00186b20, operator_delete (the members' dtors, StageList's destructor, the deleting dtor's delete)
// FS2 0x00284580

AreaGrid::~AreaGrid()
{
    for (StageList<AreaBox *>::Node *node = m_boxes.head; node != nullptr; node = node->next)
        delete node->value;
}

// ──────────────────────────────────────────────────────────────────────
// Bounds and cell sizes stay unset until build; every query asks the count
// first.
// callcheck: FUN_00186b10, __construct_array (StageList's constructor, the member array's)
// FS2 0x00284640

AreaGrid::AreaGrid()
    : _238(1)
{
}

// ──────────────────────────────────────────────────────────────────────
// Tests in box space: the matrix is a placement, so its inverse is the
// rigid one.
// FS2 0x00284690

bool AreaBox::contains(const sceVu0FVECTOR pos)
{
    sceVu0FMATRIX inverse;
    invertRigidMatrix(inverse, getMatrix());

    sceVu0FVECTOR local;
    local[0] = pos[0];
    local[1] = pos[1];
    local[2] = pos[2];
    local[3] = 1.0f;
    applyMatrix(local, inverse, local);

    for (int i = 0; i < 3; i++) {
        if (!(m_min[i] <= local[i] && local[i] <= m_max[i]))
            return false;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00284740

void AreaBox::getWorldBounds(sceVu0FVECTOR min, sceVu0FVECTOR max)
{
    sceVu0FVECTOR corners[8] = {
        { m_min[0], m_min[1], m_min[2], 1.0f },
        { m_max[0], m_min[1], m_min[2], 1.0f },
        { m_min[0], m_min[1], m_max[2], 1.0f },
        { m_max[0], m_min[1], m_max[2], 1.0f },
        { m_min[0], m_max[1], m_min[2], 1.0f },
        { m_max[0], m_max[1], m_min[2], 1.0f },
        { m_min[0], m_max[1], m_max[2], 1.0f },
        { m_max[0], m_max[1], m_max[2], 1.0f },
    };
    for (int i = 0; i < 8; i++)
        applyMatrix(corners[i], getWorldMatrix(), corners[i]);
    getBounds(corners, 8, min, max);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002848d0

void AreaBox::setSize(float width, float height, float depth)
{
    m_min[0] = -width / 2.0f;
    m_min[1] = -depth / 2.0f;
    m_min[2] = height < 0.0f ? height : 0.0f;
    m_max[0] = width / 2.0f;
    m_max[1] = depth / 2.0f;
    m_max[2] = height < 0.0f ? 0.0f : height;
}

// ──────────────────────────────────────────────────────────────────────
// callcheck: StageNodeBase, NodeTransform (StageNode's implicit ctor)
// FS2 0x00284950

AreaBox::AreaBox()
    : m_min(), m_max()
{
}
