/* ==========================================================================
 *  rena/rena_pkt_vif1pktmanage.h
 *
 *  rena's VIF1 packet manager (rena/rena_pkt_vif1pktmanage.cpp,
 *  0x2eaff0..0x2ec110). It owns a ring of frame buffers and a set of
 *  packet lists, each an sceVif1Packet with an id. While a frame is drawn
 *  the game opens one list at a time (Open / Close); every Open continues
 *  that list in a new segment at the manager's write cursor, linked to the
 *  list's previous segment through the DMA "next" tag Close left at its
 *  end. Terminate then links the lists that hold anything, in the order
 *  they were added (= id order for GamePacketManager), into one VIF1 DMA
 *  chain that starts with the buffer's first tag and ends with an "end"
 *  tag; Kick sends it, Reset moves to the next buffer and empties the
 *  lists. So a list is a draw layer and the chain order the layer order.
 *
 *  PC: the chain is never sent to hardware. The tags keep the ROM's words,
 *  except that ADDR holds the target's byte offset in the frame buffer (a
 *  host pointer does not fit 32 bits), and Kick hands the drawn lists in
 *  chain order to the host draw module (yamipan_drawui.h).
 * ======================================================================== */
#ifndef _RENA_RENA_PKT_VIF1PKTMANAGE_H
#define _RENA_RENA_PKT_VIF1PKTMANAGE_H

#include <vector>

#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

// One packet list (operator new 0x34 on the EE, vtable 0x31a818).
// layout: host -- EE: sceVif1Packet +0, data to +0x30, then the vptr +0x30
// (CodeWarrior puts it after the data); the host vptr comes first, so the
// packet is a base, not offset 0.
class Vif1PktList : public sceVif1Packet
{
public:
    virtual ~Vif1PktList();                                              // FS2 0x002eb730: deleting (obj, flag)

    int        m_id;            // FS2 +0x20: Vif1PktManage::Open's key
    u_long128 *m_head;          // FS2 +0x24: the first segment, 0 while the list is empty
    u_long128 *m_tail;          // FS2 +0x28: the "next" tag Close put after the last segment
    u_int      _2c;             // FS2 +0x2c: not read here
};

// One frame buffer (8 bytes on the EE).
// layout: host -- EE {base +0, size +4}.
struct Vif1PktBuffer
{
    u_long128 *base;            // FS2 +0x00
    u_int      size;            // FS2 +0x04: bytes
};

// The manager (0x30 on the EE, vtable 0x31a808).
// layout: host -- EE: the data +0..+0x2c, then the vptr +0x2c. The two MSL
// vectors {capacity, size, data} are the host <vector>; their out-of-line
// instances 0x2eb7d0..0x2ec110 are its.
class Vif1PktManage
{
public:
    Vif1PktManage();                                                     // FS2 0x002eb780
    virtual ~Vif1PktManage();                                            // FS2 0x002eb600: deleting (obj, flag)

    // Adds a frame buffer of `size` bytes; returns its index.
    int AddBuffer(void *base, u_int size);                               // FS2 0x002eb380
    // Adds packet list `id`: 0, or -1 (and a message) when it exists.
    int AddList(int id);                                                 // FS2 0x002eb500

    // Opens list `id` (closing another open one) at the write cursor and
    // returns its packet.
    sceVif1Packet *Open(int id);                                         // FS2 0x002eb3d0
    // Ends the open list's segment with a "next" tag.
    void Close();                                                        // FS2 0x002eb320
    // True when more of the current buffer is used than it holds.
    bool IsOverflow();                                                   // FS2 0x002eb030
    // Links the lists holding anything, in list order, into one chain.
    void Terminate();                                                    // FS2 0x002eb070
    // Sends the chain on VIF1 (CHCR 0x1c5). PC: to the host draw module.
    void Kick();                                                         // FS2 0x002eb2a0
    // Moves to the next buffer and empties the lists.
    void Reset();                                                        // FS2 0x002eb170

    // Bytes written into the current buffer (to the open list's cursor).
    u_int GetUsedSize();                                                 // FS2 0x002eaff0
    // The current buffer's size.
    u_int GetBufferSize();                                               // FS2 0x002eb010

private:
    u_long128 *cursor() const { return m_open ? m_open->pCurrent : m_cur; }

    u_long128                   *m_cur;     // FS2 +0x00: the write cursor while no list is open
    u_long128                   *m_base;    // FS2 +0x04: the current buffer: the chain's first tag
    u_long128                   *m_chain;   // FS2 +0x08: the terminated chain (VIF1 TADR), 0 before the first
    Vif1PktList                 *m_open;    // FS2 +0x0c: the open list
    int                          m_buffer;  // FS2 +0x10: the current buffer's index, -1 before the first Reset
    std::vector<Vif1PktList *>   m_lists;   // FS2 +0x14: in AddList order (size +0x18, data +0x1c)
    std::vector<Vif1PktBuffer>   m_buffers; // FS2 +0x20: (size +0x24, data +0x28)
};

#endif /* _RENA_RENA_PKT_VIF1PKTMANAGE_H */
