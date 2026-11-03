// sound/sound.cpp -- the game's sound layer (sound/sound.h): streamed
// banks, the movie audio sinks and feeder, sound effect banks, the sound
// settings, soundInit / soundUpdate / soundShutdown and the IOP / SPU2
// memory hooks rx allocates through.
//
// Three semaphores, made by soundInit: the sound one around every rx /
// SDPR2 call (rx's thread takes it too, through soundThreadLock), one for
// IOP allocations and one for the SPU2 allocator.

#include "sound/sound.h"

#include <stdint.h>
#include <string.h>

#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/sysdata.h"
#include "game/thread.h"
#include "rx/rxsnd.h"
#include "rx/rxsndstr.h"
#include "rx/rxsndut.h"
#include "rx/rxsys.h"
#include "rx/rxthread.h"
#include "sdk/eekernel.h"
#include "sdk/sif.h"
#include "sdpr2/sdpr2.h"
#include "sdpr2/sdpr2single.h"

SoundConfig *gSoundConfig;      /* sbss 0x31addc (gp-0x7a94) */

static int sSoundSema = -1;     /* sdata 0x31a8ac (gp-0x7fc4) */
static int sIopSema = -1;       /* sdata 0x31a8b0 (gp-0x7fc0) */
static int sSpuSema = -1;       /* sdata 0x31a8b4 (gp-0x7fbc) */

// The SPU2 allocator: a list of taken blocks between a head and a tail
// sentinel, 32 nodes to take them from (a node with size 0 is free).
// layout: host -- EE 0x14 per node (the links widen).
struct SpuMemBlock
{
    int          size;          // FS2 +0x0: 0 = free node
    u_int        start;         // FS2 +0x4
    u_int        end;           // FS2 +0x8
    SpuMemBlock *prev;          // FS2 +0xc
    SpuMemBlock *next;          // FS2 +0x10
};
constexpr int kSpuMemBlocks = 32;
// layout: host -- EE 0x2b0 at bss 0x4d08d0.
struct SpuMemMap
{
    SpuMemBlock head;           // FS2 +0x0
    SpuMemBlock tail;           // FS2 +0x14
    u_int       lo;             // FS2 +0x28
    u_int       hi;             // FS2 +0x2c
    SpuMemBlock blocks[kSpuMemBlocks]; // FS2 +0x30
};
static SpuMemMap sSpuMem;       /* bss 0x4d08d0 */

// The SPU2 span the game hands out (below: rx's own; above: the reverb
// work areas) and the block soundInit keeps for itself.
constexpr u_int kSpuMemLo = 0x5040;
constexpr u_int kSpuMemHi = 0x1f9000;
constexpr int   kSpuMemReservedSize = 0x7000;
constexpr u_int kSpuMemReservedAddr = 0x1d9000;
constexpr int   kSpuMemUnit = 0x40;

// copyBodyToIop writes at most this much per rxSndIopWrite.
constexpr int kSoundIopWriteChunk = 0x100000;
// stop(true) sleeps this long (us) between polls of the stream's reads.
constexpr int kSoundStopPollDelay = 4000;
// SoundSeBank::unload(true) retries the free this often.
constexpr int kSeBankFreeTries = 30;

// rxSndSetFrameRate's ticks.
constexpr int kSoundRateNtsc = 60;
constexpr int kSoundRatePal = 50;
// rxSndSetSysParam's kinds (unread: the values are the ROM's).
constexpr int kSoundSysParam0 = 0;
constexpr int kSoundSysParam1 = 1;
constexpr int kSoundSysParam2 = 2;
// sdpr2SetPairMode's modes for 60 / 50 Hz.
constexpr int kSdpr2PairModeNtsc = 0;
constexpr int kSdpr2PairModePal = 1;

// SDPR2's configuration (data 0x2f5030): 8 words, copied by its driver and
// both types. What they mean is unread.
static const int kSdpr2Config[8] = { 0, 0, -1, -1, 0, 0, 0, 0 };

constexpr float  kTwoPiF = 6.2831855f;
constexpr double kTwoPiD = 6.283185307179586;

// ──────────────────────────────────────────────────────────────────────
// Inlined helpers.

// An angle in radians as SDPR2's degrees: negated (SDPR2 turns the other
// way), wrapped into [0, 2 pi). The ROM divides in double.
static inline int panDegrees(float angle)
{
    angle *= -1.0f;
    while (angle < 0.0f || kTwoPiF <= angle) {
        if (angle < 0.0f)
            angle += kTwoPiF;
        else
            angle -= kTwoPiF;
    }
    return (int)((double)(angle * 360.0f) / kTwoPiD);
}

// A 0..kSoundVolumeFull level applied to a volume (signed division, as the
// ROM's >> 7 with its rounding fix).
static inline int scaleVolume(int volume, int level)
{
    return volume * level / kSoundVolumeFull;
}

static inline int spuMemRound(int size)
{
    return (size + kSpuMemUnit - 1) / kSpuMemUnit * kSpuMemUnit;
}

// The first free node, null when all 32 are taken.
static inline SpuMemBlock *spuMemNewBlock()
{
    for (int i = 0; i < kSpuMemBlocks; i++) {
        if (sSpuMem.blocks[i].size == 0)
            return &sSpuMem.blocks[i];
    }
    return nullptr;
}

static inline void spuMemLinkAfter(SpuMemBlock *block, SpuMemBlock *at)
{
    block->prev = at;
    block->next = at->next;
    at->next->prev = block;
    at->next = block;
}

static inline void spuMemLinkBefore(SpuMemBlock *block, SpuMemBlock *at)
{
    block->prev = at->prev;
    block->next = at;
    at->prev->next = block;
    at->prev = block;
}

// The empty map with soundInit's reserved block.
static inline void spuMemInit()
{
    memset(&sSpuMem, 0, sizeof(sSpuMem));
    sSpuMem.lo = kSpuMemLo;
    sSpuMem.hi = kSpuMemHi;
    sSpuMem.head.size = 0;
    sSpuMem.head.start = kSpuMemLo;
    sSpuMem.head.end = kSpuMemLo;
    sSpuMem.head.prev = nullptr;
    sSpuMem.head.next = &sSpuMem.tail;
    sSpuMem.tail.size = 0;
    sSpuMem.tail.start = kSpuMemHi;
    sSpuMem.tail.end = kSpuMemHi;
    sSpuMem.tail.next = nullptr;
    sSpuMem.tail.prev = &sSpuMem.head;
    spuMemReserve(kSpuMemReservedSize, kSpuMemReservedAddr);
}

// ──────────────────────────────────────────────────────────────────────
// SoundPcmSink::setAttr: PCM streams have no attributes. (CodeWarrior put
// this inline slot at the file's head.)
// FS2 0x00144080

void SoundPcmSink::setAttr(int attr)
{
}

// ──────────────────────────────────────────────────────────────────────
// SoundStream

void SoundIopBody::release()
{
    if (!m_borrowed && m_size != 0)
        rxSndIopFree(m_addr);
    m_size = 0;
    m_addr = 0;
}

inline void SoundStream::applyVolume()
{
    if (m_stream < 0)
        return;
    int volL = scaleVolume(m_volL, m_fade);
    int volR = scaleVolume(m_volR, m_fade);
    WaitSema(sSoundSema);
    rxSndStrSetVolume(m_stream, volL, volR);
    SignalSema(sSoundSema);
}

inline void SoundStream::freeSpuRing()
{
    if (m_spuRing != 0) {
        spuMemFree(m_spuRing);
        m_spuRing = 0;
    }
}

// The body copied to IOP memory in chunks, replacing an earlier copy. The
// IOP address, 0 when there is no room.
// FS2 0x00144090
// callcheck: rxSndIopFree (m_body.release inline)

int SoundStream::copyBodyToIop(const void *body, int size)
{
    u_int iop = rxSndIopAlloc(size);
    if (iop == 0)
        return 0;
    m_body.release();
    const u_char *src = static_cast<const u_char *>(body);
    u_int dst = iop;
    for (int left = size; left != 0; ) {
        int n = left;
        if (kSoundIopWriteChunk < left)
            n = kSoundIopWriteChunk;
        WaitSema(sSoundSema);
        rxSndIopWrite(dst, src, n);
        SignalSema(sSoundSema);
        dst += n;
        src += n;
        left -= n;
    }
    m_body.m_addr = iop;
    m_body.m_size = size;
    m_body.m_borrowed = 0;
    return (int)iop;
}

// FS2 0x001441a0

int SoundStream::getDataWritten()
{
    if (m_stream < 0)
        return -1;
    WaitSema(sSoundSema);
    int written = rxSndStrGetDataWritten(m_stream);
    SignalSema(sSoundSema);
    return written;
}

// FS2 0x001441f0

int SoundStream::setPitch(float pitch)
{
    if (m_stream < 0)
        return -1;
    WaitSema(sSoundSema);
    rxSndStrSetPitch(m_stream, pitch);
    SignalSema(sSoundSema);
    return 0;
}

// FS2 0x00144250
// callcheck: __extendsfdf2, __divdf3, __fixdfsi, WaitSema, rxSndStrSetVolume, SignalSema (the double divide in panDegrees; applyVolume inline)

int SoundStream::setPan(float angle, int volume)
{
    int volL, volR;
    m_panState = sdpr2CalcPanDeg(panDegrees(angle), m_panState, &volL, &volR);
    m_volL = scaleVolume(volL, volume);
    m_volR = scaleVolume(volR, volume);
    applyVolume();
    return 0;
}

// FS2 0x00144400
// callcheck: __extendsfdf2, __divdf3, __fixdfsi, sdpr2CalcPanDeg, WaitSema, rxSndStrSetVolume, SignalSema (setPan inline)

int SoundStream::playPan(int request, int start, float angle, int volume)
{
    int stream = play(request, start);
    setPan(angle, volume);
    return stream;
}

// FS2 0x001445d0

int SoundStream::getRequestRate(int request)
{
    if (m_bank < 0)
        return 0;
    if (request < 0)
        request = m_request;
    if (request < 0)
        return 0;
    WaitSema(sSoundSema);
    int rate = rxSndStrBankGetRequestRate(m_bank, request);
    SignalSema(sSoundSema);
    return rate;
}

// The fade's next frame; at its end the level is the target and the end
// action runs.
// FS2 0x00144650
// callcheck: WaitSema, rxSndStrSetVolume, SignalSema (applyVolume)

void SoundStream::updateFade()
{
    if (m_state != SoundStream_Playing)
        return;
    if (m_fadeFrames == 0)
        return;
    m_fadeFrame++;
    if (m_fadeFrame < m_fadeFrames) {
        m_fade = m_fadeFrom + (m_fadeTo - m_fadeFrom) * m_fadeFrame / m_fadeFrames;
    } else {
        m_fadeFrame = 0;
        m_fadeFrames = 0;
        m_fade = m_fadeTo;
        if (m_fadeEnd == SoundFadeEnd_Pause) {
            if (m_stream >= 0) {
                WaitSema(sSoundSema);
                rxSndStrPause(m_stream, 1, 0);
                SignalSema(sSoundSema);
                m_state = SoundStream_Paused;
            }
        } else if (m_fadeEnd == SoundFadeEnd_Stop) {
            stop(false);
        }
    }
    applyVolume();
}

// FS2 0x001447c0

void SoundStream::startFade(int frames, int from, int to, int end)
{
    m_fadeFrames = frames;
    m_fadeFrame = 0;
    m_fadeFrom = from;
    m_fadeTo = to;
    m_fadeEnd = end;
}

// FS2 0x001447e0
// callcheck: WaitSema, rxSndStrSetVolume, SignalSema (applyVolume)

void SoundStream::setVolume(int volL, int volR)
{
    m_volL = volL;
    m_volR = volR;
    applyVolume();
}

// FS2 0x00144880

void SoundStream::setAttr(int attr)
{
    if (m_stream < 0)
        return;
    WaitSema(sSoundSema);
    rxSndStrSetAttr(m_stream, (u_char)attr);
    SignalSema(sSoundSema);
}

// FS2 0x001448d0

int SoundStream::getRequestChannels(int request)
{
    if (m_bank < 0)
        return -1;
    WaitSema(sSoundSema);
    int channels = rxSndStrBankGetRequestChannels(m_bank, request);
    SignalSema(sSoundSema);
    return channels;
}

// FS2 0x00144930

int SoundStream::getRequestSize(int request)
{
    if (m_bank < 0)
        return -1;
    WaitSema(sSoundSema);
    int size = rxSndStrBankGetRequestSize(m_bank, request);
    SignalSema(sSoundSema);
    return size;
}

// FS2 0x00144990

int SoundStream::getPosition()
{
    if (m_state != SoundStream_Playing && m_state != SoundStream_Paused)
        return -1;
    WaitSema(sSoundSema);
    int pos = m_startPos + (int)rxSndAdpcmGetPosition(rxSndStrGetHandle(m_stream));
    SignalSema(sSoundSema);
    return pos;
}

// FS2 0x00144a10
// callcheck: WaitSema, SignalSema (spuMemFree inline: the SPU semaphore)

void SoundStream::update()
{
    if (m_stream < 0)
        return;
    WaitSema(sSoundSema);
    if (m_state == SoundStream_StopPending && rxSndStrSyncRead(m_stream, 0) == 0) {
        rxSndStrStop(m_stream, 0);
        m_stream = -1;
        freeSpuRing();
        m_state = SoundStream_Ended;
        m_fade = kSoundVolumeFull;
    }
    // Kept from the ROM: after a pending stop just finished this asks
    // about stream -1 (rx answers 0, which ends it once more below).
    u_int status = rxSndStrGetStatus(m_stream);
    if (status & RXSNDSTR_STATUS_END) {
        rxSndStrStop(m_stream, 0);
        m_stream = -1;
        m_request = -1;
        m_trackCount = -1;
        m_startPos = 0;
        freeSpuRing();
        m_state = SoundStream_Ended;
        m_fade = kSoundVolumeFull;
    }
    if (status == 0) {
        m_stream = -1;
        m_request = -1;
        m_trackCount = -1;
        m_startPos = 0;
        freeSpuRing();
        m_state = SoundStream_Ended;
        m_fade = kSoundVolumeFull;
    }
    SignalSema(sSoundSema);
    updateFade();
}

// FS2 0x00144d10

void SoundStream::pause(bool on)
{
    if (m_stream < 0)
        return;
    WaitSema(sSoundSema);
    rxSndStrPause(m_stream, on, 0);
    SignalSema(sSoundSema);
    m_state = on ? SoundStream_Paused : SoundStream_Playing;
}

// Silences the stream and suspends its reads. Without wait a read still
// in flight leaves it StopPending for update; with wait it polls rx (and
// runs soundUpdate's work itself) until the reads are done.
// FS2 0x00144d80
// callcheck: rxSndStrGetHandle, rxSndAdpcmGetPosition, rxSndStrSetVolume, rxSndUtUpdate, rxSndUpdate, sdpr2Update (getPosition, setVolume and soundUpdate inline)

int SoundStream::stop(bool wait)
{
    if (m_bank < 0)
        return -1;
    if (m_stream < 0)
        return -1;
    int pos = getPosition();
    m_panState = 0;
    m_fadeFrames = 0;
    m_fadeFrame = 0;
    m_state = SoundStream_Stopped;
    m_request = -1;
    m_trackCount = -1;
    setVolume(0, 0);
    WaitSema(sSoundSema);
    if (!wait) {
        rxSndStrSuspend(m_stream, 0);
        if (rxSndStrSyncRead(m_stream, 0) != 0) {
            SignalSema(sSoundSema);
            m_state = SoundStream_StopPending;
            return pos;
        }
    } else {
        rxSndStrSuspend(m_stream, 0);
        while (rxSndStrSyncRead(m_stream, 0) != 0) {
            SignalSema(sSoundSema);
            DelayThread(kSoundStopPollDelay);
            soundUpdate();
            WaitSema(sSoundSema);
        }
    }
    rxSndStrStop(m_stream, 0);
    m_stream = -1;
    freeSpuRing();
    SignalSema(sSoundSema);
    m_state = SoundStream_Ended;
    m_fade = kSoundVolumeFull;
    return pos;
}

// Stops, then closes the bank, its SPU ring and its IOP body.
// FS2 0x00145100
// callcheck: rxSndIopFree (m_body.release inline)

int SoundStream::closeBank()
{
    stop(true);
    m_panState = 0;
    if (m_bank >= 0) {
        WaitSema(sSoundSema);
        rxSndStrBankClose(m_bank);
        freeSpuRing();
        m_body.release();
        SignalSema(sSoundSema);
        m_bank = -1;
    }
    return 0;
}

// FS2 0x00145240
// callcheck: rxSndStrBankGetRequestChannels, spuMemAlloc (getRequestChannels and spuMemAllocHigh inline)

int SoundStream::play(int request, int start)
{
    if (request < 0)
        return -1;
    m_panState = 0;
    void *buf = nullptr;
    int bufSize = 0;
    stop(true);
    switch (m_bodyKind) {
    case SoundBody_Iop:
    case SoundBody_Memory:
        break;
    case SoundBody_File:
        bufSize = m_bufSize;
        if (bufSize == 0)
            return -1;
        buf = m_buffer;
        break;
    default:
        return -1;
    }
    if (m_spuRing == 0)
        m_spuRing = spuMemAllocHigh(kSoundStreamSpuRing);
    if (m_spuRing == 0)
        return -1;
    WaitSema(sSoundSema);
    int stream = rxSndStrPlayAt(m_bank, request, start, buf, bufSize, 0, 4, m_spuRing, 4, 0);
    SignalSema(sSoundSema);
    m_stream = stream;
    m_request = request;
    m_state = SoundStream_Playing;
    if (m_bank < 0 || m_request < 0) {
        m_trackCount = -1;
    } else {
        WaitSema(sSoundSema);
        m_trackCount = rxSndStrBankGetTrackCount(m_bank, m_request);
        SignalSema(sSoundSema);
    }
    m_startPos = start;
    m_channels = getRequestChannels(request);
    return stream;
}

// FS2 0x00145420
// callcheck: WaitSema, rxSndStrSetVolume, SignalSema, rxSndStrBankGetRequestChannels (setVolume and getRequestChannels inline)

int SoundStream::playVolume(int request, int start, int volL, int volR)
{
    m_panState = 0;
    int stream = play(request, start);
    setVolume(volL, volR);
    m_channels = getRequestChannels(request);
    return stream;
}

// FS2 0x00145530
// callcheck: stop, rxSndStrBankClose, rxSndIopFree (closeBank inline)

int SoundStream::openBankMemory(const void *header, const void *body, int bodySize, bool toIop)
{
    closeBank();
    m_panState = 0;
    if (header == nullptr || body == nullptr)
        return -1;
    if (!toIop) {
        m_bodyKind = SoundBody_Memory;
        WaitSema(sSoundSema);
        m_bank = rxSndStrBankOpen(0, header);
        rxSndStrBankAttachBodyMemory(m_bank, body, bodySize);
        SignalSema(sSoundSema);
    } else {
        copyBodyToIop(body, bodySize);
        u_int iop = m_body.m_addr;
        if (iop == 0)
            return -1;
        WaitSema(sSoundSema);
        m_bodyKind = SoundBody_Iop;
        m_bank = rxSndStrBankOpen(0, header);
        rxSndStrBankAttachBodyIop(m_bank, iop, bodySize);
        SignalSema(sSoundSema);
    }
    return m_bank;
}

// FS2 0x00145750
// callcheck: stop, rxSndStrBankClose, rxSndIopFree (closeBank inline)

int SoundStream::openBankFile(const void *header, const char *bodyName)
{
    closeBank();
    m_panState = 0;
    if (header == nullptr || bodyName == nullptr || bodyName[0] == '\0')
        return -1;
    WaitSema(sSoundSema);
    m_bank = rxSndStrBankOpen(0, header);
    m_bodyKind = SoundBody_File;
    rxSndStrBankAttachBodyFile(m_bank, bodyName);
    SignalSema(sSoundSema);
    return m_bank;
}

// FS2 0x001458f0
// callcheck: stop, WaitSema, rxSndStrBankClose, rxSndIopFree, SignalSema (closeBank inline)

void SoundStream::freeBuffer()
{
    closeBank();
    m_state = SoundStream_Stopped;
    if (m_bufOwned)
        gGlobalHeap.free(m_buffer);
    m_bufSize = 0;
    m_buffer = nullptr;
    m_bufOwned = 0;
}

// Kept from the ROM: an owned buffer set earlier is not freed (only
// freeBuffer and the dtor free it).
// FS2 0x00145a50
// callcheck: stop, WaitSema, rxSndStrBankClose, rxSndIopFree, SignalSema (closeBank inline)

void SoundStream::setBuffer(int size, void *buffer)
{
    closeBank();
    m_state = SoundStream_Stopped;
    m_buffer = buffer;
    m_bufSize = size;
    m_bufOwned = 0;
    if (buffer == nullptr) {
        m_buffer = gGlobalHeap.alloc((u_int)size, 0x10);
        m_bufOwned = 1;
    }
}

// FS2 0x00145be0
// callcheck: stop, WaitSema, rxSndStrBankClose, SignalSema, rxSndIopFree, operator_delete (closeBank inline, m_body's dtor, the deleting dtor)

SoundStream::~SoundStream()
{
    closeBank();
    freeBuffer();
}

// FS2 0x00145d70

SoundStream::SoundStream()
{
    m_body.m_addr = 0;
    m_body.m_size = 0;
    m_body.m_borrowed = 0;
    m_bodyKind = SoundBody_None;
    m_bufSize = 0;
    m_buffer = nullptr;
    m_bufOwned = 0;
    m_bank = -1;
    m_stream = -1;
    m_request = -1;
    m_trackCount = -1;
    m_startPos = 0;
    m_spuRing = 0;
    m_panState = 0;
    m_volL = 0;
    m_volR = 0;
    m_fade = kSoundVolumeFull;
    m_fadeFrom = 0;
    m_fadeTo = 0;
    m_fadeFrames = 0;
    m_fadeFrame = 0;
    m_state = SoundStream_Stopped;
    m_channels = 1;
    m_fadeEnd = SoundFadeEnd_None;  // PC: the ROM leaves +0x54 unset (startFade sets it)
}

// ──────────────────────────────────────────────────────────────────────
// SoundFeeder

// Moves whole units from the source into the sink while both have them:
// stereo sinks take a left and a right block of m_interleave bytes at a
// time (the right one after the left in the ring), the others multiples
// of 0x40 (or of the interleave, if larger), twice at most. Ends the sink
// once the source's end position is fed; the feeder ends with the sink.
// FS2 0x00145de0
// callcheck: getStatus, setVolume, poll, muteIfEnded, contiguous, getFree, readPtr, write, consume, endPos, end (vtable slots)

int SoundFeeder::update()
{
    if (m_state != SoundFeeder_Feeding)
        return 1;
    if (m_sink->getStatus() & SoundSinkStatus_Ended) {
        m_state = SoundFeeder_Ended;
        if (m_sink != nullptr)
            m_sink->setVolume(0, 0);
        return 0;
    }
    if (m_sink != nullptr && (m_sink->getStatus() & SoundSinkStatus_Started))
        m_sink->setVolume(m_volumeL, m_volumeR);
    m_source->poll();
    m_sink->muteIfEnded();
    if (m_sink->m_mode == SoundSinkMode_Stereo) {
        for (;;) {
            int half = m_sink->m_interleave;
            int n = m_source->contiguous();
            int space = m_sink->getFree();
            if (space < n)
                n = space;
            if (n < 1)
                n = 0;
            int block = half * 2;
            if (block < n)
                n = block;
            if (n < 1)
                n = 0;
            if (n < block)
                break;
            const u_char *left = m_source->readPtr();
            m_sink->write(left, m_source->readPtr() + half, n);
            m_source->consume(n);
            m_fed += n;
            int end = m_source->endPos();
            if (end != 0 && end <= m_fed)
                m_sink->end();
        }
    } else {
        for (int i = 0; i < 2; i++) {
            int n = m_source->contiguous();
            int space = m_sink->getFree();
            if (space < n)
                n = space;
            int unit = m_sink->m_interleave;
            int mode = m_sink->m_mode;
            if (mode == SoundSinkMode_Interleaved || mode == 3)
                unit = 0x40;
            else if (unit < 0x40)
                unit = 0x40;
            n = unit * (n / unit);
            if (n < unit)
                break;
            m_sink->write(m_source->readPtr(), nullptr, n);
            m_source->consume(n);
            m_fed += n;
            int end = m_source->endPos();
            if (end != 0 && end <= m_fed)
                m_sink->end();
        }
    }
    if (m_sink->getStatus() & SoundSinkStatus_Ended) {
        m_state = SoundFeeder_Ended;
        if (m_sink != nullptr)
            m_sink->setVolume(0, 0);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// StreamRing's empty base slots (inline in its class; CodeWarrior emitted
// them here, with the feeder that calls them).

// FS2 0x001461c0

int StreamRing::endPos()
{
    return 0;
}

// FS2 0x001461d0

int StreamRing::consume(int size)
{
    return 0;
}

// FS2 0x001461e0

u_char *StreamRing::readPtr()
{
    return nullptr;
}

// FS2 0x001461f0

int StreamRing::contiguous()
{
    return 0;
}

// FS2 0x00146200

int StreamRing::poll()
{
    return 0;
}

// FS2 0x00146210
// callcheck: setVolume (a vtable slot)

int SoundFeeder::start()
{
    switch (m_state) {
    case SoundFeeder_Ended:
    case SoundFeeder_Stopping:
        return 1;
    case SoundFeeder_Paused:
        m_state = SoundFeeder_Feeding;
        return 0;
    case SoundFeeder_Feeding:
        return 0;
    case SoundFeeder_Idle:
        if (m_source == nullptr)
            return -2;
        if (m_sink == nullptr)
            return -3;
        m_state = SoundFeeder_Feeding;
        if (m_sink != nullptr)
            m_sink->setVolume(0, 0);
        update();
        return 0;
    default:
        return -1;
    }
}

// FS2 0x001462f0

int SoundFeeder::setSink(SoundSink *sink)
{
    m_fed = 0;
    m_sink = sink;
    return 0;
}

// FS2 0x00146300

int SoundFeeder::setSource(StreamRing *source)
{
    m_fed = 0;
    m_source = source;
    return 0;
}

// Silences a feeding or paused sink and leaves the feeder paused (the
// sink and source are the owner's to delete).
// FS2 0x00146310
// callcheck: setVolume, operator_delete (a vtable slot; the deleting dtor)

SoundFeeder::~SoundFeeder()
{
    if (m_state == SoundFeeder_Feeding || m_state == SoundFeeder_Paused) {
        if (m_sink != nullptr)
            m_sink->setVolume(0, 0);
        m_state = SoundFeeder_Paused;
    }
}

// FS2 0x001463a0

SoundFeeder::SoundFeeder()
{
    m_state = SoundFeeder_Idle;
    m_source = nullptr;
    m_sink = nullptr;
    m_fed = 0;
    m_volumeL = kSoundVolumeFull;
    m_volumeR = kSoundVolumeFull;
}

// ──────────────────────────────────────────────────────────────────────
// SoundPcmSink: an rxSndPcm stream.

// FS2 0x001463d0
// callcheck: getStatus, setVolume (vtable slots)

int SoundPcmSink::muteIfEnded()
{
    if (m_handle >= 0 && (getStatus() & SoundSinkStatus_Ended))
        setVolume(0, 0);
    return 0;
}

// FS2 0x00146440

int SoundPcmSink::addCallback(int offset, void (*func)(int, int), int arg)
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    int result = rxSndPcmAddCallback(handle, offset, func, arg);
    SignalSema(sSoundSema);
    return result;
}

// FS2 0x001464c0

int SoundPcmSink::getUserData()
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    int data = rxSndPcmGetUserData(handle);
    SignalSema(sSoundSema);
    return data;
}

// FS2 0x00146510

int SoundPcmSink::setUserData(int data)
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    int result = rxSndPcmSetUserData(handle, data);
    SignalSema(sSoundSema);
    return result;
}

// FS2 0x00146570

void SoundPcmSink::end()
{
    int handle = m_handle;
    if (handle < 0)
        return;
    WaitSema(sSoundSema);
    rxSndPcmEnd(handle);
    SignalSema(sSoundSema);
}

// FS2 0x001465b0

int SoundPcmSink::write(const void *data, const void *dataR, int size)
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    int written = rxSndPcmWrite(handle, data, dataR, size);
    SignalSema(sSoundSema);
    return written;
}

// FS2 0x00146630

int SoundPcmSink::getFree()
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    int space = rxSndPcmGetFree(handle);
    SignalSema(sSoundSema);
    return space;
}

// FS2 0x00146680

u_int SoundPcmSink::getStatus()
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    u_int status = rxSndPcmGetStatus(handle);
    SignalSema(sSoundSema);
    return status;
}

// FS2 0x001466d0
// callcheck: setVolume (a vtable slot)

void SoundPcmSink::close()
{
    int handle = m_handle;
    if (handle < 0)
        return;
    setVolume(0, 0);
    m_handle = -1;
    WaitSema(sSoundSema);
    rxSndPcmClose(handle);
    SignalSema(sSoundSema);
}

// FS2 0x00146740
// callcheck: close (a vtable slot)

int SoundPcmSink::open(int mode, int rate, int a3, int a4, int a5, int a6, int interleave)
{
    close();
    m_mode = mode;
    m_interleave = interleave;
    WaitSema(sSoundSema);
    int handle = rxSndPcmOpen(mode, (u_int)a3, a4);
    SignalSema(sSoundSema);
    m_handle = handle;
    return handle;
}

// FS2 0x001467d0

void SoundPcmSink::setVolume(int volL, int volR)
{
    if (m_handle < 0)
        return;
    WaitSema(sSoundSema);
    rxSndPcmSetVolume(m_handle, volL, volR);
    SignalSema(sSoundSema);
}

// FS2 0x00146830
// callcheck: close, operator_delete (a vtable slot; the deleting dtor)

SoundPcmSink::~SoundPcmSink()
{
    close();
}

// FS2 0x001468b0

SoundPcmSink::SoundPcmSink()
{
    m_handle = -1;
    m_mode = -1;
    m_interleave = 0;
}

// ──────────────────────────────────────────────────────────────────────
// SoundAdpcmSink: an rxSndAdpcm stream.

// FS2 0x00146930
// callcheck: getStatus, setVolume (vtable slots)

int SoundAdpcmSink::muteIfEnded()
{
    if (m_handle >= 0 && (getStatus() & SoundSinkStatus_Ended))
        setVolume(0, 0);
    return 0;
}

// FS2 0x001469a0

void SoundAdpcmSink::setAttr(int attr)
{
    int handle = m_handle;
    if (handle < 0)
        return;
    WaitSema(sSoundSema);
    rxSndAdpcmSetAttr(handle, (u_char)attr);
    SignalSema(sSoundSema);
}

// FS2 0x001469f0

int SoundAdpcmSink::addCallback(int offset, void (*func)(int, int), int arg)
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    int result = rxSndAdpcmAddCallback(handle, offset, func, arg);
    SignalSema(sSoundSema);
    return result;
}

// FS2 0x00146a70

int SoundAdpcmSink::getUserData()
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    int data = rxSndAdpcmGetUserData(handle);
    SignalSema(sSoundSema);
    return data;
}

// FS2 0x00146ac0

int SoundAdpcmSink::setUserData(int data)
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    int result = rxSndAdpcmSetUserData(handle, data);
    SignalSema(sSoundSema);
    return result;
}

// FS2 0x00146b20

void SoundAdpcmSink::end()
{
    int handle = m_handle;
    if (handle < 0)
        return;
    WaitSema(sSoundSema);
    rxSndAdpcmEnd(handle);
    SignalSema(sSoundSema);
}

// FS2 0x00146b60

int SoundAdpcmSink::write(const void *data, const void *dataR, int size)
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    int written = rxSndAdpcmWrite(handle, data, dataR, size);
    SignalSema(sSoundSema);
    return written;
}

// FS2 0x00146be0

int SoundAdpcmSink::getFree()
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    int space = rxSndAdpcmGetFree(handle);
    SignalSema(sSoundSema);
    return space;
}

// FS2 0x00146c30

u_int SoundAdpcmSink::getStatus()
{
    int handle = m_handle;
    if (handle < 0)
        return 0;
    WaitSema(sSoundSema);
    u_int status = rxSndAdpcmGetStatus(handle);
    SignalSema(sSoundSema);
    return status;
}

// FS2 0x00146c80
// callcheck: setVolume (a vtable slot)

void SoundAdpcmSink::close()
{
    int handle = m_handle;
    if (handle < 0)
        return;
    setVolume(0, 0);
    WaitSema(sSoundSema);
    rxSndAdpcmClose(handle);
    SignalSema(sSoundSema);
    m_handle = -1;
}

// Opens silent: the feeder raises the volume once it plays.
// FS2 0x00146cf0
// callcheck: close, setVolume (vtable slots)

int SoundAdpcmSink::open(int mode, int rate, int a3, int a4, int a5, int a6, int interleave)
{
    close();
    m_mode = mode;
    m_interleave = interleave;
    WaitSema(sSoundSema);
    int handle = rxSndAdpcmOpen(mode, rate, (u_int)a3, a4, (u_int)a5, a6);
    SignalSema(sSoundSema);
    m_handle = handle;
    setVolume(0, 0);
    return handle;
}

// FS2 0x00146dd0

void SoundAdpcmSink::setVolume(int volL, int volR)
{
    int handle = m_handle;
    if (handle < 0)
        return;
    WaitSema(sSoundSema);
    rxSndAdpcmSetVolume(handle, volL, volR);
    SignalSema(sSoundSema);
}

// FS2 0x00146e30
// callcheck: close, operator_delete (a vtable slot; the deleting dtor)

SoundAdpcmSink::~SoundAdpcmSink()
{
    close();
}

// FS2 0x00146eb0

SoundAdpcmSink::SoundAdpcmSink()
{
    m_handle = -1;
    m_mode = -1;
    m_interleave = 0;
}

// ──────────────────────────────────────────────────────────────────────
// SoundSeBank

// FS2 0x00146ee0

int SoundSeBank::setPanPitchF(int handle, float pitch)
{
    if (handle < 0)
        return 1;
    WaitSema(sSoundSema);
    int result = sdpr2SetPitchF(handle, pitch);
    SignalSema(sSoundSema);
    return result;
}

// FS2 0x00146f40

int SoundSeBank::setPitch(int voice, float pitch)
{
    if (voice >= 0) {
        WaitSema(sSoundSema);
        rxSndSeSetPitch(voice, pitch);
        SignalSema(sSoundSema);
    }
    return voice < 0;
}

// FS2 0x00146fa0

bool SoundSeBank::isPlaying(int voice)
{
    WaitSema(sSoundSema);
    u_int status = rxSndSeGetStatus(voice);
    SignalSema(sSoundSema);
    return (status & kSoundSeStatusSounding) != 0;
}

// FS2 0x00146ff0

bool SoundSeBank::isPanPlaying(int handle)
{
    WaitSema(sSoundSema);
    int status = sdpr2GetStatus(handle);
    SignalSema(sSoundSema);
    return status == SDPR2_STATUS_PLAYING || status == SDPR2_STATUS_PAUSED;
}

// FS2 0x00147050
// callcheck: __extendsfdf2, __divdf3, __fixdfsi (the double divide in panDegrees)

int SoundSeBank::setPan(float angle, float spread, int handle, int volume)
{
    WaitSema(sSoundSema);
    int deg = panDegrees(angle);
    sdpr2SetVolume(handle, volume);
    sdpr2SetSpread(handle, (int)(spread * 128.0f));
    sdpr2SetAngle(handle, deg);
    SignalSema(sSoundSema);
    return 0;
}

// FS2 0x001471b0

int SoundSeBank::play(int prog, int note, int volL, int volR)
{
    WaitSema(sSoundSema);
    int voice = rxSndSePlay(m_id, prog, note, volL, volR, 0);
    SignalSema(sSoundSema);
    return voice;
}

// FS2 0x00147240
// callcheck: __extendsfdf2, __divdf3, __fixdfsi (the double divide in panDegrees)

int SoundSeBank::playPan(float angle, float spread, int prog, int note, int volume, bool pair)
{
    if (spread > 1.0f)
        return -2;
    int deg = panDegrees(angle);
    WaitSema(sSoundSema);
    int handle;
    if (!pair)
        handle = sdpr2PlayPan(SDPR2_TYPE_SINGLE, m_id, prog, note, volume, deg, (int)(spread * 128.0f));
    else
        handle = sdpr2PlayPan(SDPR2_TYPE_PAIR, m_id, prog, note, volume, deg, (int)(spread * 128.0f));
    SignalSema(sSoundSema);
    return handle;
}

// FS2 0x001473e0

int SoundSeBank::setVolume(int voice, int volL, int volR)
{
    WaitSema(sSoundSema);
    rxSndSeSetVolume(voice, volL, volR);
    SignalSema(sSoundSema);
    return 0;
}

// FS2 0x00147440

int SoundSeBank::setAttr(int voice, int attr)
{
    WaitSema(sSoundSema);
    rxSndSeSetAttr(voice, (u_char)attr);
    SignalSema(sSoundSema);
    return 0;
}

// FS2 0x00147490

int SoundSeBank::setPanAttr(int handle, bool on)
{
    WaitSema(sSoundSema);
    int result;
    if (!on)
        result = sdpr2SetAttr(handle, 0);
    else
        result = sdpr2SetAttr(handle, 1);
    SignalSema(sSoundSema);
    return result;
}

// FS2 0x00147500

void SoundSeBank::stop(int voice)
{
    WaitSema(sSoundSema);
    rxSndSeSetVolume(voice, 0, 0);
    SignalSema(sSoundSema);
    WaitSema(sSoundSema);
    rxSndSeStop(voice, 0);
    SignalSema(sSoundSema);
}

// FS2 0x00147560

void SoundSeBank::stopPan(int handle)
{
    WaitSema(sSoundSema);
    sdpr2SetVolume(handle, 0);
    SignalSema(sSoundSema);
    WaitSema(sSoundSema);
    sdpr2Stop(handle, 0);
    SignalSema(sSoundSema);
}

// Stops every effect, then frees the bank and waits until rx lets go of
// it ("waiting for the end... n").
// FS2 0x001475c0
// callcheck: operator_delete (the deleting dtor)

SoundSeBank::~SoundSeBank()
{
    WaitSema(sSoundSema);
    sdpr2StopAll(0);
    SignalSema(sSoundSema);
    WaitSema(sSoundSema);
    rxSndSeStopAll(0);
    SignalSema(sSoundSema);
    threadYield(2);
    unload(true);
    unload(false);
    int tries = 0;
    while (getState() != SoundSeBank_Empty) {
        threadYield(1);
        tries++;
        rxSysPrintf("\x8f\x49\x97\xb9\x91\xd2\x82\xbf\x81\x42\x81\x42\x81\x42%d\n", tries); // "終了待ち。。。%d\n" (Shift-JIS)
    }
}

// FS2 0x001476b0

SoundSeBank::SoundSeBank(const void *xvab, bool wait, u_int iopAddr, u_int spuAddr)
{
    m_state = SoundSeBank_Empty;
    m_id = -1;
    m_iopAddr = 0;
    m_spuAddr = 0;
    m_id = load(xvab, wait, iopAddr, spuAddr);
}

// Advances a load or a free without blocking rx (its wait argument 1).
// FS2 0x001476f0

void SoundSeBank::poll()
{
    WaitSema(sSoundSema);
    if (m_state == SoundSeBank_Freeing) {
        if (rxSndXvabFree(m_id, 1) == 1) {
            m_state = SoundSeBank_Freeing;
        } else {
            m_iopAddr = 0;
            m_spuAddr = 0;
            m_state = SoundSeBank_Empty;
        }
    } else if (m_state == SoundSeBank_Loading && rxSndXvabCheck(m_id, 1) == 0) {
        m_state = SoundSeBank_Loaded;
    }
    SignalSema(sSoundSema);
}

// FS2 0x00147790

int SoundSeBank::getState()
{
    WaitSema(sSoundSema);
    if (m_state == SoundSeBank_Freeing) {
        if (rxSndXvabFree(m_id, 0) == 1) {
            m_state = SoundSeBank_Freeing;
        } else {
            m_state = SoundSeBank_Empty;
            m_id = -1;
            m_iopAddr = 0;
            m_spuAddr = 0;
        }
    } else if (m_state == SoundSeBank_Loading && rxSndXvabCheck(m_id, 0) == 0) {
        m_state = SoundSeBank_Loaded;
    }
    SignalSema(sSoundSema);
    return m_state;
}

// A loaded bank: freed (force: retried kSeBankFreeTries times, then its
// IOP and SPU2 memory freed by hand). A freeing bank is polled (force).
// FS2 0x00147840
// callcheck: sceSifFreeSysMemory (soundIopFree inline)

int SoundSeBank::unload(bool force)
{
    if (getState() == SoundSeBank_Loading)
        poll();
    if (getState() == SoundSeBank_Loaded) {
        if (!force) {
            WaitSema(sSoundSema);
            if (rxSndXvabFree(m_id, 0) == 1) {
                m_state = SoundSeBank_Freeing;
            } else {
                m_state = SoundSeBank_Empty;
                m_id = -1;
                m_iopAddr = 0;
                m_spuAddr = 0;
            }
            SignalSema(sSoundSema);
        } else {
            for (int tries = kSeBankFreeTries; ; ) {
                WaitSema(sSoundSema);
                int busy = rxSndXvabFree(m_id, 0);
                SignalSema(sSoundSema);
                if (busy != 1)
                    break;
                tries--;
                if (tries < 1) {
                    // An empty locked block in the ROM (a debug print
                    // compiled out).
                    WaitSema(sSoundSema);
                    SignalSema(sSoundSema);
                    if (m_iopAddr != 0)
                        soundIopFree(m_iopAddr);
                    if (m_spuAddr != 0)
                        spuMemFree(m_spuAddr);
                    break;
                }
                threadYield(1);
            }
            m_state = SoundSeBank_Empty;
            m_id = -1;
            m_iopAddr = 0;
            m_spuAddr = 0;
        }
        return m_state;
    }
    if (getState() == SoundSeBank_Freeing) {
        if (force)
            poll();
        return getState();
    }
    m_state = SoundSeBank_Empty;
    return SoundSeBank_Empty;
}

// FS2 0x00147a80

int SoundSeBank::load(const void *xvab, bool wait, u_int iopAddr, u_int spuAddr)
{
    if (xvab == nullptr) {
        m_id = -1;
        return -1;
    }
    if (getState() == SoundSeBank_Loading)
        poll();
    if (getState() == SoundSeBank_Loaded)
        unload(true);
    if (getState() == SoundSeBank_Freeing)
        poll();
    m_state = wait ? SoundSeBank_Loaded : SoundSeBank_Loading;
    m_iopAddr = 0;
    m_spuAddr = 0;
    WaitSema(sSoundSema);
    m_id = rxSndXvabLoad(m_id, xvab, iopAddr, spuAddr);
    if (m_id < 0) {
        m_state = SoundSeBank_Error;
        SignalSema(sSoundSema);
        return m_id;
    }
    if (wait)
        rxSndXvabCheck(m_id, 1);
    m_iopAddr = rxSndXvabGetIopAddr(m_id);
    m_spuAddr = rxSndXvabGetSpuAddr(m_id);
    SignalSema(sSoundSema);
    return m_id;
}

// ──────────────────────────────────────────────────────────────────────
// SoundConfig

inline void SoundEffect::setMode(int mode)
{
    WaitSema(sSoundSema);
    rxSndSetEffectModeCore(core(), mode);
    m_mode = mode;
    SignalSema(sSoundSema);
}

inline void SoundEffect::setDepth(int depthL, int depthR)
{
    WaitSema(sSoundSema);
    rxSndSetEffectDepthCore(core(), depthL, depthR);
    SignalSema(sSoundSema);
    m_depthL = depthL;
    m_depthR = depthR;
}

inline void SoundEffect::setFeedback(int feedback)
{
    WaitSema(sSoundSema);
    rxSndSetEffectFeedbackCore(core(), feedback);
    SignalSema(sSoundSema);
    m_feedback = feedback;
}

inline void SoundEffect::setDelay(int delay)
{
    WaitSema(sSoundSema);
    rxSndSetEffectDelayCore(core(), delay);
    SignalSema(sSoundSema);
    m_delay = delay;
}

inline void SoundConfig::setStereo(bool on)
{
    m_stereo = on;
    WaitSema(sSoundSema);
    rxSndSetOutputMode(m_stereo ? 1 : 0);
    SignalSema(sSoundSema);
}

inline void SoundConfig::setSurround(bool on)
{
    m_surround = on;
    WaitSema(sSoundSema);
    sdpr2StopAll(0);
    sdpr2SetSurround(m_surround ? 1 : 0);
    SignalSema(sSoundSema);
}

// The reverb of core 0, core 1 or both (SoundEffectCore).
// FS2 0x00147be0
// callcheck: WaitSema, rxSndSetEffectModeCore, SignalSema, rxSndSetEffectDepthCore, rxSndSetEffectFeedbackCore, rxSndSetEffectDelayCore (SoundEffect's inline setters)

void SoundConfig::setEffect(int which, int mode, int depthL, int depthR, int feedback, int delay)
{
    if (which == SoundEffectCore_0 || which == SoundEffectCore_Both) {
        SoundEffect &fx = m_effect[0];
        fx.setMode(mode);
        fx.setDepth(depthL, depthR);
        fx.setFeedback(feedback);
        fx.setDelay(delay);
    }
    if (which == SoundEffectCore_1 || which == SoundEffectCore_Both) {
        SoundEffect &fx = m_effect[1];
        fx.setMode(mode);
        fx.setDepth(depthL, depthR);
        fx.setFeedback(feedback);
        fx.setDelay(delay);
    }
}

// Mono: neither stereo nor surround; stereo: stereo only; surround: both.
// FS2 0x00147ee0
// callcheck: WaitSema, rxSndSetOutputMode, SignalSema, sdpr2StopAll, sdpr2SetSurround (setStereo / setSurround inline)

void SoundConfig::setOutputMode(int mode)
{
    m_outputMode = mode;
    switch (m_outputMode) {
    case SoundOutput_Surround:
        if (m_stereo != 1)
            setStereo(true);
        if (m_surround != 1)
            setSurround(true);
        break;
    case SoundOutput_Stereo:
        if (m_stereo != 1)
            setStereo(true);
        if (m_surround != 0)
            setSurround(false);
        break;
    case SoundOutput_Mono:
        if (m_stereo != 0)
            setStereo(false);
        if (m_surround != 0)
            setSurround(false);
        break;
    }
}

// Full volumes, stereo, no reverb on either core.
// FS2 0x00148130
// callcheck: rxSndSetOutputMode, sdpr2StopAll, sdpr2SetSurround, rxSndSetEffectModeCore, rxSndSetEffectDepthCore, rxSndSetEffectFeedbackCore, rxSndSetEffectDelayCore (setStereo / setSurround and SoundEffect's setters inline)

SoundConfig::SoundConfig()
{
    m_masterVolume = kSoundVolumeFull;
    WaitSema(sSoundSema);
    rxSndSetMasterVolume(m_masterVolume, m_masterVolume);
    SignalSema(sSoundSema);
    m_bgmVolume = kSoundVolumeFull;
    WaitSema(sSoundSema);
    rxSndBgmSetMasterVolume(m_bgmVolume, m_bgmVolume);
    SignalSema(sSoundSema);
    m_seVolume = kSoundVolumeFull;
    WaitSema(sSoundSema);
    rxSndSeSetMasterVolume(m_seVolume, m_seVolume);
    SignalSema(sSoundSema);
    m_pcmVolume = kSoundVolumeFull;
    WaitSema(sSoundSema);
    rxSndPcmSetMasterVolume(m_pcmVolume, m_pcmVolume);
    SignalSema(sSoundSema);
    m_adpcmVolume = kSoundVolumeFull;
    WaitSema(sSoundSema);
    rxSndAdpcmSetMasterVolume(m_adpcmVolume, m_adpcmVolume);
    SignalSema(sSoundSema);
    setStereo(true);
    setSurround(false);
    setOutputMode(SoundOutput_Stereo);
    m_effect[0].m_core = 0;
    m_effect[1].m_core = 1;
    for (int i = 0; i < 2; i++) {
        SoundEffect &fx = m_effect[i];
        fx.setMode(0);
        fx.setDepth(0, 0);
        fx.setFeedback(0);
        fx.setDelay(0);
    }
}

// Inline in the ROM (soundShutdown's delete).
SoundConfig::~SoundConfig()
{
    WaitSema(sSoundSema);
    sdpr2StopAll(0);
    rxSndSeStopAll(0);
    rxSndBgmStopAll(0);
    SignalSema(sSoundSema);
}

// ──────────────────────────────────────────────────────────────────────
// Boot, frame and teardown.

// FS2 0x00148500

void soundUpdate()
{
    WaitSema(sSoundSema);
    rxSndUtUpdate();
    rxSndUpdate();
    sdpr2Update();
    SignalSema(sSoundSema);
}

// FS2 0x00148540
// callcheck: sdpr2StopAll, rxSndSeStopAll, rxSndBgmStopAll, WaitSema, SignalSema, operator_delete, spuMemReserve (~SoundConfig inline in the delete-expression; spuMemInit inline)

void soundShutdown()
{
    rxThRemoveCallback(soundThreadLock);
    if (gSoundConfig != nullptr) {
        delete gSoundConfig;
        gSoundConfig = nullptr;
    }
    rxSndInitModules();
    spuMemInit();
    if (sSoundSema >= 0) {
        DeleteSema(sSoundSema);
        sSoundSema = -1;
    }
    if (sIopSema >= 0) {
        DeleteSema(sIopSema);
        sIopSema = -1;
    }
    if (sSpuSema >= 0) {
        DeleteSema(sSpuSema);
        sSpuSema = -1;
    }
}

// The semaphores, rx's memory hooks over the game's allocators, rx and
// its stream layers, SDPR2 (pair mode and tick by the video mode) and the
// settings object.
// FS2 0x001486e0
// callcheck: operator_new, SoundConfig, spuMemReserve (the new-expression; spuMemInit inline)

void soundInit(int isPal)
{
    // PC: the ROM leaves currentCount, numWaitThreads and attr as they
    // were on the stack.
    SemaParam param = {};
    param.initCount = 1;
    param.maxCount = 1;
    param.option = 0;
    sSoundSema = CreateSema(&param);
    sIopSema = CreateSema(&param);
    sSpuSema = CreateSema(&param);
    rxSndSetIopAllocHook(soundIopAllocLow);
    rxSndSetIopAllocMode1Hook(soundIopAllocHigh);
    rxSndSetIopFreeHook(soundIopFree);
    spuMemInit();
    rxSndSetSpuAllocHook(spuMemAllocLow);
    rxSndSetSpuAllocMode1Hook(spuMemAllocHigh);
    rxSndSetSpuFreeHook(spuMemFree);
    rxSndInit();
    rxSndUtInit();
    rxSndSetDebugFlags(0);
    rxSndUtSetDebugFlags(0);
    rxSndSetSysParam(kSoundSysParam0, 0);
    rxSndSetSysParam(kSoundSysParam1, 1);
    rxSndSetSysParam(kSoundSysParam2, 1);
    sdpr2Init(kSdpr2Config);
    if (!isPal) {
        rxSndSetFrameRate(kSoundRateNtsc);
        sdpr2SetPairMode(kSdpr2PairModeNtsc);
    } else {
        rxSndSetFrameRate(kSoundRatePal);
        sdpr2SetPairMode(kSdpr2PairModePal);
    }
    gSoundConfig = new SoundConfig();
    rxThAddCallback(soundThreadLock);
}

// FS2 0x001488f0

void soundThreadLock(int lock)
{
    if (lock == 0)
        SignalSema(sSoundSema);
    else
        WaitSema(sSoundSema);
}

// FS2 0x00148920

void spuMemFree(u_int addr)
{
    WaitSema(sSpuSema);
    for (SpuMemBlock *block = &sSpuMem.head; block != nullptr; block = block->next) {
        if (block->size != 0 && block->start == addr) {
            block->prev->next = block->next;
            block->next->prev = block->prev;
            memset(block, 0, sizeof(*block));
            break;
        }
    }
    SignalSema(sSpuSema);
}

// FS2 0x001489d0

u_int spuMemAllocHigh(int size)
{
    WaitSema(sSpuSema);
    u_int addr = spuMemAlloc(size, true);
    SignalSema(sSpuSema);
    return addr;
}

// FS2 0x00148a20

u_int spuMemAllocLow(int size)
{
    WaitSema(sSpuSema);
    u_int addr = spuMemAlloc(size, false);
    SignalSema(sSpuSema);
    return addr;
}

// FS2 0x00148a70

u_int spuMemReserve(int size, u_int addr)
{
    SpuMemBlock *block = spuMemNewBlock();
    int n = spuMemRound(size);
    if (n == 0 || sSpuMem.head.next == nullptr)
        return 0;
    // PC: the ROM writes through a null node when all 32 are taken.
    if (block == nullptr)
        return 0;
    SpuMemBlock *at = &sSpuMem.head;
    for (SpuMemBlock *next = at->next; next != nullptr; at = next, next = next->next) {
        if (at->end <= addr && addr + (u_int)n <= next->start) {
            block->size = n;
            block->start = addr;
            block->end = addr + n;
            spuMemLinkAfter(block, at);
            return block->start;
        }
    }
    return 0;
}

// FS2 0x00148b60

u_int spuMemAlloc(int size, bool fromTop)
{
    SpuMemBlock *block = spuMemNewBlock();
    int n = spuMemRound(size);
    if (n == 0)
        return 0;
    // PC: the ROM writes through a null node when all 32 are taken.
    if (block == nullptr)
        return 0;
    if (!fromTop) {
        SpuMemBlock *at = &sSpuMem.head;
        for (SpuMemBlock *next = sSpuMem.head.next; next != nullptr; next = next->next) {
            if (n <= (int)next->start - (int)at->end) {
                block->size = n;
                block->start = at->end;
                block->end = block->start + n;
                spuMemLinkAfter(block, at);
                return block->start;
            }
            at = next;
        }
    } else {
        SpuMemBlock *at = &sSpuMem.tail;
        for (SpuMemBlock *prev = sSpuMem.tail.prev; prev != nullptr; prev = prev->prev) {
            if (n <= (int)at->start - (int)prev->end) {
                block->size = n;
                block->end = at->start;
                block->start = at->start - n;
                spuMemLinkBefore(block, at);
                return block->start;
            }
            at = prev;
        }
    }
    return 0;
}

// PC: the host's sif heap must answer IOP addresses that fit 32 bits (rx
// keeps them in u_int).
// FS2 0x00148cd0

void soundIopFree(u_int addr)
{
    WaitSema(sIopSema);
    sceSifFreeSysMemory((void *)(uintptr_t)addr);
    SignalSema(sIopSema);
}

// FS2 0x00148d10

u_int soundIopAllocHigh(int size)
{
    WaitSema(sIopSema);
    u_int addr = (u_int)(uintptr_t)sceSifAllocSysMemory(1, (u_int)size, nullptr);
    SignalSema(sIopSema);
    return addr;
}

// FS2 0x00148d60

u_int soundIopAllocLow(int size)
{
    WaitSema(sIopSema);
    u_int addr = (u_int)(uintptr_t)sceSifAllocSysMemory(0, (u_int)size, nullptr);
    SignalSema(sIopSema);
    return addr;
}
