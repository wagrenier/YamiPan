/* ==========================================================================
 *  game/sysdata.h
 *
 *  The game's system data around 0x23bb50..0x23e000 (not a TU yet: part of
 *  the unassigned run after 0x171300): persistent state 0x598b30, the
 *  options 0x599000 with their button map, and the RTC read as a local
 *  BCD date. Minimal: only what game/gamesystem.cpp touches.
 * ======================================================================== */
#ifndef _GAME_SYSDATA_H
#define _GAME_SYSDATA_H

#include <stddef.h>

#include "sdk/scetypes.h"

// Action -> button index table; GamePad keeps its own and points at the
// options' one (GamePad::setButtonMap).
struct ButtonMap
{
    void setDefaults();                                                  // FS2 0x0023bd00

    int button[8];              // FS2 +0x00: defaults 6, 5, 4, 7, 3, 1, 2, 0
};
static_assert(sizeof(ButtonMap) == 0x20, "ButtonMap: EE 0x20");

// The game options at 0x599000 (size not known yet).
struct GameOptions
{
    void setDefaults();                                                  // FS2 0x0023be70

    u_char    _00[0x24];
    ButtonMap buttonMap;        // FS2 +0x24
};
static_assert(offsetof(GameOptions, buttonMap) == 0x24, "GameOptions::buttonMap");

// The RTC as getLocalDateTime leaves it, every field BCD; on a clock error
// it is 2000-01-01 12:00:00 with flags 0x11.
struct DateTime
{
    u_char  flags;              // FS2 +0: 0x10 read, 0x11 clock error
    u_char  second;             // FS2 +1
    u_char  minute;             // FS2 +2
    u_char  hour;               // FS2 +3
    u_char  day;                // FS2 +4
    u_char  month;              // FS2 +5
    u_short year;               // FS2 +6: 0x2000 + yy
};
static_assert(sizeof(DateTime) == 0x8, "DateTime: EE 0x8");

GameOptions *getGameOptions();                                           // FS2 0x0023baf0
void systemDataInit();                                                   // FS2 0x0023bb50
void getLocalDateTime(DateTime *out, int arg);                           // FS2 0x0023de20

#endif /* _GAME_SYSDATA_H */
