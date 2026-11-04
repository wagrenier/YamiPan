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

#include "rena/rena_pkt_vif1pktmanage.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

// The packet lists (= draw layers, drawn in id order). Only the ones read
// so far.
enum PacketList
{
    PacketList_None = -1,       // GamePacketManager: no list open
    PacketList_SceneModel = 8,  // StageScene::draw: the scene's model holder
    PacketList_Scene2d    = 9,  // StageScene::draw: filters, fades and the fill over the 3D frame
    PacketList_StageClock = 10, // the stage loop's StageResult::update; StageScene::draw: the item list and the menu
    PacketList_Overlay = 11,    // overlayThread: the BG movie screen and the loading indicator; StageScene::draw: alert, fade, letterbox
    PacketList_Screen  = 12,    // runLayout: modal screens and dialogs; StageScene::draw: the pool's text, subtitles
    PacketList_Finish = 13,     // GameSystem::flipFrame puts a GS FINISH here
    PacketList_Count  = 14,
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
