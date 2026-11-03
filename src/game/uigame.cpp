// game/uigame.cpp -- the S2D UI's game side: the text font, the fMsg
// message files and TimM sprite sheets, and the hooks the layouts and
// their tracks call into the game (game/uigame.h).

#include "game/uigame.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/gsutil.h"
#include "game/progress.h"
#include "game/stageresult.h"
#include "game/sysdata.h"
#include "game/uilayout.h"
#include "game/uisequence.h"
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

// The game-state names (UiGameState order). In the ROM a table in bss
// (0x599490) that the static initializer 0x310b60 fills from the sdata
// string pointers 0x31aae8..; constant here.
static const char *const kUiGameStateNames[UiState_Count] = {
    "r16_9",
    "rF01", "rF02", "rF03", "rF04", "rF05", "rF06", "rF07", "rF08", "rF09", "rFDF",
    "rI00", "rI01", "rI02", "rI03", "rI04", "rI05", "rI06", "rI07",
    "rW00", "rW01",
    "rDunknown", "rDunlock", "rDblocked", "rDlock", "rDNone",
    "rCheckPoint", "rHint", "rMapP",
    "rMsgUpLimit", "rMsgDownLimit", "rMsgUseScroll",
    "rInGame",
    "rProgramStatusNone", "rProgramStatus00", "rProgramStatus01", "rProgramStatus02", "rProgramStatus03",
    "rMapKey0", "rMapKey1",
    "rConnectedPad",
};

// The navi trigger names a map-key state compares (sdata 0x31ab88..):
// k0..k2, anything else 3.
static const char *const kUiMapKeyNames[3] = { "k0", "k1", "k2" };

// ButtonMap's buttons as the TimM sheets name their icons (0x2fb100).
static const char *const kUiButtonIcons[8] = {
    "AI_L2", "AI_R2", "AI_L1", "AI_R1", "AI_A", "AI_O", "AI_X", "AI_H",
};

// A sprite a sheet-sprite track names by a slot.
// layout: host -- EE 0x14.
struct UiSpriteSlot
{
    const char *name;           // FS2 +0x00: the record's sprite name
    int         slot;           // FS2 +0x04: UiSpriteSlotKind
    const char *prefix;         // FS2 +0x08: around the held item's name
    const char *suffix;         // FS2 +0x0c
    int         _10;            // FS2 +0x10: unread here
};

// UiSpriteSlot::slot.
enum UiSpriteSlotKind
{
    UiSlot_ItemFirst = 0,       // kUiItemSprites: inventory slot n
    UiSlot_ItemLast = 7,
    UiSlot_WeaponFirst = 8,     // kUiItemSprites: weapon n - 8
    UiSlot_WeaponLast = 9,
    UiSlot_KeyFirst = 0,        // kUiKeySprites: the options' map, action n
    UiSlot_KeyLast = 7,
    UiSlot_DraftKeyFirst = 8,   // kUiKeySprites: the draft map, action n - 8
    UiSlot_DraftKeyLast = 15,
};

// The inventory and weapon sprites (0x2faed0, 12; 0x31abe0).
static const UiSpriteSlot kUiItemSprites[] = {
    { "rItem00", 0, "", "", 0 },
    { "rItem01", 1, "", "", 1 },
    { "rItem02", 2, "", "", 2 },
    { "rItem03", 3, "", "", 3 },
    { "rItem04", 4, "", "", 4 },
    { "rItem05", 5, "", "", 5 },
    { "rItem06", 6, "", "", 6 },
    { "rItem07", 7, "", "", 7 },
    { "rWeaponL00", 8, "", "_W", 8 },
    { "rWeaponL01", 9, "", "_W", 9 },
    { "rWeaponS00", 8, "", "", 10 },
    { "rWeaponS01", 9, "", "", 11 },
};
constexpr int kUiItemSpriteCount = sizeof(kUiItemSprites) / sizeof(kUiItemSprites[0]);

// The key-configuration sprites (0x2fafc0, 16; 0x31abe4): their names
// filled by the static initializer from sdata 0x31ab98.. in the ROM.
static const UiSpriteSlot kUiKeySprites[] = {
    { "rKcDecide", 0, "", "", 0 },
    { "rKcCancel", 1, "", "", 1 },
    { "rKcListmenu", 2, "", "", 2 },
    { "rKcLight", 3, "", "", 3 },
    { "rKcLockOn", 4, "", "", 4 },
    { "rKcView", 5, "", "", 5 },
    { "rKcParallel", 6, "", "", 6 },
    { "rKcJack", 7, "", "", 7 },
    { "rKcLDecide", 8, "", "", 0 },
    { "rKcLCancel", 9, "", "", 1 },
    { "rKcLListmenu", 10, "", "", 2 },
    { "rKcLLight", 11, "", "", 3 },
    { "rKcLLockOn", 12, "", "", 4 },
    { "rKcLView", 13, "", "", 5 },
    { "rKcLParallel", 14, "", "", 6 },
    { "rKcLJack", 15, "", "", 7 },
};
constexpr int kUiKeySpriteCount = sizeof(kUiKeySprites) / sizeof(kUiKeySprites[0]);

ButtonMap gUiButtonMapDraft;    /* bss 0x599470 */

// Outside a stage the weapon sprites cycle through the sheet's weapons:
// 0x262840 counts it up (sbss 0x31ae44, gp-0x7a2c).
static int sUiWeaponCycle;

// The item definition at index; null out of range. Inline in the ROM.
static inline const ItemDef *getItemDef(int index)
{
    if (index < 0 || index >= kItemDefCount)
        return nullptr;
    return &gItemDefs[index];
}

// The index of the item definition called name; -1 none. Inline in the
// ROM (findSprite).
static inline int findItemDef(const char *name)
{
    for (int i = 0; i < kItemDefCount; i++) {
        if (strcmp(gItemDefs[i].name, name) == 0)
            return i;
    }
    return -1;
}

// The name of a sheet's sprite index; null out of range. Inline in the
// ROM.
static inline const char *sheetSpriteName(TimSheet *sheet, int index)
{
    const TimSheetSprite *sprite = timSheetSprite(sheet, index);
    if (sprite == nullptr)
        return nullptr;
    S2dRef<const char> *names = sheet->names;
    return names[sprite->name];
}

// The sheet's sprite called name; null none. Inline in the ROM.
static inline const TimSheetSprite *sheetFindSprite(TimSheet *sheet, const char *name)
{
    int index = timSheetFind(sheet, name);
    return timSheetSprite(sheet, index);
}

// Whether the map shows where the player is (GameOptions::mapMode; in
// between, the player's map holder). Inline in the ROM (testGameState,
// twice).
static inline u_char mapShowsPlayer()
{
    GameOptions *options = getGameOptions();
    if (options->mapMode == MapMode_Always)
        return 1;
    if (options->mapMode == MapMode_Never)
        return 0;
    if (gStageScene == nullptr)
        return 0;
    StagePlayer *player = stagePlayer();
    if (player == nullptr)
        return 0;
    if (player->map == nullptr)
        return 0;
    return (*player->map->state)->showsPlayer;
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
// The game states.

// The S2D condition "game state name (arg)": UiGameState by name, an
// unknown name false. Outside a stage the states answer as a preview
// would show them (items all there, weapons carried).
// FS2 0x002604a0
// callcheck: getGameOptions, getSaveData (mapShowsPlayer and the SystemData casts, inline)

bool UiLayout::testGameState(const char *name, const char *arg, UiSequence *seq)
{
    int state = -1;
    for (int i = 0; i < UiState_Count; i++) {
        if (strcmp(name, kUiGameStateNames[i]) == 0) {
            state = i;
            break;
        }
    }
    if (state < 0)
        return false;

    // The numbered states first (the ROM's switch has a case each).
    if (state >= UiState_FloorFirst && state <= UiState_FloorLast) {
        int floor;
        if (gStageScene == nullptr) {
            floor = 0;
        } else {
            floor = gStageScene->getFloor(stagePlayer());
            if (floor < 0)
                floor = 0;
        }
        if (!mapShowsPlayer())
            floor = 0;
        return state - UiState_FloorFirst == floor;
    }
    if (state >= UiState_ItemFirst && state <= UiState_ItemLast) {
        int kinds;
        if (gStageScene == nullptr) {
            kinds = kUiItemSlots;
        } else {
            UiItemSlot slots[kUiItemSlots];
            kinds = scanItems(slots);
        }
        return state - UiState_ItemFirst < kinds;
    }
    if (state >= UiState_DoorFirst && state <= UiState_DoorLast) {
        if (arg == nullptr)
            return false;
        return state - UiState_DoorFirst == getDoorState(arg);
    }
    if (state >= UiState_ProgramStatusNone && state <= UiState_ProgramStatusLast) {
        // The ROM looks arg up even when it is null.
        int status = -1;
        UiWidget *widget = seq->findWidgetByName(arg);
        if (widget != nullptr)
            status = widget->getStatus();
        return status == state - UiState_ProgramStatusNone - 1;
    }

    switch (state) {
    case UiState_WideScreen:
        return getGameOptions()->wideScreen == 1;

    case UiState_WeaponLong:
    case UiState_WeaponShort:
        if (gStageScene == nullptr)
            return true;
        return stagePlayer()->weapon[state - UiState_WeaponLong] != nullptr;

    case UiState_CheckPoint:
        if (gStageScene == nullptr)
            return false;
        return getStageResult()->hasAttemptFlag();

    case UiState_Hint:
        return getGameOptions()->hints == 1;

    case UiState_MapShowsPlayer:
        return mapShowsPlayer() != 0;

    case UiState_MsgUpLimit:
    case UiState_MsgDownLimit: {
        if (arg == nullptr)
            return false;
        // The ROM takes the widget to be a scroll widget.
        UiScrollWidget *widget = static_cast<UiScrollWidget *>(seq->findWidgetByName(arg));
        if (widget == nullptr)
            return false;
        if (state == UiState_MsgUpLimit)
            return widget->atTop();
        return widget->atBottom();
    }

    case UiState_MsgUseScroll: {
        if (arg == nullptr)
            return false;
        UiScrollWidget *widget = static_cast<UiScrollWidget *>(seq->findWidgetByName(arg));
        if (widget == nullptr)
            return false;
        return widget->canScroll();
    }

    case UiState_InGame:
        return gStageScene != nullptr;

    case UiState_MapKey0:
    case UiState_MapKey1: {
        int key = state - UiState_MapKey0;
        SystemData *data = reinterpret_cast<SystemData *>(getSaveData());
        int trigger;
        if (gStageScene == nullptr && !data->select.isValid()) {
            // No stage chosen yet: key 0 ready, key 1 fired.
            trigger = key == 0 ? NaviTrigger_Ready : NaviTrigger_Fired;
        } else {
            NaviTable *navi = getNaviTable();
            // PC: the ROM calls through a null table while the init pack
            // is not loaded; the port answers none.
            trigger = navi != nullptr
                ? navi->getTriggerState(data->select.stage, data->select.variant, key)
                : NaviTrigger_None;
        }
        if (arg == nullptr)
            return false;
        int expected = 3;
        for (int i = 0; i < 3; i++) {
            if (strcmp(arg, kUiMapKeyNames[i]) == 0)
                expected = i;
        }
        return expected == trigger;
    }

    case UiState_ConnectedPad: {
        GamePad &pad = gGameSystem->m_pad0;
        return pad.m_connected != 0 && pad.m_termMode == PadTerm_DualShock2;
    }

    default:
        return false;
    }
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
// The sheet sprites.

// The sprite the record names: a key-configuration icon (the button an
// action has), the held item or weapon in a slot ("<prefix><item
// name><suffix>"), else nothing. Outside a stage the item slots show the
// sheet's n-th item and the weapon slots cycle through its weapons.
// FS2 0x00261d00
// callcheck: getSaveData, strcmp, timSheetRelocate, timSheetFind, sprintf (findItemDef, sheetSpriteName, sheetFindSprite, inline)

const TimSheetSprite *UiSheetSpriteTrack::findSprite()
{
    const S2dSheetSpriteRec *rec = (const S2dSheetSpriteRec *)m_rec;
    char name[0x80];

    // A key-configuration sprite (the last of that name).
    const UiSpriteSlot *key = nullptr;
    for (int i = 0; i < kUiKeySpriteCount; i++) {
        if (strcmp(rec->name, kUiKeySprites[i].name) == 0)
            key = &kUiKeySprites[i];
    }
    if (key != nullptr) {
        name[0] = '\0';
        const char *icon;
        int slot = key->slot;
        if (slot >= UiSlot_KeyFirst && slot <= UiSlot_KeyLast) {
            SystemData *data = reinterpret_cast<SystemData *>(getSaveData());
            icon = kUiButtonIcons[data->options.buttonMap.button[slot]];
        } else if (slot >= UiSlot_DraftKeyFirst && slot <= UiSlot_DraftKeyLast) {
            icon = kUiButtonIcons[gUiButtonMapDraft.button[slot - UiSlot_DraftKeyFirst]];
        } else {
            icon = nullptr;
        }
        if (icon != nullptr)
            sprintf(name, "%s", icon);
        // No icon: the ROM still looks up the empty name.
        return sheetFindSprite(m_sheet, name);
    }

    if (gStageScene == nullptr) {
        // The preview: the first slot of that name.
        const UiSpriteSlot *item = nullptr;
        for (int i = 0; i < kUiItemSpriteCount; i++) {
            if (strcmp(rec->name, kUiItemSprites[i].name) == 0) {
                item = &kUiItemSprites[i];
                break;
            }
        }
        int count = m_sheet->spriteCount;   // read before the sheet is relocated, as in the ROM
        if (item == nullptr || count == 0)
            return nullptr;

        int slot = item->slot;
        if (slot >= UiSlot_ItemFirst && slot <= UiSlot_ItemLast) {
            // The sheet's (slot + 1)-th item sprite.
            int seen = 0;
            for (int i = 0; i < count; i++) {
                const ItemDef *def = getItemDef(findItemDef(sheetSpriteName(m_sheet, i)));
                if (def != nullptr && def->kind >= ItemKind_ItemFirst)
                    seen++;
                if (seen == slot + 1)
                    return timSheetSprite(m_sheet, i);
            }
            return nullptr;
        }
        if (slot >= UiSlot_WeaponFirst && slot <= UiSlot_WeaponLast) {
            int weapons = 0;
            for (int i = 0; i < count; i++) {
                const ItemDef *def = getItemDef(findItemDef(sheetSpriteName(m_sheet, i)));
                if (def != nullptr && def->kind < ItemKind_ItemFirst)
                    weapons++;
            }
            // PC: the ROM divides by zero when the sheet has no weapon
            // (and then finds none).
            if (weapons == 0)
                return nullptr;
            int pick = (slot + sUiWeaponCycle - UiSlot_WeaponFirst) % weapons;
            const char *held = nullptr;
            int seen = 0;
            for (int i = 0; i < count; i++) {
                const ItemDef *def = getItemDef(findItemDef(sheetSpriteName(m_sheet, i)));
                if (def != nullptr && def->kind < ItemKind_ItemFirst && ++seen == pick + 1) {
                    held = sheetSpriteName(m_sheet, i);
                    break;
                }
            }
            if (held == nullptr)
                return nullptr;
            sprintf(name, "%s%s%s", item->prefix, held, item->suffix);
            return sheetFindSprite(m_sheet, name);
        }
        return nullptr;
    }

    // In a stage: what the player holds in the slot (the last slot of that
    // name).
    const UiSpriteSlot *item = nullptr;
    for (int i = 0; i < kUiItemSpriteCount; i++) {
        if (strcmp(rec->name, kUiItemSprites[i].name) == 0)
            item = &kUiItemSprites[i];
    }
    if (item == nullptr)
        return nullptr;
    name[0] = '\0';
    int slot = item->slot;
    const char *held;
    if (slot >= UiSlot_ItemFirst && slot <= UiSlot_ItemLast) {
        UiItemSlot items[kUiItemSlots];
        scanItems(items);
        held = items[slot].name;
    } else if (slot >= UiSlot_WeaponFirst && slot <= UiSlot_WeaponLast) {
        UiWeaponSlot weapons[kUiWeaponSlots];
        scanWeapons(weapons);
        held = weapons[slot - UiSlot_WeaponFirst].name;
    } else {
        held = nullptr;
    }
    if (held == nullptr)
        return nullptr;
    sprintf(name, "%s%s%s", item->prefix, held, item->suffix);
    return sheetFindSprite(m_sheet, name);
}

// ──────────────────────────────────────────────────────────────────────
// The game hooks.

// FS2 0x002626f0

int getDoorState(const char *name)
{
    if (gStageScene == nullptr)
        return (signed char)name[10] % 5;
    StageObject *door = gStageScene->objects->find(name, 0);
    if (door == nullptr)
        return UiDoor_None;
    if (door->getKind() != StageObject_Door)
        return UiDoor_None;
    switch (door->m_doorState) {
    case StageDoor_Blocked:
        return UiDoor_Blocked;
    case StageDoor_Locked:
        return UiDoor_Locked;
    case StageDoor_Open2:
    case StageDoor_Open1:
        return UiDoor_Unlocked;
    case StageDoor_Unknown:
        return UiDoor_Unknown;
    default:
        return UiDoor_None;
    }
}

// The player's items in list order, merged by name into at most eight
// slots (an item that finds no slot ends the scan); the kinds found.
// FS2 0x002628a0
// callcheck: strcmp; getItemDef (inline)

int scanItems(UiItemSlot slots[kUiItemSlots])
{
    // The ROM leaves max as it is.
    for (u_int i = 0; i < kUiItemSlots; i++) {
        slots[i].used = 0;
        slots[i].name = nullptr;
        slots[i].count = 0;
        slots[i].showsCount = 0;
    }
    StagePlayer *player = stagePlayer();
    int kinds = 0;
    for (StageItemNode *node = player->items; node != nullptr; node = node->next) {
        StageObject *item = *node->item;
        if (item->getKind() == StageObject_Kind21)
            continue;
        UiItemSlot *slot = nullptr;
        for (u_int i = 0; i < kUiItemSlots; i++) {
            if (!slots[i].used || strcmp(slots[i].name, item->m_name) == 0) {
                slot = &slots[i];
                break;
            }
        }
        if (slot == nullptr)
            return kinds;
        if (!slot->used)
            kinds++;
        slot->used = 1;
        slot->name = item->m_name;
        slot->count += item->m_count;
        // PC: the ROM reads near address 0 for an item without a
        // definition; the port answers 0.
        const ItemDef *def = getItemDef(item->m_itemDef);
        slot->max = def != nullptr ? def->maxCount : 0;
        slot->showsCount = def != nullptr && def->stackKind > 1;
    }
    return kinds;
}

// Both weapon slots: the weapon's name and, for a firearm, its two ammo
// numbers.
// FS2 0x00262a90
// callcheck: getItemDef (inline)

int scanWeapons(UiWeaponSlot slots[kUiWeaponSlots])
{
    for (u_int i = 0; i < kUiWeaponSlots; i++) {
        slots[i].used = 0;
        slots[i].name = nullptr;
        slots[i].isFirearm = 0;
        slots[i].ammoA = 0;
        slots[i].ammoB = 0;
    }
    StagePlayer *player = stagePlayer();
    for (int i = 0; i < kUiWeaponSlots; i++) {
        StageObject *weapon = player->weapon[i];
        slots[i].used = 1;
        if (weapon == nullptr)
            continue;
        slots[i].name = weapon->m_name;
        // PC: as in scanItems, no definition reads as no firearm.
        const ItemDef *def = getItemDef(weapon->m_itemDef);
        slots[i].isFirearm = def != nullptr && def->kind >= ItemKind_FirearmFirst &&
                             def->kind <= ItemKind_FirearmLast;
        if (slots[i].isFirearm) {
            slots[i].ammoA = weapon->m_ammoA;
            slots[i].ammoB = weapon->m_ammoB;
        }
    }
    return kUiWeaponSlots;
}

// The stage pack's file called name (FileLoader::getData).
// FS2 0x00262df0

void *getStagePackData(const char *name)
{
    if (gStageScene == nullptr)
        return nullptr;
    StageFiles *files = getStageFiles();
    if (files == nullptr)
        return nullptr;
    if (files->loader == nullptr)
        return nullptr;
    return files->loader->getData(name);
}

// FS2 0x00262e60

bool inStage()
{
    return gStageScene != nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The key-configuration draft.

// FS2 0x00263a70

ButtonMap *getButtonMapDraft()
{
    return &gUiButtonMapDraft;
}
