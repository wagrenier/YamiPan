/* ==========================================================================
 *  pcm.cpp  (RXSNDDRV.IRX 0xb4a0..0xce84: the PCM stream system)
 *
 *  Streams of 16-bit PCM the EE feeds a block at a time (rx/rxsnd.c
 *  rxSndPcmOpen / Write / End), mixed on the IOP into core 0's sound data
 *  input (FS2; RoR's was core 1's): an auto-DMA ring of 2..8 0x1000-byte
 *  halves (2 until command 0xb4) that the SPU2 plays in a loop at 48 kHz
 *  (sceSdBlockTrans SD_BLOCK_TRANS_LOOP, started by the first tick), each
 *  half four blocks of 0x100 L then 0x100 R samples.  While the SPU plays
 *  one half the V-blank tick mixes every ready stream into each half from
 *  the last one mixed up to it.
 *
 *    0xb0 {id, format, -, -, iop, blocks}: open (RxSndDrvPcmOpen); format 0
 *         mono (0x800-byte blocks), 1 L and R planes (the R plane `blocks` *
 *         0x800 after the L one), 2 interleaved L/R (0x1000-byte blocks),
 *         3 interleaved with the samples xored by m_xor first
 *    0xb1 {id, count, -, -} then `count` sequence numbers: that many more
 *         blocks of the IOP ring are filled (RxSndDrvPcmData)
 *    0xb2 {id, param, value}: 0 close, 1 pause, 2 the xor word (L ^ high
 *         half, R ^ low half), 4 volume L | R << 16 (-0x80..0x80), 5 the
 *         end: the stream ends once its last block is heard (RxSndDrvPcmSet)
 *    0xb3 {id, status, filled, seq, pos} per open stream (RxSndDrvPcmStatus)
 *
 *  A ring block mixes into one SPU half, so a stream keeps per half the
 *  sequence number of the block written there; the status reports the one
 *  under the play position and the sample offset in it, which is how the EE
 *  frees ring space and fires its position callbacks.
 *
 *  Command 0xb4 is FS2's (RxSndDrvPcmSetB4): the ring's halves, 2..8; the
 *  next tick stops the ring, waits for the auto-DMA to report stopped, and
 *  restarts it that long from half 0.  The EE's sender has no caller.
 *
 *  FS2 read 2026-11-09 (Ghidra's rxsnddrv.elf): the stream record grew by
 *  six more half tags (8) and m_fresh; everything else is RoR's.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "intrman.h"
#include "libsd.h"
#include "vblank.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RXSNDDRV_PCM_SLOTS  0x20
#define RXSNDDRV_PCM_NODES  0x100
#define RXSNDDRV_PCM_MIXERS 4           /* one per format */
#define RXSNDDRV_PCM_PER_MIX 3          /* streams a mixer pass takes */

/* The SPU input ring: 2..8 halves of four blocks of 0x100 L, 0x100 R. */
#define RXSNDDRV_PCM_HALF   0x1000
#define RXSNDDRV_PCM_HALVES 8           /* the most command 0xb4 sets */
#define RXSNDDRV_PCM_BLOCK  0x100       /* samples per channel */

/* RxSndDrvPcmSlot::m_flags (the low byte is the status the EE reads back). */
enum
{
    RXSNDDRV_PCM_USED    = 0x00001,     /* opened by command 0xb0 */
    RXSNDDRV_PCM_PLAYING = 0x00002,     /* a block of it is under the play position */
    RXSNDDRV_PCM_ENDED   = 0x00004,     /* the last block has been reached */
    RXSNDDRV_PCM_PAUSED  = 0x00008,
    RXSNDDRV_PCM_ACTIVE  = 0x10000,     /* seen by a tick */
    RXSNDDRV_PCM_CLOSED  = 0x20000,
    RXSNDDRV_PCM_END     = 0x40000,     /* the EE has sent its last block */
};

/* RxSndDrvPcmPending::m_mask */
enum
{
    RXSNDDRV_PCM_SET_OPEN  = 0x01,      /* opened: the tick makes it active */
    RXSNDDRV_PCM_SET_CLOSE = 0x02,
    RXSNDDRV_PCM_SET_PAUSE = 0x04,
    RXSNDDRV_PCM_SET_XOR   = 0x08,
    RXSNDDRV_PCM_SET_VOL   = 0x10,
    RXSNDDRV_PCM_SET_END   = 0x20,
};

/* A filled block's sequence number, queued until a tick mixes it (0xc on
 * the IOP, 0x100 at RoR IOP 0x35c54).  layout: host. */
struct RxSndDrvPcmNode
{
    RxSndDrvPcmNode *m_back;            // IOP +0x0: towards the newest
    RxSndDrvPcmNode *m_fwd;             // IOP +0x4: towards the oldest; the free list's link
    int              m_seq;             // IOP +0x8
};

/* The set commands' record, per bank (0x10). */
struct RxSndDrvPcmPending
{
    u_int m_mask;                       // IOP +0x0: RXSNDDRV_PCM_SET_*
    u_int m_pause;                      // IOP +0x4
    u_int m_xor;                        // IOP +0x8
    u_int m_vol;                        // IOP +0xc: L | R << 16
};
static_assert(sizeof(RxSndDrvPcmPending) == 0x10, "RxSndDrvPcmPending");

/* A stream (0x88 on the IOP, 0x20 at FS2 IOP 0x353e0; RoR's 0x6c).  layout: host. */
struct RxSndDrvPcmSlot
{
    RxSndDrvPcmSlot *m_next;            // IOP +0x00: the open list
    int     m_format;                   // IOP +0x04: 0..3, the mixer
    int     m_id;                       // IOP +0x08
    u_int   m_flags;                    // IOP +0x0c: RXSNDDRV_PCM_*
    u_int   m_iop;                      // IOP +0x10: the ring in IOP memory
    int     m_blocks;                   // IOP +0x14: its size in blocks
    int     m_written;                  // IOP +0x18: blocks announced, capped at m_blocks; read nowhere
    int     m_read;                     // IOP +0x1c: the next block to mix
    int     m_filled;                   // IOP +0x20: blocks announced and not yet mixed
    RxSndDrvPcmNode m_queue;            // IOP +0x24: the sequence numbers' list head
    int     m_tag[RXSNDDRV_PCM_HALVES]; // IOP +0x30: per SPU half, the sequence number mixed there, -1 none
    short   m_vol[2];                   // IOP +0x50: L, R, 0x80 unity
    u_int   m_xor;                      // IOP +0x54: format 3's sample xor
    int     m_playSeq;                  // IOP +0x58: the sequence number being heard
    int     m_playPos;                  // IOP +0x5c: samples into it
    int     m_lastSeq;                  // IOP +0x60: the last one announced
    int     m_fresh;                    // IOP +0x64: FS2: 1 until the first data; a volume set applies at once
    RxSndDrvPcmPending m_pending[2];    // IOP +0x68: per bank
};

struct RxSndDrvPcmMixer;

typedef void (*RxSndDrvPcmMix)(RxSndDrvPcmMixer *mix, short *dst, const short *acc);

/* One stream of a mixer pass (0x14 on the IOP).  layout: host. */
struct RxSndDrvPcmMixEntry
{
    RxSndDrvPcmSlot *m_slot;            // IOP +0x00
    const short     *m_src[2];          // IOP +0x04: the block; [1] format 1's R plane
    short            m_vol[2];          // IOP +0x0c: L, R from RxSndDrvPcmVolume, 0x1000 unity
    u_int            m_xor;             // IOP +0x10
};

/* A format's mixer (0x44 on the IOP, 4 at RoR IOP 0x38860).  layout: host. */
struct RxSndDrvPcmMixer
{
    RxSndDrvPcmMix      m_mix;          // IOP +0x00
    RxSndDrvPcmMixEntry m_entry[RXSNDDRV_PCM_PER_MIX]; // IOP +0x04
    int                 m_count;        // IOP +0x40
};

/* Volume curve (RoR IOP 0xe2e0): v * |v| / 4 for v = -0x80..0x80, the ROM's own
 * values (its 0x7f entry is 0x1000 too). */
static const short RxSndDrvPcmVolume[0x101] = {
    -4096, -4032, -3969, -3906, -3844, -3782, -3721, -3660, -3600, -3540, -3481, -3422, -3364, -3306, -3249, -3192,
    -3136, -3080, -3025, -2970, -2916, -2862, -2809, -2756, -2704, -2652, -2601, -2550, -2500, -2450, -2401, -2352,
    -2304, -2256, -2209, -2162, -2116, -2070, -2025, -1980, -1936, -1892, -1849, -1806, -1764, -1722, -1681, -1640,
    -1600, -1560, -1521, -1482, -1444, -1406, -1369, -1332, -1296, -1260, -1225, -1190, -1156, -1122, -1089, -1056,
    -1024, -992, -961, -930, -900, -870, -841, -812, -784, -756, -729, -702, -676, -650, -625, -600,
    -576, -552, -529, -506, -484, -462, -441, -420, -400, -380, -361, -342, -324, -306, -289, -272,
    -256, -240, -225, -210, -196, -182, -169, -156, -144, -132, -121, -110, -100, -90, -81, -72,
    -64, -56, -49, -42, -36, -30, -25, -20, -16, -12, -9, -6, -4, -2, -1, 0,
    0, 0, 1, 2, 4, 6, 9, 12, 16, 20, 25, 30, 36, 42, 49, 56,
    64, 72, 81, 90, 100, 110, 121, 132, 144, 156, 169, 182, 196, 210, 225, 240,
    256, 272, 289, 306, 324, 342, 361, 380, 400, 420, 441, 462, 484, 506, 529, 552,
    576, 600, 625, 650, 676, 702, 729, 756, 784, 812, 841, 870, 900, 930, 961, 992,
    1024, 1056, 1089, 1122, 1156, 1190, 1225, 1260, 1296, 1332, 1369, 1406, 1444, 1482, 1521, 1560,
    1600, 1640, 1681, 1722, 1764, 1806, 1849, 1892, 1936, 1980, 2025, 2070, 2116, 2162, 2209, 2256,
    2304, 2352, 2401, 2450, 2500, 2550, 2601, 2652, 2704, 2756, 2809, 2862, 2916, 2970, 3025, 3080,
    3136, 3192, 3249, 3306, 3364, 3422, 3481, 3540, 3600, 3660, 3721, 3782, 3844, 3906, 3969, 4096,
    4096,
};

/* FS2 IOP 0x353e0..0x3f0fc, cleared together by the init. */
static struct
{
    RxSndDrvPcmSlot  m_slot[RXSNDDRV_PCM_SLOTS];  // FS2 IOP 0x353e0
    RxSndDrvPcmSlot *m_open;                      // FS2 IOP 0x364e0
    RxSndDrvPcmNode  m_node[RXSNDDRV_PCM_NODES];  // FS2 IOP 0x364e4
    RxSndDrvPcmNode *m_free;                      // FS2 IOP 0x370e4
    alignas(64) short m_spu[RXSNDDRV_PCM_HALVES][RXSNDDRV_PCM_HALF / 2]; // FS2 IOP 0x370e8: core 0's input ring
    int              m_half;                      // FS2 IOP 0x3f0e8: the half to mix next
    int              m_halves;                    // FS2 IOP 0x3f0ec: the ring's length, 2..8
    int              m_started;                   // FS2 IOP 0x3f0f0: the ring runs
    int              m_resize;                    // FS2 IOP 0x3f0f4: command 0xb4 came
    int              m_resizeTo;                  // FS2 IOP 0x3f0f8: its halves
} s_pcm;

static RxSndDrvPcmMixer s_pcmMixer[RXSNDDRV_PCM_MIXERS]; // FS2 IOP 0x3f100

static void RxSndDrvPcmMixMono(RxSndDrvPcmMixer *mix, short *dst, const short *acc);
static void RxSndDrvPcmMixSplit(RxSndDrvPcmMixer *mix, short *dst, const short *acc);
static void RxSndDrvPcmMixPairs(RxSndDrvPcmMixer *mix, short *dst, const short *acc);
static void RxSndDrvPcmMixPairsXor(RxSndDrvPcmMixer *mix, short *dst, const short *acc);
static void RxSndDrvPcmClear(short *half);
static void RxSndDrvPcmDownmix(short *half);

static inline int RxSndDrvPcmClampSample(int s)
{
    if (s >= 0x8000)
        return 0x7fff;
    if (s < -0x8000)
        return -0x8000;
    return s;
}

static inline int RxSndDrvPcmClampVol(int v)
{
    if (v < -0x80)
        return -0x80;
    if (v > 0x80)
        return 0x80;
    return v;
}

static inline const short *RxSndDrvPcmIop(u_int iop)
{
    return (const short *)(uintptr_t)iop;
}

/* Entry `i`'s source, or the zero buffer for an entry this pass left empty
 * (its stale volume then multiplies zeros). */
static inline const short *RxSndDrvPcmSource(const RxSndDrvPcmMixer *mix, int i, int ch)
{
    if (mix->m_count <= i)
        return (const short *)RxSndDrvZeroBuffer();
    return mix->m_entry[i].m_src[ch];
}

/* The tail every mixer shares: each stream mixed moves on a block. */
static void RxSndDrvPcmMixDone(RxSndDrvPcmMixer *mix)
{
    for (int i = 0; i < mix->m_count; i++)
    {
        RxSndDrvPcmSlot *s = mix->m_entry[i].m_slot;
        int              intr;

        s->m_read = (s->m_read + 1) % s->m_blocks;
        CpuSuspendIntr(&intr);
        s->m_filled--;
        CpuResumeIntr(intr);
    }
    mix->m_count = 0;
}

/* Format 0: one channel heard on both sides.  The only mixer that folds the
 * volumes for a mono output itself (the others leave it to the downmix). */
// IOP 0x0000bf1c (RoR 0x0000bc3c, word for word)
static void RxSndDrvPcmMixMono(RxSndDrvPcmMixer *mix, short *dst, const short *acc)
{
    const short *s0 = RxSndDrvPcmSource(mix, 0, 0);
    const short *s1 = RxSndDrvPcmSource(mix, 1, 0);
    const short *s2 = RxSndDrvPcmSource(mix, 2, 0);

    int l0 = mix->m_entry[0].m_vol[0], r0 = mix->m_entry[0].m_vol[1];
    int l1 = mix->m_entry[1].m_vol[0], r1 = mix->m_entry[1].m_vol[1];
    int l2 = mix->m_entry[2].m_vol[0], r2 = mix->m_entry[2].m_vol[1];

    if (RxSndDrvGetOutputMode() == 0)
    {
        l0 = r0 = ((l0 < 0 ? -l0 : l0) + (r0 < 0 ? -r0 : r0)) >> 1;
        l1 = r1 = ((l1 < 0 ? -l1 : l1) + (r1 < 0 ? -r1 : r1)) >> 1;
        l2 = r2 = ((l2 < 0 ? -l2 : l2) + (r2 < 0 ? -r2 : r2)) >> 1;
    }

    for (int b = 0; b < 4; b++)
    {
        short       *dl = dst + b * RXSNDDRV_PCM_BLOCK * 2;
        short       *dr = dl + RXSNDDRV_PCM_BLOCK;
        const short *al = acc + b * RXSNDDRV_PCM_BLOCK * 2;
        const short *ar = al + RXSNDDRV_PCM_BLOCK;

        for (int i = 0; i < RXSNDDRV_PCM_BLOCK; i++)
        {
            int a = *s0++, c = *s1++, e = *s2++;
            int l = ((a * l0 + c * l1 + e * l2) >> 12) + al[i];
            int r = ((a * r0 + c * r1 + e * r2) >> 12) + ar[i];

            dl[i] = (short)RxSndDrvPcmClampSample(l);
            dr[i] = (short)RxSndDrvPcmClampSample(r);
        }
    }
    RxSndDrvPcmMixDone(mix);
}

/* Format 1: an L plane and an R plane. */
// IOP 0x0000c288 (RoR 0x0000bfa8, word for word)
static void RxSndDrvPcmMixSplit(RxSndDrvPcmMixer *mix, short *dst, const short *acc)
{
    const short *l0s = RxSndDrvPcmSource(mix, 0, 0), *r0s = RxSndDrvPcmSource(mix, 0, 1);
    const short *l1s = RxSndDrvPcmSource(mix, 1, 0), *r1s = RxSndDrvPcmSource(mix, 1, 1);
    const short *l2s = RxSndDrvPcmSource(mix, 2, 0), *r2s = RxSndDrvPcmSource(mix, 2, 1);

    const int l0 = mix->m_entry[0].m_vol[0], r0 = mix->m_entry[0].m_vol[1];
    const int l1 = mix->m_entry[1].m_vol[0], r1 = mix->m_entry[1].m_vol[1];
    const int l2 = mix->m_entry[2].m_vol[0], r2 = mix->m_entry[2].m_vol[1];

    for (int b = 0; b < 4; b++)
    {
        short       *dl = dst + b * RXSNDDRV_PCM_BLOCK * 2;
        short       *dr = dl + RXSNDDRV_PCM_BLOCK;
        const short *al = acc + b * RXSNDDRV_PCM_BLOCK * 2;
        const short *ar = al + RXSNDDRV_PCM_BLOCK;

        for (int i = 0; i < RXSNDDRV_PCM_BLOCK; i++)
        {
            int l = ((*l0s++ * l0 + *l1s++ * l1 + *l2s++ * l2) >> 12) + al[i];
            int r = ((*r0s++ * r0 + *r1s++ * r1 + *r2s++ * r2) >> 12) + ar[i];

            dl[i] = (short)RxSndDrvPcmClampSample(l);
            dr[i] = (short)RxSndDrvPcmClampSample(r);
        }
    }
    RxSndDrvPcmMixDone(mix);
}

/* Format 2: L, R sample pairs. */
// IOP 0x0000c5e0 (RoR 0x0000c300, word for word)
static void RxSndDrvPcmMixPairs(RxSndDrvPcmMixer *mix, short *dst, const short *acc)
{
    const short *s0 = RxSndDrvPcmSource(mix, 0, 0);
    const short *s1 = RxSndDrvPcmSource(mix, 1, 0);
    const short *s2 = RxSndDrvPcmSource(mix, 2, 0);

    const int l0 = mix->m_entry[0].m_vol[0], r0 = mix->m_entry[0].m_vol[1];
    const int l1 = mix->m_entry[1].m_vol[0], r1 = mix->m_entry[1].m_vol[1];
    const int l2 = mix->m_entry[2].m_vol[0], r2 = mix->m_entry[2].m_vol[1];

    for (int b = 0; b < 4; b++)
    {
        short       *dl = dst + b * RXSNDDRV_PCM_BLOCK * 2;
        short       *dr = dl + RXSNDDRV_PCM_BLOCK;
        const short *al = acc + b * RXSNDDRV_PCM_BLOCK * 2;
        const short *ar = al + RXSNDDRV_PCM_BLOCK;

        for (int i = 0; i < RXSNDDRV_PCM_BLOCK; i++)
        {
            int l = ((s0[0] * l0 + s1[0] * l1 + s2[0] * l2) >> 12) + al[i];
            int r = ((s0[1] * r0 + s1[1] * r1 + s2[1] * r2) >> 12) + ar[i];

            s0 += 2;
            s1 += 2;
            s2 += 2;
            dl[i] = (short)RxSndDrvPcmClampSample(l);
            dr[i] = (short)RxSndDrvPcmClampSample(r);
        }
    }
    RxSndDrvPcmMixDone(mix);
}

/* Format 3: pairs, L xored by the high half of m_xor and R by the low one
 * (0x8000 makes unsigned samples signed). */
// IOP 0x0000c894 (RoR 0x0000c5b4, word for word)
static void RxSndDrvPcmMixPairsXor(RxSndDrvPcmMixer *mix, short *dst, const short *acc)
{
    const u_short *s0 = (const u_short *)RxSndDrvPcmSource(mix, 0, 0);
    const u_short *s1 = (const u_short *)RxSndDrvPcmSource(mix, 1, 0);
    const u_short *s2 = (const u_short *)RxSndDrvPcmSource(mix, 2, 0);

    const int l0 = mix->m_entry[0].m_vol[0], r0 = mix->m_entry[0].m_vol[1];
    const int l1 = mix->m_entry[1].m_vol[0], r1 = mix->m_entry[1].m_vol[1];
    const int l2 = mix->m_entry[2].m_vol[0], r2 = mix->m_entry[2].m_vol[1];

    const u_short xl0 = (u_short)(mix->m_entry[0].m_xor >> 16), xr0 = (u_short)mix->m_entry[0].m_xor;
    const u_short xl1 = (u_short)(mix->m_entry[1].m_xor >> 16), xr1 = (u_short)mix->m_entry[1].m_xor;
    const u_short xl2 = (u_short)(mix->m_entry[2].m_xor >> 16), xr2 = (u_short)mix->m_entry[2].m_xor;

    for (int b = 0; b < 4; b++)
    {
        short       *dl = dst + b * RXSNDDRV_PCM_BLOCK * 2;
        short       *dr = dl + RXSNDDRV_PCM_BLOCK;
        const short *al = acc + b * RXSNDDRV_PCM_BLOCK * 2;
        const short *ar = al + RXSNDDRV_PCM_BLOCK;

        for (int i = 0; i < RXSNDDRV_PCM_BLOCK; i++)
        {
            int l = (((short)(s0[0] ^ xl0) * l0 + (short)(s1[0] ^ xl1) * l1 +
                      (short)(s2[0] ^ xl2) * l2) >> 12) + al[i];
            int r = (((short)(s0[1] ^ xr0) * r0 + (short)(s1[1] ^ xr1) * r1 +
                      (short)(s2[1] ^ xr2) * r2) >> 12) + ar[i];

            s0 += 2;
            s1 += 2;
            s2 += 2;
            dl[i] = (short)RxSndDrvPcmClampSample(l);
            dr[i] = (short)RxSndDrvPcmClampSample(r);
        }
    }
    RxSndDrvPcmMixDone(mix);
}

/* FS2: the ring is two halves until command 0xb4, and starts in the first
 * tick (RoR's init started it, on core 1). */
// IOP 0x0000cbd4 (RoR 0x0000b4a0, moved in FS2, 80% alike: FS2 read 2026-11-09)
void RxSndDrvPcmInit(void)
{
    memset(&s_pcm, 0, sizeof(s_pcm));
    memset(s_pcmMixer, 0, sizeof(s_pcmMixer));
    s_pcm.m_halves = 2;

    for (int i = 0; i < RXSNDDRV_PCM_SLOTS; i++)
        s_pcm.m_slot[i].m_id = i;
    for (int i = 0; i < RXSNDDRV_PCM_NODES; i++)
    {
        s_pcm.m_node[i].m_fwd = s_pcm.m_free;
        s_pcm.m_free          = &s_pcm.m_node[i];
    }

    s_pcmMixer[0].m_mix = RxSndDrvPcmMixMono;
    s_pcmMixer[1].m_mix = RxSndDrvPcmMixSplit;
    s_pcmMixer[2].m_mix = RxSndDrvPcmMixPairs;
    s_pcmMixer[3].m_mix = RxSndDrvPcmMixPairsXor;

    if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO)
        printf("snddrv : completed to initialize PCM stream system\n");
}

/* Every V-blank: apply the pending set records, drop closed streams, start
 * or resize the ring (FS2), follow the play position, and mix the next block
 * of every ready stream into each half from the last one mixed up to the one
 * playing (FS2; RoR's mixed one half a tick). */
// IOP 0x0000b790 (RoR 0x0000b5ac, 85% of it aligned: FS2 read 2026-11-09)
void RxSndDrvPcmTick(void)
{
    const int        bank = RxSndDrvTickBank();
    RxSndDrvPcmSlot *s;
    int              intr;

    for (s = s_pcm.m_open; s != NULL; s = s->m_next)
    {
        if (s->m_pending[bank].m_mask & RXSNDDRV_PCM_SET_OPEN)
            s->m_flags |= RXSNDDRV_PCM_ACTIVE;
    }

    for (s = s_pcm.m_open; s != NULL; s = s->m_next)
    {
        if (!(s->m_flags & RXSNDDRV_PCM_ACTIVE))
            continue;

        RxSndDrvPcmPending *p = &s->m_pending[bank];

        if (p->m_mask & RXSNDDRV_PCM_SET_CLOSE)
            s->m_flags |= RXSNDDRV_PCM_CLOSED;
        if (p->m_mask & RXSNDDRV_PCM_SET_PAUSE)
        {
            if (p->m_pause == 0)
                s->m_flags &= ~RXSNDDRV_PCM_PAUSED;
            else if ((int)p->m_pause > 0 && (int)p->m_pause <= 2)
                s->m_flags |= RXSNDDRV_PCM_PAUSED;
        }
        if (p->m_mask & RXSNDDRV_PCM_SET_XOR)
            s->m_xor = p->m_xor;
        if (p->m_mask & RXSNDDRV_PCM_SET_VOL)
        {
            s->m_vol[0] = (short)p->m_vol;
            s->m_vol[1] = (short)(p->m_vol >> 16);
        }
        if (p->m_mask & RXSNDDRV_PCM_SET_END)
            s->m_flags |= RXSNDDRV_PCM_END;
        p->m_mask = 0;
    }

    CpuSuspendIntr(&intr);
    for (RxSndDrvPcmSlot **link = &s_pcm.m_open; (s = *link) != NULL;)
    {
        if ((s->m_flags & (RXSNDDRV_PCM_ACTIVE | RXSNDDRV_PCM_CLOSED)) ==
            (RXSNDDRV_PCM_ACTIVE | RXSNDDRV_PCM_CLOSED))
        {
            for (RxSndDrvPcmNode *n = s->m_queue.m_fwd, *fwd; n != &s->m_queue; n = fwd)
            {
                fwd          = n->m_fwd;
                n->m_fwd     = s_pcm.m_free;
                s_pcm.m_free = n;
            }
            *link      = s->m_next;
            s->m_flags = 0;
            continue;
        }
        link = &s->m_next;
    }
    CpuResumeIntr(intr);

    if (!s_pcm.m_started)
    {
        sceSdBlockTrans(SD_CORE_0, SD_BLOCK_TRANS_LOOP, (u_char *)s_pcm.m_spu,
                        (u_int)s_pcm.m_halves * RXSNDDRV_PCM_HALF, 0);
        sceSdSetParam(SD_P_BVOLL | SD_CORE_0, 0x7fff);
        sceSdSetParam(SD_P_BVOLR | SD_CORE_0, 0x7fff);
        s_pcm.m_started = 1;
    }
    if (s_pcm.m_resize)
    {
        /* Stopped, the auto-DMA reports 0. */
        sceSdBlockTrans(SD_CORE_0, SD_BLOCK_TRANS_STAT, NULL, 0, 0);
        while (sceSdBlockTransStatus(SD_CORE_0, 0) != 0)
            WaitVblankEnd();
        s_pcm.m_half   = 0;
        s_pcm.m_halves = s_pcm.m_resizeTo;
        sceSdBlockTrans(SD_CORE_0, SD_BLOCK_TRANS_LOOP, (u_char *)s_pcm.m_spu,
                        (u_int)s_pcm.m_resizeTo * RXSNDDRV_PCM_HALF, 0);
        s_pcm.m_resize = 0;
    }

    /* PC: the IOP masks the DMA address with 0xffffff before taking its
     * ring base off; host addresses are wider, so the difference is taken
     * on the same 32-bit truncation libsd reports, and kept in the ring. */
    const u_int pos  = ((u_int)sceSdBlockTransStatus(SD_CORE_0, 0) - (u_int)(uintptr_t)s_pcm.m_spu) %
                      ((u_int)s_pcm.m_halves * RXSNDDRV_PCM_HALF);
    const int   play = (int)(pos >> 12);

    for (s = s_pcm.m_open; s != NULL; s = s->m_next)
    {
        if (!(s->m_flags & RXSNDDRV_PCM_ACTIVE))
            continue;

        if (s->m_tag[play] < 0)
        {
            s->m_flags &= ~RXSNDDRV_PCM_PLAYING;
        }
        else
        {
            s->m_flags  |= RXSNDDRV_PCM_PLAYING;
            s->m_playSeq = s->m_tag[play];
            s->m_playPos = (int)((pos & (RXSNDDRV_PCM_HALF - 1)) >> 1);
        }
        if ((s->m_flags & RXSNDDRV_PCM_END) && s->m_lastSeq <= s->m_playSeq)
            s->m_flags |= RXSNDDRV_PCM_ENDED;
    }

    while (s_pcm.m_half != play)
    {
        short       *dst   = s_pcm.m_spu[s_pcm.m_half];
        const short *acc   = (const short *)RxSndDrvZeroBuffer();
        int          empty = 1;

        for (s = s_pcm.m_open; s != NULL; s = s->m_next)
        {
            /* PC: a format past 3 has no mixer (the IOP would index past the
             * table); the EE never opens one. */
            if ((s->m_flags & (RXSNDDRV_PCM_ACTIVE | RXSNDDRV_PCM_PAUSED)) != RXSNDDRV_PCM_ACTIVE ||
                s->m_filled <= 0 || (u_int)s->m_format >= RXSNDDRV_PCM_MIXERS)
            {
                s->m_tag[s_pcm.m_half] = -1;
                continue;
            }

            RxSndDrvPcmMixer    *mix = &s_pcmMixer[s->m_format];
            RxSndDrvPcmMixEntry *e   = &mix->m_entry[mix->m_count++];

            switch (s->m_format)
            {
            case 0:
                e->m_src[0] = RxSndDrvPcmIop(s->m_iop + (s->m_read << 11));
                e->m_src[1] = NULL;
                break;
            case 1:
                e->m_src[0] = RxSndDrvPcmIop(s->m_iop + s->m_read * 0x800);
                e->m_src[1] = RxSndDrvPcmIop(s->m_iop + s->m_read * 0x800 + s->m_blocks * 0x800);
                break;
            case 2:
            case 3:
                e->m_src[0] = RxSndDrvPcmIop(s->m_iop + (s->m_read << 12));
                e->m_src[1] = NULL;
                break;
            }
            e->m_slot   = s;
            e->m_vol[0] = RxSndDrvPcmVolume[RxSndDrvPcmClampVol(s->m_vol[0]) + 0x80];
            e->m_vol[1] = RxSndDrvPcmVolume[RxSndDrvPcmClampVol(s->m_vol[1]) + 0x80];
            e->m_xor    = s->m_xor;

            /* The half takes the oldest announced block's sequence number; with
             * none queued it keeps the one it had. */
            CpuSuspendIntr(&intr);
            RxSndDrvPcmNode *n = s->m_queue.m_fwd;
            if (n != &s->m_queue)
            {
                s->m_tag[s_pcm.m_half] = n->m_seq;
                n->m_back->m_fwd = n->m_fwd;
                n->m_fwd->m_back = n->m_back;
                n->m_fwd         = s_pcm.m_free;
                s_pcm.m_free     = n;
            }
            CpuResumeIntr(intr);

            empty = 0;
            if (mix->m_count >= RXSNDDRV_PCM_PER_MIX)
            {
                mix->m_mix(mix, dst, acc);
                acc = dst;
            }
        }

        if (empty)
        {
            RxSndDrvPcmClear(dst);
        }
        else
        {
            for (int i = 0; i < RXSNDDRV_PCM_MIXERS; i++)
            {
                if (s_pcmMixer[i].m_count > 0)
                {
                    s_pcmMixer[i].m_mix(&s_pcmMixer[i], dst, acc);
                    acc = dst;
                }
            }
        }

        if (RxSndDrvGetOutputMode() == 0)
            RxSndDrvPcmDownmix(dst);

        if (++s_pcm.m_half >= s_pcm.m_halves)
            s_pcm.m_half = 0;
    }
}

/* Closes every stream and stops the input (referenced nowhere in the
 * driver). */
// IOP 0x0000ccc0 (RoR 0x0000c8f4, 85% of it aligned: FS2 read, core 0)
void RxSndDrvPcmCloseAll(void)
{
    for (int i = 0; i < RXSNDDRV_PCM_SLOTS; i++)
    {
        u_int close[2] = {0, 0};

        RxSndDrvPcmSet(i, close);
    }
    sceSdBlockTrans(SD_CORE_0, SD_BLOCK_TRANS_STAT, NULL, 0, 0);
}

// IOP 0x0000cd10 (RoR 0x0000c944, 64% of it aligned: FS2 read, 8 tags and m_fresh)
int RxSndDrvPcmOpen(int id, int format, u_int iop, u_int blocks)
{
    RxSndDrvPcmSlot *s  = &s_pcm.m_slot[id];
    int              ok = 0;
    int              intr;

    CpuSuspendIntr(&intr);
    if (!(s->m_flags & RXSNDDRV_PCM_USED))
    {
        ok               = 1;
        s->m_flags       = RXSNDDRV_PCM_USED;
        s->m_queue.m_back = &s->m_queue;
        s->m_queue.m_fwd  = &s->m_queue;
        s->m_format      = format;
        s->m_iop         = iop;
        s->m_blocks      = (int)blocks;
        s->m_written     = 0;
        s->m_read        = 0;
        s->m_filled      = 0;
        s->m_vol[0]      = 0x80;
        s->m_vol[1]      = 0x80;
        s->m_xor         = 0;
        s->m_playSeq     = 0;
        s->m_playPos     = 0;
        s->m_lastSeq     = 0;
        s->m_fresh       = 1;
        for (int h = RXSNDDRV_PCM_HALVES - 1; h >= 0; h--)
            s->m_tag[h] = -1;

        s->m_pending[RxSndDrvCmdBank()].m_mask      = RXSNDDRV_PCM_SET_OPEN;
        s->m_pending[RxSndDrvCmdBank() == 0].m_mask = 0;

        s->m_next    = s_pcm.m_open;
        s_pcm.m_open = s;
    }
    CpuResumeIntr(intr);

    if (!ok && (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR))
        printf("snddrv : failed to open PCM slot '%d'\n", id);
    return ok;
}

/* `count` more blocks of the ring are filled; `seq` holds their sequence
 * numbers, queued for the halves they will be mixed into. */
// IOP 0x0000ce84 (RoR 0x0000cabc, 92% of it aligned: FS2 read, m_fresh)
void RxSndDrvPcmData(int id, const u_int *seq, int count)
{
    RxSndDrvPcmSlot *s = &s_pcm.m_slot[id];
    int              intr;

    CpuSuspendIntr(&intr);
    if (s->m_flags & RXSNDDRV_PCM_USED)
    {
        s->m_written += count;
        if (s->m_written >= s->m_blocks)
            s->m_written = s->m_blocks;

        s->m_filled += count;
        int n = (s->m_filled < s->m_blocks) ? s->m_filled : s->m_blocks;
        if (n < 0)
            n = 0;
        s->m_filled = n;

        for (int i = 0; i < count; i++)
        {
            RxSndDrvPcmNode *node = s_pcm.m_free;

            /* With the pool dry the number is dropped. */
            if (node != NULL)
            {
                s_pcm.m_free = node->m_fwd;

                node->m_fwd              = &s->m_queue;
                node->m_back             = s->m_queue.m_back;
                s->m_queue.m_back->m_fwd = node;
                s->m_queue.m_back        = node;
                node->m_seq              = (int)seq[i];
                s->m_lastSeq             = (int)seq[i];
            }
        }
        s->m_fresh = 0;
    }
    CpuResumeIntr(intr);
}

// IOP 0x0000cfec (RoR 0x0000cc28, 89% of it aligned: FS2 read: a volume before any data applies at once)
void RxSndDrvPcmSet(u_int id, const u_int *data)
{
    int intr;

    CpuSuspendIntr(&intr);
    if (s_pcm.m_slot[id].m_flags & RXSNDDRV_PCM_USED)
    {
        RxSndDrvPcmPending *p = &s_pcm.m_slot[id].m_pending[RxSndDrvCmdBank()];

        switch (data[0])
        {
        case 0:
            p->m_mask |= RXSNDDRV_PCM_SET_CLOSE;
            break;
        case 1:
            p->m_mask |= RXSNDDRV_PCM_SET_PAUSE;
            p->m_pause = data[1];
            break;
        case 2:
            p->m_mask |= RXSNDDRV_PCM_SET_XOR;
            p->m_xor   = data[1];
            break;
        case 4:
            if (s_pcm.m_slot[id].m_fresh)
            {
                s_pcm.m_slot[id].m_vol[0] = (short)data[1];
                s_pcm.m_slot[id].m_vol[1] = (short)(data[1] >> 16);
            }
            else
            {
                p->m_mask |= RXSNDDRV_PCM_SET_VOL;
                p->m_vol   = data[1];
            }
            break;
        case 5:
            p->m_mask |= RXSNDDRV_PCM_SET_END;
            break;
        }
    }
    CpuResumeIntr(intr);
}

/* {id, status, filled, seq, pos} (0xc) per open stream; the count. */
// IOP 0x0000d13c (RoR 0x0000cd58, 95% of it aligned: FS2 read, the same)
int RxSndDrvPcmStatus(u_char *reply)
{
    int count = 0;
    int intr;

    CpuSuspendIntr(&intr);
    for (const RxSndDrvPcmSlot *s = s_pcm.m_open; s != NULL; s = s->m_next)
    {
        reply[0]                    = (u_char)s->m_id;
        reply[1]                    = (u_char)s->m_flags;
        *(short *)(reply + 2)       = (short)s->m_filled;
        *(int *)(reply + 4)         = s->m_playSeq;
        *(int *)(reply + 8)         = s->m_playPos;
        reply += 0xc;
        count++;
    }
    CpuResumeIntr(intr);
    return count;
}

// IOP 0x0000d224 (RoR 0x0000cdf8, word for word)
static void RxSndDrvPcmClear(short *half)
{
    memset(half, 0, RXSNDDRV_PCM_HALF);
}

/* Mono output: each block's L and R both become their average. */
// IOP 0x0000d244 (RoR 0x0000ce18, 100% of it aligned: FS2 differs, unread)
static void RxSndDrvPcmDownmix(short *half)
{
    for (int b = 0; b < 4; b++)
    {
        short *l = half + b * RXSNDDRV_PCM_BLOCK * 2;
        short *r = l + RXSNDDRV_PCM_BLOCK;

        for (int i = 0; i < RXSNDDRV_PCM_BLOCK; i++)
            l[i] = r[i] = (short)((l[i] + r[i]) >> 1);
    }
}

/* Command 0xb4 (FS2 only): the ring's halves, clamped to 2..8; the next tick
 * restarts the ring that long. */
// IOP 0x0000d1dc (FS2 only, read 2026-11-09)
void RxSndDrvPcmSetB4(int value)
{
    s_pcm.m_resizeTo = value < 2 ? 2 : value > RXSNDDRV_PCM_HALVES ? RXSNDDRV_PCM_HALVES : value;
    s_pcm.m_resize   = 1;
}
