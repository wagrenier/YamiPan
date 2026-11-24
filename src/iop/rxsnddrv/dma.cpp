/* ==========================================================================
 *  dma.cpp  (RXSNDDRV.IRX 0x0c60..0x1680: SPU DMA queues)
 *
 *  Every SPU RAM transfer the driver makes goes through two FIFO queues of
 *  256 entries (queue 0 is served first; the EE's command 0x40 and the
 *  effect-area clears use queue 1).  The "DMA management thread" waits on an
 *  event flag and starts the head transfer with sceSdVoiceTrans on core 0;
 *  libsd's transfer-complete handler advances it -- an EE transfer moves
 *  0x4000 bytes per step through a staging buffer, a clear 0x1000 bytes of
 *  zeros -- pops the entry when it is done (calling its completion), and
 *  sets the flag again.  Ids count up from 0 for command 0x41's busy test.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "intrman.h"
#include "libsd.h"
#include "sifman.h"
#include "sifrpc.h"
#include "thbase.h"
#include "thevent.h"

#include <stdio.h>
#include <string.h>

#include <mutex>

/* A queued transfer.  RoR IOP 0x20; layout: host (m_done, m_arg are host pointers). */
struct RxSndDrvDma
{
    int             m_id;               // IOP +0x00: -1 once cancelled
    int             m_type;             // IOP +0x04: RXSNDDRV_DMA_*, 0 once cancelled
    u_int           m_a;                // IOP +0x08: advances as an EE / clear transfer goes
    u_int           m_b;                // IOP +0x0c
    int             m_size;             // IOP +0x10: bytes left
    RxSndDrvDmaDone m_done;             // IOP +0x14: PC: a host function pointer
    void           *m_arg;              // IOP +0x18
    int             m_total;            // IOP +0x1c: the size asked for, passed to m_done
};

/* One FIFO.  RoR IOP 0x200c; layout: host (holds RxSndDrvDma). */
struct RxSndDrvDmaQueue
{
    RxSndDrvDma m_entry[0x100];         // IOP +0x0000
    u_int       m_tail;                 // IOP +0x2000: next free entry
    u_int       m_head;                 // IOP +0x2004: the entry in progress
    int         m_count;                // IOP +0x2008
};

#define RXSNDDRV_DMA_STAGE 0x4000

static RxSndDrvDmaQueue  s_queue[2];            // RoR IOP 0x00011510
static int               s_current;             // RoR IOP 0x00015528: the queue being served
static int               s_nextId;              // RoR IOP 0x0001552c
static volatile int      s_busy;                // RoR IOP 0x00015530
static int               s_thread;              // RoR IOP 0x00015534
static int               s_flag;                // RoR IOP 0x00015538
static u_char            s_stage[RXSNDDRV_DMA_STAGE]; // RoR IOP 0x00015540
static sceSifDmaData     s_sendDma;             // RoR IOP 0x00019540
static sceSifReceiveData s_recvData;            // RoR IOP 0x00019550

/* An EE <-> SPU step: whole 0x40-byte blocks, at most the staging buffer. */
static int StageChunk(int size)
{
    return size < RXSNDDRV_DMA_STAGE ? (size + 0x3f) & ~0x3f : RXSNDDRV_DMA_STAGE;
}

/* Start the head transfer of the first queue that has one, unless one is in
 * flight.  Cancelled entries at a head are dropped on the way. */
// IOP 0x00000cf0 (RoR 0x00000c60, 93% of it aligned: FS2 read, its transfers on channel 1)
void RxSndDrvDmaProc(void)
{
    int intr;

    CpuSuspendIntr(&intr);
    if (!s_busy)
    {
        for (s_current = 0; s_current < 2; s_current++)
        {
            RxSndDrvDmaQueue *q = &s_queue[s_current];

            while (q->m_entry[q->m_head].m_type == 0 && q->m_count > 0)
            {
                q->m_head = (q->m_head + 1) & 0xff;
                q->m_count--;
            }
            if (q->m_count <= 0)
                continue;

            RxSndDrvDma *e = &q->m_entry[q->m_head];
            u_char      *iop;
            u_int        spu;
            int          size;
            u_int        mode;

            s_busy = 1;
            CpuResumeIntr(intr);
            switch (e->m_type)
            {
            case RXSNDDRV_DMA_IOP_TO_SPU:
                size = e->m_size;
                iop  = (u_char *)YamiPan_IopPointer(e->m_b);
                spu  = e->m_a;
                mode = 0;
                break;
            case RXSNDDRV_DMA_SPU_TO_IOP:
                size = e->m_size;
                iop  = (u_char *)YamiPan_IopPointer(e->m_a);
                spu  = e->m_b;
                mode = 1;
                break;
            case RXSNDDRV_DMA_EE_TO_SPU:
            {
                u_int ee = e->m_b;

                spu  = e->m_a;
                size = StageChunk(e->m_size);
                e->m_a    += size;
                e->m_size -= size;
                e->m_b    += size;
                RxSndDrvGetFromEe(s_stage, ee, size);
                iop  = s_stage;
                mode = 0;
                break;
            }
            case RXSNDDRV_DMA_SPU_TO_EE:
                size = StageChunk(e->m_size);
                spu  = e->m_b;
                iop  = s_stage;
                mode = 1;
                break;
            case RXSNDDRV_DMA_ZERO_SPU:
                size = e->m_size;
                spu  = e->m_a;
                if (size > 0x1000)
                    size = 0x1000;
                e->m_a    += size;
                e->m_size -= size;
                iop  = RxSndDrvZeroBuffer();
                mode = 0;
                break;
            default:
                return;
            }
            /* FS2: channel 1 (RoR 0); core 0's auto-DMA is the PCM player's. */
            sceSdVoiceTrans(1, mode, iop, spu, (u_int)size);
            return;
        }
    }
    CpuResumeIntr(intr);
}

/* libsd's transfer-complete handler for core 1 (FS2; RoR's core 0). */
// IOP 0x00000f64 (RoR 0x00000ed4, word for word)
int RxSndDrvDmaIntr(int core, void *arg)
{
    (void)core;
    (void)arg;

    const int         cur  = s_current;
    RxSndDrvDmaQueue *q    = &s_queue[cur];
    RxSndDrvDma      *e    = &q->m_entry[q->m_head];
    int               left = 0;

    s_busy = 0;
    switch (e->m_type)
    {
    case RXSNDDRV_DMA_EE_TO_SPU:
    case RXSNDDRV_DMA_ZERO_SPU:
        left = e->m_size;
        break;
    case RXSNDDRV_DMA_SPU_TO_EE:
    {
        int size = StageChunk(e->m_size);

        RxSndDrvSendToEe(e->m_a, s_stage, size);
        e->m_a    += size;
        e->m_size -= size;
        e->m_b    += size;
        left = e->m_size;
        break;
    }
    }
    if (left <= 0)
    {
        if (e->m_type != 0 && e->m_done != NULL)
            e->m_done(e->m_arg, e->m_total);
        s_queue[cur].m_head = (s_queue[cur].m_head + 1) & 0xff;
        s_queue[cur].m_count--;
    }
    iSetEventFlag(s_flag, 1);
    return 1;
}

// IOP 0x000010d8 (RoR 0x00001048, 96% of it aligned: FS2 read, the handler on core 1)
void RxSndDrvDmaInit(void)
{
    EventFlagParam flag;
    ThreadParam    param;

    memset(s_queue, 0, sizeof(s_queue));
    s_nextId  = 0;
    s_busy    = 0;
    s_current = -1;
    s_thread  = -1;
    s_flag    = -1;
    sceSdSetTransIntrHandler(1, (void *)RxSndDrvDmaIntr, NULL); /* FS2: core 1 (RoR 0) */
    flag.attr        = 0;
    flag.initPattern = 0;
    flag.option      = 0;
    s_flag = CreateEventFlag(&flag);
    param.attr         = TH_C;
    param.entry        = (void *)RxSndDrvDmaThread;
    param.initPriority = 0xd;
    param.stackSize    = 0x1000;
    param.option       = 0;
    s_thread = CreateThread(&param);
    if (s_thread <= 0)
        printf("snddrv : failed to create DMA management thread\n");
    else
        StartThread(s_thread, 0);
}

/* Queue a transfer; answers its id, or -1 when the queue is full or the
 * manager is not up.  Nothing starts until RxSndDrvDmaKick. */
// IOP 0x000011b4 (RoR 0x00001124, word for word)
int RxSndDrvDmaPush(int queue, int type, u_int a, u_int b, int size, RxSndDrvDmaDone done, void *arg)
{
    int ret = -1;
    int intr;

    CpuSuspendIntr(&intr);
    const int id = s_nextId;
    if (s_flag >= 0 && s_thread >= 0)
    {
        RxSndDrvDmaQueue *q = &s_queue[queue];
        if (q->m_count < 0x100)
        {
            RxSndDrvDma *e = &q->m_entry[q->m_tail];
            e->m_id    = s_nextId++;
            e->m_type  = type;
            e->m_a     = a;
            e->m_b     = b;
            e->m_size  = size;
            e->m_done  = done;
            e->m_total = size;
            e->m_arg   = arg;
            q->m_tail  = (q->m_tail + 1) & 0xff;
            q->m_count++;
            ret = id;
        }
    }
    CpuResumeIntr(intr);
    return ret;
}

// IOP 0x000012e0 (RoR 0x00001250, word for word)
void RxSndDrvDmaKick(void)
{
    SetEventFlag(s_flag, 1);
}

/* Command 0x41: is transfer `id` still queued (or in flight)? */
// IOP 0x0000130c (RoR 0x0000127c, word for word)
int RxSndDrvDmaBusy(int id)
{
    int ret = 0;
    int intr;

    CpuSuspendIntr(&intr);
    for (int i = 0; i < 2 && !ret; i++)
    {
        u_int pos = s_queue[i].m_head;
        for (int n = 0; n < s_queue[i].m_count; n++)
        {
            const RxSndDrvDma *e = &s_queue[i].m_entry[pos];
            pos = (pos + 1) & 0xff;
            if (e->m_id == id)
            {
                ret = 1;
                break;
            }
        }
    }
    CpuResumeIntr(intr);
    return ret;
}

/* Drop the entry `id` (its completion runs now); the queue skips it. */
static void CancelEntry(RxSndDrvDma *e)
{
    if (e->m_done != NULL)
        e->m_done(e->m_arg, e->m_total);
    e->m_type = 0;
    e->m_id   = -1;
    e->m_done = NULL;
    e->m_arg  = NULL;
}

// IOP 0x000013bc (RoR 0x0000132c, word for word)
void RxSndDrvDmaCancel(int id)
{
    int intr;

    CpuSuspendIntr(&intr);
    for (int i = 0; i < 2; i++)
    {
        u_int pos = s_queue[i].m_head;
        for (int n = 0; n < s_queue[i].m_count; n++)
        {
            RxSndDrvDma *e = &s_queue[i].m_entry[pos];
            pos = (pos + 1) & 0xff;
            if (e->m_id == id)
            {
                CancelEntry(e);
                CpuResumeIntr(intr);
                return;
            }
        }
    }
    CpuResumeIntr(intr);
}

/* Drop every entry queued for `arg` (a stream closing). */
// IOP 0x000014a4 (RoR 0x00001414, word for word)
void RxSndDrvDmaCancelArg(void *arg)
{
    int intr;

    CpuSuspendIntr(&intr);
    for (int i = 0; i < 2; i++)
    {
        u_int pos = s_queue[i].m_head;
        for (int n = 0; n < s_queue[i].m_count; n++)
        {
            RxSndDrvDma *e = &s_queue[i].m_entry[pos];
            pos = (pos + 1) & 0xff;
            if (e->m_arg == arg)
                CancelEntry(e);
        }
    }
    CpuResumeIntr(intr);
}

// IOP 0x000015ac (RoR 0x0000151c, word for word)
void RxSndDrvDmaThread(void)
{
    u_int result;

    if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO)
        printf("snddrv : start DMA procedure\n");
    for (;;)
    {
        /* PC: answers < 0 at host shutdown. */
        if (WaitEventFlag(s_flag, 1, WEF_OR | WEF_CLEAR, &result) < 0)
            return;
        RxSndDrvDmaProc();
    }
}

// IOP 0x00001600 (RoR 0x00001570, word for word)
void RxSndDrvDmaInit2(void)
{
}

/* `size` bytes of IOP memory to the EE, waiting for the SIF. */
// IOP 0x00001610 (RoR 0x00001580, word for word)
void RxSndDrvSendToEe(u_int ee, void *iop, int size)
{
    int   intr;
    u_int id;

    CpuSuspendIntr(&intr);
    s_sendDma.mode = 0;
    s_sendDma.data = iop;
    s_sendDma.addr = RxSndDrvEePointer(ee);
    s_sendDma.size = size;
    while ((id = sceSifSetDma(&s_sendDma, 1)) == 0)
        ;
    while (sceSifDmaStat(id) >= 0)
        ;
    CpuResumeIntr(intr);
}

// IOP 0x000016a8 (RoR 0x00001618, word for word)
void RxSndDrvGetFromEe(void *iop, u_int ee, int size)
{
    while (sceSifGetOtherData(&s_recvData, RxSndDrvEePointer(ee), iop, size, 0) < 0)
        ;
}

/* PC: EE addresses.  The EE sends them in 32-bit words (RxSndCmdTransfer's
 * m_eeAddr), but its heap may lie above 4 GB on the host; rx/rxsnd.c
 * notes each full pointer it sends, and a word is widened back to the
 * latest noted pointer that has the same low half and holds it (a transfer
 * advances inside the noted block). */
#define RXSNDDRV_EE_NOTES 64

/* layout: host (PC only). */
struct RxSndDrvEeNote
{
    uintptr_t m_base;
    size_t    m_size;
};

static std::mutex     s_eeLock;
static RxSndDrvEeNote s_eeNote[RXSNDDRV_EE_NOTES];
static int            s_eeNext;

void YamiPan_RxSndNoteEeAddress(const void *ee, size_t size)
{
    std::lock_guard<std::mutex> lock(s_eeLock);
    s_eeNote[s_eeNext].m_base = (uintptr_t)ee;
    s_eeNote[s_eeNext].m_size = size;
    s_eeNext = (s_eeNext + 1) % RXSNDDRV_EE_NOTES;
}

void *RxSndDrvEePointer(u_int ee)
{
    std::lock_guard<std::mutex> lock(s_eeLock);
    for (int i = 1; i <= RXSNDDRV_EE_NOTES; i++)
    {
        const RxSndDrvEeNote *n = &s_eeNote[(s_eeNext - i + RXSNDDRV_EE_NOTES) % RXSNDDRV_EE_NOTES];
        if (n->m_base == 0)
            continue;
        const u_int lo  = (u_int)n->m_base;
        const u_int off = ee - lo;
        if (off <= n->m_size)
            return (void *)(n->m_base + off);
    }
    return (void *)(uintptr_t)ee;
}
