/* ==========================================================================
 *  game/stagecollision.cpp
 *
 *  The map's collision and the world of bodies (game/stagecollision.h). The
 *  file name is the port's: 0x231d40..0x236b90 and 0x1f4bb0..0x1f6ac0 are two
 *  separate runs of the ROM, whose real files are unknown; 0x2cedb0 and
 *  0x2cf590 sit with the surface sounds.
 *
 *  The ROM's instances written elsewhere or not at all:
 *    FS2 0x00234ca0  a face vector's dtor (host <vector>)
 *    FS2 0x00235320  the same
 *    FS2 0x001f4bc0  StageList's deleting dtor (game/stagelist.h)
 *    FS2 0x001928f0  MSL vector insert (host <vector>)
 *
 *  The ROM keeps its "seen" lists in the scratchpad, 0x70000000 and
 *  0x70002000: a face can sit in many cells, and is tested once.
 * ======================================================================== */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/stagecollision.h"
#include "game/vu0math.h"

namespace {

constexpr float kFloatMax = FLT_MAX;
constexpr u_int kBodyArrayAlign = 0x40;
constexpr u_short kBucketCapacity = 0x1fe;

constexpr char kRecordPrefix[] = "COL_";    // data 0x307348
constexpr int kRecordPrefixLength = 4;

// Under this many faces a mesh is one cell.
constexpr int kFewFaces = 0x21;
constexpr int kOneCellMargin = 5;
// How a cell's tree is cut (the ctor's call of split).
constexpr float kTreeMinHeight = 150.0f;
constexpr int kTreeMinFaces = 20;
// A raycast's point may lie this far outside the cell that found it.
constexpr float kCellSlack = 1.0f;

// A walk stops at this many faces seen. The room is the scratchpad's for
// either list (PC: a guard; past the limit the ROM goes on writing there).
constexpr int kSeenLimit = 0x200;
constexpr int kSeenRoom = 0x7ff;
// layout: host -- the two scratchpad blocks.
struct SeenFaces
{
    // Notes face; true when it was there.
    bool add(const StageCollisionFace *face)
    {
        bool seen = false;
        for (int i = 0; i < count; i++) {
            if (faces[i] == face) {
                seen = true;
                break;
            }
        }
        if (count < kSeenRoom)
            faces[count] = face;
        count++;
        return seen;
    }

    int count;
    const StageCollisionFace *faces[kSeenRoom];
};
SeenFaces sVisitSeen;           /* scratchpad 0x70000000 */
SeenFaces sHitSeen;             /* scratchpad 0x70002000 */

// The axes other than the one in hand (data 0x307310).
constexpr int kOtherAxes[3][2] = { { 1, 2 }, { 0, 2 }, { 0, 1 } };

// PC: faces keep a mesh's slot here, a pointer not fitting their 4 bytes.
std::vector<StageCollision *> sMeshSlots;

u_int takeMeshSlot(StageCollision *mesh)
{
    for (size_t i = 0; i < sMeshSlots.size(); i++) {
        if (sMeshSlots[i] == nullptr) {
            sMeshSlots[i] = mesh;
            return (u_int)i;
        }
    }
    sMeshSlots.push_back(mesh);
    return (u_int)sMeshSlots.size() - 1;
}

inline void copyVec(sceVu0FVECTOR out, const sceVu0FVECTOR in)
{
    memcpy(out, in, sizeof(sceVu0FVECTOR));
}

// All four lanes, as vsub / vadd.xyzw.
inline void subVec(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int lane = 0; lane < 4; lane++)
        out[lane] = a[lane] - b[lane];
}

inline void addVec(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    for (int lane = 0; lane < 4; lane++)
        out[lane] = a[lane] + b[lane];
}

// a . b in the order of the VU's vaddax / vmaddz.
inline float laneSum(const sceVu0FVECTOR a, const sceVu0FVECTOR b)
{
    return (a[1] * b[1] + a[0] * b[0]) + a[2] * b[2];
}

// A coordinate's cell: below the origin one less, the division truncating.
inline int cellOf(float offset, float size)
{
    int cell = (int)(offset / size);
    if (offset < 0.0f)
        cell--;
    return cell;
}

inline int clampCell(int cell, int size)
{
    if (cell < 0)
        return 0;
    return cell >= size ? size - 1 : cell;
}

inline int stepTowards(int from, int to)
{
    if (from == to)
        return 0;
    return from < to ? 1 : -1;
}

// A node's list, made on first use.
void addFace(CollisionNode *node, StageCollisionFace *face)
{
    if (node->faces == nullptr)
        node->faces = new std::vector<StageCollisionFace *>;
    node->faces->push_back(face);
}

// Where the distance from point to the sphere's skin puts the body's owner:
// both sides of a meeting are told, the second the other way.
void touchBoth(CollisionPair *pair, float depth, const float *normal, StageCollisionFace *face)
{
    CollisionContact *first = &pair->side[0];
    if (first->self->owner->collisionHook0(first, normal, face, depth) != 0)
        first->self->boxed = 0;
    CollisionContact *second = &pair->side[1];
    if (second->self->owner->collisionHook0(second, normal, face, -depth) != 0)
        second->self->boxed = 0;
}

} // namespace

// ──────────────────────────────────────────────────────────────────────
// The file

// Offsets into pointers, and "COL_" off the names.
// PC: S2dRef keeps them in the file's 4 bytes.
// FS2 0x0023ae40
// callcheck: strstr, strcpy, strcpy

void CollisionFile::relocate()
{
    CollisionRecord *record = (CollisionRecord *)(this + 1);
    records.set(record);
    StageCollisionFace *faces = (StageCollisionFace *)(record + recordCount);
    for (int i = 0; i < recordCount; i++, record++) {
        record->faces.set(faces);
        faces += record->faceCount;
        if (strstr(record->name, kRecordPrefix) != nullptr) {
            char name[kCollisionNameSize];
            strcpy(name, record->name + kRecordPrefixLength);
            strcpy(record->name, name);
        }
    }
}

// FS2 0x0023adb0
// callcheck: strcmp

CollisionRecord *collisionFindRecord(void *file, const char *name)
{
    if (file == nullptr)
        return nullptr;
    CollisionFile *collision = (CollisionFile *)file;
    CollisionRecord *record = collision->records;
    for (int i = 0; i < collision->recordCount; i++, record++) {
        if (strcmp(name, record->name) == 0)
            return record;
    }
    return nullptr;
}

/* bss 0x599830: which surfaces a stage's meshes use (resetStageGroupTable's
 * first table). Nothing written reads it yet. */
static u_char sSurfaceUsed[0x30];

// FS2 0x002cedb0

int useSurfaceOfName(const char *name)
{
    int surface = surfaceOfName(name);
    if (surface >= 0)           // PC: the ROM marks the byte before the table
        sSurfaceUsed[surface] = 1;
    return surface;
}

// The material's name by a few letters, the ROM's own hash: "S", then one
// letter at +8 and the ones that tell its neighbours apart. The names are
// the files' (the table of surfaces is not read yet), so the ids stay bare.
// FS2 0x002cf590

int surfaceOfName(const char *name)
{
    if (name[0] != 'S')
        return -1;
    switch (name[8]) {
    case 'A':
        switch (name[6]) {
        case 'G':
            if (name[0xb] == '_')
                return name[0xc] == 'W' ? 3 : 8;
            return 2;
        case 'P': return 0x1b;
        case '_': return 0x2a;
        default:  return -1;
        }
    case 'C':
        switch (name[6]) {
        case 'D': return 0x14;
        case 'R': return 0x1a;
        case 'L': return 0x24;
        default:  return -1;
        }
    case 'D':
        switch (name[9]) {
        case 'D': return 0x1f;
        case 'E': return 0x19;
        case 'I': return 0x26;
        case 'R': return 0x22;
        default:  return -1;
        }
    case 'E': return 0x18;
    case 'F': return 0x29;
    case 'G': return 0x20;
    case 'H': return 0x23;
    case 'I': return 0x21;
    case 'L': return 0x1c;
    case 'M':
        return name[3] == 'O' ? 0x2b : 0x10;
    case 'N':
        switch (name[10]) {
        case 'R':  return name[0xe] == 'G' ? 6 : 5;
        case '\0': return 4;
        case '_':  return 0x16;
        case 'K':  return 0x11;
        default:   return -1;
        }
    case 'O':
        if (name[6] == 'W') {
            if (name[0xb] == 'S')
                return 0xc;
            return name[0xe] == 'G' ? 0xb : 10;
        }
        return 9;
    case 'R':
        switch (name[9]) {
        case '\0': return 0x13;
        case 'P':  return 0x15;
        case 'T':
            switch (name[10]) {
            case '\0': return 0;
            case '2':  return 0x17;
            case '_':  return 1;
            default:   return -1;
            }
        default:   return -1;
        }
    case 'S':
        return name[3] == 'W' ? 0x27 : 0xf;
    case 'T':
        switch (name[6]) {
        case 'B': return 0x12;
        case 'F': return 0xe;
        case 'T': return name[7] == 'O' ? 0x1e : 0xd;
        case 'W': return 0x1d;
        case 'L': return 0x28;
        default:  return -1;
        }
    case 'U': return 7;
    case 'W': return 0x25;
    default:  return -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// A face

StageCollision *StageCollisionFace::mesh() const
{
    return sMeshSlots[meshId];
}

// FS2 0x00236030
// callcheck: applyMatrix

void StageCollisionFace::getNormal(sceVu0FVECTOR out) const
{
    copyVec(out, plane);
    out[3] = 0.0f;
    const StageCollision *owner = mesh();
    if (owner->m_matrix != nullptr)
        applyMatrix(out, owner->m_matrix, out);
}

// FS2 0x00235f90

void StageCollisionFace::getBox(sceVu0FVECTOR *box) const
{
    copyVec(box[0], vertex[0]);
    copyVec(box[1], vertex[0]);
    for (int v = 1; v < 3; v++) {
        for (int axis = 0; axis < 3; axis++) {
            if (box[1][axis] < vertex[v][axis])
                box[1][axis] = vertex[v][axis];
            if (vertex[v][axis] < box[0][axis])
                box[0][axis] = vertex[v][axis];
        }
    }
}

// FS2 0x00236070

void StageCollisionFace::findExtremes()
{
    extremes[0] = 0;
    extremes[1] &= 0xf0;
    for (u_int v = 1; v < 3; v++) {
        if (vertex[maxXVertex()][0] < vertex[v][0])
            extremes[0] = (extremes[0] & 0x3f) | (u_char)(v << 6);
        if (vertex[v][0] < vertex[minXVertex()][0])
            extremes[0] = (extremes[0] & 0xfc) | (u_char)v;
        if (vertex[maxYVertex()][1] < vertex[v][1])
            extremes[1] = (extremes[1] & 0xfc) | (u_char)v;
        if (vertex[v][1] < vertex[minYVertex()][1])
            extremes[0] = (extremes[0] & 0xf3) | (u_char)(v << 2);
        if (vertex[maxZVertex()][2] < vertex[v][2])
            extremes[1] = (extremes[1] & 0xf3) | (u_char)(v << 2);
        if (vertex[v][2] < vertex[minZVertex()][2])
            extremes[0] = (extremes[0] & 0xcf) | (u_char)(v << 4);
    }
}

// The ends on either side of the plane, then the triple products of the
// segment with the three corners, all one sign.
// FS2 0x002358e0
// callcheck: invertRigidMatrix, applyMatrix, applyMatrix, vecDot, vecDot, applyMatrix

bool StageCollisionFace::segmentHit(sceVu0FVECTOR hit, const sceVu0FVECTOR from,
                                    const sceVu0FVECTOR to, const sceVu0FVECTOR *matrix,
                                    int side) const
{
    sceVu0FVECTOR start;
    sceVu0FVECTOR end;
    copyVec(start, from);
    copyVec(end, to);
    if (matrix != nullptr) {
        sceVu0FMATRIX inverse;
        invertRigidMatrix(inverse, matrix);
        start[3] = 1.0f;
        end[3] = 1.0f;
        applyMatrix(start, inverse, start);
        applyMatrix(end, inverse, end);
    }
    hit[3] = 0.0f;
    float startSide = plane[3] + vecDot(plane, start);
    float endSide = plane[3] + vecDot(plane, end);
    if (signbit(startSide) == signbit(endSide))
        return false;
    if (side == FaceSide_Back) {
        if (0.0f < startSide)
            return false;
    } else if (side == FaceSide_Front && startSide < 0.0f) {
        return false;
    }

    sceVu0FVECTOR sweep, toFirst, toSecond, toThird;
    subVec(sweep, end, start);
    subVec(toFirst, vertex[0], start);
    subVec(toSecond, vertex[1], start);
    subVec(toThird, vertex[2], start);
    sceVu0FVECTOR around;
    vecCross(around, toFirst, toSecond);
    bool negative = laneSum(sweep, around) < 0.0f;
    vecCross(around, toSecond, toThird);
    if ((laneSum(sweep, around) < 0.0f) != negative)
        return false;
    float t = startSide / (startSide - endSide);
    for (int lane = 0; lane < 3; lane++)
        hit[lane] = sweep[lane] * t + start[lane];
    hit[3] = start[3];
    vecCross(around, toThird, toFirst);
    if ((laneSum(sweep, around) < 0.0f) != negative)
        return false;
    if (matrix != nullptr)
        applyMatrix(hit, matrix, hit);
    return true;
}

// FS2 0x00235b50
// callcheck: vecCross, vecCross, vecCross

bool StageCollisionFace::heightAt(float x, float y, float *height) const
{
    sceVu0FVECTOR point = { x, y, 1.0f, 0.0f };
    sceVu0FVECTOR toFirst, toSecond, toThird, around;
    subVec(toFirst, vertex[0], point);
    subVec(toSecond, vertex[1], point);
    subVec(toThird, vertex[2], point);

    vecCross(around, toFirst, toSecond);
    u_int below = around[2] < 0.0f ? 1 : 0;
    vecCross(around, toSecond, toThird);
    if (around[2] < 0.0f)
        below |= 2;
    if (below != 3 && below != 0)
        return false;
    vecCross(around, toThird, toFirst);
    if (around[2] < 0.0f)
        below |= 4;
    if (below != 7 && below != 0)
        return false;
    if (plane[2] == 0.0f)
        return false;

    float onPlane = -(plane[3] + plane[0] * x + plane[1] * y) / plane[2];
    float top = vertex[2][2];
    if (top < vertex[1][2])
        top = vertex[1][2];
    if (top < vertex[0][2])
        top = vertex[0][2];
    *height = onPlane < top ? onPlane : top;
    return true;
}

// A vertex in the rectangle, a corner in the triangle, or an edge with
// corners on both sides of it.
// FS2 0x00235d20
// callcheck: heightAt, vecCross

bool StageCollisionFace::overlapsRect(float x0, float y0, float x1, float y1) const
{
    for (int v = 0; v < 3; v++) {
        if (x0 <= vertex[v][0] && vertex[v][0] <= x1 && y0 <= vertex[v][1] && vertex[v][1] <= y1)
            return true;
    }
    float height;
    for (int corner = 0; corner < 4; corner++) {
        float y = corner / 2 == 0 ? y0 : y1;
        float x = corner % 2 == 0 ? x0 : x1;
        if (heightAt(x, y, &height))
            return true;
    }

    sceVu0FVECTOR corners[4] = {
        { x0, y0, 0.0f, 0.0f },
        { x1, y0, 0.0f, 0.0f },
        { x0, y1, 0.0f, 0.0f },
        { x1, y1, 0.0f, 0.0f },
    };
    for (int v = 0; v < 3; v++) {
        sceVu0FVECTOR edge;
        subVec(edge, vertex[(v + 1) % 3], vertex[v]);
        edge[2] = 0.0f;
        sceVu0FVECTOR around[4];
        for (int corner = 0; corner < 4; corner++) {
            sceVu0FVECTOR toCorner;
            subVec(toCorner, corners[corner], vertex[v]);
            toCorner[2] = 0.0f;
            vecCross(around[corner], edge, toCorner);
        }
        for (int corner = 1; corner < 4; corner++) {
            if (around[0][2] * around[corner][2] <= 0.0f)
                return true;
        }
    }
    return false;
}

// A vertex in the sphere, an edge through it, or the face under its center.
// FS2 0x00235740
// callcheck: invertRigidMatrix, applyMatrix, vecDistanceSq, hitsSegment, scaleVec3, segmentHit

bool StageCollisionFace::sphereTouches(float radius, const sceVu0FVECTOR center,
                                       const sceVu0FVECTOR *matrix) const
{
    sceVu0FVECTOR local;
    copyVec(local, center);
    if (matrix != nullptr) {
        sceVu0FMATRIX inverse;
        invertRigidMatrix(inverse, matrix);
        local[3] = 1.0f;
        applyMatrix(local, inverse, local);
    }
    BoundingSphere sphere;
    sphere.radius = radius;
    sphere.radiusSq = radius * radius;
    copyVec(sphere.center, local);

    for (int v = 0; v < 3; v++) {
        if (vecDistanceSq(vertex[v], sphere.center) <= sphere.radiusSq)
            return true;
    }
    for (int v = 0; v < 3; v++) {
        if (sphere.hitsSegment(0.0f, vertex[v], vertex[(v + 1) % 3], nullptr))
            return true;
    }
    sceVu0FVECTOR tip;
    sceVu0FVECTOR point;
    scaleVec3(-radius, tip, plane);
    addVec(tip, tip, local);
    return segmentHit(point, local, tip, nullptr, FaceSide_Any);
}

// The three cases of sphereTouches the other way round: the face under the
// center, the nearest edge, a vertex.
// FS2 0x002353b0
// callcheck: invertRigidMatrix, applyMatrix, scaleVec3, segmentHit, edgeTouches, vecDistanceSq, vecNormalize, scaleVec4, applyMatrix, applyMatrix, vecNormalize, scaleVec4, applyMatrix, applyMatrix, applyMatrix, applyMatrix

bool StageCollisionFace::spherePush(float radius, const sceVu0FVECTOR center,
                                    const sceVu0FVECTOR *matrix, sceVu0FVECTOR hit,
                                    sceVu0FVECTOR push) const
{
    sceVu0FVECTOR local;
    copyVec(local, center);
    if (matrix != nullptr) {
        sceVu0FMATRIX inverse;
        invertRigidMatrix(inverse, matrix);
        local[3] = 1.0f;
        applyMatrix(local, inverse, local);
    }
    BoundingSphere sphere;
    sphere.radius = radius;
    sphere.radiusSq = radius * radius;
    copyVec(sphere.center, local);

    sceVu0FVECTOR tip;
    sceVu0FVECTOR point;
    scaleVec3(-radius, tip, plane);
    addVec(tip, tip, local);
    bool found = false;
    if (segmentHit(point, local, tip, nullptr, FaceSide_Any)) {
        copyVec(hit, point);
        hit[3] = 1.0f;
        subVec(push, point, tip);
        found = true;
    } else {
        sceVu0FVECTOR edges[3];
        int nearest = -1;
        for (int v = 0; v < 3; v++) {
            if (sphere.edgeTouches(vertex[v], vertex[(v + 1) % 3], edges[v])) {
                if (nearest == -1 || edges[v][3] < edges[nearest][3])
                    nearest = v;
            }
        }
        if (nearest >= 0) {
            addVec(hit, local, edges[nearest]);
            hit[3] = 1.0f;
            float distance = edges[nearest][3];
            vecNormalize(edges[nearest], edges[nearest]);
            scaleVec4(distance - radius, push, edges[nearest]);
            found = true;
        } else {
            for (int v = 0; v < 3 && !found; v++) {
                if (vecDistanceSq(vertex[v], sphere.center) <= sphere.radiusSq) {
                    copyVec(hit, vertex[v]);
                    sceVu0FVECTOR out;
                    subVec(out, vertex[v], local);
                    float distance = sqrtf(out[2] * out[2] + out[0] * out[0] + out[1] * out[1]);
                    vecNormalize(out, out);
                    scaleVec4(-(radius - distance), push, out);
                    found = true;
                }
            }
        }
    }
    if (!found)
        return false;
    push[3] = 0.0f;             // PC: a direction; the ROM's second and third case leave it to the VU
    if (matrix != nullptr) {
        applyMatrix(hit, matrix, hit);
        applyMatrix(push, matrix, push);
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// Spheres and boxes

// FS2 0x00236570
// callcheck: vecNormalize, vecDot, vecDot, scaleVec4, vecDot

bool BoundingSphere::edgeTouches(const sceVu0FVECTOR a, const sceVu0FVECTOR b,
                                 sceVu0FVECTOR offset) const
{
    sceVu0FVECTOR edge, toCenter, along;
    subVec(edge, b, a);
    subVec(toCenter, center, a);
    vecNormalize(along, edge);
    float position = vecDot(along, toCenter);
    if (position < 0.0f)
        return false;
    if (sqrtf(vecDot(edge, edge)) < position)
        return false;
    sceVu0FVECTOR closest;
    scaleVec4(position, closest, along);
    addVec(closest, closest, a);
    sceVu0FVECTOR off;
    subVec(off, closest, center);
    if (offset != nullptr)
        copyVec(offset, off);
    float square = vecDot(off, off);
    if (square >= radiusSq)
        return false;
    if (offset != nullptr)
        offset[3] = sqrtf(square);
    return true;
}

// FS2 0x002366c0
// callcheck: applyMatrix, vecNormalize, vecDot, vecDot, scaleVec4, vecDistanceSq

bool BoundingSphere::hitsSegment(float margin, const sceVu0FVECTOR from, const sceVu0FVECTOR to,
                                 const sceVu0FVECTOR *matrix) const
{
    sceVu0FVECTOR moved;
    if (matrix == nullptr)
        copyVec(moved, center);
    else
        applyMatrix(moved, matrix, center);
    sceVu0FVECTOR edge, toCenter, along;
    subVec(edge, to, from);
    subVec(toCenter, moved, from);
    vecNormalize(along, edge);
    float position = vecDot(along, toCenter);
    if (position < 0.0f)
        return false;
    if (sqrtf(vecDot(edge, edge)) < position)
        return false;
    sceVu0FVECTOR closest;
    scaleVec4(position, closest, along);
    addVec(closest, closest, from);
    float reach = radius + margin;
    return vecDistanceSq(closest, moved) < reach * reach;
}

// FS2 0x00236800
// callcheck: hitsSegment

bool BoundingSphere::hitsSegment0(const sceVu0FVECTOR from, const sceVu0FVECTOR to) const
{
    return hitsSegment(0.0f, from, to, nullptr);
}

// FS2 0x00236310
// callcheck: applyMatrix, applyMatrix, hitsSegment

bool BoundingSphere::hitsCapsule(float capsuleRadius, const sceVu0FVECTOR from,
                                 const sceVu0FVECTOR to, const sceVu0FVECTOR *matrix) const
{
    float reach = capsuleRadius + radius;
    sceVu0FVECTOR moved, offset;
    if (matrix == nullptr)
        copyVec(moved, center);
    else
        applyMatrix(moved, matrix, center);
    subVec(offset, moved, from);
    if (offset[2] * offset[2] + offset[0] * offset[0] + offset[1] * offset[1] < reach * reach)
        return true;
    if (matrix == nullptr)
        copyVec(moved, center);
    else
        applyMatrix(moved, matrix, center);
    subVec(offset, moved, to);
    if (offset[2] * offset[2] + offset[0] * offset[0] + offset[1] * offset[1] < reach * reach)
        return true;
    return hitsSegment(0.0f, from, to, matrix);
}

// FS2 0x00236480
// callcheck: applyMatrix

bool BoundingSphere::hitsSphere(float otherRadius, const sceVu0FVECTOR otherCenter,
                                const sceVu0FVECTOR *matrix, sceVu0FVECTOR offset) const
{
    sceVu0FVECTOR moved, off;
    if (matrix == nullptr)
        copyVec(moved, center);
    else
        applyMatrix(moved, matrix, center);
    subVec(off, moved, otherCenter);
    if (offset != nullptr)
        copyVec(offset, off);
    float square = off[2] * off[2] + off[0] * off[0] + off[1] * off[1];
    float reach = otherRadius + radius;
    if (square >= reach * reach)
        return false;
    if (offset != nullptr)
        offset[3] = sqrtf(square);
    return true;
}

// FS2 0x00236810
// callcheck: divideVec4, vecDistanceSq

bool BoundingSphere::fromPoints(int count, const sceVu0FVECTOR *points)
{
    if (count < 1)
        return false;
    center[0] = 0.0f;
    center[1] = 0.0f;
    center[2] = 0.0f;
    for (int i = 0; i < count; i++)
        addVec(center, center, points[i]);
    divideVec4((float)count, center, center);
    float largest = -1.0f;
    for (int i = 0; i < count; i++) {
        float square = vecDistanceSq(points[i], center);
        if (largest < square)
            largest = square;
    }
    radiusSq = largest;
    radius = sqrtf(largest);
    return true;
}

// p * m lands in the unit cube: m takes the world into a box's own space
// (vclipw against vf0's w).
// FS2 0x002362a0

bool boxContainsPoint(const float m[4][4], const float p[4])
{
    for (int lane = 0; lane < 3; lane++) {
        float value = m[0][lane] * p[0] + m[1][lane] * p[1] + m[2][lane] * p[2] + m[3][lane] * p[3];
        if (value > 1.0f || value < -1.0f)
            return false;
    }
    return true;
}

// FS2 0x00236ab0

bool boxesOverlap(const sceVu0FVECTOR *a, const sceVu0FVECTOR *b)
{
    for (int axis = 0; axis < 3; axis++) {
        if (b[1][axis] < a[0][axis])
            return false;
        if (a[1][axis] < b[0][axis])
            return false;
    }
    return true;
}

// FS2 0x00236930

bool boxHitsSegment(const sceVu0FVECTOR *box, const sceVu0FVECTOR from, const sceVu0FVECTOR to)
{
    for (int axis = 0; axis < 3; axis++) {
        float high = from[axis] < to[axis] ? to[axis] : from[axis];
        float low = from[axis] < to[axis] ? from[axis] : to[axis];
        if (high - box[0][axis] < 0.0f || box[1][axis] - low < 0.0f)
            return false;
    }

    sceVu0FVECTOR sweep;
    subVec(sweep, to, from);
    for (int axis = 0; axis < 3; axis++) {
        if (sweep[axis] == 0.0f)
            continue;
        float scale = 1.0f / sweep[axis];
        const int first = kOtherAxes[axis][0];
        const int second = kOtherAxes[axis][1];
        for (int side = 0; side < 2; side++) {
            float distance = box[side][axis] - from[axis];
            float along = scale * distance * sweep[first] + from[first] + 0.0f;
            if (box[0][first] <= along && along <= box[1][first]) {
                along = scale * distance * sweep[second] + from[second] + 0.0f;
                if (box[0][second] <= along && along <= box[1][second])
                    return true;
            }
        }
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// The tree of a cell

// FS2 0x00235090
// callcheck: operator_delete, FUN_00234ca0, FUN_00235320, ~CollisionNode (two levels of it inline; the vectors are the host's)

CollisionNode::~CollisionNode()
{
    delete faces;
    delete upper;
    delete lower;
}

// FS2 0x00234d10
// callcheck: operator_new, operator_new, operator_new, operator_new, FUN_001928f0, FUN_001928f0, operator_delete, operator_delete, split, split, ~CollisionNode, ~CollisionNode (the vectors are the host's)

bool CollisionNode::split(float minHeight, int minFaces)
{
    if (faces == nullptr || box[1][2] - box[0][2] < minHeight)
        return false;
    if ((int)faces->size() < minFaces)
        return false;

    CollisionNode *above = new CollisionNode;
    CollisionNode *below = new CollisionNode;
    float middle = (box[1][2] + 0.0f) - (box[1][2] - box[0][2]) * 0.5f;
    for (int corner = 0; corner < 2; corner++) {
        for (int axis = 0; axis < 2; axis++) {
            above->box[corner][axis] = box[corner][axis];
            below->box[corner][axis] = box[corner][axis];
        }
    }
    above->box[0][2] = middle;
    above->box[1][2] = box[1][2];
    below->box[0][2] = box[0][2];
    below->box[1][2] = middle;

    for (StageCollisionFace *face : *faces) {
        if (middle <= face->maxZ())
            addFace(above, face);
        if (face->minZ() <= middle)
            addFace(below, face);
    }
    if (above->faces != nullptr && below->faces != nullptr
        && faces->size() != above->faces->size() && faces->size() != below->faces->size()) {
        delete faces;
        faces = nullptr;
        upper = above;
        lower = below;
        above->split(minHeight, minFaces);
        below->split(minHeight, minFaces);
        return true;
    }
    delete above;
    delete below;
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// A mesh

// The planes the first time; the box; the grid of cells, each holding the
// faces over it, highest first; each cell's tree.
// FS2 0x00234440
// callcheck: vecCross, vecNormalize, vecDot, findExtremes, useSurfaceOfName, operator_new[], FUN_00104d90, overlapsRect, operator_new, FUN_001928f0, FUN_001928f0, split

StageCollision::StageCollision(CollisionRecord *record,
                               const char (*surfaceNames)[kCollisionNameSize], int cellSize)
{
    m_inverse = nullptr;
    m_matrix = nullptr;
    m_faceCount = record->faceCount;
    m_surfaceNames = surfaceNames;
    m_faces = record->faces;
    m_slot = takeMeshSlot(this);
    for (int i = 0; i < m_faceCount; i++)
        m_faces[i].meshId = m_slot;

    if (record->prepared == 0) {
        for (int i = 0; i < m_faceCount; i++) {
            StageCollisionFace *face = &m_faces[i];
            sceVu0FVECTOR first, second;
            subVec(first, face->vertex[1], face->vertex[0]);
            subVec(second, face->vertex[2], face->vertex[0]);
            vecCross(face->plane, first, second);
            vecNormalize(face->plane, face->plane);
            face->plane[3] = -vecDot(face->plane, face->vertex[0]);
            face->findExtremes();
            face->surface = (u_char)useSurfaceOfName(m_surfaceNames[face->material]);
        }
    }

    bool first = true;
    for (int i = 0; i < m_faceCount; i++) {
        const StageCollisionFace *face = &m_faces[i];
        for (int v = 0; v < 3; v++) {
            if (first) {
                first = false;
                copyVec(m_box[0], face->vertex[0]);
                copyVec(m_box[1], face->vertex[0]);
            }
            for (int axis = 0; axis < 3; axis++) {
                if (m_box[1][axis] < face->vertex[v][axis])
                    m_box[1][axis] = face->vertex[v][axis];
                if (face->vertex[v][axis] < m_box[0][axis])
                    m_box[0][axis] = face->vertex[v][axis];
            }
        }
    }

    if (record->faceCount < kFewFaces) {
        float span = m_box[1][0] - m_box[0][0];
        if (span <= m_box[1][1] - m_box[0][1])
            span = m_box[1][1] - m_box[0][1];
        cellSize = (int)span + kOneCellMargin;
    }
    m_cellSizeInt = cellSize;
    m_cellSize = (float)cellSize;
    m_gridWidth = (int)((m_box[1][0] - m_box[0][0]) / m_cellSize) + 1;
    m_gridHeight = (int)((m_box[1][1] - m_box[0][1]) / m_cellSize) + 1;
    m_cells = new CollisionNode[m_gridWidth * m_gridHeight];
    for (int y = 0; y < m_gridHeight; y++) {
        float cellY = (float)y * m_cellSize + m_box[0][1] + 0.0f;
        for (int x = 0; x < m_gridWidth; x++) {
            float cellX = (float)x * m_cellSize + m_box[0][0] + 0.0f;
            CollisionNode *cell = cellAt(x, y);
            cell->box[0][0] = cellX;
            cell->box[0][1] = cellY;
            cell->box[0][2] = kFloatMax;
            cell->box[1][0] = cellX + m_cellSize;
            cell->box[1][1] = cellY + m_cellSize;
            cell->box[1][2] = -kFloatMax;
        }
    }
    memset(m_groupOn, 1, sizeof(m_groupOn));

    for (int i = 0; i < m_faceCount; i++) {
        StageCollisionFace *face = &m_faces[i];
        sceVu0FVECTOR box[2];
        face->getBox(box);
        float low[2] = { box[0][0] - m_box[0][0], box[0][1] - m_box[0][1] };
        float high[2] = { box[1][0] - m_box[0][0], box[1][1] - m_box[0][1] };
        for (int y = (int)(low[1] / m_cellSize); y <= (int)(high[1] / m_cellSize); y++) {
            float cellY = (float)y * m_cellSize + m_box[0][1] + 0.0f;
            for (int x = (int)(low[0] / m_cellSize); x <= (int)(high[0] / m_cellSize); x++) {
                float cellX = (float)x * m_cellSize + m_box[0][0] + 0.0f;
                CollisionNode *cell = cellAt(x, y);
                if (!face->overlapsRect(cellX, cellY, cellX + m_cellSize, cellY + m_cellSize))
                    continue;
                if (cell->faces == nullptr)
                    cell->faces = new std::vector<StageCollisionFace *>;
                std::vector<StageCollisionFace *> &faces = *cell->faces;
                auto place = faces.begin();
                while (place != faces.end() && !((*place)->maxZ() < face->maxZ()))
                    ++place;
                faces.insert(place, face);
                if (cell->box[1][2] < box[1][2])
                    cell->box[1][2] = box[1][2];
                if (box[0][2] < cell->box[0][2])
                    cell->box[0][2] = box[0][2];
            }
        }
    }
    for (int i = 0; i < m_gridWidth * m_gridHeight; i++)
        m_cells[i].split(kTreeMinHeight, kTreeMinFaces);
    record->prepared = 1;
}

// FS2 0x002343e0
// callcheck: __destroy_new_array, operator_delete

StageCollision::~StageCollision()
{
    delete[] m_cells;
    sMeshSlots[m_slot] = nullptr;
}

// FS2 0x002343c0

void StageCollision::setGroup(int group, bool on)
{
    if (group < kGroups)
        m_groupOn[group] = on;
}

// The cells under the segment, from its start: a column is walked straight,
// otherwise along the longer axis, stepping the other way where the line
// leaves the cell. Cells off the grid are passed over.
template <class Visit>
bool StageCollision::walkCells(const sceVu0FVECTOR from, const sceVu0FVECTOR to, Visit visit)
{
    int x = cellOf(from[0] - m_box[0][0], m_cellSize);
    int endX = cellOf(to[0] - m_box[0][0], m_cellSize);
    int y = cellOf(from[1] - m_box[0][1], m_cellSize);
    int endY = cellOf(to[1] - m_box[0][1], m_cellSize);
    const int stepX = stepTowards(x, endX);
    const int stepY = stepTowards(y, endY);
    const float slope = (to[1] - from[1]) / (to[0] - from[0]);
    const float intercept = (from[1] + 0.0f) - slope * from[0];

    auto tryCell = [&]() {
        return x >= 0 && x < m_gridWidth && y >= 0 && y < m_gridHeight && visit(cellAt(x, y));
    };

    if (x == endX) {
        for (;;) {
            if (tryCell())
                return true;
            if (y == endY)
                return false;
            y += stepY;
        }
    }
    if (abs(endX - x) < abs(endY - y)) {
        for (;;) {
            if (tryCell())
                return true;
            if (x == endX && y == endY)
                return false;
            int edge = stepY < 1 ? y : y + stepY;
            float edgeY = m_box[0][1] + (float)(m_cellSizeInt * edge);
            if ((int)(((1.0f / slope) * (edgeY - intercept) - m_box[0][0]) / m_cellSize) == x) {
                if (y == endY)
                    return false;
                y += stepY;
            } else {
                if (x == endX)
                    return false;
                x += stepX;
            }
        }
    }
    for (;;) {
        if (tryCell())
            return true;
        if (x == endX && y == endY)
            return false;
        int edge = stepX < 1 ? x : x + stepX;
        float edgeX = m_box[0][0] + (float)(m_cellSizeInt * edge);
        if ((int)(((slope * edgeX + intercept + 0.0f) - m_box[0][1]) / m_cellSize) == y) {
            if (x == endX)
                return false;
            x += stepX;
        } else {
            if (y == endY)
                return false;
            y += stepY;
        }
    }
}

// FS2 0x00232da0
// callcheck: raycastNode, raycastNode, vecDistanceSq, vecDistanceSq, segmentHit, vecDistanceSq

StageCollisionFace *StageCollision::raycastNode(sceVu0FVECTOR hit, const sceVu0FVECTOR from,
                                                const sceVu0FVECTOR to, u_int mask, u_int flags,
                                                const CollisionNode *node, int side)
{
    float low = from[2] < to[2] ? from[2] : to[2];
    float high = to[2] < from[2] ? from[2] : to[2];
    if (node->faces == nullptr) {
        sceVu0FVECTOR hitAbove, hitBelow;
        StageCollisionFace *above = nullptr;
        StageCollisionFace *below = nullptr;
        const CollisionNode *child = node->upper;
        if (child != nullptr && child->box[0][2] < high && low < child->box[1][2])
            above = raycastNode(hitAbove, from, to, mask, flags, child, side);
        child = node->lower;
        if (child != nullptr && child->box[0][2] < high && low < child->box[1][2])
            below = raycastNode(hitBelow, from, to, mask, flags, child, side);
        if (above == nullptr) {
            if (below != nullptr)
                copyVec(hit, hitBelow);
            return below;
        }
        if (below == nullptr || above == below
            || vecDistanceSq(hitAbove, from) < vecDistanceSq(hitBelow, from)) {
            copyVec(hit, hitAbove);
            return above;
        }
        copyVec(hit, hitBelow);
        return below;
    }

    StageCollisionFace *found = nullptr;
    if (low <= node->box[1][2] && node->box[0][2] <= high) {
        sceVu0FVECTOR nearest;
        float best = 0.0f;
        const float minX = node->box[0][0] - kCellSlack;
        const float minY = node->box[0][1] - kCellSlack;
        const float maxX = node->box[1][0] + kCellSlack;
        const float maxY = node->box[1][1] + kCellSlack;
        for (StageCollisionFace *face : *node->faces) {
            if (face->maxZ() < low)
                break;
            if (m_groupOn[face->doorId] == 0 || high < face->minZ() || !selects(face, mask, flags))
                continue;
            sceVu0FVECTOR point;
            if (!face->segmentHit(point, from, to, nullptr, side))
                continue;
            if (point[0] < minX || maxX < point[0] || point[1] < minY || maxY < point[1])
                continue;
            float distance = vecDistanceSq(point, from);
            if (found == nullptr || distance < best) {
                found = face;
                best = distance;
                copyVec(nearest, point);
            }
        }
        // PC: only on a hit; the ROM copies its stack either way.
        if (found != nullptr)
            copyVec(hit, nearest);
    }
    return found;
}

// FS2 0x00233810
// callcheck: applyMatrix, applyMatrix, boxHitsSegment, abs, abs, raycastNode, applyMatrix (walkCells: the ROM has the walk three times over)

StageCollisionFace *StageCollision::raycast(sceVu0FVECTOR hit, const sceVu0FVECTOR from,
                                            const sceVu0FVECTOR to, u_int mask, u_int flags,
                                            int side)
{
    sceVu0FVECTOR start, end;
    copyVec(start, from);
    copyVec(end, to);
    if (m_inverse != nullptr) {
        start[3] = 1.0f;
        end[3] = 1.0f;
        applyMatrix(start, m_inverse, start);
        applyMatrix(end, m_inverse, end);
    }
    if (!boxHitsSegment(m_box, start, end))
        return nullptr;

    StageCollisionFace *face = nullptr;
    walkCells(start, end, [&](const CollisionNode *cell) {
        face = raycastNode(hit, start, end, mask, flags, cell, side);
        return face != nullptr;
    });
    if (face != nullptr && m_matrix != nullptr)
        applyMatrix(hit, m_matrix, hit);
    return face;
}

// FS2 0x00232b20
// callcheck: segmentHitsNode, segmentHitsNode, segmentHit

bool StageCollision::segmentHitsNode(const sceVu0FVECTOR from, const sceVu0FVECTOR to, u_int mask,
                                     u_int flags, const CollisionNode *node, int side)
{
    float low = from[2] < to[2] ? from[2] : to[2];
    float high = to[2] < from[2] ? from[2] : to[2];
    if (node->faces == nullptr) {
        const CollisionNode *child = node->upper;
        if (child != nullptr && child->box[0][2] < high && low < child->box[1][2]
            && segmentHitsNode(from, to, mask, flags, child, side))
            return true;
        child = node->lower;
        return child != nullptr && child->box[0][2] < high && low < child->box[1][2]
            && segmentHitsNode(from, to, mask, flags, child, side);
    }
    if (low <= node->box[1][2] && node->box[0][2] <= high) {
        for (StageCollisionFace *face : *node->faces) {
            if (face->maxZ() < low)
                break;
            sceVu0FVECTOR point;
            if (m_groupOn[face->doorId] != 0 && face->minZ() <= high && selects(face, mask, flags)
                && face->segmentHit(point, from, to, nullptr, side))
                return true;
        }
    }
    return false;
}

// FS2 0x00233160
// callcheck: applyMatrix, applyMatrix, boxHitsSegment, abs, abs, segmentHitsNode (walkCells: the ROM has the walk three times over)

bool StageCollision::segmentHits(const float *from, const float *to, u_int mask, u_int flags,
                                 int side)
{
    sceVu0FVECTOR start, end;
    copyVec(start, from);
    copyVec(end, to);
    if (m_inverse != nullptr) {
        start[3] = 1.0f;
        end[3] = 1.0f;
        applyMatrix(start, m_inverse, start);
        applyMatrix(end, m_inverse, end);
    }
    if (!boxHitsSegment(m_box, start, end))
        return false;
    return walkCells(start, end, [&](const CollisionNode *cell) {
        return segmentHitsNode(start, end, mask, flags, cell, side);
    });
}

// FS2 0x00232460
// callcheck: sphereHitsNode, sphereHitsNode, sphereTouches

int StageCollision::sphereHitsNode(float radius, const CollisionNode *node, int *count,
                                   int maxHits, StageCollisionFace **out, u_int mask, u_int flags,
                                   const sceVu0FVECTOR center)
{
    if (*count >= maxHits)
        return *count;
    float low = center[2] - radius;
    float high = center[2] + radius;
    if (node->faces == nullptr) {
        const CollisionNode *child = node->upper;
        if (child != nullptr && child->box[0][2] < high && low < child->box[1][2])
            sphereHitsNode(radius, child, count, maxHits, out, mask, flags, center);
        child = node->lower;
        if (child != nullptr && child->box[0][2] < high && low < child->box[1][2])
            sphereHitsNode(radius, child, count, maxHits, out, mask, flags, center);
        return *count;
    }
    if (low <= node->box[1][2]) {
        for (StageCollisionFace *face : *node->faces) {
            if (face->maxZ() < low)
                break;
            if (m_groupOn[face->doorId] == 0)
                continue;
            if (center[0] + radius < face->minX() || face->maxX() < center[0] - radius
                || center[1] + radius < face->minY() || face->maxY() < center[1] - radius
                || center[2] + radius < face->minZ())
                continue;
            if (!selects(face, mask, flags))
                continue;
            bool seen = sHitSeen.add(face);
            if (!seen && face->sphereTouches(radius, center, nullptr)) {
                out[*count] = face;
                (*count)++;
                if (*count == maxHits)
                    return *count;
            }
            if (sHitSeen.count == kSeenLimit)
                return *count;
        }
    }
    return *count;
}

// A row of cells ends at the first hit too many, and the next row still
// looks at one cell: sphereHitsNode then stops at once.
// FS2 0x00232870
// callcheck: applyMatrix, sphereHitsNode

int StageCollision::sphereHits(float radius, int maxHits, StageCollisionFace **out, u_int mask,
                               u_int flags, const float *pos)
{
    sHitSeen.count = 0;
    sceVu0FVECTOR center;
    copyVec(center, pos);
    if (m_inverse != nullptr) {
        center[3] = 1.0f;
        applyMatrix(center, m_inverse, center);
    }
    for (int axis = 0; axis < 3; axis++) {
        if (radius + center[axis] < m_box[0][axis] || m_box[1][axis] < center[axis] - radius)
            return 0;
    }

    int x0 = clampCell((int)(((center[0] - radius) - m_box[0][0]) / m_cellSize), m_gridWidth);
    int x1 = clampCell((int)(((center[0] + radius) - m_box[0][0]) / m_cellSize), m_gridWidth);
    int y0 = clampCell((int)(((center[1] - radius) - m_box[0][1]) / m_cellSize), m_gridHeight);
    int y1 = clampCell((int)(((center[1] + radius) - m_box[0][1]) / m_cellSize), m_gridHeight);
    int count = 0;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            sphereHitsNode(radius, cellAt(x, y), &count, maxHits, out, mask, flags, center);
            if (count >= maxHits)
                break;
        }
    }
    return count;
}

// FS2 0x00231d80
// callcheck: visitSphereNode, visitSphereNode, spherePush (the visitor)

int StageCollision::visitSphereNode(const CollisionNode *node, CollisionFaceVisitor visitor,
                                    void *context, u_int mask, u_int flags,
                                    const sceVu0FVECTOR center, const BoundingSphere *sphere)
{
    float radius = sphere->radius;
    float low = center[2] - radius;
    float high = center[2] + radius;
    if (node->faces == nullptr) {
        const CollisionNode *child = node->upper;
        if (child != nullptr && child->box[0][2] < high && low < child->box[1][2]
            && visitSphereNode(child, visitor, context, mask, flags, center, sphere) != 0)
            return 1;
        child = node->lower;
        if (child != nullptr && child->box[0][2] < high && low < child->box[1][2]
            && visitSphereNode(child, visitor, context, mask, flags, center, sphere) != 0)
            return 1;
        return 0;
    }
    if (low <= node->box[1][2]) {
        for (StageCollisionFace *face : *node->faces) {
            if (face->maxZ() < low)
                break;
            if (m_groupOn[face->doorId] == 0)
                continue;
            if (center[0] + radius < face->minX() || face->maxX() < center[0] - radius
                || center[1] + radius < face->minY() || face->maxY() < center[1] - radius
                || center[2] + radius < face->minZ())
                continue;
            if (!selects(face, mask, flags))
                continue;
            bool seen = sVisitSeen.add(face);
            sceVu0FVECTOR hit, push;
            if (!seen && face->spherePush(radius, sphere->center, m_matrix, hit, push)
                && visitor(face, hit, push, context) != 0)
                return 1;
            if (sVisitSeen.count == kSeenLimit)
                return 1;
        }
    }
    return 0;
}

// A visitor's stop ends the row only; the next row goes on.
// FS2 0x00232190
// callcheck: applyMatrix, visitSphereNode

void StageCollision::visitSphere(CollisionFaceVisitor visitor, void *context, u_int mask,
                                 u_int flags, const BoundingSphere *sphere)
{
    sVisitSeen.count = 0;
    sceVu0FVECTOR center;
    copyVec(center, sphere->center);
    if (m_inverse != nullptr) {
        center[3] = 1.0f;
        applyMatrix(center, m_inverse, center);
    }
    float radius = sphere->radius;
    for (int axis = 0; axis < 3; axis++) {
        if (radius + center[axis] < m_box[0][axis] || m_box[1][axis] < center[axis] - radius)
            return;
    }

    int x0 = clampCell((int)(((center[0] - radius) - m_box[0][0]) / m_cellSize), m_gridWidth);
    int x1 = clampCell((int)(((center[0] + radius) - m_box[0][0]) / m_cellSize), m_gridWidth);
    int y0 = clampCell((int)(((center[1] - radius) - m_box[0][1]) / m_cellSize), m_gridHeight);
    int y1 = clampCell((int)(((center[1] + radius) - m_box[0][1]) / m_cellSize), m_gridHeight);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (visitSphereNode(cellAt(x, y), visitor, context, mask, flags, center, sphere) != 0)
                break;
        }
    }
}

// FS2 0x00233f90
// callcheck: heightAtNode, heightAtNode, heightAt

StageCollisionFace *StageCollision::heightAtNode(float x, float y, float low,
                                                 const CollisionNode *node, float *height,
                                                 u_int mask, u_int flags,
                                                 StageCollisionFace **second)
{
    StageCollisionFace *found = nullptr;
    const float top = *height;
    if (node->faces == nullptr) {
        const CollisionNode *child = node->upper;
        if (child != nullptr && child->box[0][2] <= top && low <= child->box[1][2])
            found = heightAtNode(x, y, low, child, height, mask, flags, second);
        child = node->lower;
        if (found == nullptr && child != nullptr && child->box[0][2] <= *height
            && low <= child->box[1][2])
            found = heightAtNode(x, y, low, child, height, mask, flags, second);
        return found;
    }

    StageCollisionFace *next = nullptr;
    float nextHeight = 0.0f;
    for (StageCollisionFace *face : *node->faces) {
        float faceTop = face->maxZ();
        if ((found != nullptr && faceTop < *height) || faceTop < low)
            break;
        float faceHeight;
        if (m_groupOn[face->doorId] == 0 || top < face->minZ() || !selects(face, mask, flags)
            || !face->heightAt(x, y, &faceHeight) || top < faceHeight)
            continue;
        if (found == nullptr) {
            *height = faceHeight;
            found = face;
        } else if (faceHeight <= *height) {
            if (next == nullptr || nextHeight < faceHeight) {
                nextHeight = faceHeight;
                next = face;
            }
        } else {
            nextHeight = *height;
            next = found;
            *height = faceHeight;
            found = face;
        }
    }
    if (second != nullptr)
        *second = next;
    return found;
}

// FS2 0x002342c0
// callcheck: heightAtNode

StageCollisionFace *StageCollision::heightAt(float x, float y, float range, float *height,
                                             u_int mask, u_int flags, StageCollisionFace **second)
{
    if (m_matrix != nullptr || m_box[1][0] < x || x < m_box[0][0] || m_box[1][1] < y
        || y < m_box[0][1])
        return nullptr;
    float low = 0.0f < range ? *height - range : m_box[0][2];
    int cellX = (int)((x - m_box[0][0]) / m_cellSize);
    int cellY = (int)((y - m_box[0][1]) / m_cellSize);
    return heightAtNode(x, y, low, cellAt(cellX, cellY), height, mask, flags, second);
}

// ──────────────────────────────────────────────────────────────────────
// The bodies

// FS2 0x001f6a40
// callcheck: alloc

CollisionBodyArray::CollisionBodyArray()
    : m_count(0), m_capacity(kInitialCapacity)
{
    m_data = (CollisionBody **)gGlobalHeap.alloc((m_capacity + 2) * sizeof(CollisionBody *),
                                                 kBodyArrayAlign);
    m_data[0] = nullptr;
    m_data[1] = nullptr;
}

// FS2 0x00184430
// callcheck: FUN_001845f0, free, operator_delete (the reset is nothing on the way out)

CollisionBodyArray::~CollisionBodyArray()
{
    gGlobalHeap.free(m_data);
}

// FS2 0x001f6760
// callcheck: alloc, free

void CollisionBodyArray::reserve(u_short capacity)
{
    if (capacity <= m_capacity)
        return;
    u_short count = m_count;
    CollisionBody **old = m_data;
    m_capacity = capacity;
    m_data = (CollisionBody **)gGlobalHeap.alloc((m_capacity + 2) * sizeof(CollisionBody *),
                                                 kBodyArrayAlign);
    for (int i = 0; i < count + 2; i++)
        m_data[i] = old[i];
    gGlobalHeap.free(old);
}

// Doubles the block when full.
// FS2 0x001f6910
// callcheck: alloc, free

int CollisionBodyArray::push(CollisionBody *const &body)
{
    if (m_count >= m_capacity)
        reserve((u_short)((m_capacity + 2) * 2 - 2));
    m_count++;
    m_data[m_count] = body;
    m_data[m_count + 1] = nullptr;
    return 1;
}

// FS2 0x001f6820

CollisionBody **CollisionBodyArray::find(CollisionBody *const &body)
{
    for (CollisionBody **slot = begin(); slot != nullptr; slot = next(slot)) {
        if (*slot == body)
            return slot;
    }
    return nullptr;
}

// FS2 0x001f6880

CollisionBody **CollisionBodyArray::erase(CollisionBody **slot)
{
    if (m_count == 0)
        return nullptr;
    for (CollisionBody **from = next(slot); from != nullptr; from = next(from))
        from[-1] = *from;
    m_data[m_count] = nullptr;
    m_count--;
    return *slot == nullptr ? nullptr : slot;
}

// FS2 0x001f6690
// callcheck: CollisionBodyArray, BoundingSphere, FUN_00236b80, FUN_001f4c20, memset, memset (0x236b80 clears capsuleMatrix, 0x1f4c20 is the list's ctor)

CollisionBody::CollisionBody(CollisionOwner *bodyOwner, int bodyBucket, u_int bodyCategory,
                             u_int bodyMask)
    : on(1), settled(0), synced(0), boxed(0), owner(bodyOwner), bucket(bodyBucket),
      category(bodyCategory), mask(bodyMask), shape(0), rayMask(0), rayFlags(0),
      capsuleRadius(0.0f), capsuleMatrix(nullptr), mesh(nullptr)
{
    memset(box, 0, sizeof(box));
    memset(point, 0, sizeof(point));
    memset(segment, 0, sizeof(segment));
    memset(capsule, 0, sizeof(capsule));
}

// FS2 0x001f6670
// callcheck: updateBox

void CollisionBody::refresh()
{
    settled = 1;
    synced = 1;
    boxed = 1;
    updateBox();
}

// FS2 0x001f6590
// callcheck: toBox, toBox

void CollisionBody::updateBox()
{
    if (shape == CollisionShape_Spheres) {
        bool first = true;
        for (StageList<BoundingSphere>::Node *node = spheres.head; node != nullptr; node = node->next) {
            sceVu0FVECTOR one[2];
            node->value.toBox(one);
            if (first) {
                first = false;
                copyVec(box[0], one[0]);
                copyVec(box[1], one[1]);
            } else {
                for (int lane = 0; lane < 4; lane++) {
                    box[0][lane] = std::min(box[0][lane], one[0][lane]);
                    box[1][lane] = std::max(box[1][lane], one[1][lane]);
                }
            }
        }
    } else if (shape == CollisionShape_Sphere) {
        sphere.toBox(box);
    }
}

// FS2 0x001f6510
// callcheck: __construct_array, CollisionBodyArray, ~CollisionBodyArray, reserve

SceneRecordBuckets::SceneRecordBuckets()
{
    for (int i = 0; i < kCollisionBuckets; i++)
        m_lists[i].reserve(kBucketCapacity);
}

// Every body left, with what it holds; the owners that remove theirs have.
// FS2 0x001f63f0
// callcheck: FUN_00184260, FUN_00184430, operator_delete, __destroy_arr, operator_delete (the body's dtor inline)

SceneRecordBuckets::~SceneRecordBuckets()
{
    for (int i = 0; i < kCollisionBuckets; i++) {
        for (CollisionBody **slot = m_lists[i].begin(); slot != nullptr;
             slot = CollisionBodyArray::next(slot))
            delete *slot;
    }
}

// The new body against every other: it goes on the list of whichever side's
// mask meets the other's category. Bodies of one category in one bucket pair
// up once, by position; two bodies of an object never do, nor one with what
// its owner calls its parent.
// FS2 0x001f6180
// callcheck: push, getCollisionObject, getCollisionObject, getCollisionParent, getCollisionParent, getCollisionObject, push, getCollisionObject, getCollisionObject, getCollisionParent, getCollisionParent, getCollisionObject, push

void SceneRecordBuckets::add(CollisionBody *body)
{
    const int bucket = body->bucket;
    const int before = m_lists[bucket].m_count;
    m_lists[bucket].push(body);
    CollisionOwner *owner = body->owner;

    auto canMeet = [&](CollisionBody *other) {
        StageObject *object = other->owner->getCollisionObject();
        if (owner->getCollisionObject() == object)
            return false;
        StageObject *parent = owner->getCollisionParent();
        return parent == nullptr || parent != object;
    };

    for (int list = 0; list < kCollisionBuckets; list++) {
        int index = 0;
        for (CollisionBody **slot = m_lists[list].begin(); slot != nullptr;
             slot = CollisionBodyArray::next(slot), index++) {
            CollisionBody *other = *slot;
            const bool apart = list != bucket || body->category != other->category;
            if ((body->category & other->mask) != 0) {
                if ((!apart && before < index) || !canMeet(other))
                    continue;
                other->partners.push(body);
            }
            if ((body->mask & other->category) != 0 && (apart || before <= index)
                && canMeet(other))
                body->partners.push(other);
        }
    }
}

// FS2 0x001f60a0
// callcheck: find, erase, find, erase

void SceneRecordBuckets::remove(CollisionBody *body)
{
    CollisionBodyArray &own = m_lists[body->bucket];
    CollisionBody **slot = own.find(body);
    if (slot == nullptr)
        return;
    own.erase(slot);
    for (int list = 0; list < kCollisionBuckets; list++) {
        for (CollisionBody **walk = m_lists[list].begin(); walk != nullptr;
             walk = CollisionBodyArray::next(walk)) {
            CollisionBodyArray &partners = (*walk)->partners;
            CollisionBody **entry = partners.find(body);
            if (entry != nullptr)
                partners.erase(entry);
        }
    }
}

// What the sphere's walk on a mesh does with a face: both owners are told the
// push, as a direction and a length.
// FS2 0x001f5fd0
// callcheck: vecNormalize (the owners' slot 0, touchBoth)

static int collisionFaceTouched(StageCollisionFace *face, const sceVu0FVECTOR hit,
                                const sceVu0FVECTOR push, void *context)
{
    (void)hit;
    float depth = sqrtf(push[2] * push[2] + push[0] * push[0] + push[1] * push[1]);
    sceVu0FVECTOR normal;
    vecNormalize(normal, push);
    touchBoth((CollisionPair *)context, depth, normal, face);
    return 0;
}

// self's kinds of shape against other's; the others mean nothing. The sphere
// indices of a list are not reset for an inner list (the ROM's), and a segment
// against a sphere reads the sphere of the segment's body and the segment of
// the sphere's body (the ROM's too, nothing written pairs them).
// FS2 0x001f5680
// callcheck: hitsCapsule, vecDistance, memcpy, hitsCapsule, vecDistance, memcpy, visitSphere, boxesOverlap, hitsSphere, vecNormalize, visitSphere, boxesOverlap, hitsSphere, vecNormalize, hitsSphere, vecNormalize, raycast, vecDistance, vecDirection, vecDistance, memcpy, hitsSegment0, vecDistance, memcpy, boxContains (the owners' slot 0, touchBoth)

void SceneRecordBuckets::collidePair(CollisionBody *self, CollisionBody *other)
{
    CollisionPair pair = { { { self, other, 0, 0 }, { other, self, 0, 0 } } };
    int &selfIndex = pair.side[0].selfIndex;
    int &otherIndex = pair.side[0].bodyIndex;

    auto noteNearest = [&](float distance) {
        if (distance < m_nearest) {
            m_nearest = distance;
            m_nearestPair = pair;
            m_nearestIsMap = 0;
        }
    };
    auto sphereOnSphere = [&](const BoundingSphere *one, const BoundingSphere *two) {
        sceVu0FVECTOR normal;
        if (one->hitsSphere(two->radius, two->center, nullptr, normal)) {
            float depth = (one->radius + two->radius) - normal[3];
            vecNormalize(normal, normal);
            touchBoth(&pair, depth, normal, nullptr);
        }
    };
    auto nextOther = [&]() {
        otherIndex++;
        pair.side[1].selfIndex++;
    };
    auto nextSelf = [&]() {
        selfIndex++;
        pair.side[1].bodyIndex++;
    };

    switch (self->shape) {
    case CollisionShape_Capsule:
        if (other->shape == CollisionShape_Spheres) {
            for (StageList<BoundingSphere>::Node *node = other->spheres.head; node != nullptr;
                 node = node->next) {
                if (node->value.hitsCapsule(self->capsuleRadius, self->capsule[0], self->capsule[1],
                                            self->capsuleMatrix)) {
                    touchBoth(&pair, 0.0f, nullptr, nullptr);
                    noteNearest(vecDistance(self->capsule[0], node->value.center));
                }
                nextOther();
            }
        } else if (other->shape == CollisionShape_Sphere) {
            if (other->sphere.hitsCapsule(self->capsuleRadius, self->capsule[0], self->capsule[1],
                                          self->capsuleMatrix)) {
                touchBoth(&pair, 0.0f, nullptr, nullptr);
                noteNearest(vecDistance(self->capsule[0], other->sphere.center));
            }
        }
        break;

    case CollisionShape_Spheres:
        if (other->shape == CollisionShape_Mesh) {
            for (StageList<BoundingSphere>::Node *node = self->spheres.head; node != nullptr;
                 node = node->next) {
                other->mesh->visitSphere(collisionFaceTouched, &pair, self->rayMask, self->rayFlags,
                                         &node->value);
                nextSelf();
            }
        } else if (other->shape == CollisionShape_Spheres && boxesOverlap(self->box, other->box)) {
            for (StageList<BoundingSphere>::Node *node = self->spheres.head; node != nullptr;
                 node = node->next) {
                for (StageList<BoundingSphere>::Node *inner = other->spheres.head; inner != nullptr;
                     inner = inner->next) {
                    sphereOnSphere(&node->value, &inner->value);
                    nextOther();
                }
                nextSelf();
            }
        }
        break;

    case CollisionShape_Sphere:
        if (other->shape == CollisionShape_Mesh) {
            other->mesh->visitSphere(collisionFaceTouched, &pair, self->rayMask, self->rayFlags,
                                     &self->sphere);
        } else if (other->shape == CollisionShape_Spheres) {
            if (boxesOverlap(self->box, other->box)) {
                for (StageList<BoundingSphere>::Node *node = other->spheres.head; node != nullptr;
                     node = node->next) {
                    sphereOnSphere(&self->sphere, &node->value);
                    nextOther();
                }
            }
        } else if (other->shape == CollisionShape_Sphere) {
            sphereOnSphere(&self->sphere, &other->sphere);
        }
        break;

    case CollisionShape_Segment:
        if (other->shape == CollisionShape_Mesh) {
            sceVu0FVECTOR hit;
            StageCollisionFace *face = other->mesh->raycast(hit, self->segment[0], self->segment[1],
                                                            self->rayMask, self->rayFlags,
                                                            FaceSide_Any);
            if (face != nullptr) {
                float fromEnd = vecDistance(hit, self->segment[1]);
                sceVu0FVECTOR sweep;
                vecDirection(sweep, self->segment[0], self->segment[1]);
                touchBoth(&pair, fromEnd, sweep, face);
                float fromStart = vecDistance(self->segment[0], hit);
                if (fromStart < m_nearest) {
                    m_nearest = fromStart;
                    m_nearestPair = pair;
                    m_nearestIsMap = 1;
                    copyVec(m_nearestSweep, sweep);
                    m_nearestT = fromEnd;
                    m_nearestFace = face;
                }
            }
        } else if (other->shape == CollisionShape_Sphere
                   && self->sphere.hitsSegment0(other->segment[0], other->segment[1])) {
            touchBoth(&pair, 0.0f, nullptr, nullptr);
            noteNearest(vecDistance(self->segment[0], self->sphere.center));
        }
        break;

    case CollisionShape_Camera:
        if (other->shape == CollisionShape_Spheres
            && boxContains(other->box[0], other->box[1], self->point))
            touchBoth(&pair, 0.0f, nullptr, nullptr);
        break;
    }
}

// The frame's collision: what moves syncs and boxes on demand, every body
// with a mask meets its partners, and hears of the nearest last.
// FS2 0x001f5250
// callcheck: getCollisionObject, getCollisionObject, syncBody, syncBody, updateBox, updateBox, collidePair, onCollide, onCollide, onCollide, onCollide, collisionHook3, updateBox (virtual slots)

void SceneRecordBuckets::collide()
{
    for (int list = 0; list < kCollisionBuckets; list++) {
        for (CollisionBody **slot = m_lists[list].begin(); slot != nullptr;
             slot = CollisionBodyArray::next(slot)) {
            CollisionBody *body = *slot;
            if (!body->settled) {
                body->synced = 0;
                body->boxed = 0;
            }
        }
    }

    auto ready = [](CollisionBody *body) {
        if (!body->synced) {
            body->owner->syncBody(body);
            body->synced = 1;
        }
    };
    auto boxed = [](CollisionBody *body) {
        if (!body->boxed) {
            body->updateBox();
            body->boxed = 1;
        }
    };

    for (int list = 0; list < kCollisionBuckets; list++) {
        for (CollisionBody **slot = m_lists[list].begin(); slot != nullptr;
             slot = CollisionBodyArray::next(slot)) {
            CollisionBody *self = *slot;
            CollisionOwner *owner = self->owner;
            if (self->mask == 0 || !self->on || !owner->m_enabled)
                continue;
            StageObject *object = owner->getCollisionObject();
            if (!object->m_active || object->m_dead)
                continue;

            m_nearest = kFloatMax;
            for (CollisionBody **partner = self->partners.begin(); partner != nullptr;
                 partner = CollisionBodyArray::next(partner)) {
                CollisionBody *other = *partner;
                if (!other->on || !other->owner->m_enabled)
                    continue;
                StageObject *otherObject = other->owner->getCollisionObject();
                if (!otherObject->m_active || otherObject->m_dead || object->m_dead)
                    continue;
                ready(self);
                ready(other);
                boxed(self);
                boxed(other);
                collidePair(self, other);
            }

            if (m_nearest != kFloatMax) {
                CollisionContact *first = &m_nearestPair.side[0];
                CollisionContact *second = &m_nearestPair.side[1];
                const float *sweep = m_nearestIsMap ? m_nearestSweep : nullptr;
                StageCollisionFace *face = m_nearestIsMap ? m_nearestFace : nullptr;
                float t = m_nearestIsMap ? m_nearestT : 0.0f;
                if (first->self->owner->onCollide(first, sweep, face, t, m_nearest) != 0)
                    first->self->boxed = 0;
                if (second->self->owner->onCollide(second, sweep, face, -t, m_nearest) != 0)
                    second->self->boxed = 0;
            }
            owner->collisionHook3(self);
            boxed(self);
        }
    }
}
