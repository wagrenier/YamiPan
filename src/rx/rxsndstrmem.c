/* ==========================================================================
 *  rx/rxsndstrmem.c
 *
 *  The memory streams (FS2 0x1660e0..0x166b50, bss 0x522a00..0x526a08): the
 *  type-1 sources of rx/rxsndstrstream.c. rxFile's ring calls, on bytes that
 *  are already in EE or IOP memory, so nothing is ever filled: a read
 *  position, markers that stop a read and may jump it (loops), tags a format
 *  asks for. IOP bytes come over a piece at a time, through one scratch
 *  buffer every stream shares.
 *
 *  FS2-only (not in RoR, no symbols): every name was read from the code
 *  (batch 008); the message says "rxsndstr". The marker and tag records are
 *  rxFile's, with their own pools. See rxsndstrmem.h.
 * ======================================================================== */
#include "rx/rxsndstrmem.h"

#include "rx/rxsnd.h"
#include "rx/rxsndut.h"
#include "rx/rxsys.h"
#include "rx/rxthread.h"

#include <stddef.h>

#define RXSNDSTR_MEM_MAX        64
#define RXSNDSTR_MEM_MARKERS    256
#define RXSNDSTR_MEM_TAGS       256
/* The most of IOP memory one read sees: the size of the scratch buffer. */
#define RXSNDSTR_MEM_IOP_CHUNK  0x1000

/* A memory stream (EE 0x20 bytes). layout: host -- holds pointers. */
typedef struct RxSndStrMem
{
    /* FS2 +0x00: four bytes nothing reads or writes. */
    u_char        open;             // FS2 +0x04
    u_char        end;              // FS2 +0x05  the read position reached size
    u_char        iop;              // FS2 +0x06  addr is an IOP address
    uintptr_t     addr;             // FS2 +0x08
    u_int         pos;              // FS2 +0x0c
    u_int         size;             // FS2 +0x10
    RxFileTag    *tags;             // FS2 +0x14  sorted by pos
    RxFileMarker *markers;          // FS2 +0x18  sorted by pos
    void         *arg;              // FS2 +0x1c  the marker callbacks' first argument
} RxSndStrMem;

alignas(64) static u_char s_iopChunk[RXSNDSTR_MEM_IOP_CHUNK];   /* bss 0x522a00 */
static RxSndStrMem   s_streams[RXSNDSTR_MEM_MAX];               /* bss 0x523a00 */
static RxFileMarker  s_markerPool[RXSNDSTR_MEM_MARKERS];        /* bss 0x524200 */
static RxFileMarker *s_markerFree;                              /* bss 0x525a00 */
static RxFileTag     s_tagPool[RXSNDSTR_MEM_TAGS];              /* bss 0x525a04 */
static RxFileTag    *s_tagFree;                                 /* bss 0x526a04 */

/* The check every entry point opens with (inlined in the ROM). */
static inline RxSndStrMem *rxSndStrMemGetOpen(int id)
{
    if ((u_int)id >= RXSNDSTR_MEM_MAX || !s_streams[id].open)
        return NULL;
    return &s_streams[id];
}

// FS2 0x001660e0
void rxSndStrMemInit(void)
{
    for (int i = 0; i < RXSNDSTR_MEM_MAX; i++)
        s_streams[i].open = 0;

    s_markerFree = NULL;
    for (int i = 0; i < RXSNDSTR_MEM_MARKERS; i++)
    {
        s_markerPool[i].next = s_markerFree;
        s_markerFree         = &s_markerPool[i];
    }

    s_tagFree = NULL;
    for (int i = 0; i < RXSNDSTR_MEM_TAGS; i++)
    {
        s_tagPool[i].next = s_tagFree;
        s_tagFree         = &s_tagPool[i];
    }
}

// FS2 0x00166178
void rxSndStrMemExit(void)
{
    for (int i = 0; i < RXSNDSTR_MEM_MAX; i++)
        rxSndStrMemClose(i);
}

// FS2 0x001661b8
void rxSndStrMemUpdate(void)
{
}

// The search starts at 1: stream 0 is never given out. Dispatch is back on
// before the slot is taken, as in the ROM (rxSndStrStreamOpen holds it off).
// FS2 0x001661c0
int rxSndStrMemOpen(uintptr_t addr, int size, int iop)
{
    int id = 1;

    rxThDisableDispatch();
    while (id < RXSNDSTR_MEM_MAX && s_streams[id].open)
        id++;
    rxThEnableDispatch();

    if (id == RXSNDSTR_MEM_MAX)
    {
        if (rxSndUtGetDebugFlags() & 1)
            rxSysPrintf("rxsndstr : empty memory stream slot\n");
        return -1;
    }

    RxSndStrMem *stream = &s_streams[id];
    stream->iop     = (u_char)iop;
    stream->addr    = addr;
    stream->size    = (u_int)size;
    stream->arg     = NULL;
    stream->end     = 0;
    stream->pos     = 0;
    stream->tags    = NULL;
    stream->markers = NULL;
    stream->open    = 1;
    return id;
}

// FS2 0x001662b0
void rxSndStrMemClose(int id)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    if (stream == NULL)
        return;
    rxSndStrMemSetMarkers(id, NULL, 0, NULL);
    rxSndStrMemSetTags(id, NULL, 0);
    stream->open = 0;
}

// ROM quirk kept: a seek back does not clear the end flag, so a stream that
// ended stays ended. Past the end it stops at the end.
// FS2 0x00166320
void rxSndStrMemSeek(int id, u_int pos)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    if (stream == NULL)
        return;
    stream->pos = pos < stream->size ? pos : stream->size;
    if (stream->pos == stream->size)
        stream->end = 1;
}

// One read goes up to the next marker past the position, or to the end, and
// no further than the scratch buffer for IOP memory. Disabled markers bound
// it too. -1 when the stream is not open.
// FS2 0x00166370
int rxSndStrMemGetReadable(int id)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    if (stream == NULL)
        return -1;

    u_int limit = stream->size;
    for (RxFileMarker *m = stream->markers; m != NULL; m = m->next)
    {
        if (m->def.pos > stream->pos)
        {
            limit = m->def.pos;
            break;
        }
    }

    int readable = (int)(limit - stream->pos);
    if (stream->iop && readable > RXSNDSTR_MEM_IOP_CHUNK)
        readable = RXSNDSTR_MEM_IOP_CHUNK;
    return readable;
}

// Move on by size (no more than one read gives), then run the first marker
// standing exactly there that stops: it jumps the position, -1 to the end.
// (The ROM asks GetReadable again for each side of the comparison, a MIN /
// MAX macro's work; here once.)
// FS2 0x00166418
void rxSndStrMemConsume(int id, int size)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    if (stream == NULL || stream->end == 1)
        return;

    int readable = rxSndStrMemGetReadable(id);
    int taken    = size < readable ? size : readable;
    if (taken < 0)
        taken = 0;

    u_int pos   = stream->pos + (u_int)taken;
    stream->pos = pos;

    for (RxFileMarker *m = stream->markers; m != NULL; m = m->next)
    {
        RxFileMarkerDef *def = &m->def;
        if (pos != def->pos || def->active != 1)
            continue;
        if (def->max != 0 && def->count++ >= def->max)
            continue;
        if (def->callback != NULL && def->callback(stream->arg, def) != 1)
        {
            pos = stream->pos;      /* the callback may have seeked */
            continue;
        }
        pos         = (u_int)def->jumpPos < stream->size ? (u_int)def->jumpPos : stream->size;
        stream->pos = pos;
        break;
    }

    if (pos == stream->size)
        stream->end = 1;
}

// EE bytes are read in place. IOP bytes are fetched here, the whole of one
// read, so the pointer is good until any stream's next call.
// FS2 0x001665b8
const u_char *rxSndStrMemGetData(int id)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    if (stream == NULL)
        return NULL;
    if (!stream->iop)
        return (const u_char *)(stream->addr + stream->pos);
    rxSndIopRead(s_iopChunk, (u_int)stream->addr + stream->pos, rxSndStrMemGetReadable(id));
    return s_iopChunk;
}

// FS2 0x00166650
int rxSndStrMemIsEnd(int id)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    return stream != NULL ? stream->end : 0;
}

// Replaces the stream's markers. Sorted by pos, equal ones in the order
// given; what the pool cannot hold is dropped. How many were taken.
// FS2 0x00166688
int rxSndStrMemSetMarkers(int id, const RxFileMarkerDef *defs, int count, void *arg)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    if (stream == NULL)
        return 0;

    rxThDisableDispatch();
    for (RxFileMarker *m = stream->markers; m != NULL; m = stream->markers)
    {
        stream->markers = m->next;
        m->next         = s_markerFree;
        s_markerFree    = m;
    }

    int taken = 0;
    for (; taken < count && s_markerFree != NULL; taken++)
    {
        RxFileMarker *node = s_markerFree;
        s_markerFree = node->next;

        RxFileMarker **link = &stream->markers;
        while (*link != NULL && (*link)->def.pos <= defs[taken].pos)
            link = &(*link)->next;
        node->next = *link;
        *link      = node;
        node->def  = defs[taken];
    }
    rxThEnableDispatch();

    stream->arg = arg;
    return taken;
}

// Nothing in the ROM calls it.
// FS2 0x00166828
void rxSndStrMemSetMarkerEnable(int id, int markerId, int active)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    if (stream == NULL)
        return;
    for (RxFileMarker *m = stream->markers; m != NULL; m = m->next)
    {
        if (m->def.id == markerId)
            m->def.active = (u_char)active;
    }
}

// Replaces the stream's tags, as SetMarkers does its markers.
// FS2 0x00166890
int rxSndStrMemSetTags(int id, const RxFileTagDef *defs, int count)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    if (stream == NULL)
        return 0;

    rxThDisableDispatch();
    for (RxFileTag *t = stream->tags; t != NULL; t = stream->tags)
    {
        stream->tags = t->next;
        t->next      = s_tagFree;
        s_tagFree    = t;
    }

    int taken = 0;
    for (; taken < count && s_tagFree != NULL; taken++)
    {
        RxFileTag *node = s_tagFree;
        s_tagFree = node->next;

        RxFileTag **link = &stream->tags;
        while (*link != NULL && (*link)->def.pos <= defs[taken].pos)
            link = &(*link)->next;
        node->next = *link;
        *link      = node;
        node->def  = defs[taken];
    }
    rxThEnableDispatch();
    return taken;
}

// Nothing in the ROM calls it.
// FS2 0x00166a10
void rxSndStrMemSetTagEnable(int id, int tagId, int active)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    if (stream == NULL)
        return;
    for (RxFileTag *t = stream->tags; t != NULL; t = t->next)
    {
        if (t->def.id == tagId)
            t->def.active = (u_char)active;
    }
}

// The enabled tags in the size bytes ahead, nearest first: at most max go to
// out, and the count of all of them comes back, as rxFileGetRingTags does.
// FS2 0x00166a78
int rxSndStrMemGetTags(int id, RxFileTagHit *out, int max, int size)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    if (stream == NULL)
        return 0;

    int found = 0;
    for (RxFileTag *t = stream->tags; t != NULL; t = t->next)
    {
        if (t->def.active != 1 || t->def.pos < stream->pos
            || t->def.pos >= stream->pos + (u_int)size)
            continue;
        if (found < max)
        {
            out[found].offset = (int)(t->def.pos - stream->pos);
            out[found].value  = t->def.value;
        }
        found++;
    }
    return found;
}

// FS2 0x00166b18
int rxSndStrMemGetSize(int id)
{
    RxSndStrMem *stream = rxSndStrMemGetOpen(id);
    return stream != NULL ? (int)stream->size : 0;
}
