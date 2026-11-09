/* ==========================================================================
 *  se.cpp  (RXSNDDRV.IRX 0x6ab0..0x8f10: sound effects)
 *
 *  0x40 SE slots, each a little sequencer over NOTE slots (note.cpp).
 *  Command 0x80 {slot, xvab, group, index} plays the bank's SE request
 *  SEIF[group] -> SEGP[index] (xvab.cpp RxSndDrvXvabFindSe: its SESQ byte
 *  code, ticks a second, volume L / R and voice group); 0x81 {slot, xvab,
 *  program, key} plays one key of a program straight away.  0x82 {slot,
 *  param, value} sets a slot (RxSndDrvSeSet); 0x83 lists the slots in use
 *  with their flags (2 playing, 8 paused).  Like every subsystem the set
 *  commands go into the pending record of the command bank and the V-blank
 *  tick applies them: apply, reap, run the byte code, ramps, push to NOTE.
 *
 *  SESQ byte code: an opcode byte (bits 0..5 the code, bit 6 "every SE":
 *  the target is each playing SE that has a note whose NOTE id, or a
 *  channel whose id, is the operand -- not only this one), its operands,
 *  then a variable-length delta in ticks (MIDI style, 7 bits a byte):
 *    00 end                               01 / 0b  nothing (just the delta)
 *    02 n  loop start, n more times (0: forever)       03  loop end
 *    04 tag channel program key velocity  note on (tag and channel varlen)
 *    05 tag  key off the note with that tag     06 tag  free it at once
 *    07 / 09 channel kind value          set a channel parameter (09 adds)
 *    08 / 0a channel kind value time     ramp it over `time` ticks (0a adds,
 *                                        around 0x80)
 *  Channel parameters (kind): 0 volume 0x80, 1 pan 0x40, 2 bend 0x40,
 *  3 modulation depth 0, 4 modulation rate 0x80, 5 pitch offset 0x80 (in
 *  1/8 semitones); any other kind is the volume.
 *
 *  A slot stays in use until the EE stops it (parameter 0) and its last
 *  note is gone; flag 2 drops when the code has ended and no note is left,
 *  which is what the EE waits for (rx/rxsnd.c).
 *
 *  All pools are static: 0x100 notes, 0x100 ramps, 0x400 channels, 0x100
 *  loop records and the 0x40 slots, one block cleared at start-up (0x9e14
 *  at RoR IOP 0x22fd0).
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "intrman.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RXSNDDRV_SES          0x40
#define RXSNDDRV_SE_NOTES     0x100
#define RXSNDDRV_SE_RAMPS     0x100
#define RXSNDDRV_SE_CHANNELS  0x400
#define RXSNDDRV_SE_LOOPS     0x100

/* RxSndDrvSe::m_flags; the low byte is the status the EE reads (0x83). */
enum
{
    RXSNDDRV_SE_USED    = 0x00001,      /* the slot is taken */
    RXSNDDRV_SE_PLAYING = 0x00002,      /* code not ended or a note left */
    RXSNDDRV_SE_PAUSED  = 0x00008,      /* parameter 1 != 0: FS2's SeRun holds its code (RoR's read it nowhere) */
    RXSNDDRV_SE_STARTED = 0x10000,      /* its play request has been applied */
    RXSNDDRV_SE_STOP    = 0x20000,      /* parameter 0: reap once the notes are gone */
    RXSNDDRV_SE_LOCKED  = 0x40000,      /* RxSndDrvXvabFindSe counted it as a bank user */
};

/* RxSndDrvSePending::m_mask: what the next tick applies. */
enum
{
    RXSNDDRV_SE_SET_PLAY      = 0x001,
    RXSNDDRV_SE_SET_STOP      = 0x002,  /* parameter 0 */
    RXSNDDRV_SE_SET_PAUSE     = 0x004,  /* 1 */
    RXSNDDRV_SE_SET_SWITCH    = 0x008,  /* 2 */
    RXSNDDRV_SE_SET_VOLUME    = 0x010,  /* 3 */
    RXSNDDRV_SE_SET_SCALE     = 0x020,  /* 4 */
    RXSNDDRV_SE_SET_PARAM5    = 0x040,  /* 5 */
    RXSNDDRV_SE_SET_CHAN_VOL  = 0x080,  /* 6, and 0xb: the voice group */
    RXSNDDRV_SE_SET_CHAN_PAN  = 0x100,  /* 7, and 0xc: the owner */
    RXSNDDRV_SE_SET_CHAN_BEND = 0x200,  /* 8 */
    RXSNDDRV_SE_SET_CHAN_MOD  = 0x400,  /* 9 */
    RXSNDDRV_SE_SET_CHAN_RATE = 0x800,  /* 10 */
};

/* m_dirty of a slot, channel and note: which NOTE setters the update calls. */
enum
{
    RXSNDDRV_SE_DIRTY_SWITCH = 0x01,
    RXSNDDRV_SE_DIRTY_VOLUME = 0x02,
    RXSNDDRV_SE_DIRTY_PAN    = 0x04,
    RXSNDDRV_SE_DIRTY_PITCH  = 0x08,
    RXSNDDRV_SE_DIRTY_BEND   = 0x10,
    RXSNDDRV_SE_DIRTY_DEPTH  = 0x20,
    RXSNDDRV_SE_DIRTY_RATE   = 0x40,
    RXSNDDRV_SE_DIRTY_OFFSET = 0x80,
};

/* Channel parameters (the byte code's `kind`, the index into m_param). */
enum
{
    RXSNDDRV_SE_CHAN_VOLUME,
    RXSNDDRV_SE_CHAN_PAN,
    RXSNDDRV_SE_CHAN_BEND,
    RXSNDDRV_SE_CHAN_DEPTH,
    RXSNDDRV_SE_CHAN_RATE,
    RXSNDDRV_SE_CHAN_OFFSET,
};

/* SESQ opcodes (bits 0..5 of the opcode byte). */
enum
{
    RXSNDDRV_SESQ_END       = 0x00,
    RXSNDDRV_SESQ_NOP       = 0x01,
    RXSNDDRV_SESQ_LOOP      = 0x02,
    RXSNDDRV_SESQ_LOOP_END  = 0x03,
    RXSNDDRV_SESQ_NOTE_ON   = 0x04,
    RXSNDDRV_SESQ_KEY_OFF   = 0x05,
    RXSNDDRV_SESQ_FREE      = 0x06,
    RXSNDDRV_SESQ_SET       = 0x07,
    RXSNDDRV_SESQ_RAMP      = 0x08,
    RXSNDDRV_SESQ_SET_ADD   = 0x09,
    RXSNDDRV_SESQ_RAMP_ADD  = 0x0a,
    RXSNDDRV_SESQ_NOP2      = 0x0b,
};
#define RXSNDDRV_SESQ_ALL 0x40          /* opcode bit 6: every SE that holds the operand */

/* A ramp: one channel parameter moving to m_end (0x18 on the IOP).
 * layout: host -- pointers; the IOP offsets are in the comments. */
struct RxSndDrvSeRamp
{
    RxSndDrvSeRamp *m_next;             // IOP +0x00: the channel's list, or the free list
    u_char *m_param;                    // IOP +0x04: the channel's m_param byte it drives
    u_char  m_kind;                     // IOP +0x08: RXSNDDRV_SE_CHAN_*, as given
    u_char  m_dirty;                    // IOP +0x09: RXSNDDRV_SE_DIRTY_* of the parameter
    u_char  m_end;                      // IOP +0x0a: target value
    int     m_value;                    // IOP +0x0c: current value << 12
    int     m_step;                     // IOP +0x10: per V-blank, << 12
    int     m_time;                     // IOP +0x14: ticks left << 12; < 0 done
};

/* A channel: the parameters its notes share (0x14 on the IOP).
 * layout: host -- pointers; the IOP offsets are in the comments. */
struct RxSndDrvSeChannel
{
    RxSndDrvSeChannel *m_next;          // IOP +0x00: the SE's list, or the free list
    int     m_id;                       // IOP +0x04: the byte code's channel number
    u_char  m_pad;                      // IOP +0x08
    u_char  m_dirty;                    // IOP +0x09: RXSNDDRV_SE_DIRTY_*
    u_char  m_param[6];                 // IOP +0x0a: RXSNDDRV_SE_CHAN_*
    RxSndDrvSeRamp *m_ramps;            // IOP +0x10
};

/* A note the SE started (0x10 on the IOP).
 * layout: host -- pointers; the IOP offsets are in the comments. */
struct RxSndDrvSeNote
{
    RxSndDrvSeNote    *m_next;          // IOP +0x00: the SE's list (newest first), or the free list
    RxSndDrvSeChannel *m_channel;       // IOP +0x04
    int          m_tag;                 // IOP +0x08: the byte code's note number
    signed char  m_note;                // IOP +0x0c: NOTE id, -1 freed
    u_char       m_dirty;               // IOP +0x0d: RXSNDDRV_SE_DIRTY_*, 0xff when new
    u_char       m_key;                 // IOP +0x0e: read nowhere
};

/* An open loop (0xc on the IOP).
 * layout: host -- pointers; the IOP offsets are in the comments. */
struct RxSndDrvSeLoop
{
    RxSndDrvSeLoop *m_next;             // IOP +0x00: the SE's stack, or the free list
    const u_char   *m_pos;              // IOP +0x04: the code after the loop start
    u_char          m_count;            // IOP +0x08: repeats left
    u_char          m_forever;          // IOP +0x09: the count was 0
};

/* The parameters of RxSndDrvSeSet waiting for the tick (0x18, IOP layout). */
struct RxSndDrvSePending
{
    u_int   m_mask;                     // IOP +0x00: RXSNDDRV_SE_SET_*
    u_char  m_stop;                     // IOP +0x04: 1 release, else cut
    u_char  m_pause;                    // IOP +0x05: 0 resume, 1 release, else cut
    u_char  m_switch;                   // IOP +0x06
    u_char  m_pad;                      // IOP +0x07
    u_int   m_volume;                   // IOP +0x08: L | R << 16, 0x80 = 1
    short   m_scale;                    // IOP +0x0c: pitch scale, 0x1000 = 1
    short   m_param5;                   // IOP +0x0e
    u_char  m_chan[5];                  // IOP +0x10: first channel's volume, pan, bend, depth, rate
    signed char m_group;                // IOP +0x15: voice group for new notes
    short   m_owner;                    // IOP +0x16: voice owner for new notes
};
static_assert(sizeof(RxSndDrvSePending) == 0x18, "RxSndDrvSePending");
static_assert(offsetof(RxSndDrvSePending, m_stop) == 0x4, "RxSndDrvSePending::m_stop");
static_assert(offsetof(RxSndDrvSePending, m_volume) == 0x8, "RxSndDrvSePending::m_volume");
static_assert(offsetof(RxSndDrvSePending, m_scale) == 0xc, "RxSndDrvSePending::m_scale");
static_assert(offsetof(RxSndDrvSePending, m_param5) == 0xe, "RxSndDrvSePending::m_param5");
static_assert(offsetof(RxSndDrvSePending, m_chan) == 0x10, "RxSndDrvSePending::m_chan");
static_assert(offsetof(RxSndDrvSePending, m_group) == 0x15, "RxSndDrvSePending::m_group");
static_assert(offsetof(RxSndDrvSePending, m_owner) == 0x16, "RxSndDrvSePending::m_owner");

/* An SE slot (0x68 on the IOP).  layout: host -- pointers; the IOP offsets
 * are in the comments. */
struct RxSndDrvSe
{
    RxSndDrvSe *m_next;                 // IOP +0x00: the active list (oldest first)
    u_char  m_xvab;                     // IOP +0x04
    u_char  m_program;                  // IOP +0x05: SE group (0x80) or program (0x81)
    u_char  m_key;                      // IOP +0x06: SE index (0x80) or key (0x81)
    u_char  m_end;                      // IOP +0x07: 1 the code has ended (0x81: from the start)
    u_int   m_flags;                    // IOP +0x08: RXSNDDRV_SE_*
    u_char  m_id;                       // IOP +0x0c: its index
    u_char  m_sequence;                 // IOP +0x0d: 1 an SE request (0x80), 0 one key (0x81)
    u_char  m_switch;                   // IOP +0x0e
    u_char  m_dirty;                    // IOP +0x0f: RXSNDDRV_SE_DIRTY_*
    int     m_tempo;                    // IOP +0x10: ticks a second, 0x3c
    short   m_vol[2];                   // IOP +0x14: the request's L, R 0x80
    short   m_master[2];                // IOP +0x18: parameter 3, L, R 0x80
    short   m_scale;                    // IOP +0x1c: parameter 4, 0x1000
    short   m_param5;                   // IOP +0x1e: parameter 5, 0x1000, read nowhere
    signed char m_group;                // IOP +0x20: voice group of new notes, -1 the region's
    short   m_owner;                    // IOP +0x22: voice owner of new notes, -1 the region's
    const u_char *m_code;               // IOP +0x24: SESQ position
    int     m_wait;                     // IOP +0x28: ticks to the next event << 12; < 0 run
    RxSndDrvSeNote    *m_notes;         // IOP +0x2c
    RxSndDrvSeChannel *m_channels;      // IOP +0x30
    RxSndDrvSeLoop    *m_loops;         // IOP +0x34
    RxSndDrvSePending  m_pending[2];    // IOP +0x38: per command bank
};

/* The SE block (0x9e14 at RoR IOP 0x22fd0, cleared as one).
 * layout: host (holds the host records); the IOP addresses are in the comments. */
struct RxSndDrvSeState
{
    RxSndDrvSeNote     m_note[RXSNDDRV_SE_NOTES];      // RoR IOP 0x22fd0
    RxSndDrvSeNote    *m_noteFree;                     // RoR IOP 0x23fd0
    RxSndDrvSeRamp     m_ramp[RXSNDDRV_SE_RAMPS];      // RoR IOP 0x23fd4
    RxSndDrvSeRamp    *m_rampFree;                     // RoR IOP 0x257d4
    RxSndDrvSeChannel  m_channel[RXSNDDRV_SE_CHANNELS]; // RoR IOP 0x257d8
    RxSndDrvSeChannel *m_channelFree;                  // RoR IOP 0x2a7d8
    RxSndDrvSeLoop     m_loop[RXSNDDRV_SE_LOOPS];      // RoR IOP 0x2a7dc
    RxSndDrvSeLoop    *m_loopFree;                     // RoR IOP 0x2b3dc
    RxSndDrvSe         m_se[RXSNDDRV_SES];             // RoR IOP 0x2b3e0
    RxSndDrvSe        *m_active;                       // RoR IOP 0x2cde0
};

static RxSndDrvSeState s_se;            // RoR IOP 0x00022fd0

static int  RxSndDrvSeChannelRamp(RxSndDrvSe *se, int channel, u_int kind, int value, int time,
                                  int add);
static int  RxSndDrvSeNoteOn(RxSndDrvSe *se, int tag, int channel, int program, int key,
                             int velocity);
static void RxSndDrvSeNoteOff(RxSndDrvSe *se, int tag, int release);
static int  RxSndDrvSeChannelSet(RxSndDrvSe *se, int channel, u_int kind, int value, int add);
static RxSndDrvSeChannel *RxSndDrvSeGetChannel(RxSndDrvSe *se, int channel);
static void RxSndDrvSeFreeChannels(RxSndDrvSe *se);
static void RxSndDrvSeFreeLoops(RxSndDrvSe *se);
static void RxSndDrvSeFreeRamps(RxSndDrvSeChannel *channel);
static void RxSndDrvSeNotesOff(RxSndDrvSe *se, int release);
static void RxSndDrvSeReap(void);

/* A MIDI-style variable-length number (7 bits a byte, 0x80 continues). */
static u_int RxSndDrvSeReadVar(const u_char **code)
{
    const u_char *p     = *code;
    u_int         value = 0;
    u_char        c;

    do
    {
        c     = *p++;
        value = value << 7 | (c & 0x7f);
    } while (c & 0x80);
    *code = p;
    return value;
}

/* The m_param byte of `kind` (any unknown kind is the volume) and its dirty bit. */
static u_char *RxSndDrvSeChannelParam(RxSndDrvSeChannel *channel, u_int kind, u_char *dirty)
{
    switch (kind)
    {
    default:
        *dirty = RXSNDDRV_SE_DIRTY_VOLUME;
        return &channel->m_param[RXSNDDRV_SE_CHAN_VOLUME];
    case RXSNDDRV_SE_CHAN_PAN:
        *dirty = RXSNDDRV_SE_DIRTY_PAN;
        return &channel->m_param[RXSNDDRV_SE_CHAN_PAN];
    case RXSNDDRV_SE_CHAN_BEND:
        *dirty = RXSNDDRV_SE_DIRTY_BEND;
        return &channel->m_param[RXSNDDRV_SE_CHAN_BEND];
    case RXSNDDRV_SE_CHAN_DEPTH:
        *dirty = RXSNDDRV_SE_DIRTY_DEPTH;
        return &channel->m_param[RXSNDDRV_SE_CHAN_DEPTH];
    case RXSNDDRV_SE_CHAN_RATE:
        *dirty = RXSNDDRV_SE_DIRTY_RATE;
        return &channel->m_param[RXSNDDRV_SE_CHAN_RATE];
    case RXSNDDRV_SE_CHAN_OFFSET:
        *dirty = RXSNDDRV_SE_DIRTY_OFFSET;
        return &channel->m_param[RXSNDDRV_SE_CHAN_OFFSET];
    }
}

/* The SE an opcode with RXSNDDRV_SESQ_ALL acts on next: the first playing SE
 * from `se` on, of bank `xvab` (FS2: RoR's took any bank), that has a note
 * whose NOTE id is `id` (the original compares the NOTE id, not the tag
 * RxSndDrvSeNoteOff looks for) or a channel `id`. */
static RxSndDrvSe *RxSndDrvSeFindHolder(RxSndDrvSe *se, u_int id, u_char xvab)
{
    for (; se != NULL; se = se->m_next)
    {
        if (se->m_xvab != xvab)
            continue;
        for (RxSndDrvSeNote *note = se->m_notes; note != NULL; note = note->m_next)
            if ((int)note->m_note == (int)id)
                return se;
        for (RxSndDrvSeChannel *channel = se->m_channels; channel != NULL; channel = channel->m_next)
            if ((u_int)channel->m_id == id)
                return se;
    }
    return NULL;
}

// IOP 0x00006d00 (RoR 0x00006ab0, word for word)
void RxSndDrvSeInit(void)
{
    memset(&s_se, 0, sizeof(s_se));
    for (int i = 0; i < RXSNDDRV_SE_NOTES; i++)
    {
        s_se.m_note[i].m_next = s_se.m_noteFree;
        s_se.m_noteFree       = &s_se.m_note[i];
    }
    for (int i = 0; i < RXSNDDRV_SE_RAMPS; i++)
    {
        s_se.m_ramp[i].m_next = s_se.m_rampFree;
        s_se.m_rampFree       = &s_se.m_ramp[i];
    }
    for (int i = 0; i < RXSNDDRV_SE_CHANNELS; i++)
    {
        s_se.m_channel[i].m_next = s_se.m_channelFree;
        s_se.m_channelFree       = &s_se.m_channel[i];
    }
    for (int i = 0; i < RXSNDDRV_SE_LOOPS; i++)
    {
        s_se.m_loop[i].m_next = s_se.m_loopFree;
        s_se.m_loopFree       = &s_se.m_loop[i];
    }
    for (int i = 0; i < RXSNDDRV_SES; i++)
        s_se.m_se[i].m_id = (u_char)i;
    if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO)
        printf("snddrv : completed to initialize SE system\n");
}

/* Command 0x82 {slot, param, value}: 0 stop (1 release, else cut), 1 pause
 * (0 resume, 1 release, else cut), 2 NOTE switch, 3 volume L | R << 16,
 * 4 pitch scale, 5 (kept, read nowhere), 6..10 the first channel's volume,
 * pan, bend, modulation depth and rate, 0xb / 0xc the voice group and owner
 * of the notes started next.  0xb and 0xc share the mask bits of 6 and 7,
 * so either also applies the channel's volume or pan (the original's). */
// IOP 0x00006e1c (RoR 0x00006bcc, word for word)
void RxSndDrvSeSet(u_int slot, const u_int *data)
{
    int intr;

    CpuSuspendIntr(&intr);
    if (s_se.m_se[slot].m_flags & RXSNDDRV_SE_USED)
    {
        RxSndDrvSePending *pending = &s_se.m_se[slot].m_pending[RxSndDrvCmdBank()];

        switch (data[0])
        {
        case 0:
            pending->m_mask |= RXSNDDRV_SE_SET_STOP;
            pending->m_stop = (u_char)data[1];
            break;
        case 1:
            pending->m_mask |= RXSNDDRV_SE_SET_PAUSE;
            pending->m_pause = (u_char)data[1];
            break;
        case 2:
            pending->m_mask |= RXSNDDRV_SE_SET_SWITCH;
            pending->m_switch = (u_char)data[1];
            break;
        case 3:
            pending->m_mask |= RXSNDDRV_SE_SET_VOLUME;
            pending->m_volume = data[1];
            break;
        case 4:
            pending->m_mask |= RXSNDDRV_SE_SET_SCALE;
            pending->m_scale = (short)data[1];
            break;
        case 5:
            pending->m_mask |= RXSNDDRV_SE_SET_PARAM5;
            pending->m_param5 = (short)data[1];
            break;
        case 6:
            pending->m_mask |= RXSNDDRV_SE_SET_CHAN_VOL;
            pending->m_chan[RXSNDDRV_SE_CHAN_VOLUME] = (u_char)data[1];
            break;
        case 7:
            pending->m_mask |= RXSNDDRV_SE_SET_CHAN_PAN;
            pending->m_chan[RXSNDDRV_SE_CHAN_PAN] = (u_char)data[1];
            break;
        case 8:
            pending->m_mask |= RXSNDDRV_SE_SET_CHAN_BEND;
            pending->m_chan[RXSNDDRV_SE_CHAN_BEND] = (u_char)data[1];
            break;
        case 9:
            pending->m_mask |= RXSNDDRV_SE_SET_CHAN_MOD;
            pending->m_chan[RXSNDDRV_SE_CHAN_DEPTH] = (u_char)data[1];
            break;
        case 10:
            pending->m_mask |= RXSNDDRV_SE_SET_CHAN_RATE;
            pending->m_chan[RXSNDDRV_SE_CHAN_RATE] = (u_char)data[1];
            break;
        case 0xb:
            pending->m_mask |= RXSNDDRV_SE_SET_CHAN_VOL;
            pending->m_group = (signed char)data[1];
            break;
        case 0xc:
            pending->m_mask |= RXSNDDRV_SE_SET_CHAN_PAN;
            pending->m_owner = (short)data[1];
            break;
        }
    }
    CpuResumeIntr(intr);
}

/* Applies the play requests and the pending parameters of the tick bank. */
// IOP 0x00007054 (RoR 0x00006e04, word for word)
static void RxSndDrvSeApply(void)
{
    for (RxSndDrvSe *se = s_se.m_active; se != NULL; se = se->m_next)
        if (se->m_pending[RxSndDrvTickBank()].m_mask & RXSNDDRV_SE_SET_PLAY)
            se->m_flags |= RXSNDDRV_SE_STARTED;

    for (RxSndDrvSe *se = s_se.m_active; se != NULL; se = se->m_next)
    {
        RxSndDrvSePending *pending = &se->m_pending[RxSndDrvTickBank()];

        if (!(se->m_flags & RXSNDDRV_SE_STARTED))
            continue;

        if (pending->m_mask & RXSNDDRV_SE_SET_PLAY)
        {
            if (se->m_sequence == 0)
            {
                RxSndDrvSeNoteOn(se, 0, 0, se->m_program, se->m_key, 0x80);
            }
            else if (se->m_sequence == 1)
            {
                int   ticks, volL, volR, group;
                u_int code = RxSndDrvXvabFindSe(se->m_xvab, se->m_program, se->m_key, &ticks, &volL,
                                                &volR, &group);
                if (code == 0)
                {
                    if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_WARN)
                        printf("snddrv : unregisted SE request '%d, %d, %d'\n", se->m_xvab,
                               se->m_program, se->m_key);
                }
                else
                {
                    se->m_code   = (const u_char *)(uintptr_t)code;
                    se->m_flags |= RXSNDDRV_SE_LOCKED;
                    se->m_tempo  = ticks < 1 ? 0x3c : ticks;
                    se->m_vol[0] = (short)volL;
                    se->m_vol[1] = (short)volR;
                    se->m_group  = (signed char)group;
                }
            }
        }
        if (pending->m_mask & RXSNDDRV_SE_SET_STOP)
        {
            se->m_flags |= RXSNDDRV_SE_STOP;
            RxSndDrvSeNotesOff(se, pending->m_stop == 1);
        }
        if (pending->m_mask & RXSNDDRV_SE_SET_PAUSE)
        {
            if (pending->m_pause == 1)
            {
                se->m_flags |= RXSNDDRV_SE_PAUSED;
                RxSndDrvSeNotesOff(se, 1);
            }
            else if (pending->m_pause == 0)
            {
                se->m_flags &= ~RXSNDDRV_SE_PAUSED;
            }
            else
            {
                se->m_flags |= RXSNDDRV_SE_PAUSED;
                RxSndDrvSeNotesOff(se, 0);
            }
        }
        if (pending->m_mask & RXSNDDRV_SE_SET_SWITCH)
        {
            se->m_switch = pending->m_switch;
            se->m_dirty |= RXSNDDRV_SE_DIRTY_SWITCH;
        }
        if (pending->m_mask & RXSNDDRV_SE_SET_VOLUME)
        {
            se->m_master[0] = (short)(pending->m_volume & 0xffff);
            se->m_master[1] = (short)(pending->m_volume >> 16);
            se->m_dirty    |= RXSNDDRV_SE_DIRTY_VOLUME;
        }
        if (pending->m_mask & RXSNDDRV_SE_SET_SCALE)
        {
            se->m_scale  = pending->m_scale;
            se->m_dirty |= RXSNDDRV_SE_DIRTY_PITCH;
        }
        if (pending->m_mask & RXSNDDRV_SE_SET_PARAM5)
            se->m_param5 = pending->m_param5;

        /* Parameters 6..10 go to the first channel (the newest) of a one-key SE. */
        if (se->m_sequence == 0 && (pending->m_mask & 0xf80) != 0)
        {
            RxSndDrvSeChannel *channel = se->m_channels;

            if (channel != NULL)
            {
                if (pending->m_mask & RXSNDDRV_SE_SET_CHAN_VOL)
                {
                    channel->m_param[RXSNDDRV_SE_CHAN_VOLUME] = pending->m_chan[RXSNDDRV_SE_CHAN_VOLUME];
                    channel->m_dirty |= RXSNDDRV_SE_DIRTY_VOLUME;
                }
                if (pending->m_mask & RXSNDDRV_SE_SET_CHAN_PAN)
                {
                    channel->m_param[RXSNDDRV_SE_CHAN_PAN] = pending->m_chan[RXSNDDRV_SE_CHAN_PAN];
                    channel->m_dirty |= RXSNDDRV_SE_DIRTY_PAN;
                }
                if (pending->m_mask & RXSNDDRV_SE_SET_CHAN_BEND)
                {
                    channel->m_param[RXSNDDRV_SE_CHAN_BEND] = pending->m_chan[RXSNDDRV_SE_CHAN_BEND];
                    channel->m_dirty |= RXSNDDRV_SE_DIRTY_BEND;
                }
                if (pending->m_mask & RXSNDDRV_SE_SET_CHAN_MOD)
                {
                    channel->m_param[RXSNDDRV_SE_CHAN_DEPTH] = pending->m_chan[RXSNDDRV_SE_CHAN_DEPTH];
                    channel->m_dirty |= RXSNDDRV_SE_DIRTY_DEPTH;
                }
                if (pending->m_mask & RXSNDDRV_SE_SET_CHAN_RATE)
                {
                    channel->m_param[RXSNDDRV_SE_CHAN_RATE] = pending->m_chan[RXSNDDRV_SE_CHAN_RATE];
                    channel->m_dirty |= RXSNDDRV_SE_DIRTY_RATE;
                }
            }
        }
        if (pending->m_mask & RXSNDDRV_SE_SET_CHAN_VOL)
            se->m_group = pending->m_group;
        if (pending->m_mask & RXSNDDRV_SE_SET_CHAN_PAN)
            se->m_owner = pending->m_owner;
        pending->m_mask = 0;
    }
}

/* Runs every playing SE's byte code up to its next wait (the header lists
 * the opcodes), then takes one V-blank of ticks off the wait. */
// IOP 0x000074b0 (RoR 0x00007260, 74% of it aligned: FS2 read 2026-11-09)
static void RxSndDrvSeRun(void)
{
    for (RxSndDrvSe *se = s_se.m_active; se != NULL; se = se->m_next)
    {
        int rate  = RxSndDrvGetFrameRate();
        int tempo = se->m_tempo;

        /* FS2: a paused SE's code holds, its wait too. */
        if (!(se->m_flags & RXSNDDRV_SE_STARTED) || se->m_end == 1 || (se->m_flags & RXSNDDRV_SE_PAUSED))
            continue;

        int wait = se->m_wait;
        if (wait < 0)
        {
            do
            {
                u_char op  = *se->m_code++;
                u_char all = op >> 6 & 1;
                u_int  id;

                op &= 0x3f;
                switch (op)
                {
                case RXSNDDRV_SESQ_END:
                    se->m_end = 1;
                    goto done;

                case RXSNDDRV_SESQ_NOP:
                case RXSNDDRV_SESQ_NOP2:
                    break;

                case RXSNDDRV_SESQ_LOOP:
                {
                    u_char          count = *se->m_code++;
                    RxSndDrvSeLoop *loop  = s_se.m_loopFree;

                    if (loop != NULL)
                    {
                        s_se.m_loopFree = loop->m_next;
                        loop->m_next    = se->m_loops;
                        se->m_loops     = loop;
                        loop->m_count   = count;
                        loop->m_forever = count == 0;
                        loop->m_pos     = se->m_code;
                    }
                    break;
                }

                case RXSNDDRV_SESQ_LOOP_END:
                {
                    RxSndDrvSeLoop *loop = se->m_loops;

                    if (loop != NULL)
                    {
                        if (loop->m_count == 0 && !loop->m_forever)
                        {
                            se->m_loops     = loop->m_next;
                            loop->m_next    = s_se.m_loopFree;
                            s_se.m_loopFree = loop;
                        }
                        else
                        {
                            se->m_code = loop->m_pos;
                            loop->m_count--;
                        }
                    }
                    break;
                }

                case RXSNDDRV_SESQ_NOTE_ON:
                {
                    u_int  tag      = RxSndDrvSeReadVar(&se->m_code);
                    u_int  channel  = RxSndDrvSeReadVar(&se->m_code);
                    u_char program  = se->m_code[0];
                    u_char key      = se->m_code[1];
                    u_char velocity = se->m_code[2];

                    se->m_code += 3;
                    RxSndDrvSeNoteOn(se, (int)tag, (int)channel, program, key, velocity);
                    break;
                }

                case RXSNDDRV_SESQ_KEY_OFF:
                case RXSNDDRV_SESQ_FREE:
                {
                    id = RxSndDrvSeReadVar(&se->m_code);
                    for (RxSndDrvSe *target = all ? RxSndDrvSeFindHolder(s_se.m_active, id, se->m_xvab) : se;
                         target != NULL;
                         target = all ? RxSndDrvSeFindHolder(target->m_next, id, se->m_xvab) : NULL)
                        RxSndDrvSeNoteOff(target, (int)id, op == RXSNDDRV_SESQ_KEY_OFF);
                    break;
                }

                case RXSNDDRV_SESQ_SET:
                case RXSNDDRV_SESQ_SET_ADD:
                {
                    id = RxSndDrvSeReadVar(&se->m_code);
                    u_char kind  = se->m_code[0];
                    u_char value = se->m_code[1];

                    se->m_code += 2;
                    for (RxSndDrvSe *target = all ? RxSndDrvSeFindHolder(s_se.m_active, id, se->m_xvab) : se;
                         target != NULL;
                         target = all ? RxSndDrvSeFindHolder(target->m_next, id, se->m_xvab) : NULL)
                        RxSndDrvSeChannelSet(target, (int)id, kind, value, op == RXSNDDRV_SESQ_SET_ADD);
                    break;
                }

                case RXSNDDRV_SESQ_RAMP:
                case RXSNDDRV_SESQ_RAMP_ADD:
                {
                    id = RxSndDrvSeReadVar(&se->m_code);
                    u_char kind  = se->m_code[0];
                    u_char value = se->m_code[1];

                    se->m_code += 2;
                    u_int time = RxSndDrvSeReadVar(&se->m_code);
                    for (RxSndDrvSe *target = all ? RxSndDrvSeFindHolder(s_se.m_active, id, se->m_xvab) : se;
                         target != NULL;
                         target = all ? RxSndDrvSeFindHolder(target->m_next, id, se->m_xvab) : NULL)
                        RxSndDrvSeChannelRamp(target, (int)id, kind, value, (int)time,
                                              op == RXSNDDRV_SESQ_RAMP_ADD);
                    break;
                }

                default:
                    if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
                    {
                        const u_char *p = se->m_code;
                        printf("snddrv : unknow SE sequence code '%02X %02X %02X %02X'\n", p[-1], p[0],
                               p[1], p[2]);
                    }
                    goto done;
                }
                wait = se->m_wait + (int)(RxSndDrvSeReadVar(&se->m_code) * 0x1000);
                se->m_wait = wait;
            } while (wait < 0);
        done:
            wait = se->m_wait;
        }
        se->m_wait = wait - (tempo << 12) / rate;
    }
}

/* Byte code 08 / 0a: moves channel `channel`'s parameter `kind` to `value`
 * (add: m_param - 0x80 + value) over `time` ticks; time 0 sets it at once
 * (the same code as RxSndDrvSeChannelSet, inlined in the original). */
// IOP 0x00007f8c (RoR 0x00007cb4, word for word)
static int RxSndDrvSeChannelRamp(RxSndDrvSe *se, int channel, u_int kind, int value, int time,
                                 int add)
{
    if (time == 0)
        return RxSndDrvSeChannelSet(se, channel, kind, value, add);

    RxSndDrvSeChannel *chan = RxSndDrvSeGetChannel(se, channel);
    if (chan == NULL)
        return 0;

    RxSndDrvSeRamp *ramp;
    for (ramp = chan->m_ramps; ramp != NULL; ramp = ramp->m_next)
        if (ramp->m_kind == kind)
            break;
    if (ramp == NULL)
    {
        ramp = s_se.m_rampFree;
        if (ramp == NULL)
            return 0;
        s_se.m_rampFree = ramp->m_next;
        ramp->m_next    = chan->m_ramps;
        chan->m_ramps   = ramp;
    }

    u_char  dirty;
    u_char *param = RxSndDrvSeChannelParam(chan, kind, &dirty);
    u_int   cur   = *param;

    if (add)
        value = (int)(cur - 0x80) + value;
    if (value > 0xff)
        value = 0xff;
    if (value < 0)
        value = 0;

    ramp->m_kind  = (u_char)kind;
    ramp->m_dirty = dirty;
    ramp->m_param = param;
    ramp->m_value = (int)(cur << 12);

    int rate = RxSndDrvGetFrameRate();
    int step = (value - (int)cur) * 0x1000 / time * se->m_tempo;

    ramp->m_end  = (u_char)value;
    ramp->m_time = time << 12;
    ramp->m_step = step / rate;
    return 1;
}

/* Moves every ramp one V-blank on, writing its channel parameter. */
// IOP 0x000082f8 (RoR 0x00008020, word for word)
static void RxSndDrvSeRampTick(void)
{
    for (RxSndDrvSe *se = s_se.m_active; se != NULL; se = se->m_next)
    {
        int rate  = RxSndDrvGetFrameRate();
        int tempo = se->m_tempo;

        if (!(se->m_flags & RXSNDDRV_SE_STARTED))
            continue;
        for (RxSndDrvSeChannel *chan = se->m_channels; chan != NULL; chan = chan->m_next)
        {
            RxSndDrvSeRamp **link = &chan->m_ramps;
            RxSndDrvSeRamp  *ramp;

            while ((ramp = *link) != NULL)
            {
                int time = ramp->m_time - (tempo << 12) / rate;

                if (ramp->m_time < 0)
                {
                    *link           = ramp->m_next;
                    ramp->m_next    = s_se.m_rampFree;
                    s_se.m_rampFree = ramp;
                    continue;
                }
                ramp->m_time = time;
                if (time < 0)
                    ramp->m_value = ramp->m_end << 12;
                else
                    ramp->m_value += ramp->m_step;

                int    v    = ramp->m_value >> 12;
                u_char byte = (u_char)v;
                if (v > 0xff)
                {
                    byte = 0xff;
                    v    = 0xff;
                }
                if (v < 0)
                    byte = 0;
                *ramp->m_param = byte;
                chan->m_dirty |= ramp->m_dirty;
                link = &ramp->m_next;
            }
        }
    }
}

/* Drops the notes whose NOTE has fallen silent (and frees it) or was freed,
 * and pushes what changed in the SE, the channel or the note to the NOTE
 * layer; clears RXSNDDRV_SE_PLAYING once the code has ended and no note is
 * left. */
// IOP 0x0000848c (RoR 0x000081b4, word for word)
static void RxSndDrvSeUpdate(void)
{
    for (RxSndDrvSe *se = s_se.m_active; se != NULL; se = se->m_next)
    {
        if (!(se->m_flags & RXSNDDRV_SE_STARTED))
            continue;

        RxSndDrvSeNote **link = &se->m_notes;
        RxSndDrvSeNote  *note;

        while ((note = *link) != NULL)
        {
            if (note->m_note < 0 || !(RxSndDrvNoteGetFlags(note->m_note) & 2))
            {
                if (note->m_note >= 0)
                    RxSndDrvNoteFree(note->m_note);
                *link           = note->m_next;
                note->m_next    = s_se.m_noteFree;
                s_se.m_noteFree = note;
                /* The original still pushes the dropped note's changes
                 * below, with its stale id. */
            }
            else
            {
                link = &note->m_next;
            }

            RxSndDrvSeChannel *chan  = note->m_channel;
            int                id    = note->m_note;
            u_char             dirty = se->m_dirty | chan->m_dirty | note->m_dirty;

            /* PC: a freed record (id -1) would write the setters' fields into
             * the bytes before NOTE 0 (the IOP's last NOTE layer); skipped. */
            if (id >= 0)
            {
                if (dirty & RXSNDDRV_SE_DIRTY_SWITCH)
                    RxSndDrvNoteSetSwitch(id, se->m_switch != 0);
                if (dirty & RXSNDDRV_SE_DIRTY_VOLUME)
                    RxSndDrvNoteSetVolume(id,
                        (short)((se->m_vol[0] * se->m_master[0] >> 7) * chan->m_param[RXSNDDRV_SE_CHAN_VOLUME] >> 7),
                        (short)((se->m_vol[1] * se->m_master[1] >> 7) * chan->m_param[RXSNDDRV_SE_CHAN_VOLUME] >> 7));
                if (dirty & RXSNDDRV_SE_DIRTY_PAN)
                    RxSndDrvNoteSetPan(id, chan->m_param[RXSNDDRV_SE_CHAN_PAN]);
                if (dirty & RXSNDDRV_SE_DIRTY_PITCH)
                    RxSndDrvNoteSetPitch(id, se->m_scale);
                if (dirty & RXSNDDRV_SE_DIRTY_BEND)
                    RxSndDrvNoteSetBend(id, (chan->m_param[RXSNDDRV_SE_CHAN_BEND] - 0x40) * 0x40);
                if (dirty & RXSNDDRV_SE_DIRTY_OFFSET)
                {
                    int offset = chan->m_param[RXSNDDRV_SE_CHAN_OFFSET] << 4;
                    RxSndDrvNoteSetOffset(id, (offset >> 7) - 0x10, offset & 0x70);
                }
                if (dirty & RXSNDDRV_SE_DIRTY_DEPTH)
                    RxSndDrvNoteSetModDepth(id, chan->m_param[RXSNDDRV_SE_CHAN_DEPTH]);
                if (dirty & RXSNDDRV_SE_DIRTY_RATE)
                    RxSndDrvNoteSetModRate(id, chan->m_param[RXSNDDRV_SE_CHAN_RATE]);
            }
            note->m_dirty = 0;
        }
        for (RxSndDrvSeChannel *chan = se->m_channels; chan != NULL; chan = chan->m_next)
            chan->m_dirty = 0;
        se->m_dirty = 0;
        if (se->m_end == 1 && se->m_notes == NULL)
            se->m_flags &= ~RXSNDDRV_SE_PLAYING;
    }
}

/* Stops every SE at once (parameter 0, value 2); nothing calls it. */
// IOP 0x00008730 (RoR 0x00008458, word for word)
void RxSndDrvSeStopAll(void)
{
    for (RxSndDrvSe *se = s_se.m_active; se != NULL; se = se->m_next)
    {
        u_int data[2] = {0, 2};
        RxSndDrvSeSet(se->m_id, data);
    }
}

/* Takes free slot `slot` for a play request: SE request (group, index) of
 * the bank, or with `sequence` 0 one key of a program; 1 when taken.  The
 * original has it twice (0x84b0 / 0x8658), differing only in the mode. */
static int RxSndDrvSeStart(int slot, u_char xvab, u_char program, u_char key, int sequence)
{
    int         intr;
    int         ok = 0;
    RxSndDrvSe *se = &s_se.m_se[slot];

    CpuSuspendIntr(&intr);
    if (!(se->m_flags & RXSNDDRV_SE_USED))
    {
        se->m_flags     = RXSNDDRV_SE_USED | RXSNDDRV_SE_PLAYING;
        se->m_sequence  = sequence ? 1 : 0;
        se->m_end       = sequence ? 0 : 1;
        se->m_tempo     = 0x3c;
        se->m_vol[0]    = 0x80;
        se->m_vol[1]    = 0x80;
        se->m_master[0] = 0x80;
        se->m_master[1] = 0x80;
        se->m_scale     = 0x1000;
        se->m_param5    = 0x1000;
        se->m_group     = -1;
        se->m_xvab      = xvab;
        se->m_program   = program;
        se->m_key       = key;
        se->m_switch    = 0;
        se->m_owner     = -1;
        se->m_code      = NULL;
        se->m_wait      = 0;
        se->m_notes     = NULL;
        se->m_channels  = NULL;
        se->m_loops     = NULL;
        se->m_pending[RxSndDrvCmdBank()].m_mask = RXSNDDRV_SE_SET_PLAY;
        se->m_pending[RxSndDrvCmdBank() == 0].m_mask = 0;

        RxSndDrvSe **link = &s_se.m_active;
        while (*link != NULL)
            link = &(*link)->m_next;
        se->m_next = NULL;
        *link      = se;
        ok         = 1;
    }
    CpuResumeIntr(intr);
    if (ok == 0 && (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR))
        printf("snddrv : failed to play SE slot '%d'\n", slot);
    return ok;
}

/* Command 0x80: plays SE request (group, index) of bank `xvab` on `slot`. */
// IOP 0x00008788 (RoR 0x000084b0, word for word)
int RxSndDrvSePlay(int slot, int xvab, int group, int index)
{
    return RxSndDrvSeStart(slot, (u_char)xvab, (u_char)group, (u_char)index, 1);
}

/* Command 0x81: plays key `key` of program `program` of bank `xvab` on
 * `slot` (a copy of 0x84b0 in the original but for the mode). */
// IOP 0x00008930 (RoR 0x00008658, word for word)
int RxSndDrvSePlay2(int slot, int xvab, int program, int key)
{
    return RxSndDrvSeStart(slot, (u_char)xvab, (u_char)program, (u_char)key, 0);
}

/* Command 0x83: {slot, status} per slot in use, 4 bytes each; the count. */
// IOP 0x00008ad8 (RoR 0x00008800, word for word)
int RxSndDrvSeStatus(u_char *reply)
{
    int intr;
    int count = 0;

    CpuSuspendIntr(&intr);
    for (RxSndDrvSe *se = s_se.m_active; se != NULL; se = se->m_next)
    {
        reply[0] = se->m_id;
        count++;
        reply[1] = (u_char)se->m_flags;
        reply += 4;
    }
    CpuResumeIntr(intr);
    return count;
}

// IOP 0x00008b50 (RoR 0x00008878, word for word)
void RxSndDrvSeTick(void)
{
    RxSndDrvSeApply();
    RxSndDrvSeReap();
    RxSndDrvSeRun();
    RxSndDrvSeRampTick();
    RxSndDrvSeUpdate();
}

/* Keys off (release) or frees every note of `se`; freeing also drops the
 * records. */
// IOP 0x00008b90 (RoR 0x000088b8, word for word)
static void RxSndDrvSeNotesOff(RxSndDrvSe *se, int release)
{
    if (release)
    {
        for (RxSndDrvSeNote *note = se->m_notes; note != NULL; note = note->m_next)
            if (note->m_note >= 0)
                RxSndDrvNoteKeyOff(note->m_note);
        return;
    }
    while (se->m_notes != NULL)
    {
        RxSndDrvSeNote *note = se->m_notes;

        if (note->m_note >= 0)
            RxSndDrvNoteFree(note->m_note);
        se->m_notes     = note->m_next;
        note->m_next    = s_se.m_noteFree;
        s_se.m_noteFree = note;
    }
}

/* Gives back the slots that were stopped and have no note left: the bank
 * lock, channels, ramps and loops. */
// IOP 0x00008c60 (RoR 0x00008988, word for word)
static void RxSndDrvSeReap(void)
{
    int intr;

    CpuSuspendIntr(&intr);
    RxSndDrvSe **link = &s_se.m_active;
    RxSndDrvSe  *se;

    while ((se = *link) != NULL)
    {
        if ((se->m_flags & (RXSNDDRV_SE_STARTED | RXSNDDRV_SE_STOP)) ==
                (RXSNDDRV_SE_STARTED | RXSNDDRV_SE_STOP) &&
            se->m_notes == NULL)
        {
            if (se->m_flags & RXSNDDRV_SE_LOCKED)
                RxSndDrvXvabUnlock(se->m_xvab);
            RxSndDrvSeFreeChannels(se);
            RxSndDrvSeFreeLoops(se);
            se->m_flags = 0;
            *link       = se->m_next;
            continue;
        }
        link = &se->m_next;
    }
    CpuResumeIntr(intr);
}

/* Starts a NOTE (the SE's bank, voice group and owner) for byte code 04 or
 * a one-key SE and keeps it under `tag` on channel `channel`; 1 when it
 * plays. */
// IOP 0x00008d34 (RoR 0x00008a5c, word for word)
static int RxSndDrvSeNoteOn(RxSndDrvSe *se, int tag, int channel, int program, int key,
                            int velocity)
{
    RxSndDrvSeNote *note = s_se.m_noteFree;

    if (note == NULL)
        return 0;

    RxSndDrvSeChannel *chan = RxSndDrvSeGetChannel(se, channel);
    if (chan == NULL)
        return 0;

    int id = RxSndDrvNotePlay(se->m_xvab, program, key, se->m_group, se->m_owner);
    if (id < 0)
        return 0;

    s_se.m_noteFree = note->m_next;
    note->m_next    = se->m_notes;
    se->m_notes     = note;
    note->m_channel = chan;
    note->m_tag     = tag;
    note->m_note    = (signed char)id;
    note->m_key     = (u_char)key;
    note->m_dirty   = 0xff;
    RxSndDrvNoteSetVelocity(id, velocity);
    return 1;
}

/* Byte code 05 / 06: keys off (release) or frees the newest note `tag` of
 * `se`; a freed note keeps its record (id -1) until the update drops it. */
// IOP 0x00008e20 (RoR 0x00008b48, word for word)
static void RxSndDrvSeNoteOff(RxSndDrvSe *se, int tag, int release)
{
    RxSndDrvSeNote *note;

    for (note = se->m_notes; note != NULL; note = note->m_next)
        if (note->m_tag == tag)
            break;
    if (note == NULL)
        return;
    /* PC: the original keys off a freed record's id -1 too (reading the
     * bytes before NOTE 0); skipped. */
    if (release)
    {
        if (note->m_note >= 0)
            RxSndDrvNoteKeyOff(note->m_note);
        return;
    }
    RxSndDrvNoteFree(note->m_note);
    note->m_note = -1;
}

/* Byte code 07 / 09: sets channel `channel`'s parameter `kind` to `value`
 * (add: m_param + value), clamped to 0..0xff, and cancels its ramp. */
// IOP 0x00008ea4 (RoR 0x00008bcc, word for word)
static int RxSndDrvSeChannelSet(RxSndDrvSe *se, int channel, u_int kind, int value, int add)
{
    RxSndDrvSeChannel *chan = RxSndDrvSeGetChannel(se, channel);

    if (chan == NULL)
        return 0;

    RxSndDrvSeRamp **link = &chan->m_ramps;
    for (RxSndDrvSeRamp *ramp = *link; ramp != NULL; link = &ramp->m_next, ramp = ramp->m_next)
    {
        if (ramp->m_kind == kind)
        {
            *link           = ramp->m_next;
            ramp->m_next    = s_se.m_rampFree;
            s_se.m_rampFree = ramp;
            break;
        }
    }

    u_char  dirty;
    u_char *param = RxSndDrvSeChannelParam(chan, kind, &dirty);

    chan->m_dirty |= dirty;
    if (add)
    {
        value += *param;
        if (value > 0xff)
            value = 0xff;
        if (value < 0)
            value = 0;
    }
    *param = (u_char)value;
    return 1;
}

/* Channel `channel` of `se`, made with the defaults when new; NULL when the
 * pool is empty. */
// IOP 0x00009024 (RoR 0x00008d4c, word for word)
static RxSndDrvSeChannel *RxSndDrvSeGetChannel(RxSndDrvSe *se, int channel)
{
    for (RxSndDrvSeChannel *chan = se->m_channels; chan != NULL; chan = chan->m_next)
        if (chan->m_id == channel)
            return chan;

    RxSndDrvSeChannel *chan = s_se.m_channelFree;
    if (chan == NULL)
        return NULL;
    s_se.m_channelFree = chan->m_next;
    chan->m_next       = se->m_channels;
    se->m_channels     = chan;
    chan->m_id         = channel;
    chan->m_dirty      = 0;
    chan->m_param[RXSNDDRV_SE_CHAN_VOLUME] = 0x80;
    chan->m_param[RXSNDDRV_SE_CHAN_PAN]    = 0x40;
    chan->m_param[RXSNDDRV_SE_CHAN_BEND]   = 0x40;
    chan->m_param[RXSNDDRV_SE_CHAN_DEPTH]  = 0;
    chan->m_param[RXSNDDRV_SE_CHAN_RATE]   = 0x80;
    chan->m_param[RXSNDDRV_SE_CHAN_OFFSET] = 0x80;
    chan->m_ramps      = NULL;
    return chan;
}

// IOP 0x000090c8 (RoR 0x00008df0, word for word)
static void RxSndDrvSeFreeChannels(RxSndDrvSe *se)
{
    while (se->m_channels != NULL)
    {
        RxSndDrvSeChannel *chan = se->m_channels;

        RxSndDrvSeFreeRamps(chan);
        se->m_channels     = chan->m_next;
        chan->m_next       = s_se.m_channelFree;
        s_se.m_channelFree = chan;
    }
}

// IOP 0x00009144 (RoR 0x00008e6c, word for word)
static void RxSndDrvSeFreeLoops(RxSndDrvSe *se)
{
    while (se->m_loops != NULL)
    {
        RxSndDrvSeLoop *loop = se->m_loops;

        se->m_loops     = loop->m_next;
        loop->m_next    = s_se.m_loopFree;
        s_se.m_loopFree = loop;
    }
}

// IOP 0x00009190 (RoR 0x00008eb8, 90% of it aligned: FS2 read, the same but for an address)
static void RxSndDrvSeFreeRamps(RxSndDrvSeChannel *chan)
{
    while (chan->m_ramps != NULL)
    {
        RxSndDrvSeRamp *ramp = chan->m_ramps;

        chan->m_ramps   = ramp->m_next;
        ramp->m_next    = s_se.m_rampFree;
        s_se.m_rampFree = ramp;
    }
}
