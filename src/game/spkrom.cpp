// game/spkrom.cpp -- the game's rx user layer and the SPK.ROM index (game/spkrom.h).
//
// 0x152730 / 0x152740 are jump thunks to rxFileCallLoadHook / rxFileCallSizeHook:
// the port calls the rx functions.

#include "game/spkrom.h"

#include <ctype.h>
#include <string.h>

#include "common/yamipan_stub.h"
#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/streamring.h"
#include "rx/filecd.h"
#include "rx/rxfile.h"
#include "rx/rxsys.h"
#include "rx/rxthread.h"
#include "sdk/eekernel.h"

constexpr int kFindTrayOpen = -2;
constexpr int kFindNotFound = -3;
constexpr int kNotFound = -2;
constexpr int kRxDispatchInterval = 4;      // H-syncs between rx's thread dispatches
constexpr int kRxDeviceParam = 0x80000;
constexpr int kRxStreamRead = 1;
constexpr int kRxStreamReadNoCache = 0x8001;
constexpr int kSectorShift = 11;
constexpr u_int kSpkAlign = 0x10;
constexpr u_int kRxMemAlign = 0x40;

static SpkHeader *sSpk;                 /* sbss 0x31a... gp-0x7a8c */
static RxFileCdFindFunc sCdFind;        /* sbss gp-0x7a88: the driver's own lookup */

// PC: the ROM relocates each entry's two offsets to pointers in place,
// which 32 bits cannot hold here: the entries keep their offsets.
static inline const char *spkString(u_int offset)
{
    return (const char *)sSpk + sSpk->strings + offset;
}

// The index is sorted: a binary search, case-blind, past one leading slash.
// Inline three times in the ROM.
static SpkEntry *spkFind(const char *name)
{
    int low = 0;
    int high = (int)sSpk->count;
    const int skip = name[0] == '/' || name[0] == '\\';

    do {
        const int mid = (high + low) >> 1;
        SpkEntry *entry = &sSpk->entries[mid];
        const char *a = spkString(entry->name);
        const char *b = name + skip;
        int order;
        for (;;) {
            const int ca = toupper((u_char)*a++);
            const int cb = toupper((u_char)*b++);
            if (ca == 0 && cb == 0) {
                order = 0;
                break;
            }
            if (ca == 0 || ca < cb) {
                order = -1;
                break;
            }
            if (cb == 0 || ca > cb) {
                order = 1;
                break;
            }
        }
        if (order == 0)
            return entry;
        if (order < 0)
            low = mid + 1;
        else
            high = mid;
    } while (high != low);
    return nullptr;
}

static inline void spkSetLayer(const SpkEntry *entry)
{
    const int layer = (entry->size & kSpkLayer1) != 0 ? 1 : 0;
    if (rxFileCdGetLayerMode() != layer)
        rxFileCdSetLayerMode(layer);
}

// ──────────────────────────────────────────────────────────────────────
// Unhooks the index, frees it and ends rx.
// FS2 0x00152750

void rxTerm()
{
    rxFileCdSetFindFunc(nullptr);
    rxFileSetLoadHook(nullptr);
    rxFileSetSizeHook(nullptr);
    if (sSpk != nullptr)
        gGlobalHeap.free(sSpk);
    sSpk = nullptr;
    rxSysTerm();
}

// ──────────────────────────────────────────────────────────────────────
// Brings rx up: thread priorities relative to the caller, the boot path,
// the system, the heap under it, the CD as the file device and the index.
// FS2 0x001527a0

void rxInit(const char *bootPath, int prio0, int prio1, int prio2)
{
    ThreadParam status;
    ReferThreadStatus(GetThreadId(), &status);
    rxThSetPrioIndex(prio0 - status.currentPriority, prio1 - status.currentPriority,
                     prio2 - status.currentPriority);
    rxSysSetBootPath((char *)bootPath);
    rxSysInit();
    rxSysSetMemFunc0((RxSysMemFunc)gameRxMemAlloc);
    rxSysSetMemFunc1((RxSysMemFunc)gameRxMemAlloc);
    rxSysSetMemFunc2((RxSysMemFunc)gameRxMemRealloc);
    rxSysSetMemFunc3((RxSysMemFunc)gameRxMemFree);
    rxThSetInterval(kRxDispatchInterval);
    rxFileCdSetLayerMode(0);
    rxSysInitFileDevice(RX_SYS_DEVICE_CDROM);
    spkRomLoad();
    rxSysSetDeviceParam(kRxDeviceParam);
}

// ──────────────────────────────────────────────────────────────────────
// Reads SPK.ROM whole into the heap, checks and relocates it once, and
// hooks the CD driver's lookup and rx's size and load.
// FS2 0x00152870

void spkRomLoad()
{
    if (sSpk != nullptr)
        return;
    const int size = rxFileDefaultSizeHook("SPK.ROM");
    if (size <= 0)
        return;

    const int stream = rxSysStreamOpen("SPK.ROM", kRxStreamRead);
    rxFileSync(stream, 0);
    if (stream < 0)
        return;

    sSpk = (SpkHeader *)gGlobalHeap.alloc(size, kSpkAlign);
    rxFileSeek(stream, 0, 0);
    rxFileRead(stream, sSpk, size);
    rxFileClose(stream);
    if (sSpk->magic == kSpkMagic && sSpk->relocated == 0)
        sSpk->relocated = 1;

    sCdFind = rxFileCdSetFindFunc(spkLocateFile);
    rxFileSetLoadHook(spkLoadFile);
    rxFileSetSizeHook(spkGetFileSize);
}

// ──────────────────────────────────────────────────────────────────────
// The CD driver's lookup: 1 with the sector and size, or the driver's
// answer. An indexed file is its archive's sector plus its own.
// FS2 0x001529d0
// callcheck: strlen, toupper (spkFind, inline in the ROM)

int spkLocateFile(const char *name, int *lsn, int *size)
{
    if (strstr(name, "ROM.") != nullptr) {
        if (rxFileCdGetLayerMode() != 0)
            rxFileCdSetLayerMode(0);
        return sCdFind(name, lsn, size);
    }
    if (strstr(name, "ROMD.") != nullptr) {
        if (rxFileCdGetLayerMode() != 1)
            rxFileCdSetLayerMode(1);
        return sCdFind(name, lsn, size);
    }

    const SpkEntry *entry = spkFind(name);
    if (entry == nullptr)
        return kFindNotFound;

    spkSetLayer(entry);
    int archiveLsn;
    int archiveSize;
    const int result = sCdFind(spkString(entry->archive), &archiveLsn, &archiveSize);
    if (result == 1) {
        *lsn = archiveLsn + (int)(entry->offset >> kSectorShift);
        *size = (int)(entry->size & kSpkSizeMask);
    }
    return result;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00152c90
// callcheck: strlen, toupper (spkFind, inline in the ROM)

int spkGetFileSize(const char *name)
{
    const SpkEntry *entry = spkFind(name);
    return entry != nullptr ? (int)(entry->size & kSpkSizeMask) : kNotFound;
}

// ──────────────────────────────────────────────────────────────────────
// Reads a whole file out of its archive into buf: its size, -2 when the
// index has no such file, -1 when the archive does not open.
// FS2 0x00152e20
// callcheck: strlen, toupper, rxFileCdGetLayerMode, rxFileCdSetLayerMode (spkFind and spkSetLayer, inline in the ROM)

int spkLoadFile(const char *name, void *buf)
{
    const SpkEntry *entry = spkFind(name);
    if (entry == nullptr)
        return kNotFound;

    spkSetLayer(entry);
    const int stream = rxSysStreamOpen(spkString(entry->archive), kRxStreamReadNoCache);
    rxFileSync(stream, 0);
    if (stream < 0)
        return -1;

    const int size = (int)(entry->size & kSpkSizeMask);
    rxFileSeek(stream, (int)entry->offset, 0);
    rxFileSync(stream, 0);
    rxFileRead(stream, buf, size);
    rxFileSync(stream, 0);
    rxFileClose(stream);
    rxFileSync(stream, 0);
    return size;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153080

void gameRxMemFree(void *ptr)
{
    gGlobalHeap.free(ptr);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00153090

void *gameRxMemRealloc(void *ptr, u_int size)
{
    return gGlobalHeap.realloc(ptr, size);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001530b0

void *gameRxMemAlloc(u_int size)
{
    return gGlobalHeap.alloc(size, kRxMemAlign);
}

// ──────────────────────────────────────────────────────────────────────
// StreamRing's base slots: an empty ring answers nothing.
// FS2 0x001530d0

int StreamRing::filled()
{
    return 0;
}

// FS2 0x001530e0

int StreamRing::freeSpace()
{
    return 0;
}

// FS2 0x001530f0

void StreamRing::write(const void *src, int size)
{
    (void)src;
    (void)size;
}

// FS2 0x00153100

int StreamRing::readPos()
{
    return 0;
}
