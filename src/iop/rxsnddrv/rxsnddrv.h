/* ==========================================================================
 *  rxsnddrv.h  (RXSNDDRV.IRX, the rx IOP sound driver: Rule of Rose's, revised in FS2)
 *
 *  "Sound driver version 1.00": the server of SIF RPC 0xd000 that the EE's
 *  rxSnd* functions (rx/rxsnd.c) call.  The module is the only game code on
 *  the IOP; FS2's MODULES/RXSNDDRV.IRX is linked at 0, gp 0x16910, no symbols.
 *  This is RosePan's reconstruction of RoR's build (2026-10-30), brought to
 *  YamiPan 2026-11-07 because the two builds are one source tree: 152 of these
 *  192 functions are in FS2 word for word (tools/reverse/iop.py cmp,
 *  architecture.md "The IOP side").  One file per subsystem, FS2's extents:
 *
 *    rxsnddrv.cpp  0x0000..0x0b90
 *    rpc.cpp       0x0b90..0x0cf0
 *    dma.cpp       0x0cf0..0x1710
 *    sys.cpp       0x1710..0x1a70
 *    mix.cpp       0x1a70..0x1dc0
 *    effect.cpp    0x1dc0..0x2450
 *    xvab.cpp      0x2450..0x3000
 *    xseq.cpp      0x3000..0x37c0
 *    voice.cpp     0x37c0..0x4800
 *    bgm.cpp       0x4800..0x6d00
 *    se.cpp        0x6d00..0x91e0
 *    note.cpp      0x91e0..0xa1a0
 *    adpcm.cpp     0xa1a0..0xb790
 *    pcm.cpp       0xb790..0xd520
 *
 *  Functions carry `IOP 0x<addr>`, FS2's address (never `FS2 0x`: the TU tools
 *  count those as EE functions), then RoR's and how FS2's build compares:
 *  "word for word" (relocated fields masked), "N% of it aligned: FS2 differs,
 *  unread", or "moved in FS2".  Other addresses in comments are RoR's
 *  (`RoR IOP 0x...`): FS2's .bss and parts of its code moved.
 *
 *  FS2 (Ghidra program rxsnddrv.elf): 38 changed functions, 2 moved ones and
 *  4 functions RoR does not have (0x2d9c, 0x3660, 0x4428, 0xd1dc; 0x4454 is
 *  RxSndDrvVoiceKeyOn, after the new 0x4428).  Read 2026-11-09 and taken in:
 *  the dispatcher's four FS2 commands (0x52, 0x53, 0x62, 0xb4), the three new
 *  functions behind them, RxSndDrvXvabGetUsers's test, RxSndDrvAdpcmStart's
 *  group / owner, RxSndDrvSeRun's pause and same-bank holders, the PCM
 *  player (pcm.cpp: core 0, a ring of 2..8 halves, m_fresh) and with it the
 *  DMA thread's move to core 1 (dma.cpp), the mixer's input volume (mix.cpp,
 *  command 0x31 param 2).  Read and the
 *  same as RoR apart from the record offsets: RxSndDrvNoteTick's switch bits,
 *  RxSndDrvVoiceAlloc, RxSndDrvSeFreeRamps, RxSndDrvPcmStatus.  Still unread:
 *  the rest marked "FS2 differs, unread" (the voice tick's noise clock, the
 *  XSEQ chunk lookups).  The one-word differences say the voice, mix
 *  and PCM records grew (a voice stride 0x38 -> 0x3c, PCM offsets up by
 *  0x18..0x1c): the struct layouts below are still RoR's.  sdk/sif.cpp boots it when rxfile loads
 *  RXSNDDRV.IRX; its imports are sdk/iop.cpp's, on yamipan::iop.
 *
 *  The double buffer.  Commands arrive on the EE's RPC (any time) and are
 *  written into bank RxSndDrvCmdBank(); the double-buffer thread flips the
 *  bank once per V-blank and then applies RxSndDrvTickBank(), the bank the
 *  commands of the last frame went into.  Every subsystem keeps two pending
 *  records {mask, values} indexed by bank for that.
 *
 *  Host: IOP addresses the EE hands over (heap blocks, SPU addresses) are
 *  u_int words, as on the IOP; IOP heap blocks come from the host IOP arena
 *  (YamiPan_IopAllocSysMemory), which lies below 4 GB, so they are host
 *  pointers.  EE addresses are not (see RxSndDrvEePointer).
 * ======================================================================== */

#ifndef YAMIPAN_IOP_RXSNDDRV_H
#define YAMIPAN_IOP_RXSNDDRV_H

#include "scetypes.h"

#include <stddef.h>

/* ---- debug flags (system parameter 0; RxSndDrvGetDebugFlags) ----------- */
#define RXSNDDRV_DBG_ERROR   0x1     /* "snddrv : invalide RPC command" and the other errors */
#define RXSNDDRV_DBG_WARN    0x2     /* unsupported MIDI controllers / meta events, slot shortages */
#define RXSNDDRV_DBG_INFO    0x4     /* start-up / open / close messages */

/* ---- rxsnddrv.cpp ------------------------------------------------------ */

int   RxSndDrvStart(void);                          // IOP 0x00000000 (_start)
int   RxSndDrvStartTick(void);                      // IOP 0x00000190
int   RxSndDrvStopTick(void);                       // IOP 0x00000238
int   RxSndDrvTickBank(void);                       // IOP 0x00000254
int   RxSndDrvCmdBank(void);                        // IOP 0x00000268
int   RxSndDrvGetVersion(void);                     // IOP 0x0000027c
u_char *RxSndDrvZeroBuffer(void);                   // IOP 0x0000028c
void  RxSndDrvTickThread(void);                     // IOP 0x0000029c
u_int RxSndDrvCommand(u_int cmd, u_char *reply, const u_int *data); // IOP 0x00000360

/* ---- rpc.cpp ----------------------------------------------------------- */

int   RxSndDrvRpcStart(void);                       // IOP 0x00000b90
void  RxSndDrvRpcThread(void);                      // IOP 0x00000c4c
void *RxSndDrvRpcServe(u_int fno, void *data, int size); // IOP 0x00000cac

/* ---- dma.cpp ----------------------------------------------------------- */

/* Transfer types (command 0x40 {type, a, b, size}). */
enum
{
    RXSNDDRV_DMA_IOP_TO_SPU = 1,    /* a SPU, b IOP */
    RXSNDDRV_DMA_SPU_TO_IOP = 2,    /* a IOP, b SPU */
    RXSNDDRV_DMA_EE_TO_SPU  = 3,    /* a SPU, b EE: 0x4000 at a time through the staging buffer */
    RXSNDDRV_DMA_SPU_TO_EE  = 4,    /* a EE, b SPU: the same, the other way */
    RXSNDDRV_DMA_ZERO_SPU   = 5,    /* a SPU: zeros from RxSndDrvZeroBuffer, 0x1000 at a time */
};

typedef void (*RxSndDrvDmaDone)(void *arg, int size);

void  RxSndDrvDmaProc(void);                        // IOP 0x00000cf0
int   RxSndDrvDmaIntr(int core, void *arg);         // IOP 0x00000f64
void  RxSndDrvDmaInit(void);                        // IOP 0x000010d8
int   RxSndDrvDmaPush(int queue, int type, u_int a, u_int b, int size,
                      RxSndDrvDmaDone done, void *arg); // IOP 0x000011b4
void  RxSndDrvDmaKick(void);                        // IOP 0x000012e0
int   RxSndDrvDmaBusy(int id);                      // IOP 0x0000130c
void  RxSndDrvDmaCancel(int id);                    // IOP 0x000013bc
void  RxSndDrvDmaCancelArg(void *arg);              // IOP 0x000014a4
void  RxSndDrvDmaThread(void);                      // IOP 0x000015ac
void  RxSndDrvDmaInit2(void);                       // IOP 0x00001600
void  RxSndDrvSendToEe(u_int ee, void *iop, int size);   // IOP 0x00001610
void  RxSndDrvGetFromEe(void *iop, u_int ee, int size);  // IOP 0x000016a8

/* PC: an EE address the EE sent as a 32-bit word, widened back to the host
 * pointer rx/rxsnd.c registered for it (YamiPan_RxSndNoteEeAddress), or the
 * word itself when none was registered. */
void *RxSndDrvEePointer(u_int ee);
void  YamiPan_RxSndNoteEeAddress(const void *ee, size_t size);

/* ---- sys.cpp ----------------------------------------------------------- */

void  RxSndDrvSysTick(void);                        // IOP 0x00001710
void  RxSndDrvSysInit(void);                        // IOP 0x000018a4
int   RxSndDrvGetFrameRate(void);                   // IOP 0x00001910
u_int RxSndDrvGetDebugFlags(void);                  // IOP 0x00001920
int   RxSndDrvGetOutputMode(void);                  // IOP 0x00001930
void  RxSndDrvSysSet(const u_int *data);            // IOP 0x00001940

/* ---- mix.cpp ----------------------------------------------------------- */

void  RxSndDrvMixTick(void);                        // IOP 0x00001a70
void  RxSndDrvMixInit(void);                        // IOP 0x00001c28
void  RxSndDrvMixSet(int core, const u_int *data);  // IOP 0x00001cb4

/* ---- effect.cpp -------------------------------------------------------- */

void  RxSndDrvEffectTick(void);                     // IOP 0x00001dc0
void  RxSndDrvEffectInit(void);                     // IOP 0x000021e4
void  RxSndDrvEffectSet(int core, const u_int *data); // IOP 0x00002300

/* ---- xvab.cpp ---------------------------------------------------------- */

/* A bank's tables (IOP addresses, 0 none) and its SPU sample base. */
struct RxSndDrvXvabTables
{
    u_int m_tgif;                       // IOP +0x00: programs
    u_int m_seif;                       // IOP +0x04: SE groups
    u_int m_velt;                       // IOP +0x08: velocity curves
    u_int m_pant;                       // IOP +0x0c: pan curves
    u_int m_lfot;                       // IOP +0x10: LFO curves
    u_int m_spu;                        // IOP +0x14
};
static_assert(sizeof(RxSndDrvXvabTables) == 0x18, "RxSndDrvXvabTables");

/* A slot (0x80 at RoR IOP 0x19680). */
struct RxSndDrvXvab
{
    u_int              m_file;          // IOP +0x00: the file in IOP memory, 0 free
    RxSndDrvXvabTables m_tables;        // IOP +0x04
    int                m_users;         // IOP +0x1c: notes / SEs using it
};
static_assert(sizeof(RxSndDrvXvab) == 0x20, "RxSndDrvXvab");
static_assert(offsetof(RxSndDrvXvab, m_tables) == 0x4, "RxSndDrvXvab::m_tables");
static_assert(offsetof(RxSndDrvXvab, m_users) == 0x1c, "RxSndDrvXvab::m_users");

int   RxSndDrvXvabOpen(u_int id, u_int iop, u_int spu);    // IOP 0x00002450
int   RxSndDrvXvabRelocate(u_char *file, int dir, int open); // IOP 0x00002698
void  RxSndDrvXvabInit(void);                       // IOP 0x00002974
void  RxSndDrvXvabCloseAll(void);                   // IOP 0x000029a0
int   RxSndDrvXvabClose(u_int id);                  // IOP 0x000029d8
RxSndDrvXvabTables *RxSndDrvXvabGetTables(u_int id); // IOP 0x00002ad0
u_int RxSndDrvXvabGetSpu(int id);                   // IOP 0x00002b08
int   RxSndDrvXvabLock(int id);                     // IOP 0x00002b34
void  RxSndDrvXvabUnlock(int id);                   // IOP 0x00002ba8
u_int RxSndDrvXvabFindSe(int id, int group, int index, int *ticks, int *a, int *b,
                         int *mode);                // IOP 0x00002c08
int   RxSndDrvXvabAttach(u_char *file);             // IOP 0x00002eac
int   RxSndDrvXvabDetach(u_char *file);             // IOP 0x00002ed0
u_int RxSndDrvChunkGetVersion(const void *file);    // IOP 0x00002ef4
u_char *RxSndDrvChunkFind(const void *file, u_int tag);   // IOP 0x00002f00
int   RxSndDrvXvabGetUsers(int id);                 // IOP 0x00002e80
int   RxSndDrvXvabHasSe(int id, int group, int index); // IOP 0x00002d9c (FS2 only)
u_char *RxSndDrvChunkFindV1(const void *file, u_int tag); // IOP 0x00002f44
u_char *RxSndDrvChunkFindV2(const void *file, u_int tag); // IOP 0x00002fa4

/* ---- xseq.cpp ---------------------------------------------------------- */

/* A slot (0x20 at RoR IOP 0x1a680). */
struct RxSndDrvXseq
{
    u_int m_file;                       // IOP +0x00: the file in IOP memory, 0 free
    u_int m_head;                       // IOP +0x04: its HEAD (or MIDI) chunk
    int   m_state;                      // IOP +0x08: -1 after a close, read nowhere
    int   m_users;                      // IOP +0x0c: BGMs playing it
    short m_bank[8];                    // IOP +0x10: XVAB per bank index, -1 unset
};
static_assert(sizeof(RxSndDrvXseq) == 0x20, "RxSndDrvXseq");
static_assert(offsetof(RxSndDrvXseq, m_head) == 0x4, "RxSndDrvXseq::m_head");
static_assert(offsetof(RxSndDrvXseq, m_state) == 0x8, "RxSndDrvXseq::m_state");
static_assert(offsetof(RxSndDrvXseq, m_users) == 0xc, "RxSndDrvXseq::m_users");
static_assert(offsetof(RxSndDrvXseq, m_bank) == 0x10, "RxSndDrvXseq::m_bank");

int   RxSndDrvXseqOpen(u_int id, u_int iop);        // IOP 0x00003000
int   RxSndDrvXseqRelocate(u_char *file, int dir, int open); // IOP 0x000031a0
void  RxSndDrvXseqInit(void);                       // IOP 0x00003370
void  RxSndDrvXseqCloseAll(void);                   // IOP 0x0000339c
int   RxSndDrvXseqClose(u_int id);                  // IOP 0x000033d4
int   RxSndDrvXseqGetUsers(u_int id);               // IOP 0x00003660 (FS2 only)
u_int *RxSndDrvXseqGetHead(u_int id);               // IOP 0x000034d8
void  RxSndDrvXseqSetBank(u_int id, u_int index, u_int xvab); // IOP 0x00003510
int   RxSndDrvXseqGetBank(int id, u_int index);     // IOP 0x00003548
int   RxSndDrvXseqLock(int id);                     // IOP 0x0000358c
void  RxSndDrvXseqUnlock(int id);                   // IOP 0x00003600
int   RxSndDrvXseqAttach(u_char *file);             // IOP 0x0000368c
int   RxSndDrvXseqDetach(u_char *file);             // IOP 0x000036b0
u_char *RxSndDrvXseqChunkFind(const void *file, u_int tag);   // IOP 0x000036d4
u_char *RxSndDrvXseqChunkFindV1(const void *file, u_int tag); // IOP 0x00003718
u_char *RxSndDrvXseqChunkFindV2(const void *file, u_int tag); // IOP 0x00003768

/* ---- voice.cpp --------------------------------------------------------- */

/* Told when an owner loses its voice: reason 0 it ended or was released,
 * 1 another allocation took it. */
typedef void (*RxSndDrvVoiceDone)(int voice, int reason, void *arg);

void  RxSndDrvVoiceTick(void);                      // IOP 0x000037c0
int   RxSndDrvVoiceAlloc(int voice, int priority, int autoFree, int group, int owner,
                         RxSndDrvVoiceDone done, void *arg); // IOP 0x00003da0
void  RxSndDrvVoiceInit(void);                      // IOP 0x000040d8
void  RxSndDrvVoiceSetOutputMode(int mode);         // IOP 0x0000418c
void  RxSndDrvVoiceRelease(int voice, int reason, int noFade); // IOP 0x000041ec
void  RxSndDrvVoiceSetSample(int voice, u_int ssa, u_short adsr1, u_short adsr2); // IOP 0x000042bc
void  RxSndDrvVoiceSetLoop(int voice, u_int lsax);  // IOP 0x000042f0
void  RxSndDrvVoiceSetSwitch(int voice, u_char bits); // IOP 0x0000431c
void  RxSndDrvVoiceSetVolume(int voice, int l, int r);    // IOP 0x00004348
void  RxSndDrvVoiceSetVolumeRaw(int voice, int l, int r); // IOP 0x00004380
void  RxSndDrvVoiceSetPitch(int voice, u_short center, u_short centerFine, u_short note,
                            short fine, int scale); // IOP 0x000043b0
void  RxSndDrvVoiceKeyOn(int voice);                // IOP 0x00004428
void  RxSndDrvVoiceKeyOff(int voice);               // IOP 0x000044e8
u_short RxSndDrvVoiceGetEnvx(int voice);            // IOP 0x0000458c
void  RxSndDrvVoiceGetEnvxAll(u_char *reply);       // IOP 0x000045d0
void  RxSndDrvVoiceGetTable(u_int kind, u_char *reply); // IOP 0x00004600
u_int RxSndDrvVoiceGetNax(int voice);               // IOP 0x000046b4
void  RxSndDrvVoiceSetEnable(u_int voice, u_int enable); // IOP 0x00004768
void  RxSndDrvVoiceGetFlags(u_char *reply);         // IOP 0x0000478c
void  RxSndDrvVoiceFree(int voice);                 // IOP 0x000047bc
/* PC: the player's volume group (YAMIPAN_VOICE_GROUP_*, sdk/iop_voice.h) of
 * the voices allocated from here on; the tick thread sets it per subsystem. */
void  RxSndDrvVoiceSetMixGroup(int group);

/* ---- bgm.cpp ----------------------------------------------------------- */

void  RxSndDrvBgmInit(void);                        // IOP 0x00004800
int   RxSndDrvBgmOpen(u_int slot, u_int xseq);      // IOP 0x000048d0
void  RxSndDrvBgmSet(u_int slot, const u_int *data);    // IOP 0x00005c3c
int   RxSndDrvBgmStatus(u_char *reply, u_char *values); // IOP 0x00005db8
void  RxSndDrvBgmChannelSet(int slot, int channel, const u_int *data); // IOP 0x00005e60
void  RxSndDrvBgmTrackSet(int slot, u_int track, const u_int *data);   // IOP 0x00005f70
void  RxSndDrvBgmTick(void);                        // IOP 0x000060b0

/* ---- se.cpp ------------------------------------------------------------ */

void  RxSndDrvSeInit(void);                         // IOP 0x00006d00
void  RxSndDrvSeSet(u_int slot, const u_int *data); // IOP 0x00006e1c
void  RxSndDrvSeStopAll(void);                      // IOP 0x00008730
int   RxSndDrvSePlay(int slot, int xvab, int group, int index);  // IOP 0x00008788
int   RxSndDrvSePlay2(int slot, int xvab, int program, int key); // IOP 0x00008930
int   RxSndDrvSeStatus(u_char *reply);              // IOP 0x00008ad8
void  RxSndDrvSeTick(void);                         // IOP 0x00008b50

/* ---- note.cpp ---------------------------------------------------------- */

/* One key of an XVAB program on as many voices as it has regions: a NOTE id
 * from Play (-1 none), then key off, free and its parameters; GetFlags bit 1
 * = a voice still sounds. */
int   RxSndDrvNotePlay(int xvab, int program, int key, int group, int owner); // IOP 0x000091e0
void  RxSndDrvNoteTick(void);                       // IOP 0x00009690
void  RxSndDrvNoteInit(void);                       // IOP 0x00009bd8
void  RxSndDrvNoteKeyOff(int id);                   // IOP 0x00009c68
void  RxSndDrvNoteFree(int id);                     // IOP 0x00009cbc
void  RxSndDrvNoteSetSwitch(int id, int on);        // IOP 0x00009da4
void  RxSndDrvNoteSetVolume(int id, short l, short r); // IOP 0x00009dd8
void  RxSndDrvNoteSetVelocity(int id, int velocity);   // IOP 0x00009e10
void  RxSndDrvNoteSetPan(int id, int pan);          // IOP 0x00009ea4
void  RxSndDrvNoteSetPitch(int id, int scale);      // IOP 0x00009f00
void  RxSndDrvNoteSetBend(int id, int bend);        // IOP 0x00009f34
void  RxSndDrvNoteSetOffset(int id, int semitones, int fine); // IOP 0x00009f94
void  RxSndDrvNoteSetModDepth(int id, int depth);   // IOP 0x00009fcc
void  RxSndDrvNoteSetModRate(int id, int rate);     // IOP 0x0000a058
u_int RxSndDrvNoteGetFlags(int id);                 // IOP 0x0000a0a8

/* ---- adpcm.cpp --------------------------------------------------------- */

int   RxSndDrvAdpcmOpen(int id, int pitch, const u_int *chans, int count, u_int iopSize,
                        int spuSize, int priority); // IOP 0x0000a1a0
void  RxSndDrvAdpcmInit(void);                      // IOP 0x0000af9c
void  RxSndDrvAdpcmCloseAll(void);                  // IOP 0x0000b044
int   RxSndDrvAdpcmData(int id, int size, int end); // IOP 0x0000b084
void  RxSndDrvAdpcmSet(u_int id, const u_int *data);    // IOP 0x0000b190
void  RxSndDrvAdpcmChannelSet(int id, u_int no, const u_int *data); // IOP 0x0000b30c
int   RxSndDrvAdpcmStatus(u_char *reply);           // IOP 0x0000b450
void  RxSndDrvAdpcmTick(void);                      // IOP 0x0000b4cc

/* ---- pcm.cpp ----------------------------------------------------------- */

void  RxSndDrvPcmInit(void);                        // IOP 0x0000cbd4
void  RxSndDrvPcmTick(void);                        // IOP 0x0000b790
void  RxSndDrvPcmCloseAll(void);                    // IOP 0x0000ccc0
int   RxSndDrvPcmOpen(int id, int format, u_int iop, u_int blocks); // IOP 0x0000cd10
void  RxSndDrvPcmData(int id, const u_int *seq, int count);         // IOP 0x0000ce84
void  RxSndDrvPcmSet(u_int id, const u_int *data);  // IOP 0x0000cfec
int   RxSndDrvPcmStatus(u_char *reply);             // IOP 0x0000d13c
void  RxSndDrvPcmSetB4(int value);                  // IOP 0x0000d1dc (FS2 only)

#endif /* YAMIPAN_IOP_RXSNDDRV_H */
