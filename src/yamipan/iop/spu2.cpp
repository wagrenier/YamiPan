#include "yamipan/iop/spu2.h"

#include <cstring>

#include "yamipan/core/logger.h"

namespace yamipan::iop
{

namespace
{

// libsd's register numbers (src/sdk/libsd.h), the register field of an entry.
constexpr std::uint16_t kVpEnvx = 0x0500;
constexpr std::uint16_t kPMmix = 0x0800;
constexpr std::uint16_t kPMvolL = 0x0980;
constexpr std::uint16_t kPMvolR = 0x0a80;
constexpr std::uint16_t kPEvolL = 0x0b80;
constexpr std::uint16_t kPEvolR = 0x0c80;
constexpr std::uint16_t kPBvolL = 0x0f80;
constexpr std::uint16_t kPBvolR = 0x1080;
constexpr std::uint16_t kSKon = 0x1500;
constexpr std::uint16_t kSKoff = 0x1600;
constexpr std::uint16_t kSEndx = 0x1700;
constexpr std::uint16_t kSVmixL = 0x1800;
constexpr std::uint16_t kSVmixEL = 0x1900;
constexpr std::uint16_t kSVmixR = 0x1a00;
constexpr std::uint16_t kSVmixER = 0x1b00;
constexpr std::uint16_t kAIrqa = 0x1f00;
constexpr std::uint16_t kAEea = 0x1d00;
constexpr std::uint16_t kVaSsa = 0x2040;
constexpr std::uint16_t kVaLsax = 0x2140;
constexpr std::uint16_t kVaNax = 0x2240;
constexpr std::uint16_t kCEffectEnable = 0x02;

// sceSdBlockTrans's mode: the operation in the low bits, looping in 0x10.
constexpr std::uint16_t kBlockOp = 0x03;
constexpr std::uint16_t kBlockStop = 0x02;
constexpr std::uint16_t kBlockWriteFrom = 0x03;

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

std::uint32_t low32(const void *p)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}

} // namespace

struct Spu2::Voice
{
    std::uint16_t param[8] = {};
};

struct Spu2::Core
{
    Voice voice[kVoices];
    std::uint32_t switches[0x20] = {};
    std::uint16_t param[0x20] = {};
    std::uint32_t irqa = 0;
    std::uint32_t eea = 0;
    std::uint16_t attr[0x80] = {};
    Handler transHandler = nullptr;
    void *transArg = nullptr;
    EffectAttr effect{};

    // The auto-DMA ring as armed, and where it stopped.
    std::uint8_t *ring = nullptr;
    bool ringRunning = false;
    std::uint32_t ringStopped = 0;
};

Spu2::Spu2()
    : m_cores(std::make_unique<Core[]>(kCores)), m_ram(std::make_unique<std::uint8_t[]>(kRamSize)),
      m_voices(std::make_unique<Spu2Voices>(m_ram.get(), kRamSize))
{
    std::memset(m_ram.get(), 0, kRamSize);
}

Spu2::~Spu2() = default;

// sceSdInit is also when the sound card opens: the driver's first call, at boot.
void Spu2::reset()
{
    for (int c = 0; c < kCores; c++)
        m_cores[c] = Core{};
    m_spu2Handler = nullptr;
    m_spu2Arg = nullptr;
    m_voices->reset();
    m_voices->start();
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

void Spu2::pushMixMasks(int c)
{
    const Core &core = m_cores[c];
    m_voices->setMixMasks(c, core.switches[registerIndex(kSVmixL)], core.switches[registerIndex(kSVmixR)],
                          core.switches[registerIndex(kSVmixEL)], core.switches[registerIndex(kSVmixER)]);
}

// ---- parameters -----------------------------------------------------------------------------------

void Spu2::setParam(std::uint16_t entry, std::uint16_t value)
{
    const int c = entryCore(entry);
    Core &core = m_cores[c];
    const std::uint16_t reg = entryRegister(entry);
    if (reg < kPMmix)
    {
        Voice *v = voiceAt(entry);
        if (v == nullptr)
            return;
        v->param[registerIndex(reg) & 7] = value;
        m_voices->setParam(c, entryVoice(entry), registerIndex(reg) & 7, value);
        return;
    }
    core.param[registerIndex(reg)] = value;
    if (reg == kPMvolL || reg == kPMvolR)
        m_voices->setMasterVolume(c, core.param[registerIndex(kPMvolL)], core.param[registerIndex(kPMvolR)]);
    // EVOL is the reverb's return level (sceSdSetEffectAttr's depth sets it too).
    if (reg == kPEvolL || reg == kPEvolR)
        m_voices->setReverbDepth(c, core.param[registerIndex(kPEvolL)], core.param[registerIndex(kPEvolR)]);
    // BVOL is the external input's level: the auto-DMA ring's (the PCM player fades through it).
    if (reg == kPBvolL || reg == kPBvolR)
        m_voices->autoDmaSetVolume(c, core.param[registerIndex(kPBvolL)], core.param[registerIndex(kPBvolR)]);
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
            return m_voices->envx(entryCore(entry), entryVoice(entry));
        return v->param[registerIndex(reg) & 7];
    }
    return core.param[registerIndex(reg)];
}

// ---- switches -------------------------------------------------------------------------------------

void Spu2::setSwitch(std::uint16_t entry, std::uint32_t value)
{
    const int c = entryCore(entry);
    const std::uint16_t reg = entryRegister(entry);
    m_cores[c].switches[registerIndex(reg)] = value;
    switch (reg)
    {
    case kSKon:
        m_voices->keyOn(c, value);
        break;
    case kSKoff:
        m_voices->keyOff(c, value);
        break;
    case kSVmixL:
    case kSVmixR:
    case kSVmixEL:
    case kSVmixER:
        pushMixMasks(c);
        break;
    default:
        break;
    }
}

std::uint32_t Spu2::switchBits(std::uint16_t entry)
{
    if (entryRegister(entry) == kSEndx)
        return m_voices->endx(entryCore(entry));
    return m_cores[entryCore(entry)].switches[registerIndex(entryRegister(entry))];
}

// ---- addresses ------------------------------------------------------------------------------------

void Spu2::setAddr(std::uint16_t entry, std::uint32_t value)
{
    const int c = entryCore(entry);
    Core &core = m_cores[c];
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
    if (voiceAt(entry) == nullptr)
        return;
    switch (reg)
    {
    case kVaSsa:
        m_voices->setStartAddr(c, entryVoice(entry), value);
        break;
    case kVaLsax:
        m_voices->setLoopAddr(c, entryVoice(entry), value);
        break;
    case kVaNax:
        m_voices->setNowAddr(c, entryVoice(entry), value);
        break;
    default:
        break;
    }
}

std::uint32_t Spu2::addr(std::uint16_t entry)
{
    const int c = entryCore(entry);
    const Core &core = m_cores[c];
    const std::uint16_t reg = entryRegister(entry);
    if (reg == kAIrqa)
        return core.irqa;
    if (reg == kAEea)
        return core.eea;
    if (voiceAt(entry) == nullptr)
        return 0;
    switch (reg)
    {
    case kVaSsa:
        return m_voices->startAddr(c, entryVoice(entry));
    case kVaLsax:
        return m_voices->loopAddr(c, entryVoice(entry));
    case kVaNax:
        // Playing: where it reads. Ended: on LSAX, following later writes to it (the stream's stop
        // sequence ends the voice, then points LSAX at the stop block and waits for NAX to get there).
        return m_voices->nowAddr(c, entryVoice(entry));
    default:
        return 0;
    }
}

void Spu2::setCoreAttr(std::uint16_t entry, std::uint16_t value)
{
    m_cores[entryCore(entry)].attr[(entry & 0xff) >> 1] = value;
    // The core bit is bit 0: masked off before the attribute is named.
    if ((entry & 0xfe) == kCEffectEnable)
        m_voices->setReverbEnable(entryCore(entry), value != 0);
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

// RXSNDDRV's PCM player is the only user: a looping ring on core 0 (FS2; RoR's on core 1), whose status
// it reads as a position. Its half-buffer interrupts are not raised: core 0 has no transfer handler
// (FS2's DMA thread transfers on, and installs its handler for, core 1).
int Spu2::blockTrans(int channel, std::uint16_t mode, std::uint8_t *iop, std::uint32_t size, std::uint32_t start)
{
    const int c = channel & 1;
    Core &core = m_cores[c];

    if ((mode & kBlockOp) == kBlockStop)
    {
        // Stop and say where: the address heard so far.
        if (core.ringRunning)
            core.ringStopped = m_voices->autoDmaOffset(c);
        core.ringRunning = false;
        m_voices->autoDmaStop(c);
        return static_cast<int>(low32(core.ring) + core.ringStopped);
    }

    std::uint32_t offset = 0;
    if ((mode & kBlockOp) == kBlockWriteFrom && iop != nullptr && size != 0)
        offset = (start - low32(iop)) % size;
    core.ring = iop;
    core.ringStopped = offset;
    core.ringRunning = iop != nullptr && size != 0;
    if (core.ringRunning)
        m_voices->autoDmaStart(c, iop, size, offset);
    return 0;
}

// Flag 0 asks where the auto-DMA reads, as an address (callers take their ring's base off it, on the
// same 32-bit truncation), and 0 once it is stopped: FS2's PCM player stops the ring and waits for that
// before it restarts it at another length. Any other flag asks whether a transfer is busy, and none is.
int Spu2::blockTransStatus(int channel, int flag)
{
    if (flag != 0)
        return 0;
    const int c = channel & 1;
    const Core &core = m_cores[c];
    if (!core.ringRunning)
        return 0;
    return static_cast<int>(low32(core.ring) + m_voices->autoDmaOffset(c));
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

// Stored only: the SPU2 interrupt is IRQA reached, and RXSNDDRV installs no handler (its ADPCM streams
// poll NAX instead).
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
    // The preset over the work area that ends at the EEA written just before; the depth is EVOLL / EVOLR,
    // which reads answer alike whichever way it was set.
    m_voices->setReverbPreset(c, attr.mode, core.eea);
    core.param[registerIndex(kPEvolL)] = static_cast<std::uint16_t>(attr.depthL);
    core.param[registerIndex(kPEvolR)] = static_cast<std::uint16_t>(attr.depthR);
    m_voices->setReverbDepth(c, core.param[registerIndex(kPEvolL)], core.param[registerIndex(kPEvolR)]);
    return 0;
}

void Spu2::clearEffectWorkArea(int c)
{
    m_voices->clearReverb(c);
}

void Spu2::setVoiceGroup(int c, int voice, int group)
{
    m_voices->setGroup(c, voice, group);
}

} // namespace yamipan::iop
