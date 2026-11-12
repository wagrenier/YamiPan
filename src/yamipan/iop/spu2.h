// The SPU2 as libsd shows it to RXSNDDRV: two cores of 24 voices, their registers, 2 MB of sound RAM and
// the transfers into it. RosePan's sdk/iop_libsd.cpp as a class: it keeps the registers and answers the
// driver's reads; the voices themselves are Spu2Voices, the synthesiser, which these registers drive and
// which reports back where each voice reads (NAX), its envelope (ENVX), its end flags (ENDX) and how far
// the auto-DMA ring has been heard.
//
//   - Transfers complete. sceSdVoiceTrans copies between IOP memory and sound RAM and then runs the
//     core's transfer handler, on the caller's thread before it returns (the DMA interrupt, early): the
//     driver's DMA thread starts one transfer and parks until that handler sets its flag.
//   - The auto-DMA ring (sceSdBlockTrans, the driver's PCM player) plays through the synthesiser, and its
//     status is the address heard so far.
//   - Not modelled: the SPU2 interrupt at IRQA (RXSNDDRV installs no handler; its ADPCM streams poll NAX),
//     the auto-DMA's half-buffer interrupts (the ring's core has no transfer handler).
//
// Its registers are touched only under the IOP's big kernel lock (every libsd call comes from IOP code);
// the synthesiser has a lock of its own for its mixer thread.
#pragma once

#include <cstdint>
#include <memory>

#include "yamipan/iop/spu2_voices.h"

namespace yamipan::iop
{

class Spu2
{
public:
    static constexpr int kCores = 2;
    static constexpr int kVoices = 24;
    static constexpr std::uint32_t kRamSize = 0x200000;

    // A transfer-complete or SPU2 interrupt handler, as libsd calls it: the core, then its argument.
    using Handler = int (*)(int core, void *arg);

    // The reverb attributes sceSdSetEffectAttr takes, as RXSNDDRV fills them.
    struct EffectAttr
    {
        std::uint32_t core;
        std::uint32_t mode;
        std::int16_t depthL;
        std::int16_t depthR;
        std::int32_t delay;
        std::int32_t feedback;
    };

    Spu2();
    ~Spu2();
    Spu2(const Spu2 &) = delete;
    Spu2 &operator=(const Spu2 &) = delete;

    // sceSdInit: every register back to 0, the voices silent. Sound RAM keeps its contents.
    void reset();

    // ---- registers, by libsd entry (bit 0 the core, bits 1..5 the voice, the rest the register) ------
    void setParam(std::uint16_t entry, std::uint16_t value);
    std::uint16_t param(std::uint16_t entry);
    void setSwitch(std::uint16_t entry, std::uint32_t value);
    std::uint32_t switchBits(std::uint16_t entry);
    void setAddr(std::uint16_t entry, std::uint32_t value);
    std::uint32_t addr(std::uint16_t entry);
    void setCoreAttr(std::uint16_t entry, std::uint16_t value);

    // ---- transfers --------------------------------------------------------------------------------
    // `mode` bit 0: 0 writes IOP memory into sound RAM, 1 reads it back.
    int voiceTrans(int channel, std::uint32_t mode, std::uint8_t *iop, std::uint32_t spu, std::uint32_t size);
    int voiceTransStatus(int channel, int flag) const;
    int blockTrans(int channel, std::uint16_t mode, std::uint8_t *iop, std::uint32_t size, std::uint32_t start);
    int blockTransStatus(int channel, int flag);
    Handler setTransHandler(int core, Handler handler, void *arg);
    Handler setSpu2Handler(Handler handler, void *arg);

    // ---- reverb (Spu2Reverb, through the synthesiser) ---------------------------------------------
    int setEffectAttr(int core, const EffectAttr &attr);
    void clearEffectWorkArea(int core);

    // ---- the player's volume group of a voice (music, effects) --------------------------------------
    void setVoiceGroup(int core, int voice, int group);

    Spu2Voices &voices() { return *m_voices; }

    // Sound RAM at `spu`, or nullptr when [spu, spu + size) is not inside it.
    std::uint8_t *ram(std::uint32_t spu, std::uint32_t size);

private:
    struct Voice;
    struct Core;

    Voice *voiceAt(std::uint16_t entry);
    void pushMixMasks(int core);

    std::unique_ptr<Core[]> m_cores;
    std::unique_ptr<std::uint8_t[]> m_ram;
    std::unique_ptr<Spu2Voices> m_voices;
    Handler m_spu2Handler = nullptr;
    void *m_spu2Arg = nullptr;
};

} // namespace yamipan::iop
