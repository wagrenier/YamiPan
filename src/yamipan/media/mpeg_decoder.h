// The MPEG-2 decoder behind sceMpegGetPicture: what the IPU did in hardware, done by FFmpeg's avcodec on the
// elementary stream MpegStream separates out. There is no container left by then, so no avformat.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

struct AVCodecContext;
struct AVCodecParserContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace yamipan::media
{

class MpegDecoder
{
public:
    MpegDecoder();
    ~MpegDecoder();

    MpegDecoder(const MpegDecoder &) = delete;
    MpegDecoder &operator=(const MpegDecoder &) = delete;

    // Drops everything and starts a new movie.
    void reset();

    // Elementary-stream bytes, as the demultiplexer separates them.
    void feed(std::span<const std::byte> data);

    // The stream's rate, from the sequence header's frame_rate_code. Only the first call counts.
    void setFrameRateCode(int code);

    // The demultiplexer saw the program end: nothing more will be fed, so the codec may give up the
    // pictures it still holds back for reordering.
    void endOfStream();

    // Bytes fed but not yet parsed into pictures. The demultiplexer runs ahead of the decoder, and this
    // tells a stream that is finished from one that is merely fully read.
    [[nodiscard]] int pendingBytes() const;

    // The last decode attempt ran out of stream. With the end of the program seen, the movie is over.
    [[nodiscard]] bool starved() const { return m_starved; }

    // The picture that is due, as 16 x 16 RGBA32 macroblocks in column-major order (the IPU's output and
    // what the ROM's image transfer walks), alpha 0x80. False when none is due yet: the caller keeps the
    // last one, which is what a held frame looks like.
    bool decodePicture(std::span<std::byte> macroblocks, int &width, int &height);

private:
    struct CodecDeleter
    {
        void operator()(AVCodecContext *context) const;
        void operator()(AVCodecParserContext *parser) const;
        void operator()(AVPacket *packet) const;
        void operator()(AVFrame *frame) const;
        void operator()(SwsContext *scaler) const;
    };

    bool open();
    bool scaler(int width, int height);
    bool nextPicture(AVFrame &target);
    void flushCodec();
    void writeMacroblocks(int width, int height, std::span<std::byte> macroblocks) const;

    std::unique_ptr<AVCodecContext, CodecDeleter> m_codec;
    std::unique_ptr<AVCodecParserContext, CodecDeleter> m_parser;
    std::unique_ptr<AVPacket, CodecDeleter> m_packet;
    std::unique_ptr<AVFrame, CodecDeleter> m_frame;
    std::unique_ptr<AVFrame, CodecDeleter> m_spare; // the catch-up's decode target
    std::unique_ptr<SwsContext, CodecDeleter> m_scaler;
    int m_scalerWidth = 0;
    int m_scalerHeight = 0;
    std::vector<std::uint8_t> m_rgba;

    std::vector<std::uint8_t> m_stream;
    std::size_t m_parsed = 0;

    double m_framePeriodNs = 0.0;
    double m_nextDueNs = 0.0;
    bool m_hasFirstPicture = false;
    bool m_starved = false;
    bool m_streamEnded = false;
    bool m_flushed = false;
};

} // namespace yamipan::media
