/* ==========================================================================
 *  game/areaclip.h
 *
 *  What the camera's place hides, 0x2dac70..0x2e00c0: the areaclip keyword's
 *  file ("ac1") holds boxes, each a matrix taking the world to the cube
 *  -1..1, and for each box the names it hides and, optionally, the boxes
 *  whose characters it hides. A grid of 8 x 8 cells over the map keeps which
 *  boxes touch each cell. Each frame update finds the boxes the camera is in
 *  and hides or shows what changed. The class, the file and every name here
 *  are the port's: no source string, no symbols. architecture.md "The area
 *  clip".
 * ======================================================================== */
#ifndef _GAME_AREACLIP_H
#define _GAME_AREACLIP_H

#include <cstddef>
#include <vector>

#include "game/gamehooks.h"
#include "game/s2ddata.h"

constexpr int    kAreaClipCells = 8;                // a side of the grid
constexpr u_int  kAreaClipFileMagic = 0x00316361;   // "ac1": as on the disc
constexpr u_int  kAreaClipRelocated = 0x00314341;   // "AC1": relocate has been here
constexpr u_int  kAreaClipMagicFold = 0x00202020;   // magic | this: its case does not matter
constexpr short  kAreaClipListEnd = -1;             // ends a list of the file
constexpr size_t kAreaClipWholeName = 0xff;         // a length that compares a whole name (modelFindPart)
constexpr u_int  kAreaClipHidingSpace = 200;        // what update reserves

// The file. EE layout: it is the file; relocate sets its refs in place (on
// the disc they are offsets from the file's start).
struct AreaClipFile
{
    void relocate();                                                     // FS2 0x002dceb0

    u_int magic;                // FS2 +0x00: kAreaClipFileMagic, then kAreaClipRelocated
    u_int boxCount;             // FS2 +0x04
    u_int targetCount;          // FS2 +0x08
    u_char _0c[4];
    float min[4];               // FS2 +0x10: the map's corner, x and y read
    float max[4];               // FS2 +0x20: the other one
    S2dRef<float[4][4]> boxes;  // FS2 +0x30: boxCount, 0x40 each
    S2dRef<S2dRef<const char> > targetNames; // FS2 +0x34: targetCount
    // Per box, indices into targetNames, up to kAreaClipListEnd.
    S2dRef<S2dRef<short> > hiddenTargets; // FS2 +0x38
    // Per box, boxes a character hides in while the camera is here, up to
    // kAreaClipListEnd. May be null, as a whole and each list.
    S2dRef<S2dRef<short> > hiddenBoxes; // FS2 +0x3c
};
static_assert(offsetof(AreaClipFile, boxes) == 0x30, "AreaClipFile::boxes");
static_assert(sizeof(AreaClipFile) == 0x40, "AreaClipFile: EE 0x40");

// A particle effect of the stage as a target (vtable 0x31a6e0, EE 8): hidden
// stops its emitter. The name is the file's. layout: host.
class ClipEffect : public ClipTarget
{
public:
    explicit ClipEffect(const char *name) : m_name(name) {}
    // FS2 0x002dcff0: deleting
    ~ClipEffect() override {}
    void   setClipped(u_char clipped) override;                          // FS2 0x002dd050
    // FS2 0x002dd0b0
    u_char getClipped() const override { return 0; }

private:
    const char *m_name;         // FS2 +0x04
};

// StageScene +0x58: built over the areaclip keyword's file (setAreaClip).
// layout: host (EE 0x34).
class AreaClip
{
public:
    explicit AreaClip(void *file);                                       // FS2 0x002dccf0
    ~AreaClip();                                                         // FS2 0x002dcab0: deleting
    // Looks up every target: the stage's models exist by now.
    void setup();                                                        // FS2 0x002dcc30
    // The frame: camera may be null (no box then).
    void update(StageObject *camera);                                    // FS2 0x002dbd10

private:
    typedef std::vector<u_short> IndexList;

    ClipTarget *findTarget(const char *name);                            // FS2 0x002dacc0
    bool boxTouchesCell(const float (*box)[4], u_int cellX, u_int cellY) const; // FS2 0x002db080
    // The size of a cell; the cell of a world place. A place off the map
    // answers past the grid, except up to a cell before it (the ROM's cast).
    void  getCellSize(float &width, float &height) const;
    u_int cellOf(float place, int axis, float size) const;
    // Does a box of the list hide this target too?
    bool boxesHide(short target, const IndexList &boxes) const;

    AreaClipFile           *m_file = nullptr;     // FS2 +0x00
    std::vector<std::vector<IndexList> > m_cells; // FS2 +0x04: [x][y], the boxes touching the cell, sorted
    std::vector<ClipTarget *> m_targets;          // FS2 +0x10: by the file's name, null when nothing has that name
    IndexList               m_boxes;              // FS2 +0x1c: the boxes the camera was in, sorted
    std::vector<ClipTarget *> m_effects;          // FS2 +0x28: the ClipEffects, owned
};

#endif /* _GAME_AREACLIP_H */
