/* ==========================================================================
 *  game/pakarchive.h
 *
 *  The "pak" reader (unassigned@00231410: the pak parser 0x231410..
 *  0x231bb0, not written yet): a pack file in memory, opened in place, its
 *  items found by id or by name. PackFile (game/fileloader.h) holds one at
 *  +0xc4 and forwards its file slots to it. Stand-in: only what
 *  game/fileloader.cpp calls; the parser's TU completes it.
 * ======================================================================== */
#ifndef _GAME_PAKARCHIVE_H
#define _GAME_PAKARCHIVE_H

#include <stddef.h>

#include "sdk/scetypes.h"

// The pack file's header, "pak\0" (0x231980 checks the four bytes). Only
// the fields game/fileloader.cpp reads; open relocates the 0x10-byte item
// entries from +0x20 (name / data offsets made pointers, items outside the
// variant's mask dropped and the kept data packed together).
struct PakHeader
{
    char  magic[4];             // FS2 +0x00: "pak\0"
    int   _04;                  // FS2 +0x04: open relocates only while 0 (unread)
    int   _08;                  // FS2 +0x08: the name strings' offset (unread)
    int   _0c;                  // FS2 +0x0c: the item data's offset (unread)
    int   count;                // FS2 +0x10: items
    int   _14;                  // FS2 +0x14 (unread)
    int   _18;                  // FS2 +0x18 (unread)
    u_int size;                 // FS2 +0x1c: the bytes PackFile::compact keeps
};
static_assert(offsetof(PakHeader, count) == 0x10, "PakHeader::count");
static_assert(offsetof(PakHeader, size) == 0x1c, "PakHeader::size");

// A pack in memory. The data is the owner's (PackJob's job data or its
// unpack buffer); the destructor frees nothing.
// layout: host -- EE 0xc: +0 data, +4 (cleared by open), +8 open's result.
class PakArchive
{
public:
    PakArchive();                                                        // FS2 0x002315c0: no data
    ~PakArchive();                                                       // FS2 0x00231570: deleting (obj, short)
    // Relocates the pack at data in place and keeps the items whose
    // variant mask has bit variant (-1: every item); false when the magic
    // is wrong.
    bool open(void *data, int variant);                                  // FS2 0x00231520
    int         getCount();                                              // FS2 0x00231410: 0 without data
    int         getItemSize(int id);                                     // FS2 0x00231430
    int         getSize(const char *name);                               // FS2 0x00231450: -1 without one
    void       *getItem(int id);                                         // FS2 0x00231480
    void       *getData(const char *name);                               // FS2 0x002314a0: null without one
    const char *getName(int index);                                      // FS2 0x002314d0
    int         findIndex(const char *name);                             // FS2 0x002314f0: -1 without one

    PakHeader *m_data;          // FS2 +0x0
    int        _04;             // FS2 +0x4: cleared by open (unread)
    int        m_result;        // FS2 +0x8: what the relocation answered, < 0 failed
};

#endif /* _GAME_PAKARCHIVE_H */
