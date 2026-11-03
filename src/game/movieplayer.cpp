// game/movieplayer.cpp -- the MPEG movie player the prgMovie widget plays
// (game/movieplayer.h): a PSS file through a file ring, libmpeg's demuxer
// into video / audio / data rings, the IPU fed from the video ring, the
// audio pumped into rx streams, one RGB32 picture uploaded per frame.
//
// PC: no IPU and no GS memory on the host. The toIPU DMA registers are
// written as the ROM does (write-only on the host); the host libmpeg
// decodes from the demuxer, so feedIpu retires only what its decoder has
// parsed (RosePan's rxMpegNodataCallback did the same). The picture goes
// to the host draw module as this player's texture (YamiPan_DrawUiMovie):
// the macroblocks are decoded at their source, never uploaded.

#include "game/movieplayer.h"

#include <stdint.h>
#include <string.h>

#include "game/gamepacket.h"
#include "game/gamesystem.h"
#include "game/heap.h"
#include "rx/rxthread.h"
#include "sdk/eekernel.h"
#include "sdk/eeregs.h"
#include "sdk/libgraph.h"
#include "sdk/libmpeg_video.h"
#include "yamipan/rendering/yamipan_drawui.h"

// The work area's blocks and the rings carved from them.
constexpr int kMovieWorkAlign = 0x80;
constexpr int kMovieVideoRingSize = 0x30000;
constexpr int kMovieAudioRingSize = 0x20000;
// sceMpegCreate's work beyond the picture: w * h * 9 / 2 + this.
constexpr int kMovieMpegWorkExtra = 0x2800;

// demux asks the file ring for this much at a time.
constexpr int kMovieDemuxChunk = 0x4000;
// feedIpu keeps this much video ahead of the IPU and sends at most this
// much per DMA; isDrained calls the video ring empty below it.
constexpr int kMovieIpuBlock = 0x1000;
// The file ring is spent when fewer bytes than these are left: demux
// (file end) and feedIpu (the end code).
constexpr int kMovieFileEndSlack = 0x10;
constexpr int kMovieEndCodeSlack = 5;
// An audio ring ends this far before its SSbd size (the tail the SPU
// stream never asks for: guess).
constexpr int kMovieAudioTail = 0x4000;

// The full volume of a channel.
constexpr int kMovieVolumeFull = 0x80;

// prime draws the first picture into the movie's own buffer: tbp 0x700,
// 512 x 512 at (0, 0), in the last packet list.
constexpr int kMoviePrimeTbp = 0x700;
constexpr int kMoviePrimeSize = 0x200;

// The toIPU DMA: STR with tag transfer off (CHCR), its busy bit.
constexpr u_int kIpuDmaStart = 0x101;
constexpr u_int kIpuDmaBusy = 0x100;

static u_char sMpegInitDone;    /* sbss 0x31ae28 */
static int sDemuxed;            /* sbss 0x31ae2c: bytes demuxed, all movies */

// Four MPEG sequence end codes: what the IPU gets once the file is spent.
alignas(16) static const u_char sIpuEndCode[16] = { /* data 0x2fa440 */
    0x00, 0x00, 0x01, 0xb7, 0x00, 0x00, 0x01, 0xb7,
    0x00, 0x00, 0x01, 0xb7, 0x00, 0x00, 0x01, 0xb7,
};

// The libmpeg callbacks open registers (defined after it, as in the ROM).
static int movieRestartDmaCallback(sceMpeg *mp, sceMpegCbData *data, void *arg);
static int movieStopDmaCallback(sceMpeg *mp, sceMpegCbData *data, void *arg);
static int movieBackgroundCallback(sceMpeg *mp, sceMpegCbData *data, void *arg);
static int movieErrorCallback(sceMpeg *mp, sceMpegCbData *data, void *arg);
static int movieNodataCallback(sceMpeg *mp, sceMpegCbData *data, void *arg);
static int movieDataCallback(sceMpeg *mp, sceMpegCbData *data, void *arg);
static int movieAudioCallback(sceMpeg *mp, sceMpegCbData *data, void *arg);
static int movieVideoCallback(sceMpeg *mp, sceMpegCbData *data, void *arg);

static int alignWork(int size)
{
    return (size + kMovieWorkAlign - 1) / kMovieWorkAlign * kMovieWorkAlign;
}

// The movie's pixels rounded up to whole macroblocks.
static int macroblockPixels(int width, int height)
{
    return (width + 15) / 16 * 16 * ((height + 15) / 16 * 16);
}

// The RGB32 picture of that many pixels.
static int pictureBytes(int pixels)
{
    return (int)(((u_int)pixels & 0x3fffff) << 2);
}

// A feeder still playing: neither idle, stopping nor ended.
static bool feederActive(const SoundFeeder *feeder)
{
    u_int state = (u_int)feeder->m_state;
    return !(state < 5 && ((1u << state) &
             (1u << SoundFeeder_Idle | 1u << SoundFeeder_Stopping | 1u << SoundFeeder_Ended)));
}

// ──────────────────────────────────────────────────────────────────────
// MovieRing: a StreamRing in memory, written by the demux callbacks.

MovieRing::MovieRing(int size, void *mem, int threshold)
    : StreamRing(size, mem, threshold)
{
    m_writePos = 0;
    m_readPos = 0;
    m_filled = 0;
    _20 = 0;
    m_written = 0;
    m_consumed = 0;
    m_pending = 0;
    m_endPos = 0;
}

MovieAudioRing::MovieAudioRing(int size, void *mem, int threshold, int channel, MoviePlayer *player)
    : MovieRing(size, mem, threshold)
{
    m_channel = channel;
    m_player = player;
    m_headerSize = 0;
    memset(&m_header, 0, sizeof(m_header));
}

// ──────────────────────────────────────────────────────────────────────
// MoviePlayer: the slot accessors

// FS2 0x001faa30

void MoviePlayer::setVolume(int channel, int left, int right)
{
    m_volume[channel][0] = left;
    m_volume[channel][1] = right;
    SoundFeeder *feeder = m_feeders[channel];
    if (feeder != nullptr) {
        feeder->m_volumeL = left;
        feeder->m_volumeR = right;
    }
}

// FS2 0x001faa70

int MoviePlayer::getFrame()
{
    if (!(m_state & MovieState_Playing))
        return -1;
    return m_mpeg.frameCount;
}

// ──────────────────────────────────────────────────────────────────────
// Once a picture is there: the channels get their volumes and start.
// FS2 0x001faa90

int MoviePlayer::startAudio()
{
    u_int state = m_state;
    if (state == 0)
        return -1;
    if (!(state & MovieState_Ready))
        return -2;
    if (state & MovieState_Playing)
        return 0;
    m_state = state | MovieState_Playing;
    for (int i = 0; i < kMovieMaxChannels; i++) {
        SoundFeeder *feeder = m_feeders[i];
        if (feeder != nullptr) {
            feeder->m_volumeL = m_volume[i][0];
            feeder->m_volumeR = m_volume[i][1];
            m_feeders[i]->start();
        }
    }
    return 1;
}

// FS2 0x001fab30

bool MoviePlayer::isPlaying()
{
    return (m_state & MovieState_Playing) != 0;
}

// FS2 0x001fab40

bool MoviePlayer::isReady()
{
    return (m_state & MovieState_Ready) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// libmpeg wants more video: retire what the last DMA took, demux until
// the video ring holds a block (or the file ends), and DMA the next block
// -- or the end code once the file is spent. False once the file ended.
// FS2 0x001fab50
// callcheck: consume, filled, contiguous, readPtr, endPos (MovieRing / FileStreamRing slots)

bool MoviePlayer::feedIpu(sceMpeg *mp, sceMpegCbData *data)
{
    WaitSema(m_sema);
    while (REG_DMAC_4_IPU_TO_CHCR & kIpuDmaBusy)
        ;
    MovieRing *video = m_videoRing;
    video->consume(video->m_pending);
    video->m_pending = 0;
    if (sceMpegIsEnd(mp)) {
        demux(kMovieDemuxChunk);
        SignalSema(m_sema);
        return true;
    }
    SignalSema(m_sema);

    int filled = m_videoRing->filled();
    while (filled < kMovieIpuBlock && !(m_state & MovieState_FileEnd) && !sceMpegIsEnd(mp)) {
        demux(kMovieDemuxChunk);
        if (m_state & MovieState_Abort)
            return false;
        filled = m_videoRing->filled();
    }

    WaitSema(m_sema);
    int n = m_videoRing->contiguous();
    if ((m_state & MovieState_FileEnd) && isDrained()) {
        n += 15;
        m_state |= MovieState_Drained;
    }
    n &= ~15;
    if (n > kMovieIpuBlock)
        n = kMovieIpuBlock;
    // PC: the host decoder took the stream from the demuxer; only what it
    // has parsed leaves the ring, so the ring's room paces the demux.
    int parsed = (m_videoRing->filled() - YamiPanMpegVideoPending()) & ~15;
    if (n > parsed)
        n = parsed < 0 ? 0 : parsed;
    u_char *src = m_videoRing->readPtr();
    int consumed = m_reader->m_consumed;
    int end = m_reader->endPos();
    if (n < 1) {
        if (end - consumed < kMovieEndCodeSlack) {
            FlushCache(0);
            REG_DMAC_4_IPU_TO_QWC = 1;
            REG_DMAC_4_IPU_TO_MADR = (u_int)(uintptr_t)sIpuEndCode;
            REG_DMAC_4_IPU_TO_CHCR = kIpuDmaStart;
        }
    } else {
        FlushCache(0);
        REG_DMAC_4_IPU_TO_QWC = n / 16;
        REG_DMAC_4_IPU_TO_MADR = (u_int)(uintptr_t)src;
        REG_DMAC_4_IPU_TO_CHCR = kIpuDmaStart;
    }
    m_videoRing->m_pending += n;
    SignalSema(m_sema);
    return !(m_state & MovieState_FileEnd);
}

// ──────────────────────────────────────────────────────────────────────
// Every audio ring (with a feeder) and the data ring consumed to their
// end, and the video ring below a block.
// FS2 0x001fadc0
// callcheck: endPos, filled (MovieRing slots)

bool MoviePlayer::isDrained()
{
    bool drained = true;
    for (int i = 0; i < kMovieMaxChannels; i++) {
        if (m_feeders[i] != nullptr) {
            int consumed = m_audioRings[i]->m_consumed;
            if (consumed < m_audioRings[i]->endPos()) {
                drained = false;
                break;
            }
        }
    }
    if (m_dataRing != nullptr) {
        int consumed = m_dataRing->m_consumed;
        if (consumed < m_dataRing->endPos())
            drained = false;
    }
    if (m_videoRing->filled() >= kMovieIpuBlock)
        drained = false;
    return drained;
}

// ──────────────────────────────────────────────────────────────────────
// Waits for want bytes of the file in the reader (or its end), demuxes
// what is there and consumes what the demuxer took. Once the file ended,
// only watches for the rings to drain.
// FS2 0x001faec0
// callcheck: poll, filled, endPos, readPtr, consume (FileStreamRing slots)

int MoviePlayer::demux(int want)
{
    if (m_state & MovieState_FileEnd) {
        if (isDrained())
            m_state |= MovieState_Drained;
        return 0;
    }
    m_reader->poll();
    m_reader->filled();     // the ROM reads it and drops it
    if (want > 0) {
        for (;;) {
            int filled = m_reader->filled();
            int consumed = m_reader->m_consumed;
            int end = m_reader->endPos();
            int left = end - consumed;
            if (end == consumed) {
                m_state |= MovieState_FileEnd;
                return 0;
            }
            if (want <= filled || (left >= 0 && left <= filled))
                break;
            m_reader->poll();
        }
    }
    u_char *src = m_reader->readPtr();
    int filled = m_reader->filled();
    u_char *ring = m_reader->m_data;
    int n = sceMpegDemuxPssRing(&m_mpeg, src, filled, ring, m_reader->size());
    m_reader->consume(n);
    sDemuxed += n;
    if (m_state & MovieState_Abort)
        return 0;
    int consumed = m_reader->m_consumed;
    int left = m_reader->endPos() - consumed;
    if (left >= 0 && left < kMovieFileEndSlack)
        m_state |= MovieState_FileEnd;
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// One audio packet for ring's channel. Its first 0x28 bytes are the
// SShd / SSbd header: once complete it opens the sink (PCM or ADPCM,
// mono / stereo / interleaved) and wires the channel's feeder; the rest
// goes into the ring. False when it does not fit (libmpeg retries).
// FS2 0x001fb0b0
// callcheck: operator_new, SoundPcmSink, SoundAdpcmSink (the new-expressions; open, freeSpace and write are slots)

bool MoviePlayer::receiveAudio(sceMpeg *mp, sceMpegCbDataStr *data, MovieAudioRing *ring)
{
    bool ok = true;
    u_int len = data->len - 4;
    const u_char *src = data->data + 4;
    if (m_feeders[ring->m_channel] == nullptr)
        return true;
    m_feeders[ring->m_channel]->update();
    if (ring->m_headerSize < sizeof(MovieAudioHeader)) {
        u_int n = sizeof(MovieAudioHeader) - ring->m_headerSize;
        if (len < n)
            n = len;
        memcpy((u_char *)&ring->m_header + ring->m_headerSize, src, n);
        len -= n;
        ring->m_headerSize += n;
        src += n;
        if (ring->m_headerSize == sizeof(MovieAudioHeader)) {
            const MovieAudioHeader &header = ring->m_header;
            ring->m_endPos = header.bodySize - kMovieAudioTail;
            SoundSink *sink;
            if (header.type < kMovieAudioTypeAdpcm)
                sink = new SoundPcmSink();
            else
                sink = new SoundAdpcmSink();
            int mode;
            if (header.channels < 2) {
                mode = 0;
            } else {
                mode = 1;
                if (header.interleave == 0x10 && header.channels == 2)
                    mode = 2;
            }
            sink->open(mode, header.rate, 0, 4, 0, 4, header.interleave);
            SoundFeeder *feeder = m_feeders[ring->m_channel];
            feeder->update();
            feeder->setSource(ring);
            feeder->setSink(sink);
        }
    }
    rxThDisableDispatch();
    if (len != 0) {
        if ((u_int)ring->freeSpace() < len)
            ok = false;
        else
            ring->write(src, len);
    }
    rxThEnableDispatch();
    return ok;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001fb2d0

bool MoviePlayer::drawFrame(sceVif1Packet *packet, int tbp, int width, int height, int x, int y)
{
    bool ok = decodeFrame();
    if (ok)
        uploadFrame(packet, tbp, width, height, x, y);
    return ok;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001fb380
// callcheck: poll (FileStreamRing slot)

bool MoviePlayer::prime()
{
    if (m_reader != nullptr) {
        m_reader->poll();
        if (!(m_state & MovieState_Primed) && !m_reader->isPrimed())
            return false;
    }
    bool ok = decodeFrame();
    if (ok) {
        sceVif1Packet *packet = gGameSystem->m_packets.openList(PacketList_Finish);
        uploadFrame(packet, kMoviePrimeTbp, kMoviePrimeSize, kMoviePrimeSize, 0, 0);
        gGameSystem->m_packets.closeList();
    }
    return ok;
}

// ──────────────────────────────────────────────────────────────────────
// The picture into the buffer at tbp (width / 64 pages wide) at (x, y):
// the macroblocks both the movie and width x height hold, a column at a
// time, then a TEXFLUSH.
// PC: the ROM sends each 16 x 16 macroblock by sceVif1PkRefLoadImage
// (PSMCT32, 0x40 qw) to (x + 16 * column, y + 16 * row), then a DMA cnt /
// DIRECT / A+D TEXFLUSH by hand; the host draw module takes the decoded
// macroblocks (column-major, as sent) as this player's texture instead.
// FS2 0x001fb450
// callcheck: sceVif1PkRefLoadImage (PC: YamiPan_DrawUiMovie)

void MoviePlayer::uploadFrame(sceVif1Packet *packet, int tbp, int width, int height, int x, int y)
{
    int columns = m_mpeg.width / 16;
    if (width / 16 < columns)
        columns = width / 16;
    int rows = m_mpeg.height / 16;
    if (height / 16 < rows)
        rows = height / 16;
    int tbw = width / 64;
    if (columns > 0 && rows > 0)
        YamiPan_DrawUiMovie(packet, this, m_picture, columns, rows, tbp, tbw & 0xffff, x, y);
    YamiPan_DrawUiReg(packet, SCE_GS_TEXFLUSH, 0);
}

void MoviePlayer::updateFeeders()
{
    for (int i = 0; i < kMovieMaxChannels; i++) {
        if (m_feeders[i] != nullptr)
            m_feeders[i]->update();
    }
}

// ──────────────────────────────────────────────────────────────────────
// One frame of the player: demux, poll the rings, keep the sound fed and
// take the next step -- open -> primed once the file ring is, primed ->
// the first picture (ready), playing -> the next picture. Once drained it
// waits for the sound to end and is over. True when a picture is there
// to draw.
// FS2 0x001fb630
// callcheck: update, poll (SoundFeeder::update in the inline updateFeeders; poll is a StreamRing slot)

bool MoviePlayer::decodeFrame()
{
    if ((m_state & MovieState_Drained) || (m_state & MovieState_Over)) {
        updateFeeders();
        for (int i = 0; i < kMovieMaxChannels; i++) {
            if (m_feeders[i] != nullptr && feederActive(m_feeders[i]))
                return true;
        }
        m_state |= MovieState_Over;
        return false;
    }

    demux(kMovieDemuxChunk);
    m_reader->poll();
    if (!(m_state & MovieState_Closing) && (m_state & MovieState_Playing))
        updateFeeders();
    if (m_videoRing != nullptr)
        m_videoRing->poll();
    if (m_dataRing != nullptr)
        m_dataRing->poll();

    u_int state = m_state;
    if (state & MovieState_Closing)
        return false;
    if (state & MovieState_Playing) {
        updateFeeders();
        if (!sceMpegIsEnd(&m_mpeg))
            sceMpegGetPicture(&m_mpeg, m_picture, (u_int)m_pictureSize >> 10);
        else
            demux(kMovieDemuxChunk);
        updateFeeders();
        return true;
    }
    if (state & MovieState_Ready)
        return false;
    if (state & MovieState_Primed) {
        if (!sceMpegIsEnd(&m_mpeg))
            sceMpegGetPicture(&m_mpeg, m_picture, (u_int)m_pictureSize >> 10);
        else
            demux(kMovieDemuxChunk);
        m_state |= MovieState_Ready;
        return false;
    }
    if (state & MovieState_Open) {
        if (m_reader->isPrimed())
            m_state |= MovieState_Primed;
        return false;
    }
    return true;
}

// FS2 0x001fb920

bool MoviePlayer::testState(u_int mask)
{
    if (mask == 0)
        return m_state == 0;
    return (m_state & mask) != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Stops the movie and deletes its file ring (not cleared: open makes a
// new one).
// FS2 0x001fb950
// callcheck: ~FileStreamRing (deleted through its vtable)

void MoviePlayer::close()
{
    // The ROM marks a running movie closing twice (an inline helper
    // called twice) before it clears the state.
    if (m_state != 0 && !(m_state & MovieState_Closing))
        m_state |= MovieState_Closing;
    if (m_state != 0 && !(m_state & MovieState_Closing))
        m_state |= MovieState_Closing;
    m_state = 0;
    if (m_reader != nullptr)
        delete m_reader;
}

// ──────────────────────────────────────────────────────────────────────
// Opens name: the file ring (primed at 3/4), the video ring (and the data
// ring when one is set up), an audio ring per channel, the decoder over
// its work block with the stream and event callbacks, a feeder per
// channel. < 0 when the file does not open.
// FS2 0x001fb9d0
// callcheck: StreamRing, operator_new (MovieRing / MovieAudioRing constructors inline)

int MoviePlayer::open(const char *name)
{
    if (m_state != 0 && !(m_state & MovieState_Closing))
        m_state |= MovieState_Closing;
    if (m_state != 0 && !(m_state & MovieState_Closing))
        m_state |= MovieState_Closing;
    m_state = 0;

    m_reader = new FileStreamRing(m_readerSize, m_readerMem, m_readerSize / 4 * 3);
    if (m_reader->open(name) < 0)
        return -1;
    m_videoRing = new MovieRing(m_videoSize, m_videoMem, m_videoSize / 2);
    if (m_dataSize != 0)
        m_dataRing = new MovieRing(m_dataSize, m_dataMem, m_dataSize / 2);
    for (int i = 0; i < m_channels; i++)
        m_audioRings[i] = new MovieAudioRing(kMovieAudioRingSize, m_audioMem + i * kMovieAudioRingSize,
                                             kMovieAudioRingSize / 2, i, this);

    sceMpegCreate(&m_mpeg, m_mpegWork, m_mpegWorkSize);
    sceMpegAddStrCallback(&m_mpeg, sceMpegStrM2V, 0, movieVideoCallback, this);
    for (int i = 0; i < m_channels; i++) {
        sceMpegAddStrCallback(&m_mpeg, sceMpegStrPCM, i, movieAudioCallback, m_audioRings[i]);
        sceMpegAddStrCallback(&m_mpeg, sceMpegStrADPCM, i, movieAudioCallback, m_audioRings[i]);
    }
    if (m_dataMem != nullptr)
        sceMpegAddStrCallback(&m_mpeg, sceMpegStrDATA, 0, movieDataCallback, this);
    sceMpegAddCallback(&m_mpeg, sceMpegCbNodata, movieNodataCallback, this);
    sceMpegAddCallback(&m_mpeg, sceMpegCbError, movieErrorCallback, this);
    sceMpegAddCallback(&m_mpeg, sceMpegCbBackground, movieBackgroundCallback, this);
    sceMpegAddCallback(&m_mpeg, sceMpegCbStopDMA, movieStopDmaCallback, this);
    sceMpegAddCallback(&m_mpeg, sceMpegCbRestartDMA, movieRestartDmaCallback, this);

    for (int i = 0; i < m_channels; i++)
        m_feeders[i] = new SoundFeeder();
    m_state |= MovieState_Open;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The libmpeg callbacks (arg: the player, or the audio channel's ring).

// FS2 0x001fbe00
// callcheck: sceIpuRestartDMA (no IPU on the host)

static int movieRestartDmaCallback(sceMpeg *mp, sceMpegCbData *data, void *arg)
{
    // PC: sceIpuRestartDMA(&player->m_ipuDmaEnv) -- no IPU on the host.
    return 1;
}

// FS2 0x001fbe20
// callcheck: sceIpuStopDMA (no IPU on the host)

static int movieStopDmaCallback(sceMpeg *mp, sceMpegCbData *data, void *arg)
{
    // PC: sceIpuStopDMA(&player->m_ipuDmaEnv) -- no IPU on the host.
    return 1;
}

// FS2 0x001fbe40

static int movieBackgroundCallback(sceMpeg *mp, sceMpegCbData *data, void *arg)
{
    return 1;
}

// FS2 0x001fbe50

static int movieErrorCallback(sceMpeg *mp, sceMpegCbData *data, void *arg)
{
    return 1;
}

// FS2 0x001fbe60

static int movieNodataCallback(sceMpeg *mp, sceMpegCbData *data, void *arg)
{
    return static_cast<MoviePlayer *>(arg)->feedIpu(mp, data);
}

// The data stream into the data ring when it has room.
// FS2 0x001fbe80
// callcheck: freeSpace, write (MovieRing slots)

static int movieDataCallback(sceMpeg *mp, sceMpegCbData *data, void *arg)
{
    MoviePlayer *player = static_cast<MoviePlayer *>(arg);
    int ok = 1;
    if (player->m_dataRing != nullptr) {
        u_int len = data->str.len;
        if ((u_int)player->m_dataRing->freeSpace() < len)
            ok = 0;
        else
            player->m_dataRing->write(data->str.data, len);
    }
    return ok;
}

// FS2 0x001fbf10

static int movieAudioCallback(sceMpeg *mp, sceMpegCbData *data, void *arg)
{
    MovieAudioRing *ring = static_cast<MovieAudioRing *>(arg);
    return ring->m_player->receiveAudio(mp, &data->str, ring);
}

// The video stream into the video ring (under the player's semaphore:
// feedIpu consumes it) when it has room.
// FS2 0x001fbf30
// callcheck: freeSpace, write (MovieRing slots)

static int movieVideoCallback(sceMpeg *mp, sceMpegCbData *data, void *arg)
{
    MoviePlayer *player = static_cast<MoviePlayer *>(arg);
    WaitSema(player->m_sema);
    u_int len = data->str.len;
    u_int room = player->m_videoRing->freeSpace();
    if (len <= room)
        player->m_videoRing->write(data->str.data, len);
    SignalSema(player->m_sema);
    return len <= room;
}

// ──────────────────────────────────────────────────────────────────────
// The work area open needs: the file ring (the picture's size), the video
// ring, the picture, the decoder's work and the audio rings, each rounded
// to 128 bytes, plus 128.
// FS2 0x001fbfd0

int MoviePlayer::workSize()
{
    int pixels = macroblockPixels(m_width, m_height);
    int size = alignWork(pixels * 4);
    size = alignWork(size + kMovieVideoRingSize);
    size = alignWork(size + pictureBytes(pixels));
    size = alignWork(size + m_width * m_height * 9 / 2 + kMovieMpegWorkExtra);
    size = alignWork(size + m_channels * kMovieAudioRingSize);
    return size + kMovieWorkAlign;
}

// The next size bytes of the work area, null when they do not fit.
void *MoviePlayer::carve(int size)
{
    if (m_work == nullptr)
        return nullptr;
    u_char *block = m_carve;
    if (m_workSize - (int)(block - m_work) < size)
        return nullptr;
    m_carve = block + alignWork(size);
    return block;
}

// ──────────────────────────────────────────────────────────────────────
// Cuts the work area into the blocks workSize counts; -1 without one.
// FS2 0x001fc0e0

int MoviePlayer::carveWorkArea()
{
    freeStreams();
    if (m_work == nullptr)
        return -1;
    int pixels = macroblockPixels(m_width, m_height);
    m_readerSize = pixels * 4;
    m_readerMem = static_cast<u_char *>(carve(m_readerSize));
    m_videoSize = kMovieVideoRingSize;
    m_videoMem = static_cast<u_char *>(carve(m_videoSize));
    m_pictureSize = pictureBytes(macroblockPixels(m_width, m_height));
    m_picture = static_cast<sceIpuRGB32 *>(carve(m_pictureSize));
    m_mpegWorkSize = m_width * m_height * 9 / 2 + kMovieMpegWorkExtra;
    m_mpegWork = static_cast<u_char *>(carve(m_mpegWorkSize));
    m_audioSize = m_channels * kMovieAudioRingSize;
    m_audioMem = static_cast<u_char *>(carve(m_audioSize));
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Deletes the rings and the feeders (with their sinks) and restarts the
// carve.
// FS2 0x001fc3a0
// callcheck: ~SoundFeeder (the delete-expression; the rings and sinks are deleted through their vtables)

void MoviePlayer::freeStreams()
{
    m_carve = m_work;
    m_readerSize = 0;
    m_readerMem = nullptr;
    m_videoSize = 0;
    m_videoMem = nullptr;
    m_pictureSize = 0;
    m_picture = nullptr;
    m_mpegWorkSize = 0;
    m_mpegWork = nullptr;
    m_audioSize = 0;
    m_audioMem = nullptr;
    if (m_videoRing != nullptr) {
        delete m_videoRing;
        m_videoRing = nullptr;
    }
    for (int i = 0; i < kMovieMaxChannels; i++) {
        if (m_feeders[i] != nullptr) {
            SoundSink *sink = m_feeders[i]->m_sink;
            delete m_feeders[i];
            m_feeders[i] = nullptr;
            if (sink != nullptr)
                delete sink;
        }
        if (m_audioRings[i] != nullptr) {
            delete m_audioRings[i];
            m_audioRings[i] = nullptr;
        }
    }
    if (m_dataRing != nullptr) {
        delete m_dataRing;
        m_dataRing = nullptr;
    }
}

// FS2 0x001fc4e0
// callcheck: release, ~StreamRing, operator_delete (MovieRing's destructor inline, then the base's)

MovieAudioRing::~MovieAudioRing()
{
}

// ──────────────────────────────────────────────────────────────────────
// mem null: workSize from the global heap (freed again by the next call
// or the destructor).
// FS2 0x001fc560

void MoviePlayer::setWorkArea(void *mem, int size)
{
    if (m_workOwned)
        gGlobalHeap.free(m_work);
    m_work = nullptr;
    m_workOwned = 0;
    m_workSize = 0;
    freeStreams();
    m_work = static_cast<u_char *>(mem);
    m_workSize = size;
    m_workOwned = 0;
    if (mem == nullptr) {
        m_workSize = workSize();
        m_work = static_cast<u_char *>(gGlobalHeap.alloc(m_workSize, kMovieWorkAlign));
        m_workOwned = 1;
    }
    freeStreams();
    carveWorkArea();
}

// FS2 0x001fc720
// callcheck: operator_delete (the deleting destructor)

MoviePlayer::~MoviePlayer()
{
    DeleteSema(m_sema);
    m_sema = 0;
    close();
    if (m_workOwned)
        gGlobalHeap.free(m_work);
    m_work = nullptr;
    m_workOwned = 0;
    m_workSize = 0;
    freeStreams();
}

// ──────────────────────────────────────────────────────────────────────
// sceMpegInit runs once for all players; every channel at full volume.
// FS2 0x001fc7b0

MoviePlayer::MoviePlayer(int width, int height, int audioChannels)
{
    m_width = width;
    m_height = height;
    m_channels = audioChannels;
    m_workSize = 0;
    m_work = nullptr;
    m_workOwned = 0;
    m_dataSize = 0;
    m_dataMem = nullptr;
    _6c = 0;
    m_state = 0;
    // PC: the ROM leaves the reader as the heap gave it (open sets it
    // before anything reads it; the destructor deletes it).
    m_reader = nullptr;
    // PC: the ROM leaves currentCount, numWaitThreads, attr and option as
    // they were on the stack.
    SemaParam param = {};
    param.maxCount = 1;
    param.initCount = 1;
    m_sema = CreateSema(&param);
    m_videoRing = nullptr;
    for (int i = 0; i < kMovieMaxChannels; i++) {
        m_audioRings[i] = nullptr;
        m_feeders[i] = nullptr;
    }
    m_dataRing = nullptr;
    freeStreams();
    if (!sMpegInitDone) {
        sceMpegInit();
        sMpegInitDone = 1;
    }
    for (int i = 0; i < kMovieMaxChannels; i++) {
        m_volume[i][0] = kMovieVolumeFull;
        m_volume[i][1] = kMovieVolumeFull;
        SoundFeeder *feeder = m_feeders[i];
        if (feeder != nullptr) {
            feeder->m_volumeL = kMovieVolumeFull;
            feeder->m_volumeR = kMovieVolumeFull;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// MovieRing's slots (CodeWarrior emitted them after the player).

// FS2 0x001fc8b0

int MovieRing::endPos()
{
    return m_endPos;
}

// FS2 0x001fc8c0

int MovieRing::poll()
{
    return 0;
}

// FS2 0x001fc8d0

int MovieRing::consume(int size)
{
    m_filled -= size;
    m_readPos = (m_readPos + size) % this->size();
    m_consumed += size;
    return m_consumed;
}

// FS2 0x001fc930

int MovieRing::readPos()
{
    return m_readPos;
}

// FS2 0x001fc940

u_char *MovieRing::readPtr()
{
    return m_data + m_readPos;
}

// ──────────────────────────────────────────────────────────────────────
// Copies in up to the wrap and around it. The caller checked freeSpace.
// FS2 0x001fc950

void MovieRing::write(const void *src, int size)
{
    const u_char *from = static_cast<const u_char *>(src);
    for (int left = size; left > 0;) {
        int pos = m_writePos;
        int n = this->size() - pos;
        if (left <= n)
            n = left;
        if (n < 1)
            n = 0;
        memcpy(m_data + pos, from, n);
        from += n;
        m_writePos = (m_writePos + n) % this->size();
        m_filled += n;
        left -= n;
    }
    m_written += size;
}

// FS2 0x001fca30
// callcheck: filled (the slot, through the vtable)

int MovieRing::freeSpace()
{
    return size() - filled();
}

// FS2 0x001fca80

int MovieRing::contiguous()
{
    int toWrap = size() - m_readPos;
    int n = m_filled;
    if (toWrap < m_filled)
        n = toWrap;
    return n;
}

// FS2 0x001fcac0

int MovieRing::filled()
{
    return m_filled;
}

// FS2 0x001fcad0
// callcheck: ~StreamRing, operator_delete (the base's destructor, the deleting destructor)

MovieRing::~MovieRing()
{
    release();
}
