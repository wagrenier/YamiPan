/* ==========================================================================
 *  rx/rxfile.c
 *
 *  The rx middleware's file layer: async file slots, whole-file loads, IOP
 *  reboot and module loads, contexts, dirs, and the file thread that serves
 *  the slots' requests through the device drivers (rx/filehost.c,
 *  rx/filecd.c, rx/filehdd.c, linked between this file's two ranges
 *  0x153518..0x156060 and 0x158400..0x159928). See rxfile.h.
 *
 *  FS2 links Rule of Rose's prebuilt rx: this file is RosePan's
 *  src/rx/rxfile.cpp (every function written there, checked against RoR)
 *  without the drivers, carried over to C. rxcmp.py '^rxFile' finds 106 of
 *  the 108 functions identical (jump targets, lui and gp offsets masked);
 *  FS2's build is a little newer: rxFileOnCreate / rxFileOnGetSize print
 *  __FILE__ / __LINE__ and the slot in their failure message (marked FS2).
 *  Data: RoR's +0x198f0 (s_verbose 0x2f5aa0; the device map from 0x2f5b18),
 *  bss: RoR's -0x30a030 (slot pools, drivers) and -0x30a040 (contexts,
 *  devices, the thread, after the drivers' bss).
 *
 *  The ROM saves the COP0 Status around every DIntr ... EIntr section and
 *  restores it after (so a section nested in another leaves interrupts
 *  off); the host's sections are a recursive mutex that nests by itself,
 *  so the save / restore is left out. Every spin-wait in the ROM also
 *  prints "DI error" when it finds interrupts disabled (the file thread
 *  could then never answer): rxFileSpin, empty on the host.
 * ======================================================================== */
#include "rx/rxfile.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "rx/filecd.h"
#include "rx/filehdd.h"
#include "rx/filehost.h"
#include "rx/rxsys.h"
#include "rx/rxthread.h"
#include "sdk/eekernel.h"
#include "sdk/fileio.h"
#include "sdk/sifcmd.h"
#include "sdk/sifdev.h"

/* ---- data ------------------------------------------------------------- */

int rxFileVerbose = 1;
#define s_verbose rxFileVerbose                 /* data 0x2f5aa0: print rxfile messages */
/* data 0x2f5aa4: a zero word nothing in rxfile reads (FS2 only: RoR's
 * filehost data follows s_verbose directly). */
static int s_deviceMap[RX_SYS_DEVICE_NUM] = { 0, 1, 2, 3 };             /* data 0x2f5b18 */
static int s_fallbackDevice[RX_SYS_DEVICE_NUM] = { 0, 1, 2, -1 };       /* data 0x2f5b28 */
static RxFileErrorHook s_errorHook;             /* data 0x2f5b38 */
static int s_deviceThread[RX_SYS_DEVICE_NUM];   /* data 0x2f5b40: file thread per device (all 0) */

RxFileSlot     rxFileSlots[64];                 /* bss 0x4d0b80 */
RxFileTag      rxFileTagPool[64];               /* bss 0x4d3f80 */
RxFileTag     *rxFileTagFree;                   /* bss 0x4d4380 */
RxFileMarker   rxFileMarkerPool[64];            /* bss 0x4d4384 */
RxFileMarker  *rxFileMarkerFree;                /* bss 0x4d4984 */
RxFileSegment  rxFileSegmentPool[256];          /* bss 0x4d4988 */
RxFileSegment *rxFileSegmentFree;               /* bss 0x4d5988 */

static RxFileDriver s_drivers[RX_SYS_DEVICE_NUM];   /* bss 0x4d5990 (HOST 0x4d5990, CDROM 0x4d59b4, HDD 0x4d59fc) */
static int s_contextIndex;                      /* bss 0x4e7e80 */
static RxFileContext s_contexts[2];             /* bss 0x4e7e84 */
static RxFileDevInfo s_devInfo[RX_SYS_DEVICE_NUM];  /* bss 0x4e7e9c */
static RxFileThread s_threads[1];               /* bss 0x4ec8d0 */

// The ROM's spin-wait step: warn ("DI error") when interrupts are off.
// PORT: no Status register on the host; the wait just spins.
static void rxFileSpin(void)
{
}

// PORT: the host's file thread runs beside the game, and a callback lowers
// pending part way through (rxFileOnGetSize sets the failed bit after it), so
// a waiter that sees pending reach 0 could read the slot before the callback
// is done: fileExists took a missing data/options.txt for there and the
// FileReader that followed retried it forever (hazards.md). The callbacks run
// inside the file thread's DIntr ... EIntr; taking the section once waits the
// last one out. The PS2's waiter cannot run before the callback returns.
static void rxFileWaitCallbacks(void)
{
    DIntr();
    EIntr();
}

/* ---- async file slots ------------------------------------------------- */

static RxFileSlot *rxFileGetSlot(int fd)
{
    if ((unsigned)(fd - 1) < 0x3f && rxFileSlots[fd].used != 0)
        return &rxFileSlots[fd];
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153518

void rxFileClose(int fd)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot != 0)
        rxFileSlotClose(slot, 0);
}

// ──────────────────────────────────────────────────────────────────────
// Close, also cancelling the request the file thread is serving.
// FS2 0x00153578

void rxFileAbort(int fd)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot != 0)
        rxFileSlotClose(slot, 1);
}

// ──────────────────────────────────────────────────────────────────────
// 0 closed; else 1, | 4 requests pending, | 8 at the end, | 2 paused.
// FS2 0x001535d8

int rxFileGetState(int fd)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return 0;
    int state = slot->pending > 0 ? 5 : 1;
    if (slot->size <= slot->pos)
        state |= 8;
    if (slot->paused != 0)
        state |= 2;
    return state;
}

// ──────────────────────────────────────────────────────────────────────
// Read up to size bytes at the position (waits for the size to be known);
// returns the count asked for, clamped to the end. Without RX_FILE_NOWAIT
// it waits for the data too.
// FS2 0x00153658

int rxFileRead(int fd, void *buf, int size)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return -1;
    if ((slot->flags & SCE_RDONLY) == 0)
        return -1;
    while ((slot->state & 2) == 0)
        rxFileSpin();             // callcheck: EIntr, rxSysPrintf (the DI check)

    unsigned int count = (unsigned int)size;
    unsigned int left = slot->size - slot->pos;
    if (left <= count)
        count = left;
    if ((int)count > 0 && slot->pos < slot->size)
    {
        unsigned char *dst = (unsigned char *)buf;
        int want = (int)count;
        do
        {
            int got = rxFileSlotTransfer(slot, RX_FILE_OP_READ, dst, want);
            dst += got;
            rxFileSpin();             // callcheck: EIntr, rxSysPrintf (the DI check)
            if (want - got < 1)
                break;
            want -= got;
        } while (slot->pos < slot->size);
    }
    if ((slot->flags & RX_FILE_NOWAIT) == 0)
        rxFileSync(fd, 0);
    return (int)count;
}

// ──────────────────────────────────────────────────────────────────────
// Write size bytes at the position; the size grows to the position.
// FS2 0x00153818

int rxFileWrite(int fd, const void *buf, int size)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return -1;
    if ((slot->flags & SCE_WRONLY) == 0)
        return -1;
    unsigned char *src = (unsigned char *)buf;
    int left = size;
    if (size > 0)
    {
        do
        {
            int got = rxFileSlotTransfer(slot, RX_FILE_OP_WRITE, src, left);
            src += got;
            left -= got;
            rxFileSpin();             // callcheck: EIntr, rxSysPrintf (the DI check)
        } while (left > 0);
    }
    if ((slot->flags & RX_FILE_NOWAIT) == 0)
        rxFileSync(fd, 0);
    if (slot->size < slot->pos)
        slot->size = slot->pos;
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// Whence 0 / 1 as lseek, 2 = size - offset; returns the position.
// FS2 0x00153960

int rxFileSeek(int fd, int offset, int whence)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return -1;
    if (whence == SCE_SEEK_CUR)
        slot->pos = slot->pos + offset;
    else if (whence == SCE_SEEK_SET)
        slot->pos = offset;
    else if (whence == SCE_SEEK_END)
        slot->pos = slot->size - offset;
    return (int)slot->pos;
}

// ──────────────────────────────────────────────────────────────────────
// Wait until the slot's requests are served (noWait: just look); returns
// whether some are still pending.
// FS2 0x001539f8

int rxFileSync(int fd, int noWait)
{
    int busy = 0;
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot != 0)
    {
        do
        {
            busy = slot->pending > 0;
            rxFileSpin();             // callcheck: EIntr, rxSysPrintf (the DI check)
        } while (busy && noWait == 0);
    }
    return busy;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153ac8

int rxFileSetMarkers(int fd, const RxFileMarkerDef *defs, int count, void *arg)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return 0;
    return rxFileSlotSetMarkers(slot, defs, count, arg);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153b28

void rxFileEnableMarker(int fd, int id, int active)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot != 0)
        rxFileSlotEnableMarker(slot, id, active);
}

// ──────────────────────────────────────────────────────────────────────
// Pause / resume the ring refill (a slot opens paused); returns the old
// setting.
// FS2 0x00153b80

int rxFilePause(int fd, int pause)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return 0;
    int old = slot->paused;
    slot->paused = (unsigned char)pause;
    if ((unsigned char)pause == 0)
        rxFileSlotKick(slot);
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// The ring's data at the read offset.
// FS2 0x00153bf8

unsigned char *rxFileGetRingData(int fd)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return 0;
    return slot->ring + slot->ringRead;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153c48

int rxFileGetRingReadPos(int fd)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return -1;
    return (int)slot->ringRead;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153c90

int rxFileGetRingSize(int fd)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return 0;
    return (int)slot->ringSize;
}

// ──────────────────────────────────────────────────────────────────────
// Bytes in the ring; also restarts the refill if it can run.
// FS2 0x00153cd8

int rxFileGetRingFilled(int fd)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return 0;
    int filled = (int)slot->ringFilled;
    rxFileSlotKick(slot);
    return filled;
}

// ──────────────────────────────────────────────────────────────────────
// The caller used size bytes of the ring: free them for the refill.
// FS2 0x00153d48

void rxFileRingConsume(int fd, int size)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return;
    int filled = (int)slot->ringFilled;
    int n = size < filled ? size : filled;
    if (n < 0)
        n = 0;
    slot->ringRead = (int)(slot->ringRead + n) % (int)slot->ringSize;
    DIntr();
    slot->ringFilled -= n;
    EIntr();
    rxFileSlotDropSegments(slot, n);
    rxFileSlotKick(slot);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153e98

int rxFileGetRingFilePos(int fd, int offset)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return -1;
    return rxFileSlotSegmentPos(slot, offset);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153ef0

int rxFileSetTags(int fd, const RxFileTagDef *defs, int count)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return 0;
    return rxFileSlotSetTags(slot, defs, count);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153f50

void rxFileEnableTag(int fd, int id, int active)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot != 0)
        rxFileSlotEnableTag(slot, id, active);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153fa8

int rxFileGetRingTags(int fd, RxFileTagHit *out, int max, int size)
{
    RxFileSlot *slot = rxFileGetSlot(fd);
    if (slot == 0)
        return 0;
    return rxFileSlotGetTags(slot, out, max, size);
}

// ──────────────────────────────────────────────────────────────────────
// Open name on the current device: its dir, its search dir and the name,
// as rxFileMakePath joins them ("/" in front makes them absolute), without
// the driver's root (the driver adds it per request).
// FS2 0x00154008

int rxFileOpenEx(const char *name, int flags, void *ring, int ringSize, int threshold)
{
    char path[256];

    int device = rxFileGetCurrent();
    rxFileSetLastName(device, name);
    char *search = rxFileGetSearchDir(device);
    if (search[0] == '/' || name[0] == '/')
        path[0] = '\0';
    else
        strcpy(path, rxFileGetDir(device));
    if (name[0] != '/')
        strcpy(path + strlen(path), rxFileGetSearchDir(device));
    strcpy(path + strlen(path), name);
    return rxFileOpenSlot(device, path, flags, ring, ringSize, threshold);
}

// ──────────────────────────────────────────────────────────────────────
// Take a free slot and ask the file thread to create (SCE_CREAT) or size
// the file; without RX_FILE_NOWAIT wait for the answer. Returns the fd, -1
// when no slot is free or the file does not open.
// FS2 0x00154130

int rxFileOpenSlot(int device, const char *path, int flags, void *ring, int ringSize, int threshold)
{
    int fd;

    DIntr();
    for (fd = 1; fd < 64; fd++)
    {
        if (rxFileSlots[fd].used == 0)
        {
            rxFileSlots[fd].used = 1;
            break;
        }
    }
    EIntr();
    if (fd == 64)
    {
        if (s_verbose != 0)
            rxSysPrintf("rxfile : failed to alloc slot\n");
        return -1;
    }

    RxFileSlot *slot = &rxFileSlots[fd];
    strcpy(slot->path, path);
    slot->paused = 1;
    slot->ringFilled = 0;
    slot->ring = (unsigned char *)ring;
    slot->state = 0;
    slot->ringSize = ringSize;
    slot->ringThreshold = threshold;
    slot->device = (unsigned char)device;
    slot->flags = flags;
    slot->pos = 0;
    slot->size = 0;
    slot->ringWrite = 0;
    slot->ringRead = 0;
    slot->markers = 0;
    slot->markerArg = 0;
    slot->tags = 0;
    slot->pending = 0;
    slot->segments.prev = &slot->segments;
    slot->segments.next = &slot->segments;
    slot->segments.pos = 0;
    slot->segments.size = 0;

    int retry = (flags >> 15) & 1;
    if ((flags & SCE_CREAT) != 0)
    {
        // A created file is never sized: rxFileWrite grows it.
        while (rxFilePostRequest(0, device, slot->path, RX_FILE_OP_CREATE, (flags >> 10) & 1,
                                 0, 0, retry, rxFileOnCreate, slot) == 0)
            rxFileSpin();             // callcheck: EIntr, rxSysPrintf (the DI check)
    }
    else
    {
        while (rxFilePostRequest(0, device, slot->path, RX_FILE_OP_SIZE, 0, 0, 0, retry,
                                 rxFileOnGetSize, slot) == 0)
            rxFileSpin();             // callcheck: EIntr, rxSysPrintf (the DI check)
    }
    DIntr();
    slot->pending++;
    EIntr();

    if ((flags & RX_FILE_NOWAIT) != 0)
        return fd;
    while (slot->pending > 0)
        rxFileSpin();             // callcheck: EIntr, rxSysPrintf (the DI check)
    rxFileWaitCallbacks();
    if ((slot->state & 4) != 0)
    {
        slot->used = 0;
        return -1;
    }
    return fd;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00154518

void rxFileOnCreate(RxFileSlot *slot, int offset, int result)
{
    (void)offset;
    slot->state |= 1;
    slot->pending--;
    if (result < 1)
    {
        slot->state |= 4;
        if (s_verbose != 0)     /* FS2: file, line and the slot (RoR: the path only) */
            rxSysPrintf("rxfile(%s,%d) : failed to open file '%s' %d %d %X,%d %d\n",
                        "rxfile.c", 1018, slot->path, slot->device, slot->flags,
                        (unsigned int)(uintptr_t)slot->ring, slot->ringSize, result);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001545a0

void rxFileOnGetSize(RxFileSlot *slot, int offset, int result)
{
    (void)offset;
    slot->size = result;
    slot->state |= 2;
    slot->pending--;
    rxFileSlotKick(slot);
    if (result < 1)
    {
        slot->state |= 4;
        if (s_verbose != 0)     /* FS2: file, line and the slot (RoR: the path only) */
            rxSysPrintf("rxfile(%s,%d) : failed to open file '%s' %d %d %X,%d %d\n",
                        "rxfile.c", 1041, slot->path, slot->device, slot->flags,
                        (unsigned int)(uintptr_t)slot->ring, slot->ringSize, result);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Cancel the slot's queued requests (abort: also the one being served),
// wait for the rest, free its segments, markers and tags.
// FS2 0x00154640

void rxFileSlotClose(RxFileSlot *slot, int abort)
{
    rxFileCancelRequests(slot->device, slot, abort);
    while (slot->pending > 0)
        rxFileSpin();             // callcheck: EIntr, rxSysPrintf (the DI check)
    rxFileWaitCallbacks();
    rxFileSlotFreeSegments(slot);
    rxFileSlotSetMarkers(slot, 0, 0, 0);
    rxFileSlotSetTags(slot, 0, 0);
    slot->used = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Refill the ring when it may: not paused, not at the end, nothing
// pending, the size known and the ring down to its threshold.
// FS2 0x00154708

void rxFileSlotKick(RxFileSlot *slot)
{
    if (slot->paused == 0 && slot->pos < slot->size && slot->pending == 0
        && (slot->state & 2) != 0 && slot->ringFilled <= slot->ringThreshold
        && slot->pending == 0)
        rxFileSlotRefill(slot);
}

// ──────────────────────────────────────────────────────────────────────
// A caller's read / write: queue 1.
// FS2 0x00154780

int rxFileSlotTransfer(RxFileSlot *slot, int op, void *buf, int size)
{
    return rxFileSlotRequest(slot, 1, op, buf, size);
}

// ──────────────────────────────────────────────────────────────────────
// Ask for the free part of the ring in whole sectors at its write offset,
// split where it wraps; returns the bytes asked for.
// FS2 0x001547b0

int rxFileSlotRefill(RxFileSlot *slot)
{
    int done = 0;
    unsigned int ringSize = slot->ringSize;
    unsigned int want = (ringSize - slot->ringFilled) & 0xfffff800;
    unsigned int write = slot->ringWrite;

    if ((int)want > 0)
    {
        while (slot->pos < slot->size)
        {
            int n = (int)want - done;
            if (ringSize <= write + n)
                n = (int)(ringSize - write);
            int got = rxFileSlotRequest(slot, 0, RX_FILE_OP_READ, slot->ring + write, n);
            ringSize = slot->ringSize;
            write += got;
            done += got;
            if (ringSize <= write)
                write = 0;
            if (got < n)
                return done;
            if ((int)want <= done)
                return done;
        }
    }
    return done;
}

// ──────────────────────────────────────────────────────────────────────
// Post a transfer in pieces that end at markers: an active marker inside
// the next piece whose pass limit is not reached stops it when its callback
// agrees (no callback: always). After each piece the markers it reached
// count a pass (past the limit they reset), and a stop at a marker jumps
// the position to the marker's target (-1: the end). Reads stop at the end.
// FS2 0x00154888

int rxFileSlotRequest(RxFileSlot *slot, int queue, int op, void *buf, int size)
{
    int done = 0;
    if (size <= 0)
        return 0;

    unsigned int start = slot->pos;
    if (op != RX_FILE_OP_WRITE && slot->size <= start)
        return 0;

    for (;;)
    {
        int n = size - done;
        int stop = 0;
        for (RxFileMarker *m = slot->markers; m != 0; m = m->next)
        {
            RxFileMarkerDef *d = &m->def;
            if (d->active == 0 || d->pos < slot->pos || slot->pos + n < d->pos)
                continue;
            if (d->count < d->max || d->max == 0)
            {
                stop = d->callback != 0 ? d->callback(slot->markerArg, d) : 1;
                if (stop != 0)
                {
                    n = (int)(d->pos - slot->pos);
                    break;
                }
            }
        }

        int got = rxFileSlotRequestChunked(slot, queue, op, (unsigned char *)buf + done, n);
        done += got;
        for (RxFileMarker *m = slot->markers; m != 0; m = m->next)
        {
            RxFileMarkerDef *d = &m->def;
            if (d->active == 0 || d->pos < start || start + got < d->pos)
                continue;
            if (d->count < d->max || d->max == 0)
            {
                d->count++;
                if (d->pos == start + got && stop != 0)
                    slot->pos = d->jumpPos != -1 ? (unsigned int)d->jumpPos : slot->size;
            }
            else
            {
                d->count = 0;
            }
        }

        if (got < n)
            return done;
        if (size <= done)
            return done;
        start = slot->pos;
        if (op != RX_FILE_OP_WRITE && slot->size <= start)
            return done;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Refill reads stop at the end; a caller's read is posted in pieces of
// the device param's size when it has one.
// FS2 0x00154ab8

int rxFileSlotRequestChunked(RxFileSlot *slot, int queue, int op, void *buf, int size)
{
    if (queue == 0 && slot->size < slot->pos + size)
        size = (int)(slot->size - slot->pos);
    int chunk = rxFileGetDeviceParam(slot->device);
    if (queue == 1 && op == RX_FILE_OP_READ && chunk > 0)
    {
        int done = 0;
        while (done < size)
        {
            if (slot->size <= slot->pos)
                break;
            int n = size - done;
            if (chunk <= n)
                n = chunk;
            int got = rxFileSlotPost(slot, 1, RX_FILE_OP_READ, (unsigned char *)buf + done, n);
            done += got;
            if (got < n)
                return done;
        }
        return done;
    }
    return rxFileSlotPost(slot, queue, op, buf, size);
}

// ──────────────────────────────────────────────────────────────────────
// Queue one transfer at the position and move the position past it;
// 0 when the queue is full.
// FS2 0x00154c68

int rxFileSlotPost(RxFileSlot *slot, int queue, int op, void *buf, int size)
{
    if (size <= 0)
        return 0;
    if (rxFilePostRequest(queue, slot->device, slot->path, op, (int)slot->pos, buf, size, 1,
                          rxFileOnTransfer, slot) == 0)
        return 0;
    slot->pos += size;
    DIntr();
    slot->pending++;
    EIntr();
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// A transfer was served: in ring mode the bytes join the ring.
// FS2 0x00154d48

void rxFileOnTransfer(RxFileSlot *slot, int offset, int result)
{
    if (result > 0 && (slot->flags & RX_FILE_RING) != 0)
    {
        slot->ringFilled += result;
        slot->ringWrite = (slot->ringWrite + result) % slot->ringSize;
        rxFileSlotAddSegment(slot, offset, result);
    }
    slot->pending--;
}

// ──────────────────────────────────────────────────────────────────────
// Replace the slot's markers with count defs, kept sorted by position
// (as many as the pool has); returns how many were taken.
// FS2 0x00154dc0

int rxFileSlotSetMarkers(RxFileSlot *slot, const RxFileMarkerDef *defs, int count, void *arg)
{
    int taken = 0;

    DIntr();
    RxFileMarker *m;
    while ((m = slot->markers) != 0)
    {
        slot->markers = m->next;
        m->next = rxFileMarkerFree;
        rxFileMarkerFree = m;
    }
    for (int i = 0; i < count && rxFileMarkerFree != 0; i++)
    {
        RxFileMarker *node = rxFileMarkerFree;
        rxFileMarkerFree = node->next;
        RxFileMarker **link = &slot->markers;
        RxFileMarker *p = *link;
        if (p == 0 || defs[i].pos < p->def.pos)
        {
            node->next = p;
        }
        else
        {
            RxFileMarker *prev;
            do
            {
                prev = p;
                p = p->next;
            } while (p != 0 && p->def.pos <= defs[i].pos);
            node->next = p;
            link = &prev->next;
        }
        *link = node;
        taken++;
        node->def = defs[i];
    }
    EIntr();
    slot->markerArg = arg;
    return taken;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00154f88

void rxFileSlotEnableMarker(RxFileSlot *slot, int id, int active)
{
    for (RxFileMarker *m = slot->markers; m != 0; m = m->next)
    {
        if (m->def.id == (unsigned int)id)
            m->def.active = (unsigned char)active;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Append a ring segment (dropped when the pool is empty).
// FS2 0x00154fc8

void rxFileSlotAddSegment(RxFileSlot *slot, int pos, int size)
{
    DIntr();
    RxFileSegment *seg = rxFileSegmentFree;
    if (seg != 0)
    {
        rxFileSegmentFree = seg->next;
        seg->pos = pos;
        seg->size = size;
        RxFileSegment *tail = slot->segments.prev;
        seg->next = &slot->segments;
        seg->prev = tail;
        tail->next = seg;
        seg->next->prev = seg;
    }
    EIntr();
}

// ──────────────────────────────────────────────────────────────────────
// Remove size bytes from the front of the segments.
// FS2 0x00155090

void rxFileSlotDropSegments(RxFileSlot *slot, int size)
{
    DIntr();
    RxFileSegment *end = &slot->segments;
    RxFileSegment *seg = end->next;
    if (seg != end)
    {
        int len = seg->size;
        while (len <= size)
        {
            RxFileSegment *next = seg->next;
            seg->prev->next = next;
            next->prev = seg->prev;
            seg->next = rxFileSegmentFree;
            rxFileSegmentFree = seg;
            size -= len;
            seg = next;
            if (seg == end)
            {
                EIntr();
                return;
            }
            len = seg->size;
        }
        seg->size = len - size;
        seg->pos = seg->pos + size;
    }
    EIntr();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001551b0

void rxFileSlotFreeSegments(RxFileSlot *slot)
{
    DIntr();
    RxFileSegment *seg = slot->segments.next;
    while (seg != &slot->segments)
    {
        RxFileSegment *next = seg->next;
        seg->prev->next = next;
        next->prev = seg->prev;
        seg->next = rxFileSegmentFree;
        rxFileSegmentFree = seg;
        seg = next;
    }
    EIntr();
}

// ──────────────────────────────────────────────────────────────────────
// File position of the byte offset bytes past the ring read offset; -1
// past the segments.
// FS2 0x00155268

int rxFileSlotSegmentPos(RxFileSlot *slot, int offset)
{
    int pos = -1;

    DIntr();
    RxFileSegment *end = &slot->segments;
    for (RxFileSegment *seg = end->next; seg != end; seg = seg->next)
    {
        if (offset < seg->size)
        {
            pos = (int)seg->pos + offset;
            break;
        }
        offset -= seg->size;
    }
    EIntr();
    return pos;
}

// ──────────────────────────────────────────────────────────────────────
// Replace the slot's tags with count defs, kept sorted by position.
// FS2 0x00155348

int rxFileSlotSetTags(RxFileSlot *slot, const RxFileTagDef *defs, int count)
{
    int taken = 0;

    DIntr();
    RxFileTag *t;
    while ((t = slot->tags) != 0)
    {
        slot->tags = t->next;
        t->next = rxFileTagFree;
        rxFileTagFree = t;
    }
    for (int i = 0; i < count && rxFileTagFree != 0; i++)
    {
        RxFileTag *node = rxFileTagFree;
        rxFileTagFree = node->next;
        RxFileTag **link = &slot->tags;
        RxFileTag *p = *link;
        if (p == 0 || defs[i].pos < p->def.pos)
        {
            node->next = p;
        }
        else
        {
            RxFileTag *prev;
            do
            {
                prev = p;
                p = p->next;
            } while (p != 0 && p->def.pos <= defs[i].pos);
            node->next = p;
            link = &prev->next;
        }
        *link = node;
        taken++;
        node->def = defs[i];
    }
    EIntr();
    return taken;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001554f0

void rxFileSlotEnableTag(RxFileSlot *slot, int id, int active)
{
    for (RxFileTag *t = slot->tags; t != 0; t = t->next)
    {
        if (t->def.id == (unsigned int)id)
            t->def.active = (unsigned char)active;
    }
}

// ──────────────────────────────────────────────────────────────────────
// List the active tags inside the first size bytes of the ring (by its
// segments): their offset past the read offset and value, at most max
// stored; returns how many there are.
// FS2 0x00155530

int rxFileSlotGetTags(RxFileSlot *slot, RxFileTagHit *out, int max, int size)
{
    RxFileSegment *end = &slot->segments;
    RxFileSegment *seg = end->next;
    int base = 0;
    unsigned int found = 0;

    if (seg == end || size < 1)
        return 0;
    for (;;)
    {
        unsigned int lo = seg->pos;
        int len = seg->size;
        unsigned int hi = lo + size;
        if (lo + len <= lo + size)
            hi = lo + len;
        for (RxFileTag *t = slot->tags; t != 0; t = t->next)
        {
            if (t->def.active == 0)
                continue;
            unsigned int p = t->def.pos;
            if (p < lo || hi <= p)
                continue;
            if (found < (unsigned int)max)
            {
                out[found].offset = base + (int)(p - lo);
                out[found].value = t->def.value;
            }
            found++;
        }
        seg = seg->next;
        size -= len;
        base += len;
        if (seg == end || size < 1)
            return (int)found;
    }
}

/* ---- the file layer --------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Set a device driver's root (-1 = the boot device): rxSysInit hands the
// boot path's device part here.
// FS2 0x001555f8

void rxFileCallDevice(int device, const char *root)
{
    if (device == -1)
        device = rxSysGetBootDevice();
    RxFileDriver *drv = rxFileGetDriver(device);
    if (drv != 0)
        drv->setRoot(root);     // callcheck: rxFileHostSetRoot (driver slot 7)
}

// ──────────────────────────────────────────────────────────────────────
// A device driver's root (-1 = the boot device).
// FS2 0x00155648

char *rxFileGetDeviceRoot(int device)
{
    if (device == -1)
        device = rxSysGetBootDevice();
    RxFileDriver *drv = rxFileGetDriver(device);
    if (drv != 0)
        return drv->getRoot();  // callcheck: rxFileHostGetRoot (driver slot 8)
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Load a whole file through the current context's load hook.
// FS2 0x00155690

int rxFileCallLoadHook(const char *name, void *buf)
{
    RxFileLoadHook hook = rxFileGetLoadHook();
    return hook(name, buf);
}

// ──────────────────────────────────────────────────────────────────────
// A file's size through the current context's size hook.
// FS2 0x001556d0

int rxFileCallSizeHook(const char *name)
{
    RxFileSizeHook hook = rxFileGetSizeHook();
    return hook(name);
}

// ──────────────────────────────────────────────────────────────────────
// Install the current context's load hook (null = the default).
// FS2 0x00155700

RxFileLoadHook rxFileSetLoadHook(RxFileLoadHook hook)
{
    RxFileLoadHook old = rxFileGetLoadHook();
    if (hook == 0)
        hook = rxFileDefaultLoadHook;
    rxFileSwapLoadHook(hook);
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// Install the current context's size hook (null = the default).
// FS2 0x00155748

RxFileSizeHook rxFileSetSizeHook(RxFileSizeHook hook)
{
    RxFileSizeHook old = rxFileGetSizeHook();
    if (hook == 0)
        hook = rxFileDefaultSizeHook;
    rxFileSwapSizeHook(hook);
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// A file's size through a slot; -2 when it does not open.
// FS2 0x00155790

int rxFileDefaultSizeHook(const char *name)
{
    int size = -2;
    if (name == 0)
        return -2;
    FlushCache(0);
    int fd = rxSysStreamOpen(name, SCE_RDONLY);
    if (fd != -1)
    {
        size = rxFileSeek(fd, 0, SCE_SEEK_END);
        rxFileClose(fd);
    }
    if (s_verbose != 0)
        rxSysPrintf("[seek]: %s (%dBytes)\n", name, size);
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// Load a whole file through a slot; its size, -2 when it does not open,
// -1 when it does not read whole.
// FS2 0x00155830

int rxFileDefaultLoadHook(const char *name, void *buf)
{
    int size = -2;
    if (name == 0)
        return -2;
    FlushCache(0);
    int fd = rxSysStreamOpen(name, SCE_RDONLY);
    if (fd != -1)
    {
        size = rxFileSeek(fd, 0, SCE_SEEK_END);
        rxFileSeek(fd, 0, SCE_SEEK_SET);
        if (rxFileRead(fd, buf, size) == size)
        {
            rxFileClose(fd);
            FlushCache(0);
        }
        else
        {
            size = -1;
            rxFileClose(fd);
        }
    }
    if (s_verbose != 0)
        rxSysPrintf("[read]: %s $%X (%dBytes)\n", name, (unsigned int)(size_t)buf, size);
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// Write a whole file (created, truncated) through a slot; size, or -1.
// FS2 0x00155928

int rxFileDefaultSaveHook(const char *name, const void *buf, int size)
{
    FlushCache(0);
    int fd = rxSysStreamOpen(name, SCE_WRONLY | SCE_CREAT | SCE_TRUNC);
    if (fd == -1)
    {
        size = -1;
    }
    else
    {
        if (rxFileWrite(fd, buf, size) != size)
            size = -1;
        rxFileClose(fd);
    }
    if (s_verbose != 0)
        rxSysPrintf("[write]: %s $%X (%dBytes)\n", name, (unsigned int)(size_t)buf, size);
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// Build a device path: the current device's dir, then its search dir, then
// the name ("/" in front of the search dir or the name makes it absolute),
// and the driver puts its root in front.
// FS2 0x001559f8

int rxFileMakePath(char *out, const char *name)
{
    char path[256];

    if (name == 0)
        return 0;

    RxFileDriver *drv = rxFileGetDriver(rxFileGetCurrent());
    path[0] = '\0';
    char *search = rxFileGetSearchDir(rxFileGetCurrent());
    if (search[0] != '/')
    {
        if (name[0] == '/')
            goto join;
        strcpy(path, rxFileGetDir(rxFileGetCurrent()));
    }
    if (name[0] != '/')
        strcpy(path + strlen(path), rxFileGetSearchDir(rxFileGetCurrent()));
join:
    strcpy(path + strlen(path), name);

    // The ROM reads the slot without checking the driver: MCARD (no driver)
    // is refused by rxFileSetDevice, so the current device always has one.
    void (*makePath)(char *, const char *) = drv->makePath;
    if (makePath != 0)
    {
        makePath(out, path);    // callcheck: rxFileHostMakePath (driver slot 0)
        FlushCache(0);
    }
    return makePath != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Run another ELF from the current device (checks that it opens first);
// returns only when it does not: -2.
// FS2 0x00155b00

int rxFileLoadExec(const char *path, int argc, char **argv)
{
    char full[256];

    if (rxFileMakePath(full, path) == 0)
        return -2;
    int fd = rxSysOpen(full, SCE_RDONLY);
    if (fd < 0)
    {
        if (s_verbose != 0)
            rxSysPrintf("rxfile : Failed to open file '%s'\n", full);
        return -2;
    }
    rxSysClose(fd);
    rxSysPrintf("LoadExecPS2:%s\n", full);
    rxSysPrintf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++)
        rxSysPrintf("argv[%d] =%s\n", i, argv[i]);
    sceSifExitCmd();
    LoadExecPS2(full, argc, argv);
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// Load a whole file into buf; returns its size, -2 when it does not open,
// -1 when it does not read. Reads 0x80 bytes more than the size.
// FS2 0x00155c20

int rxFileLoad(const char *name, void *buf)
{
    char path[256];
    const char *msg;

    if (rxFileMakePath(path, name) == 0)
        return -2;

    int fd = rxSysOpen(path, SCE_RDONLY);
    if (fd < 0)
    {
        if (s_verbose != 0)
            rxSysPrintf("rxfile : Failed to open file '%s'\n", path);
        return -2;
    }

    int size = rxSysLseek(fd, 0, SCE_SEEK_END);
    if (size < 0)
    {
        rxSysClose(fd);
        if (s_verbose == 0)
            return -1;
        msg = "rxfile : Failed to seek file '%s'\n";
    }
    else
    {
        rxSysLseek(fd, 0, SCE_SEEK_SET);
        int got = rxSysRead(fd, buf, size + 0x80);
        if (got >= 0)
        {
            rxSysClose(fd);
            FlushCache(0);
            if (s_verbose != 0)
                rxSysPrintf("rxfile : Load file   '%s' (%d [%d] Bytes)\n", path, size, got);
            return size;
        }
        rxSysClose(fd);
        if (s_verbose == 0)
            return -1;
        msg = "rxfile : Failed to load file '%s'\n";
    }
    rxSysPrintf(msg, path);
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// Load an IOP module (args: arglen bytes of NUL-separated strings); 1 on
// success. Opens the file first to report a missing module by name.
// FS2 0x00155d78

int rxFileLoadModule(const char *path, int arglen, const char *args)
{
    char full[256];
    const char *msg;

    if (rxFileMakePath(full, path) == 0)
        return 0;

    int fd = rxSysOpen(full, SCE_RDONLY);
    if (fd < 0)
    {
        if (s_verbose == 0)
            return 0;
        msg = "rxfile : Failed to open module '%s'\n";
    }
    else
    {
        rxSysClose(fd);
        if (s_verbose != 0)
            rxSysPrintf("rxfile : Load module '%s'\n", full);
        int n = 0;
        int pos = 0;
        while (pos < arglen)
        {
            const char *arg = args + pos;
            if (s_verbose != 0)
                rxSysPrintf("  arg[%d] '%s'\n", n, arg);
            n++;
            pos += (int)strlen(arg) + 1;
        }
        if (sceSifLoadModule(full, arglen, args) >= 0)
            return 1;
        if (s_verbose == 0)
            return 0;
        msg = "rxfile : Failed to load module '%s'\n";
    }
    rxSysPrintf(msg, full);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Load count modules without arguments; 0 at the first failure.
// FS2 0x00155ec0

int rxFileLoadModuleList(const char *const *names, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (rxFileLoadModule(names[i], 0, "") == 0)
            return 0;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Reboot the IOP with an IOPRP image and wait for it; 1 on success.
// FS2 0x00155f38

int rxFileRebootIop(const char *img)
{
    char path[256];

    if (rxFileMakePath(path, img) == 0)
        return 0;
    if (sceSifRebootIop(path) == 0)
    {
        if (s_verbose != 0)
            rxSysPrintf("rxfile : Failed to Reboot Iop '%s'\n", path);
        return 0;
    }
    while (sceSifSyncIop() == 0)
    {
    }
    if (s_verbose != 0)
        rxSysPrintf("rxfile : Reboot Iop '%s'\n", path);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Fill the three device drivers.
// FS2 0x00155fc0

void rxFileInit(void)
{
    rxFileHostInit(&s_drivers[RX_SYS_DEVICE_HOST]);
    rxFileCdInit(&s_drivers[RX_SYS_DEVICE_CDROM]);
    rxFileHddInit(&s_drivers[RX_SYS_DEVICE_HDD]);
}

// ──────────────────────────────────────────────────────────────────────
// The drivers' (empty) terms.
// FS2 0x00155ff8

void rxFileTerm(void)
{
    rxFileHostTerm();
    rxFileCdTerm();
    rxFileHddTerm();
}

// ──────────────────────────────────────────────────────────────────────
// A device's driver: HOST, CDROM and HDD have one, MCARD none.
// FS2 0x00156020

RxFileDriver *rxFileGetDriver(int device)
{
    if (device >= 0 && (device < RX_SYS_DEVICE_MCARD || device == RX_SYS_DEVICE_HDD))
        return &s_drivers[device];
    return 0;
}

/* ---- contexts, devices and dirs --------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Empty dirs and name stacks for every device; both contexts on HOST with
// the default hooks; context 0 current.
// FS2 0x00158400

void rxFileInitSlots(void)
{
    s_contextIndex = 0;
    for (int dev = 0; dev < RX_SYS_DEVICE_NUM; dev++)
    {
        RxFileDevInfo *info = &s_devInfo[dev];
        info->param = -1;
        info->dir[0] = '\0';
        for (int i = 0; i < 2; i++)
        {
            info->ctx[i].searchDir[0] = '\0';
            info->ctx[i].stackTop = 0x7ff;
            info->ctx[i].lastName[0] = '\0';
        }
    }
    for (int i = 0; i < 2; i++)
    {
        s_contexts[i].device = RX_SYS_DEVICE_HOST;
        s_contexts[i].loadHook = rxFileDefaultLoadHook;
        s_contexts[i].sizeHook = rxFileDefaultSizeHook;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Empty in the retail build (rxsys calls it every tick).
// FS2 0x001584a8

void rxFileUpdate(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// Select context 0 or 1; returns the previous one.
// FS2 0x001584b0

int rxFileSetContext(int index)
{
    int old = s_contextIndex;
    if (index < 2 && index >= 0)
        s_contextIndex = index;
    else if (s_verbose != 0)
        rxSysPrintf("rxfile : unknow thread type '%d'\n", index);
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// Set the current context's device (-1 = the boot device); returns the
// previous one. MCARD has no driver and is refused.
// FS2 0x00158518

int rxFileSetCurrent(int device)
{
    RxFileContext *ctx = rxFileGetContext();
    int old = ctx->device;
    if (device == -1)
        device = rxSysGetBootDevice();
    if (rxSysIsDeviceReady(device) == 0)
    {
        if (s_verbose != 0)
            rxSysPrintf("rxfile : device disabled %d\n", device);
        return old;
    }
    if (device >= 0 && device != RX_SYS_DEVICE_MCARD && device <= RX_SYS_DEVICE_HDD)
    {
        ctx->device = device;
        if (s_verbose != 0)
            rxSysPrintf("rxfile : device changed %d\n", device);
        return old;
    }
    if (s_verbose != 0)
        rxSysPrintf("rxfile : unknown device type '%d'\n", device);
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// The current context's device.
// FS2 0x00158610

int rxFileGetCurrent(void)
{
    return rxFileGetContext()->device;
}

// ──────────────────────────────────────────────────────────────────────
// rxFileSetCurrent for a given context.
// FS2 0x00158630

int rxFileSetDevice(int ctx, int device)
{
    int *slot = &s_contexts[ctx].device;
    int old = *slot;
    if (device == -1)
        device = rxSysGetBootDevice();
    if (rxSysIsDeviceReady(device) == 0)
    {
        if (s_verbose != 0)
            rxSysPrintf("rxfile : device disabled %d\n", device);
        return old;
    }
    if (device >= 0 && device != RX_SYS_DEVICE_MCARD && device <= RX_SYS_DEVICE_HDD)
    {
        *slot = device;
        if (s_verbose != 0)
            rxSysPrintf("rxfile : device changed %d\n", device);
        return old;
    }
    if (s_verbose != 0)
        rxSysPrintf("rxfile : unknown device type '%d'\n", device);
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// A device's dir: lower case, '\' -> '/', a '/' added at the end.
// FS2 0x00158738

void rxFileSetDir(int device, const char *dir)
{
    char *out = rxFileGetDeviceInfo(device)->dir;
    if (dir == 0)
    {
        out[0] = '\0';
        return;
    }
    int n = 0;
    while (*dir != '\0')
    {
        char c = *dir;
        if (isupper((unsigned char)c))
            c += ' ';
        out[n] = c;
        if (c == '\\')
            out[n] = '/';
        n++;
        dir++;
    }
    out += n;
    *out = '\0';
    if (n > 0 && out[-1] != '/')
    {
        out[0] = '/';
        out[1] = '\0';
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158800

char *rxFileGetDir(int device)
{
    return rxFileGetDeviceInfo(device)->dir;
}

// ──────────────────────────────────────────────────────────────────────
// Walk a dir onto the device's search dir element by element: "/" restarts
// at the root, "." stays, ".." (and "...") climbs one less than its dots,
// anything else is appended. A '/' is added at the end.
// FS2 0x00158820

void rxFileSetSearchDir(int device, const char *dir)
{
    char elem[256];
    char *out = rxFileGetDirInfo(device)->searchDir;

    if (dir == 0)
    {
        out[0] = '\0';
        return;
    }
    while (*dir != '\0')
    {
        dir = rxFilePathNextElem(elem, dir);
        int dots = rxFilePathCountDots(elem);
        if (dots < 1 || strlen(out) < 3)
        {
            if (elem[0] == '/')
            {
                out[0] = '/';
                out[1] = '\0';
            }
            else
            {
                strcpy(out + strlen(out), elem);
            }
        }
        else
        {
            for (int i = 1; i < dots; i++)
                rxFilePathUp(out);
        }
    }
    int len = (int)strlen(out);
    if (len > 0 && out[len - 1] != '/')
    {
        out[len] = '/';
        out[len + 1] = '\0';
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158950

char *rxFileGetSearchDir(int device)
{
    return rxFileGetDirInfo(device)->searchDir;
}

// ──────────────────────────────────────────────────────────────────────
// Push the search dir onto the name stack.
// FS2 0x00158968

void rxFilePushSearchDir(int device)
{
    RxFileDirInfo *info = rxFileGetDirInfo(device);
    int top = info->stackTop - ((int)strlen(info->searchDir) + 1);
    if (top >= 0)
    {
        info->stackTop = top;
        strcpy(info->stack + top, info->searchDir);
        return;
    }
    if (s_verbose != 0)
        rxSysPrintf("rxfile : name stack is overflow\n", info);
}

// ──────────────────────────────────────────────────────────────────────
// Pop the name stack into the search dir.
// FS2 0x001589f0

void rxFilePopSearchDir(int device)
{
    RxFileDirInfo *info = rxFileGetDirInfo(device);
    if (info->stackTop < 0x7ff)
    {
        strcpy(info->searchDir, info->stack + info->stackTop);
        info->stackTop += (int)strlen(info->stack + info->stackTop) + 1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158a58

void rxFileSetLastName(int device, const char *name)
{
    strcpy(rxFileGetDirInfo(device)->lastName, name);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158a88

char *rxFileGetLastName(int device)
{
    return rxFileGetDirInfo(device)->lastName;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158aa8

int rxFileSetDeviceParam(int device, int value)
{
    RxFileDevInfo *info = rxFileGetDeviceInfo(device);
    int old = info->param;
    info->param = value;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158ad8

int rxFileGetDeviceParam(int device)
{
    return rxFileGetDeviceInfo(device)->param;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158af8

RxFileLoadHook rxFileGetLoadHook(void)
{
    return rxFileGetContext()->loadHook;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158b18

RxFileLoadHook rxFileSwapLoadHook(RxFileLoadHook hook)
{
    RxFileContext *ctx = rxFileGetContext();
    RxFileLoadHook old = ctx->loadHook;
    ctx->loadHook = hook;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158b48

RxFileSizeHook rxFileGetSizeHook(void)
{
    return rxFileGetContext()->sizeHook;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158b68

RxFileSizeHook rxFileSwapSizeHook(RxFileSizeHook hook)
{
    RxFileContext *ctx = rxFileGetContext();
    RxFileSizeHook old = ctx->sizeHook;
    ctx->sizeHook = hook;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// A device's record. The ROM also calls rxFileGetContext and drops the
// result.
// FS2 0x00158b98

RxFileDevInfo *rxFileGetDeviceInfo(int device)
{
    rxFileGetContext();
    return &s_devInfo[device];
}

// ──────────────────────────────────────────────────────────────────────
// A device's dir block for the current context.
// FS2 0x00158bd0

RxFileDirInfo *rxFileGetDirInfo(int device)
{
    return &rxFileGetDeviceInfo(device)->ctx[s_contextIndex];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158c10

RxFileContext *rxFileGetContext(void)
{
    return &s_contexts[s_contextIndex];
}

// ──────────────────────────────────────────────────────────────────────
// Copy one element of path, up to and including its '/', and return the
// rest. ROM quirk kept: the "./" test reads the byte after the copy, not
// the element, so it only fires on stale buffer contents.
// FS2 0x00158c38

const char *rxFilePathNextElem(char *out, const char *path)
{
    if (*path == '\0')
        return path;
    char c;
    while ((c = *path) != '\0')
    {
        *out = c;
        path++;
        out++;
        if (c == '/')
            break;
    }
    if (*out == '.' && out[1] == '/')
        out -= 2;
    *out = '\0';
    return path;
}

// ──────────────────────────────────────────────────────────────────────
// Drop the last element of a dir ("a/b/" -> "a/").
// FS2 0x00158ca0

void rxFilePathUp(char *dir)
{
    int len = (int)strlen(dir);
    if (len > 1)
    {
        int i = len - 1;
        dir[i] = '\0';
        while (i > 0 && dir[i - 1] != '/')
        {
            i--;
            dir[i] = '\0';
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The number of dots of an element made only of dots ("..", "../"), else 0.
// FS2 0x00158d28

int rxFilePathCountDots(const char *elem)
{
    if (*elem != '.')
        return 0;
    int n = 0;
    char c = '.';
    elem++;
    for (;;)
    {
        if (c != '.')
            return 0;
        c = *elem++;
        n++;
        if (c == '\0' || c == '/')
            return n;
    }
}

/* ---- the file thread -------------------------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Start file thread 0 at priority 2 running the cdrom reader.
// FS2 0x00158d78

void rxFileStartThread(void)
{
    rxFileCreateThread(0, 2, rxFileThreadProc);
}

// ──────────────────────────────────────────────────────────────────────
// Ask the file thread to stop, wait for it, delete it.
// FS2 0x00158da0

void rxFileStopThread(void)
{
    s_threads[0].stop = 1;
    while (s_threads[0].stop > 0)
        rxFileSpin();             // callcheck: EIntr, rxSysPrintf (the DI check)
    rxThDelete(s_threads[0].thread);
}

// ──────────────────────────────────────────────────────────────────────
// Queue a request on queue 0 / 1 of the device's file thread; 0 when it
// is full.
// FS2 0x00158e38

int rxFilePostRequest(int queue, int device, const char *path, int op, int offset, void *buf,
                      int size, int retryMissing, RxFileRequestCallback callback,
                      RxFileSlot *owner)
{
    RxFileThread *th = &s_threads[s_deviceThread[device]];
    return rxFileQueuePush(&th->queue[queue], device, path, op, offset, buf, size, retryMissing,
                           callback, owner);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00158e80

void rxFileCancelRequests(int device, RxFileSlot *owner, int abort)
{
    DIntr();
    RxFileThread *th = &s_threads[s_deviceThread[device]];
    for (int i = 0; i < 2; i++)
        rxFileQueueCancel(&th->queue[i], owner, abort);
    EIntr();
}

// ──────────────────────────────────────────────────────────────────────
// Set file thread index up (proc runs before every request) and start it
// at priority.
// FS2 0x00158f60

void rxFileCreateThread(int index, int priority, void (*proc)(int step))
{
    RxFileThread *th = &s_threads[index];
    th->thread = -1;
    th->proc = proc;
    th->step = priority + 1;
    th->stop = 0;
    rxFileQueueInit(&th->queue[0]);
    rxFileQueueInit(&th->queue[1]);
    while ((th->thread = rxThCreate(rxFileThreadMain, th->stack, sizeof(th->stack), priority, th))
           == -1)
    {
        if (s_verbose != 0)
            rxSysPrintf("rxfile : failed to create thread\n");
    }
    rxThSetState(th->thread, 1);
}

// ──────────────────────────────────────────────────────────────────────
// The file thread: run proc, then serve the first request of queue 0 (or
// else queue 1) through the mapped device's driver. A failed request stays
// queued and is tried again (a missing file only when it asked for that);
// a served one gets its callback unless it was cancelled. Yields when the
// queues are empty; once asked to stop it answers -1 and idles.
// FS2 0x00159050

void rxFileThreadMain(void)
{
    int result = 0;

    if (s_verbose != 0)
        rxSysPrintf("rxfile : start execute request queue thread %d\n", rxThGetIndex());
    RxFileThread *th = (RxFileThread *)rxThGetParam(rxThGetIndex());

    while (th->stop == 0)
    {
        if (th->proc != 0)
            th->proc(th->step);     // callcheck: rxFileThreadProc
        DIntr();
        RxFileQueue *q = 0;
        RxFileRequest *req = 0;
        for (int i = 0; i < 2 && req == 0; i++)
        {
            q = &th->queue[i];
            while ((req = rxFileQueueFront(q)) != 0 && req->state == 1)
                rxFileQueuePop(q);
        }
        if (req == 0)
        {
            EIntr();
            rxThYield();
            continue;
        }

        req->busy = 1;
        int device = s_deviceMap[req->device];
        EIntr();
        RxFileDriver *drv = rxFileGetDriver(device);
        switch (req->op)
        {
        case RX_FILE_OP_CREATE:
            result = drv->create(req->path, req->offset);   // callcheck: rxFileHostCreate (driver slot 4)
            break;
        case RX_FILE_OP_SIZE:
            result = drv->getSize(req->path);               // callcheck: rxFileHostGetSize (driver slot 3)
            break;
        case RX_FILE_OP_READ:
            result = drv->read(req->path, req->buf, req->offset, req->size); // callcheck: rxFileHostRead (driver slot 5)
            break;
        case RX_FILE_OP_WRITE:
            result = drv->write(req->path, req->offset, req->buf, req->size); // callcheck: rxFileHostWrite (driver slot 6)
            break;
        default:
            // The ROM passes no type (a1 is left over); result stays.
            if (s_verbose != 0)
                rxSysPrintf("invalide request type %d\n", req->op);
            break;
        }
        if (result < 1 && (result != -2 || req->retryMissing != 0))
        {
            req->busy = 0;          // served again after proc
            continue;
        }
        DIntr();
        if (req->state == 0 && req->callback != 0)
            req->callback(req->owner, req->offset, result);
        rxFileQueuePop(q);
        EIntr();
    }
    th->stop = -1;
    for (;;)
        rxThYield();
}

// ──────────────────────────────────────────────────────────────────────
// The file thread's work.
// FS2 0x00159380

void rxFileThreadProc(int step)
{
    rxFileCdProcess(step);
}

// ──────────────────────────────────────────────────────────────────────
// Empty and unreferenced.
// FS2 0x00159398

void rxFileNop0(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// Empty and unreferenced.
// FS2 0x001593a0

void rxFileNop1(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001593a8

void rxFileQueueInit(RxFileQueue *q)
{
    q->tail = 0;
    q->head = 0;
}

// ──────────────────────────────────────────────────────────────────────
// Append a request; 0 when the queue is full ("Queue Over!").
// FS2 0x001593b8

int rxFileQueuePush(RxFileQueue *q, int device, const char *path, int op, int offset, void *buf,
                    int size, int retryMissing, RxFileRequestCallback callback, RxFileSlot *owner)
{
    if (rxFileQueueSpace(q) < 1)
    {
        rxSysPrintf("Queue Over!\n");
        return 0;
    }
    DIntr();
    int i = q->head;
    q->head = (i + 1) % 64;
    RxFileRequest *req = &q->req[i];
    req->device = device;
    req->owner = owner;
    req->path = path;
    req->op = op;
    req->offset = offset;
    req->buf = buf;
    req->size = size;
    req->retryMissing = retryMissing;
    req->state = 0;
    req->callback = callback;
    req->busy = 0;
    EIntr();
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00159518

RxFileRequest *rxFileQueueFront(RxFileQueue *q)
{
    if (rxFileQueueCount(q) < 1)
        return 0;
    return &q->req[q->tail];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00159560

void rxFileQueuePop(RxFileQueue *q)
{
    if (rxFileQueueCount(q) > 0)
        q->tail = (q->tail + 1) % 64;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001595b8

int rxFileQueueCount(RxFileQueue *q)
{
    int n = q->head - q->tail;
    if (n < 0)
        n += 64;
    return n;
}

// ──────────────────────────────────────────────────────────────────────
// Free entries, one kept empty to tell full from empty.
// FS2 0x001595d8

int rxFileQueueSpace(RxFileQueue *q)
{
    int n = 64 - rxFileQueueCount(q);
    return n - (n > 0);
}

// ──────────────────────────────────────────────────────────────────────
// Cancel owner's queued requests: each gets its callback with its size
// and is marked done; the one being served only when abort.
// FS2 0x00159608

void rxFileQueueCancel(RxFileQueue *q, RxFileSlot *owner, int abort)
{
    DIntr();
    for (int i = q->tail; i != q->head; i = (i + 1) % 64)
    {
        RxFileRequest *req = &q->req[i];
        if (req->owner != owner || req->state == 1)
            continue;
        if (req->busy == 1 && abort == 0)
            continue;
        if (req->callback != 0)
            req->callback(req->owner, req->offset, req->size);
        req->state = 1;
        req->callback = 0;
        req->owner = 0;
    }
    EIntr();
}

/* ---- device map and the HDD error hook -------------------------------- */

// ──────────────────────────────────────────────────────────────────────
// Map device onto target (both -1 = the boot device, clamped to 0..3).
// FS2 0x00159758

void rxFileSetDeviceMap(int device, int target)
{
    if (device == -1)
        device = rxSysGetBootDevice();
    if (target == -1)
        target = rxSysGetBootDevice();
    if (device < 0)
        device = 0;
    else if (device > 3)
        device = 3;
    if (target < 0)
        target = 0;
    else if (target > 3)
        target = 3;
    s_deviceMap[device] = target;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00159808

int rxFileGetDeviceMap(int device)
{
    if (device == -1)
        device = rxSysGetBootDevice();
    int i;
    if (device < 0)
        i = 0;
    else if (device < 4)
        i = device;
    else
        i = 3;
    return s_deviceMap[i];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00159870

void rxFileSetFallbackDevice(int device, int fallback)
{
    s_fallbackDevice[device] = fallback;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00159888

int rxFileGetFallbackDevice(int device)
{
    return s_fallbackDevice[device];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001598a0

void rxFileSetErrorHook(RxFileErrorHook hook)
{
    s_errorHook = hook;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001598b0

RxFileErrorHook rxFileGetErrorHook(void)
{
    return s_errorHook;
}

// ──────────────────────────────────────────────────────────────────────
// Change the file thread's priority; returns the old one.
// FS2 0x001598c0

int rxFileSetThreadPriority(int priority)
{
    int old = rxFileGetThreadPriority();
    rxThSetPriority(s_threads[0].thread, priority);
    s_threads[0].step = priority + 1;
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00159918

int rxFileGetThreadPriority(void)
{
    return s_threads[0].step - 1;
}
