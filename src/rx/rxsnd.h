/* ==========================================================================
 *  rx/rxsnd.h
 *
 *  The rx sound library (0x159bc0..0x162178, pinned; no file string;
 *  "rxsnd :" messages): the EE client of the IOP sound module RXSNDDRV.IRX
 *  (RPC 0xd000). Commands are either sent at once (rxSndRpcCommand) or
 *  queued and flushed once a frame by rxSndUpdate (rxSndQueue*). Families:
 *  0x1x system, 0x2x IOP heap, 0x4x XVAB banks, 0x6x XSEQ, 0x7x BGM, 0x8x SE
 *  voices, 0xax ADPCM streams, 0xbx PCM streams. Each family keeps its slots
 *  on an active / stopping / free list; the per-frame status replies move them.
 *
 *  FS2 links Rule of Rose's prebuilt rx (RosePan's src/rx/rxsnd.h is the
 *  same library, names from there): 215 of its functions are RoR's word for
 *  word (addresses aside). FS2's build adds the commands 0x52 / 0x53 / 0x62,
 *  the four GetActiveList functions, a 64-bit ADPCM position, a multiplied
 *  rxSndAdpcmGetPosition and a queue that keeps room for its end marker.
 *  On the host the IOP side is the reconstructed driver, src/iop/rxsnddrv/
 *  (RosePan's, with the platform layer).
 * ======================================================================== */
#ifndef _RX_RXSND_H
#define _RX_RXSND_H

#include "sdk/scetypes.h"
#include "sdk/sifrpc.h"

#include <assert.h>     /* static_assert in C11 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RxSndQueueHeader      RxSndQueueHeader;
typedef struct RxSndCmdSet           RxSndCmdSet;
typedef struct RxSndCmdChannelSet    RxSndCmdChannelSet;
typedef struct RxSndCmdPcmOpen       RxSndCmdPcmOpen;
typedef struct RxSndCmdPcmData       RxSndCmdPcmData;
typedef struct RxSndCmdBgmChannelSet RxSndCmdBgmChannelSet;
typedef struct RxSndCmdTransfer      RxSndCmdTransfer;
typedef struct RxSndCmdSePlay        RxSndCmdSePlay;
typedef struct RxSndCmdAdpcmOpen     RxSndCmdAdpcmOpen;
typedef struct RxSndCmdAdpcmBuf      RxSndCmdAdpcmBuf;
typedef struct RxSndCmdAdpcmData     RxSndCmdAdpcmData;
typedef struct RxSndStatusEntry      RxSndStatusEntry;
typedef struct RxSndBgmStatus        RxSndBgmStatus;
typedef struct RxSndSeStatus         RxSndSeStatus;
typedef struct RxSndPcmStatusEntry   RxSndPcmStatusEntry;
typedef struct RxSndPcmStatus        RxSndPcmStatus;
typedef struct RxSndAdpcmStatusEntry RxSndAdpcmStatusEntry;
typedef struct RxSndAdpcmStatus      RxSndAdpcmStatus;
typedef struct RxSndConfig           RxSndConfig;
typedef struct RxSndIopBuf           RxSndIopBuf;
typedef struct RxSndSpuNode          RxSndSpuNode;
typedef struct RxSndSpuSlot          RxSndSpuSlot;
typedef struct RxSndSpuRegion        RxSndSpuRegion;
typedef struct RxSndXvab             RxSndXvab;
typedef struct RxSndXseq             RxSndXseq;
typedef struct RxSndBgm              RxSndBgm;
typedef struct RxSndSeVoice          RxSndSeVoice;
typedef struct RxSndPosCallback      RxSndPosCallback;
typedef struct RxSndPcm              RxSndPcm;
typedef struct RxSndAdpcm            RxSndAdpcm;

/* ---- IOP command data (EE layout: it is the RPC payload) --------------- */

/* The queue's per-command header; the command's data follows. EE 0xc. */
struct RxSndQueueHeader
{
    u_int m_cmd;                        // FS2 +0x0
    int   m_replySize;                  // FS2 +0x4: bytes reserved in the reply area
    int   m_dataSize;                   // FS2 +0x8
};
static_assert(sizeof(RxSndQueueHeader) == 0xc, "RxSndQueueHeader");
static_assert(offsetof(RxSndQueueHeader, m_replySize) == 0x4, "RxSndQueueHeader::m_replySize");
static_assert(offsetof(RxSndQueueHeader, m_dataSize) == 0x8, "RxSndQueueHeader::m_dataSize");

/* {slot, parameter, value} of the set commands 0x71 / 0x82 / 0xa2 / 0xb2.
 * Parameters seen: 0 stop / close, 1 pause, 2 attribute, 3 (0xb2: 4)
 * volume (value = two s16 L / R), 4 pitch (value = pitch * 4096). EE 0xc. */
struct RxSndCmdSet
{
    int m_id;                           // FS2 +0x0
    int m_param;                        // FS2 +0x4
    union
    {
        int   m_value;                  // FS2 +0x8
        short m_vol[2];                 // FS2 +0x8: L, R
    };
};
static_assert(sizeof(RxSndCmdSet) == 0xc, "RxSndCmdSet");
static_assert(offsetof(RxSndCmdSet, m_param) == 0x4, "RxSndCmdSet::m_param");
static_assert(offsetof(RxSndCmdSet, m_value) == 0x8, "RxSndCmdSet::m_value");

/* Command 0xa4: one channel's parameter of an ADPCM stream (the volume of
 * a mono / stereo stream goes per channel). EE 0xc. */
struct RxSndCmdChannelSet
{
    u_short m_id;                       // FS2 +0x0
    u_short m_channel;                  // FS2 +0x2
    u_short m_param;                    // FS2 +0x4: 3 volume, 5 not known
    u_short m_6;                        // FS2 +0x6: always 0
    union
    {
        short m_vol[2];                 // FS2 +0x8: L, R (the other side 0)
        int   m_value;                  // FS2 +0x8: parameter 5
    };
};
static_assert(sizeof(RxSndCmdChannelSet) == 0xc, "RxSndCmdChannelSet");
static_assert(offsetof(RxSndCmdChannelSet, m_channel) == 0x2, "RxSndCmdChannelSet::m_channel");
static_assert(offsetof(RxSndCmdChannelSet, m_param) == 0x4, "RxSndCmdChannelSet::m_param");
static_assert(offsetof(RxSndCmdChannelSet, m_vol) == 0x8, "RxSndCmdChannelSet::m_vol");

/* Queued command 0xb0: open a PCM stream. EE 0xc. */
struct RxSndCmdPcmOpen
{
    u_char m_id;                        // FS2 +0x0
    u_char m_mode;                      // FS2 +0x1
    u_char _2[2];
    u_int  m_iopBuf;                    // FS2 +0x4
    int    m_blocks;                    // FS2 +0x8
};
static_assert(sizeof(RxSndCmdPcmOpen) == 0xc, "RxSndCmdPcmOpen");
static_assert(offsetof(RxSndCmdPcmOpen, m_mode) == 0x1, "RxSndCmdPcmOpen::m_mode");
static_assert(offsetof(RxSndCmdPcmOpen, m_iopBuf) == 0x4, "RxSndCmdPcmOpen::m_iopBuf");
static_assert(offsetof(RxSndCmdPcmOpen, m_blocks) == 0x8, "RxSndCmdPcmOpen::m_blocks");

/* Queued command 0xb1: m_count more blocks of a PCM stream are filled; their
 * sequence numbers (int each) follow. EE 0x4 + 4 * m_count. */
struct RxSndCmdPcmData
{
    u_char m_id;                        // FS2 +0x0
    u_char m_count;                     // FS2 +0x1
    u_char _2[2];
};
static_assert(sizeof(RxSndCmdPcmData) == 0x4, "RxSndCmdPcmData");
static_assert(offsetof(RxSndCmdPcmData, m_count) == 0x1, "RxSndCmdPcmData::m_count");

/* Commands 0x73 / 0x74: one channel's parameter of a BGM slot. EE 0xc. */
struct RxSndCmdBgmChannelSet
{
    u_char m_id;                        // FS2 +0x0
    u_char m_channel;                   // FS2 +0x1
    u_char _2[2];
    int    m_param;                     // FS2 +0x4: 3 volume
    short  m_vol[2];                    // FS2 +0x8: L, R
};
static_assert(sizeof(RxSndCmdBgmChannelSet) == 0xc, "RxSndCmdBgmChannelSet");
static_assert(offsetof(RxSndCmdBgmChannelSet, m_channel) == 0x1, "RxSndCmdBgmChannelSet::m_channel");
static_assert(offsetof(RxSndCmdBgmChannelSet, m_param) == 0x4, "RxSndCmdBgmChannelSet::m_param");
static_assert(offsetof(RxSndCmdBgmChannelSet, m_vol) == 0x8, "RxSndCmdBgmChannelSet::m_vol");

/* Command 0x40: an EE -> SPU transfer. EE 0x10. */
struct RxSndCmdTransfer
{
    u_char m_type;                      // FS2 +0x0: 3
    u_char m_dir;                       // FS2 +0x1: 1
    u_char _2[2];
    u_int  m_spuAddr;                   // FS2 +0x4
    u_int  m_eeAddr;                    // FS2 +0x8
    int    m_size;                      // FS2 +0xc
};
static_assert(sizeof(RxSndCmdTransfer) == 0x10, "RxSndCmdTransfer");
static_assert(offsetof(RxSndCmdTransfer, m_dir) == 0x1, "RxSndCmdTransfer::m_dir");
static_assert(offsetof(RxSndCmdTransfer, m_spuAddr) == 0x4, "RxSndCmdTransfer::m_spuAddr");
static_assert(offsetof(RxSndCmdTransfer, m_eeAddr) == 0x8, "RxSndCmdTransfer::m_eeAddr");
static_assert(offsetof(RxSndCmdTransfer, m_size) == 0xc, "RxSndCmdTransfer::m_size");

/* Queued commands 0x80 / 0x81: start an SE voice. EE 0x4. */
struct RxSndCmdSePlay
{
    u_char m_voice;                     // FS2 +0x0
    u_char m_xvab;                      // FS2 +0x1
    u_char m_prog;                      // FS2 +0x2
    u_char m_note;                      // FS2 +0x3
};
static_assert(sizeof(RxSndCmdSePlay) == 0x4, "RxSndCmdSePlay");
static_assert(offsetof(RxSndCmdSePlay, m_xvab) == 0x1, "RxSndCmdSePlay::m_xvab");
static_assert(offsetof(RxSndCmdSePlay, m_prog) == 0x2, "RxSndCmdSePlay::m_prog");
static_assert(offsetof(RxSndCmdSePlay, m_note) == 0x3, "RxSndCmdSePlay::m_note");

/* Queued command 0xa0: open an ADPCM stream; m_channels RxSndCmdAdpcmBuf
 * follow. EE 0x10. */
struct RxSndCmdAdpcmOpen
{
    u_char m_id;                        // FS2 +0x0
    u_char _1;
    u_char m_channels;                  // FS2 +0x2
    u_char _3;
    short  m_arg;                       // FS2 +0x4: rxSndAdpcmOpenEx's argument, -1
    short  m_pitch;                     // FS2 +0x6: rate * 4096 / 48000
    int    m_iopSize;                   // FS2 +0x8: per channel
    int    m_spuSize;                   // FS2 +0xc: per channel
};
static_assert(sizeof(RxSndCmdAdpcmOpen) == 0x10, "RxSndCmdAdpcmOpen");
static_assert(offsetof(RxSndCmdAdpcmOpen, m_channels) == 0x2, "RxSndCmdAdpcmOpen::m_channels");
static_assert(offsetof(RxSndCmdAdpcmOpen, m_arg) == 0x4, "RxSndCmdAdpcmOpen::m_arg");
static_assert(offsetof(RxSndCmdAdpcmOpen, m_pitch) == 0x6, "RxSndCmdAdpcmOpen::m_pitch");
static_assert(offsetof(RxSndCmdAdpcmOpen, m_iopSize) == 0x8, "RxSndCmdAdpcmOpen::m_iopSize");
static_assert(offsetof(RxSndCmdAdpcmOpen, m_spuSize) == 0xc, "RxSndCmdAdpcmOpen::m_spuSize");

struct RxSndCmdAdpcmBuf                 /* EE 0x8 */
{
    u_int m_iopBuf;                     // FS2 +0x0
    u_int m_spuBuf;                     // FS2 +0x4
};
static_assert(sizeof(RxSndCmdAdpcmBuf) == 0x8, "RxSndCmdAdpcmBuf");
static_assert(offsetof(RxSndCmdAdpcmBuf, m_spuBuf) == 0x4, "RxSndCmdAdpcmBuf::m_spuBuf");

/* Queued command 0xa1: `m_size` more bytes per channel are in the IOP ring
 * (whole 0x40 units); m_end 1: the last of them, the end block. EE 0x8. */
struct RxSndCmdAdpcmData
{
    u_char m_id;                        // FS2 +0x0
    u_char m_end;                       // FS2 +0x1
    u_char _2[2];
    int    m_size;                      // FS2 +0x4
};
static_assert(sizeof(RxSndCmdAdpcmData) == 0x8, "RxSndCmdAdpcmData");
static_assert(offsetof(RxSndCmdAdpcmData, m_end) == 0x1, "RxSndCmdAdpcmData::m_end");
static_assert(offsetof(RxSndCmdAdpcmData, m_size) == 0x4, "RxSndCmdAdpcmData::m_size");

/* ---- status replies (queued commands 0x72 / 0x83 / 0xb3 / 0xa3) -------- */
/* Each lists the slots the IOP still plays: the slot id and its status bits
 * (2 playing, 4 ended, 8 paused), then the count. */

struct RxSndStatusEntry                 /* EE 0x4 */
{
    u_char m_id;                        // FS2 +0x0
    u_char m_status;                    // FS2 +0x1
    u_char _2[2];
};
static_assert(sizeof(RxSndStatusEntry) == 0x4, "RxSndStatusEntry");
static_assert(offsetof(RxSndStatusEntry, m_status) == 0x1, "RxSndStatusEntry::m_status");

struct RxSndBgmStatus                   /* EE 0x104 */
{
    RxSndStatusEntry m_entry[32];       // FS2 +0x0
    u_int            m_value[32];       // FS2 +0x80: per entry (stored in RxSndBgm::m_value)
    int              m_count;           // FS2 +0x100
};
static_assert(sizeof(RxSndBgmStatus) == 0x104, "RxSndBgmStatus");
static_assert(offsetof(RxSndBgmStatus, m_value) == 0x80, "RxSndBgmStatus::m_value");
static_assert(offsetof(RxSndBgmStatus, m_count) == 0x100, "RxSndBgmStatus::m_count");

struct RxSndSeStatus                    /* EE 0x104 */
{
    RxSndStatusEntry m_entry[64];       // FS2 +0x0
    int              m_count;           // FS2 +0x100
};
static_assert(sizeof(RxSndSeStatus) == 0x104, "RxSndSeStatus");
static_assert(offsetof(RxSndSeStatus, m_count) == 0x100, "RxSndSeStatus::m_count");

struct RxSndPcmStatusEntry              /* EE 0xc */
{
    u_char m_id;                        // FS2 +0x0
    u_char m_status;                    // FS2 +0x1
    short  m_2;                         // FS2 +0x2: the IOP's filled blocks not yet mixed (pcm.cpp), stored in RxSndPcm::m_queued
    int    m_posHi;                     // FS2 +0x4: play position, compared signed
    int    m_posLo;                     // FS2 +0x8
};
static_assert(sizeof(RxSndPcmStatusEntry) == 0xc, "RxSndPcmStatusEntry");
static_assert(offsetof(RxSndPcmStatusEntry, m_2) == 0x2, "RxSndPcmStatusEntry::m_2");
static_assert(offsetof(RxSndPcmStatusEntry, m_posHi) == 0x4, "RxSndPcmStatusEntry::m_posHi");
static_assert(offsetof(RxSndPcmStatusEntry, m_posLo) == 0x8, "RxSndPcmStatusEntry::m_posLo");

struct RxSndPcmStatus                   /* EE 0x184 */
{
    RxSndPcmStatusEntry m_entry[32];    // FS2 +0x0
    int                 m_count;        // FS2 +0x180
};
static_assert(sizeof(RxSndPcmStatus) == 0x184, "RxSndPcmStatus");
static_assert(offsetof(RxSndPcmStatus, m_count) == 0x180, "RxSndPcmStatus::m_count");

struct RxSndAdpcmStatusEntry            /* EE 0x10 */
{
    u_char m_id;                        // FS2 +0x0
    u_char m_status;                    // FS2 +0x1
    u_char _2[2];
    int    m_free;                      // FS2 +0x4: bytes the IOP buffer can take
    u_int  m_posHi;                     // FS2 +0x8: play position, compared unsigned
    u_int  m_posLo;                     // FS2 +0xc
};
static_assert(sizeof(RxSndAdpcmStatusEntry) == 0x10, "RxSndAdpcmStatusEntry");
static_assert(offsetof(RxSndAdpcmStatusEntry, m_status) == 0x1, "RxSndAdpcmStatusEntry::m_status");
static_assert(offsetof(RxSndAdpcmStatusEntry, m_free) == 0x4, "RxSndAdpcmStatusEntry::m_free");
static_assert(offsetof(RxSndAdpcmStatusEntry, m_posHi) == 0x8, "RxSndAdpcmStatusEntry::m_posHi");
static_assert(offsetof(RxSndAdpcmStatusEntry, m_posLo) == 0xc, "RxSndAdpcmStatusEntry::m_posLo");

struct RxSndAdpcmStatus                 /* EE 0x304 */
{
    RxSndAdpcmStatusEntry m_entry[48];  // FS2 +0x0
    int                   m_count;      // FS2 +0x300
};
static_assert(sizeof(RxSndAdpcmStatus) == 0x304, "RxSndAdpcmStatus");
static_assert(offsetof(RxSndAdpcmStatus, m_count) == 0x300, "RxSndAdpcmStatus::m_count");

/* ---- EE-side state ----------------------------------------------------- */

/* bss 0x4fdf00. EE 0x8. */
struct RxSndConfig
{
    u_char m_debugFlags;                // FS2 +0x0: 1 errors, 2 BG-call warnings, 4 bind messages
    u_char m_1;                         // FS2 +0x1: set 1, not read here (unverified)
    u_char m_2;                         // FS2 +0x2: set 0x3c, not read here (unverified)
    u_char _3;
    short  m_masterVolL;                // FS2 +0x4
    short  m_masterVolR;                // FS2 +0x6
};
static_assert(sizeof(RxSndConfig) == 0x8, "RxSndConfig");
static_assert(offsetof(RxSndConfig, m_masterVolL) == 0x4, "RxSndConfig::m_masterVolL");
static_assert(offsetof(RxSndConfig, m_masterVolR) == 0x6, "RxSndConfig::m_masterVolR");

/* bss 0x4fef40: the bounce buffers of the unaligned EE <-> IOP copies, one
 * per buffer bank: EE ones (rxSndIopRead) and two IOP ones taken at init. EE 0x80. */
struct RxSndIopBuf
{
    u_char m_eeBuf[2][0x20];            // FS2 +0x0: EE side, rxSndIopRead's bounce
    u_int m_buf[2];                     // FS2 +0x40: IOP address, one per buffer bank (rxSndIopWrite's bounce)
    u_int _48[0xe];
};
static_assert(sizeof(RxSndIopBuf) == 0x80, "RxSndIopBuf");
static_assert(offsetof(RxSndIopBuf, m_buf) == 0x40, "RxSndIopBuf::m_buf");

/* A block of the SPU heap (bss 0x4fefc0, 256 of them), on a circular list
 * through the sentinel 0x5007c0: m_prev runs up the SPU addresses from
 * 0x5010, m_next down from 0x1c0000. A block's address is the sum of the
 * sizes before it (m_addr is only set at init). layout: host -- pointers.
 * EE 0x18. */
struct RxSndSpuNode
{
    RxSndSpuNode *m_next;               // FS2 +0x0
    RxSndSpuNode *m_prev;               // FS2 +0x4: also the free node list's link
    RxSndSpuSlot *m_owner;              // FS2 +0x8: the slot it was allocated for
    int           m_used;               // FS2 +0xc
    u_int         m_addr;               // FS2 +0x10: SPU address (init only)
    u_int         m_size;               // FS2 +0x14
};

/* A window of SPU memory the allocator may place blocks in (bss 0x5007e0,
 * 256 of them; rxSndSpuHeapGetSlot). EE 0x14. */
struct RxSndSpuSlot
{
    int   m_index;                      // FS2 +0x0: -1 for the whole region
    int   m_used;                       // FS2 +0x4
    u_int m_addr;                       // FS2 +0x8: SPU address
    u_int m_size;                       // FS2 +0xc
    int   m_count;                      // FS2 +0x10: blocks allocated in it
};
static_assert(sizeof(RxSndSpuSlot) == 0x14, "RxSndSpuSlot");
static_assert(offsetof(RxSndSpuSlot, m_used) == 0x4, "RxSndSpuSlot::m_used");
static_assert(offsetof(RxSndSpuSlot, m_addr) == 0x8, "RxSndSpuSlot::m_addr");
static_assert(offsetof(RxSndSpuSlot, m_size) == 0xc, "RxSndSpuSlot::m_size");
static_assert(offsetof(RxSndSpuSlot, m_count) == 0x10, "RxSndSpuSlot::m_count");

/* bss 0x501be0: the whole SPU heap as a slot, and the slot each buffer bank
 * allocates from (-1: the whole region). EE 0x20. */
struct RxSndSpuRegion
{
    RxSndSpuSlot m_slot;                // FS2 +0x0: index -1, used, 0x5010, 0x1baff0
    int          m_bankSlot[2];         // FS2 +0x14
    int          _1c;
};
static_assert(sizeof(RxSndSpuRegion) == 0x20, "RxSndSpuRegion");
static_assert(offsetof(RxSndSpuRegion, m_bankSlot) == 0x14, "RxSndSpuRegion::m_bankSlot");

/* An XVAB bank (bss 0x501c00, 256 of them; filled by rxSndXvabLoad). EE 0x18. */
struct RxSndXvab
{
    u_int  m_flags;                     // FS2 +0x0: bit 0 used, 0x10000 / 0x20000 m_iopBuf / m_spuBuf are ours, 0x40000 samples sent
    u_int  m_iopBuf;                    // FS2 +0x4: IOP address of the bank file
    int    m_iopSize;                   // FS2 +0x8
    u_int  m_spuBuf;                    // FS2 +0xc: SPU address of the samples
    int    m_spuSize;                   // FS2 +0x10
    int    m_transfer;                  // FS2 +0x14: IOP transfer handle, -1 when done
};
static_assert(sizeof(RxSndXvab) == 0x18, "RxSndXvab");
static_assert(offsetof(RxSndXvab, m_iopBuf) == 0x4, "RxSndXvab::m_iopBuf");
static_assert(offsetof(RxSndXvab, m_iopSize) == 0x8, "RxSndXvab::m_iopSize");
static_assert(offsetof(RxSndXvab, m_spuBuf) == 0xc, "RxSndXvab::m_spuBuf");
static_assert(offsetof(RxSndXvab, m_spuSize) == 0x10, "RxSndXvab::m_spuSize");
static_assert(offsetof(RxSndXvab, m_transfer) == 0x14, "RxSndXvab::m_transfer");

/* An XSEQ sequence (bss 0x503400, 256 of them; filled by rxSndXseqLoad). EE 0xc. */
struct RxSndXseq
{
    u_int m_flags;                      // FS2 +0x0: bit 0 used, 0x10000 m_iopBuf is ours
    u_int m_iopBuf;                     // FS2 +0x4: IOP address of the file
    int   m_iopSize;                    // FS2 +0x8
};
static_assert(sizeof(RxSndXseq) == 0xc, "RxSndXseq");
static_assert(offsetof(RxSndXseq, m_iopBuf) == 0x4, "RxSndXseq::m_iopBuf");
static_assert(offsetof(RxSndXseq, m_iopSize) == 0x8, "RxSndXseq::m_iopSize");

/* A BGM slot (bss 0x504000, 32). layout: host -- next pointer. EE 0x18. */
struct RxSndBgm
{
    RxSndBgm *m_next;                   // FS2 +0x0
    int       m_id;                     // FS2 +0x4: index, also the IOP's id
    u_int     m_flags;                  // FS2 +0x8: 1 open, 2 playing, 4 ended, 8 paused
    int       m_keep;                   // FS2 +0xc: 0 = stop by itself when ended (unverified)
    u_int     m_value;                  // FS2 +0x10: from RxSndBgmStatus::m_value
    short     m_volL;                   // FS2 +0x14
    short     m_volR;                   // FS2 +0x16
};

/* An SE voice (bss 0x504310, 64). layout: host -- next pointer. EE 0x14. */
struct RxSndSeVoice
{
    RxSndSeVoice *m_next;               // FS2 +0x0
    int           m_id;                 // FS2 +0x4
    u_int         m_flags;              // FS2 +0x8: 1 open, 2 playing, 4 ended, 8 paused
    int           m_keep;               // FS2 +0xc: 0 = stop by itself when ended (unverified)
    short         m_volL;               // FS2 +0x10
    short         m_volR;               // FS2 +0x12
};

/* A play-position callback of a PCM (pool 0x504840) or ADPCM (pool
 * 0x507400) stream, 256 each, sorted by position. layout: host. EE 0x14. */
typedef void (*RxSndPosFunc)(int id, int arg);
struct RxSndPosCallback
{
    RxSndPosCallback *m_next;           // FS2 +0x0
    u_int             m_posHi;          // FS2 +0x4
    u_int             m_posLo;          // FS2 +0x8
    RxSndPosFunc      m_func;           // FS2 +0xc
    int               m_arg;            // FS2 +0x10
};

/* A PCM stream (bss 0x505c44, 32): an IOP ring of m_blocks blocks of
 * m_blockSize bytes (mode 0: 0x800, modes 1..3: 0x1000; mode 1 keeps the
 * left half of each block in the first m_blocks * 0x800 bytes and the right
 * half after them). Whole blocks are announced to the IOP with a running
 * sequence number. layout: host -- pointers. EE 0x3c. */
struct RxSndPcm
{
    RxSndPcm         *m_next;           // FS2 +0x0
    int               m_id;             // FS2 +0x4
    int               m_user;           // FS2 +0x8: rxSndPcmSetUserData, unread by the library
    u_int             m_iopBuf;         // FS2 +0xc: IOP address
    int               m_blocks;         // FS2 +0x10
    int               m_blockSize;      // FS2 +0x14
    u_char            m_status;         // FS2 +0x18: 1 open, 2 playing, 4 ended, 8 paused
    u_char            m_mode;           // FS2 +0x19: 0..3
    u_char            m_alloc;          // FS2 +0x1a: bit 0 m_iopBuf is ours
    u_char            _1b;
    short             m_volL;           // FS2 +0x1c
    short             m_volR;           // FS2 +0x1e
    int               m_queued;         // FS2 +0x20: blocks the IOP holds (RxSndPcmStatusEntry::m_2)
    int               m_ringSize;       // FS2 +0x24: m_blocks * m_blockSize
    int               m_writePos;       // FS2 +0x28: next write offset in the ring
    int               m_pending;        // FS2 +0x2c: bytes written past the last whole block
    int               m_seq;            // FS2 +0x30: sequence number of the next block
    int               m_attr;           // FS2 +0x34: rxSndPcmSetAttr; rxSndPcmEnd's mode 3 padding
    RxSndPosCallback *m_callbacks;      // FS2 +0x38
};

/* An ADPCM stream (bss 0x508808, 48). Each channel has m_bufSize bytes of
 * IOP ring at m_iopBuf + channel * m_bufSize and m_spuSize bytes of SPU
 * buffer. layout: host -- pointers. EE 0x50. */
struct RxSndAdpcm
{
    RxSndAdpcm       *m_next;           // FS2 +0x0
    int               m_mode;           // FS2 +0x4: 0 mono, 1 stereo from two buffers, 2 stereo interleaved, 0x1000000 n channels interleaved
    u_char            m_id;             // FS2 +0x8
    u_char            m_status;         // FS2 +0x9: 1 open, 2 playing, 4 ended, 8 paused
    u_char            m_alloc;          // FS2 +0xa: bit 0 m_iopBuf, bit 1 m_spuBuf are ours
    u_char            m_end;            // FS2 +0xb: rxSndAdpcmEnd sent the end block
    u_char            m_channels;       // FS2 +0xc
    u_char            _d[3];
    int               m_user;           // FS2 +0x10: rxSndAdpcmSetUserData, 0 at open
    short             m_volL;           // FS2 +0x14
    short             m_volR;           // FS2 +0x16
    u_int             m_iopBuf;         // FS2 +0x18: IOP address
    int               m_bufSize;        // FS2 +0x1c: per channel
    int               m_bufFill;        // FS2 +0x20: m_bufSize - the IOP's free bytes
    int               m_writePos;       // FS2 +0x24: next write offset in the ring
    int               m_pending;        // FS2 +0x28: bytes written, not yet announced (< 0x40)
    u_int             m_spuBuf;         // FS2 +0x2c: SPU address
    int               m_spuSize;        // FS2 +0x30: per channel
    int               m_spuPos;         // FS2 +0x34: write position modulo m_spuSize
    u_int             m_writtenHi;      // FS2 +0x38: bytes written per channel, 64-bit
    u_int             m_writtenLo;      // FS2 +0x3c
    u_long            m_position;       // FS2 +0x40: per channel; the status reply's two words as hi:lo
    RxSndPosCallback *m_callbacks;      // FS2 +0x48
};

typedef u_int (*RxSndAllocFunc)(int size);
typedef void  (*RxSndFreeFunc)(u_int addr);
typedef void  (*RxSndBgCallHook)(const char *func);

/* ---- functions --------------------------------------------------------- */

/* rxTh yield callback: selects the RPC buffer bank (1 while yielding). */
void  rxSndThreadCallback(int yielding);                                 /* FS2 0x00159bc0 */
/* BG-call hook: warns when a library call comes from a yielded BG thread. */
void  rxSndCheckBgCall(const char *func);                                /* FS2 0x00159bd8 */
void  rxSndInit(void);                                                   /* FS2 0x00159c58 */
void  rxSndTerm(void);                                                   /* FS2 0x00159c88 */
void  rxSndInitModules(void);                                            /* FS2 0x00159ca8 */
void  rxSndTermModules(void);                                            /* FS2 0x00159d08 */
/* Once a frame: flushes the queue, asks every family's status, applies it. */
void  rxSndUpdate(void);                                                 /* FS2 0x00159d88 */
/* Command 0x30 {2, mode != 0} (stereo / mono output: not known). */
void  rxSndSetOutputMode(int mode);                                      /* FS2 0x00159e68 */
/* Stores the flags (command 0x30 {0, flags} tells the IOP); the old ones back. */
u_char rxSndSetDebugFlags(int flags);                                   /* FS2 0x00159eb8 */
u_char rxSndGetDebugFlags(void);                                         /* FS2 0x00159f10 */
/* Command 0x30 {1, rate}: 50 passes, anything else sends 60. */
void  rxSndSetFrameRate(int rate);                                       /* FS2 0x00159f20 */
/* Command 0x30 {3 + kind, value} for kind 0..2 (meaning not known). */
void  rxSndSetSysParam(int kind, int value);                             /* FS2 0x00159f78 */
/* Queued command 0x31 {id, 1, L, R}. */
void  rxSndSetSysVolume(int id, int volL, int volR);                     /* FS2 0x00159ff8 */
void  rxSndSetMasterVolume(int volL, int volR);                          /* FS2 0x0015a060 */
int   rxSndSetBufferBank(int bank);                                      /* FS2 0x0015a140 */
/* Commands 0x90 / 0x93 / 0x92: 48 words, one per SPU2 voice; 0x91 sets one. */
void  rxSndGetVoiceTable(u_int *out);                                    /* FS2 0x0015a178 */
void  rxSndGetVoiceTableEx(int arg, u_int *out);                         /* FS2 0x0015a1d8 */
void  rxSndSetVoiceEntry(u_int voice, int value);                        /* FS2 0x0015a238 */
void  rxSndGetVoiceTable2(u_int *out);                                   /* FS2 0x0015a280 */
/* The pitch ratio of `cents` (clamped to +-4800): 2^(cents / 1200) by table. */
float rxSndCentToPitch(int cents);                                       /* FS2 0x0015a2e0 */
void  rxSndInitConfig(void);                                             /* FS2 0x0015a350 */
void  rxSndTermConfig(void);                                             /* FS2 0x0015a380 */
int   rxSndGetBufferBank(void);                                          /* FS2 0x0015a390 */
short rxSndGetMasterVolumeL(void);                                       /* FS2 0x0015a3a0 */
short rxSndGetMasterVolumeR(void);                                       /* FS2 0x0015a3b0 */
void *rxSndGetWorkBuffer(void);                                          /* FS2 0x0015a3c0 */
void  rxSndWait(void);                                                   /* FS2 0x0015a3d0 */
void  rxSndSetBgCallHook(RxSndBgCallHook hook);                          /* FS2 0x0015a3d8 */
void  rxSndCallBgCallHook(const char *func);                             /* FS2 0x0015a3e8 */
void  rxSndMemMove(void *dst, const void *src, int size);              /* FS2 0x0015a410 */
u_int rxSndDefIopAlloc(int size);                                        /* FS2 0x0015a490 */
u_int rxSndDefIopAllocMode1(int size);                                   /* FS2 0x0015a508 */
void  rxSndDefIopFree(u_int addr);                                       /* FS2 0x0015a588 */
void  rxSndSetIopAllocHook(RxSndAllocFunc func);                         /* FS2 0x0015a5b8 */
void  rxSndSetIopAllocMode1Hook(RxSndAllocFunc func);                    /* FS2 0x0015a5d8 */
void  rxSndSetIopFreeHook(RxSndFreeFunc func);                           /* FS2 0x0015a5f8 */
u_int rxSndIopAlloc(int size);                                           /* FS2 0x0015a618 */
u_int rxSndIopAllocMode1(int size);                                      /* FS2 0x0015a640 */
void  rxSndIopFree(u_int addr);                                          /* FS2 0x0015a668 */
/* Copies `size` bytes of EE memory to IOP address `iopAddr`: one SIF DMA
 * when everything is 16-byte aligned, else the unaligned head and tail go
 * through a bounce buffer and IOP command 0x23. Returns 1. */
int   rxSndIopWrite(u_int iopAddr, const void *src, int size);           /* FS2 0x0015a690 */
/* IOP address `iopAddr` -> EE `dst`, the mirror of rxSndIopWrite. Returns 1. */
int   rxSndIopRead(void *dst, u_int iopAddr, int size);                  /* FS2 0x0015a8a0 */
/* Writes the work buffer to `iopAddr` in 0x1000-byte chunks until `size`
 * bytes went (the ROM never advances the address). Returns 1. */
int   rxSndIopWriteWorkBuffer(u_int iopAddr, int size);                  /* FS2 0x0015aa88 */
/* rxSndIopAlloc + rxSndIopWrite; the IOP address, 0 when the heap is full. */
u_int rxSndIopAllocWrite(const void *src, int size);                     /* FS2 0x0015ab00 */
void  rxSndIopHeapCmd1(void);                                            /* FS2 0x0015ab58 */
void  rxSndIopHeapCmd2(void);                                            /* FS2 0x0015ab90 */
void  rxSndIopCopy(u_int dst, u_int src, int size);                      /* FS2 0x0015abc8 */
void  rxSndIopBufInit(void);                                             /* FS2 0x0015ac00 */
void  rxSndIopBufTerm(void);                                             /* FS2 0x0015ac50 */
/* Defines SPU slot `id` (-1: the first unused) over [addr, addr + size)
 * inside the heap; the id, -1 when out of range. */
int   rxSndSpuHeapSetSlot(int id, u_int addr, u_int size);               /* FS2 0x0015aca0 */
void  rxSndSpuHeapReleaseSlot(u_int id);                                 /* FS2 0x0015add8 */
/* The slot the current buffer bank allocates from (-1: the whole heap). */
int   rxSndSpuHeapSetBankSlot(int slot);                                 /* FS2 0x0015ae30 */
int   rxSndSpuHeapGetBankSlot(void);                                     /* FS2 0x0015ae90 */
u_int rxSndDefSpuAlloc(int size);                                        /* FS2 0x0015aec8 */
u_int rxSndDefSpuAllocMode1(int size);                                   /* FS2 0x0015aee8 */
void  rxSndDefSpuFree(u_int addr);                                       /* FS2 0x0015af08 */
void  rxSndSetSpuAllocHook(RxSndAllocFunc func);                         /* FS2 0x0015af20 */
void  rxSndSetSpuAllocMode1Hook(RxSndAllocFunc func);                    /* FS2 0x0015af40 */
void  rxSndSetSpuFreeHook(RxSndFreeFunc func);                           /* FS2 0x0015af60 */
u_int rxSndSpuAlloc(int size);                                           /* FS2 0x0015af80 */
u_int rxSndSpuAllocMode1(int size);                                      /* FS2 0x0015afa8 */
void  rxSndSpuFree(u_int addr);                                          /* FS2 0x0015afd0 */
/* Command 0x40: starts sending `size` bytes of EE memory to SPU address
 * `spuAddr`; the handle for rxSndXvabPoll, negative while the IOP is busy. */
int   rxSndSpuTransfer(u_int spuAddr, const void *src, int size);        /* FS2 0x0015aff8 */
/* Command 0x40 with the other transfer types (directions not known). */
void  rxSndSpuTransferType4(u_int a, u_int b, int size);                 /* FS2 0x0015b040 */
void  rxSndSpuTransferType1(u_int a, u_int b, int size);                 /* FS2 0x0015b088 */
void  rxSndSpuTransferType2(u_int a, u_int b, int size);                 /* FS2 0x0015b0d0 */
void  rxSndSpuTransferType5(u_int a, int size);                          /* FS2 0x0015b118 */
/* Asks the IOP whether XVAB transfer `handle` is still running (command
 * 0x41); wait != 0 polls until it is done. */
int   rxSndXvabPoll(int handle, int wait);                               /* FS2 0x0015b160 */
u_int rxSndSpuHeapGetFreeSize(void);                                     /* FS2 0x0015b1f8 */
u_int rxSndSpuHeapGetMaxFreeBlock(void);                                 /* FS2 0x0015b208 */
void  rxSndSpuHeapInit(void);                                            /* FS2 0x0015b258 */
void  rxSndSpuHeapTerm(void);                                            /* FS2 0x0015b378 */
RxSndSpuSlot *rxSndSpuHeapGetSlot(u_int id);                             /* FS2 0x0015b380 */
/* First fit of `size` (rounded up to 0x40) in the current bank's slot:
 * mode 0 lowest address, 1 highest, 2 exactly at `addr`. Returns the SPU
 * address, 0 when it does not fit. */
u_int rxSndSpuHeapAlloc(int mode, u_int addr, int size);                 /* FS2 0x0015b3b8 */
void  rxSndSpuHeapFree(u_int addr);                                      /* FS2 0x0015b7d0 */
/* Frees every block allocated in slot `id`. */
void  rxSndSpuHeapFreeSlot(u_int id);                                    /* FS2 0x0015b9d0 */
RxSndSpuNode *rxSndSpuHeapNewNode(void);                                 /* FS2 0x0015bac0 */
void  rxSndSpuHeapFreeNode(RxSndSpuNode *node);                          /* FS2 0x0015bae8 */
void  rxSndSpuHeapPrintStatus(void);                                     /* FS2 0x0015bb08 */
/* Queued command 0x32 {core, attribute, value}: the SPU2 cores' effect
 * (mode 0..9 as libsd's reverb modes, depth L / R, delay, feedback). */
void  rxSndSetEffectModeCore(int core, int mode);                        /* FS2 0x0015bd30 */
void  rxSndSetEffectDepthCore(int core, int depthL, int depthR);         /* FS2 0x0015bdd0 */
void  rxSndSetEffectDelayCore(int core, int delay);                      /* FS2 0x0015be58 */
void  rxSndSetEffectFeedbackCore(int core, int feedback);                /* FS2 0x0015bed8 */
void  rxSndSetEffectMode(int mode);                                      /* FS2 0x0015bf58 */
void  rxSndSetEffectDepth(int depthL, int depthR);                       /* FS2 0x0015bf90 */
void  rxSndSetEffectDelay(int delay);                                    /* FS2 0x0015bfd8 */
void  rxSndSetEffectFeedback(int feedback);                              /* FS2 0x0015c010 */
void  rxSndXvabInit(void);                                               /* FS2 0x0015c048 */
void  rxSndXvabTerm(void);                                               /* FS2 0x0015c050 */
/* A MIDI variable-length number at *p (7 bits a byte, 0x80 continues). */
u_int rxSndReadVarLen(const u_char **p);                                 /* FS2 0x0015c058 */
/* The bank's SPU samples: 1 and their address / size, or 0 when it has none. */
int   rxSndXvabGetSampleData(const u_char **data, int *size, const void *xvab); /* FS2 0x0015c970 */
u_int rxSndXvabGetIopAddr(int id);                                       /* FS2 0x0015c6c8 */
u_int rxSndXvabGetSpuAddr(int id);                                       /* FS2 0x0015c758 */
/* FS2 only: command 0x52 {id} and 0x53 {id, a, b} of a loaded bank (meaning
 * not known; nothing in FS2 calls them); the IOP's answer, 0 for a bad id. */
u_int rxSndXvabRpc52(int id);                                            /* FS2 0x0015c7e8 */
u_int rxSndXvabSetParam(int id, int a, int b);                           /* FS2 0x0015c890 */
int   rxSndXvabGetFileSize(const void *xvab);                            /* FS2 0x0015c958 */
const u_char *rxSndXvabGetSampleAddr(const void *xvab);                  /* FS2 0x0015ca48 */
int   rxSndXvabGetSampleSize(const void *xvab);                          /* FS2 0x0015ca78 */
/* XSEQ sequences: Load as rxSndXvabLoad (no SPU part; command 0x60), the id
 * or -1; Free as rxSndXvabFree (command 0x61). */
int   rxSndXseqLoad(int id, const void *xseq, u_int iopAddr);            /* FS2 0x0015caa8 */
int   rxSndXseqFree(int id, int wait);                                   /* FS2 0x0015cda0 */
/* Command 0x63 {id, a, b} (meaning not known). */
void  rxSndXseqSetParam(int id, int a, int b);                           /* FS2 0x0015cef0 */
/* FS2 only: command 0x62 {id} of a loaded sequence (not known; no caller). */
u_int rxSndXseqRpc62(int id);                                            /* FS2 0x0015d040 */
u_int rxSndXseqGetIopAddr(int id);                                       /* FS2 0x0015cfb0 */
int   rxSndXseqGetFileSize(const void *xseq);                            /* FS2 0x0015d0e8 */
void  rxSndXseqInit(void);                                               /* FS2 0x0015d100 */
void  rxSndXseqTerm(void);                                               /* FS2 0x0015d108 */
u_int rxSndReadU32(const void *p);                                       /* FS2 0x0015d110 */
/* XVAB / XSEQ files: words {magic, version, size}, then chunks {tag, size}
 * (version >= 0x200: a 16-byte file header and chunks {tag, ?, size, ?}). */
u_int rxSndChunkGetVersion(const void *file);                            /* FS2 0x0015d118 */
int   rxSndChunkGetFileSize(const void *file);                           /* FS2 0x0015d120 */
int   rxSndChunkFind(const u_char **data, int *size, const void *file, u_int tag); /* FS2 0x0015d160 */
const u_char *rxSndChunkFindData(const void *file, u_int tag);           /* FS2 0x0015d2b0 */
int   rxSndChunkGetSize(const void *file, u_int tag);                    /* FS2 0x0015d2e8 */
void  rxSndBgmSetMasterVolume(int volL, int volR);                       /* FS2 0x0015d320 */
/* Plays XSEQ `xseq` on a free BGM slot (command 0x70); the slot, -1 when
 * all 32 are taken. keep as rxSndSePlay. */
int   rxSndBgmPlay(int xseq, int volL, int volR, int keep);              /* FS2 0x0015d3b8 */
void  rxSndBgmStop(int bgm, int arg);                                    /* FS2 0x0015d4a0 */
void  rxSndBgmStopAll(int arg);                                          /* FS2 0x0015d580 */
void  rxSndBgmPause(int bgm, int pause, int arg);                        /* FS2 0x0015d5d0 */
u_int rxSndBgmGetStatus(int bgm);                                        /* FS2 0x0015d698 */
u_int rxSndBgmGetValue(int bgm);                                         /* FS2 0x0015d6d0 */
/* Queued commands 0x73 / 0x74 {slot, channel, 3, L, R}: raw volumes. */
void  rxSndBgmSetChannelVolume(int bgm, int channel, int volL, int volR);   /* FS2 0x0015d710 */
void  rxSndBgmSetChannelVolume74(int bgm, int channel, int volL, int volR); /* FS2 0x0015d7d0 */
void  rxSndBgmSetAttr(int bgm, int value);                               /* FS2 0x0015d890 */
void  rxSndBgmSetVolume(int bgm, int volL, int volR);                    /* FS2 0x0015d928 */
void  rxSndBgmSetPitch(int bgm, float pitch);                            /* FS2 0x0015da78 */
void  rxSndBgmSetParam5(int bgm, float value);                           /* FS2 0x0015db20 */
void  rxSndBgmSetParam6(int bgm, int value);                             /* FS2 0x0015dbc8 */
/* FS2 only (no caller): the ids of the active BGM slots into ids[] (may be
 * null); their count. The SE / PCM / ADPCM twins below. */
int   rxSndBgmGetActiveList(int *ids);                                   /* FS2 0x0015dc60 */
void  rxSndBgmInit(void);                                                /* FS2 0x0015dca8 */
void  rxSndBgmTerm(void);                                                /* FS2 0x0015dd00 */
void  rxSndBgmUpdate(const RxSndBgmStatus *status);                      /* FS2 0x0015dd08 */
short rxSndBgmGetMasterVolumeL(void);                                    /* FS2 0x0015de80 */
short rxSndBgmGetMasterVolumeR(void);                                    /* FS2 0x0015de90 */
void  rxSndSeSetMasterVolume(int volL, int volR);                        /* FS2 0x0015dea0 */
/* rxSndSePlay's parameters 0xb / 0xc for every voice it starts (-1 none). */
void  rxSndSeSetDefaultParamB(int value);                                /* FS2 0x0015df38 */
void  rxSndSeSetDefaultParamC(int value);                                /* FS2 0x0015df68 */
void  rxSndSeInit(void);                                                 /* FS2 0x0015ea88 */
void  rxSndSeTerm(void);                                                 /* FS2 0x0015eae8 */
void  rxSndSeUpdate(const RxSndSeStatus *status);                        /* FS2 0x0015eaf0 */
short rxSndSeGetMasterVolumeL(void);                                     /* FS2 0x0015ec60 */
short rxSndSeGetMasterVolumeR(void);                                     /* FS2 0x0015ec70 */
void  rxSndPcmSetMasterVolume(int volL, int volR);                       /* FS2 0x0015ec80 */
void  rxSndPcmSetVolume(int pcm, int volL, int volR);                    /* FS2 0x0015f958 */
int   rxSndPcmGetActiveList(int *ids);                                   /* FS2 0x0015fa80 */
void  rxSndPcmInit(void);                                                /* FS2 0x0015fac8 */
void  rxSndPcmTerm(void);                                                /* FS2 0x0015fb58 */
void  rxSndPcmUpdate(const RxSndPcmStatus *status);                      /* FS2 0x0015fb60 */
short rxSndPcmGetMasterVolumeL(void);                                    /* FS2 0x0015fd70 */
short rxSndPcmGetMasterVolumeR(void);                                    /* FS2 0x0015fd80 */
void  rxSndAdpcmSetMasterVolume(int volL, int volR);                     /* FS2 0x0015fd90 */
/* The bytes of the stream's IOP buffer that are free; raw == 0 keeps 0x80
 * per channel in reserve. */
int   rxSndAdpcmGetFreeSize(int stream, int raw);                        /* FS2 0x00160360 */
void  rxSndAdpcmInit(void);                                              /* FS2 0x00161868 */
void  rxSndAdpcmTerm(void);                                              /* FS2 0x00161900 */
void  rxSndAdpcmUpdate(const RxSndAdpcmStatus *status);                  /* FS2 0x00161908 */
short rxSndAdpcmGetMasterVolumeL(void);                                  /* FS2 0x00161b60 */
short rxSndAdpcmGetMasterVolumeR(void);                                  /* FS2 0x00161b70 */

/* ---- transport --------------------------------------------------------- */
int   rxSndCreateSema(int maxCount, int initCount);                      /* FS2 0x00161b80 */
/* Binds RPC `rpcNo`, waiting until the IOP serves it; 1 bound, 0 error. */
int   rxSndBindRpc(sceSifClientData *cd, u_int rpcNo);                   /* FS2 0x00161bd0 */
void  rxSndUnbindRpc(sceSifClientData *cd);                              /* FS2 0x00161cc8 */
void  rxSndRpcEndFunc(void);                                             /* FS2 0x00161ce8 */
void  rxSndCallRpc(sceSifClientData *cd, u_int fno, void *send, int sendSize,
                   void *reply, int replySize);                          /* FS2 0x00161d18 */
void  rxSndRpcInit(void);                                                /* FS2 0x00161de0 */
void  rxSndRpcTerm(void);                                                /* FS2 0x00161e18 */
/* Sends command `cmd` at once: `send` goes out, the reply's words 1.. come
 * back in `reply`; returns the reply's word 0. */
u_int rxSndRpcCommand(u_int cmd, void *reply, int replySize,
                      const void *send, int sendSize);                   /* FS2 0x00161e38 */
/* Queues command `cmd` with `dataSize` bytes of data (write them through
 * rxSndQueueGetData) and `replySize` bytes of reply (rxSndQueueGetReply,
 * valid after the next flush). */
void  rxSndQueueBegin(u_int cmd, int replySize, int dataSize);           /* FS2 0x00161fc8 */
void *rxSndQueueGetReply(void);                                          /* FS2 0x00162060 */
void *rxSndQueueGetData(void);                                           /* FS2 0x00162078 */
void  rxSndQueueEnd(void);                                               /* FS2 0x00162090 */
void  rxSndQueueFlush(void);                                             /* FS2 0x001620d8 */

/* ---- the families' play / stream calls --------------------------------- */

/* XVAB banks: Load returns the bank's id (id -1 takes a free one), -1 on
 * failure; Check is 1 while the transfer runs (wait 1 waits for it). */
int   rxSndXvabLoad(int id, const void *xvab, u_int iopAddr, u_int spuAddr); /* FS2 0x0015c090 */
/* wait 0: 1 while the IOP still uses the bank; wait 1 waits for it. */
int   rxSndXvabFree(int id, int wait);                                   /* FS2 0x0015c480 */
int   rxSndXvabCheck(int id, int wait);                                  /* FS2 0x0015c5f8 */

/* SE voices of an XVAB. Status: bit 0 open, bit 2 ended; 0 when closed.
 * Play returns the voice, -1 when all 64 are taken; keep != 0 leaves an
 * ended voice open for its owner to stop. */
int   rxSndSePlay(int xvab, int prog, int note, int volL, int volR, int keep); /* FS2 0x0015df98 */
/* rxSndSePlay with command 0x81. */
int   rxSndSePlayEx(int xvab, int prog, int note, int volL, int volR, int keep); /* FS2 0x0015e0e0 */
void  rxSndSeStop(int voice, int arg);                                   /* FS2 0x0015e228 */
void  rxSndSeStopAll(int arg);                                           /* FS2 0x0015e308 */
u_int rxSndSeGetStatus(int voice);                                       /* FS2 0x0015e358 */
void  rxSndSePause(int voice, int pause, int arg);                       /* FS2 0x0015e390 */
/* Queued command 0x82 {voice, param, value}. */
void  rxSndSeSetParam(int voice, int param, int value);                  /* FS2 0x0015e790 */
/* Parameters 6..9, 0..0x80. */
void  rxSndSeSetParam6(int voice, int value);                            /* FS2 0x0015e830 */
void  rxSndSeSetParam7(int voice, int value);                            /* FS2 0x0015e870 */
void  rxSndSeSetParam8(int voice, int value);                            /* FS2 0x0015e8b0 */
void  rxSndSeSetParam9(int voice, int value);                            /* FS2 0x0015e8f0 */
/* Parameter 10: pan -1..1 as 0..255. */
void  rxSndSeSetPan(int voice, float pan);                               /* FS2 0x0015e930 */
void  rxSndSeSetParamB(int voice, int value);                            /* FS2 0x0015e990 */
void  rxSndSeSetParamC(int voice, int value);                            /* FS2 0x0015e9e8 */
int   rxSndSeGetActiveList(int *ids);                                    /* FS2 0x0015ea40 */
/* IOP command 0x82, parameter 2 (pan or an effect level: not known). */
void  rxSndSeSetAttr(int voice, int value);                              /* FS2 0x0015e458 */
void  rxSndSeSetVolume(int voice, int volL, int volR);                   /* FS2 0x0015e4f0 */
void  rxSndSeSetPitch(int voice, float pitch);                           /* FS2 0x0015e640 */
void  rxSndSeSetParam5(int voice, float value);                          /* FS2 0x0015e6e8 */

/* ADPCM streams fed from memory. Status: bit 0 open, bit 2 ended, bit 3
 * paused. GetFree: the bytes Write can take now. */
/* Opens a stream: mode (RxSndAdpcm::m_mode), sample rate, IOP ring and SPU
 * buffer per channel (address 0: allocated here; sizes in KB). Returns the
 * stream, -1 on failure. */
int   rxSndAdpcmOpen(int mode, int rate, u_int iopAddr, int iopKB, u_int spuAddr, int spuKB); /* FS2 0x0015fe88 */
int   rxSndAdpcmOpenEx(int mode, int rate, u_int iopAddr, int iopKB, u_int spuAddr, int spuKB,
                       int arg);                                         /* FS2 0x0015fea8 */
int   rxSndAdpcmOpenBytes(int mode, int rate, u_int iopAddr, int iopSize, u_int spuAddr,
                          int spuSize, int arg);                         /* FS2 0x0015fec8 */
void  rxSndAdpcmClose(int stream);                                       /* FS2 0x001601e0 */
u_int rxSndAdpcmGetStatus(int stream);                                   /* FS2 0x00160318 */
int   rxSndAdpcmGetFree(int stream);                                     /* FS2 0x00160400 */
/* Feeds `size` bytes of ADPCM (mode 1: the second channel's from dataR);
 * takes what fits in the IOP ring. Returns size. */
int   rxSndAdpcmWrite(int stream, const void *data, const void *dataR, int size); /* FS2 0x00160cd0 */
int   rxSndAdpcmWriteEx(int stream, const void *data, const void *dataR, int size,
                        int raw);                                        /* FS2 0x00160418 */
/* No more data: the stream plays out what it has. */
void  rxSndAdpcmEnd(int stream);                                         /* FS2 0x00160ce8 */
void  rxSndAdpcmPause(int stream, int pause);                            /* FS2 0x001610b8 */
/* IOP command 0xa2, parameter 2 (the stream twin of rxSndSeSetAttr). */
void  rxSndAdpcmSetAttr(int stream, int value);                          /* FS2 0x00161158 */
void  rxSndAdpcmSetVolume(int stream, int volL, int volR);               /* FS2 0x001611f8 */
void  rxSndAdpcmSetPitch(int stream, float pitch);                       /* FS2 0x001614d0 */
/* Queued command 0xa2 parameters 6 (0..2) and 7 (0..0x7f). */
void  rxSndAdpcmSetParam6(int stream, int value);                        /* FS2 0x00161630 */
void  rxSndAdpcmSetParam7(int stream, int value);                        /* FS2 0x001616f8 */
/* rxSndAdpcmOpenBytes's defaults for parameters 6 (0..2) and 7 (0..0x7f). */
void  rxSndAdpcmSetDefaultParam6(int value);                             /* FS2 0x0015fe28 */
void  rxSndAdpcmSetDefaultParam7(int value);                             /* FS2 0x0015fe58 */
void  rxSndAdpcmCloseAll(void);                                          /* FS2 0x001602d8 */
/* Calls func(stream, arg) once the play position passes `offset` bytes
 * (all channels) after what has been written so far. 0 when no node is free. */
int   rxSndAdpcmAddCallback(int stream, int offset, RxSndPosFunc func, int arg); /* FS2 0x00160ed0 */
int   rxSndAdpcmSetUserData(int stream, int data);                       /* FS2 0x00161028 */
int   rxSndAdpcmGetUserData(int stream);                                 /* FS2 0x00161070 */
/* Command 0xa4 parameter 3 for one channel, without the master volumes. */
void  rxSndAdpcmSetChannelVolume(int stream, int channel, int volL, int volR); /* FS2 0x00161398 */
/* Command 0xa4 parameter 5 (not known) for one channel. */
void  rxSndAdpcmSetChannelParam5(int stream, int channel, int value);    /* FS2 0x00161580 */
/* FS2: the play position times the channel count (RoR: the position). */
u_long rxSndAdpcmGetPosition(int stream);                                /* FS2 0x001617c0 */
int   rxSndAdpcmGetActiveList(int *ids);                                 /* FS2 0x00161820 */

/* PCM streams fed from memory, the block-ring twin of the ADPCM streams
 * (RoR's sound driver pairs them in one slot table). Status: bit 0
 * open, bit 2 ended, bit 3 paused. */
/* Queued command 0x31 {0, 0, 0xffc when on, else 0xfcc}: mix bits (not known). */
void  rxSndSetSysMix(int on);                                            /* FS2 0x0015ed18 */
/* Queued command 0x31 {0, 2, L, R}: 0x7fff both, 0 both when muted. */
void  rxSndSetSysMute(int mute);                                         /* FS2 0x0015ed70 */
/* Queued command 0xb4 {value} (not known). */
void  rxSndPcmSetParamB4(int value);                                     /* FS2 0x0015ede0 */
/* Opens a stream of `blocks` blocks (RxSndPcm); iopAddr 0: allocated here.
 * Returns the stream, -1 on failure. */
int   rxSndPcmOpen(int mode, u_int iopAddr, int blocks);                 /* FS2 0x0015ee20 */
void  rxSndPcmClose(int stream);                                         /* FS2 0x0015f010 */
void  rxSndPcmCloseAll(void);                                            /* FS2 0x0015f108 */
u_int rxSndPcmGetStatus(int stream);                                     /* FS2 0x0015f148 */
/* The bytes rxSndPcmWrite can take now. */
int   rxSndPcmGetFree(int stream);                                       /* FS2 0x0015f190 */
/* Feeds up to `size` bytes (mode 1: half from data, half from dataR);
 * returns what was taken. */
int   rxSndPcmWrite(int stream, const void *data, const void *dataR, int size); /* FS2 0x0015f1f0 */
/* Pads the last block and tells the IOP no more data comes. */
void  rxSndPcmEnd(int stream);                                           /* FS2 0x0015f4d8 */
/* Calls func(stream, arg) once the play position passes `offset` bytes
 * after what has been written so far. 0 when no node is free. */
int   rxSndPcmAddCallback(int stream, int offset, RxSndPosFunc func, int arg); /* FS2 0x0015f630 */
int   rxSndPcmSetUserData(int stream, int data);                         /* FS2 0x0015f788 */
int   rxSndPcmGetUserData(int stream);                                   /* FS2 0x0015f7d0 */
/* Command 0xb2 parameter 2 (the stream twin of rxSndSeSetAttr). */
void  rxSndPcmSetAttr(int stream, int value);                            /* FS2 0x0015f818 */
void  rxSndPcmPause(int stream, int pause);                              /* FS2 0x0015f8b8 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSND_H */
