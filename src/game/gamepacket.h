/* ==========================================================================
 *  game/gamepacket.h
 *
 *  GamePacketManager (game/gamepacket.cpp, not written yet) on rena's
 *  Vif1PktManage: two 0xc0000-byte packet buffers flipped each frame, 14
 *  packet lists that Terminate links into one VIF1 DMA chain in id order.
 *  The list id is therefore the draw layer order; the port's host draw
 *  module keys its layers on it. Minimal: only what game/gamesystem.cpp
 *  touches.
 * ======================================================================== */
#ifndef _GAME_GAMEPACKET_H
#define _GAME_GAMEPACKET_H

#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

// The packet lists (= draw layers, drawn in id order). Only the last one is
// known so far.
enum PacketList
{
    PacketList_Finish = 13,     // GameSystem::flipFrame puts a GS FINISH here
    PacketList_Count  = 14,
};

// layout: host -- rena's packet manager (rena_pkt_vif1pktmanage.cpp 0x310640):
// {+0 write ptr, +4 buffer base, +8 chain start, +0xc open list, +0x10 buffer
// index, +0x14 list vector, +0x20 buffer vector}, vptr at +0x2c, EE 0x30.
class Vif1PktManage
{
public:
    u_int GetUsedSize();                                                 // FS2 0x002eaff0
    u_int GetBufferSize();                                               // FS2 0x002eb010

    u_char _00[0x30];
};

// layout: host -- vtable 0x311040; EE 0x3c.
class GamePacketManager : public Vif1PktManage
{
public:
    GamePacketManager(void *buf0, u_int size0, void *buf1, u_int size1); // FS2 0x0012dee0
    ~GamePacketManager();                                                // FS2 0x0012de80

    sceVif1Packet *openList(int id);                                     // FS2 0x0012de00
    void closeList();                                                    // FS2 0x0012dd30
    void endFrame();                                                     // FS2 0x0012ddc0
    void kickAndSwap();                                                  // FS2 0x0012dd70

    u_char _30[0x3c - 0x30];
};

#endif /* _GAME_GAMEPACKET_H */
