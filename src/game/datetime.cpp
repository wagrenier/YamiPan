// game/datetime.cpp -- the console clock as a DateTime (game/datetime.h).

#include "game/datetime.h"

#include "sdk/libcdvd.h"
#include "sdk/libscf.h"
#include "sdk/scetypes.h"

// The date getLocalDateTime reports when the drive cannot read the clock:
// 2000-01-01 12:00:00. The hour is stored as 0x0c, not BCD 0x12; read back
// from BCD it is still 12.
constexpr u_char kFallbackHour = 0x0c;
constexpr u_char kFallbackDay = 0x01;
constexpr u_char kFallbackMonth = 0x01;
constexpr u_short kFallbackYear = 0x2000;

// The RTC keeps a two-digit year; the game puts it in this century, except
// for the one wrap it tests for (see getLocalDateTime).
constexpr u_short kBcdCentury2000 = 0x2000;
constexpr u_short kBcdCentury1900 = 0x1900;
constexpr u_char kYearBeforeWrap = 99;

static inline u_char bcdToByte(u_char v)
{
    return (v & 0xf) + (v >> 4) * 10;
}

static inline u_char byteToBcd(u_char v)
{
    return v % 10 + ((v / 10) % 10) * 0x10;
}

// ──────────────────────────────────────────────────────────────────────
// Reads the RTC and converts it to the console's local time. When the read
// worked, the year was 00 before the time-zone shift and 99 after it (the
// shift went back past New Year 2000), the year is put in 1900.
// FS2 0x0023de20

void getLocalDateTime(DateTime *out, int format)
{
    sceCdCLOCK clock;
    DateTime local;

    if (sceCdReadClock(&clock) == 0 || clock.stat != 0) {
        local.status = DateStatus_ClockError;
        local.format = DateFormat_Bcd;
        local.second = 0;
        local.minute = 0;
        local.hour = kFallbackHour;
        local.day = kFallbackDay;
        local.month = kFallbackMonth;
        local.year = kFallbackYear;
    } else {
        bool yearWas00 = clock.year == 0;
        sceScfGetLocalTimefromRTC(&clock);
        local.status = DateStatus_Ok;
        local.second = clock.second;
        local.minute = clock.minute;
        local.hour = clock.hour;
        local.day = clock.day;
        local.month = clock.month;
        local.format = DateFormat_Bcd;
        // The clock is BCD here, so a year of 99 reads 0x99: the ROM tests
        // decimal 99 (0x63) and never takes this branch.
        if (yearWas00 && clock.year == kYearBeforeWrap) {
            local.year = kBcdCentury1900 + clock.year;
        } else {
            local.year = kBcdCentury2000 + clock.year;
        }
    }
    local.setFormat(format);
    *out = local;
}

// ──────────────────────────────────────────────────────────────────────
// Re-encodes second..year to newFormat (DateFormat); nothing when they are
// stored that way already. The ROM unrolls every field.
// FS2 0x0023dfa0

void DateTime::setFormat(u_char newFormat)
{
    if (format == DateFormat_Bcd && newFormat == DateFormat_Bcd) {
        return;
    }
    if (format == DateFormat_Binary && newFormat == DateFormat_Binary) {
        return;
    }

    if (newFormat == DateFormat_Binary) {
        format = DateFormat_Binary;
        second = bcdToByte(second);
        minute = bcdToByte(minute);
        hour = bcdToByte(hour);
        day = bcdToByte(day);
        month = bcdToByte(month);
        year = (year & 0xf) + ((year >> 4) & 0xf) * 10 + ((year >> 8) & 0xf) * 100
             + (year >> 12) * 1000;
    } else {
        format = DateFormat_Bcd;
        second = byteToBcd(second);
        minute = byteToBcd(minute);
        hour = byteToBcd(hour);
        day = byteToBcd(day);
        month = byteToBcd(month);
        year = year % 10 + ((year / 10) % 10) * 0x10 + ((year / 100) % 10) * 0x100
             + ((year / 1000) % 10) * 0x1000;
    }
}
