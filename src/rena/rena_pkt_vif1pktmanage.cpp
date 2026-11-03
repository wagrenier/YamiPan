// rena/rena_pkt_vif1pktmanage.cpp -- rena's VIF1 packet manager: frame buffers, packet lists, the frame's DMA chain (rena/rena_pkt_vif1pktmanage.h).

#include "rena/rena_pkt_vif1pktmanage.h"

#include <stdint.h>

#include "rx/rxsys.h"
#include "sdk/eekernel.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"
#include "yamipan/rendering/yamipan_drawui.h"

// The DMA tag ids the chain uses (tag bits 28..30; QWC 0, so a tag only
// jumps to its ADDR or stops).
enum DmaTagId
{
    DmaTag_Next = 2,    // go on at ADDR: the buffer's first tag, every segment's last
    DmaTag_End  = 7,    // the chain's end
};

// Writes a tag {QWC 0, id, ADDR 0} and two VIF NOPs at `at`.
static void putTag(u_long128 *at, DmaTagId id)
{
    u_int *w = (u_int *)at;
    w[0] = (u_int)id << 28;
    w[1] = 0;
    w[2] = 0;
    w[3] = 0;
}

// Points the tag at `tag` (its ADDR word) at `to`.
// PC: ADDR is the byte offset of `to` in the frame buffer `base`, where the
// EE stores the address.
static void linkTag(u_long128 *tag, const u_long128 *to, const u_long128 *base)
{
    ((u_int *)tag)[1] = (u_int)((const u_char *)to - (const u_char *)base);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002eaff0

u_int Vif1PktManage::GetUsedSize()
{
    return (u_int)((u_char *)cursor() - (u_char *)m_base);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002eb010

u_int Vif1PktManage::GetBufferSize()
{
    return m_buffers.data()[m_buffer].size;
}

// ──────────────────────────────────────────────────────────────────────
// GetUsedSize against GetBufferSize, both inline.
// FS2 0x002eb030

bool Vif1PktManage::IsOverflow()
{
    return m_buffers.data()[m_buffer].size < (u_int)((u_char *)cursor() - (u_char *)m_base);
}

// ──────────────────────────────────────────────────────────────────────
// Closes the open list, then chains from the buffer's first tag through
// the segments of every list that holds any, in list order (each list's
// segments are already linked by Open), to an end tag at the cursor; the
// chain is what Kick sends.
// FS2 0x002eb070
// callcheck: sceVif1PkNext, sceVif1PkCloseDmaTag (Close, inline in the ROM)

void Vif1PktManage::Terminate()
{
    Close();
    m_chain = m_base;

    u_long128 *last = m_base;
    for (Vif1PktList **it = m_lists.data(), **end = it + m_lists.size(); it != end; it++)
    {
        if ((*it)->m_head)
        {
            linkTag(last, (*it)->m_head, m_base);
            last = (*it)->m_tail;
        }
    }
    linkTag(last, m_cur, m_base);
    putTag(m_cur, DmaTag_End);
    m_cur++;
    FlushCache(WRITEBACK_DCACHE);
}

// ──────────────────────────────────────────────────────────────────────
// Takes the next buffer in the ring (the first from index -1), empties
// every list and puts the chain's first tag at the buffer's start.
// FS2 0x002eb170

void Vif1PktManage::Reset()
{
    if (m_buffers.empty())
    {
        rxSysPrintf("rena_pkt_vif1pktmanage.cpp: (Reset) \x83\x70\x83\x50\x83\x62\x83\x67\x83\x6f\x83\x62\x83\x74\x83\x40"
                    "\x82\xaa\x93\x6f\x98\x5e\x82\xb3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb9\x82\xf1\x81\x42\n");   // "no packet buffer is registered."
        return;
    }

    m_buffer++;
    if (m_buffer < 0 || (u_int)m_buffer >= m_buffers.size())
        m_buffer = 0;
    m_open = 0;
    m_base = m_buffers.data()[m_buffer].base;
    for (Vif1PktList **it = m_lists.data(), **end = it + m_lists.size(); it != end; it++)
    {
        Vif1PktList *list = *it;
        sceVif1PkReset(list);
        list->m_head = 0;
        list->m_tail = 0;
    }
    putTag(m_base, DmaTag_Next);
    m_cur = m_base + 1;
}

// ──────────────────────────────────────────────────────────────────────
// Sends the terminated chain: DMAC on, the VIF1 channel status cleared
// with interrupts off, then QWC 0, TADR the chain, CHCR 0x1c5 (from
// memory, chain mode, tag transfer, start). Nothing before the first
// Terminate.
// PC: no DMAC. The host draw module gets the lists the chain runs
// through, in its order.
// FS2 0x002eb2a0
// callcheck: DIntr, EIntr (PC: no DMAC; the chain goes to the host draw module)

void Vif1PktManage::Kick()
{
    if (!m_chain)
        return;

    std::vector<const void *> drawn;
    for (Vif1PktList *list : m_lists)
    {
        if (list->m_head)
            drawn.push_back(static_cast<const sceVif1Packet *>(list));
    }
    YamiPan_DrawUiKick(drawn.data(), (int)drawn.size());
}

// ──────────────────────────────────────────────────────────────────────
// Ends the open list's segment with a "next" tag (ADDR 0 until Open or
// Terminate links it), which becomes the list's tail; the write cursor
// continues after it.
// FS2 0x002eb320

void Vif1PktManage::Close()
{
    if (!m_open)
        return;
    m_open->m_tail = m_open->pCurrent;
    sceVif1PkNext(m_open, 0, 0);
    sceVif1PkCloseDmaTag(m_open);
    m_cur = m_open->pCurrent;
    m_open = 0;
}

// ──────────────────────────────────────────────────────────────────────
// push_back of {base, size}; the index it gets.
// FS2 0x002eb380
// callcheck: insert (push_back: the ROM calls Vif1PktBufferVector::insert at end with a count of 1)

int Vif1PktManage::AddBuffer(void *base, u_int size)
{
    int index = (int)m_buffers.size();
    Vif1PktBuffer buffer = { (u_long128 *)base, size };
    m_buffers.push_back(buffer);
    return index;
}

// ──────────────────────────────────────────────────────────────────────
// Unless list `id` is the open one: closes the open list, opens `id` at
// the write cursor and links the new segment after the list's last (or
// makes it the head). An id never added leaves no list open, and the ROM
// then writes through the null pointer.
// FS2 0x002eb3d0
// callcheck: sceVif1PkNext, sceVif1PkCloseDmaTag (Close, inline in the ROM)

sceVif1Packet *Vif1PktManage::Open(int id)
{
    if (!m_open || m_open->m_id != id)
    {
        Close();
        for (Vif1PktList *list : m_lists)
        {
            if (list->m_id == id)
            {
                m_open = list;
                break;
            }
        }

        m_open->pCurrent = m_cur;
        if (!m_open->m_head)
            m_open->m_head = m_cur;
        else
            linkTag(m_open->m_tail, m_cur, m_base);
    }
    return m_open;
}

// ──────────────────────────────────────────────────────────────────────
// A new, empty list `id` at the end, unless one has that id.
// FS2 0x002eb500
// callcheck: operator_new, insert (new Vif1PktList; push_back: Vif1PktListVector::insert at end with a count of 1)

int Vif1PktManage::AddList(int id)
{
    Vif1PktList *found = 0;
    for (Vif1PktList *list : m_lists)
    {
        if (list->m_id == id)
        {
            found = list;
            break;
        }
    }

    if (found)
    {
        // ROM: the %d gets the found list's address (a1 left over from the
        // search); PC: the id.
        rxSysPrintf("rena_pkt_vif1pktmanage.cpp:\x82\xb1\x82\xccID(%d)\x82\xcd\x82\xb7\x82\xc5\x82\xc9\x93\x6f\x98\x5e"
                    "\x82\xb3\x82\xea\x82\xc4\x82\xa2\x82\xdc\x82\xb7\x81\x42 \n", id);   // "this ID (%d) is already registered."
        return -1;
    }

    Vif1PktList *list = new Vif1PktList;
    list->m_id = id;
    m_lists.push_back(list);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Deletes every list (through its virtual destructor), then the vectors
// free their buffers into the global heap.
// FS2 0x002eb600
// callcheck: ~Vif1PktBufferVector, free, operator_delete (the members' destructors; the deleting destructor's flag)

Vif1PktManage::~Vif1PktManage()
{
    for (Vif1PktList *list : m_lists)
    {
        if (list)
            delete list;
    }
}

// ──────────────────────────────────────────────────────────────────────
// EE slot 0 (+0x8) of vtable 0x31a818.
// FS2 0x002eb730
// callcheck: operator_delete (the deleting destructor's flag)

Vif1PktList::~Vif1PktList()
{
}

// ──────────────────────────────────────────────────────────────────────
// Empty, no buffer yet (index -1: the first Reset takes buffer 0).
// FS2 0x002eb780

Vif1PktManage::Vif1PktManage()
    : m_cur(0), m_base(0), m_chain(0), m_open(0), m_buffer(-1)
{
}
