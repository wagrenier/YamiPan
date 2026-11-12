// game/pakarchive.cpp -- the "pak" reader (game/pakarchive.h).

#include "game/pakarchive.h"

#include <string.h>

#include "sdk/scetypes.h"

// ──────────────────────────────────────────────────────────────────────
// PakArchive: one pak in memory, forwarding to its header. getCount and
// findIndex test for data; the others read through it.
// FS2 0x00231410

int PakArchive::getCount()
{
    return m_data == nullptr ? 0 : m_data->count;
}

// The host guards (null data) stand for the ROM's read of low EE memory
// through a null pak.
// FS2 0x00231430

int PakArchive::getItemSize(int id)
{
    if (m_data == nullptr)
        return -1;
    return m_data->getItemSize(PakId::fromInt(id));
}

// FS2 0x00231450

int PakArchive::getSize(const char *name)
{
    PakEntry *entry = m_data != nullptr ? m_data->findEntry(name) : nullptr;
    return entry == nullptr ? -1 : entry->size;
}

// FS2 0x00231480

void *PakArchive::getItem(int id)
{
    if (m_data == nullptr)
        return nullptr;
    return m_data->getItem(PakId::fromInt(id));
}

// FS2 0x002314a0

void *PakArchive::getData(const char *name)
{
    PakEntry *entry = m_data != nullptr ? m_data->findEntry(name) : nullptr;
    return entry == nullptr ? nullptr : entry->data.get();
}

// FS2 0x002314d0

const char *PakArchive::getName(int index)
{
    if (m_data == nullptr)
        return nullptr;
    return m_data->getName(PakId::fromInt(index));
}

// FS2 0x002314f0

int PakArchive::findIndex(const char *name)
{
    if (m_data == nullptr)
        return kPakIdNone;
    return m_data->findIndex(name).toInt();
}

// FS2 0x00231520

bool PakArchive::open(void *data, int variant)
{
    m_data = (PakHeader *)data;
    _04 = 0;
    m_result = m_data->relocate(variant);
    return m_result >= 0;
}

// callcheck: operator_delete (the deleting dtor's delete, implicit)
// FS2 0x00231570

PakArchive::~PakArchive()
{
}

// FS2 0x002315c0

PakArchive::PakArchive()
    : m_data(nullptr)
{
}

// ──────────────────────────────────────────────────────────────────────
// PakHeader: the lookups by id.
// FS2 0x002315d0

int PakHeader::getItemSize(PakId id)
{
    PakEntry *entry = getEntry(id);
    return entry == nullptr ? -1 : entry->size;
}

// FS2 0x00231600

void *PakHeader::getItem(PakId id)
{
    PakEntry *entry = getEntry(id);
    return entry == nullptr ? nullptr : entry->data.get();
}

// FS2 0x00231630

const char *PakHeader::getName(PakId id)
{
    PakEntry *entry = getEntry(id);
    return entry == nullptr ? nullptr : entry->name.get();
}

// ──────────────────────────────────────────────────────────────────────
// The lookups by name: the first entry whose name contains name, this
// pak's entries before its sub-paks'. findIndex answers a sub-pak's hit
// as that sub-pak's own index (level 0), as the ROM.
// FS2 0x00231660

PakId PakHeader::findIndex(const char *name)
{
    for (int i = 0; i < count; i++) {
        if (strstr(entries()[i].name.get(), name) != nullptr) {
            PakId id;
            id.index = (u_short)i;
            id.level = 0;
            return id;
        }
    }
    for (int i = 0; i < subPakCount; i++) {
        PakId id = subPak(i)->findIndex(name);
        if (id.toInt() != kPakIdNone)
            return id;
    }
    return PakId::fromInt(kPakIdNone);
}

// FS2 0x00231770

PakEntry *PakHeader::findEntry(const char *name)
{
    for (int i = 0; i < count; i++) {
        if (strstr(entries()[i].name.get(), name) != nullptr)
            return &entries()[i];
    }
    for (int i = 0; i < subPakCount; i++) {
        PakEntry *entry = subPak(i)->findEntry(name);
        if (entry != nullptr)
            return entry;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Level 0 is this pak's entry (not bounds-checked). Any other level
// descends into the first sub-pak with the same id until a pak has none:
// null (the ROM's, its recursion unrolled three times).
// FS2 0x00231860

PakEntry *PakHeader::getEntry(PakId id)
{
    if (id.level == 0)
        return &entries()[id.index];
    if (subPakCount < 1)
        return nullptr;
    return subPak(0)->getEntry(id);
}

// ──────────────────────────────────────────────────────────────────────
// Makes the offsets pointers, once (the relocated flag), then the
// sub-paks'. With a variant (>= 0) and entry 0 of kind
// PakEntryKind_Variants, the items whose mask lacks the variant's bit are
// dropped (data null) and the kept ones packed from the data's start;
// size becomes the header and names plus the kept data. Answers variant,
// or -1 for a wrong magic (passed on to the later sub-paks, which then
// keep every item).
// callcheck: memcpy (memmove on the host: the packed copy can overlap its source)
// FS2 0x00231980

int PakHeader::relocate(int variant)
{
    if (!(magic[0] == 'p' && magic[1] == 'a' && magic[2] == 'k' && magic[3] == '\0'))
        return -1;

    if (relocated == 0) {
        char *base = (char *)this;
        int names = nameBase;
        char *dst = base + dataBase;
        PakEntry *entry = entries();
        for (int i = 0; i < count; i++) {
            entry[i].name.set(base + entry[i].name.rel + names);
            entry[i].data.set(dst + entry[i].data.rel);
        }

        if (variant >= 0 && entry[0].kind == PakEntryKind_Variants) {
            int kept = 0;
            for (int i = 0; i < count; i++) {
                // An item's size is the gap to the next one's data; the
                // last one's is its own size field.
                int itemSize = i == count - 1
                    ? entry[i].size
                    : (int)((char *)entry[i + 1].data.get() - (char *)entry[i].data.get());
                if (entry[i].variants == 0)
                    continue;
                if ((entry[i].variants & (1u << (variant & 0x1f))) == 0) {
                    entry[i].data.set(nullptr);
                } else {
                    if (entry[i].data.get() != dst)
                        memmove(dst, entry[i].data.get(), itemSize);
                    entry[i].data.set(dst);
                    dst += itemSize;
                    kept += itemSize;
                }
            }
            size = kept + dataBase;
        }
        relocated = 1;
    }

    for (int i = 0; i < subPakCount; i++) {
        // Host guard: a sub-pak dropped by its variant mask is null; the
        // ROM reads low EE memory there, whose magic fails as well.
        PakHeader *sub = subPak(i);
        variant = sub != nullptr ? sub->relocate(variant) : -1;
    }
    return variant;
}
