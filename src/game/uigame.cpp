// game/uigame.cpp -- the S2D UI's game side: the text font, the fMsg
// message files and TimM sprite sheets, and the hooks the layouts and
// their tracks call into the game (game/uigame.h).

#include "game/uigame.h"

#include <ctype.h>
#include <string.h>

#include "game/gamehooks.h"
#include "game/gsutil.h"
#include "game/sysdata.h"
#include "game/uilayout.h"
#include "game/uitrack.h"
#include "sdk/scetypes.h"

// The target the rotate-to and pin-rot tracks follow (S2dRotateRec /
// S2dPinRec::target): the player character. The spelling is the game's.
static const char kUiTargetPlayer[] = "rCharactorPostion";

// The target's yaw outside a stage: a little under pi / 2 (0x3fc90e56).
constexpr float kUiTargetDefaultYaw = 1.57075f;
// The target's position outside a stage (world units, before the / 10).
constexpr float kUiTargetDefaultX = 6000.0f;
constexpr float kUiTargetDefaultZ = 5000.0f;
// World units to layout pixels for the pin-rot target.
constexpr float kUiTargetPosScale = 0.1f;
constexpr float kPi = 3.1415927f;

// The fMsg / TimM magics.
static inline bool isFMsg(const char *magic)
{
    return magic[0] == 'f' && magic[1] == 'M' && magic[2] == 's' && magic[3] == 'g';
}

static inline bool isTimM(const char *magic)
{
    return magic[0] == 'T' && magic[1] == 'i' && magic[2] == 'm' && magic[3] == 'M';
}

/* PC: the ROM's libc strcasecmp (0x127ea0, folding with newlib's _ctype_);
   MSVC has no such name. */
static int uiStrCaseCmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int ca = tolower((u_char)*a);
        int cb = tolower((u_char)*b);
        if (ca != cb || ca == 0)
            return ca - cb;
    }
}

// relocate's pieces (fMsg and TimM), inline in the ROM.

// A table field: an offset from the file's start.
template <class T>
static inline T *relocateTable(void *file, S2dRef<T> &field)
{
    field.set((char *)file + field.rel);
    return field.get();
}

// A table entry: an offset from base (the file's start plus a header
// offset).
template <class T>
static inline void relocateEntry(const void *file, int base, S2dRef<T> &entry)
{
    entry.set((const char *)file + base + entry.rel);
}

// A message's name / text: an index into its relocated table, negative
// none.
template <class T>
static inline void relocateIndex(S2dRef<T> &ref, S2dRef<T> *table)
{
    if (ref.rel < 0)
        ref.rel = 0;
    else
        ref.set(table[ref.rel].get());
}

// The player of the current stage; null outside one. Inline in the ROM.
static inline StagePlayer *stagePlayer()
{
    return gStageScene != nullptr ? gStageScene->player : nullptr;
}

// The angle the S2D draws the player's heading at: pi - yaw, wrapped.
static inline float playerHeading()
{
    if (gStageScene == nullptr)
        return kUiTargetDefaultYaw;
    return normalizeAngle(-stagePlayer()->yaw + kPi);
}

// ──────────────────────────────────────────────────────────────────────
// The font.

// FS2 0x0025fa50
// callcheck: operator_delete (the deleting destructor's own delete; the texture's virtual destructor, slot 0, is a call through its vtable)

UiFont::~UiFont()
{
    if (m_texture != nullptr)
        delete m_texture;
}

// FS2 0x0025fac0
// callcheck: operator_new (the new-expression)

UiFont::UiFont(const void *glyphs, const void *image)
{
    m_glyphs = glyphs;
    m_texture = new GsTexture(image);
    m_spacing[0] = 2.0f;
    m_spacing[1] = 0.0f;
    m_text = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// fMsg message files.

// FS2 0x0025fb30
// callcheck: strcasecmp (the host's uiStrCaseCmp)

int fMsgFind(void *data, const char *name)
{
    FMsgFile *file = (FMsgFile *)data;
    if (!fMsgRelocate(file))
        return -1;
    for (int i = 0; i < file->count; i++) {
        if (uiStrCaseCmp(file->entries[i].name, name) == 0)
            return i;
    }
    return -1;
}

// Once per file: the name and text tables' offsets, then every message's
// indices, become pointers in place (S2dRef on the host). Already
// relocated: nothing to do.
// FS2 0x0025fbd0
// callcheck: relocateTable, relocateEntry, relocateIndex (inline helpers)

bool fMsgRelocate(void *data)
{
    FMsgFile *file = (FMsgFile *)data;
    if (file->relocated != 0)
        return true;
    if (!isFMsg(file->magic))
        return false;

    S2dRef<const char> *names = relocateTable(file, file->names);
    for (int i = 0; i < file->nameCount; i++)
        relocateEntry(file, file->nameBase, names[i]);
    S2dRef<const u_short> *texts = relocateTable(file, file->texts);
    for (int i = 0; i < file->textCount; i++)
        relocateEntry(file, file->textBase, texts[i]);

    for (int i = 0; i < file->count; i++) {
        relocateIndex(file->entries[i].name, names);
        relocateIndex(file->entries[i].text, texts);
    }
    file->relocated = 1;
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// TimM sprite sheets.

// FS2 0x0025fd60
// callcheck: strcasecmp (the host's uiStrCaseCmp)

int timSheetFind(void *data, const char *name)
{
    TimSheet *sheet = (TimSheet *)data;
    if (!timSheetRelocate(sheet))
        return -1;
    S2dRef<const char> *names = sheet->names;
    for (int i = 0; i < sheet->spriteCount; i++) {
        if (uiStrCaseCmp(names[sheet->sprites[i].name], name) == 0)
            return i;
    }
    return -1;
}

// Once per sheet: the sprite-name and picture tables. The sprites keep
// their indices (timSheetFind looks their names up).
// FS2 0x0025fe10
// callcheck: relocateTable, relocateEntry (inline helpers)

bool timSheetRelocate(void *data)
{
    TimSheet *sheet = (TimSheet *)data;
    if (sheet->relocated != 0)
        return true;
    if (!isTimM(sheet->magic))
        return false;

    S2dRef<const char> *names = relocateTable(sheet, sheet->names);
    for (int i = 0; i < sheet->nameCount; i++)
        relocateEntry(sheet, sheet->nameBase, names[i]);
    TimSheetImage *images = relocateTable(sheet, sheet->images);
    for (int i = 0; i < sheet->imageCount; i++) {
        relocateEntry(sheet, sheet->imageNameBase, images[i].name);
        relocateEntry(sheet, sheet->imageDataBase, images[i].image);
    }
    sheet->relocated = 1;
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// The layout's sounds: four slots, each a name and (outside a stage) the
// effect player's handle. Inside a stage the stage's own sound plays and
// stops them by name.

// FS2 0x0025ff40
// callcheck: strcmp (SeTable::find, inline in the ROM)

void UiLayout::playSound(int slot, const char *name)
{
    if (slot >= kUiLayoutSoundSlots || slot < 0)
        return;
    if (m_soundName[slot] != nullptr)
        stopSound(slot);

    if (gStageScene == nullptr) {
        SePlayer *player = getSePlayer();
        const SeEntry *se = player->m_table != nullptr ? player->m_table->find(name) : nullptr;
        m_soundHandle[slot] = se != nullptr ? player->play(se, 0x80, 0x80) : -1;
        m_soundName[slot] = name;
    } else {
        if (gStageScene->sound->playNamedAlways(name, 1.0f) == nullptr)
            name = nullptr;
        m_soundName[slot] = name;
    }
}

// -1 does nothing: the "every slot" branch below is unreachable after the
// range check (as in the ROM; UiSubLayout::clear relies on it).
// FS2 0x00260100

void UiLayout::stopSound(int slot)
{
    if (slot >= kUiLayoutSoundSlots || slot < 0)
        return;
    int first = slot;
    int end = slot + 1;
    if (slot < 0) {
        first = 0;
        end = kUiLayoutSoundSlots;
    }
    for (int i = first; i < end; i++) {
        if (m_soundName[i] == nullptr)
            continue;
        if (gStageScene != nullptr) {
            gStageScene->sound->stopNamed(m_soundName[i]);
        } else {
            SeHandle handle = m_soundHandle[i];
            SePlayer *player = getSePlayer();
            if (handle >= 0)
                player->stop(handle);
        }
        m_soundName[i] = nullptr;
        m_soundHandle[i] = -1;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The game values. The command's packed argument: byte 0 says which of
// the three arguments in bytes 1..3 are variable indices (bit i: argument
// i is S2dData::vars[n].value), the others are taken as they are.

// The arguments of a packed game-value command, inline in the ROM (all
// three hooks). first: the variable argument 0 named, else null.
static inline void unpackGameArgs(S2dData *data, u_int packed, int args[3], S2dVar **first)
{
    *first = nullptr;
    for (int i = 0; i < 3; i++) {
        args[i] = (u_char)(packed >> (8 * (i + 1)));
        if ((packed & 0xff) & (1u << i)) {
            S2dVar *var = &data->vars.get()[args[i]];
            args[i] = var->value;
            if (i == 0)
                *first = var;
        }
    }
}

// Stores the game value kind (arguments 1, 2) in the variable argument 0
// names.
// FS2 0x002601f0
// callcheck: unpackGameArgs (inline)

void UiLayout::readGameValue(int kind, u_int packed)
{
    int args[3];
    S2dVar *var;
    unpackGameArgs(m_data, packed, args, &var);
    int value = getGameValue(kind, args[1], args[2], 0);
    // PC: the ROM stores through a null variable (EE address 0x10) when
    // argument 0 is not one; the port drops the value.
    if (var != nullptr)
        var->value = value;
}

// Kind 13 (the stage flags) is written here: argument 0 nonzero ORs the
// bits argument 2 into stage argument 1's flags, and with both unlock
// flags set marks the save, as SystemData::recordStageResult does.
// FS2 0x002602a0
// callcheck: unpackGameArgs (inline)

void UiLayout::writeGameValue(int kind, u_int packed)
{
    int args[3];
    S2dVar *var;
    unpackGameArgs(m_data, packed, args, &var);
    if (kind != GameValue_StageFlags) {
        setGameValue(kind, args[0], args[1], args[2]);
        return;
    }
    if (args[0] == 0)
        return;
    SaveData *save = getSaveData();
    u_char flags = save->stageFlags[args[1]];
    if ((u_int)args[2] & ~(u_int)flags) {
        save->stageFlags[args[1]] = flags | (u_char)args[2];
        if (save->stageFlags[kStageFlagUnlockA] != 0 && save->stageFlags[kStageFlagUnlockB] != 0)
            save->markUnlock468();
    }
}

// Whether the game value kind (arguments 1, 2) is argument 0; kind 13
// tests stage argument 1's flags against mask argument 2.
// FS2 0x002603d0
// callcheck: unpackGameArgs (inline)

bool UiLayout::testGameValue(int kind, u_int packed)
{
    int args[3];
    S2dVar *var;
    unpackGameArgs(m_data, packed, args, &var);
    u_int value;
    if (kind == GameValue_StageFlags)
        value = (u_char)getSaveData()->testStageFlags(args[1], args[2]);
    else
        value = getGameValue(kind, args[1], args[2], args[0]);
    return value == (u_int)args[0];
}

// ──────────────────────────────────────────────────────────────────────
// The tracks' game targets.

// FS2 0x00260a20
// callcheck: normalizeAngle (playerHeading, inline)

void UiRotateToTrack::bindTarget(const char *name)
{
    if (strcmp(name, kUiTargetPlayer) == 0)
        m_targetAngle = playerHeading();
    else
        m_targetAngle = 0.0f;
}

// The player's map position (x, -z) / 10 and heading.
// FS2 0x00260ab0
// callcheck: normalizeAngle (playerHeading, inline)

void UiPinRotTrack::bindTarget(const char *name)
{
    if (strcmp(name, kUiTargetPlayer) != 0) {
        m_pos[0] = 0.0f;
        m_pos[1] = 0.0f;
        m_angle = 0.0f;
        return;
    }
    float z;
    if (gStageScene == nullptr) {
        z = kUiTargetDefaultZ;
        m_pos[0] = kUiTargetDefaultX;
    } else {
        StagePlayer *player = stagePlayer();
        z = player->posZ;
        m_pos[0] = player->posX;
    }
    m_pos[1] = -z;
    m_pos[0] = m_pos[0] * kUiTargetPosScale;
    m_pos[1] = m_pos[1] * kUiTargetPosScale;
    m_angle = playerHeading();
}

// ──────────────────────────────────────────────────────────────────────
// The game hooks.

// FS2 0x00262e60

bool inStage()
{
    return gStageScene != nullptr;
}
