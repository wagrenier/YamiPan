/* ==========================================================================
 *  rx/rxsndstrbank.h
 *
 *  FS2's XWS banks (rx/rxsndstrbank.c; not in RoR): 256 bank slots, each a
 *  view of an RXWS file in EE memory -- its request records (FORM), their
 *  names (FTXT), marks (MARK), branches (BRCH) and tracks (TRCK) -- and of
 *  where the sample data (the body) is: in the file's BODY chunk, attached
 *  memory, a separate file or IOP memory. rxsndstr.h includes it.
 * ======================================================================== */
#ifndef _RX_RXSNDSTRBANK_H
#define _RX_RXSNDSTRBANK_H

#include "sdk/scetypes.h"

#include <assert.h>     /* static_assert in C11 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RXSNDSTR_BANK_MAX           256     /* bank slots (bss 0x514780) */
#define RXSNDSTR_XWS_VERSION_MIN    0x200   /* rxSndStrBankOpen refuses older files */

/* A chunk tag as the file stores it: the four characters in file order, read
 * as one little-endian word (the ROM loads the word of the string "FORM"). */
#define RXSNDSTR_XWS_TAG(a, b, c, d) \
    ((u_int)(u_char)(a) | ((u_int)(u_char)(b) << 8) | ((u_int)(u_char)(c) << 16) | ((u_int)(u_char)(d) << 24))

#define RXSNDSTR_XWS_MAGIC      RXSNDSTR_XWS_TAG('R', 'X', 'W', 'S')
#define RXSNDSTR_XWS_FORM       RXSNDSTR_XWS_TAG('F', 'O', 'R', 'M')   /* request records */
#define RXSNDSTR_XWS_FTXT       RXSNDSTR_XWS_TAG('F', 'T', 'X', 'T')   /* request names */
#define RXSNDSTR_XWS_MARK       RXSNDSTR_XWS_TAG('M', 'A', 'R', 'K')   /* per-request marks */
#define RXSNDSTR_XWS_BRCH       RXSNDSTR_XWS_TAG('B', 'R', 'C', 'H')   /* per-request branches */
#define RXSNDSTR_XWS_TRCK       RXSNDSTR_XWS_TAG('T', 'R', 'C', 'K')   /* per-request tracks */
#define RXSNDSTR_XWS_BODY       RXSNDSTR_XWS_TAG('B', 'O', 'D', 'Y')   /* the sample data */

/* Where a bank's body is (slot +0x2). */
enum RxSndStrBankBody
{
    RxSndStrBankBody_Memory = 0,        /* EE memory: the BODY chunk or rxSndStrBankAttachBodyMemory */
    RxSndStrBankBody_File   = 1,        /* a file, with the device and search dir of the attach */
    RxSndStrBankBody_Iop    = 2,        /* IOP memory */
    RxSndStrBankBody_None   = 0xff,     /* never attached (set by rxSndStrBankOpen) */
};

/* An RXWS file's header; its chunks follow at +0x10. layout: EE (file bytes). */
typedef struct RxSndStrXwsHeader
{
    u_int magic;                        // FS2 +0x0: RXSNDSTR_XWS_MAGIC
    int   size;                         // FS2 +0x4: bytes of chunks after the header
    int   version;                      // FS2 +0x8: >= RXSNDSTR_XWS_VERSION_MIN
    int   _0c;                          // FS2 +0xc
} RxSndStrXwsHeader;
static_assert(sizeof(RxSndStrXwsHeader) == 0x10, "RxSndStrXwsHeader");

/* A chunk's header; size bytes of data follow at +0x10. layout: EE (file bytes). */
typedef struct RxSndStrXwsChunk
{
    u_int tag;                          // FS2 +0x0: RXSNDSTR_XWS_*
    int   size;                         // FS2 +0x4: bytes of data
    int   _08;                          // FS2 +0x8
    int   _0c;                          // FS2 +0xc
} RxSndStrXwsChunk;
static_assert(sizeof(RxSndStrXwsChunk) == 0x10, "RxSndStrXwsChunk");

/* A count and that many byte offsets, each from the start of the chunk's
 * data (FTXT; MARK / BRCH at +4 and each request's list; TRCK and each
 * request's list at +4). layout: EE (file bytes). */
typedef struct RxSndStrXwsIndex
{
    int count;                          // FS2 +0x0
    int offsets[1];                     // FS2 +0x4: count of them
} RxSndStrXwsIndex;
static_assert(offsetof(RxSndStrXwsIndex, offsets) == 0x4, "RxSndStrXwsIndex::offsets");

/* A mark or branch record (MARK / BRCH): the formats read the rest
 * (architecture.md "Batch 008": +8 type, +0xa / +0xb loop count).
 * layout: EE (file bytes). */
typedef struct RxSndStrMark
{
    u_int offset;                       // FS2 +0x0: position in the stream
    int   param;                        // FS2 +0x4
    u_char _08[4];                      // FS2 +0x8
    char  name[1];                      // FS2 +0xc: NUL-terminated
} RxSndStrMark;
static_assert(offsetof(RxSndStrMark, param) == 0x4, "RxSndStrMark::param");
static_assert(offsetof(RxSndStrMark, name) == 0xc, "RxSndStrMark::name");

/* A track record of a request (TRCK). layout: EE (file bytes). */
typedef struct RxSndStrTrack
{
    u_char _0;                          // FS2 +0x0
    u_char channels;                    // FS2 +0x1
    u_char _2[2];                       // FS2 +0x2
    short  volL;                        // FS2 +0x4
    short  volR;                        // FS2 +0x6
    char   name[1];                     // FS2 +0x8: NUL-terminated
} RxSndStrTrack;
static_assert(offsetof(RxSndStrTrack, channels) == 0x1, "RxSndStrTrack::channels");
static_assert(offsetof(RxSndStrTrack, volL) == 0x4, "RxSndStrTrack::volL");
static_assert(offsetof(RxSndStrTrack, volR) == 0x6, "RxSndStrTrack::volR");
static_assert(offsetof(RxSndStrTrack, name) == 0x8, "RxSndStrTrack::name");

/* A request record (FORM; variable length, the next one length bytes on).
 * architecture.md "Batch 008": +4 / +5 a volume the formats read.
 * layout: EE (file bytes). */
typedef struct RxSndStrRequest
{
    u_char format;                      // FS2 +0x0: RxSndStrFormatId
    u_char length;                      // FS2 +0x1: bytes of this record
    u_char flags;                       // FS2 +0x2: bit 0 loop
    u_char _3[6];                       // FS2 +0x3
    u_char channels;                    // FS2 +0x9: 1 or 2
    u_short rate;                       // FS2 +0xa: sample rate
    u_char _c[4];                       // FS2 +0xc
    u_int  offset;                      // FS2 +0x10: in the body
    u_int  size;                        // FS2 +0x14
    u_int  loopOffset;                  // FS2 +0x18
} RxSndStrRequest;
static_assert(offsetof(RxSndStrRequest, length) == 0x1, "RxSndStrRequest::length");
static_assert(offsetof(RxSndStrRequest, flags) == 0x2, "RxSndStrRequest::flags");
static_assert(offsetof(RxSndStrRequest, channels) == 0x9, "RxSndStrRequest::channels");
static_assert(offsetof(RxSndStrRequest, rate) == 0xa, "RxSndStrRequest::rate");
static_assert(offsetof(RxSndStrRequest, offset) == 0x10, "RxSndStrRequest::offset");
static_assert(offsetof(RxSndStrRequest, size) == 0x14, "RxSndStrRequest::size");
static_assert(offsetof(RxSndStrRequest, loopOffset) == 0x18, "RxSndStrRequest::loopOffset");

/* Open a bank on an RXWS file in memory (kept there, not copied); owner is
 * stored and not read (the game passes 0). The bank, -1 failed (not RXWS,
 * older than 0x200, every slot taken). A BODY chunk is attached at once. */
int  rxSndStrBankOpen(int owner, const void *header);                   /* FS2 0x001622f8 */
void rxSndStrBankClose(int bank);                                       /* FS2 0x001624d8 */
/* The body: name under the current device and search dir (remembered for
 * rxSndStrBankOpenBody), EE memory, or IOP memory. */
void rxSndStrBankAttachBodyFile(int bank, const char *name);            /* FS2 0x00162530 */
void rxSndStrBankAttachBodyMemory(int bank, const void *body, int size); /* FS2 0x001625c8 */
void rxSndStrBankAttachBodyIop(int bank, u_int iopAddr, int size);      /* FS2 0x00162610 */
/* The RXWS header, null for an unknown bank. */
const void *rxSndStrBankGetData(int bank);                              /* FS2 0x00162658 */
/* RxSndStrBankBody, -1 for an unknown bank. */
int  rxSndStrBankGetBodyType(int bank);                                 /* FS2 0x001626a0 */
/* The body's EE pointer or IOP address, 0 for an unknown bank. */
uintptr_t rxSndStrBankGetBodyAddr(int bank);                            /* FS2 0x001626e8 */
/* A file body's device, search dir and name; each out may be null. */
void rxSndStrBankGetBodyFile(int bank, int *device, char *dir, char *name); /* FS2 0x00162730 */

/* Requests: 0 / null / -1 when the bank or request is unknown. */
int  rxSndStrBankGetRequestCount(int bank);                             /* FS2 0x001627c0 */
const char *rxSndStrBankGetRequestName(int bank, int request);          /* FS2 0x001627d8 */
int  rxSndStrBankGetRequestFormat(int bank, int request);               /* FS2 0x001627f0: -1 */
int  rxSndStrBankGetRequestRate(int bank, int request);                 /* FS2 0x00162820 */
int  rxSndStrBankGetRequestChannels(int bank, int request);             /* FS2 0x00162850 */
u_int rxSndStrBankGetRequestOffset(int bank, int request);              /* FS2 0x00162880 */
int  rxSndStrBankGetRequestSize(int bank, int request);                 /* FS2 0x001628b0 */
u_int rxSndStrBankGetRequestLoopOffset(int bank, int request);          /* FS2 0x001628e0 */
/* The request of that name, -1 none. */
int  rxSndStrBankFindRequest(int bank, const char *name);               /* FS2 0x00162a00 */
const RxSndStrRequest *rxSndStrBankGetRequest(int bank, int request);   /* FS2 0x00162fd8 */

/* Marks and branches of a request: 0 / "" when unknown. */
int  rxSndStrBankGetMarkCount(int bank, int request);                   /* FS2 0x00162910 */
const char *rxSndStrBankGetMarkName(int bank, int request, int mark);   /* FS2 0x00162928 */
u_int rxSndStrBankGetMarkOffset(int bank, int request, int mark);       /* FS2 0x00162958 */
int  rxSndStrBankGetMarkParam(int bank, int request, int mark);         /* FS2 0x00162988 */
const RxSndStrMark *rxSndStrBankGetMark(int bank, int request, int mark);         /* FS2 0x00163070 */
int  rxSndStrBankGetBranchCount(int bank, int request);                 /* FS2 0x001629b8 */
const char *rxSndStrBankGetBranchName(int bank, int request, int branch); /* FS2 0x001629d0 */
const RxSndStrMark *rxSndStrBankGetBranch(int bank, int request, int branch);     /* FS2 0x00163088 */

/* Tracks of a request: 0 / null when unknown. */
int  rxSndStrBankGetTrackCount(int bank, int request);                  /* FS2 0x00162c20 */
const RxSndStrTrack *rxSndStrBankGetTrack(int bank, int request, int track); /* FS2 0x00162b70 */
int  rxSndStrBankGetTrackChannels(int bank, int request, int track);    /* FS2 0x00162c48 */
const char *rxSndStrBankGetTrackName(int bank, int request, int track); /* FS2 0x00162c70 */
/* The track of that name; -1 none, 0 also for an unknown bank or request. */
int  rxSndStrBankFindTrack(int bank, int request, const char *name);    /* FS2 0x00162ca0 */
/* RXSNDSTR_VOLUME_UNITY (0x80) each when unknown; each out may be null. */
void rxSndStrBankGetTrackVolume(int bank, int request, int track, int *volL, int *volR); /* FS2 0x00162d60 */

void rxSndStrBankInit(void);                                            /* FS2 0x00162dd8 */
void rxSndStrBankExit(void);                                            /* FS2 0x00162e18 */
void rxSndStrBankUpdate(void);                                          /* FS2 0x00162e20 */
/* A stream source on the bank's body (rxsndstrstream.c), read through buf
 * for a file body. The source id, -1 failed. */
int  rxSndStrBankOpenBody(int bank, void *buf, int bufSize);            /* FS2 0x00162e28 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDSTRBANK_H */
