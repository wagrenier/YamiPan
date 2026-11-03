/* ==========================================================================
 *  rx/filecd.c
 *
 *  rx's cdrom0: device driver (0x1565c8..0x157db8): reads the ISO 9660 file
 *  system itself (primary volume descriptor, path table, directory records)
 *  through a 32-sector LRU cache. See filecd.h.
 *
 *  FS2 links Rule of Rose's prebuilt rx: this file is RosePan's (rxcmp.py:
 *  34 of 35 functions identical, jump targets and data masked). The one
 *  difference is rxFileCdFindDir, which in FS2 also looks in the root when
 *  the path table holds nothing else. Data: RoR's +0x198f0.
 *
 *  PC: the file system comes from sceCdRead as on the console: the host
 *  serves the data folder as a disc image (yamipan::io::Disc).
 * ======================================================================== */
#include "rx/filecd.h"

#include <stdint.h>
#include <string.h>

#include "rx/rxfile.h"
#include "rx/rxsys.h"
#include "rx/rxthread.h"
#include "sdk/eekernel.h"
#include "sdk/libcdvd.h"
#include "sdk/libgraph.h"

/* ---- data ------------------------------------------------------------- */

static const char *s_cdRoot = "cdrom0:";        /* data 0x2f5ab0 */
static const char *s_cdIsoPrefix = "\\";        /* data 0x2f5ab4 */
static const char *s_cdIsoSuffix = ";1";        /* data 0x2f5ab8 */
static int s_cdFlags;                           /* data 0x2f5abc: 1 tray was open, 2 reading */
static int s_cdReadPos;                         /* data 0x2f5ac0: byte position of the last read */
static int s_cdReadPos2;                        /* data 0x2f5ac4: never written */
static int s_cdTrayReq;                         /* data 0x2f5ac8: 1 open, 2 close requested */
static RxFileCdFindFunc s_cdFind = rxFileCdFind; /* data 0x2f5acc */
static int s_cdClockRead;                       /* data 0x2f5ad0: read the clock periodically */
static int s_cdClockTick;                       /* data 0x2f5ad4 */
static int s_cdClockCountdown;                  /* data 0x2f5ad8 */
static int s_cdClockInterval = 0x1e;            /* data 0x2f5adc: ticks between reads */
static sceCdCLOCK s_cdClock;                    /* data 0x2f5ae0 */
static int s_cdClockValid;                      /* data 0x2f5ae8 */
static int s_cdDual;                            /* data 0x2f5aec: dual-layer disc */
static u_int s_cdLayer1Start;                   /* data 0x2f5af0 */
static int s_cdLayer;                           /* data 0x2f5af4: layer the lookups use */
static int s_cdLayerMode;                       /* data 0x2f5af8: layer asked for */
static int s_cdLayerPending;                    /* data 0x2f5afc: s_cdLayerMode changed */
static int s_cdTrayTimer;                       /* data 0x2f5b00 */
static long long s_cdLastSector = -1;           /* data 0x2f5b08 (a doubleword): last read offset */

static RxFileCdCacheNode s_cdCacheNodes[32];    /* bss 0x4d5b40 */
static RxFileCdCacheNode s_cdCacheUsed;         /* bss 0x4d5d40: sentinel of the used list */
static RxFileCdCacheNode *s_cdCacheFree;        /* bss 0x4d5d50 */
static int s_cdDiscRead;                        /* bss 0x4d5d54: a lookup read the disc (tray polling) */
static int s_cdNeedSetup;                       /* bss 0x4d5d58: next read sets the media mode up */
static unsigned char s_cdCacheBuf[32][0x800];   /* bss 0x4d5d80 */
static unsigned char s_cdTailBuf[4 * 0x800];    /* bss 0x4e5d80: unaligned head / tail sectors */

// ──────────────────────────────────────────────────────────────────────
// Give the other rx threads a turn while the drive works.
// FS2 0x001565c8
// callcheck: EIntr, rxSysPrintf (the DI check, rxFileSpin in rx/rxfile.c: empty on the host)

void rxFileCdWait(void)
{
    rxThYield();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00156618

void rxFileCdSyncWait(void)
{
    while (sceCdSync(1) != 0)
        rxFileCdWait();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00156648

void rxFileCdInit(RxFileDriver *drv)
{
    drv->makePath  = rxFileCdMakePath;
    drv->setRoot   = rxFileCdSetRoot;
    drv->getStatus = rxFileCdGetStatus;
    drv->control   = rxFileCdControl;
    drv->getSize   = rxFileCdGetSize;
    drv->create    = rxFileCdCreate;
    drv->read      = rxFileCdRead;
    drv->write     = rxFileCdWrite;
    drv->getRoot   = rxFileCdGetRoot;
    rxFileCdSetRoot(0);
    rxFileCdInitCache();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001566d8

void rxFileCdTerm(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// The drive status once it answers.
// FS2 0x001566e0

int rxFileCdStatusWait(void)
{
    int status;
    while ((status = sceCdStatus()) < 0)
        rxFileCdWait();
    return status;
}

// ──────────────────────────────────────────────────────────────────────
// Open (0) or close (1) the tray.
// FS2 0x00156710

void rxFileCdTray(int req)
{
    u_int count[4];
    sceCdTrayReq(req, count);
}

// ──────────────────────────────────────────────────────────────────────
// The file thread's work before every request: once a lookup read the
// disc, poll the tray every 0x400 steps (open: drop the cache, flag it);
// serve tray requests; read the drive's clock every s_cdClockInterval
// ticks while asked to.
// FS2 0x00156730

void rxFileCdProcess(int step)
{
    if (s_cdDiscRead != 0)
    {
        s_cdTrayTimer += step;
        if (s_cdTrayTimer > 0x400)
        {
            if (rxFileCdStatusWait() == SCECdStatShellOpen)
            {
                rxFileCdFlushCache();
                s_cdFlags |= 1;
            }
            else
            {
                s_cdFlags &= ~1;
            }
            s_cdTrayTimer = 0;
        }
    }

    if ((s_cdTrayReq & 1) != 0)
    {
        DIntr();
        s_cdTrayReq &= ~1;
        EIntr();
        rxFileCdTray(0);
        rxFileCdSyncWait();
    }
    if ((s_cdTrayReq & 2) != 0)
    {
        DIntr();
        s_cdTrayReq &= ~2;
        EIntr();
        rxFileCdTray(1);
        rxFileCdSyncWait();
    }

    int tick = s_cdClockTick;
    if (s_cdClockRead != 0)
    {
        int now = rxThSync(-1);
        if (s_cdClockTick != now || s_cdClockValid == 0)
        {
            tick = now;
            s_cdClockCountdown--;
            sceCdCLOCK clock;
            if (s_cdClockCountdown < 0 && sceCdReadClock(&clock) != 0)
            {
                DIntr();
                s_cdClock = clock;
                EIntr();
                s_cdClockValid = 1;
                s_cdClockCountdown = s_cdClockInterval;
            }
        }
    }
    s_cdClockTick = tick;
}

// ──────────────────────────────────────────────────────────────────────
// "cdrom0:" and the ISO 9660 name.
// FS2 0x001569f8

void rxFileCdMakePath(char *out, const char *name)
{
    strcpy(out, s_cdRoot);
    rxFileCdIsoName(out + strlen(out), name);
}

// ──────────────────────────────────────────────────────────────────────
// Cmd 0: ask the file thread to open (arg != 0) or close the tray.
// FS2 0x00156a40

int rxFileCdControl(int cmd, int arg)
{
    if (cmd == 0)
    {
        DIntr();
        if (arg == 0)
            s_cdTrayReq = (s_cdTrayReq & ~1) | 2;
        else
            s_cdTrayReq = (s_cdTrayReq & ~2) | 1;
        EIntr();
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Status 0: the tray was found open; 1: reading; 2: byte position of the
// last read.
// FS2 0x00156b08

int rxFileCdGetStatus(int what)
{
    if (what == 1)
        return (s_cdFlags >> 1) & 1;
    if (what > 1)
    {
        if (what == 2)
            return s_cdReadPos + s_cdReadPos2;
        return 0;
    }
    if (what != 0)
        return 0;
    return s_cdFlags & 1;
}

// ──────────────────────────────────────────────────────────────────────
// A file's size; -2 when it is not on the disc, -1 on errors.
// FS2 0x00156b88

int rxFileCdGetSize(const char *name)
{
    int lsn;
    int size;
    int r = s_cdFind(name, &lsn, &size);
    if (r == -2)
    {
        rxFileCdFlushCache();
        return -1;
    }
    if (r < -1)
        return r == -3 ? -2 : -1;
    if (r == 1)
        return size;
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// The disc is read-only.
// FS2 0x00156c00

int rxFileCdCreate(const char *name, int truncate)
{
    (void)name;
    (void)truncate;
    return -2;
}

// ──────────────────────────────────────────────────────────────────────
// Read size bytes at offset of a file; size, -2 when it is not on the
// disc, -1 on errors.
// FS2 0x00156c08

int rxFileCdRead(const char *name, void *buf, int offset, int size)
{
    int lsn;
    int fileSize;
    int r = s_cdFind(name, &lsn, &fileSize);
    if (r == -2)
    {
        rxFileCdFlushCache();
        return -1;
    }
    if (r < -1)
        return r == -3 ? -2 : -1;
    if (r != 1)
        return -1;
    r = rxFileCdReadAt(buf, (long long)(unsigned int)lsn * 0x800 + (unsigned int)offset, size);
    if (r < 0 || r != 1)
        return -1;
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// The disc is read-only.
// FS2 0x00156cf0

int rxFileCdWrite(const char *name, int offset, const void *buf, int size)
{
    (void)name;
    (void)offset;
    (void)buf;
    (void)size;
    return -2;
}

// ──────────────────────────────────────────────────────────────────────
// ISO 9660 spelling: "\" in front unless absolute, "\" separators, upper
// case, ";1".
// FS2 0x00156cf8

void rxFileCdIsoName(char *out, const char *name)
{
    out[0] = '\0';
    if (name[0] != '/')
        strcpy(out, s_cdIsoPrefix);
    strcpy(out + strlen(out), name);
    for (char *p = out; *p != '\0'; p++)
    {
        if (*p == '/')
            *p = '\\';
        else if ((unsigned char)(*p - 'a') < 26)
            *p = (char)(*p - 0x20);
    }
    strcpy(out + strlen(out), s_cdIsoSuffix);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00156dd0

int rxFileCdMapSector(int lsn)
{
    if (s_cdDual != 0 && s_cdLayer == 1)
        lsn += s_cdLayer1Start;
    return lsn;
}

// ──────────────────────────────────────────────────────────────────────
// Read size bytes at a disc offset: the whole sectors straight into the
// buffer from its first 64-byte boundary, moved down to where they belong,
// then the sectors left through s_cdTailBuf. Returns the last sector
// read's result.
// FS2 0x00156e08

int rxFileCdReadBytes(void *buf, long long offset, int size)
{
    unsigned char *dst = (unsigned char *)buf;
    unsigned int head = (0u - ((unsigned int)(size_t)dst & 0x3f)) & 0x3f;
    unsigned char *aligned = dst + head;
    int skip = (int)(offset & 0x7ff);
    long long base = offset & ~0x7ffLL;
    int body = size - (int)head;
    int bulk = body < 1 ? 0 : (body & ~0x7ff);
    long long end = base + bulk;
    int done = 0;
    int from = skip;
    int r = 1;

    s_cdFlags |= 2;
    if (bulk > 0)
    {
        r = rxFileCdReadSectors(aligned, (int)(base / 0x800), bulk >> 11);
        if (r < 1)
            goto out;
        if (head != 0 || skip != 0)
        {
            memmove(dst, aligned + skip, (size_t)(bulk - skip));
            done = bulk - skip;
        }
        else
        {
            done = bulk;
        }
        from = 0;
    }
    if (done < size)
    {
        long long rest = ((size + offset + 0x7ff) & ~0x7ffLL) - end;
        r = rxFileCdReadSectors(s_cdTailBuf, (int)(end / 0x800), (int)(rest / 0x800));
        if (r > 0)
            memcpy(dst + done, s_cdTailBuf + from, (size_t)(size - done));
    }
out:
    s_cdFlags &= ~2;
    return r;
}

// ──────────────────────────────────────────────────────────────────────
// A read at the same offset as the last one first reads the sector 32
// before it (the drive then really seeks: how a failed read is retried).
// FS2 0x00157090

int rxFileCdReadAt(void *buf, long long offset, int size)
{
    int sector = (int)(offset / 0x800) - 0x20;
    if (s_cdLastSector == offset)
    {
        int r = rxFileCdReadSectors(s_cdTailBuf, sector < 0 ? 0 : sector, 1);
        if (r < 0)
            return r;
    }
    s_cdLastSector = offset;
    return rxFileCdReadBytes(buf, offset, size);
}

// ──────────────────────────────────────────────────────────────────────
// Read count sectors. The first read after a cache reset waits for the
// disc and sets the media mode and the dual-layer info. 1 read, -2 the tray
// opened or the disc went (cache dropped), -1 a read error.
// FS2 0x00157150

int rxFileCdReadSectors(void *buf, int lsn, int count)
{
    sceCdRMode mode;
    int err;

    mode.trycount = 0;
    mode.spindlctrl = 0;
    mode.datapattern = 0;
    FlushCache(0);
    if (s_cdNeedSetup != 0)
    {
        while (sceCdDiskReady(SCECdNonblock) != SCECdComplete)
        {
            if (rxFileCdStatusWait() == SCECdStatShellOpen)
                goto changed;
            rxFileCdWait();
        }
        int type;
        while ((type = sceCdGetDiskType()) == SCECdDETCT)
            rxFileCdWait();
        rxSysPrintf("rxfile : disk type = %d\n", type);
        if (type == SCECdPS2DVD || type == SCECdDVDV)
            sceCdMmode(SCECdDVD);
        else
            sceCdMmode(SCECdCD);
        s_cdDual = 0;
        s_cdLayer1Start = 0;
        if (sceCdReadDvdDualInfo(&s_cdDual, &s_cdLayer1Start) == 0)
        {
            err = sceCdGetError();
            if (err >= 0x13)
            {
                if (err == SCECdErTRMOPN)
                    goto changed;
                goto error;
            }
            if (err > 0x10)
                goto changed;
            if (err != SCECdErNO)
                goto error;
        }
        if (s_cdDual != 0)
            rxSysPrintf("rxfile : dual mode\n");
        s_cdNeedSetup = 0;
    }

    s_cdReadPos = lsn << 11;
    while (sceCdRead(rxFileCdMapSector(lsn), count, buf, &mode) == 0)
    {
        if (rxFileCdStatusWait() == SCECdStatShellOpen)
            goto changed;
        rxFileCdWait();
    }
    rxFileCdSyncWait();
    FlushCache(0);
    err = sceCdGetError();
    if (err < 0x13)
    {
        if (err > 0x10)             // SCECdErOPENS, SCECdErNODISC
            goto changed;
        if (err == SCECdErNO)
            return 1;
        goto error;
    }
    if (err == SCECdErTRMOPN)
        goto changed;
error:
    rxSysPrintf("*** read error retry\n");
    return -1;
changed:
    rxFileCdFlushCache();
    return -2;
}

// ──────────────────────────────────────────────────────────────────────
// Put the 32 sector buffers on the free list, empty the used list.
// FS2 0x00157390

void rxFileCdInitCache(void)
{
    s_cdCacheFree = 0;
    for (int i = 0; i < 32; i++)
    {
        RxFileCdCacheNode *node = &s_cdCacheNodes[i];
        node->buf = s_cdCacheBuf[i];
        node->next = s_cdCacheFree;
        s_cdCacheFree = node;
    }
    s_cdNeedSetup = 1;
    s_cdCacheUsed.next = &s_cdCacheUsed;
    s_cdCacheUsed.prev = &s_cdCacheUsed;
    s_cdDiscRead = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Return every used sector buffer to the free list.
// FS2 0x001573f0

void rxFileCdFlushCache(void)
{
    s_cdNeedSetup = 1;
    s_cdDiscRead = 0;
    RxFileCdCacheNode *node;
    while ((node = s_cdCacheUsed.next) != &s_cdCacheUsed)
    {
        s_cdCacheUsed.next = node->next;
        node->next = s_cdCacheFree;
        s_cdCacheFree = node;
    }
    s_cdCacheUsed.next = &s_cdCacheUsed;
    s_cdLastSector = -1;
    s_cdCacheUsed.prev = &s_cdCacheUsed;
}

// ──────────────────────────────────────────────────────────────────────
// The default lookup: the path table's place and size from the primary
// volume descriptor (sector 16), then the path. 1 found (sector and size),
// -2 the tray opened, -3 not found.
// FS2 0x00157458

int rxFileCdFind(const char *name, int *lsn, int *size)
{
    char iso[256];
    int tableLsn;
    int tableSize;

    *lsn = 0;
    *size = 0;
    if (s_cdLayerPending != 0)
    {
        s_cdLayerPending = 0;
        s_cdLayer = s_cdLayerMode;
    }
    rxFileCdIsoName(iso, name);
    const char *path = iso;
    while (path[0] == '\\')
        path++;
    for (;;)
    {
        int r = rxFileCdReadCached(&tableLsn, 0x808c, 4);
        if (r == 1)
            break;
        if (r == -2)
            return -2;
    }
    s_cdDiscRead = 1;
    for (;;)
    {
        int r = rxFileCdReadCached(&tableSize, 0x8084, 4);
        if (r == 1)
            return rxFileCdFindDir(tableLsn << 11, tableSize, path, lsn, size);
        if (r == -2)
            return -2;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Walk the path table: each dir element is the record whose parent is the
// previous element's record number (the root is 1); the file is then
// looked up in the last dir's records.
// FS2 0x00157598

int rxFileCdFindDir(int tableOffset, int tableSize, const char *path, int *lsn, int *size)
{
    unsigned char rec[8];         // len, ext len, extent (4), parent (2)
    char want[256];
    char got[256];
    int r;

    while ((r = rxFileCdReadCached(rec, tableOffset, 8)) != 1)
    {
        if (r == -2)
            return -2;
    }
    unsigned int dirNo = 1;
    int at = (int)((rec[0] | rec[1] << 8) + 9) & ~1;
    unsigned int no = 2;
    int more = 1;
    if (at < tableSize)
    {
        do
        {
            if (rxFileCdIsLastElem(path))
                goto found;
            path = rxFileCdPathElem(want, path);
            do
            {
                while ((r = rxFileCdReadCached(rec, tableOffset + at, 8)) != 1)
                {
                    if (r == -2)
                        return -2;
                }
                unsigned int len = rec[0] | rec[1] << 8;
                if ((unsigned int)(rec[6] | rec[7] << 8) == dirNo)
                {
                    while ((r = rxFileCdReadCached(got, tableOffset + at + 8, (int)len)) != 1)
                    {
                        if (r == -2)
                            return -2;
                    }
                    got[len] = '\0';
                    if (strcmp(got, want) == 0)
                        break;
                }
                at += (int)(len + 9) & ~1;
                more = at < tableSize;
                no++;
            } while (more);
            dirNo = no;
        } while (more);
    }
    else if (rxFileCdIsLastElem(path))
    {
found:
        return rxFileCdFindInDir((rec[2] | rec[3] << 8 | rec[4] << 16 | rec[5] << 24) << 11, path, lsn, size);
    }
    return -3;
}

// ──────────────────────────────────────────────────────────────────────
// Scan a dir's records (after ".") for a file of that name; records do not
// cross sectors, a zero length moves to the next sector.
// FS2 0x00157780

int rxFileCdFindInDir(int dirOffset, const char *name, int *lsn, int *size)
{
    unsigned char rec[0x22];
    char got[256];
    int r;

    while ((r = rxFileCdReadCached(rec, dirOffset, 0x22)) != 1)
    {
        if (r == -2)
            return -2;
    }
    unsigned int dirSize = rec[10] | rec[11] << 8 | rec[12] << 16 | (unsigned int)rec[13] << 24;
    int at = (int)((rec[0] | rec[1] << 8) + 1) & ~1;
    for (;;)
    {
        while ((r = rxFileCdReadCached(rec, dirOffset + at, 0x22)) != 1)
        {
            if (r == -2)
                return -2;
        }
        unsigned int len = rec[0] | rec[1] << 8;
        int next;
        if (len == 0)
        {
            next = (at & ~0x7ff) + 0x800;
        }
        else
        {
            unsigned int nameLen = rec[0x20];
            if (nameLen != 0 && (rec[0x19] & 2) == 0)    // not a dir
            {
                while ((r = rxFileCdReadCached(got, dirOffset + at + 0x21, (int)nameLen)) != 1)
                {
                    if (r == -2)
                        return -2;
                }
                got[nameLen] = '\0';
                if (strcmp(got, name) == 0)
                {
                    *lsn = rec[2] | rec[3] << 8 | rec[4] << 16 | rec[5] << 24;
                    *size = rec[10] | rec[11] << 8 | rec[12] << 16 | rec[13] << 24;
                    return 1;
                }
            }
            if ((at & 0x7ff) + 0x22 > 0x800)
                next = (at & ~0x7ff) + 0x800;
            else
                next = at + (int)len;
        }
        at = next;
        if ((long long)dirSize <= at)
            return -3;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Copy the next path element (up to "\"); returns what follows it.
// FS2 0x00157950

const char *rxFileCdPathElem(char *out, const char *path)
{
    if (*path != '\0')
    {
        if (*path == '\\')
        {
            *out = '\0';
            return path + 1;
        }
        *out = *path;
        for (;;)
        {
            out++;
            path++;
            if (*path == '\0' || *path == '\\')
                break;
            *out = *path;
        }
    }
    *out = '\0';
    return *path == '\\' ? path + 1 : path;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001579b0

int rxFileCdIsLastElem(const char *path)
{
    while (*path != '\0' && *path != '\\')
        path++;
    return *path == '\0';
}

// ──────────────────────────────────────────────────────────────────────
// Copy size bytes at a disc offset through the sector cache: a hit moves
// the sector to the front, a miss takes a free buffer (or the least
// recently used one) and reads it until it reads. 1, or -2 when the tray
// opened.
// FS2 0x00157a00

int rxFileCdReadCached(void *out, int offset, int size)
{
    unsigned char *dst = (unsigned char *)out;
    int sector = offset / 0x800;
    int skip = offset - sector * 0x800;

    for (;;)
    {
        if (size < 1)
            return 1;
        int key = rxFileCdMapSector(sector);
        RxFileCdCacheNode *node = 0;
        for (RxFileCdCacheNode *n = s_cdCacheUsed.next; n != &s_cdCacheUsed; n = n->next)
        {
            if (n->sector == key)
            {
                node = n;
                break;
            }
        }
        if (node != 0)
        {
            node->prev->next = node->next;
            node->next->prev = node->prev;
        }
        else
        {
            if (s_cdCacheFree == 0)
            {
                // Recycle the least recently used sector.
                RxFileCdCacheNode *lru = s_cdCacheUsed.prev;
                lru->prev->next = lru->next;
                lru->next->prev = lru->prev;
                lru->next = s_cdCacheFree;
                s_cdCacheFree = lru;
            }
            node = s_cdCacheFree;
            node->sector = key;
            for (;;)
            {
                int r = rxFileCdReadAt(node->buf, (long long)sector << 11, 0x800);
                if (r == 1)
                    break;
                if (r == -2)
                    return -2;
            }
            if (rxFileVerbose != 0)
                rxSysPrintf("rxfile : completed to read sector '%d'\n", key);
            s_cdCacheFree = node->next;
        }
        node->prev = &s_cdCacheUsed;
        node->next = s_cdCacheUsed.next;
        s_cdCacheUsed.next = node;
        node->next->prev = node;

        while (skip < 0x800 && size > 0)
        {
            *dst++ = node->buf[skip++];
            size--;
        }
        skip = 0;
        sector++;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00157c28

char *rxFileCdGetRoot(void)
{
    return (char *)s_cdRoot;
}

// ──────────────────────────────────────────────────────────────────────
// The cdrom0: root is fixed.
// FS2 0x00157c38

void rxFileCdSetRoot(const char *root)
{
    (void)root;
}

// ──────────────────────────────────────────────────────────────────────
// Replace the file lookup; returns the old one.
// FS2 0x00157c40

RxFileCdFindFunc rxFileCdSetFindFunc(RxFileCdFindFunc func)
{
    RxFileCdFindFunc old = s_cdFind;
    s_cdFind = func;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// Turn the periodic clock read on or off (a change restarts it); returns
// the old setting.
// FS2 0x00157c58

int rxFileCdSetClockRead(int enable)
{
    int old = s_cdClockRead;
    if (old != enable)
    {
        s_cdClockInterval = 0x1e;
        s_cdClockTick = 0;
        s_cdClockCountdown = 0;
        s_cdClockValid = 0;
        s_cdClockRead = enable;
    }
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00157ca0

int rxFileCdGetClockRead(void)
{
    return s_cdClockRead;
}

// ──────────────────────────────────────────────────────────────────────
// The last clock read (an sceCdCLOCK); 0 when there is none.
// FS2 0x00157cb0

int rxFileCdGetClock(void *clock)
{
    if (s_cdClockValid == 0 || s_cdClockRead == 0)
        return 0;
    if (clock != 0)
    {
        DIntr();
        memcpy(clock, &s_cdClock, sizeof(s_cdClock));
        EIntr();
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Pick the DVD layer (0 / 1) the next lookup switches to; returns the old
// mode, -1 for a bad one.
// FS2 0x00157d70

int rxFileCdSetLayerMode(int mode)
{
    int old = s_cdLayerMode;
    if ((unsigned int)mode >= 2)
        return -1;
    s_cdLayerMode = mode;
    s_cdLayerPending = 1;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00157da8

int rxFileCdGetLayerMode(void)
{
    return s_cdLayerMode;
}
