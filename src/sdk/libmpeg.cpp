// sdk/libmpeg.cpp -- SCE's libmpeg on the host: the names, forwarding to yamipan::media::MpegStream.
#include "libmpeg.h"
#include "libmpeg_video.h"

#include <cstdint>

#include "yamipan/media/mpeg_stream.h"

using yamipan::media::MpegStream;

void sceMpegInit() {}

int sceMpegCreate(sceMpeg *mp, void *, int)
{
    if (mp == nullptr)
        return -1;
    MpegStream::instance().create(*mp);
    return 0;
}

void sceMpegDelete(sceMpeg *mp)
{
    if (mp != nullptr)
        mp->sys = nullptr;
}

void sceMpegReset(sceMpeg *mp)
{
    if (mp != nullptr)
        MpegStream::instance().reset(*mp);
}

int sceMpegIsEnd(sceMpeg *)
{
    return MpegStream::instance().isEnd() ? 1 : 0;
}

int sceMpegGetPicture(sceMpeg *mp, sceIpuRGB32 *rgb32, int qwc)
{
    if (mp != nullptr && rgb32 != nullptr && qwc > 0)
        MpegStream::instance().getPicture(*mp, rgb32, qwc);
    return 0;
}

int sceMpegDemuxPss(sceMpeg *mp, void *buff, int size)
{
    return sceMpegDemuxPssRing(mp, buff, size, buff, size);
}

int sceMpegDemuxPssRing(sceMpeg *mp, void *buff, int size, void *ringStart, int ringSize)
{
    if (mp == nullptr || buff == nullptr || size <= 0)
        return 0;
    return MpegStream::instance().demux(*mp, static_cast<const std::uint8_t *>(buff), size,
                                        static_cast<const std::uint8_t *>(ringStart), ringSize);
}

int sceMpegAddCallback(sceMpeg *, sceMpegCbType type, sceMpegCallback func, void *arg)
{
    MpegStream::instance().addEventCallback(type, func, arg);
    return 0;
}

int sceMpegAddStrCallback(sceMpeg *, sceMpegStrType type, int ch, sceMpegCallback func, void *arg)
{
    MpegStream::instance().addStreamCallback(type, ch, func, arg);
    return 0;
}

int YamiPanMpegVideoPending()
{
    return MpegStream::instance().pendingVideoBytes();
}
