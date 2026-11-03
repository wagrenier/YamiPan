// game/s2ddata.cpp -- the S2D file a UiLayout plays: relocating it in place
// and looking up its strings, variables, objects, sequences and images
// (game/s2ddata.h).

#include "game/s2ddata.h"

#include <string.h>

#include "game/uisequence.h"
#include "game/uitrack.h"
#include "sdk/scetypes.h"

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024d530
// callcheck: get (S2dRef, inline)

const void *S2dData::getImage(int index)
{
    S2dImage *image = index < imageCount ? &images[index] : nullptr;
    return image != nullptr ? image->data.get() : nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024d570

S2dImage *S2dData::getImageEntry(int index)
{
    return index < imageCount ? &images[index] : nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024d5a0

int S2dData::findSequence(const char *name)
{
    for (int i = 0; i < seqCount; i++) {
        if (strcmp(seqs[i]->name, name) == 0)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024d640

int S2dData::findObject(const char *name)
{
    for (int i = 0; i < objectCount; i++) {
        if (strcmp(objects[i]->name, name) == 0)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024d6e0

int S2dData::findVarIndex(const char *name)
{
    for (int i = 0; i < varCount; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return i;
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024d770

S2dVar *S2dData::findVar(const char *name)
{
    for (int i = 0; i < varCount; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return &vars[i];
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024d810

S2dVar *S2dData::getVar(int index)
{
    return &vars[index];
}

// ──────────────────────────────────────────────────────────────────────
// relocate's pieces, inline in the ROM.

// A table field: an offset from the file's start.
template <class T>
static inline T *relocateTable(S2dData *data, S2dRef<T> &field)
{
    field.set((char *)data + field.rel);
    return field.get();
}

// A table entry: an offset from its table's start.
template <class T>
static inline void relocateEntry(const void *table, S2dRef<T> &entry)
{
    entry.set((const char *)table + entry.rel);
}

// A name: a strings index (relocated first), out of range null.
static inline void relocateName(S2dData *data, S2dRef<const char> &name)
{
    name.set(data->getString(name.rel));
}

// The textured kinds: a name at +8 in every one of their records.
static inline bool isTexturedKind(int kind)
{
    return kind == UiTrackKind_TexSprite || kind == UiTrackKind_SheetSprite
        || kind == UiTrackKind_TexTriangles || kind == UiTrackKind_TexStrip
        || kind == UiTrackKind_TexSpriteFlat || kind == UiTrackKind_SheetSpriteFlat
        || kind == UiTrackKind_TexTrianglesFlat || kind == UiTrackKind_TexStripFlat
        || kind == UiTrackKind_TexTrianglesNoTest;
}

// The names in a track record.
static void relocateTrack(S2dData *data, S2dTrackRec *rec)
{
    if (isTexturedKind(rec->kind))
        relocateName(data, ((S2dTexSpriteRec *)rec)->label);
    // Only kind 6: the flat sheet sprite's name (0x22, +0x14 as well)
    // stays an index in the ROM.
    if (rec->kind == UiTrackKind_SheetSprite)
        relocateName(data, ((S2dSheetSpriteRec *)rec)->name);
    if (rec->kind == UiTrackKind_PinRot || rec->kind == UiTrackKind_PinRot2)
        relocateName(data, ((S2dPinRec *)rec)->target);
    if (rec->kind == UiTrackKind_RotateTo)
        relocateName(data, ((S2dRotateRec *)rec)->target);
}

// ──────────────────────────────────────────────────────────────────────
// Once per file: every offset and name becomes a pointer, in place
// (S2dRef on the host). Already relocated: nothing to do.
// FS2 0x0024d820
// callcheck: getString, relocateEntry, relocateName, relocateTable, relocateTrack (inline helpers)

bool S2dData::relocate()
{
    if (magic[0] != 'S' || magic[1] != '2' || magic[2] != 'D' || magic[3] != '\0')
        return false;
    if (relocated != 0)
        return true;
    relocated = 1;

    // The strings first: every name below is an index into them.
    S2dRef<const char> *stringTable = relocateTable(this, strings);
    for (int i = 0; i < stringCount; i++)
        relocateEntry(stringTable, stringTable[i]);

    relocateTable(this, vars);

    // The track records' offsets are from the objects table as well.
    S2dRef<S2dObject> *objectTable = relocateTable(this, objects);
    for (int i = 0; i < objectCount; i++) {
        relocateEntry(objectTable, objectTable[i]);
        S2dObject *obj = objectTable[i];
        for (u_int t = 0; t < obj->trackCount; t++) {
            relocateEntry(objectTable, obj->tracks[t]);
            relocateTrack(this, obj->tracks[t]);
        }
        relocateName(this, obj->name);
    }

    S2dRef<S2dActorDef> *actorTable = relocateTable(this, actorDefs);
    for (int i = 0; i < actorDefCount; i++) {
        relocateEntry(actorTable, actorTable[i]);
        relocateName(this, actorTable[i]->name);
    }

    S2dRef<S2dAnimSet> *animSetTable = relocateTable(this, animSets);
    for (int i = 0; i < animSetCount; i++) {
        relocateEntry(animSetTable, animSetTable[i]);
        relocateName(this, animSetTable[i]->name);
    }

    S2dWidgetDef *widgetTable = relocateTable(this, widgetDefs);
    for (int i = 0; i < widgetDefCount; i++) {
        relocateName(this, widgetTable[i].name);
        relocateName(this, widgetTable[i].type);
    }

    S2dRef<S2dSeqDef> *seqTable = relocateTable(this, seqs);
    for (int i = 0; i < seqCount; i++) {
        relocateEntry(seqTable, seqTable[i]);
        relocateName(this, seqTable[i]->name);
    }

    S2dImage *imageTable = relocateTable(this, images);
    for (int i = 0; i < imageCount; i++) {
        relocateEntry(imageTable, imageTable[i].data);
        relocateName(this, imageTable[i].name);
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024df60

const char *S2dData::getString(int index)
{
    if (index < 0)
        return nullptr;
    if (index < stringCount)
        return strings[index];
    return nullptr;
}
