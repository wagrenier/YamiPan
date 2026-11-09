/* ==========================================================================
 *  rx/rxsnd.c
 *
 *  The rx sound library: the EE client of RXSNDDRV.IRX (RPC 0xd000). See
 *  rxsnd.h for the command families. FS2 links Rule of Rose's prebuilt rx:
 *  this file is RosePan's src/rx/rxsnd.cpp (every function written there,
 *  checked against RoR) carried over, with FS2's own differences, each
 *  marked "FS2:" (rxsnd.h lists them). The 215 functions FS2 shares with RoR
 *  were compared word for word in the two ELFs (addresses and gp / lui
 *  offsets masked): identical. All FS2 data is RoR's moved by one offset
 *  (bss -0x2b7bc0); the data words are listed one by one.
 *
 *  PC: the IOP side is the reconstructed driver (src/iop/rxsnddrv/, RosePan's,
 *  with the platform layer), started at the IOP boot; without it
 *  rxSndRpcInit would bind for ever. The RPC completes on the caller's
 *  thread, so rxSndRpcEndFunc signals the semaphore before sceSifCallRpc
 *  returns, as the interrupt did.
 * ======================================================================== */
#include "rx/rxsnd.h"

#include "rx/rxsys.h"
#include "rx/rxthread.h"
#include "iop/rxsnddrv/rxsnddrv.h"
#include "sdk/eekernel.h"
#include "sdk/sifman.h"
#include "sdk/sifrpc.h"

#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- data ------------------------------------------------------------- */

static int             s_bufferBank;               /* data 0x2f5b60 */
static RxSndBgCallHook s_bgCallHook;               /* data 0x2f5e80 */
static RxSndAllocFunc  s_iopAllocHook      = rxSndDefIopAlloc;      /* data 0x2f5e88 */
static RxSndAllocFunc  s_iopAllocMode1Hook = rxSndDefIopAllocMode1; /* data 0x2f5e8c */
static RxSndFreeFunc   s_iopFreeHook       = rxSndDefIopFree;       /* data 0x2f5e90 */
static RxSndAllocFunc  s_spuAllocHook      = rxSndDefSpuAlloc;      /* data 0x2f5e98 */
static RxSndAllocFunc  s_spuAllocMode1Hook = rxSndDefSpuAllocMode1; /* data 0x2f5e9c */
static RxSndFreeFunc   s_spuFreeHook       = rxSndDefSpuFree;       /* data 0x2f5ea0 */
static int             s_rpcSema = -1;             /* data 0x2f5f00 */
static int             s_queuePos;                 /* data 0x2f5f04: bytes queued */
static int             s_queueReplyPos;            /* data 0x2f5f08: reply bytes reserved */

static RxSndConfig s_config;                       /* bss 0x4fdf00 */
static u_char      s_workBuffer[0x1000];           /* bss 0x4fdf40 */
/* PC: aligned as on the EE (0x4fef40): rxSndIopRead's bounce buffers must be
 * 16-byte aligned or its unaligned path recurses for ever. */
alignas(16) static RxSndIopBuf s_iopBuf;           /* bss 0x4fef40 */

static RxSndSpuNode   s_spuNodes[256];             /* bss 0x4fefc0 */
static RxSndSpuNode   s_spuList;                   /* bss 0x5007c0: sentinel of the block list */
static RxSndSpuNode  *s_spuFreeNodes;              /* bss 0x5007d8 */
static u_int          s_spuHeapSize;               /* bss 0x5007dc */
static RxSndSpuSlot   s_spuSlots[256];             /* bss 0x5007e0 */
static RxSndSpuRegion s_spuRegion;                 /* bss 0x501be0 */
static RxSndXvab      s_xvab[256];                 /* bss 0x501c00 */
static RxSndXseq      s_xseq[256];                 /* bss 0x503400 */

static RxSndBgm  s_bgm[32];                        /* bss 0x504000 */
static RxSndBgm *s_bgmActive;                      /* bss 0x504300 */
static RxSndBgm *s_bgmStopping;                    /* bss 0x504304: stopped, the IOP still lists it */
static RxSndBgm *s_bgmFree;                        /* bss 0x504308 */
static short     s_bgmMasterVolL;                  /* bss 0x50430c */
static short     s_bgmMasterVolR;                  /* bss 0x50430e */

static RxSndSeVoice  s_se[64];                     /* bss 0x504310 */
static RxSndSeVoice *s_seActive;                   /* bss 0x504810 */
static RxSndSeVoice *s_seStopping;                 /* bss 0x504814 */
static RxSndSeVoice *s_seFree;                     /* bss 0x504818 */
static short         s_seMasterVolL;               /* bss 0x50481c */
static short         s_seMasterVolR;               /* bss 0x50481e */
static int           s_seParamB;                   /* bss 0x504820: rxSndSePlay's default, -1 none */
static int           s_seParamC;                   /* bss 0x504824: rxSndSePlay's default, -1 none */

static RxSndPosCallback  s_pcmCallbacks[256];      /* bss 0x504840 */
static RxSndPosCallback *s_pcmCallbackFree;        /* bss 0x505c40 */
static RxSndPcm          s_pcm[32];                /* bss 0x505c44 */
static RxSndPcm         *s_pcmActive;              /* bss 0x5063c4 */
static RxSndPcm         *s_pcmStopping;            /* bss 0x5063c8 */
static RxSndPcm         *s_pcmFree;                /* bss 0x5063cc */
static short             s_pcmMasterVolL;          /* bss 0x5063d0 */
static short             s_pcmMasterVolR;          /* bss 0x5063d2 */
/* rxSndPcmEnd's mode 3 padding: 0x400 copies of the stream's attr word. */
static short             s_pcmPad[0x800];          /* bss 0x506400 */

static RxSndPosCallback  s_adpcmCallbacks[256];    /* bss 0x507400 */
static RxSndPosCallback *s_adpcmCallbackFree;      /* bss 0x508800 */
static RxSndAdpcm        s_adpcm[48];              /* bss 0x508808 */
static RxSndAdpcm       *s_adpcmActive;            /* bss 0x509708 */
static RxSndAdpcm       *s_adpcmStopping;          /* bss 0x50970c */
static RxSndAdpcm       *s_adpcmFree;              /* bss 0x509710 */
static short             s_adpcmMasterVolL;        /* bss 0x509714 */
static short             s_adpcmMasterVolR;        /* bss 0x509716 */
static int               s_adpcmParam6;            /* bss 0x509718: rxSndAdpcmOpenBytes's default, 0 none */
static int               s_adpcmParam7;            /* bss 0x50971c: rxSndAdpcmOpenBytes's default, 0 none */
/* rxSndAdpcmWriteEx copies each chunk here (channel 0, 1) to clear and set
 * the blocks' loop flags before sending it to the IOP ring. */
static u_char            s_adpcmStage[2][0x4000];  /* bss 0x509740, 0x50d740 */
/* rxSndAdpcmEnd's end block: four ADPCM blocks of silence, the first with
 * the end flag (1). */
static const u_int       s_adpcmEndBlock[16] = {   /* data 0x2f5ec0 */
    0x00000100, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

/* rxSndCentToPitch's ratios as float bits: 2^((n - 48) / 12) for the
 * semitones -48..48, 2^(c / 1200) for the cents 0..99. */
static const u_int s_semitoneRatio[97] = {         /* data 0x2f5b68 */
    0x3d800000, 0x3d879c41, 0x3d8face6, 0x3d9837b4, 0x3da1450e, 0x3daadbc6, 0x3db504c4, 0x3dbfc86c,
    0x3dcb302f, 0x3dd744f5, 0x3de411c2, 0x3df1a198, 0x3e000000, 0x3e079c84, 0x3e0face6, 0x3e1837f7,
    0x3e21450e, 0x3e2adc09, 0x3e350507, 0x3e3fc86c, 0x3e4b2fec, 0x3e5744f5, 0x3e641205, 0x3e71a1db,
    0x3e800000, 0x3e879c84, 0x3e8face6, 0x3e9837f7, 0x3ea1450e, 0x3eaadc09, 0x3eb504e6, 0x3ebfc88e,
    0x3ecb2fec, 0x3ed744f5, 0x3ee411e4, 0x3ef1a1b9, 0x3f000000, 0x3f079c84, 0x3f0facd5, 0x3f1837f7,
    0x3f21451f, 0x3f2adc09, 0x3f3504f6, 0x3f3fc88e, 0x3f4b2ffd, 0x3f5744f5, 0x3f6411f4, 0x3f71a1b9,
    0x3f800000, 0x3f879c7b, 0x3f8facd5, 0x3f9837ef, 0x3fa14517, 0x3faadc09, 0x3fb504f6, 0x3fbfc886,
    0x3fcb2ff4, 0x3fd744fe, 0x3fe411ec, 0x3ff1a1c2, 0x40000000, 0x40079c7b, 0x400facd5, 0x401837ef,
    0x40214517, 0x402adc09, 0x403504f2, 0x403fc886, 0x404b2ff4, 0x405744fe, 0x406411f0, 0x4071a1be,
    0x40800000, 0x40879c7b, 0x408facd5, 0x409837ef, 0x40a14517, 0x40aadc07, 0x40b504f2, 0x40bfc886,
    0x40cb2ff4, 0x40d744fc, 0x40e411f0, 0x40f1a1c0, 0x41000000, 0x41079c7c, 0x410facd5, 0x411837f0,
    0x41214517, 0x412adc08, 0x413504f2, 0x413fc887, 0x414b2ff4, 0x415744fd, 0x416411ef, 0x4171a1bf,
    0x41800000,
};
static const u_int s_centRatio[100] = {            /* data 0x2f5cf0 */
    0x3f800000, 0x3f8012f0, 0x3f8025e1, 0x3f8038d1, 0x3f804bca, 0x3f805ec3, 0x3f8071c5, 0x3f8084c6,
    0x3f8097c8, 0x3f80aac9, 0x3f80bdd3, 0x3f80d0dc, 0x3f80e3ef, 0x3f80f6f8, 0x3f810a13, 0x3f811d25,
    0x3f813040, 0x3f81435a, 0x3f815675, 0x3f816998, 0x3f817cbb, 0x3f818fe6, 0x3f81a311, 0x3f81b63d,
    0x3f81c968, 0x3f81dc9c, 0x3f81efd8, 0x3f82030c, 0x3f821648, 0x3f822984, 0x3f823cc8, 0x3f82500d,
    0x3f826351, 0x3f827696, 0x3f8289e3, 0x3f829d38, 0x3f82b085, 0x3f82c3da, 0x3f82d72f, 0x3f82ea8d,
    0x3f82fdeb, 0x3f831148, 0x3f8324af, 0x3f833815, 0x3f834b7b, 0x3f835ee9, 0x3f837258, 0x3f8385c6,
    0x3f83993d, 0x3f83acb4, 0x3f83c02a, 0x3f83d3aa, 0x3f83e729, 0x3f83faa8, 0x3f840e30, 0x3f8421b7,
    0x3f843547, 0x3f8448cf, 0x3f845c67, 0x3f846ff7, 0x3f848390, 0x3f849728, 0x3f84aac9, 0x3f84be61,
    0x3f84d20a, 0x3f84e5ab, 0x3f84f954, 0x3f850cfe, 0x3f8520af, 0x3f853461, 0x3f854812, 0x3f855bcc,
    0x3f856f86, 0x3f858340, 0x3f859702, 0x3f85aac5, 0x3f85be87, 0x3f85d252, 0x3f85e61c, 0x3f85f9e7,
    0x3f860dba, 0x3f86218d, 0x3f863569, 0x3f864944, 0x3f865d20, 0x3f8670fb, 0x3f8684df, 0x3f8698c3,
    0x3f86acaf, 0x3f86c09c, 0x3f86d488, 0x3f86e87d, 0x3f86fc71, 0x3f871066, 0x3f872463, 0x3f873860,
    0x3f874c5d, 0x3f876063, 0x3f877468, 0x3f87886d,
};

/* The direct commands' buffers, one bank per rxTh yield state. */
static u_int            s_rpcSend[2][0x100];       /* bss 0x511740 */
static u_int            s_rpcReply[2][0x100];      /* bss 0x511f40 */
static sceSifClientData s_rpcClient;               /* bss 0x512740 */
/* FS2: rxSndQueueBegin flushes early enough that the end marker and the
 * flush's rounded size stay inside (RoR's ran over; RosePan's is 0x1040). */
static u_char           s_queue[0x1000];           /* bss 0x512780 */
static u_char           s_queueReply[0x1000];      /* bss 0x513780 */

static inline int rxSndClamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float rxSndBitsToFloat(u_int bits)
{
    float f;

    memcpy(&f, &bits, sizeof(f));
    return f;
}

/* ---- init / term ------------------------------------------------------ */

// FS2 0x00159bc0
void rxSndThreadCallback(int yielding)
{
    rxSndSetBufferBank(yielding);
}

// FS2 0x00159bd8
void rxSndCheckBgCall(const char *func)
{
    if (rxThIsYielding() == 1 && rxThIsRequestLocked() == 0 && rxThIsDispatchDisabled() == 0
        && (rxSndGetDebugFlags() & 2))
        rxSysPrintf("*** rxsndut : BG thread called! %s\n", func);
}

// FS2 0x00159c58
void rxSndInit(void)
{
    rxSndInitModules();
    rxThAddCallback(rxSndThreadCallback);
    rxSndSetBgCallHook(rxSndCheckBgCall);
}

// FS2 0x00159c88
void rxSndTerm(void)
{
    rxThRemoveCallback(rxSndThreadCallback);
    rxSndTermModules();
}

// FS2 0x00159ca8
void rxSndInitModules(void)
{
    rxSndRpcInit();
    rxSndInitConfig();
    rxSndIopBufInit();
    rxSndSpuHeapInit();
    rxSndXvabInit();
    rxSndXseqInit();
    rxSndBgmInit();
    rxSndSeInit();
    rxSndPcmInit();
    rxSndAdpcmInit();
}

// FS2 0x00159d08
void rxSndTermModules(void)
{
    /* Command 0x11 until the IOP reports it has stopped everything. */
    while (rxSndRpcCommand(0x11, NULL, 0, NULL, 0) != 0)
        ;
    rxSndAdpcmTerm();
    rxSndPcmTerm();
    rxSndSeTerm();
    rxSndBgmTerm();
    rxSndXseqTerm();
    rxSndXvabTerm();
    rxSndSpuHeapTerm();
    rxSndIopBufTerm();
    rxSndTermConfig();
    rxSndRpcTerm();
}

// FS2 0x00159d88
void rxSndUpdate(void)
{
    rxSndQueueFlush();

    rxSndQueueBegin(0x72, sizeof(RxSndBgmStatus), 0);
    const RxSndBgmStatus *bgm = (const RxSndBgmStatus *)rxSndQueueGetReply();
    rxSndQueueEnd();
    rxSndQueueBegin(0x83, sizeof(RxSndSeStatus), 0);
    const RxSndSeStatus *se = (const RxSndSeStatus *)rxSndQueueGetReply();
    rxSndQueueEnd();
    rxSndQueueBegin(0xb3, sizeof(RxSndPcmStatus), 0);
    const RxSndPcmStatus *pcm = (const RxSndPcmStatus *)rxSndQueueGetReply();
    rxSndQueueEnd();
    rxSndQueueBegin(0xa3, sizeof(RxSndAdpcmStatus), 0);
    const RxSndAdpcmStatus *adpcm = (const RxSndAdpcmStatus *)rxSndQueueGetReply();
    rxSndQueueEnd();

    rxSndQueueFlush();
    rxSndBgmUpdate(bgm);
    rxSndSeUpdate(se);
    rxSndPcmUpdate(pcm);
    rxSndAdpcmUpdate(adpcm);
}

// FS2 0x00159e68
void rxSndSetOutputMode(int mode)
{
    u_int req[2];

    req[0] = 2;
    req[1] = mode != 0;
    rxSndRpcCommand(0x30, NULL, 0, req, sizeof(req));
}

// FS2 0x00159eb8
u_char rxSndSetDebugFlags(int flags)
{
    u_char old = s_config.m_debugFlags;
    u_int  req[2];

    s_config.m_debugFlags = (u_char)flags;
    req[0] = 0;
    req[1] = flags;
    rxSndRpcCommand(0x30, NULL, 0, req, sizeof(req));
    return old;
}

// FS2 0x00159f10
u_char rxSndGetDebugFlags(void)
{
    return s_config.m_debugFlags;
}

// FS2 0x00159f20
void rxSndSetFrameRate(int rate)
{
    u_int req[2];

    req[0] = 1;
    req[1] = rate == 50 ? 50 : 60;
    rxSndRpcCommand(0x30, NULL, 0, req, sizeof(req));
}

// FS2 0x00159f78
void rxSndSetSysParam(int kind, int value)
{
    u_int req[2];

    switch (kind)
    {
    case 0:
        req[0] = 3;
        break;
    case 1:
        req[0] = 4;
        break;
    case 2:
        req[0] = 5;
        break;
    default:
        return;
    }
    req[1] = value;
    rxSndRpcCommand(0x30, NULL, 0, req, sizeof(req));
}

// FS2 0x00159ff8
void rxSndSetSysVolume(int id, int volL, int volR)
{
    rxSndQueueBegin(0x31, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_vol[1] = (short)volR;
    cmd->m_id     = id;
    cmd->m_vol[0] = (short)volL;
    cmd->m_param  = 1;
    rxSndQueueEnd();
}

// FS2 0x0015a060
void rxSndSetMasterVolume(int volL, int volR)
{
    volL = rxSndClamp(volL, -0x80, 0x7f);
    volR = rxSndClamp(volR, -0x80, 0x7f);
    s_config.m_masterVolR = (short)volR;
    s_config.m_masterVolL = (short)volL;
    rxSndBgmSetMasterVolume(rxSndBgmGetMasterVolumeL(), rxSndBgmGetMasterVolumeR());
    rxSndSeSetMasterVolume(rxSndSeGetMasterVolumeL(), rxSndSeGetMasterVolumeR());
    rxSndPcmSetMasterVolume(rxSndPcmGetMasterVolumeL(), rxSndPcmGetMasterVolumeR());
    rxSndAdpcmSetMasterVolume(rxSndAdpcmGetMasterVolumeL(), rxSndAdpcmGetMasterVolumeR());
}

// FS2 0x0015a140
int rxSndSetBufferBank(int bank)
{
    int old = s_bufferBank;

    if (bank < 0)
        s_bufferBank = 0;
    else if (bank < 2)
        s_bufferBank = bank;
    else
        s_bufferBank = 1;
    return old;
}

// FS2 0x0015a178
void rxSndGetVoiceTable(u_int *out)
{
    u_int reply[48];

    rxSndRpcCommand(0x90, reply, sizeof(reply), NULL, 0);
    for (int i = 0; i < 48; i++)
        out[i] = reply[i];
}

// FS2 0x0015a1d8
void rxSndGetVoiceTableEx(int arg, u_int *out)
{
    u_int reply[48];
    u_int req[1];

    req[0] = arg;
    rxSndRpcCommand(0x93, reply, sizeof(reply), req, sizeof(req));
    for (int i = 0; i < 48; i++)
        out[i] = reply[i];
}

// FS2 0x0015a238
void rxSndSetVoiceEntry(u_int voice, int value)
{
    if (voice < 48)
    {
        u_int req[2];

        req[0] = voice;
        req[1] = value;
        rxSndRpcCommand(0x91, NULL, 0, req, sizeof(req));
    }
}

// FS2 0x0015a280
void rxSndGetVoiceTable2(u_int *out)
{
    u_int reply[48];

    rxSndRpcCommand(0x92, reply, sizeof(reply), NULL, 0);
    for (int i = 0; i < 48; i++)
        out[i] = reply[i];
}

// FS2 0x0015a2e0
float rxSndCentToPitch(int cents)
{
    int v = rxSndClamp(cents, -4800, 4800) + 4800;

    return rxSndBitsToFloat(s_semitoneRatio[v / 100]) * rxSndBitsToFloat(s_centRatio[v % 100]);
}

// FS2 0x0015a350
void rxSndInitConfig(void)
{
    s_config.m_masterVolR = 0x80;
    s_config.m_debugFlags = 7;
    s_config.m_1          = 1;
    s_config.m_2          = 0x3c;
    s_config.m_masterVolL = 0x80;
}

// FS2 0x0015a380
void rxSndTermConfig(void)
{
}

// FS2 0x0015a390
int rxSndGetBufferBank(void)
{
    return s_bufferBank;
}

// FS2 0x0015a3a0
short rxSndGetMasterVolumeL(void)
{
    return s_config.m_masterVolL;
}

// FS2 0x0015a3b0
short rxSndGetMasterVolumeR(void)
{
    return s_config.m_masterVolR;
}

// FS2 0x0015a3c0
void *rxSndGetWorkBuffer(void)
{
    return s_workBuffer;
}

// FS2 0x0015a3d0
void rxSndWait(void)
{
}

// FS2 0x0015a3d8
void rxSndSetBgCallHook(RxSndBgCallHook hook)
{
    s_bgCallHook = hook;
}

// FS2 0x0015a3e8
void rxSndCallBgCallHook(const char *func)
{
    if (s_bgCallHook != NULL)
        s_bgCallHook(func);
}

/* ---- IOP / SPU memory ------------------------------------------------- */

// FS2 0x0015a410
void rxSndMemMove(void *dst, const void *src, int size)
{
    u_char       *d = (u_char *)dst;
    const u_char *s = (const u_char *)src;

    if (d < s)
    {
        for (int i = 0; i < size; i++)
            *d++ = *s++;
    }
    else
    {
        d += size;
        s += size;
        for (; size > 0; size--)
            *--d = *--s;
    }
}

/* Command 0x20 {mode, size, 0}: the IOP address, 0 when the heap is full. */
// FS2 0x0015a490
u_int rxSndDefIopAlloc(int size)
{
    u_int req[3];
    u_int bytes = (size + 0x3f) & ~0x3f;

    req[0] = 0;
    req[1] = bytes;
    req[2] = 0;
    u_int addr = rxSndRpcCommand(0x20, NULL, 0, req, sizeof(req));
    if (addr == 0)
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : can't allocate IOP heap '%d'\n", bytes);
        addr = 0;
    }
    return addr;
}

// FS2 0x0015a508
u_int rxSndDefIopAllocMode1(int size)
{
    u_int req[3];
    u_int bytes = (size + 0x3f) & ~0x3f;

    req[0] = 1;
    req[1] = bytes;
    req[2] = 0;
    u_int addr = rxSndRpcCommand(0x20, NULL, 0, req, sizeof(req));
    if (addr == 0)
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : can't allocate IOP heap '%d'\n", bytes);
        addr = 0;
    }
    return addr;
}

// FS2 0x0015a588
void rxSndDefIopFree(u_int addr)
{
    u_int req[1];

    req[0] = addr;
    rxSndRpcCommand(0x21, NULL, 0, req, sizeof(req));
}

// FS2 0x0015a5b8
void rxSndSetIopAllocHook(RxSndAllocFunc func)
{
    s_iopAllocHook = func != NULL ? func : rxSndDefIopAlloc;
}

// FS2 0x0015a5d8
void rxSndSetIopAllocMode1Hook(RxSndAllocFunc func)
{
    s_iopAllocMode1Hook = func != NULL ? func : rxSndDefIopAllocMode1;
}

// FS2 0x0015a5f8
void rxSndSetIopFreeHook(RxSndFreeFunc func)
{
    s_iopFreeHook = func != NULL ? func : rxSndDefIopFree;
}

// FS2 0x0015a618
u_int rxSndIopAlloc(int size)
{
    return s_iopAllocHook(size);
}

// FS2 0x0015a640
u_int rxSndIopAllocMode1(int size)
{
    return s_iopAllocMode1Hook(size);
}

// FS2 0x0015a668
void rxSndIopFree(u_int addr)
{
    s_iopFreeHook(addr);
}

/* PC: an IOP address is a host address in the IOP's memory (sdk/sif.cpp), so
 * sceSifSetDma copies. */
// FS2 0x0015a690
int rxSndIopWrite(u_int iopAddr, const void *src, int size)
{
    uintptr_t from = (uintptr_t)src;
    u_int     mis  = (u_int)(from & 0xf);
    u_int     head;

    if (mis == 0)
    {
        head = 0;
        if ((size & 0xf) == 0 && (iopAddr & 3) == 0 && (size & 3) == 0)
        {
            sceSifDmaData dma;

            FlushCache(0);
            dma.data = (void *)src;
            dma.addr = (void *)(uintptr_t)iopAddr;
            dma.size = size;
            dma.mode = 0;
            u_int id = sceSifSetDma(&dma, 1);
            while (sceSifDmaStat(id) >= 0)
                ;
            FlushCache(0);
            return 1;
        }
    }
    else
    {
        head = 0u - mis;
    }
    head &= 0xf;

    /* The unaligned head: the 0x20 bytes around it go to the bounce buffer,
     * the IOP copies the part that belongs to iopAddr. */
    int rest = size;
    if (size > 0 && mis != 0)
    {
        int n = size <= (int)head ? size : (int)head;
        rest  = size - n;
        rxSndIopWrite(s_iopBuf.m_buf[rxSndGetBufferBank()], (const void *)(from - mis), 0x20);
        rxSndIopCopy(iopAddr, s_iopBuf.m_buf[rxSndGetBufferBank()] + mis, n);
    }

    /* The aligned middle, sent to a 4-aligned IOP address and moved down
     * when iopAddr is not. */
    if (rest > 15)
    {
        u_int pad = (0u - (iopAddr & 3)) & 3;
        int   mid = rest - (int)pad;
        if (mid < 0)
            mid = 0;
        mid &= ~0xf;
        if (mid > 0)
        {
            u_int tmp = iopAddr + head + pad;
            rxSndIopWrite(tmp, (const void *)(from + head), mid);
            if (pad != 0)
                rxSndIopCopy(iopAddr + head, tmp, mid);
            rest -= mid;
        }
    }

    /* The tail through the bounce buffer. */
    if (rest > 0)
    {
        rxSndIopWrite(s_iopBuf.m_buf[rxSndGetBufferBank()], (const u_char *)src + size - rest,
                      (rest + 0xf) & ~0xf);
        rxSndIopCopy(iopAddr + size - rest, s_iopBuf.m_buf[rxSndGetBufferBank()], rest);
    }
    return 1;
}

/* The mirror of rxSndIopWrite: the unaligned head and tail come through the
 * EE bounce buffer; the middle is read 16-byte aligned (the pad from `dst`'s
 * own alignment, as the ROM computes it) and moved into place.
 * PC: an IOP address is a host address in the IOP's memory (sdk/sif.cpp), so
 * sceSifGetOtherData copies. */
// FS2 0x0015a8a0
int rxSndIopRead(void *dst, u_int iopAddr, int size)
{
    uintptr_t to  = (uintptr_t)dst;
    u_int     mis = iopAddr & 3;
    u_int     head;

    if ((to & 0xf) == 0)
    {
        head = 0u - mis;
        if ((size & 0xf) == 0 && (iopAddr & 3) == 0)
        {
            if ((size & 3) == 0)
            {
                sceSifReceiveData rd;

                FlushCache(0);
                while (sceSifGetOtherData(&rd, (void *)(uintptr_t)iopAddr, dst, size, 0) < 0)
                    ;
                FlushCache(0);
                return 1;
            }
            head = 0;
        }
    }
    else
    {
        head = 0u - mis;
    }
    head &= 3;

    int rest = size;
    if (size > 0 && head != 0)
    {
        int n = size <= (int)head ? size : (int)head;
        rxSndIopRead(s_iopBuf.m_eeBuf[rxSndGetBufferBank()], iopAddr - mis, 0x20);
        rest = size - n;
        rxSndMemMove(dst, s_iopBuf.m_eeBuf[rxSndGetBufferBank()] + mis, n);
    }

    if (rest > 3)
    {
        u_int pad = (0u - (u_int)(to & 0xf)) & 0xf;
        int   mid = rest - (int)pad;
        if (mid < 0)
            mid = 0;
        mid &= ~0xf;
        u_char *at = (u_char *)dst + head;
        if (mid > 0)
        {
            u_char *tmp = at + pad;
            rxSndIopRead(tmp, iopAddr + head, mid);
            if (pad != 0)
                rxSndMemMove(at, tmp, mid);
            rest -= mid;
        }
    }

    if (rest > 0)
    {
        rxSndIopRead(s_iopBuf.m_eeBuf[rxSndGetBufferBank()], iopAddr + size - rest,
                     (rest + 0xf) & ~0xf);
        rxSndMemMove((u_char *)dst + size - rest, s_iopBuf.m_eeBuf[rxSndGetBufferBank()], rest);
    }
    return 1;
}

// FS2 0x0015aa88
int rxSndIopWriteWorkBuffer(u_int iopAddr, int size)
{
    while (size > 0)
    {
        int n = size <= 0x1000 ? size : 0x1000;
        size -= n;
        rxSndIopWrite(iopAddr, rxSndGetWorkBuffer(), n);
    }
    return 1;
}

// FS2 0x0015ab00
u_int rxSndIopAllocWrite(const void *src, int size)
{
    u_int addr = rxSndIopAlloc(size);

    if (addr != 0)
        rxSndIopWrite(addr, src, size);
    return addr;
}

// FS2 0x0015ab58
void rxSndIopHeapCmd1(void)
{
    u_int req[1];

    req[0] = 1;
    rxSndRpcCommand(0x22, NULL, 0, req, sizeof(req));
}

// FS2 0x0015ab90
void rxSndIopHeapCmd2(void)
{
    u_int req[1];

    req[0] = 2;
    rxSndRpcCommand(0x22, NULL, 0, req, sizeof(req));
}

// FS2 0x0015abc8
void rxSndIopCopy(u_int dst, u_int src, int size)
{
    u_int req[3];

    req[0] = dst;
    req[1] = src;
    req[2] = size;
    rxSndRpcCommand(0x23, NULL, 0, req, sizeof(req));
}

// FS2 0x0015ac00
void rxSndIopBufInit(void)
{
    memset(&s_iopBuf, 0, sizeof(s_iopBuf));
    s_iopBuf.m_buf[0] = rxSndIopAlloc(0x20);
    s_iopBuf.m_buf[1] = rxSndIopAlloc(0x20);
}

// FS2 0x0015ac50
void rxSndIopBufTerm(void)
{
    if (s_iopBuf.m_buf[0] != 0)
        rxSndIopFree(s_iopBuf.m_buf[0]);
    if (s_iopBuf.m_buf[1] != 0)
        rxSndIopFree(s_iopBuf.m_buf[1]);
}

// FS2 0x0015aca0
int rxSndSpuHeapSetSlot(int id, u_int addr, u_int size)
{
    if (id < 0)
    {
        for (id = 0; id < 0x100 && s_spuSlots[id].m_used != 0; id++)
            ;
    }
    if ((u_int)id >= 0x100)
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : out of range over heap block ID '%d'\n", id);
        return -1;
    }

    if (s_spuSlots[id].m_used != 0)
        rxSndSpuHeapReleaseSlot(id);
    if (addr < 0x5010)
        addr = 0x5010;
    else if (addr > 0x1bffff)
        addr = 0x1bffff;
    u_int room = 0x1c0000 - addr;
    s_spuSlots[id].m_used = 1;
    if (size >= room)
        size = room;
    s_spuSlots[id].m_addr = addr;
    s_spuSlots[id].m_size = size;
    return id;
}

// FS2 0x0015add8
void rxSndSpuHeapReleaseSlot(u_int id)
{
    if (id < 0x100 && s_spuSlots[id].m_used != 0)
    {
        rxSndSpuHeapFreeSlot(id);
        s_spuSlots[id].m_used = 0;
    }
}

// FS2 0x0015ae30
int rxSndSpuHeapSetBankSlot(int slot)
{
    int old = s_spuRegion.m_bankSlot[rxSndGetBufferBank()];

    s_spuRegion.m_bankSlot[rxSndGetBufferBank()] = slot;
    return old;
}

// FS2 0x0015ae90
int rxSndSpuHeapGetBankSlot(void)
{
    return s_spuRegion.m_bankSlot[rxSndGetBufferBank()];
}

// FS2 0x0015aec8
u_int rxSndDefSpuAlloc(int size)
{
    return rxSndSpuHeapAlloc(0, 0, size);
}

// FS2 0x0015aee8
u_int rxSndDefSpuAllocMode1(int size)
{
    return rxSndSpuHeapAlloc(1, 0, size);
}

// FS2 0x0015af08
void rxSndDefSpuFree(u_int addr)
{
    rxSndSpuHeapFree(addr);
}

// FS2 0x0015af20
void rxSndSetSpuAllocHook(RxSndAllocFunc func)
{
    s_spuAllocHook = func != NULL ? func : rxSndDefSpuAlloc;
}

// FS2 0x0015af40
void rxSndSetSpuAllocMode1Hook(RxSndAllocFunc func)
{
    s_spuAllocMode1Hook = func != NULL ? func : rxSndDefSpuAllocMode1;
}

// FS2 0x0015af60
void rxSndSetSpuFreeHook(RxSndFreeFunc func)
{
    s_spuFreeHook = func != NULL ? func : rxSndDefSpuFree;
}

// FS2 0x0015af80
u_int rxSndSpuAlloc(int size)
{
    return s_spuAllocHook(size);
}

// FS2 0x0015afa8
u_int rxSndSpuAllocMode1(int size)
{
    return s_spuAllocMode1Hook(size);
}

// FS2 0x0015afd0
void rxSndSpuFree(u_int addr)
{
    s_spuFreeHook(addr);
}

/* PC: m_eeAddr keeps the low 32 bits of the host pointer; the driver
 * widens it back from the note (src/iop/rxsnddrv/dma.cpp). */
// FS2 0x0015aff8
int rxSndSpuTransfer(u_int spuAddr, const void *src, int size)
{
    RxSndCmdTransfer req;

    memset(&req, 0, sizeof(req));
    req.m_type    = 3;
    req.m_dir     = 1;
    req.m_spuAddr = spuAddr;
    req.m_eeAddr  = (u_int)(uintptr_t)src;
    req.m_size    = size;
    YamiPan_RxSndNoteEeAddress(src, (size_t)size);
    return (int)rxSndRpcCommand(0x40, NULL, 0, &req, sizeof(req));
}

/* The other transfer types of command 0x40; the fields are rxSndSpuTransfer's
 * (SPU address, EE address, size), what each type moves is not known. */
static void rxSndSpuTransferCmd(u_char type, u_int a, u_int b, int size)
{
    RxSndCmdTransfer req;

    memset(&req, 0, sizeof(req));
    req.m_type    = type;
    req.m_dir     = 1;
    req.m_spuAddr = a;
    req.m_eeAddr  = b;
    req.m_size    = size;
    rxSndRpcCommand(0x40, NULL, 0, &req, sizeof(req));
}

// callcheck: rxSndRpcCommand (through rxSndSpuTransferCmd)
// FS2 0x0015b040
void rxSndSpuTransferType4(u_int a, u_int b, int size)
{
    rxSndSpuTransferCmd(4, a, b, size);
}

// callcheck: rxSndRpcCommand (through rxSndSpuTransferCmd)
// FS2 0x0015b088
void rxSndSpuTransferType1(u_int a, u_int b, int size)
{
    rxSndSpuTransferCmd(1, a, b, size);
}

// callcheck: rxSndRpcCommand (through rxSndSpuTransferCmd)
// FS2 0x0015b0d0
void rxSndSpuTransferType2(u_int a, u_int b, int size)
{
    rxSndSpuTransferCmd(2, a, b, size);
}

// callcheck: rxSndRpcCommand (through rxSndSpuTransferCmd)
// FS2 0x0015b118
void rxSndSpuTransferType5(u_int a, int size)
{
    rxSndSpuTransferCmd(5, a, 0, size);
}

// FS2 0x0015b160
int rxSndXvabPoll(int handle, int wait)
{
    for (;;)
    {
        u_int req[1];

        req[0] = handle;
        int busy = (int)rxSndRpcCommand(0x41, NULL, 0, req, sizeof(req));
        if (busy == 0)
            return 0;
        if (!wait)
            return busy;
        for (int i = 9999; i >= 0; i--)
            rxSndWait();
    }
}

// FS2 0x0015b1f8
u_int rxSndSpuHeapGetFreeSize(void)
{
    return s_spuHeapSize;
}

// FS2 0x0015b208
u_int rxSndSpuHeapGetMaxFreeBlock(void)
{
    int best = 0;

    for (RxSndSpuNode *node = s_spuList.m_prev; node != &s_spuList; node = node->m_prev)
    {
        if (node->m_used == 0 && best < (int)node->m_size)
            best = (int)node->m_size;
    }
    return (u_int)best;
}

// FS2 0x0015b258
void rxSndSpuHeapInit(void)
{
    s_spuHeapSize  = 0x1baff0;
    s_spuFreeNodes = NULL;
    for (int i = 0; i < 256; i++)
    {
        s_spuNodes[i].m_prev = s_spuFreeNodes;  /* the free list links through +0x4 */
        s_spuFreeNodes       = &s_spuNodes[i];
    }

    /* One free block covering SPU 0x5010..0x1c0000 on the circular list. */
    RxSndSpuNode *node = rxSndSpuHeapNewNode();
    node->m_prev    = &s_spuList;
    node->m_next    = &s_spuList;
    s_spuList.m_prev = node;
    node->m_prev->m_next = node;
    node->m_size    = 0x1baff0;
    node->m_addr    = 0x5010;
    node->m_owner   = NULL;
    node->m_used    = 0;

    for (int i = 0; i < 256; i++)
    {
        s_spuSlots[i].m_index = i;
        s_spuSlots[i].m_used  = 0;
    }

    s_spuRegion.m_slot.m_addr  = 0x5010;
    s_spuRegion.m_slot.m_count = 0;
    s_spuRegion.m_slot.m_index = -1;
    s_spuRegion.m_slot.m_used  = 1;
    s_spuRegion.m_slot.m_size  = 0x1baff0;
    for (int i = 1; i >= 0; i--)
        s_spuRegion.m_bankSlot[i] = -1;
}

// FS2 0x0015b378
void rxSndSpuHeapTerm(void)
{
}

// FS2 0x0015b380
RxSndSpuSlot *rxSndSpuHeapGetSlot(u_int id)
{
    if (id > 0xff)
        return &s_spuRegion.m_slot;
    return &s_spuSlots[id];
}

/* The window tests compare unsigned, as the ROM does: a block outside the
 * slot gives hi < lo and passes the size test. */
// FS2 0x0015b3b8
u_int rxSndSpuHeapAlloc(int mode, u_int addr, int size)
{
    int   bytes  = (size + 0x3f) / 0x40 * 0x40;
    u_int result = 0;

    DIntr();
    if (bytes <= 0)
    {
        if (rxSndGetDebugFlags() & 2)
            rxSysPrintf("rxsnd : can't allocate SPU heap '%d'\n", bytes);
        EIntr();
        return 0;
    }

    RxSndSpuSlot *slot  = rxSndSpuHeapGetSlot((u_int)s_spuRegion.m_bankSlot[rxSndGetBufferBank()]);
    u_int         lo    = slot->m_addr;
    u_int         hi    = slot->m_addr + slot->m_size;
    RxSndSpuNode *node  = NULL;
    u_int         start = 0;
    bool          found = false;

    if (mode == 1)
    {
        /* Down from the top of SPU memory. */
        node  = s_spuList.m_next;
        start = 0x1c0000 - node->m_size;
        while (node != &s_spuList)
        {
            u_int a = start < lo ? lo : start;
            u_int b = hi < start + node->m_size ? hi : start + node->m_size;
            if (node->m_used == 0 && b - a >= (u_int)bytes)
            {
                addr  = b - bytes;
                found = true;
                break;
            }
            node   = node->m_next;
            start -= node->m_size;
        }
    }
    else if (mode == 0 || mode == 2)
    {
        /* Up from 0x5010: the lowest fit, or the block holding `addr`. */
        start = 0x5010;
        for (node = s_spuList.m_prev; node != &s_spuList; node = node->m_prev)
        {
            u_int a = start < lo ? lo : start;
            u_int b = hi < start + node->m_size ? hi : start + node->m_size;
            if (node->m_used == 0)
            {
                if (mode == 0 && b - a >= (u_int)bytes)
                {
                    addr  = a;
                    found = true;
                    break;
                }
                if (mode == 2 && addr >= a && addr + bytes <= b)
                {
                    found = true;
                    break;
                }
            }
            start += node->m_size;
        }
    }

    if (!found)
    {
        if (rxSndGetDebugFlags() & 2)
            rxSysPrintf("**********rxsnd : can't allocate SPU heap '%d'\n", bytes);
        EIntr();
        return 0;
    }

    RxSndSpuNode *front = rxSndSpuHeapNewNode();
    RxSndSpuNode *back  = NULL;
    if (front != NULL)
        back = rxSndSpuHeapNewNode();
    if (front == NULL || back == NULL)
    {
        rxSndSpuHeapFreeNode(front);
        rxSndSpuHeapFreeNode(back);
        if (rxSndGetDebugFlags() & 2)
            rxSysPrintf("rxsnd : can't allocate SPU heap slot\n");
        EIntr();
        return 0;
    }

    /* Split off the free space below addr (m_next side) and above the
     * block (m_prev side). */
    if (start < addr)
    {
        front->m_size         = addr - start;
        front->m_next         = node->m_next;
        front->m_prev         = node;
        front->m_used         = 0;
        node->m_next->m_prev  = front;
        node->m_next          = front;
        node->m_size         -= front->m_size;
    }
    else
    {
        rxSndSpuHeapFreeNode(front);
    }
    if (bytes < (int)node->m_size)
    {
        back->m_next          = node;
        back->m_used          = 0;
        back->m_prev          = node->m_prev;
        back->m_size          = node->m_size - bytes;
        node->m_prev->m_next  = back;
        node->m_prev          = back;
        node->m_size         -= back->m_size;
    }
    else
    {
        rxSndSpuHeapFreeNode(back);
    }

    node->m_owner  = slot;
    node->m_used   = 1;
    s_spuHeapSize -= bytes;
    slot->m_count++;
    if (rxSndGetDebugFlags() & 4)
        rxSysPrintf("rxsnd : SPU heap alloc %08X - %08X (%d) left %d / %d\n", addr,
                    addr + bytes - 1, bytes, s_spuHeapSize, 0x1baff0);
    result = addr;
    EIntr();
    return result;
}

// FS2 0x0015b7d0
void rxSndSpuHeapFree(u_int addr)
{
    DIntr();
    u_int start = 0x5010;
    for (RxSndSpuNode *node = s_spuList.m_prev; node != &s_spuList; node = node->m_prev)
    {
        if (node->m_used == 1 && start == addr)
        {
            node->m_used = 0;
            node->m_owner->m_count--;
            s_spuHeapSize += node->m_size;

            /* Merge with a free block below, then above. */
            RxSndSpuNode *below = node->m_next;
            if (below != &s_spuList && below->m_used == 0)
            {
                below->m_prev        = node->m_prev;
                node->m_prev->m_next = below;
                below->m_size       += node->m_size;
                rxSndSpuHeapFreeNode(node);
                node = below;
            }
            RxSndSpuNode *above = node->m_prev;
            if (above != &s_spuList && above->m_used == 0)
            {
                node->m_next->m_prev = above;
                above->m_next        = node->m_next;
                above->m_size       += node->m_size;
                rxSndSpuHeapFreeNode(node);
                node = above;
            }
            if (node->m_used == 0 && node->m_size == 0)
            {
                node->m_next->m_prev = node->m_prev;
                node->m_prev->m_next = node->m_next;
                rxSndSpuHeapFreeNode(node);
            }

            if (rxSndGetDebugFlags() & 4)
                rxSysPrintf("rxsnd : SPU heap free (%d / %d)\n", s_spuHeapSize, 0x1baff0);
            EIntr();
            return;
        }
        start += node->m_size;
    }

    if (rxSndGetDebugFlags() & 2)
        rxSysPrintf("rxsnd : can't found SPU heap address %08X\n", addr);
    EIntr();
}

/* The size is read after the free, as the ROM does: a block merged into
 * the one above keeps its own m_size. */
// FS2 0x0015b9d0
void rxSndSpuHeapFreeSlot(u_int id)
{
    RxSndSpuSlot *slot = rxSndSpuHeapGetSlot(id);

    if (slot->m_used == 0)
        return;

    DIntr();
    u_int         addr = 0x5010;
    RxSndSpuNode *next;
    for (RxSndSpuNode *node = s_spuList.m_prev; node != &s_spuList; node = next)
    {
        next = node->m_prev;
        if (node->m_owner == slot)
            rxSndSpuHeapFree(addr);
        addr += node->m_size;
    }
    EIntr();
}

// FS2 0x0015bac0
RxSndSpuNode *rxSndSpuHeapNewNode(void)
{
    RxSndSpuNode *node = s_spuFreeNodes;

    if (node != NULL)
    {
        s_spuFreeNodes = node->m_prev;
        return node;
    }
    return NULL;
}

// FS2 0x0015bae8
void rxSndSpuHeapFreeNode(RxSndSpuNode *node)
{
    if (node != NULL)
    {
        node->m_prev   = s_spuFreeNodes;
        s_spuFreeNodes = node;
    }
}

// FS2 0x0015bb08
void rxSndSpuHeapPrintStatus(void)
{
    int used = 0;
    int free = 0;

    if (rxSndGetDebugFlags() & 4)
        rxSysPrintf("-------- < SPU heap status > --------\n");

    u_int addr = 0x5010;
    for (RxSndSpuNode *node = s_spuList.m_prev; node != &s_spuList; node = node->m_prev)
    {
        u_int last = addr + node->m_size - 1;
        if (node->m_used == 0)
        {
            if (rxSndGetDebugFlags() & 4)
                rxSysPrintf("[x] %08X - %08X (%07d)\n", addr, last, node->m_size);
        }
        else if (node->m_owner == &s_spuRegion.m_slot)
        {
            if (rxSndGetDebugFlags() & 4)
                rxSysPrintf("[o] %08X - %08X (%07d) in global\n", addr, last, node->m_size);
        }
        else
        {
            const RxSndSpuSlot *slot = node->m_owner;
            if (rxSndGetDebugFlags() & 4)
                rxSysPrintf("[o] %08X - %08X (%07d) in ( BLOCK %03d : %08X - %08X )\n", addr, last,
                            node->m_size, slot->m_index, slot->m_addr, slot->m_addr + slot->m_size - 1);
        }
        used++;
        addr += node->m_size;
    }
    for (RxSndSpuNode *node = s_spuFreeNodes; node != NULL; node = node->m_prev)
        free++;

    if (rxSndGetDebugFlags() & 4)
        rxSysPrintf("use slot  %d\nfree slot %d\n", used, free);
    if (rxSndGetDebugFlags() & 4)
        rxSysPrintf("-------------------------------------\n");
}

/* ---- effect ------------------------------------------------------------ */

/* Queued command 0x32 {core, attribute, value}, core clamped to 0..1. */
static void rxSndQueueEffect(int core, int attr, int value)
{
    rxSndQueueBegin(0x32, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = value;
    cmd->m_id    = rxSndClamp(core, 0, 1);
    cmd->m_param = attr;
    rxSndQueueEnd();
}

// callcheck: rxSndQueueBegin, rxSndQueueGetData, rxSndQueueEnd (through rxSndQueueEffect)
// FS2 0x0015bd30
void rxSndSetEffectModeCore(int core, int mode)
{
    rxSndQueueEffect(core, 0, rxSndClamp(mode, 0, 9));
}

// FS2 0x0015bdd0
void rxSndSetEffectDepthCore(int core, int depthL, int depthR)
{
    rxSndQueueBegin(0x32, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_vol[1] = (short)depthR;
    cmd->m_id     = rxSndClamp(core, 0, 1);
    cmd->m_vol[0] = (short)depthL;
    cmd->m_param  = 2;
    rxSndQueueEnd();
}

// callcheck: rxSndQueueBegin, rxSndQueueGetData, rxSndQueueEnd (through rxSndQueueEffect)
// FS2 0x0015be58
void rxSndSetEffectDelayCore(int core, int delay)
{
    rxSndQueueEffect(core, 3, delay);
}

// callcheck: rxSndQueueBegin, rxSndQueueGetData, rxSndQueueEnd (through rxSndQueueEffect)
// FS2 0x0015bed8
void rxSndSetEffectFeedbackCore(int core, int feedback)
{
    rxSndQueueEffect(core, 4, feedback);
}

// FS2 0x0015bf58
void rxSndSetEffectMode(int mode)
{
    rxSndSetEffectModeCore(0, mode);
    rxSndSetEffectModeCore(1, mode);
}

// FS2 0x0015bf90
void rxSndSetEffectDepth(int depthL, int depthR)
{
    rxSndSetEffectDepthCore(0, depthL, depthR);
    rxSndSetEffectDepthCore(1, depthL, depthR);
}

// FS2 0x0015bfd8
void rxSndSetEffectDelay(int delay)
{
    rxSndSetEffectDelayCore(0, delay);
    rxSndSetEffectDelayCore(1, delay);
}

// FS2 0x0015c010
void rxSndSetEffectFeedback(int feedback)
{
    rxSndSetEffectFeedbackCore(0, feedback);
    rxSndSetEffectFeedbackCore(1, feedback);
}

/* ---- XVAB / XSEQ ------------------------------------------------------ */

// FS2 0x0015c048
void rxSndXvabInit(void)
{
}

// FS2 0x0015c050
void rxSndXvabTerm(void)
{
}

// FS2 0x0015c058
u_int rxSndReadVarLen(const u_char **p)
{
    const u_char *at = *p;
    u_int         v  = 0;
    u_char        c;

    do
    {
        c = *at++;
        v = (v << 7) | (c & 0x7f);
    } while (c & 0x80);
    *p = at;
    return v;
}

/* Takes bank `id` (-1: the first free one), copies the file to IOP memory,
 * opens it there (command 0x50) and starts sending its samples to SPU
 * memory; the IOP's reply is the bank id. iopAddr / spuAddr 0 allocate. */
// FS2 0x0015c090
int rxSndXvabLoad(int id, const void *xvab, u_int iopAddr, u_int spuAddr)
{
    char       msg[0x100];
    u_int      owned = 0;
    RxSndXvab *bank  = NULL;

    msg[0] = '\0';
    DIntr();
    if (id < 0)
    {
        for (id = 0; id < 0x100; id++)
        {
            if (!(s_xvab[id].m_flags & 1))
            {
                s_xvab[id].m_flags = 1;
                bank               = &s_xvab[id];
                break;
            }
        }
        if (id == 0x100)
            strcpy(msg, "XVAB slot is empty\n");
    }
    else if (id < 0x100)
    {
        if (!(s_xvab[id].m_flags & 1))
        {
            s_xvab[id].m_flags = 1;
            bank               = &s_xvab[id];
        }
        else
        {
            sprintf(msg, "XVAB ID '%d' is already exist\n", id);
        }
    }
    else
    {
        sprintf(msg, "XVAB ID '%d' is out of range\n", id);
    }
    EIntr();

    if (bank == NULL)
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("%s", msg);
        return -1;
    }

    u_int magic = rxSndReadU32(xvab);
    if (magic == 0x42415658)            /* "XVAB" */
    {
        int           size = rxSndChunkGetFileSize(xvab);
        const u_char *samples;
        int           sampleSize;
        rxSndXvabGetSampleData(&samples, &sampleSize, xvab);

        if (iopAddr == 0)
        {
            iopAddr = rxSndIopAlloc(size);
            if (iopAddr == 0)
            {
                if (rxSndGetDebugFlags() & 2)
                    rxSysPrintf("rxsnd : can't allocate IOP heap size '%d'\n", size);
                goto fail;
            }
            owned = 0x10000;
        }
        if (spuAddr == 0 && samples != NULL)
        {
            spuAddr = rxSndSpuAlloc(sampleSize);
            if (spuAddr == 0)
            {
                if (rxSndGetDebugFlags() & 2)
                    rxSysPrintf("rxsnd : can't allocate SPU heap size '%d'\n", sampleSize);
                goto fail;
            }
            owned |= 0x20000;
        }

        rxSndIopWrite(iopAddr, xvab, size);
        u_int req[3];
        req[0]  = id;
        req[1]  = iopAddr;
        req[2]  = spuAddr;
        int ret = (int)rxSndRpcCommand(0x50, NULL, 0, req, sizeof(req));
        if (ret >= 0)
        {
            bank->m_iopBuf    = iopAddr;
            bank->m_iopSize   = size;
            bank->m_flags    |= owned;
            bank->m_spuBuf    = spuAddr;
            bank->m_transfer  = -1;
            bank->m_spuSize   = sampleSize;
            if (samples != NULL)
            {
                int handle;
                do
                    handle = rxSndSpuTransfer(spuAddr, samples, sampleSize);
                while (handle < 0);
                bank->m_transfer  = handle;
                bank->m_flags    |= 0x40000;
            }
            return ret;
        }
        if (rxSndGetDebugFlags() & 2)
            rxSysPrintf("rxsnd : failed to open XVAB\n");
    }
    else if (rxSndGetDebugFlags() & 1)
    {
        rxSysPrintf("rxsnd : wrong type XVAB ID '%c%c%c%c'\n", magic & 0xff, (magic >> 8) & 0xff,
                    (magic >> 16) & 0xff, magic >> 24);
    }

fail:
    bank->m_flags = 0;
    if ((owned & 0x10000) && iopAddr != 0)
        rxSndIopFree(iopAddr);
    if ((owned & 0x20000) && spuAddr != 0)
        rxSndSpuFree(spuAddr);
    return -1;
}

// FS2 0x0015c480
int rxSndXvabFree(int id, int wait)
{
    if ((u_int)id >= 0x100)
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : out of range XVAB ID '%d'\n", id);
        return 0;
    }

    RxSndXvab *bank = &s_xvab[id];
    if (!(bank->m_flags & 1))
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : specified XVAB ID '%d' is unused\n", id);
        return 0;
    }

    /* Command 0x51 answers 1 while voices still play the bank. */
    u_int req[1];
    req[0] = id;
    while (rxSndRpcCommand(0x51, NULL, 0, req, sizeof(req)) == 1)
    {
        if (!wait)
            return 1;
        if (rxSndGetBufferBank() == 0)
            rxSndUpdate();
        for (int i = 9999; i >= 0; i--)
            rxSndWait();
    }

    if ((bank->m_flags & 0x10000) && bank->m_iopBuf != 0)
        rxSndIopFree(bank->m_iopBuf);
    if ((bank->m_flags & 0x20000) && bank->m_spuBuf != 0)
        rxSndSpuFree(bank->m_spuBuf);
    bank->m_flags = 0;
    return 0;
}

// FS2 0x0015c5f8
int rxSndXvabCheck(int id, int wait)
{
    if ((u_int)id >= 0x100)
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : out of range XVAB ID '%d'\n", id);
        return 0;
    }

    RxSndXvab *xvab = &s_xvab[id];
    if (!(xvab->m_flags & 1))
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : specified XVAB ID '%d' is unused\n", id);
        return 0;
    }
    if (xvab->m_transfer < 0)
        return 0;

    int busy = rxSndXvabPoll(xvab->m_transfer, wait);
    if (busy == 0)
        xvab->m_transfer = -1;
    return busy;
}

// FS2 0x0015c6c8
u_int rxSndXvabGetIopAddr(int id)
{
    if ((u_int)id < 0x100)
    {
        if (s_xvab[id].m_flags & 1)
            return s_xvab[id].m_iopBuf;
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : specified XVAB ID '%d' is unused\n", id);
    }
    else if (rxSndGetDebugFlags() & 1)
    {
        rxSysPrintf("rxsnd : out of range XVAB ID '%d'\n", id);
    }
    return 0;
}

// FS2 0x0015c758
u_int rxSndXvabGetSpuAddr(int id)
{
    if ((u_int)id < 0x100)
    {
        if (s_xvab[id].m_flags & 1)
            return s_xvab[id].m_spuBuf;
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : specified XVAB ID '%d' is unused\n", id);
    }
    else if (rxSndGetDebugFlags() & 1)
    {
        rxSysPrintf("rxsnd : out of range XVAB ID '%d'\n", id);
    }
    return 0;
}

/* FS2 only (no caller): command 0x52 {id} of a loaded XVAB bank (meaning not
 * known); the IOP's answer, 0 for a bad id. */
// FS2 0x0015c7e8
u_int rxSndXvabRpc52(int id)
{
    if ((u_int)id < 0x100)
    {
        if (s_xvab[id].m_flags & 1)
        {
            int req[1];

            req[0] = id;
            return rxSndRpcCommand(0x52, NULL, 0, req, sizeof(req));
        }
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : specified XVAB ID '%d' is unused\n", id);
    }
    else if (rxSndGetDebugFlags() & 1)
    {
        rxSysPrintf("rxsnd : out of range XVAB ID '%d'\n", id);
    }
    return 0;
}

/* FS2 only (no caller): command 0x53 {id, a, b} of a loaded XVAB bank (meaning not
 * known); the IOP's answer, 0 for a bad id. */
// FS2 0x0015c890
u_int rxSndXvabSetParam(int id, int a, int b)
{
    if ((u_int)id < 0x100)
    {
        if (s_xvab[id].m_flags & 1)
        {
            int req[3];

            req[0] = id;
            req[1] = a;
            req[2] = b;
            return rxSndRpcCommand(0x53, NULL, 0, req, sizeof(req));
        }
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : specified XVAB ID '%d' is unused\n", id);
    }
    else if (rxSndGetDebugFlags() & 1)
    {
        rxSysPrintf("rxsnd : out of range XVAB ID '%d'\n", id);
    }
    return 0;
}

// FS2 0x0015c958
int rxSndXvabGetFileSize(const void *xvab)
{
    return rxSndChunkGetFileSize(xvab);
}

// FS2 0x0015c970
int rxSndXvabGetSampleData(const u_char **data, int *size, const void *xvab)
{
    /* A HEAD chunk with byte 1 set: the samples follow the file. */
    const u_char *head = rxSndChunkFindData(xvab, 0x44414548);      /* "HEAD" */
    if (head != NULL && head[1] != 0)
    {
        *data = (const u_char *)xvab + ((rxSndChunkGetFileSize(xvab) + 0xf) & ~0xf);
        *size = *(const int *)(head + 4);
        return 1;
    }

    const u_char *smpl = rxSndChunkFindData(xvab, 0x4c504d53);      /* "SMPL" */
    if (smpl == NULL)
    {
        *data = NULL;
        *size = 0;
        return 0;
    }
    *data = (const u_char *)xvab + *(const int *)(smpl + 8);
    *size = *(const int *)(smpl + 0xc);
    return 1;
}

// FS2 0x0015ca48
const u_char *rxSndXvabGetSampleAddr(const void *xvab)
{
    const u_char *data;
    int           size;

    if (rxSndXvabGetSampleData(&data, &size, xvab) == 0)
        data = NULL;
    return data;
}

// FS2 0x0015ca78
int rxSndXvabGetSampleSize(const void *xvab)
{
    const u_char *data;
    int           size;

    if (rxSndXvabGetSampleData(&data, &size, xvab) == 0)
        size = 0;
    return size;
}

/* Takes sequence `id` (-1: the first free one), copies the file to IOP
 * memory (iopAddr 0 allocates) and opens it there (command 0x60); the IOP's
 * reply is the id. */
// FS2 0x0015caa8
int rxSndXseqLoad(int id, const void *xseq, u_int iopAddr)
{
    char       msg[0x100];
    u_int      owned = 0;
    RxSndXseq *seq   = NULL;

    msg[0] = '\0';
    DIntr();
    if (id < 0)
    {
        for (id = 0; id < 0x100; id++)
        {
            if (!(s_xseq[id].m_flags & 1))
            {
                s_xseq[id].m_flags = 1;
                seq                = &s_xseq[id];
                break;
            }
        }
        if (id == 0x100)
            strcpy(msg, "XSEQ slot is empty\n");
    }
    else if (id < 0x100)
    {
        if (!(s_xseq[id].m_flags & 1))
        {
            s_xseq[id].m_flags = 1;
            seq                = &s_xseq[id];
        }
        else
        {
            sprintf(msg, "XSEQ ID '%d' is already exist\n", id);
        }
    }
    else
    {
        sprintf(msg, "XSEQ ID '%d' is out of range\n", id);
    }
    EIntr();

    if (seq == NULL)
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("%s", msg);
        return -1;
    }

    u_int magic = rxSndReadU32(xseq);
    if (magic == 0x51455358)            /* "XSEQ" */
    {
        int size = rxSndChunkGetFileSize(xseq);
        if (iopAddr == 0)
        {
            iopAddr = rxSndIopAlloc(size);
            if (iopAddr == 0)
            {
                if (rxSndGetDebugFlags() & 2)
                    rxSysPrintf("rxsnd : can't allocate IOP heap size '%d'\n", size);
                goto fail;
            }
            owned = 0x10000;
        }

        rxSndIopWrite(iopAddr, xseq, size);
        u_int req[2];
        req[0]  = id;
        req[1]  = iopAddr;
        int ret = (int)rxSndRpcCommand(0x60, NULL, 0, req, sizeof(req));
        if (ret >= 0)
        {
            seq->m_iopSize  = size;
            seq->m_iopBuf   = iopAddr;
            seq->m_flags   |= owned;
            return ret;
        }
        if (rxSndGetDebugFlags() & 2)
            rxSysPrintf("rxsnd : failed to open XSEQ\n");
    }
    else if (rxSndGetDebugFlags() & 1)
    {
        rxSysPrintf("error : wrong type XSEQ ID '%c%c%c%c'\n", magic & 0xff, (magic >> 8) & 0xff,
                    (magic >> 16) & 0xff, magic >> 24);
    }

fail:
    seq->m_flags = 0;
    if (owned != 0 && iopAddr != 0)
        rxSndIopFree(iopAddr);
    return -1;
}

// FS2 0x0015cda0
int rxSndXseqFree(int id, int wait)
{
    if ((u_int)id >= 0x100)
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : out of range XSEQ ID '%d'\n", id);
        return 0;
    }

    RxSndXseq *seq = &s_xseq[id];
    if (!(seq->m_flags & 1))
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : specified XSEQ ID '%d' is unused\n", id);
        return 0;
    }

    /* Command 0x61 answers 1 while a BGM still plays the sequence. */
    u_int req[1];
    req[0] = id;
    while (rxSndRpcCommand(0x61, NULL, 0, req, sizeof(req)) == 1)
    {
        if (!wait)
            return 1;
        if (rxSndGetBufferBank() == 0)
            rxSndUpdate();
        for (int i = 9999; i >= 0; i--)
            rxSndWait();
    }

    if ((seq->m_flags & 0x10000) && seq->m_iopBuf != 0)
        rxSndIopFree(seq->m_iopBuf);
    seq->m_flags = 0;
    return 0;
}

// FS2 0x0015cef0
void rxSndXseqSetParam(int id, int a, int b)
{
    if ((u_int)id < 0x100)
    {
        if (s_xseq[id].m_flags & 1)
        {
            u_int req[3];

            req[0] = id;
            req[1] = a;
            req[2] = b;
            rxSndRpcCommand(0x63, NULL, 0, req, sizeof(req));
            return;
        }
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : specified XSEQ ID '%d' is unused\n", id);
    }
    else if (rxSndGetDebugFlags() & 1)
    {
        rxSysPrintf("rxsnd : out of range XSEQ ID '%d'\n", id);
    }
}

// FS2 0x0015cfb0
u_int rxSndXseqGetIopAddr(int id)
{
    if ((u_int)id < 0x100)
    {
        if (s_xseq[id].m_flags & 1)
            return s_xseq[id].m_iopBuf;
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : specified XSEQ ID '%d' is unused\n", id);
    }
    else if (rxSndGetDebugFlags() & 1)
    {
        rxSysPrintf("rxsnd : out of range XSEQ ID '%d'\n", id);
    }
    return 0;
}

/* FS2 only (no caller): command 0x62 {id} of a loaded XSEQ (meaning not
 * known); the IOP's answer, 0 for a bad id. */
// FS2 0x0015d040
u_int rxSndXseqRpc62(int id)
{
    if ((u_int)id < 0x100)
    {
        if (s_xseq[id].m_flags & 1)
        {
            int req[1];

            req[0] = id;
            return rxSndRpcCommand(0x62, NULL, 0, req, sizeof(req));
        }
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : specified XSEQ ID '%d' is unused\n", id);
    }
    else if (rxSndGetDebugFlags() & 1)
    {
        rxSysPrintf("rxsnd : out of range XSEQ ID '%d'\n", id);
    }
    return 0;
}

// FS2 0x0015d0e8
int rxSndXseqGetFileSize(const void *xseq)
{
    return rxSndChunkGetFileSize(xseq);
}

// FS2 0x0015d100
void rxSndXseqInit(void)
{
}

// FS2 0x0015d108
void rxSndXseqTerm(void)
{
}

// FS2 0x0015d110
u_int rxSndReadU32(const void *p)
{
    return *(const u_int *)p;
}

// FS2 0x0015d118
u_int rxSndChunkGetVersion(const void *file)
{
    return ((const u_int *)file)[1];
}

// FS2 0x0015d120
int rxSndChunkGetFileSize(const void *file)
{
    int size = ((const int *)file)[2];
    if (rxSndChunkGetVersion(file) > 0x1ff)
        size += 0x10;
    return size;
}

// FS2 0x0015d160
int rxSndChunkFind(const u_char **data, int *size, const void *file, u_int tag)
{
    const u_char *base  = (const u_char *)file;
    int           total = ((const int *)file)[2];

    if (rxSndChunkGetVersion(file) > 0x1ff)
    {
        for (int pos = 0; pos < total;)
        {
            const int *chunk = (const int *)(base + 0x10 + pos);
            if ((u_int)chunk[0] == tag)
            {
                *data = (const u_char *)(chunk + 4);
                *size = chunk[2];
                return 1;
            }
            pos += chunk[2] + 0x10;
        }
        return 0;
    }

    for (int pos = 0xc; pos < total;)
    {
        const int *chunk = (const int *)(base + pos);
        if ((u_int)chunk[0] == tag)
        {
            *data = (const u_char *)(chunk + 2);
            *size = chunk[1];
            return 1;
        }
        pos += chunk[1] + 8;
    }
    return 0;
}

// FS2 0x0015d2b0
const u_char *rxSndChunkFindData(const void *file, u_int tag)
{
    const u_char *data;
    int           size;

    if (rxSndChunkFind(&data, &size, file, tag) == 0)
        data = NULL;
    return data;
}

// FS2 0x0015d2e8
int rxSndChunkGetSize(const void *file, u_int tag)
{
    const u_char *data;
    int           size;

    if (rxSndChunkFind(&data, &size, file, tag) == 0)
        size = 0;
    return size;
}

/* ---- BGM -------------------------------------------------------------- */

// FS2 0x0015d320
void rxSndBgmSetMasterVolume(int volL, int volR)
{
    s_bgmMasterVolL = (short)rxSndClamp(volL, -0x80, 0x80);
    s_bgmMasterVolR = (short)rxSndClamp(volR, -0x80, 0x80);
    for (RxSndBgm *bgm = s_bgmActive; bgm != NULL; bgm = bgm->m_next)
        rxSndBgmSetVolume(bgm->m_id, bgm->m_volL, bgm->m_volR);
}

// FS2 0x0015d3b8
int rxSndBgmPlay(int xseq, int volL, int volR, int keep)
{
    RxSndBgm *bgm = s_bgmFree;

    if (bgm == NULL)
    {
        if (rxSndGetDebugFlags() & 4)
            rxSysPrintf("rxsnd : can't allocate BGM slot ( BGM ID '%d' )\n", xseq);
        return -1;
    }

    bgm->m_keep  = keep;
    bgm->m_volL  = (short)volL;
    bgm->m_volR  = (short)volR;
    bgm->m_flags = 3;
    s_bgmFree    = bgm->m_next;
    bgm->m_next  = s_bgmActive;
    s_bgmActive  = bgm;

    rxSndQueueBegin(0x70, 0, 8);
    u_int *cmd = (u_int *)rxSndQueueGetData();
    cmd[1] = xseq;
    cmd[0] = bgm->m_id;
    rxSndQueueEnd();

    rxSndBgmSetVolume(bgm->m_id, volL, volR);
    return bgm->m_id;
}

// FS2 0x0015d4a0
void rxSndBgmStop(int id, int arg)
{
    rxSndCallBgCallHook("rxSndBgmStop");

    RxSndBgm **link = &s_bgmActive;
    RxSndBgm  *bgm;
    for (;;)
    {
        bgm = *link;
        if (bgm == NULL)
            return;
        if (bgm->m_id == id)
            break;
        link = &bgm->m_next;
    }

    rxSndQueueBegin(0x71, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_param = 0;
    cmd->m_value = arg != 0 ? 1 : 2;
    cmd->m_id    = bgm->m_id;
    rxSndQueueEnd();

    bgm->m_flags  = 0;
    *link         = bgm->m_next;
    bgm->m_next   = s_bgmStopping;
    s_bgmStopping = bgm;
}

// FS2 0x0015d580
void rxSndBgmStopAll(int arg)
{
    for (int i = 0; i < 0x20; i++)
        rxSndBgmStop(i, arg);
}

/* As rxSndSePause. */
// FS2 0x0015d5d0
void rxSndBgmPause(int id, int pause, int arg)
{
    if ((u_int)id >= 0x20 || !(s_bgm[id].m_flags & 1))
        return;

    rxSndQueueBegin(0x71, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_param = 1;
    cmd->m_id    = s_bgm[id].m_id;
    cmd->m_value = pause != 0 ? (arg != 0 ? 1 : 2) : 0;
    rxSndQueueEnd();
}

// FS2 0x0015d698
u_int rxSndBgmGetStatus(int id)
{
    if ((u_int)id >= 0x20 || !(s_bgm[id].m_flags & 1))
        return 0;
    return s_bgm[id].m_flags;
}

// FS2 0x0015d6d0
u_int rxSndBgmGetValue(int id)
{
    if ((u_int)id >= 0x20 || !(s_bgm[id].m_flags & 1))
        return 0;
    return s_bgm[id].m_value;
}

static void rxSndBgmQueueChannelVolume(u_int cmdNo, int id, int channel, int volL, int volR)
{
    if ((u_int)id >= 0x20 || !(s_bgm[id].m_flags & 1))
        return;

    rxSndQueueBegin(cmdNo, 0, sizeof(RxSndCmdBgmChannelSet));
    RxSndCmdBgmChannelSet *cmd = (RxSndCmdBgmChannelSet *)rxSndQueueGetData();
    cmd->m_vol[1]  = (short)volR;
    cmd->m_channel = (u_char)channel;
    cmd->m_vol[0]  = (short)volL;
    cmd->m_id      = (u_char)s_bgm[id].m_id;
    cmd->m_param   = 3;
    rxSndQueueEnd();
}

// callcheck: rxSndQueueBegin, rxSndQueueGetData, rxSndQueueEnd (through rxSndBgmQueueChannelVolume)
// FS2 0x0015d710
void rxSndBgmSetChannelVolume(int id, int channel, int volL, int volR)
{
    rxSndBgmQueueChannelVolume(0x73, id, channel, volL, volR);
}

// callcheck: rxSndQueueBegin, rxSndQueueGetData, rxSndQueueEnd (through rxSndBgmQueueChannelVolume)
// FS2 0x0015d7d0
void rxSndBgmSetChannelVolume74(int id, int channel, int volL, int volR)
{
    rxSndBgmQueueChannelVolume(0x74, id, channel, volL, volR);
}

/* Queued 0x71 {slot, param, value} for an open slot. */
static void rxSndBgmQueueSet(int id, int param, int value)
{
    if ((u_int)id >= 0x20 || !(s_bgm[id].m_flags & 1))
        return;

    rxSndQueueBegin(0x71, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = value;
    cmd->m_id    = s_bgm[id].m_id;
    cmd->m_param = param;
    rxSndQueueEnd();
}

// callcheck: rxSndQueueBegin, rxSndQueueGetData, rxSndQueueEnd (through rxSndBgmQueueSet)
// FS2 0x0015d890
void rxSndBgmSetAttr(int id, int value)
{
    rxSndBgmQueueSet(id, 2, value);
}

// FS2 0x0015d928
void rxSndBgmSetVolume(int id, int volL, int volR)
{
    if ((u_int)id >= 0x20 || !(s_bgm[id].m_flags & 1))
        return;

    RxSndBgm *bgm = &s_bgm[id];
    volL = rxSndClamp(volL, -0x80, 0x80);
    volR = rxSndClamp(volR, -0x80, 0x80);
    bgm->m_volL = (short)volL;
    bgm->m_volR = (short)volR;
    int masterL = rxSndGetMasterVolumeL();
    int famL    = s_bgmMasterVolL;
    int masterR = rxSndGetMasterVolumeR();
    int famR    = s_bgmMasterVolR;

    rxSndQueueBegin(0x71, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_vol[0] = (short)(masterL * famL * volL >> 14);
    cmd->m_vol[1] = (short)(masterR * famR * volR >> 14);
    cmd->m_id     = bgm->m_id;
    cmd->m_param  = 3;
    rxSndQueueEnd();
}

// callcheck: rxSndQueueBegin, rxSndQueueGetData, rxSndQueueEnd (through rxSndBgmQueueSet)
// FS2 0x0015da78
void rxSndBgmSetPitch(int id, float pitch)
{
    rxSndBgmQueueSet(id, 4, (int)(pitch * 4096.0f));
}

// callcheck: rxSndQueueBegin, rxSndQueueGetData, rxSndQueueEnd (through rxSndBgmQueueSet)
// FS2 0x0015db20
void rxSndBgmSetParam5(int id, float value)
{
    rxSndBgmQueueSet(id, 5, (int)(value * 4096.0f));
}

// callcheck: rxSndQueueBegin, rxSndQueueGetData, rxSndQueueEnd (through rxSndBgmQueueSet)
// FS2 0x0015dbc8
void rxSndBgmSetParam6(int id, int value)
{
    rxSndBgmQueueSet(id, 6, value);
}

/* FS2 only (no caller): the ids of the active BGM slots into ids[] (null: just
 * count them); their count. */
// FS2 0x0015dc60
int rxSndBgmGetActiveList(int *ids)
{
    int n = 0;

    for (RxSndBgm *p = s_bgmActive; p != NULL; p = p->m_next)
    {
        if (ids != NULL)
            ids[n] = p->m_id;
        n++;
    }
    return n;
}

// FS2 0x0015dca8
void rxSndBgmInit(void)
{
    s_bgmMasterVolR = 0x80;
    s_bgmActive     = NULL;
    s_bgmStopping   = NULL;
    s_bgmFree       = NULL;
    s_bgmMasterVolL = 0x80;
    for (int i = 0; i < 0x20; i++)
    {
        s_bgm[i].m_id   = i;
        s_bgm[i].m_next = s_bgmFree;
        s_bgmFree       = &s_bgm[i];
    }
}

// FS2 0x0015dd00
void rxSndBgmTerm(void)
{
}

// FS2 0x0015dd08
void rxSndBgmUpdate(const RxSndBgmStatus *status)
{
    /* Playing slots take the IOP's status; an ended one stops itself
     * unless m_keep is set. */
    RxSndBgm *next;
    for (RxSndBgm *bgm = s_bgmActive; bgm != NULL; bgm = next)
    {
        next = bgm->m_next;
        for (int i = 0; i < status->m_count; i++)
        {
            const RxSndStatusEntry *e = &status->m_entry[i];
            if (e->m_id == (u_int)bgm->m_id)
            {
                u_char st = e->m_status;
                bgm->m_flags = (bgm->m_flags & ~0xeu) | (st & 2) | (st & 8) | (st & 4);
                bgm->m_value = status->m_value[i];
                if ((st & 4) && bgm->m_keep == 0)
                    rxSndBgmStop(e->m_id, 0);
                break;
            }
        }
    }

    /* Stopped slots go back to the free list once the IOP drops them. */
    RxSndBgm **link = &s_bgmStopping;
    for (RxSndBgm *bgm = *link; bgm != NULL; bgm = *link)
    {
        bool listed = false;
        for (int i = 0; i < status->m_count; i++)
        {
            if (status->m_entry[i].m_id == (u_int)bgm->m_id)
            {
                listed = true;
                break;
            }
        }
        if (listed)
        {
            link = &bgm->m_next;
            continue;
        }
        *link       = bgm->m_next;
        bgm->m_next = s_bgmFree;
        s_bgmFree   = bgm;
    }
}

// FS2 0x0015de80
short rxSndBgmGetMasterVolumeL(void)
{
    return s_bgmMasterVolL;
}

// FS2 0x0015de90
short rxSndBgmGetMasterVolumeR(void)
{
    return s_bgmMasterVolR;
}

/* ---- SE --------------------------------------------------------------- */

// FS2 0x0015dea0
void rxSndSeSetMasterVolume(int volL, int volR)
{
    s_seMasterVolL = (short)rxSndClamp(volL, -0x80, 0x80);
    s_seMasterVolR = (short)rxSndClamp(volR, -0x80, 0x80);
    for (RxSndSeVoice *voice = s_seActive; voice != NULL; voice = voice->m_next)
        rxSndSeSetVolume(voice->m_id, voice->m_volL, voice->m_volR);
}

// FS2 0x0015df38
void rxSndSeSetDefaultParamB(int value)
{
    s_seParamB = rxSndClamp(value, -1, 2);
}

// FS2 0x0015df68
void rxSndSeSetDefaultParamC(int value)
{
    s_seParamC = rxSndClamp(value, -1, 0x7e);
}

// FS2 0x0015df98
int rxSndSePlay(int xvab, int prog, int note, int volL, int volR, int keep)
{
    RxSndSeVoice *voice = s_seFree;

    if (voice == NULL)
    {
        if (rxSndGetDebugFlags() & 4)
            rxSysPrintf("rxsnd : can't allocate SE slot ( SE '%d, %d, %d' )\n", xvab, prog, note);
        return -1;
    }

    rxSndQueueBegin(0x80, 0, sizeof(RxSndCmdSePlay));
    RxSndCmdSePlay *cmd = (RxSndCmdSePlay *)rxSndQueueGetData();
    cmd->m_note  = (u_char)note;
    cmd->m_voice = (u_char)voice->m_id;
    cmd->m_xvab  = (u_char)xvab;
    cmd->m_prog  = (u_char)prog;
    rxSndQueueEnd();

    voice->m_flags = 3;
    voice->m_keep  = keep;
    voice->m_volL  = (short)volL;
    voice->m_volR  = (short)volR;
    s_seFree       = voice->m_next;
    voice->m_next  = s_seActive;
    s_seActive     = voice;

    rxSndSeSetVolume(voice->m_id, volL, volR);
    if (s_seParamB > 0)
        rxSndSeSetParamB(voice->m_id, s_seParamB);
    if (s_seParamC > 0)
        rxSndSeSetParamC(voice->m_id, s_seParamC);
    return voice->m_id;
}

// FS2 0x0015e0e0
int rxSndSePlayEx(int xvab, int prog, int note, int volL, int volR, int keep)
{
    RxSndSeVoice *voice = s_seFree;

    if (voice == NULL)
    {
        if (rxSndGetDebugFlags() & 4)
            rxSysPrintf("rxsnd : can't allocate SE slot ( SE '%d, %d, %d' )\n", xvab, prog, note);
        return -1;
    }

    rxSndQueueBegin(0x81, 0, sizeof(RxSndCmdSePlay));
    RxSndCmdSePlay *cmd = (RxSndCmdSePlay *)rxSndQueueGetData();
    cmd->m_note  = (u_char)note;
    cmd->m_voice = (u_char)voice->m_id;
    cmd->m_xvab  = (u_char)xvab;
    cmd->m_prog  = (u_char)prog;
    rxSndQueueEnd();

    voice->m_flags = 3;
    voice->m_keep  = keep;
    voice->m_volL  = (short)volL;
    voice->m_volR  = (short)volR;
    s_seFree       = voice->m_next;
    voice->m_next  = s_seActive;
    s_seActive     = voice;

    rxSndSeSetVolume(voice->m_id, volL, volR);
    if (s_seParamB > 0)
        rxSndSeSetParamB(voice->m_id, s_seParamB);
    if (s_seParamC > 0)
        rxSndSeSetParamC(voice->m_id, s_seParamC);
    return voice->m_id;
}

// FS2 0x0015e228
void rxSndSeStop(int id, int arg)
{
    rxSndCallBgCallHook("rxSndSeStop");

    RxSndSeVoice **link = &s_seActive;
    RxSndSeVoice  *voice;
    for (;;)
    {
        voice = *link;
        if (voice == NULL)
            return;
        if (voice->m_id == id)
            break;
        link = &voice->m_next;
    }

    rxSndQueueBegin(0x82, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_param = 0;
    cmd->m_value = arg != 0 ? 1 : 2;
    cmd->m_id    = voice->m_id;
    rxSndQueueEnd();

    voice->m_flags = 0;
    *link          = voice->m_next;
    voice->m_next  = s_seStopping;
    s_seStopping   = voice;
}

// FS2 0x0015e308
void rxSndSeStopAll(int arg)
{
    for (int i = 0; i < 0x40; i++)
        rxSndSeStop(i, arg);
}

// FS2 0x0015e358
u_int rxSndSeGetStatus(int id)
{
    if ((u_int)id > 0x3f || !(s_se[id].m_flags & 1))
        return 0;
    return s_se[id].m_flags;
}

/* pause != 0: arg != 0 pauses with value 1, else 2 (the IOP's two pause
 * kinds, not known); pause 0 resumes. */
// FS2 0x0015e390
void rxSndSePause(int id, int pause, int arg)
{
    if ((u_int)id >= 0x40 || !(s_se[id].m_flags & 1))
        return;

    rxSndQueueBegin(0x82, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_param = 1;
    cmd->m_id    = s_se[id].m_id;
    cmd->m_value = pause != 0 ? (arg != 0 ? 1 : 2) : 0;
    rxSndQueueEnd();
}

// FS2 0x0015e458
void rxSndSeSetAttr(int id, int value)
{
    if ((u_int)id >= 0x40 || !(s_se[id].m_flags & 1))
        return;

    rxSndQueueBegin(0x82, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = value;
    cmd->m_id    = s_se[id].m_id;
    cmd->m_param = 2;
    rxSndQueueEnd();
}

// FS2 0x0015e4f0
void rxSndSeSetVolume(int id, int volL, int volR)
{
    if ((u_int)id >= 0x40 || !(s_se[id].m_flags & 1))
        return;

    RxSndSeVoice *voice = &s_se[id];
    volL = rxSndClamp(volL, -0x80, 0x80);
    volR = rxSndClamp(volR, -0x80, 0x80);
    voice->m_volL = (short)volL;
    voice->m_volR = (short)volR;
    int masterL = rxSndGetMasterVolumeL();
    int famL    = s_seMasterVolL;
    int masterR = rxSndGetMasterVolumeR();
    int famR    = s_seMasterVolR;

    rxSndQueueBegin(0x82, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_vol[0] = (short)(masterL * famL * volL >> 14);
    cmd->m_vol[1] = (short)(masterR * famR * volR >> 14);
    cmd->m_id     = voice->m_id;
    cmd->m_param  = 3;
    rxSndQueueEnd();
}

// FS2 0x0015e640
void rxSndSeSetPitch(int id, float pitch)
{
    if ((u_int)id >= 0x40 || !(s_se[id].m_flags & 1))
        return;

    rxSndQueueBegin(0x82, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = (int)(pitch * 4096.0f);
    cmd->m_id    = s_se[id].m_id;
    cmd->m_param = 4;
    rxSndQueueEnd();
}

// FS2 0x0015e6e8
void rxSndSeSetParam5(int id, float value)
{
    if ((u_int)id >= 0x40 || !(s_se[id].m_flags & 1))
        return;

    rxSndQueueBegin(0x82, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = (int)(value * 4096.0f);
    cmd->m_id    = s_se[id].m_id;
    cmd->m_param = 5;
    rxSndQueueEnd();
}

// FS2 0x0015e790
void rxSndSeSetParam(int id, int param, int value)
{
    if ((u_int)id >= 0x40 || !(s_se[id].m_flags & 1))
        return;

    rxSndQueueBegin(0x82, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = value;
    cmd->m_param = param;
    cmd->m_id    = s_se[id].m_id;
    rxSndQueueEnd();
}

// FS2 0x0015e830
void rxSndSeSetParam6(int id, int value)
{
    rxSndSeSetParam(id, 6, rxSndClamp(value, 0, 0x80));
}

// FS2 0x0015e870
void rxSndSeSetParam7(int id, int value)
{
    rxSndSeSetParam(id, 7, rxSndClamp(value, 0, 0x80));
}

// FS2 0x0015e8b0
void rxSndSeSetParam8(int id, int value)
{
    rxSndSeSetParam(id, 8, rxSndClamp(value, 0, 0x80));
}

// FS2 0x0015e8f0
void rxSndSeSetParam9(int id, int value)
{
    rxSndSeSetParam(id, 9, rxSndClamp(value, 0, 0x80));
}

// FS2 0x0015e930
void rxSndSeSetPan(int id, float pan)
{
    rxSndSeSetParam(id, 10, rxSndClamp((int)((pan + 1.0f) * 128.0f), 0, 0xff));
}

// FS2 0x0015e990
void rxSndSeSetParamB(int id, int value)
{
    if (value < 0)
        return;
    rxSndSeSetParam(id, 0xb, rxSndClamp(value, -1, 2));
}

// FS2 0x0015e9e8
void rxSndSeSetParamC(int id, int value)
{
    if (value < 0)
        return;
    rxSndSeSetParam(id, 0xc, rxSndClamp(value, -1, 0x7e));
}

/* FS2 only (no caller): the ids of the active SE voices into ids[] (null: just
 * count them); their count. */
// FS2 0x0015ea40
int rxSndSeGetActiveList(int *ids)
{
    int n = 0;

    for (RxSndSeVoice *p = s_seActive; p != NULL; p = p->m_next)
    {
        if (ids != NULL)
            ids[n] = p->m_id;
        n++;
    }
    return n;
}

// FS2 0x0015ea88
void rxSndSeInit(void)
{
    s_seMasterVolR = 0x80;
    s_seParamC       = -1;
    s_seActive     = NULL;
    s_seStopping   = NULL;
    s_seFree       = NULL;
    s_seMasterVolL = 0x80;
    s_seParamB       = -1;
    for (int i = 0; i < 0x40; i++)
    {
        s_se[i].m_id   = i;
        s_se[i].m_next = s_seFree;
        s_seFree       = &s_se[i];
    }
}

// FS2 0x0015eae8
void rxSndSeTerm(void)
{
}

// FS2 0x0015eaf0
void rxSndSeUpdate(const RxSndSeStatus *status)
{
    /* A voice the IOP no longer plays is marked ended (4) and stops itself
     * unless m_keep is set. */
    RxSndSeVoice *next;
    for (RxSndSeVoice *voice = s_seActive; voice != NULL; voice = next)
    {
        next = voice->m_next;
        for (int i = 0; i < status->m_count; i++)
        {
            const RxSndStatusEntry *e = &status->m_entry[i];
            if (e->m_id == (u_int)voice->m_id)
            {
                u_char st = e->m_status;
                voice->m_flags = (voice->m_flags & ~0xau) | (st & 2) | (st & 8);
                if (!(st & 2))
                    voice->m_flags |= 4;
                if (!(st & 2) && voice->m_keep == 0)
                    rxSndSeStop(voice->m_id, 0);
                break;
            }
        }
    }

    RxSndSeVoice **link = &s_seStopping;
    for (RxSndSeVoice *voice = *link; voice != NULL; voice = *link)
    {
        bool listed = false;
        for (int i = 0; i < status->m_count; i++)
        {
            if (status->m_entry[i].m_id == (u_int)voice->m_id)
            {
                listed = true;
                break;
            }
        }
        if (listed)
        {
            link = &voice->m_next;
            continue;
        }
        *link         = voice->m_next;
        voice->m_next = s_seFree;
        s_seFree      = voice;
    }
}

// FS2 0x0015ec60
short rxSndSeGetMasterVolumeL(void)
{
    return s_seMasterVolL;
}

// FS2 0x0015ec70
short rxSndSeGetMasterVolumeR(void)
{
    return s_seMasterVolR;
}

/* ---- PCM -------------------------------------------------------------- */

// FS2 0x0015ec80
void rxSndPcmSetMasterVolume(int volL, int volR)
{
    s_pcmMasterVolL = (short)rxSndClamp(volL, -0x80, 0x80);
    s_pcmMasterVolR = (short)rxSndClamp(volR, -0x80, 0x80);
    for (RxSndPcm *pcm = s_pcmActive; pcm != NULL; pcm = pcm->m_next)
        rxSndPcmSetVolume(pcm->m_id, pcm->m_volL, pcm->m_volR);
}

// FS2 0x0015ed18
void rxSndSetSysMix(int on)
{
    rxSndQueueBegin(0x31, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = 0xfcc;
    cmd->m_id    = 0;
    cmd->m_param = 0;
    if (on != 0)
        cmd->m_value = 0xffc;
    rxSndQueueEnd();
}

// FS2 0x0015ed70
void rxSndSetSysMute(int mute)
{
    rxSndQueueBegin(0x31, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_param = 2;
    cmd->m_id    = 0;
    if (mute == 0)
    {
        cmd->m_vol[0] = 0x7fff;
        cmd->m_vol[1] = 0x7fff;
    }
    else
    {
        cmd->m_vol[0] = 0;
        cmd->m_vol[1] = 0;
    }
    rxSndQueueEnd();
}

// FS2 0x0015ede0
void rxSndPcmSetParamB4(int value)
{
    rxSndQueueBegin(0xb4, 0, sizeof(int));
    int *cmd = (int *)rxSndQueueGetData();
    *cmd     = value;
    rxSndQueueEnd();
}

// FS2 0x0015ee20
int rxSndPcmOpen(int mode, u_int iopAddr, int blocks)
{
    RxSndPcm *pcm   = s_pcmFree;
    u_char    owned = 0;
    if (pcm == NULL)
    {
        if (rxSndGetDebugFlags() & 4)
            rxSysPrintf("rxsnd : can't allocate PCM stream slot\n");
        return -1;
    }

    int blockSize;
    if (mode == 0)
    {
        blockSize = 0x800;
    }
    else if (mode > 0 && mode < 4)
    {
        blockSize = 0x1000;
    }
    else
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : unknow PCM mode '%d'\n", mode);
        return -1;
    }

    if (iopAddr == 0)
    {
        iopAddr = rxSndIopAlloc(blocks * blockSize);
        owned   = 1;
        if (iopAddr == 0)
            return -1;
    }

    rxSndQueueBegin(0xb0, 0, sizeof(RxSndCmdPcmOpen));
    RxSndCmdPcmOpen *cmd = (RxSndCmdPcmOpen *)rxSndQueueGetData();
    cmd->m_blocks = blocks;
    cmd->m_id     = (u_char)pcm->m_id;
    cmd->m_mode   = (u_char)mode;
    cmd->m_iopBuf = iopAddr;
    rxSndQueueEnd();

    pcm->m_mode      = (u_char)rxSndClamp(mode, 0, 3);
    pcm->m_status    = 1;
    pcm->m_volR      = 0x80;
    pcm->m_ringSize  = blocks * blockSize;
    pcm->m_volL      = 0x80;
    pcm->m_alloc     = owned;
    pcm->m_user      = 0;
    pcm->m_blockSize = blockSize;
    pcm->m_iopBuf    = iopAddr;
    pcm->m_blocks    = blocks;
    pcm->m_queued    = blocks;
    pcm->m_writePos  = 0;
    pcm->m_pending   = 0;
    pcm->m_seq       = 0;
    pcm->m_callbacks = NULL;
    pcm->m_attr      = 0;
    rxSndPcmSetVolume(pcm->m_id, 0x80, 0x80);
    s_pcmFree   = pcm->m_next;
    pcm->m_next = s_pcmActive;
    s_pcmActive = pcm;

    if (rxSndGetDebugFlags() & 4)
        rxSysPrintf("rxsnd : open PCM '%d'\n        IOP %08X - %08X ( %d )\n", pcm->m_id, iopAddr,
                    iopAddr + pcm->m_blockSize * blocks, pcm->m_blockSize * blocks);
    return pcm->m_id;
}

// FS2 0x0015f010
void rxSndPcmClose(int id)
{
    rxSndCallBgCallHook("rxSndPcmClose");

    RxSndPcm **link = &s_pcmActive;
    RxSndPcm  *pcm;
    for (;;)
    {
        pcm = *link;
        if (pcm == NULL)
            return;
        if (pcm->m_id == id)
            break;
        link = &pcm->m_next;
    }

    rxSndQueueBegin(0xb2, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = 0;
    cmd->m_id    = pcm->m_id;
    cmd->m_param = 0;
    rxSndQueueEnd();

    while (pcm->m_callbacks != NULL)
    {
        RxSndPosCallback *cb = pcm->m_callbacks;
        pcm->m_callbacks     = cb->m_next;
        cb->m_next           = s_pcmCallbackFree;
        s_pcmCallbackFree    = cb;
    }
    pcm->m_status = 0;
    *link         = pcm->m_next;
    pcm->m_next   = s_pcmStopping;
    s_pcmStopping = pcm;
}

// FS2 0x0015f108
void rxSndPcmCloseAll(void)
{
    for (int i = 0; i < 0x20; i++)
        rxSndPcmClose(i);
}

// FS2 0x0015f148
u_int rxSndPcmGetStatus(int id)
{
    if ((u_int)id < 0x20 && (s_pcm[id].m_status & 1))
        return s_pcm[id].m_status;
    return 0;
}

// FS2 0x0015f190
int rxSndPcmGetFree(int id)
{
    if ((u_int)id >= 0x20 || !(s_pcm[id].m_status & 1))
        return 0;

    RxSndPcm *pcm = &s_pcm[id];
    return (pcm->m_blocks - pcm->m_queued) * pcm->m_blockSize - pcm->m_pending;
}

// FS2 0x0015f1f0
int rxSndPcmWrite(int id, const void *data, const void *dataR, int size)
{
    if ((u_int)id > 0x1f)
        return 0;
    RxSndPcm *pcm = &s_pcm[id];
    if (!(pcm->m_status & 1))
        return 0;

    int n = size;
    if (n > rxSndPcmGetFree(id))
        n = rxSndPcmGetFree(id);
    if (n < 0)
        n = 0;
    if (n < 1)
        return n;

    const u_char *src  = (const u_char *)data;
    const u_char *srcR = (const u_char *)dataR;
    int           rest = n;
    switch (pcm->m_mode)
    {
    case 1:
        /* Left halves in the first m_blocks * 0x800 bytes, right ones after. */
        do
        {
            int chunk = pcm->m_ringSize - pcm->m_writePos;
            if (rest < chunk)
                chunk = rest;
            if (chunk < 0)
                chunk = 0;
            rest -= chunk;
            int half = chunk / 2;
            rxSndIopWrite(pcm->m_iopBuf + pcm->m_writePos / 2, src, half);
            rxSndIopWrite(pcm->m_iopBuf + pcm->m_writePos / 2 + pcm->m_blocks * 0x800, srcR, half);
            pcm->m_writePos += chunk;
            if (pcm->m_writePos >= pcm->m_ringSize)
                pcm->m_writePos = 0;
            src += half;
            srcR += half;
        } while (rest > 0);
        break;
    case 0:
    case 2:
    case 3:
        do
        {
            int chunk = pcm->m_ringSize - pcm->m_writePos;
            if (rest < chunk)
                chunk = rest;
            if (chunk < 0)
                chunk = 0;
            rest -= chunk;
            rxSndIopWrite(pcm->m_iopBuf + pcm->m_writePos, src, chunk);
            src += chunk;
            pcm->m_writePos += chunk;
            if (pcm->m_writePos >= pcm->m_ringSize)
                pcm->m_writePos = 0;
        } while (rest > 0);
        break;
    }

    /* Announce the blocks this write completed. */
    int total      = pcm->m_pending + n;
    int count      = total / pcm->m_blockSize;
    pcm->m_pending = total % pcm->m_blockSize;
    rxSndQueueBegin(0xb1, 0, count * (int)sizeof(int) + (int)sizeof(RxSndCmdPcmData));
    RxSndCmdPcmData *cmd = (RxSndCmdPcmData *)rxSndQueueGetData();
    cmd->m_count = (u_char)count;
    cmd->m_id    = (u_char)pcm->m_id;
    int *seq     = (int *)(cmd + 1);
    for (int i = 0; i < count; i++)
        seq[i] = pcm->m_seq++;
    rxSndQueueEnd();
    pcm->m_queued += count;
    return n;
}

// FS2 0x0015f4d8
void rxSndPcmEnd(int id)
{
    if ((u_int)id >= 0x20 || !(s_pcm[id].m_status & 1))
        return;

    RxSndPcm   *pcm = &s_pcm[id];
    const void *pad;
    if (pcm->m_mode == 3)
    {
        /* Mode 3 pads with the attr word, its halves swapped. */
        short hi = (short)(pcm->m_attr >> 16);
        short lo = (short)pcm->m_attr;
        for (int i = 0; i < 0x400; i++)
        {
            s_pcmPad[i * 2]     = hi;
            s_pcmPad[i * 2 + 1] = lo;
        }
        pad = s_pcmPad;
    }
    else
    {
        pad = rxSndGetWorkBuffer();
    }

    if (pcm->m_pending > 0)
    {
        for (int rest = 0x1000 - pcm->m_pending; rest > 0;)
        {
            int chunk = rest > 0x1000 ? 0x1000 : rest;
            rest -= chunk;
            rxSndPcmWrite(id, pad, pad, chunk);
        }
    }

    rxSndQueueBegin(0xb2, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_param = 5;
    cmd->m_id    = s_pcm[id].m_id;
    cmd->m_value = 0;
    rxSndQueueEnd();
}

// FS2 0x0015f630
int rxSndPcmAddCallback(int id, int offset, RxSndPosFunc func, int arg)
{
    RxSndPosCallback *node = s_pcmCallbackFree;
    if ((u_int)id > 0x1f || !(s_pcm[id].m_status & 1) || node == NULL)
        return 0;

    /* The position is (block sequence number, offset in the block; modes
     * 1..3 count the offset per channel). */
    RxSndPcm *pcm   = &s_pcm[id];
    int       total = pcm->m_pending + offset;
    int       block;
    int       off;
    if (pcm->m_mode == 0)
    {
        off   = total % pcm->m_blockSize;
        block = pcm->m_seq + total / pcm->m_blockSize;
    }
    else if (pcm->m_mode < 4)
    {
        block = pcm->m_seq + total / pcm->m_blockSize;
        off   = total % pcm->m_blockSize / 2;
    }
    else
    {
        block = 0;
        off   = 0;
    }

    s_pcmCallbackFree = node->m_next;
    node->m_func      = func;
    node->m_arg       = arg;
    node->m_posHi     = (u_int)block;
    node->m_posLo     = (u_int)off;

    RxSndPosCallback **link = &pcm->m_callbacks;
    RxSndPosCallback  *cb;
    for (cb = *link; cb != NULL; cb = *link)
    {
        if (block < (int)cb->m_posHi || (block == (int)cb->m_posHi && off < (int)cb->m_posLo))
            break;
        link = &cb->m_next;
    }
    node->m_next = cb;
    *link        = node;
    return 1;
}

// FS2 0x0015f788
int rxSndPcmSetUserData(int id, int data)
{
    if ((u_int)id >= 0x20 || !(s_pcm[id].m_status & 1))
        return 0;
    int old          = s_pcm[id].m_user;
    s_pcm[id].m_user = data;
    return old;
}

// FS2 0x0015f7d0
int rxSndPcmGetUserData(int id)
{
    if ((u_int)id < 0x20 && (s_pcm[id].m_status & 1))
        return s_pcm[id].m_user;
    return 0;
}

// FS2 0x0015f818
void rxSndPcmSetAttr(int id, int value)
{
    if ((u_int)id >= 0x20 || !(s_pcm[id].m_status & 1))
        return;

    s_pcm[id].m_attr = value;
    rxSndQueueBegin(0xb2, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = value;
    cmd->m_id    = s_pcm[id].m_id;
    cmd->m_param = 2;
    rxSndQueueEnd();
}

// FS2 0x0015f8b8
void rxSndPcmPause(int id, int pause)
{
    if ((u_int)id >= 0x20 || !(s_pcm[id].m_status & 1))
        return;

    rxSndQueueBegin(0xb2, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = pause;
    cmd->m_id    = s_pcm[id].m_id;
    cmd->m_param = 1;
    rxSndQueueEnd();
}

// FS2 0x0015f958
void rxSndPcmSetVolume(int id, int volL, int volR)
{
    if ((u_int)id > 0x1f)
        return;
    RxSndPcm *pcm = &s_pcm[id];
    if (!(pcm->m_status & 1))
        return;

    pcm->m_volL = (short)rxSndClamp(volL, -0x80, 0x80);
    pcm->m_volR = (short)rxSndClamp(volR, -0x80, 0x80);
    int masterL = rxSndGetMasterVolumeL();
    int famL    = s_pcmMasterVolL;
    int masterR = rxSndGetMasterVolumeR();
    int famR    = s_pcmMasterVolR;

    rxSndQueueBegin(0xb2, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_vol[0] = (short)(masterL * famL * pcm->m_volL >> 14);
    cmd->m_vol[1] = (short)(masterR * famR * pcm->m_volR >> 14);
    cmd->m_id     = pcm->m_id;
    cmd->m_param  = 4;
    rxSndQueueEnd();
}

/* FS2 only (no caller): the ids of the active PCM streams into ids[] (null: just
 * count them); their count. */
// FS2 0x0015fa80
int rxSndPcmGetActiveList(int *ids)
{
    int n = 0;

    for (RxSndPcm *p = s_pcmActive; p != NULL; p = p->m_next)
    {
        if (ids != NULL)
            ids[n] = p->m_id;
        n++;
    }
    return n;
}

// FS2 0x0015fac8
void rxSndPcmInit(void)
{
    s_pcmCallbackFree = NULL;
    for (int i = 0; i < 256; i++)
    {
        s_pcmCallbacks[i].m_next = s_pcmCallbackFree;
        s_pcmCallbackFree        = &s_pcmCallbacks[i];
    }

    s_pcmMasterVolR = 0x80;
    s_pcmActive     = NULL;
    s_pcmStopping   = NULL;
    s_pcmFree       = NULL;
    s_pcmMasterVolL = 0x80;
    for (int i = 0; i < 0x20; i++)
    {
        s_pcm[i].m_id     = i;
        s_pcm[i].m_status = 0;
        s_pcm[i].m_next   = s_pcmFree;
        s_pcmFree         = &s_pcm[i];
    }
}

// FS2 0x0015fb58
void rxSndPcmTerm(void)
{
}

// FS2 0x0015fb60
void rxSndPcmUpdate(const RxSndPcmStatus *status)
{
    /* Playing streams take the IOP's status and fire the position callbacks
     * they have passed. */
    for (RxSndPcm *pcm = s_pcmActive; pcm != NULL; pcm = pcm->m_next)
    {
        for (int i = 0; i < status->m_count; i++)
        {
            const RxSndPcmStatusEntry *e = &status->m_entry[i];
            if (e->m_id != (u_int)pcm->m_id)
                continue;

            u_char st     = e->m_status;
            pcm->m_status = (u_char)((pcm->m_status & 0xf1) | (st & 2) | (st & 8) | (st & 4));
            pcm->m_queued = e->m_2;
            if (e->m_status & 2)
            {
                RxSndPosCallback **link = &pcm->m_callbacks;
                for (RxSndPosCallback *cb = *link; cb != NULL; cb = *link)
                {
                    if ((int)cb->m_posHi < e->m_posHi
                        || ((int)cb->m_posHi == e->m_posHi && (int)cb->m_posLo <= e->m_posLo))
                    {
                        if (cb->m_func != NULL)
                            cb->m_func(pcm->m_id, cb->m_arg);
                        *link             = cb->m_next;
                        cb->m_next        = s_pcmCallbackFree;
                        s_pcmCallbackFree = cb;
                    }
                    else
                    {
                        link = &cb->m_next;
                    }
                }
            }
            break;
        }
    }

    /* Stopped streams give back their IOP buffer once the IOP drops them. */
    RxSndPcm **link = &s_pcmStopping;
    for (RxSndPcm *pcm = *link; pcm != NULL; pcm = *link)
    {
        bool listed = false;
        for (int i = 0; i < status->m_count; i++)
        {
            if (status->m_entry[i].m_id == (u_int)pcm->m_id)
            {
                listed = true;
                break;
            }
        }
        if (listed)
        {
            link = &pcm->m_next;
            continue;
        }
        if ((pcm->m_alloc & 1) && pcm->m_iopBuf != 0)
            rxSndIopFree(pcm->m_iopBuf);
        *link       = pcm->m_next;
        pcm->m_next = s_pcmFree;
        s_pcmFree   = pcm;
    }
}

// FS2 0x0015fd70
short rxSndPcmGetMasterVolumeL(void)
{
    return s_pcmMasterVolL;
}

// FS2 0x0015fd80
short rxSndPcmGetMasterVolumeR(void)
{
    return s_pcmMasterVolR;
}

/* ---- ADPCM ------------------------------------------------------------ */

// FS2 0x0015fd90
void rxSndAdpcmSetMasterVolume(int volL, int volR)
{
    s_adpcmMasterVolL = (short)rxSndClamp(volL, -0x80, 0x80);
    s_adpcmMasterVolR = (short)rxSndClamp(volR, -0x80, 0x80);
    for (RxSndAdpcm *s = s_adpcmActive; s != NULL; s = s->m_next)
        rxSndAdpcmSetVolume(s->m_id, s->m_volL, s->m_volR);
}

// FS2 0x0015fe28
void rxSndAdpcmSetDefaultParam6(int value)
{
    s_adpcmParam6 = rxSndClamp(value, 0, 2);
}

// FS2 0x0015fe58
void rxSndAdpcmSetDefaultParam7(int value)
{
    s_adpcmParam7 = rxSndClamp(value, 0, 0x7f);
}

// FS2 0x0015fe88
int rxSndAdpcmOpen(int mode, int rate, u_int iopAddr, int iopKB, u_int spuAddr, int spuKB)
{
    return rxSndAdpcmOpenBytes(mode, rate, iopAddr, iopKB << 10, spuAddr, spuKB << 10, -1);
}

// FS2 0x0015fea8
int rxSndAdpcmOpenEx(int mode, int rate, u_int iopAddr, int iopKB, u_int spuAddr, int spuKB,
                     int arg)
{
    return rxSndAdpcmOpenBytes(mode, rate, iopAddr, iopKB << 10, spuAddr, spuKB << 10, arg);
}

// FS2 0x0015fec8
int rxSndAdpcmOpenBytes(int mode, int rate, u_int iopAddr, int iopSize, u_int spuAddr,
                        int spuSize, int arg)
{
    RxSndAdpcm *s        = s_adpcmFree;
    u_int       owned    = 0;
    int         channels = 0;

    if (s == NULL)
    {
        if (rxSndGetDebugFlags() & 2)
            rxSysPrintf("rxsnd : can't allocate ADPCM stream slot\n");
        goto fail;
    }

    if (mode & 0x1000000)
    {
        channels = mode & 0xff;
        mode     = 0x1000000;
    }
    else if (mode == 0)
    {
        channels = 1;
    }
    else if (mode > 0 && mode < 3)
    {
        channels = 2;
    }
    else
    {
        if (rxSndGetDebugFlags() & 1)
            rxSysPrintf("rxsnd : no supported ADPCM stream mode '%d'\n", mode);
        goto fail;
    }

    if (iopAddr == 0)
    {
        iopAddr = rxSndIopAlloc(iopSize * channels);
        if (iopAddr == 0)
            goto fail;
        owned = 1;
    }
    if (spuAddr == 0)
    {
        spuAddr = rxSndSpuAlloc(spuSize * channels);
        if (spuAddr == 0)
            goto fail;
        owned |= 2;
    }

    {
        rxSndQueueBegin(0xa0, 0, channels * sizeof(RxSndCmdAdpcmBuf) + sizeof(RxSndCmdAdpcmOpen));
        RxSndCmdAdpcmOpen *cmd = (RxSndCmdAdpcmOpen *)rxSndQueueGetData();
        cmd->m_arg      = (short)arg;
        cmd->m_id       = s->m_id;
        cmd->m_channels = (u_char)channels;
        cmd->m_iopSize  = iopSize;
        cmd->m_spuSize  = spuSize;
        cmd->m_pitch    = (short)((rate << 12) / 48000);
        RxSndCmdAdpcmBuf *buf = (RxSndCmdAdpcmBuf *)(cmd + 1);
        for (int i = 0; i < channels; i++)
        {
            buf[i].m_iopBuf = iopAddr + iopSize * i;
            buf[i].m_spuBuf = spuAddr + spuSize * i;
        }
        rxSndQueueEnd();
    }

    s->m_status     = 1;
    s->m_mode       = mode;
    s->m_alloc      = (u_char)owned;
    s->m_volR       = 0x80;
    s->m_end        = 0;
    s->m_channels   = (u_char)channels;
    s->m_user       = 0;
    s->m_volL       = 0x80;
    s->m_iopBuf     = iopAddr;
    s->m_bufSize    = iopSize;
    s->m_bufFill    = 0;
    s->m_writePos   = 0;
    s->m_pending    = 0;
    s->m_spuBuf     = spuAddr;
    s->m_spuSize    = spuSize;
    s->m_spuPos     = 0;
    s->m_writtenHi  = 0;
    s->m_writtenLo  = 0;
    s->m_position   = 0;
    s->m_callbacks  = NULL;
    s_adpcmFree     = s->m_next;
    s->m_next       = s_adpcmActive;
    s_adpcmActive   = s;

    if (rxSndGetDebugFlags() & 4)
        rxSysPrintf("rxsnd : open ADPCM '%d'\n        IOP   %08X - %08X ( %d )\n"
                    "        ADPCM %08X - %08X ( %d )\n",
                    s->m_id, iopAddr, iopAddr + iopSize * channels, iopSize * channels, spuAddr,
                    spuAddr + spuSize * channels, spuSize * channels);
    rxSndAdpcmSetVolume(s->m_id, 0x80, 0x80);
    if (s_adpcmParam6 > 0)
        rxSndAdpcmSetParam6(s->m_id, s_adpcmParam6);
    if (s_adpcmParam7 > 0)
        rxSndAdpcmSetParam7(s->m_id, s_adpcmParam7);
    return s->m_id;

fail:
    if ((owned & 1) && iopAddr != 0)
        rxSndIopFree(iopAddr);
    if ((owned & 2) && spuAddr != 0)
        rxSndSpuFree(spuAddr);
    return -1;
}

// FS2 0x001601e0
void rxSndAdpcmClose(int id)
{
    rxSndCallBgCallHook("rxSndAdpcmClose");

    RxSndAdpcm **link = &s_adpcmActive;
    RxSndAdpcm  *s;
    for (;;)
    {
        s = *link;
        if (s == NULL)
            return;
        if (s->m_id == (u_int)id)
            break;
        link = &s->m_next;
    }

    rxSndQueueBegin(0xa2, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = 0;
    cmd->m_id    = s->m_id;
    cmd->m_param = 0;
    rxSndQueueEnd();

    while (s->m_callbacks != NULL)
    {
        RxSndPosCallback *cb = s->m_callbacks;
        s->m_callbacks       = cb->m_next;
        cb->m_next           = s_adpcmCallbackFree;
        s_adpcmCallbackFree  = cb;
    }
    s->m_status     = 0;
    *link           = s->m_next;
    s->m_next       = s_adpcmStopping;
    s_adpcmStopping = s;
}

// FS2 0x001602d8
void rxSndAdpcmCloseAll(void)
{
    for (int i = 0; i < 0x30; i++)
        rxSndAdpcmClose(i);
}

// FS2 0x00160318
u_int rxSndAdpcmGetStatus(int id)
{
    if ((u_int)id < 0x30 && (s_adpcm[id].m_status & 1))
        return s_adpcm[id].m_status;
    return 0;
}

// FS2 0x00160360
int rxSndAdpcmGetFreeSize(int id, int raw)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return 0;

    RxSndAdpcm *s    = &s_adpcm[id];
    int         free = s->m_bufSize - s->m_bufFill;
    if (s->m_mode < 3)
    {
        if (s->m_mode > 0)
            free *= 2;
    }
    else if (s->m_mode == 0x1000000)
    {
        free *= s->m_channels;
    }
    if (raw == 0)
        free -= s->m_channels * 0x80;
    return free;
}

// FS2 0x00160400
int rxSndAdpcmGetFree(int id)
{
    return rxSndAdpcmGetFreeSize(id, 0);
}

/* Copies `count` 16-byte blocks from src (one every `stride` bytes) to dst,
 * clearing each block's flag byte (inlined in the ROM). */
static inline void rxSndAdpcmStage(u_char *dst, const u_char *src, int stride, u_int count)
{
    for (u_int i = 0; i < count; i++)
    {
        memcpy(dst, src, 0x10);
        dst[1]  = 0;
        dst    += 0x10;
        src    += stride;
    }
}

/* The last block before each wrap of the SPU buffer gets flags 3 (loop end
 * + repeat): `pos` is the chunk's position in the SPU buffer (inlined).
 * PC: a chunk that does not start on a block (a write of an odd size)
 * would put the flag before the stage buffer; skipped. */
static inline void rxSndAdpcmMarkLoops(u_char *stage, int pos, int spuSize, int n)
{
    for (int k = spuSize; k <= pos + n; k += spuSize)
    {
        int at = k - pos - 0x10 + 1;
        if (at >= 0)
            stage[at] = 3;
    }
}

/* Chunks are min(run, 0x4000) for modes 0 / 1, not min(run left, 0x4000):
 * a run over 0x4000 bytes overshoots it, as in the ROM (mode 2 and the
 * interleaved mode take what is left). */
// FS2 0x00160418
int rxSndAdpcmWriteEx(int id, const void *data, const void *dataR, int size, int raw)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return 0;
    RxSndAdpcm *s = &s_adpcm[id];
    if (s->m_end == 1)
        return 0;

    /* The bytes per channel. */
    int per = size;
    if (s->m_mode < 3)
    {
        if (s->m_mode > 0)
            per = size / 2;
    }
    else if (s->m_mode == 0x1000000)
    {
        per = size / s->m_channels / 0x10 * 0x10;
    }
    int free = rxSndAdpcmGetFreeSize(id, raw);
    int n    = per < free ? per : free;
    if (n < 0)
        n = 0;
    if (n <= 0)
        return size;

    const u_char *src  = (const u_char *)data;
    const u_char *srcR = (const u_char *)dataR;
    if (s->m_mode == 0 || s->m_mode == 1)
    {
        bool two = s->m_mode == 1;
        for (int done = 0; done < n;)
        {
            int run = n - done;
            if (s->m_bufSize - s->m_writePos <= run)
                run = s->m_bufSize - s->m_writePos;
            if (run < 0)
                run = 0;
            for (int sent = 0; sent < run;)
            {
                int chunk = (u_int)run < 0x4001 ? run : 0x4000;
                rxSndAdpcmStage(s_adpcmStage[0], src, 0x10, (u_int)chunk >> 4);
                if (two)
                    rxSndAdpcmStage(s_adpcmStage[1], srcR, 0x10, (u_int)chunk >> 4);
                rxSndAdpcmMarkLoops(s_adpcmStage[0], s->m_spuPos, s->m_spuSize, chunk);
                if (two)
                    rxSndAdpcmMarkLoops(s_adpcmStage[1], s->m_spuPos, s->m_spuSize, chunk);
                s->m_spuPos = (s->m_spuPos + chunk) % s->m_spuSize;
                rxSndIopWrite(s->m_iopBuf + s->m_writePos, s_adpcmStage[0], chunk);
                if (two)
                    rxSndIopWrite(s->m_iopBuf + s->m_writePos + s->m_bufSize, s_adpcmStage[1], chunk);
                s->m_writePos += chunk;
                if (s->m_bufSize <= s->m_writePos)
                    s->m_writePos = 0;
                sent       += chunk;
                src        += chunk;
                if (two)
                    srcR   += chunk;
                s->m_bufFill += chunk;
            }
            done += run;
        }
    }
    else if (s->m_mode == 2)
    {
        /* Stereo interleaved by block: L, R, L, R ... */
        for (int left = n; left > 0;)
        {
            int run = s->m_bufSize - s->m_writePos;
            if (left < run)
                run = left;
            for (int rest = run; rest > 0;)
            {
                int chunk = (u_int)rest < 0x4001 ? rest : 0x4000;
                rxSndAdpcmStage(s_adpcmStage[0], src, 0x20, (u_int)chunk >> 4);
                rxSndAdpcmStage(s_adpcmStage[1], src + 0x10, 0x20, (u_int)chunk >> 4);
                rxSndAdpcmMarkLoops(s_adpcmStage[0], s->m_spuPos, s->m_spuSize, chunk);
                rxSndAdpcmMarkLoops(s_adpcmStage[1], s->m_spuPos, s->m_spuSize, chunk);
                s->m_spuPos = (s->m_spuPos + chunk) % s->m_spuSize;
                rxSndIopWrite(s->m_iopBuf + s->m_writePos, s_adpcmStage[0], chunk);
                rxSndIopWrite(s->m_iopBuf + s->m_writePos + s->m_bufSize, s_adpcmStage[1], chunk);
                s->m_writePos += chunk;
                if (s->m_bufSize <= s->m_writePos)
                    s->m_writePos = 0;
                src          += chunk * 2;
                s->m_bufFill += chunk;
                rest         -= chunk;
            }
            left -= run;
        }
    }
    else if (s->m_mode == 0x1000000)
    {
        /* m_channels channels interleaved by block. */
        int channels = s->m_channels;
        for (int left = n; left > 0;)
        {
            u_int chunk = (u_int)(s->m_bufSize - s->m_writePos);
            if (left < (int)chunk)
                chunk = left;
            if (chunk > 0x4000)
                chunk = 0x4000;
            for (int ch = 0; ch < channels; ch++)
            {
                rxSndAdpcmStage(s_adpcmStage[0], src + ch * 0x10, channels * 0x10, chunk >> 4);
                rxSndAdpcmMarkLoops(s_adpcmStage[0], s->m_spuPos, s->m_spuSize, (int)chunk);
                rxSndIopWrite(s->m_iopBuf + s->m_bufSize * ch + s->m_writePos, s_adpcmStage[0],
                              (int)chunk);
            }
            left         -= chunk;
            src          += chunk * channels;
            s->m_spuPos   = (s->m_spuPos + (int)chunk) % s->m_spuSize;
            s->m_writePos = (s->m_writePos + (int)chunk) % s->m_bufSize;
            s->m_bufFill += chunk;
        }
    }

    /* Announce the whole 0x40 units; the rest waits for the next write. */
    rxSndQueueBegin(0xa1, 0, sizeof(RxSndCmdAdpcmData));
    RxSndCmdAdpcmData *cmd = (RxSndCmdAdpcmData *)rxSndQueueGetData();
    int pending   = s->m_pending;
    s->m_pending  = (pending + n) & 0x3f;
    cmd->m_size   = (pending + n) & ~0x3f;
    cmd->m_id     = s->m_id;
    cmd->m_end    = 0;
    rxSndQueueEnd();

    u_int lo       = s->m_writtenLo;
    s->m_writtenLo = lo + n;
    if (s->m_writtenLo < lo)
        s->m_writtenHi++;
    return size;
}

// FS2 0x00160cd0
int rxSndAdpcmWrite(int id, const void *data, const void *dataR, int size)
{
    return rxSndAdpcmWriteEx(id, data, dataR, size, 0);
}

// FS2 0x00160ce8
void rxSndAdpcmEnd(int id)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return;
    RxSndAdpcm *s = &s_adpcm[id];

    /* Pad the last unit with the (zero) work buffer, then the end block. */
    int mode = s->m_mode;
    if (s->m_pending > 0)
    {
        int pad;
        if (mode < 4)
            pad = mode < 1 ? 0x40 - s->m_pending : (0x40 - s->m_pending) * 2;
        else if (mode == 0x1000000)
            pad = (0x40 - s->m_pending) * s->m_channels;
        else
            pad = 0x40 - s->m_pending;
        rxSndAdpcmWriteEx(id, rxSndGetWorkBuffer(), rxSndGetWorkBuffer(), pad, 1);
        mode = s->m_mode;
    }

    if (mode < 4 && mode >= 1)
    {
        rxSndIopWrite(s->m_iopBuf + s->m_writePos, s_adpcmEndBlock, 0x40);
        rxSndIopWrite(s->m_iopBuf + s->m_writePos + s->m_bufSize, s_adpcmEndBlock, 0x40);
    }
    else if (mode == 0x1000000)
    {
        for (int ch = 0; ch < s->m_channels; ch++)
            rxSndIopWrite(s->m_iopBuf + s->m_bufSize * ch + s->m_writePos, s_adpcmEndBlock, 0x40);
    }
    else
    {
        rxSndIopWrite(s->m_iopBuf + s->m_writePos, s_adpcmEndBlock, 0x40);
    }

    rxSndQueueBegin(0xa1, 0, sizeof(RxSndCmdAdpcmData));
    RxSndCmdAdpcmData *cmd = (RxSndCmdAdpcmData *)rxSndQueueGetData();
    cmd->m_size = 0x40;
    cmd->m_id   = s->m_id;
    cmd->m_end  = 1;
    rxSndQueueEnd();
    s->m_end = 1;
}

// FS2 0x00160ed0
int rxSndAdpcmAddCallback(int id, int offset, RxSndPosFunc func, int arg)
{
    RxSndPosCallback *node = s_adpcmCallbackFree;
    if ((u_int)id > 0x2f || !(s_adpcm[id].m_status & 1) || node == NULL)
        return 0;

    /* The position counts bytes per channel, as m_written. */
    RxSndAdpcm *s = &s_adpcm[id];
    if (s->m_mode < 4)
    {
        if (s->m_mode > 0)
            offset /= 2;
    }
    else if (s->m_mode == 0x1000000)
    {
        offset /= s->m_channels;
    }
    u_long pos    = ((u_long)s->m_writtenHi << 32 | s->m_writtenLo) + (u_int)offset;
    node->m_posHi = (u_int)(pos >> 32);
    node->m_func  = func;
    node->m_arg   = arg;
    node->m_posLo = (u_int)pos;

    RxSndPosCallback **link = &s->m_callbacks;
    RxSndPosCallback  *cb;
    for (cb = *link; cb != NULL; cb = *link)
    {
        if (node->m_posHi < cb->m_posHi
            || (node->m_posHi == cb->m_posHi && node->m_posLo < cb->m_posLo))
            break;
        link = &cb->m_next;
    }
    s_adpcmCallbackFree = node->m_next;
    node->m_next        = cb;
    *link               = node;
    return 1;
}

// FS2 0x00161028
int rxSndAdpcmSetUserData(int id, int data)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return 0;
    int old            = s_adpcm[id].m_user;
    s_adpcm[id].m_user = data;
    return old;
}

// FS2 0x00161070
int rxSndAdpcmGetUserData(int id)
{
    if ((u_int)id < 0x30 && (s_adpcm[id].m_status & 1))
        return s_adpcm[id].m_user;
    return 0;
}

// FS2 0x001610b8
void rxSndAdpcmPause(int id, int pause)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return;

    rxSndQueueBegin(0xa2, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = pause;
    cmd->m_id    = s_adpcm[id].m_id;
    cmd->m_param = 1;
    rxSndQueueEnd();
}

// FS2 0x00161158
void rxSndAdpcmSetAttr(int id, int value)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return;

    rxSndQueueBegin(0xa2, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = value;
    cmd->m_id    = s_adpcm[id].m_id;
    cmd->m_param = 2;
    rxSndQueueEnd();
}

// FS2 0x001611f8
void rxSndAdpcmSetVolume(int id, int volL, int volR)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return;

    RxSndAdpcm *s = &s_adpcm[id];
    s->m_volL = (short)rxSndClamp(volL, -0x80, 0x80);
    s->m_volR = (short)rxSndClamp(volR, -0x80, 0x80);
    int   masterL = rxSndGetMasterVolumeL();
    int   famL    = s_adpcmMasterVolL;
    int   masterR = rxSndGetMasterVolumeR();
    short outR    = (short)(masterR * s_adpcmMasterVolR * s->m_volR >> 14);
    short outL    = (short)(masterL * famL * s->m_volL >> 14);

    if (s->m_mode < 4 && s->m_mode > 0)
    {
        /* Modes 1..3 take the volume per channel: left on channel 0, right
         * on channel 1. */
        rxSndQueueBegin(0xa4, 0, sizeof(RxSndCmdChannelSet));
        RxSndCmdChannelSet *ch = (RxSndCmdChannelSet *)rxSndQueueGetData();
        ch->m_vol[0]  = outL;
        ch->m_id      = s->m_id;
        ch->m_vol[1]  = 0;
        ch->m_channel = 0;
        ch->m_param   = 3;
        ch->m_6       = 0;
        rxSndQueueEnd();

        rxSndQueueBegin(0xa4, 0, sizeof(RxSndCmdChannelSet));
        ch = (RxSndCmdChannelSet *)rxSndQueueGetData();
        ch->m_vol[1]  = outR;
        ch->m_id      = s->m_id;
        ch->m_channel = 1;
        ch->m_param   = 3;
        ch->m_6       = 0;
        ch->m_vol[0]  = 0;
        rxSndQueueEnd();
    }
    else
    {
        rxSndQueueBegin(0xa2, 0, sizeof(RxSndCmdSet));
        RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
        cmd->m_vol[1] = outR;
        cmd->m_id     = s->m_id;
        cmd->m_param  = 3;
        cmd->m_vol[0] = outL;
        rxSndQueueEnd();
    }
}

// FS2 0x00161398
void rxSndAdpcmSetChannelVolume(int id, int channel, int volL, int volR)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return;
    if (channel < 0 || channel > s_adpcm[id].m_channels - 1)
        return;

    int outL = rxSndClamp(volL, -0x80, 0x80);
    int outR = rxSndClamp(volR, -0x80, 0x80);
    rxSndQueueBegin(0xa4, 0, sizeof(RxSndCmdChannelSet));
    RxSndCmdChannelSet *ch = (RxSndCmdChannelSet *)rxSndQueueGetData();
    ch->m_vol[1]  = (short)outR;
    ch->m_channel = (u_short)channel;
    ch->m_vol[0]  = (short)outL;
    ch->m_id      = s_adpcm[id].m_id;
    ch->m_param   = 3;
    ch->m_6       = 0;
    rxSndQueueEnd();
}

// FS2 0x001614d0
void rxSndAdpcmSetPitch(int id, float pitch)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return;

    rxSndQueueBegin(0xa2, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_param = 4;
    cmd->m_value = (int)(pitch * 4096.0f);
    cmd->m_id    = s_adpcm[id].m_id;
    rxSndQueueEnd();
}

// FS2 0x00161580
void rxSndAdpcmSetChannelParam5(int id, int channel, int value)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return;

    rxSndQueueBegin(0xa4, 0, sizeof(RxSndCmdChannelSet));
    RxSndCmdChannelSet *ch = (RxSndCmdChannelSet *)rxSndQueueGetData();
    ch->m_value   = value;
    ch->m_channel = (u_short)channel;
    ch->m_id      = s_adpcm[id].m_id;
    ch->m_param   = 5;
    ch->m_6       = 0;
    rxSndQueueEnd();
}

// FS2 0x00161630
void rxSndAdpcmSetParam6(int id, int value)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return;

    rxSndQueueBegin(0xa2, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = rxSndClamp(value, 0, 2);
    cmd->m_id    = s_adpcm[id].m_id;
    cmd->m_param = 6;
    rxSndQueueEnd();
}

// FS2 0x001616f8
void rxSndAdpcmSetParam7(int id, int value)
{
    if ((u_int)id >= 0x30 || !(s_adpcm[id].m_status & 1))
        return;

    rxSndQueueBegin(0xa2, 0, sizeof(RxSndCmdSet));
    RxSndCmdSet *cmd = (RxSndCmdSet *)rxSndQueueGetData();
    cmd->m_value = rxSndClamp(value, 0, 0x7f);
    cmd->m_id    = s_adpcm[id].m_id;
    cmd->m_param = 7;
    rxSndQueueEnd();
}

// callcheck: __muldi3 (the u_long multiply)
// FS2 0x001617c0
u_long rxSndAdpcmGetPosition(int id)
{
    if ((u_int)id < 0x30 && (s_adpcm[id].m_status & 1))
        return s_adpcm[id].m_position * s_adpcm[id].m_channels;   /* FS2: __muldi3 (RoR: the position) */
    return 0;
}

/* FS2 only (no caller): the ids of the active ADPCM streams into ids[] (null: just
 * count them); their count. */
// FS2 0x00161820
int rxSndAdpcmGetActiveList(int *ids)
{
    int n = 0;

    for (RxSndAdpcm *p = s_adpcmActive; p != NULL; p = p->m_next)
    {
        if (ids != NULL)
            ids[n] = p->m_id;
        n++;
    }
    return n;
}

// FS2 0x00161868
void rxSndAdpcmInit(void)
{
    s_adpcmCallbackFree = NULL;
    for (int i = 0; i < 256; i++)
    {
        s_adpcmCallbacks[i].m_next = s_adpcmCallbackFree;
        s_adpcmCallbackFree        = &s_adpcmCallbacks[i];
    }

    s_adpcmMasterVolR = 0x80;
    s_adpcmActive     = NULL;
    s_adpcmStopping   = NULL;
    s_adpcmFree       = NULL;
    s_adpcmMasterVolL = 0x80;
    for (int i = 0; i < 0x30; i++)
    {
        s_adpcm[i].m_id     = (u_char)i;
        s_adpcm[i].m_status = 0;
        s_adpcm[i].m_next   = s_adpcmFree;
        s_adpcmFree         = &s_adpcm[i];
    }
    s_adpcmParam7 = 0;
    s_adpcmParam6 = 0;
}

// FS2 0x00161900
void rxSndAdpcmTerm(void)
{
}

// FS2 0x00161908
void rxSndAdpcmUpdate(const RxSndAdpcmStatus *status)
{
    for (RxSndAdpcm *s = s_adpcmActive; s != NULL; s = s->m_next)
    {
        for (int i = 0; i < status->m_count; i++)
        {
            const RxSndAdpcmStatusEntry *e = &status->m_entry[i];
            if (e->m_id != s->m_id)
                continue;

            u_char st   = e->m_status;
            s->m_status = (u_char)((s->m_status & 0xf1) | (st & 2) | (st & 8) | (st & 4));
            s->m_bufFill = s->m_bufSize - e->m_free;
            /* FS2: hi:lo as one 64-bit number (RoR ORed the two words). */
            s->m_position = ((u_long)e->m_posHi << 32) | e->m_posLo;
            if (e->m_status & 2)
            {
                /* Fire the callbacks the play position has passed. */
                RxSndPosCallback **link = &s->m_callbacks;
                for (RxSndPosCallback *cb = *link; cb != NULL; cb = *link)
                {
                    if (cb->m_posHi < e->m_posHi
                        || (cb->m_posHi == e->m_posHi && cb->m_posLo < e->m_posLo))
                    {
                        if (cb->m_func != NULL)
                            cb->m_func(s->m_id, cb->m_arg);
                        *link               = cb->m_next;
                        cb->m_next          = s_adpcmCallbackFree;
                        s_adpcmCallbackFree = cb;
                    }
                    else
                    {
                        link = &cb->m_next;
                    }
                }
            }
            break;
        }
    }

    /* Closed streams give back their IOP / SPU buffers once the IOP drops
     * them. */
    RxSndAdpcm **link = &s_adpcmStopping;
    for (RxSndAdpcm *s = *link; s != NULL; s = *link)
    {
        bool listed = false;
        for (int i = 0; i < status->m_count; i++)
        {
            if (status->m_entry[i].m_id == s->m_id)
            {
                listed = true;
                break;
            }
        }
        if (listed)
        {
            link = &s->m_next;
            continue;
        }
        if ((s->m_alloc & 1) && s->m_iopBuf != 0)
            rxSndIopFree(s->m_iopBuf);
        if ((s->m_alloc & 2) && s->m_spuBuf != 0)
            rxSndSpuFree(s->m_spuBuf);
        *link       = s->m_next;
        s->m_next   = s_adpcmFree;
        s_adpcmFree = s;
    }
}

// FS2 0x00161b60
short rxSndAdpcmGetMasterVolumeL(void)
{
    return s_adpcmMasterVolL;
}

// FS2 0x00161b70
short rxSndAdpcmGetMasterVolumeR(void)
{
    return s_adpcmMasterVolR;
}

/* ---- transport -------------------------------------------------------- */

// FS2 0x00161b80
int rxSndCreateSema(int maxCount, int initCount)
{
    struct SemaParam param;

    memset(&param, 0, sizeof(param));
    param.maxCount  = maxCount;
    param.initCount = initCount;
    return CreateSema(&param);
}

// FS2 0x00161bd0
int rxSndBindRpc(sceSifClientData *cd, u_int rpcNo)
{
    if (rxSndGetDebugFlags() & 4)
        rxSysPrintf("binding IOP module %08X ...\n", rpcNo);
    do
    {
        if (sceSifBindRpc(cd, rpcNo, 0) < 0)
        {
            if (rxSndGetDebugFlags() & 1)
                rxSysPrintf("bind error '%08X'\n", rpcNo);
            return 0;
        }
        for (int i = 9999; i >= 0; i--)
            rxSndWait();
    } while (cd->serve == NULL);
    FlushCache(0);
    if (rxSndGetDebugFlags() & 4)
        rxSysPrintf("... binding completed\n");
    s_rpcSema = rxSndCreateSema(1, 1);
    return 1;
}

// FS2 0x00161cc8
void rxSndUnbindRpc(sceSifClientData *cd)
{
    (void)cd;
    DeleteSema(s_rpcSema);
}

/* The RPC's end function (interrupt context on the EE: iSignalSema, then
 * sync.p and ei, which the host does not need). */
// FS2 0x00161ce8
void rxSndRpcEndFunc(void)
{
    iSignalSema(s_rpcSema);
}

// FS2 0x00161d18
void rxSndCallRpc(sceSifClientData *cd, u_int fno, void *send, int sendSize,
                  void *reply, int replySize)
{
    rxThDisableDispatch();
    WaitSema(s_rpcSema);
    FlushCache(0);
    sceSifCallRpc(cd, fno, 1, send, sendSize, reply, replySize, rxSndRpcEndFunc, NULL);
    rxThEnableDispatch();
    while (sceSifCheckStatRpc(&cd->rpcd) == 1)
        ;
    FlushCache(0);
}

// FS2 0x00161de0
void rxSndRpcInit(void)
{
    while (rxSndBindRpc(&s_rpcClient, 0xd000) == 0)
        ;
}

// FS2 0x00161e18
void rxSndRpcTerm(void)
{
    rxSndUnbindRpc(&s_rpcClient);
}

// FS2 0x00161e38
u_int rxSndRpcCommand(u_int cmd, void *reply, int replySize, const void *send, int sendSize)
{
    const u_int *src = (const u_int *)send;
    for (int i = 0; i < (sendSize + 3) / 4; i++)
        s_rpcSend[rxSndGetBufferBank()][i] = *src++;

    int sendBank  = rxSndGetBufferBank();
    int replyBank = rxSndGetBufferBank();
    rxSndCallRpc(&s_rpcClient, cmd, s_rpcSend[sendBank], (sendSize + 0x3f) & ~0x3f,
                 s_rpcReply[replyBank], (replySize + 0x43) & ~0x3f);

    u_int *dst = (u_int *)reply;
    for (int i = 0; i < (replySize + 3) / 4; i++)
        *dst++ = s_rpcReply[rxSndGetBufferBank()][1 + i];
    return s_rpcReply[rxSndGetBufferBank()][0];
}

// FS2 0x00161fc8
void rxSndQueueBegin(u_int cmd, int replySize, int dataSize)
{
    /* FS2: keeps room for the end marker and the flush's rounding (RoR: + 0xc
     * and + 0, which ran over). */
    if ((u_int)(s_queuePos + dataSize + 0x58) > 0x1000 || (u_int)(s_queueReplyPos + replySize + 0x40) > 0x1000)
        rxSndQueueFlush();

    RxSndQueueHeader *head = (RxSndQueueHeader *)&s_queue[s_queuePos];
    head->m_dataSize  = dataSize;
    head->m_cmd       = cmd;
    head->m_replySize = replySize;
}

// FS2 0x00162060
void *rxSndQueueGetReply(void)
{
    return &s_queueReply[s_queueReplyPos];
}

// FS2 0x00162078
void *rxSndQueueGetData(void)
{
    return &s_queue[s_queuePos + sizeof(RxSndQueueHeader)];
}

// FS2 0x00162090
void rxSndQueueEnd(void)
{
    RxSndQueueHeader *head = (RxSndQueueHeader *)&s_queue[s_queuePos];
    s_queuePos      += head->m_dataSize + sizeof(RxSndQueueHeader);
    s_queueReplyPos += head->m_replySize;
}

/* Sends the queue as one call (fno 0x200), closed by an empty header; the
 * replies land in s_queueReply from offset 4 (word 0 is the call's own). */
// FS2 0x001620d8
void rxSndQueueFlush(void)
{
    int pos      = s_queuePos;
    int replyPos = s_queueReplyPos;

    if (s_queuePos > 0)
    {
        RxSndQueueHeader *end = (RxSndQueueHeader *)&s_queue[s_queuePos];
        end->m_dataSize  = 0;
        s_queuePos      += sizeof(RxSndQueueHeader);
        end->m_cmd       = 0;
        end->m_replySize = 0;
        rxSndCallRpc(&s_rpcClient, 0x200, s_queue, (pos + 0x4b) & ~0x3f,
                     s_queueReply, (replyPos + 0x3f) & ~0x3f);
        s_queuePos      = 0;
        s_queueReplyPos = 4;
    }
}
