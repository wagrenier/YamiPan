/* ==========================================================================
 *  game/pakarchive.h
 *
 *  The "pak" reader (game/pakarchive.cpp, 0x231410..0x231bb0): a pack file
 *  in memory, opened in place, its items found by id or by name. PackFile
 *  (game/fileloader.h) holds one at +0xc4 and forwards its file slots to
 *  it. The names are the port's (no source string).
 *
 *  A pak is a 0x20-byte header and its 0x10-byte item entries; the data of
 *  the first subPakCount entries is a nested pak (a sub-pak). Name and data
 *  offsets become pointers in place on the first PakHeader::relocate. Every
 *  pointer the ROM writes into the file is an S2dRef (game/s2ddata.h), so
 *  the file keeps its EE layout on the host (asserted).
 * ======================================================================== */
#ifndef _GAME_PAKARCHIVE_H
#define _GAME_PAKARCHIVE_H

#include <stddef.h>

#include "game/s2ddata.h"
#include "sdk/scetypes.h"

// An item id, passed in one register: the entry's index in the low half, a
// level in the high half. Only level 0 finds anything: getEntry descends
// with the same id (the ROM's), and findIndex answers {index, 0} for an
// item of a sub-pak as well (the sub-pak's own index). -1 is "none".
struct PakId
{
    u_short index;              // FS2 +0x0: the entry
    u_short level;              // FS2 +0x2: 0 the pak's own entries

    static PakId fromInt(int id)
    {
        PakId pid;
        pid.index = (u_short)((u_int)id & 0xffff);
        pid.level = (u_short)((u_int)id >> 16);
        return pid;
    }
    int toInt() const { return (int)((u_int)level << 16 | index); }
};
static_assert(sizeof(PakId) == 4, "PakId: EE 4 (one register)");

static const int kPakIdNone = -1;

// PakEntry::kind of entry 0 that marks a pak whose items carry variant
// masks (relocate packs only those; the other kinds are not read).
enum PakEntryKind
{
    PakEntryKind_Variants = 4,
};

// An item (0x10 bytes).
struct PakEntry
{
    u_short       kind;         // FS2 +0x0: read on entry 0 only (PakEntryKind)
    u_short       variants;     // FS2 +0x2: bit n kept for variant n; 0 always kept, never moved
    S2dRef<char>  name;         // FS2 +0x4: file: offset from the header + nameBase
    S2dRef<void>  data;         // FS2 +0x8: file: offset from the header + dataBase; null when dropped
    int           size;         // FS2 +0xc: bytes (read as the last item's size by relocate)
};
static_assert(sizeof(PakEntry) == 0x10, "PakEntry: EE 0x10");
static_assert(offsetof(PakEntry, name) == 0x4, "PakEntry::name");
static_assert(offsetof(PakEntry, data) == 0x8, "PakEntry::data");
static_assert(offsetof(PakEntry, size) == 0xc, "PakEntry::size");

// The pack file's header, "pak\0"; the entries follow it.
struct PakHeader
{
    char  magic[4];             // FS2 +0x00: "pak\0"
    int   relocated;            // FS2 +0x04: 1 once relocate made the offsets pointers
    int   nameBase;             // FS2 +0x08: added to each entry's name offset
    int   dataBase;             // FS2 +0x0c: the item data's offset from the header
    int   count;                // FS2 +0x10: entries
    int   _14;                  // FS2 +0x14 (unread)
    int   subPakCount;          // FS2 +0x18: the first entries whose data is a sub-pak
    u_int size;                 // FS2 +0x1c: dataBase + the kept data after a variant pack (PackFile::compact keeps it)

    PakEntry   *entries() { return (PakEntry *)(this + 1); }             // FS2 +0x20
    PakHeader  *subPak(int i) { return (PakHeader *)entries()[i].data.get(); }

    int         getItemSize(PakId id);                                   // FS2 0x002315d0: -1 without one
    void       *getItem(PakId id);                                       // FS2 0x00231600: null without one
    const char *getName(PakId id);                                       // FS2 0x00231630: null without one
    PakId       findIndex(const char *name);                             // FS2 0x00231660: -1 without one
    PakEntry   *findEntry(const char *name);                             // FS2 0x00231770: null without one
    PakEntry   *getEntry(PakId id);                                      // FS2 0x00231860: null without one
    int         relocate(int variant);                                   // FS2 0x00231980: variant, -1 bad magic
};
static_assert(sizeof(PakHeader) == 0x20, "PakHeader: EE 0x20 (the entries follow)");
static_assert(offsetof(PakHeader, count) == 0x10, "PakHeader::count");
static_assert(offsetof(PakHeader, subPakCount) == 0x18, "PakHeader::subPakCount");
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
    // variant mask has bit variant (-1: every item). Answers result >= 0:
    // false for a wrong magic (of the pak or a sub-pak), and with variant
    // -1 always (relocate answers the variant; PackJob::load tests the
    // count instead).
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
