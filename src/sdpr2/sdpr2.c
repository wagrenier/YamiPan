/* ==========================================================================
 *  sdpr2/sdpr2.c
 *
 *  SDPR2's core and public API (FS2 0x1681c0..0x16c870, data 0x2f5f80,
 *  bss 0x537140..0x537c4c): the global switches, the type registry, the
 *  64 handle slots, and one API call per type function, each checking and
 *  limiting its arguments before it calls the type's table. See sdpr2.h.
 *
 *  FS2-only middleware (not in RoR, no symbols): every name was read from
 *  the code (batch 011). The checks are inline in the ROM, with this file's
 *  own copy of their messages (0x3027b8..0x30291c): sdpr2check.h. Where a
 *  failed check prints twice, or a limit's result is dropped, the ROM does
 *  the same.
 * ======================================================================== */
#include "sdpr2/sdpr2.h"

#include "rx/rxsys.h"
#include "sdpr2/sdpr2check.h"
#include "sdpr2/sdpr2drv.h"
#include "sdpr2/sdpr2pair.h"
#include "sdpr2/sdpr2single.h"

/* ---- data ------------------------------------------------------------- */

static const Sdpr2Type *s_types[SDPR2_TYPE_MAX];    /* data 0x2f5f80: all null in the ELF */

static Sdpr2Slot s_slots[SDPR2_SLOT_MAX];           /* bss 0x537140 */
static int s_surround;                              /* bss 0x537c40: 0 forces both phase signs + */
static int s_pairMode;                              /* bss 0x537c44: 0..5, the pair's detune */
static int s_debug;                                 /* bss 0x537c48: 1 prints the checks */

// ──────────────────────────────────────────────────────────────────────
// The value is stored as given: the limit only reports (the ROM drops
// its result).
// FS2 0x001681c0

void sdpr2SetSurround(int on)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2LimitRange, inline)
    sdpr2LimitRange(on, 0, 1);
    s_surround = on;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00168250

int sdpr2GetSurround(void)
{
    return s_surround;
}

// ──────────────────────────────────────────────────────────────────────
// Stored as given, as sdpr2SetSurround.
// FS2 0x00168260

void sdpr2SetPairMode(int mode)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2LimitRange, inline)
    sdpr2LimitRange(mode, 0, SDPR2_PAIR_MODE_MAX);
    s_pairMode = mode;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001682f0

int sdpr2GetPairMode(void)
{
    return s_pairMode;
}

// ──────────────────────────────────────────────────────────────────────
// This one keeps the limited value.
// FS2 0x00168300

void sdpr2SetDebug(int on)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2LimitRange, inline)
    s_debug = sdpr2LimitRange(on, 0, 1);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00168398

int sdpr2GetDebug(void)
{
    return s_debug;
}

// ──────────────────────────────────────────────────────────────────────
// Defaults, every slot free, then the driver and the two types (each
// registers itself). A failed init is called a second time for the
// answer (SDPR2_TRY), so it runs twice.
// FS2 0x001683a8

int sdpr2Init(const void *config)
{
    int i;

    sdpr2SetSurround(1);
    sdpr2SetPairMode(0);
    sdpr2SetDebug(0);
    for (i = 0; i < SDPR2_SLOT_MAX; i++)
    {
        s_slots[i].type = NULL;
        s_slots[i].voice[0] = SDPR2_VOICE_FREE;
        s_slots[i].voice[1] = SDPR2_VOICE_FREE;
        s_slots[i].flip = 0;
        s_slots[i]._10 = 0;
        s_slots[i].angle = 0;
        s_slots[i]._18 = 0;
        s_slots[i].volume = 0;
        s_slots[i].pitch = SDPR2_PITCH_ONE;
        s_slots[i].spread = 0;
        s_slots[i].state = 0;
    }
    SDPR2_TRY(sdpr2DrvInit(config));
    SDPR2_TRY(sdpr2PairInit());
    SDPR2_TRY(sdpr2SingleInit());
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// -1 when the entry is taken.
// FS2 0x001684a0

int sdpr2RegisterType(int type, const Sdpr2Type *table)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(type, SDPR2_TYPE_MAX));
    if (s_types[type] != NULL)
        return -1;
    s_types[type] = table;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// -1 when the entry is empty.
// FS2 0x001685d8

int sdpr2UnregisterType(int type)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    SDPR2_TRY(sdpr2CheckRange(type, SDPR2_TYPE_MAX));
    if (s_types[type] == NULL)
        return -1;
    s_types[type] = NULL;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The type plays and answers the slot it filled; the slot then gets the
// type, so the handle calls reach the same table.
// FS2 0x00168708

int sdpr2PlayPan(int type, int xvab, int prog, int note, int volume,
                 int deg, int spread)
{
    const Sdpr2Type *table;
    int handle;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(type, SDPR2_TYPE_MAX));
    xvab = sdpr2LimitRange(xvab, 0, SDPR2_XVAB_MAX);
    prog = sdpr2LimitRange(prog, 0, SDPR2_PROG_MAX);
    note = sdpr2LimitRange(note, 0, SDPR2_NOTE_MAX);
    volume = sdpr2LimitRange(volume, 0, SDPR2_VOLUME_MAX);
    deg = sdpr2RoundRange(deg, SDPR2_ANGLE_RANGE);
    spread = sdpr2LimitRange(spread, 0, SDPR2_SPREAD_MAX);
    table = s_types[type];
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->playPan));
    handle = table->playPan(xvab, prog, note, volume, deg, spread);
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    s_slots[handle].type = table;
    return handle;
}

// ──────────────────────────────────────────────────────────────────────
// sdpr2PlayPan without position (the types use volume 0x80, angle 0 and
// spread 0x80).
// FS2 0x00168d38

int sdpr2Play(int type, int xvab, int prog, int note)
{
    const Sdpr2Type *table;
    int handle;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(type, SDPR2_TYPE_MAX));
    xvab = sdpr2LimitRange(xvab, 0, SDPR2_XVAB_MAX);
    prog = sdpr2LimitRange(prog, 0, SDPR2_PROG_MAX);
    note = sdpr2LimitRange(note, 0, SDPR2_NOTE_MAX);
    table = s_types[type];
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->play));
    handle = table->play(xvab, prog, note);
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    s_slots[handle].type = table;
    return handle;
}

// ──────────────────────────────────────────────────────────────────────
// sdpr2PlayPan with a parameter set (0..3).
// FS2 0x00169198

int sdpr2PlayPanEx(int type, int xvab, int prog, int note, int volume,
                   int deg, int spread, int preset)
{
    const Sdpr2Type *table;
    int handle;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(type, SDPR2_TYPE_MAX));
    xvab = sdpr2LimitRange(xvab, 0, SDPR2_XVAB_MAX);
    prog = sdpr2LimitRange(prog, 0, SDPR2_PROG_MAX);
    note = sdpr2LimitRange(note, 0, SDPR2_NOTE_MAX);
    volume = sdpr2LimitRange(volume, 0, SDPR2_VOLUME_MAX);
    deg = sdpr2RoundRange(deg, SDPR2_ANGLE_RANGE);
    spread = sdpr2LimitRange(spread, 0, SDPR2_SPREAD_MAX);
    preset = sdpr2LimitRange(preset, 0, SDPR2_PRESET_MAX);
    table = s_types[type];
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->playPanEx));
    handle = table->playPanEx(xvab, prog, note, volume, deg, spread, preset);
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    s_slots[handle].type = table;
    return handle;
}

// ──────────────────────────────────────────────────────────────────────
// sdpr2Play with a parameter set (0..3).
// FS2 0x00169838

int sdpr2PlayEx(int type, int xvab, int prog, int note, int preset)
{
    const Sdpr2Type *table;
    int handle;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(type, SDPR2_TYPE_MAX));
    xvab = sdpr2LimitRange(xvab, 0, SDPR2_XVAB_MAX);
    prog = sdpr2LimitRange(prog, 0, SDPR2_PROG_MAX);
    note = sdpr2LimitRange(note, 0, SDPR2_NOTE_MAX);
    preset = sdpr2LimitRange(preset, 0, SDPR2_PRESET_MAX);
    table = s_types[type];
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->playEx));
    handle = table->playEx(xvab, prog, note, preset);
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    s_slots[handle].type = table;
    return handle;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00169d20

int sdpr2Stop(int handle, int mode)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    mode = sdpr2LimitRange(mode, 0, 1);
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->stop));
    return table->stop(handle, mode);
}

// ──────────────────────────────────────────────────────────────────────
// Every handle of one type.
// FS2 0x00169f78

int sdpr2StopType(int type, int mode)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(type, SDPR2_TYPE_MAX));
    mode = sdpr2LimitRange(mode, 0, 1);
    table = s_types[type];
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->stopAll));
    return table->stopAll(mode);
}

// ──────────────────────────────────────────────────────────────────────
// Every type's stopAll; an empty entry or function is skipped quietly.
// FS2 0x0016a1c0

int sdpr2StopAll(int mode)
{
    int i;

    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2LimitRange, inline)
    mode = sdpr2LimitRange(mode, 0, 1);
    for (i = 0; i < SDPR2_TYPE_MAX; i++)
    {
        const Sdpr2Type *table = s_types[i];
        if (table != NULL && table->stopAll != NULL)
            table->stopAll(mode);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016a2b0

int sdpr2Pause(int handle, int on)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    on = sdpr2LimitRange(on, 0, 1);
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->pause));
    return table->pause(handle, on);
}

// ──────────────────────────────────────────────────────────────────────
// pitch: SDPR2_PITCH_ONE = 1.0, up to 2.0.
// FS2 0x0016a508

int sdpr2SetPitch(int handle, int pitch)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    pitch = sdpr2LimitRange(pitch, 0, SDPR2_PITCH_MAX);
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->setPitch));
    return table->setPitch(handle, pitch);
}

// ──────────────────────────────────────────────────────────────────────
// pitch: 0.0..2.0.
// FS2 0x0016a770

int sdpr2SetPitchF(int handle, float pitch)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf, __extendsfdf2 (the checks of sdpr2check.h, inline; %f's double)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    pitch = sdpr2LimitRangeF(pitch, 0.0f, 2.0f);
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->setPitchF));
    return table->setPitchF(handle, pitch);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016aa08

int sdpr2SetAngle(int handle, int deg)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    deg = sdpr2RoundRange(deg, SDPR2_ANGLE_RANGE);
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->setAngle));
    return table->setAngle(handle, deg);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016acd0

int sdpr2SetVolume(int handle, int volume)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    volume = sdpr2LimitRange(volume, 0, SDPR2_VOLUME_MAX);
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->setVolume));
    return table->setVolume(handle, volume);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016af38

int sdpr2SetSpread(int handle, int spread)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    spread = sdpr2LimitRange(spread, 0, SDPR2_SPREAD_MAX);
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->setSpread));
    return table->setSpread(handle, spread);
}

// ──────────────────────────────────────────────────────────────────────
// attr 0 / 1 (sound.cpp passes 0 or 1 from its own flag).
// FS2 0x0016b1a0

int sdpr2SetAttr(int handle, int attr)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    attr = sdpr2LimitRange(attr, 0, 1);
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->setAttr));
    return table->setAttr(handle, attr);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016b3f8

int sdpr2GetLevels(int handle, int *levels)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->getLevels));
    return table->getLevels(handle, levels);
}

// ──────────────────────────────────────────────────────────────────────
// Quiet: -1 for a bad handle, a free slot or a type without getStatus.
// FS2 0x0016b5f0

int sdpr2GetStatus(int handle)
{
    const Sdpr2Type *table;

    if (handle < 0 || handle >= SDPR2_SLOT_MAX)
        return -1;
    table = s_slots[handle].type;
    if (table == NULL || table->getStatus == NULL)
        return -1;
    return table->getStatus(handle);
}

// ──────────────────────────────────────────────────────────────────────
// Every type's update, quietly skipping the empty ones.
// FS2 0x0016b698

int sdpr2Update(void)
{
    int i;

    for (i = 0; i < SDPR2_TYPE_MAX; i++)
    {
        const Sdpr2Type *table = s_types[i];
        if (table != NULL && table->update != NULL)
            table->update();
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The three type extensions: no type has them (null slots), so on the
// disc these only print "Call Undefine Function".
// FS2 0x0016b718

int sdpr2TypeExt40(int type, int a)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(type, SDPR2_TYPE_MAX));
    table = s_types[type];
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->ext40));
    return table->ext40(a);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016b900

int sdpr2TypeExt44(int type, int a, int b)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(type, SDPR2_TYPE_MAX));
    table = s_types[type];
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->ext44));
    return table->ext44(a, b);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016baf8

int sdpr2TypeExt48(int type, int a, int b, int c)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(type, SDPR2_TYPE_MAX));
    table = s_types[type];
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->ext48));
    return table->ext48(a, b, c);
}

// ──────────────────────────────────────────────────────────────────────
// The getters read the slot, but only when the type has the matching
// setter (a type without it never keeps the value).
// FS2 0x0016bd00

int sdpr2GetPitch(int handle)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->setPitch));
    return s_slots[handle].pitch;
}

// ──────────────────────────────────────────────────────────────────────
// The pitch as a float (1.0 = SDPR2_PITCH_ONE); -1.0 on an error.
// FS2 0x0016bf10

float sdpr2GetPitchF(int handle)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->setPitchF));
    return (float)s_slots[handle].pitch * (1.0f / SDPR2_PITCH_ONE);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016c138

int sdpr2GetAngle(int handle)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->setAngle));
    return s_slots[handle].angle;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016c348

int sdpr2GetVolume(int handle)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->setVolume));
    return s_slots[handle].volume;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016c558

int sdpr2GetSpread(int handle)
{
    const Sdpr2Type *table;

    // callcheck: sdpr2GetDebug, rxSysPrintf (the checks of sdpr2check.h, inline)
    SDPR2_TRY(sdpr2CheckRange(handle, SDPR2_SLOT_MAX));
    table = s_slots[handle].type;
    SDPR2_TRY(sdpr2CheckFunc(table));
    SDPR2_TRY(sdpr2CheckFunc(table->setSpread));
    return s_slots[handle].spread;
}

// ──────────────────────────────────────────────────────────────────────
// A slot is free while both its voices are.
// FS2 0x0016c768

int sdpr2FindFreeSlot(void)
{
    int i;

    for (i = 0; i < SDPR2_SLOT_MAX; i++)
    {
        if (s_slots[i].voice[0] == SDPR2_VOICE_FREE && s_slots[i].voice[1] == SDPR2_VOICE_FREE)
            break;
    }
    return i != SDPR2_SLOT_MAX ? i : -1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0016c7c0

Sdpr2Slot *sdpr2GetSlot(int handle)
{
    // callcheck: sdpr2GetDebug, rxSysPrintf (sdpr2CheckRange, inline)
    if (sdpr2CheckRange(handle, SDPR2_SLOT_MAX) == SDPR2_ERR)
        return NULL;
    return &s_slots[handle];
}
