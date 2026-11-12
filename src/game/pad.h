/* ==========================================================================
 *  game/pad.h
 *
 *  The pad layer 0x12f290..0x130a20 (game/pad.cpp): libpad's two ports
 *  (PadPort, polled by padSysUpdate every frame) and Pad, the game's view
 *  of one port. padSysInit 0x130130 opens the ports and empties the 16
 *  listener slots; padSysUpdate reads the ports, advances the pad frame
 *  and calls each listening pad's update (its one virtual). GameSystem
 *  holds three (game/gamesystem.h).
 *
 *  Button words are libpad's, inverted to active-high: the SCE_PAD* bits
 *  of sdk/libpad.h (L2 0x1, R2 0x2, L1 0x4, R1 0x8, ... select 0x100,
 *  L3 0x200, R3 0x400, start 0x800), plus the stick directions as virtual
 *  buttons 16..23 (PadBit).
 *
 *  Pad::update only reads buttons from a DualShock 2 in pressure mode
 *  (PadTerm_DualShock2): any other pad reads as connected with nothing
 *  held.
 * ======================================================================== */
#ifndef _GAME_PAD_H
#define _GAME_PAD_H

#include <stddef.h>

#include "sdk/libpad.h"
#include "sdk/scetypes.h"

// libpad's two ports (slot 0 only).
constexpr int kPadPortCount = 2;
// padSysUpdate's listener table.
constexpr int kPadListenerMax = 16;
// Pad's per-button arrays (pressure, repeat): bits 0..23 are used, the
// 25th entry never gets a bit.
constexpr int kPadButtonCount = 25;
// A libpad report (scePadRead's rdata).
constexpr int kPadDataSize = 32;

// A button's bit index in Pad's words: libpad's 16, then the sticks.
enum PadBit
{
    PadBit_L2, PadBit_R2, PadBit_L1, PadBit_R1,
    PadBit_Triangle, PadBit_Circle, PadBit_Cross, PadBit_Square,
    PadBit_Select, PadBit_L3, PadBit_R3, PadBit_Start,
    PadBit_Up, PadBit_Right, PadBit_Down, PadBit_Left,
    PadBit_LStickUp, PadBit_LStickRight, PadBit_LStickDown, PadBit_LStickLeft,
    PadBit_RStickUp, PadBit_RStickRight, PadBit_RStickDown, PadBit_RStickLeft,
};

// Byte offsets in a libpad report.
enum PadData
{
    PadData_Status = 0,         // 0: the report is valid
    PadData_Buttons = 2,        // 2 bytes, active low: high byte first
    PadData_Sticks = 4,         // right x, right y, left x, left y; 0x80 centre, y down
    PadData_Pressure = 8,       // 12 pressure bytes (DualShock 2 pressure mode)
};
constexpr int kPadPressureCount = 12;

// PadPort::m_termMode, copied into Pad::m_termMode: scePadInfoMode's
// terminal id, or the game's own codes for a DualShock (7) once its modes
// are known.
enum PadTerm
{
    PadTerm_None = 0,           // disconnected / not detected yet
    PadTerm_NeGcon = 2,
    PadTerm_KonamiGun = 3,
    PadTerm_Digital = 4,        // digital pad without an analog mode
    PadTerm_Joystick = 5,
    PadTerm_NamcoGun = 6,
    PadTerm_DualShock = 7,      // analog, no pressure mode
    PadTerm_DualShock2 = 8,     // analog, pressure mode entered: the only one Pad reads
    PadTerm_NoActuators = 9,    // analog, scePadInfoAct says no motors
    PadTerm_Ext100 = 0x100,     // extended ids (scePadInfoMode's CurExID): device a guess
    PadTerm_Ext300 = 0x300,
    PadTerm_Unknown = 999,
};

// PadPort::m_phase: PadPort::read's detection steps, numbered as the game
// numbered them; a wait step goes back one on scePadReqStateFailed.
enum PadPhase
{
    PadPhase_Detect = 0,        // 0 and 1: read the terminal id
    PadPhase_Redetect = 1,
    PadPhase_DigitalCheck = 0x28,       // has a mode table: switch to analog
    PadPhase_DigitalSetAnalog = 0x29,
    PadPhase_DigitalWaitAnalog = 0x2a,  // then detect again
    PadPhase_AnalogCheckAct = 0x46,     // motors? then lock analog mode
    PadPhase_AnalogWaitLock = 0x47,
    PadPhase_CheckPress = 0x48,
    PadPhase_EnterPress = 0x4c,
    PadPhase_WaitPress = 0x4d,
    PadPhase_SetActAlign = 0x50,
    PadPhase_WaitActAlign = 0x51,
    PadPhase_Ready = 99,        // and anything not listed: read every frame
};

// Pad::setVibration's motor, also the index into PadPort::m_actRequest.
enum PadMotor
{
    PadMotor_Small = 0,         // on / off
    PadMotor_Large = 1,         // 0..255
};

// Pad::registerListener's result.
enum PadListen
{
    PadListen_Full = -1,
    PadListen_Ok = 0,
    PadListen_Already = 1,      // adding: already listening
    PadListen_NotFound = 2,     // removing: was not listening
};

// One libpad port (bss 0x4aed00, two, 0x180 apart).
struct PadPort
{
    int read();                                                          // FS2 0x00130230

    u_long128 m_dmaBuffer[scePadDmaBufferMax]; // FS2 +0x000: scePadPortOpen's buffer
    u_char  m_data[kPadDataSize];       // FS2 +0x100: this frame's report
    u_char  m_prevData[kPadDataSize];   // FS2 +0x120: last frame's
    int     m_termMode;         // FS2 +0x140: PadTerm
    int     m_phase;            // FS2 +0x144: PadPhase
    u_char  m_actAlign[6];      // FS2 +0x148: scePadSetActAlign: motor 0, motor 1, none
    u_char  _14e[2];
    u_char  m_actDirect[6];     // FS2 +0x150: scePadSetActDirect, from m_actRequest
    u_char  m_actRequest[2];    // FS2 +0x156: PadMotor values Pad::setVibration asks for
    u_char  m_actOneShot;       // FS2 +0x158: requests cleared once sent (padSysInit sets it)
    u_char  m_opened;           // FS2 +0x159
    u_char  _15a[2];
    int     m_port;             // FS2 +0x15c
    u_char  _160[0x1f];
    u_char  m_readOk;           // FS2 +0x17f: scePadRead succeeded this frame
};
static_assert(sizeof(PadPort) == 0x180, "PadPort: EE 0x180");
static_assert(offsetof(PadPort, m_data) == 0x100, "PadPort::m_data");
static_assert(offsetof(PadPort, m_prevData) == 0x120, "PadPort::m_prevData");
static_assert(offsetof(PadPort, m_termMode) == 0x140, "PadPort::m_termMode");
static_assert(offsetof(PadPort, m_phase) == 0x144, "PadPort::m_phase");
static_assert(offsetof(PadPort, m_actAlign) == 0x148, "PadPort::m_actAlign");
static_assert(offsetof(PadPort, m_actDirect) == 0x150, "PadPort::m_actDirect");
static_assert(offsetof(PadPort, m_actRequest) == 0x156, "PadPort::m_actRequest");
static_assert(offsetof(PadPort, m_actOneShot) == 0x158, "PadPort::m_actOneShot");
static_assert(offsetof(PadPort, m_opened) == 0x159, "PadPort::m_opened");
static_assert(offsetof(PadPort, m_port) == 0x15c, "PadPort::m_port");
static_assert(offsetof(PadPort, m_readOk) == 0x17f, "PadPort::m_readOk");

// A button's auto-repeat: it repeats at count == first, then every
// next - first frames; the constructors set {7, 9}.
struct PadRepeat
{
    u_short first;              // FS2 +0x0
    u_short next;               // FS2 +0x2
    int     count;              // FS2 +0x4: frames held
};
static_assert(sizeof(PadRepeat) == 0x8, "PadRepeat: EE 0x8");

// layout: host -- polymorphic; on the EE the vptr follows the data at +0x120
// (vtable 0x311050, one slot), EE size 0x124.
class Pad
{
public:
    Pad(int port, bool listen);                                          // FS2 0x0012fcf0
    Pad(bool listen);                                                    // FS2 0x0012fb10
    ~Pad();                                                              // FS2 0x0012fa90

    // EE slot 0 (+0x8, through the thunk 0x130060): read the port once per
    // pad frame; 1 read, 0 already read this frame, -1 nothing to read.
    virtual int update();                                                // FS2 0x0012f300
    void clear();                                                        // FS2 0x0012f960
    void setListening(bool on);                                          // FS2 0x0012fa30
    int  registerListener(bool on);                                      // FS2 0x0012fed0
    void setVibration(int motor, u_char value);                          // FS2 0x0012f290

    void readSticks(const u_char *data, const u_char *prevData);         // FS2 0x0012f6b0
    void updateRepeat();                                                 // FS2 0x0012f600

    int     m_frame;            // FS2 +0x00: pad frame read
    int     m_port;             // FS2 +0x04
    u_char  m_listening;        // FS2 +0x08: in padSysUpdate's listener table
    int     m_termMode;         // FS2 +0x0c: PadTerm
    u_int   m_previous;         // FS2 +0x10
    u_int   m_held;             // FS2 +0x14
    u_int   m_pressed;          // FS2 +0x18
    u_int   m_released;         // FS2 +0x1c
    u_int   m_repeat;           // FS2 +0x20: pressed, or auto-repeating (PadRepeat)
    u_char  m_analog[4];        // FS2 +0x24: raw stick bytes in libpad's order, 0x80 = centre
    float   m_stick[4];         // FS2 +0x28: right x, right y, left x, left y (-1..1, y down)
    u_char  m_pressure[kPadButtonCount]; // FS2 +0x38: per PadBit; held without pressure reads 1
    PadRepeat m_repeatState[kPadButtonCount]; // FS2 +0x54
    u_char  m_connected;        // FS2 +0x11c: the port's report was valid
};

int padSysInit();                                                        // FS2 0x00130130
int padSysUpdate();                                                      // FS2 0x0012ffa0
int padSysShutdown();                                                    // FS2 0x00130070

#endif /* _GAME_PAD_H */
