/* ==========================================================================
 *  rpc.cpp  (RXSNDDRV.IRX 0x0b08..0x0c60: the sound RPC server)
 *
 *  A thread that registers RPC 0xd000 and serves it.  Each call's answer is
 *  one word followed by the command's reply data: RxSndDrvCommand writes the
 *  data, the server puts its return value in front.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "sifcmd.h"
#include "thbase.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RXSNDDRV_RPC_NO 0xd000

/* The server's thread and buffers, cleared together (0x2004 bytes from
 * 0xf500).  PC: the receive buffer is 0x1100, not 0x1000 -- the EE rounds a
 * full command queue up to 0x1040 bytes and the host copies all of it; on the
 * IOP the overrun fell into the reply word, which the answer then rewrote. */
static struct
{
    int   thread;                       // RoR IOP 0x0000f500
    u_int recv[0x1100 / 4];             // RoR IOP 0x0000f504
    u_int reply[1 + 0x1100 / 4];        // RoR IOP 0x00010504: the answer, then the reply data
} s_rpc;

// IOP 0x00000b90 (RoR 0x00000b08, word for word)
int RxSndDrvRpcStart(void)
{
    ThreadParam param;

    memset(&s_rpc, 0, sizeof(s_rpc));
    param.attr         = TH_C;
    param.entry        = (void *)RxSndDrvRpcThread;
    param.initPriority = 0xc;
    param.stackSize    = 0x800;
    param.option       = 0;
    s_rpc.thread = CreateThread(&param);
    if (s_rpc.thread <= 0)
    {
        printf("failed to create sound RPC server\n");
        return 0;
    }
    StartThread(s_rpc.thread, 0);
    if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO)
        printf("Start RX - sound RPC server\n");
    return 1;
}

// IOP 0x00000c4c (RoR 0x00000bc4, word for word)
void RxSndDrvRpcThread(void)
{
    /* On the IOP these lived on the thread's stack for good (RpcLoop never
     * returns); the host's RpcLoop returns at once and serves from the
     * table, so they must outlive the thread. */
    static sceSifQueueData queue;
    static sceSifServeData serve;

    sceSifSetRpcQueue(&queue, GetThreadId());
    sceSifRegisterRpc(&serve, RXSNDDRV_RPC_NO, RxSndDrvRpcServe, s_rpc.recv, NULL, NULL, &queue);
    sceSifRpcLoop(&queue);
}

/* PC: the first four calls of each command number (queued ones too) and
 * what they answered, for the smoke-run log.  YAMIPAN_RXSNDDRV_LOG=all logs
 * every call except the per-frame status polls (0x72 / 0x83 / 0xa3 / 0xb3). */
static void LogCommand(u_int fno, const u_int *data, u_int ret)
{
    static u_char seen[0x201];
    static int all = -1;

    if (all < 0) {
        const char *env = getenv("YAMIPAN_RXSNDDRV_LOG");
        all = env != NULL && strcmp(env, "all") == 0;
    }
    if (fno > 0x200)
        return;
    bool poll = fno == 0x72 || fno == 0x83 || fno == 0xa3 || fno == 0xb3;
    if (!(all && !poll)) {
        if (seen[fno] >= 4)
            return;
        seen[fno]++;
    }
    printf("rxsnddrv: cmd %#x {%#x %#x %#x %#x} -> %#x\n", fno, data[0], data[1], data[2], data[3], ret);
}

// IOP 0x00000cac (RoR 0x00000c24, word for word)
void *RxSndDrvRpcServe(u_int fno, void *data, int size)
{
    (void)size;
    s_rpc.reply[0] = RxSndDrvCommand(fno, (u_char *)&s_rpc.reply[1], (const u_int *)data);

    const u_int *d = (const u_int *)data;
    if (fno == 0x200)
    {
        for (; d[0] != 0; d = (const u_int *)((const u_char *)d + d[2] + 0xc))
            LogCommand(d[0], d + 3, 0);
    }
    else
    {
        LogCommand(fno, d, s_rpc.reply[0]);
    }
    return s_rpc.reply;
}
