// game/gsblockmap.cpp -- the GS block map and the texture area (game/gsblockmap.h).

#include "game/gsblockmap.h"

#include <string.h>

#include "game/gsutil.h"
#include "game/heap.h"

// Heap::alloc's alignment for the list tables.
static const u_int kTexAreaAlign = 0x10;

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00142dd0

TexAreaList *TexArea::getList(int list)
{
    return m_lists[list];
}

// FS2 0x00142df0

void TexArea::resetLists()
{
    for (u_int i = 0; i < m_listCount; i++)
        m_lists[i]->reset();
}

// The lists may use the pages up to the first one with a block in use
// (the area's end when none); slots reaching past a lowered end are
// emptied. Inlined in the three slots.

void TexArea::clipLists()
{
    u_int last = m_last & ~(kGsBlocksPerPage - 1);
    u_int page = last;
    for (u_int block = (m_first + kGsBlocksPerPage - 1) & ~(kGsBlocksPerPage - 1);
         block < last; block += kGsBlocksPerPage) {
        if (m_bits[block / kGsBlocksPerPage] != 0) {
            page = block;
            break;
        }
    }

    for (u_int i = 0; i < m_listCount; i++) {
        TexAreaList *list = m_lists[i];
        int oldEnd = list->m_endPage;
        list->m_endPage = page / kGsBlocksPerPage;
        if (list->m_endPage < oldEnd) {
            for (u_int s = 0; s < list->m_slotCount; s++) {
                if (list->m_endPage < list->m_slots[s].endPage)
                    list->m_slots[s].clear();
            }
        }
    }
}

// FS2 0x00142e70
// callcheck: setRange (GsBlockMap::mark inlined)

void TexArea::mark(u_int lo, u_int hi, int used)
{
    setRange(lo, hi, used);
    clipLists();
}

// FS2 0x00142fa0
// callcheck: mark (GsBlockMap::free inlined: mark through the vtable)

void TexArea::free(int block, u_int count)
{
    GsBlockMap::free(block, count);
    clipLists();
}

// FS2 0x001430f0
// callcheck: findFree, mark (GsBlockMap::alloc inlined with 1, 0: mark through the vtable)

int TexArea::alloc(u_int count, int fromTop, int pageAligned)
{
    (void)fromTop;
    (void)pageAligned;
    int block = GsBlockMap::alloc(count, 1, 0);
    clipLists();
    return block;
}

// FS2 0x00143270
// callcheck: alloc, operator_new, alloc, mark (the GsBlockMap ctor and the list ctor inlined; mark through the base vtable)

TexArea::TexArea(u_int firstBlock, u_int lastBlock, u_int lists, u_int slotsPerList)
    : GsBlockMap(firstBlock, lastBlock)
{
    m_listCount = lists;
    m_lists = (TexAreaList **)gGlobalHeap.alloc(lists * sizeof(TexAreaList *), kTexAreaAlign);
    for (u_int i = 0; i < lists; i++)
        m_lists[i] = new TexAreaList(firstBlock, lastBlock, slotsPerList);
}

TexAreaList::TexAreaList(u_int firstBlock, u_int lastBlock, u_int slotCount)
{
    m_firstPage = (firstBlock + kGsBlocksPerPage - 1) / kGsBlocksPerPage;
    m_endPage = lastBlock / kGsBlocksPerPage;
    m_nextPage = m_firstPage;
    m_slotCount = slotCount;
    m_slots = (TexAreaSlot *)gGlobalHeap.alloc(slotCount * sizeof(TexAreaSlot), kTexAreaAlign);
    reset();
}

void TexAreaList::reset()
{
    for (u_int i = 0; i < m_slotCount; i++)
        m_slots[i].clear();
}

// FS2 0x00143450

void TexAreaList::insert(const void *key, int firstPage, int pageCount)
{
    TexAreaSlot *target = nullptr;
    for (u_int i = 0; i < m_slotCount; i++) {
        TexAreaSlot *slot = &m_slots[i];
        if (slot->key == nullptr) {
            if (target == nullptr)
                target = slot;
        } else if (slot->firstPage < firstPage + pageCount && firstPage < slot->endPage) {
            slot->clear();
            if (target == nullptr)
                target = slot;
        }
    }
    if (target == nullptr)
        target = m_slots;
    target->key = key;
    target->firstPage = firstPage;
    target->endPage = firstPage + pageCount;
}

// The pages go round robin from nextPage, back to the first page when the
// rest of the list is too small.
// FS2 0x00143510

int TexAreaList::place(int *tbp, Tim2Texture *tex)
{
    const void *key = tex->m_image;
    int page = -1;
    for (u_int i = 0; i < m_slotCount; i++) {
        if (m_slots[i].key == key) {
            page = m_slots[i].firstPage;
            break;
        }
    }
    if (page >= 0) {
        *tbp = page * kGsBlocksPerPage;
        return 0;
    }

    int pages = (int)(((u_int)tex->getVramBlocks() + kGsBlocksPerPage - 1) / kGsBlocksPerPage);
    page = m_nextPage;
    if (m_endPage - page < pages) {
        page = m_firstPage;
        if (m_endPage - page < pages) {
            // Does not fit: the ROM's page -1 as a block.
            *tbp = kGsNoBlock * (int)kGsBlocksPerPage;
            return 1;
        }
    }
    m_nextPage = page + pages;
    insert(key, page, pages);
    *tbp = page * kGsBlocksPerPage;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// The first free bit run of mask's width in word (its lowest position),
// -1 when none. The ROM stops only when the mask's top bit is set and
// still collides, so a run at the word's top may come out shorter than
// asked (bits shifted out): kept.

static int findBitsInWord(u_int word, u_int mask)
{
    if (word == 0xffffffff || mask == 0)
        return -1;
    int pos = 0;
    for (; (word & mask) != 0; mask <<= 1) {
        if ((mask & 0x80000000) != 0)
            return -1;
        pos++;
    }
    return pos;
}

// FS2 0x00143640

int GsBlockMap::findFree(u_int count, int fromTop, int pageAligned)
{
    if (count == 0)
        return kGsNoBlock;
    if ((u_char)pageAligned == 0 && count > 3)
        pageAligned = 1;

    if (pageAligned == 0) {
        // Inside one word, every word touching [first, last).
        u_int mask = (1u << count) - 1;
        int lo = (int)(m_first / kGsBlocksPerPage);
        int hi = (int)((m_last - 1) / kGsBlocksPerPage);
        if (fromTop == 0) {
            for (int w = lo; w <= hi; w++) {
                int bit = findBitsInWord(m_bits[w], mask);
                if (bit != -1)
                    return w * (int)kGsBlocksPerPage + bit;
            }
        } else {
            for (int w = hi; w >= lo; w--) {
                int bit = findBitsInWord(m_bits[w], mask);
                if (bit != -1)
                    return w * (int)kGsBlocksPerPage + bit;
            }
        }
        return kGsNoBlock;
    }

    // Whole free words, those inside [first, last).
    int lo = (int)((m_first + kGsBlocksPerPage - 1) / kGsBlocksPerPage);
    int hi = (int)(m_last / kGsBlocksPerPage);
    u_int run = 0;
    if (fromTop == 0) {
        int start = lo;
        for (int w = lo; w < hi; w++) {
            if (m_bits[w] == 0) {
                run += kGsBlocksPerPage;
                if (count <= run)
                    return start * (int)kGsBlocksPerPage;
            } else {
                start = w + 1;
                run = 0;
            }
        }
        return kGsNoBlock;
    }
    for (int w = hi - 1; w >= lo; w--) {
        if (m_bits[w] != 0) {
            run = 0;
            continue;
        }
        run += kGsBlocksPerPage;
        if (run >= count)
            return w * (int)kGsBlocksPerPage;
    }
    return kGsNoBlock;
}

// Short ranges bit by bit; from 0x200 blocks on, a head, whole words and a
// tail. The ROM counts the head's and the tail's bits as (block & 0x7ff)
// >> 6, not block & 31: right only where the two agree, as for every range
// the game marks (0x800-aligned ones, 0x2800..0x3700 whose tail only sets
// bits already set, 0x3700..0x3fbe). Kept.
// FS2 0x001438c0

void GsBlockMap::setRange(u_int lo, u_int hi, int used)
{
    if (lo >= hi)
        return;

    if (hi - lo < 0x200) {
        if (used == 0) {
            for (; lo < hi; lo++)
                m_bits[lo / kGsBlocksPerPage] &= ~(1u << (lo & 31));
        } else {
            for (; lo < hi; lo++)
                m_bits[lo / kGsBlocksPerPage] |= 1u << (lo & 31);
        }
        return;
    }

    u_int head = (lo & 0x7ff) >> 6;
    if (head != 0) {
        u_int n = kGsBlocksPerPage - head;
        u_int bits = ((1u << n) - 1) << (lo & 31);
        if (used == 0)
            m_bits[lo / kGsBlocksPerPage] &= ~bits;
        else
            m_bits[lo / kGsBlocksPerPage] |= bits;
        lo += n;
    }

    u_int fill = used == 0 ? 0 : 0xffffffff;
    for (u_int w = lo / kGsBlocksPerPage; w < hi / kGsBlocksPerPage; w++)
        m_bits[w] = fill;

    u_int tail = (hi & 0x7ff) >> 6;
    if (tail != 0) {
        u_int bits = ((1u << tail) - 1) << ((hi - tail) & 31);
        if (used == 0)
            m_bits[(hi - tail) / kGsBlocksPerPage] &= ~bits;
        else
            m_bits[(hi - tail) / kGsBlocksPerPage] |= bits;
    }
}

// FS2 0x00143f00
// callcheck: operator_delete (the deleting dtor's flag)

GsBlockMap::~GsBlockMap()
{
}

// FS2 0x00143f50
// callcheck: mark (through the vtable)

GsBlockMap::GsBlockMap(u_int first, u_int last)
{
    m_first = first;
    m_last = last;
    memset(m_bits, 0xff, sizeof(m_bits));
    mark(first, last, 0);
}

// FS2 0x00143fd0
// callcheck: findFree, mark (through the vtable)

int GsBlockMap::alloc(u_int count, int fromTop, int pageAligned)
{
    int block = findFree(count, fromTop, pageAligned);
    if (block == kGsNoBlock)
        return kGsNoBlock;
    mark(block, block + count, 1);
    return block;
}

// FS2 0x00144040
// callcheck: mark (through the vtable)

void GsBlockMap::free(int block, u_int count)
{
    if (block != kGsNoBlock)
        mark(block, block + count, 0);
}
