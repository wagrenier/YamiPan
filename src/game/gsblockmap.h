/* ==========================================================================
 *  game/gsblockmap.h
 *
 *  The GS memory allocators (game/gsblockmap.cpp, 0x142dd0..0x144080):
 *  GsBlockMap, one bit per GS block (64 words) over the 0x4000 blocks of
 *  local memory, and its subclass TexArea, the texture area 0x3700..0x3fbe
 *  of gfxInitRenderTargets: long-lived allocations come from its top, and
 *  each packet list (layer) has a TexAreaList that places the frame's UI
 *  textures in the free pages below them, round robin, keyed by image.
 *  The file and class names are the port's (no source string).
 *
 *  PC: the host draw module keeps textures on the GPU (the user's rule: no
 *  GS memory model), so nothing on the host asks these for blocks to
 *  upload into; they are kept for the game code that calls them.
 * ======================================================================== */
#ifndef _GAME_GSBLOCKMAP_H
#define _GAME_GSBLOCKMAP_H

#include "sdk/scetypes.h"

class Tim2Texture;

// GS local memory in blocks, and blocks per page (one bitmap word).
constexpr u_int kGsBlockCount    = 0x4000;
constexpr u_int kGsBlocksPerPage = 32;
// GsBlockMap::findFree and alloc's "not found".
constexpr int   kGsNoBlock       = -1;

// GS blocks in use, one bit each (1 = in use), over [first, last).
// layout: host -- EE 0x80c: first +0, last +4, the bitmap +8, the vptr +0x808
// (Vt_00311120: mark, alloc, free).
class GsBlockMap
{
public:
    // All in use, then [first, last) marked free (through the vtable).
    GsBlockMap(u_int first, u_int last);                                 // FS2 0x00143f50
    // EE: not virtual (not in the vtable, called directly); virtual on the
    // host so that delete through the base is defined.
    virtual ~GsBlockMap();                                               // FS2 0x00143f00

    // EE slot 0 (+0x8): marks [lo, hi) in use or free (setRange). Out of
    // line in game/gfxtarget.cpp, as the ROM has it.
    virtual void mark(u_int lo, u_int hi, int used);                     // FS2 0x0012ea60
    // EE slot 1 (+0xc): findFree, then the blocks marked in use; answers
    // the first block or kGsNoBlock.
    virtual int  alloc(u_int count, int fromTop, int pageAligned);       // FS2 0x00143fd0
    // EE slot 2 (+0x10): marks [block, block + count) free (kGsNoBlock:
    // nothing).
    virtual void free(int block, u_int count);                           // FS2 0x00144040

    void setRange(u_int lo, u_int hi, int used);                         // FS2 0x001438c0
    // The first block of a free run of count blocks, or kGsNoBlock. Runs of
    // 1..3 blocks are looked for inside one page (word) unless pageAligned;
    // longer ones are whole free pages. fromTop: the highest page first.
    int  findFree(u_int count, int fromTop, int pageAligned);            // FS2 0x00143640

    u_int m_first;              // FS2 +0x000
    u_int m_last;               // FS2 +0x004
    u_int m_bits[kGsBlockCount / kGsBlocksPerPage]; // FS2 +0x008
};

// One placed texture: its image (the key), the pages [firstPage, endPage).
// Empty: {nullptr, -1, -1}. layout: host -- EE 0xc.
struct TexAreaSlot
{
    const void *key;            // FS2 +0x0: the Tim2Texture's TIM2 file (+0x40)
    int         firstPage;      // FS2 +0x4
    int         endPage;        // FS2 +0x8

    void clear()
    {
        key = nullptr;
        firstPage = -1;
        endPage = -1;
    }
};

// One packet list's texture pages: [firstPage, endPage) of the texture
// area, handed out from nextPage on and wrapping to the start when the
// rest is too small; the slots remember what is where so that an image
// already placed is not uploaded again. layout: host -- EE 0x14.
class TexAreaList
{
public:
    TexAreaList(u_int firstBlock, u_int lastBlock, u_int slotCount);     // inlined in TexArea::TexArea

    // The image's pages for tex: *tbp is its first block (TBP0). Answers 1
    // when it was newly placed (the caller uploads it), 0 when it is
    // already there; 1 with *tbp -0x20 when it does not fit at all.
    int  place(int *tbp, Tim2Texture *tex);                              // FS2 0x00143510
    // Stores {key, first, first + count} in the first empty slot (slot 0
    // when none), emptying every slot it overlaps.
    void insert(const void *key, int firstPage, int pageCount);          // FS2 0x00143450
    // Every slot empty (inlined in TexArea::resetLists and the ctor).
    void reset();

    int          m_firstPage;   // FS2 +0x00: (first block + 31) / 32
    int          m_endPage;     // FS2 +0x04: lowered to the first page in use
    int          m_nextPage;    // FS2 +0x08
    u_int        m_slotCount;   // FS2 +0x0c
    TexAreaSlot *m_slots;       // FS2 +0x10: on the global heap
};

// The texture area (EE 0x814, Vt_00311100). Every change to the block map
// lowers the lists' end to the first page with a block in use (clipLists),
// so the lists only use the pages below the area's allocations.
// layout: host -- EE: GsBlockMap's 0x80c, then +0x80c / +0x810.
class TexArea : public GsBlockMap
{
public:
    TexArea(u_int firstBlock, u_int lastBlock, u_int lists, u_int slotsPerList); // FS2 0x00143270

    // EE slot 0: setRange, then clipLists.
    void mark(u_int lo, u_int hi, int used) override;                    // FS2 0x00142e70
    // EE slot 1: GsBlockMap::alloc from the top, the arguments ignored
    // (as the ROM does it), then clipLists.
    int  alloc(u_int count, int fromTop, int pageAligned) override;      // FS2 0x001430f0
    // EE slot 2: GsBlockMap::free, then clipLists.
    void free(int block, u_int count) override;                          // FS2 0x00142fa0

    TexAreaList *getList(int list);                                      // FS2 0x00142dd0
    // Every list's slots back to empty: once per frame (gfxFlipDisplay).
    void resetLists();                                                   // FS2 0x00142df0

    u_int         m_listCount;  // FS2 +0x80c
    TexAreaList **m_lists;      // FS2 +0x810: on the global heap

private:
    // Inlined in the three slots.
    void clipLists();
};

#endif /* _GAME_GSBLOCKMAP_H */
