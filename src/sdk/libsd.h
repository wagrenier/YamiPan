/* ==========================================================================
 *  libsd.h  (the SPU2 sound library, IOP side: SCE's API as RXSNDDRV calls it)
 *
 *  Register "entries" are an address in the top bits and a voice key in the
 *  low ones: bit 0 selects the core, bits 1..5 the voice.  RosePan's header;
 *  in YamiPan sdk/iop.cpp implements it on yamipan::iop::Spu2 (registers,
 *  SPU RAM, transfers, voices that play in time, no sound yet).
 * ======================================================================== */

#ifndef _LIBSD_H
#define _LIBSD_H

#include "scetypes.h"

/* ---- core selector, the low bit of every entry ------------------------- */
#define SD_CORE_0           0x00
#define SD_CORE_1           0x01

/* ---- per-voice parameters (sceSdSetParam / sceSdGetParam) -------------- */
#define SD_VP_VOLL          0x0000
#define SD_VP_VOLR          0x0100
#define SD_VP_PITCH         0x0200
#define SD_VP_ADSR1         0x0300
#define SD_VP_ADSR2         0x0400
#define SD_VP_ENVX          0x0500

/* ---- per-core parameters ----------------------------------------------- */
#define SD_P_MMIX           0x0800
#define SD_P_MVOLL          0x0980
#define SD_P_MVOLR          0x0a80
#define SD_P_EVOLL          0x0b80
#define SD_P_EVOLR          0x0c80
#define SD_P_AVOLL          0x0d80
#define SD_P_AVOLR          0x0e80
#define SD_P_BVOLL          0x0f80
#define SD_P_BVOLR          0x1080

/* ---- switches (sceSdSetSwitch / sceSdGetSwitch), one bit per voice ----- */
#define SD_S_PMON           0x1300  /* pitch modulation by the voice below  */
#define SD_S_NON            0x1400  /* noise instead of the sample          */
#define SD_S_KON            0x1500
#define SD_S_KOFF           0x1600
#define SD_S_ENDX           0x1700
#define SD_S_VMIXL          0x1800
#define SD_S_VMIXEL         0x1900
#define SD_S_VMIXR          0x1a00
#define SD_S_VMIXER         0x1b00

/* ---- addresses (sceSdSetAddr / sceSdGetAddr) --------------------------- */
#define SD_A_IRQA           0x1f00  /* interrupt when playback reaches here */
#define SD_A_EEA            0x1d00  /* effect work area end                 */
#define SD_VA_SSA           0x2040  /* sample start                         */
#define SD_VA_LSAX          0x2140  /* loop start                           */
#define SD_VA_NAX           0x2240  /* current play position                */

/* ---- core attributes (sceSdSetCoreAttr) -------------------------------- */
#define SD_C_EFFECT_ENABLE  0x02
#define SD_C_IRQ_ENABLE     0x04
#define SD_C_SPDIF_MODE     0x0a

/* ---- sceSdBlockTrans modes --------------------------------------------- */
#define SD_BLOCK_TRANS_STAT 0x02
#define SD_BLOCK_TRANS_LOOP 0x10
#define SD_BLOCK_TRANS_CONT 0x13

/* SPU2 reverb attributes.  Named as the SDK names them so the field uses in
 * iopSndSetEffect() read the same way. */
typedef struct                      /* 0x14 */
{
    u_int  core;
    u_int  mode;
    short  depth_L;
    short  depth_R;
    int    delay;
    int    feedback;
} sceSdEffectAttrSdk;

/* sceSdProcBatch commands: {func, entry, value}, as the SDK lays them out. */
typedef struct                      /* 0x8 */
{
    u_short func;
    u_short entry;
    u_int   value;
} sceSdBatch;

#define SD_BATCH_SETPARAM   0x01
#define SD_BATCH_SETSWITCH  0x02
#define SD_BATCH_SETADDR    0x03
#define SD_BATCH_SETCORE    0x04
#define SD_BATCH_WRITEIOP   0x05
#define SD_BATCH_WRITEEE    0x06
#define SD_BATCH_EERETURN   0x07
#define SD_BATCH_GETPARAM   0x10
#define SD_BATCH_GETSWITCH  0x12
#define SD_BATCH_GETADDR    0x13
#define SD_BATCH_GETCORE    0x14

#ifdef __cplusplus
extern "C" {
#endif

int    sceSdInit(int flag);

void   sceSdSetParam(u_short entry, u_short value);
u_short sceSdGetParam(u_short entry);
void   sceSdSetSwitch(u_short entry, u_int value);
u_int  sceSdGetSwitch(u_short entry);
void   sceSdSetAddr(u_short entry, u_int value);
u_int  sceSdGetAddr(u_short entry);
void   sceSdSetCoreAttr(u_short entry, u_short value);

/* Uploads `size` bytes of ADPCM from IOP RAM to SPU RAM.  Returns < 0 if the
 * core is busy; every caller retries in a loop. */
int    sceSdVoiceTrans(short chan, u_int mode, u_char *iopaddr, u_int spuaddr, u_int size);
/* The streaming form.  Called with four arguments at some sites and five at
 * others -- `start_addr` is only read for SD_BLOCK_TRANS_CONT. */
int    sceSdBlockTrans(short chan, u_short mode, u_char *iopaddr, u_int size, u_int start_addr);
int    sceSdVoiceTransStatus(short chan, short flag);
/* `flag` 0 answers with the address the auto-DMA is reading from, which is how
 * audiodec.c finds the SPU2's play position inside its own IOP ring. */
int    sceSdBlockTransStatus(short chan, short flag);

int    sceSdSetEffectAttr(int core, void *attr);
int    sceSdProcBatch(sceSdBatch *batch, u_int returns[], u_int num);
/* The PITCH register value that plays `note` + `fine` (1/128 semitones) of a
 * sample recorded at `center_note` + `center_fine`: 0x1000 at the centre. */
u_short sceSdNote2Pitch(u_short center_note, u_short center_fine, u_short note, short fine);
int    sceSdClearEffectWorkArea(int core, int channel, int effect_mode);

/* Completion callbacks.  The transfer handler fires when a voice/block upload
 * finishes; the SPU2 handler when playback crosses a core's SD_A_IRQA. */
void  *sceSdSetTransIntrHandler(int core, void *func, void *arg);
void  *sceSdSetSpu2IntrHandler(void *func, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _LIBSD_H */
