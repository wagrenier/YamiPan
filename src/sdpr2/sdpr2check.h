/* ==========================================================================
 *  sdpr2/sdpr2check.h
 *
 *  SDPR2's argument checks, inline in every caller. Each of the four SDPR2
 *  files has its own copy of their messages in the ROM (0x3027b8 for
 *  sdpr2.c, 0x302920, 0x302a88, 0x302bf0), so they were static or macro
 *  code in a shared header: this is it, static inline, one copy per file.
 *  Every message prints only while sdpr2GetDebug() is 1.
 *
 *  Two shapes:
 *  - Limit / Round: clamp or wrap an argument and go on.
 *  - Check: answer 0 or -1 (SDPR2_ERR); the API wraps them in SDPR2_TRY,
 *    which evaluates its expression a second time to return it, so a
 *    failed check prints its message twice (kept as the ROM does it).
 *  architecture.md "SDPR2".
 * ======================================================================== */
#ifndef _SDPR2_SDPR2CHECK_H
#define _SDPR2_SDPR2CHECK_H

#include <stddef.h>

#include "rx/rxsys.h"
#include "sdpr2/sdpr2.h"

#define SDPR2_OK    0
#define SDPR2_ERR   (-1)

/* Return expr when it is SDPR2_ERR -- by evaluating it again, as the
 * ROM's macro does (its messages print a second time). */
#define SDPR2_TRY(expr)                 \
    do {                                \
        if ((expr) == SDPR2_ERR)        \
            return (expr);              \
    } while (0)

/* value clamped to lo..hi. */
static inline int sdpr2LimitRange(int value, int lo, int hi)
{
    if (value < lo)
    {
        if (sdpr2GetDebug() == 1)
            rxSysPrintf("SDPR2 LimitRange (Under) !! %d\n", value);
        return lo;
    }
    if (value > hi)
    {
        if (sdpr2GetDebug() == 1)
            rxSysPrintf("SDPR2 LimitRange (Over) !! %d\n", value);
        return hi;
    }
    return value;
}

static inline float sdpr2LimitRangeF(float value, float lo, float hi)
{
    if (value < lo)
    {
        if (sdpr2GetDebug() == 1)
            rxSysPrintf("SDPR2 LimitRangeF (Under) !! %f\n", (double)value);
        return lo;
    }
    if (value > hi)
    {
        if (sdpr2GetDebug() == 1)
            rxSysPrintf("SDPR2 LimitRangeF (Over) !! %f\n", (double)value);
        return hi;
    }
    return value;
}

/* value wrapped into 0..range-1 (angles: range 360). A negative multiple
 * of range comes out as range itself, not 0 (kept as the ROM does it). */
static inline int sdpr2RoundRange(int value, int range)
{
    if (value < 0)
    {
        if (sdpr2GetDebug() == 1)
            rxSysPrintf("SDPR2 RoundRange (Under) !! %d\n", value);
        value = value % range + range;
        if (sdpr2GetDebug() == 1)
            rxSysPrintf("SDPR2 RoundRange (Processed) !! %d\n", value);
    }
    else if (value >= range)
    {
        if (sdpr2GetDebug() == 1)
            rxSysPrintf("SDPR2 RoundRange (Over) !! %d\n", value);
        value = value % range;
        if (sdpr2GetDebug() == 1)
            rxSysPrintf("SDPR2 RoundRange (Processed) !! %d\n", value);
    }
    return value;
}

/* SDPR2_OK when 0 <= value < count (a type, a handle). */
static inline int sdpr2CheckRange(int value, int count)
{
    if (value < 0)
    {
        if (sdpr2GetDebug() == 1)
            rxSysPrintf("SDPR2 ERR OutOfRange (Under) !! %d\n", value);
        return SDPR2_ERR;
    }
    if (value >= count)
    {
        if (sdpr2GetDebug() == 1)
            rxSysPrintf("SDPR2 ERR OutOfRange (Over) !! %d\n", value);
        return SDPR2_ERR;
    }
    return SDPR2_OK;
}

/* SDPR2_OK when the type or its function is there. */
static inline int sdpr2CheckDefined(int defined)
{
    if (!defined)
    {
        if (sdpr2GetDebug() == 1)
            rxSysPrintf("SDPR2 ERR Call Undefine Function !!\n");
        return SDPR2_ERR;
    }
    return SDPR2_OK;
}

/* A type table or one of its functions (data or function pointer). */
#define sdpr2CheckFunc(p)   sdpr2CheckDefined((p) != NULL)

#endif /* _SDPR2_SDPR2CHECK_H */
