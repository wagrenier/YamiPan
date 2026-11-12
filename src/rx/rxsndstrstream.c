/* ==========================================================================
 *  rx/rxsndstrstream.c
 *
 *  The stream sources (FS2 0x1656e8..0x1660e0, bss 0x5226e0): 64 ids that a
 *  stream format reads its bytes through, without knowing where they come
 *  from. A source is an rxFile ring (the bank's body is a file) or a memory
 *  stream (rx/rxsndstrmem.c, the body is in EE or IOP memory); every call
 *  here checks the id and passes on to one or the other.
 *
 *  FS2-only (not in RoR, no symbols): every name was read from the code
 *  (batch 008); the message says "rxsndstr". See rxsndstrstream.h.
 * ======================================================================== */
#include "rx/rxsndstrstream.h"

#include "rx/rxfile.h"
#include "rx/rxsndstrmem.h"
#include "rx/rxsndut.h"
#include "rx/rxsys.h"
#include "rx/rxthread.h"

#include <stddef.h>

#define RXSNDSTR_SOURCE_MAX     64

/* rxFileGetState: the read position has reached the file's end. */
#define RXFILE_STATE_AT_END     0x08

typedef enum RxSndStrSourceType
{
    RxSndStrSource_File   = 0,      /* an rxFile ring */
    RxSndStrSource_Memory = 1,      /* an rxSndStrMem stream */
} RxSndStrSourceType;

/* A source (EE 0xc bytes). layout: host -- holds a pointer. */
typedef struct RxSndStrSource RxSndStrSource;
struct RxSndStrSource
{
    RxSndStrSource *next;           // FS2 +0x00
    u_char          id;             // FS2 +0x04
    u_char          open;           // FS2 +0x05
    u_char          type;           // FS2 +0x06  RxSndStrSourceType
    int             handle;         // FS2 +0x08  rxFile fd or rxSndStrMem id
};

static RxSndStrSource  s_sources[RXSNDSTR_SOURCE_MAX];  /* bss 0x5226e0 */
static RxSndStrSource *s_active;                        /* bss 0x5229e0 */
static RxSndStrSource *s_free;                          /* bss 0x5229e4 */

/* The check every entry point opens with (inlined in the ROM). */
static inline RxSndStrSource *rxSndStrStreamGetOpen(int source)
{
    if ((u_int)source >= RXSNDSTR_SOURCE_MAX || !s_sources[source].open)
        return NULL;
    return &s_sources[source];
}

// FS2 0x001656e8
void rxSndStrStreamInit(void)
{
    s_active = NULL;
    s_free   = NULL;
    for (int i = 0; i < RXSNDSTR_SOURCE_MAX; i++)
    {
        s_sources[i].next = s_free;
        s_free            = &s_sources[i];
        s_sources[i].id   = (u_char)i;
        s_sources[i].open = 0;
    }
}

// FS2 0x00165738
void rxSndStrStreamExit(void)
{
    while (s_active != NULL)
        rxSndStrStreamClose(s_active->id, 0);
}

// FS2 0x00165788
void rxSndStrStreamUpdate(void)
{
}

// FS2 0x00165790
int rxSndStrStreamOpenFile(const char *name, void *buf, int bufSize)
{
    return rxSndStrStreamOpen(RxSndStrSource_File, name, (uintptr_t)buf, bufSize, 0);
}

// FS2 0x001657c0
int rxSndStrStreamOpenMemory(uintptr_t addr, int size, int iop)
{
    return rxSndStrStreamOpen(RxSndStrSource_Memory, NULL, addr, size, iop);
}

// A failed open leaves the source's open byte set, on the free list, as the
// ROM does: the next open takes the same source and sets it again.
// FS2 0x001657f0
int rxSndStrStreamOpen(int type, const char *name, uintptr_t addr, int size, int iop)
{
    int result = -1;

    rxThDisableDispatch();
    RxSndStrSource *source = s_free;
    if (source == NULL)
    {
        if (rxSndUtGetDebugFlags() & 1)
            rxSysPrintf("rxsndstr : empty file stream slot\n");
    }
    else
    {
        int opened = 1;

        source->type = (u_char)type;
        source->open = 1;
        if (type == RxSndStrSource_File)
        {
            /* The ring asks for more once half of it or less is left. */
            source->handle = rxSysStreamOpenRing(name, (void *)addr, size, (size / 2) & ~0xf);
            opened         = source->handle >= 0;
        }
        else if (type == RxSndStrSource_Memory)
        {
            source->handle = rxSndStrMemOpen(addr, size, iop);
            opened         = source->handle >= 0;
        }

        if (opened)
        {
            s_free       = source->next;
            source->next = s_active;
            s_active     = source;
            result       = source->id;
        }
    }
    rxThEnableDispatch();
    return result;
}

// FS2 0x00165938
void rxSndStrStreamClose(int source, int abort)
{
    rxThDisableDispatch();
    RxSndStrSource **link  = &s_active;
    RxSndStrSource  *found = s_active;
    while (found != NULL)
    {
        if (found->id == (u_int)source)
        {
            *link       = found->next;
            found->open = 0;
            break;
        }
        link  = &found->next;
        found = found->next;
    }
    rxThEnableDispatch();
    if (found == NULL)
        return;

    if (found->type == RxSndStrSource_File)
    {
        if (abort)
            rxFileAbort(found->handle);
        else
            rxFileClose(found->handle);
    }
    else if (found->type == RxSndStrSource_Memory)
    {
        rxSndStrMemClose(found->handle);
    }

    rxThDisableDispatch();
    found->next = s_free;
    s_free      = found;
    rxThEnableDispatch();
}

// The file's read goes on from the new position, also after a pause.
// FS2 0x00165a38
void rxSndStrStreamSeek(int source, u_int pos)
{
    RxSndStrSource *open = rxSndStrStreamGetOpen(source);
    if (open == NULL)
        return;
    if (open->type == RxSndStrSource_File)
    {
        rxFileSeek(open->handle, (int)pos, 0);
        rxFilePause(open->handle, 0);
    }
    else if (open->type == RxSndStrSource_Memory)
    {
        rxSndStrMemSeek(open->handle, pos);
    }
}

// FS2 0x00165ac8
void rxSndStrStreamSetMarkers(int source, const RxFileMarkerDef *defs, int count, void *arg)
{
    RxSndStrSource *open = rxSndStrStreamGetOpen(source);
    if (open == NULL)
        return;
    if (open->type != RxSndStrSource_Memory)
        rxFileSetMarkers(open->handle, defs, count, arg);
    else
        rxSndStrMemSetMarkers(open->handle, defs, count, arg);
}

// FS2 0x00165b38
void rxSndStrStreamSetTags(int source, const RxFileTagDef *defs, int count)
{
    RxSndStrSource *open = rxSndStrStreamGetOpen(source);
    if (open == NULL)
        return;
    if (open->type != RxSndStrSource_Memory)
        rxFileSetTags(open->handle, defs, count);
    else
        rxSndStrMemSetTags(open->handle, defs, count);
}

// FS2 0x00165ba8
int rxSndStrStreamGetRingTags(int source, RxFileTagHit *out, int max, int size)
{
    RxSndStrSource *open = rxSndStrStreamGetOpen(source);
    if (open == NULL)
        return 0;
    if (open->type != RxSndStrSource_Memory)
        return rxFileGetRingTags(open->handle, out, max, size);
    return rxSndStrMemGetTags(open->handle, out, max, size);
}

/* size, no more than limit, not below 0. */
static inline int rxSndStrStreamClamp(int size, int limit)
{
    int value = size < limit ? size : limit;
    return value < 0 ? 0 : value;
}

// The ring is read in one piece only up to its end. (The ROM asks rxFile again
// for each side of each comparison, a MIN / MAX macro's work; here once.)
// FS2 0x00165c20
int rxSndStrStreamGetReadable(int source, int size)
{
    RxSndStrSource *open = rxSndStrStreamGetOpen(source);
    if (open == NULL)
        return 0;
    if (open->type == RxSndStrSource_Memory)
        return rxSndStrStreamClamp(size, rxSndStrMemGetReadable(open->handle));

    size = rxSndStrStreamClamp(size, rxFileGetRingFilled(open->handle));
    return rxSndStrStreamClamp(size, rxFileGetRingSize(open->handle)
                                         - rxFileGetRingReadPos(open->handle));
}

// FS2 0x00165df8
int rxSndStrStreamGetFilled(int source)
{
    RxSndStrSource *open = rxSndStrStreamGetOpen(source);
    if (open == NULL)
        return 0;
    if (open->type != RxSndStrSource_Memory)
        return rxFileGetRingFilled(open->handle);
    return rxSndStrMemGetSize(open->handle);
}

// FS2 0x00165e70
void rxSndStrStreamConsume(int source, int size)
{
    RxSndStrSource *open = rxSndStrStreamGetOpen(source);
    if (open == NULL)
        return;
    if (open->type != RxSndStrSource_Memory)
        rxFileRingConsume(open->handle, size);
    else
        rxSndStrMemConsume(open->handle, size);
}

// FS2 0x00165ee0
const u_char *rxSndStrStreamGetData(int source)
{
    RxSndStrSource *open = rxSndStrStreamGetOpen(source);
    if (open == NULL)
        return NULL;
    if (open->type != RxSndStrSource_Memory)
        return rxFileGetRingData(open->handle);
    return rxSndStrMemGetData(open->handle);
}

// FS2 0x00165f58
int rxSndStrStreamIsEnd(int source)
{
    RxSndStrSource *open = rxSndStrStreamGetOpen(source);
    if (open == NULL)
        return 0;
    if (open->type == RxSndStrSource_Memory)
        return rxSndStrMemIsEnd(open->handle);
    if (rxFileGetRingFilled(open->handle) != 0)
        return 0;
    return (rxFileGetState(open->handle) & RXFILE_STATE_AT_END) != 0;
}

// A memory stream has nothing to pause.
// FS2 0x00166008
void rxSndStrStreamPause(int source, int pause)
{
    RxSndStrSource *open = rxSndStrStreamGetOpen(source);
    if (open != NULL && open->type != RxSndStrSource_Memory)
        rxFilePause(open->handle, pause);
}

// FS2 0x00166068
int rxSndStrStreamSync(int source, int wait)
{
    RxSndStrSource *open = rxSndStrStreamGetOpen(source);
    if (open == NULL || open->type == RxSndStrSource_Memory)
        return 0;
    return rxFileSync(open->handle, wait == 0);
}
