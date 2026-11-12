// game/layoutpaths.cpp -- the event movie and menu layout paths
// (game/layoutpaths.h).
//
// All 24 functions have one shape: format the file's name with the language
// code into a static buffer and answer the buffer. Each ROM function also
// calls getSaveData and ignores the answer; the call stays so the functions
// keep the ROM's calls.

#include "game/layoutpaths.h"

#include <stdio.h>

#include "game/sysdata.h"

// The static buffers the functions answer (bss, 0x20 each, the ROM lays them
// out downward from the first function's).
constexpr int kPathBufferSize = 0x20;
static char sEv2506Path[kPathBufferSize];         /* bss 0x5a55e0 */
static char sEv2502Path[kPathBufferSize];         /* bss 0x5a55c0 */
static char sEv2301Path[kPathBufferSize];         /* bss 0x5a55a0 */
static char sEv2205Path[kPathBufferSize];         /* bss 0x5a5580 */
static char sEv2103Path[kPathBufferSize];         /* bss 0x5a5560 */
static char sEv1608Path[kPathBufferSize];         /* bss 0x5a5540 */
static char sEv1506Path[kPathBufferSize];         /* bss 0x5a5520 */
static char sEv1505Path[kPathBufferSize];         /* bss 0x5a5500 */
static char sEv1405Path[kPathBufferSize];         /* bss 0x5a54e0 */
static char sEv1404Path[kPathBufferSize];         /* bss 0x5a54c0 */
static char sEv1206Path[kPathBufferSize];         /* bss 0x5a54a0 */
static char sEv0202Path[kPathBufferSize];         /* bss 0x5a5480 */
static char sEv0102Path[kPathBufferSize];         /* bss 0x5a5460 */
static char sNaviPath[kPathBufferSize];           /* bss 0x5a5440 */
static char sNaviTuPath[kPathBufferSize];         /* bss 0x5a5420 */
static char sMcBasePath[kPathBufferSize];         /* bss 0x5a5400 */
static char sOptionPath[kPathBufferSize];         /* bss 0x5a53e0 */
static char sMemAltPath[kPathBufferSize];         /* bss 0x5a53c0 */
static char sNewGamePath[kPathBufferSize];        /* bss 0x5a53a0 */
static char sSceLogoPath[kPathBufferSize];        /* bss 0x5a5380 */
static char sTopMenuPath[kPathBufferSize];        /* bss 0x5a5360 */
static char sArchivePath[kPathBufferSize];        /* bss 0x5a5340 */
static char sHelpPath[kPathBufferSize];           /* bss 0x5a5320 */
static char sTitlePath[kPathBufferSize];          /* bss 0x5a5300 */

// FS2 0x002ec160

const char *eventMoviePath2506()
{
    getSaveData();
    sprintf(sEv2506Path, "movieEU/ev2506%s.d2b", getLanguageCode(getLanguage()));
    return sEv2506Path;
}

// FS2 0x002ec1b0

const char *eventMoviePath2502()
{
    getSaveData();
    sprintf(sEv2502Path, "movieEU/ev2502%s.d2b", getLanguageCode(getLanguage()));
    return sEv2502Path;
}

// FS2 0x002ec200

const char *eventMoviePath2301()
{
    getSaveData();
    sprintf(sEv2301Path, "movieEU/ev2301%s.d2b", getLanguageCode(getLanguage()));
    return sEv2301Path;
}

// FS2 0x002ec250

const char *eventMoviePath2205()
{
    getSaveData();
    sprintf(sEv2205Path, "movieEU/ev2205%s.d2b", getLanguageCode(getLanguage()));
    return sEv2205Path;
}

// FS2 0x002ec2a0

const char *eventMoviePath2103()
{
    getSaveData();
    sprintf(sEv2103Path, "movieEU/ev2103%s.d2b", getLanguageCode(getLanguage()));
    return sEv2103Path;
}

// FS2 0x002ec2f0

const char *eventMoviePath1608()
{
    getSaveData();
    sprintf(sEv1608Path, "movieEU/ev1608%s.d2b", getLanguageCode(getLanguage()));
    return sEv1608Path;
}

// FS2 0x002ec340

const char *eventMoviePath1506()
{
    getSaveData();
    sprintf(sEv1506Path, "movieEU/ev1506%s.d2b", getLanguageCode(getLanguage()));
    return sEv1506Path;
}

// FS2 0x002ec390

const char *eventMoviePath1505()
{
    getSaveData();
    sprintf(sEv1505Path, "movieEU/ev1505%s.d2b", getLanguageCode(getLanguage()));
    return sEv1505Path;
}

// FS2 0x002ec3e0

const char *eventMoviePath1405()
{
    getSaveData();
    sprintf(sEv1405Path, "movieEU/ev1405%s.d2b", getLanguageCode(getLanguage()));
    return sEv1405Path;
}

// FS2 0x002ec430

const char *eventMoviePath1404()
{
    getSaveData();
    sprintf(sEv1404Path, "movieEU/ev1404%s.d2b", getLanguageCode(getLanguage()));
    return sEv1404Path;
}

// FS2 0x002ec480

const char *eventMoviePath1206()
{
    getSaveData();
    sprintf(sEv1206Path, "movieEU/ev1206%s.d2b", getLanguageCode(getLanguage()));
    return sEv1206Path;
}

// FS2 0x002ec4d0

const char *eventMoviePath0202()
{
    getSaveData();
    sprintf(sEv0202Path, "movieEU/ev0202%s.d2b", getLanguageCode(getLanguage()));
    return sEv0202Path;
}

// FS2 0x002ec520

const char *eventMoviePath0102()
{
    getSaveData();
    sprintf(sEv0102Path, "movieEU/ev0102%s.d2b", getLanguageCode(getLanguage()));
    return sEv0102Path;
}

// FS2 0x002ec570

const char *naviLayoutPath()
{
    getSaveData();
    sprintf(sNaviPath, "2dEU/navi%s.d2b", getLanguageCode(getLanguage()));
    return sNaviPath;
}

// FS2 0x002ec5c0

const char *naviTuLayoutPath()
{
    getSaveData();
    sprintf(sNaviTuPath, "2dEU/navi_tu%s.d2b", getLanguageCode(getLanguage()));
    return sNaviTuPath;
}

// FS2 0x002ec610

const char *mcBaseLayoutPath()
{
    getSaveData();
    sprintf(sMcBasePath, "2dEU/mcBase%s.d2b", getLanguageCode(getLanguage()));
    return sMcBasePath;
}

// FS2 0x002ec660

const char *optionLayoutPath()
{
    getSaveData();
    sprintf(sOptionPath, "2dEU/option%s.d2b", getLanguageCode(getLanguage()));
    return sOptionPath;
}

// FS2 0x002ec6b0

const char *memAltLayoutPath()
{
    getSaveData();
    sprintf(sMemAltPath, "2dEU/memAlt%s.d2b", getLanguageCode(getLanguage()));
    return sMemAltPath;
}

// FS2 0x002ec700

const char *newGameLayoutPath()
{
    getSaveData();
    sprintf(sNewGamePath, "2dEU/newGame%s.d2b", getLanguageCode(getLanguage()));
    return sNewGamePath;
}

// FS2 0x002ec750

const char *sceLogoLayoutPath()
{
    getSaveData();
    sprintf(sSceLogoPath, "2dEU/scelogo%s.d2b", getLanguageCode(getLanguage()));
    return sSceLogoPath;
}

// FS2 0x002ec7a0

const char *topMenuLayoutPath()
{
    getSaveData();
    sprintf(sTopMenuPath, "2dEU/topMenu%s.d2b", getLanguageCode(getLanguage()));
    return sTopMenuPath;
}

// FS2 0x002ec7f0

const char *archiveLayoutPath()
{
    getSaveData();
    sprintf(sArchivePath, "2dEU/archive%s.d2b", getLanguageCode(getLanguage()));
    return sArchivePath;
}

// FS2 0x002ec840

const char *helpLayoutPath()
{
    getSaveData();
    sprintf(sHelpPath, "2dEU/help%s.d2b", getLanguageCode(getLanguage()));
    return sHelpPath;
}

// FS2 0x002ec890

const char *titleLayoutPath()
{
    getSaveData();
    sprintf(sTitlePath, "2dEU/title%s.d2b", getLanguageCode(getLanguage()));
    return sTitlePath;
}
