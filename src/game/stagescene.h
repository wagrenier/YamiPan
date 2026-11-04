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
 *  0x19bbe0 (both written; the member classes got names in batch 080 and
 *  stand-ins in gamehooks.h, the fields below). "sub" marks what only a sub-scene builds, "stage" what
 *  only a stage builds (the other one leaves it null):
 *    +0x10 StageObjectList (0xdc)       +0x14 0x171680 (0x2c; dtor 0x171620)
 *    +0x18 0x1f6510 (0x70; 0x1f63f0)    +0x1c 0x190c20 (4; 0x190bc0)
 *    +0x20 ownsSound: +0x24 made here   +0x24 StageSound (stage; given or new)
 *    +0x28 DemoStream (sub, "streamEU/demo/"), and 0x2cf510() for a stage
 *    +0x2c 0x1fa4a0 (0xe70; 0x1fa230)   +0x30 KeepList (stage; 0x2bc770)
 *    +0x34 StageViewStack (8 entries)   +0x38..+0x5c zero; +0x44 / +0x54
 *          arrays, +0x4c an object (vptr +0), +0x58 0x2dcab0's (dtor only)
 *    +0x60 a Clock under gGameSystem +0x510
 *    +0x78 player .. +0x84 (controlRequest), +0x8c..+0x9c zero (+0x8c..+0x94 objects with
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

#include "game/stagelist.h"
#include <stdint.h>

#include "sdk/libvu0.h"     /* sceVu0FVECTOR */
#include "sdk/scetypes.h"

// The members' classes: game/gamehooks.h (stand-ins, batch 080) unless
// noted.
class Clock;                    // game/clock.h
class DemoStream;
class SceneOwnedObject;
class ScreenFill;
class StageMenu;
class SubSceneObject;
class UiItemList;
class WideFontPrinter;          // game/fontdata.h
class FontData;                 // game/fontdata.h
class RoutePlayer;
class Tim2Texture;              // game/gsutil.h
struct AreaGrid;
struct ClearSequence;
struct AreaClip;
struct GameOverSequence;
struct MenuProgHolder;
struct MenuProgSet;
struct PauseMenuState;
struct Rand48;
struct SceneArrayPair;
struct SceneBoxList;
struct SceneBlock98;
struct SceneBufferSet;
struct SceneDirector;
struct SceneModelHolder;
struct ScenePairTable;
struct ScenePool;
struct SceneRecordBuckets;
struct SceneRender;
struct SceneUiState;
struct StageAlert;
struct StageNodeTree;
struct UiShowFlags;             // game/uiwidget.h
class FileLoader;               // game/fileloader.h
class GamePacketManager;        // game/gamepacket.h
class StageEvent;               // game/gamehooks.h
class StageFade;                // game/gamehooks.h
class PakArchive;               // game/pakarchive.h
class StageFiles;
class StageObject;              // game/gamehooks.h
class StageObjectList;          // game/stageobjlist.h
class StageSound;               // game/stagesound.h
struct KeepList;                // game/gamehooks.h
struct LptFile;                 // game/gamehooks.h
struct StageCollisionFace;      // game/gamehooks.h
struct StageParty;              // game/gamehooks.h
struct StagePlayer;             // game/gamehooks.h
struct StageMapFile;            // game/gamehooks.h
struct StageScript;             // game/gamehooks.h
struct StreamTable;             // game/seplayer.h

struct SubSceneData;            // game/gamehooks.h
class AreaBox;                  // game/gamehooks.h

// The stack of who the stage is heard and seen through (StageScene +0x34;
// new'd by the constructor with 8 entries of 0xc, top -1). Only the top's
// object is read here (StageSound's listener); who pushes is unread
// (guess: the player, and a character the player looks through).
// remove: the entry's object leaves wherever it stands, and the new top
// becomes active.
// layout: host -- EE 0xc: +0 capacity, +4 top (-1 empty), +8 the entries.
struct StageViewEntry
{
    StageObject *object;        // FS2 +0x0
    int          priority;      // FS2 +0x4: the stack stays sorted by it, highest on top
    u_char       flag;          // FS2 +0x8: only stored here (0 at both calls read)
};
// layout: host -- EE 0xc (above).
struct StageViewStack
{
    // The top entry's object; null while the stack is empty (inline in
    // the ROM).
    StageObject *topObject() const { return top < 0 ? nullptr : entries[top].object; }
    void remove(StageObject *object);                                    // FS2 0x0019cb50
    // object goes above the entries of its priority or lower, unless it is
    // in the stack already; a new top becomes active.
    void push(StageObject *object, int priority, u_char flag);           // FS2 0x0019cdc0
    // The topmost entry of that priority leaves; its object, null none.
    StageObject *removePriority(int priority);                           // FS2 0x0019cc80

    int             capacity;   // FS2 +0x0: 8
    int             top;        // FS2 +0x4: -1 empty
    StageViewEntry *entries;    // FS2 +0x8
};

// The object at StageScene +0x100 (set outside the constructor). Only the
// character at +0x3c0 is read (drawPaused); the name is a guess.
// layout: host -- EE: +0x3c0 the character.
struct StageModel;              // game/gamehooks.h
struct StageViewTarget
{
    StageModel  *model;         // FS2 +0x30: prepareDrawFlags marks it
    StageObject *character;     // FS2 +0x3c0
};

// The stage's mission state (StageScene +0xe8: 0x287900 builds it, 0x13d0
// bytes; unassigned@002681e0). Only the purpose text is read here; the
// class name is a guess.
// layout: host -- EE: the purpose text inline at +4.
struct StageMission
{
    StageMission();                                                      // FS2 0x00287900
    ~StageMission();                                                     // FS2 0x00287820: deleting
    // The start time (+0xac) = the scene clock's seconds (beginPlay).
    void markStart();                                                    // FS2 0x002870c0
    // Forwards the message file to the object in the first field.
    void setMessages(void *file);                                        // FS2 0x00286c30
    // Takes the map's volumes by name, after the map is read.
    void setup();                                                        // FS2 0x002870f0

    // spawn id's place (kept once; the first call wins).
    void setSpawnPoint(int id, const float *pos, const float *rot);      // FS2 0x002867a0
    // StageScene::restoreObjectFlags, first.
    void onRestore();                                                    // FS2 0x002868c0

    u_char  _00[0x4];
    u_short purposeText[0x5c];  // FS2 +0x4: 16-bit, 0-terminated (extent: up to keptObject)
    StageObject *keptObject;    // FS2 +0xbc: keeps its flags through restoreObjectFlags
    u_char  _c0[0x8];
    u_char  busy;               // FS2 +0xc8: StageScene::isBusy
    u_char  _c9[0x11];
    u_char  blackout;           // FS2 +0xda: set: StageScene::update fades the screen to black, clear: back
};

// The scene's four area grids (+0xac..+0xb8), filled from the map file's
// boxes (StageFiles::applyMapTarget).
enum StageArea
{
    StageArea_Floor  = 0,       // +0xac: ENV_MB_f%02d (getFloor)
    StageArea_Ab     = 1,       // +0xb0: ENV_AB (unread)
    StageArea_Smoke  = 2,       // +0xb4: SMOKE_A
    StageArea_Reverb = 3,       // +0xb8: REVERB_ON (getSoundAttr)
    StageArea_Count  = 4,
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
    // Once, after the stage's files: the cameras, bullet and flash, the
    // bg script, the first player, what setup needs of the map.
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
    // What the player is doing, for the two tests above (inline in the
    // ROM): PlayerAction.
    int  playerAction();
    // What the menus must wait for: a full fade, or isBusy.
    bool isInputBlocked();                                               // FS2 0x00195850
    // The stage is ending, the mission is busy, or an event runs whose
    // script is not the one asking.
    bool isBusy();                                                       // FS2 0x001957a0
    // Every object's saved flags back, keptObject's left as they were.
    void restoreObjectFlags();                                           // FS2 0x00195520
    // object may act with the partner (+0x7c): the partner's answer while
    // the player's look angle is in the window. The reading of the fields
    // is a guess.
    bool partnerResponds(StageObject *object);                           // FS2 0x001953f0
    // The object leaves the view stack.
    void removeView(StageObject *object);                                // FS2 0x0019bb60
    void removeViewPriority(int priority);                               // FS2 0x0019bb70
    void pushView(StageObject *object, int priority, u_char flag);       // FS2 0x0019bb80
    // who becomes the player: input, brain and the two cameras move over.
    void setPlayer(StagePlayer *who);                                    // FS2 0x0019b560
    // The view target is in view: the target is the player and the view is
    // not camjack, or the view is camjack and the target the companion
    // (inline in the ROM: drawPaused, update, prepareDrawFlags).
    bool targetInView();
    // What takes the buttons now (cached in focus for the frame): the open
    // menu, the item list, the pause state, the character.
    void *getInputFocus();                                               // FS2 0x00195c10
    // loadCommon: the menu program of the init pack's item.
    void createMenuProg(const void *data);                               // FS2 0x0019af40

    // The floor the player is on (the S2D's rF01..rFDF: 0..9); negative
    // none.
    int getFloor(StagePlayer *who);                                      // FS2 0x0019aea0

    // The nearest collision face on the segment from..to (hit: where; left
    // as the ROM leaves it -- see the code -- when nothing is hit); null
    // none. Only the active Root objects' (group 1) meshes count, skip
    // left out; mask / flags / arg go to each mesh (StageSound's values in
    // game/stagesound.cpp).
    StageCollisionFace *raycast(sceVu0FVECTOR hit, const sceVu0FVECTOR from,
                                const sceVu0FVECTOR to, int arg, u_int mask,
                                u_int flags, StageObject *skip);         // FS2 0x00198170
    // The sound attr at pos: inside a REVERB_ON box (+0xb8, 0x284190).
    // StageSound hands it to every placed sound (setAttr).
    bool getSoundAttr(const sceVu0FVECTOR pos);                          // FS2 0x0019ae10

    // A number in 0..n-1 from the stage's generator.
    int random(int n);                                                   // FS2 0x00195d40
    SubSceneData *getSubSceneBlock();                                    // FS2 0x00195d70
    // The stage's menu while it is open; null otherwise.
    StageMenu *getOpenMenu();                                            // FS2 0x001963d0
    // AreaGrid::addBox on the grids.
    void addReverbBox(AreaBox *box);                                     // FS2 0x0019ae30
    void addSmokeBox(AreaBox *box);                                      // FS2 0x0019ae40
    void addAbBox(AreaBox *box);                                         // FS2 0x0019ae90
    void addFloorBox(AreaBox *box);                                      // FS2 0x0019af30
    // At the next update: the player's control back; object takes it.
    void releaseControl();                                               // FS2 0x0019b540
    void requestControl(StageObject *object);                            // FS2 0x0019b550
    // The bg target's: the last model's name, its areaclip.
    void setBgName(const char *name);                                    // FS2 0x0019b740
    void setAreaClip(void *file);                                        // FS2 0x0019b750
    // The message keyword's file goes to the current mission.
    void setMessages(void *file);                                        // FS2 0x00196840
    // A stage whose StageEvent is in its active state.
    bool isEventActive();                                                // FS2 0x001954c0

    // The stage script's two files (StageFiles::applyScriptTarget) and a
    // zeroed slot per the first's slotCount.
    void setScripts(StageScript *script, StageScript *script2);          // FS2 0x0019b7a0
    // The rute file and a new RoutePlayer over it, started.
    void setRoute(void *rute);                                           // FS2 0x00196280

    // The scene a sub-scene was built under (null for the stage).
    StageScene *getParent();                                             // FS2 0x00196950
    // Second out-of-line copies of getParent / getMission (the same code;
    // likely the const overloads): most callers use these.
    StageScene *parentScene();                                           // FS2 0x00196a40
    StageMission *currentMission();                                      // FS2 0x00196960
    // The mission state: this scene's, else the nearest parent's (a
    // sub-scene has none of its own).
    StageMission *getMission();                                          // FS2 0x00196870
    // The stage's current objective as 16-bit text (the S2D's rPurpose).
    const u_short *getPurposeText();                                     // FS2 0x00195d50

    // The frame's pieces (update and draw only).
    void calcMatrices();                                                 // FS2 0x0019a850
    void propagateNodes();                                               // FS2 0x0019aa30
    void updateVcms();                                                   // FS2 0x0019a710
    void updateRumble(float frames);                                     // FS2 0x00199130
    // Whose KeepList entries rumble.
    StageObject *getRumbleSource();                                      // FS2 0x00199250
    void collectViewTargets();                                           // FS2 0x00197160
    void prepareDrawFlags();                                             // FS2 0x00198560
    void drawViewFilter(GamePacketManager *packets, StageObject *camera); // FS2 0x001983b0
    // A stage's scripts and mission lists, in update's script step.
    void runStageScripts();                                              // FS2 0x0019aba0
    void updateBlock98();                                                // FS2 0x0019b1f0
    void updateBuffers();                                                // FS2 0x0019afb0
    // controlled gives the buttons back (inline in update).
    void releaseControlled();

    u_char              isSubScene;      // FS2 +0x00: the constructor's first argument
    int                 subSceneRequest; // FS2 +0x04: set: stageThread builds a sub-scene
    SubSceneObject     *subScene;        // FS2 +0x08: a sub-scene's own scene (enterSubScene); null once it ended
    SubSceneData       *subSceneBlock;   // FS2 +0x0c: what enterSubScene was given; deleted beside subScene
    StageObjectList    *objects;         // FS2 +0x10
    ScenePairTable     *pairs;           // FS2 +0x14
    SceneRecordBuckets *records;         // FS2 +0x18
    StageNodeTree      *nodeTree;        // FS2 +0x1c
    u_char              ownsSound;       // FS2 +0x20: sound was made by the constructor (the dtor deletes it)
    StageSound         *sound;           // FS2 +0x24: a stage's (null in a sub-scene)
    DemoStream         *demoStream;      // FS2 +0x28: a sub-scene's (null in a stage)
    SceneRender        *render;          // FS2 +0x2c
    KeepList           *keepList;        // FS2 +0x30: a stage's
    StageViewStack     *views;           // FS2 +0x34
    StageScript        *bgScript;        // FS2 +0x38: the bg target's last script (unread)
    StageScript        *scripts[2];      // FS2 +0x3c, +0x40: setScripts
    int                *scriptSlots;     // FS2 +0x44: one per scripts[0]->slotCount
    void               *route;           // FS2 +0x48: the rute file (setRoute)
    RoutePlayer        *routePlayer;     // FS2 +0x4c: setRoute
    int                 viewTargetCount; // FS2 +0x50: collectViewTargets
    StageObject       **viewTargets;     // FS2 +0x54: the view target, oSJ24_02, oSJ24_03 where they exist
    AreaClip           *areaClip;        // FS2 +0x58: setAreaClip
    LptFile            *lpt;             // FS2 +0x5c: the bg target's
    Clock              *clock;           // FS2 +0x60: paused while a menu runs
    int                 frameCount;      // FS2 +0x64: update counts
    char                bgName[0x10];    // FS2 +0x68: setBgName; not set by the ctor
    StagePlayer        *player;          // FS2 +0x78
    StageObject        *partner;         // FS2 +0x7c: partnerResponds; the name is a guess
    StageObject        *controlled;      // FS2 +0x80: acts for the player when set (playerAction)
    // 0 nothing, 1 back to the player, else the StageObject to take over
    // (the ROM keeps both in one word).
    uintptr_t           controlRequest;  // FS2 +0x84
    StageParty         *party;           // FS2 +0x88: a stage's
    SceneOwnedObject   *owned[3];        // FS2 +0x8c..+0x94
    SceneBlock98       *block98;         // FS2 +0x98: deleted
    SceneBufferSet     *buffers;         // FS2 +0x9c
    ScreenFill         *screenFill;      // FS2 +0xa0
    MenuProgHolder     *menuProg;        // FS2 +0xa4
    MenuProgSet        *menuProgs;       // FS2 +0xa8
    // Four area grids, a stage's; [3] (+0xb8) is the sound one (getSoundAttr).
    AreaGrid           *areas[StageArea_Count]; // FS2 +0xac
    WideFontPrinter    *printer;         // FS2 +0xbc
    ScenePool          *pool;            // FS2 +0xc0: a stage's
    PauseMenuState     *pauseState;      // FS2 +0xc4: a stage's
    u_char              inFrame;         // FS2 +0xc8: set by update (unread)
    u_char              itemPicked;      // FS2 +0xc9: the item list took the action button this frame
    UiItemList         *itemList;        // FS2 +0xcc: a stage's
    StageMenu          *menu;            // FS2 +0xd0: a stage's
    StageObject        *rootAny;         // FS2 +0xd4: the last group's first "Root"
    StageObject        *root;            // FS2 +0xd8: group 1's
    SceneBoxList       *boxes;           // FS2 +0xdc
    SceneUiState       *uiState;         // FS2 +0xe0: a stage's
    Rand48             *rng;             // FS2 +0xe4: a stage's
    StageMission       *mission;         // FS2 +0xe8: a stage's (getMission)
    StageEvent         *event;           // FS2 +0xec
    UiShowFlags        *showFlags;       // FS2 +0xf0: a stage's
    StageAlert         *alert;           // FS2 +0xf4: a stage's
    void               *focus;           // FS2 +0xf8: getInputFocus's cache, cleared each frame; not set by the ctor
    StageFade          *fade;            // FS2 +0xfc
    StageViewTarget    *viewTarget;      // FS2 +0x100
    u_char              _104;            // FS2 +0x104: zero (unread)
    int                 _108;            // FS2 +0x108: zero (unread)
    StageScene         *parent;          // FS2 +0x10c: a sub-scene's (stageThread)
    StageFiles         *files;           // FS2 +0x110: a sub-scene's (stageThread)
    u_char              camerasReady;    // FS2 +0x114: setupCameras ran
    GameOverSequence   *gameOver;        // FS2 +0x118
    ClearSequence      *clear;           // FS2 +0x11c
    SceneDirector      *director;        // FS2 +0x120
    SceneArrayPair     *arrays;          // FS2 +0x124
    u_char              altLatch;        // FS2 +0x128: while set, update latches the player's alternate action; not set by the ctor
    SceneModelHolder   *model;           // FS2 +0x12c
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

// One model line under a `character` target (StageFiles::characters).
// layout: host -- EE 0x1c: the offsets below.
struct StageCharacterModel
{
    void             *model;    // FS2 +0x00: the model's item (null for "nothing")
    char             *altName;  // FS2 +0x04: new[]'d; null without one
    StageList<void *> motions;  // FS2 +0x08: from the line's first motion list
    StageList<void *> motions2; // FS2 +0x10: from the next one
    void             *mdl;      // FS2 +0x18: the mdl of the same position; null past the end
};

// A value of StageFiles::groups (+0x14c), one per model of an `item` target.
// layout: host -- EE 0xc.
struct StageFileGroup
{
    void             *model;    // FS2 +0x0: the model's item
    StageList<void *> motions;  // FS2 +0x4: copied from the block's motion list
};

// The stage's files (sbss 0x31adfc: getStageFiles; built by stageLoad over
// the stage loader, game/stageloop.cpp): the stage script and what its
// lines name, loaded in two passes. The lists +0xc..+0x9c live for one
// `target` .. `end` block (reset).
// layout: host -- EE 0x160: the offsets below.
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

    // A character of the characters map, and a new item of the groups map,
    // at pos / rot (null: where they are); the ROM's event code calls
    // these too (0x1b3dc0, 0x1b9680).
    void placeCharacter(const char *name, const float *pos, const float *rot); // FS2 0x0019d4e0
    void placeItem(const char *name, const float *pos, const float *rot); // FS2 0x0019d0c0

    FileLoader *loader;         // FS2 +0x000
    u_char      sharedLoader;   // FS2 +0x004: 1: the dtor leaves loader alone (the ctor's value)
    const char *target;         // FS2 +0x008: the open block's name, in its line; null none

    // What a block's lines took (by keyword), +0xc..+0x9c.
    StageList<void *>  models;       // FS2 +0x00c: model (null for "nothing")
    StageList<char *>  modelNames;   // FS2 +0x014: model, new[]'d
    StageList<char *>  modelAltNames; // FS2 +0x01c: model, new[]'d; null without one
    StageList<StageList<void *> *> motionLists; // FS2 +0x024: motion
    StageList<char *>  motionNames;  // FS2 +0x02c: motion, new[]'d
    StageList<void *>  mdls;         // FS2 +0x034: mdl
    StageList<void *>  collisions;   // FS2 +0x03c: collision
    StageList<void *>  particles;    // FS2 +0x044: particle
    StageList<void *>  maps;         // FS2 +0x04c: map
    StageList<char *>  mapNames;     // FS2 +0x054: map, new[]'d
    StageList<StageScript *> scripts; // FS2 +0x05c: script
    StageList<void *>  rutes;        // FS2 +0x064: rute
    StageList<void *>  sndxvs;       // FS2 +0x06c: sndxv
    StageList<void *>  sndsehs;      // FS2 +0x074: sndseh
    StageList<StageList<void *> *> sndsehLists; // FS2 +0x07c: sndseh
    StageList<FontData *> fonts;     // FS2 +0x084: font
    StageList<void *>  areaclips;    // FS2 +0x08c: areaclip
    StageList<void *>  bbxs;         // FS2 +0x094: bbx
    StageList<void *>  lpts;         // FS2 +0x09c: lpt
    char        streamTablePath[0x40]; // FS2 +0x0a4: sndsth's .sth
    char        fsmPath[0x40];  // FS2 +0x0e4: sndsth's .fsm

    // Per StageDataKind: read from ../data/debug/ rather than
    // ../data/proof/ (set outside this slice).
    u_char      debugTree[StageData_KindCount]; // FS2 +0x124
    u_char      listPass;       // FS2 +0x128: 1 during load's first pass (lines list their files)

    // What outlives the blocks, by name.
    StringMap<Tim2Texture *> textures;   // FS2 +0x12c: owned
    StringMap<StageList<StageCharacterModel *> *> characters; // FS2 +0x13c: owned
    StringMap<StageFileGroup *> groups;  // FS2 +0x14c: owned

private:
    // Runs the script the loader holds at index line by line; false on the
    // first line that fails or that has an unknown keyword.
    bool parseScript(int index);                                         // FS2 0x001a20f0
    // One line, its keyword already cut (inline in parseScript).
    bool parseLine(const char *keyword, bool isSubScene);
    // A keyword line that only names a file: the first pass lists it, the
    // second puts its item in list and in *item. False with no path or no
    // item (inline for every such keyword in the ROM).
    template <class T>
    bool takeFile(StageList<T> &list, T *item);
    // Drops everything the lists +0xc..+0xe4 hold.
    void reset();                                                        // FS2 0x001a3dc0

    // target NAME and end: one block.
    bool parseTarget();                                                  // FS2 0x001a1b50
    bool parseEnd();                                                     // FS2 0x001a1820
    // What end does with the block, by target.
    bool applyScriptTarget();                                            // FS2 0x0019fcd0
    bool applyCharacterTarget();                                         // FS2 0x0019fac0
    bool applyBgTarget();                                                // FS2 0x0019f680
    bool applyBgMotionTarget();                                          // FS2 0x0019f230
    bool applyParticleTarget();                                          // FS2 0x0019f140
    bool applyItemTarget();                                              // FS2 0x0019f020
    bool applyMapTarget();                                               // FS2 0x0019ddf0
    bool applySoundTarget();                                             // FS2 0x0019d9e0

    // The keywords with more to do than takeFile.
    bool parseTexture();                                                 // FS2 0x001a1430
    bool parseModel();                                                   // FS2 0x001a0e70
    bool parseMotion();                                                  // FS2 0x001a0ae0
    bool parseMap();                                                     // FS2 0x001a0950
    bool parseSndseh();                                                  // FS2 0x001a0580
    bool parseSndsth();                                                  // FS2 0x001a0040
    bool parseMessage();                                                 // FS2 0x0019fdf0
    // A .mot's lines: each names an item of pack, or of the loader without
    // a pack (only parseMotion calls it, with one); a # line stands for a
    // null entry. The first pass only lists. False on a missing item.
    bool readMotionList(const char *text, PakArchive *pack, int size,
                        StageList<void *> *list);                        // FS2 0x001a1df0
    // The loader's directory = its own + path's (inline in parseModel,
    // parseMotion, parseSndseh).
    void enterDirOf(const char *path);
    // A bank's file in parseSndseh (inline in the ROM).
    void takeBankFile(const char *file, StageList<void *> *list);
    // loadCommon's texture: the one of that name, else a new one of the
    // init pack's item (inline in the ROM).
    Tim2Texture *commonTexture(const char *name, const void *image);
    // A map file's sections (inline in applyMapTarget).
    void placeMapPoints(const StageMapFile *map);
    void placeMapCameras(const StageMapFile *map);
    void placeMapLights(const StageMapFile *map);
    void placeMapBoxes(const StageMapFile *map);
    void placeMapMarks(const StageMapFile *map);
};

// An StDT stream table's header (beside its other reader 0x1a0040, which
// relocates it): relocated, entry count.
bool stdtIsRelocated(const StreamTable *table);                          // FS2 0x001a0570
int  stdtEntryCount(const StreamTable *table);                           // FS2 0x001a0560

// The stage clock's frames per second.
int stageFps();                                                          // FS2 0x00196300

#endif /* _GAME_STAGESCENE_H */
