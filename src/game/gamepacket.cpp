// game/gamepacket.cpp -- GamePacketManager, the frame's packet lists on rena's Vif1PktManage (game/gamepacket.h).

#include "game/gamepacket.h"

#include "sdk/eekernel.h"
#include "sdk/scetypes.h"

// ──────────────────────────────────────────────────────────────────────
// Ends drawing into the open list: write the cache back, forget the id,
// close the list's DMA tag. The overflow test's result is left unread.
// FS2 0x0012dd30

void GamePacketManager::closeList()
{
    FlushCache(WRITEBACK_DCACHE);
    m_openList = PacketList_None;
    Close();
    IsOverflow();
}

// ──────────────────────────────────────────────────────────────────────
// Once per frame, after endFrame: the chain goes out on VIF1 unless a
// skip was asked for, then the other buffer becomes the frame's.
// FS2 0x0012dd70

void GamePacketManager::kickAndSwap()
{
    FlushCache(WRITEBACK_DCACHE);
    if (!m_skipKick)
        Kick();
    m_skipKick = 0;
    Reset();
}

// ──────────────────────────────────────────────────────────────────────
// Ends the frame's drawing: link the lists into one chain (in id order).
// FS2 0x0012ddc0

void GamePacketManager::endFrame()
{
    FlushCache(WRITEBACK_DCACHE);
    m_openList = PacketList_None;
    Terminate();
}

// ──────────────────────────────────────────────────────────────────────
// Starts drawing into list `id` and returns its packet. Unless `id` is
// the list already open, the open one is closed first -- also when none
// is (m_openList PacketList_None), so a closed list gets closed again.
// FS2 0x0012de00
// callcheck: FlushCache, Close, IsOverflow (closeList inline)

sceVif1Packet *GamePacketManager::openList(int id)
{
    FlushCache(WRITEBACK_DCACHE);
    if (m_openList != id)
        closeList();
    m_openList = id;
    return Open(id);
}

// ──────────────────────────────────────────────────────────────────────
// EE slot 0 (+0x8) of vtable 0x311040.
// FS2 0x0012de80
// callcheck: ~Vif1PktManage, operator_delete (the base destructor; the deleting destructor's flag)

GamePacketManager::~GamePacketManager()
{
}

// ──────────────────────────────────────────────────────────────────────
// The two frame buffers, the lists 0..13, the first buffer current, an
// empty chain, no list open.
// FS2 0x0012dee0
// callcheck: Vif1PktManage (the base constructor)

GamePacketManager::GamePacketManager(void *buf0, u_int size0, void *buf1, u_int size1)
{
    AddBuffer(buf0, size0);
    AddBuffer(buf1, size1);
    for (int id = 0; id < PacketList_Count; id++)
        AddList(id);
    Reset();
    Terminate();
    m_openList = PacketList_None;
    m_skipKick = 0;
}
