/* ==========================================================================
 *  game/datetime.h
 *
 *  The console clock as the game reads it (game/datetime.cpp,
 *  0x23de20..0x23e740): getLocalDateTime takes the drive's RTC (Japan
 *  time, BCD), moves it to the console's time zone with libscf and hands
 *  back a DateTime in the format asked for. Both callers (gameInit for the
 *  random seed, the memcard code at 0x2ac3f0) ask for binary.
 * ======================================================================== */
#ifndef _GAME_DATETIME_H
#define _GAME_DATETIME_H

#include "sdk/scetypes.h"

// DateTime::format: how second..year are stored.
enum DateFormat
{
    DateFormat_Binary = 0,
    DateFormat_Bcd = 1,
};

// DateTime::status.
enum DateStatus
{
    DateStatus_Ok = 0,
    DateStatus_ClockError = 1,      // sceCdReadClock failed: the fallback date
};

struct DateTime
{
    u_char  status : 4;         // FS2 +0 bits 0-3: DateStatus
    u_char  format : 4;         // FS2 +0 bits 4-7: DateFormat
    u_char  second;             // FS2 +1
    u_char  minute;             // FS2 +2
    u_char  hour;               // FS2 +3
    u_char  day;                // FS2 +4
    u_char  month;              // FS2 +5
    u_short year;               // FS2 +6: four digits (BCD 0x2006 or 2006)

    void setFormat(u_char newFormat);                                    // FS2 0x0023dfa0
};
static_assert(sizeof(DateTime) == 0x8, "DateTime: EE 0x8");

void getLocalDateTime(DateTime *out, int format);                        // FS2 0x0023de20

#endif /* _GAME_DATETIME_H */
