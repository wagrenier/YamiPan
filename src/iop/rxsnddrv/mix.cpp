/* ==========================================================================
 *  mix.cpp  (RXSNDDRV.IRX 0x19e0..0x1ca0: per-core master mix and volume)
 *
 *  Command 0x31 {core, param, value} (rx/rxsnd.c rxSndSetSysMix,
 *  rxSndSetSysVolume): param 0 the core's MMIX bits, 1 its master volume
 *  {L, R} as two halves of the value.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "intrman.h"
#include "libsd.h"

#include <string.h>

/* RoR IOP 0xc. */
struct RxSndDrvMixPending
{
    u_int m_mask;                       // IOP +0x0: 1 mix, 2 volume
    u_int m_mix;                        // IOP +0x4
    short m_vol[2];                     // IOP +0x8: L, R
};
static_assert(sizeof(RxSndDrvMixPending) == 0xc, "RxSndDrvMixPending");
static_assert(offsetof(RxSndDrvMixPending, m_vol) == 0x8, "RxSndDrvMixPending::m_vol");

/* RoR IOP 0x20 per core, at 0x195b0. */
struct RxSndDrvMix
{
    u_int              m_mix;           // IOP +0x00
    short              m_vol[2];        // IOP +0x04: L, R
    RxSndDrvMixPending m_pending[2];    // IOP +0x08
};
static_assert(sizeof(RxSndDrvMix) == 0x20, "RxSndDrvMix");
static_assert(offsetof(RxSndDrvMix, m_vol) == 0x4, "RxSndDrvMix::m_vol");
static_assert(offsetof(RxSndDrvMix, m_pending) == 0x8, "RxSndDrvMix::m_pending");

static RxSndDrvMix s_mix[2];            // RoR IOP 0x000195b0

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

// IOP 0x00001a70 (RoR 0x000019e0, 88% of it aligned: FS2 differs, unread)
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
        p->m_mask = 0;
    }
}

// IOP 0x00001c28 (RoR 0x00001b5c, 75% of it aligned: FS2 differs, unread)
void RxSndDrvMixInit(void)
{
    sceSdProcBatch(s_mixDefaults, NULL, 6);
    memset(s_mix, 0, sizeof(s_mix));
    for (int core = 0; core < 2; core++)
    {
        s_mix[core].m_mix    = 0xfcc;
        s_mix[core].m_vol[0] = 0x3fff;
        s_mix[core].m_vol[1] = 0x3fff;
    }
}

/* Command 0x31: {param, value} for `core`. */
// IOP 0x00001cb4 (RoR 0x00001bdc, 77% of it aligned: FS2 differs, unread)
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
    CpuResumeIntr(intr);
}
