// game/gamepaths.cpp -- the stage's file paths and the resident init /
// language packs (game/gamepaths.h).

#include "game/gamepaths.h"

#include <stdio.h>

#include "game/gamehooks.h"
#include "game/sysdata.h"
#include "sdk/scetypes.h"

static const char kMovieDir[] = "movieEU/";
static const char kScreenDir[] = "2dEU/";
static const char kInitPackPath[] = "dataEU/packinitEU.pak";

// The static buffers the path builders answer (bss, 0x20 each).
constexpr int kPathBufferSize = 0x20;
static char sMoviePath[kPathBufferSize];        /* bss 0x5995c0 */
static char sScreenPathH[kPathBufferSize];      /* bss 0x5995a0 */
static char sScreenPathE[kPathBufferSize];      /* bss 0x599580 */
static char sScreenPath[kPathBufferSize];       /* bss 0x599560 */
static char sScreenPathMp[kPathBufferSize];     /* bss 0x599540 */

static PackFile *sInitPack;         /* sbss 0x31ae60 (gp-0x7a10) */
static u_char    sInitPackLoaded;   /* sbss 0x31ae64 (gp-0x7a0c) */
static PackFile *sLangPack;         /* sbss 0x31ae68 (gp-0x7a08) */
static u_char    sLangPackLoaded;   /* sbss 0x31ae6c (gp-0x7a04) */

// The event movies, by the record they end (data 0x2fb520; the event ids
// are the stage's own).
static const StageMovie kStageMovies[] = {
    {  1, 0, 0, eventMoviePath0102 },
    {  2, 0, 0, eventMoviePath0202 },
    { 12, 1, 0, eventMoviePath1206 },
    { 14, 1, 1, eventMoviePath1404 },
    { 14, 1, 2, eventMoviePath1405 },
    { 15, 1, 0, eventMoviePath1505 },
    { 15, 0, 0, eventMoviePath1506 },
    { 16, 1, 0, eventMoviePath1608 },
    { 21, 0, 0, eventMoviePath2103 },
    { 22, 1, 0, eventMoviePath2205 },
    { 23, 0, 0, eventMoviePath2301 },
    { 25, 0, 0, eventMoviePath2502 },
    { 25, 1, 0, eventMoviePath2506 },
};
static int sStageMovieCount = sizeof(kStageMovies) / sizeof(kStageMovies[0]); /* sdata 0x31ac2c (gp-0x7c44): 13 */

// The path digit for the variant: 1, or 2 for any other than 0. Inline in
// the ROM.
static inline int variantNumber(const StageSelect *select)
{
    return select->variant != 0 ? 2 : 1;
}

// The path letter for the kind. Inline in the ROM.
static inline char kindLetter(const StageSelect *select)
{
    if (select->kind == StageKind_Normal)
        return 'n';
    if (select->kind == StageKind_Easy)
        return 'e';
    return 'h';
}

// ──────────────────────────────────────────────────────────────────────
// The stage's movie: the plain one for event 0, else the event movie for
// the record and branch.
// FS2 0x0028ff60

const char *stageMoviePath(const StageSelect *select, int event, int branch)
{
    if (event == 0) {
        if (select->stage == 0)
            return nullptr;
        // The options are fetched and dropped (an inline accessor's
        // leftover); the language is a global of its own.
        getGameOptions();
        sprintf(sMoviePath, "%smv%02d%s.d2b", kMovieDir, select->stage,
                getLanguageCode(getLanguage()));
        return sMoviePath;
    }

    // findStageMovie, inline in the ROM.
    int index = findStageMovie(select, branch);
    if (index < 0)
        return nullptr;
    return kStageMovies[index].path();
}

// ──────────────────────────────────────────────────────────────────────
// The first event movie for the record's stage and variant whose branch is
// 0 or the one given; -1 without one.
// FS2 0x00290080

int findStageMovie(const StageSelect *select, int branch)
{
    for (int i = 0; i < sStageMovieCount; i++) {
        const StageMovie *movie = &kStageMovies[i];
        if (select->stage == movie->stage && select->variant == movie->variant) {
            if (movie->branch == 0)
                return i;
            if (branch == movie->branch)
                return i;
        }
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// script/s<stage>/c<variant>/<kind>/export/s<stage>c<variant><kind>.slt
// FS2 0x00290100

int stageScriptPath(const StageSelect *select, char *out)
{
    if (out == nullptr)
        return 0;
    out[0] = '\0';
    if (select == nullptr)
        return 0;
    if (!select->isValid())
        return 0;
    sprintf(out, "script/s%02d/c%1d%c/export/s%02dc%1d%c.slt",
            select->stage, variantNumber(select), kindLetter(select),
            select->stage, variantNumber(select), kindLetter(select));
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// data<region>/pks<stage>c<variant><region><suffix>.enc; the suffix comes
// from GameOptions::_04 (getFileSuffix: "_e" when set).
// FS2 0x002901e0

int stagePackPath(const StageSelect *select, char *out)
{
    if (out == nullptr)
        return 0;
    out[0] = '\0';
    if (select == nullptr)
        return 0;
    if (!select->isValid())
        return 0;
    const char *region = getRegionCode();
    int stage = select->stage;
    const char *region2 = getRegionCode();
    const char *suffix = getFileSuffix(getGameOptions()->_04);
    sprintf(out, "data%s/pks%02dc%1d%s%s.enc", region, stage,
            variantNumber(select), region2, suffix);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// 2dEU/s<stage>m<variant>h<kind><lang>.d2b; none for stage 1 variant 0.
// FS2 0x002902d0

const char *stageScreenPathH(const StageSelect *select)
{
    if (!select->isValid())
        return nullptr;
    int stage = select->stage;
    if (stage == 1 && select->variant == 0)
        return nullptr;
    getGameOptions();
    const char *lang = getLanguageCode(getLanguage());
    sprintf(sScreenPathH, "%ss%02dm%1dh%c%s.d2b", kScreenDir, stage,
            variantNumber(select), kindLetter(select), lang);
    return sScreenPathH;
}

// ──────────────────────────────────────────────────────────────────────
// 2dEU/s<stage>m<variant>e<lang>.d2b; none for stage 1 variant 0.
// FS2 0x002903b0

const char *stageScreenPathE(const StageSelect *select)
{
    if (!select->isValid())
        return nullptr;
    int stage = select->stage;
    if (stage == 1 && select->variant == 0)
        return nullptr;
    getGameOptions();
    const char *lang = getLanguageCode(getLanguage());
    sprintf(sScreenPathE, "%ss%02dm%1de%s.d2b", kScreenDir, stage,
            variantNumber(select), lang);
    return sScreenPathE;
}

// ──────────────────────────────────────────────────────────────────────
// 2dEU/s<stage>m<variant><lang>.d2b, or with alt on stages 22 and 27
// 2dEU/s<stage>m<variant>b<lang>.d2b.
// FS2 0x00290460

const char *stageScreenPath(const StageSelect *select, bool alt)
{
    if (!select->isValid())
        return nullptr;
    int stage = select->stage;
    bool hasAlt = stage == kStageAltScreenA || stage == kStageAltScreenB;
    if (!hasAlt)
        alt = false;
    if (!alt) {
        getGameOptions();
        const char *lang = getLanguageCode(getLanguage());
        sprintf(sScreenPath, "%ss%02dm%1d%s.d2b", kScreenDir, stage,
                variantNumber(select), lang);
    } else {
        getGameOptions();
        const char *lang = getLanguageCode(getLanguage());
        sprintf(sScreenPath, "%ss%02dm%1db%s.d2b", kScreenDir, stage,
                variantNumber(select), lang);
    }
    return sScreenPath;
}

// ──────────────────────────────────────────────────────────────────────
// 2dEU/s<stage>m<variant>mp<lang>.d2b.
// FS2 0x00290590

const char *stageScreenPathMp(const StageSelect *select)
{
    if (!select->isValid())
        return nullptr;
    int stage = select->stage;
    getGameOptions();
    const char *lang = getLanguageCode(getLanguage());
    sprintf(sScreenPathMp, "%ss%02dm%1dmp%s.d2b", kScreenDir, stage,
            variantNumber(select), lang);
    return sScreenPathMp;
}

// ──────────────────────────────────────────────────────────────────────
// The language pack's item by id; null while it is not loaded.
// FS2 0x00290630

void *getLangPackItem(int id)
{
    if (!sLangPackLoaded)
        return nullptr;
    return sLangPack->getItem(id);
}

// ──────────────────────────────────────────────────────────────────────
// Deletes the language pack held and loads the one for the language
// (English for every language but the four below).
// FS2 0x00290670
// callcheck: operator_new (the new-expression)

void loadLangPack()
{
    if (sLangPack != nullptr)
        delete sLangPack;
    getGameOptions();
    int language = getLanguage();
    const char *path;
    if (language == Language_IT)
        path = "dataEU/packinitIT.pak";
    else if (language == Language_ES)
        path = "dataEU/packinitES.pak";
    else if (language == Language_FR)
        path = "dataEU/packinitFR.pak";
    else if (language == Language_DE)
        path = "dataEU/packinitDE.pak";
    else
        path = "dataEU/packinitEN.pak";
    PackFile *pack = new PackFile(path, 0);
    pack->load(0, 0);
    sLangPack = pack;
    sLangPackLoaded = 1;
}

// ──────────────────────────────────────────────────────────────────────
// The init pack's item size by id; 0 while it is not loaded (-1 from the
// pack for an id it does not hold).
// FS2 0x00290780

int getInitPackItemSize(int id)
{
    if (!sInitPackLoaded)
        return 0;
    return sInitPack->getItemSize(id);
}

// ──────────────────────────────────────────────────────────────────────
// The init pack's item by id; null while it is not loaded.
// FS2 0x002907c0

void *getInitPackItem(int id)
{
    if (!sInitPackLoaded)
        return nullptr;
    return sInitPack->getItem(id);
}

// ──────────────────────────────────────────────────────────────────────
// The init pack's file data by name; null while it is not loaded.
// FS2 0x00290800

void *getInitPackData(const char *name)
{
    if (!sInitPackLoaded)
        return nullptr;
    return sInitPack->getData(name);
}

// ──────────────────────────────────────────────────────────────────────
// Deletes the init pack held and loads dataEU/packinitEU.pak again.
// FS2 0x00290840
// callcheck: operator_new (the new-expression)

void loadInitPack()
{
    if (sInitPack != nullptr)
        delete sInitPack;
    PackFile *pack = new PackFile(kInitPackPath, 0);
    pack->load(0, 0);
    sInitPack = pack;
    sInitPackLoaded = 1;
}
