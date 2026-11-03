/* ==========================================================================
 *  game/stagescene.h
 *
 *  The stage scene (StageScene, 0x130 bytes on the EE: gStageScene, and a
 *  sub-scene built under it) and the stage's files (StageFiles: the stage
 *  script and the lists it names), 0x1953f0..0x1a68b0. One file is a guess
 *  (no source-path string): the two share the path chooser 0x1a1ca0 and
 *  the string-map templates 0x1a4e80..0x1a6790 CodeWarrior emits last.
 *
 *  The scene's members, from the constructor 0x19c180 and the destructor
 *  0x19bbe0 (written in a later session; the classes are named as their
 *  TUs are read). "sub" marks what only a sub-scene builds, "stage" what
 *  only a stage builds (the other one leaves it null):
 *    +0x10 StageObjectList (0xdc)       +0x14 0x171680 (0x2c; dtor 0x171620)
 *    +0x18 0x1f6510 (0x70; 0x1f63f0)    +0x1c 0x190c20 (4; 0x190bc0)
 *    +0x20 ownsSound: +0x24 made here   +0x24 StageSound (stage; given or new)
 *    +0x28 DemoStream (sub, "streamEU/demo/"), and 0x2cf510() for a stage
 *    +0x2c 0x1fa4a0 (0xe70; 0x1fa230)   +0x30 KeepList (stage; 0x2bc770)
 *    +0x34 StageViewStack (8 entries)   +0x38..+0x5c zero; +0x44 / +0x54
 *          arrays, +0x4c an object (vptr +0), +0x58 0x2dcab0's (dtor only)
 *    +0x60 a Clock under gGameSystem +0x510
 *    +0x78 player .. +0x84, +0x8c..+0x9c zero (+0x8c..+0x94 objects with
 *          the vptr at +0, +0x98 a block, +0x9c 0x2ce5a0's: dtor only)
 *    +0x88 StageParty (stage; 0x1fecf0, 4 bytes)
 *    +0xa0 0x151f20 (0x24; 0x04808080, 0x700, 0x700, 0x200, 0x200; vptr +4)
 *    +0xa4 / +0xa8 zero (0x2ad070 / 0x2bb3e0 delete them)
 *    +0xac..+0xb8 four 0x284640 area grids (stage; 0x240 each; dtor
 *          0x284580); getSoundAttr reads +0xb8
 *    +0xbc 0x2d4110 (0x40; 0x700, 0x700: a FontPrinter inside; vptr +0x10)
 *    +0xc0 0x289be0 (stage; 0x2c; 4, 0x800, 0x878)
 *    +0xc4 0x207ef0 (stage; 0x678)      +0xc8 u16 zero
 *    +0xcc 0x23f0b0 (stage; 0x8c, on +0xbc; vptr +0x88)
 *    +0xd0 0x2308a0 (stage; 0x18, on +0xbc; vptr +8)  +0xd4 / +0xd8 zero
 *    +0xdc 0x23ad60 (0xc4; 0x23ad10)    +0xe0 0x243150 (stage; 0x3cc)
 *    +0xe4 0x2682e0 (stage; 8)          +0xe8 StageMission (stage; 0x287900, 0x13d0)
 *    +0xec StageEvent (zero here)       +0xf0 UiShowFlags (stage; 0x11)
 *    +0xf4 0x264070 (stage; 0x1c: game/stagealert.cpp)
 *    +0xfc StageFade (0x151600; 0x3c; 0x700, 0x700, 0x200, 0x200, 0)
 *    +0x100 the view target (zero here)  +0x104 byte, +0x108 zero
 *    +0x10c parent, +0x110 files (zero here; stageThread sets them)
 *    +0x114 byte; +0x118 / +0x11c blocks, +0x120 0x2b2280's, +0x124 a
 *          record with two arrays, +0x12c an object (+0x24: 0x2cff30) --
 *          zero here, deleted by the dtor.
 *  A sub-scene also calls 0x2d7c70 in the ctor and 0x2d7c00 in the dtor.
 * ======================================================================== */
#ifndef _GAME_STAGESCENE_H
#define _GAME_STAGESCENE_H

#include "sdk/libvu0.h"     /* sceVu0FVECTOR */
#include "sdk/scetypes.h"

class Clock;                    // game/clock.h
class DemoStream;               // game/gamehooks.h
class FileLoader;               // game/fileloader.h
class StageEvent;               // game/gamehooks.h
class StageFade;                // game/gamehooks.h
class StageFiles;
class StageObject;              // game/gamehooks.h
class StageObjectList;          // game/stageobjlist.h
class StageSound;               // game/stagesound.h
struct KeepList;                // game/gamehooks.h
struct StageCollisionFace;      // game/gamehooks.h
struct StageParty;              // game/gamehooks.h
struct StagePlayer;             // game/gamehooks.h
struct StreamTable;             // game/seplayer.h

// The stack of who the stage is heard and seen through (StageScene +0x34;
// new'd by the constructor with 8 entries of 0xc, top -1). Only the top's
// object is read here (StageSound's listener); who pushes is unread
// (guess: the player, and a character the player looks through).
// layout: host -- EE 0xc: +0 capacity, +4 top (-1 empty), +8 the entries.
struct StageViewEntry
{
    StageObject *object;        // FS2 +0x0
    u_char       _04[0x8];
};
// layout: host -- EE 0xc (above).
struct StageViewStack
{
    // The top entry's object; null while the stack is empty (inline in
    // the ROM).
    StageObject *topObject() const { return top < 0 ? nullptr : entries[top].object; }

    int             capacity;   // FS2 +0x0: 8
    int             top;        // FS2 +0x4: -1 empty
    StageViewEntry *entries;    // FS2 +0x8
};

// The object at StageScene +0x100 (set outside the constructor). Only the
// character at +0x3c0 is read (drawPaused); the name is a guess.
// layout: host -- EE: +0x3c0 the character.
struct StageViewTarget
{
    StageObject *character;     // FS2 +0x3c0
};

// The stage's mission state (StageScene +0xe8: 0x287900 builds it, 0x13d0
// bytes; unassigned@002681e0). Only the purpose text is read here; the
// class name is a guess.
// layout: host -- EE: the purpose text inline at +4.
struct StageMission
{
    u_char  _00[0x4];
    u_short purposeText[1];     // FS2 +0x4: 16-bit, 0-terminated (extent unread)
};

// The current stage object (0x130 bytes): built by stageLoad 0x17fb90 for
// the stage and by stageThread for a sub-scene (game/stageloop.cpp).
// layout: host -- EE offsets in the comments; the full member map is the
// file comment above.
struct StageScene
{
    StageScene(bool isSubScene, StageSound *sound);                      // FS2 0x0019c180
    ~StageScene();                                                       // FS2 0x0019bbe0: deleting

    // The frame: update, then draw for the active character.
    void update();                                                       // FS2 0x00199400
    void draw(StageObject *active);                                      // FS2 0x00198970
    // The frame while a dialog or menu thread runs: nothing is drawn, only
    // the sound goes on (a stage's StageSound, a sub-scene's demo voice).
    void drawPaused();                                                   // FS2 0x00198fc0
    // The character the player controls (null none).
    StageObject *getActiveCharacter();                                   // FS2 0x0019bbb0
    // Once, before the stage's first frame.
    void beginPlay();                                                    // FS2 0x001963a0
    void setupCameras();                                                 // FS2 0x00199f00
    void enterSubScene(void *data);                                      // FS2 0x00195e10
    void clearSubSceneRequest();                                         // FS2 0x00196050: subSceneRequest = 0

    bool isGameOver();                                                   // FS2 0x001956e0
    bool isCleared();                                                    // FS2 0x00195640
    // A sub-scene whose own scene (subScene) has ended.
    bool isSubSceneDone();                                               // FS2 0x00195d80
    // Start / select on pad 0 while the stage allows its menu.
    bool wantsPauseMenu();                                               // FS2 0x00195a50
    bool wantsMapMenu();                                                 // FS2 0x001958b0

    // The floor the player is on (the S2D's rF01..rFDF: 0..9); negative
    // none.
    int getFloor(StagePlayer *player);                                   // FS2 0x0019aea0

    // The nearest collision face on the segment from..to (hit: where; left
    // as the ROM leaves it -- see the code -- when nothing is hit); null
    // none. Only the active Root objects' (group 1) meshes count, skip
    // left out; mask / flags / arg go to each mesh (StageSound's values in
    // game/stagesound.cpp).
    StageCollisionFace *raycast(sceVu0FVECTOR hit, const sceVu0FVECTOR from,
                                const sceVu0FVECTOR to, int arg, u_int mask,
                                u_int flags, StageObject *skip);         // FS2 0x00198170
    // The sound attr at pos: inside the area grid of +0xb8 (0x284190).
    // StageSound hands it to every placed sound (setAttr); reverb likely.
    bool getSoundAttr(const sceVu0FVECTOR pos);                          // FS2 0x0019ae10

    // The scene a sub-scene was built under (null for the stage).
    StageScene *getParent();                                             // FS2 0x00196950
    // The mission state: this scene's, else the nearest parent's (a
    // sub-scene has none of its own).
    StageMission *getMission();                                          // FS2 0x00196870
    // The stage's current objective as 16-bit text (the S2D's rPurpose).
    const u_short *getPurposeText();                                     // FS2 0x00195d50

    u_char           isSubScene;      // FS2 +0x00: the constructor's first argument
    u_char           _01[0x3];
    int              subSceneRequest; // FS2 +0x04: set: stageThread builds a sub-scene
    void            *subScene;        // FS2 +0x08: a sub-scene's own scene (enterSubScene); null once it ended
    u_char           _0c[0x4];
    StageObjectList *objects;         // FS2 +0x10
    u_char           _14[0xc];
    u_char           ownsSound;       // FS2 +0x20: sound was made by the constructor (the dtor deletes it)
    u_char           _21[0x3];
    StageSound      *sound;           // FS2 +0x24: a stage's (null in a sub-scene)
    DemoStream      *demoStream;      // FS2 +0x28: a sub-scene's (null in a stage)
    u_char           _2c[0x4];
    KeepList        *keepList;        // FS2 +0x30
    StageViewStack  *views;           // FS2 +0x34
    u_char           _38[0x28];
    Clock           *clock;           // FS2 +0x60: paused while a menu runs
    u_char           _64[0x14];
    StagePlayer     *player;          // FS2 +0x78
    u_char           _7c[0xc];
    StageParty      *party;           // FS2 +0x88
    u_char           _8c[0x5c];
    StageMission    *mission;         // FS2 +0xe8: a stage's (getMission)
    StageEvent      *event;           // FS2 +0xec
    u_char           _f0[0xc];
    StageFade       *fade;            // FS2 +0xfc
    StageViewTarget *viewTarget;      // FS2 +0x100
    u_char           _104[0x8];
    StageScene      *parent;          // FS2 +0x10c: a sub-scene's (stageThread)
    StageFiles      *files;           // FS2 +0x110: a sub-scene's (stageThread)
};

// The path kinds that can read from a debug or a proof tree
// (StageFiles::dataDir, StageFiles::debugTree).
enum StageDataKind
{
    StageData_Bg        = 0,    // bg/
    StageData_Chara     = 1,    // chara/
    StageData_Motion    = 2,    // motion/
    StageData_Item      = 3,    // item/
    StageData_KindCount = 4,
};

// The stage's files (sbss 0x31adfc: getStageFiles; built by stageLoad over
// the stage loader, game/stageloop.cpp): the stage script and what its
// lines name, loaded in two passes.
// layout: host -- EE 0x160: +0 the loader, +4 a flag (1), 22 containers
// from +0xc (the string maps / lists 0x1a4e80..0x1a6790), +0x124..+0x127
// the debug-tree flags, +0x128 the pass.
class StageFiles
{
public:
    explicit StageFiles(FileLoader *loader);                             // FS2 0x001a4270
    ~StageFiles();                                                       // FS2 0x001a3f90: deleting

    // Reads the stage script at path: the loader loads it, then two passes
    // over its lines -- the first lists every file they name, the loader
    // loads them all, the second takes what was loaded. The second pass's
    // answer; 0 when the script did not load or the first pass failed.
    int  load(const char *path);                                         // FS2 0x001a37e0
    // spotlight / shadow_00.
    void loadCommon();                                                   // FS2 0x001a38e0
    // ../data/listfile/s%02dlist.lst for a stage (not a sub-scene).
    void loadList();                                                     // FS2 0x001a3a60
    // Called for a sub-scene where loadList would be: empty in retail.
    void loadSubSceneList();                                             // FS2 0x001a3db0

    // The data tree a script path is read from: ../data/debug/ for a bg/,
    // chara/, motion/ or item/ path whose debug flag is set, ../data/proof/
    // for one whose flag is clear, ../data/ for any other.
    const char *dataDir(const char *path);                               // FS2 0x001a1ca0

    FileLoader *loader;         // FS2 +0x000
    // Per StageDataKind: read from ../data/debug/ rather than
    // ../data/proof/ (set outside this slice).
    u_char      debugTree[StageData_KindCount]; // FS2 +0x124
    u_char      listPass;       // FS2 +0x128: 1 during load's first pass (lines list their files)

private:
    // Runs the script the loader holds at index line by line (target,
    // texture, model, motion, collision, particle, script, sndxv, sndseh,
    // areaclip, ...); false on the first line that fails.
    bool parseScript(int index);                                         // FS2 0x001a20f0
};

// An StDT stream table's header (beside its other reader 0x1a0040, which
// relocates it): relocated, entry count.
bool stdtIsRelocated(const StreamTable *table);                          // FS2 0x001a0570
int  stdtEntryCount(const StreamTable *table);                           // FS2 0x001a0560

#endif /* _GAME_STAGESCENE_H */
