/* ==========================================================================
 *  sdk/libscf.h  (SCE PS2 system-configuration library)
 * ======================================================================== */

#pragma once
#include "libcdvd.h"                    /* sceCdCLOCK */

#define SCE_JAPANESE_LANGUAGE 0
#define SCE_ENGLISH_LANGUAGE 1
#define SCE_FRENCH_LANGUAGE 2
#define SCE_SPANISH_LANGUAGE 3
#define SCE_GERMAN_LANGUAGE 4
#define SCE_ITALIAN_LANGUAGE 5
#define SCE_DUTCH_LANGUAGE 6
#define SCE_PORTUGUESE_LANGUAGE 7

int sceScfGetLanguage(void);
/* The RTC (Japan time on the PS2) to the console's local time zone, in place. */
void sceScfGetLocalTimefromRTC(sceCdCLOCK *rtc);

