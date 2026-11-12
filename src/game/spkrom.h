/* ==========================================================================
 *  game/spkrom.h
 *
 *  The game's rx user layer and SPK.ROM (game/spkrom.cpp, 0x152750..
 *  0x153108). SPK.ROM is the name index over the disc's archives: magic
 *  'SLPK', a table of entries sorted by name, each naming its archive
 *  (ROM.000.. on layer 0, ROMD.008.. on layer 1) with the byte offset and
 *  size inside it. spkRomLoad reads it whole and installs the three hooks
 *  below, so every file the game opens by name resolves through it.
 * ======================================================================== */
#ifndef _GAME_SPKROM_H
#define _GAME_SPKROM_H

#include <stddef.h>

#include "sdk/scetypes.h"

constexpr u_int kSpkMagic = 0x4b504c53;     // 'SLPK'
constexpr u_int kSpkSizeMask = 0x7fffffff;
constexpr u_int kSpkLayer1 = 0x80000000;    // SpkEntry::size: the archive is on layer 1

// One file (0x10).
struct SpkEntry
{
    u_int name;     // FS2 +0x0: offset into the strings; relocated to a pointer in the ROM
    u_int archive;  // FS2 +0x4: the same
    u_int offset;   // FS2 +0x8: bytes into the archive, on a sector
    u_int size;     // FS2 +0xc: kSpkSizeMask, and kSpkLayer1
};
static_assert(sizeof(SpkEntry) == 0x10, "SpkEntry size");

// The file's head (0x20), its entries after it.
struct SpkHeader
{
    u_int magic;        // FS2 +0x00
    u_int _04;          // FS2 +0x04: 1.0f, unread
    u_int count;        // FS2 +0x08
    u_int strings;      // FS2 +0x0c: where the strings start, from the file's start
    u_int relocated;    // FS2 +0x10: set once by spkRomLoad
    u_int _14[3];       // FS2 +0x14

    SpkEntry entries[1];    // FS2 +0x20
};
static_assert(offsetof(SpkHeader, entries) == 0x20, "SpkHeader::entries");

void rxTerm();                                                           // FS2 0x00152750
void rxInit(const char *bootPath, int prio0, int prio1, int prio2);      // FS2 0x001527a0
void spkRomLoad();                                                       // FS2 0x00152870

// The CD driver's file lookup: names holding "ROM." / "ROMD." are the
// archives themselves, anything else goes through the index.
int  spkLocateFile(const char *name, int *lsn, int *size);               // FS2 0x001529d0
// rx's size and load hooks. -2 when the index has no such file.
int  spkGetFileSize(const char *name);                                   // FS2 0x00152c90
int  spkLoadFile(const char *name, void *buf);                           // FS2 0x00152e20

// rxInit's memory hooks over the global heap.
void  gameRxMemFree(void *ptr);                                          // FS2 0x00153080
void *gameRxMemRealloc(void *ptr, u_int size);                           // FS2 0x00153090
void *gameRxMemAlloc(u_int size);                                        // FS2 0x001530b0

#endif /* _GAME_SPKROM_H */
