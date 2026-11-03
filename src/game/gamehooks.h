/* ==========================================================================
 *  game/gamehooks.h
 *
 *  Subsystems main and its loop call that have no TU of their own yet:
 *  the rx runtime's game layer and SPK.ROM (game/spkrom.cpp), sound
 *  (sound/sound.cpp), the empty debug hooks
 *  0x151270..0x151290, the scene controller's callees (game/scene.cpp), and
 *  the pack files, UI layout and stage object game/stageresult.cpp calls.
 *  Each moves to its TU's header when that TU is written. Minimal.
 * ======================================================================== */
#ifndef _GAME_GAMEHOOKS_H
#define _GAME_GAMEHOOKS_H

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "game/uilayout.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

struct StageSelect;             // game/sysdata.h

// rx boot: thread priorities given absolute, made relative to the caller.
void rxInit(const char *bootPath, int prio0, int prio1, int prio2);      // FS2 0x001527a0
void rxTerm();                                                           // FS2 0x00152750

void soundInit(int isPal);                                               // FS2 0x001486e0
void soundUpdate();                                                      // FS2 0x00148500
void soundShutdown();                                                    // FS2 0x00148540

// The sound output settings (only what game/sysdata.cpp calls).
// layout: host -- no fields declared yet (EE: the mode at +0x14, the
// output / surround bytes at +0x48 / +0x49).
struct SoundConfig
{
    void setOutputMode(int mode);                                        // FS2 0x00147ee0: SoundOutput
};
extern SoundConfig *gSoundConfig;   /* sbss 0x31addc (gp-0x7a94) */

// Empty in retail: a stripped system's init / per-frame / teardown (guess).
void debugSysInit();                                                     // FS2 0x00151290
void debugSysUpdate();                                                   // FS2 0x00151270
void debugSysShutdown();                                                 // FS2 0x00151280

// True when the point (16-aligned xyzw) lies in the box whose world-to-unit-
// cube matrix is m (VU0 transform + vclip against w; game/boxgrid.cpp).
bool boxContainsPoint(const float m[4][4], const float p[4]);            // FS2 0x002362a0

// What game/scene.cpp calls.
// The resident packs: delete the one held and load the file again
// (dataEU/packinitEU.pak at 0x31ae60; dataEU/packmenuEU.pak at 0x31ae98,
// only while not loaded).
void loadInitPack();                                                     // FS2 0x00290840
void loadMenuPack();                                                     // FS2 0x002c3e10
// The scene table's only scene, "Menu (Pack File)": the menu flow 0x2b5770(1).
void menuSceneThread(void *arg);                                         // FS2 0x002b5740

// A list of files loaded together by a job (unassigned@001319f0, 0x1320f0..):
// addFile each, startLoad, waitLoaded, then getData / getSize by name (the
// first listed path containing it). PackFile's base.
// layout: host -- EE 0x9c, vptr at +0x98 (vtable 0x3110b0, 18 slots): +0
// state, +4 / +8 / +0xc the file list head / tail / count (0x2c-byte
// entries: +0xc size, +0x14 data, +0x28 next), +0x10 the job, +0x14 the
// constructor's argument, +0x18 the path prefix.
class FileLoader
{
public:
    explicit FileLoader(int arg);                                        // FS2 0x00132920
    virtual ~FileLoader();                                               // FS2 0x001328b0: slot 0, deleting
    virtual int   addFile(const char *name);                             // FS2 0x001327a0: slot 2, answers the index
    virtual void *getData(const char *name);                             // FS2 0x00132630: slot 5
    virtual int   getSize(const char *name);                             // FS2 0x00132550: slot 7
    virtual void  waitLoaded(int arg, bool showLoading);                 // FS2 0x001320f0: slot 15 (arg unused)
    // Queues the load job; 0 while no job slot is free.
    int startLoad();                                                     // FS2 0x00132280
};

// FileJob::m_state.
enum FileJobState
{
    FileJob_Idle = 0,
    FileJob_Queued = 1,
    FileJob_Loading = 2,
    FileJob_Done = 3,           // m_data holds the file (null when it is empty)
    FileJob_Failed = 0x80,      // bit: 0x80 not found, 0x81 no memory
    FileJob_NoMemory = 0x81,
};

// One file loaded by a job (unassigned@001319f0, 0x132a40..0x132cf0):
// m_dir + m_path first, then m_path, through the rx file hooks.
// layout: host -- EE 0x28.
class FileJob
{
public:
    FileJob(const char *path, int arg);                                  // FS2 0x00132cc0: path copied to new[]
    ~FileJob();                                                          // FS2 0x00132c10: deleting (obj, 1)
    // jobAdd(run, this); queued (state 1) when it answers non-zero.
    int start();                                                         // FS2 0x00132bc0
    static int run(void *job);                                           // FS2 0x00132a40

    int         m_state;        // FS2 +0x00: FileJobState
    char       *m_path;         // FS2 +0x04
    int         m_arg;          // FS2 +0x08
    int         m_size;         // FS2 +0x0c
    u_char      m_notOwned;     // FS2 +0x10: the destructor leaves m_data alone
    void       *m_data;         // FS2 +0x14: gGlobalHeap unless m_buffer is given
    int         m_jobId;        // FS2 +0x18
    const char *m_dir;          // FS2 +0x1c: tried first as a prefix
    void       *m_buffer;       // FS2 +0x20: the caller's buffer, if any
    int         m_bufferOffset; // FS2 +0x24: negative: from the end
};

// A pack file (dataEU/pack*.pak; unassigned@001319f0, 0x1319f0..).
// layout: host -- EE 0xf8, vptr at +0x98 (Vt_00311060, 18 slots; slot 6 the
// item lookup getInitPackItem calls), the path in new[] at +0xa0.
class PackFile : public FileLoader
{
public:
    PackFile(const char *path, int arg);                                 // FS2 0x00131f00
    virtual ~PackFile();                                                 // FS2 0x00131d80: slot 0, deleting
    void load(int a, int b);                                             // FS2 0x00131d70: every loader passes (0, 0)
};

// The resident packs' items (pack vtable slot 6; null while not loaded):
// dataEU/packinitEU.pak (loadInitPack) and the language's packinit<lang>.pak
// (0x290670).
void *getInitPackItem(int index);                                        // FS2 0x002907c0
void *getLangPackItem(int index);                                        // FS2 0x00290630
// The init pack's file data by name (FileLoader::getData; null while not
// loaded).
void *getInitPackData(const char *name);                                 // FS2 0x00290800

// The stage screens' layout files for a play record (null when it is not
// valid, and for stage 1 variant 0), formatted into static buffers:
// 2dEU/s<stage>m<variant + 1>e<lang>.d2b and, per difficulty (the kind:
// e / n / h), 2dEU/s<stage>m<variant + 1>h<e|n|h><lang>.d2b.
const char *stageScreenPathE(const StageSelect *select);                 // FS2 0x002903b0
const char *stageScreenPathH(const StageSelect *select);                 // FS2 0x002902d0
// 2dEU/archive<lang>.d2b and 2dEU/option<lang>.d2b (static buffers).
const char *archiveLayoutPath();                                         // FS2 0x002ec7f0
const char *optionLayoutPath();                                          // FS2 0x002ec660

// getInitPackItem indices game/stageresult.cpp asks for (their content is
// not read yet).
enum InitPackItem
{
    InitItem_21     = 0x15,     // getInitItem21
    InitItem_LangDE = 0x16,
    InitItem_LangEN = 0x17,     // every language but the four below
    InitItem_LangES = 0x18,
    InitItem_LangFR = 0x19,
    InitItem_LangIT = 0x1a,
};

// getLangPackItem indices: game/screen.cpp's loading indicator and dialogs,
// the clock layouts StageResult::begin builds.
enum LangPackItem
{
    LangItem_Loading        = 0,    // overlayShowLoading
    LangItem_Notice         = 1,    // noticeDialogStart
    LangItem_GameOver       = 2,    // gameOverDialogStart
    LangItem_GameOverAttempt = 3,   // the same while StageResult::hasAttemptFlag
    LangItem_ClockCountUp   = 4,
    LangItem_ClockCountdown = 5,
};

// The tables PackTables::parse reads out of packgameEU.pak (columns ラベル
// "label", ctype, SEtype). The element types are read from the MSL
// instances' destructors only (unverified): a ListMap value is a 4-byte key
// and a vector of 0x10-byte {4 bytes, string} entries, a NameMap value a
// string key and 4 trivially destroyed bytes.
// layout: host -- std::string; EE 0x10.
struct PackEntry
{
    int         id;             // FS2 +0x0 (unverified)
    std::string text;           // FS2 +0x4: MSL string, EE 0xc
};
typedef std::vector<PackEntry> EntryList;
typedef std::map<int, EntryList> ListMap;
typedef std::map<std::string, int> NameMap;

// layout: host -- EE 0x80: eight MSL maps of 0x10, ListMap at +0x00 / +0x20 /
// +0x40 / +0x60, NameMap at +0x10 / +0x30 / +0x50 / +0x70 (0x2c16b0 /
// 0x2c1710); the destructor is implicit (inline in loadGamePackTables).
class PackTables
{
public:
    PackTables();                                                        // FS2 0x002c0dc0
    void parse(PackFile *pack);                                          // FS2 0x002bcf10

    ListMap lists0;             // FS2 +0x00
    NameMap names0;             // FS2 +0x10
    ListMap lists1;             // FS2 +0x20
    NameMap names1;             // FS2 +0x30
    ListMap lists2;             // FS2 +0x40
    NameMap names2;             // FS2 +0x50
    ListMap lists3;             // FS2 +0x60
    NameMap names3;             // FS2 +0x70
};

// The current stage's sound effects (the stage object's +0x24).
// layout: host -- no fields declared yet.
class StageSound
{
public:
    void playNamed(const char *name, float volume);                      // FS2 0x001d6a80
};

// The current stage object (0x130 bytes, built by the stage loop 0x17f160:
// 0x19c180). Only what game/stageresult.cpp reads.
// layout: host -- EE: +0x1c the box-grid owner (game/boxgrid.h), +0x24 the
// sound.
struct StageScene
{
    u_char      _00[0x24];
    StageSound *sound;          // FS2 +0x24
};
extern StageScene *gStageScene;     /* sbss 0x31adf4 (gp-0x7a7c) */

// The 2D layouts' gfx state set-ups (UiDrawer::setDrawPreset, UiPreset;
// unassigned@00171300 0x230980..0x231200): A+D runs of fixed registers,
// then FRAME_1 / ZBUF_1 from the current draw env with the masks noted.
void gfxUiPresetNormal(sceVif1Packet *packet);                           // FS2 0x00231090: FBMSK 0, ZMSK
void gfxUiPresetAlphaOnly1(sceVif1Packet *packet);                       // FS2 0x00230e40: FBMSK 0x00ffffff
void gfxUiPresetAlphaOnly2(sceVif1Packet *packet);                       // FS2 0x00230d10
void gfxUiPresetRgbOnly3(sceVif1Packet *packet);                         // FS2 0x00230be0: FBMSK 0xff000000
void gfxUiPresetRgbOnly4(sceVif1Packet *packet);                         // FS2 0x00230ab0
void gfxUiPresetRgbOnly5(sceVif1Packet *packet);                         // FS2 0x00230980
void gfxUiPresetDrawEnv(sceVif1Packet *packet);                          // FS2 0x00230f70: FRAME / ZBUF / SCISSOR as the env

#endif /* _GAME_GAMEHOOKS_H */
