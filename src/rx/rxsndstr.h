/* ==========================================================================
 *  rx/rxsndstr.h
 *
 *  FS2's rx sound streams (rx/rxsndstr*.c; not in RoR, so RosePan has no
 *  counterpart): XWS banks (rxSndStrBank*, rxsndstrbank.h / .c) and the
 *  stream core (rxSndStr*, rxsndstr.c): 64 stream slots, each played
 *  by one of 8 registered formats (PCM, ADPCM: rxsndstrpcm.c /
 *  rxsndstradpcm.c) that apply the slot's dirty settings in their update.
 *  architecture.md "Batch 008" has the layers.
 * ======================================================================== */
#ifndef _RX_RXSNDSTR_H
#define _RX_RXSNDSTR_H

#include "rx/rxsndstrbank.h"
#include "sdk/scetypes.h"

#include <assert.h>     /* static_assert in C11 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RXSNDSTR_SLOT_MAX       64      /* stream slots (bss 0x51ef80) */
#define RXSNDSTR_FORMAT_MAX     8       /* format slots (bss 0x52168c) */
#define RXSNDSTR_TRACK_MAX      16      /* per-track volumes of a stream */
#define RXSNDSTR_VOLUME_UNITY   0x80    /* volumes reset to it */

/* Format ids (a request record's first byte; rxSndUtInit registers them). */
enum RxSndStrFormatId
{
    RxSndStrFormat_Adpcm = 0,           /* "ADPCM stream", table 0x2f5f38 */
    RxSndStrFormat_Pcm   = 1,           /* "PCM stream", table 0x2f5f18 */
};

/* rxSndStrGetStatus bits (slot +0x19). */
#define RXSNDSTR_STATUS_ACTIVE  0x1     /* the slot plays a stream */
#define RXSNDSTR_STATUS_PLAYING 0x2     /* the voice runs (rxSndAdpcmGetStatus's bit) */
#define RXSNDSTR_STATUS_END     0x4     /* the stream played out */
#define RXSNDSTR_STATUS_PAUSED  0x8     /* rxSndAdpcmGetStatus's bit */

/* Dirty bits (slot +0x1c): the setters mark what changed, the format's
 * update applies it and clears the bit. */
enum RxSndStrDirty
{
    RxSndStrDirty_Pause       = 0x01,
    RxSndStrDirty_Attr        = 0x02,
    RxSndStrDirty_Volume      = 0x04,   /* also set by rxSndStrSetMasterVolume */
    RxSndStrDirty_Pitch       = 0x08,
    RxSndStrDirty_FloatParam  = 0x10,   /* nobody reads it (architecture.md) */
    RxSndStrDirty_TrackVolume = 0x20,
};

typedef struct RxSndStrSlot    RxSndStrSlot;
typedef struct RxSndStrFormat  RxSndStrFormat;

/* Called with the stream id and the mark's / branch's index in its request;
 * run with interrupts off. The branch callback's result: non-zero takes the
 * branch (the format's default without a callback is 1). */
typedef void (*RxSndStrMarkCallback)(int stream, int mark);
typedef int  (*RxSndStrBranchCallback)(int stream, int branch);

/* A format's function table, copied in by rxSndStrRegisterFormat (EE 0x20).
 * layout: host -- code pointers. */
struct RxSndStrFormat
{
    void (*init)(void);                                     // FS2 +0x00: called on registration
    void (*exit)(void);                                     // FS2 +0x04: rxSndStrExit
    void (*update)(void);                                   // FS2 +0x08: rxSndStrUpdate
    int  (*open)(RxSndStrSlot *slot);                       // FS2 +0x0c: the handle, < 0 failed
    void (*close)(int handle, int async);                   // FS2 +0x10
    int  (*getHandle)(int handle);                          // FS2 +0x14: the rxSndPcm / rxSndAdpcm stream, may be null
    void (*pauseRead)(int handle, int pause);               // FS2 +0x18: may be null
    int  (*syncRead)(int handle, int arg);                  // FS2 +0x1c: non-zero while a read is in flight, may be null
};

/* A stream slot (bss 0x51ef80, 64 x 0x9c on the EE; free / active lists
 * through next). The play arguments are kept for the format's open.
 * layout: host -- holds pointers. */
struct RxSndStrSlot
{
    RxSndStrSlot          *next;                            // FS2 +0x00
    const RxSndStrFormat  *format;                          // FS2 +0x04: the registered table
    int                    bank;                            // FS2 +0x08
    int                    request;                         // FS2 +0x0c
    int                    _10;                             // FS2 +0x10: zeroed by play, unread here
    int                    _14;                             // FS2 +0x14: zeroed by play, unread here
    u_char                 id;                              // FS2 +0x18: the slot's index, the stream id
    u_char                 status;                          // FS2 +0x19: RXSNDSTR_STATUS_*
    u_char                 _1a;                             // FS2 +0x1a: zeroed by play
    u_char                 keepOnEnd;                       // FS2 +0x1b: 0: rxSndStrUpdate stops it when it ends
    u_char                 dirty;                           // FS2 +0x1c: RxSndStrDirty
    u_char                 attr;                            // FS2 +0x1d
    u_char                 pause;                           // FS2 +0x1e
    u_char                 dataWritten;                     // FS2 +0x1f: set by the format
    short                  volL;                            // FS2 +0x20
    short                  volR;                            // FS2 +0x22
    float                  pitch;                           // FS2 +0x24
    float                  floatParam;                      // FS2 +0x28
    u_int                  start;                           // FS2 +0x2c: byte offset; the format clamps it
    int                    filled;                          // FS2 +0x30: set by the format
    void                  *buf;                             // FS2 +0x34: file bodies' read buffer
    int                    bufSize;                         // FS2 +0x38
    u_int                  iopAddr;                         // FS2 +0x3c: rxSndPcmOpen's IOP buffer, 0 its own
    int                    iopKB;                           // FS2 +0x40: rxSndPcmOpen's blocks (the game: 4)
    u_int                  spuAddr;                         // FS2 +0x44
    int                    spuKB;                           // FS2 +0x48: a guess from rxSndAdpcmOpen's order (the game: 4)
    int                    handle;                          // FS2 +0x4c: the format's open result, -1 none
    void                  *userData;                        // FS2 +0x50
    RxSndStrMarkCallback   markCallback;                    // FS2 +0x54
    RxSndStrBranchCallback branchCallback;                  // FS2 +0x58
    short                  trackVolL[RXSNDSTR_TRACK_MAX];   // FS2 +0x5c
    short                  trackVolR[RXSNDSTR_TRACK_MAX];   // FS2 +0x7c
};

/* Streams: request of bank from start, read through buf (file bodies;
 * null for memory / IOP ones). The stream id, -1 failed. */
int  rxSndStrPlayEx(int bank, int request, u_int start, void *buf, int bufSize,
                    u_int iopAddr, int iopKB, u_int spuAddr, int spuKB,
                    int keepOnEnd);                                     /* FS2 0x001634b8 */
int  rxSndStrPlay(int bank, int request, void *buf, int bufSize, u_int iopAddr,
                  int iopKB, u_int spuAddr, int spuKB, int keepOnEnd);  /* FS2 0x001636e8 */
int  rxSndStrPlayAt(int bank, int request, u_int start, void *buf, int bufSize,
                    u_int iopAddr, int iopKB, u_int spuAddr, int spuKB,
                    int keepOnEnd);                                     /* FS2 0x00163738 */
/* arg: passed on, unread (the game passes 0). */
void rxSndStrPause(int stream, int pause, int arg);                     /* FS2 0x00163750 */
void rxSndStrStop(int stream, int arg);                                 /* FS2 0x00163848 */
void rxSndStrStopAsync(int stream, int arg);                            /* FS2 0x00163860 */
void rxSndStrStopAll(int arg);                                          /* FS2 0x00163878 */
/* Pause the stream and its reads. */
void rxSndStrSuspend(int stream, int arg);                              /* FS2 0x001638d8 */
u_int rxSndStrGetStatus(int stream);                                    /* FS2 0x00163910: RXSNDSTR_STATUS_* */
void rxSndStrSetAttr(int stream, int attr);                             /* FS2 0x00163960 */
void rxSndStrSetMasterVolume(int volL, int volR);                       /* FS2 0x001639b8 */
int  rxSndStrGetMasterVolumeL(void);                                    /* FS2 0x00163a00 */
int  rxSndStrGetMasterVolumeR(void);                                    /* FS2 0x00163a10 */
void rxSndStrSetVolume(int stream, int volL, int volR);                 /* FS2 0x00163a20 */
void rxSndStrSetTrackVolume(int stream, int track, int volL, int volR); /* FS2 0x00163a78 */
void rxSndStrSetPitch(int stream, float pitch);                         /* FS2 0x00163ae8 */
void rxSndStrSetFloatParam(int stream, float value);                    /* FS2 0x00163b40 */
int  rxSndStrGetFilled(int stream);                                     /* FS2 0x00163b98 */
/* The setters return the previous value. */
void *rxSndStrSetUserData(int stream, void *userData);                  /* FS2 0x00163be8 */
void *rxSndStrGetUserData(int stream);                                  /* FS2 0x00163c38 */
RxSndStrMarkCallback rxSndStrSetMarkCallback(int stream, RxSndStrMarkCallback cb);       /* FS2 0x00163c88 */
RxSndStrBranchCallback rxSndStrSetBranchCallback(int stream, RxSndStrBranchCallback cb); /* FS2 0x00163cd8 */
int  rxSndStrGetBank(int stream);                                       /* FS2 0x00163d28 */
int  rxSndStrGetRequest(int stream);                                    /* FS2 0x00163d78 */
/* The rxSndAdpcm / rxSndPcm stream under it, -1 none. */
int  rxSndStrGetHandle(int stream);                                     /* FS2 0x00163dc8 */
void rxSndStrPauseRead(int stream, int pause);                          /* FS2 0x00163e40 */
/* Non-zero while a read is still in flight. */
int  rxSndStrSyncRead(int stream, int arg);                             /* FS2 0x00163ea8 */
int  rxSndStrGetDataWritten(int stream);                                /* FS2 0x00163f20 */
/* The active stream ids into ids (may be null); their count. */
int  rxSndStrGetActiveIds(u_int *ids);                                  /* FS2 0x00163f70 */
void rxSndStrInit(void);                                                /* FS2 0x00163fb8 */
void rxSndStrExit(void);                                                /* FS2 0x00164040 */
void rxSndStrUpdate(void);                                              /* FS2 0x00164088 */
/* 1 registered, 0 a bad id or already taken. */
int  rxSndStrRegisterFormat(int id, const char *name, const RxSndStrFormat *format); /* FS2 0x00164128 */

/* For the formats. */
int  rxSndStrFmtGetMasterVolumeL(void);                                 /* FS2 0x00164228 */
int  rxSndStrFmtGetMasterVolumeR(void);                                 /* FS2 0x00164238 */
void rxSndStrFmtCallMarkCallback(RxSndStrSlot *slot, int mark);         /* FS2 0x00164248 */
int  rxSndStrFmtCallBranchCallback(RxSndStrSlot *slot, int branch);     /* FS2 0x001642e8 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXSNDSTR_H */
