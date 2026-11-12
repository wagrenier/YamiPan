/* ==========================================================================
 *  rx/rxsndut.c
 *
 *  The rx sound utility layer (FS2 0x162178..0x1622f8, data 0x2f5f10..
 *  0x2f5f58): the one entry point the game uses to bring up, step and take
 *  down the rxsndstr layers -- memory and file stream sources, XWS banks
 *  and the stream core -- and to register the two stream formats, PCM and
 *  ADPCM. It also keeps the layer's debug flags, which every rxsndstr file
 *  reads before printing. See rxsndut.h.
 *
 *  FS2-only (not in RoR, no symbols): the names were read from the code
 *  (batch 008); the file name comes from its own message
 *  "*** rxsndut : BG thread called! %s".
 * ======================================================================== */
#include "rx/rxsndut.h"

#include "rx/rxsnd.h"
#include "rx/rxsndstr.h"
#include "rx/rxsndstradpcm.h"
#include "rx/rxsndstrbank.h"
#include "rx/rxsndstrmem.h"
#include "rx/rxsndstrpcm.h"
#include "rx/rxsndstrstream.h"
#include "rx/rxsys.h"
#include "rx/rxthread.h"

/* ---- data ------------------------------------------------------------- */

static u_int s_debugFlags = RXSNDUT_DEBUG_DEFAULT;          /* data 0x2f5f10 */

/* The two formats' function tables (data 0x2f5f18 / 0x2f5f38, 0x20 each). */
static const RxSndStrFormat s_pcmFormat = {                 /* data 0x2f5f18 */
    rxSndStrPcmInit,      rxSndStrPcmExit,      rxSndStrPcmUpdate,
    rxSndStrPcmOpen,      rxSndStrPcmClose,     rxSndStrPcmGetHandle,
    rxSndStrPcmPauseRead, rxSndStrPcmSyncRead,
};
static const RxSndStrFormat s_adpcmFormat = {               /* data 0x2f5f38 */
    rxSndStrAdpcmInit,      rxSndStrAdpcmExit,      rxSndStrAdpcmUpdate,
    rxSndStrAdpcmOpen,      rxSndStrAdpcmClose,     rxSndStrAdpcmGetHandle,
    rxSndStrAdpcmPauseRead, rxSndStrAdpcmSyncRead,
};

/* ---- code ------------------------------------------------------------- */

// FS2 0x00162178
void rxSndUtInit(void)
{
    rxSndStrMemInit();
    rxSndStrStreamInit();
    rxSndStrBankInit();
    rxSndStrInit();
    rxSndSetDebugFlags(rxSndGetDebugFlags() & ~RXSND_DEBUG_SPUHEAP);
    rxSndStrRegisterFormat(RxSndStrFormat_Pcm, "PCM stream", &s_pcmFormat);
    rxSndStrRegisterFormat(RxSndStrFormat_Adpcm, "ADPCM stream", &s_adpcmFormat);
}

// FS2 0x001621f0
void rxSndUtExit(void)
{
    rxSndStrStreamExit();
    rxSndStrBankExit();
    rxSndStrExit();
    rxSndStrMemExit();
}

// FS2 0x00162220
void rxSndUtUpdate(void)
{
    rxSndStrMemUpdate();
    rxSndStrStreamUpdate();
    rxSndStrBankUpdate();
    rxSndStrUpdate();
}

// FS2 0x00162250
u_int rxSndUtSetDebugFlags(u_int flags)
{
    u_int old = s_debugFlags;

    s_debugFlags = flags;
    return old;
}

// FS2 0x00162268
u_int rxSndUtGetDebugFlags(void)
{
    return s_debugFlags;
}

// FS2 0x00162278
void rxSndUtCheckBgCall(const char *name)
{
    if (rxThIsYielding() == 1 && rxThIsRequestLocked() == 0 && rxThIsDispatchDisabled() == 0
        && (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_BGCALL))
        rxSysPrintf("*** rxsndut : BG thread called! %s\n", name);
}
