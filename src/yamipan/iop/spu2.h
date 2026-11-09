// The SPU2 as libsd shows it to RXSNDDRV: two cores of 24 voices, their registers, 2 MB of sound RAM and
// the transfers into it. A register model and a clock, not yet a synthesiser: nothing is heard, but what
// the driver reads back moves as it would on the console, because the driver waits on it.
//
//   - Transfers complete. sceSdVoiceTrans copies between IOP memory and sound RAM and then runs the
//     core's transfer handler, on the caller's thread before it returns (the DMA interrupt, early): the
//     driver's DMA thread starts one transfer and parks until that handler sets its flag.
//   - Voices play in time. A keyed-on voice walks its ADPCM blocks in sound RAM at its pitch (0x1000 is
//     48 kHz, 28 samples a 16-byte block), following the blocks' own flags: loop start sets LSAX (unless
//     the driver wrote LSAX itself), loop end jumps to LSAX and latches ENDX, loop end without repeat
//     ends the voice. NAX is that position, read when asked; an ended voice idles on LSAX and follows
//     later writes to it, which is how a stream's stop sequence finds its stop block. ENVX is full while
//     the voice plays and 0 once it has ended or been keyed off: the envelope (ADSR) comes with the voice
//     engine, so a key-off releases at once.
//   - The auto-DMA ring (sceSdBlockTrans) reads at 48 kHz stereo, 4 bytes a frame, and its status is
//     the address it has reached.
//
// RosePan's sdk/iop_libsd.cpp is the model: the same registers, the same answers, its LSAX latch and
// its ENDX on every loop end. What it forwarded to its voice engine and reverb is stored here until
// YamiPan has them; the clock above stands in for the engine's play position meanwhile.
//
// Touched only under the IOP's big kernel lock (every libsd call comes from IOP code), so it has no
// lock of its own; the handlers it runs are IOP code and run under that lock too.
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

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

    // ---- reverb (stored until the effect unit exists) ---------------------------------------------
    int setEffectAttr(int core, const EffectAttr &attr);

    // ---- the host mixer's volume group of a voice (BGM, SE, voice), kept for the voice engine -------
    void setVoiceGroup(int core, int voice, int group);

    // Sound RAM at `spu`, or nullptr when [spu, spu + size) is not inside it.
    std::uint8_t *ram(std::uint32_t spu, std::uint32_t size);

private:
    using Clock = std::chrono::steady_clock;
    struct Voice;
    struct Core;

    Voice *voiceAt(std::uint16_t entry);
    // Plays `voice` forward to now at its pitch, block by block, following the blocks' flags.
    void advance(Core &core, int number, Clock::time_point now);
    void advanceCore(Core &core, Clock::time_point now);
    std::uint32_t blockTransPosition(int core, Clock::time_point now) const;

    std::unique_ptr<Core[]> m_cores;
    std::unique_ptr<std::uint8_t[]> m_ram;
    Handler m_spu2Handler = nullptr;
    void *m_spu2Arg = nullptr;
};

} // namespace yamipan::iop
