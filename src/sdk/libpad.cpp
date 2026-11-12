// sdk/libpad.cpp -- libpad's report format over yamipan::io::InputSystem, which owns the devices. The port
// is always a DualShock 2: FS2's Pad reads terminal mode 8 only, so the host answers the detection it walks
// (id 7, actuators, pressure mode) and reports in that shape.
#include "libpad.h"

#include <array>
#include <cstring>

#include "yamipan/core/application.h"
#include "yamipan/io/input_system.h"

using yamipan::Application;
using yamipan::io::PadButton;
using yamipan::io::PadReport;

namespace
{

constexpr int kDualShockId = 7;
constexpr int kActuatorCount = 2;
constexpr std::size_t kReportBytes = 32;
constexpr unsigned char kReportHeader = 0x79; // DualShock 2, 9 words
constexpr unsigned char kPressureFull = 0xff;

// The report's word, byte-swapped by the game: PadButton n's bit.
constexpr std::array<u_short, yamipan::io::kPadButtonCount> kButtonBits{
    SCE_PADLup, SCE_PADLdown, SCE_PADLleft, SCE_PADLright, SCE_PADRup, SCE_PADRdown, SCE_PADRleft, SCE_PADRright,
    SCE_PADL1,  SCE_PADL2,    SCE_PADR1,    SCE_PADR2,     SCE_PADstart, SCE_PADselect, SCE_PADi, SCE_PADj,
};

// Which report byte each stick axis goes to, in PadStick order.
constexpr std::array<std::size_t, yamipan::io::kPadStickCount> kStickBytes{6, 7, 4, 5};

// The pressure byte of each pressure-sensitive button, in report order.
struct PressureByte
{
    std::size_t offset;
    PadButton button;
};

constexpr std::array<PressureByte, 12> kPressureBytes{{
    {8, PadButton::Right},
    {9, PadButton::Left},
    {10, PadButton::Up},
    {11, PadButton::Down},
    {12, PadButton::Triangle},
    {13, PadButton::Circle},
    {14, PadButton::Cross},
    {15, PadButton::Square},
    {16, PadButton::L1},
    {17, PadButton::R1},
    {18, PadButton::L2},
    {19, PadButton::R2},
}};

bool held(const PadReport &report, PadButton button)
{
    return (report.held >> static_cast<unsigned>(button) & 1) != 0;
}

} // namespace

int scePadInit(int)
{
    return 1;
}

int scePadPortOpen(int, int, void *)
{
    return 1;
}

int scePadPortClose(int, int)
{
    return 1;
}

int scePadGetState(int port, int)
{
    return port == 0 ? scePadStateStable : scePadStateDiscon;
}

int scePadGetReqState(int, int)
{
    return scePadReqStateComplete;
}

int scePadInfoMode(int port, int, int term, int)
{
    if (port != 0)
        return 0;
    switch (term)
    {
    case InfoModeCurID:
    case InfoModeCurExID:
        return kDualShockId;
    default:
        return 0;
    }
}

int scePadInfoAct(int port, int, int actuator, int)
{
    if (port != 0)
        return 0;
    return actuator < 0 ? kActuatorCount : 1;
}

int scePadSetMainMode(int, int, int, int)
{
    return 1;
}

int scePadInfoPressMode(int port, int)
{
    return port == 0 ? 1 : 0;
}

int scePadEnterPressMode(int, int)
{
    return 1;
}

int scePadSetActAlign(int, int, const unsigned char *)
{
    return 1;
}

int scePadSetActDirect(int port, int, const unsigned char *data)
{
    if (port == 0 && data != nullptr)
        Application::instance().input().rumble(data[1] != 0, data[0]);
    return 1;
}

int scePadRead(int port, int, unsigned char *rdata)
{
    if (port != 0 || rdata == nullptr)
        return 0;

    const PadReport report = Application::instance().input().read();

    std::memset(rdata, 0, kReportBytes);
    rdata[1] = kReportHeader;

    u_short buttons = 0xffff; // a report's bits are active low
    for (std::size_t button = 0; button < kButtonBits.size(); button++)
    {
        if ((report.held >> button & 1) != 0)
            buttons ^= kButtonBits[button];
    }
    rdata[2] = static_cast<u_char>(buttons >> 8);
    rdata[3] = static_cast<u_char>(buttons);

    for (std::size_t stick = 0; stick < kStickBytes.size(); stick++)
        rdata[kStickBytes[stick]] = report.sticks[stick];

    for (const PressureByte &pressure : kPressureBytes)
        rdata[pressure.offset] = held(report, pressure.button) ? kPressureFull : 0;
    return 1;
}
