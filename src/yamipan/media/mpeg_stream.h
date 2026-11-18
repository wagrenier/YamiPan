// libmpeg on the host: the one program stream being played. The demultiplexer is byte parsing and is real; the
// video goes to MpegDecoder, the audio to the game's own callbacks. One movie plays at a time, so sceMpegCreate
// allocates nothing and sceMpeg::sys points here, as the real library's private context.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "sdk/libmpeg.h"
#include "yamipan/media/mpeg_decoder.h"

namespace yamipan::media
{

class MpegStream
{
public:
    static MpegStream &instance();

    void create(sceMpeg &mpeg);
    void reset(sceMpeg &mpeg);
    void addEventCallback(sceMpegCbType type, sceMpegCallback function, void *argument);
    void addStreamCallback(sceMpegStrType type, int channel, sceMpegCallback function, void *argument);

    // Whole packets from the ring position `source`, of `available` bytes, until they run out or a callback
    // refuses one (it stays for the next call). Answers the bytes consumed.
    int demux(sceMpeg &mpeg, const std::uint8_t *source, int available, const std::uint8_t *ring, int ringSize);

    [[nodiscard]] bool isEnd();
    void getPicture(sceMpeg &mpeg, sceIpuRGB32 *picture, int blocks);
    [[nodiscard]] int pendingVideoBytes() const { return m_decoder.pendingBytes(); }

private:
    struct EventCallback
    {
        sceMpegCbType type;
        sceMpegCallback function;
        void *argument;
    };

    // A packet whose stream id and sub-stream header, masked, equal key goes to function.
    struct StreamCallback
    {
        std::uint64_t key;
        std::uint64_t mask;
        sceMpegCallback function;
        void *argument;
    };

    void clearPlayback();
    int deliverPayload(sceMpeg &mpeg, const std::uint8_t *packet, int size);
    void readSequenceHeader(sceMpeg &mpeg, std::span<const std::uint8_t> data);
    [[nodiscard]] const StreamCallback *findStream(std::uint64_t key) const;
    void requestData(sceMpeg &mpeg);

    MpegDecoder m_decoder;
    std::vector<EventCallback> m_events;
    std::vector<StreamCallback> m_streams;
    std::vector<std::uint8_t> m_linear; // a packet that wraps the ring
    bool m_programEnded = false;
    bool m_pictureCleared = false;
    bool m_warnedResync = false;
    int m_endStall = 0;
};

} // namespace yamipan::media
