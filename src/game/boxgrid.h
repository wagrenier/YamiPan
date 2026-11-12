/* ==========================================================================
 *  game/boxgrid.h
 *
 *  A 2D lookup grid over a set of oriented boxes (game/boxgrid.cpp,
 *  0x2647e0..0x2676e0).
 *
 *  A BoxSet is a block of loaded data: a bounding rectangle and a run of
 *  boxes, each stored as the matrix taking world points into the box's unit
 *  cube. BoxGrid cuts the rectangle (x, y) into size x size cells and lists,
 *  per cell, the boxes whose xy footprint touches it, so that a point query
 *  (BoxGrid::find) tests only its own cell's boxes. The stage loader 0x19fcd0
 *  builds one with size 8 through 0x190900 and keeps it at +0x3c of the
 *  object behind gp-0x7a7c +0x1c; 0x1908d0 asks BoxGrid::contains. What the
 *  boxes mean to the game (areas, volumes) is not read yet.
 * ======================================================================== */
#ifndef _GAME_BOXGRID_H
#define _GAME_BOXGRID_H

#include <stddef.h>
#include <vector>

#include "sdk/libvu0.h"     /* sceVu0FVECTOR / sceVu0FMATRIX */
#include "sdk/scetypes.h"

// One box: the matrix taking world points into its unit cube (row vectors,
// p * toLocal; inside when |x|, |y|, |z| <= w: boxContainsPoint).
struct Box
{
    sceVu0FMATRIX toLocal;          // FS2 +0x00
};
static_assert(sizeof(Box) == 0x40, "Box");

// A block of boxes as loaded (file data: EE bytes).
struct BoxSet
{
    int           _00;              // FS2 +0x00: not read here
    int           count;            // FS2 +0x04: boxes from +0x40
    u_char        _08[0x18];        // FS2 +0x08: not read here
    sceVu0FVECTOR min;              // FS2 +0x20: xy = the grid's low corner
    sceVu0FVECTOR max;              // FS2 +0x30: xy = the grid's high corner
    Box           boxes[1];         // FS2 +0x40: count of them
};
static_assert(offsetof(BoxSet, count) == 0x04, "BoxSet::count");
static_assert(offsetof(BoxSet, min) == 0x20, "BoxSet::min");
static_assert(offsetof(BoxSet, max) == 0x30, "BoxSet::max");
static_assert(offsetof(BoxSet, boxes) == 0x40, "BoxSet::boxes");

// The cell lists. On the EE each is an MSL vector, 0xc bytes
// {capacity +0, size +4, data +8}; their out-of-line instances
// (0x265970..0x2676e0) are the host <vector>'s.
typedef std::vector<const Box *> BoxList;       // one cell
typedef std::vector<BoxList>     BoxColumn;     // one x, every y
typedef std::vector<BoxColumn>   BoxCells;      // [x][y]

// The grid (operator new 0x18 on the EE).
// layout: host (std::vector members; EE offsets in the comments)
class BoxGrid
{
public:
    BoxGrid(const BoxSet *set, u_int size);                              // FS2 0x00265740

    const Box *find(const sceVu0FVECTOR point) const;                    // FS2 0x00265570
    bool contains(const sceVu0FVECTOR point) const;                      // FS2 0x00265720

private:
    bool cellTouchesBox(const Box *box, u_int ix, u_int iy) const;       // FS2 0x002647e0

    // EE, 0x18 bytes:
    const BoxSet *m_set;            // FS2 +0x00
    BoxCells      m_cells;          // FS2 +0x04: size x size lists, [x][y]
    float         m_scaleX;         // FS2 +0x10: cells per world unit along x
    float         m_scaleY;         // FS2 +0x14: along y
};

#endif /* _GAME_BOXGRID_H */
