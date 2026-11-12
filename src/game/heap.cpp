// game/heap.cpp -- the game's heap allocator, the global heap and the game
// RNG (game/heap.h).

#include "game/heap.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sdk/eekernel.h"
#include "sdk/scetypes.h"

Heap gGlobalHeap;               /* bss 0x329970 */

// The EE memory layout words initGlobalHeap sizes the global heap from (sdata
// 0x31a880..0x31a888, read nowhere else): the image end, the main thread's
// stack size, and a fixed heap size (-1: everything up to 32 MB). The port
// keeps the EE's numbers, so the global heap is as big as on the console.
static u_int sImageEnd = 0x5a5600;
static u_int sStackSize = 0x10000;
static int sHeapSize = -1;

constexpr u_int kEeMainMemory = 0x2000000;
constexpr u_int kHeapReserve = 0x1000;  // left to newlib below the stack, and inside a fixed size
constexpr u_int kMallocOverhead = 0x20; // newlib's chunk header and slack
constexpr u_int kReallocAlign = 0x10;
constexpr u_int kGapAlign = 0x10;       // HeapGap records sit on 16-byte boundaries
constexpr u_int kDefaultMinGap = 0x40;

// ──────────────────────────────────────────────────────────────────────
// Inlined helpers. The ROM computes these with signed division on 32-bit
// addresses; the host works on uintptr_t.

static inline uintptr_t alignUp(uintptr_t v, u_int align)
{
    return align * ((v + align - 1) / align);
}

// The space reserved in front of an allocation's data: one header, rounded up
// to the alignment. PC: the EE header is 0x10 bytes, the host's 0x20.
inline u_int Heap::headerRoom(u_int align)
{
    return align * ((align + sizeof(HeapBlock) - 1) / align);
}

inline HeapBlock *Heap::blockOf(void *ptr)
{
    if (ptr == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<HeapBlock *>(static_cast<u_char *>(ptr) - sizeof(HeapBlock));
}

// Where the space after a block starts: its data end (the heap base for
// "before the head").
inline u_char *Heap::dataEnd(HeapBlock *block)
{
    if (block == nullptr) {
        return m_base;
    }
    return block->data + (block->sizeFlags & kHeapSizeMask);
}

// Where the space after a block ends: the next block's header, or the heap
// end.
inline u_char *Heap::spaceLimit(HeapBlock *block)
{
    HeapBlock *next = block == nullptr ? m_head : block->next;
    if (next == nullptr) {
        return m_end;
    }
    return reinterpret_cast<u_char *>(next);
}

// The free bytes after a block once its end is aligned, 0 when none.
inline int Heap::spaceAfter(HeapBlock *block, u_int align)
{
    uintptr_t start = alignUp(reinterpret_cast<uintptr_t>(dataEnd(block)), align);
    int space = (int)(reinterpret_cast<uintptr_t>(spaceLimit(block)) - start);
    if (space < 0) {
        space = 0;
    }
    return space;
}

inline void Heap::lock()
{
    if (m_sema >= 0) {
        WaitSema(m_sema);
    }
}

inline void Heap::unlock()
{
    if (m_sema >= 0) {
        SignalSema(m_sema);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Sets a usage mark (HeapMark_All: all four) to the tail block's data end,
// the heap base when the heap is empty.
// FS2 0x001037c0

void Heap::setMark(int mark)
{
    if (mark == HeapMark_All) {
        for (int i = 0; i < HeapMark_Count; i++) {
            m_marks[i] = dataEnd(m_tail);
        }
    } else {
        m_marks[mark] = dataEnd(m_tail);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The bytes from the heap base to the highest data end since the mark was
// set.
// FS2 0x00103870

int Heap::markedUsage(int mark)
{
    int used = (int)(m_marks[mark] - m_base);
    if (used < 0) {
        used = 0;
    }
    return used;
}

// ──────────────────────────────────────────────────────────────────────
// Resizes an allocation: in place when it shrinks or the space up to the
// next block holds the new size, else a new 16-aligned block, the old data
// copied and the old block freed. Null for a null pointer or when the heap is
// full (the old block is kept then).
// FS2 0x001038a0
// callcheck: WaitSema, SignalSema (inside the inlined Heap::lock / unlock)

void *Heap::realloc(void *ptr, u_int size)
{
    lock();
    HeapBlock *block = blockOf(ptr);
    if (block == nullptr) {
        unlock();
        return nullptr;
    }

    if ((block->sizeFlags & kHeapSizeMask) < size) {
        u_int room = (u_int)(spaceLimit(block) - block->data);
        if (room < size) {
            void *moved = allocBlock(size, kReallocAlign);
            if (moved == nullptr) {
                unlock();
                return nullptr;
            }
            memcpy(moved, block->data, block->sizeFlags & kHeapSizeMask);
            freeBlock(ptr);
            unlock();
            return moved;
        }
    }

    unlinkGap(block);
    block->sizeFlags = (block->sizeFlags & kHeapFlagMask) | (size & kHeapSizeMask);
    linkGap(block);
    unlock();
    return ptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00103aa0
// callcheck: WaitSema, SignalSema (inside the inlined Heap::lock / unlock)

void Heap::free(void *ptr)
{
    lock();
    freeBlock(ptr);
    unlock();
}

// ──────────────────────────────────────────────────────────────────────
// Unlinks an allocation's block. The space it leaves joins the gap after the
// block below it: a new tail records no gap, any other block's gap record is
// made (or kept: it sits at the same place). An empty heap starts over at
// its base.
// FS2 0x00103b10

void Heap::freeBlock(void *ptr)
{
    if (ptr == nullptr) {
        return;
    }
    HeapBlock *block = blockOf(ptr);
    if (block == nullptr) {
        return;
    }

    unlinkGap(block);
    HeapBlock *gapOwner = nullptr;
    HeapBlock *newTail = nullptr;
    if (m_head == block) {
        m_head = block->next;
    }
    if (m_tail == block) {
        m_tail = block->prev;
    }
    if (block->next == nullptr) {
        newTail = block->prev;
    } else {
        block->next->prev = block->prev;
    }
    if (block->prev != nullptr) {
        block->prev->next = block->next;
        gapOwner = block->prev;
    }
    block->data = nullptr;
    block->sizeFlags &= kHeapFlagMask;
    block->prev = nullptr;
    block->next = nullptr;

    if (newTail != nullptr) {
        unlinkGap(newTail);
    } else if (gapOwner != nullptr) {
        linkGap(gapOwner);
    }

    m_count--;
    if (m_count == 0) {
        m_count = 0;
        m_head = nullptr;
        m_tail = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Locked allocation: size bytes at the given alignment, null when no space
// holds them.
// FS2 0x00103c50
// callcheck: WaitSema, SignalSema (inside the inlined Heap::lock / unlock)

void *Heap::alloc(u_int size, u_int align)
{
    lock();
    void *ptr = allocBlock(size, align);
    unlock();
    return ptr;
}

// ──────────────────────────────────────────────────────────────────────
// Finds the space for a header and size bytes (see m_tailFirst for the
// order), places the block, records the gap after it and raises every usage
// mark to its end. Null for size 0 or when nothing fits.
// FS2 0x00103cd0

void *Heap::allocBlock(u_int size, u_int align)
{
    if (size == 0) {
        return nullptr;
    }
    int need = (int)(headerRoom(align) + alignUp(size, align));
    HeapBlock *block = nullptr;

    if (m_head == nullptr) {
        if (need <= spaceAfter(nullptr, align)) {
            block = insertBlock(nullptr, align);
        }
    } else if (!m_tailFirst) {
        HeapBlock *after = nullptr;
        for (HeapGap *gap = m_gapHead; gap != nullptr; gap = gap->next) {
            if (need <= spaceAfter(gap->block, align)) {
                after = gap->block;
                break;
            }
        }
        if (after != nullptr) {
            block = insertBlock(after, align);
        } else if (need <= spaceAfter(nullptr, align)) {
            block = insertBlock(nullptr, align);
        } else {
            // From the tail down. The ROM tries the space before the head
            // once more at the end of the walk, which was just found too
            // small.
            for (HeapBlock *cand = m_tail; cand != nullptr; cand = cand->prev) {
                if (need <= spaceAfter(cand, align)) {
                    block = insertBlock(cand, align);
                    break;
                }
            }
        }
    } else {
        if (need <= spaceAfter(m_tail, align)) {
            block = insertBlock(m_tail, align);
        } else {
            HeapBlock *after = nullptr;
            for (HeapGap *gap = m_gapHead; gap != nullptr; gap = gap->next) {
                if (need <= spaceAfter(gap->block, align)) {
                    after = gap->block;
                    break;
                }
            }
            if (after != nullptr) {
                block = insertBlock(after, align);
            }
        }
    }

    if (block == nullptr) {
        return nullptr;
    }
    block->sizeFlags = (block->sizeFlags & kHeapFlagMask) | (size & kHeapSizeMask);
    linkGap(block);
    u_char *end = dataEnd(block);
    for (int i = 0; i < HeapMark_Count; i++) {
        if (m_marks[i] < end) {
            m_marks[i] = end;
        }
    }
    return block->data;
}

// ──────────────────────────────────────────────────────────────────────
// Drops the gap record after a block, if it has one; 1 when it did.
// FS2 0x00104150

int Heap::unlinkGap(HeapBlock *block)
{
    if (block == nullptr || !(block->sizeFlags & HeapBlockFlag_GapRecorded)) {
        return 0;
    }
    HeapGap *gap = reinterpret_cast<HeapGap *>(alignUp(reinterpret_cast<uintptr_t>(dataEnd(block)), kGapAlign));
    if (gap == nullptr || gap->magic != kHeapGapMagic) {
        return 0;
    }
    if (gap->prev == nullptr) {
        m_gapHead = gap->next;
    } else {
        gap->prev->next = gap->next;
    }
    if (gap->next == nullptr) {
        m_gapTail = gap->prev;
    } else {
        gap->next->prev = gap->prev;
    }
    block->sizeFlags &= ~HeapBlockFlag_GapRecorded;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Records the space after a block in the gap list when it is at least
// m_minGap; 1 when it did. Never for the tail (its space is the heap's end)
// or a block recorded already.
// FS2 0x00104230

int Heap::linkGap(HeapBlock *block)
{
    if (block->sizeFlags & HeapBlockFlag_GapRecorded) {
        return 0;
    }
    if (block->next == nullptr) {
        return 0;
    }
    if ((u_int)spaceAfter(block, kGapAlign) < m_minGap) {
        block->sizeFlags &= ~HeapBlockFlag_GapRecorded;
        return 0;
    }

    block->sizeFlags |= HeapBlockFlag_GapRecorded;
    HeapGap *gap = reinterpret_cast<HeapGap *>(alignUp(reinterpret_cast<uintptr_t>(dataEnd(block)), kGapAlign));
    gap->magic = kHeapGapMagic;
    gap->block = block;
    if (m_gapHead == nullptr) {
        gap->prev = nullptr;
        gap->next = nullptr;
        m_gapHead = gap;
        m_gapTail = gap;
    } else {
        gap->prev = m_gapTail;
        gap->next = nullptr;
        m_gapTail->next = gap;
        m_gapTail = gap;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Places a new block in the space after prev (before the head when null):
// the data aligned and a header room past the aligned start, the header right
// below it, linked in address order. The size is the caller's to set.
// FS2 0x001043a0

HeapBlock *Heap::insertBlock(HeapBlock *prev, u_int align)
{
    if (prev != nullptr) {
        unlinkGap(prev);
    }
    u_char *data = reinterpret_cast<u_char *>(
        alignUp(reinterpret_cast<uintptr_t>(dataEnd(prev)), align) + headerRoom(align));
    HeapBlock *block = reinterpret_cast<HeapBlock *>(data - sizeof(HeapBlock));
    block->data = data;
    block->prev = prev;
    if (prev != nullptr) {
        block->next = prev->next;
        prev->next = block;
        if (block->next == nullptr) {
            block->next = nullptr;
            m_tail = block;
        } else {
            block->next->prev = block;
        }
    } else {
        block->prev = nullptr;
        if (m_head == nullptr) {
            block->next = nullptr;
            m_tail = block;
        } else {
            m_head->prev = block;
            block->next = m_head;
        }
        m_head = block;
    }

    m_count++;
    if (m_peakCount < m_count) {
        m_peakCount = m_count;
    }
    if (block != nullptr) {
        block->sizeFlags &= ~HeapBlockFlag_GapRecorded;
    }
    return block;
}

// ──────────────────────────────────────────────────────────────────────
// An empty heap over [base, base + size) with its own semaphore (one count).
// FS2 0x00104500

void Heap::init(void *base, int size)
{
    // PC: the ROM leaves currentCount, numWaitThreads and attr as they
    // were on the stack.
    SemaParam param = {};
    param.maxCount = 1;
    param.initCount = 1;
    param.option = 0;
    m_sema = CreateSema(&param);
    m_base = static_cast<u_char *>(base);
    m_end = static_cast<u_char *>(base) + size;
    m_size = size;
    m_head = nullptr;
    m_tail = nullptr;
    m_count = 0;
    for (int i = 0; i < HeapMark_Count; i++) {
        m_marks[i] = nullptr;
    }
    m_peakCount = 0;
    _30 = 0;
    m_tailFirst = 0;
    m_gapHead = nullptr;
    m_gapTail = nullptr;
    m_minGap = kDefaultMinGap;
}

// ──────────────────────────────────────────────────────────────────────
// Deletes the global heap's semaphore and gives its block back to newlib.
// FS2 0x001045a0
// callcheck: _free_r (newlib's free on _impure_ptr: free on the host)

void shutdownGlobalHeap()
{
    u_char *base = gGlobalHeap.m_base;
    if (gGlobalHeap.m_sema >= 0) {
        DeleteSema(gGlobalHeap.m_sema);
        gGlobalHeap.m_sema = -1;
    }
    ::free(base);
}

// ──────────────────────────────────────────────────────────────────────
// Takes the global heap from newlib: size bytes, or (size <= 0) all the
// memory left minus -size. A first malloc / free of the whole amount grows
// newlib's arena before the real one.
// FS2 0x00104600
// callcheck: _malloc_r, _free_r (newlib's malloc / free on _impure_ptr: the host's)

void initGlobalHeap(int size)
{
    int avail;
    if (sHeapSize < 0) {
        avail = kEeMainMemory - (sImageEnd + sStackSize + 2 * kHeapReserve);
    } else {
        avail = sHeapSize - kHeapReserve;
    }
    ::free(malloc(avail - kMallocOverhead));
    if (size <= 0) {
        size += avail - kMallocOverhead;
    }
    void *base = malloc(size);
    gGlobalHeap.init(base, size);
}

// ──────────────────────────────────────────────────────────────────────
// The RNG: a table of 97 LCG outputs; each draw returns the entry the last
// draw pointed at and refills it with the next LCG output.

constexpr int kRandTableSize = 97;
constexpr u_int kRandMul = 0x5d588b65;

static u_int sRandSeed = 1;                     /* sdata 0x31a88c (gp-0x7fe4) */
static u_int sRandTable[kRandTableSize];        /* bss 0x3299c0 */
static u_int sRandIndex;                        /* sbss 0x31ad80 (gp-0x7af0) */

static inline u_int randLcg()
{
    sRandSeed = sRandSeed * kRandMul + 1;
    return sRandSeed;
}

static inline u_int randDraw()
{
    sRandIndex = sRandTable[sRandIndex] % kRandTableSize;
    u_int r = sRandTable[sRandIndex];
    sRandTable[sRandIndex] = randLcg();
    return r >> 1;
}

// FS2 0x001046b0

u_int randNext()
{
    return randDraw();
}

// ──────────────────────────────────────────────────────────────────────
// Draws again when the float division rounds up to 1.
// FS2 0x00104720

float randUnit()
{
    float f;
    do {
        f = (float)(int)randDraw() / 2147483648.0f;
    } while (f >= 1.0f);
    return f;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001047f0

u_int randNext2()
{
    return randDraw();
}

// ──────────────────────────────────────────────────────────────────────
// The ROM unrolls both loops by eight. The LCG state and the index carry
// over from before (the seed starts at 1).
// FS2 0x00104860

void rndInit(int skip)
{
    for (int i = 0; i < kRandTableSize; i++) {
        sRandTable[i] = randLcg();
    }
    for (int i = 0; i < skip; i++) {
        randDraw();
    }
}
