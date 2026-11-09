#include "yamipan/iop/spu2.h"

#include <algorithm>
#include <cstring>

#include "yamipan/core/logger.h"

namespace yamipan::iop
{

namespace
{

// libsd's register numbers (src/sdk/libsd.h), the register field of an entry.
constexpr std::uint16_t kVpPitch = 0x0200;
constexpr std::uint16_t kVpEnvx = 0x0500;
constexpr std::uint16_t kPMmix = 0x0800;
constexpr std::uint16_t kPEvolL = 0x0b80;
constexpr std::uint16_t kPEvolR = 0x0c80;
constexpr std::uint16_t kSKon = 0x1500;
constexpr std::uint16_t kSKoff = 0x1600;
constexpr std::uint16_t kSEndx = 0x1700;
constexpr std::uint16_t kAIrqa = 0x1f00;
constexpr std::uint16_t kAEea = 0x1d00;
constexpr std::uint16_t kVaSsa = 0x2040;
constexpr std::uint16_t kVaLsax = 0x2140;
constexpr std::uint16_t kVaNax = 0x2240;

// sceSdBlockTrans's mode: the operation in the low bits, looping in 0x10.
constexpr std::uint16_t kBlockOp = 0x03;
constexpr std::uint16_t kBlockStop = 0x02;
constexpr std::uint16_t kBlockWriteFrom = 0x03;
constexpr std::uint16_t kBlockLoop = 0x10;

// An ADPCM block: 16 bytes, 28 samples; its second byte holds the loop flags.
constexpr std::uint32_t kBlockBytes = 16;
constexpr double kBlockSamples = 28.0;
constexpr std::uint8_t kFlagLoopEnd = 0x01;
constexpr std::uint8_t kFlagRepeat = 0x02;
constexpr std::uint8_t kFlagLoopStart = 0x04;

// Pitch 0x1000 plays at the SPU2's 48 kHz; the auto-DMA reads 48 kHz of 16-bit stereo.
constexpr double kOutputRate = 48000.0;
constexpr double kPitchUnity = 4096.0;
constexpr double kBlockTransBytesPerSecond = kOutputRate * 4.0;

// ENVX of a voice that plays: full scale, until the voice engine runs the envelope.
constexpr std::uint16_t kEnvxFull = 0x7fff;

// The most blocks one read plays: all of sound RAM. A voice left unread for longer than that skips the
// rest (it only matters to a voice nobody is watching).
constexpr int kMaxBlocksPerAdvance = static_cast<int>(Spu2::kRamSize / kBlockBytes);

int entryCore(std::uint16_t entry)
{
    return entry & 1;
}

int entryVoice(std::uint16_t entry)
{
    return (entry >> 1) & 0x1f;
}

// The register field is masked with ~0x3f, not 0xff00: the SD_VA_* numbers carry a 0x40 bit (RosePan
// lost every address write to that).
std::uint16_t entryRegister(std::uint16_t entry)
{
    return entry & 0xffc0;
}

int registerIndex(std::uint16_t reg)
{
    return (reg >> 8) & 0x1f;
}

} // namespace

struct Spu2::Voice
{
    std::uint32_t ssa = 0;
    std::uint32_t lsax = 0;
    std::uint32_t nax = 0;
    // A write to LSAX owns it from then on: the blocks' loop-start flag no longer moves it. RosePan's
    // latch: RXSNDDRV swings a streamed voice between its buffers through LSAX, and the stream's blocks
    // carry loop-start flags that would otherwise pull it back into the buffer that is playing.
    bool lsaxWritten = false;
    bool playing = false;
    std::uint16_t param[8] = {};
    int group = 0;
    Clock::time_point clock{};
    // Samples played into the block at `nax`.
    double samples = 0.0;
};

struct Spu2::Core
{
    Voice voice[kVoices];
    std::uint32_t endx = 0;
    std::uint32_t switches[0x20] = {};
    std::uint16_t param[0x20] = {};
    std::uint32_t irqa = 0;
    std::uint32_t eea = 0;
    std::uint16_t attr[0x80] = {};
    Handler transHandler = nullptr;
    void *transArg = nullptr;
    EffectAttr effect{};

    // The auto-DMA ring: where it is, how long, and where it was at `ringClock`.
    std::uint8_t *ring = nullptr;
    std::uint32_t ringSize = 0;
    std::uint32_t ringOffset = 0;
    bool ringLoops = false;
    bool ringRunning = false;
    Clock::time_point ringClock{};
};

Spu2::Spu2() : m_cores(std::make_unique<Core[]>(kCores)), m_ram(std::make_unique<std::uint8_t[]>(kRamSize))
{
    std::memset(m_ram.get(), 0, kRamSize);
}

Spu2::~Spu2() = default;

void Spu2::reset()
{
    for (int c = 0; c < kCores; c++)
        m_cores[c] = Core{};
    m_spu2Handler = nullptr;
    m_spu2Arg = nullptr;
}

std::uint8_t *Spu2::ram(std::uint32_t spu, std::uint32_t size)
{
    if (spu >= kRamSize || size > kRamSize - spu)
        return nullptr;
    return m_ram.get() + spu;
}

Spu2::Voice *Spu2::voiceAt(std::uint16_t entry)
{
    // Voice numbers past 23 name no voice: RXSNDDRV's defaults batch clears "voice 31" of both cores.
    if (entryVoice(entry) >= kVoices)
        return nullptr;
    return &m_cores[entryCore(entry)].voice[entryVoice(entry)];
}

// ---- the clock ------------------------------------------------------------------------------------

void Spu2::advance(Core &core, int number, Clock::time_point now)
{
    Voice &v = core.voice[number];
    const double elapsed = std::chrono::duration<double>(now - v.clock).count();
    v.clock = now;
    if (!v.playing || elapsed <= 0.0)
        return;

    v.samples += elapsed * kOutputRate * v.param[registerIndex(kVpPitch) & 7] / kPitchUnity;
    for (int blocks = 0; v.samples >= kBlockSamples && v.playing; blocks++)
    {
        if (blocks == kMaxBlocksPerAdvance)
        {
            v.samples = 0.0;
            break;
        }
        v.samples -= kBlockSamples;

        const std::uint32_t at = v.nax & ~(kBlockBytes - 1);
        const std::uint8_t *block = ram(at, kBlockBytes);
        if (block == nullptr)
        {
            v.playing = false; // off the end of sound RAM: nothing left to play
            break;
        }
        const std::uint8_t flags = block[1];
        if ((flags & kFlagLoopStart) && !v.lsaxWritten)
            v.lsax = at;
        v.nax = at + kBlockBytes;
        if (flags & kFlagLoopEnd)
        {
            // ENDX latches on every loop end, repeating or not (RosePan: RXSNDDRV parks finished one-shots
            // in a self-repeating stop block, which must still latch).
            v.nax = v.lsax;
            core.endx |= 1u << number;
            if (!(flags & kFlagRepeat))
                v.playing = false;
            else if (v.lsax == at)
            {
                v.samples = 0.0; // a block that repeats itself: the position stays, the rest is the same
                break;
            }
        }
    }
}

void Spu2::advanceCore(Core &core, Clock::time_point now)
{
    for (int v = 0; v < kVoices; v++)
        advance(core, v, now);
}

// ---- parameters -----------------------------------------------------------------------------------

void Spu2::setParam(std::uint16_t entry, std::uint16_t value)
{
    Core &core = m_cores[entryCore(entry)];
    const std::uint16_t reg = entryRegister(entry);
    if (reg < kPMmix)
    {
        Voice *v = voiceAt(entry);
        if (v == nullptr)
            return;
        advance(core, entryVoice(entry), Clock::now()); // a new pitch counts from now
        v->param[registerIndex(reg) & 7] = value;
        return;
    }
    core.param[registerIndex(reg)] = value;
}

std::uint16_t Spu2::param(std::uint16_t entry)
{
    Core &core = m_cores[entryCore(entry)];
    const std::uint16_t reg = entryRegister(entry);
    if (reg < kPMmix)
    {
        Voice *v = voiceAt(entry);
        if (v == nullptr)
            return 0;
        // ENVX is the envelope, never a stored write: RXSNDDRV frees an auto-free voice after 9 ticks of
        // 0 and ends a stream whose voice reads 0.
        if (reg == kVpEnvx)
        {
            advance(core, entryVoice(entry), Clock::now());
            return v->playing ? kEnvxFull : 0;
        }
        return v->param[registerIndex(reg) & 7];
    }
    return core.param[registerIndex(reg)];
}

// ---- switches -------------------------------------------------------------------------------------

void Spu2::setSwitch(std::uint16_t entry, std::uint32_t value)
{
    Core &core = m_cores[entryCore(entry)];
    const std::uint16_t reg = entryRegister(entry);
    core.switches[registerIndex(reg)] = value;

    if (reg != kSKon && reg != kSKoff)
        return;
    const auto now = Clock::now();
    for (int n = 0; n < kVoices; n++)
    {
        if (((value >> n) & 1) == 0)
            continue;
        advance(core, n, now);
        Voice &v = core.voice[n];
        if (reg == kSKon)
        {
            v.playing = true;
            v.nax = v.ssa;
            v.samples = 0.0;
            v.clock = now;
            core.endx &= ~(1u << n); // RXSNDDRV drops the bit from its own copy on the same assumption
        }
        else
            v.playing = false; // the release runs at once until the voice engine has an envelope
    }
}

std::uint32_t Spu2::switchBits(std::uint16_t entry)
{
    Core &core = m_cores[entryCore(entry)];
    if (entryRegister(entry) == kSEndx)
    {
        advanceCore(core, Clock::now());
        return core.endx;
    }
    return core.switches[registerIndex(entryRegister(entry))];
}

// ---- addresses ------------------------------------------------------------------------------------

void Spu2::setAddr(std::uint16_t entry, std::uint32_t value)
{
    Core &core = m_cores[entryCore(entry)];
    const std::uint16_t reg = entryRegister(entry);
    switch (reg)
    {
    case kAIrqa:
        core.irqa = value;
        return;
    case kAEea:
        core.eea = value; // the effect work area's last byte, written just before sceSdSetEffectAttr
        return;
    default:
        break;
    }

    Voice *v = voiceAt(entry);
    if (v == nullptr)
        return;
    advance(core, entryVoice(entry), Clock::now());
    switch (reg)
    {
    case kVaSsa:
        v->ssa = value;
        break;
    case kVaLsax:
        v->lsax = value;
        v->lsaxWritten = true;
        break;
    case kVaNax:
        v->nax = value;
        break;
    default:
        break;
    }
}

std::uint32_t Spu2::addr(std::uint16_t entry)
{
    Core &core = m_cores[entryCore(entry)];
    const std::uint16_t reg = entryRegister(entry);
    if (reg == kAIrqa)
        return core.irqa;
    if (reg == kAEea)
        return core.eea;

    Voice *v = voiceAt(entry);
    if (v == nullptr)
        return 0;
    switch (reg)
    {
    case kVaSsa:
        return v->ssa;
    case kVaLsax:
        return v->lsax;
    case kVaNax:
        // Playing: where it has got to. Ended: on LSAX, following later writes to it (the stream's stop
        // sequence ends the voice, then points LSAX at the stop block and waits for NAX to get there).
        advance(core, entryVoice(entry), Clock::now());
        return v->playing ? v->nax : v->lsax;
    default:
        return 0;
    }
}

void Spu2::setCoreAttr(std::uint16_t entry, std::uint16_t value)
{
    m_cores[entryCore(entry)].attr[(entry & 0xff) >> 1] = value;
}

// ---- transfers ------------------------------------------------------------------------------------

int Spu2::voiceTrans(int channel, std::uint32_t mode, std::uint8_t *iop, std::uint32_t spu, std::uint32_t size)
{
    const int c = channel & 1;
    std::uint8_t *sound = ram(spu, size);
    if (sound == nullptr || iop == nullptr)
    {
        static int reported = 0;
        if (reported < 8)
        {
            reported++;
            yamipan::log::warning("spu2: transfer core {} spu {:#x} size {:#x} iop {} dropped (outside sound RAM)",
                                  c, spu, size, static_cast<const void *>(iop));
        }
    }
    else if ((mode & 1) == 0)
        std::memcpy(sound, iop, size);
    else
        std::memcpy(iop, sound, size);

    // The transfer is done before the call returns, so its interrupt comes now, on this thread (under
    // the big kernel lock, as interrupt code ran with everything else stopped). RXSNDDRV ignores the answer.
    Core &core = m_cores[c];
    if (core.transHandler != nullptr)
        core.transHandler(c, core.transArg);
    return 0;
}

int Spu2::voiceTransStatus(int, int) const
{
    return 1; // finished: every transfer is, by the time anyone asks
}

std::uint32_t Spu2::blockTransPosition(int c, Clock::time_point now) const
{
    const Core &core = m_cores[c];
    if (!core.ringRunning || core.ringSize == 0)
        return core.ringOffset;

    const double elapsed = std::chrono::duration<double>(now - core.ringClock).count();
    std::uint64_t at = core.ringOffset + static_cast<std::uint64_t>(elapsed * kBlockTransBytesPerSecond);
    at &= ~std::uint64_t{3}; // a whole frame
    if (core.ringLoops)
        return static_cast<std::uint32_t>(at % core.ringSize);
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(at, core.ringSize));
}

// RXSNDDRV's PCM player is the only user: a looping ring on core 1, whose status it reads as a position.
// Its half-buffer interrupts are not raised: RXSNDDRV installs no transfer handler on core 1.
int Spu2::blockTrans(int channel, std::uint16_t mode, std::uint8_t *iop, std::uint32_t size, std::uint32_t start)
{
    const int c = channel & 1;
    Core &core = m_cores[c];
    const auto now = Clock::now();

    if ((mode & kBlockOp) == kBlockStop)
    {
        // Stop and say where: the address the ring had reached.
        core.ringOffset = blockTransPosition(c, now);
        core.ringRunning = false;
        return static_cast<int>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(core.ring)) +
                                core.ringOffset);
    }

    core.ring = iop;
    core.ringSize = size;
    core.ringLoops = (mode & kBlockLoop) != 0;
    core.ringOffset = 0;
    if ((mode & kBlockOp) == kBlockWriteFrom && iop != nullptr && size != 0)
        core.ringOffset = (start - static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(iop))) % size;
    core.ringRunning = iop != nullptr && size != 0;
    core.ringClock = now;
    return 0;
}

// Flag 0 asks where the auto-DMA reads, as an address (callers take their ring's base off it, on the
// same 32-bit truncation); any other flag asks whether a transfer is busy, and none ever is.
int Spu2::blockTransStatus(int channel, int flag)
{
    if (flag != 0)
        return 0;
    const int c = channel & 1;
    const Core &core = m_cores[c];
    return static_cast<int>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(core.ring)) +
                            blockTransPosition(c, Clock::now()));
}

Spu2::Handler Spu2::setTransHandler(int c, Handler handler, void *arg)
{
    if (c < 0 || c >= kCores)
        return nullptr;
    Core &core = m_cores[c];
    const Handler old = core.transHandler;
    core.transHandler = handler;
    core.transArg = arg;
    return old;
}

// Stored only: the SPU2 interrupt is IRQA reached, which needs the voice engine's play position at sample
// grain, and RXSNDDRV installs no handler (its ADPCM streams poll NAX instead).
Spu2::Handler Spu2::setSpu2Handler(Handler handler, void *arg)
{
    const Handler old = m_spu2Handler;
    m_spu2Handler = handler;
    m_spu2Arg = arg;
    return old;
}

// ---- reverb ---------------------------------------------------------------------------------------

int Spu2::setEffectAttr(int c, const EffectAttr &attr)
{
    if (c < 0 || c >= kCores)
        return -1;
    Core &core = m_cores[c];
    core.effect = attr;
    // The depth is EVOLL / EVOLR: a same-mode change sets it through sceSdSetParam instead, and reads
    // answer alike either way.
    core.param[registerIndex(kPEvolL)] = static_cast<std::uint16_t>(attr.depthL);
    core.param[registerIndex(kPEvolR)] = static_cast<std::uint16_t>(attr.depthR);
    return 0;
}

void Spu2::setVoiceGroup(int c, int voice, int group)
{
    if (c < 0 || c >= kCores || voice < 0 || voice >= kVoices)
        return;
    m_cores[c].voice[voice].group = group;
}

} // namespace yamipan::iop
