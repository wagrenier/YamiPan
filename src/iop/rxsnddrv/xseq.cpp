/* ==========================================================================
 *  xseq.cpp  (RXSNDDRV.IRX 0x2e00..0x35c0: XSEQ sequences)
 *
 *  Command 0x60 {id, iop} opens a sequence the EE copied to IOP memory
 *  (rx/rxsnd.c rxSndXseqLoad), command 0x61 {id} closes it (1 while a BGM
 *  still plays it, so the EE waits), command 0x63 {id, index, xvab} picks the
 *  XVAB bank (rxSndXseqSetParam).  0x20 slots of 0x20 bytes; a slot keeps
 *  the file, its HEAD (or MIDI) chunk, eight bank numbers the BGM sequencer
 *  looks notes up in (RxSndDrvXseqGetBank: a track's byte 4 indexes them)
 *  and counts the BGMs that play it.
 *
 *  The file is a chunk file like XVAB (xvab.cpp), magic "XSEQ", but the
 *  driver links its own copy of the chunk finder for it (0x34a8..0x35c0;
 *  its version < 0x200 walk does not stop at a zero tag).  The HEAD chunk:
 *    +0 u8 track count, +1 u8 "relocated", +2 s16 division, +4 int offset[count] to the
 *    tracks' TRCK records, 8 bytes each whose second word is the offset of
 *    the track's event data.
 *  Version < 0x200 stores those offsets from the file start: opening rebases
 *  them on the HEAD chunk, closing undoes it; version 0x200 files are
 *  HEAD-relative already.  No uncompressed XSEQ file was found on the disc
 *  (DATA/SEQ holds XVAB banks), so the format is the driver's reading only.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "intrman.h"

#include <stdio.h>
#include <string.h>

#define RXSNDDRV_XSEQ_MAX 0x20

enum
{
    RXSNDDRV_TAG_XSEQ = 0x51455358,     /* "XSEQ" */
    RXSNDDRV_TAG_HEAD = 0x44414548,     /* "HEAD" */
    RXSNDDRV_TAG_MIDI = 0x4944494d,     /* "MIDI" */
    RXSNDDRV_TAG_TRCK = 0x4b435254,     /* "TRCK" */
};

/* The HEAD chunk. */
struct RxSndDrvXseqHead
{
    u_char  m_tracks;                   // +0x00
    u_char  m_relocated;                // +0x01
    short   m_division;                 // +0x02: ticks a quarter note (bgm.cpp)
    int     m_offset[1];                // +0x04: [m_tracks], into TRCK, 0 none
};
static_assert(offsetof(RxSndDrvXseqHead, m_relocated) == 0x1, "RxSndDrvXseqHead::m_relocated");
static_assert(offsetof(RxSndDrvXseqHead, m_offset) == 0x4, "RxSndDrvXseqHead::m_offset");

static RxSndDrvXseq s_xseq[RXSNDDRV_XSEQ_MAX]; // RoR IOP 0x0001a680

/* Takes slot `id` (negative: the first free one) for the file at IOP address
 * `iop`; the slot, or -1. */
// IOP 0x00003000 (RoR 0x00002e00, word for word)
int RxSndDrvXseqOpen(u_int id, u_int iop)
{
    int           no   = (int)id;
    u_char       *file = (u_char *)YamiPan_IopPointer(iop);
    RxSndDrvXseq *slot;

    if (no < 0)
    {
        for (no = 0; no < RXSNDDRV_XSEQ_MAX; no++)
        {
            if (s_xseq[no].m_file == 0)
                break;
        }
        if (no == RXSNDDRV_XSEQ_MAX)
        {
            if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
                printf("snddrv : can't allocate XSEQ slot\n");
            return -1;
        }
    }
    else if (no >= RXSNDDRV_XSEQ_MAX)
    {
        /* PC: the IOP indexes past its slots here (the EE allows ids to 0xff). */
        return -1;
    }
    else if (s_xseq[no].m_file != 0)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
            printf("snddrv : specified XSEQ ID '%d' is already used\n", no);
        return -1;
    }
    slot = &s_xseq[no];

    if (!RxSndDrvXseqAttach(file))
        return -1;

    slot->m_head = YamiPan_IopAddress(RxSndDrvXseqChunkFind(file, RXSNDDRV_TAG_HEAD));
    if (slot->m_head == 0)
    {
        slot->m_head = YamiPan_IopAddress(RxSndDrvXseqChunkFind(file, RXSNDDRV_TAG_MIDI));
        if (slot->m_head == 0)
        {
            if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
                printf("snddrv : not found 'MIDI' or 'HEAD' chunk\n");
            return -1;
        }
    }
    slot->m_file  = iop;
    slot->m_users = 0;
    for (int i = 7; i >= 0; i--)
        slot->m_bank[i] = -1;

    if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO)
    {
        int size = ((const int *)file)[2];
        printf("snddrv : open XSEQ '%d'\n         data - IOP : %08X - %08X ( %d )\n",
               no, slot->m_file, slot->m_file + size, size);
    }
    return no;
}

/* Rebases a version < 0x200 file's track offsets: `dir` 1 from the file
 * start onto the HEAD chunk (state `open` 1), -1 back (`open` 0).  0 when the
 * file is no XSEQ or has no HEAD chunk. */
// IOP 0x000031a0 (RoR 0x00002fa0, word for word)
int RxSndDrvXseqRelocate(u_char *file, int dir, int open)
{
    const u_int *hdr = (const u_int *)file;

    if (hdr[0] != RXSNDDRV_TAG_XSEQ)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
            printf("snddrv : invalide XSEQ data\n");
        return 0;
    }
    if (hdr[1] > 0x200)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
            printf("snddrv : unsupported XSEQ version '%X'\n", hdr[1]);
        return 1;
    }
    if (hdr[1] >= 0x200)
        return 1;

    RxSndDrvXseqHead *head = (RxSndDrvXseqHead *)RxSndDrvXseqChunkFind(file, RXSNDDRV_TAG_HEAD);
    if (head == NULL)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
            printf("snddrv : not found 'MIDI' or 'HEAD' chunk\n");
        return 0;
    }
    if (open == 1)
    {
        if (head->m_relocated == 1)
            return 1;
    }
    else if (head->m_relocated == 0)
    {
        return 1;
    }
    head->m_relocated = (u_char)open;

    head = (RxSndDrvXseqHead *)RxSndDrvXseqChunkFind(file, RXSNDDRV_TAG_HEAD);
    if (head != NULL)
    {
        int delta = (int)(file - (u_char *)head) * dir;
        int used  = 0;

        for (int i = 0; i < head->m_tracks; i++)
        {
            if (head->m_offset[i] != 0)
            {
                head->m_offset[i] += delta;
                used++;
            }
        }

        int *trck = (int *)RxSndDrvXseqChunkFind(file, RXSNDDRV_TAG_TRCK);
        for (int i = 0; i < used; i++)
            trck[i * 2 + 1] += delta;
    }
    return 1;
}

// IOP 0x00003370 (RoR 0x00003170, word for word)
void RxSndDrvXseqInit(void)
{
    memset(s_xseq, 0, sizeof(s_xseq));
}

/* Unreferenced in the driver. */
// IOP 0x0000339c (RoR 0x0000319c, word for word)
void RxSndDrvXseqCloseAll(void)
{
    for (int i = 0; i < RXSNDDRV_XSEQ_MAX; i++)
        RxSndDrvXseqClose(i);
}

/* Frees slot `id` unless a BGM still plays it: 0 closed, 1 not. */
// IOP 0x000033d4 (RoR 0x000031d4, word for word)
int RxSndDrvXseqClose(u_int id)
{
    int intr;
    int ret = 1;

    /* PC: as RxSndDrvXseqOpen, the IOP does not range-check. */
    if (id >= RXSNDDRV_XSEQ_MAX)
        return 1;

    RxSndDrvXseq *slot = &s_xseq[id];

    CpuSuspendIntr(&intr);
    if (slot->m_file == 0)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO)
            printf("snddrv : specified XSEQ '%d' is already closed\n", (int)id);
    }
    else if (slot->m_users <= 0)
    {
        ret = 0;
        RxSndDrvXseqDetach((u_char *)YamiPan_IopPointer(slot->m_file));
        slot->m_file  = 0;
        slot->m_state = -1;
        /* The IOP copies m_head unaligned from IOP address 0 (lwl / lwr off
           an absolute 0); PC: cleared. */
        slot->m_head  = 0;
    }
    CpuResumeIntr(intr);

    if (ret == 0 && (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO))
        printf("snddrv : closed XSEQ '%d'\n", (int)id);
    return ret;
}

/* Slot `id`'s HEAD chunk field (RxSndDrvXseq::m_head on), NULL when it is
 * not open. */
// IOP 0x000034d8 (RoR 0x000032d8, word for word)
u_int *RxSndDrvXseqGetHead(u_int id)
{
    if (id < RXSNDDRV_XSEQ_MAX && s_xseq[id].m_file != 0)
        return &s_xseq[id].m_head;
    return NULL;
}

/* Command 0x63: bank `index` (0..7) of slot `id` is XVAB `xvab`. */
// IOP 0x00003510 (RoR 0x00003310, word for word)
void RxSndDrvXseqSetBank(u_int id, u_int index, u_int xvab)
{
    /* PC: the IOP does not range-check the slot. */
    if (id >= RXSNDDRV_XSEQ_MAX)
        return;
    if (index < 8 && s_xseq[id].m_file != 0)
        s_xseq[id].m_bank[index] = (short)xvab;
}

/* Bank `index` of slot `id`: its XVAB, -1 unset or not open. */
// IOP 0x00003548 (RoR 0x00003348, word for word)
int RxSndDrvXseqGetBank(int id, u_int index)
{
    if (index < 8 && s_xseq[id].m_file != 0)
        return s_xseq[id].m_bank[index];
    return -1;
}

/* Counts one more BGM playing slot `id`; whether it is open. */
// IOP 0x0000358c (RoR 0x0000338c, word for word)
int RxSndDrvXseqLock(int id)
{
    int intr;

    CpuSuspendIntr(&intr);
    u_int file = s_xseq[id].m_file;
    if (file != 0)
        s_xseq[id].m_users++;
    CpuResumeIntr(intr);
    return file != 0;
}

/* One BGM less. */
// IOP 0x00003600 (RoR 0x00003400, word for word)
void RxSndDrvXseqUnlock(int id)
{
    int intr;

    CpuSuspendIntr(&intr);
    if (s_xseq[id].m_file != 0)
        s_xseq[id].m_users--;
    CpuResumeIntr(intr);
}

// IOP 0x0000368c (RoR 0x00003460, word for word)
int RxSndDrvXseqAttach(u_char *file)
{
    return RxSndDrvXseqRelocate(file, 1, 1);
}

// IOP 0x000036b0 (RoR 0x00003484, word for word)
int RxSndDrvXseqDetach(u_char *file)
{
    return RxSndDrvXseqRelocate(file, -1, 0);
}

/* The data of chunk `tag`, NULL when there is none. */
// IOP 0x000036d4 (RoR 0x000034a8, 60% of it aligned: FS2 read, the same but for record offsets)
u_char *RxSndDrvXseqChunkFind(const void *file, u_int tag)
{
    if ((int)((const u_int *)file)[1] < 0x200)
        return RxSndDrvXseqChunkFindV1(file, tag);
    return RxSndDrvXseqChunkFindV2(file, tag);
}

/* Version < 0x200: {tag, size, data} from +0xc. */
// IOP 0x00003718 (RoR 0x00003518, word for word)
u_char *RxSndDrvXseqChunkFindV1(const void *file, u_int tag)
{
    u_char *base  = (u_char *)file;
    int     total = ((const int *)file)[2];

    for (int pos = 0xc; pos < total;)
    {
        u_int *chunk = (u_int *)(base + pos);

        if (chunk[0] == tag)
            return (u_char *)(chunk + 2);
        pos += 8 + (int)chunk[1];
    }
    return NULL;
}

/* Version 0x200: {tag, ?, size, ?, data} from +0x10. */
// IOP 0x00003768 (RoR 0x00003568, word for word)
u_char *RxSndDrvXseqChunkFindV2(const void *file, u_int tag)
{
    u_char *base  = (u_char *)file;
    int     total = ((const int *)file)[2];

    if (total <= 0)
        return NULL;
    for (int pos = 0x10;;)
    {
        u_int *chunk = (u_int *)(base + pos);

        if (chunk[0] == tag)
            return (u_char *)(chunk + 4);
        int end = pos + (int)chunk[2];
        if (end >= total)
            return NULL;
        pos = end + 0x10;
    }
}

/* Command 0x62 (FS2 only): slot `id`'s user count (the BGMs playing it), 0
 * when it is free. */
// IOP 0x00003660 (FS2 only, read 2026-11-09)
int RxSndDrvXseqGetUsers(u_int id)
{
    if (s_xseq[id].m_file == 0)
        return 0;
    return s_xseq[id].m_users;
}
