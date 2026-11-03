/* ==========================================================================
 *  game/s2ddata.h
 *
 *  The S2D file a UiLayout plays (the .d2b files under 2dEU/ and the pack
 *  items, magic "S2D\0"): variables, objects (an anim's tracks), actors
 *  (lists of objects), anim sets, widget definitions, sequences with their
 *  script commands, and strings. Its code (relocate, the lookups) sits at
 *  0x24d640..0x24df90 in the rest of unassigned@00246230; what game/
 *  uilayout.cpp and game/uisequence.cpp need of it is declared here until
 *  that TU is cut. The names are the port's (no source string).
 *
 *  Offsets become pointers in place on the first S2dData::relocate.
 *  layout: host -- EE offsets in the comments; the pointer fields are
 *  32-bit offsets in the file and pointers after relocate (the S2D TU must
 *  widen them on the host: side tables, or offsets resolved on access).
 * ======================================================================== */
#ifndef _GAME_S2DDATA_H
#define _GAME_S2DDATA_H

#include <stddef.h>

#include "sdk/scetypes.h"

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
static_assert(sizeof(S2dVar) == 0x20, "S2dVar: EE 0x20");
static_assert(offsetof(S2dVar, value) == 0x10, "S2dVar::value");

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

// UiTrack::setPlay's flags (also an anim set entry's flags, which
// UiActor::startAnim hands on).
enum UiPlayFlag
{
    UiPlay_Forward = 0x01,      // play state 1
    UiPlay_Backward = 0x02,     // play state 2
    UiPlay_Pause = 0x04,        // play state 3
    UiPlay_Rewind = 0x08,       // frame 0
    UiPlay_ToEnd = 0x10,        // the last frame
    UiPlay_Stop = 0x80,         // play state 0; overrides the rest. An anim set entry with it
                                // and anim -1 removes the actor, and AnimSetDone does not wait on it
};

// An anim set (S2dData::animSets[i]): entries of 0x10 bytes, grouped by
// actor.
struct S2dAnimSetEntry
{
    int   actorId;              // FS2 +0x0: S2dData::actorDefs index, the actor's id in its sequence
    int   priority;             // FS2 +0x4: the actor's draw order in its sequence, lowest first
    int   anim;                 // FS2 +0x8: an object index, or a UiAnimSelect; -1 with
                                // UiPlay_Stop: the entry removes the actor
    u_int flags;                // FS2 +0xc: UiPlayFlag
};
static_assert(sizeof(S2dAnimSetEntry) == 0x10, "S2dAnimSetEntry: EE 0x10");
static_assert(offsetof(S2dAnimSetEntry, anim) == 0x8, "S2dAnimSetEntry::anim");
struct S2dAnimSet
{
    u_int           count;      // FS2 +0x0
    int             _04;
    S2dAnimSetEntry entries[1]; // FS2 +0x8: count
};
static_assert(offsetof(S2dAnimSet, entries) == 0x8, "S2dAnimSet::entries");

// A track record (S2dObject::tracks[i]): what a UiTrack plays. Its kind
// picks the track class (the factory UiAnim::init 0x249550); the rest of
// the record is the kind's.
struct S2dTrackRec
{
    int kind;                   // FS2 +0x0: UiTrackKind
    int length;                 // FS2 +0x4: the keyed kinds' frame count
};
static_assert(offsetof(S2dTrackRec, length) == 0x4, "S2dTrackRec::length");

// An object (S2dData::objects[i]): a named list of tracks, played as one
// UiAnim.
// layout: host -- EE: +0 count, +4 name, +8 the track records (offsets).
struct S2dObject
{
    u_int        trackCount;    // FS2 +0x0
    const char  *name;          // FS2 +0x4
    S2dTrackRec *tracks[1];     // FS2 +0x8: trackCount
};

// An actor definition (S2dData::actorDefs[i]): the objects an actor plays.
struct S2dActorAnim
{
    int object;                 // FS2 +0x0: S2dData::objects index
    int loop;                   // FS2 +0x4: non-zero: the tracks wrap (UiTrack::m_loop)
};
static_assert(sizeof(S2dActorAnim) == 0x8, "S2dActorAnim: EE 0x8");
struct S2dActorDef
{
    u_int        animCount;     // FS2 +0x0
    int          _04;
    S2dActorAnim anims[1];      // FS2 +0x8: animCount
};
static_assert(offsetof(S2dActorDef, anims) == 0x8, "S2dActorDef::anims");

// A widget definition (S2dData::widgetDefs[i], EE 0x30 bytes).
// layout: host -- EE: +0 name, +4 type name (offsets in the file).
struct S2dWidgetDef
{
    const char *name;           // FS2 +0x00: UiSequence::findWidgetByName
    const char *type;           // FS2 +0x04: UiWidget::create looks it up in its class table (0x2facf0)
    int         id;             // FS2 +0x08: the widget's id in its sequence
    int         priority;       // FS2 +0x0c: draw order among the sequence's actors, lowest first
    u_char      _10[0x20];      // FS2 +0x10: the type's parameters (floats)
};

// An image of the S2D (S2dData::images[i], EE 8 bytes).
// layout: host -- EE: +0 the image (a pointer), +4 id.
struct S2dImage
{
    const void *data;           // FS2 +0x0: the image record (GsTexture's input, or a TimM sheet)
    int         id;             // FS2 +0x4: kept by the textured tracks (UiTexSpriteTrack::m_imageId)
};

// layout: host -- see above; EE header 0x50 (the images at +0x48 / +0x4c).
struct S2dData
{
    char          magic[4];     // FS2 +0x00: "S2D\0"
    int           _04;
    int           relocated;    // FS2 +0x08
    int           startSeq;     // FS2 +0x0c: started by setData; -1 none
    int           varCount;     // FS2 +0x10
    S2dVar       *vars;         // FS2 +0x14
    int           objectCount;  // FS2 +0x18
    S2dObject   **objects;      // FS2 +0x1c
    int           _20;
    S2dActorDef **actorDefs;    // FS2 +0x24
    int           _28;
    S2dAnimSet  **animSets;     // FS2 +0x2c
    int           _30;
    S2dWidgetDef *widgetDefs;   // FS2 +0x34
    int           _38;
    S2dSeqDef   **seqs;         // FS2 +0x3c
    int           stringCount;  // FS2 +0x40
    const char  **strings;      // FS2 +0x44
    int           imageCount;   // FS2 +0x48
    S2dImage     *images;       // FS2 +0x4c

    bool        relocate();                                              // FS2 0x0024d820
    const char *getString(int index);                                    // FS2 0x0024df60
    S2dVar     *findVar(const char *name);                               // FS2 0x0024d770
    S2dVar     *getVar(int index);                                       // FS2 0x0024d810
    int         findObject(const char *name);                            // FS2 0x0024d640: -1 none
    // Past the end (no lower bound check, as the ROM): null.
    const void *getImage(int index);                                     // FS2 0x0024d530
    S2dImage   *getImageEntry(int index);                                // FS2 0x0024d570
};

#endif /* _GAME_S2DDATA_H */
