/* ==========================================================================
 *  rena/rena_2d_texture_vram.h
 *
 *  rena's texture VRAM (rena_2d_texture_vram.cpp, 0x2e0840..0x2e1700; the
 *  file name is a guess, no path string names it): GsRegionAlloc hands GS
 *  blocks [base, base + size) to Tim2Textures as GsRegions, best fit, the
 *  oldest unpinned region evicted when nothing fits, free neighbours merged
 *  again. gfxInitRenderTargets makes the long-lived one over 0x2800..0x3700
 *  (game/gfxtarget.cpp), drawSysInit a frame one (game/drawsys.cpp); rena's
 *  surface set-up (rena_draw 0x2e7240) assigns, uploads and binds through it.
 *
 *  PC: no GS memory. The bookkeeping is the game's, kept as it is; the
 *  packets it writes go nowhere the host draws from (the host module binds a
 *  texture by its owner record, rena_draw.cpp setSurface), and
 *  Tim2Texture::upload sends nothing.
 * ======================================================================== */
#ifndef _RENA_RENA_2D_TEXTURE_VRAM_H
#define _RENA_RENA_2D_TEXTURE_VRAM_H

#include "sdk/scetypes.h"

class Tim2Texture;              // rena/rena_2d_texture_tim2.h

// GsRegion::m_state.
enum GsRegionState
{
    GsRegion_Free     = 0,      // on the free list
    GsRegion_ToSend   = 1,      // assigned, the pixels not sent yet
    GsRegion_Sent     = 2,      // upload sent them
    GsRegion_Released = 3,      // its texture is gone (release); freed when found
};

// GsRegionAlloc::bind's flags: what to send with a texture's TEX0.
enum GsBindFlag
{
    GsBind_Tex0_1 = 1 << 0,     // A+D to TEX0_1
    GsBind_Tex0_2 = 1 << 1,     // A+D to TEX0_2
    GsBind_Vu1    = 1 << 2,     // UNPACK of the TEX0 and its second-CLUT twin to VU1
};

// A run of GS blocks, in the address order of its neighbours. EE 0x18.
// layout: host (pointers)
class GsRegion
{
public:
    // Tim2Texture's dtor: the region waits as released until the allocator
    // finds it.
    void release();                                                      // FS2 0x002e0840

    GsRegion    *m_lower;       // FS2 +0x00: the neighbour below
    GsRegion    *m_higher;      // FS2 +0x04: the neighbour above; the spare-record link
    u_int        m_block;       // FS2 +0x08: the first GS block
    u_int        m_size;        // FS2 +0x0c: in GS blocks
    short        m_pinned;      // FS2 +0x10: nonzero: alloc does not evict it (rena_draw pins it for a surface)
    short        m_state;       // FS2 +0x12: GsRegionState
    Tim2Texture *m_owner;       // FS2 +0x14: the texture in it, 0 if none
};

// The ROM's list of pointers over a fixed pool of N nodes (inlined at every
// use; no name survives). push_back takes a node without looking: the pool
// is never exhausted in the ROM's use (128 regions at most). EE 0x10 + 0xc N.
// layout: host (pointers)
template <class T, int N>
class GsPoolList
{
public:
    struct Node                 // EE 0xc
    {
        Node *next;             // FS2 +0x0: also the spare-node link
        Node *prev;             // FS2 +0x4
        T     value;            // FS2 +0x8
    };

    GsPoolList() { clear(); }

    // Empty, every node spare.
    void clear()
    {
        m_count = 0;
        m_spare = nullptr;
        m_tail = nullptr;
        m_head = nullptr;
        for (u_int i = 0; i < N; i++) {
            m_nodes[i].next = m_spare;
            m_spare = &m_nodes[i];
        }
    }

    u_int size() const { return m_count; }
    Node *head() const { return m_head; }

    T at(u_int index) const
    {
        Node *node = m_head;
        for (u_int i = 0; i < index; i++)
            node = node->next;
        return node->value;
    }

    void push_back(T value)
    {
        Node *node = m_spare;
        m_spare = node->next;
        node->value = value;
        node->prev = m_tail;
        if (m_tail == nullptr)
            m_head = node;
        else
            m_tail->next = node;
        m_tail = node;
        node->next = nullptr;
        m_count++;
    }

    // -1 when absent (remove then walks off the list: the ROM never asks).
    u_int indexOf(T value) const
    {
        u_int index = 0;
        for (Node *node = m_head; node != nullptr; node = node->next, index++) {
            if (node->value == value)
                return index;
        }
        return (u_int)-1;
    }

    // The ROM leaves a new head's prev pointing at the node it unlinked;
    // nothing reads a head's prev.
    void erase(u_int index)
    {
        Node *node = m_head;
        if (index == 0) {
            m_head = node->next;
            if (node == m_tail)
                m_tail = nullptr;
        } else {
            for (u_int i = 0; i < index; i++)
                node = node->next;
            node->prev->next = node->next;
            if (node->next == nullptr)
                m_tail = node->prev;
            else
                node->next->prev = node->prev;
        }
        if (node != nullptr) {
            node->next = m_spare;
            m_spare = node;
        }
        m_count--;
    }

    void remove(T value) { erase(indexOf(value)); }

private:
    u_int m_count;              // FS2 +0x00
    Node *m_head;               // FS2 +0x04
    Node *m_tail;               // FS2 +0x08
    Node *m_spare;              // FS2 +0x0c
    Node  m_nodes[N];           // FS2 +0x10
};

// The region allocator over GS blocks [base, base + size). EE 0x1838, the
// vptr (0x31a7b0) after the data at +0x1834. layout: host (pointers, vptr
// first)
class GsRegionAlloc
{
public:
    enum
    {
        kRegions = 128,         // region records and list nodes
    };

    GsRegionAlloc(int base, int size);                                   // FS2 0x002e14d0
    // Detaches every texture still holding a region. EE slot 0 (+0x8),
    // deleting (obj, flag).
    virtual ~GsRegionAlloc();                                            // FS2 0x002e1440

    // A region for a texture that has none: alloc(its blocks), pinned as
    // given, to be sent.
    void assign(Tim2Texture *texture, short pinned);                     // FS2 0x002e0a10
    // A texture whose region waits to be sent: its upload at the region.
    void upload(u_long **cursor, Tim2Texture *texture);                  // FS2 0x002e09b0
    // The texture's TEX0 at its region (GsBindFlag), unless it was the last
    // one bound.
    void bind(u_long **cursor, Tim2Texture *texture, u_int flags);       // FS2 0x002e0850

private:
    // Best fit on the free list; 0 when nothing fits.
    GsRegion *findFree(u_int blocks);                                    // FS2 0x002e0a90
    // In use -> free, merged with free neighbours.
    void freeRegion(GsRegion *region);                                   // FS2 0x002e0b80
    // A region of exactly `blocks`, evicting until one fits.
    GsRegion *alloc(u_int blocks);                                       // FS2 0x002e10c0

    // The spare region records, linked through m_higher (inlined).
    GsRegion *takeRecord()
    {
        GsRegion *region = m_spareRegions;
        if (region != nullptr)
            m_spareRegions = region->m_higher;
        return region;
    }
    void putRecord(GsRegion *region)
    {
        if (region != nullptr) {
            region->m_higher = m_spareRegions;
            m_spareRegions = region;
        }
    }

    int                              m_base;          // FS2 +0x000: the first GS block
    int                              m_size;          // FS2 +0x004: in GS blocks
    GsPoolList<GsRegion *, kRegions> m_used;          // FS2 +0x008: oldest first
    GsPoolList<GsRegion *, kRegions> m_free;          // FS2 +0x618
    GsRegion                        *m_root;          // FS2 +0xc28: the whole range, as made
    Tim2Texture                     *m_bound;         // FS2 +0xc2c: the last texture bind sent
    GsRegion                         m_regions[kRegions]; // FS2 +0xc30
    GsRegion                        *m_spareRegions;  // FS2 +0x1830
};

#endif /* _RENA_RENA_2D_TEXTURE_VRAM_H */
