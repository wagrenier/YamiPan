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

#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

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
// Thread slot 6 (when free): draws the overlay objects gp-0x7a28 / -0x7a20
// into packet list 0xb every frame.
void overlayThreadStart();                                               // FS2 0x00267780
// Deletes the overlay object gp-0x7a28 and clears its show flag.
void overlayReleaseScreen();                                             // FS2 0x00267a00
// The scene table's only scene, "Menu (Pack File)": the menu flow 0x2b5770(1).
void menuSceneThread(void *arg);                                         // FS2 0x002b5740

// A pack file (dataEU/pack*.pak; unassigned@001319f0, 0x1319f0..).
// layout: host -- EE 0xf8, vptr at +0x98 (Vt_00311060, 18 slots; slot 6 the
// item lookup getInitPackItem calls), the path in new[] at +0xa0.
class PackFile
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

// getLangPackItem indices: the clock layouts StageResult::begin builds.
enum LangPackItem
{
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

// A 2D layout drawn from a pack item (unassigned@0023e740, 0x2434c0..):
// parts sorted by priority, a 0xe0-byte child at +0x20. The overlay
// (gp-0x7a28, overlayThreadStart) is one too.
// layout: host -- EE 0x98.
class UiLayout
{
public:
    UiLayout();                                                          // FS2 0x00246160
    ~UiLayout();                                                         // FS2 0x00246010: deleting (obj, 1)
    bool setData(void *item, int arg);                                   // FS2 0x00245e80
    void advance();                                                      // FS2 0x00243b90
    void update();                                                       // FS2 0x00243c10
    void draw(sceVif1Packet *packet, void *drawCtx);                     // FS2 0x002439f0
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

#endif /* _GAME_GAMEHOOKS_H */
