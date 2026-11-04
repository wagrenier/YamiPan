/* ==========================================================================
 *  game/gamepaths.h
 *
 *  The stage's file paths and the resident init / language packs
 *  (game/gamepaths.cpp, 0x28ff60..0x2908d0). The path builders format the
 *  play record (StageSelect: stage, kind, variant) into a static buffer or
 *  the caller's; the pack getters reach the packs loadInitPack /
 *  loadLangPack keep (PackFile, game/fileloader.h), answering null / 0
 *  while one is not loaded.
 * ======================================================================== */
#ifndef _GAME_GAMEPATHS_H
#define _GAME_GAMEPATHS_H

#include <stddef.h>

struct StageSelect;             // game/sysdata.h

// The two stages whose screen has a "b" layout (stageScreenPath's alt).
constexpr int kStageAltScreenA = 22;
constexpr int kStageAltScreenB = 27;

// One event movie stageMoviePath can pick: the record's stage and variant,
// and the branch the caller passes (0 here: any). Table 0x2fb520.
// layout: host -- the path function at +0xc; EE 0x10.
struct StageMovie
{
    int          stage;         // FS2 +0x0: StageSelect::stage
    int          variant;       // FS2 +0x4: StageSelect::variant
    int          branch;        // FS2 +0x8: 0 any, else the caller's branch
    const char *(*path)();      // FS2 +0xc: eventMoviePath* (game/layoutpaths.h)
};
static_assert(offsetof(StageMovie, branch) == 0x8, "StageMovie::branch");

// The stage's movie: with event 0 movieEU/mv<stage><lang>.d2b (null for
// stage 0), else the event movie of the table entry findStageMovie picks
// (null without one).
const char *stageMoviePath(const StageSelect *select, int event, int branch); // FS2 0x0028ff60
// The index of the event movie for the record and branch, -1 without one.
int         findStageMovie(const StageSelect *select, int branch);       // FS2 0x00290080

// The stage's script and data pack paths, written to out; 0 for a null out
// (out is cleared first otherwise), a null or invalid record:
// script/s<stage>/c<variant + 1><e|n|h>/export/s<stage>c<variant + 1><e|n|h>.slt
// and data<region>/pks<stage>c<variant + 1><region><suffix>.enc.
int stageScriptPath(const StageSelect *select, char *out);               // FS2 0x00290100
int stagePackPath(const StageSelect *select, char *out);                 // FS2 0x002901e0

// The stage screens' layout files for a play record (null when it is not
// valid; the e and h screens also for stage 1 variant 0), formatted into
// static buffers: 2dEU/s<stage>m<variant + 1>e<lang>.d2b, per difficulty
// (the kind: e / n / h) 2dEU/s<stage>m<variant + 1>h<e|n|h><lang>.d2b, the
// plain 2dEU/s<stage>m<variant + 1><lang>.d2b (with alt on stages 22 / 27
// the ...b<lang>.d2b one) and 2dEU/s<stage>m<variant + 1>mp<lang>.d2b.
const char *stageScreenPathH(const StageSelect *select);                 // FS2 0x002902d0
const char *stageScreenPathE(const StageSelect *select);                 // FS2 0x002903b0
const char *stageScreenPath(const StageSelect *select, bool alt);        // FS2 0x00290460
const char *stageScreenPathMp(const StageSelect *select);                // FS2 0x00290590

// The resident packs: delete the one held and load the file again
// (dataEU/packinitEU.pak; the language's dataEU/packinit<lang>.pak).
void loadInitPack();                                                     // FS2 0x00290840
void loadLangPack();                                                     // FS2 0x00290670

// The resident packs' items by id (PackFile::getItem / getItemSize; null /
// 0 while not loaded) and the init pack's file data by name.
void *getLangPackItem(int id);                                           // FS2 0x00290630
void *getInitPackItem(int id);                                           // FS2 0x002907c0
int   getInitPackItemSize(int id);                                       // FS2 0x00290780
void *getInitPackData(const char *name);                                 // FS2 0x00290800

#endif /* _GAME_GAMEPATHS_H */
