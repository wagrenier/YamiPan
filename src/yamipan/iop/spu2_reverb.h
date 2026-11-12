// The SPU2's effect unit, one per core: the hardware's comb / all-pass reverb network, running at 24 kHz
// (half the output rate) out of a work area in sound RAM that ends at the core's SD_A_EEA. Fed by the
// voices whose VMIXEL / VMIXER bit is set, returned to the mix scaled by EVOLL / EVOLR. RosePan's
// sdk/iop_reverb.cpp as a class: the ten SDK presets (off, room, studio A / B / C, hall, space, echo,
// delay, pipe), their work-area sizes (RXSNDDRV's own table has the same ones), the network as psx-spx
// spells it.
//
// RXSNDDRV re-sends the same preset on every step of a depth ramp (sceSdSetEffectAttr each tick), so a
// preset that is already set keeps its work area and position; the driver clears the area itself
// (zero-fill DMA) before it turns the effect on.
//
// Not thread-safe: Spu2Voices calls it under its own lock, from the IOP's setters and its mixer thread.
#pragma once

#include <cstdint>

namespace yamipan::iop
{

class Spu2Reverb
{
public:
    static constexpr int kCores = 2;
    // The network runs every other output frame.
    static constexpr int kRateDivider = 2;

    Spu2Reverb(std::uint8_t *ram, std::uint32_t ramSize);

    void reset();
    // sceSdSetEffectAttr's mode (bit 8, SD_REV_MODE_CLEAR_WA, also clears the area) with the core's EEA.
    void setPreset(int core, std::uint32_t mode, std::uint32_t eea);
    void setDepth(int core, std::uint16_t left, std::uint16_t right);
    void setEnable(int core, bool on);
    // sceSdClearEffectWorkArea: the core's area as it is set now.
    void clearWorkArea(int core);

    // Enabled, with a preset, a work area that fits and a return above 0: the mixer skips the wet path
    // of a core that is not.
    bool active(int core) const;
    // One 24 kHz step: the summed sends in (s16 scale, unclamped), the return out, already scaled by EVOL.
    void tick(int core, std::int32_t inL, std::int32_t inR, std::int32_t &outL, std::int32_t &outR);

private:
    static constexpr int kRegisters = 32;

    struct Core
    {
        bool enabled = false;
        std::uint32_t mode = 0;
        std::uint32_t eea = 0;
        std::int16_t *buf = nullptr; // into sound RAM; nullptr when EEA leaves no room
        std::uint32_t samples = 0;
        std::uint32_t cursor = 0;
        std::int16_t evolL = 0;
        std::int16_t evolR = 0;
        std::int32_t v[kRegisters] = {}; // volumes as Q15, addresses as sample offsets
    };

    std::uint32_t wrap(const Core &r, std::int32_t offset) const;
    std::int32_t read(const Core &r, std::int32_t offset) const;
    void write(Core &r, std::int32_t offset, std::int32_t value);

    std::uint8_t *m_ram;
    std::uint32_t m_ramSize;
    Core m_cores[kCores];
};

} // namespace yamipan::iop
