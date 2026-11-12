/* ==========================================================================
 *  game/areaclip.cpp
 *
 *  What the camera's place hides (areaclip.h). The grid and the lists are
 *  the ROM's; the MSL containers they were built on are std:: here.
 * ======================================================================== */
#include "game/areaclip.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

#include "game/drawsys.h"
#include "game/stageloop.h"
#include "game/stageobjlist.h"
#include "game/stageparticle.h"
#include "game/stagescene.h"

#ifdef _MSC_VER
// PC: the ROM's libc strcasecmp (0x127ea0); MSVC has no such names.
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

namespace {

constexpr float kClipEpsilon = 0.0001f;
constexpr float kUintBoundary = 2147483648.0f;
constexpr int kBoxCorners = 8;
constexpr int kBoxFaces = 6;
constexpr int kFaceCorners = 4;
constexpr int kCellEdges = 4;

// The box's faces, by the bits of its corners (bit 0 x, 1 y, 2 z).
const int kFaceOf[kBoxFaces][kFaceCorners] = {
    {0, 1, 3, 2}, {4, 5, 7, 6}, {0, 2, 6, 4}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 3, 7, 6},
};

// matrix * place, inside the cube -1..1: VCLIP, so the faces count as inside.
// w is the place's own.
// FS2 0x002dac70

bool clipBoxContains(const float (*box)[4], const float *place)
{
    for (int axis = 0; axis < 3; axis++) {
        float value = box[0][axis] * place[0] + box[1][axis] * place[1]
                    + box[2][axis] * place[2] + box[3][axis] * place[3];
        if (value > 1.0f || value < -1.0f)
            return false;
    }
    return true;
}

// (x, y) on the same side of all four edges of the quad, as seen from above.
// FS2 0x002db9f0

bool clipQuadContains(float x, float y, const float *const *quad)
{
    bool side = false;
    for (int i = 0; i < kFaceCorners; i++) {
        const float *from = quad[i];
        const float *to = quad[(i + 1) % kFaceCorners];
        float dx = to[0] - from[0];
        float dy = to[1] - from[1];
        bool here = dx * (y - from[1]) + dy * -(x - from[0]) < 0.0f;
        if (i == 0)
            side = here;
        else if (here != side)
            return false;
    }
    return true;
}

// A segment in the plane: a line y = slope x + offset over x in lo..hi, or,
// nearly upright, x = place over y in lo..hi. layout: host (locals of the
// ROM's function).
struct ClipLine
{
    bool  upright;
    float slope;
    float offset;
    float place;
    float lo;
    float hi;
};

// The ROM's inline, twice in 0x2db540.
ClipLine makeLine(const float *a, const float *b)
{
    if (b[0] < a[0])
        std::swap(a, b);
    ClipLine line = {};
    float dx = b[0] - a[0];
    line.upright = !(kClipEpsilon < std::fabs(dx));
    if (!line.upright) {
        line.slope = (b[1] - a[1]) / dx;
        line.offset = a[1] - a[0] * line.slope;
        line.lo = std::min(a[0], b[0]);
        line.hi = std::max(a[0], b[0]);
    } else {
        line.place = a[0];
        line.lo = std::min(a[1], b[1]);
        line.hi = std::max(a[1], b[1]);
    }
    return line;
}

// The ROM's inline, the same whichever of the two is upright.
bool uprightCrosses(const ClipLine &upright, const ClipLine &line)
{
    float y = upright.place * line.slope + line.offset;
    return !(upright.place < line.lo || line.hi < upright.place
             || y < upright.lo || upright.hi < y);
}

bool linesCross(const ClipLine &a, const ClipLine &b)
{
    if (a.upright && b.upright)
        return std::fabs(a.place - b.place) < kClipEpsilon;
    if (a.upright)
        return uprightCrosses(a, b);
    if (b.upright)
        return uprightCrosses(b, a);
    float slopeGap = a.slope - b.slope;
    float offsetGap = b.offset - a.offset;
    if (std::fabs(slopeGap) < kClipEpsilon) {
        // Parallel: only the same line can meet, where x's ranges overlap.
        if (kClipEpsilon <= std::fabs(offsetGap))
            return false;
        return b.lo <= a.hi && !(b.hi < a.lo);
    }
    float x = offsetGap / slopeGap;
    return !(x < a.lo || a.hi < x || x < b.lo || b.hi < x);
}

// The segment a-b meets an edge of the quad.
// FS2 0x002db540
// callcheck: swap, abs, min, max, swap, abs, min, max, min, max, min, max, abs, abs, abs (std:: inlined in makeLine and linesCross; abs is FS2 0x002db9e0)

bool clipQuadCrossesSegment(const float *const *quad, const float *a, const float *b)
{
    ClipLine segment = makeLine(a, b);
    for (int i = 0; i < kFaceCorners; i++) {
        ClipLine edge = makeLine(quad[(i + 1) % kFaceCorners], quad[i]);
        if (linesCross(segment, edge))
            return true;
    }
    return false;
}

// A float as the ROM's cast to unsigned: below zero it stays huge, past the
// sign bit it carries on, and -0.5 is 0.
u_int toCell(float value)
{
    if (value < kUintBoundary)
        return (u_int)(int)value;
    return (u_int)(int)(value - kUintBoundary) | 0x80000000u;
}

// The instance's mesh called name, whole: the search's length may let a
// longer name through. The ROM's inline, three times in findTarget.
ClipTarget *findMesh(ModelInstance *model, const char *name, size_t length)
{
    ModelPartRec *part = modelFindPart(model, name, length, 0);
    if (part == nullptr)
        return nullptr;
    const char *meshName = model->node->meshes.get()[part - model->partRecs].name;
    return strlen(name) == strlen(meshName) ? part : nullptr;
}

}  // namespace

// FS2 0x002dceb0

void AreaClipFile::relocate()
{
    if ((magic | kAreaClipMagicFold) != kAreaClipFileMagic || magic == kAreaClipRelocated)
        return;
    magic = kAreaClipRelocated;
    char *base = reinterpret_cast<char *>(this);
    boxes.set(base + boxes.rel);
    targetNames.set(base + targetNames.rel);
    for (u_int i = 0; i < targetCount; i++)
        targetNames.get()[i].set(base + targetNames.get()[i].rel);
    hiddenTargets.set(base + hiddenTargets.rel);
    for (u_int i = 0; i < boxCount; i++)
        hiddenTargets.get()[i].set(base + hiddenTargets.get()[i].rel);
    if (hiddenBoxes.rel != 0) {
        hiddenBoxes.set(base + hiddenBoxes.rel);
        for (u_int i = 0; i < boxCount; i++) {
            S2dRef<short> &list = hiddenBoxes.get()[i];
            if (list.rel != 0)
                list.set(base + list.rel);
        }
    }
}

// FS2 0x002dd050
// callcheck: findLive, setEmitting

void ClipEffect::setClipped(u_char clipped)
{
    if (gStageScene == nullptr || gStageScene->nodeTree == nullptr)
        return;
    StageParticle *effect = gStageScene->nodeTree->findLive(m_name);
    if (effect != nullptr)
        effect->setEmitting(clipped == 0);
}

// What a name of the file is, first found wins: the lead's model or a mesh of
// it, a model named far.. or a mesh of it, a character's model, any model or
// its mesh, a particle effect (kept in m_effects).
// FS2 0x002dacc0
// callcheck: strcasecmp, modelFindPart, strlen, strlen, drawSysCurrent, drawSysCurrent, strncasecmp, strcasecmp, strlen, modelFindPart, strlen, strlen, findCharacter, drawSysCurrent, drawSysCurrent, strcasecmp, modelFindPart, strlen, strlen, effectOffset, operator_new, ClipEffect, insert (the meshes' calls are findMesh; push_back is the insert)

ClipTarget *AreaClip::findTarget(const char *name)
{
    // PC: the ROM reads through both; low memory answers 0 on the EE.
    StageObject *root = gStageScene->rootAny;
    ModelInstance *lead = root != nullptr && root->m_model != nullptr ? root->m_model->data : nullptr;
    if (lead != nullptr) {
        if (strcasecmp(lead->node->name, name) == 0)
            return lead;
        ClipTarget *mesh = findMesh(lead, name, kAreaClipWholeName);
        if (mesh != nullptr)
            return mesh;
    }

    int count = (int)drawSysCurrent()->models.size();
    for (int i = 0; i < count; i++) {
        ModelInstance *model = drawSysCurrent()->models[i];
        if (strncasecmp(model->node->name, "far", 3) != 0)
            continue;
        if (strcasecmp(model->node->name, name) == 0)
            return model;
        ClipTarget *mesh = findMesh(model, name, strlen(name));
        if (mesh != nullptr)
            return mesh;
    }

    StageObject *character = gStageScene->objects->findCharacter(name, 0);
    if (character != nullptr)
        return character->m_model != nullptr ? character->m_model->data : nullptr;

    count = (int)drawSysCurrent()->models.size();
    for (int i = 0; i < count; i++) {
        ModelInstance *model = drawSysCurrent()->models[i];
        if (strcasecmp(model->node->name, name) == 0)
            return model;
        ClipTarget *mesh = findMesh(model, name, kAreaClipWholeName);
        if (mesh != nullptr)
            return mesh;
    }

    if (gStageScene == nullptr || gStageScene->nodeTree == nullptr
        || gStageScene->nodeTree->effectOffset(name) == nullptr)
        return nullptr;
    ClipEffect *effect = new ClipEffect(name);
    m_effects.push_back(effect);
    return effect;
}

// Does the box, seen from above, touch the cell? A corner of the box in the
// cell, a corner of the cell in a face of the box, or an edge of the cell
// across an edge of a face.
// FS2 0x002db080
// callcheck: clipQuadContains, clipQuadCrossesSegment

bool AreaClip::boxTouchesCell(const float (*box)[4], u_int cellX, u_int cellY) const
{
    u_int cells = (u_int)m_cells.size();
    if (cellX >= cells || cellY >= cells)
        return false;

    // The box's matrix inverted by hand, as the ROM does: the rows of the
    // 3 x 3 are the columns here.
    const float a[3] = {box[0][0], box[1][0], box[2][0]};
    const float b[3] = {box[0][1], box[1][1], box[2][1]};
    const float c[3] = {box[0][2], box[1][2], box[2][2]};
    const float bc[3] = {b[1] * c[2] - b[2] * c[1], b[2] * c[0] - b[0] * c[2], b[0] * c[1] - b[1] * c[0]};
    const float ca[3] = {c[1] * a[2] - c[2] * a[1], c[2] * a[0] - c[0] * a[2], c[0] * a[1] - c[1] * a[0]};
    const float ab[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    float scale = 1.0f / (bc[0] * a[0] + bc[1] * a[1] + bc[2] * a[2]);
    float inverse[4][3];
    for (int i = 0; i < 3; i++) {
        inverse[0][i] = bc[i] * scale;
        inverse[1][i] = ca[i] * scale;
        inverse[2][i] = ab[i] * scale;
    }
    for (int i = 0; i < 3; i++)
        inverse[3][i] = inverse[0][i] * -box[3][0] + inverse[1][i] * -box[3][1] + inverse[2][i] * -box[3][2];

    // Its eight corners in the world; only x and y are read.
    float corners[kBoxCorners][3];
    for (int i = 0; i < kBoxCorners; i++) {
        float x = (i & 1) != 0 ? 1.0f : -1.0f;
        float y = (i & 2) != 0 ? 1.0f : -1.0f;
        float z = (i & 4) != 0 ? 1.0f : -1.0f;
        for (int axis = 0; axis < 3; axis++)
            corners[i][axis] = inverse[0][axis] * x + inverse[1][axis] * y + inverse[2][axis] * z + inverse[3][axis];
    }

    float width, height;
    getCellSize(width, height);
    float x0 = width * (float)(int)cellX + m_file->min[0];
    float x1 = width * (float)(int)(cellX + 1) + m_file->min[0];
    float y0 = height * (float)(int)cellY + m_file->min[1];
    float y1 = height * (float)(int)(cellY + 1) + m_file->min[1];

    for (int i = 0; i < kBoxCorners; i++) {
        if (x0 <= corners[i][0] && corners[i][0] < x1 && y0 <= corners[i][1] && corners[i][1] < y1)
            return true;
    }

    const float *faces[kBoxFaces][kFaceCorners];
    for (int face = 0; face < kBoxFaces; face++)
        for (int i = 0; i < kFaceCorners; i++)
            faces[face][i] = corners[kFaceOf[face][i]];

    for (int face = 0; face < kBoxFaces; face++) {
        for (int i = 0; i < kFaceCorners; i++) {
            if (clipQuadContains((i & 1) != 0 ? x1 : x0, (i & 2) != 0 ? y1 : y0, faces[face]))
                return true;
        }
    }

    const float edges[kCellEdges][4] = {
        {x0, y0, x1, y0}, {x0, y1, x1, y1}, {x0, y0, x0, y1}, {x1, y0, x1, y1},
    };
    for (int face = 0; face < kBoxFaces; face++) {
        for (int i = 0; i < kCellEdges; i++) {
            if (clipQuadCrossesSegment(faces[face], &edges[i][0], &edges[i][2]))
                return true;
        }
    }
    return false;
}

void AreaClip::getCellSize(float &width, float &height) const
{
    float cells = (float)(u_int)m_cells.size();
    width = (m_file->max[0] - m_file->min[0]) / cells;
    height = (m_file->max[1] - m_file->min[1]) / cells;
}

u_int AreaClip::cellOf(float place, int axis, float size) const
{
    return toCell((place - m_file->min[axis]) / size);
}

bool AreaClip::boxesHide(short target, const IndexList &boxes) const
{
    for (u_short box : boxes) {
        for (const short *list = m_file->hiddenTargets.get()[box]; *list != kAreaClipListEnd; list++) {
            if (*list == target)
                return true;
        }
    }
    return false;
}

// FS2 0x002dbd10
// callcheck: reserve, clipBoxContains, insert, sort, ClipSet, lower_bound, insert, ClipSet, reserve, lower_bound, insert, insert, operator_delete, ClipSet, ClipSet, find, insert, mslTreeIncrement, find, insert, mslTreeIncrement, setClipped, mslTreeIncrement, setClipped, mslTreeIncrement, eraseTree, eraseTree, resize, insert, reserve, insert, sort, unique, erase, lower_bound, clipBoxContains, operator_delete, eraseTree, eraseTree, operator_delete (std:: instances, setClipped through the vptr)

void AreaClip::update(StageObject *camera)
{
    if (m_file == nullptr)
        return;

    IndexList inside;
    inside.reserve(m_file->boxCount);
    sceVu0FVECTOR eye = {0.0f, 0.0f, 0.0f, 1.0f};
    if (camera != nullptr)
        memcpy(eye, camera->m_xform.position, sizeof(float) * 3);

    u_int cells = (u_int)m_cells.size();
    float width, height;
    getCellSize(width, height);
    // No camera: -1 is no cell.
    u_int cellX = camera != nullptr ? cellOf(eye[0], 0, width) : toCell(-1.0f);
    u_int cellY = camera != nullptr ? cellOf(eye[1], 1, height) : toCell(-1.0f);
    if (cellX < cells && cellY < cells) {
        for (u_short box : m_cells[cellX][cellY]) {
            if (clipBoxContains(m_file->boxes.get()[box], eye))
                inside.push_back(box);
        }
        std::sort(inside.begin(), inside.end());
    }

    // What the boxes left hid and no box the camera is in hides.
    std::set<u_short> leaving;
    for (u_short box : m_boxes) {
        if (std::binary_search(inside.begin(), inside.end(), box))
            continue;
        for (const short *list = m_file->hiddenTargets.get()[box]; *list != kAreaClipListEnd; list++) {
            if (!boxesHide(*list, inside))
                leaving.insert((u_short)*list);
        }
    }

    // What the boxes entered hide.
    std::set<u_short> entering;
    for (u_short box : inside) {
        if (std::binary_search(m_boxes.begin(), m_boxes.end(), box))
            continue;
        for (const short *list = m_file->hiddenTargets.get()[box]; *list != kAreaClipListEnd; list++)
            entering.insert((u_short)*list);
    }

    // The ROM takes both differences before it calls anything.
    std::set<u_short> show, hide;
    for (u_short target : leaving) {
        if (entering.count(target) == 0)
            show.insert(target);
    }
    for (u_short target : entering) {
        if (leaving.count(target) == 0)
            hide.insert(target);
    }
    for (u_short index : show) {
        if (m_targets[index] != nullptr)
            m_targets[index]->setClipped(0);
    }
    for (u_short index : hide) {
        if (m_targets[index] != nullptr)
            m_targets[index]->setClipped(1);
    }

    m_boxes = inside;

    if (m_file->hiddenBoxes.get() == nullptr)
        return;

    // The boxes characters hide in while the camera is in these.
    IndexList hiding;
    hiding.reserve(kAreaClipHidingSpace);
    for (u_short box : m_boxes) {
        const short *list = m_file->hiddenBoxes.get()[box];
        if (list == nullptr)
            continue;
        for (; *list != kAreaClipListEnd; list++)
            hiding.push_back((u_short)*list);
    }
    std::sort(hiding.begin(), hiding.end());
    hiding.erase(std::unique(hiding.begin(), hiding.end()), hiding.end());

    StageObjArray &characters = gStageScene->objects->m_lists[StageObjGroup_Characters];
    for (StageObject **slot = characters.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *character = *slot;
        if (!character->m_active)
            continue;
        ModelInstance *model = character->m_model != nullptr ? character->m_model->data : nullptr;
        if (model == nullptr)
            continue;
        model->clipped = 0;
        const float *place = character->m_xform.position;
        u_int x = cellOf(place[0], 0, width);
        u_int y = cellOf(place[1], 1, height);
        if (x >= cells || y >= cells)
            continue;
        for (u_short box : m_cells[x][y]) {
            if (std::binary_search(hiding.begin(), hiding.end(), box)
                && clipBoxContains(m_file->boxes.get()[box], place)) {
                model->clipped = 1;
                break;
            }
        }
    }
}

// FS2 0x002dcab0
// callcheck: operator_delete, operator_delete, operator_delete, ~ClipGrid, operator_delete (the member vectors)

AreaClip::~AreaClip()
{
    for (ClipTarget *effect : m_effects)
        delete effect;
}

// FS2 0x002dcc30
// callcheck: resize, findTarget

void AreaClip::setup()
{
    if (m_file == nullptr)
        return;
    m_targets.resize(m_file->targetCount);
    for (u_int i = 0; i < m_file->targetCount; i++)
        m_targets[i] = findTarget(m_file->targetNames.get()[i]);
}

// FS2 0x002dccf0
// callcheck: relocate, resize, resize, boxTouchesCell, insert, sort (push_back is the insert)

AreaClip::AreaClip(void *file) : m_file(static_cast<AreaClipFile *>(file))
{
    if (m_file == nullptr)
        return;
    m_file->relocate();
    m_cells.resize(kAreaClipCells);
    for (int x = 0; x < kAreaClipCells; x++) {
        m_cells[x].resize(kAreaClipCells);
        for (int y = 0; y < kAreaClipCells; y++) {
            IndexList &cell = m_cells[x][y];
            for (u_int box = 0; box < m_file->boxCount; box++) {
                if (boxTouchesCell(m_file->boxes.get()[box], x, y))
                    cell.push_back((u_short)box);
            }
            std::sort(cell.begin(), cell.end());
        }
    }
}

// The instances the object emitted, std:: here (ties cannot show: equal
// u_shorts are the same u_short, so the MSL sort need not be kept):
// FS2 0x002db030: ClipTarget's dtor (game/gamehooks.h).
// FS2 0x002db9e0: abs(float).
// FS2 0x002dd0c0, 0x002dd630, 0x002dd6e0, 0x002dd9f0: the vector of u_short's resize, erase, insert, reserve.
// FS2 0x002dd140, 0x002dd260, 0x002dd300, 0x002de020, 0x002de100, 0x002de1b0, 0x002de420, 0x002de690: unique, lower_bound, sort and their parts over u_short.
// FS2 0x002dd5b0, 0x002ddcf0, 0x002ddf00, 0x002ddf10, 0x002ddf20, 0x002ddf30, 0x002ddf40, 0x002de000, 0x002df690: the set of u_short.
// FS2 0x002ddb10, 0x002de7b0, 0x002de920, 0x002df7c0, 0x002df860, 0x002df8b0, 0x002df900, 0x002dfa30: a column of the grid, a vector of those vectors.
// FS2 0x002ddbd0, 0x002ddc80, 0x002deda0, 0x002def30, 0x002df550, 0x002df560, 0x002df570, 0x002df5e0, 0x002dfb80, 0x002dfce0, 0x002dfd20, 0x002dfd30, 0x002e0070, 0x002e0080, 0x002e0090: the grid, a vector of columns.
