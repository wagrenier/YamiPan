/* ==========================================================================
 *  game/uilayout.h
 *
 *  The 2D layout player (game/uilayout.cpp, 0x243390..0x246230): a UiLayout
 *  plays one S2D file (the .d2b files under 2dEU/ and the pack items, magic
 *  "S2D\0"): its sequences run small scripts that start and stop other
 *  sequences, animate actors through anim sets, create widgets, play
 *  sounds, read the pad, set variables, open a nested layout and end the
 *  layout with a result (m_result / m_done, which the modal runner in
 *  game/screen.cpp waits for).
 *
 *  A frame is advance() (sequences step their animations), update() (each
 *  sequence's frame commands until nothing starts or stops any more) and
 *  draw() (sequences in priority order through the UiDrawer). A layout can
 *  hold one nested layout (UiSubLayout, its base) loaded by a script command
 *  from a file or the stage pack; it runs and draws on top of its parent.
 *
 *  The sequence / actor / widget / drawer classes and the S2D file live in
 *  the rest of unassigned@0023e740 (0x246230..0x2640e8) and the layout's game
 *  hooks at 0x25ff40..; what this file needs of them is declared here until
 *  those TUs are cut. The file and class names are the port's (no source
 *  string in the range).
 * ======================================================================== */
#ifndef _GAME_UILAYOUT_H
#define _GAME_UILAYOUT_H

#include <stddef.h>

#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

class FileJob;                  // game/gamehooks.h
class UiLayout;
class UiSequence;

// ──────────────────────────────────────────────────────────────────────
// The S2D file (UiLayout::m_data). Offsets become pointers in place on
// the first S2dData::relocate.
// layout: host -- EE offsets in the comments; the pointer fields are 32-bit
// offsets in the file and pointers after relocate (the S2D TU must widen
// them on the host).

// A script variable (0x20 bytes).
struct S2dVar
{
    char name[0x10];            // FS2 +0x00
    int  value;                 // FS2 +0x10
    int  min;                   // FS2 +0x14
    int  max;                   // FS2 +0x18
    int  wrap;                  // FS2 +0x1c: out of range wraps to the other end instead of clamping

    // Inline in the ROM (runCommands ops 3 and 0x18).
    void add(int delta)
    {
        value += delta;
        if (wrap == 0) {
            if (value < min)
                value = min;
            if (max < value)
                value = max;
        } else {
            if (value < min)
                value = max;
            if (max < value)
                value = min;
        }
    }
};

// UiLayout::testCondition's kinds (S2dCond::kind).
enum S2dCondKind
{
    S2dCond_Always = 0,
    S2dCond_KeyPressed = 1,     // b: UiKey bits pressed this frame
    S2dCond_AnimSetDone = 2,    // a: anim set; every waited-on actor of it in this sequence is done
    S2dCond_ActorDone = 3,      // a: actor id in this sequence; all its anims are done
    S2dCond_Var = 4,            // var a ? b
    S2dCond_SequenceRunning = 5, // a: sequence id
    S2dCond_GameState = 6,      // strings a, b: UiLayout::testGameState
    S2dCond_VarVar = 7,         // var a ? var b
    S2dCond_Arg = 8,            // the layout's arg (setData) ? b
    S2dCond_SeqValue = 9,       // sequence a's value (this one when a < 0) ? b
    S2dCond_SeqValueVar = 10,   // sequence a's value ? var b
    S2dCond_SubLayout = 11,     // b == 0: no nested layout; else one is loading
    S2dCond_SubResult = 12,     // the last nested layout's result ? (a < 0 ? b : var a)
    S2dCond_GameValue = 13,     // UiLayout::testGameValue(a, b)
    S2dCond_KeyRepeat = 14,     // as KeyPressed on the auto-repeating buttons
    S2dCond_KeyHeld = 15,       // as KeyPressed on the held buttons
    S2dCond_SeqAnimSetDone = 16, // as AnimSetDone (set a) in sequence b
};

// S2dCond::flags.
enum S2dCondFlag
{
    S2dCondFlag_AllKeys = 0x1,  // key kinds: every key of b, not any
    S2dCondFlag_Negate = 0x2,
    S2dCondFlag_OpMask = 0xf00, // UiLayout::compare's operator
};

// UiLayout::compare's operators (S2dCond::flags & S2dCondFlag_OpMask): a ? b.
enum S2dCompareOp
{
    S2dCompare_Equal = 0x000,
    S2dCompare_Greater = 0x100,
    S2dCompare_GreaterEqual = 0x200,
    S2dCompare_Less = 0x300,
    S2dCompare_LessEqual = 0x400,
    S2dCompare_AnyBit = 0x500, // a & b != 0
};

// The key bits of the key condition kinds: the pad's own PadBit bits, plus
// these the layout adds.
enum UiKey
{
    UiKey_RepeatUp = 0x02000000,    // the d-pad, always from the auto-repeat word
    UiKey_RepeatRight = 0x04000000,
    UiKey_RepeatDown = 0x08000000,
    UiKey_RepeatLeft = 0x10000000,
    UiKey_Decide = 0x20000000,      // ButtonMap::defaultButton(0)'s button
    UiKey_Cancel = 0x40000000,      // ButtonMap::defaultButton(1)'s button
    UiKey_Any = (int)0x80000000,    // in the condition: any key at all
};

// A command's condition (0x10 bytes).
struct S2dCond
{
    int   kind;                 // FS2 +0x0: S2dCondKind
    u_int flags;                // FS2 +0x4: S2dCondFlag
    int   a;                    // FS2 +0x8
    int   b;                    // FS2 +0xc
};
static_assert(sizeof(S2dCond) == 0x10, "S2dCond: EE 0x10");
static_assert(offsetof(S2dCond, a) == 0x8, "S2dCond::a");

// S2dCmd::trigger: which runCommands pass executes the command.
enum S2dTrigger
{
    S2dTrigger_Frame = 0,       // every update (any value but 1 and 2)
    S2dTrigger_Start = 1,       // once, when the sequence starts
    S2dTrigger_Stop = 2,        // once, when it is stopped
};

// UiLayout::runCommands' opcodes (S2dCmd::op). Args are arg0 / arg1; "lo" /
// "hi" are their 16-bit halves (signed), "var n" S2dData::vars[n].value.
enum S2dOp
{
    S2dOp_AddAnimSet = 1,       // anim set arg0 (+ var named by string arg1 if arg1) into this sequence
    S2dOp_SetVar = 2,           // var arg0 = arg1
    S2dOp_AddVar = 3,           // var arg0 += arg1, clamped or wrapped
    S2dOp_StopSelf = 4,         // stop this sequence; ends the pass
    S2dOp_StopSequence = 5,     // sequence arg0
    S2dOp_StartSequence = 6,    // sequence arg0 with value arg1
    S2dOp_Yield = 7,            // end the pass; unconditional: resume after this command
    S2dOp_Restart = 8,          // end the pass; resume at the first command
    S2dOp_CreateWidget = 9,     // widget def arg0, arg1
    S2dOp_RemoveWidget = 10,    // widget def arg0's widget
    S2dOp_CopyVar = 11,         // var arg0 = var arg1
    S2dOp_RemoveAnimSet = 12,   // anim set arg0 (+ var named by arg1)'s actors from this sequence
    S2dOp_AddAnimSetTo = 13,    // anim set arg0 into sequence arg1
    S2dOp_RemoveAnimSetFrom = 14, // anim set arg0's actors from sequence arg1
    S2dOp_End = 15,             // end the layout: result arg1, or var arg0 when arg0 >= 0
    S2dOp_SetSeqValue = 16,     // sequence arg0's (this one when < 0) value = arg1
    S2dOp_SetSeqValueVar = 17,  // sequence arg0's value = var arg1
    S2dOp_Jump = 18,            // skip arg1 more commands (negative: back)
    S2dOp_PlaySound = 19,       // slot arg1, the sound named by string arg0
    S2dOp_StopSound = 20,       // slot arg1
    S2dOp_OpenLayout = 21,      // the nested layout: file string arg0 under the base dir, arg arg1
    S2dOp_WriteGameValue = 22,  // UiLayout::writeGameValue(arg0, arg1)
    S2dOp_ReadGameValue = 23,   // UiLayout::readGameValue(arg0, arg1)
    S2dOp_AddVarVar = 24,       // var arg0 += var arg1, clamped or wrapped
    S2dOp_SetFrame = 25,        // anim set lo arg0 (+ var hi arg1) frame lo arg1
    S2dOp_SetFrameVar = 26,     // anim set lo arg0 (+ var hi arg1) frame var lo arg1
    S2dOp_SetPlay = 27,         // anim set lo arg0 (+ var hi arg1) play flags lo arg1
    S2dOp_SetFrameIn = 28,      // as SetFrame in sequence hi arg0
    S2dOp_SetFrameVarIn = 29,   // as SetFrameVar in sequence hi arg0
    S2dOp_SetPlayIn = 30,       // as SetPlay in sequence hi arg0
};

// A script command (0x24 bytes).
struct S2dCmd
{
    int     trigger;            // FS2 +0x00: S2dTrigger
    int     op;                 // FS2 +0x04: S2dOp
    int     skip;               // FS2 +0x08: commands to skip besides this one when it does not run
    S2dCond cond;               // FS2 +0x0c
    int     arg0;               // FS2 +0x1c
    int     arg1;               // FS2 +0x20
};
static_assert(sizeof(S2dCmd) == 0x24, "S2dCmd: EE 0x24");
static_assert(offsetof(S2dCmd, cond) == 0x0c, "S2dCmd::cond");
static_assert(offsetof(S2dCmd, arg0) == 0x1c, "S2dCmd::arg0");

// A sequence's definition (S2dData::seqs[id]).
struct S2dSeqDef
{
    int    priority;            // FS2 +0x0: draw and update order, lowest first
    int    cmdCount;            // FS2 +0x4
    int    _08;
    S2dCmd cmds[1];             // FS2 +0xc: cmdCount
};
static_assert(offsetof(S2dSeqDef, cmds) == 0xc, "S2dSeqDef::cmds");

// An anim set (S2dData::animSets[i]): entries of 0x10 bytes.
struct S2dAnimSetEntry
{
    int   actorId;              // FS2 +0x0
    int   _04;
    int   anim;                 // FS2 +0x8: -1: the entry only names the actor (removal)
    u_int flags;                // FS2 +0xc: S2dAnimSetEntry_NoWait
};
static_assert(sizeof(S2dAnimSetEntry) == 0x10, "S2dAnimSetEntry: EE 0x10");
static_assert(offsetof(S2dAnimSetEntry, anim) == 0x8, "S2dAnimSetEntry::anim");
enum
{
    S2dAnimSetEntry_NoWait = 0x80, // AnimSetDone does not wait for this entry
};
struct S2dAnimSet
{
    u_int           count;      // FS2 +0x0
    int             _04;
    S2dAnimSetEntry entries[1]; // FS2 +0x8: count
};
static_assert(offsetof(S2dAnimSet, entries) == 0x8, "S2dAnimSet::entries");

// A widget definition (S2dData::widgetDefs[i], 0x30 bytes).
struct S2dWidgetDef
{
    int    _00[2];
    int    id;                  // FS2 +0x08: the widget's id in its sequence
    u_char _0c[0x24];
};
static_assert(sizeof(S2dWidgetDef) == 0x30, "S2dWidgetDef: EE 0x30");
static_assert(offsetof(S2dWidgetDef, id) == 0x8, "S2dWidgetDef::id");

// layout: host -- see above; EE header 0x48.
struct S2dData
{
    char          magic[4];     // FS2 +0x00: "S2D\0"
    int           _04;
    int           relocated;    // FS2 +0x08
    int           startSeq;     // FS2 +0x0c: started by setData; -1 none
    int           varCount;     // FS2 +0x10
    S2dVar       *vars;         // FS2 +0x14
    int           objectCount;  // FS2 +0x18
    void        **objects;      // FS2 +0x1c
    int           _20[3];
    S2dAnimSet  **animSets;     // FS2 +0x2c
    int           _30;
    S2dWidgetDef *widgetDefs;   // FS2 +0x34
    int           _38;
    S2dSeqDef   **seqs;         // FS2 +0x3c
    int           stringCount;  // FS2 +0x40
    const char  **strings;      // FS2 +0x44

    bool        relocate();                                              // FS2 0x0024d820
    const char *getString(int index);                                    // FS2 0x0024df60
    S2dVar     *findVar(const char *name);                               // FS2 0x0024d770
    S2dVar     *getVar(int index);                                       // FS2 0x0024d810
};

// ──────────────────────────────────────────────────────────────────────
// The sequence side (0x246230..; minimal stand-ins).

// layout: host -- EE: +0 the kind record (+0 kind, +4 length), +4 play
// state, +8 frame, +0xc loops.
class UiTrack
{
public:
    bool isDone();                                                       // FS2 0x0024d2c0
    void setPlay(u_int flags);                                           // FS2 0x0024d360
};

// layout: host -- EE: +8 track count, +0xc tracks.
class UiAnim
{
public:
    void setFrame(int frame);                                            // FS2 0x002491c0

    u_char    _00[8];
    u_int     m_trackCount;     // FS2 +0x08
    UiTrack **m_tracks;         // FS2 +0x0c
};

// An animated object of a sequence (0x18 bytes).
// layout: host
class UiActor
{
public:
    void setAnimFrame(int frame, int which);                             // FS2 0x002480a0
    void setAnimPlay(u_int flags, int which);                            // FS2 0x00248410

    // Every track of every anim done. Inline in the ROM
    // (UiLayout::testCondition).
    bool isDone()
    {
        for (u_int i = 0; i < m_animCount; i++) {
            UiAnim *anim = m_anims[i];
            for (u_int j = 0; j < anim->m_trackCount; j++) {
                if (!anim->m_tracks[j]->isDone())
                    return false;
            }
        }
        return true;
    }

    void    *m_data;            // FS2 +0x00
    int      _04;
    int      m_id;              // FS2 +0x08
    u_int    m_animCount;       // FS2 +0x0c
    UiAnim **m_anims;           // FS2 +0x10
};

// A running sequence (0x70 bytes).
// layout: host
class UiSequence
{
public:
    UiSequence(S2dData *data, UiLayout *layout, int value);              // FS2 0x00247d60
    // Inline in the ROM (UiLayout::removeSequence).
    ~UiSequence()
    {
        removeActor(-1);
        removeWidget(-1);
    }

    void advance();                                                      // FS2 0x00246db0
    void draw(class UiDrawer *drawer);                                   // FS2 0x00246400
    void addAnimSet(S2dAnimSet *set);                                    // FS2 0x00247330
    int  removeActor(int id);                                            // FS2 0x002479b0: -1 all
    int  removeWidget(int id);                                           // FS2 0x002476c0: -1 all
    void setAnimSetFrame(S2dAnimSet *set, int frame);                    // FS2 0x002471d0
    void setAnimSetPlay(S2dAnimSet *set, u_int flags);                   // FS2 0x00247070

    // The actor with that id, or null. Inline in the ROM (it searches the
    // whole capacity, skipping empty entries).
    UiActor *findActor(int id)
    {
        if (m_actors == nullptr || id < 0)
            return nullptr;
        for (int i = 0; i < m_actorCapacity; i++) {
            if (m_actors[i] != nullptr && m_actors[i]->m_id == id)
                return m_actors[i];
        }
        return nullptr;
    }

    // Every actor of the set this sequence waits on (entries without
    // S2dAnimSetEntry_NoWait) is here and done. Inline in the ROM
    // (UiLayout::testCondition, twice).
    bool isAnimSetDone(const S2dAnimSet *set)
    {
        for (u_int i = 0; i < set->count; i++) {
            const S2dAnimSetEntry &entry = set->entries[i];
            if (entry.flags & S2dAnimSetEntry_NoWait)
                continue;
            UiActor *actor = findActor(entry.actorId);
            if (actor == nullptr || !actor->isDone())
                return false;
        }
        return true;
    }

    // Moves the cursor n commands on: the command there, or null past the
    // end (or when there is no cursor). Inline in the ROM.
    S2dCmd *step(int n)
    {
        if (m_cursor == nullptr)
            return nullptr;
        m_cursor += n;
        return m_cursor < m_cmdEnd ? m_cursor : nullptr;
    }

    S2dSeqDef *m_def;           // FS2 +0x00
    int        m_id;            // FS2 +0x04: its index in S2dData::seqs
    int        m_priority;      // FS2 +0x08: m_def->priority
    S2dCmd    *m_cmdBegin;      // FS2 +0x0c
    S2dCmd    *m_cmdEnd;        // FS2 +0x10
    S2dCmd    *m_cursor;        // FS2 +0x14: where the next frame pass starts
    S2dData   *m_data;          // FS2 +0x18
    int        m_actorCapacity; // FS2 +0x1c
    int        m_actorCount;    // FS2 +0x20
    UiActor  **m_actors;        // FS2 +0x24
    int        m_widgetCapacity; // FS2 +0x28
    int        m_widgetCount;   // FS2 +0x2c
    void     **m_widgets;       // FS2 +0x30
    int        m_pending;       // FS2 +0x34: UiLayout::update's not-yet-run mark
    u_char     _38[0x30];
    UiLayout  *m_layout;        // FS2 +0x68
    int        m_value;         // FS2 +0x6c: the start value; script-readable
};

// A sequence's widget (the Vt_00316a90.. classes, vptr at +0x1c).
// layout: host -- no fields declared yet.
class UiWidget
{
public:
    static void create(UiSequence *seq, S2dWidgetDef *def, UiLayout *layout, int arg); // FS2 0x0025e260
};

// The 0xe0-byte draw helper of a layout (UiLayout::m_drawer).
// layout: host
class UiDrawer
{
public:
    UiDrawer();                                                          // FS2 0x0024fb30
    ~UiDrawer();                                                         // FS2 0x0024fae0: deleting (obj, 1)

    void begin(sceVif1Packet *packet, void *texList);                    // FS2 0x0024fad0
    int  setMode(int mode);                                              // FS2 0x0024e990: answers the old one
    void end();                                                          // FS2 0x0024f9e0
};

// ──────────────────────────────────────────────────────────────────────
// The layout.

// Four floats (UiLayout's colour and offset).
struct UiVec4
{
    float x, y, z, w;

    void set(float x_, float y_, float z_, float w_);                    // FS2 0x00243890
};
static_assert(sizeof(UiVec4) == 0x10, "UiVec4: EE 0x10");

// UiSubLayout::m_state.
enum SubLayoutState
{
    SubLayout_None = 0,
    SubLayout_Loading = 1,      // the FileJob runs; advance opens its data
    SubLayout_Open = 2,         // the child runs and draws
    SubLayout_Closing = 3,      // closed on the next advance (nothing here sets it)
};

// The nested layout a layout can hold: UiLayout's base (+0x00..+0x1c).
// layout: host -- EE 0x1c.
class UiSubLayout
{
public:
    // Inline in the ROM (UiLayout::UiLayout).
    UiSubLayout()
        : m_state(SubLayout_None), m_lastResult(-1), m_child(nullptr), m_job(nullptr),
          m_arg(0), m_childDir(nullptr), m_18(0)
    {
    }
    ~UiSubLayout();                                                      // FS2 0x002438b0: (obj, flag)

    void load(const char *dir, const char *path, int arg);               // FS2 0x00243390
    bool open(void *data);                                               // FS2 0x00243630
    void close();                                                        // FS2 0x00243770
    void advance();                                                      // FS2 0x00243520
    void update();                                                       // FS2 0x002434c0

    int         m_state;        // FS2 +0x00: SubLayoutState
    int         m_lastResult;   // FS2 +0x04: the last child's result
    UiLayout   *m_child;        // FS2 +0x08
    FileJob    *m_job;          // FS2 +0x0c: while loading
    int         m_arg;          // FS2 +0x10: the child's setData arg
    const char *m_childDir;     // FS2 +0x14: the child's base dir (nothing here sets it)
    int         m_18;           // FS2 +0x18: only cleared
};

// The sound slots a layout's scripts play into.
constexpr int kUiLayoutSoundSlots = 4;

// A 2D layout playing an S2D file. The overlay layouts (game/screen.cpp)
// and the modal runner build them on pack items.
// layout: host -- EE 0x98.
class UiLayout : public UiSubLayout
{
public:
    UiLayout();                                                          // FS2 0x00246160
    ~UiLayout();                                                         // FS2 0x00246010: deleting (obj, 1)

    bool setData(void *data, int arg);                                   // FS2 0x00245e80
    void advance();                                                      // FS2 0x00243b90
    void update();                                                       // FS2 0x00243c10
    int  draw(sceVif1Packet *packet, void *texList);                     // FS2 0x002439f0: answers the sequence count
    // Replaces the new[] copy at +0x60 (null clears it); runArchiveScreen
    // passes "2dEU/".
    void setBaseDir(const char *dir);                                    // FS2 0x00245f10
    void clear();                                                        // FS2 0x00245f80

    int         startSequence(int id, int value);                        // FS2 0x00244d90
    int         stopSequence(int id);                                    // FS2 0x00243dc0
    int         removeSequence(int id);                                  // FS2 0x00245c80: -1 all
    UiSequence *findSequence(int id);                                    // FS2 0x00245e00
    int         runCommands(UiSequence *seq, int trigger);               // FS2 0x00243f40
    bool        testCondition(UiSequence *seq, const S2dCond *cond);     // FS2 0x00245060
    bool        compare(int a, int b, const S2dCond *cond);              // FS2 0x00245c00

    // The game hooks (0x25ff40.., not cut yet).
    void playSound(int slot, const char *name);                          // FS2 0x0025ff40
    void stopSound(int slot);                                            // FS2 0x00260100: -1 does nothing
    void readGameValue(int kind, u_int packed);                          // FS2 0x002601f0
    void writeGameValue(int kind, u_int packed);                         // FS2 0x002602a0
    bool testGameValue(int kind, u_int packed);                          // FS2 0x002603d0
    bool testGameState(const char *name, const char *arg, UiSequence *seq); // FS2 0x002604a0

    S2dData     *m_data;        // FS2 +0x1c
    UiDrawer    *m_drawer;      // FS2 +0x20
    int          m_seqCapacity; // FS2 +0x24
    int          m_seqCount;    // FS2 +0x28
    UiSequence **m_seqs;        // FS2 +0x2c: gGlobalHeap, grown by 16
    u_char       m_drawMode;    // FS2 +0x30: UiDrawer::setMode's mode (0 / 1)
    int          m_arg;         // FS2 +0x34: setData's arg
    int          m_result;      // FS2 +0x38: the answer, once m_done
    u_char       m_done;        // FS2 +0x3c: the layout has ended
    UiVec4       m_color;       // FS2 +0x40
    UiVec4       m_offset;      // FS2 +0x50
    char        *m_baseDir;     // FS2 +0x60: new[]
    const char  *m_soundName[kUiLayoutSoundSlots];   // FS2 +0x64
    long long    m_soundHandle[kUiLayoutSoundSlots]; // FS2 +0x78: -1 none

private:
    // Priority order, lowest first (an exchange sort). Inline in the ROM
    // everywhere the array changes.
    void sortSequences()
    {
        if (m_seqs == nullptr)
            return;
        for (int i = 0; i < m_seqCount - 1; i++) {
            for (int j = i + 1; j < m_seqCount; j++) {
                UiSequence *a = m_seqs[i];
                UiSequence *b = m_seqs[j];
                if (b->m_priority < a->m_priority) {
                    m_seqs[i] = b;
                    m_seqs[j] = a;
                }
            }
        }
    }
};

// In a stage (gStageScene set).
bool  inStage();                                                         // FS2 0x00262e60
// The stage pack's data by name (null outside a stage).
void *getStagePackData(const char *name);                                // FS2 0x00262df0

#endif /* _GAME_UILAYOUT_H */
