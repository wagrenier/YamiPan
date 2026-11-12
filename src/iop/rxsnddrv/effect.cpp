/* ==========================================================================
 *  effect.cpp  (RXSNDDRV.IRX 0x1ca0..0x2330: per-core SPU2 reverb)
 *
 *  Command 0x32 {core, param, value} (rx/rxsnd.c rxSndSetEffectModeCore,
 *  rxSndSetEffectDepthCore ...): 0 mode (0..9, the SDK's SD_REV_MODE_*),
 *  1 work-area end address, 2 depth {L, R}, 3 feedback, 4 delay.
 *
 *  A new mode or end address restarts the core: effect off, mode 0, then the
 *  work area is zeroed by a queue-1 DMA (type 5) whose completion lets the
 *  tick go on; once it is done the depth ramps toward its target by at most
 *  8 a tick (the depth is kept as a byte-scale value, sent << 8), and every
 *  change rewrites the effect attributes and turns the effect on.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "intrman.h"
#include "libsd.h"

#include <string.h>

/* RoR IOP 0x14. */
struct RxSndDrvEffectPending
{
    u_int m_mask;                       // IOP +0x00: 2 mode, 1 end, 4 depth, 8 feedback, 0x10 delay
    int   m_mode;                       // IOP +0x04
    u_int m_end;                        // IOP +0x08
    short m_depth[2];                   // IOP +0x0c: L, R
    short m_feedback;                   // IOP +0x10
    short m_delay;                      // IOP +0x12
};
static_assert(sizeof(RxSndDrvEffectPending) == 0x14, "RxSndDrvEffectPending");
static_assert(offsetof(RxSndDrvEffectPending, m_depth) == 0xc, "RxSndDrvEffectPending::m_depth");
static_assert(offsetof(RxSndDrvEffectPending, m_delay) == 0x12, "RxSndDrvEffectPending::m_delay");

enum
{
    RXSNDDRV_EFFECT_START = 0,          /* restart: clear the work area */
    RXSNDDRV_EFFECT_CLEAR = 1,          /* waiting for the clear */
    RXSNDDRV_EFFECT_RUN   = 2,
};

/* RoR IOP 0x44 per core, at 0x195f0. */
struct RxSndDrvEffect
{
    int                   m_mode;       // IOP +0x00
    u_int                 m_end;        // IOP +0x04: SD_A_EEA
    short                 m_depth[2];   // IOP +0x08: L, R now
    short                 m_target[2];  // IOP +0x0c: L, R
    short                 m_feedback;   // IOP +0x10
    short                 m_delay;      // IOP +0x12
    int                   m_state;      // IOP +0x14: RXSNDDRV_EFFECT_*
    int                   m_clearDma;   // IOP +0x18: the clear's DMA id, -1 when done
    RxSndDrvEffectPending m_pending[2]; // IOP +0x1c
};
static_assert(sizeof(RxSndDrvEffect) == 0x44, "RxSndDrvEffect");
static_assert(offsetof(RxSndDrvEffect, m_target) == 0xc, "RxSndDrvEffect::m_target");
static_assert(offsetof(RxSndDrvEffect, m_state) == 0x14, "RxSndDrvEffect::m_state");
static_assert(offsetof(RxSndDrvEffect, m_clearDma) == 0x18, "RxSndDrvEffect::m_clearDma");
static_assert(offsetof(RxSndDrvEffect, m_pending) == 0x1c, "RxSndDrvEffect::m_pending");

/* The modes' SDK numbers and work-area sizes (RoR IOP 0xdda0). */
static const struct
{
    int m_mode;
    int m_size;
} s_effectModes[10] = {
    { 0, 0x80 },    { 1, 0x26c0 },  { 2, 0x1f40 },  { 3, 0x4840 },  { 4, 0x6fe0 },
    { 5, 0xade0 },  { 6, 0xf6c0 },  { 7, 0x18040 }, { 8, 0x18040 }, { 9, 0x3c00 },
};

/* Effect volumes 0, end addresses 0x1fffff / 0x1dffff, effect on (RoR IOP 0xddf0). */
static sceSdBatch s_effectDefaults[8] = {
    { SD_BATCH_SETPARAM, SD_P_EVOLL | SD_CORE_0, 0 },
    { SD_BATCH_SETPARAM, SD_P_EVOLR | SD_CORE_0, 0 },
    { SD_BATCH_SETPARAM, SD_P_EVOLL | SD_CORE_1, 0 },
    { SD_BATCH_SETPARAM, SD_P_EVOLR | SD_CORE_1, 0 },
    { SD_BATCH_SETADDR,  SD_A_EEA | SD_CORE_0,   0x1fffff },
    { SD_BATCH_SETADDR,  SD_A_EEA | SD_CORE_1,   0x1dffff },
    { SD_BATCH_SETCORE,  SD_C_EFFECT_ENABLE | SD_CORE_0, 1 },
    { SD_BATCH_SETCORE,  SD_C_EFFECT_ENABLE | SD_CORE_1, 1 },
};

static RxSndDrvEffect s_effect[2];      // RoR IOP 0x000195f0

static short ClampShort(int v)
{
    return (short)(v > 0x7fff ? 0x7fff : v < -0x8000 ? -0x8000 : v);
}

static int ClampByte(int v)
{
    return v > 0x7f ? 0x7f : v < 0 ? 0 : v;
}

/* One step of at most 8 from `now` toward `to`; answers whether it moved. */
static int Ramp(short *now, short to)
{
    int diff = to - *now;

    if (diff == 0)
        return 0;
    if (diff < 0)
        diff = -diff;
    if (diff > 8)
        diff = 8;
    *now += (short)(to < *now ? -diff : diff);
    return 1;
}

// IOP 0x0000243c (RoR 0x0000231c, word for word)
static void RxSndDrvEffectCleared(void *arg, int size)
{
    (void)size;
    ((RxSndDrvEffect *)arg)->m_clearDma = -1;
}

// IOP 0x00001dc0 (RoR 0x00001ca0, word for word)
void RxSndDrvEffectTick(void)
{
    for (u_int core = 0; core < 2; core++)
    {
        RxSndDrvEffect        *fx      = &s_effect[core];
        RxSndDrvEffectPending *p       = &fx->m_pending[RxSndDrvTickBank()];
        const u_int            mask    = p->m_mask;
        int                    start   = 0;
        int                    update  = 0;

        if (mask != 0)
        {
            if (mask & 2)
            {
                fx->m_mode  = p->m_mode;
                fx->m_state = RXSNDDRV_EFFECT_START;
            }
            if (mask & 1)
            {
                fx->m_state = RXSNDDRV_EFFECT_START;
                fx->m_end   = p->m_end;
            }
            if (mask & 4)
            {
                fx->m_target[0] = p->m_depth[0];
                fx->m_target[1] = p->m_depth[1];
            }
            if (mask & 0x10)
                fx->m_delay = p->m_delay;
            if (mask & 8)
                fx->m_feedback = p->m_feedback;
        }
        p->m_mask = 0;

        switch (fx->m_state)
        {
        case RXSNDDRV_EFFECT_START:
            start       = 1;
            fx->m_state = RXSNDDRV_EFFECT_CLEAR;
            break;
        case RXSNDDRV_EFFECT_CLEAR:
            if (fx->m_clearDma < 0)
            {
                update         = 1;
                fx->m_depth[0] = 0;
                fx->m_depth[1] = 0;
                fx->m_state    = RXSNDDRV_EFFECT_RUN;
            }
            break;
        case RXSNDDRV_EFFECT_RUN:
            update |= Ramp(&fx->m_depth[0], fx->m_target[0]);
            update |= Ramp(&fx->m_depth[1], fx->m_target[1]);
            if (mask != 0)
                update = 1;
            break;
        }

        if (start)
        {
            sceSdEffectAttrSdk off;
            int intr;

            memset(&off, 0, sizeof(off));
            off.core = core;
            sceSdSetCoreAttr((u_short)(core | SD_C_EFFECT_ENABLE), 0);
            sceSdSetAddr((u_short)(core | SD_A_EEA), fx->m_end);
            sceSdSetEffectAttr((int)core, &off);
            CpuSuspendIntr(&intr);
            if (fx->m_clearDma >= 0)
            {
                RxSndDrvDmaCancel(fx->m_clearDma);
                fx->m_clearDma = -1;
            }
            const int size = (s_effectModes[fx->m_mode].m_size + 0x3f) & ~0x3f;
            fx->m_clearDma = RxSndDrvDmaPush(1, RXSNDDRV_DMA_ZERO_SPU, fx->m_end - (size - 1), 0, size,
                                             RxSndDrvEffectCleared, fx);
            CpuResumeIntr(intr);
            RxSndDrvDmaKick();
        }
        if (update)
        {
            sceSdEffectAttrSdk attr;

            attr.core     = core;
            attr.mode     = (u_int)s_effectModes[fx->m_mode].m_mode;
            attr.depth_L  = ClampShort(fx->m_depth[0] << 8);
            attr.depth_R  = ClampShort(fx->m_depth[1] << 8);
            attr.delay    = ClampByte(fx->m_delay);
            attr.feedback = ClampByte(fx->m_feedback);
            sceSdSetAddr((u_short)(core | SD_A_EEA), fx->m_end);
            sceSdSetEffectAttr((int)core, &attr);
            sceSdSetCoreAttr((u_short)(core | SD_C_EFFECT_ENABLE), 1);
        }
    }
}

// IOP 0x000021e4 (RoR 0x000020c4, word for word)
void RxSndDrvEffectInit(void)
{
    memset(s_effect, 0, sizeof(s_effect));
    sceSdProcBatch(s_effectDefaults, NULL, 8);
    for (u_int core = 0; core < 2; core++)
    {
        RxSndDrvEffect    *fx = &s_effect[core];
        sceSdEffectAttrSdk off;

        fx->m_mode      = 0;
        fx->m_end       = 0x1fffff - core * 0x20000;
        fx->m_depth[0]  = 0;
        fx->m_depth[1]  = 0;
        fx->m_target[0] = 0;
        fx->m_target[1] = 0;
        fx->m_delay     = 0;
        fx->m_feedback  = 0;
        fx->m_state     = RXSNDDRV_EFFECT_RUN;
        fx->m_clearDma  = -1;
        memset(&off, 0, sizeof(off));
        off.core = core;
        sceSdSetCoreAttr((u_short)(core | SD_C_EFFECT_ENABLE), 0);
        sceSdSetAddr((u_short)(core | SD_A_EEA), fx->m_end);
        sceSdSetEffectAttr((int)core, &off);
    }
}

/* Command 0x32: {param, value} for `core`. */
// IOP 0x00002300 (RoR 0x000021e0, word for word)
void RxSndDrvEffectSet(int core, const u_int *data)
{
    int intr;

    CpuSuspendIntr(&intr);
    RxSndDrvEffectPending *p = &s_effect[core].m_pending[RxSndDrvCmdBank()];
    switch (data[0])
    {
    case 0:
        p->m_mask |= 2;
        p->m_mode = (int)data[1];
        break;
    case 1:
        p->m_mask |= 1;
        p->m_end = data[1];
        break;
    case 2:
        p->m_mask |= 4;
        memcpy(p->m_depth, &data[1], sizeof(p->m_depth));
        break;
    case 3:
        p->m_mask |= 8;
        p->m_feedback = (short)data[1];
        break;
    case 4:
        p->m_mask |= 0x10;
        p->m_delay = (short)data[1];
        break;
    }
    CpuResumeIntr(intr);
}
