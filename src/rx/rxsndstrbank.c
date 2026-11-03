/* ==========================================================================
 *  rx/rxsndstrbank.c
 *
 *  XWS banks (FS2 0x1622f8..0x1634b8, bss 0x514780..0x51ef80): 256 slots,
 *  each a view of an RXWS file the caller keeps in EE memory. Open finds
 *  the file's chunks once; the getters walk them on every call (FORM
 *  records are variable length, so a request is found by stepping over the
 *  ones before it). The body -- the sample data -- is attached separately
 *  and opened as a stream source by the formats (rxSndStrBankOpenBody).
 *  See rxsndstrbank.h.
 *
 *  FS2-only (not in RoR, no symbols): every name was read from the code
 *  (batch 008); the messages say "rxsndstr". The rxSndStrBankGet* request,
 *  mark and branch getters sit on lower-level rxSndStrForm* / rxSndStrText*
 *  / rxSndStrMark* / rxSndStrBranch* readers kept at the end of the file,
 *  as in the ROM.
 * ======================================================================== */
#include "rx/rxsndstrbank.h"

#include "rx/rxsndstr.h"
#include "rx/rxsndstrstream.h"
#include "rx/rxsndut.h"
#include "rx/rxsys.h"
#include "rx/rxthread.h"

#include <stddef.h>
#include <string.h>

#define RXSNDSTR_BANK_PATH_MAX  0x40    /* the slot's search dir and file name */

/* A bank slot (bss 0x514780, 256 x 0xa8 on the EE). The chunk pointers
 * point at each chunk's data, null when the file lacks it.
 * layout: host -- holds pointers. */
typedef struct RxSndStrBankSlot
{
    u_char                   used;                          // FS2 +0x00
    u_char                   owner;                         // FS2 +0x01: rxSndStrBankOpen's first argument, unread
    u_char                   bodyType;                      // FS2 +0x02: RxSndStrBankBody
    const RxSndStrXwsHeader *header;                        // FS2 +0x04
    const int               *form;                          // FS2 +0x08: FORM: count, then the records
    const RxSndStrXwsIndex  *text;                          // FS2 +0x0c: FTXT
    const char              *mark;                          // FS2 +0x10: MARK: +4 an index of per-request lists
    const char              *branch;                        // FS2 +0x14: BRCH: as MARK
    const RxSndStrXwsIndex  *track;                         // FS2 +0x18: TRCK: lists, each with an index at +4
    uintptr_t                bodyAddr;                      // FS2 +0x1c: EE pointer or IOP address
    int                      bodySize;                      // FS2 +0x20
    int                      device;                        // FS2 +0x24: a file body's rxSys file device, -1
    char                     dir[RXSNDSTR_BANK_PATH_MAX];   // FS2 +0x28: a file body's search dir
    char                     name[RXSNDSTR_BANK_PATH_MAX];  // FS2 +0x68: a file body's name
} RxSndStrBankSlot;

/* ---- data ------------------------------------------------------------- */

static RxSndStrBankSlot s_banks[RXSNDSTR_BANK_MAX];        /* bss 0x514780 */

/* ---- this file's own readers (defined below, in ROM order) ------------- */

static RxSndStrBankSlot       *rxSndStrBankGetSlot(int bank);
static const char             *rxSndStrBankGetTrackTable(int bank, int request);
static const RxSndStrXwsChunk *rxSndStrXwsFindChunk(const void *header, u_int tag);
static const void             *rxSndStrXwsGetChunkData(const void *header, u_int tag);
static int                     rxSndStrFormGetCount(int bank);
static const RxSndStrRequest  *rxSndStrFormGet(int bank, int request);
static const char             *rxSndStrTextGet(int bank, int request);
static int                     rxSndStrMarkGetCount(int bank, int request);
static const RxSndStrMark     *rxSndStrMarkGet(int bank, int request, int mark);
static int                     rxSndStrBranchGetCount(int bank, int request);
static const RxSndStrMark     *rxSndStrBranchGet(int bank, int request, int branch);

/* The bank's slot, null for a bad id or a free slot (inline in most calls). */
static RxSndStrBankSlot *rxSndStrBankGetOpenSlot(int bank)
{
    if ((u_int)bank >= RXSNDSTR_BANK_MAX)
        return NULL;
    RxSndStrBankSlot *slot = &s_banks[bank];
    if (!slot->used)
        return NULL;
    return slot;
}

/* The list at index entry i, or null when i is out of range. */
static const RxSndStrXwsIndex *rxSndStrXwsGetList(const char *base, const RxSndStrXwsIndex *index, int i)
{
    if (i < 0 || i > index->count - 1)
        return NULL;
    return (const RxSndStrXwsIndex *)(base + index->offsets[i]);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001622f8

int rxSndStrBankOpen(int owner, const void *header)
{
    const RxSndStrXwsHeader *xws = (const RxSndStrXwsHeader *)header;
    int bank;

    rxThDisableDispatch();
    if (xws->magic != RXSNDSTR_XWS_MAGIC) {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR) {
            rxSysPrintf("rxsndstr : specified file is not XWS format file '%c%c%c%c'\n",
                        xws->magic & 0xff, ((const u_char *)xws)[1], ((const u_char *)xws)[2],
                        xws->magic >> 24);
        }
        bank = -1;
    } else if (xws->version < RXSNDSTR_XWS_VERSION_MIN) {
        if (rxSndUtGetDebugFlags() & RXSNDUT_DEBUG_ERROR)
            rxSysPrintf("rxsndstr : specified file is low version XWS format %X\n", xws->version);
        bank = -1;
    } else {
        for (bank = 0; bank < RXSNDSTR_BANK_MAX; bank++) {
            if (!s_banks[bank].used)
                break;
        }
        if (bank == RXSNDSTR_BANK_MAX) {
            bank = -1;
        } else {
            RxSndStrBankSlot *slot = &s_banks[bank];
            slot->used = 1;
            slot->owner = (u_char)owner;
            slot->header = xws;
            slot->form = (const int *)rxSndStrXwsGetChunkData(xws, RXSNDSTR_XWS_FORM);
            slot->text = (const RxSndStrXwsIndex *)rxSndStrXwsGetChunkData(xws, RXSNDSTR_XWS_FTXT);
            slot->mark = (const char *)rxSndStrXwsGetChunkData(xws, RXSNDSTR_XWS_MARK);
            slot->branch = (const char *)rxSndStrXwsGetChunkData(xws, RXSNDSTR_XWS_BRCH);
            slot->track = (const RxSndStrXwsIndex *)rxSndStrXwsGetChunkData(xws, RXSNDSTR_XWS_TRCK);
            slot->name[0] = '\0';
            slot->bodyType = RxSndStrBankBody_None;
            slot->device = -1;
            slot->bodyAddr = 0;
            slot->bodySize = 0;
            slot->dir[0] = '\0';
            const RxSndStrXwsChunk *body = rxSndStrXwsFindChunk(xws, RXSNDSTR_XWS_BODY);
            if (body != NULL)
                rxSndStrBankAttachBodyMemory(bank, body + 1, body->size);
        }
    }
    rxThEnableDispatch();
    return bank;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001624d8

void rxSndStrBankClose(int bank)
{
    rxThDisableDispatch();
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot != NULL)
        slot->used = 0;
    rxThEnableDispatch();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162530

void rxSndStrBankAttachBodyFile(int bank, const char *name)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL)
        return;
    slot->bodyType = RxSndStrBankBody_File;
    slot->device = rxSysGetFileDevice();
    strcpy(slot->dir, rxSysGetFileSearchDir());
    strcpy(slot->name, name);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001625c8

void rxSndStrBankAttachBodyMemory(int bank, const void *body, int size)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL)
        return;
    slot->bodySize = size;
    slot->bodyAddr = (uintptr_t)body;
    slot->bodyType = RxSndStrBankBody_Memory;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162610

void rxSndStrBankAttachBodyIop(int bank, u_int iopAddr, int size)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL)
        return;
    slot->bodySize = size;
    slot->bodyAddr = iopAddr;
    slot->bodyType = RxSndStrBankBody_Iop;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162658

const void *rxSndStrBankGetData(int bank)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL)
        return NULL;
    return slot->header;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001626a0

int rxSndStrBankGetBodyType(int bank)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL)
        return -1;
    return slot->bodyType;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001626e8

uintptr_t rxSndStrBankGetBodyAddr(int bank)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL)
        return 0;
    return slot->bodyAddr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162730

void rxSndStrBankGetBodyFile(int bank, int *device, char *dir, char *name)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL)
        return;
    if (device != NULL)
        *device = slot->device;
    if (dir != NULL)
        strcpy(dir, slot->dir);
    if (name != NULL)
        strcpy(name, slot->name);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001627c0

int rxSndStrBankGetRequestCount(int bank)
{
    return rxSndStrFormGetCount(bank);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001627d8

const char *rxSndStrBankGetRequestName(int bank, int request)
{
    return rxSndStrTextGet(bank, request);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001627f0

int rxSndStrBankGetRequestFormat(int bank, int request)
{
    const RxSndStrRequest *req = rxSndStrBankGetRequest(bank, request);
    return req != NULL ? req->format : -1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162820

int rxSndStrBankGetRequestRate(int bank, int request)
{
    const RxSndStrRequest *req = rxSndStrBankGetRequest(bank, request);
    return req != NULL ? req->rate : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162850

int rxSndStrBankGetRequestChannels(int bank, int request)
{
    const RxSndStrRequest *req = rxSndStrBankGetRequest(bank, request);
    return req != NULL ? req->channels : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162880

u_int rxSndStrBankGetRequestOffset(int bank, int request)
{
    const RxSndStrRequest *req = rxSndStrBankGetRequest(bank, request);
    return req != NULL ? req->offset : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001628b0

int rxSndStrBankGetRequestSize(int bank, int request)
{
    const RxSndStrRequest *req = rxSndStrBankGetRequest(bank, request);
    return req != NULL ? (int)req->size : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001628e0

u_int rxSndStrBankGetRequestLoopOffset(int bank, int request)
{
    const RxSndStrRequest *req = rxSndStrBankGetRequest(bank, request);
    return req != NULL ? req->loopOffset : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162910

int rxSndStrBankGetMarkCount(int bank, int request)
{
    return rxSndStrMarkGetCount(bank, request);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162928

const char *rxSndStrBankGetMarkName(int bank, int request, int mark)
{
    const RxSndStrMark *rec = rxSndStrMarkGet(bank, request, mark);
    return rec != NULL ? rec->name : "";
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162958

u_int rxSndStrBankGetMarkOffset(int bank, int request, int mark)
{
    const RxSndStrMark *rec = rxSndStrMarkGet(bank, request, mark);
    return rec != NULL ? rec->offset : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162988

int rxSndStrBankGetMarkParam(int bank, int request, int mark)
{
    const RxSndStrMark *rec = rxSndStrMarkGet(bank, request, mark);
    return rec != NULL ? rec->param : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001629b8

int rxSndStrBankGetBranchCount(int bank, int request)
{
    return rxSndStrBranchGetCount(bank, request);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001629d0

const char *rxSndStrBankGetBranchName(int bank, int request, int branch)
{
    const RxSndStrMark *rec = rxSndStrBranchGet(bank, request, branch);
    return rec != NULL ? rec->name : "";
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162a00

int rxSndStrBankFindRequest(int bank, const char *name)
{
    if (rxSndStrBankGetOpenSlot(bank) == NULL || name == NULL)
        return -1;
    int count = rxSndStrBankGetRequestCount(bank);
    for (int i = 0; i < count; i++) {
        if (strcmp(rxSndStrBankGetRequestName(bank, i), name) == 0)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162ac0

static RxSndStrBankSlot *rxSndStrBankGetSlot(int bank)
{
    return rxSndStrBankGetOpenSlot(bank);
}

// ──────────────────────────────────────────────────────────────────────
// The request's track list (count at +4, offsets from TRCK's data at +8),
// null when the bank has no TRCK or the request is out of range.
// FS2 0x00162b00

static const char *rxSndStrBankGetTrackTable(int bank, int request)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetSlot(bank);
    if (slot == NULL || slot->track == NULL || request < 0)
        return NULL;
    if (request > slot->track->count - 1)
        return NULL;
    int offset = slot->track->offsets[request];
    if (offset == 0)
        return NULL;
    return (const char *)slot->track + offset;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162b70

const RxSndStrTrack *rxSndStrBankGetTrack(int bank, int request, int track)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetSlot(bank);
    if (slot == NULL)
        return NULL;
    const char *table = rxSndStrBankGetTrackTable(bank, request);
    if (table == NULL)
        return NULL;
    const RxSndStrXwsIndex *index = (const RxSndStrXwsIndex *)(table + 4);
    if (track < 0 || track > index->count - 1)
        return NULL;
    int offset = index->offsets[track];
    if (offset == 0)
        return NULL;
    return (const RxSndStrTrack *)((const char *)slot->track + offset);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162c20

int rxSndStrBankGetTrackCount(int bank, int request)
{
    const char *table = rxSndStrBankGetTrackTable(bank, request);
    return table != NULL ? ((const RxSndStrXwsIndex *)(table + 4))->count : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162c48

int rxSndStrBankGetTrackChannels(int bank, int request, int track)
{
    const RxSndStrTrack *rec = rxSndStrBankGetTrack(bank, request, track);
    return rec != NULL ? rec->channels : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162c70

const char *rxSndStrBankGetTrackName(int bank, int request, int track)
{
    const RxSndStrTrack *rec = rxSndStrBankGetTrack(bank, request, track);
    return rec != NULL ? rec->name : NULL;
}

// ──────────────────────────────────────────────────────────────────────
// 0, not -1, for an unknown bank or request, as in the ROM.
// FS2 0x00162ca0

int rxSndStrBankFindTrack(int bank, int request, const char *name)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetSlot(bank);
    if (slot == NULL)
        return 0;
    const char *table = rxSndStrBankGetTrackTable(bank, request);
    if (table == NULL)
        return 0;
    const RxSndStrXwsIndex *index = (const RxSndStrXwsIndex *)(table + 4);
    for (int i = 0; i < index->count; i++) {
        const RxSndStrTrack *rec = (const RxSndStrTrack *)((const char *)slot->track + index->offsets[i]);
        if (strcmp(rec->name, name) == 0)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162d60

void rxSndStrBankGetTrackVolume(int bank, int request, int track, int *volL, int *volR)
{
    if (volL != NULL)
        *volL = RXSNDSTR_VOLUME_UNITY;
    if (volR != NULL)
        *volR = RXSNDSTR_VOLUME_UNITY;
    const RxSndStrTrack *rec = rxSndStrBankGetTrack(bank, request, track);
    if (rec == NULL)
        return;
    if (volL != NULL)
        *volL = rec->volL;
    if (volR != NULL)
        *volR = rec->volR;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162dd8

void rxSndStrBankInit(void)
{
    for (int i = RXSNDSTR_BANK_MAX - 1; i >= 0; i--)
        s_banks[i].used = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162e18

void rxSndStrBankExit(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162e20

void rxSndStrBankUpdate(void)
{
}

// ──────────────────────────────────────────────────────────────────────
// A file body opens under the device and search dir it was attached with,
// switched only when they differ and restored after.
// FS2 0x00162e28

int rxSndStrBankOpenBody(int bank, void *buf, int bufSize)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL)
        return -1;
    switch (slot->bodyType) {
    case RxSndStrBankBody_Memory:
    case RxSndStrBankBody_Iop:
        return rxSndStrStreamOpenMemory(slot->bodyAddr, slot->bodySize,
                                        slot->bodyType == RxSndStrBankBody_Iop);
    case RxSndStrBankBody_File: {
        int prevDevice = -1;
        if (rxSysGetFileDevice() != slot->device)
            prevDevice = rxSysInitFileDevice(slot->device);
        int pushed = 0;
        if (strcmp(rxSysGetFileSearchDir(), slot->dir) != 0) {
            rxSysPushSearchDir();
            rxSysSetFileSearchDir(NULL);
            rxSysSetFileSearchDir(slot->dir);
            pushed = 1;
        }
        int source = rxSndStrStreamOpenFile(slot->name, buf, bufSize);
        if (pushed)
            rxSysPopSearchDir();
        if (prevDevice >= 0)
            rxSysInitFileDevice(prevDevice);
        return source;
    }
    default:
        /* The ROM has a "rxsndstr : stream body has never attached '%d'\n"
         * print under a case -1 that the unsigned type byte never matches
         * (RxSndStrBankBody_None is 0xff): dead in the game, left out.
         * callcheck: rxSndUtGetDebugFlags, rxSysPrintf (that dead print) */
        return -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00162fd8

const RxSndStrRequest *rxSndStrBankGetRequest(int bank, int request)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL || slot->form == NULL || request < 0 || request > slot->form[0] - 1)
        return NULL;
    const u_char *rec = (const u_char *)(slot->form + 1);
    for (int i = 0; i < request; i++)
        rec += ((const RxSndStrRequest *)rec)->length;
    return (const RxSndStrRequest *)rec;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163070

const RxSndStrMark *rxSndStrBankGetMark(int bank, int request, int mark)
{
    return rxSndStrMarkGet(bank, request, mark);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163088

const RxSndStrMark *rxSndStrBankGetBranch(int bank, int request, int branch)
{
    return rxSndStrBranchGet(bank, request, branch);
}

// ──────────────────────────────────────────────────────────────────────
// The chunk with that tag, null none.
// FS2 0x001630a0

static const RxSndStrXwsChunk *rxSndStrXwsFindChunk(const void *header, u_int tag)
{
    const RxSndStrXwsHeader *xws = (const RxSndStrXwsHeader *)header;
    const char *chunks = (const char *)(xws + 1);
    for (int at = 0; at < xws->size; ) {
        const RxSndStrXwsChunk *chunk = (const RxSndStrXwsChunk *)(chunks + at);
        if (chunk->tag == tag)
            return chunk;
        at += chunk->size + (int)sizeof(RxSndStrXwsChunk);
    }
    return NULL;
}

// ──────────────────────────────────────────────────────────────────────
// The data of the chunk with that tag, null none.
// FS2 0x001630e8

static const void *rxSndStrXwsGetChunkData(const void *header, u_int tag)
{
    const RxSndStrXwsChunk *chunk = rxSndStrXwsFindChunk(header, tag);
    return chunk != NULL ? chunk + 1 : NULL;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163118

static int rxSndStrFormGetCount(int bank)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL || slot->form == NULL)
        return 0;
    return slot->form[0];
}

// ──────────────────────────────────────────────────────────────────────
// rxSndStrBankGetRequest's twin (the same code, nobody calls it).
// FS2 0x00163168

static const RxSndStrRequest *rxSndStrFormGet(int bank, int request)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL || slot->form == NULL || request < 0 || request > slot->form[0] - 1)
        return NULL;
    const u_char *rec = (const u_char *)(slot->form + 1);
    for (int i = 0; i < request; i++)
        rec += ((const RxSndStrRequest *)rec)->length;
    return (const RxSndStrRequest *)rec;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163200

static const char *rxSndStrTextGet(int bank, int request)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL || slot->text == NULL)
        return NULL;
    if (request < 0 || request > slot->text->count - 1)
        return NULL;
    return (const char *)slot->text + slot->text->offsets[request];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163278

static int rxSndStrMarkGetCount(int bank, int request)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL || slot->mark == NULL)
        return 0;
    const RxSndStrXwsIndex *list = rxSndStrXwsGetList(slot->mark, (const RxSndStrXwsIndex *)(slot->mark + 4), request);
    return list != NULL ? list->count : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001632f8

static const RxSndStrMark *rxSndStrMarkGet(int bank, int request, int mark)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL || slot->mark == NULL)
        return NULL;
    const RxSndStrXwsIndex *list = rxSndStrXwsGetList(slot->mark, (const RxSndStrXwsIndex *)(slot->mark + 4), request);
    if (list == NULL)
        return NULL;
    return (const RxSndStrMark *)rxSndStrXwsGetList(slot->mark, list, mark);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163398

static int rxSndStrBranchGetCount(int bank, int request)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL || slot->branch == NULL)
        return 0;
    const RxSndStrXwsIndex *list = rxSndStrXwsGetList(slot->branch, (const RxSndStrXwsIndex *)(slot->branch + 4), request);
    return list != NULL ? list->count : 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00163418

static const RxSndStrMark *rxSndStrBranchGet(int bank, int request, int branch)
{
    RxSndStrBankSlot *slot = rxSndStrBankGetOpenSlot(bank);
    if (slot == NULL || slot->branch == NULL)
        return NULL;
    const RxSndStrXwsIndex *list = rxSndStrXwsGetList(slot->branch, (const RxSndStrXwsIndex *)(slot->branch + 4), request);
    if (list == NULL)
        return NULL;
    return (const RxSndStrMark *)rxSndStrXwsGetList(slot->branch, list, branch);
}
