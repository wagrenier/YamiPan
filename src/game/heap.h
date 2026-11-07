/* ==========================================================================
 *  game/heap.h
 *
 *  The game's heap allocator and its one instance, the global heap
 *  0x329970 (initGlobalHeap takes almost all of the 32 MB from newlib's
 *  malloc at boot), and the game RNG: a 97-word shuffle table over an LCG.
 *  One object file, 0x1037c0..0x104b40 (game/heap.cpp).
 * ======================================================================== */
#ifndef _GAME_HEAP_H
#define _GAME_HEAP_H

#include <stddef.h>

#include "sdk/scetypes.h"

// Usage marks GameSystem::flipFrame reports; mark 2 is reset every frame,
// mark 1 at each stage start (GameSystem::beginStage). Mark 0's meaning is
// a guess until its setter is read.
enum HeapMark
{
    HeapMark_Base  = 0,         // guess
    HeapMark_Stage = 1,         // GameSystem::beginStage sets it
    HeapMark_Frame = 2,         // GameSystem::flipFrame sets it each frame
    HeapMark_Count = 4,
    HeapMark_All   = -1,        // setMark: every mark
};

// HeapBlock::sizeFlags: the user size in the low 30 bits; bit 31 says a
// HeapGap record sits in the free space after the block. Bit 30 is kept
// through every size change but nothing here sets it.
constexpr u_int kHeapSizeMask = 0x3fffffff;
constexpr u_int kHeapFlagMask = 0xc0000000;
constexpr u_int HeapBlockFlag_GapRecorded = 0x80000000;

// The header in front of every allocation, directly below the user pointer.
// Blocks form a list in address order.
// layout: host -- pointers; EE 0x10: data +0x0, sizeFlags +0x4, prev +0x8,
// next +0xc. The user pointer is at least sizeof(HeapBlock) past the start
// of the free space (Heap::headerRoom).
struct HeapBlock
{
    u_char    *data;            // FS2 +0x0: the user pointer (this + sizeof(HeapBlock))
    u_int      sizeFlags;       // FS2 +0x4: user size | HeapBlockFlag_*
    HeapBlock *prev;            // FS2 +0x8: lower address
    HeapBlock *next;            // FS2 +0xc: higher address
};
static_assert(offsetof(HeapBlock, data) == 0, "HeapBlock::data");

// The free space after a block, when it is at least Heap::m_minGap, is
// recorded by a record written into that space (at the block's data end
// rounded up to 16), so the default allocation mode finds holes without
// walking every block.
constexpr u_int kHeapGapMagic = 0x7381a9bc;

// layout: host -- pointers; EE 0x10: magic +0x0, block +0x4, prev +0x8,
// next +0xc.
struct HeapGap
{
    u_int      magic;           // FS2 +0x0: kHeapGapMagic
    HeapBlock *block;           // FS2 +0x4: the block the space follows
    HeapGap   *prev;            // FS2 +0x8
    HeapGap   *next;            // FS2 +0xc
};
static_assert(offsetof(HeapGap, magic) == 0, "HeapGap::magic");

// One heap over a block of memory, guarded by a kernel semaphore (none when
// the id is negative). EE 0x44.
// layout: host -- pointers; the EE offsets are in the comments.
class Heap
{
public:
    void init(void *base, int size);                                     // FS2 0x00104500
    void setMark(int mark);                                              // FS2 0x001037c0
    int  markedUsage(int mark);                                          // FS2 0x00103870
    void *realloc(void *ptr, u_int size);                                // FS2 0x001038a0
    void free(void *ptr);                                                // FS2 0x00103aa0
    void *alloc(u_int size, u_int align);                                // FS2 0x00103c50

    int        m_sema;          // FS2 +0x00: WaitSema / SignalSema around every call when >= 0
    u_char    *m_base;          // FS2 +0x04
    u_char    *m_end;           // FS2 +0x08: m_base + m_size
    int        m_size;          // FS2 +0x0c
    HeapBlock *m_head;          // FS2 +0x10: lowest block
    HeapBlock *m_tail;          // FS2 +0x14: highest block
    u_int      m_count;         // FS2 +0x18: blocks in use
    u_char    *m_marks[HeapMark_Count]; // FS2 +0x1c: the highest data end since setMark
    u_int      m_peakCount;     // FS2 +0x2c
    int        _30;             // FS2 +0x30: cleared by init, read by 0x2945cc.. (unread)
    // FS2 +0x34: the allocation mode. Clear: reuse a recorded gap first,
    // then the space before the head, then search from the tail down. Set:
    // after the tail first, then a recorded gap. Callers flip it around one
    // alloc (0x298e1c, 0x299190, ...).
    u_char     m_tailFirst;
    HeapGap   *m_gapHead;       // FS2 +0x38
    HeapGap   *m_gapTail;       // FS2 +0x3c
    u_int      m_minGap;        // FS2 +0x40: the smallest space linkGap records (0x40)

private:
    void *allocBlock(u_int size, u_int align);                           // FS2 0x00103cd0
    void freeBlock(void *ptr);                                           // FS2 0x00103b10
    int  unlinkGap(HeapBlock *block);                                    // FS2 0x00104150
    int  linkGap(HeapBlock *block);                                      // FS2 0x00104230
    HeapBlock *insertBlock(HeapBlock *prev, u_int align);                // FS2 0x001043a0

    // Inlined in the ROM.
    static u_int headerRoom(u_int align);
    static HeapBlock *blockOf(void *ptr);
    u_char *dataEnd(HeapBlock *block);
    u_char *spaceLimit(HeapBlock *block);
    int spaceAfter(HeapBlock *block, u_int align);
    void lock();
    void unlock();
};

extern Heap gGlobalHeap;        /* bss 0x329970 */

void initGlobalHeap(int size);                                           // FS2 0x00104600
void shutdownGlobalHeap();                                               // FS2 0x001045a0

// Fills the table from the LCG, then throws away `skip` draws (gameInit
// passes a sum of the clock's fields).
void rndInit(int skip);                                                  // FS2 0x00104860
// The next draw of the RNG, 31 bits.
u_int randNext();                                                        // FS2 0x001046b0
// The same draw as randNext, a second copy in the ROM.
u_int randNext2();                                                       // FS2 0x001047f0
// A float in [0, 1).
float randUnit();                                                        // FS2 0x00104720

#endif /* _GAME_HEAP_H */
