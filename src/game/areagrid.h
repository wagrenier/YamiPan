/* ==========================================================================
 *  game/areagrid.h
 *
 *  The map's area boxes and the grids that find them, 0x283ff0..0x2849c0,
 *  and the distance volume curve 0x283df0 before them (the file name is the
 *  port's; no source string names it). The ENV_ boxes of the map file are
 *  AreaBoxes (StageFiles::placeMapBoxes); the scene and its render each keep
 *  grids of them, which own their boxes.
 * ======================================================================== */
#ifndef _GAME_AREAGRID_H
#define _GAME_AREAGRID_H

#include "game/stagelist.h"
#include "game/stagenode.h"
#include "sdk/libvu0.h"     /* sceVu0FVECTOR */
#include "sdk/scetypes.h"

// Cells per side of an AreaGrid.
constexpr int kAreaGridSide  = 8;
constexpr int kAreaGridCells = kAreaGridSide * kAreaGridSide;

// A box of the map (vtable 0x314180 over StageNode, EE 0x140): the
// AreaGrids hold these. The box +0x100..+0x118, in the node's own space, is
// centred on x / y; z runs from 0 to height, whichever way height points.
// The dtor is the implicit one (slot 2, 0x186550).
// layout: host
class AreaBox : public StageNode
{
public:
    AreaBox();                                                           // FS2 0x00284950
    void setSize(float width, float height, float depth);                // FS2 0x002848d0
    // pos (world, xyz) inside the box.
    bool contains(const sceVu0FVECTOR pos);                              // FS2 0x00284690
    // The world bounds of the box's eight corners.
    void getWorldBounds(sceVu0FVECTOR min, sceVu0FVECTOR max);           // FS2 0x00284740

    sceVu0FVECTOR m_min;        // FS2 +0x100: 0 from the ctor, w stays 0
    sceVu0FVECTOR m_max;        // FS2 +0x110: 0 from the ctor, w stays 0
};

// An 8 x 8 grid over the boxes' xy bounds. addBox lists a box, build sizes
// the grid and enters each box in every cell its bounds touch; a point then
// tests only its cell's boxes. Boxes added after build are in no cell.
// StageScene +0xac..+0xb8 has four (StageArea); SceneRender has its own.
// layout: host -- EE 0x240: StageLists 8 bytes each, the bounds at +0x210.
struct AreaGrid
{
    AreaGrid();                                                          // FS2 0x00284640
    ~AreaGrid();                                                         // FS2 0x00284580: deleting; deletes the boxes
    // The first box of pos's cell that holds pos, or null.
    AreaBox *contains(const sceVu0FVECTOR pos);                          // FS2 0x00284190
    void addBox(AreaBox *box);                                           // FS2 0x00284560
    // The boxes at pos, at most max of them; how many.
    int  collect(const sceVu0FVECTOR pos, AreaBox **out, int max);       // FS2 0x00283ff0
    // Bounds and cells from the boxes added so far.
    void build();                                                        // FS2 0x002842f0

    StageList<AreaBox *> m_boxes;                  // FS2 +0x000
    StageList<AreaBox *> m_cells[kAreaGridCells];  // FS2 +0x008: x + y * kAreaGridSide
    sceVu0FVECTOR m_min;        // FS2 +0x210
    sceVu0FVECTOR m_max;        // FS2 +0x220
    float   m_cellSizeX;        // FS2 +0x230
    float   m_cellSizeY;        // FS2 +0x234
    u_char  _238;               // FS2 +0x238: 1 from the ctor; no reader found yet

private:
    // The first link of pos's cell; null with no boxes or outside the bounds.
    StageList<AreaBox *>::Node *cellAt(const sceVu0FVECTOR pos);
};

// A placed sound's volume at ratio (its distance over its range, 0..1;
// 0 beyond 1) by the entry's falloff curve (SeFalloff), from volume
// (SePlayer / SoundTrack). In this file by link position only.
u_int seDistanceVolume(float ratio, int falloff, u_int volume);          // FS2 0x00283df0

#endif /* _GAME_AREAGRID_H */
