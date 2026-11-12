/* ==========================================================================
 *  rx/rxfile.h
 *
 *  The rx middleware's file layer (rx/rxfile.c 0x153518..0x156060 and
 *  0x158400..0x159928, around the three device drivers rx/filehost.c,
 *  rx/filecd.c and rx/filehdd.c; RoR 0x2437f8..0x249b88). Three pieces:
 *  - device drivers: a table of nine functions per device (HOST, CDROM, HDD;
 *    MCARD has none) that turns a name into a device path and reads, writes
 *    or sizes a whole file;
 *  - two contexts ("thread type" 0 / 1: the caller's and the file thread's),
 *    each with a current device and a load / size hook pair;
 *  - per device and context, a current dir, a search dir and a name stack.
 *  rxFileLoad / rxFileLoadModule / rxFileRebootIop build the path from the
 *  current device's dirs and go through rxsys's sce* wrappers.
 *  Beside them, the async file slots (0x153518..0x155530): 63 open files
 *  ("fd" 1..63) whose create / size / read / write requests go to the file
 *  thread's queues (0x158da0..0x159758), with an optional ring buffer the
 *  thread keeps filled, position markers that split, count and loop reads,
 *  and data tags reported per ring segment. The default load / size / save
 *  hooks use them.
 *
 *  RosePan's src/rx/rxfile.h carried over to C (FS2 links RoR's prebuilt
 *  rx; the layouts are RoR's). FS2's game code calls the two hooks through
 *  jump thunks of its own (rxFileCallLoadHook 0x152730 / rxFileCallSizeHook
 *  0x152740, beside rxTerm / rxInit): the port calls the rx functions.
 * ======================================================================== */
#ifndef _RX_RXFILE_H
#define _RX_RXFILE_H

#include <assert.h>     /* static_assert in C11 */
#include <stddef.h>

#include "rx/rxsys.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RxFileDriver          RxFileDriver;
/* The driver files share rxfile.c's message switch (data 0x2f5aa0). */
extern int rxFileVerbose;

typedef struct RxFileContext         RxFileContext;
typedef struct RxFileDirInfo         RxFileDirInfo;
typedef struct RxFileDevInfo         RxFileDevInfo;
typedef struct RxFileMarkerDef       RxFileMarkerDef;
typedef struct RxFileMarker          RxFileMarker;
typedef struct RxFileTagDef          RxFileTagDef;
typedef struct RxFileTag             RxFileTag;
typedef struct RxFileTagHit          RxFileTagHit;
typedef struct RxFileSegment         RxFileSegment;
typedef struct RxFileSlot            RxFileSlot;
typedef struct RxFileRequest         RxFileRequest;
typedef struct RxFileQueue           RxFileQueue;
typedef struct RxFileThread          RxFileThread;

/* Device driver (bss 0x4d5990, 0x24 bytes each on the EE, indexed by
 * RX_SYS_DEVICE; MCARD's entry 0x4d59d8 stays empty).
 * layout: host -- a table of code pointers built at run time. */
struct RxFileDriver
{
    void  (*makePath)(char *out, const char *name);                     // FS2 0x00
    int   (*getStatus)(int what);                                       // FS2 0x04
    int   (*control)(int cmd, int arg);                                 // FS2 0x08
    int   (*getSize)(const char *name);                                 // FS2 0x0c
    int   (*create)(const char *name, int truncate);                    // FS2 0x10
    int   (*read)(const char *name, void *buf, int offset, int size);   // FS2 0x14
    int   (*write)(const char *name, int offset, const void *buf, int size); // FS2 0x18
    void  (*setRoot)(const char *root);                                 // FS2 0x1c
    char *(*getRoot)(void);                                             // FS2 0x20
};

typedef int (*RxFileLoadHook)(const char *name, void *buf);
typedef int (*RxFileSizeHook)(const char *name);

/* A context ("thread type" 0 / 1, bss 0x4e7e84, 12 bytes each on the EE).
 * layout: host -- holds code pointers. */
struct RxFileContext
{
    int            device;      // FS2 0x0, RX_SYS_DEVICE
    RxFileLoadHook loadHook;    // FS2 0x4, default rxFileDefaultLoadHook
    RxFileSizeHook sizeHook;    // FS2 0x8, default rxFileDefaultSizeHook
};

/* Per device and context: search dir and the name stack it is pushed on.
 * EE layout (plain data). */
struct RxFileDirInfo
{
    char searchDir[0x80];       // FS2 0x000
    char stack[0x800];          // FS2 0x080, grows down from stackTop
    int  stackTop;              // FS2 0x880, 0x7ff when empty
    char lastName[0x80];        // FS2 0x884
};
static_assert(sizeof(RxFileDirInfo) == 0x904, "RxFileDirInfo size");
static_assert(offsetof(RxFileDirInfo, searchDir) == 0x000, "RxFileDirInfo::searchDir");
static_assert(offsetof(RxFileDirInfo, stack) == 0x080, "RxFileDirInfo::stack");
static_assert(offsetof(RxFileDirInfo, stackTop) == 0x880, "RxFileDirInfo::stackTop");
static_assert(offsetof(RxFileDirInfo, lastName) == 0x884, "RxFileDirInfo::lastName");

/* Per device (bss 0x4e7e9c). EE layout (plain data). */
struct RxFileDevInfo
{
    int           param;        // FS2 0x00, -1 after rxFileInitSlots; meaning open
    char          dir[0x80];    // FS2 0x04, lower case, '/' separators, ends in '/'
    RxFileDirInfo ctx[2];       // FS2 0x84, one per context
};
static_assert(sizeof(RxFileDevInfo) == 0x128c, "RxFileDevInfo size");
static_assert(offsetof(RxFileDevInfo, param) == 0x00, "RxFileDevInfo::param");
static_assert(offsetof(RxFileDevInfo, dir) == 0x04, "RxFileDevInfo::dir");
static_assert(offsetof(RxFileDevInfo, ctx) == 0x84, "RxFileDevInfo::ctx");

/* Called by rxFileHddOnError with the name and the I/O error. */
typedef void (*RxFileErrorHook)(const char *name, int error);
/* ---- async file slots ------------------------------------------------- */

/* Open flags: SCE_RDONLY / WRONLY / CREAT / TRUNC plus the rx bits. */
#define RX_FILE_NOWAIT  0x8000      /* return before the request is served  */
#define RX_FILE_RING    0x10000     /* keep the slot's ring buffer filled   */

/* Request ops (RxFileRequest::op), served through the device driver. */
enum
{
    RX_FILE_OP_CREATE = 0,
    RX_FILE_OP_SIZE   = 1,
    RX_FILE_OP_READ   = 2,
    RX_FILE_OP_WRITE  = 3,
};

/* A position marker as the caller passes it (20 bytes on the EE; copied
 * into a pool node). A read crossing an active marker whose callback
 * accepts stops at it, counts a pass, and continues at jumpPos (-1: the
 * end): stream loops. layout: host -- holds a code pointer. */
struct RxFileMarkerDef
{
    unsigned int  pos;          // FS2 0x00, file position
    unsigned char active;       // FS2 0x04
    unsigned char id;           // FS2 0x05, for rxFileEnableMarker
    unsigned char count;        // FS2 0x06, passes so far
    unsigned char max;          // FS2 0x07, passes that stop (0: all)
    int           jumpPos;      // FS2 0x08
    int         (*callback)(void *arg, RxFileMarkerDef *marker); // FS2 0x0c, 0: always stop
    int           user;         // FS2 0x10
};

/* Marker pool node (bss 0x4d4384, 64, 0x18 bytes on the EE). layout: host. */
struct RxFileMarker
{
    RxFileMarker   *next;       // FS2 0x00, sorted by pos
    RxFileMarkerDef def;        // FS2 0x04
};

/* A data tag as the caller passes it (12 bytes). EE layout (plain data). */
struct RxFileTagDef
{
    unsigned int  pos;          // FS2 0x0, file position
    unsigned char active;       // FS2 0x4
    unsigned char id;           // FS2 0x5, for rxFileEnableTag
    unsigned char pad[2];       // FS2 0x6
    int           value;        // FS2 0x8
};
static_assert(sizeof(RxFileTagDef) == 0xc, "RxFileTagDef size");
static_assert(offsetof(RxFileTagDef, pos) == 0x0, "RxFileTagDef::pos");
static_assert(offsetof(RxFileTagDef, active) == 0x4, "RxFileTagDef::active");
static_assert(offsetof(RxFileTagDef, id) == 0x5, "RxFileTagDef::id");
static_assert(offsetof(RxFileTagDef, value) == 0x8, "RxFileTagDef::value");

/* Tag pool node (bss 0x4d3f80, 64, 0x10 bytes on the EE). layout: host. */
struct RxFileTag
{
    RxFileTag   *next;          // FS2 0x0, sorted by pos
    RxFileTagDef def;           // FS2 0x4
};

/* What rxFileGetRingTags reports: the tag's offset past the ring read
 * position, and its value. EE layout (plain data). */
struct RxFileTagHit
{
    int offset;                 // FS2 0x0
    int value;                  // FS2 0x4
};
static_assert(sizeof(RxFileTagHit) == 0x8, "RxFileTagHit size");
static_assert(offsetof(RxFileTagHit, offset) == 0x0, "RxFileTagHit::offset");
static_assert(offsetof(RxFileTagHit, value) == 0x4, "RxFileTagHit::value");

/* What one transfer put in the ring: file position and size, in a circular
 * list per slot (pool bss 0x4d4988, 256, 0x10 bytes on the EE; the free
 * list links through next). layout: host. */
struct RxFileSegment
{
    RxFileSegment *prev;        // FS2 0x0
    RxFileSegment *next;        // FS2 0x4
    unsigned int   pos;         // FS2 0x8
    int            size;        // FS2 0xc
};

/* An open file (bss 0x4d0b80, 64 of 0xd0 bytes on the EE; slot 0 is never
 * handed out). layout: host -- holds pointers. */
struct RxFileSlot
{
    int            _00;         // FS2 0x00, untouched
    unsigned char  used;        // FS2 0x04
    unsigned char  paused;      // FS2 0x05, ring refill paused (set at open)
    unsigned char  device;      // FS2 0x06, RX_SYS_DEVICE
    char           path[0x80];  // FS2 0x08
    unsigned int   flags;       // FS2 0x88, open flags
    unsigned int   pos;         // FS2 0x8c, position the requests reached
    unsigned int   size;        // FS2 0x90, file size once known
    unsigned char *ring;        // FS2 0x94
    unsigned int   ringSize;    // FS2 0x98
    unsigned int   ringThreshold; // FS2 0x9c, refill while filled <= this
    unsigned int   ringWrite;   // FS2 0xa0
    unsigned int   ringRead;    // FS2 0xa4
    unsigned int   ringFilled;  // FS2 0xa8
    RxFileMarker  *markers;     // FS2 0xac
    void          *markerArg;   // FS2 0xb0, first argument of the callbacks
    RxFileSegment  segments;    // FS2 0xb4, sentinel of the segment list
    RxFileTag     *tags;        // FS2 0xc4
    volatile unsigned int state; // FS2 0xc8, 1 created, 2 size known, 4 failed
    volatile int   pending;     // FS2 0xcc, requests not served yet
};

/* Called by the file thread when a request is served (or cancelled): the
 * slot, the request's file offset, and the driver's result (the size for a
 * cancel). */
typedef void (*RxFileRequestCallback)(RxFileSlot *slot, int offset, int result);

/* A request in a file thread queue (0x2c bytes on the EE). layout: host. */
struct RxFileRequest
{
    volatile int state;         // FS2 0x00, 0 queued, 1 done or cancelled
    int          device;        // FS2 0x04, before the device map
    const char  *path;          // FS2 0x08, the slot's path
    int          op;            // FS2 0x0c, RX_FILE_OP_*
    int          offset;        // FS2 0x10, file offset (create: truncate)
    void        *buf;           // FS2 0x14
    int          size;          // FS2 0x18
    int          retryMissing;  // FS2 0x1c, retry a -2 (not found) answer
    volatile int busy;          // FS2 0x20, being served
    RxFileRequestCallback callback; // FS2 0x24
    RxFileSlot  *owner;         // FS2 0x28
};

/* 64-entry ring of requests (0xb08 bytes on the EE). layout: host. */
struct RxFileQueue
{
    RxFileRequest req[64];      // FS2 0x000
    volatile int  head;         // FS2 0xb00, next push
    volatile int  tail;         // FS2 0xb04, next to serve
};

/* The file thread (bss 0x4ec8d0, 0x11620 bytes on the EE): queue 0 for the
 * ring refills and opens, queue 1 for rxFileRead / rxFileWrite. layout: host. */
struct RxFileThread
{
    int           step;         // FS2 0x00, priority + 1, passed to proc
    void        (*proc)(int step); // FS2 0x04, run before every request
    int           thread;       // FS2 0x08, rx thread index
    volatile int  stop;         // FS2 0x0c, 1 asks to stop, -1 stopped
    unsigned char stack[0x10000]; // FS2 0x10
    RxFileQueue   queue[2];     // FS2 0x10010
};

/* The pools rxSysInitModules threads into free lists (bss 0x4d0b80..). */
extern RxFileSlot     rxFileSlots[64];          /* bss 0x4d0b80 */
extern RxFileTag      rxFileTagPool[64];        /* bss 0x4d3f80 */
extern RxFileTag     *rxFileTagFree;            /* bss 0x4d4380 */
extern RxFileMarker   rxFileMarkerPool[64];     /* bss 0x4d4384 */
extern RxFileMarker  *rxFileMarkerFree;         /* bss 0x4d4984 */
extern RxFileSegment  rxFileSegmentPool[256];   /* bss 0x4d4988 */
extern RxFileSegment *rxFileSegmentFree;        /* bss 0x4d5988 */

void   rxFileClose(int fd);                                         /* FS2 0x00153518 */
void   rxFileAbort(int fd);                                         /* FS2 0x00153578 */
int    rxFileGetState(int fd);                                      /* FS2 0x001535d8 */
int    rxFileRead(int fd, void *buf, int size);                     /* FS2 0x00153658 */
int    rxFileWrite(int fd, const void *buf, int size);              /* FS2 0x00153818 */
int    rxFileSeek(int fd, int offset, int whence);                  /* FS2 0x00153960 */
int    rxFileSync(int fd, int noWait);                              /* FS2 0x001539f8 */
int    rxFileSetMarkers(int fd, const RxFileMarkerDef *defs, int count, void *arg); /* FS2 0x00153ac8 */
void   rxFileEnableMarker(int fd, int id, int active);              /* FS2 0x00153b28 */
int    rxFilePause(int fd, int pause);                              /* FS2 0x00153b80 */
unsigned char *rxFileGetRingData(int fd);                           /* FS2 0x00153bf8 */
int    rxFileGetRingReadPos(int fd);                                /* FS2 0x00153c48 */
int    rxFileGetRingSize(int fd);                                   /* FS2 0x00153c90 */
int    rxFileGetRingFilled(int fd);                                 /* FS2 0x00153cd8 */
void   rxFileRingConsume(int fd, int size);                         /* FS2 0x00153d48 */
int    rxFileGetRingFilePos(int fd, int offset);                    /* FS2 0x00153e98 */
int    rxFileSetTags(int fd, const RxFileTagDef *defs, int count);  /* FS2 0x00153ef0 */
void   rxFileEnableTag(int fd, int id, int active);                 /* FS2 0x00153f50 */
int    rxFileGetRingTags(int fd, RxFileTagHit *out, int max, int size); /* FS2 0x00153fa8 */
int    rxFileOpenEx(const char *name, int flags, void *ring, int ringSize, int threshold); /* FS2 0x00154008 */
int    rxFileOpenSlot(int device, const char *path, int flags, void *ring, int ringSize, int threshold); /* FS2 0x00154130 */
void   rxFileOnCreate(RxFileSlot *slot, int offset, int result);    /* FS2 0x00154518 */
void   rxFileOnGetSize(RxFileSlot *slot, int offset, int result);   /* FS2 0x001545a0 */
void   rxFileSlotClose(RxFileSlot *slot, int abort);                /* FS2 0x00154640 */
void   rxFileSlotKick(RxFileSlot *slot);                            /* FS2 0x00154708 */
int    rxFileSlotTransfer(RxFileSlot *slot, int op, void *buf, int size); /* FS2 0x00154780 */
int    rxFileSlotRefill(RxFileSlot *slot);                          /* FS2 0x001547b0 */
int    rxFileSlotRequest(RxFileSlot *slot, int queue, int op, void *buf, int size); /* FS2 0x00154888 */
int    rxFileSlotRequestChunked(RxFileSlot *slot, int queue, int op, void *buf, int size); /* FS2 0x00154ab8 */
int    rxFileSlotPost(RxFileSlot *slot, int queue, int op, void *buf, int size); /* FS2 0x00154c68 */
void   rxFileOnTransfer(RxFileSlot *slot, int offset, int result);  /* FS2 0x00154d48 */
int    rxFileSlotSetMarkers(RxFileSlot *slot, const RxFileMarkerDef *defs, int count, void *arg); /* FS2 0x00154dc0 */
void   rxFileSlotEnableMarker(RxFileSlot *slot, int id, int active); /* FS2 0x00154f88 */
void   rxFileSlotAddSegment(RxFileSlot *slot, int pos, int size);   /* FS2 0x00154fc8 */
void   rxFileSlotDropSegments(RxFileSlot *slot, int size);          /* FS2 0x00155090 */
void   rxFileSlotFreeSegments(RxFileSlot *slot);                    /* FS2 0x001551b0 */
int    rxFileSlotSegmentPos(RxFileSlot *slot, int offset);          /* FS2 0x00155268 */
int    rxFileSlotSetTags(RxFileSlot *slot, const RxFileTagDef *defs, int count); /* FS2 0x00155348 */
void   rxFileSlotEnableTag(RxFileSlot *slot, int id, int active);   /* FS2 0x001554f0 */
int    rxFileSlotGetTags(RxFileSlot *slot, RxFileTagHit *out, int max, int size); /* FS2 0x00155530 */

/* ---- the file layer --------------------------------------------------- */

void   rxFileCallDevice(int device, const char *root);              /* FS2 0x001555f8 */
char  *rxFileGetDeviceRoot(int device);                             /* FS2 0x00155648 */
int    rxFileCallLoadHook(const char *name, void *buf);             /* FS2 0x00155690 */
int    rxFileCallSizeHook(const char *name);                        /* FS2 0x001556d0 */
RxFileLoadHook rxFileSetLoadHook(RxFileLoadHook hook);              /* FS2 0x00155700 */
RxFileSizeHook rxFileSetSizeHook(RxFileSizeHook hook);              /* FS2 0x00155748 */
int    rxFileDefaultSizeHook(const char *name);                     /* FS2 0x00155790 */
int    rxFileDefaultLoadHook(const char *name, void *buf);          /* FS2 0x00155830 */
int    rxFileDefaultSaveHook(const char *name, const void *buf, int size); /* FS2 0x00155928 */
int    rxFileMakePath(char *out, const char *name);                 /* FS2 0x001559f8 */
int    rxFileLoadExec(const char *path, int argc, char **argv);     /* FS2 0x00155b00 */
int    rxFileLoad(const char *name, void *buf);                     /* FS2 0x00155c20 */
int    rxFileLoadModule(const char *path, int arglen, const char *args); /* FS2 0x00155d78 */
int    rxFileLoadModuleList(const char *const *names, int count);   /* FS2 0x00155ec0 */
int    rxFileRebootIop(const char *img);                            /* FS2 0x00155f38 */
void   rxFileInit(void);                                            /* FS2 0x00155fc0 */
void   rxFileTerm(void);                                            /* FS2 0x00155ff8 */
RxFileDriver *rxFileGetDriver(int device);                          /* FS2 0x00156020 */

/* contexts, devices and dirs */
void   rxFileInitSlots(void);                                       /* FS2 0x00158400 */
void   rxFileUpdate(void);                                          /* FS2 0x001584a8 */
int    rxFileSetContext(int index);                                 /* FS2 0x001584b0 */
int    rxFileSetCurrent(int device);                                /* FS2 0x00158518 */
int    rxFileGetCurrent(void);                                      /* FS2 0x00158610 */
int    rxFileSetDevice(int ctx, int device);                        /* FS2 0x00158630 */
void   rxFileSetDir(int device, const char *dir);                   /* FS2 0x00158738 */
char  *rxFileGetDir(int device);                                    /* FS2 0x00158800 */
void   rxFileSetSearchDir(int device, const char *dir);             /* FS2 0x00158820 */
char  *rxFileGetSearchDir(int device);                              /* FS2 0x00158950 */
void   rxFilePushSearchDir(int device);                             /* FS2 0x00158968 */
void   rxFilePopSearchDir(int device);                              /* FS2 0x001589f0 */
void   rxFileSetLastName(int device, const char *name);             /* FS2 0x00158a58 */
char  *rxFileGetLastName(int device);                               /* FS2 0x00158a88 */
int    rxFileSetDeviceParam(int device, int value);                 /* FS2 0x00158aa8 */
int    rxFileGetDeviceParam(int device);                            /* FS2 0x00158ad8 */
RxFileLoadHook rxFileGetLoadHook(void);                             /* FS2 0x00158af8 */
RxFileLoadHook rxFileSwapLoadHook(RxFileLoadHook hook);             /* FS2 0x00158b18 */
RxFileSizeHook rxFileGetSizeHook(void);                             /* FS2 0x00158b48 */
RxFileSizeHook rxFileSwapSizeHook(RxFileSizeHook hook);             /* FS2 0x00158b68 */
RxFileDevInfo *rxFileGetDeviceInfo(int device);                     /* FS2 0x00158b98 */
RxFileDirInfo *rxFileGetDirInfo(int device);                        /* FS2 0x00158bd0 */
RxFileContext *rxFileGetContext(void);                              /* FS2 0x00158c10 */
const char *rxFilePathNextElem(char *out, const char *path);        /* FS2 0x00158c38 */
void   rxFilePathUp(char *dir);                                     /* FS2 0x00158ca0 */
int    rxFilePathCountDots(const char *elem);                       /* FS2 0x00158d28 */

/* the file thread */
void   rxFileStartThread(void);                                     /* FS2 0x00158d78 */
void   rxFileStopThread(void);                                      /* FS2 0x00158da0 */
int    rxFilePostRequest(int queue, int device, const char *path, int op, int offset,
                         void *buf, int size, int retryMissing,
                         RxFileRequestCallback callback, RxFileSlot *owner); // FS2 0x00249098
void   rxFileCancelRequests(int device, RxFileSlot *owner, int abort); /* FS2 0x00158e80 */
void   rxFileCreateThread(int index, int priority, void (*proc)(int step)); /* FS2 0x00158f60 */
void   rxFileThreadMain(void);                                      /* FS2 0x00159050 */
void   rxFileThreadProc(int step);                                  /* FS2 0x00159380 */
void   rxFileNop0(void);                                            /* FS2 0x00159398 */
void   rxFileNop1(void);                                            /* FS2 0x001593a0 */
void   rxFileQueueInit(RxFileQueue *q);                             /* FS2 0x001593a8 */
int    rxFileQueuePush(RxFileQueue *q, int device, const char *path, int op, int offset,
                       void *buf, int size, int retryMissing,
                       RxFileRequestCallback callback, RxFileSlot *owner); // FS2 0x00249618
RxFileRequest *rxFileQueueFront(RxFileQueue *q);                    /* FS2 0x00159518 */
void   rxFileQueuePop(RxFileQueue *q);                              /* FS2 0x00159560 */
int    rxFileQueueCount(RxFileQueue *q);                            /* FS2 0x001595b8 */
int    rxFileQueueSpace(RxFileQueue *q);                            /* FS2 0x001595d8 */
void   rxFileQueueCancel(RxFileQueue *q, RxFileSlot *owner, int abort); /* FS2 0x00159608 */

/* device map and the HDD error hook */
void   rxFileSetDeviceMap(int device, int target);                  /* FS2 0x00159758 */
int    rxFileGetDeviceMap(int device);                              /* FS2 0x00159808 */
void   rxFileSetFallbackDevice(int device, int fallback);           /* FS2 0x00159870 */
int    rxFileGetFallbackDevice(int device);                         /* FS2 0x00159888 */
void   rxFileSetErrorHook(RxFileErrorHook hook);                    /* FS2 0x001598a0 */
RxFileErrorHook rxFileGetErrorHook(void);                           /* FS2 0x001598b0 */
int    rxFileSetThreadPriority(int priority);                       /* FS2 0x001598c0 */
int    rxFileGetThreadPriority(void);                               /* FS2 0x00159918 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXFILE_H */
