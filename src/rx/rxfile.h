/* ==========================================================================
 *  rx/rxfile.h
 *
 *  rx's file layer (rx/rxfile.c 0x153518..0x159928, not written yet:
 *  RosePan's src/rx/rxfile.h is the same library, carried over here as far
 *  as written TUs need it). Device drivers, two file contexts with their
 *  current device and dirs, and the async file slots with ring buffers,
 *  markers, segments and tags that a file thread serves. FS2's build is a
 *  little newer than RoR's (messages with __FILE__ / __LINE__, so a few
 *  functions are longer); the slot pools rxSysInitModules threads have
 *  RoR's strides (rxsys is word for word RoR's).
 * ======================================================================== */
#ifndef _RX_RXFILE_H
#define _RX_RXFILE_H

#include <assert.h>     /* static_assert in C11 */
#include <stddef.h>

#include "rx/rxsys.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RxFileDriver    RxFileDriver;
typedef struct RxFileMarkerDef RxFileMarkerDef;
typedef struct RxFileMarker    RxFileMarker;
typedef struct RxFileTagDef    RxFileTagDef;
typedef struct RxFileTag       RxFileTag;
typedef struct RxFileSegment   RxFileSegment;
typedef struct RxFileSlot      RxFileSlot;

/* Device driver (0x24 bytes each on the EE, indexed by RX_SYS_DEVICE;
 * MCARD has none). layout: host -- a table of code pointers built at run
 * time. */
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

/* Open flags: SCE_RDONLY / WRONLY / CREAT / TRUNC plus the rx bits. */
#define RX_FILE_NOWAIT  0x8000      /* return before the request is served  */
#define RX_FILE_RING    0x10000     /* keep the slot's ring buffer filled   */

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

/* The pools rxSysInitModules threads into free lists (bss 0x4d0b80..). */
extern RxFileSlot     rxFileSlots[64];          /* bss 0x4d0b80 */
extern RxFileTag      rxFileTagPool[64];        /* bss 0x4d3f80 */
extern RxFileTag     *rxFileTagFree;            /* bss 0x4d4380 */
extern RxFileMarker   rxFileMarkerPool[64];     /* bss 0x4d4384 */
extern RxFileMarker  *rxFileMarkerFree;         /* bss 0x4d4984 */
extern RxFileSegment  rxFileSegmentPool[256];   /* bss 0x4d4988 */
extern RxFileSegment *rxFileSegmentFree;        /* bss 0x4d5988 */

void   rxFileClose(int fd);                                         /* FS2 0x00153518 */
int    rxFileOpenEx(const char *name, int flags, void *ring, int ringSize, int threshold); /* FS2 0x00154008 */
int    rxFileOpenSlot(int device, const char *path, int flags, void *ring, int ringSize, int threshold); /* FS2 0x00154130 */
void   rxFileCallDevice(int device, const char *root);              /* FS2 0x001555f8 */
int    rxFileLoad(const char *name, void *buf);                     /* FS2 0x00155c20 */
int    rxFileLoadModule(const char *path, int arglen, const char *args); /* FS2 0x00155d78 */
int    rxFileRebootIop(const char *img);                            /* FS2 0x00155f38 */
void   rxFileInit(void);                                            /* FS2 0x00155fc0 */
void   rxFileTerm(void);                                            /* FS2 0x00155ff8 */
RxFileDriver *rxFileGetDriver(int device);                          /* FS2 0x00156020 */
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
char  *rxFileGetLastName(int device);                               /* FS2 0x00158a88 */
int    rxFileSetDeviceParam(int device, int value);                 /* FS2 0x00158aa8 */
void   rxFileStartThread(void);                                     /* FS2 0x00158d78 */
void   rxFileStopThread(void);                                      /* FS2 0x00158da0 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_RXFILE_H */
