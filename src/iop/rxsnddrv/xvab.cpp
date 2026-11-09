/* ==========================================================================
 *  xvab.cpp  (RXSNDDRV.IRX 0x2330..0x2e00: XVAB sound banks)
 *
 *  Command 0x50 {id, iop, spu} opens a bank the EE copied to IOP memory
 *  (rx/rxsnd.c rxSndXvabLoad; its samples go to SPU address `spu` by
 *  command 0x40), command 0x51 {id} closes it.  0x80 slots of 0x20 bytes;
 *  a slot keeps the file, its five tables and the sample base, and counts the
 *  notes / SEs that use it so a bank in use is not closed under them.
 *
 *  The file is a chunk file, as on the EE (rxSndChunkFind):
 *    +0 "XVAB", +4 version, +8 size (version 0x200: of the chunks after the
 *    0x10-byte header; before: of the whole header part).
 *    version < 0x200: chunks {tag, size, data} from +0xc; a HEAD chunk holds
 *      byte 0 "relocated", byte 1 "samples follow the file", +4 the sample
 *      size and +8..+0x18 the file offsets of the five tables.
 *    version 0x200: chunks {tag, ?, size, ?, data} from +0x10.
 *  The five tables are TGIF (programs: NOTE looks programs up there), SEIF
 *  (SE groups), VELT, PANT and LFOT (velocity, pan and LFO curves, by their
 *  tags).  TGIF and SEIF are {u16 count, u16, int offset[count]}.  TGIF's
 *  entries lead to 8-byte TGPG records {?, offset into TGSP}; SEIF's to SEGP
 *  lists of the same shape, whose entries lead to 8-byte SERQ records (the
 *  SEs: RxSndDrvXvabSe) whose offsets lead into SESQ.  Files in the game
 *  (DATA/SEQ/G000.GRB, the area banks *.ARA) are version 0x101.
 *  Version < 0x200 stores those offsets from the file start: opening rebases
 *  them on their own table (and on SEIF for SEGP and SERQ, on TGIF for TGPG),
 *  closing undoes it.  Version 0x200 files are table-relative already.
 *
 *  Chunk tags are the four characters read as a little-endian word.
 * ======================================================================== */

#include "iop/rxsnddrv/rxsnddrv.h"

#include "intrman.h"

#include <stdio.h>
#include <string.h>

#define RXSNDDRV_XVAB_MAX 0x80

enum
{
    RXSNDDRV_TAG_XVAB = 0x42415658,     /* "XVAB" */
    RXSNDDRV_TAG_HEAD = 0x44414548,     /* "HEAD" */
    RXSNDDRV_TAG_TGIF = 0x46494754,     /* "TGIF" */
    RXSNDDRV_TAG_TGPG = 0x47504754,     /* "TGPG" */
    RXSNDDRV_TAG_SEIF = 0x46494553,     /* "SEIF" */
    RXSNDDRV_TAG_SEGP = 0x50474553,     /* "SEGP" */
    RXSNDDRV_TAG_SERQ = 0x51524553,     /* "SERQ" */
    RXSNDDRV_TAG_VELT = 0x544c4556,     /* "VELT" */
    RXSNDDRV_TAG_PANT = 0x544e4150,     /* "PANT" */
    RXSNDDRV_TAG_LFOT = 0x544f464c,     /* "LFOT" */
};

/* An offset table: TGIF, SEIF, and each SEGP list. */
struct RxSndDrvXvabTable
{
    u_short m_count;                    // +0x00
    u_short m_pad;                      // +0x02
    int     m_offset[1];                // +0x04: [m_count], 0 none
};
static_assert(offsetof(RxSndDrvXvabTable, m_offset) == 0x4, "RxSndDrvXvabTable::m_offset");

/* A SERQ record: one SE. */
struct RxSndDrvXvabSe
{
    u_char  m_a;                        // +0x00: negated by flag 0x1000
    u_char  m_b;                        // +0x01: negated by flag 0x2000
    u_short m_flags;                    // +0x02: 0x3ff ticks, 0xc000 mode
    int     m_data;                     // +0x04: into SESQ, from the SEIF table
};
static_assert(sizeof(RxSndDrvXvabSe) == 0x8, "RxSndDrvXvabSe");

static RxSndDrvXvab s_xvab[RXSNDDRV_XVAB_MAX]; // RoR IOP 0x00019680

/* Takes slot `id` (negative: the first free one) for the file at IOP address
 * `iop` whose samples are at SPU address `spu`; the slot, or -1. */
// IOP 0x00002450 (RoR 0x00002330, word for word)
int RxSndDrvXvabOpen(u_int id, u_int iop, u_int spu)
{
    int           no   = (int)id;
    u_char       *file = (u_char *)(uintptr_t)iop;
    RxSndDrvXvab *slot;

    if (no < 0)
    {
        for (no = 0; no < RXSNDDRV_XVAB_MAX; no++)
        {
            if (s_xvab[no].m_file == 0)
                break;
        }
        if (no == RXSNDDRV_XVAB_MAX)
        {
            if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
                printf("snddrv : can't allocate XVAB slot\n");
            return -1;
        }
    }
    else if (no >= RXSNDDRV_XVAB_MAX)
    {
        /* PC: the IOP indexes past its slots here (the EE allows ids to 0xff). */
        return -1;
    }
    else if (s_xvab[no].m_file != 0)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
            printf("snddrv : specified XVAB ID '%d' is already used\n", no);
        return -1;
    }
    slot = &s_xvab[no];

    if (!RxSndDrvXvabAttach(file))
        return -1;

    if (RxSndDrvChunkGetVersion(file) < 0x200)
    {
        const int *head = (const int *)RxSndDrvChunkFind(file, RXSNDDRV_TAG_HEAD);

        slot->m_tables.m_tgif = head[2] != 0 ? iop + head[2] : 0;
        slot->m_tables.m_seif = head[3] != 0 ? iop + head[3] : 0;
        slot->m_tables.m_velt = head[4] != 0 ? iop + head[4] : 0;
        slot->m_tables.m_pant = head[5] != 0 ? iop + head[5] : 0;
        slot->m_tables.m_lfot = head[6] != 0 ? iop + head[6] : 0;
    }
    else
    {
        slot->m_tables.m_tgif = (u_int)(uintptr_t)RxSndDrvChunkFind(file, RXSNDDRV_TAG_TGIF);
        slot->m_tables.m_seif = (u_int)(uintptr_t)RxSndDrvChunkFind(file, RXSNDDRV_TAG_SEIF);
        slot->m_tables.m_velt = (u_int)(uintptr_t)RxSndDrvChunkFind(file, RXSNDDRV_TAG_VELT);
        slot->m_tables.m_pant = (u_int)(uintptr_t)RxSndDrvChunkFind(file, RXSNDDRV_TAG_PANT);
        slot->m_tables.m_lfot = (u_int)(uintptr_t)RxSndDrvChunkFind(file, RXSNDDRV_TAG_LFOT);
    }
    slot->m_file          = iop;
    slot->m_tables.m_spu  = spu;
    slot->m_users         = 0;

    if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO)
    {
        int size = ((const int *)file)[2];
        printf("snddrv : open XVAB '%d'\n         header - IOP : $%08X - $%08X ( %d )\n"
               "         body   - SPU : $%08X\n",
               no, slot->m_file, slot->m_file + size, size, slot->m_tables.m_spu);
    }
    return no;
}

/* Adds `delta` to every non-zero offset of `table`; how many there were. */
static int RelocateTable(RxSndDrvXvabTable *table, int delta)
{
    int used = 0;

    for (int i = 0; i < table->m_count; i++)
    {
        if (table->m_offset[i] != 0)
        {
            table->m_offset[i] += delta;
            used++;
        }
    }
    return used;
}

/* Adds `delta` to the second word of `count` 8-byte records (TGPG, SERQ). */
static void RelocatePairs(u_char *pairs, int count, int delta)
{
    for (int i = 0; i < count; i++)
        ((int *)pairs)[i * 2 + 1] += delta;
}

/* Rebases a version < 0x200 file's table offsets: `dir` 1 from the file
 * start onto each table (state `open` 1), -1 back (`open` 0).  0 when the
 * file is no XVAB. */
// IOP 0x00002698 (RoR 0x00002578, word for word)
int RxSndDrvXvabRelocate(u_char *file, int dir, int open)
{
    const u_int *hdr     = (const u_int *)file;
    int          groups  = 0;
    int          lists   = 0;
    int          entries = 0;

    if (hdr[0] != RXSNDDRV_TAG_XVAB)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
            printf("snddrv : invalide XVAB data\n");
        return 0;
    }
    if (hdr[1] > 0x200)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_ERROR)
            printf("snddrv : unsupported XVAB version '%X'\n", hdr[1]);
        return 1;
    }
    if (hdr[1] == 0x200)
        return 1;

    u_char *head = RxSndDrvChunkFind(file, RXSNDDRV_TAG_HEAD);
    if (open == 1)
    {
        if (head[0] == 1)
            return 1;
    }
    else if (head[0] == 0)
    {
        return 1;
    }
    head[0] = (u_char)open;

    RxSndDrvXvabTable *tgif = (RxSndDrvXvabTable *)RxSndDrvChunkFind(file, RXSNDDRV_TAG_TGIF);
    if (tgif != NULL)
    {
        int delta = (int)(file - (u_char *)tgif) * dir;

        groups = RelocateTable(tgif, delta);
        RelocatePairs(RxSndDrvChunkFind(file, RXSNDDRV_TAG_TGPG), groups, delta);
    }

    RxSndDrvXvabTable *seif = (RxSndDrvXvabTable *)RxSndDrvChunkFind(file, RXSNDDRV_TAG_SEIF);
    if (seif != NULL)
    {
        int delta = (int)(file - (u_char *)seif) * dir;

        lists = RelocateTable(seif, delta);

        /* The SEGP lists follow each other: {count, pad, offset[count]}. */
        u_char *segp = RxSndDrvChunkFind(file, RXSNDDRV_TAG_SEGP);
        for (int i = 0; i < lists; i++)
        {
            RxSndDrvXvabTable *list = (RxSndDrvXvabTable *)segp;

            entries += RelocateTable(list, delta);
            segp    += 4 + list->m_count * 4;
        }
        RelocatePairs(RxSndDrvChunkFind(file, RXSNDDRV_TAG_SERQ), entries, delta);
    }
    return 1;
}

// IOP 0x00002974 (RoR 0x00002854, word for word)
void RxSndDrvXvabInit(void)
{
    memset(s_xvab, 0, sizeof(s_xvab));
}

/* Unreferenced in the driver. */
// IOP 0x000029a0 (RoR 0x00002880, word for word)
void RxSndDrvXvabCloseAll(void)
{
    for (int i = 0; i < RXSNDDRV_XVAB_MAX; i++)
        RxSndDrvXvabClose(i);
}

/* Frees slot `id` unless a note or SE still uses it: 0 closed, 1 not. */
// IOP 0x000029d8 (RoR 0x000028b8, word for word)
int RxSndDrvXvabClose(u_int id)
{
    int intr;
    int ret = 1;

    /* PC: as RxSndDrvXvabOpen, the IOP does not range-check. */
    if (id >= RXSNDDRV_XVAB_MAX)
        return 1;

    RxSndDrvXvab *slot = &s_xvab[id];

    CpuSuspendIntr(&intr);
    if (slot->m_file == 0)
    {
        if (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO)
            printf("snddrv : specified XVAB '%d' is already closed\n", (int)id);
    }
    else if (slot->m_users <= 0)
    {
        ret = 0;
        RxSndDrvXvabDetach((u_char *)(uintptr_t)slot->m_file);
        slot->m_file         = 0;
        slot->m_tables.m_spu = 0;
        memset(&slot->m_tables, 0, 0x14);
    }
    CpuResumeIntr(intr);

    if (ret == 0 && (RxSndDrvGetDebugFlags() & RXSNDDRV_DBG_INFO))
        printf("snddrv : closed XVAB '%d'\n", (int)id);
    return ret;
}

/* Slot `id`'s tables and sample base, NULL when it is not open. */
// IOP 0x00002ad0 (RoR 0x000029b0, word for word)
RxSndDrvXvabTables *RxSndDrvXvabGetTables(u_int id)
{
    if (id < RXSNDDRV_XVAB_MAX && s_xvab[id].m_file != 0)
        return &s_xvab[id].m_tables;
    return NULL;
}

/* Slot `id`'s SPU sample base, 0 when it is not open. */
// IOP 0x00002b08 (RoR 0x000029e8, word for word)
u_int RxSndDrvXvabGetSpu(int id)
{
    if (s_xvab[id].m_file != 0)
        return s_xvab[id].m_tables.m_spu;
    return 0;
}

/* Counts one more user of slot `id`; whether it is open. */
// IOP 0x00002b34 (RoR 0x00002a14, word for word)
int RxSndDrvXvabLock(int id)
{
    int intr;

    CpuSuspendIntr(&intr);
    u_int file = s_xvab[id].m_file;
    if (file != 0)
        s_xvab[id].m_users++;
    CpuResumeIntr(intr);
    return file != 0;
}

/* One user of slot `id` less. */
// IOP 0x00002ba8 (RoR 0x00002a88, word for word)
void RxSndDrvXvabUnlock(int id)
{
    int intr;

    CpuSuspendIntr(&intr);
    if (s_xvab[id].m_file != 0)
        s_xvab[id].m_users--;
    CpuResumeIntr(intr);
}

/* SE `index` of SEIF group `group` in slot `id`: its data (IOP address, 0
 * when there is none) and its parameters -- `ticks` the low 10 flag bits
 * (the SE tick puts 60 for 0), `a` / `b` its two bytes, negated by flags
 * 0x1000 / 0x2000, `mode` 0 for flags 0xc000, 1 for 0x4000, 2 for 0x8000,
 * else -1.  A found SE counts as a user of the slot. */
// IOP 0x00002c08 (RoR 0x00002ae8, word for word)
u_int RxSndDrvXvabFindSe(int id, int group, int index, int *ticks, int *a, int *b, int *mode)
{
    int   intr;
    u_int data = 0;

    CpuSuspendIntr(&intr);
    if (s_xvab[id].m_file != 0 && s_xvab[id].m_tables.m_seif != 0)
    {
        u_char                  *base = (u_char *)(uintptr_t)s_xvab[id].m_tables.m_seif;
        const RxSndDrvXvabTable *seif = (const RxSndDrvXvabTable *)base;

        if (group < seif->m_count && seif->m_offset[group] != 0)
        {
            const RxSndDrvXvabTable *list = (const RxSndDrvXvabTable *)(base + seif->m_offset[group]);

            if (index < list->m_count && list->m_offset[index] != 0)
            {
                const RxSndDrvXvabSe *se    = (const RxSndDrvXvabSe *)(base + list->m_offset[index]);
                u_short               flags = se->m_flags;

                data   = (u_int)(uintptr_t)(base + se->m_data);
                *ticks = flags & 0x3ff;
                *a     = se->m_a;
                if (flags & 0x1000)
                    *a = -(int)se->m_a;
                *b = se->m_b;
                if (flags & 0x2000)
                    *b = -(int)se->m_b;
                *mode = -1;
                if ((flags & 0xc000) == 0xc000)
                    *mode = 0;
                else if (flags & 0x4000)
                    *mode = 1;
                else if (flags & 0x8000)
                    *mode = 2;
                s_xvab[id].m_users++;
            }
        }
    }
    CpuResumeIntr(intr);
    return data;
}

// IOP 0x00002eac (RoR 0x00002c7c, word for word)
int RxSndDrvXvabAttach(u_char *file)
{
    return RxSndDrvXvabRelocate(file, 1, 1);
}

// IOP 0x00002ed0 (RoR 0x00002ca0, word for word)
int RxSndDrvXvabDetach(u_char *file)
{
    return RxSndDrvXvabRelocate(file, -1, 0);
}

// IOP 0x00002ef4 (RoR 0x00002cc4, word for word)
u_int RxSndDrvChunkGetVersion(const void *file)
{
    return ((const u_int *)file)[1];
}

/* The data of chunk `tag`, NULL when there is none. */
// IOP 0x00002f00 (RoR 0x00002cd0, word for word)
u_char *RxSndDrvChunkFind(const void *file, u_int tag)
{
    if ((int)((const u_int *)file)[1] < 0x200)
        return RxSndDrvChunkFindV1(file, tag);
    return RxSndDrvChunkFindV2(file, tag);
}

/* Command 0x52: slot `id`'s user count, 0 when it is free. RoR's (folded
 * after RxSndDrvChunkFind, unreferenced) read the test the other way round;
 * FS2's is the right way and reachable (read 2026-11-09). */
// IOP 0x00002e80 (RoR 0x00002d14, moved in FS2, 90% alike: FS2 read)
int RxSndDrvXvabGetUsers(int id)
{
    if (s_xvab[id].m_file == 0)
        return 0;
    return s_xvab[id].m_users;
}

/* Command 0x53 (FS2 only): whether SE `index` of group `group` is in slot
 * `id`, the lookup of RxSndDrvXvabFindSe without its answers. */
// IOP 0x00002d9c (FS2 only, read 2026-11-09)
int RxSndDrvXvabHasSe(int id, int group, int index)
{
    int   intr;
    u_int data = 0;

    CpuSuspendIntr(&intr);
    if (s_xvab[id].m_file != 0 && s_xvab[id].m_tables.m_seif != 0)
    {
        u_char                  *base = (u_char *)(uintptr_t)s_xvab[id].m_tables.m_seif;
        const RxSndDrvXvabTable *seif = (const RxSndDrvXvabTable *)base;

        if (group < seif->m_count && seif->m_offset[group] != 0)
        {
            const RxSndDrvXvabTable *list = (const RxSndDrvXvabTable *)(base + seif->m_offset[group]);

            if (index < list->m_count && list->m_offset[index] != 0)
                data = (u_int)(uintptr_t)(base + ((const RxSndDrvXvabSe *)(base + list->m_offset[index]))->m_data);
        }
    }
    CpuResumeIntr(intr);
    return data != 0;
}

/* Version < 0x200: {tag, size, data} from +0xc, ended by a zero tag. */
// IOP 0x00002f44 (RoR 0x00002d40, word for word)
u_char *RxSndDrvChunkFindV1(const void *file, u_int tag)
{
    u_char *base  = (u_char *)file;
    int     total = ((const int *)file)[2];

    for (int pos = 0xc; pos < total;)
    {
        u_int *chunk = (u_int *)(base + pos);

        if (chunk[0] == 0)
            return NULL;
        if (chunk[0] == tag)
            return (u_char *)(chunk + 2);
        pos += 8 + (int)chunk[1];
    }
    return NULL;
}

/* Version 0x200: {tag, ?, size, ?, data} from +0x10. */
// IOP 0x00002fa4 (RoR 0x00002da0, 95% of it aligned: FS2 differs, unread)
u_char *RxSndDrvChunkFindV2(const void *file, u_int tag)
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
