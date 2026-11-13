#include "yamipan/media/mpeg_decoder.h"

#include <cstring>

#if YAMIPAN_HAVE_FFMPEG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

namespace yamipan::media
{

namespace
{

constexpr int kMacroblockSize = 16;
constexpr std::size_t kMacroblockBytes = kMacroblockSize * kMacroblockSize * 4;
constexpr std::uint8_t kOpaqueAlpha = 0x80; // the PS2's opaque, which YamiPan_DrawUiMovie's contract keeps
constexpr std::size_t kConsumedTrimBytes = 1u << 20;
constexpr int kMaxCatchUpPictures = 4;
constexpr double kNanoseconds = 1e9;

double framePeriodNs(int frameRateCode)
{
    switch (frameRateCode)
    {
    case 1:
        return kNanoseconds / (24000.0 / 1001.0);
    case 2:
        return kNanoseconds / 24.0;
    case 3:
        return kNanoseconds / 25.0;
    case 5:
        return kNanoseconds / 30.0;
    case 6:
        return kNanoseconds / 50.0;
    case 7:
        return kNanoseconds / (60000.0 / 1001.0);
    case 8:
        return kNanoseconds / 60.0;
    default:
        return kNanoseconds / (30000.0 / 1001.0);
    }
}

double nowNs()
{
    return static_cast<double>(av_gettime_relative()) * 1000.0;
}

} // namespace

void MpegDecoder::CodecDeleter::operator()(AVCodecContext *context) const
{
    avcodec_free_context(&context);
}

void MpegDecoder::CodecDeleter::operator()(AVCodecParserContext *parser) const
{
    av_parser_close(parser);
}

void MpegDecoder::CodecDeleter::operator()(AVPacket *packet) const
{
    av_packet_free(&packet);
}

void MpegDecoder::CodecDeleter::operator()(AVFrame *frame) const
{
    av_frame_free(&frame);
}

void MpegDecoder::CodecDeleter::operator()(SwsContext *scaler) const
{
    sws_freeContext(scaler);
}

MpegDecoder::MpegDecoder() = default;
MpegDecoder::~MpegDecoder() = default;

void MpegDecoder::reset()
{
    m_scaler.reset();
    m_parser.reset();
    m_frame.reset();
    m_spare.reset();
    m_packet.reset();
    m_codec.reset();
    m_scalerWidth = 0;
    m_scalerHeight = 0;
    m_stream.clear();
    m_parsed = 0;
    m_framePeriodNs = 0.0;
    m_nextDueNs = 0.0;
    m_hasFirstPicture = false;
    m_starved = false;
    m_streamEnded = false;
    m_flushed = false;
}

void MpegDecoder::feed(std::span<const std::byte> data)
{
    const auto *const bytes = reinterpret_cast<const std::uint8_t *>(data.data());
    m_stream.insert(m_stream.end(), bytes, bytes + data.size());
}

void MpegDecoder::setFrameRateCode(int code)
{
    if (m_framePeriodNs == 0.0)
        m_framePeriodNs = framePeriodNs(code);
}

void MpegDecoder::endOfStream()
{
    m_streamEnded = true;
}

int MpegDecoder::pendingBytes() const
{
    return m_stream.size() > m_parsed ? static_cast<int>(m_stream.size() - m_parsed) : 0;
}

bool MpegDecoder::open()
{
    if (m_codec != nullptr)
        return true;

    const AVCodec *const codec = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
    if (codec == nullptr)
        return false;
    m_codec.reset(avcodec_alloc_context3(codec));
    if (m_codec == nullptr)
        return false;
    if (avcodec_open2(m_codec.get(), codec, nullptr) < 0)
    {
        m_codec.reset();
        return false;
    }

    // PES payloads are not aligned on pictures: the parser finds their boundaries.
    m_parser.reset(av_parser_init(AV_CODEC_ID_MPEG2VIDEO));
    m_packet.reset(av_packet_alloc());
    m_frame.reset(av_frame_alloc());
    m_spare.reset(av_frame_alloc());
    return m_parser != nullptr && m_packet != nullptr && m_frame != nullptr && m_spare != nullptr;
}

bool MpegDecoder::scaler(int width, int height)
{
    if (m_scaler == nullptr || m_scalerWidth != width || m_scalerHeight != height)
    {
        m_scaler.reset(sws_getContext(width, height, m_codec->pix_fmt, width, height, AV_PIX_FMT_RGBA, SWS_BILINEAR,
                                      nullptr, nullptr, nullptr));
        if (m_scaler == nullptr)
            return false;
        m_scalerWidth = width;
        m_scalerHeight = height;
    }
    m_rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    return true;
}

void MpegDecoder::flushCodec()
{
    m_flushed = true;
    std::uint8_t *out = nullptr;
    int outSize = 0;
    av_parser_parse2(m_parser.get(), m_codec.get(), &out, &outSize, nullptr, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    if (outSize > 0)
    {
        m_packet->data = out;
        m_packet->size = outSize;
        avcodec_send_packet(m_codec.get(), m_packet.get());
    }
    avcodec_send_packet(m_codec.get(), nullptr);
}

// Pushes what the parser can take until a picture comes back or the stream runs dry.
bool MpegDecoder::nextPicture(AVFrame &target)
{
    for (;;)
    {
        if (avcodec_receive_frame(m_codec.get(), &target) == 0)
            return true;

        if (m_parsed >= m_stream.size())
        {
            if (!m_streamEnded || m_flushed)
                return false;
            flushCodec();
            continue;
        }

        std::uint8_t *out = nullptr;
        int outSize = 0;
        const int used =
            av_parser_parse2(m_parser.get(), m_codec.get(), &out, &outSize, m_stream.data() + m_parsed,
                             static_cast<int>(m_stream.size() - m_parsed), AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (used < 0)
            return false;
        m_parsed += static_cast<std::size_t>(used);

        if (outSize > 0)
        {
            m_packet->data = out;
            m_packet->size = outSize;
            avcodec_send_packet(m_codec.get(), m_packet.get());
        }
        else if (used == 0)
        {
            return false; // the parser wants more than there is
        }

        // A movie is megabytes long: what the parser has passed goes.
        if (m_parsed > kConsumedTrimBytes)
        {
            m_stream.erase(m_stream.begin(), m_stream.begin() + static_cast<std::ptrdiff_t>(m_parsed));
            m_parsed = 0;
        }
    }
}

void MpegDecoder::writeMacroblocks(int width, int height, std::span<std::byte> macroblocks) const
{
    const int columns = width / kMacroblockSize;
    const int rows = height / kMacroblockSize;
    const auto stride = static_cast<std::size_t>(width) * 4;
    if (static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows) * kMacroblockBytes > macroblocks.size())
        return;

    for (int column = 0; column < columns; column++)
    {
        for (int row = 0; row < rows; row++)
        {
            auto *dst = reinterpret_cast<std::uint8_t *>(macroblocks.data()) +
                        static_cast<std::size_t>(column * rows + row) * kMacroblockBytes;
            for (int y = 0; y < kMacroblockSize; y++)
            {
                const std::uint8_t *const src = m_rgba.data() +
                                                static_cast<std::size_t>(row * kMacroblockSize + y) * stride +
                                                static_cast<std::size_t>(column * kMacroblockSize) * 4;
                std::memcpy(dst, src, kMacroblockSize * 4);
                for (int x = 0; x < kMacroblockSize; x++)
                    dst[x * 4 + 3] = kOpaqueAlpha;
                dst += kMacroblockSize * 4;
            }
        }
    }
}

bool MpegDecoder::decodePicture(std::span<std::byte> macroblocks, int &width, int &height)
{
    if (macroblocks.empty() || !open())
        return false;

    // The IPU and the PTS machinery held the rate on the console. The game asks on its 25 Hz tick, which
    // keeps real time only on average (33 and 50 ms steps at 60 Hz), so the stream's own rate decides which
    // picture is due: the one whose time is nearest, half a period either way. A one-sided test (none before
    // its time, a skip a whole period late) refused an early tick and skipped on the next, 5 pictures a
    // second at 25 fps.
    const double now = nowNs();
    const double halfPeriod = m_framePeriodNs / 2.0;
    if (!m_hasFirstPicture)
        m_nextDueNs = now;
    else if (now < m_nextDueNs - halfPeriod)
        return false;

    if (!nextPicture(*m_frame))
    {
        m_starved = true;
        return false;
    }
    m_starved = false;

    if (m_framePeriodNs <= 0.0)
        m_framePeriodNs = framePeriodNs(0);

    // After a slow frame, decode through the backlog and keep the newest (a dropped frame), so the film does
    // not run late for the rest of its length: while the next picture's time is nearer than this one's. A skip
    // decodes into the spare frame first: a failed decode must not take the picture in hand with it (RosePan
    // froze for good that way).
    for (int skipped = 0;
         skipped < kMaxCatchUpPictures && m_hasFirstPicture && now >= m_nextDueNs + m_framePeriodNs / 2.0; skipped++)
    {
        if (!nextPicture(*m_spare))
            break;
        av_frame_unref(m_frame.get());
        av_frame_move_ref(m_frame.get(), m_spare.get());
        m_nextDueNs += m_framePeriodNs;
    }

    const int pictureWidth = m_frame->width;
    const int pictureHeight = m_frame->height;
    if (pictureWidth <= 0 || pictureHeight <= 0 || !scaler(pictureWidth, pictureHeight))
    {
        av_frame_unref(m_frame.get());
        return false;
    }

    std::uint8_t *const planes[4] = {m_rgba.data(), nullptr, nullptr, nullptr};
    const int strides[4] = {pictureWidth * 4, 0, 0, 0};
    sws_scale(m_scaler.get(), m_frame->data, m_frame->linesize, 0, pictureHeight, planes, strides);
    writeMacroblocks(pictureWidth, pictureHeight, macroblocks);
    av_frame_unref(m_frame.get());

    width = pictureWidth;
    height = pictureHeight;

    // Whole periods, so the film keeps its timeline; a stall too long to catch up is written off, the picture
    // just shown taken as due now (due at the next call, the next would be skipped at once).
    m_nextDueNs += m_framePeriodNs;
    if (m_nextDueNs < now - m_framePeriodNs * kMaxCatchUpPictures)
        m_nextDueNs = now + m_framePeriodNs;
    m_hasFirstPicture = true;
    return true;
}

} // namespace yamipan::media

#else // !YAMIPAN_HAVE_FFMPEG: a movie's stream ends as soon as it has been read, on a blank picture.

namespace yamipan::media
{

void MpegDecoder::CodecDeleter::operator()(AVCodecContext *) const {}
void MpegDecoder::CodecDeleter::operator()(AVCodecParserContext *) const {}
void MpegDecoder::CodecDeleter::operator()(AVPacket *) const {}
void MpegDecoder::CodecDeleter::operator()(AVFrame *) const {}
void MpegDecoder::CodecDeleter::operator()(SwsContext *) const {}

MpegDecoder::MpegDecoder() = default;
MpegDecoder::~MpegDecoder() = default;

void MpegDecoder::reset() {}
void MpegDecoder::feed(std::span<const std::byte>) {}
void MpegDecoder::setFrameRateCode(int) {}
void MpegDecoder::endOfStream() {}

int MpegDecoder::pendingBytes() const
{
    return 0;
}

bool MpegDecoder::decodePicture(std::span<std::byte>, int &, int &)
{
    m_starved = true;
    return false;
}

} // namespace yamipan::media

#endif
