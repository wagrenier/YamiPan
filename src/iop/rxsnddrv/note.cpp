/* ==========================================================================
 *  note.cpp  (RXSNDDRV.IRX 0x8f10..0x9eb0: NOTE slots)
 *
 *  A NOTE is one key of one XVAB program: RxSndDrvNotePlay() looks program
 *  `program` up in the bank's TGIF table, takes one layer for every region
 *  of its TGSP list whose key range holds `key`, gives each a voice (auto
 *  free, the region's priority and voice group, the bank and region group
 *  as owner), sets its sample and keys it on, and answers the slot's id.
 *  The BGM sequencer and the SEs then drive it by id: switch, volume,
 *  velocity, pan, pitch scale, bend, pitch offset and the modulation depth
 *  and rate.  The setters only mark the slot dirty; RxSndDrvNoteTick()
 *  writes every dirty layer's voice once per V-blank (after BGM and SE,
 *  before the voice tick applies the registers).
 *
 *  Volume: L = vol L * region L * VELT[velocity] >> 7 * PANT[pan] >> 7, R
 *  the same with the right halves (PANT entries are 0x100: L at [pan], R at
 *  [pan + 0x80]).  Pitch: (key << 7 | fine) + bend * region range + the
 *  offset, clamped to 0..0x3fff, against the region's centre (or the note's
 *  own key when region flag 0x40 is set), scaled by the note's scale times
 *  the region's.  With a modulation depth (flag 0x80000) every region's
 *  three LFOs (LFOT curves of 0x80 steps: tremolo, auto-pan, vibrato) run
 *  at the note rate times their own, one cycle a second at rate 0x80.
 *
 *  A voice that is taken from a layer (reason 1) releases the whole note;
 *  flag 2 (RXSNDDRV_NOTE_PLAYING) clears when the last voice has gone, and
 *  the owner then frees the slot.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RXSNDDRV_NOTES  0x40
#define RXSNDDRV_LAYERS 0x100

/* RxSndDrvNote::m_flags */
enum
{
    RXSNDDRV_NOTE_USED  = 0x00001,      /* set by Play; key off only acts on it */
    RXSNDDRV_NOTE_VOICE = 0x00002,      /* a voice still sounds (RxSndDrvNoteGetFlags) */
    RXSNDDRV_NOTE_BEND  = 0x40000,      /* bend non-zero */
    RXSNDDRV_NOTE_MOD   = 0x80000,      /* modulation depth non-zero: the LFOs run */
};

/* m_dirty of a note and of a layer: what the next tick writes. */
enum
{
    RXSNDDRV_NOTE_SET_SWITCH = 0x1,
    RXSNDDRV_NOTE_SET_VOLUME = 0x2,
    RXSNDDRV_NOTE_SET_PITCH  = 0x4,
};

/* TGIF entry: a program (TGPG record, 8 bytes). */
struct RxSndDrvXvabProgram
{
    u_short m_unknown;                  // +0x00: read nowhere
    u_short m_count;                    // +0x02: regions
    int     m_regions;                  // +0x04: their TGSP records, from TGIF
};
static_assert(sizeof(RxSndDrvXvabProgram) == 0x8, "RxSndDrvXvabProgram");
static_assert(offsetof(RxSndDrvXvabProgram, m_count) == 0x2, "RxSndDrvXvabProgram::m_count");
static_assert(offsetof(RxSndDrvXvabProgram, m_regions) == 0x4, "RxSndDrvXvabProgram::m_regions");

/* TGSP record: one region of a program (0x24 bytes, from the file). */
struct RxSndDrvXvabRegion
{
    u_char      m_flags;                // +0x00: 1 dry, 2 reverb, 4 noise; 0x40 the key is the centre
    u_char      m_priority;             // +0x01: voice priority
    u_char      m_group;                // +0x02: owner = bank << 16 | group (0 none)
    u_char      m_voiceGroup;           // +0x03: 0 any core, 1 core 0, 2 core 1
    u_char      m_keyLo;                // +0x04
    u_char      m_keyHi;                // +0x05
    u_char      m_center;               // +0x06: the sample's key
    u_char      m_centerFine;           // +0x07
    u_char      m_velt;                 // +0x08: VELT curve
    u_char      m_pant;                 // +0x09: PANT curve
    signed char m_vol[2];               // +0x0a: L, R
    short       m_scale;                // +0x0c: pitch scale, 0x1000 = 1
    u_short     m_pad;                  // +0x0e
    u_short     m_adsr1;                // +0x10
    u_short     m_adsr2;                // +0x12
    u_int       m_ssa;                  // +0x14: from the bank's SPU base
    u_char      m_lfo[3];               // +0x18: LFOT curves: volume, pan, pitch
    u_char      m_depth[3];             // +0x1b: their depths
    u_char      m_rate[3];              // +0x1e: their rates (RxSndDrvNoteRate)
    u_char      m_bendDown;             // +0x21: bend range, semitones
    u_char      m_bendUp;               // +0x22
    u_char      m_pad2;                 // +0x23
};
static_assert(sizeof(RxSndDrvXvabRegion) == 0x24, "RxSndDrvXvabRegion");
static_assert(offsetof(RxSndDrvXvabRegion, m_keyLo) == 0x04, "RxSndDrvXvabRegion::m_keyLo");
static_assert(offsetof(RxSndDrvXvabRegion, m_velt) == 0x08, "RxSndDrvXvabRegion::m_velt");
static_assert(offsetof(RxSndDrvXvabRegion, m_vol) == 0x0a, "RxSndDrvXvabRegion::m_vol");
static_assert(offsetof(RxSndDrvXvabRegion, m_scale) == 0x0c, "RxSndDrvXvabRegion::m_scale");
static_assert(offsetof(RxSndDrvXvabRegion, m_adsr1) == 0x10, "RxSndDrvXvabRegion::m_adsr1");
static_assert(offsetof(RxSndDrvXvabRegion, m_ssa) == 0x14, "RxSndDrvXvabRegion::m_ssa");
static_assert(offsetof(RxSndDrvXvabRegion, m_lfo) == 0x18, "RxSndDrvXvabRegion::m_lfo");
static_assert(offsetof(RxSndDrvXvabRegion, m_depth) == 0x1b, "RxSndDrvXvabRegion::m_depth");
static_assert(offsetof(RxSndDrvXvabRegion, m_rate) == 0x1e, "RxSndDrvXvabRegion::m_rate");
static_assert(offsetof(RxSndDrvXvabRegion, m_bendDown) == 0x21, "RxSndDrvXvabRegion::m_bendDown");

struct RxSndDrvNote;

/* A layer: one region of a note on one voice (0x30 on the IOP).
 * layout: host -- pointers; the IOP offsets are in the comments. */
struct RxSndDrvNoteLayer
{
    RxSndDrvNoteLayer        *m_next;   // IOP +0x00: the note's list, or the free list
    RxSndDrvNote             *m_note;   // IOP +0x04
    const RxSndDrvXvabRegion *m_region; // IOP +0x08
    const u_char *m_velt;               // IOP +0x0c: VELT curve (0x80)
    const u_char *m_pant;               // IOP +0x10: PANT curve (L 0x80, R 0x80)
    const u_char *m_lfo[3];             // IOP +0x14: LFOT curves (0x80): volume, pan, pitch
    signed char   m_voice;              // IOP +0x20: -1 none
    u_char        m_dirty;              // IOP +0x21: RXSNDDRV_NOTE_SET_*
    int           m_phase[3];           // IOP +0x24: LFO phases, 0x80 steps << 12
};

/* A NOTE slot (0x2c on the IOP).  layout: host -- pointers; the IOP
 * offsets are in the comments. */
struct RxSndDrvNote
{
    RxSndDrvNote *m_next;               // IOP +0x00: the active list, or the free list
    int     m_xvab;                     // IOP +0x04: locked while the slot is taken
    u_int   m_flags;                    // IOP +0x08: RXSNDDRV_NOTE_*
    int     m_id;                       // IOP +0x0c: its index
    u_char  m_dirty;                    // IOP +0x10: RXSNDDRV_NOTE_SET_*
    u_char  m_key;                      // IOP +0x11
    u_char  m_fine;                     // IOP +0x12: always 0
    u_char  m_switch;                   // IOP +0x13: reverb on every voice
    short   m_vol[2];                   // IOP +0x14: L, R 0x80
    signed char m_velocity;             // IOP +0x18: 0..0x7f
    signed char m_pan;                  // IOP +0x19: 0..0x7f, 0x40 centre
    short   m_scale;                    // IOP +0x1a: pitch scale 0x1000
    short   m_unused;                   // IOP +0x1c: 0x1000 at Play, read nowhere
    short   m_bend;                     // IOP +0x1e: -0x1000..0x1000
    u_char  m_modDepth;                 // IOP +0x20: 0..0x80
    u_char  m_modRate;                  // IOP +0x21: RxSndDrvNoteRate index, 0x80 = 1
    signed char m_offset;               // IOP +0x22: pitch offset, semitones
    signed char m_offsetFine;           // IOP +0x23: and 1/128ths
    RxSndDrvNoteLayer *m_layers;        // IOP +0x24
    int     m_voices;                   // IOP +0x28: voices still held
};

/* The NOTE block (0x3b0c at RoR IOP 0x2cdf0, cleared as one).
 * layout: host (holds the host records); the IOP addresses are in the comments. */
struct RxSndDrvNoteState
{
    RxSndDrvNoteLayer  m_layer[RXSNDDRV_LAYERS]; // RoR IOP 0x2cdf0
    RxSndDrvNoteLayer *m_layerFree;              // RoR IOP 0x2fdf0
    RxSndDrvNote       m_note[RXSNDDRV_NOTES];   // RoR IOP 0x2fdf4
    RxSndDrvNote      *m_active;                 // RoR IOP 0x308f4
    RxSndDrvNote      *m_free;                   // RoR IOP 0x308f8
};

static RxSndDrvNoteState s_note;        // RoR IOP 0x0002cdf0

/* LFO rates (RoR IOP 0xdee0): 0x80 * 2^(i / 25.6)-ish, 0x1000 at index 0x80. */
static const int RxSndDrvNoteRate[0x100] = {
    128, 131, 135, 138, 142, 146, 150, 154,
    158, 163, 167, 172, 177, 182, 186, 192,
    197, 202, 208, 214, 219, 226, 232, 238,
    245, 251, 258, 265, 273, 280, 288, 296,
    304, 312, 321, 330, 339, 348, 358, 367,
    378, 388, 399, 410, 421, 432, 444, 456,
    469, 482, 495, 509, 523, 537, 552, 567,
    583, 599, 615, 632, 649, 667, 685, 704,
    724, 743, 764, 785, 806, 829, 851, 875,
    899, 923, 949, 975, 1002, 1029, 1057, 1086,
    1116, 1147, 1178, 1211, 1244, 1278, 1313, 1349,
    1386, 1424, 1463, 1504, 1545, 1587, 1631, 1676,
    1722, 1769, 1817, 1867, 1919, 1971, 2025, 2081,
    2138, 2197, 2257, 2319, 2383, 2448, 2515, 2584,
    2655, 2728, 2803, 2880, 2959, 3040, 3124, 3210,
    3298, 3388, 3481, 3577, 3675, 3776, 3880, 3986,
    4096, 4208, 4323, 4442, 4564, 4689, 4818, 4950,
    5086, 5226, 5369, 5517, 5668, 5824, 5983, 6148,
    6316, 6490, 6668, 6851, 7039, 7232, 7431, 7635,
    7844, 8059, 8281, 8508, 8742, 8981, 9228, 9481,
    9741, 10009, 10284, 10566, 10856, 11154, 11460, 11774,
    12098, 12430, 12771, 13121, 13482, 13852, 14232, 14622,
    15024, 15436, 15860, 16295, 16742, 17202, 17674, 18159,
    18657, 19169, 19696, 20236, 20792, 21362, 21949, 22551,
    23170, 23806, 24459, 25131, 25820, 26529, 27257, 28005,
    28774, 29564, 30375, 31209, 32065, 32945, 33850, 34779,
    35733, 36714, 37722, 38757, 39821, 40914, 42037, 43190,
    44376, 45594, 46845, 48131, 49452, 50809, 52204, 53636,
    55108, 56621, 58175, 59772, 61412, 63098, 64830, 66609,
    68437, 70315, 72245, 74228, 76265, 78359, 80509, 82719,
    84989, 87322, 89718, 92181, 94711, 97310, 99981, 102725,
    105545, 108441, 111418, 114476, 117618, 120846, 124162, 127570,
};

static u_char *IopPointer(u_int iop)
{
    return (u_char *)YamiPan_IopPointer(iop);
}

static void RxSndDrvNoteStop(RxSndDrvNote *note, int keyOff);
static void RxSndDrvNoteVoiceDone(int voice, int reason, void *arg);

/* Plays `key` of program `program` in bank `xvab`; `group` / `owner` >= 0
 * override every region's voice group / voice owner.  The slot id, or -1. */
// IOP 0x000091e0 (RoR 0x00008f10, word for word)
int RxSndDrvNotePlay(int xvab, int program, int key, int group, int owner)
{
    RxSndDrvNote *note = s_note.m_free;

    if (note == NULL)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_WARN)
            printf("snddrv : can't allocate NOTE slot\n");
        return -1;
    }
    if (RxSndDrvXvabLock(xvab) == 0)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_WARN)
            printf("snddrv : unregisted XVAB ID '%d'\n", xvab);
        return -1;
    }

    RxSndDrvXvabTables *tables = RxSndDrvXvabGetTables((u_int)xvab);
    if (tables == NULL)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_WARN)
            printf("snddrv : unregisted XVAB ID '%d'\n", xvab);
        goto fail;
    }

    {
        u_char *tgif = IopPointer(tables->m_tgif);
        u_char *velt = IopPointer(tables->m_velt);
        u_char *pant = IopPointer(tables->m_pant);
        u_char *lfot = IopPointer(tables->m_lfot);

        RxSndDrvXvabProgram *prog = NULL;
        if (program < *(u_short *)tgif)
            prog = (RxSndDrvXvabProgram *)(tgif + ((int *)tgif)[1 + program]);
        if (prog == NULL)
        {
            if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_WARN)
                printf("snddrv : unregisted program '%d' in XVAB '%d'\n", program, xvab);
            goto fail;
        }

        RxSndDrvXvabRegion  *reg  = (RxSndDrvXvabRegion *)(tgif + prog->m_regions);

        note->m_layers = NULL;
        for (int i = 0; i < prog->m_count; i++, reg++)
        {
            if (key < reg->m_keyLo || key > reg->m_keyHi)
                continue;

            RxSndDrvNoteLayer *layer = s_note.m_layerFree;
            if (layer == NULL)
                goto fail;

            layer->m_note   = note;
            layer->m_region = reg;
            layer->m_velt   = velt + reg->m_velt * 0x80 + 4;
            layer->m_pant   = pant + reg->m_pant * 0x100 + 4;
            layer->m_lfo[0] = lfot + reg->m_lfo[0] * 0x80 + 4;
            layer->m_lfo[1] = lfot + reg->m_lfo[1] * 0x80 + 4;
            layer->m_voice  = -1;
            layer->m_dirty  = 0;
            layer->m_phase[0] = 0;
            layer->m_phase[1] = 0;
            layer->m_phase[2] = 0;
            layer->m_lfo[2] = lfot + reg->m_lfo[2] * 0x80 + 4;

            s_note.m_layerFree = layer->m_next;
            layer->m_next      = note->m_layers;
            note->m_layers     = layer;
        }

        note->m_voices = 0;
        for (RxSndDrvNoteLayer *layer = note->m_layers; layer != NULL; layer = layer->m_next)
        {
            const RxSndDrvXvabRegion *r = layer->m_region;

            int vgroup = r->m_voiceGroup;
            if (group >= 0)
                vgroup = group;
            int vowner = r->m_group != 0 ? (xvab << 16 | r->m_group) : 0;
            if (owner >= 0)
                vowner = owner;

            layer->m_voice = (signed char)RxSndDrvVoiceAlloc(-1, r->m_priority, 1, vgroup, vowner,
                                                             RxSndDrvNoteVoiceDone, layer);
            if (layer->m_voice >= 0)
            {
                RxSndDrvVoiceSetSample(layer->m_voice, RxSndDrvXvabGetSpu(xvab) + r->m_ssa,
                                       r->m_adsr1, r->m_adsr2);
                RxSndDrvVoiceKeyOn(layer->m_voice);
                note->m_voices++;
                layer->m_dirty |= RXSNDDRV_NOTE_SET_SWITCH | RXSNDDRV_NOTE_SET_VOLUME |
                                  RXSNDDRV_NOTE_SET_PITCH;
            }
        }

        /* All or nothing: one region without a voice releases the others
         * (the note still starts while they fade). */
        for (RxSndDrvNoteLayer *layer = note->m_layers; layer != NULL; layer = layer->m_next)
        {
            if (layer->m_voice == -1)
            {
                RxSndDrvNoteStop(note, 0);
                break;
            }
        }

        if (note->m_voices > 0)
        {
            note->m_vol[0]     = 0x80;
            note->m_vol[1]     = 0x80;
            note->m_modRate    = 0x80;
            s_note.m_free      = note->m_next;
            note->m_flags      = RXSNDDRV_NOTE_USED | RXSNDDRV_NOTE_VOICE;
            note->m_velocity   = 0x7f;
            note->m_pan        = 0x40;
            note->m_scale      = 0x1000;
            note->m_unused     = 0x1000;
            note->m_xvab       = xvab;
            note->m_key        = (u_char)key;
            note->m_fine       = 0;
            note->m_switch     = 0;
            note->m_dirty      = 0;
            note->m_bend       = 0;
            note->m_modDepth   = 0;
            note->m_offset     = 0;
            note->m_offsetFine = 0;
            note->m_next       = s_note.m_active;
            s_note.m_active    = note;
            return note->m_id;
        }
    }

fail:
    RxSndDrvXvabUnlock(xvab);
    while (note->m_layers != NULL)
    {
        RxSndDrvNoteLayer *layer = note->m_layers;

        if (layer->m_voice >= 0)
            RxSndDrvVoiceRelease(layer->m_voice, 0, 0);
        note->m_layers     = layer->m_next;
        layer->m_next      = s_note.m_layerFree;
        s_note.m_layerFree = layer;
    }
    return -1;
}

/* Writes every dirty layer's voice: switches, volume (with tremolo and
 * auto-pan) and pitch (with bend and vibrato). */
// IOP 0x00009690 (RoR 0x000093c0, 98% of it aligned: FS2 read, the noise clock)
void RxSndDrvNoteTick(void)
{
    int rate = RxSndDrvGetFrameRate();

    for (RxSndDrvNote *note = s_note.m_active; note != NULL; note = note->m_next)
    {
        int step = 0;

        if (note->m_flags & RXSNDDRV_NOTE_MOD)
        {
            step = RxSndDrvNoteRate[note->m_modRate];
            note->m_dirty |= RXSNDDRV_NOTE_SET_VOLUME | RXSNDDRV_NOTE_SET_PITCH;
            step = (0x80000 / rate) * step >> 12;
        }

        for (RxSndDrvNoteLayer *layer = note->m_layers; layer != NULL; layer = layer->m_next)
        {
            if (layer->m_voice < 0)
                continue;

            const RxSndDrvXvabRegion *r = layer->m_region;
            u_char dirty = note->m_dirty | layer->m_dirty;

            if (dirty & RXSNDDRV_NOTE_SET_SWITCH)
            {
                u_char bits = r->m_flags & 1;
                if (r->m_flags & 4)
                    bits |= 4;
                if ((r->m_flags & 2) || note->m_switch != 0)
                    bits |= 2;
                RxSndDrvVoiceSetSwitch(layer->m_voice, bits);
                /* FS2: a noise region's centre note is the noise clock. */
                if (r->m_flags & 4)
                    RxSndDrvVoiceSetNoise(layer->m_voice, r->m_center);
            }

            if (dirty & RXSNDDRV_NOTE_SET_VOLUME)
            {
                const u_char *pan = layer->m_pant + note->m_pan;
                int vel = layer->m_velt[note->m_velocity];
                int l   = (note->m_vol[0] * r->m_vol[0] * vel >> 7) * pan[0] >> 7;
                int rr  = (note->m_vol[1] * r->m_vol[1] * vel >> 7) * pan[0x80] >> 7;

                if (note->m_flags & RXSNDDRV_NOTE_MOD)
                {
                    if (r->m_depth[1] != 0)
                    {
                        /* Auto-pan: indexes the curve's base, not the note's pan. */
                        int lfo = layer->m_lfo[1][layer->m_phase[1] >> 12];
                        const u_char *p =
                            layer->m_pant + (((lfo - 0x40) * r->m_depth[1] >> 7) * note->m_modDepth >> 7);

                        l  = l * p[0] >> 7;
                        layer->m_phase[1] =
                            (layer->m_phase[1] + (step * RxSndDrvNoteRate[r->m_rate[1]] >> 12)) & 0x7ffff;
                        rr = rr * p[0x80] >> 7;
                    }
                    if (r->m_depth[0] != 0)
                    {
                        int lfo = layer->m_lfo[0][layer->m_phase[0] >> 12];
                        int k   = 0x80 - ((lfo * r->m_depth[0] >> 7) * note->m_modDepth >> 7);

                        l  = l * k >> 7;
                        layer->m_phase[0] =
                            (layer->m_phase[0] + (step * RxSndDrvNoteRate[r->m_rate[0]] >> 12)) & 0x7ffff;
                        rr = rr * k >> 7;
                    }
                }
                RxSndDrvVoiceSetVolumeRaw(layer->m_voice, l, rr);
            }

            if (dirty & RXSNDDRV_NOTE_SET_PITCH)
            {
                short scale  = note->m_scale;
                short rscale = r->m_scale;
                int   pitch  = note->m_key << 7 | note->m_fine;

                if (note->m_flags & RXSNDDRV_NOTE_BEND)
                {
                    int range = note->m_bend < 0 ? r->m_bendDown : r->m_bendUp;
                    pitch += range * 0x80 * note->m_bend >> 12;
                }
                if (note->m_flags & RXSNDDRV_NOTE_MOD)
                {
                    int lfo = layer->m_lfo[2][layer->m_phase[2] >> 12];

                    layer->m_phase[2] =
                        (layer->m_phase[2] + (step * RxSndDrvNoteRate[r->m_rate[2]] >> 12)) & 0x7ffff;
                    pitch += (r->m_depth[2] * 0x80 * note->m_modDepth >> 7) * (lfo - 0x40) >> 6;
                }
                pitch += note->m_offset << 7 | note->m_offsetFine;
                if (pitch > 0x3fff)
                    pitch = 0x3fff;
                if (pitch < 0)
                    pitch = 0;

                u_char center, centerFine;
                if (r->m_flags & 0x40)
                {
                    center     = note->m_key;
                    centerFine = note->m_fine;
                }
                else
                {
                    center     = r->m_center;
                    centerFine = r->m_centerFine;
                }
                RxSndDrvVoiceSetPitch(layer->m_voice, center, centerFine, (u_short)(pitch >> 7),
                                      (short)(pitch & 0x7f), scale * rscale >> 12);
            }
            layer->m_dirty = 0;
        }
        note->m_dirty = 0;
    }
}

// IOP 0x00009bd8 (RoR 0x000098e4, word for word)
void RxSndDrvNoteInit(void)
{
    memset(&s_note, 0, sizeof(s_note));
    for (int i = 0; i < RXSNDDRV_LAYERS; i++)
    {
        s_note.m_layer[i].m_next = s_note.m_layerFree;
        s_note.m_layerFree       = &s_note.m_layer[i];
    }
    for (int i = 0; i < RXSNDDRV_NOTES; i++)
    {
        s_note.m_note[i].m_id   = i;
        s_note.m_note[i].m_next = s_note.m_free;
        s_note.m_free           = &s_note.m_note[i];
    }
}

/* Keys every voice of note `id` off (they ring out on their release). */
// IOP 0x00009c68 (RoR 0x00009974, word for word)
void RxSndDrvNoteKeyOff(int id)
{
    if (s_note.m_note[id].m_flags & RXSNDDRV_NOTE_USED)
        RxSndDrvNoteStop(&s_note.m_note[id], 1);
}

/* Releases note `id`'s voices and gives the slot, its layers and its bank
 * lock back. */
// IOP 0x00009cbc (RoR 0x000099c8, word for word)
void RxSndDrvNoteFree(int id)
{
    RxSndDrvNote **link = &s_note.m_active;
    RxSndDrvNote  *note = s_note.m_active;

    for (;;)
    {
        if (note == NULL)
            return;
        if (note->m_id == id)
            break;
        link = &note->m_next;
        note = note->m_next;
    }

    RxSndDrvXvabUnlock(note->m_xvab);
    RxSndDrvNoteStop(note, 0);
    while (note->m_layers != NULL)
    {
        RxSndDrvNoteLayer *layer = note->m_layers;

        note->m_layers     = layer->m_next;
        layer->m_next      = s_note.m_layerFree;
        s_note.m_layerFree = layer;
    }
    *link         = note->m_next;
    note->m_next  = s_note.m_free;
    s_note.m_free = note;
}

/* Reverb on every voice (adds switch bit 2). */
// IOP 0x00009da4 (RoR 0x00009ab0, word for word)
void RxSndDrvNoteSetSwitch(int id, int on)
{
    s_note.m_note[id].m_switch = (u_char)on;
    s_note.m_note[id].m_dirty |= RXSNDDRV_NOTE_SET_SWITCH;
}

// IOP 0x00009dd8 (RoR 0x00009ae4, word for word)
void RxSndDrvNoteSetVolume(int id, short l, short r)
{
    s_note.m_note[id].m_vol[0] = l;
    s_note.m_note[id].m_vol[1] = r;
    s_note.m_note[id].m_dirty |= RXSNDDRV_NOTE_SET_VOLUME;
}

// IOP 0x00009e10 (RoR 0x00009b1c, word for word)
void RxSndDrvNoteSetVelocity(int id, int velocity)
{
    if (velocity > 0x7f)
        velocity = 0x7f;
    if (velocity < 0)
        velocity = 0;
    s_note.m_note[id].m_velocity = (signed char)velocity;
    s_note.m_note[id].m_dirty |= RXSNDDRV_NOTE_SET_VOLUME;
}

// IOP 0x00009ea4 (RoR 0x00009bb0, word for word)
void RxSndDrvNoteSetPan(int id, int pan)
{
    if (pan > 0x7f)
        pan = 0x7f;
    if (pan < 0)
        pan = 0;
    s_note.m_note[id].m_pan = (signed char)pan;
    s_note.m_note[id].m_dirty |= RXSNDDRV_NOTE_SET_VOLUME;
}

/* Pitch scale, 0x1000 = 1. */
// IOP 0x00009f00 (RoR 0x00009c0c, word for word)
void RxSndDrvNoteSetPitch(int id, int scale)
{
    s_note.m_note[id].m_scale = (short)scale;
    s_note.m_note[id].m_dirty |= RXSNDDRV_NOTE_SET_PITCH;
}

/* Bend -0x1000..0x1000: the regions' bend ranges at the ends. */
// IOP 0x00009f34 (RoR 0x00009c40, word for word)
void RxSndDrvNoteSetBend(int id, int bend)
{
    RxSndDrvNote *note = &s_note.m_note[id];

    note->m_bend = (short)bend;
    if (bend == 0)
        note->m_flags &= ~RXSNDDRV_NOTE_BEND;
    else
        note->m_flags |= RXSNDDRV_NOTE_BEND;
    note->m_dirty |= RXSNDDRV_NOTE_SET_PITCH;
}

/* Pitch offset: semitones and 1/128ths (SEs). */
// IOP 0x00009f94 (RoR 0x00009ca0, word for word)
void RxSndDrvNoteSetOffset(int id, int semitones, int fine)
{
    s_note.m_note[id].m_offset     = (signed char)semitones;
    s_note.m_note[id].m_offsetFine = (signed char)fine;
    s_note.m_note[id].m_dirty |= RXSNDDRV_NOTE_SET_PITCH;
}

/* Modulation depth 0..0x80 (SEs); non-zero runs the LFOs. */
// IOP 0x00009fcc (RoR 0x00009cd8, word for word)
void RxSndDrvNoteSetModDepth(int id, int depth)
{
    RxSndDrvNote *note = &s_note.m_note[id];

    if (depth > 0x80)
        depth = 0x80;
    if (depth < 0)
        depth = 0;
    note->m_modDepth = (u_char)depth;
    if (depth == 0)
        note->m_flags &= ~RXSNDDRV_NOTE_MOD;
    else
        note->m_flags |= RXSNDDRV_NOTE_MOD;
    note->m_dirty |= RXSNDDRV_NOTE_SET_VOLUME | RXSNDDRV_NOTE_SET_PITCH;
}

/* Modulation rate 0..0xff, 0x80 = 1 (SEs); marks nothing dirty. */
// IOP 0x0000a058 (RoR 0x00009d64, word for word)
void RxSndDrvNoteSetModRate(int id, int rate)
{
    if (rate > 0xff)
        rate = 0xff;
    if (rate < 0)
        rate = 0;
    s_note.m_note[id].m_modRate = (u_char)rate;
}

// IOP 0x0000a0a8 (RoR 0x00009db4, word for word)
u_int RxSndDrvNoteGetFlags(int id)
{
    return s_note.m_note[id].m_flags;
}

/* Every voice of `note`: key off (keyOff) or release with the fade. */
// IOP 0x0000a0d0 (RoR 0x00009ddc, word for word)
static void RxSndDrvNoteStop(RxSndDrvNote *note, int keyOff)
{
    for (RxSndDrvNoteLayer *layer = note->m_layers; layer != NULL; layer = layer->m_next)
    {
        if (layer->m_voice < 0)
            continue;
        if (keyOff == 0)
            RxSndDrvVoiceRelease(layer->m_voice, 0, 0);
        else
            RxSndDrvVoiceKeyOff(layer->m_voice);
    }
}

/* A layer's voice has gone (reason 0 ended, 1 taken: then the whole note
 * goes).  A freed slot's layers can still hear this while their voices
 * fade; their m_note is the old one, as on the IOP. */
// IOP 0x0000a144 (RoR 0x00009e50, 95% of it aligned: FS2 read, the same but for record offsets)
static void RxSndDrvNoteVoiceDone(int voice, int reason, void *arg)
{
    RxSndDrvNoteLayer *layer = (RxSndDrvNoteLayer *)arg;
    RxSndDrvNote      *note  = layer->m_note;

    (void)voice;
    layer->m_voice = -1;
    if (--note->m_voices < 1)
        note->m_flags &= ~RXSNDDRV_NOTE_VOICE;
    if (reason == 1)
        RxSndDrvNoteStop(note, 0);
}
