/* ==========================================================================
 *  adpcm.cpp  (RXSNDDRV.IRX 0x9eb0..0xb4a0: the ADPCM stream system)
 *
 *  Streams of SPU2 ADPCM the EE feeds a little at a time: the title music,
 *  voices and movie audio (rx/rxsnd.c rxSndAdpcmOpen / Write / End).  A
 *  stream has 1..n channels; each channel owns a ring in IOP memory and a
 *  looping buffer in SPU RAM (the EE allocates both and marks the last block
 *  before every wrap of the SPU buffer as loop end + repeat).
 *
 *    0xa0 {id, -, channels, -, priority, pitch, iopSize, spuSize} then
 *         {iop, spu} per channel: open (RxSndDrvAdpcmOpen)
 *    0xa1 {id, end, -, -, size}: `size` more bytes per channel are in the
 *         IOP rings; end 1 = the last of them (RxSndDrvAdpcmData)
 *    0xa2 {id, param, value}: 0 close, 1 pause, 2 reverb, 3 volume L | R << 16,
 *         4 pitch scale, 6 voice group, 7 voice owner (RxSndDrvAdpcmSet)
 *    0xa3 {id, status, free, posHi, posLo} per open stream (RxSndDrvAdpcmStatus)
 *    0xa4 {id, channel, param, -, value}: 3 a channel's volume, 5 the voice
 *         it asks for (RxSndDrvAdpcmChannelSet)
 *
 *  Every V-blank (RxSndDrvAdpcmTick): apply the pending set records, drop
 *  closed streams, then per stream start the voices once the SPU buffer
 *  holds 1.5 frames of data (or the end has come), follow the first
 *  channel's NAX to count what was played, and move what the IOP rings hold
 *  into the SPU buffers on DMA queue 0, at most 0x400 bytes a stream a pass.
 *  The voices play the SPU buffer as a loop: SSA at the play position, LSAX
 *  at its start; when all the data left fits before the wrap, no loop is set
 *  and the voice stops at the EE's end block, which ends the stream.
 *
 *  Sizes are bytes per channel; every channel moves in step with the first.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "intrman.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RXSNDDRV_ADPCM_STREAMS  0x30
#define RXSNDDRV_ADPCM_CHANNELS 0x100

/* Bytes of ADPCM one second at 48 kHz (28 samples in 16 bytes). */
#define RXSNDDRV_ADPCM_BYTES_PER_SEC 0x6b24

/* RxSndDrvAdpcm::m_flags (the low byte is the status the EE reads back). */
enum
{
    RXSNDDRV_ADPCM_USED    = 0x00001,   /* opened by command 0xa0 */
    RXSNDDRV_ADPCM_PLAYING = 0x00002,   /* the voices are keyed */
    RXSNDDRV_ADPCM_ENDED   = 0x00004,   /* played to its end (or its voice was taken) */
    RXSNDDRV_ADPCM_PAUSED  = 0x00008,
    RXSNDDRV_ADPCM_ACTIVE  = 0x10000,   /* seen by a tick */
    RXSNDDRV_ADPCM_CLOSED  = 0x20000,
};

/* RxSndDrvAdpcmPending::m_mask */
enum
{
    RXSNDDRV_ADPCM_SET_OPEN   = 0x01,   /* opened: the tick makes it active */
    RXSNDDRV_ADPCM_SET_CLOSE  = 0x02,
    RXSNDDRV_ADPCM_SET_PAUSE  = 0x04,
    RXSNDDRV_ADPCM_SET_REVERB = 0x08,
    RXSNDDRV_ADPCM_SET_VOLUME = 0x10,
    RXSNDDRV_ADPCM_SET_PITCH  = 0x20,
    RXSNDDRV_ADPCM_SET_GROUP  = 0x40,
    RXSNDDRV_ADPCM_SET_OWNER  = 0x80,
};

/* RxSndDrvAdpcmChannelPending::m_mask */
enum
{
    RXSNDDRV_ADPCM_CH_SET_VOLUME = 0x2,
    RXSNDDRV_ADPCM_CH_SET_VOICE  = 0x8,
};

/* RxSndDrvAdpcm::m_dirty: what the update pushes to the voices. */
enum
{
    RXSNDDRV_ADPCM_DIRTY_SWITCH = 0x1,
    RXSNDDRV_ADPCM_DIRTY_VOLUME = 0x2,
    RXSNDDRV_ADPCM_DIRTY_PITCH  = 0x4,
};

/* A channel's set record, per bank (0xc). */
struct RxSndDrvAdpcmChannelPending
{
    u_int m_mask;                       // IOP +0x0: RXSNDDRV_ADPCM_CH_SET_*
    u_int m_vol;                        // IOP +0x4: L | R << 16
    u_int m_voice;                      // IOP +0x8: its low byte
};
static_assert(sizeof(RxSndDrvAdpcmChannelPending) == 0xc, "RxSndDrvAdpcmChannelPending");

struct RxSndDrvAdpcm;

/* A channel (0x30 on the IOP, 0x100 at RoR IOP 0x31ec8).  layout: host. */
struct RxSndDrvAdpcmChannel
{
    RxSndDrvAdpcmChannel *m_next;       // IOP +0x00: the stream's list
    RxSndDrvAdpcm        *m_stream;     // IOP +0x04
    u_char  m_no;                       // IOP +0x08: index in the stream (command 0xa4)
    signed char m_voice;                // IOP +0x09: -1 none
    signed char m_voiceWant;            // IOP +0x0a: the voice to allocate, -1 any
    u_char  m_keyed;                    // IOP +0x0b: cleared at key on, read nowhere
    short   m_vol[2];                   // IOP +0x0c: L, R, 0x80 unity
    u_int   m_iop;                      // IOP +0x10: the IOP ring
    u_int   m_spu;                      // IOP +0x14: the SPU buffer
    RxSndDrvAdpcmChannelPending m_pending[2]; // IOP +0x18: per bank
};

/* The set commands' record, per bank (0x10). */
struct RxSndDrvAdpcmPending
{
    u_int  m_mask;                      // IOP +0x0: RXSNDDRV_ADPCM_SET_*
    u_int  m_vol;                       // IOP +0x4: L | R << 16
    u_char m_pause;                     // IOP +0x8
    u_char m_reverb;                    // IOP +0x9
    short  m_pitch;                     // IOP +0xa
    short  m_group;                     // IOP +0xc
    short  m_owner;                     // IOP +0xe
};
static_assert(sizeof(RxSndDrvAdpcmPending) == 0x10, "RxSndDrvAdpcmPending");
static_assert(offsetof(RxSndDrvAdpcmPending, m_vol) == 0x4, "RxSndDrvAdpcmPending::m_vol");
static_assert(offsetof(RxSndDrvAdpcmPending, m_pause) == 0x8, "RxSndDrvAdpcmPending::m_pause");
static_assert(offsetof(RxSndDrvAdpcmPending, m_pitch) == 0xa, "RxSndDrvAdpcmPending::m_pitch");
static_assert(offsetof(RxSndDrvAdpcmPending, m_group) == 0xc, "RxSndDrvAdpcmPending::m_group");
static_assert(offsetof(RxSndDrvAdpcmPending, m_owner) == 0xe, "RxSndDrvAdpcmPending::m_owner");

/* A stream (0x74 on the IOP, 0x30 at RoR IOP 0x30904).  layout: host. */
struct RxSndDrvAdpcm
{
    RxSndDrvAdpcm *m_next;              // IOP +0x00: the active list
    u_int   m_flags;                    // IOP +0x04: RXSNDDRV_ADPCM_*
    int     m_priority;                 // IOP +0x08: the voices'; >= 0: a stolen voice ends it
    int     m_group;                    // IOP +0x0c: the first channel's voice group, 0 any
    int     m_owner;                    // IOP +0x10: voice owner, 0 none
    u_char  m_id;                       // IOP +0x14
    u_char  m_reverb;                   // IOP +0x15
    u_char  m_dirty;                    // IOP +0x16: RXSNDDRV_ADPCM_DIRTY_*
    u_char  m_end;                      // IOP +0x17: 1 no more data comes
    short   m_vol[2];                   // IOP +0x18: L, R, 0x80 unity
    short   m_pitch;                    // IOP +0x1c: rate * 0x1000 / 48000
    short   m_scale;                    // IOP +0x1e: 0x1000 unity
    int     m_need;                     // IOP +0x20: 1.5 frames of data, the start threshold
    int     m_iopSize;                  // IOP +0x24
    int     m_iopWrite;                 // IOP +0x28
    int     m_iopRead;                  // IOP +0x2c
    int     m_iopFill;                  // IOP +0x30: written, not yet sent
    int     m_dmaPending;               // IOP +0x34: sent, not yet transferred
    int     m_spuSize;                  // IOP +0x38
    int     m_spuWrite;                 // IOP +0x3c
    int     m_spuPlay;                  // IOP +0x40: the voices' position
    int     m_spuFill;                  // IOP +0x44: sent, not yet played
    u_int   m_posHi;                    // IOP +0x48: bytes played, -1 at the end
    u_int   m_posLo;                    // IOP +0x4c
    RxSndDrvAdpcmChannel *m_channels;   // IOP +0x50
    RxSndDrvAdpcmPending  m_pending[2]; // IOP +0x54: per bank
};

/* The stream system's block (0x45cc at RoR IOP 0x30900).  layout: host. */
struct RxSndDrvAdpcmState
{
    int                   m_frameBytes;   // IOP +0x0000: bytes a V-blank at pitch 0x1000
    RxSndDrvAdpcm         m_stream[RXSNDDRV_ADPCM_STREAMS];   // IOP +0x0004
    RxSndDrvAdpcm        *m_active;       // IOP +0x15c4
    RxSndDrvAdpcmChannel  m_channelPool[RXSNDDRV_ADPCM_CHANNELS]; // IOP +0x15c8
    RxSndDrvAdpcmChannel *m_channelFree;  // IOP +0x45c8
};

static RxSndDrvAdpcmState s_adpcm;      // RoR IOP 0x00030900

static void RxSndDrvAdpcmApply(void);
static void RxSndDrvAdpcmUpdate(void);
static void RxSndDrvAdpcmFollow(RxSndDrvAdpcm *s);
static void RxSndDrvAdpcmStart(RxSndDrvAdpcm *s);
static void RxSndDrvAdpcmFeed(void);
static void RxSndDrvAdpcmReap(void);
static void RxSndDrvAdpcmStop(RxSndDrvAdpcm *s);
static void RxSndDrvAdpcmVoiceDone(int voice, int reason, void *arg);
static void RxSndDrvAdpcmDmaDone(void *arg, int size);

/* PC: the first few starts, underruns and ends, for the smoke-run log (the
 * driver itself prints none of them). */
static void LogStream(const char *what, const RxSndDrvAdpcm *s)
{
    static int count;

    if (count >= 16)
        return;
    count++;
    printf("rxsnddrv: ADPCM %d %s (voice %d, spu fill %#x, play %#x, pos %#x)\n", s->m_id, what,
           s->m_channels != NULL ? s->m_channels->m_voice : -1, s->m_spuFill, s->m_spuPlay, s->m_posLo);
}

/* 1.5 frames of data at the stream's rate, in 0x40 units. */
static int StartThreshold(const RxSndDrvAdpcm *s)
{
    int bytes = s_adpcm.m_frameBytes * (s->m_scale * s->m_pitch >> 12) * 3;
    return ((bytes / 2 >> 12) + 0x3f) & ~0x3f;
}

/* Command 0xa0: stream `id` with `count` channels {iop, spu} (`chans`); 1,
 * or 0 when the stream is open already.  A short channel pool opens it with
 * the channels it got. */
// IOP 0x0000a1a0 (RoR 0x00009eb0, word for word)
int RxSndDrvAdpcmOpen(int id, int pitch, const u_int *chans, int count, u_int iopSize,
                      int spuSize, int priority)
{
    RxSndDrvAdpcm *s  = &s_adpcm.m_stream[id];
    int            ok = 0;
    int            intr;

    CpuSuspendIntr(&intr);
    if (!(s->m_flags & RXSNDDRV_ADPCM_USED))
    {
        s->m_flags      = RXSNDDRV_ADPCM_USED;
        s->m_reverb     = 0;
        s->m_group      = 0;
        s->m_owner      = 0;
        s->m_end        = 0;
        s->m_pitch      = (short)pitch;
        s->m_need       = 0;
        s->m_posHi      = 0;
        s->m_posLo      = 0;
        s->m_channels   = NULL;
        s->m_priority   = priority;
        s->m_vol[0]     = 0x80;
        s->m_vol[1]     = 0x80;
        s->m_scale      = 0x1000;
        s->m_iopWrite   = 0;
        s->m_iopRead    = 0;
        s->m_iopFill    = 0;
        s->m_dmaPending = 0;
        s->m_iopSize    = (int)iopSize;
        s->m_spuWrite   = 0;
        s->m_spuPlay    = 0;
        s->m_spuFill    = 0;
        s->m_spuSize    = spuSize;
        s->m_pending[RxSndDrvCmdBank()].m_mask     = RXSNDDRV_ADPCM_SET_OPEN;
        s->m_pending[RxSndDrvCmdBank() == 0].m_mask = 0;
        s->m_need       = StartThreshold(s);

        for (int i = 0; i < count; i++)
        {
            RxSndDrvAdpcmChannel *ch = s_adpcm.m_channelFree;
            if (ch == NULL)
            {
                if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_WARN)
                    printf("rxsnddrv : can't allocate ADPCM channel\n");
                break;
            }
            s_adpcm.m_channelFree = ch->m_next;
            ch->m_next            = s->m_channels;
            s->m_channels         = ch;
            ch->m_stream          = s;
            ch->m_no              = (u_char)i;
            ch->m_voice           = -1;
            ch->m_voiceWant       = -1;
            ch->m_keyed           = 0;
            ch->m_vol[0]          = 0x80;
            ch->m_vol[1]          = 0x80;
            ch->m_iop             = chans[0];
            ch->m_pending[0].m_mask = 0;
            ch->m_pending[1].m_mask = 0;
            ch->m_spu             = chans[1];
            chans += 2;
        }

        ok           = 1;
        s->m_next    = s_adpcm.m_active;
        s_adpcm.m_active = s;
    }
    CpuResumeIntr(intr);

    if (!ok && (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR))
        printf("snddrv : failed to open ADPCM slot '%d'\n", id);
    return ok;
}

/* The tick's first step: the set records of the last frame's commands. */
// IOP 0x0000a430 (RoR 0x0000a140, word for word)
static void RxSndDrvAdpcmApply(void)
{
    for (RxSndDrvAdpcm *s = s_adpcm.m_active; s != NULL; s = s->m_next)
    {
        if (s->m_pending[RxSndDrvTickBank()].m_mask & RXSNDDRV_ADPCM_SET_OPEN)
            s->m_flags |= RXSNDDRV_ADPCM_ACTIVE;
    }

    for (RxSndDrvAdpcm *s = s_adpcm.m_active; s != NULL; s = s->m_next)
    {
        if (!(s->m_flags & RXSNDDRV_ADPCM_ACTIVE))
            continue;

        RxSndDrvAdpcmPending *p = &s->m_pending[RxSndDrvTickBank()];
        if (p->m_mask & RXSNDDRV_ADPCM_SET_CLOSE)
            s->m_flags |= RXSNDDRV_ADPCM_CLOSED;
        if (p->m_mask & RXSNDDRV_ADPCM_SET_PAUSE)
        {
            if (p->m_pause == 0)
                s->m_flags &= ~RXSNDDRV_ADPCM_PAUSED;
            else
                s->m_flags |= RXSNDDRV_ADPCM_PAUSED;
        }
        if (p->m_mask & RXSNDDRV_ADPCM_SET_REVERB)
        {
            s->m_reverb  = p->m_reverb;
            s->m_dirty  |= RXSNDDRV_ADPCM_DIRTY_SWITCH;
        }
        if (p->m_mask & RXSNDDRV_ADPCM_SET_VOLUME)
        {
            s->m_vol[0]  = (short)p->m_vol;
            s->m_vol[1]  = (short)(p->m_vol >> 16);
            s->m_dirty  |= RXSNDDRV_ADPCM_DIRTY_VOLUME;
        }
        if (p->m_mask & RXSNDDRV_ADPCM_SET_PITCH)
        {
            s->m_scale   = p->m_pitch;
            s->m_dirty  |= RXSNDDRV_ADPCM_DIRTY_PITCH;
            s->m_need    = StartThreshold(s);
        }
        if (p->m_mask & RXSNDDRV_ADPCM_SET_GROUP)
            s->m_group = p->m_group;
        if (p->m_mask & RXSNDDRV_ADPCM_SET_OWNER)
            s->m_owner = p->m_owner;

        for (RxSndDrvAdpcmChannel *ch = s->m_channels; ch != NULL; ch = ch->m_next)
        {
            RxSndDrvAdpcmChannelPending *cp = &ch->m_pending[RxSndDrvTickBank()];
            if (cp->m_mask & RXSNDDRV_ADPCM_CH_SET_VOICE)
                ch->m_voiceWant = (signed char)cp->m_voice;
        }
        p->m_mask = 0;
    }
}

/* The tick's third step: start, follow or stop each stream's voices and
 * push the changed voice parameters. */
// IOP 0x0000a6dc (RoR 0x0000a3ec, word for word)
static void RxSndDrvAdpcmUpdate(void)
{
    for (RxSndDrvAdpcm *s = s_adpcm.m_active; s != NULL; s = s->m_next)
    {
        if (!(s->m_flags & RXSNDDRV_ADPCM_ACTIVE))
            continue;

        /* The last data played out: a voice went silent after the end. */
        for (RxSndDrvAdpcmChannel *ch = s->m_channels; ch != NULL; ch = ch->m_next)
        {
            if (s->m_end == 1 && ch->m_voice >= 0 && RxSndDrvVoiceGetEnvx(ch->m_voice) == 0)
            {
                RxSndDrvAdpcmStop(s);
                s->m_posHi  = 0xffffffff;
                s->m_posLo  = 0xffffffff;
                s->m_flags |= RXSNDDRV_ADPCM_ENDED;
                LogStream("ended", s);
                break;
            }
        }

        if (s->m_flags & RXSNDDRV_ADPCM_ENDED)
            continue;

        if (s->m_flags & RXSNDDRV_ADPCM_PAUSED)
            RxSndDrvAdpcmStop(s);
        else if (s->m_flags & RXSNDDRV_ADPCM_PLAYING)
            RxSndDrvAdpcmFollow(s);
        else
            RxSndDrvAdpcmStart(s);

        for (RxSndDrvAdpcmChannel *ch = s->m_channels; ch != NULL; ch = ch->m_next)
        {
            u_char                       dirty = s->m_dirty;
            RxSndDrvAdpcmChannelPending *cp    = &ch->m_pending[RxSndDrvTickBank()];
            u_int                        bits  = dirty | cp->m_mask;

            if (cp->m_mask & RXSNDDRV_ADPCM_CH_SET_VOLUME)
            {
                ch->m_vol[0] = (short)cp->m_vol;
                ch->m_vol[1] = (short)(cp->m_vol >> 16);
            }
            cp->m_mask = 0;

            if (ch->m_voice < 0)
                continue;
            if (bits & RXSNDDRV_ADPCM_DIRTY_SWITCH)
                RxSndDrvVoiceSetSwitch(ch->m_voice, s->m_reverb != 0 ? 3 : 1);
            if (bits & RXSNDDRV_ADPCM_DIRTY_VOLUME)
                RxSndDrvVoiceSetVolume(ch->m_voice, s->m_vol[0] * ch->m_vol[0] >> 7,
                                       s->m_vol[1] * ch->m_vol[1] >> 7);
            if (bits & RXSNDDRV_ADPCM_DIRTY_PITCH)
                RxSndDrvVoiceSetPitch(ch->m_voice, 0x40, 0, 0x40, 0, s->m_pitch * s->m_scale >> 12);
        }
        s->m_dirty = 0;
    }
}

/* Playing: count what the voices played since the last tick (the first
 * channel's NAX); an underrun (the voices reached data not yet there)
 * stops them until there is enough again. */
// IOP 0x0000a92c (RoR 0x0000a63c, word for word)
static void RxSndDrvAdpcmFollow(RxSndDrvAdpcm *s)
{
    int intr;

    if (s->m_channels != NULL)
    {
        RxSndDrvAdpcmChannel *ch  = s->m_channels;
        int                   pos = (int)((RxSndDrvVoiceGetNax(ch->m_voice) - ch->m_spu) & ~0x3fu);

        if (pos >= 0 && pos <= s->m_spuSize
            && (s->m_end != 0 || s->m_need <= s->m_spuFill - s->m_dmaPending))
        {
            u_int played = (u_int)(pos - s->m_spuPlay);
            if ((int)played < 0)
                played += (u_int)s->m_spuSize;

            CpuSuspendIntr(&intr);
            s->m_spuFill -= (int)played;
            s->m_spuPlay += (int)played;
            if (s->m_spuPlay >= s->m_spuSize)
                s->m_spuPlay -= s->m_spuSize;
            CpuResumeIntr(intr);

            u_int lo  = s->m_posLo;
            s->m_posLo = lo + played;
            if (s->m_posLo < lo)
                s->m_posHi++;
            return;
        }
    }

    LogStream("underrun", s);
    RxSndDrvAdpcmStop(s);
}

/* Not playing: key the voices on once the SPU buffer holds m_need bytes
 * (or no more data comes). */
// IOP 0x0000aabc (RoR 0x0000a7cc, 93% of it aligned: FS2 differs, unread)
static void RxSndDrvAdpcmStart(RxSndDrvAdpcm *s)
{
    int intr;

    if (s->m_end == 0 && s->m_channels != NULL && s->m_spuFill - s->m_dmaPending < s->m_need)
    {
        RxSndDrvAdpcmStop(s);
        return;
    }

    for (RxSndDrvAdpcmChannel *ch = s->m_channels; ch != NULL; ch = ch->m_next)
    {
        int group = 0;
        int owner = 0;

        if (s->m_group > 0 && ch == s->m_channels)
            group = s->m_group;
        if (s->m_owner > 0)
            owner = s->m_owner | 0x2000000;
        ch->m_voice = (signed char)RxSndDrvVoiceAlloc(ch->m_voiceWant, s->m_priority, 0, group, owner,
                                                      RxSndDrvAdpcmVoiceDone, ch);
        if (ch->m_voice < 0)
        {
            RxSndDrvAdpcmStop(s);
            return;
        }
    }

    /* All the data left fits before the SPU buffer wraps: no loop, the
     * voice stops at the end block. */
    CpuSuspendIntr(&intr);
    int once = 0;
    if (s->m_end == 1)
        once = s->m_spuPlay + s->m_spuFill + s->m_dmaPending + s->m_iopFill < s->m_spuSize;
    CpuResumeIntr(intr);

    for (RxSndDrvAdpcmChannel *ch = s->m_channels; ch != NULL; ch = ch->m_next)
    {
        RxSndDrvVoiceSetSample(ch->m_voice, ch->m_spu + (u_int)ch->m_stream->m_spuPlay, 0x8000, 0x8026);
        if (!once)
            RxSndDrvVoiceSetLoop(ch->m_voice, ch->m_spu);
        RxSndDrvVoiceKeyOn(ch->m_voice);
        ch->m_keyed = 0;
    }
    s->m_flags |= RXSNDDRV_ADPCM_PLAYING;
    LogStream("keyed on", s);
    s->m_dirty |= RXSNDDRV_ADPCM_DIRTY_SWITCH | RXSNDDRV_ADPCM_DIRTY_VOLUME | RXSNDDRV_ADPCM_DIRTY_PITCH;
}

/* The tick's last step: move IOP ring data into the SPU buffers, the
 * largest run that wraps neither, every channel on DMA queue 0 (the last
 * channel's transfer counts m_dmaPending down).  A stream sends at most
 * 0x400 bytes a pass; passes repeat while one was cut. */
// IOP 0x0000acd0 (RoR 0x0000a9e0, word for word)
static void RxSndDrvAdpcmFeed(void)
{
    int more;

    do
    {
        more = 0;
        for (RxSndDrvAdpcm *s = s_adpcm.m_active; s != NULL; s = s->m_next)
        {
            if (!(s->m_flags & RXSNDDRV_ADPCM_ACTIVE))
                continue;

            int left = s->m_iopFill;
            if (s->m_spuSize - s->m_spuFill <= left)
                left = s->m_spuSize - s->m_spuFill;
            if (left > 0x400)
            {
                left = 0x400;
                more = 1;
            }

            while (left > 0)
            {
                int run = left;
                int iopRun = s->m_iopSize - s->m_iopRead;
                if (s->m_iopFill < iopRun)
                    iopRun = s->m_iopFill;
                if (iopRun <= run)
                    run = iopRun;
                int spuRun = s->m_spuSize - s->m_spuWrite;
                if (s->m_spuSize - s->m_spuFill < spuRun)
                    spuRun = s->m_spuSize - s->m_spuFill;
                if (spuRun <= run)
                    run = spuRun;
                if (run < 1)
                    break;

                int intr, intr2, intr3;
                CpuSuspendIntr(&intr);
                for (RxSndDrvAdpcmChannel *ch = s->m_channels; ch != NULL; ch = ch->m_next)
                {
                    RxSndDrvDmaDone done = ch->m_next == NULL ? RxSndDrvAdpcmDmaDone : NULL;
                    RxSndDrvDmaPush(0, RXSNDDRV_DMA_IOP_TO_SPU, ch->m_spu + (u_int)ch->m_stream->m_spuWrite,
                                    ch->m_iop + (u_int)ch->m_stream->m_iopRead, run, done, s);
                }

                CpuSuspendIntr(&intr2);
                s->m_iopFill -= run;
                s->m_iopRead += run;
                if (s->m_iopRead >= s->m_iopSize)
                    s->m_iopRead -= s->m_iopSize;
                CpuResumeIntr(intr2);

                s->m_dmaPending += run;

                CpuSuspendIntr(&intr3);
                s->m_spuFill  += run;
                s->m_spuWrite += run;
                if (s->m_spuWrite >= s->m_spuSize)
                    s->m_spuWrite -= s->m_spuSize;
                CpuResumeIntr(intr3);

                CpuResumeIntr(intr);
                RxSndDrvDmaKick();
                left -= run;
            }
        }
    } while (more);
}

// IOP 0x0000af9c (RoR 0x0000acac, word for word)
void RxSndDrvAdpcmInit(void)
{
    memset(&s_adpcm, 0, sizeof s_adpcm);

    for (int i = 0; i < RXSNDDRV_ADPCM_STREAMS; i++)
        s_adpcm.m_stream[i].m_id = (u_char)i;
    for (int i = 0; i < RXSNDDRV_ADPCM_CHANNELS; i++)
    {
        s_adpcm.m_channelPool[i].m_next = s_adpcm.m_channelFree;
        s_adpcm.m_channelFree           = &s_adpcm.m_channelPool[i];
    }
    s_adpcm.m_frameBytes = 0x400;

    if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO)
        printf("snddrv : completed to initialize ADPCM stream system.\n");
}

/* Closes every stream (referenced nowhere in the driver). */
// IOP 0x0000b044 (RoR 0x0000ad54, word for word)
void RxSndDrvAdpcmCloseAll(void)
{
    for (int i = 0; i < RXSNDDRV_ADPCM_STREAMS; i++)
    {
        u_int data[2] = { 0, 0 };
        RxSndDrvAdpcmSet((u_int)i, data);
    }
}

/* Command 0xa1: the EE put `size` more bytes per channel in the IOP rings
 * (only what the rings can take counts); `end` 1: they are the last. */
// IOP 0x0000b084 (RoR 0x0000ad94, word for word)
int RxSndDrvAdpcmData(int id, int size, int end)
{
    RxSndDrvAdpcm *s = &s_adpcm.m_stream[id];
    int            intr, intr2;

    CpuSuspendIntr(&intr);
    if (s->m_flags & RXSNDDRV_ADPCM_USED)
    {
        int n    = size;
        int free = s->m_iopSize - (s->m_iopFill + s->m_dmaPending);
        if (free <= size)
            n = free;

        CpuSuspendIntr(&intr2);
        s->m_iopFill  += n;
        s->m_iopWrite += n;
        if (s->m_iopWrite >= s->m_iopSize)
            s->m_iopWrite -= s->m_iopSize;
        CpuResumeIntr(intr2);
        s->m_end = (u_char)end;
    }
    CpuResumeIntr(intr);
    return size;
}

/* Command 0xa2 {param, value}: into the command bank's set record. */
// IOP 0x0000b190 (RoR 0x0000aea0, word for word)
void RxSndDrvAdpcmSet(u_int id, const u_int *data)
{
    RxSndDrvAdpcm *s = &s_adpcm.m_stream[id];
    int            intr;

    CpuSuspendIntr(&intr);
    if (s->m_flags & RXSNDDRV_ADPCM_USED)
    {
        RxSndDrvAdpcmPending *p = &s->m_pending[RxSndDrvCmdBank()];
        switch (data[0])
        {
        case 0:
            p->m_mask |= RXSNDDRV_ADPCM_SET_CLOSE;
            break;
        case 1:
            p->m_mask  |= RXSNDDRV_ADPCM_SET_PAUSE;
            p->m_pause  = (u_char)data[1];
            break;
        case 2:
            p->m_mask   |= RXSNDDRV_ADPCM_SET_REVERB;
            p->m_reverb  = (u_char)data[1];
            break;
        case 3:
            p->m_mask |= RXSNDDRV_ADPCM_SET_VOLUME;
            p->m_vol   = data[1];
            break;
        case 4:
            p->m_mask  |= RXSNDDRV_ADPCM_SET_PITCH;
            p->m_pitch  = (short)data[1];
            break;
        case 6:
            p->m_mask  |= RXSNDDRV_ADPCM_SET_GROUP;
            p->m_group  = (short)data[1];
            break;
        case 7:
            p->m_mask  |= RXSNDDRV_ADPCM_SET_OWNER;
            p->m_owner  = (short)data[1];
            break;
        }
    }
    CpuResumeIntr(intr);
}

/* Command 0xa4 {param, value} for channel `no` of stream `id`: 3 volume
 * L | R << 16, 5 the voice to allocate. */
// IOP 0x0000b30c (RoR 0x0000b01c, word for word)
void RxSndDrvAdpcmChannelSet(int id, u_int no, const u_int *data)
{
    RxSndDrvAdpcm *s = &s_adpcm.m_stream[id];
    int            intr;

    CpuSuspendIntr(&intr);
    if (s->m_flags & RXSNDDRV_ADPCM_USED)
    {
        for (RxSndDrvAdpcmChannel *ch = s->m_channels; ch != NULL; ch = ch->m_next)
        {
            if (ch->m_no != no)
                continue;
            RxSndDrvAdpcmChannelPending *cp = &ch->m_pending[RxSndDrvCmdBank()];
            if (data[0] == 3)
            {
                cp->m_mask |= RXSNDDRV_ADPCM_CH_SET_VOLUME;
                cp->m_vol   = data[1];
            }
            else if (data[0] == 5)
            {
                cp->m_mask  |= RXSNDDRV_ADPCM_CH_SET_VOICE;
                cp->m_voice  = data[1];
            }
        }
    }
    CpuResumeIntr(intr);
}

/* Command 0xa3: {id, status, -, -, free, posHi, posLo} per open stream;
 * answers the count.  `free` is what the IOP rings can take. */
// IOP 0x0000b450 (RoR 0x0000b160, word for word)
int RxSndDrvAdpcmStatus(u_char *reply)
{
    int n = 0;

    for (RxSndDrvAdpcm *s = s_adpcm.m_active; s != NULL; s = s->m_next)
    {
        int free = s->m_iopSize - (s->m_iopFill + s->m_dmaPending);

        reply[0] = s->m_id;
        reply[1] = (u_char)s->m_flags;
        memcpy(reply + 0x4, &free, 4);
        memcpy(reply + 0x8, &s->m_posHi, 4);
        memcpy(reply + 0xc, &s->m_posLo, 4);
        reply += 0x10;
        n++;
    }
    return n;
}

// IOP 0x0000b4cc (RoR 0x0000b1dc, word for word)
void RxSndDrvAdpcmTick(void)
{
    s_adpcm.m_frameBytes = RXSNDDRV_ADPCM_BYTES_PER_SEC / RxSndDrvGetFrameRate();
    RxSndDrvAdpcmApply();
    RxSndDrvAdpcmReap();
    RxSndDrvAdpcmUpdate();
    RxSndDrvAdpcmFeed();
}

/* Unlinks the closed streams: their transfers cancelled, voices released
 * and channels back in the pool. */
// IOP 0x0000b540 (RoR 0x0000b250, word for word)
static void RxSndDrvAdpcmReap(void)
{
    int intr;

    CpuSuspendIntr(&intr);
    RxSndDrvAdpcm **link = &s_adpcm.m_active;
    while (*link != NULL)
    {
        RxSndDrvAdpcm *s = *link;
        if ((s->m_flags & (RXSNDDRV_ADPCM_ACTIVE | RXSNDDRV_ADPCM_CLOSED))
            != (RXSNDDRV_ADPCM_ACTIVE | RXSNDDRV_ADPCM_CLOSED))
        {
            link = &s->m_next;
            continue;
        }

        RxSndDrvDmaCancelArg(s);
        while (s->m_channels != NULL)
        {
            RxSndDrvAdpcmChannel *ch = s->m_channels;
            if (ch->m_voice >= 0)
                RxSndDrvVoiceRelease(ch->m_voice, 0, 0);
            s->m_channels         = ch->m_next;
            ch->m_next            = s_adpcm.m_channelFree;
            s_adpcm.m_channelFree = ch;
        }
        s->m_flags = 0;
        *link      = s->m_next;
    }
    CpuResumeIntr(intr);
}

/* Releases the stream's voices; it waits to start again. */
// IOP 0x0000b644 (RoR 0x0000b354, word for word)
static void RxSndDrvAdpcmStop(RxSndDrvAdpcm *s)
{
    for (RxSndDrvAdpcmChannel *ch = s->m_channels; ch != NULL; ch = ch->m_next)
    {
        if (ch->m_voice >= 0)
            RxSndDrvVoiceRelease(ch->m_voice, 0, 0);
    }
    s->m_flags &= ~RXSNDDRV_ADPCM_PLAYING;
}

/* The voice layer took a channel's voice: reason 1 (another allocation
 * stole it) stops the stream, and ends it when it has a priority. */
// IOP 0x0000b6b4 (RoR 0x0000b3c4, word for word)
static void RxSndDrvAdpcmVoiceDone(int voice, int reason, void *arg)
{
    RxSndDrvAdpcmChannel *ch = (RxSndDrvAdpcmChannel *)arg;

    (void)voice;
    ch->m_voice = -1;
    if (reason == 1)
    {
        RxSndDrvAdpcmStop(ch->m_stream);
        if (ch->m_stream->m_priority >= 0)
            ch->m_stream->m_flags |= RXSNDDRV_ADPCM_ENDED;
    }
}

/* A stream's run reached SPU RAM (the done of its last channel's transfer;
 * a leaf after 0xb3c4 Ghidra folds into it). */
// IOP 0x0000b770 (RoR 0x0000b480, word for word)
static void RxSndDrvAdpcmDmaDone(void *arg, int size)
{
    ((RxSndDrvAdpcm *)arg)->m_dmaPending -= size;
}
