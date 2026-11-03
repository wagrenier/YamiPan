/* ==========================================================================
 *  game/gamesystem.h
 *
 *  main, the boot (gameInit), the main loop (gameMainThread) and the
 *  global object GameSystem (0x540 bytes, gGameSystem), with the two pad
 *  classes the game's input goes through. The file name is a guess (no
 *  source string in 0x12cd20..0x12dd30), as is GameSystem's class name.
 *
 *  GameSystem is built once by gameInit and deleted by main after the game
 *  ends; GameSystem::flipFrame runs once per frame from gameMainThread: the
 *  retail-stripped debug meter, the end of the frame's packet lists, the
 *  master clock, and the DMA kick of the finished chain.
 *
 *  CodeWarrior places a class's vptr after its data (the pads' at +0x120);
 *  the host compilers put it at 0, so every class here is layout: host and
 *  keeps the EE offsets in comments.
 * ======================================================================== */
#ifndef _GAME_GAMESYSTEM_H
#define _GAME_GAMESYSTEM_H

#include "game/clock.h"
#include "game/gamepacket.h"
#include "game/pad.h"
#include "game/perfmeter.h"
#include "game/sysdata.h"
#include "sdk/scetypes.h"

// Vsyncs per frame: the game runs at 30 fps (PAL 25) unless a frame is late.
constexpr int kVsyncsPerFrame = 2;
// The master clock's ticks per frame (6 per vsync).
constexpr int kClockTicksPerFrame = 12;

// The pad GameSystem wraps for the game: Pad plus sticks squared into a
// circle, their lengths and angles, and the button map the actions go
// through. GameSystem's two GamePads and its MaskedGamePad are built by the
// inline constructor; 0x12d5a0 / 0x12d540 are the out-of-line default
// constructor and destructor other code uses.
// layout: host -- polymorphic (vtable 0x311020, one slot; EE vptr at +0x120);
// EE size 0x178.
class GamePad : public Pad
{
public:
    // Inline in the ROM (GameSystem::GameSystem holds three copies).
    GamePad(int port, bool listen) : Pad(port, listen) { init(); }
    GamePad();                                                           // FS2 0x0012d5a0
    ~GamePad();                                                          // FS2 0x0012d540

    int  update() override;                                              // FS2 0x0012dc60
    int  read();                                                         // FS2 0x0012d4f0
    void computeSticks();                                                // FS2 0x0012d2c0
    void setButtonMap(ButtonMap *map);                                   // FS2 0x0012d2b0

    // The constructors' common part: the own map's defaults, no options map,
    // sticks cleared.
    void init()
    {
        m_buttonMap.setDefaults();
        m_optionsMap = 0;
        for (int i = 0; i < 4; i++) {
            m_dir[i] = 0.0f;
            m_angle[i] = 0.0f;
            m_length[i] = 0.0f;
        }
    }

    float      m_dir[4];        // FS2 +0x124: left x, left y, right x, right y, squared to a circle, y up
    float      m_angle[4];      // FS2 +0x134: left atan2(y, x), left atan2(-x, y), right the same
    float      m_length[4];     // FS2 +0x144: left length twice, right length twice
    ButtonMap  m_buttonMap;     // FS2 +0x154
    ButtonMap *m_optionsMap;    // FS2 +0x174: the options' map (GameOptions::buttonMap)
};

// A GamePad that clears its state while L1 + L2 are held (vtable 0x311030).
// layout: host -- EE size 0x178.
class MaskedGamePad : public GamePad
{
public:
    MaskedGamePad(int port, bool listen) : GamePad(port, listen) {}

    int  update() override;                                              // FS2 0x0012dcb0
};

// The debug meter's modes (GameSystem::m_meterMode).
enum MeterMode
{
    MeterMode_Off    = 0,
    MeterMode_Bars   = 1,       // guess: only != 0 and == 1 are tested
};

// layout: host -- members are polymorphic or hold pointers; EE 0x540
// (operator new(0x540) in gameInit).
class GameSystem
{
public:
    GameSystem();                                                        // FS2 0x0012d0d0

    void flipFrame(int vsyncs);                                          // FS2 0x0012cdf0
    void endFrameSync();                                                 // FS2 0x0012cd20

    int               m_meterMode;  // FS2 +0x000: MeterMode, 0 in retail
    GamePad           m_pad0;       // FS2 +0x004: port 0
    GamePad           m_pad1;       // FS2 +0x17c: port 1 (R3 here dumps the meter groups)
    MaskedGamePad     m_maskedPad;  // FS2 +0x2f4: port 1
    GamePacketManager m_packets;    // FS2 +0x46c
    PerfMeter         m_perfMeter;  // FS2 +0x4a8
    MasterClock       m_clock;      // FS2 +0x510: the root game clock
};

extern GameSystem *gGameSystem;     /* sbss 0x31ad84 (gp-0x7aec) */
extern int         gArgc;           /* sbss 0x31ad88 (gp-0x7ae8) */
extern char      **gArgv;           /* sbss 0x31ad8c (gp-0x7ae4) */
extern u_char      gDmacBusError;   /* sbss 0x31ad90 (gp-0x7ae0): set by the DMAC handler */
extern u_char      gGfxFlipNoPackets; /* sbss 0x31ad94 (gp-0x7adc): flip without packet or draw env */
extern u_char      gGfxPutDrawEnv;  /* sdata 0x31a890 (gp-0x7fe0): 1 */
extern u_char      gGfxKickDisplayPacket; /* sdata 0x31a894 (gp-0x7fdc): 1; 0 after a GS reset */

int main(int argc, char **argv);                                         // FS2 0x0012d910

#endif /* _GAME_GAMESYSTEM_H */
