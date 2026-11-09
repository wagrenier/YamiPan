#include "yamipan/iop/spu2_voices.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "yamipan/audio/audio_output.h"
#include "yamipan/core/logger.h"
#include "yamipan/os/thread.h"

namespace yamipan::iop
{

namespace
{

// An ADPCM block: a 2-byte header (shift, filter, loop flags), then 14 bytes of nibbles: 28 samples.
constexpr std::uint32_t kBlockBytes = 16;
constexpr int kBlockSamples = 28;
constexpr std::uint16_t kFlagLoopEnd = 1 << 8;
constexpr std::uint16_t kFlagRepeat = 1 << 9;
constexpr std::uint16_t kFlagLoopStart = 1 << 10;

// One mixed block, and how far ahead of the speaker the mixer keeps (8 blocks, ~42 ms), at most 8 blocks a
// pass so a stall is made up over a few passes.
constexpr int kMixFrames = 256;
constexpr int kTargetBytes = 8 * kMixFrames * 2 * static_cast<int>(sizeof(std::int16_t));
constexpr int kMaxBlocksPerPass = 8;

// The envelope: Q16 over the SPU2's 0..0x7fff.
constexpr std::int32_t kLevelMax = (1 << 15) - 1;
constexpr int kFpShift = 16;
constexpr std::int32_t kLevelMaxFp = static_cast<std::int32_t>(static_cast<long long>(kLevelMax) << kFpShift);
constexpr std::int32_t kMinStepFp = 1 << 8;

// Pitch is 4.12: 0x1000 is one source sample per output sample (48 kHz).
constexpr std::uint32_t kPitchOne = 0x1000;

// The auto-DMA ring: stereo in alternating 512-byte blocks, left first (256 samples a channel).
constexpr std::uint32_t kAutoDmaBlock = 512;
constexpr std::uint32_t kAutoDmaFrame = kAutoDmaBlock * 2;
constexpr int kAutoDmaSamples = static_cast<int>(kAutoDmaBlock / 2);

constexpr std::uint32_t kAllVoices = (1u << Spu2Voices::kVoicesPerCore) - 1;
// The master volume until the driver writes one (the value RXSNDDRV's defaults write too).
constexpr std::uint16_t kMasterDefault = 0x3fff;

// The first three 10-second windows of mixing are reported, to show in a log that sound is made.
constexpr std::uint64_t kMeterWindowFrames = 10 * Spu2Voices::kSampleRate;
constexpr int kMeterReports = 3;

enum class Adsr
{
    Off,
    Attack,
    Decay,
    Sustain,
    Release,
};

// The SPU's ADPCM prediction filters; ids above 4 read as 0.
constexpr std::int32_t kFilter[16][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};

std::int16_t toS16(std::int32_t value)
{
    return static_cast<std::int16_t>(std::clamp(value, -32768, 32767));
}

void decodeAdpcm(std::int16_t *out, const std::uint8_t *block, std::int32_t &prev1, std::int32_t &prev2)
{
    const std::int32_t header = block[0] | (block[1] << 8);
    const int shift = (header & 0x0f) + 16;
    const int id = (header >> 4) & 0x0f;
    const std::int32_t pred1 = kFilter[id][0];
    const std::int32_t pred2 = kFilter[id][1];

    for (std::uint32_t i = 2; i < kBlockBytes; i++)
    {
        // The low nibble first, each sign-extended from the top of a word.
        const auto low = static_cast<std::int32_t>(static_cast<std::uint32_t>(block[i]) << 28);
        std::int32_t pcm = (low >> shift) + ((pred1 * prev1 + pred2 * prev2 + 32) >> 6);
        pcm = std::clamp(pcm, -0x8000, 0x7fff);
        *out++ = static_cast<std::int16_t>(pcm);

        const auto high = static_cast<std::int32_t>((static_cast<std::uint32_t>(block[i]) << 24) & 0xf0000000u);
        std::int32_t pcm2 = (high >> shift) + ((pred1 * pcm + pred2 * prev1 + 32) >> 6);
        pcm2 = std::clamp(pcm2, -0x8000, 0x7fff);
        *out++ = static_cast<std::int16_t>(pcm2);

        prev2 = pcm;
        prev1 = pcm2;
    }
}

// 4-point Catmull-Rom between p[1] and p[2] (the SPU2's gaussian table is in its ROM, not derivable).
std::int32_t interpolate(const std::int16_t *p, std::uint32_t frac)
{
    const float t = static_cast<float>(frac) * (1.0f / static_cast<float>(kPitchOne));
    const float a0 = p[0];
    const float a1 = p[1];
    const float a2 = p[2];
    const float a3 = p[3];
    const float c1 = 0.5f * (a2 - a0);
    const float c2 = a0 - 2.5f * a1 + 2.0f * a2 - 0.5f * a3;
    const float c3 = 0.5f * (a3 - a0) + 1.5f * (a1 - a2);
    return static_cast<std::int32_t>(((c3 * t + c2) * t + c1) * t + a1);
}

// A volume word with bit 15 set is a sweep, not a level; the driver writes plain levels.
std::int32_t decodeVolume(std::uint16_t value)
{
    return (value & 0x8000) != 0 ? 0 : static_cast<std::int32_t>(value & 0x7fff);
}

std::int32_t applyEnvelope(std::int32_t volume, std::int32_t envelope)
{
    return static_cast<std::int32_t>(static_cast<long long>(volume) * envelope / kLevelMax);
}

std::int32_t applyVolume(std::int32_t sample, std::int32_t volume)
{
    return static_cast<std::int32_t>((static_cast<long long>(sample) * volume) >> 15);
}

std::int32_t clampLevel(long long level)
{
    return static_cast<std::int32_t>(std::clamp<long long>(level, 0, kLevelMaxFp));
}

// A rate register to a step per output sample: rate 0 is the fastest, `fastestSamples` long.
std::int32_t rateStep(int rate, int maxRate, int fastestSamples)
{
    const long long fastness = maxRate - rate + 1;
    const long long step = fastness * fastness * kLevelMaxFp /
                           (static_cast<long long>(maxRate + 1) * (maxRate + 1) * fastestSamples);
    return static_cast<std::int32_t>(std::max<long long>(step, kMinStepFp));
}

std::int32_t curvedStep(std::int32_t base, std::int32_t level, bool increasing)
{
    const std::int32_t curve = increasing ? kLevelMaxFp - level : level;
    const long long step = static_cast<long long>(base) * curve / kLevelMaxFp;
    return static_cast<std::int32_t>(std::max<long long>(step, kMinStepFp));
}

} // namespace

struct Spu2Voices::Voice
{
    int core = 0;
    int number = 0;
    bool playing = false;

    std::uint32_t ssa = 0;
    std::uint32_t lsa = 0;
    bool lsaWritten = false; // RosePan's latch: a write to LSAX keeps the loop-start flag off it
    std::uint32_t nax = 0;
    std::uint16_t header = 0;

    std::int32_t hist1 = 0;
    std::int32_t hist2 = 0;
    std::int16_t block[kBlockSamples] = {};
    int blockPos = kBlockSamples; // == kBlockSamples: spent

    std::int16_t interp[4] = {}; // the resampler's window, interp[3] the newest
    std::uint32_t phase = 0;

    std::uint16_t volL = 0;
    std::uint16_t volR = 0;
    std::uint16_t pitch = 0;
    std::uint16_t adsr1 = 0;
    std::uint16_t adsr2 = 0;
    std::int32_t level = 0;
    Adsr adsr = Adsr::Off;

    int group = 0;

    bool adsrSet() const { return adsr1 != 0 || adsr2 != 0; }
    int attackRate() const { return (adsr1 >> 8) & 0x7f; }
    bool attackExp() const { return (adsr1 & 0x8000) != 0; }
    int decayRate() const { return (adsr1 >> 4) & 0x0f; }
    int releaseRate() const { return adsr2 & 0x1f; }
    bool releaseExp() const { return (adsr2 & 0x20) != 0; }
    int sustainRate() const { return (adsr2 >> 6) & 0x7f; }
    bool sustainDecreases() const { return (adsr2 & 0x2000) != 0; }
    int sustainMode() const { return (adsr2 >> 14) & 0x03; }
    std::int32_t sustainLevel() const
    {
        return static_cast<std::int32_t>(static_cast<long long>((adsr1 & 0x0f) + 1) * kLevelMaxFp / 16);
    }
    bool sounding() const { return playing || blockPos < kBlockSamples; }

    // The level for this output sample, then one step of the envelope (per output sample, as the
    // hardware runs it). `ended` turns true when the envelope has run out.
    std::int32_t envelope(bool &ended);
};

struct Spu2Voices::AutoDma
{
    const std::uint8_t *ring = nullptr;
    std::uint32_t size = 0;
    std::uint32_t readOff = 0; // the frame being read
    int framePos = 0;          // the sample within it
    std::uint32_t consumed = 0;
    bool armed = false;
    std::uint16_t volL = 0;
    std::uint16_t volR = 0;
};

struct Spu2Voices::Core
{
    std::uint32_t dryL = kAllVoices;
    std::uint32_t dryR = kAllVoices;
    std::uint32_t wetL = 0;
    std::uint32_t wetR = 0;
    std::uint16_t masterL = kMasterDefault;
    std::uint16_t masterR = kMasterDefault;
    std::uint32_t endx = 0;
    AutoDma autoDma;
    std::int32_t accL[kMixFrames] = {}; // dry
    std::int32_t accR[kMixFrames] = {};
    std::int32_t sendL[kMixFrames] = {}; // to the reverb
    std::int32_t sendR[kMixFrames] = {};
    std::int32_t returnL[kMixFrames] = {}; // from it
    std::int32_t returnR[kMixFrames] = {};
};

struct Spu2Voices::Meter
{
    std::uint64_t frames = 0;
    std::uint64_t windowFrames = 0;
    int peak = 0;
    int reverbPeak = 0;
    int keyOns = 0;
    int reports = 0;
};

Spu2Voices::Spu2Voices(std::uint8_t *ram, std::uint32_t ramSize)
    : m_ram(ram), m_ramSize(ramSize), m_voices(std::make_unique<Voice[]>(kCores * kVoicesPerCore)),
      m_cores(std::make_unique<Core[]>(kCores)), m_meter(std::make_unique<Meter>()),
      m_reverb(std::make_unique<Spu2Reverb>(ram, ramSize))
{
    reset();
}

Spu2Voices::~Spu2Voices() = default;

void Spu2Voices::reset()
{
    std::lock_guard lock(m_lock);
    for (int c = 0; c < kCores; c++)
    {
        m_cores[c] = Core{};
        for (int n = 0; n < kVoicesPerCore; n++)
        {
            Voice &v = m_voices[c * kVoicesPerCore + n];
            v = Voice{};
            v.core = c;
            v.number = n;
        }
    }
    m_reverb->reset();
}

void Spu2Voices::start()
{
    std::lock_guard lock(m_lock);
    if (m_started)
        return;
    m_started = true;
    m_output = audio::AudioOutput::open(kSampleRate);
    if (m_output != nullptr)
        log::info("audio: '{}', {} Hz stereo", m_output->deviceName(), kSampleRate);
    m_thread = std::make_unique<os::Thread>("YamiPanAudio", 256 * 1024, [this] { mixerLoop(); });
    m_thread->detach();
}

Spu2Voices::Voice *Spu2Voices::voiceAt(int core, int voice)
{
    if (core < 0 || core >= kCores || voice < 0 || voice >= kVoicesPerCore)
        return nullptr;
    return &m_voices[core * kVoicesPerCore + voice];
}

// ---- a voice ---------------------------------------------------------------------------------------

// The header of the block at NAX: its loop-start flag sets LSAX, unless the driver wrote LSAX itself
// (it swings a streamed voice between its buffers through LSAX, and the stream's blocks carry the flag).
void Spu2Voices::readHeader(Voice &v)
{
    const std::uint32_t at = v.nax & ~(kBlockBytes - 1);
    if (at + 2 > m_ramSize)
    {
        v.header = 0;
        return;
    }
    v.header = static_cast<std::uint16_t>(m_ram[at] | (m_ram[at + 1] << 8));
    if ((v.header & kFlagLoopStart) != 0 && !v.lsaWritten)
        v.lsa = at;
}

// The sample has run out: ENDX latches, and the voice idles on its loop address, following later writes
// to it (the stream's stop sequence points LSAX at a stop block and waits for NAX to get there).
void Spu2Voices::beginEnd(Voice &v)
{
    if (!v.playing)
        return;
    v.playing = false;
    v.nax = v.lsa;
    readHeader(v);
    m_cores[v.core].endx |= 1u << v.number;
}

bool Spu2Voices::decodeBlock(Voice &v)
{
    if (v.nax > m_ramSize - kBlockBytes)
    {
        v.playing = false;
        v.blockPos = kBlockSamples;
        return false;
    }
    decodeAdpcm(v.block, m_ram + v.nax, v.hist1, v.hist2);
    v.nax += kBlockBytes;

    if ((v.header & kFlagLoopEnd) != 0)
    {
        // ENDX latches on every loop end, repeating or not: RXSNDDRV parks a finished one-shot in a
        // self-repeating stop block, which must latch too.
        v.nax = v.lsa;
        m_cores[v.core].endx |= 1u << v.number;
        if ((v.header & kFlagRepeat) == 0)
            beginEnd(v);
    }
    readHeader(v);
    return true;
}

// The next source sample; a voice that ended mid-block plays the rest of that block out.
bool Spu2Voices::nextSourceSample(Voice &v, std::int16_t &out)
{
    if (v.blockPos >= kBlockSamples)
    {
        if (!v.playing || !decodeBlock(v))
            return false;
        v.blockPos = 0;
    }
    out = v.block[v.blockPos++];
    return true;
}

bool Spu2Voices::nextOutputSample(Voice &v, std::int32_t &out)
{
    // Pitch 0 would hold the voice on one sample for ever and nothing would retire it (RosePan's guard).
    const std::uint32_t step = v.pitch != 0 ? v.pitch : kPitchOne;
    while (v.phase >= kPitchOne)
    {
        std::int16_t s = 0;
        if (!nextSourceSample(v, s))
            return false;
        v.interp[0] = v.interp[1];
        v.interp[1] = v.interp[2];
        v.interp[2] = v.interp[3];
        v.interp[3] = s;
        v.phase -= kPitchOne;
    }
    out = interpolate(v.interp, v.phase);
    v.phase += step;
    return true;
}

void Spu2Voices::keyOnLocked(Voice &v)
{
    v.hist1 = 0;
    v.hist2 = 0;
    v.nax = v.ssa;
    v.blockPos = kBlockSamples;
    v.level = 0;
    v.adsr = Adsr::Attack;
    std::memset(v.interp, 0, sizeof v.interp);
    v.phase = 4 * kPitchOne; // four source samples prime the window before the first output sample
    v.playing = true;
    readHeader(v);
}

void Spu2Voices::keyOffLocked(Voice &v)
{
    if (!v.playing || !v.adsrSet())
    {
        v.playing = false;
        v.blockPos = kBlockSamples;
        v.level = 0;
        v.adsr = Adsr::Off;
        return;
    }
    if (v.level <= 0 || v.level > kLevelMaxFp)
        v.level = kLevelMaxFp;
    v.adsr = Adsr::Release;
}

// ---- the mixer -------------------------------------------------------------------------------------

void Spu2Voices::mixVoices()
{
    for (int i = 0; i < kCores * kVoicesPerCore; i++)
    {
        Voice &v = m_voices[i];
        if (!v.sounding())
            continue;
        Core &core = m_cores[v.core];
        const std::uint32_t bit = 1u << v.number;
        const std::int32_t volL = decodeVolume(v.volL);
        const std::int32_t volR = decodeVolume(v.volR);
        const bool dryL = (core.dryL & bit) != 0;
        const bool dryR = (core.dryR & bit) != 0;
        const bool wetL = (core.wetL & bit) != 0;
        const bool wetR = (core.wetR & bit) != 0;

        for (int f = 0; f < kMixFrames; f++)
        {
            std::int32_t s = 0;
            if (!nextOutputSample(v, s))
                break;
            bool ended = false;
            const std::int32_t env = v.envelope(ended);
            if (ended)
                beginEnd(v);
            const std::int32_t l = applyVolume(s, applyEnvelope(volL, env));
            const std::int32_t r = applyVolume(s, applyEnvelope(volR, env));
            if (dryL)
                core.accL[f] += l;
            if (dryR)
                core.accR[f] += r;
            if (wetL)
                core.sendL[f] += l;
            if (wetR)
                core.sendR[f] += r;
        }
    }
}

void Spu2Voices::mixAutoDma()
{
    for (int c = 0; c < kCores; c++)
    {
        AutoDma &ad = m_cores[c].autoDma;
        if (!ad.armed || ad.ring == nullptr || ad.size < kAutoDmaFrame)
            continue;
        const std::int32_t volL = ad.volL;
        const std::int32_t volR = ad.volR;
        for (int f = 0; f < kMixFrames; f++)
        {
            std::int16_t l = 0;
            std::int16_t r = 0;
            std::memcpy(&l, ad.ring + ad.readOff + ad.framePos * 2, sizeof l);
            std::memcpy(&r, ad.ring + ad.readOff + kAutoDmaBlock + ad.framePos * 2, sizeof r);
            m_cores[c].accL[f] += (l * volL) >> 15;
            m_cores[c].accR[f] += (r * volR) >> 15;
            if (++ad.framePos >= kAutoDmaSamples)
            {
                ad.framePos = 0;
                ad.readOff = (ad.readOff + kAutoDmaFrame) % ad.size;
                ad.consumed += kAutoDmaFrame;
            }
        }
    }
}

// The reverb at its half rate: each pair of sends averaged in (a two-tap box, a poor anti-alias filter
// but far better than dropping one), the return held over both frames of the pair. RosePan's; not the
// hardware's resamplers, whose images sit high in the band where the reverb has little energy.
void Spu2Voices::mixReverb()
{
    for (int c = 0; c < kCores; c++)
    {
        Core &core = m_cores[c];
        if (!m_reverb->active(c))
        {
            std::fill(std::begin(core.returnL), std::end(core.returnL), 0);
            std::fill(std::begin(core.returnR), std::end(core.returnR), 0);
            continue;
        }
        for (int f = 0; f < kMixFrames; f += Spu2Reverb::kRateDivider)
        {
            std::int32_t l = 0;
            std::int32_t r = 0;
            m_reverb->tick(c, (core.sendL[f] + core.sendL[f + 1]) / 2, (core.sendR[f] + core.sendR[f + 1]) / 2, l,
                           r);
            core.returnL[f] = core.returnL[f + 1] = l;
            core.returnR[f] = core.returnR[f + 1] = r;
            m_meter->reverbPeak = std::max({m_meter->reverbPeak, std::abs(l), std::abs(r)});
        }
    }
}

// Both cores' output summed, each through its master volume (the dry mix and the reverb's return). Core 0 feeding core 1 (AVOL, MMIX) is not
// modelled, as in RosePan: on the console core 0 also passes core 1's master.
void Spu2Voices::mixBlock(std::int16_t *out)
{
    for (int c = 0; c < kCores; c++)
    {
        Core &core = m_cores[c];
        std::fill(std::begin(core.accL), std::end(core.accL), 0);
        std::fill(std::begin(core.accR), std::end(core.accR), 0);
        std::fill(std::begin(core.sendL), std::end(core.sendL), 0);
        std::fill(std::begin(core.sendR), std::end(core.sendR), 0);
    }
    mixVoices();
    mixAutoDma();
    mixReverb();
    for (int f = 0; f < kMixFrames; f++)
    {
        std::int32_t l = 0;
        std::int32_t r = 0;
        for (int c = 0; c < kCores; c++)
        {
            l += applyVolume(m_cores[c].accL[f] + m_cores[c].returnL[f], decodeVolume(m_cores[c].masterL));
            r += applyVolume(m_cores[c].accR[f] + m_cores[c].returnR[f], decodeVolume(m_cores[c].masterR));
        }
        out[f * 2] = toS16(l);
        out[f * 2 + 1] = toS16(r);
        m_meter->peak = std::max({m_meter->peak, std::abs(static_cast<int>(out[f * 2])),
                                  std::abs(static_cast<int>(out[f * 2 + 1]))});
    }
}

void Spu2Voices::mixerLoop()
{
    std::int16_t block[kMixFrames * 2];
    const auto begin = std::chrono::steady_clock::now();
    for (;;)
    {
        {
            std::lock_guard lock(m_lock);
            for (int b = 0; b < kMaxBlocksPerPass; b++)
            {
                if (m_output != nullptr)
                {
                    if (m_output->queuedBytes() >= kTargetBytes)
                        break;
                }
                else
                {
                    // No device: the same pace by the wall clock, the same lead.
                    const double elapsed =
                        std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
                    if (static_cast<double>(m_meter->frames) >= elapsed * kSampleRate + 8 * kMixFrames)
                        break;
                }
                mixBlock(block);
                if (m_output != nullptr)
                    m_output->put(block, static_cast<int>(sizeof block));

                m_meter->frames += kMixFrames;
                m_meter->windowFrames += kMixFrames;
                if (m_meter->windowFrames >= kMeterWindowFrames && m_meter->reports < kMeterReports)
                {
                    const double db = m_meter->peak > 0 ? 20.0 * std::log10(m_meter->peak / 32768.0) : -999.0;
                    log::info("audio: {} s mixed, peak {} ({:.1f} dBFS), reverb return peak {}, {} key-ons",
                              m_meter->frames / kSampleRate, m_meter->peak, db, m_meter->reverbPeak,
                              m_meter->keyOns);
                    m_meter->reports++;
                    m_meter->windowFrames = 0;
                    m_meter->peak = 0;
                    m_meter->reverbPeak = 0;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

// ---- the envelope ----------------------------------------------------------------------------------

std::int32_t Spu2Voices::Voice::envelope(bool &ended)
{
    Voice &v = *this;
    if (!v.adsrSet())
    {
        v.level = kLevelMaxFp;
        v.adsr = Adsr::Sustain;
        return kLevelMax;
    }
    if (v.adsr == Adsr::Off)
    {
        v.level = 0;
        return 0;
    }
    v.level = clampLevel(v.level);
    const std::int32_t level = v.level >> kFpShift;
    std::int32_t step = 0;
    switch (v.adsr)
    {
    case Adsr::Attack:
        step = rateStep(v.attackRate(), 0x7f, 32);
        if (v.attackExp())
            step = curvedStep(step, v.level, true);
        v.level = clampLevel(static_cast<long long>(v.level) + step);
        if (v.level >= kLevelMaxFp)
        {
            v.level = kLevelMaxFp;
            v.adsr = Adsr::Decay;
        }
        break;
    case Adsr::Decay:
    {
        const std::int32_t sustain = v.sustainLevel();
        if (v.level <= sustain)
        {
            v.level = sustain;
            v.adsr = Adsr::Sustain;
            break;
        }
        step = curvedStep(rateStep(v.decayRate(), 0x0f, 64), v.level, false);
        if (v.level - sustain <= step)
        {
            v.level = sustain;
            v.adsr = Adsr::Sustain;
        }
        else
            v.level -= step;
        break;
    }
    case Adsr::Sustain:
        if (v.sustainMode() == 0)
            break;
        step = rateStep(v.sustainRate(), 0x7f, 64);
        if (v.sustainDecreases())
        {
            if (v.sustainMode() >= 2)
                step = curvedStep(step, v.level, false);
            v.level = clampLevel(static_cast<long long>(v.level) - step);
        }
        else
        {
            if (v.sustainMode() >= 2)
                step = curvedStep(step, v.level, true);
            v.level = clampLevel(static_cast<long long>(v.level) + step);
        }
        if (v.level == 0)
        {
            v.adsr = Adsr::Off;
            ended = true;
        }
        break;
    case Adsr::Release:
        step = rateStep(v.releaseRate(), 0x1f, 64);
        if (v.releaseExp())
            step = curvedStep(step, v.level, false);
        if (v.level <= step)
        {
            v.level = 0;
            v.adsr = Adsr::Off;
            ended = true;
        }
        else
            v.level -= step;
        break;
    case Adsr::Off:
        break;
    }
    return level;
}

// ---- register writes -------------------------------------------------------------------------------

void Spu2Voices::keyOn(int core, std::uint32_t mask)
{
    std::lock_guard lock(m_lock);
    for (int n = 0; n < kVoicesPerCore; n++)
    {
        if (((mask >> n) & 1) == 0)
            continue;
        if (Voice *v = voiceAt(core, n))
        {
            keyOnLocked(*v);
            m_cores[core].endx &= ~(1u << n); // RXSNDDRV drops the bit from its own copy on the same assumption
            m_meter->keyOns++;
        }
    }
}

void Spu2Voices::keyOff(int core, std::uint32_t mask)
{
    std::lock_guard lock(m_lock);
    for (int n = 0; n < kVoicesPerCore; n++)
    {
        if (((mask >> n) & 1) == 0)
            continue;
        if (Voice *v = voiceAt(core, n))
            keyOffLocked(*v);
    }
}

void Spu2Voices::setStartAddr(int core, int voice, std::uint32_t addr)
{
    std::lock_guard lock(m_lock);
    if (Voice *v = voiceAt(core, voice))
        v->ssa = addr;
}

void Spu2Voices::setLoopAddr(int core, int voice, std::uint32_t addr)
{
    std::lock_guard lock(m_lock);
    if (Voice *v = voiceAt(core, voice))
    {
        v->lsa = addr;
        v->lsaWritten = true;
    }
}

void Spu2Voices::setNowAddr(int core, int voice, std::uint32_t addr)
{
    std::lock_guard lock(m_lock);
    if (Voice *v = voiceAt(core, voice))
        v->nax = addr;
}

void Spu2Voices::setParam(int core, int voice, int reg, std::uint16_t value)
{
    std::lock_guard lock(m_lock);
    Voice *v = voiceAt(core, voice);
    if (v == nullptr)
        return;
    switch (reg)
    {
    case 0:
        v->volL = value;
        break;
    case 1:
        v->volR = value;
        break;
    case 2:
        v->pitch = value;
        break;
    case 3:
        v->adsr1 = value;
        break;
    case 4:
        v->adsr2 = value;
        break;
    default:
        break;
    }
}

void Spu2Voices::setMasterVolume(int core, std::uint16_t left, std::uint16_t right)
{
    if (core < 0 || core >= kCores)
        return;
    std::lock_guard lock(m_lock);
    m_cores[core].masterL = left;
    m_cores[core].masterR = right;
}

void Spu2Voices::setMixMasks(int core, std::uint32_t dryLeft, std::uint32_t dryRight, std::uint32_t wetLeft,
                             std::uint32_t wetRight)
{
    if (core < 0 || core >= kCores)
        return;
    std::lock_guard lock(m_lock);
    Core &c = m_cores[core];
    c.dryL = dryLeft;
    c.dryR = dryRight;
    c.wetL = wetLeft;
    c.wetR = wetRight;
}

void Spu2Voices::setGroup(int core, int voice, int group)
{
    if (group < 0 || group >= kGroups)
        return;
    std::lock_guard lock(m_lock);
    if (Voice *v = voiceAt(core, voice))
        v->group = group;
}

// ---- the effect unit ------------------------------------------------------------------------------

void Spu2Voices::setReverbPreset(int core, std::uint32_t mode, std::uint32_t eea)
{
    std::lock_guard lock(m_lock);
    m_reverb->setPreset(core, mode, eea);
}

void Spu2Voices::setReverbDepth(int core, std::uint16_t left, std::uint16_t right)
{
    std::lock_guard lock(m_lock);
    m_reverb->setDepth(core, left, right);
}

void Spu2Voices::setReverbEnable(int core, bool on)
{
    std::lock_guard lock(m_lock);
    m_reverb->setEnable(core, on);
}

void Spu2Voices::clearReverb(int core)
{
    std::lock_guard lock(m_lock);
    m_reverb->clearWorkArea(core);
}

// ---- what the hardware shows -----------------------------------------------------------------------

std::uint32_t Spu2Voices::startAddr(int core, int voice)
{
    std::lock_guard lock(m_lock);
    const Voice *v = voiceAt(core, voice);
    return v != nullptr ? v->ssa : 0;
}

std::uint32_t Spu2Voices::loopAddr(int core, int voice)
{
    std::lock_guard lock(m_lock);
    const Voice *v = voiceAt(core, voice);
    return v != nullptr ? v->lsa : 0;
}

std::uint32_t Spu2Voices::nowAddr(int core, int voice)
{
    std::lock_guard lock(m_lock);
    const Voice *v = voiceAt(core, voice);
    if (v == nullptr)
        return 0;
    return v->playing ? v->nax : v->lsa;
}

// 0 once the voice has ended (a loop end without repeat) or its release ran out; full when it plays
// without an envelope. A voice keyed on that the mixer has not reached yet is still at the attack's 0,
// where the SPU2 would be climbing: it reads 1 (RosePan: a stream's end check reads ENVX once).
std::uint16_t Spu2Voices::envx(int core, int voice)
{
    std::lock_guard lock(m_lock);
    const Voice *v = voiceAt(core, voice);
    if (v == nullptr || !v->playing)
        return 0;
    if (!v->adsrSet())
        return static_cast<std::uint16_t>(kLevelMax);
    std::uint16_t level = 0;
    if (v->adsr != Adsr::Off)
        level = static_cast<std::uint16_t>(clampLevel(v->level) >> kFpShift);
    if (level == 0 && v->adsr == Adsr::Attack)
        level = 1;
    return level;
}

std::uint32_t Spu2Voices::endx(int core)
{
    if (core < 0 || core >= kCores)
        return 0;
    std::lock_guard lock(m_lock);
    return m_cores[core].endx;
}

// ---- the auto-DMA ring -----------------------------------------------------------------------------

void Spu2Voices::autoDmaStart(int core, const std::uint8_t *ring, std::uint32_t size, std::uint32_t startOffset)
{
    if (core < 0 || core >= kCores || ring == nullptr || size < kAutoDmaFrame)
        return;
    std::lock_guard lock(m_lock);
    AutoDma &ad = m_cores[core].autoDma;
    ad.ring = ring;
    ad.size = size - size % kAutoDmaFrame;
    ad.readOff = startOffset % ad.size;
    ad.readOff -= ad.readOff % kAutoDmaFrame;
    ad.framePos = 0;
    ad.consumed = 0;
    ad.armed = true; // no volume of its own: the driver writes BVOLL / BVOLR before it arms the ring
}

void Spu2Voices::autoDmaStop(int core)
{
    if (core < 0 || core >= kCores)
        return;
    std::lock_guard lock(m_lock);
    m_cores[core].autoDma.armed = false;
}

// Heard = mixed, less what is still queued on the device: one output frame is four bytes of the device's
// queue and four bytes of the ring alike.
std::uint32_t Spu2Voices::autoDmaOffset(int core)
{
    if (core < 0 || core >= kCores)
        return 0;
    std::lock_guard lock(m_lock);
    const AutoDma &ad = m_cores[core].autoDma;
    if (ad.size == 0)
        return 0;
    const int queued = m_output != nullptr ? m_output->queuedBytes() : 0;
    const std::uint32_t behind = std::min(static_cast<std::uint32_t>(std::max(queued, 0)), ad.consumed);
    return (ad.readOff + ad.size - behind % ad.size) % ad.size;
}

void Spu2Voices::autoDmaSetVolume(int core, std::uint16_t left, std::uint16_t right)
{
    if (core < 0 || core >= kCores)
        return;
    std::lock_guard lock(m_lock);
    m_cores[core].autoDma.volL = left;
    m_cores[core].autoDma.volR = right;
}

} // namespace yamipan::iop
