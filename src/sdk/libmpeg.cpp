// sdk/libmpeg.cpp -- no video decoder yet: a movie reads as a stream that has already ended, so the game
// moves past it. The real one (PSS demux + FFmpeg, as in RosePan) follows with the movie path.
#include "libmpeg.h"
#include "libmpeg_video.h"

void sceMpegInit() {}

int sceMpegCreate(sceMpeg *, void *, int)
{
    return 1;
}

int sceMpegIsEnd(sceMpeg *)
{
    return 1;
}

int sceMpegGetPicture(sceMpeg *, sceIpuRGB32 *, int)
{
    return -1;
}

int sceMpegDemuxPssRing(sceMpeg *, void *, int, void *, int)
{
    return 0;
}

int sceMpegAddCallback(sceMpeg *, sceMpegCbType, sceMpegCallback, void *)
{
    return 0;
}

int sceMpegAddStrCallback(sceMpeg *, sceMpegStrType, int, sceMpegCallback, void *)
{
    return 0;
}

int YamiPanMpegVideoPending()
{
    return 0;
}
