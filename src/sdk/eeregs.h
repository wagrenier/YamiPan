// sdk/eeregs.h -- the EE registers FS2 touches. On the console these are memory-mapped; here they are
// host variables, and the timer counts objects that move by themselves (ee::Kernel's clock), so that
// `REG_RCNT0_COUNT = 0` and `x = REG_RCNT0_COUNT` read as the ROM wrote them.
#pragma once

#include "scetypes.h"

class EeTimerCount
{
public:
    explicit constexpr EeTimerCount(int timer) : m_timer(timer) {}
    operator u_int() const;
    u_int operator=(u_int value);

private:
    int m_timer;
};

inline EeTimerCount g_regRcnt0Count{0};
inline EeTimerCount g_regRcnt1Count{1};
inline volatile u_int g_regRcnt0Mode;
inline volatile u_int g_regRcnt1Mode;
inline volatile u_int g_regDStat;
inline volatile u_int g_regIpuToChcr;
inline volatile u_int g_regIpuToMadr;
inline volatile u_int g_regIpuToQwc;
inline volatile u_long g_regGsPmode;
inline volatile u_long g_regGsSmode2;
inline volatile u_long g_regGsDispfb1;
inline volatile u_long g_regGsDisplay1;
inline volatile u_long g_regGsDispfb2;
inline volatile u_long g_regGsDisplay2;
inline volatile u_long g_regGsBgcolor;
inline volatile u_long g_regGsCsr;

#define REG_RCNT0_COUNT g_regRcnt0Count
#define REG_RCNT1_COUNT g_regRcnt1Count
#define REG_RCNT0_MODE g_regRcnt0Mode
#define REG_RCNT1_MODE g_regRcnt1Mode
#define REG_DMAC_4_IPU_TO_CHCR g_regIpuToChcr
#define REG_DMAC_4_IPU_TO_MADR g_regIpuToMadr
#define REG_DMAC_4_IPU_TO_QWC g_regIpuToQwc
#define REG_GS_PMODE g_regGsPmode
#define REG_GS_SMODE2 g_regGsSmode2
#define REG_GS_DISPFB1 g_regGsDispfb1
#define REG_GS_DISPLAY1 g_regGsDisplay1
#define REG_GS_DISPFB2 g_regGsDispfb2
#define REG_GS_DISPLAY2 g_regGsDisplay2
#define REG_GS_BGCOLOR g_regGsBgcolor
#define REG_GS_CSR g_regGsCsr
#define D_STAT (&g_regDStat)
