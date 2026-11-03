/* ==========================================================================
 *  game/gamepacket.h
 *
 *  GamePacketManager (game/gamepacket.cpp) on rena's Vif1PktManage: two
 *  0xc0000-byte packet buffers flipped each frame, 14 packet lists that
 *  Terminate links into one VIF1 DMA chain in id order. The list id is
 *  therefore the draw layer order; the port's host draw module keys its
 *  layers on it. GamePacketManager adds the open list's id (so opening a
 *  list closes the one still open) and a flag that drops one frame's kick.
 * ======================================================================== */
#ifndef _GAME_GAMEPACKET_H
#define _GAME_GAMEPACKET_H

#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

// The packet lists (= draw layers, drawn in id order). Only the ones read
// so far.
enum PacketList
{
    PacketList_None = -1,       // GamePacketManager: no list open
    PacketList_StageClock = 10, // the stage loop's StageResult::update
    PacketList_Overlay = 11,    // overlayThread: the BG movie screen and the loading indicator
    PacketList_Screen  = 12,    // runLayout: modal screens and dialogs
    PacketList_Finish = 13,     // GameSystem::flipFrame puts a GS FINISH here
    PacketList_Count  = 14,
};

// ──────────────────────────────────────────────────────────────────────
// rena's packet manager (rena_pkt_vif1pktmanage.cpp, 0x2eaff0..; not
// written yet). layout: host -- EE 0x30, vtable 0x31a808 at +0x2c after
// the data: {+0 write ptr, +4 buffer base, +8 chain start, +0xc open list,
// +0x10 buffer index, +0x14 list vector (count +0x18, data +0x1c), +0x20
// buffer vector {base, size} (count +0x24, data +0x28)}.
class Vif1PktManage
{
public:
    Vif1PktManage();                                                     // FS2 0x002eb780
    virtual ~Vif1PktManage();                                            // FS2 0x002eb600: deleting (obj, flag)

    // Adds a {base, size} frame buffer (GamePacketManager adds two).
    void AddBuffer(void *base, u_int size);                              // FS2 0x002eb380
    // Adds packet list `id` (a 0x34-byte Vif1PktList).
    void AddList(int id);                                                // FS2 0x002eb500

    // Opens list `id` and returns its packet, the write cursor first.
    sceVif1Packet *Open(int id);                                         // FS2 0x002eb3d0
    // Closes the open list (its DMA tag).
    void Close();                                                        // FS2 0x002eb320
    // True when the current buffer's used part is larger than the buffer.
    bool IsOverflow();                                                   // FS2 0x002eb030
    // Links the non-empty lists in id order into one chain with an end tag.
    void Terminate();                                                    // FS2 0x002eb070
    // Sends the chain on VIF1 (CHCR 0x1c5). PC: hands it to the host draw module.
    void Kick();                                                         // FS2 0x002eb2a0
    // Flips to the other buffer and empties the lists.
    void Reset();                                                        // FS2 0x002eb170

    u_int GetUsedSize();                                                 // FS2 0x002eaff0
    u_int GetBufferSize();                                               // FS2 0x002eb010

    u_char _00[0x2c];
};

// ──────────────────────────────────────────────────────────────────────
// layout: host -- EE 0x3c, vtable 0x311040 (1 slot: the destructor) at
// +0x2c, Vif1PktManage's vptr slot; GameSystem +0x46c.
class GamePacketManager : public Vif1PktManage
{
public:
    GamePacketManager(void *buf0, u_int size0, void *buf1, u_int size1); // FS2 0x0012dee0
    virtual ~GamePacketManager();                                        // FS2 0x0012de80: deleting (obj, flag)

    sceVif1Packet *openList(int id);                                     // FS2 0x0012de00
    void closeList();                                                    // FS2 0x0012dd30
    void endFrame();                                                     // FS2 0x0012ddc0
    void kickAndSwap();                                                  // FS2 0x0012dd70

    int    m_openList;          // FS2 +0x30: PacketList_None or the open list's id
    u_char m_skipKick;          // FS2 +0x34: drop the next kick (no writer found yet: kickAndSwap clears it)
    u_char _35[0x3c - 0x35];
};

#endif /* _GAME_GAMEPACKET_H */
