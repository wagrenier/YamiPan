// game/boxgrid.cpp -- a 2D lookup grid over a set of oriented boxes
// (game/boxgrid.h): building the per-cell box lists and finding the box a
// point is in.

#include "game/boxgrid.h"

#include <algorithm>
#include <cmath>

#include "game/gamehooks.h"

// Below this a line counts as vertical, two slopes or intercepts as equal
// (0x38d1b717).
constexpr float kLineEpsilon = 0.0001f;

// The EE's float -> unsigned conversion (CodeWarrior: cvt.w.s, with 2^31
// taken off above it). A point left of the grid gives a negative int, a huge
// index the size test rejects.
// PC: (u_int) of a negative float is undefined in C++; go through int as the EE does.
static inline u_int floatToUint(float f)
{
    if (f < 2147483648.0f)
        return (u_int)(int)f;
    return (u_int)(int)(f - 2147483648.0f) | 0x80000000u;
}

// A 2D line through two points, as quadCrossesSegment compares them: either
// vertical at x, or slope / intercept; lo..hi is the segment's extent along
// x (along y when vertical). The ROM inlines this for both lines.
// layout: host (a helper of locals, no EE object)
struct SegmentLine
{
    bool  vertical;
    float x;
    float slope;
    float intercept;
    float lo;
    float hi;
};

static inline SegmentLine segmentLine(const float *a, const float *b)
{
    SegmentLine l;
    if (*b < *a)                    // order the end points by x
        std::swap(a, b);
    float dx = b[0] - a[0];
    l.vertical = std::fabs(dx) <= kLineEpsilon;
    if (!l.vertical) {
        l.slope     = (b[1] - a[1]) / dx;
        l.intercept = a[1] - a[0] * l.slope;
        l.lo        = std::min(a[0], b[0]);
        l.hi        = std::max(a[0], b[0]);
    } else {
        l.x  = a[0];
        l.lo = std::min(a[1], b[1]);
        l.hi = std::max(a[1], b[1]);
    }
    return l;
}

// ──────────────────────────────────────────────────────────────────────
// Whether the 2D segment ab crosses an edge of the quad (four xy points,
// in order). Collinear overlapping edges count; two vertical lines count
// whenever they share x, whatever their y extents (as the ROM tests it).
// FS2 0x00264d40
// callcheck: max, min, swap (called from segmentLine, inlined twice in the ROM)

static bool quadCrossesSegment(const float *const quad[4], const float *a, const float *b)
{
    SegmentLine s = segmentLine(a, b);
    for (int i = 0; i < 4; i++) {
        SegmentLine e = segmentLine(quad[i], quad[(i + 1) % 4]);
        bool hit;
        if (!s.vertical && !e.vertical) {
            float dslope = s.slope - e.slope;
            if (std::fabs(dslope) >= kLineEpsilon) {
                float x = (e.intercept - s.intercept) / dslope;
                hit = !(x < s.lo || s.hi < x || x < e.lo || e.hi < x);
            } else if (std::fabs(e.intercept - s.intercept) < kLineEpsilon) {
                hit = e.lo <= s.hi && !(e.hi < s.lo);       // the same line: overlap
            } else {
                hit = false;
            }
        } else if (s.vertical && e.vertical) {
            hit = std::fabs(s.x - e.x) < kLineEpsilon;
        } else if (s.vertical) {
            float y = s.x * e.slope + e.intercept;
            hit = !(s.x < e.lo || e.hi < s.x || y < s.lo || s.hi < y);
        } else {
            float y = e.x * s.slope + s.intercept;
            hit = !(e.x < s.lo || s.hi < e.x || y < e.lo || e.hi < y);
        }
        if (hit)
            return true;
    }
    return false;
}

// The template instances 0x2651e0..0x265250 the ROM calls from here (and
// from 0x2db540) are the host's:
//   FS2 0x002651e0  std::max (const float &, const float &)
//   FS2 0x00265200  std::min (const float &, const float &)
//   FS2 0x00265220  std::fabs
//   FS2 0x00265230  std::swap<const float *>

// ──────────────────────────────────────────────────────────────────────
// Whether (x, y) lies inside the quad (four xy points in order): the point
// is on the same side of all four edges.
// FS2 0x00265250

static bool quadContainsPoint(float x, float y, const float *const quad[4])
{
    bool side = false;
    for (int i = 0; i < 4; i++) {
        const float *a = quad[i];
        const float *b = quad[(i + 1) % 4];
        float ex = b[0] - a[0];
        float ey = b[1] - a[1];
        bool s = ex * (y - a[1]) + ey * -(x - a[0]) < 0.0f;
        if (i == 0)
            side = s;
        else if (s != side)
            return false;
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// Whether the box's xy footprint touches cell (ix, iy): one of its corners
// lies in the cell, a cell corner lies in one of its faces, or a cell edge
// crosses a face. The box's matrix is inverted to take the unit cube's
// eight corners back to the world (VU0 macro code in the ROM, inlined).
// FS2 0x002647e0

bool BoxGrid::cellTouchesBox(const Box *box, u_int ix, u_int iy) const
{
    u_int size = (u_int)m_cells.size();
    if (ix >= size || iy >= size)
        return false;

    // toWorld = the inverse of toLocal: the 3x3 inverse from the cross
    // products of toLocal's columns over its determinant, then the
    // translation taken back through it.
    const float (*m)[4] = box->toLocal;
    float t[3][3];
    for (int c = 0; c < 3; c++)
        for (int r = 0; r < 3; r++)
            t[c][r] = m[r][c];
    sceVu0FMATRIX toWorld;
    for (int k = 0; k < 3; k++) {
        const float *u = t[(k + 1) % 3];
        const float *v = t[(k + 2) % 3];
        toWorld[k][0] = u[1] * v[2] - u[2] * v[1];
        toWorld[k][1] = u[2] * v[0] - u[0] * v[2];
        toWorld[k][2] = u[0] * v[1] - u[1] * v[0];
        toWorld[k][3] = 0.0f;
    }
    float det = t[0][0] * toWorld[0][0] + t[0][1] * toWorld[0][1] + t[0][2] * toWorld[0][2];
    float q = 1.0f / det;
    for (int k = 0; k < 3; k++)
        for (int c = 0; c < 3; c++)
            toWorld[k][c] *= q;
    for (int c = 0; c < 4; c++)
        toWorld[3][c] = -m[3][0] * toWorld[0][c] - m[3][1] * toWorld[1][c] - m[3][2] * toWorld[2][c];
    toWorld[3][3] = 1.0f;

    // Corner i has +1 on axis k when bit k of i is set, -1 otherwise.
    sceVu0FVECTOR corner[8];
    for (int i = 0; i < 8; i++) {
        sceVu0FVECTOR v;
        for (int k = 0; k < 3; k++)
            v[k] = (i & (1 << k)) ? 1.0f : -1.0f;
        v[3] = 1.0f;
        for (int c = 0; c < 4; c++)
            corner[i][c] = v[0] * toWorld[0][c] + v[1] * toWorld[1][c]
                         + v[2] * toWorld[2][c] + v[3] * toWorld[3][c];
    }

    const BoxSet *set = m_set;
    float cellW = (set->max[0] - set->min[0]) / (float)size;
    float cellH = (set->max[1] - set->min[1]) / (float)size;
    float x0 = cellW * (float)ix + set->min[0];
    float x1 = cellW * (float)(ix + 1) + set->min[0];
    float y0 = cellH * (float)iy + set->min[1];
    float y1 = cellH * (float)(iy + 1) + set->min[1];

    for (int i = 0; i < 8; i++)
        if (x0 <= corner[i][0] && corner[i][0] < x1 && y0 <= corner[i][1] && corner[i][1] < y1)
            return true;

    const float *const face[6][4] = {
        { corner[0], corner[1], corner[3], corner[2] },     // z -1
        { corner[4], corner[5], corner[7], corner[6] },     // z +1
        { corner[0], corner[2], corner[6], corner[4] },     // x -1
        { corner[1], corner[3], corner[7], corner[5] },     // x +1
        { corner[0], corner[1], corner[5], corner[4] },     // y -1
        { corner[2], corner[3], corner[7], corner[6] },     // y +1
    };
    for (int f = 0; f < 6; f++)
        for (int k = 0; k < 4; k++)
            if (quadContainsPoint((k & 1) ? x1 : x0, (k & 2) ? y1 : y0, face[f]))
                return true;

    // The cell's edges, {start xy, end xy}: bottom, top, left, right.
    const float edge[4][4] = {
        { x0, y0, x1, y0 },
        { x0, y1, x1, y1 },
        { x0, y0, x0, y1 },
        { x1, y0, x1, y1 },
    };
    for (int f = 0; f < 6; f++)
        for (int e = 0; e < 4; e++)
            if (quadCrossesSegment(face[f], &edge[e][0], &edge[e][2]))
                return true;
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// The first box of the point's cell that contains the point; 0 outside the
// grid or when none does.
// FS2 0x00265570

const Box *BoxGrid::find(const sceVu0FVECTOR point) const
{
    const BoxSet *set = m_set;
    u_int cx = floatToUint(m_scaleX * (point[0] - set->min[0]));
    u_int cy = floatToUint(m_scaleY * (point[1] - set->min[1]));
    u_int size = (u_int)m_cells.size();
    if (cx < size && cy < size) {
        const BoxList &list = m_cells[cx][cy];
        for (BoxList::const_iterator it = list.begin(); it != list.end(); ++it)
            if (boxContainsPoint((*it)->toLocal, point))
                return *it;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Whether some box contains the point.
// FS2 0x00265720

bool BoxGrid::contains(const sceVu0FVECTOR point) const
{
    return find(point) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Builds the size x size cell lists of the set's boxes and the scales from
// world units to cells.
// FS2 0x00265740
// callcheck: insert (push_back: the ROM calls BoxList::insert at end with a count of 1)

BoxGrid::BoxGrid(const BoxSet *set, u_int size)
    : m_set(set)
{
    m_cells.resize(size);
    for (u_int ix = 0; ix < size; ix++) {
        m_cells[ix].resize(size);
        for (u_int iy = 0; iy < size; iy++) {
            const Box *box = set->boxes;
            for (int i = 0; i < set->count; i++, box++)
                if (cellTouchesBox(box, ix, iy))
                    m_cells[ix][iy].push_back(box);
        }
    }
    m_scaleX = (float)size / (m_set->max[0] - m_set->min[0]);
    m_scaleY = (float)size / (m_set->max[1] - m_set->min[1]);
}

// The MSL std::vector instances of the cell lists, out of line in the ROM
// after the code above; the host <vector> provides them.
//   FS2 0x00265970  BoxList::insert (pos, n, value)
//   FS2 0x00265d90  BoxColumn::resize
//   FS2 0x00265e40  BoxCells::resize
//   FS2 0x00265ef0  BoxColumn::erase (first, last)
//   FS2 0x00266080  BoxColumn::insert (pos, n, value)
//   FS2 0x002665e0  BoxCells::erase (first, last)
//   FS2 0x00266770  BoxCells::insert (pos, n, value)
//   FS2 0x00266d90  BoxCells::insertIdentity0 (returns its argument)
//   FS2 0x00266da0  BoxCells::insertIdentity1 (returns its argument)
//   FS2 0x00266db0  BoxList::initRange (first, last)
//   FS2 0x00266ef0  BoxList::assign (first, last)
//   FS2 0x002671c0  BoxColumn::initRange (first, last)
//   FS2 0x00267320  BoxList::BoxList (copy)
//   FS2 0x00267390  BoxList::end
//   FS2 0x002673b0  BoxList::begin
//   FS2 0x002673c0  BoxList::initEmpty (the base constructor)
//   FS2 0x002673e0  BoxList::getAllocator (returns its argument)
//   FS2 0x002673f0  operator new (size, void *): placement new
//   FS2 0x00267400  BoxColumn::assign (first, last)
