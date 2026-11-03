/* libdma.h — PS2 EE DMA controller library — PC-port declarations.
 *
 * Declarations adapted from the Fatal Frame 1 PC port (MikuPan sdk/sce/libdma.h);
 * the EE and its SDK are identical across the two titles.  Implementations are a
 * later (link-time) concern — a PC backend must drive the actual transfers.
 */
#pragma once
#include "scetypes.h"

typedef struct {
    u_char  sts;
    u_char  std;
    u_char  mfd;
    u_char  rcycle;
    u_short express;
    u_short notify;
    u_short sqwc;
    u_short tqwc;
    void   *rbadr;
    u_int   rbmsk;
} sceDmaEnv;

typedef union {
    u_int bits;
    struct {
    unsigned DIR : 1;
    unsigned p0  : 1;
    unsigned MOD : 2;
    unsigned ASP : 2;
    unsigned TTE : 1;
    unsigned TIE : 1;
    unsigned STR : 1;
    unsigned p1  : 7;
    unsigned TAG : 16;
    };
} tD_CHCR;

typedef struct _sceDmaTag {
    u_short            qwc;
    u_char             mark;
    u_char             id;
    struct _sceDmaTag *next;
    u_int              p[2];
} sceDmaTag;

struct _sceDmaChan {
             tD_CHCR    chcr;  u_int p0[3];
    volatile void      *madr;  u_int p1[3];
    volatile u_int      qwc;   u_int p2[3];
    volatile sceDmaTag *tadr;  u_int p3[3];
    volatile void      *as0;   u_int p4[3];
    volatile void      *as1;   u_int p5[3];
             u_int      p6[4];
             u_int      p7[4];
    volatile void      *sadr;  u_int p8[3];
};
typedef struct _sceDmaChan sceDmaChan;

#define settag(t,i,a,q) (t)->id=(i),(t)->next=(sceDmaTag *)(a),(t)->qwc=(u_short)(q)

static inline void *sceDmaAddRef(sceDmaTag **tag, int qwc, void *addr)
{
    settag(*tag, 0x30, addr, qwc);
    (*tag)++;
    return addr;
}

static inline void *sceDmaAddEnd(sceDmaTag **tag, int qwc, void *addr)
{
    void *ret;
    settag(*tag, 0x70, addr, qwc);
    ret = *tag + 1;
    (*tag) += qwc + 1;
    return ret;
}

/* The DMAC channels. */
#define SCE_DMA_VIF0    0
#define SCE_DMA_VIF1    1
#define SCE_DMA_GIF     2
#define SCE_DMA_fromIPU 3
#define SCE_DMA_toIPU   4
#define SCE_DMA_fromSIF0 5
#define SCE_DMA_toSIF1  6
#define SCE_DMA_SIF2    7
#define SCE_DMA_fromSPR 8
#define SCE_DMA_toSPR   9

int         sceDmaReset(int mode);
void        sceDmaSendN(sceDmaChan *d, void *addr, int qwc);
sceDmaEnv  *sceDmaGetEnv(sceDmaEnv *env);
int         sceDmaPutEnv(sceDmaEnv *env);
sceDmaChan *sceDmaGetChan(int id);
void        sceDmaSend(sceDmaChan *d, void *tag);
int         sceDmaSync(sceDmaChan *d, int mode, int timeout);

