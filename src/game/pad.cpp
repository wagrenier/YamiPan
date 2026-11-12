// game/pad.cpp -- the pad layer: libpad's ports and Pad (game/pad.h).

#include "game/pad.h"
#include "game/gamerate.h"

#include <string.h>

#include "sdk/libpad.h"
#include "sdk/scetypes.h"

// Stick dead zone: raw bytes 0x58..0xa7 read 0; outside it the distance
// past the edge over 88 gives -1..1.
constexpr int kStickDeadLow = 0x58;
constexpr int kStickDeadHigh = 0xa7;
constexpr float kStickRange = 88.0f;

// scePadSetActAlign's "no motor" byte.
constexpr u_char kPadActNone = 0xff;

// A report's button bytes when nothing is held (active low).
constexpr u_char kPadButtonsNone = 0xff;

// The repeat delays the constructors give every button.
constexpr u_short kPadRepeatFirst = 7;
constexpr u_short kPadRepeatNext = 9;

u_char gPadSysOpen;                     /* sbss 0x31adb0 */
int    gPadFrame;                       /* sbss 0x31adb4: 0 until the first padSysUpdate, then never 0 */

// Only users found: this TU (lui 0x4b pairs and gp offsets scanned).
alignas(64) static PadPort sPadPorts[kPadPortCount];  /* bss 0x4aed00 */
static Pad *sPadListeners[kPadListenerMax];           /* bss 0x4af000 */

// Pad::readSticks: each stick axis's (negative, positive) button, in
// libpad's axis order (data 0x2f3870).
static const int sStickBits[4][2] = {
    { PadBit_RStickLeft, PadBit_RStickRight },
    { PadBit_RStickUp, PadBit_RStickDown },
    { PadBit_LStickLeft, PadBit_LStickRight },
    { PadBit_LStickUp, PadBit_LStickDown },
};

// Pad::update: the button each of libpad's 12 pressure bytes belongs to
// (data 0x2f3890).
static const int sPressureBits[kPadPressureCount] = {
    PadBit_Right, PadBit_Left, PadBit_Up, PadBit_Down,
    PadBit_Triangle, PadBit_Circle, PadBit_Cross, PadBit_Square,
    PadBit_L1, PadBit_R1, PadBit_L2, PadBit_R2,
};

// A report's 16 button bits, active high.
static inline u_int padButtons(const u_char *data)
{
    return ~((u_int)(data[PadData_Buttons] << 8 | data[PadData_Buttons + 1]) | 0xffff0000);
}

// A report emptied: zeros with the buttons released.
static inline void padDataClear(u_char *data)
{
    memset(data, 0, kPadDataSize);
    data[PadData_Buttons] = kPadButtonsNone;
    data[PadData_Buttons + 1] = kPadButtonsNone;
}

// A raw stick byte to -1..1 (0 in the dead zone).
static inline float padStickValue(u_char raw)
{
    if (raw < kStickDeadLow)
        return (float)(raw - kStickDeadLow) / kStickRange;
    if (raw > kStickDeadHigh)
        return (float)(raw - kStickDeadHigh) / kStickRange;
    return 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Only for a DualShock 2: ask for a motor's value; PadPort::read sends it
// with the next report (and forgets it, m_actOneShot).
// FS2 0x0012f290

void Pad::setVibration(int motor, u_char value)
{
    if (m_termMode != PadTerm_DualShock2)
        return;
    PadPort *port = &sPadPorts[m_port];
    if (motor == PadMotor_Large)
        port->m_actRequest[PadMotor_Large] = value;
    else if (motor == PadMotor_Small)
        port->m_actRequest[PadMotor_Small] = value & 1;
}

// ──────────────────────────────────────────────────────────────────────
// EE slot 0 (+0x8) of Pad's vtable 0x311050 (the thunk 0x130060 jumps
// here): once per pad frame, the port's report into the button words,
// sticks, pressures and repeats.
// FS2 0x0012f300

int Pad::update()
{
    if (gPadFrame == m_frame)
        return 0;

    PadPort *port = &sPadPorts[m_port];
    m_frame = 0;
    m_termMode = PadTerm_None;
    m_connected = 0;
    clear();
    m_frame = gPadFrame;
    m_connected = 0;

    if (!port->m_readOk)
        return -1;
    if (port->m_data[PadData_Status] != 0)
        return -1;

    m_connected = 1;
    m_termMode = port->m_termMode;
    if (m_termMode == PadTerm_DualShock2) {
        u_int previous = padButtons(port->m_prevData);
        m_previous = previous;
        u_int held = padButtons(port->m_data);
        u_int changed = previous ^ held;
        m_held = held;
        m_pressed = held & changed;
        m_released = ~held & changed;

        readSticks(port->m_data, port->m_prevData);

        for (u_int i = 0; i < kPadPressureCount; i++)
            m_pressure[sPressureBits[i]] = port->m_data[PadData_Pressure + i];

        // Buttons without a pressure byte (select, L3, R3, start) read 1
        // while held; nothing reads pressure while released.
        for (int i = 0; i < kPadButtonCount; i++) {
            if (!(m_held & (1u << i))) {
                if (m_pressure[i] != 0)
                    m_pressure[i] = 0;
            } else if (m_pressure[i] == 0) {
                m_pressure[i] = 1;
            }
        }
    }
    updateRepeat();
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// m_repeat: a button repeats the frame it is pressed, then at
// count == first and every next - first frames while held.
// FS2 0x0012f600

void Pad::updateRepeat()
{
    u_int held = m_held;
    u_int repeat = 0;
    u_int pressed = m_pressed;
    u_int bit = 1;
    PadRepeat *r = m_repeatState;
    for (int i = 0; i < kPadButtonCount; i++, r++, bit <<= 1) {
        if (pressed & bit) {
            r->count = 0;
            repeat |= bit;
        } else if (held & bit) {
            // PC: game_rate (game/gamerate.h): the repeat counts 25 Hz frames.
            if (palTicks() == 0)
                continue;
            r->count++;
            int first = r->first;
            if (first <= r->count && (r->count - first) % (r->next - first) == 0)
                repeat |= bit;
        } else {
            r->count = 0;
        }
    }
    m_repeat = repeat;
}

// ──────────────────────────────────────────────────────────────────────
// The sticks: raw bytes, -1..1 values, and each direction past the dead
// zone as a button (pressure |value| * 255), ORed into the button words
// with last frame's directions from prevData.
// FS2 0x0012f6b0

void Pad::readSticks(const u_char *data, const u_char *prevData)
{
    u_int previous = 0;
    u_int held = 0;
    for (int i = 0; i < 4; i++) {
        m_analog[i] = data[PadData_Sticks + i];
        float v = padStickValue(data[PadData_Sticks + i]);
        m_stick[i] = v;
        if (v < 0.0f) {
            held |= 1u << sStickBits[i][0];
            m_pressure[sStickBits[i][0]] = (u_char)(u_int)(v * -255.0f);
        } else if (v > 0.0f) {
            held |= 1u << sStickBits[i][1];
            m_pressure[sStickBits[i][1]] = (u_char)(u_int)(v * 255.0f);
        }

        float p = padStickValue(prevData[PadData_Sticks + i]);
        if (p < 0.0f)
            previous |= 1u << sStickBits[i][0];
        else if (p > 0.0f)
            previous |= 1u << sStickBits[i][1];
    }
    u_int changed = previous ^ held;
    m_previous |= previous;
    m_held |= held;
    m_pressed |= held & changed;
    m_released |= ~held & changed;
}

// ──────────────────────────────────────────────────────────────────────
// Everything read back to rest (sticks centred); MaskedGamePad::update
// calls it, the constructors and Pad::update inline the same stores.
// FS2 0x0012f960

void Pad::clear()
{
    m_previous = 0;
    m_held = 0;
    m_pressed = 0;
    m_released = 0;
    m_repeat = 0;
    memset(m_analog, 0x80, sizeof(m_analog));
    memset(m_stick, 0, sizeof(m_stick));
    memset(m_pressure, 0, sizeof(m_pressure));
}

// ──────────────────────────────────────────────────────────────────────
// Joins or leaves padSysUpdate's listeners when that changes; the
// constructors and the destructor inline it.
// FS2 0x0012fa30

void Pad::setListening(bool on)
{
    if (m_listening != on) {
        registerListener(on);
        m_listening = on;
    }
}

// ──────────────────────────────────────────────────────────────────────
// CodeWarrior destructor (this, short flags): Pad's vtable back, out of
// the listener table, operator delete when flags > 0.
// callcheck: operator_delete (the deleting variant, compiler generated)
// callcheck: registerListener (through setListening, inline in the ROM)
// FS2 0x0012fa90

Pad::~Pad()
{
    setListening(false);
}

// ──────────────────────────────────────────────────────────────────────
// Port 0, for GamePad's default constructor (0x12fcf0 with the port fixed).
// callcheck: registerListener (through setListening, inline in the ROM)
// FS2 0x0012fb10

Pad::Pad(bool listen)
{
    m_frame = 0;
    m_termMode = PadTerm_None;
    m_connected = 0;
    clear();
    m_port = 0;
    m_listening = 0;
    m_connected = 0;
    setListening(listen);
    for (int i = 0; i < kPadButtonCount; i++) {
        m_repeatState[i].first = kPadRepeatFirst;
        m_repeatState[i].next = kPadRepeatNext;
        m_repeatState[i].count = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// callcheck: registerListener (through setListening, inline in the ROM)
// FS2 0x0012fcf0

Pad::Pad(int port, bool listen)
{
    m_frame = 0;
    m_termMode = PadTerm_None;
    m_connected = 0;
    clear();
    m_port = port;
    m_listening = 0;
    m_connected = 0;
    setListening(listen);
    for (int i = 0; i < kPadButtonCount; i++) {
        m_repeatState[i].first = kPadRepeatFirst;
        m_repeatState[i].next = kPadRepeatNext;
        m_repeatState[i].count = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Adds (on) or removes this pad in the listener table; PadListen.
// FS2 0x0012fed0

int Pad::registerListener(bool on)
{
    if (!on) {
        bool found = false;
        for (int i = 0; i < kPadListenerMax; i++) {
            if (sPadListeners[i] == this) {
                sPadListeners[i] = 0;
                found = true;
            }
        }
        return found ? PadListen_Ok : PadListen_NotFound;
    }

    int slot = -1;
    for (int i = 0; i < kPadListenerMax; i++) {
        if (sPadListeners[i] == this)
            return PadListen_Already;
        if (sPadListeners[i] == 0)
            slot = i;
    }
    if (slot < 0)
        return PadListen_Full;
    sPadListeners[slot] = this;
    return PadListen_Ok;
}

// ──────────────────────────────────────────────────────────────────────
// Every frame from gameMainThread: read both ports, advance the pad frame
// (never 0, the value a new Pad holds), update every listening pad.
// FS2 0x0012ffa0

int padSysUpdate()
{
    if (gPadSysOpen == 1) {
        for (u_int i = 0; i < kPadPortCount; i++)
            sPadPorts[i].read();
    }
    if (++gPadFrame == 0)
        gPadFrame = 1;
    for (int i = 0; i < kPadListenerMax; i++) {
        if (sPadListeners[i] != 0)
            sPadListeners[i]->update();
    }
    return gPadFrame;
}

// ──────────────────────────────────────────────────────────────────────
// main's teardown: close the open ports (a closed port's record is
// zeroed). gPadSysOpen stays set.
// FS2 0x00130070

int padSysShutdown()
{
    if (gPadSysOpen == 1) {
        for (u_int i = 0; i < kPadPortCount; i++) {
            PadPort *port = &sPadPorts[i];
            if (port->m_opened == 1 && scePadPortClose(port->m_port, 0) == 1)
                memset(port, 0, sizeof(*port));
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// From gameInit: libpad and both ports (slot 0) once, then the pad frame
// and the listener table reset. 1 when scePadInit failed.
// FS2 0x00130130

int padSysInit()
{
    int result = 0;
    if (gPadSysOpen == 0) {
        if (scePadInit(0) == 1) {
            gPadSysOpen = 1;
            for (u_int i = 0; i < kPadPortCount; i++) {
                PadPort *port = &sPadPorts[i];
                if (scePadPortOpen(i, 0, port->m_dmaBuffer) == 1) {
                    port->m_opened = 1;
                    port->m_port = i;
                    port->m_readOk = 0;
                }
                port->m_actOneShot = 1;
            }
        } else {
            result = 1;
        }
    }
    gPadFrame = 0;
    for (int i = 0; i < kPadListenerMax; i++)
        sPadListeners[i] = 0;
    return result;
}

// ──────────────────────────────────────────────────────────────────────
// Per frame and port: shift the report, then by libpad's state either
// reset (disconnected), step the detection (PadPhase), or read the report
// and send the motor requests. Other states (finding the pad, busy) wait.
// Always 0.
// FS2 0x00130230

int PadPort::read()
{
    int port = m_port;
    int state = scePadGetState(port, 0);
    memcpy(m_prevData, m_data, kPadDataSize);
    padDataClear(m_data);
    m_readOk = 0;

    if (state == scePadStateDiscon) {
        padDataClear(m_data);
        padDataClear(m_prevData);
        m_termMode = PadTerm_None;
        m_phase = PadPhase_Detect;
        memset(m_actAlign, 0, sizeof(m_actAlign));
        memset(m_actDirect, 0, sizeof(m_actDirect));
        memset(m_actRequest, 0, sizeof(m_actRequest));
        return 0;
    }
    if (state != scePadStateStable && state != scePadStateFindCTP1)
        return 0;

    switch (m_phase) {
    case PadPhase_Detect:
    case PadPhase_Redetect: {
        int id = scePadInfoMode(port, 0, InfoModeCurID, 0);
        int exId = scePadInfoMode(port, 0, InfoModeCurExID, 0);
        if (exId > 0)
            id = exId;
        switch (id) {
        case 0:
            padDataClear(m_data);
            padDataClear(m_prevData);
            m_termMode = PadTerm_None;
            m_phase = PadPhase_Detect;
            memset(m_actAlign, 0, sizeof(m_actAlign));
            memset(m_actDirect, 0, sizeof(m_actDirect));
            memset(m_actRequest, 0, sizeof(m_actRequest));
            break;
        case PadTerm_Ext300:
        case PadTerm_Ext100:
        case PadTerm_NamcoGun:
        case PadTerm_Joystick:
        case PadTerm_KonamiGun:
        case PadTerm_NeGcon:
            m_termMode = id;
            m_phase = PadPhase_Ready;
            break;
        case PadTerm_DualShock:
            m_phase = PadPhase_AnalogCheckAct;
            break;
        case PadTerm_Digital:
            m_phase = PadPhase_DigitalCheck;
            break;
        default:
            m_termMode = PadTerm_Unknown;
            m_phase = PadPhase_Ready;
            break;
        }
        return 0;
    }

    case PadPhase_DigitalCheck:
        if (scePadInfoMode(port, 0, InfoModeIdTable, -1) == 0) {
            m_phase = PadPhase_Ready;
            m_termMode = PadTerm_Digital;
            return 0;
        }
        m_phase++;
        // fall through: switch it to analog
    case PadPhase_DigitalSetAnalog:
        if (scePadSetMainMode(port, 0, 1, 3) == 1)
            m_phase++;
        break;

    case PadPhase_DigitalWaitAnalog:
        if (scePadGetReqState(port, 0) == scePadReqStateFailed)
            m_phase--;
        if (scePadGetReqState(port, 0) == scePadReqStateComplete)
            m_phase = PadPhase_Redetect;
        break;

    case PadPhase_AnalogCheckAct:
        if (scePadInfoAct(port, 0, -1, 0) == 0) {
            m_termMode = PadTerm_NoActuators;
            m_phase = PadPhase_Ready;
        } else if (scePadSetMainMode(port, 0, 1, 3) == 1) {
            m_phase++;
        }
        break;

    case PadPhase_AnalogWaitLock:
        if (scePadGetReqState(port, 0) == scePadReqStateFailed)
            m_phase--;
        if (scePadGetReqState(port, 0) == scePadReqStateComplete)
            m_phase = PadPhase_CheckPress;
        break;

    case PadPhase_CheckPress:
        if (scePadInfoPressMode(port, 0) == 1) {
            m_termMode = PadTerm_DualShock2;
            m_phase = PadPhase_EnterPress;
        } else {
            m_termMode = PadTerm_DualShock;
            m_phase = PadPhase_SetActAlign;
        }
        break;

    case PadPhase_EnterPress:
        if (scePadEnterPressMode(port, 0) == 1)
            m_phase++;
        break;

    case PadPhase_WaitPress:
        if (scePadGetReqState(port, 0) == scePadReqStateFailed)
            m_phase--;
        if (scePadGetReqState(port, 0) == scePadReqStateComplete)
            m_phase = PadPhase_SetActAlign;
        break;

    case PadPhase_SetActAlign:
        // No motors: ready, but the align is still sent (and a success
        // steps the phase to 100, which reads like Ready).
        if (scePadInfoAct(port, 0, -1, 0) == 0)
            m_phase = PadPhase_Ready;
        m_actAlign[0] = PadMotor_Small;
        m_actAlign[1] = PadMotor_Large;
        m_actAlign[2] = kPadActNone;
        m_actAlign[3] = kPadActNone;
        m_actAlign[4] = kPadActNone;
        m_actAlign[5] = kPadActNone;
        memset(m_actDirect, 0, sizeof(m_actDirect));
        memset(m_actRequest, 0, sizeof(m_actRequest));
        if (scePadSetActAlign(port, 0, m_actAlign) != 0)
            m_phase++;
        break;

    case PadPhase_WaitActAlign:
        if (scePadGetReqState(port, 0) == scePadReqStateFailed)
            m_phase--;
        if (scePadGetReqState(port, 0) == scePadReqStateComplete)
            m_phase = PadPhase_Ready;
        break;

    default:
        if (scePadRead(port, 0, m_data) == 0)
            return 0;
        m_readOk = 1;
        m_actDirect[PadMotor_Small] = m_actRequest[PadMotor_Small];
        m_actDirect[PadMotor_Large] = m_actRequest[PadMotor_Large];
        if (m_actOneShot == 1)
            memset(m_actRequest, 0, sizeof(m_actRequest));
        if (scePadInfoAct(port, 0, -1, 0) == 0)
            return 0;
        scePadSetActDirect(port, 0, m_actDirect);
        return 0;
    }
    return 0;
}
