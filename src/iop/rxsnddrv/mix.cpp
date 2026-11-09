/* ==========================================================================
 *  mix.cpp  (RXSNDDRV.IRX 0x19e0..0x1ca0: per-core master mix and volume)
 *
 *  Command 0x31 {core, param, value} (rx/rxsnd.c rxSndSetSysMix,
 *  rxSndSetSysVolume, rxSndSetSysMute): param 0 the core's MMIX bits, 1 its
 *  master volume {L, R} as two halves of the value, 2 (FS2) its external
 *  input's volume BVOL {L, R} the same way (rxSndSetSysMute: core 0, the PCM
 *  ring's core, 0x7fff or 0).
 *
 *  FS2 read 2026-11-09 (Ghidra's rxsnddrv.elf): param 2 and the record's
 *  m_input are FS2's; the defaults batch is RoR's.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "intrman.h"
#include "libsd.h"

#include <string.h>

/* FS2 0x10 (RoR 0xc). */
struct RxSndDrvMixPending
{
    u_int m_mask;                       // IOP +0x0: 1 mix, 2 volume, 4 input volume (FS2)
    u_int m_mix;                        // IOP +0x4
    short m_vol[2];                     // IOP +0x8: L, R
    short m_input[2];                   // IOP +0xc: FS2: BVOL L, R
};
static_assert(sizeof(RxSndDrvMixPending) == 0x10, "RxSndDrvMixPending");
static_assert(offsetof(RxSndDrvMixPending, m_vol) == 0x8, "RxSndDrvMixPending::m_vol");
static_assert(offsetof(RxSndDrvMixPending, m_input) == 0xc, "RxSndDrvMixPending::m_input");

/* FS2 0x2c per core, at 0x199e0 (RoR 0x20 at 0x195b0). */
struct RxSndDrvMix
{
    u_int              m_mix;           // IOP +0x00
    short              m_vol[2];        // IOP +0x04: L, R
    short              m_input[2];      // IOP +0x08: FS2: the external input's volume, L, R
    RxSndDrvMixPending m_pending[2];    // IOP +0x0c
};
static_assert(sizeof(RxSndDrvMix) == 0x2c, "RxSndDrvMix");
static_assert(offsetof(RxSndDrvMix, m_vol) == 0x4, "RxSndDrvMix::m_vol");
static_assert(offsetof(RxSndDrvMix, m_input) == 0x8, "RxSndDrvMix::m_input");
static_assert(offsetof(RxSndDrvMix, m_pending) == 0xc, "RxSndDrvMix::m_pending");

static RxSndDrvMix s_mix[2];            // FS2 IOP 0x000199e0

/* The master-volume and mix defaults (RoR IOP 0xdd70): MMIX 0xfcc, MVOL 0x3fff,
 * both cores. */
static sceSdBatch s_mixDefaults[6] = {
    { SD_BATCH_SETPARAM, SD_P_MMIX | SD_CORE_0, 0xfcc },
    { SD_BATCH_SETPARAM, SD_P_MMIX | SD_CORE_1, 0xfcc },
    { SD_BATCH_SETPARAM, SD_P_MVOLL | SD_CORE_0, 0x3fff },
    { SD_BATCH_SETPARAM, SD_P_MVOLR | SD_CORE_0, 0x3fff },
    { SD_BATCH_SETPARAM, SD_P_MVOLL | SD_CORE_1, 0x3fff },
    { SD_BATCH_SETPARAM, SD_P_MVOLR | SD_CORE_1, 0x3fff },
};

// IOP 0x00001a70 (RoR 0x000019e0, 88% of it aligned: FS2 read, the input volume)
void RxSndDrvMixTick(void)
{
    for (u_int core = 0; core < 2; core++)
    {
        RxSndDrvMix        *m = &s_mix[core];
        RxSndDrvMixPending *p = &m->m_pending[RxSndDrvTickBank()];

        if (p->m_mask & 1)
        {
            /* The ROM copies the twelve MMIX bits (0x001..0x800) one by one. */
            m->m_mix = p->m_mix;
            sceSdSetParam((u_short)(core | SD_P_MMIX), (u_short)(p->m_mix & 0xfff));
        }
        if (p->m_mask & 2)
        {
            m->m_vol[0] = p->m_vol[0];
            m->m_vol[1] = p->m_vol[1];
            sceSdSetParam((u_short)(core | SD_P_MVOLL), (u_short)p->m_vol[0]);
            sceSdSetParam((u_short)(core | SD_P_MVOLR), (u_short)m->m_vol[1]);
        }
        if (p->m_mask & 4)
        {
            m->m_input[0] = p->m_input[0];
            m->m_input[1] = p->m_input[1];
            sceSdSetParam((u_short)(core | SD_P_BVOLL), (u_short)p->m_input[0]);
            sceSdSetParam((u_short)(core | SD_P_BVOLR), (u_short)m->m_input[1]);
        }
        p->m_mask = 0;
    }
}

// IOP 0x00001c28 (RoR 0x00001b5c, 75% of it aligned: FS2 read, the input volume too)
void RxSndDrvMixInit(void)
{
    sceSdProcBatch(s_mixDefaults, NULL, 6);
    memset(s_mix, 0, sizeof(s_mix));
    for (int core = 0; core < 2; core++)
    {
        s_mix[core].m_mix    = 0xfcc;
        s_mix[core].m_vol[0] = 0x3fff;
        s_mix[core].m_vol[1] = 0x3fff;
        s_mix[core].m_input[0] = 0x7fff;
        s_mix[core].m_input[1] = 0x7fff;
    }
}

/* Command 0x31: {param, value} for `core`. */
// IOP 0x00001cb4 (RoR 0x00001bdc, 77% of it aligned: FS2 read, param 2)
void RxSndDrvMixSet(int core, const u_int *data)
{
    RxSndDrvMixPending *p = &s_mix[core].m_pending[RxSndDrvCmdBank()];
    int intr;

    CpuSuspendIntr(&intr);
    if (data[0] == 0)
    {
        p->m_mask |= 1;
        p->m_mix = data[1];
    }
    else if (data[0] == 1)
    {
        p->m_mask |= 2;
        memcpy(p->m_vol, &data[1], sizeof(p->m_vol));
    }
    else if (data[0] == 2)
    {
        p->m_mask |= 4;
        memcpy(p->m_input, &data[1], sizeof(p->m_input));
    }
    CpuResumeIntr(intr);
}
