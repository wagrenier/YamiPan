/* ==========================================================================
 *  sys.cpp  (RXSNDDRV.IRX 0x1680..0x19e0: system parameters)
 *
 *  Command 0x30 {param, value} (rx/rxsnd.c rxSndSetDebugFlags,
 *  rxSndSetFrameRate, rxSndSetOutputMode, rxSndSetSysParam):
 *    0 debug flags (RXSNDDRV_DBG_*), 1 frame rate (50 / 60), 2 output mode
 *    (passed on to the voices), 3..5 three bytes that make SPDIF_MODE:
 *    3 == 0 sets bit 0x800, 4 picks 0 -> 2 / 1 -> 0 / 2 -> 1 for the low
 *    bits, 5 == 1 sets 0x80 (the SDK's media / copy-protection / output
 *    mode bits of SD_C_SPDIF_MODE).
 *  Like every command it is written into the command bank and applied by
 *  the next tick.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "intrman.h"
#include "libsd.h"

#include <string.h>

/* What one bank holds until the tick.  RoR IOP 0x14. */
struct RxSndDrvSysPending
{
    u_int  m_mask;                      // IOP +0x00: 1 << param
    u_int  m_debugFlags;                // IOP +0x04
    int    m_frameRate;                 // IOP +0x08
    int    m_outputMode;                // IOP +0x0c
    u_char m_spdif[3];                  // IOP +0x10: params 3..5
};
static_assert(sizeof(RxSndDrvSysPending) == 0x14, "RxSndDrvSysPending");
static_assert(offsetof(RxSndDrvSysPending, m_outputMode) == 0xc, "RxSndDrvSysPending::m_outputMode");
static_assert(offsetof(RxSndDrvSysPending, m_spdif) == 0x10, "RxSndDrvSysPending::m_spdif");

/* RoR IOP 0x19570, 0x38 bytes cleared at init. */
static struct
{
    u_int              m_debugFlags;    // RoR IOP 0x00019570
    int                m_frameRate;     // RoR IOP 0x00019574
    int                m_outputMode;    // RoR IOP 0x00019578
    u_char             m_spdif[3];      // RoR IOP 0x0001957c
    RxSndDrvSysPending m_pending[2];    // RoR IOP 0x00019580
} s_sys;

static void SetSpdifMode(void)
{
    u_int attr = s_sys.m_spdif[0] == 0 ? 0x800 : 0;

    switch (s_sys.m_spdif[1])
    {
    case 0: attr |= 2; break;
    case 2: attr |= 1; break;
    }
    if (s_sys.m_spdif[2] == 1)
        attr |= 0x80;
    sceSdSetCoreAttr(SD_C_SPDIF_MODE, (u_short)attr);
}

// IOP 0x00001710 (RoR 0x00001680, word for word)
void RxSndDrvSysTick(void)
{
    RxSndDrvSysPending *p = &s_sys.m_pending[RxSndDrvTickBank()];

    if (p->m_mask & 0x1)
        s_sys.m_debugFlags = p->m_debugFlags;
    if (p->m_mask & 0x2)
        s_sys.m_frameRate = p->m_frameRate;
    if (p->m_mask & 0x4)
    {
        s_sys.m_outputMode = p->m_outputMode;
        RxSndDrvVoiceSetOutputMode(p->m_outputMode);
    }
    const int s0 = (p->m_mask & 0x08) != 0;
    if (s0)
        s_sys.m_spdif[0] = p->m_spdif[0];
    const int s1 = (p->m_mask & 0x10) != 0;
    if (s1)
        s_sys.m_spdif[1] = p->m_spdif[1];
    const int s2 = (p->m_mask & 0x20) != 0;
    if (s2)
        s_sys.m_spdif[2] = p->m_spdif[2];
    if (s0 || s1 || s2)
        SetSpdifMode();
    p->m_mask = 0;
}

// IOP 0x000018a4 (RoR 0x00001814, word for word)
void RxSndDrvSysInit(void)
{
    memset(&s_sys, 0, sizeof(s_sys));
    s_sys.m_debugFlags = RXSNDDRV_DBG_ERROR | 0x2 | RXSNDDRV_DBG_INFO;
    s_sys.m_frameRate  = 60;
    s_sys.m_outputMode = 1;
    s_sys.m_spdif[0]   = 1;
    s_sys.m_spdif[1]   = 1;
    s_sys.m_spdif[2]   = 1;
    sceSdSetCoreAttr(SD_C_SPDIF_MODE, 0x80);
}

// IOP 0x00001910 (RoR 0x00001880, word for word)
int RxSndDrvGetFrameRate(void)
{
    return s_sys.m_frameRate;
}

// IOP 0x00001920 (RoR 0x00001890, word for word)
u_int RxSndDrvGetDebugFlags(void)
{
    return s_sys.m_debugFlags;
}

// IOP 0x00001930 (RoR 0x000018a0, word for word)
int RxSndDrvGetOutputMode(void)
{
    return s_sys.m_outputMode;
}

/* Command 0x30 {param, value}. */
// IOP 0x00001940 (RoR 0x000018b0, word for word)
void RxSndDrvSysSet(const u_int *data)
{
    RxSndDrvSysPending *p = &s_sys.m_pending[RxSndDrvCmdBank()];
    int intr;

    CpuSuspendIntr(&intr);
    switch (data[0])
    {
    case 0:
        p->m_mask |= 0x1;
        p->m_debugFlags = data[1];
        break;
    case 1:
        p->m_mask |= 0x2;
        p->m_frameRate = (int)data[1];
        break;
    case 2:
        p->m_mask |= 0x4;
        p->m_outputMode = (int)data[1];
        break;
    case 3:
    case 4:
    case 5:
        p->m_mask |= 0x8 << (data[0] - 3);
        p->m_spdif[data[0] - 3] = (u_char)data[1];
        break;
    }
    CpuResumeIntr(intr);
}
