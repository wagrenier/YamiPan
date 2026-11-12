/* ==========================================================================
 *  bgm.cpp  (RXSNDDRV.IRX 0x45b0..0x6ab0: the BGM MIDI sequencer)
 *
 *  Command 0x70 {slot, xseq} plays an open XSEQ sequence (xseq.cpp) on one of
 *  0x20 BGM slots (rx/rxsnd.c rxSndBgmPlay); 0x71 {slot, param, value}
 *  stops, pauses or sets it (RxSndDrvBgmSet); 0x72 lists the slots still
 *  playing; 0x73 / 0x74 {slot, channel or track, 3, L | R << 16} set one
 *  MIDI channel's or one track's volume.  Like every subsystem the set
 *  commands go into the pending record of the command bank and the V-blank
 *  tick (RxSndDrvBgmTick) applies them.
 *
 *  A sequence is standard MIDI event data, one stream per track (the HEAD
 *  chunk: track count, the division at +2, offsets of the TRCK records).
 *  Each tick every track runs its events until its wait is positive; a
 *  delta of n ticks adds n * m_tickWait, and every V-blank takes m_frameStep
 *  off (RxSndDrvBgmTiming: both in 1/0x1000 of a frame at speed 0x1000).
 *  Notes go to the NOTE layer (0x8f10.., RxSndDrvNotePlay) with the XVAB the
 *  sequence maps the channel's bank to and the channel's program; the slot
 *  keeps each in its note list and pushes volume, pan, pitch and bend to it
 *  when the slot, the track, the channel or the note changes.
 *
 *  Supported: note on / off, control change (bank, modulation, portamento,
 *  data entry, volume, pan, expression, sustain, portamento switch,
 *  sostenuto, RPN 0..2, NRPN 0 / 0xa00 loop start and 1 / 0xf00 loop end,
 *  sound / notes off, reset, mono / poly), program change, pitch bend, sysex
 *  (skipped), meta end of track and tempo.  Aftertouch and channel pressure
 *  are read and dropped.
 *
 *  All pools are static: 0x40 notes, 0x100 loop records, 0x40 tracks and the
 *  0x20 slots, one block cleared at start-up (0x7a90 at RoR IOP 0x1b540).
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "intrman.h"

#include <stdio.h>
#include <string.h>

#define RXSNDDRV_BGM_SLOTS    0x20
#define RXSNDDRV_BGM_TRACKS   0x40
#define RXSNDDRV_BGM_NOTES    0x40
#define RXSNDDRV_BGM_LOOPS    0x100
#define RXSNDDRV_BGM_CHANNELS 0x10

/* RxSndDrvBgm::m_flags (the low byte is the status the EE reads back). */
enum
{
    RXSNDDRV_BGM_USED    = 0x00001,     /* allocated by command 0x70 */
    RXSNDDRV_BGM_PLAYING = 0x00002,
    RXSNDDRV_BGM_ENDED   = 0x00004,     /* every track reached its end */
    RXSNDDRV_BGM_PAUSED  = 0x00008,
    RXSNDDRV_BGM_ACTIVE  = 0x10000,     /* seen by a tick: the sequencer runs it */
    RXSNDDRV_BGM_STOPPED = 0x20000,
};

/* RxSndDrvBgmPending::m_mask */
enum
{
    RXSNDDRV_BGM_SET_OPEN    = 0x001,   /* opened: the tick makes it active */
    RXSNDDRV_BGM_SET_STOP    = 0x002,
    RXSNDDRV_BGM_SET_PAUSE   = 0x004,
    RXSNDDRV_BGM_SET_SWITCH  = 0x008,
    RXSNDDRV_BGM_SET_VOLUME  = 0x010,
    RXSNDDRV_BGM_SET_PITCH   = 0x020,
    RXSNDDRV_BGM_SET_SPEED   = 0x040,
    RXSNDDRV_BGM_SET_CHANNEL = 0x080,   /* a channel's own pending record */
    RXSNDDRV_BGM_SET_TRACK   = 0x100,   /* a track's own pending record */
};

/* The dirty bits of the slot, a track, a channel and a note: what the next
 * update pushes to the NOTE layer. */
enum
{
    RXSNDDRV_BGM_DIRTY_SWITCH = 0x01,
    RXSNDDRV_BGM_DIRTY_VOLUME = 0x02,   /* velocity, pan, volume */
    RXSNDDRV_BGM_DIRTY_PITCH  = 0x04,
    RXSNDDRV_BGM_DIRTY_BEND   = 0x08,
    RXSNDDRV_BGM_DIRTY_MOD    = 0x10,   /* set, read nowhere in the driver */
};

/* RxSndDrvBgmChannel::m_flags */
enum
{
    RXSNDDRV_BGM_CH_MONO      = 0x01,   /* CC 0x7e: a note kills the channel's others */
    RXSNDDRV_BGM_CH_SUSTAIN   = 0x02,   /* CC 0x40 >= 0x40 */
    RXSNDDRV_BGM_CH_SOSTENUTO = 0x04,   /* CC 0x42 >= 0x40 */
    RXSNDDRV_BGM_CH_PORTA     = 0x08,   /* CC 0x41 >= 0x40 */
};

/* RxSndDrvBgmNote::m_state */
enum
{
    RXSNDDRV_BGM_NOTE_ON       = 0,
    RXSNDDRV_BGM_NOTE_RELEASED = 1,     /* keyed off, freed when the NOTE ends */
    RXSNDDRV_BGM_NOTE_KILLED   = 2,     /* NOTE freed, the record goes next update */
};

/* NOTE flag (RxSndDrvNoteGetFlags): still sounding. */
#define RXSNDDRV_NOTE_PLAYING 0x2

/* {mask, value} of the channel and track set commands. */
struct RxSndDrvBgmSub
{
    u_int m_mask;                       // IOP +0x0: 1 volume
    u_int m_vol;                        // IOP +0x4: L | R << 16
};
static_assert(sizeof(RxSndDrvBgmSub) == 0x8, "RxSndDrvBgmSub");
static_assert(offsetof(RxSndDrvBgmSub, m_vol) == 0x4, "RxSndDrvBgmSub::m_vol");

/* A loop of NRPN 0 / 1 (0xc on the IOP, 0x100 at RoR IOP 0x1ba44).  layout: host. */
struct RxSndDrvBgmLoop
{
    RxSndDrvBgmLoop *m_next;            // IOP +0x0
    u_char          *m_pos;             // IOP +0x4: the event after the loop start
    u_char           m_count;           // IOP +0x8: times jumped back
    u_char           m_times;           // IOP +0x9: 0 forever
};

/* A MIDI channel (0x28, 16 per slot).  Plain data: IOP layout. */
struct RxSndDrvBgmChannel
{
    u_char  m_dirty;                    // IOP +0x00: RXSNDDRV_BGM_DIRTY_*
    u_char  m_flags;                    // IOP +0x01: RXSNDDRV_BGM_CH_*
    u_char  m_volume;                   // IOP +0x02: CC 7
    u_char  m_pan;                      // IOP +0x03: CC 10
    u_char  m_bank;                     // IOP +0x04: CC 0 << 7 | CC 0x20, kept in a byte
    u_char  m_program;                  // IOP +0x05
    u_char  m_lastKey;                  // IOP +0x06: the last note on, CC 0x54; 0xff none
    u_char  m_portaTime;                // IOP +0x07: CC 5
    u_char  m_expression;               // IOP +0x08: CC 11
    u_char  m_modulation;               // IOP +0x09: CC 1
    u_char  m_bend;                     // IOP +0x0a: pitch bend MSB, 0x40 centre
    u_char  m_bendRange;                // IOP +0x0b: RPN 0
    u_char  m_fineTune;                 // IOP +0x0c: RPN 1
    u_char  m_coarseTune;               // IOP +0x0d: RPN 2
    u_short m_data;                     // IOP +0x0e: data entry, CC 6 << 7 | CC 0x26
    short   m_rpn;                      // IOP +0x10: 0x3fff none
    short   m_nrpn;                     // IOP +0x12: 0x3fff none
    short   m_vol[2];                   // IOP +0x14: L, R (command 0x73), 0x80 unity
    RxSndDrvBgmSub m_pending[2];        // IOP +0x18: per bank
};
static_assert(sizeof(RxSndDrvBgmChannel) == 0x28, "RxSndDrvBgmChannel");
static_assert(offsetof(RxSndDrvBgmChannel, m_bend) == 0x0a, "RxSndDrvBgmChannel::m_bend");
static_assert(offsetof(RxSndDrvBgmChannel, m_data) == 0x0e, "RxSndDrvBgmChannel::m_data");
static_assert(offsetof(RxSndDrvBgmChannel, m_rpn) == 0x10, "RxSndDrvBgmChannel::m_rpn");
static_assert(offsetof(RxSndDrvBgmChannel, m_nrpn) == 0x12, "RxSndDrvBgmChannel::m_nrpn");
static_assert(offsetof(RxSndDrvBgmChannel, m_vol) == 0x14, "RxSndDrvBgmChannel::m_vol");
static_assert(offsetof(RxSndDrvBgmChannel, m_pending) == 0x18, "RxSndDrvBgmChannel::m_pending");

/* A track (0x3c on the IOP, 0x40 at RoR IOP 0x1c648).  layout: host. */
struct RxSndDrvBgmTrack
{
    RxSndDrvBgmTrack *m_next;           // IOP +0x00
    u_char  *m_trck;                    // IOP +0x04: its TRCK record
    u_char  *m_pos;                     // IOP +0x08: the next event
    u_char  *m_start;                   // IOP +0x0c
    int      m_offset;                  // IOP +0x10: m_pos - m_start before the event (status 0x72)
    u_char   m_no;                      // IOP +0x14: index in HEAD (command 0x74)
    u_char   m_dirty;                   // IOP +0x15
    u_char   m_ended;                   // IOP +0x16
    u_char   m_status;                  // IOP +0x17: running status
    int      m_notes;                   // IOP +0x18: notes it started still in the list
    int      m_wait;                    // IOP +0x1c: 1/0x1000 frames to the next event
    short    m_vol[2];                  // IOP +0x20: L, R (command 0x74), 0x80 unity
    short    m_pitch;                   // IOP +0x24: 0x1000 unity
    RxSndDrvBgmLoop *m_loops;           // IOP +0x28: innermost first
    RxSndDrvBgmSub   m_pending[2];      // IOP +0x2c: per bank
};

/* A note started by a track (0x14 on the IOP, 0x40 at RoR IOP 0x1b540).
 * layout: host. */
struct RxSndDrvBgmNote
{
    RxSndDrvBgmNote    *m_next;         // IOP +0x00
    RxSndDrvBgmTrack   *m_track;        // IOP +0x04
    RxSndDrvBgmChannel *m_channel;      // IOP +0x08
    signed char         m_note;         // IOP +0x0c: the NOTE layer's id, -1 none
    u_char              m_dirty;        // IOP +0x0d
    u_char              m_state;        // IOP +0x0e: RXSNDDRV_BGM_NOTE_*
    u_char              m_key;          // IOP +0x0f
    u_char              m_velocity;     // IOP +0x10
    u_char              m_sostenuto;    // IOP +0x11: held by CC 0x42
};

/* The set commands' record, per bank (0x10). */
struct RxSndDrvBgmPending
{
    u_int  m_mask;                      // IOP +0x0: RXSNDDRV_BGM_SET_*
    u_char m_stop;                      // IOP +0x4: 2 cut, else release
    u_char m_pause;                     // IOP +0x5: 1 release, 2 cut, else resume
    u_char m_switch;                    // IOP +0x6
    u_int  m_vol;                       // IOP +0x8: L | R << 16
    short  m_pitch;                     // IOP +0xc
    short  m_speed;                     // IOP +0xe
};
static_assert(sizeof(RxSndDrvBgmPending) == 0x10, "RxSndDrvBgmPending");
static_assert(offsetof(RxSndDrvBgmPending, m_stop) == 0x4, "RxSndDrvBgmPending::m_stop");
static_assert(offsetof(RxSndDrvBgmPending, m_vol) == 0x8, "RxSndDrvBgmPending::m_vol");
static_assert(offsetof(RxSndDrvBgmPending, m_pitch) == 0xc, "RxSndDrvBgmPending::m_pitch");
static_assert(offsetof(RxSndDrvBgmPending, m_speed) == 0xe, "RxSndDrvBgmPending::m_speed");

/* A BGM slot (0x2d4 on the IOP, 0x20 at RoR IOP 0x1d54c).  layout: host. */
struct RxSndDrvBgm
{
    RxSndDrvBgm *m_next;                // IOP +0x000: the active list
    int      m_xseq;                    // IOP +0x004
    u_int    m_flags;                   // IOP +0x008: RXSNDDRV_BGM_*
    int      m_division;                // IOP +0x00c: ticks a quarter note (HEAD +2)
    int      m_tempo;                   // IOP +0x010: microseconds a quarter note
    int      m_tickWait;                // IOP +0x014: m_wait a MIDI tick
    int      m_frameStep;               // IOP +0x018: m_wait a V-blank
    u_char   m_no;                      // IOP +0x01c
    u_char   m_switch;                  // IOP +0x01d: RxSndDrvNoteSetSwitch for every note
    u_char   m_dirty;                   // IOP +0x01e
    short    m_vol[2];                  // IOP +0x020: L, R, 0x80 unity
    short    m_pitch;                   // IOP +0x024: 0x1000 unity
    short    m_speed;                   // IOP +0x026: 0x1000 unity
    int      m_seek;                    // IOP +0x028: run events without waiting up to this offset
    RxSndDrvBgmTrack  *m_tracks;        // IOP +0x02c
    RxSndDrvBgmChannel m_channel[RXSNDDRV_BGM_CHANNELS]; // IOP +0x030
    RxSndDrvBgmNote   *m_notes;         // IOP +0x2b0
    RxSndDrvBgmPending m_pending[2];    // IOP +0x2b4: per bank
};

/* The sequencer's block (0x7a90 at RoR IOP 0x1b540).  layout: host. */
struct RxSndDrvBgmState
{
    RxSndDrvBgmNote   m_notePool[RXSNDDRV_BGM_NOTES];   // IOP +0x0000
    RxSndDrvBgmNote  *m_noteFree;                       // IOP +0x0500
    RxSndDrvBgmLoop   m_loopPool[RXSNDDRV_BGM_LOOPS];   // IOP +0x0504
    RxSndDrvBgmLoop  *m_loopFree;                       // IOP +0x1104
    RxSndDrvBgmTrack  m_trackPool[RXSNDDRV_BGM_TRACKS]; // IOP +0x1108
    RxSndDrvBgmTrack *m_trackFree;                      // IOP +0x2008
    RxSndDrvBgm       m_slot[RXSNDDRV_BGM_SLOTS];       // IOP +0x200c
    RxSndDrvBgm      *m_active;                         // IOP +0x7a8c
};

static RxSndDrvBgmState s_bgm;          // RoR IOP 0x0001b540

typedef void (*RxSndDrvBgmEvent)(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);

static void RxSndDrvBgmUpdate(RxSndDrvBgm *bgm);
static void RxSndDrvBgmRunTrack(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track);
static void RxSndDrvBgmNoteOn(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);
static void RxSndDrvBgmControl(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);
static void RxSndDrvBgmResetControllers(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);
static void RxSndDrvBgmApply(void);
static void RxSndDrvBgmEndTrack(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track);
static void RxSndDrvBgmStopTracks(RxSndDrvBgm *bgm, int release);
static void RxSndDrvBgmStopNotes(RxSndDrvBgm *bgm, int release);
static void RxSndDrvBgmStopChannel(RxSndDrvBgm *bgm, RxSndDrvBgmChannel *ch, int release);
static void RxSndDrvBgmTiming(RxSndDrvBgm *bgm);
static void RxSndDrvBgmReap(void);
static void RxSndDrvBgmUpdateAll(void);
static void RxSndDrvBgmNoteOff(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);
static void RxSndDrvBgmAftertouch(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);
static void RxSndDrvBgmProgram(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);
static void RxSndDrvBgmPressure(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);
static void RxSndDrvBgmBend(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);
static void RxSndDrvBgmSystem(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);
static void RxSndDrvBgmSustain(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch, int on);
static void RxSndDrvBgmSostenuto(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch, int on);
static void RxSndDrvBgmSoundOff(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);
static void RxSndDrvBgmNotesOff(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch);
static void RxSndDrvBgmParam(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch, int data);
static void RxSndDrvBgmLoopStart(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch, int data);
static void RxSndDrvBgmLoopEnd(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch, int data);
static void RxSndDrvBgmSysex(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track);
static void RxSndDrvBgmMeta(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track);

static u_int ReadVarLen(u_char **pos)
{
    u_char *p = *pos;
    u_int   v = 0;
    u_char  c;

    do
    {
        c = *p++;
        v = v << 7 | (c & 0x7f);
    } while (c & 0x80);
    *pos = p;
    return v;
}

static int ReadInt(const u_char *p)
{
    int v;
    memcpy(&v, p, sizeof v);
    return v;
}

/* Releases (key off) or marks killed one note of the list. */
static void ReleaseNote(RxSndDrvBgmNote *n)
{
    if (n->m_note >= 0)
        RxSndDrvNoteKeyOff(n->m_note);
    n->m_state = RXSNDDRV_BGM_NOTE_RELEASED;
}

// IOP 0x00004800 (RoR 0x000045b0, word for word)
void RxSndDrvBgmInit(void)
{
    memset(&s_bgm, 0, sizeof s_bgm);

    for (int i = 0; i < RXSNDDRV_BGM_NOTES; i++)
    {
        s_bgm.m_notePool[i].m_next = s_bgm.m_noteFree;
        s_bgm.m_noteFree           = &s_bgm.m_notePool[i];
    }
    for (int i = 0; i < RXSNDDRV_BGM_LOOPS; i++)
    {
        s_bgm.m_loopPool[i].m_next = s_bgm.m_loopFree;
        s_bgm.m_loopFree           = &s_bgm.m_loopPool[i];
    }
    for (int i = 0; i < RXSNDDRV_BGM_TRACKS; i++)
    {
        s_bgm.m_trackPool[i].m_next = s_bgm.m_trackFree;
        s_bgm.m_trackFree           = &s_bgm.m_trackPool[i];
    }
    for (int i = 0; i < RXSNDDRV_BGM_SLOTS; i++)
        s_bgm.m_slot[i].m_no = (u_char)i;
}

/* Command 0x70: slot `slot` plays XSEQ `xseq` from its start; 1, or 0 when
 * the slot is taken, the sequence not open or the track pool empty. */
// IOP 0x000048d0 (RoR 0x00004680, word for word)
int RxSndDrvBgmOpen(u_int slot, u_int xseq)
{
    int intr;
    int ok = 0;

    /* PC: the IOP does not range-check the slot. */
    if (slot >= RXSNDDRV_BGM_SLOTS)
        return 0;

    CpuSuspendIntr(&intr);
    RxSndDrvBgm *bgm = &s_bgm.m_slot[slot];

    if (bgm->m_flags & RXSNDDRV_BGM_USED)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_WARN)
            printf("snddrv : can't allocate BGM slot\n");
    }
    else if (!RxSndDrvXseqLock((int)xseq))
    {
        /* sic: the driver says XVAB for the sequence */
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
            printf("snddrv : unregisted XVAB ID '%d'\n", xseq);
    }
    else
    {
        u_int *headp = RxSndDrvXseqGetHead(xseq);
        if (headp == NULL)
        {
            if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
                printf("snddrv : unregisted XVAB ID '%d'\n", xseq);
            goto fail;
        }

        u_char *head = (u_char *)(uintptr_t)*headp;
        for (int i = 0; i < head[0]; i++)
        {
            RxSndDrvBgmTrack *t = s_bgm.m_trackFree;
            if (t == NULL)
                goto fail;
            int trck = ReadInt(head + 4 + i * 4);

            s_bgm.m_trackFree = t->m_next;
            t->m_next         = bgm->m_tracks;
            bgm->m_tracks     = t;

            t->m_trck   = head + trck;
            int data    = ReadInt(head + trck + 4);
            t->m_offset = 0;
            t->m_no     = (u_char)i;
            t->m_dirty  = 0;
            t->m_ended  = 0;
            t->m_status = 0;
            t->m_wait   = 0;
            t->m_vol[0] = 0x80;
            t->m_vol[1] = 0x80;
            t->m_pitch  = 0x1000;
            t->m_pending[0].m_mask = 0;
            t->m_pending[1].m_mask = 0;
            /* +1: past the first event's delta */
            t->m_pos   = head + data + 1;
            t->m_start = head + data + 1;
        }

        bgm->m_xseq      = (int)xseq;
        bgm->m_flags     = RXSNDDRV_BGM_USED | RXSNDDRV_BGM_PLAYING;
        short division   = (short)(head[2] | head[3] << 8);
        bgm->m_vol[0]    = 0x80;
        bgm->m_vol[1]    = 0x80;
        bgm->m_tempo     = 500000;
        bgm->m_tickWait  = 0;
        bgm->m_frameStep = 0;
        bgm->m_switch    = 0;
        bgm->m_dirty     = 0;
        bgm->m_pitch     = 0x1000;
        bgm->m_speed     = 0x1000;
        bgm->m_seek      = 0;
        bgm->m_division  = division;

        int bank = RxSndDrvCmdBank();
        bgm->m_pending[bank].m_mask = RXSNDDRV_BGM_SET_OPEN;
        bank = RxSndDrvCmdBank();
        bgm->m_pending[bank == 0].m_mask = 0;

        for (int c = 0; c < RXSNDDRV_BGM_CHANNELS; c++)
        {
            RxSndDrvBgmChannel *ch = &bgm->m_channel[c];

            ch->m_dirty      = 0;
            ch->m_flags      = 0;
            ch->m_volume     = 0x7f;
            ch->m_pan        = 0x40;
            ch->m_bank       = 0;
            ch->m_program    = 0;
            ch->m_lastKey    = 0xff;
            ch->m_portaTime  = 0x7f;
            ch->m_expression = 0x7f;
            ch->m_modulation = 0;
            ch->m_bend       = 0x40;
            ch->m_bendRange  = 0;
            ch->m_fineTune   = 0x40;
            ch->m_coarseTune = 0x40;
            ch->m_data       = 0;
            ch->m_rpn        = 0x3fff;
            ch->m_nrpn       = 0x3fff;
            ch->m_vol[0]     = 0x80;
            ch->m_vol[1]     = 0x80;
            ch->m_pending[0].m_mask = 0;
            ch->m_pending[1].m_mask = 0;
        }

        RxSndDrvBgmTiming(bgm);
        ok            = 1;
        bgm->m_next   = s_bgm.m_active;
        s_bgm.m_active = bgm;
    }
    goto done;

fail:
    RxSndDrvXseqUnlock((int)xseq);
    while (bgm->m_tracks != NULL)
    {
        RxSndDrvBgmTrack *t = bgm->m_tracks;
        bgm->m_tracks      = t->m_next;
        t->m_next          = s_bgm.m_trackFree;
        s_bgm.m_trackFree  = t;
    }

done:
    CpuResumeIntr(intr);
    if (!ok && (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO))
        printf("snddrv : failed to play BGM slot '%d'\n", slot);
    return ok;
}

/* The set commands of the slots: the pending records of the tick bank. */
// IOP 0x00004c5c (RoR 0x00004a0c, word for word)
static void RxSndDrvBgmApply(void)
{
    for (RxSndDrvBgm *bgm = s_bgm.m_active; bgm != NULL; bgm = bgm->m_next)
    {
        if (bgm->m_pending[RxSndDrvTickBank()].m_mask & RXSNDDRV_BGM_SET_OPEN)
            bgm->m_flags |= RXSNDDRV_BGM_ACTIVE;
    }

    for (RxSndDrvBgm *bgm = s_bgm.m_active; bgm != NULL; bgm = bgm->m_next)
    {
        RxSndDrvBgmPending *p = &bgm->m_pending[RxSndDrvTickBank()];

        if (bgm->m_flags & RXSNDDRV_BGM_ACTIVE)
        {
            if (p->m_mask & RXSNDDRV_BGM_SET_STOP)
            {
                RxSndDrvBgmStopTracks(bgm, p->m_stop == 1 || p->m_stop != 2);
                bgm->m_flags |= RXSNDDRV_BGM_STOPPED;
            }
            if (p->m_mask & RXSNDDRV_BGM_SET_PAUSE)
            {
                if (p->m_pause == 1 || p->m_pause == 2)
                {
                    RxSndDrvBgmStopNotes(bgm, p->m_pause == 1);
                    bgm->m_flags |= RXSNDDRV_BGM_PAUSED;
                    bgm->m_flags &= ~RXSNDDRV_BGM_PLAYING;
                }
                else
                {
                    bgm->m_flags &= ~RXSNDDRV_BGM_PAUSED;
                    bgm->m_flags |= RXSNDDRV_BGM_PLAYING;
                }
            }
            if (p->m_mask & RXSNDDRV_BGM_SET_SWITCH)
            {
                bgm->m_switch = p->m_switch;
                bgm->m_dirty |= RXSNDDRV_BGM_DIRTY_SWITCH;
            }
            if (p->m_mask & RXSNDDRV_BGM_SET_VOLUME)
            {
                bgm->m_vol[0] = (short)(p->m_vol & 0xffff);
                bgm->m_vol[1] = (short)(p->m_vol >> 16);
                bgm->m_dirty |= RXSNDDRV_BGM_DIRTY_VOLUME;
            }
            if (p->m_mask & RXSNDDRV_BGM_SET_PITCH)
            {
                bgm->m_pitch = p->m_pitch;
                bgm->m_dirty |= RXSNDDRV_BGM_DIRTY_PITCH;
            }
            if (p->m_mask & RXSNDDRV_BGM_SET_SPEED)
            {
                bgm->m_speed = p->m_speed;
                RxSndDrvBgmTiming(bgm);
            }
            if (p->m_mask & RXSNDDRV_BGM_SET_CHANNEL)
            {
                for (int c = 0; c < RXSNDDRV_BGM_CHANNELS; c++)
                {
                    RxSndDrvBgmChannel *ch  = &bgm->m_channel[c];
                    RxSndDrvBgmSub     *sub = &ch->m_pending[RxSndDrvTickBank()];

                    if (sub->m_mask & 1)
                    {
                        ch->m_vol[0] = (short)(sub->m_vol & 0xffff);
                        ch->m_vol[1] = (short)(sub->m_vol >> 16);
                        ch->m_dirty |= RXSNDDRV_BGM_DIRTY_VOLUME;
                    }
                    sub->m_mask = 0;
                }
            }
            if (p->m_mask & RXSNDDRV_BGM_SET_TRACK)
            {
                for (RxSndDrvBgmTrack *t = bgm->m_tracks; t != NULL; t = t->m_next)
                {
                    RxSndDrvBgmSub *sub = &t->m_pending[RxSndDrvTickBank()];

                    if (sub->m_mask & 1)
                    {
                        t->m_vol[0] = (short)(sub->m_vol & 0xffff);
                        t->m_vol[1] = (short)(sub->m_vol >> 16);
                        t->m_dirty |= RXSNDDRV_BGM_DIRTY_VOLUME;
                    }
                    sub->m_mask = 0;
                }
            }
        }
        p->m_mask = 0;
    }
}

/* One V-blank of an active slot: frees finished tracks, runs the others'
 * events, frees the notes that ended and pushes the changed parameters to
 * the rest. */
// IOP 0x00004fd8 (RoR 0x00004d88, word for word)
static void RxSndDrvBgmUpdate(RxSndDrvBgm *bgm)
{
    int intr;

    CpuSuspendIntr(&intr);
    for (RxSndDrvBgmTrack **pp = &bgm->m_tracks; *pp != NULL;)
    {
        RxSndDrvBgmTrack *t = *pp;

        if (t->m_ended && t->m_notes < 1)
        {
            while (t->m_loops != NULL)
            {
                RxSndDrvBgmLoop *l = t->m_loops;
                t->m_loops        = l->m_next;
                l->m_next         = s_bgm.m_loopFree;
                s_bgm.m_loopFree  = l;
            }
            *pp               = t->m_next;
            t->m_next         = s_bgm.m_trackFree;
            s_bgm.m_trackFree = t;
            continue;
        }
        pp = &t->m_next;
    }
    CpuResumeIntr(intr);

    if (!(bgm->m_flags & RXSNDDRV_BGM_STOPPED) && !(bgm->m_flags & RXSNDDRV_BGM_PAUSED))
    {
        for (RxSndDrvBgmTrack *t = bgm->m_tracks; t != NULL; t = t->m_next)
        {
            if (!t->m_ended)
                RxSndDrvBgmRunTrack(bgm, t);
        }
    }

    u_char dirty = bgm->m_dirty;
    for (RxSndDrvBgmNote **pp = &bgm->m_notes; *pp != NULL;)
    {
        RxSndDrvBgmNote *n = *pp;

        if (n->m_state == RXSNDDRV_BGM_NOTE_KILLED ||
            !(RxSndDrvNoteGetFlags(n->m_note) & RXSNDDRV_NOTE_PLAYING))
        {
            if (n->m_note >= 0)
                RxSndDrvNoteFree(n->m_note);
            n->m_track->m_notes--;
            *pp              = n->m_next;
            n->m_next        = s_bgm.m_noteFree;
            s_bgm.m_noteFree = n;
            continue;
        }

        RxSndDrvBgmTrack   *t    = n->m_track;
        RxSndDrvBgmChannel *ch   = n->m_channel;
        u_char              bits = (u_char)(dirty | t->m_dirty | ch->m_dirty | n->m_dirty);
        if (bits != 0)
        {
            if (bits & RXSNDDRV_BGM_DIRTY_SWITCH)
                RxSndDrvNoteSetSwitch(n->m_note, bgm->m_switch != 0);
            if (bits & RXSNDDRV_BGM_DIRTY_VOLUME)
            {
                int vol = (int)(ch->m_volume * ch->m_expression) >> 7;
                int l   = ((vol * ch->m_vol[0] >> 7) * t->m_vol[0] >> 7) * bgm->m_vol[0] >> 7;
                int r   = ((vol * ch->m_vol[1] >> 7) * t->m_vol[1] >> 7) * bgm->m_vol[1] >> 7;

                RxSndDrvNoteSetVelocity(n->m_note, n->m_velocity);
                RxSndDrvNoteSetPan(n->m_note, ch->m_pan);
                RxSndDrvNoteSetVolume(n->m_note, (short)l, (short)r);
            }
            if (bits & RXSNDDRV_BGM_DIRTY_PITCH)
                RxSndDrvNoteSetPitch(n->m_note, bgm->m_pitch * t->m_pitch >> 12);
            if (bits & RXSNDDRV_BGM_DIRTY_BEND)
                RxSndDrvNoteSetBend(n->m_note, (ch->m_bend - 0x40) * 0x40);
        }
        n->m_dirty = 0;
        pp         = &n->m_next;
    }

    for (RxSndDrvBgmTrack *t = bgm->m_tracks; t != NULL; t = t->m_next)
        t->m_dirty = 0;
    for (int c = RXSNDDRV_BGM_CHANNELS - 1; c >= 0; c--)
        bgm->m_channel[c].m_dirty = 0;
    bgm->m_dirty = 0;
}

/* Runs a track's events until its next one is in the future (or, while
 * the slot seeks, until the track is past the seek offset). */
// IOP 0x00005380 (RoR 0x00005130, word for word)
static void RxSndDrvBgmRunTrack(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track)
{
    int wait = track->m_wait;

    for (;;)
    {
        if (wait > 0 && bgm->m_seek < 1)
            break;

        track->m_offset = (int)(track->m_pos - track->m_start);
        if (bgm->m_seek > 0 && bgm->m_seek <= (int)(track->m_pos - track->m_start))
        {
            track->m_wait = 0;
            bgm->m_seek   = 0;
            break;
        }

        u_char status = *track->m_pos;
        if (status & 0x80)
        {
            track->m_status = status;
            track->m_pos++;
        }
        else
        {
            status = track->m_status;
        }

        RxSndDrvBgmChannel *ch = &bgm->m_channel[status & 0xf];
        switch (status >> 4)
        {
        case 0x8: RxSndDrvBgmNoteOff(bgm, track, ch); break;
        case 0x9: RxSndDrvBgmNoteOn(bgm, track, ch); break;
        case 0xa: RxSndDrvBgmAftertouch(bgm, track, ch); break;
        case 0xb: RxSndDrvBgmControl(bgm, track, ch); break;
        case 0xc: RxSndDrvBgmProgram(bgm, track, ch); break;
        case 0xd: RxSndDrvBgmPressure(bgm, track, ch); break;
        case 0xe: RxSndDrvBgmBend(bgm, track, ch); break;
        case 0xf: RxSndDrvBgmSystem(bgm, track, ch); break;
        default:
            if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO)
                printf("snddrv : error MIDI message '%X'\n", status);
            RxSndDrvBgmEndTrack(bgm, track);
            break;
        }
        if (track->m_ended)
            break;

        u_int delta   = ReadVarLen(&track->m_pos);
        wait          = track->m_wait + (int)delta * bgm->m_tickWait;
        track->m_wait = wait;
    }
    track->m_wait -= bgm->m_frameStep;
}

/* 0x9n key velocity (velocity 0: a note off). */
// IOP 0x000055c0 (RoR 0x00005370, word for word)
static void RxSndDrvBgmNoteOn(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch)
{
    u_char *p = track->m_pos;

    if (p[1] == 0)
    {
        u_char key   = p[0];
        track->m_pos = p + 2;
        if (!(ch->m_flags & RXSNDDRV_BGM_CH_SUSTAIN))
        {
            for (RxSndDrvBgmNote *n = bgm->m_notes; n != NULL; n = n->m_next)
            {
                if (n->m_channel == ch && n->m_key == key && n->m_state == RXSNDDRV_BGM_NOTE_ON &&
                    !n->m_sostenuto)
                    ReleaseNote(n);
            }
        }
        return;
    }

    u_char key      = p[0];
    u_char velocity = p[1];
    track->m_pos    = p + 2;

    if (ch->m_flags & RXSNDDRV_BGM_CH_MONO)
        RxSndDrvBgmStopChannel(bgm, ch, 0);

    int              xvab = RxSndDrvXseqGetBank(bgm->m_xseq, ch->m_bank);
    RxSndDrvBgmNote *n    = s_bgm.m_noteFree;
    if (xvab >= 0 && n != NULL)
    {
        signed char note = (signed char)RxSndDrvNotePlay(xvab, ch->m_program, key, -1, -1);
        n->m_note        = note;
        if (note >= 0)
        {
            n->m_track   = track;
            n->m_channel = ch;

            u_char dirty = RXSNDDRV_BGM_DIRTY_SWITCH | RXSNDDRV_BGM_DIRTY_VOLUME;
            if (bgm->m_pitch != 0x1000)
                dirty = dirty | RXSNDDRV_BGM_DIRTY_PITCH;
            if (ch->m_bend != 0x40)
                dirty |= RXSNDDRV_BGM_DIRTY_BEND;
            if (ch->m_modulation != 0)
                dirty |= RXSNDDRV_BGM_DIRTY_MOD;
            n->m_dirty       = dirty;
            n->m_key         = key;
            n->m_velocity    = velocity;
            n->m_state       = RXSNDDRV_BGM_NOTE_ON;
            s_bgm.m_noteFree = n->m_next;
            n->m_sostenuto   = (ch->m_flags >> 2) & 1;
            n->m_next        = bgm->m_notes;
            bgm->m_notes     = n;
            track->m_notes++;
        }
    }
    ch->m_lastKey = key;
}

/* 0xBn controller value. */
// IOP 0x000057e4 (RoR 0x00005594, word for word)
static void RxSndDrvBgmControl(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch)
{
    u_char *p     = track->m_pos;
    u_char  cc    = p[0];
    u_char  value = p[1];
    track->m_pos  = p + 2;

    switch (cc)
    {
    case 0x00:                          /* bank select */
        ch->m_bank = (u_char)(value << 7);
        break;
    case 0x01:                          /* modulation */
        ch->m_modulation = value;
        ch->m_dirty |= RXSNDDRV_BGM_DIRTY_MOD;
        break;
    case 0x05:                          /* portamento time */
        ch->m_portaTime = value;
        break;
    case 0x06:                          /* data entry */
        ch->m_data = (u_short)(value << 7);
        RxSndDrvBgmParam(bgm, track, ch, 1);
        break;
    case 0x07:                          /* volume */
        ch->m_volume = value;
        ch->m_dirty |= RXSNDDRV_BGM_DIRTY_VOLUME;
        break;
    case 0x0a:                          /* pan */
        ch->m_pan = value;
        ch->m_dirty |= RXSNDDRV_BGM_DIRTY_VOLUME;
        break;
    case 0x0b:                          /* expression */
        ch->m_expression = value;
        ch->m_dirty |= RXSNDDRV_BGM_DIRTY_VOLUME;
        break;
    case 0x20:                          /* bank select LSB */
        ch->m_bank = (u_char)((ch->m_bank & 0x7f) | value);
        break;
    case 0x21:
    case 0x25:
    case 0x27:
    case 0x2a:
    case 0x2b:
    case 0x43:
    case 0x44:
        break;
    case 0x26:                          /* data entry LSB */
        ch->m_data = (u_short)((ch->m_data & 0x7f) | value);
        RxSndDrvBgmParam(bgm, track, ch, 1);
        break;
    case 0x40:                          /* sustain */
        RxSndDrvBgmSustain(bgm, track, ch, value >= 0x40);
        break;
    case 0x41:                          /* portamento */
        if (value < 0x40)
        {
            if (ch->m_flags & RXSNDDRV_BGM_CH_PORTA)
                ch->m_flags &= ~RXSNDDRV_BGM_CH_PORTA;
        }
        else if (!(ch->m_flags & RXSNDDRV_BGM_CH_PORTA))
        {
            ch->m_lastKey = 0xff;
            ch->m_flags |= RXSNDDRV_BGM_CH_PORTA;
        }
        break;
    case 0x42:                          /* sostenuto */
        RxSndDrvBgmSostenuto(bgm, track, ch, value >= 0x40);
        break;
    case 0x54:                          /* portamento control */
        ch->m_lastKey = value;
        break;
    case 0x60:                          /* data increment */
        ch->m_data = (u_short)(ch->m_data + 1);
        RxSndDrvBgmParam(bgm, track, ch, 1);
        break;
    case 0x61:                          /* data decrement */
        ch->m_data = (u_short)(ch->m_data - 1);
        RxSndDrvBgmParam(bgm, track, ch, 1);
        break;
    case 0x62:                          /* NRPN LSB */
        ch->m_rpn  = 0x3fff;
        ch->m_data = 0;
        ch->m_nrpn = (short)((ch->m_nrpn & 0x7f) | value);
        RxSndDrvBgmParam(bgm, track, ch, 0);
        break;
    case 0x63:                          /* NRPN MSB */
        ch->m_nrpn = (short)(value << 7);
        ch->m_rpn  = 0x3fff;
        ch->m_data = 0;
        RxSndDrvBgmParam(bgm, track, ch, 0);
        break;
    case 0x64:                          /* RPN LSB */
        ch->m_nrpn = 0x3fff;
        ch->m_data = 0;
        ch->m_rpn  = (short)((ch->m_rpn & 0x7f) | value);
        RxSndDrvBgmParam(bgm, track, ch, 0);
        break;
    case 0x65:                          /* RPN MSB */
        ch->m_rpn  = (short)(value << 7);
        ch->m_nrpn = 0x3fff;
        ch->m_data = 0;
        RxSndDrvBgmParam(bgm, track, ch, 0);
        break;
    case 0x78:                          /* all sound off */
        RxSndDrvBgmSoundOff(bgm, track, ch);
        break;
    case 0x79:                          /* reset all controllers */
        RxSndDrvBgmResetControllers(bgm, track, ch);
        break;
    case 0x7b:                          /* all notes off */
        RxSndDrvBgmNotesOff(bgm, track, ch);
        break;
    case 0x7e:                          /* mono */
        ch->m_flags |= RXSNDDRV_BGM_CH_MONO;
        break;
    case 0x7f:                          /* poly */
        ch->m_flags &= ~RXSNDDRV_BGM_CH_MONO;
        break;
    default:
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_WARN)
            printf("snddrv : no supported CC '%X'\n", cc);
        break;
    }
}

/* CC 0x79: releases the held notes, the controllers back to their resets. */
// IOP 0x00005a90 (RoR 0x00005840, word for word)
static void RxSndDrvBgmResetControllers(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch)
{
    (void)track;

    if (ch->m_flags & RXSNDDRV_BGM_CH_SUSTAIN)
    {
        for (RxSndDrvBgmNote *n = bgm->m_notes; n != NULL; n = n->m_next)
        {
            if (n->m_channel == ch && n->m_state == RXSNDDRV_BGM_NOTE_ON)
                ReleaseNote(n);
        }
        ch->m_flags &= ~RXSNDDRV_BGM_CH_SUSTAIN;
    }
    if (ch->m_flags & RXSNDDRV_BGM_CH_SOSTENUTO)
    {
        for (RxSndDrvBgmNote *n = bgm->m_notes; n != NULL; n = n->m_next)
        {
            if (n->m_channel == ch && n->m_state == RXSNDDRV_BGM_NOTE_ON && n->m_sostenuto == 1)
                ReleaseNote(n);
        }
        ch->m_flags &= ~RXSNDDRV_BGM_CH_SOSTENUTO;
    }

    ch->m_dirty      = RXSNDDRV_BGM_DIRTY_MOD | RXSNDDRV_BGM_DIRTY_BEND | RXSNDDRV_BGM_DIRTY_VOLUME;
    ch->m_lastKey    = 0xff;
    ch->m_flags      = 0;
    ch->m_volume     = 0x7f;
    ch->m_pan        = 0x40;
    ch->m_bank       = 0;
    ch->m_program    = 0;
    ch->m_portaTime  = 0;
    ch->m_expression = 0x7f;
    ch->m_modulation = 0;
    ch->m_bend       = 0x40;
    ch->m_bendRange  = 0;
    ch->m_fineTune   = 0;
    ch->m_coarseTune = 0;
    ch->m_data       = 0;
    ch->m_rpn        = 0x3fff;
    ch->m_nrpn       = 0x3fff;
}

/* Command 0x71 {param, value} of slot `slot`: 0 stop, 1 pause, 2 the NOTE
 * switch, 3 volume (L | R << 16), 4 pitch, 5 speed (0x1000 unity), 6 seek
 * (applied at once, not on the tick). */
// IOP 0x00005c3c (RoR 0x000059ec, word for word)
void RxSndDrvBgmSet(u_int slot, const u_int *data)
{
    int intr;

    /* PC: the IOP does not range-check the slot. */
    if (slot >= RXSNDDRV_BGM_SLOTS)
        return;

    CpuSuspendIntr(&intr);
    RxSndDrvBgm *bgm = &s_bgm.m_slot[slot];
    if (bgm->m_flags & RXSNDDRV_BGM_USED)
    {
        RxSndDrvBgmPending *p = &bgm->m_pending[RxSndDrvCmdBank()];

        switch (data[0])
        {
        case 0:
            p->m_mask |= RXSNDDRV_BGM_SET_STOP;
            p->m_stop = (u_char)data[1];
            break;
        case 1:
            p->m_mask |= RXSNDDRV_BGM_SET_PAUSE;
            p->m_pause = (u_char)data[1];
            break;
        case 2:
            p->m_mask |= RXSNDDRV_BGM_SET_SWITCH;
            p->m_switch = (u_char)data[1];
            break;
        case 3:
            p->m_mask |= RXSNDDRV_BGM_SET_VOLUME;
            p->m_vol = data[1];
            break;
        case 4:
            p->m_mask |= RXSNDDRV_BGM_SET_PITCH;
            p->m_pitch = (short)data[1];
            break;
        case 5:
            p->m_mask |= RXSNDDRV_BGM_SET_SPEED;
            p->m_speed = (short)data[1];
            break;
        case 6:
            bgm->m_seek = (int)data[1];
            break;
        }
    }
    CpuResumeIntr(intr);
}

/* Command 0x72: {slot, status} of every slot in the active list into
 * `reply`, the first track's offset into `values`; the count. */
// IOP 0x00005db8 (RoR 0x00005b68, word for word)
int RxSndDrvBgmStatus(u_char *reply, u_char *values)
{
    int intr;
    int count = 0;

    CpuSuspendIntr(&intr);
    for (RxSndDrvBgm *bgm = s_bgm.m_active; bgm != NULL; bgm = bgm->m_next)
    {
        reply[0] = bgm->m_no;
        reply[1] = (u_char)bgm->m_flags;
        reply += 4;

        u_int value = 0;
        if (bgm->m_tracks != NULL)
            value = (u_int)bgm->m_tracks->m_offset;
        memcpy(values, &value, sizeof value);
        values += 4;
        count++;
    }
    CpuResumeIntr(intr);
    return count;
}

/* Command 0x73 {slot, channel, 3, L | R << 16}: a channel's volume. */
// IOP 0x00005e60 (RoR 0x00005c10, word for word)
void RxSndDrvBgmChannelSet(int slot, int channel, const u_int *data)
{
    int intr;

    /* PC: the IOP does not range-check slot or channel. */
    if (slot < 0 || slot >= RXSNDDRV_BGM_SLOTS || channel < 0 || channel >= RXSNDDRV_BGM_CHANNELS)
        return;

    CpuSuspendIntr(&intr);
    RxSndDrvBgm *bgm = &s_bgm.m_slot[slot];
    if (bgm->m_flags & RXSNDDRV_BGM_USED)
    {
        RxSndDrvBgmPending *p   = &bgm->m_pending[RxSndDrvCmdBank()];
        RxSndDrvBgmSub     *sub = &bgm->m_channel[channel].m_pending[RxSndDrvCmdBank()];

        if (data[0] == 3)
        {
            p->m_mask |= RXSNDDRV_BGM_SET_CHANNEL;
            sub->m_mask |= 1;
            sub->m_vol = data[1];
        }
    }
    CpuResumeIntr(intr);
}

/* Command 0x74 {slot, track, 3, L | R << 16}: a track's volume. */
// IOP 0x00005f70 (RoR 0x00005d20, word for word)
void RxSndDrvBgmTrackSet(int slot, u_int track, const u_int *data)
{
    int intr;

    /* PC: the IOP does not range-check the slot. */
    if (slot < 0 || slot >= RXSNDDRV_BGM_SLOTS)
        return;

    CpuSuspendIntr(&intr);
    RxSndDrvBgm *bgm = &s_bgm.m_slot[slot];
    if (bgm->m_flags & RXSNDDRV_BGM_USED)
    {
        RxSndDrvBgmPending *p = &bgm->m_pending[RxSndDrvCmdBank()];

        for (RxSndDrvBgmTrack *t = bgm->m_tracks; t != NULL; t = t->m_next)
        {
            if (t->m_no == track)
            {
                p->m_mask |= RXSNDDRV_BGM_SET_TRACK;
                RxSndDrvBgmSub *sub = &t->m_pending[RxSndDrvCmdBank()];
                if (data[0] == 3)
                {
                    p->m_mask |= RXSNDDRV_BGM_SET_TRACK;
                    sub->m_mask |= 1;
                    sub->m_vol = data[1];
                }
                break;
            }
        }
    }
    CpuResumeIntr(intr);
}

// IOP 0x000060b0 (RoR 0x00005e60, word for word)
void RxSndDrvBgmTick(void)
{
    RxSndDrvBgmApply();
    RxSndDrvBgmReap();
    RxSndDrvBgmUpdateAll();
}

/* A track is over; the slot has ended when every track has. */
// IOP 0x000060e0 (RoR 0x00005e90, word for word)
static void RxSndDrvBgmEndTrack(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track)
{
    track->m_ended = 1;
    for (RxSndDrvBgmTrack *t = bgm->m_tracks; t != NULL; t = t->m_next)
    {
        if (!t->m_ended)
            return;
    }
    bgm->m_flags &= ~RXSNDDRV_BGM_PLAYING;
    bgm->m_flags |= RXSNDDRV_BGM_ENDED;
}

// IOP 0x00006148 (RoR 0x00005ef8, word for word)
static void RxSndDrvBgmStopTracks(RxSndDrvBgm *bgm, int release)
{
    for (RxSndDrvBgmTrack *t = bgm->m_tracks; t != NULL; t = t->m_next)
        t->m_ended = 1;
    RxSndDrvBgmStopNotes(bgm, release);
}

/* Every note of the slot: `release` keys them off, else frees them now. */
// IOP 0x0000618c (RoR 0x00005f3c, word for word)
static void RxSndDrvBgmStopNotes(RxSndDrvBgm *bgm, int release)
{
    for (RxSndDrvBgmNote *n = bgm->m_notes; n != NULL; n = n->m_next)
    {
        if (n->m_state == RXSNDDRV_BGM_NOTE_KILLED)
            continue;
        if (n->m_note >= 0)
        {
            if (release == 0)
            {
                RxSndDrvNoteFree(n->m_note);
                n->m_note = -1;
            }
            else
            {
                RxSndDrvNoteKeyOff(n->m_note);
            }
        }
        n->m_state = release == 0 ? RXSNDDRV_BGM_NOTE_KILLED : RXSNDDRV_BGM_NOTE_RELEASED;
    }
}

/* The same for one channel's notes. */
// IOP 0x0000624c (RoR 0x00005ffc, word for word)
static void RxSndDrvBgmStopChannel(RxSndDrvBgm *bgm, RxSndDrvBgmChannel *ch, int release)
{
    for (RxSndDrvBgmNote *n = bgm->m_notes; n != NULL; n = n->m_next)
    {
        if (n->m_channel != ch || n->m_state == RXSNDDRV_BGM_NOTE_KILLED)
            continue;
        if (n->m_note >= 0)
        {
            if (release == 0)
            {
                RxSndDrvNoteFree(n->m_note);
                n->m_note = -1;
            }
            else
            {
                RxSndDrvNoteKeyOff(n->m_note);
            }
        }
        n->m_state = release == 0 ? RXSNDDRV_BGM_NOTE_KILLED : RXSNDDRV_BGM_NOTE_RELEASED;
    }
}

/* m_tickWait and m_frameStep from tempo, division, speed and frame rate:
 * a second of music is 60 * 0x1000 of m_wait at speed 0x1000. */
// IOP 0x00006328 (RoR 0x000060d8, word for word)
static void RxSndDrvBgmTiming(RxSndDrvBgm *bgm)
{
    int rate = RxSndDrvGetFrameRate();
    int num  = bgm->m_tempo * 60;
    int den  = bgm->m_division * 1000000 >> 12;

    bgm->m_frameStep = rate * bgm->m_speed / 60;
    /* PC: the IOP traps on a zero division (a HEAD division below 5). */
    bgm->m_tickWait = den != 0 ? num / den : 0;
}

/* Takes the active slots whose tracks are all gone off the list. */
// IOP 0x000063e0 (RoR 0x00006190, word for word)
static void RxSndDrvBgmReap(void)
{
    int intr;

    CpuSuspendIntr(&intr);
    for (RxSndDrvBgm **pp = &s_bgm.m_active; *pp != NULL;)
    {
        RxSndDrvBgm *bgm = *pp;

        if ((bgm->m_flags & RXSNDDRV_BGM_ACTIVE) && bgm->m_tracks == NULL)
        {
            RxSndDrvXseqUnlock(bgm->m_xseq);
            *pp          = bgm->m_next;
            bgm->m_flags = 0;
            continue;
        }
        pp = &bgm->m_next;
    }
    CpuResumeIntr(intr);
}

// IOP 0x00006494 (RoR 0x00006244, word for word)
static void RxSndDrvBgmUpdateAll(void)
{
    for (RxSndDrvBgm *bgm = s_bgm.m_active; bgm != NULL; bgm = bgm->m_next)
    {
        if (bgm->m_flags & RXSNDDRV_BGM_ACTIVE)
            RxSndDrvBgmUpdate(bgm);
    }
}

/* 0x8n key velocity. */
// IOP 0x000064f4 (RoR 0x000062a4, word for word)
static void RxSndDrvBgmNoteOff(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch)
{
    u_char key = *track->m_pos;
    track->m_pos += 2;

    if (ch->m_flags & RXSNDDRV_BGM_CH_SUSTAIN)
        return;
    for (RxSndDrvBgmNote *n = bgm->m_notes; n != NULL; n = n->m_next)
    {
        if (n->m_channel == ch && n->m_key == key && n->m_state == RXSNDDRV_BGM_NOTE_ON && !n->m_sostenuto)
            ReleaseNote(n);
    }
}

/* 0xAn key pressure: skipped. */
// IOP 0x000065cc (RoR 0x0000637c, word for word)
static void RxSndDrvBgmAftertouch(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch)
{
    (void)bgm;
    (void)ch;
    track->m_pos += 2;
}

// IOP 0x000065e0 (RoR 0x00006390, word for word)
static void RxSndDrvBgmProgram(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch)
{
    (void)bgm;
    ch->m_program = *track->m_pos++;
}

/* 0xDn pressure: skipped. */
// IOP 0x000065fc (RoR 0x000063ac, word for word)
static void RxSndDrvBgmPressure(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch)
{
    (void)bgm;
    (void)ch;
    track->m_pos += 1;
}

/* 0xEn LSB MSB: only the MSB is kept (the LSB's bit 7 is always 0). */
// IOP 0x00006610 (RoR 0x000063c0, word for word)
static void RxSndDrvBgmBend(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch)
{
    (void)bgm;
    u_char *p    = track->m_pos;
    u_char  msb  = p[1];
    u_char  lsb  = p[0];
    track->m_pos = p + 2;

    ch->m_bend = (u_char)(lsb >> 7 | msb);
    ch->m_dirty |= RXSNDDRV_BGM_DIRTY_BEND;
}

/* 0xF0 / 0xF7 sysex, 0xFF meta; anything else ends the track. */
// IOP 0x00006644 (RoR 0x000063f4, word for word)
static void RxSndDrvBgmSystem(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch)
{
    (void)ch;
    u_char type = track->m_status & 0xf;

    if (type == 0x0 || type == 0x7)
    {
        RxSndDrvBgmSysex(bgm, track);
        return;
    }
    if (type == 0xf)
    {
        RxSndDrvBgmMeta(bgm, track);
        return;
    }
    if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
        printf("snddrv : no supported system message '%X'\n", track->m_status);
    RxSndDrvBgmEndTrack(bgm, track);
}

/* CC 0x40: the pedal holds the channel's notes; letting go releases those
 * still on. */
// IOP 0x00006704 (RoR 0x000064b4, word for word)
static void RxSndDrvBgmSustain(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch, int on)
{
    (void)track;

    if (on == 1)
    {
        if (!(ch->m_flags & RXSNDDRV_BGM_CH_SUSTAIN))
            ch->m_flags |= RXSNDDRV_BGM_CH_SUSTAIN;
        return;
    }
    if (!(ch->m_flags & RXSNDDRV_BGM_CH_SUSTAIN))
        return;
    for (RxSndDrvBgmNote *n = bgm->m_notes; n != NULL; n = n->m_next)
    {
        if (n->m_channel == ch && n->m_state == RXSNDDRV_BGM_NOTE_ON)
            ReleaseNote(n);
    }
    ch->m_flags &= ~RXSNDDRV_BGM_CH_SUSTAIN;
}

/* CC 0x42: holds the notes on at the press; letting go releases them. */
// IOP 0x000067d8 (RoR 0x00006588, word for word)
static void RxSndDrvBgmSostenuto(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch, int on)
{
    (void)track;

    if (on == 1)
    {
        if (ch->m_flags & RXSNDDRV_BGM_CH_SOSTENUTO)
            return;
        for (RxSndDrvBgmNote *n = bgm->m_notes; n != NULL; n = n->m_next)
        {
            if (n->m_channel == ch && n->m_state == RXSNDDRV_BGM_NOTE_ON)
                n->m_sostenuto = 1;
        }
        ch->m_flags |= RXSNDDRV_BGM_CH_SOSTENUTO;
        return;
    }
    if (!(ch->m_flags & RXSNDDRV_BGM_CH_SOSTENUTO))
        return;
    for (RxSndDrvBgmNote *n = bgm->m_notes; n != NULL; n = n->m_next)
    {
        if (n->m_channel == ch && n->m_state == RXSNDDRV_BGM_NOTE_ON && n->m_sostenuto == 1)
            ReleaseNote(n);
    }
    ch->m_flags &= ~RXSNDDRV_BGM_CH_SOSTENUTO;
}

/* CC 0x78. */
// IOP 0x0000690c (RoR 0x000066bc, word for word)
static void RxSndDrvBgmSoundOff(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch)
{
    (void)track;
    RxSndDrvBgmStopChannel(bgm, ch, 0);
}

/* CC 0x7b. */
// IOP 0x00006930 (RoR 0x000066e0, word for word)
static void RxSndDrvBgmNotesOff(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch)
{
    (void)track;
    RxSndDrvBgmStopChannel(bgm, ch, 1);
}

/* A parameter number was selected (`data` 0) or its data entered (1).
 * RPN 0..2 take the data MSB; NRPN 0 / 0xa00 and 1 / 0xf00 are the loops. */
// IOP 0x00006954 (RoR 0x00006704, word for word)
static void RxSndDrvBgmParam(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch, int data)
{
    if (ch->m_rpn == 0x3fff)
    {
        switch (ch->m_nrpn)
        {
        case 0x3fff:
            return;
        case 0x000:
        case 0xa00:
            RxSndDrvBgmLoopStart(bgm, track, ch, data);
            return;
        case 0x001:
        case 0xf00:
            RxSndDrvBgmLoopEnd(bgm, track, ch, data);
            return;
        }
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_WARN)
            printf("snddrv : no supported NRPN code '%X'\n", ch->m_nrpn);
        return;
    }

    switch (ch->m_rpn)
    {
    case 0:
        ch->m_bendRange = (u_char)(ch->m_data >> 7);
        return;
    case 1:
        ch->m_fineTune = (u_char)(ch->m_data >> 7);
        return;
    case 2:
        ch->m_coarseTune = (u_char)(ch->m_data >> 7);
        return;
    }
    if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_WARN)
        printf("snddrv : no supported RPN code '%X'\n", ch->m_rpn);
}

/* NRPN 0 selected: a loop starts after this event; its data entry sets the
 * repeat count (the data's low byte, 0 forever). */
// IOP 0x00006aa4 (RoR 0x00006854, word for word)
static void RxSndDrvBgmLoopStart(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch, int data)
{
    (void)bgm;

    if (data == 0)
    {
        RxSndDrvBgmLoop *l = s_bgm.m_loopFree;
        if (l == NULL)
            return;
        s_bgm.m_loopFree = l->m_next;
        l->m_count       = 0;
        l->m_times       = 0;
        l->m_pos         = track->m_pos;
        l->m_next        = track->m_loops;
        track->m_loops   = l;
    }
    else if (data == 1)
    {
        if (track->m_loops != NULL)
            track->m_loops->m_times = (u_char)ch->m_data;
    }
}

/* NRPN 1 selected: back to the innermost loop's start, or past it once it
 * has repeated m_times times. */
// IOP 0x00006b28 (RoR 0x000068d8, word for word)
static void RxSndDrvBgmLoopEnd(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track, RxSndDrvBgmChannel *ch, int data)
{
    (void)bgm;
    (void)ch;

    RxSndDrvBgmLoop *l = track->m_loops;
    if (data != 0 || l == NULL)
        return;
    if (l->m_times == 0 || l->m_count != l->m_times)
    {
        track->m_pos = l->m_pos;
        l->m_count++;
        return;
    }
    track->m_loops   = l->m_next;
    l->m_next        = s_bgm.m_loopFree;
    s_bgm.m_loopFree = l;
}

// IOP 0x00006ba8 (RoR 0x00006958, word for word)
static void RxSndDrvBgmSysex(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track)
{
    (void)bgm;
    u_int len = ReadVarLen(&track->m_pos);
    track->m_pos += len;
}

/* 0xFF type length data: 0x2f end of track, 0x51 tempo. */
// IOP 0x00006bec (RoR 0x0000699c, word for word)
static void RxSndDrvBgmMeta(RxSndDrvBgm *bgm, RxSndDrvBgmTrack *track)
{
    u_int type = *track->m_pos++;
    u_int len  = ReadVarLen(&track->m_pos);
    u_char *p  = track->m_pos;
    track->m_pos = p + len;

    if (type == 0x2f)
    {
        RxSndDrvBgmEndTrack(bgm, track);
    }
    else if (type == 0x51)
    {
        bgm->m_tempo = p[0] << 16 | p[1] << 8 | p[2];
        RxSndDrvBgmTiming(bgm);
    }
    else if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_WARN)
    {
        printf("snddrv : no supported meta event '%X : %d : %X, %X, %X...'\n", type, len, p[0], p[1], p[2]);
    }
}
