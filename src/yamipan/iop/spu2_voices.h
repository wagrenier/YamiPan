// The SPU2's synthesiser: its 48 voices read ADPCM out of sound RAM, decode it, step it at their pitch
// (4-point interpolation), run the ADSR envelope, and are mixed with the auto-DMA ring (the PCM input)
// into one 48 kHz stereo stream for the sound card. RosePan's sdk/iop_voice.cpp as a class; Spu2 drives
// it from libsd's register writes and reads back what the hardware would show: the play position (NAX),
// the envelope (ENVX), the end flags (ENDX) and how far the PCM ring has been heard.
//
// A mixer thread tops the device up to about 42 ms ahead, a 256-frame block at a time. With no device it
// keeps the same pace by the wall clock and discards the mix, so voices still end and the driver's waits
// still resolve.
//
// The voices whose VMIXEL / VMIXER bit is set also feed their core's reverb (Spu2Reverb), whose return
// joins the dry mix before the master volume. A voice whose NON bit is set plays its core's noise
// generator instead of its samples (which still run: its loops, end and envelope are its sample's). Not
// yet: pitch modulation (PMON). The SPU2's gaussian interpolation is RosePan's Catmull-Rom stand-in.
//
// Its own lock guards every voice: register writes come from IOP code (under the big kernel lock), the
// mixer runs on its own thread, and sound RAM is read without a lock (a transfer racing a voice reads
// bytes that are being replaced, as on the console).
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>

#include "yamipan/iop/spu2_reverb.h"

namespace yamipan::audio
{
class AudioOutput;
}

namespace yamipan::os
{
class Thread;
}

namespace yamipan::iop
{

class Spu2Voices
{
public:
    static constexpr int kCores = 2;
    static constexpr int kVoicesPerCore = 24;
    static constexpr int kSampleRate = 48000;

    // The player's volume groups of a voice (src/sdk/iop_voice.h): music, effects.
    static constexpr int kGroups = 2;

    // The reverb writes its work areas into `ram`.
    Spu2Voices(std::uint8_t *ram, std::uint32_t ramSize);
    ~Spu2Voices();
    Spu2Voices(const Spu2Voices &) = delete;
    Spu2Voices &operator=(const Spu2Voices &) = delete;

    // Opens the sound card and starts the mixer, once.
    void start();
    // sceSdInit: every voice silent and back to its defaults, the masks to the hardware's.
    void reset();

    // ---- register writes -----------------------------------------------------------------------------
    void keyOn(int core, std::uint32_t mask);
    void keyOff(int core, std::uint32_t mask);
    void setStartAddr(int core, int voice, std::uint32_t addr);
    // A write owns LSAX from then on: the blocks' loop-start flag no longer moves it.
    void setLoopAddr(int core, int voice, std::uint32_t addr);
    void setNowAddr(int core, int voice, std::uint32_t addr);
    // `reg` is the SD_VP_* index: 0 VOLL, 1 VOLR, 2 PITCH, 3 ADSR1, 4 ADSR2.
    void setParam(int core, int voice, int reg, std::uint16_t value);
    void setMasterVolume(int core, std::uint16_t left, std::uint16_t right);
    void setMixMasks(int core, std::uint32_t dryLeft, std::uint32_t dryRight, std::uint32_t wetLeft,
                     std::uint32_t wetRight);
    void setGroup(int core, int voice, int group);
    // SD_S_NON: the voices that play noise; SD_C_NOISE_CLK: the generator's clock, 0..0x3f.
    void setNoiseMask(int core, std::uint32_t mask);
    void setNoiseClock(int core, int clock);

    // ---- the effect unit (Spu2Reverb) ---------------------------------------------------------------
    void setReverbPreset(int core, std::uint32_t mode, std::uint32_t eea);
    void setReverbDepth(int core, std::uint16_t left, std::uint16_t right);
    void setReverbEnable(int core, bool on);
    void clearReverb(int core);

    // ---- what the hardware shows --------------------------------------------------------------------
    std::uint32_t startAddr(int core, int voice);
    std::uint32_t loopAddr(int core, int voice);
    // Where the voice reads while it plays; once it has ended, its loop address (and later writes to it).
    std::uint32_t nowAddr(int core, int voice);
    // 0..0x7fff; 0 once the voice has ended or released.
    std::uint16_t envx(int core, int voice);
    std::uint32_t endx(int core);

    // ---- the auto-DMA ring: 16-bit stereo PCM out of IOP memory, 512-byte blocks left then right -----
    void autoDmaStart(int core, const std::uint8_t *ring, std::uint32_t size, std::uint32_t startOffset);
    void autoDmaStop(int core);
    // Bytes into the ring that have been heard (mixed and no longer queued on the device).
    std::uint32_t autoDmaOffset(int core);
    void autoDmaSetVolume(int core, std::uint16_t left, std::uint16_t right);

private:
    struct Voice;
    struct AutoDma;
    struct Core;
    struct Meter;

    Voice *voiceAt(int core, int voice);
    void mixerLoop();
    void mixBlock(std::int16_t *out);
    void mixVoices();
    void mixAutoDma();
    void mixReverb();
    void runNoise();

    void keyOnLocked(Voice &v);
    void keyOffLocked(Voice &v);
    void beginEnd(Voice &v);
    bool decodeBlock(Voice &v);
    bool nextSourceSample(Voice &v, std::int16_t &out);
    bool nextOutputSample(Voice &v, std::int32_t &out);
    void readHeader(Voice &v);

    std::uint8_t *m_ram;
    std::uint32_t m_ramSize;

    std::mutex m_lock;
    std::unique_ptr<Voice[]> m_voices;
    std::unique_ptr<Core[]> m_cores;
    std::unique_ptr<Meter> m_meter;
    std::unique_ptr<Spu2Reverb> m_reverb;
    std::unique_ptr<audio::AudioOutput> m_output;
    std::unique_ptr<os::Thread> m_thread;
    bool m_started = false;
};

} // namespace yamipan::iop
