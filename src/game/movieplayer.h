/* ==========================================================================
 *  game/movieplayer.h
 *
 *  The MPEG movie player (game/movieplayer.cpp, 0x1faa30..0x1fcb40): the
 *  prgMovie widget's (game/uiwidget.cpp) PSS player on SDK 3.0 libmpeg.
 *  One file ring reads the movie, sceMpegDemuxPssRing splits it into the
 *  video ring (fed to the IPU by the Nodata callback), up to four audio
 *  rings (each pumped into an rx PCM / ADPCM stream by a SoundFeeder) and
 *  an optional data ring; each frame decodes one RGB32 picture and uploads
 *  it, one macroblock at a time, into a GS buffer the widget names.
 *
 *  Every buffer is carved from one work area (workSize / setWorkArea).
 *  The state bits (MovieState) walk open -> primed (the file ring past
 *  its threshold) -> ready (the first picture) -> playing (startAudio)
 *  -> file end -> drained -> over (the sound has ended).
 *
 *  PC: the host has no IPU and no GS memory. The host libmpeg (RosePan's,
 *  bootstrap 6) decodes from the demuxer itself, so the video ring only
 *  paces the demux; the picture goes to the host draw module as the
 *  player's own texture (YamiPan_DrawUiMovie) instead of the image
 *  upload. The file name is the port's.
 * ======================================================================== */
#ifndef _GAME_MOVIEPLAYER_H
#define _GAME_MOVIEPLAYER_H

#include "game/gamehooks.h"
#include "sdk/libmpeg.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

// The audio streams a movie can carry (one feeder, ring and volume each).
constexpr int kMovieMaxChannels = 4;

// MoviePlayer::m_state.
enum MovieState
{
    MovieState_Open     = 0x1,      // open succeeded
    MovieState_Primed   = 0x2,      // the file ring reached its threshold
    MovieState_Ready    = 0x4,      // a picture is decoded
    MovieState_Playing  = 0x8,      // startAudio ran
    MovieState_Closing  = 0x10,     // close / reopen: decodeFrame stops
    MovieState_FileEnd  = 0x20,     // the file ring has delivered the file
    MovieState_Drained  = 0x40,     // every ring is consumed
    MovieState_Over     = 0x80,     // the sound has ended too
    MovieState_Abort    = 0x8000,   // demux / feedIpu give up (no writer in this file)
};

class MoviePlayer;

// A ring in memory (one of the work area's blocks): the video, audio and
// data streams the demuxer delivers.
// layout: host -- EE 0x34, vtable 0x315f50 (StreamRing's 10 slots).
class MovieRing : public StreamRing
{
public:
    MovieRing(int size, void *mem, int threshold);   // inline in MoviePlayer::open
    virtual ~MovieRing();                                                // FS2 0x001fcad0: deleting (obj, flag)
    virtual int     filled();                                            // FS2 0x001fcac0
    virtual int     contiguous();                                        // FS2 0x001fca80
    virtual int     freeSpace();                                         // FS2 0x001fca30
    virtual void    write(const void *src, int size);                    // FS2 0x001fc950
    virtual u_char *readPtr();                                           // FS2 0x001fc940
    virtual int     readPos();                                           // FS2 0x001fc930
    virtual int     consume(int size);                                   // FS2 0x001fc8d0
    virtual int     poll();                                              // FS2 0x001fc8c0
    virtual int     endPos();                                            // FS2 0x001fc8b0

    int m_writePos;             // FS2 +0x14
    int m_readPos;              // FS2 +0x18
    int m_filled;               // FS2 +0x1c
    int _20;                    // FS2 +0x20: zeroed only
    int m_written;              // FS2 +0x24: total
    int m_consumed;             // FS2 +0x28: total
    int m_pending;              // FS2 +0x2c: the video ring's bytes on their way to the IPU
    int m_endPos;               // FS2 +0x30: the consumed total at the stream's end (audio rings)
};

// The SShd / SSbd header in front of each audio stream (SDK ezmpegstr).
struct MovieAudioHeader         // 0x28
{
    char id[4];                 // FS2 +0x00: "SShd"
    int  size;                  // FS2 +0x04
    int  type;                  // FS2 +0x08: < 0x10 PCM, else SPU2 ADPCM
    int  rate;                  // FS2 +0x0c
    int  channels;              // FS2 +0x10
    int  interleave;            // FS2 +0x14
    int  loopStart;             // FS2 +0x18
    int  loopEnd;               // FS2 +0x1c
    char bodyId[4];             // FS2 +0x20: "SSbd"
    int  bodySize;              // FS2 +0x24
};
static_assert(sizeof(MovieAudioHeader) == 0x28, "MovieAudioHeader is 0x28 bytes on the EE");

// The audio type at which the SoundAdpcmSink takes over.
constexpr int kMovieAudioTypeAdpcm = 0x10;

// One audio channel's ring: it keeps the header that opens the channel's
// sink.
// layout: host -- EE 0x68, vtable 0x315f80.
class MovieAudioRing : public MovieRing
{
public:
    MovieAudioRing(int size, void *mem, int threshold, int channel, MoviePlayer *player); // inline in MoviePlayer::open
    virtual ~MovieAudioRing();                                           // FS2 0x001fc4e0: deleting (obj, flag)

    int              m_channel;     // FS2 +0x34
    MoviePlayer     *m_player;      // FS2 +0x38
    u_int            m_headerSize;  // FS2 +0x3c: header bytes collected
    MovieAudioHeader m_header;      // FS2 +0x40
};

// The libipu DMA state sceIpuStopDMA saves and sceIpuRestartDMA restores
// (SDK sceIpuDmaEnv).
struct MovieIpuDmaEnv           // 0x24
{
    u_int d4madr, d4tadr, d4qwc, d4chcr;
    u_int d3madr, d3qwc, d3chcr;
    u_int ipubp, ipuctrl;
};
static_assert(sizeof(MovieIpuDmaEnv) == 0x24, "MovieIpuDmaEnv is 0x24 bytes on the EE");

// layout: host -- EE 0x110 (the sceMpeg first: libmpeg's callbacks get
// it, and getFrame reads its frameCount).
class MoviePlayer
{
public:
    // audioChannels: the streams the movie carries (prgMovie: 2).
    MoviePlayer(int width, int height, int audioChannels);               // FS2 0x001fc7b0: CreateSema, sceMpegInit once
    ~MoviePlayer();                                                      // FS2 0x001fc720: deleting (obj, 1)
    // Mutes / sets one of the audio channels.
    void setVolume(int channel, int left, int right);                    // FS2 0x001faa30
    int  workSize();                                                     // FS2 0x001fbfd0
    // null allocates workSize itself.
    void setWorkArea(void *mem, int size);                               // FS2 0x001fc560
    int  open(const char *name);                                         // FS2 0x001fb9d0: < 0 failed
    void close();                                                        // FS2 0x001fb950
    // state & mask (mask 0: state == 0): MovieState.
    bool testState(u_int mask);                                          // FS2 0x001fb920
    bool isReady();                                                      // FS2 0x001fab40: MovieState_Ready
    bool isPlaying();                                                    // FS2 0x001fab30: MovieState_Playing
    int  startAudio();                                                   // FS2 0x001faa90: 1 started, 0 already, -1 not open, -2 no picture yet
    // Until the first picture: decodes and draws it into the movie's own
    // buffer (tbp 0x700, packet list 13).
    bool prime();                                                        // FS2 0x001fb380
    // The next picture into packet: uploaded at tbp (width / 64 wide),
    // width x height at most, at (x, y).
    bool drawFrame(sceVif1Packet *packet, int tbp, int width, int height, int x, int y); // FS2 0x001fb2d0
    int  getFrame();                                                     // FS2 0x001faa70: -1 not playing

    // libmpeg's callbacks (game/movieplayer.cpp) call these.
    bool feedIpu(sceMpeg *mp, sceMpegCbData *data);                      // FS2 0x001fab50
    bool receiveAudio(sceMpeg *mp, sceMpegCbDataStr *data, MovieAudioRing *ring); // FS2 0x001fb0b0

    sceMpeg         m_mpeg;         // FS2 +0x00
    u_int           m_state;        // FS2 +0x48: MovieState
    int             m_width;        // FS2 +0x4c
    int             m_height;       // FS2 +0x50
    int             m_channels;     // FS2 +0x54: audio streams
    int             m_workSize;     // FS2 +0x58
    u_char         *m_work;         // FS2 +0x5c
    u_char          m_workOwned;    // FS2 +0x60: m_work from Heap::alloc
    int             m_dataSize;     // FS2 +0x64: the data ring (no writer in this file)
    u_char         *m_dataMem;      // FS2 +0x68
    int             _6c;            // FS2 +0x6c: zeroed only
    int             m_sema;         // FS2 +0x70: guards the video ring against the IPU feed
    MovieIpuDmaEnv  m_ipuDmaEnv;    // FS2 +0x74
    FileStreamRing *m_reader;       // FS2 +0x98: the movie file
    int             m_volume[kMovieMaxChannels][2]; // FS2 +0x9c: left, right (0x80)
    u_char         *m_carve;        // FS2 +0xbc: carveWorkArea's cursor
    int             m_readerSize;   // FS2 +0xc0
    u_char         *m_readerMem;    // FS2 +0xc4
    int             m_videoSize;    // FS2 +0xc8
    u_char         *m_videoMem;     // FS2 +0xcc
    int             m_pictureSize;  // FS2 +0xd0
    sceIpuRGB32    *m_picture;      // FS2 +0xd4: the decoded macroblocks
    int             m_mpegWorkSize; // FS2 +0xd8
    u_char         *m_mpegWork;     // FS2 +0xdc
    int             m_audioSize;    // FS2 +0xe0
    u_char         *m_audioMem;     // FS2 +0xe4
    MovieRing      *m_videoRing;    // FS2 +0xe8
    MovieAudioRing *m_audioRings[kMovieMaxChannels]; // FS2 +0xec
    MovieRing      *m_dataRing;     // FS2 +0xfc
    SoundFeeder    *m_feeders[kMovieMaxChannels];    // FS2 +0x100

private:
    bool isDrained();                                                    // FS2 0x001fadc0
    int  demux(int want);                                                // FS2 0x001faec0
    void uploadFrame(sceVif1Packet *packet, int tbp, int width, int height, int x, int y); // FS2 0x001fb450
    bool decodeFrame();                                                  // FS2 0x001fb630
    int  carveWorkArea();                                                // FS2 0x001fc0e0
    void freeStreams();                                                  // FS2 0x001fc3a0
    void *carve(int size);      // inline: a 128-byte aligned block, null when it does not fit
    void  updateFeeders();      // inline: SoundFeeder::update on each
};

#endif /* _GAME_MOVIEPLAYER_H */
