// game/scriptrunner.h -- the stage scripts: the file ("SSS"), the runner and the saved frame.
//
// A script file is a header, 0x1c-byte entries, each entry's code and an SSS block with the string and
// label tables. The file is 32-bit on the disc and the ROM turns its offsets into pointers in place
// (StageScript::relocate); on the host those fields stay offsets (from the field's owner), and the
// accessors here add them. architecture.md "The stage script VM".
#pragma once

#include <stddef.h>

#include "sdk/scetypes.h"

class StageObject;
class StageCharacter;
class StageDoor;

enum { kScriptStackSize = 16 };

// The three names every label table starts with (0 the player, 1 the
// partner, 2 the running object): readNameArg / readObjectArg answer them
// from the scene, not from the table.
enum ScriptLabelIndex
{
    ScriptLabel_Player  = 0,
    ScriptLabel_Partner = 1,
    ScriptLabel_Self    = 2,
    ScriptLabel_First   = 3,    // the first real name (scriptPrepare)
};

// The VM's words. Anything from 0x36 up is a call: its top byte is one of
// ScriptCall.
enum ScriptOp
{
    ScriptOp_Restart      = 0x00,   // the entry's end: from the top next time
    ScriptOp_Exit         = 0x01,
    ScriptOp_SetWait      = 0x02,   // acc 30ths of a second from now, and yields
    ScriptOp_Wait         = 0x03,
    ScriptOp_JumpIfZero   = 0x04,   // word: distance from the word itself
    ScriptOp_Jump         = 0x05,
    ScriptOp_TargetName   = 0x06,   // word: label
    ScriptOp_LoadVar      = 0x07,   // word: ScriptVarBits
    ScriptOp_LoadConst    = 0x08,
    ScriptOp_StoreVar     = 0x09,
    ScriptOp_Push         = 0x0a,
    ScriptOp_PopApply     = 0x0b,
    ScriptOp_PrefixAdd    = 0x0c,   // ..0x12: the next load combines with acc (ScriptMath)
    ScriptOp_PrefixSub    = 0x0d,
    ScriptOp_PrefixMul    = 0x0e,
    ScriptOp_PrefixDiv    = 0x0f,
    ScriptOp_PrefixAnd    = 0x10,
    ScriptOp_PrefixOr     = 0x11,
    ScriptOp_PrefixXor    = 0x12,
    ScriptOp_PopEqual     = 0x13,   // ..0x18: pop, compare with acc
    ScriptOp_PopLess      = 0x14,
    ScriptOp_PopGreater   = 0x15,
    ScriptOp_PopLessEq    = 0x16,
    ScriptOp_PopGreaterEq = 0x17,
    ScriptOp_PopNotEqual  = 0x18,
    ScriptOp_LoadNamed    = 0x19,   // word: name of a local of the target or its parents
    ScriptOp_StoreNamed   = 0x1a,
    ScriptOp_LoadParent   = 0x1b,   // the parent's locals
    ScriptOp_StoreParent  = 0x1c,
    ScriptOp_TargetIndex  = 0x1d,   // acc: 1.. in the mission's character list
    ScriptOp_TargetHandle = 0x1e,   // acc: an object's handle
    ScriptOp_Skip         = 0x1f,   // one word, unread
    ScriptOp_LoadGrand    = 0x20,   // the parent's parent's locals
    ScriptOp_StoreGrand   = 0x21,
    ScriptOp_TargetRaw    = 0x22,   // word: an EE pointer
    ScriptOp_TargetPlayer = 0x23,
    ScriptOp_TargetPartner = 0x24,
    ScriptOp_TargetSelf   = 0x25,
    ScriptOp_LoadLocal    = 0x26,
    ScriptOp_Nop27        = 0x27,
    ScriptOp_LoadProperty = 0x28,
    ScriptOp_Nop29        = 0x29,
    ScriptOp_LoadPropertySub = 0x2a,
    ScriptOp_Nop2b        = 0x2b,
    ScriptOp_LoadGlobal   = 0x2c,
    ScriptOp_Nop2d        = 0x2d,
    ScriptOp_ConstEqual   = 0x30,   // ..0x35: acc against the word
    ScriptOp_ConstLess    = 0x31,
    ScriptOp_ConstGreater = 0x32,
    ScriptOp_ConstLessEq  = 0x33,
    ScriptOp_ConstGreaterEq = 0x34,
    ScriptOp_ConstNotEqual = 0x35,
    ScriptOp_Count        = 0x36,
};
constexpr u_int kScriptCallMask = 0xff000000;
constexpr u_int kScriptCallId   = 0x00ffffff;
constexpr u_int kScriptCallNone    = 0x00000000;    // a word below 0x1000000 that no case takes
constexpr u_int kScriptCallNative  = 0x01000000;    // the table 0x304230
constexpr u_int kScriptCallCommand = 0x02000000;    // ScriptRunner::runCommand
constexpr u_int kScriptCallLine    = 0xff000000;    // nothing

// How ScriptOp_LoadVar / StoreVar read their word.
enum ScriptVarBits
{
    ScriptVar_Property    = 0x8000,     // of the target; the id in the low 15 bits
    ScriptVar_PropertyId  = 0x7fff,
    ScriptVar_Flag        = 0x4000,     // FlagTable
    ScriptVar_Local       = 0x2000,
    ScriptVar_IndexMask   = 0x0fff,
    ScriptVar_FirstGlobal = 0x40,       // without a bit: a local below it, a global from it
    ScriptVar_SubShift    = 16,         // a property's second index, stores only
    ScriptVar_SubMask     = 0xff0000,
};
// What a ScriptOp_Prefix* leaves pending for the next load (scriptApply).
enum ScriptMath
{
    ScriptMath_Assign, ScriptMath_Add, ScriptMath_Sub, ScriptMath_Mul, ScriptMath_Div,
    ScriptMath_And, ScriptMath_Or, ScriptMath_Xor,
    ScriptMath_Count,
};

enum
{
    kScriptMagic      = 0x1a535353, // "SSS\x1a": the block, and the trailer of each entry's code
    kScriptMagicStr   = 0x1a525453, // "STR\x1a"
    kScriptMagicLabel = 0x1a4c424c, // "LBL\x1a"
    kScriptMagicSst   = 0x1a545353, // "SST\x1a": relocated, never read
    kScriptTrailerSize = 0xc,
    kScriptEntryNameSize = 0x10,
};

// A sub-block of the SSS block: tables of 32-bit offsets. ROM: relocate
// makes them pointers; PC: they stay offsets, label() / string() add them.
// layout: file
struct ScriptTable
{
    int     magic;              // FS2 +0x0: kScriptMagic*
    int     size;               // FS2 +0x4: low two bits unused
    int     tableOffset;        // FS2 +0x8
    u_short count;              // FS2 +0xc
    u_short relocated;          // FS2 +0xe
    int     table[1];           // FS2 +0x10

    // STR: from the block. LBL: from the block + 4; the word before a label
    // is its cell (scriptLabelCell).
    const char *string(int index) const { return (const char *)this + table[index]; }
    const char *label(int index) const { return (const char *)this + table[index] + 4; }
    const ScriptTable *next() const { return (const ScriptTable *)((const char *)this + (size & ~3)); }
};

static_assert(offsetof(ScriptTable, table) == 0x10, "ScriptTable: the file's");

static_assert(offsetof(ScriptTable, table) == 0x10, "ScriptTable: the file");

// The SSS block.
// layout: file
struct ScriptBlock
{
    int     magic;              // FS2 +0x0: kScriptMagic
    int     firstTable;         // FS2 +0x4: from the block
    u_short versionMajor;       // FS2 +0x8
    u_short versionMinor;       // FS2 +0xa: from 0.4 labels hold item texts
    u_short tableCount;         // FS2 +0xc

    const ScriptTable *firstOf() const { return (const ScriptTable *)((const char *)this + firstTable); }
    // The first table that is neither of the two skipped magics, when it is
    // wanted; the ROM has one copy per kind.
    const ScriptTable *findStrings() const;                              // FS2 0x001d3850
    const ScriptTable *findLabels() const;                               // FS2 0x001d38e0
};

static_assert(sizeof(ScriptBlock) == 0x10, "ScriptBlock: the file's");

static_assert(sizeof(ScriptBlock) == 0x10, "ScriptBlock: the file");

// An entry of the first table.
// layout: file -- 0x1c.
struct ScriptEntry
{
    char    name[kScriptEntryNameSize]; // FS2 +0x00
    int     codeOffset;         // FS2 +0x10: ROM a pointer; PC: from the entry
    int     slotNames;          // FS2 +0x14: ROM a pointer, 0x10 bytes each; PC: from the entry, 0 none
    u_short sizeBits;           // FS2 +0x18: the top two size bits are in the bottom two
    short   slotCount;          // FS2 +0x1a: its locals

    u_int codeSize() const { return (sizeBits & 0xfffc) | (u_int)(sizeBits & 3) << 16; }
    const int *code() const { return (const int *)((const char *)this + codeOffset); }
    // The word at the end of the code: ROM a pointer to the block; PC: from
    // the word. Null when the code has no room for a trailer.
    const ScriptBlock *block() const;                                    // FS2 0x001d3970
    // The same, checked: both trailer words and the block's magic.
    const ScriptBlock *checkedBlock() const;                             // FS2 0x0029fd20
};
static_assert(sizeof(ScriptEntry) == 0x1c, "ScriptEntry: the file's");

// A script file as loaded.
// layout: file
struct StageScript
{
    // PC: the two tables at +0 / +4 are 32-bit fields in the file: they keep
    // the offset, from this header.
    int   table;                // FS2 +0x0
    int   table2;               // FS2 +0x4
    short count;                // FS2 +0x8
    short slotCount;            // FS2 +0xa: StageScene::scriptSlots

    // The pointers of the file; the block, null when the file is bad.
    const ScriptBlock *relocate(bool withTable2);                        // FS2 0x001d3a40
    const void *entry(int index) const { return (const char *)this + table + index * sizeof(ScriptEntry); }
    // The entry of the first table whose name is name, null none (also on a
    // null script: the ROM tests this).
    const void *findEntry(const char *name);                             // FS2 0x001d3d20
};
static_assert(sizeof(StageScript) == 0xc, "StageScript: the file header");

// What a label's cell holds: the object of that name, or an item's text
// (scriptPrepare, ScriptRunner::readObjectArg). PC: the 4-byte cell keeps a
// handle into a host table.
void *scriptLabelCell(const char *label);
void  scriptSetLabelCell(const char *label, void *value);

class ScriptFrame;

// A script-run object: the map's name is the script's. Stage objects have
// one at +0x40 (so has a sub-scene, as its base).
//
// Vtable 0x315850, 108 slots: most are the script's commands, and this base
// answers each one with a default (0x315690 forwards them to a target).
// The slots with a host name here are the ones a native calls so far, in
// the ROM's order; the commandNN stand-ins wait for the natives that call
// them. CodeWarrior put a copy of the defaults beside DemoExecuter's vtable
// in game/stagescene.cpp.
// layout: host -- EE 0xb8, vptr +0xb4.
class ScriptRunner
{
    friend class ScriptFrame;
    friend class CharacterBrain;
    friend void scriptYield(int a, int b);

public:
    ScriptRunner();                                                      // FS2 0x001d3130
    virtual ~ScriptRunner();                                             // FS2 0x001d30c0: deleting

    // The entry it runs from the top, its locals cleared; null ends it.
    void setEntry(const void *entry);                                    // FS2 0x001d2f40
    // Until the script yields or ends: it is gRunningScript meanwhile.
    void run(int arg = 0);                                               // FS2 0x001d27e0

    bool isRunning() const { return m_entry != nullptr; }
    // What the last yield left for the native waiting here, cleared; 0 with
    // none. Inline in the ROM's waiting natives.
    int takeYield()
    {
        int value = m_yieldA != 0 ? m_yieldB : 0;
        m_yieldA = 0;
        m_yieldB = 0;
        return value;
    }
    // FS2 0x001d1a00
    const ScriptEntry *getEntry() const { return m_entry; }
    // FS2 0x001d19f0
    ScriptRunner *getParent() const { return m_parentScript; }
    void setScriptParent(ScriptRunner *parent) { m_parentScript = parent; }
    // FS2 0x001d19a0
    int *getLocals() const { return m_locals; }
    // FS2 0x001d1980
    int  getSlotCount() const { return m_entry == nullptr ? 0 : m_entry->slotCount; }
    const char *getSlotName(int index) const;                            // FS2 0x001d19b0
    ScriptRunner *getTarget() const { return m_target; }
    void setTarget(ScriptRunner *target) { m_target = target; }

    // Natives. The code words are table indices: each answers the next word.
    // A label as a name; the three first ones from the scene.
    const int *readNameArg(const char **out, const int *code);           // FS2 0x001d32e0
    // A label, and the three first ones refresh their cell with the object.
    const int *readObjectArg(const char **out, const int *code);         // FS2 0x001d3200
    const int *readLabelArg(const char **out, const int *code);          // FS2 0x001d33c0
    // A label's cell, or the label when the cell is empty.
    const int *readCellArg(void **out, const int *code);                 // FS2 0x001d3410
    const int *readStringArg(const char **out, const int *code);         // FS2 0x001d3470
    // count values off the stack, out[0] the deepest.
    void popArgs(int *out, int count);                                   // FS2 0x001d34c0
    void dropArgs(int count) { m_sp -= count; }
    // The stack pops count: the deepest of them. Natives read them in place.
    const int *popped(int count)
    {
        m_sp -= count;
        return &m_stack[m_sp];
    }

    // A local of this script or a parent, by name; the store does nothing
    // when the name is unknown.
    int  getNamed(const char *name);                                     // FS2 0x001d1a10
    void setNamed(const char *name, int value);                          // FS2 0x001d1750

    // Word 0x02000000 | id; answers the code after it. Each class its own.
    // FS2 0x001d1330
    virtual const int *runCommand(int id, const int *code) { (void)id; return code; }
    // A script's object properties; the base has the game's global ones.
    virtual int  getProperty(int id, int sub);                           // FS2 0x001cfc20
    virtual void setProperty(int id, int sub, int value);                // FS2 0x001cf200
    // The script ran its exit.
    // FS2 0x00171b30
    virtual void onScriptExit() {}

    // The command's default: logs "unsupported command" (Shift-JIS at
    // 0x304ab0; the log is empty in retail) and answers 0.
    int unsupportedCommand();                                            // FS2 0x001cf1b0
    // The default of most commands: 0.
    int defaultCommand();                                                // FS2 0x001cf1e0

    // Slot +0x24: the class's name; null here (0x1a7a10).
    virtual const char *getName() { return nullptr; }

    // FS2 0x0019cf50
    virtual int command10() { return 0; }
    // FS2 0x0019cf60
    virtual int command11() { return 0; }
    // FS2 0x0019cf70
    virtual int command12() { return 0; }
    // Slot +0x3c.
    // FS2 0x0019cf80
    virtual void scriptSetPosition(const float *) {}
    // Slot +0x40.
    // FS2 0x0019cf90
    virtual void scriptSetRotation(const float *) {}
    // Slot +0x44.
    // FS2 0x0019cfa0
    virtual void scriptSetYaw(const float *) {}

    // ROM vptr +0x98: the character it is, natives 0x79, 0xf6, 0x11b..;
    // null with the "unsupported" log here.
    // FS2 0x00171cc0
    // callcheck: unsupportedCommand
    virtual StageCharacter *getScriptCharacter()
    {
        unsupportedCommand();
        return nullptr;
    }
    // ROM vptr +0xa0: the door it is. The same.
    // FS2 0x00177160
    // callcheck: unsupportedCommand
    virtual StageDoor *getScriptDoor()
    {
        unsupportedCommand();
        return nullptr;
    }
    // ROM vptr +0xfc: other stands at it, looking at it. Native 0x44.
    // FS2 0x00177180
    // callcheck: defaultCommand
    virtual bool scriptIsFacing(StageObject *other)
    {
        (void)other;
        return defaultCommand() != 0;
    }
    // ROM vptr +0x184, +0x188, +0x18c. Natives 0x87, 0x88, 0x89.
    // FS2 0x00172120
    // callcheck: defaultCommand
    virtual bool scriptOpenDoor() { return defaultCommand() != 0; }
    // FS2 0x00172140
    // callcheck: defaultCommand
    virtual bool scriptCloseDoor() { return defaultCommand() != 0; }
    // FS2 0x00172160
    // callcheck: defaultCommand
    virtual bool scriptUseDoorKey(int lock) { (void)lock; return defaultCommand() != 0; }

    // Slots +0x48..+0x8c: the light's commands (game/stagelight.h).
    // FS2 0x00171b90
    virtual void cmdSetColor(const float *) {}
    // FS2 0x00171ba0
    virtual void cmdAddColorKey(int, const float *) {}
    // FS2 0x00171bb0
    virtual void cmdSetColorLoop(u_char) {}
    // FS2 0x00171bc0
    virtual void cmdSetRadiusSquared(float) {}
    // FS2 0x00171bd0
    virtual void cmdSetConeAngle(int) {}
    // FS2 0x00171be0
    virtual void cmdSetValue2d8(int) {}
    // FS2 0x00171c00
    virtual void cmdSetTreeSwitchedOn(bool) {}
    // Runner slot 86 (+0x160): StageObject::getProperty's Prop_SwitchedOn.
    // FS2 0x00172050
    virtual bool cmdIsLit() { return false; }
    // FS2 0x00171c20
    virtual void cmdSetTreeBroken(bool) {}
    // FS2 0x00171c30
    virtual void cmdSetFadeFrames(int) {}
    // FS2 0x00171c40
    virtual void cmdSetFlag240(bool) {}
    // FS2 0x00171c50
    virtual void cmdSetValue244(int) {}
    // FS2 0x00171c60
    virtual void cmdSetFlag248(bool) {}
    // FS2 0x00171c70
    virtual void cmdSetColor250(const float *) {}
    // FS2 0x00171c80
    virtual void cmdSetColor260(const float *) {}
    // FS2 0x00171c90
    virtual void cmdSetCastsShadow(bool) {}
    // FS2 0x00171ca0
    virtual void cmdSetEnabled(bool) {}

    // FS2 0x0019cfb0
    virtual void command34(void *) {}
    // Slot +0xa4: the object it is, null with the "unsupported" log here.
    // FS2 0x0019cfc0
    // callcheck: unsupportedCommand
    virtual StageObject *getScriptObject()
    {
        unsupportedCommand();
        return nullptr;
    }
    // FS2 0x0019cfe0
    // callcheck: defaultCommand
    virtual float command58(void *) { return (float)defaultCommand(); }
    // FS2 0x0019d010
    // callcheck: defaultCommand
    virtual float command59(void *) { return (float)defaultCommand(); }
    // Slot +0x140: an event's object plays the pack's motion called name; the
    // demo camera takes it as its cut. Native 0x148.
    virtual void scriptPlayMotion(const char *name) { (void)name; defaultCommand(); }
    // A tail jump to defaultCommand.
    // FS2 0x0019d050
    virtual int command80() { return defaultCommand(); }
    // ROM vptr +0x154: a line of the stage menu, offered by this object
    // (native 0x7a). A tail jump to defaultCommand.
    // FS2 0x0019d040
    virtual void scriptAddMenuLine(const u_short *, int, bool) { defaultCommand(); }
    // ROM vptr +0xa8: a script for one of the brain's slots (native 0x2f).
    // FS2 0x00171ce0
    virtual void scriptSetBrainEntry(int, const ScriptEntry *) { defaultCommand(); }
    // ROM vptr +0xb0: who was heard: 0 nobody, 1 an ally, 2 another, 3 an
    // enemy, 4 the player (native 0x31).
    // FS2 0x00171d00
    virtual int scriptHeardKind() { return defaultCommand(); }
    // ROM vptr +0x124: the character is down in one of its crouched motions.
    // FS2 0x00171f60
    virtual bool scriptIsCrouching() { return defaultCommand() != 0; }
    // A tail jump to defaultCommand.
    // FS2 0x0019d060
    virtual int command87() { return defaultCommand(); }
    // FS2 0x0019d070
    virtual void command88() {}
    // FS2 0x0019d080
    // callcheck: defaultCommand
    virtual bool command99() { return defaultCommand() != 0; }
    // A tail jump to defaultCommand.
    // FS2 0x0019d0a0
    virtual int command100() { return defaultCommand(); }
    // A tail jump to defaultCommand.
    // FS2 0x0019d0b0
    virtual int command101(void *) { return defaultCommand(); }

    u_char m_cutWaited = 0;     // FS2 +0xb8: natives 0x147, 0x159 and one more wait one frame, once ever
    u_char m_skippable = 0;     // FS2 +0xb9: a sub-scene's: start ends it

private:
    // The VM: words from the saved position until one yields or ends.
    int  execute();                                                      // FS2 0x001d1c40
    // The entry and what is looked up through it.
    void bindEntry(const ScriptEntry *entry);                            // FS2 0x001d2ea0
    const ScriptTable *getStrings();                                     // FS2 0x001d2830
    const ScriptTable *getLabels();                                      // FS2 0x001d2bc0
    const ScriptBlock *getBlock();                                       // FS2 0x001d2b10
    void setTargetByName(const char *name);                              // FS2 0x001d0a60
    void setTargetByHandle(int handle);                                  // FS2 0x001d09a0
    void setTargetByIndex(int index);                                    // FS2 0x001d09e0

    ScriptRunner       *m_parentScript;       // FS2 +0x00: whose tables and locals it shares; the brain's and its sub-script's
    int                 m_depth;        // FS2 +0x04: run's nesting
    const ScriptTable  *m_labels;       // FS2 +0x08
    const ScriptTable  *m_strings;      // FS2 +0x0c
    const ScriptBlock  *m_block;        // FS2 +0x10: ROM the trailer word's address
    int                *m_locals;       // FS2 +0x14: slotCount ints; sNoLocals with none
    int                 m_wait;         // FS2 +0x18: ScriptOp_Wait's frames
    int                 m_stack[kScriptStackSize];   // FS2 +0x1c
    int                 m_stackMath[kScriptStackSize]; // FS2 +0x5c: the pending ScriptMath of each push
    int                 m_sp;           // FS2 +0x9c
    const int          *m_pc;           // FS2 +0xa0: null from the top
    ScriptRunner       *m_target;       // FS2 +0xa4: of the next native, then itself again
    const ScriptEntry  *m_entry;        // FS2 +0xa8: null: no script
    int                 m_yieldA;       // FS2 +0xac: scriptYield's: what a waiting native keeps
    int                 m_yieldB;       // FS2 +0xb0
};
extern ScriptRunner *gRunningScript;    /* sbss 0x31ae18 (gp-0x7a58) */

// The running native waits: the VM stops, and runs the same word again next
// frame with the stack as it was (a and b stay on the runner).
void scriptYield(int a, int b);                                          // FS2 0x001d3710
// n of the game's frames in the 30ths the scripts count, and back.
int  scriptFramesTo30(int frames);                                       // FS2 0x001d3740
int  scriptFramesFrom30(int frames30);                                   // FS2 0x001d37c0
// Binds each name of the script's label table: an object, or for @ and _
// names a text.
void scriptPrepare(StageScript *script);                                 // FS2 0x0029f050

// A runner's state put aside, to run another entry and come back.
// layout: host -- EE 0xa8, vptr +0xa4; no field of it is read elsewhere.
class ScriptFrame
{
public:
    ScriptFrame();                                                       // FS2 0x001d1710
    virtual ~ScriptFrame();                                              // FS2 0x001d39b0: deleting

    // Clears it for entry, with fresh locals when the entry changes.
    void prepare(const ScriptEntry *entry);                              // FS2 0x001d1340
    void clear();                                                        // FS2 0x001d1630
    void release();                                                      // FS2 0x001d16b0
    // Back onto the runner, and off it.
    void restoreTo(ScriptRunner *runner) const;                          // FS2 0x001d1480
    void saveFrom(ScriptRunner *runner);                                 // FS2 0x001d1550

private:
    int                *m_locals;       // FS2 +0x00
    int                 m_wait;         // FS2 +0x04
    int                 m_stack[kScriptStackSize];     // FS2 +0x08
    int                 m_stackMath[kScriptStackSize]; // FS2 +0x48
    int                 m_sp;           // FS2 +0x88
    const int          *m_pc;           // FS2 +0x8c
    ScriptRunner       *m_target;       // FS2 +0x90
    const ScriptEntry  *m_entry;        // FS2 +0x94
    const ScriptBlock  *m_block;        // FS2 +0x98
    int                 m_yieldA;       // FS2 +0x9c
    int                 m_yieldB;       // FS2 +0xa0
};
