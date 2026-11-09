#include "yamipan/iop/spu2_reverb.h"

#include <algorithm>
#include <cstring>

#include "yamipan/core/logger.h"

namespace yamipan::iop
{

namespace
{

// The registers in the hardware's order, rev00..rev1F.
enum Register
{
    dAPF1,
    dAPF2,
    vIIR,
    vCOMB1,
    vCOMB2,
    vCOMB3,
    vCOMB4,
    vWALL,
    vAPF1,
    vAPF2,
    mLSAME,
    mRSAME,
    mLCOMB1,
    mRCOMB1,
    mLCOMB2,
    mRCOMB2,
    dLSAME,
    dRSAME,
    mLDIFF,
    mRDIFF,
    mLCOMB3,
    mRCOMB3,
    mLCOMB4,
    mRCOMB4,
    dLDIFF,
    dRDIFF,
    mLAPF1,
    mRAPF1,
    mLAPF2,
    mRAPF2,
    vLIN,
    vRIN,
};

constexpr int kModes = 10;
constexpr std::uint32_t kModeMask = 0xff;
constexpr std::uint32_t kModeClearWorkArea = 0x100;

constexpr const char *kModeNames[kModes] = {"off",      "room", "studio A", "studio B", "studio C",
                                            "hall",     "space", "echo",    "delay",    "pipe"};

// The SDK's presets, by the mode libsd takes.
constexpr std::uint16_t kPresets[kModes][32] = {
    // 0: off
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001,
     0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001,
     0x0001, 0x0001, 0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0000, 0x0000},
    // 1: room
    {0x007D, 0x005B, 0x6D80, 0x54B8, 0xBED0, 0x0000, 0x0000, 0xBA80, 0x5800, 0x5300, 0x04D6,
     0x0333, 0x03F0, 0x0227, 0x0374, 0x01EF, 0x0334, 0x01B5, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x01B4, 0x0136, 0x00B8, 0x005C, 0x8000, 0x8000},
    // 2: studio A (small)
    {0x0033, 0x0025, 0x70F0, 0x4FA8, 0xBCE0, 0x4410, 0xC0F0, 0x9C00, 0x5280, 0x4EC0, 0x03E4,
     0x031B, 0x03A4, 0x02AF, 0x0372, 0x0266, 0x031C, 0x025D, 0x025C, 0x018E, 0x022F, 0x0135,
     0x01D2, 0x00B7, 0x018F, 0x00B5, 0x00B4, 0x0080, 0x004C, 0x0026, 0x8000, 0x8000},
    // 3: studio B (medium)
    {0x00B1, 0x007F, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xB4C0, 0x5280, 0x4EC0, 0x0904,
     0x076B, 0x0824, 0x065F, 0x07A2, 0x0616, 0x076C, 0x05ED, 0x05EC, 0x042E, 0x050F, 0x0305,
     0x0462, 0x02B7, 0x042F, 0x0265, 0x0264, 0x01B2, 0x0100, 0x0080, 0x8000, 0x8000},
    // 4: studio C (large)
    {0x00E3, 0x00A9, 0x6F60, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xA680, 0x5680, 0x52C0, 0x0DFB,
     0x0B58, 0x0D09, 0x0A3C, 0x0BD9, 0x0973, 0x0B59, 0x08DA, 0x08D9, 0x05E9, 0x07EC, 0x04B0,
     0x06EF, 0x03D2, 0x05EA, 0x031D, 0x031C, 0x0238, 0x0154, 0x00AA, 0x8000, 0x8000},
    // 5: hall
    {0x01A5, 0x0139, 0x6000, 0x5000, 0x4C00, 0xB800, 0xBC00, 0xC000, 0x6000, 0x5C00, 0x15BA,
     0x11BB, 0x14C2, 0x10BD, 0x11BC, 0x0DC1, 0x11C0, 0x0DC3, 0x0DC0, 0x09C1, 0x0BC4, 0x07C1,
     0x0A00, 0x06CD, 0x09C2, 0x05C1, 0x05C0, 0x041A, 0x0274, 0x013A, 0x8000, 0x8000},
    // 6: space echo
    {0x033D, 0x0231, 0x7E00, 0x5000, 0xB400, 0xB000, 0x4C00, 0xB000, 0x6000, 0x5400, 0x1ED6,
     0x1A31, 0x1D14, 0x183B, 0x1BC2, 0x16B2, 0x1A32, 0x15EF, 0x15EE, 0x1055, 0x1334, 0x0F2D,
     0x11F6, 0x0C5D, 0x1056, 0x0AE1, 0x0AE0, 0x07A2, 0x0464, 0x0232, 0x8000, 0x8000},
    // 7: echo
    {0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x8100, 0x0000, 0x0000, 0x1FFF,
     0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000, 0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000},
    // 8: delay
    {0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FFF,
     0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000, 0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000},
    // 9: pipe
    {0x0017, 0x0013, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0x8500, 0x5F80, 0x54C0, 0x0371,
     0x02AF, 0x02E5, 0x01DF, 0x02B0, 0x01D7, 0x0358, 0x026A, 0x01D6, 0x011E, 0x012D, 0x00B1,
     0x011F, 0x0059, 0x01A0, 0x00E3, 0x0058, 0x0040, 0x0028, 0x0014, 0x8000, 0x8000},
};

// The work area of each mode, in bytes (RXSNDDRV's s_effectModes has the same sizes).
constexpr std::uint32_t kSizes[kModes] = {0x00000080, 0x000026c0, 0x00001f40, 0x00004840, 0x00006fe0,
                                          0x0000ade0, 0x0000f6c0, 0x00018040, 0x00018040, 0x00003c00};

std::int32_t mulVol(std::int32_t sample, std::int32_t volume)
{
    return static_cast<std::int32_t>((static_cast<long long>(sample) * volume) >> 15);
}

std::int16_t toS16(std::int32_t value)
{
    return static_cast<std::int16_t>(std::clamp(value, -32768, 32767));
}

} // namespace

Spu2Reverb::Spu2Reverb(std::uint8_t *ram, std::uint32_t ramSize) : m_ram(ram), m_ramSize(ramSize)
{
}

void Spu2Reverb::reset()
{
    for (Core &core : m_cores)
        core = Core{};
}

void Spu2Reverb::setPreset(int c, std::uint32_t requested, std::uint32_t eea)
{
    if (c < 0 || c >= kCores)
        return;
    const std::uint32_t mode = requested & kModeMask;
    if (mode >= static_cast<std::uint32_t>(kModes))
        return;
    Core &r = m_cores[c];

    // The same preset over the same area: the network keeps running (a depth ramp re-sends it each tick).
    const bool same = mode == r.mode && eea == r.eea && (r.buf != nullptr || mode == 0);
    if (!same)
    {
        r.mode = mode;
        r.eea = eea;
        r.buf = nullptr;
        r.samples = 0;
        r.cursor = 0;
        const std::uint32_t size = kSizes[mode];
        // EEA is the area's last byte. A mode with no network, or an EEA with no room below it, leaves the
        // core inactive rather than running over sample data.
        if (mode != 0 && eea < m_ramSize && eea + 1 >= size)
        {
            r.buf = reinterpret_cast<std::int16_t *>(m_ram + (eea + 1 - size));
            r.samples = size / sizeof(std::int16_t);
        }
        for (int i = 0; i < kRegisters; i++)
            r.v[i] = static_cast<std::int16_t>(kPresets[mode][i]);
        // The address registers count 8 bytes: 4 samples.
        for (int i = mLSAME; i <= mRAPF2; i++)
            r.v[i] = static_cast<std::int32_t>(kPresets[mode][i]) * 4;
        r.v[dAPF1] = static_cast<std::int32_t>(kPresets[mode][dAPF1]) * 4;
        r.v[dAPF2] = static_cast<std::int32_t>(kPresets[mode][dAPF2]) * 4;

        static bool logged[kCores] = {};
        if (mode != 0 && !logged[c])
        {
            logged[c] = true;
            log::info("spu2: reverb on core {}: {} (mode {}), work area {:#x}..{:#x}", c, kModeNames[mode], mode,
                      eea + 1 - size, eea);
        }
    }
    if ((requested & kModeClearWorkArea) != 0)
        clearWorkArea(c);
}

void Spu2Reverb::setDepth(int c, std::uint16_t left, std::uint16_t right)
{
    if (c < 0 || c >= kCores)
        return;
    m_cores[c].evolL = static_cast<std::int16_t>(left);
    m_cores[c].evolR = static_cast<std::int16_t>(right);
}

void Spu2Reverb::setEnable(int c, bool on)
{
    if (c < 0 || c >= kCores)
        return;
    m_cores[c].enabled = on;
}

void Spu2Reverb::clearWorkArea(int c)
{
    if (c < 0 || c >= kCores)
        return;
    Core &r = m_cores[c];
    if (r.buf != nullptr)
        std::memset(r.buf, 0, r.samples * sizeof(std::int16_t));
    r.cursor = 0;
}

bool Spu2Reverb::active(int c) const
{
    if (c < 0 || c >= kCores)
        return false;
    const Core &r = m_cores[c];
    return r.enabled && r.buf != nullptr && r.samples > 0 && (r.evolL != 0 || r.evolR != 0);
}

// Offsets are in samples and may be negative (psx-spx's "-2" byte steps, mLAPF1 - dAPF1).
std::uint32_t Spu2Reverb::wrap(const Core &r, std::int32_t offset) const
{
    std::int32_t at = (static_cast<std::int32_t>(r.cursor) + offset) % static_cast<std::int32_t>(r.samples);
    if (at < 0)
        at += static_cast<std::int32_t>(r.samples);
    return static_cast<std::uint32_t>(at);
}

std::int32_t Spu2Reverb::read(const Core &r, std::int32_t offset) const
{
    return r.buf[wrap(r, offset)];
}

void Spu2Reverb::write(Core &r, std::int32_t offset, std::int32_t value)
{
    r.buf[wrap(r, offset)] = toS16(value);
}

void Spu2Reverb::tick(int c, std::int32_t inL, std::int32_t inR, std::int32_t &outL, std::int32_t &outR)
{
    outL = 0;
    outR = 0;
    if (c < 0 || c >= kCores)
        return;
    Core &r = m_cores[c];
    if (!r.enabled || r.buf == nullptr || r.samples == 0)
        return;
    const std::int32_t *v = r.v;

    // The send bus sums every routed voice; clamped as the hardware's mixer does.
    const std::int32_t lin = mulVol(toS16(inL), v[vLIN]);
    const std::int32_t rin = mulVol(toS16(inR), v[vRIN]);

    // Same-side reflection (the -1 is psx-spx's [mLSAME-2], one sample back).
    {
        const std::int32_t prevL = read(r, v[mLSAME] - 1);
        const std::int32_t prevR = read(r, v[mRSAME] - 1);
        write(r, v[mLSAME], mulVol(lin + mulVol(read(r, v[dLSAME]), v[vWALL]) - prevL, v[vIIR]) + prevL);
        write(r, v[mRSAME], mulVol(rin + mulVol(read(r, v[dRSAME]), v[vWALL]) - prevR, v[vIIR]) + prevR);
    }
    // Cross-side reflection: the left store from the right delay and the other way round.
    {
        const std::int32_t prevL = read(r, v[mLDIFF] - 1);
        const std::int32_t prevR = read(r, v[mRDIFF] - 1);
        write(r, v[mLDIFF], mulVol(lin + mulVol(read(r, v[dRDIFF]), v[vWALL]) - prevL, v[vIIR]) + prevL);
        write(r, v[mRDIFF], mulVol(rin + mulVol(read(r, v[dLDIFF]), v[vWALL]) - prevR, v[vIIR]) + prevR);
    }
    // Early echo.
    std::int32_t lout = mulVol(read(r, v[mLCOMB1]), v[vCOMB1]) + mulVol(read(r, v[mLCOMB2]), v[vCOMB2]) +
                        mulVol(read(r, v[mLCOMB3]), v[vCOMB3]) + mulVol(read(r, v[mLCOMB4]), v[vCOMB4]);
    std::int32_t rout = mulVol(read(r, v[mRCOMB1]), v[vCOMB1]) + mulVol(read(r, v[mRCOMB2]), v[vCOMB2]) +
                        mulVol(read(r, v[mRCOMB3]), v[vCOMB3]) + mulVol(read(r, v[mRCOMB4]), v[vCOMB4]);
    // Two all-pass sections, each reading its delayed tap once, before its write.
    {
        const std::int32_t tapL = read(r, v[mLAPF1] - v[dAPF1]);
        const std::int32_t tapR = read(r, v[mRAPF1] - v[dAPF1]);
        lout -= mulVol(tapL, v[vAPF1]);
        rout -= mulVol(tapR, v[vAPF1]);
        write(r, v[mLAPF1], lout);
        write(r, v[mRAPF1], rout);
        lout = mulVol(lout, v[vAPF1]) + tapL;
        rout = mulVol(rout, v[vAPF1]) + tapR;
    }
    {
        const std::int32_t tapL = read(r, v[mLAPF2] - v[dAPF2]);
        const std::int32_t tapR = read(r, v[mRAPF2] - v[dAPF2]);
        lout -= mulVol(tapL, v[vAPF2]);
        rout -= mulVol(tapR, v[vAPF2]);
        write(r, v[mLAPF2], lout);
        write(r, v[mRAPF2], rout);
        lout = mulVol(lout, v[vAPF2]) + tapL;
        rout = mulVol(rout, v[vAPF2]) + tapR;
    }
    // EVOL: the return level.
    outL = mulVol(lout, r.evolL);
    outR = mulVol(rout, r.evolR);

    if (++r.cursor >= r.samples)
        r.cursor = 0;
}

} // namespace yamipan::iop
