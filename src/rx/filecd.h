/* ==========================================================================
 *  rx/filecd.h
 *
 *  rx's cdrom0: device driver: reads the ISO 9660 file system itself (primary
 *  volume descriptor, path table, directory records) through a 32-sector
 *  LRU cache (0x1565c8..0x157db8).
 *  Not written yet (its own TU, rx/filecd.c): the prototypes rx/rxfile.c
 *  calls, from RosePan's src/rx/rxfile.h. See rxfile.h.
 * ======================================================================== */
#ifndef _RX_FILECD_H
#define _RX_FILECD_H

#include "rx/rxfile.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The cdrom driver's sector cache node (bss 0x4d5b40, 16 bytes each on the
 * EE, 32 of them; the used list is circular through a sentinel at 0x4d5d40).
 * layout: host -- pointers. */
typedef struct RxFileCdCacheNode RxFileCdCacheNode;
struct RxFileCdCacheNode
{
    RxFileCdCacheNode *prev;    // FS2 0x0
    RxFileCdCacheNode *next;    // FS2 0x4, also the free-list link
    unsigned char     *buf;     // FS2 0x8, 0x800 bytes
    int                sector;  // FS2 0xc
};

/* The cdrom driver's file lookup (data 0x2f5acc, default rxFileCdFind):
 * 1 found, -2 tray opened, -3 not found. */
typedef int (*RxFileCdFindFunc)(const char *name, int *lsn, int *size);

/* cdrom0: driver */
void   rxFileCdWait(void);                                          /* FS2 0x001565c8 */
void   rxFileCdSyncWait(void);                                      /* FS2 0x00156618 */
void   rxFileCdInit(RxFileDriver *drv);                             /* FS2 0x00156648 */
void   rxFileCdTerm(void);                                          /* FS2 0x001566d8 */
int    rxFileCdStatusWait(void);                                    /* FS2 0x001566e0 */
void   rxFileCdTray(int req);                                       /* FS2 0x00156710 */
void   rxFileCdProcess(int step);                                   /* FS2 0x00156730 */
void   rxFileCdMakePath(char *out, const char *name);               /* FS2 0x001569f8 */
int    rxFileCdControl(int cmd, int arg);                           /* FS2 0x00156a40 */
int    rxFileCdGetStatus(int what);                                 /* FS2 0x00156b08 */
int    rxFileCdGetSize(const char *name);                           /* FS2 0x00156b88 */
int    rxFileCdCreate(const char *name, int truncate);              /* FS2 0x00156c00 */
int    rxFileCdRead(const char *name, void *buf, int offset, int size); /* FS2 0x00156c08 */
int    rxFileCdWrite(const char *name, int offset, const void *buf, int size); /* FS2 0x00156cf0 */
void   rxFileCdIsoName(char *out, const char *name);                /* FS2 0x00156cf8 */
int    rxFileCdMapSector(int lsn);                                  /* FS2 0x00156dd0 */
int    rxFileCdReadBytes(void *buf, long long offset, int size);    /* FS2 0x00156e08 */
int    rxFileCdReadAt(void *buf, long long offset, int size);       /* FS2 0x00157090 */
int    rxFileCdReadSectors(void *buf, int lsn, int count);          /* FS2 0x00157150 */
void   rxFileCdInitCache(void);                                     /* FS2 0x00157390 */
void   rxFileCdFlushCache(void);                                    /* FS2 0x001573f0 */
int    rxFileCdFind(const char *name, int *lsn, int *size);         /* FS2 0x00157458 */
int    rxFileCdFindDir(int tableOffset, int tableSize, const char *path, int *lsn, int *size); /* FS2 0x00157598 */
int    rxFileCdFindInDir(int dirOffset, const char *name, int *lsn, int *size); /* FS2 0x00157780 */
const char *rxFileCdPathElem(char *out, const char *path);          /* FS2 0x00157950 */
int    rxFileCdIsLastElem(const char *path);                        /* FS2 0x001579b0 */
int    rxFileCdReadCached(void *out, int offset, int size);         /* FS2 0x00157a00 */
char  *rxFileCdGetRoot(void);                                       /* FS2 0x00157c28 */
void   rxFileCdSetRoot(const char *root);                           /* FS2 0x00157c38 */
RxFileCdFindFunc rxFileCdSetFindFunc(RxFileCdFindFunc func);        /* FS2 0x00157c40 */
int    rxFileCdSetClockRead(int enable);                            /* FS2 0x00157c58 */
int    rxFileCdGetClockRead(void);                                  /* FS2 0x00157ca0 */
int    rxFileCdGetClock(void *clock);                               /* FS2 0x00157cb0 */
int    rxFileCdSetLayerMode(int mode);                              /* FS2 0x00157d70 */
int    rxFileCdGetLayerMode(void);                                  /* FS2 0x00157da8 */

#ifdef __cplusplus
}
#endif

#endif /* _RX_FILECD_H */
