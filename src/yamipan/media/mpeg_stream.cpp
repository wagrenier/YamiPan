#include "yamipan/media/mpeg_stream.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "sdk/eeregs.h"
#include "yamipan/core/logger.h"

namespace yamipan::media
{

namespace
{

constexpr int kPackHeaderBytes = 14;
constexpr int kPesHeaderBytes = 6;
constexpr int kPesExtendedBytes = 9; // start code, length, two flag bytes and the header length
constexpr int kStreamIdOffset = 3;
constexpr int kPackStuffingMask = 7;

constexpr std::uint8_t kIdProgramEnd = 0xb9;
constexpr std::uint8_t kIdPack = 0xba;
constexpr std::uint8_t kIdPrivateStream1 = 0xbd; // SCE's audio and data: a sub-stream header names kind and channel
constexpr std::uint8_t kIdVideo = 0xe0;

// How libmpeg tells streams apart (FS2: the table at 0x2f1240, read by sceMpegAddStrCallback 0x116590 and
// sceMpegDemuxPssRing 0x116190). A packet is 40 bits: its stream id, then the first four payload bytes, which
// in SCE's private stream 1 are a sub-stream header: 0xff, the kind, 0, the channel. A registration is its
// type's key with the channel in the byte the mask leaves to it; the first one a packet matches takes it.
struct StreamKey
{
    std::uint64_t key;
    std::uint64_t mask;
};

constexpr std::uint64_t kMaskStreamId = 0xff00000000;   // the channel is added to the stream id
constexpr std::uint64_t kMaskSubStream = 0xffff000000;  // the channel is added to the sub-stream byte
constexpr std::uint64_t kMaskSubHeader = 0xffffffffff;  // the channel is the header's last byte
constexpr int kSubHeaderBytes = 4;

constexpr std::array<StreamKey, 10> kStreamKeys = {{
    {0xe000000000, kMaskStreamId},  // sceMpegStrM2V: video, 0xe0 + channel
    {0xbdffc00000, kMaskSubHeader}, // sceMpegStrIPU
    {0xbdffa00000, kMaskSubHeader}, // sceMpegStrPCM
    {0xbdffa10000, kMaskSubHeader}, // sceMpegStrADPCM: FS2's movies (SShd type 0x10)
    {0xbdff900000, kMaskSubHeader}, // sceMpegStrDATA
    {0xc000000000, kMaskStreamId},  // MPEG audio, 0xc0 + channel
    {0xbd80000000, kMaskSubStream}, // the DVD sub-streams: AC-3,
    {0xbda0000000, kMaskSubStream}, // LPCM,
    {0xbd88000000, kMaskSubStream}, // DTS,
    {0xbd90000000, kMaskSubStream}, // SDDS
}};

// The channel's place in a registration's key, by its type's mask.
int channelShift(std::uint64_t mask)
{
    if (mask == kMaskStreamId)
        return 32;
    if (mask == kMaskSubStream)
        return 24;
    return 0;
}

// The packet's 40 bits; a payload shorter than the sub-stream header reads as zeros past its end.
std::uint64_t packetKey(std::uint8_t id, const std::uint8_t *payload, int payloadBytes)
{
    std::uint64_t key = static_cast<std::uint64_t>(id) << 32;
    for (int i = 0; i < kSubHeaderBytes && i < payloadBytes; i++)
        key |= static_cast<std::uint64_t>(payload[i]) << (8 * (kSubHeaderBytes - 1 - i));
    return key;
}

constexpr std::uint8_t kSequenceHeaderCode = 0xb3;

constexpr std::uint8_t kPtsPresent = 0x80;
constexpr std::uint8_t kPtsDtsMask = 0xc0;
constexpr int kPtsOffset = 9;
constexpr int kDtsOffset = 14;

constexpr std::size_t kSequenceHeaderBytes = 8;

// sceMpegGetPicture retires this many slices of the video ring a call: a picture is several of them, and
// fewer leave the ring full, which stops the demultiplexer.
constexpr int kDataRequestsPerPicture = 8;

// Calls the parser gets to give up what it holds, once starved at the end of the program.
constexpr int kEndStallCalls = 8;

// A 33-bit timestamp over five bytes with marker bits between.
std::int64_t readTimeStamp(const std::uint8_t *p)
{
    return (static_cast<std::int64_t>(p[0] & 0x0e) << 29) | (static_cast<std::int64_t>(p[1]) << 22) |
           (static_cast<std::int64_t>(p[2] & 0xfe) << 14) | (static_cast<std::int64_t>(p[3]) << 7) |
           (static_cast<std::int64_t>(p[4] & 0xfe) >> 1);
}

} // namespace

MpegStream &MpegStream::instance()
{
    static MpegStream stream;
    return stream;
}

void MpegStream::clearPlayback()
{
    m_programEnded = false;
    m_pictureCleared = false;
    m_endStall = 0;
    m_decoder.reset();
}

void MpegStream::create(sceMpeg &mpeg)
{
    std::memset(&mpeg, 0, sizeof(mpeg));
    mpeg.sys = this;
    m_events.clear();
    m_streams.clear();
    clearPlayback();
}

void MpegStream::reset(sceMpeg &mpeg)
{
    mpeg.width = 0;
    mpeg.height = 0;
    mpeg.frameCount = 0;
    mpeg.pts = 0;
    mpeg.dts = 0;
    mpeg.flags = 0;
    clearPlayback();
}

void MpegStream::addEventCallback(sceMpegCbType type, sceMpegCallback function, void *argument)
{
    m_events.push_back({type, function, argument});
}

// A second registration of the same stream replaces the first, as in the real library.
void MpegStream::addStreamCallback(sceMpegStrType type, int channel, sceMpegCallback function, void *argument)
{
    const auto index = static_cast<std::size_t>(type);
    if (index >= kStreamKeys.size())
        return;
    const StreamKey &base = kStreamKeys[index];
    const StreamCallback stream{base.key | (static_cast<std::uint64_t>(channel) << channelShift(base.mask)), base.mask,
                                function, argument};
    const auto same = std::ranges::find_if(m_streams, [&](const StreamCallback &s) { return s.key == stream.key; });
    if (same != m_streams.end())
        *same = stream;
    else
        m_streams.push_back(stream);
}

// The game registers PCM and ADPCM for each of its channels with the same ring; the sub-stream header picks.
const MpegStream::StreamCallback *MpegStream::findStream(std::uint64_t key) const
{
    const auto found = std::ranges::find_if(m_streams, [key](const StreamCallback &s) { return (key & s.mask) == s.key; });
    return found != m_streams.end() ? &*found : nullptr;
}

// The picture size and rate, read here so they are known before the first decoded picture.
void MpegStream::readSequenceHeader(sceMpeg &mpeg, std::span<const std::uint8_t> data)
{
    if (mpeg.width != 0 || data.size() < kSequenceHeaderBytes)
        return;
    for (std::size_t i = 0; i + kSequenceHeaderBytes <= data.size(); i++)
    {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1 && data[i + 3] == kSequenceHeaderCode)
        {
            mpeg.width = (data[i + 4] << 4) | (data[i + 5] >> 4);
            mpeg.height = ((data[i + 5] & 0x0f) << 8) | data[i + 6];
            m_decoder.setFrameRateCode(data[i + 7] & 0x0f);
            return;
        }
    }
}

// A video or audio packet. Answers its size, or 0 when the callback refused it: that is back-pressure, the
// packet stays where it is and nothing of it may reach the decoder twice.
int MpegStream::deliverPayload(sceMpeg &mpeg, const std::uint8_t *packet, int size)
{
    const std::uint8_t id = packet[kStreamIdOffset];
    const std::uint8_t flags = packet[7];
    const int headerBytes = packet[8];
    const int payloadBytes = size - kPesExtendedBytes - headerBytes;
    if (payloadBytes <= 0)
        return size;

    const std::uint8_t *const payload = packet + kPesExtendedBytes + headerBytes;
    const StreamCallback *const callback = findStream(packetKey(id, payload, payloadBytes));
    if (callback != nullptr)
    {
        sceMpegCbData data{};
        data.str.type = sceMpegCbStr;
        data.str.header = const_cast<u_char *>(packet);
        data.str.data = const_cast<u_char *>(payload);
        data.str.len = static_cast<u_int>(payloadBytes);
        if (flags & kPtsPresent)
            data.str.pts = readTimeStamp(packet + kPtsOffset);
        if ((flags & kPtsDtsMask) == kPtsDtsMask)
            data.str.dts = readTimeStamp(packet + kDtsOffset);
        if (callback->function(&mpeg, &data, callback->argument) == 0)
            return 0;
    }

    if (id == kIdVideo)
    {
        const std::span<const std::uint8_t> bytes(payload, static_cast<std::size_t>(payloadBytes));
        readSequenceHeader(mpeg, bytes);
        m_decoder.feed(std::as_bytes(bytes));
    }
    return size;
}

int MpegStream::demux(sceMpeg &mpeg, const std::uint8_t *source, int available, const std::uint8_t *ring, int ringSize)
{
    // After the terminator only the file's tail is left.
    if (m_programEnded)
        return available;

    int position = static_cast<int>(source - ring);
    const auto byteAt = [&](int offset) { return ring[(position + offset) % ringSize]; };

    int done = 0;
    while (available - done >= kPesHeaderBytes)
    {
        if (byteAt(0) != 0 || byteAt(1) != 0 || byteAt(2) != 1)
        {
            // The real library is always handed a packet boundary; off one, the next start code is the only
            // way on.
            if (!m_warnedResync)
                log::warning("libmpeg: not on a packet at {:#x}, looking for the next one", position);
            m_warnedResync = true;
            position = (position + 1) % ringSize;
            done++;
            continue;
        }

        const std::uint8_t id = byteAt(kStreamIdOffset);
        if (id == kIdProgramEnd)
        {
            m_programEnded = true;
            m_decoder.endOfStream();
            return available;
        }

        int size;
        if (id == kIdPack)
        {
            if (available - done < kPackHeaderBytes)
                break;
            size = kPackHeaderBytes + (byteAt(kPackHeaderBytes - 1) & kPackStuffingMask);
        }
        else
        {
            size = kPesHeaderBytes + ((byteAt(4) << 8) | byteAt(5));
        }
        if (size > available - done)
            break; // the rest of the packet is still being read

        if (id == kIdVideo || id == kIdPrivateStream1)
        {
            const std::uint8_t *packet = ring + position;
            if (position + size > ringSize)
            {
                m_linear.resize(static_cast<std::size_t>(size));
                const int firstPart = ringSize - position;
                std::memcpy(m_linear.data(), ring + position, static_cast<std::size_t>(firstPart));
                std::memcpy(m_linear.data() + firstPart, ring, static_cast<std::size_t>(size - firstPart));
                packet = m_linear.data();
            }
            if (deliverPayload(mpeg, packet, size) == 0)
                break;
        }

        position = (position + size) % ringSize;
        done += size;
    }
    return done;
}

// The terminator says the stream was read, not played: the decoder keeps its own copy and paces itself, so
// the end waits for it to run out of stream, and for the parser's last pictures.
bool MpegStream::isEnd()
{
    if (!m_programEnded)
        return false;
    if (!m_decoder.starved())
    {
        m_endStall = 0;
        return false;
    }
    return ++m_endStall >= kEndStallCalls;
}

// The real library's Nodata callback retires the slices the IPU has finished with. Without it the video
// ring never empties and the demultiplexer cannot step past a refused packet to what is behind it.
void MpegStream::requestData(sceMpeg &mpeg)
{
    const auto found = std::ranges::find_if(m_events, [](const EventCallback &e) { return e.type == sceMpegCbNodata; });
    if (found == m_events.end())
        return;
    for (int i = 0; i < kDataRequestsPerPicture; i++)
    {
        // There is no IPU: the game's last transfer is over the moment it started.
        REG_DMAC_4_IPU_TO_CHCR = 0;
        sceMpegCbData data{};
        data.type = sceMpegCbNodata;
        found->function(&mpeg, &data, found->argument);
    }
}

// No picture this call is not an error: the last one stays in `picture`.
void MpegStream::getPicture(sceMpeg &mpeg, sceIpuRGB32 *picture, int blocks)
{
    const std::span pictureBytes(reinterpret_cast<std::byte *>(picture),
                                 static_cast<std::size_t>(blocks) * sizeof(sceIpuRGB32));
    if (!m_pictureCleared)
    {
        std::ranges::fill(pictureBytes, std::byte{0});
        m_pictureCleared = true;
    }

    int width = 0;
    int height = 0;
    if (m_decoder.decodePicture(pictureBytes, width, height))
    {
        mpeg.width = width;
        mpeg.height = height;
        mpeg.frameCount++;
    }

    requestData(mpeg);
}

} // namespace yamipan::media
