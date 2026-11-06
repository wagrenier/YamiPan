// game/scriptrunner.cpp -- the stage script VM and its natives.
//
// The VM is a word machine with one accumulator, a 16-value stack and three kinds of call: opcodes
// below 0x36, natives (0x01000000 | id, table 0x304230, ids 0x19..0x235) and the object's own commands
// (0x02000000 | id). A native takes the runner, the accumulator's address and the code position, pops
// its own arguments and answers the next code position. architecture.md "The stage script VM".
//
// The natives written so far are those stage 1's bg script calls: lighting and light setup. Any other
// still pops its count and skips its words (scriptnatives.inc, generated from the ROM), so a script
// runs straight through it and the [stub] log names it.
#include "game/scriptrunner.h"
#include "game/stagedoor.h"
#include "game/stageparticle.h"
#include "game/wstring.h"

#include <algorithm>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "common/yamipan_stub.h"
#include "game/characterbrain.h"
#include "game/characterroute.h"
#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/playermotion.h"
#include "game/scenerender.h"
#include "game/stagelight.h"
#include "game/stageobjlist.h"
#include "game/stageitem.h"
#include "game/stageresult.h"
#include "game/stagescene.h"
#include "game/subsceneobject.h"
#include "game/stagecar.h"
#include "game/sysdata.h"

ScriptRunner *gRunningScript;           /* sbss 0x31ae18 (gp-0x7a58) */
static u_char sScriptYield;             /* sbss 0x31ae14 (gp-0x7a5c) */
static int    sNoLocals;                /* sbss 0x31ae1c (gp-0x7a54): the locals of an entry with none */

// PC: what the 4-byte cell before a label refers to.
static std::vector<void *> sLabelCells;

static const char kScriptUnknownName[] = "(unknown)";   /* 0x304dd0 */
constexpr float kDegreesToRadians = 0.017453292f;
// The object kinds a script moves as a group of the same name (natives
// 0x26, 0x27): StageObject_Light .. and the unnamed 0x12.
constexpr int kLastLightKind = 0x12;
constexpr int kSlotNameSize = 0x10;


// ──────────────────────────────────────────────────────────────────────
// Label cells.

void *scriptLabelCell(const char *label)
{
    int handle;
    memcpy(&handle, label - 4, sizeof(handle));
    if (handle <= 0 || handle > (int)sLabelCells.size())
        return nullptr;
    return sLabelCells[handle - 1];
}

void scriptSetLabelCell(const char *label, void *value)
{
    int handle;
    memcpy(&handle, label - 4, sizeof(handle));
    if (handle <= 0 || handle > (int)sLabelCells.size()) {
        sLabelCells.push_back(value);
        handle = (int)sLabelCells.size();
        memcpy(const_cast<char *>(label) - 4, &handle, sizeof(handle));
    } else {
        sLabelCells[handle - 1] = value;
    }
}

// The player, or what acts for it.
static StageObject *scenePlayer()
{
    StageObject *object = gStageScene->controlled;
    return object != nullptr ? object : gStageScene->player;
}


// ──────────────────────────────────────────────────────────────────────
// The file.

// The trailer word's block.
// FS2 0x001d3970; the ROM answers the word's address, the callers read it.

const ScriptBlock *ScriptEntry::block() const
{
    u_int size = codeSize();
    if (size < kScriptTrailerSize)
        return nullptr;
    const int *trailer = (const int *)((const char *)code() + size - kScriptTrailerSize);
    return (const ScriptBlock *)((const char *)trailer + *trailer);
}

// The block of a good entry, null otherwise.
// FS2 0x0029fd20

const ScriptBlock *ScriptEntry::checkedBlock() const
{
    u_int size = codeSize();
    if (size < kScriptTrailerSize)
        return nullptr;
    const int *trailer = (const int *)((const char *)code() + size - kScriptTrailerSize);
    if (trailer[2] != kScriptMagic || trailer[1] != kScriptTrailerSize)
        return nullptr;
    const ScriptBlock *block = (const ScriptBlock *)((const char *)trailer + *trailer);
    if (*trailer == 0 || (*trailer & 3) != 0 || block->magic != kScriptMagic)
        return nullptr;
    return block;
}

// FS2 0x001d3850

const ScriptTable *ScriptBlock::findStrings() const
{
    const ScriptTable *table = firstOf();
    for (u_int i = 0; i < tableCount; i++) {
        if (table->magic != kScriptMagicSst && table->magic != kScriptMagicLabel)
            return table->magic == kScriptMagicStr ? table : nullptr;
        table = table->next();
    }
    return nullptr;
}

// FS2 0x001d38e0

const ScriptTable *ScriptBlock::findLabels() const
{
    const ScriptTable *table = firstOf();
    for (u_int i = 0; i < tableCount; i++) {
        if (table->magic != kScriptMagicStr && table->magic != kScriptMagicSst)
            return table->magic == kScriptMagicLabel ? table : nullptr;
        table = table->next();
    }
    return nullptr;
}

// Lays the file out: the entries, table 2, each entry's code with its slot
// names, the SSS block, whose address goes into each code's trailer.
// PC: every one of those is a 32-bit field, so it keeps an offset, and the
// tables of the block keep theirs (ROM: pointers). Safe to run twice.
// FS2 0x001d3a40

const ScriptBlock *StageScript::relocate(bool withTable2)
{
    table = sizeof(StageScript);
    int pos = table + count * (int)sizeof(ScriptEntry);
    if (withTable2) {
        table2 = pos;
        pos += slotCount * kSlotNameSize;
    } else {
        table2 = 0;
    }

    char *base = (char *)this;
    bool ok = count > 0;
    for (int i = 0; i < count; i++) {
        int entryPos = table + i * (int)sizeof(ScriptEntry);
        ScriptEntry *entry = (ScriptEntry *)(base + entryPos);
        entry->codeOffset = pos - entryPos;
        u_int size = entry->codeSize();
        pos += size;
        if (size <= kScriptTrailerSize) {
            ok = false;
        } else {
            const int *end = (const int *)(base + pos);
            ok = ok && end[-1] == kScriptMagic;
            ok = ok && end[-2] == kScriptTrailerSize;
        }
        if (withTable2) {
            entry->slotNames = pos - entryPos;
            pos += entry->slotCount * kSlotNameSize;
        } else {
            entry->slotNames = 0;
        }
    }
    if (!ok)
        return nullptr;

    ScriptBlock *block = (ScriptBlock *)(base + pos);
    if (block->magic != kScriptMagic)
        return nullptr;
    for (int i = 0; i < count; i++) {
        ScriptEntry *entry = (ScriptEntry *)(base + table + i * (int)sizeof(ScriptEntry));
        int *trailer = (int *)((char *)entry->code() + entry->codeSize() - kScriptTrailerSize);
        *trailer = (int)((char *)block - (char *)trailer);
    }
    ScriptTable *sub = const_cast<ScriptTable *>(block->firstOf());
    for (u_int i = 0; i < block->tableCount; i++) {
        ScriptTable *next = const_cast<ScriptTable *>(sub->next());
        if (sub->magic == kScriptMagicStr || sub->magic == kScriptMagicSst
            || sub->magic == kScriptMagicLabel)
            sub->relocated = 1;
        else
            next = sub;     // the ROM stays on an unknown block
        sub = next;
    }
    return block;
}

// FS2 0x001d3d20
// callcheck: strcmp

const void *StageScript::findEntry(const char *name)
{
    if (this == nullptr)
        return nullptr;
    for (int i = 0; i < count; i++) {
        const ScriptEntry *entry = (const ScriptEntry *)this->entry(i);
        if (strcmp(name, entry->name) == 0)
            return entry;
    }
    return nullptr;
}

// Each name of the entry's label table past the three fixed ones: names
// starting @ or _ are the mission's item texts (the name itself when it has
// none), others objects, null if absent.
// FS2 0x0029f050
// callcheck: checkedBlock, currentMission, getItemText, findAny

void scriptPrepare(StageScript *script)
{
    if (script == nullptr)
        return;
    const ScriptBlock *block = ((const ScriptEntry *)script->entry(0))->checkedBlock();
    if (block == nullptr)
        return;
    const ScriptTable *labels = block->findLabels();
    if (labels == nullptr)
        return;
    StageObjectList *objects = gStageScene->objects;
    for (int i = ScriptLabel_First; i < labels->count; i++) {
        const char *label = labels->label(i);
        if (label[0] == '@' || label[0] == '_') {
            const char *text = gStageScene->currentMission()->getItemText(label);
            scriptSetLabelCell(label, text != nullptr ? (void *)text : (void *)label);
        } else {
            scriptSetLabelCell(label, objects->findAny(label, 0));
        }
    }
}


// ──────────────────────────────────────────────────────────────────────
// The runner.

// FS2 0x001d3130

ScriptRunner::ScriptRunner()
{
    m_parentScript = nullptr;
    m_depth = 0;
    m_locals = nullptr;
    m_wait = 0;
    m_sp = 0;
    m_pc = nullptr;
    m_target = this;
    m_entry = nullptr;
    m_labels = nullptr;
    m_strings = nullptr;
    m_block = nullptr;
    m_yieldA = 0;
    m_yieldB = 0;
}

// FS2 0x001d30c0
// callcheck: operator_delete[], operator_delete (the locals; deleting)

ScriptRunner::~ScriptRunner()
{
    if (m_locals != &sNoLocals)
        delete[] m_locals;
    m_locals = nullptr;
}

// FS2 0x001d2ea0
// callcheck: getLabels, getStrings (getLabels, getStrings)

void ScriptRunner::bindEntry(const ScriptEntry *entry)
{
    m_entry = entry;
    if (m_entry == nullptr)
        return;
    if (m_block == nullptr)
        m_block = m_entry->block();
    getLabels();
    getStrings();
}

// FS2 0x001d2f40
// callcheck: operator_delete[], bindEntry, operator_new[] (bindEntry and the locals)

void ScriptRunner::setEntry(const void *entry)
{
    if (m_entry != nullptr) {
        if (m_locals != &sNoLocals)
            delete[] m_locals;
        m_locals = nullptr;
    }
    m_locals = nullptr;
    m_wait = 0;
    m_sp = 0;
    m_pc = nullptr;
    m_target = this;
    m_entry = nullptr;
    m_labels = nullptr;
    m_strings = nullptr;
    m_block = nullptr;
    m_yieldA = 0;
    m_yieldB = 0;
    bindEntry((const ScriptEntry *)entry);
    if (entry == nullptr)
        return;

    short slots = m_entry->slotCount;
    m_locals = slots == 0 ? &sNoLocals : new int[slots];
    for (int i = 0; i < m_entry->slotCount; i++)
        m_locals[i] = 0;
}

// The ROM's call has one more argument, which it never reads.
// FS2 0x001d27e0
// callcheck: execute (execute)

void ScriptRunner::run(int arg)
{
    (void)arg;
    ScriptRunner *previous = gRunningScript;
    m_depth++;
    gRunningScript = this;
    execute();
    m_depth--;
    gRunningScript = previous;
}

// FS2 0x001d2b10
// callcheck: none (the block is a field here)

const ScriptBlock *ScriptRunner::getBlock()
{
    if (m_block == nullptr)
        m_block = m_entry == nullptr ? nullptr : m_entry->block();
    return m_block;
}

// A parent's table if it has one, else the file's. The ROM spells out two
// levels before it recurses.
// FS2 0x001d2830
// callcheck: getParent2, getStrings, getBlock, findStrings, getEntry, block

const ScriptTable *ScriptRunner::getStrings()
{
    if (m_strings == nullptr) {
        if (m_parentScript != nullptr) {
            m_strings = m_parentScript->getStrings();
        } else {
            const ScriptBlock *block = getBlock();
            m_strings = block != nullptr ? block->findStrings() : nullptr;
        }
    }
    return m_strings;
}

// FS2 0x001d2bc0
// callcheck: getParent2, getLabels, getBlock, findLabels, getEntry, block

const ScriptTable *ScriptRunner::getLabels()
{
    if (m_labels == nullptr) {
        if (m_parentScript != nullptr) {
            m_labels = m_parentScript->getLabels();
        } else {
            const ScriptBlock *block = getBlock();
            m_labels = block != nullptr ? block->findLabels() : nullptr;
        }
    }
    return m_labels;
}

// FS2 0x001d19b0

const char *ScriptRunner::getSlotName(int index) const
{
    if (m_entry == nullptr || m_entry->slotNames == 0)
        return kScriptUnknownName;
    return (const char *)m_entry + m_entry->slotNames + index * kSlotNameSize;
}

// The ROM spells out this object and its parent before it recurses.
// FS2 0x001d1a10
// callcheck: strcmp, getEntry, getSlotCount, getSlotName, getLocals2, getParent2, getNamed

int ScriptRunner::getNamed(const char *name)
{
    for (int i = 0; i < getSlotCount(); i++) {
        if (strcmp(name, getSlotName(i)) == 0)
            return m_locals[i];
    }
    return m_parentScript != nullptr ? m_parentScript->getNamed(name) : 0;
}

// FS2 0x001d1750
// callcheck: strcmp, getEntry, getParent, getSlotCount, getSlotName, getLocals, setNamed

void ScriptRunner::setNamed(const char *name, int value)
{
    for (int i = 0; i < getSlotCount(); i++) {
        if (strcmp(name, getSlotName(i)) == 0) {
            m_locals[i] = value;
            return;
        }
    }
    if (m_parentScript != nullptr)
        m_parentScript->setNamed(name, value);
}

// FS2 0x001d32e0
// callcheck: getLabels

const int *ScriptRunner::readNameArg(const char **out, const int *code)
{
    const ScriptTable *labels = getLabels();
    int index = *code;
    if (index == ScriptLabel_Self) {
        *out = getName();
    } else if (index == ScriptLabel_Partner) {
        if (gStageScene->partner != nullptr)
            *out = gStageScene->partner->getObjectName();
    } else if (index == ScriptLabel_Player) {
        StageObject *player = scenePlayer();
        if (player != nullptr)
            *out = player->getObjectName();
    } else {
        *out = labels->label(index);
    }
    return code + 1;
}

// FS2 0x001d3200
// callcheck: getLabels

const int *ScriptRunner::readObjectArg(const char **out, const int *code)
{
    const ScriptTable *labels = getLabels();
    int index = *code;
    *out = labels->label(index);
    if (index == ScriptLabel_Self)
        scriptSetLabelCell(*out, getScriptObject());
    else if (index == ScriptLabel_Partner)
        scriptSetLabelCell(*out, gStageScene->partner);
    else if (index == ScriptLabel_Player)
        scriptSetLabelCell(*out, scenePlayer());
    return code + 1;
}

// FS2 0x001d33c0
// callcheck: getLabels

const int *ScriptRunner::readLabelArg(const char **out, const int *code)
{
    *out = getLabels()->label(*code);
    return code + 1;
}

// FS2 0x001d3410
// callcheck: getLabels

const int *ScriptRunner::readCellArg(void **out, const int *code)
{
    const char *label = getLabels()->label(*code);
    void *cell = scriptLabelCell(label);
    *out = cell != nullptr ? cell : (void *)label;
    return code + 1;
}

// FS2 0x001d3470
// callcheck: getStrings

const int *ScriptRunner::readStringArg(const char **out, const int *code)
{
    *out = getStrings()->string(*code);
    return code + 1;
}

// FS2 0x001d34c0

void ScriptRunner::popArgs(int *out, int count)
{
    for (int i = count - 1; i >= 0; i--)
        out[i] = m_stack[--m_sp];
}

// FS2 0x001d3710

void scriptYield(int a, int b)
{
    if (gRunningScript != nullptr) {
        gRunningScript->m_yieldA = a;
        gRunningScript->m_yieldB = b;
        sScriptYield = 1;
    }
}

void scriptYieldWhile(int value)
{
    enum { NativeWaitMotion = 0x63 };
    ScriptRunner *run = gRunningScript;
    // PC: the ROM reads low memory with no script running.
    if (run == nullptr)
        return;
    if (run->m_yieldA == 0) {
        scriptYield(NativeWaitMotion, value);
    } else if (run->m_yieldB == value) {
        sScriptYield = 1;
    } else {
        run->m_yieldA = 0;
        run->m_yieldB = 0;
    }
}

// FS2 0x001d3740
// callcheck: getDeltaSeconds

int scriptFramesTo30(int frames)
{
    float dt = gStageScene->clock->getDeltaSeconds();
    return (int)((float)(frames * 30 / (int)(1.0f / dt)) + 0.5f);
}

// FS2 0x001d37c0
// callcheck: getDeltaSeconds

int scriptFramesFrom30(int frames30)
{
    float dt = gStageScene->clock->getDeltaSeconds();
    return (int)((float)(frames30 * (int)(1.0f / dt) / 30) + 0.5f);
}

// FS2 0x001d0a60
// callcheck: strcmp, findAny

void ScriptRunner::setTargetByName(const char *name)
{
    if (strcmp(name, "PC") == 0) {
        m_target = scenePlayer();
    } else if (strcmp(name, "NPC") == 0) {
        m_target = gStageScene->partner;
    } else if (strcmp(name, "SELF") == 0) {
        m_target = this;
    } else {
        m_target = gStageScene->objects->findAny(name, 0);
    }
}

// An odd value is an object's handle, among the mission's characters; an
// even one was an object's EE address.
// PC: an address does not fit a script register: natives must leave handles
// (StageObject::getHandle) there. None does yet.
// FS2 0x0029f500
// callcheck: currentMission, currentMission, getHandle

static StageObject *scriptObjectOfValue(int value)
{
    if ((value & 1) == 0)
        return nullptr;
    StageMission *mission = gStageScene->currentMission();
    for (int i = 0; i < mission->characterCount; i++) {
        StageObject *object = mission->characters[i];
        if (value == (int)object->getHandle())
            return object;
    }
    return nullptr;
}

// FS2 0x001d09a0
// callcheck: scriptObjectOfValue

void ScriptRunner::setTargetByHandle(int handle)
{
    m_target = scriptObjectOfValue(handle);
}

// FS2 0x001d09e0
// callcheck: currentMission

void ScriptRunner::setTargetByIndex(int index)
{
    StageMission *mission = gStageScene->currentMission();
    StageObject *object = nullptr;
    if (index >= 1 && index <= mission->characterCount)
        object = mission->characters[index - 1];
    m_target = object;
}

// FS2 0x001cf1b0
// callcheck: FUN_001a69a0 (the log, empty in retail)

int ScriptRunner::unsupportedCommand()
{
    return 0;
}

// FS2 0x001cf1e0

int ScriptRunner::defaultCommand()
{
    return 0;
}

// FS2 0x001cfc20

int ScriptRunner::getProperty(int id, int sub)
{
    (void)id;
    (void)sub;
    YAMIPAN_STUB("ScriptRunner::getProperty", 0x001cfc20);
    return 0;
}

// FS2 0x001cf200

void ScriptRunner::setProperty(int id, int sub, int value)
{
    (void)id;
    (void)sub;
    (void)value;
    YAMIPAN_STUB("ScriptRunner::setProperty", 0x001cf200);
}


// ──────────────────────────────────────────────────────────────────────
// The VM.

// acc after a load: the value, or the pending operation on the old acc.
// FS2 0x001d3180

static int scriptApply(int math, int left, int right)
{
    switch (math) {
    case ScriptMath_Add: return left + right;
    case ScriptMath_Sub: return left - right;
    case ScriptMath_Mul: return left * right;
    // PC: the EE's div has no trap; the ROM's answer by 0 is undefined.
    case ScriptMath_Div: return right == 0 ? 0 : left / right;
    case ScriptMath_And: return left & right;
    case ScriptMath_Or:  return left | right;
    case ScriptMath_Xor: return left ^ right;
    default:             return right;
    }
}

// The word of ScriptOp_LoadParent and its kin.
static inline int scriptVarIndex(int word)
{
    if (word & ScriptVar_Property)
        return word & ScriptVar_PropertyId;
    int index = word & ScriptVar_IndexMask;
    if (!(word & (ScriptVar_Flag | ScriptVar_Local)) && index >= ScriptVar_FirstGlobal)
        index -= ScriptVar_FirstGlobal;
    return index;
}

typedef const int *(*ScriptNative)(ScriptRunner *self, int *acc, const int *code);
static const int *callNative(ScriptRunner *self, int id, const int *code, int *acc);

// Words until one yields or ends. Yields: 2 and 3 (m_wait), a native
// through scriptYield. Word 0 ends the pass: next time from the top.
// callcheck: getSeconds, getDeltaSeconds, setEntry, scriptApply, getFlagTable, get, getFlagTable, set, readNameArg, getLabels, setTargetByName, getNamed, setNamed, setTargetByIndex, setTargetByHandle, scriptCallNative (setTarget*, scriptApply, readNameArg, callNative; the ROM repeats them per case)
// FS2 0x001d1c40

int ScriptRunner::execute()
{
    if (m_entry == nullptr)
        return 0;

    int math = ScriptMath_Assign;
    int acc = 0;
    const int *pc = m_pc != nullptr ? m_pc : m_entry->code();
    int *globals = gStageScene->scriptSlots;
    int *locals = m_locals;
    const int *next = nullptr;

    while (*pc != ScriptOp_Restart) {
        sScriptYield = 0;
        int word = *pc;
        const int *args = pc + 1;
        const int *after = args;
        int value;

        // PC: no script here names an object that is missing, but the objects of
        // a sub-scene are stubs until their TU, and neither player nor partner
        // exists yet. Delete with SubSceneCharacter and StageCharacter.
        if (m_target == nullptr)
            m_target = this;
        switch (word) {
        case ScriptOp_Exit:
            setEntry(nullptr);
            onScriptExit();
            return -1;
        case ScriptOp_SetWait: {
            Clock *clock = gStageScene->clock;
            float now = clock->getSeconds();
            float then = now + (float)acc / 30.0f;
            float dt = clock->getDeltaSeconds();
            int frames = (int)(then / dt + 0.5f) - (int)(now / dt + 0.5f);
            if (gStageScene->isSubScene)
                frames--;
            m_wait = frames;
            m_pc = args;
            return 0;
        }
        case ScriptOp_Wait:
            if (m_wait > 0) {
                m_wait--;
                m_pc = pc;
                return 0;
            }
            break;
        case ScriptOp_JumpIfZero:
            after = acc != 0 ? args + 1 : args + *args;
            break;
        case ScriptOp_Jump:
            after = args + *args;
            break;
        case ScriptOp_TargetName: {
            const char *name = nullptr;
            after = readNameArg(&name, args);
            // PC: a fixed label's name is the object's, with no cell before
            // it (the ROM reads the object's +0x1c there; no script does it).
            void *cell = getLabels() != nullptr && *args >= ScriptLabel_First
                ? scriptLabelCell(name) : nullptr;
            if (cell == nullptr)
                setTargetByName(name);
            else
                m_target = static_cast<StageObject *>(cell);
            break;
        }
        case ScriptOp_LoadVar: {
            int var = *args;
            int index = scriptVarIndex(var);
            if (var & ScriptVar_Property)
                // PC: the ROM leaves a stale register as the sub index.
                value = m_target->getProperty(index, 0);
            else if (var & ScriptVar_Flag)
                value = getFlagTable()->get(index);
            else if ((var & ScriptVar_Local) || (var & ScriptVar_IndexMask) < ScriptVar_FirstGlobal)
                value = locals[index];
            else
                value = globals[index];
            acc = scriptApply(math, acc, value);
            math = ScriptMath_Assign;
            after = args + 1;
            break;
        }
        case ScriptOp_LoadConst:
            acc = scriptApply(math, acc, *args);
            math = ScriptMath_Assign;
            after = args + 1;
            break;
        case ScriptOp_StoreVar: {
            int var = *args;
            int index = scriptVarIndex(var);
            if (var & ScriptVar_Property)
                m_target->setProperty(index, (var & ScriptVar_SubMask) >> ScriptVar_SubShift, acc);
            else if (var & ScriptVar_Flag)
                getFlagTable()->set(index, acc);
            else if ((var & ScriptVar_Local) || (var & ScriptVar_IndexMask) < ScriptVar_FirstGlobal)
                locals[index] = acc;
            else
                globals[index] = acc;
            after = args + 1;
            break;
        }
        case ScriptOp_Push:
            m_stack[m_sp] = acc;
            m_stackMath[m_sp] = math;
            m_sp++;
            math = ScriptMath_Assign;
            break;
        case ScriptOp_PopApply:
            m_sp--;
            acc = scriptApply(m_stackMath[m_sp], m_stack[m_sp], acc);
            math = ScriptMath_Assign;
            break;
        case ScriptOp_PrefixAdd: math = ScriptMath_Add; break;
        case ScriptOp_PrefixSub: math = ScriptMath_Sub; break;
        case ScriptOp_PrefixMul: math = ScriptMath_Mul; break;
        case ScriptOp_PrefixDiv: math = ScriptMath_Div; break;
        case ScriptOp_PrefixAnd: math = ScriptMath_And; break;
        case ScriptOp_PrefixOr:  math = ScriptMath_Or;  break;
        case ScriptOp_PrefixXor: math = ScriptMath_Xor; break;
        case ScriptOp_PopEqual:
        case ScriptOp_PopLess:
        case ScriptOp_PopGreater:
        case ScriptOp_PopLessEq:
        case ScriptOp_PopGreaterEq:
        case ScriptOp_PopNotEqual:
            m_sp--;
            value = m_stack[m_sp];
            math = m_stackMath[m_sp];
            switch (word) {
            case ScriptOp_PopEqual:     acc = value == acc; break;
            case ScriptOp_PopLess:      acc = value < acc;  break;
            case ScriptOp_PopGreater:   acc = value > acc;  break;
            case ScriptOp_PopLessEq:    acc = value <= acc; break;
            case ScriptOp_PopGreaterEq: acc = value >= acc; break;
            default:                    acc = value != acc; break;
            }
            break;
        case ScriptOp_LoadNamed: {
            const char *name = nullptr;
            after = readNameArg(&name, args);
            acc = scriptApply(math, acc, m_target->getNamed(name));
            math = ScriptMath_Assign;
            break;
        }
        case ScriptOp_StoreNamed: {
            const char *name = nullptr;
            after = readNameArg(&name, args);
            m_target->setNamed(name, acc);
            break;
        }
        case ScriptOp_LoadParent:
            after = args + 1;
            if (m_parentScript != nullptr) {
                acc = scriptApply(math, acc, m_parentScript->m_locals[scriptVarIndex(*args)]);
                math = ScriptMath_Assign;
            }
            break;
        case ScriptOp_StoreParent:
            after = args + 1;
            if (m_parentScript != nullptr)
                m_parentScript->m_locals[scriptVarIndex(*args)] = acc;
            break;
        case ScriptOp_TargetIndex:
            setTargetByIndex(acc);
            m_sp--;
            acc = m_stack[m_sp];
            math = m_stackMath[m_sp];
            break;
        case ScriptOp_TargetHandle:
            setTargetByHandle(acc);
            m_sp--;
            acc = m_stack[m_sp];
            math = m_stackMath[m_sp];
            break;
        case ScriptOp_Skip:
            after = args + 1;
            break;
        case ScriptOp_LoadGrand: {
            after = args + 1;
            ScriptRunner *grand = m_parentScript != nullptr ? m_parentScript->m_parentScript : nullptr;
            if (grand != nullptr) {
                acc = scriptApply(math, acc, grand->m_locals[scriptVarIndex(*args)]);
                math = ScriptMath_Assign;
            }
            break;
        }
        case ScriptOp_StoreGrand: {
            after = args + 1;
            ScriptRunner *grand = m_parentScript != nullptr ? m_parentScript->m_parentScript : nullptr;
            if (grand != nullptr)
                grand->m_locals[scriptVarIndex(*args)] = acc;
            break;
        }
        case ScriptOp_TargetRaw:
            // PC: an EE address means nothing here; no script read uses it.
            m_target = this;
            after = args + 1;
            break;
        case ScriptOp_TargetPlayer:
            m_target = scenePlayer();
            break;
        case ScriptOp_TargetPartner:
            m_target = gStageScene->partner;
            break;
        case ScriptOp_TargetSelf:
            m_target = this;
            break;
        case ScriptOp_LoadLocal:
            acc = scriptApply(math, acc, locals[*args]);
            math = ScriptMath_Assign;
            after = args + 1;
            break;
        case ScriptOp_Nop27:
        case ScriptOp_Nop29:
        case ScriptOp_Nop2b:
        case ScriptOp_Nop2d:
            break;
        case ScriptOp_LoadProperty:
            acc = scriptApply(math, acc, m_target->getProperty(*args, 0));
            math = ScriptMath_Assign;
            after = args + 1;
            break;
        case ScriptOp_LoadPropertySub:
            value = m_target->getProperty(*args & 0xffff, (*args >> 16) & 0xff);
            acc = scriptApply(math, acc, value);
            math = ScriptMath_Assign;
            after = args + 1;
            break;
        case ScriptOp_LoadGlobal:
            acc = scriptApply(math, acc, globals[*args]);
            math = ScriptMath_Assign;
            after = args + 1;
            break;
        case ScriptOp_ConstEqual:     acc = acc == *args; after = args + 1; break;
        case ScriptOp_ConstLess:      acc = acc < *args;  after = args + 1; break;
        case ScriptOp_ConstGreater:   acc = acc > *args;  after = args + 1; break;
        case ScriptOp_ConstLessEq:    acc = acc <= *args; after = args + 1; break;
        case ScriptOp_ConstGreaterEq: acc = acc >= *args; after = args + 1; break;
        case ScriptOp_ConstNotEqual:  acc = acc != *args; after = args + 1; break;
        default: {
            sScriptYield = 0;
            int sp = m_sp;
            u_int kind = (u_int)word & kScriptCallMask;
            int id = (int)((u_int)word & kScriptCallId);
            bool yield;
            if (kind == kScriptCallLine) {
                next = args;
                yield = sScriptYield != 0;
            } else if (kind == kScriptCallCommand) {
                next = runCommand(id, args);
                yield = sScriptYield != 0;
            } else if (kind == kScriptCallNative) {
                next = callNative(this, id, args, &acc);
                yield = sScriptYield != 0;
            } else if (kind == kScriptCallNone) {
                yield = sScriptYield != 0;  // an unknown word below 0x36..: the last call's position
            } else {
                yield = true;               // an unknown call stays where it is
            }
            sScriptYield = 0;
            if (yield) {
                m_sp = sp;
                m_pc = pc;
                return 0;
            }
            if (next == nullptr)
                return 0;
            after = next;
            break;
        }
        }

        pc = after;
        switch (word) {
        case ScriptOp_TargetSelf:
        case ScriptOp_TargetPartner:
        case ScriptOp_TargetPlayer:
        case ScriptOp_TargetRaw:
        case ScriptOp_TargetHandle:
        case ScriptOp_TargetIndex:
        case ScriptOp_TargetName:
            break;
        default:
            m_target = this;
            break;
        }
    }
    m_sp = 0;
    m_pc = nullptr;
    return 0;
}


// ──────────────────────────────────────────────────────────────────────
// A saved frame.

// FS2 0x001d1710

ScriptFrame::ScriptFrame()
{
    m_locals = nullptr;
    m_wait = 0;
    m_sp = 0;
    m_pc = nullptr;
    m_target = nullptr;
    m_entry = nullptr;
    m_block = nullptr;
    m_yieldA = 0;
    m_yieldB = 0;
}

// FS2 0x001d16b0
// callcheck: operator_delete[]

void ScriptFrame::release()
{
    if (m_locals != &sNoLocals)
        delete[] m_locals;
    m_locals = nullptr;
    m_wait = 0;
    m_sp = 0;
    m_pc = nullptr;
    m_target = nullptr;
    m_entry = nullptr;
    m_block = nullptr;
    m_yieldA = 0;
    m_yieldB = 0;
}

// FS2 0x001d39b0
// callcheck: operator_delete[], operator_delete (release inline; deleting)

ScriptFrame::~ScriptFrame()
{
    release();
}

// FS2 0x001d1630

void ScriptFrame::clear()
{
    if (m_entry == nullptr)
        return;
    m_pc = nullptr;
    m_wait = 0;
    m_sp = 0;
    m_target = nullptr;
    if (m_locals != nullptr) {
        for (int i = 0; i < m_entry->slotCount; i++)
            m_locals[i] = 0;
    }
    m_yieldA = 0;
    m_yieldB = 0;
}

// The ROM leaves new locals uncleared.
// FS2 0x001d1340
// callcheck: operator_delete[], operator_new[]

void ScriptFrame::prepare(const ScriptEntry *entry)
{
    if (entry == m_entry) {
        clear();
        return;
    }
    release();
    m_entry = entry;
    if (m_entry != nullptr) {
        short slots = m_entry->slotCount;
        m_locals = slots == 0 ? &sNoLocals : new int[slots];
    }
}

// FS2 0x001d1480
// callcheck: bindEntry (bindEntry)

void ScriptFrame::restoreTo(ScriptRunner *runner) const
{
    runner->m_locals = m_locals;
    runner->m_wait = m_wait;
    runner->m_sp = m_sp;
    runner->m_pc = m_pc;
    runner->m_target = m_target != nullptr ? m_target : runner;
    runner->bindEntry(m_entry);
    for (int i = 0; i < m_sp; i++) {
        runner->m_stack[i] = m_stack[i];
        runner->m_stackMath[i] = m_stackMath[i];
    }
    runner->m_yieldA = m_yieldA;
    runner->m_yieldB = m_yieldB;
}

// FS2 0x001d1550

void ScriptFrame::saveFrom(ScriptRunner *runner)
{
    m_locals = runner->m_locals;
    m_wait = runner->m_wait;
    m_sp = runner->m_sp;
    m_pc = runner->m_pc;
    m_target = runner->m_target;
    m_entry = runner->m_entry;
    if (runner->m_block == nullptr && runner->m_entry != nullptr)
        runner->m_block = runner->m_entry->block();
    m_block = runner->m_block;
    for (int i = 0; i < m_sp; i++) {
        m_stack[i] = runner->m_stack[i];
        m_stackMath[i] = runner->m_stackMath[i];
    }
    m_yieldA = runner->m_yieldA;
    m_yieldB = runner->m_yieldB;
}


// ──────────────────────────────────────────────────────────────────────
// Natives: a light's or a named group's setup. ids in the comment.

// Every object of group 5 (the lights) named like the target. The target
// itself need not be one of them.
// PC: a target the map does not have is null here; the ROM reads through it.
template <class Fn> static void forEachNamed(ScriptRunner *target, Fn fn)
{
    if (target == nullptr || target->getName() == nullptr)
        return;
    StageObjArray *list = gStageScene->objects->findAll5(target->getName());
    for (StageObject **slot = list->begin(); slot != nullptr; slot = StageObjArray::next(slot))
        fn(*slot);
}

static inline void gsColor(float out[4], const int *rgb)
{
    int color[4] = { rgb[0], rgb[1], rgb[2], 0 };
    colorToFloatGs(out, color);
}

static inline bool isLightGroup(StageObject *object)
{
    if (object == nullptr)
        return false;
    int kind = object->getKind();
    return kind >= StageObject_Light && kind <= kLastLightKind;
}

// 0x26 position(x, y, z): a light moves with its namesakes.
// FS2 0x001cdc50
// callcheck: findAll5 (forEachNamed)

static const int *nativeSetPosition(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(3);
    sceVu0FVECTOR pos = { (float)args[0], (float)args[1], (float)args[2], 1.0f };
    if (target == nullptr)
        return code;
    if (isLightGroup(target->getScriptObject()))
        forEachNamed(target, [&](StageObject *object) { object->scriptSetPosition(pos); });
    else
        target->scriptSetPosition(pos);
    return code;
}

// 0x27 rotation(x, y, z), degrees. The ROM's walk calls the target each
// time, not the namesake: kept.
// FS2 0x001cd940
// callcheck: findAll5 (forEachNamed)

static const int *nativeSetRotation(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(3);
    sceVu0FVECTOR rot = { (float)args[0] * kDegreesToRadians, (float)args[1] * kDegreesToRadians,
                          (float)args[2] * kDegreesToRadians, 1.0f };
    if (target == nullptr)
        return code;
    if (isLightGroup(target->getScriptObject()))
        forEachNamed(target, [&](StageObject *) { target->scriptSetRotation(rot); });
    else
        target->scriptSetRotation(rot);
    return code;
}

// 0x2a lightColor(r, g, b)
// FS2 0x001cd440
// callcheck: colorToFloatGs, findAll5 (forEachNamed)

static const int *nativeLightColor(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    sceVu0FVECTOR color;
    gsColor(color, self->popped(3));
    forEachNamed(target, [&](StageObject *object) { object->cmdSetColor(color); });
    return code;
}

// 0x2b lightRadiusSquared(value)
// FS2 0x001cd240
// callcheck: findAll5 (forEachNamed)

static const int *nativeLightRadiusSquared(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    int value = self->popped(1)[0];
    forEachNamed(target, [&](StageObject *object) { object->cmdSetRadiusSquared((float)value); });
    return code;
}

// 0x2c lightConeAngle(degrees)
// FS2 0x001cd040
// callcheck: findAll5 (forEachNamed)

static const int *nativeLightConeAngle(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    int value = self->popped(1)[0];
    forEachNamed(target, [&](StageObject *object) { object->cmdSetConeAngle(value); });
    return code;
}

// 0x203 lightRadius(radius)
// FS2 0x001ab180
// callcheck: findAll5 (forEachNamed)

static const int *nativeLightRadius(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    int value = self->popped(1)[0];
    forEachNamed(target, [&](StageObject *object) { object->cmdSetRadiusSquared((float)(value * value)); });
    return code;
}

// 0x229 lightIntensityPercent(percent)
// FS2 0x001a86b0
// callcheck: findAll5 (forEachNamed)

static const int *nativeLightValue2d8(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    int value = self->popped(1)[0];
    forEachNamed(target, [&](StageObject *object) { object->cmdSetValue2d8(value); });
    return code;
}

// 0x20c lightEnabled(on)
// FS2 0x001aa720
// callcheck: findAll5 (forEachNamed)

static const int *nativeLightEnabled(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    int value = self->popped(1)[0];
    forEachNamed(target, [&](StageObject *object) { object->cmdSetEnabled(value != 0); });
    return code;
}

// 0x1b3 lightModelFade(on)
// FS2 0x001b1850
// callcheck: findAll5 (forEachNamed)

static const int *nativeLightFlag240(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    int value = self->popped(1)[0];
    forEachNamed(target, [&](StageObject *object) { object->cmdSetFlag240(value != 0); });
    return code;
}

// 0x1b4 lightLightsModels(on)
// FS2 0x001b1650
// callcheck: findAll5 (forEachNamed)

static const int *nativeLightFlag248(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    int value = self->popped(1)[0];
    forEachNamed(target, [&](StageObject *object) { object->cmdSetFlag248(value != 0); });
    return code;
}

// 0x1e2 lightFadeFrames(frames)
// FS2 0x001ad520
// callcheck: findAll5 (forEachNamed)

static const int *nativeLightFadeFrames(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    int value = self->popped(1)[0];
    forEachNamed(target, [&](StageObject *object) { object->cmdSetFadeFrames(value); });
    return code;
}

// 0x1e3 lightModelFadeFrames(frames)
// FS2 0x001ad320
// callcheck: findAll5 (forEachNamed)

static const int *nativeLightValue244(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    int value = self->popped(1)[0];
    forEachNamed(target, [&](StageObject *object) { object->cmdSetValue244(value); });
    return code;
}

// 0x1e4 lightModelColors(r, g, b, ambient r, g, b)
// FS2 0x001ad090
// callcheck: colorToFloatGs, colorToFloatGs, findAll5 (forEachNamed)

static const int *nativeLightModelColors(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(6);
    sceVu0FVECTOR color, ambient;
    gsColor(color, args);
    gsColor(ambient, args + 3);
    forEachNamed(target, [&](StageObject *object) {
        object->cmdSetColor250(color);
        object->cmdSetColor260(ambient);
    });
    return code;
}

// 0x1e5 lightCastsShadow(on)
// FS2 0x001ace90
// callcheck: findAll5 (forEachNamed)

static const int *nativeLightCastsShadow(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    int value = self->popped(1)[0];
    forEachNamed(target, [&](StageObject *object) { object->cmdSetCastsShadow(value != 0); });
    return code;
}

// 0x1d4 isActive(label): the object of the label exists and is active.
// FS2 0x001ae310
// callcheck: readObjectArg (readObjectArg)

static const int *nativeIsActive(ScriptRunner *self, int *acc, const int *code)
{
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    *acc = object != nullptr && object->m_active != 0;
    return next;
}


// ──────────────────────────────────────────────────────────────────────
// Natives: SceneRender's tracks (architecture.md "The lights of a stage").
// Colours are integers, /127.5 (Gs) or /255; angles degrees.

static inline void setKey(VectorTrack &track, int key, int x, int y, int z, int w)
{
    float *value = track.keyAt(key)->value;
    value[0] = (float)x;
    value[1] = (float)y;
    value[2] = (float)z;
    value[3] = (float)w;
}

static inline void setGsColorKey(VectorTrack &track, int key, const int *rgb)
{
    int color[4] = { rgb[0], rgb[1], rgb[2], 0 };
    colorToFloatGs(track.keyAt(key)->value, color);
}

static inline void setColorKey(VectorTrack &track, int key, const int *rgb)
{
    int color[4] = { rgb[0], rgb[1], rgb[2], 0 };
    colorToFloat(track.keyAt(key)->value, color);
}

// 0x1b6 clearColor(set, key, r, g, b): 0..255
// FS2 0x001b11d0
// callcheck: keyAt, keyAt, keyAt, keyAt (setKey)

static const int *nativeClearColor(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(5);
    setKey(gStageScene->render->getSet(args[0]).clearColor, args[1], args[2], args[3], args[4], 0);
    return code;
}

// 0x1b7 ambient(set, key, r, g, b)
// FS2 0x001b10a0
// callcheck: keyAt, colorToFloatGs (setGsColorKey)

static const int *nativeAmbient(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(5);
    setGsColorKey(gStageScene->render->getSet(args[0]).ambient, args[1], args + 2);
    return code;
}

// 0x1b8 light(set, i, key, rx, ry, rz, r, g, b)
// FS2 0x001b0d50
// callcheck: keyAt, keyAt, keyAt, keyAt, updateDirection, keyAt, colorToFloatGs (setKey, setGsColorKey)

static const int *nativeLight(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(9);
    SceneRender *render = gStageScene->render;
    SceneLightSet &set = render->getSet(args[0]);
    setKey(set.angles[args[1]], args[2], args[3], args[4], args[5], 0);
    render->updateDirection(args[0], args[1], args[2]);
    setGsColorKey(set.color[args[1]], args[2], args + 6);
    return code;
}

// 0x1b9 modelAmbient(set, key, r, g, b)
// FS2 0x001b0c20
// callcheck: keyAt, colorToFloatGs (setGsColorKey)

static const int *nativeModelAmbient(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(5);
    setGsColorKey(gStageScene->render->getSet(args[0]).modelAmbient, args[1], args + 2);
    return code;
}

// 0x1ba modelLight(set, i, key, rx, ry, rz, r, g, b)
// FS2 0x001b08d0
// callcheck: keyAt, keyAt, keyAt, keyAt, updateModelDirection, keyAt, colorToFloatGs (setKey, setGsColorKey)

static const int *nativeModelLight(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(9);
    SceneRender *render = gStageScene->render;
    SceneLightSet &set = render->getSet(args[0]);
    setKey(set.modelAngles[args[1]], args[2], args[3], args[4], args[5], 0);
    render->updateModelDirection(args[0], args[1], args[2]);
    setGsColorKey(set.modelColor[args[1]], args[2], args + 6);
    return code;
}

// 0x1bb fog(set, key, near, far, near value, far value, r, g, b)
// FS2 0x001b0600
// callcheck: keyAt, keyAt, keyAt, keyAt, keyAt, colorToFloat (setKey, setColorKey)

static const int *nativeFog(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(9);
    SceneLightSet &set = gStageScene->render->getSet(args[0]);
    setKey(set.fog, args[1], args[2], args[3], args[4], args[5]);
    setColorKey(set.fogColor, args[1], args + 6);
    return code;
}

// 0x1bc farFog0(set, key, near, far, near value, far value, r, g, b): the
// first far fog; 0x217 names any.
// FS2 0x001b0320
// callcheck: keyAt, keyAt, keyAt, keyAt, keyAt, colorToFloat (setKey, setColorKey)

static const int *nativeFarFog0(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(9);
    SceneLightSet &set = gStageScene->render->getSet(args[0]);
    setKey(set.farFog[0], args[1], args[2], args[3], args[4], args[5]);
    setColorKey(set.farFogColor[0], args[1], args + 6);
    return code;
}

// 0x217 farFog(set, i, key, near, far, near value, far value, r, g, b)
// FS2 0x001a9a40
// callcheck: keyAt, keyAt, keyAt, keyAt, keyAt, colorToFloat (setKey, setColorKey)

static const int *nativeFarFog(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(10);
    SceneLightSet &set = gStageScene->render->getSet(args[0]);
    setKey(set.farFog[args[1]], args[2], args[3], args[4], args[5], args[6]);
    setColorKey(set.farFogColor[args[1]], args[2], args + 7);
    return code;
}

// 0x1bd transitionRange(set, key, value)
// FS2 0x001b0210
// callcheck: keyAt

static const int *nativeTransitionRange(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(3);
    gStageScene->render->getSet(args[0]).transition.keyAt(args[1])->value[0] = (float)args[2];
    return code;
}

// 0x1be transitionFrames(set, key, value)
// FS2 0x001b0100
// callcheck: keyAt

static const int *nativeTransitionFrames(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(3);
    gStageScene->render->getSet(args[0]).transition.keyAt(args[1])->value[1] = (float)args[2];
    return code;
}

// 0x1e1 transitionSbFrames(set, key, value)
// FS2 0x001ad720
// callcheck: keyAt

static const int *nativeTransitionSbFrames(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(3);
    gStageScene->render->getSet(args[0]).transition.keyAt(args[1])->value[2] = (float)args[2];
    return code;
}

// 0x1c2 addSetKeys(set, time)
// FS2 0x001af770
// callcheck: addSetKeys

static const int *nativeAddSetKeys(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    gStageScene->render->addSetKeys(args[0], args[1]);
    return code;
}

// 0x1c3 setSetLoop(set, loop)
// FS2 0x001af6a0
// callcheck: setSetLoop

static const int *nativeSetSetLoop(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    gStageScene->render->setSetLoop(args[0], args[1] != 0);
    return code;
}

// 0x1df modelAmbientSb(set, key, r, g, b)
// FS2 0x001ad970
// callcheck: keyAt, colorToFloatGs (setGsColorKey)

static const int *nativeModelAmbientSb(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(5);
    setGsColorKey(gStageScene->render->getSet(args[0]).modelAmbientSb, args[1], args + 2);
    return code;
}

// 0x1e0 modelColorSb(set, i, key, r, g, b)
// FS2 0x001ad830
// callcheck: keyAt, colorToFloatGs (setGsColorKey)

static const int *nativeModelColorSb(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(6);
    setGsColorKey(gStageScene->render->getSet(args[0]).modelColorSb[args[1]], args[2], args + 3);
    return code;
}


// ──────────────────────────────────────────────────────────────────────
// The table.

// ──────────────────────────────────────────────────────────────────────
// Natives: starting an event (game/demoevent.cpp).

constexpr int   kEventDefaultDelay = 15;    // 30ths of a second, but for d0503
constexpr float kScriptFramesPerSecond = 30.0f;
static const char kEventNoDelay[] = "d0503";    /* data 0x304b98 */

// What the waiting native left on the runner (scriptYield), cleared; 0 the
// first time.
static int takeScriptYield()
{
    ScriptRunner *running = gRunningScript;
    if (running == nullptr)
        return 0;
    return running->takeYield();
}

// A file event from a script: starts it, and waits for it to end. 0x15f, 0x160
static void waitFileEvent(float seconds, const char *name)
{
    enum { Native = 0x160, Start = 0, Running = 1, Done = 2 };
    StageEvent *event = gStageScene->event;
    switch (takeScriptYield()) {
    case Start:
        if (event == nullptr) {
            gStageScene->startFileEvent(seconds, name);
            scriptYield(Native, Running);
        } else {
            scriptYield(Native, Start);
        }
        break;
    case Running:
        scriptYield(Native, event == nullptr ? Done : Running);
        break;
    default:
        break;
    }
}

// The same with a script event. 0x161, 0x162
static void waitSimpleEvent(float seconds, const char *name)
{
    enum { Native = 0x162, Start = 0, Running = 1 };
    StageEvent *event = gStageScene->event;
    switch (takeScriptYield()) {
    case Start:
        if (event == nullptr) {
            gStageScene->startSimpleEvent(seconds, name);
            scriptYield(Native, Running);
        } else {
            scriptYield(Native, Start);
        }
        break;
    case Running:
        if (event != nullptr)
            scriptYield(Native, Running);
        break;
    default:
        break;
    }
}

// 0x15f fileEvent(name): 15 / 30 s of fade, none for d0503
// FS2 0x001b77a0
// callcheck: readNameArg, strcmp, startFileEvent, scriptYield

static const int *nativeFileEvent(ScriptRunner *self, int *, const int *code)
{
    const char *name = nullptr;
    const int *next = self->readNameArg(&name, code);
    int delay = strcmp(name, kEventNoDelay) == 0 ? 0 : kEventDefaultDelay;
    waitFileEvent((float)delay / kScriptFramesPerSecond, name);
    return next;
}

// 0x160 fileEventDelayed(delay; name)
// FS2 0x001b7550
// callcheck: readNameArg, startFileEvent, scriptYield

static const int *nativeFileEventDelayed(ScriptRunner *self, int *, const int *code)
{
    int delay = self->popped(1)[0];
    const char *name = nullptr;
    const int *next = self->readNameArg(&name, code);
    waitFileEvent((float)delay / kScriptFramesPerSecond, name);
    return next;
}

// 0x144 demoPosition(x, y, z): the target's place, three float words in the
// code.
// FS2 0x001b9ac0

static const int *nativeDemoPosition(ScriptRunner *self, int *, const int *code)
{
    enum { Words = 3 };
    ScriptRunner *target = self->getTarget();
    sceVu0FVECTOR pos = { 0.0f, 0.0f, 0.0f, 1.0f };
    memcpy(pos, code, Words * sizeof(float));
    target->scriptSetPosition(pos);
    return code + Words;
}

// The object joins the event with motion and shows.
// FS2 0x001b5460
// callcheck: setActive, setShown

static void demoSwitchOn(StageObject *object, bool on)
{
    object->m_tree->setActive(on);
    object->m_tree->setShown(on);
}

// 0x145 demoBind(name, motion): before the event's cameras are ready, the
// object called name takes motion and comes on. One missing is made: the
// files' character or item, else an actor with no model of its own.
static void demoBindObject(StageObject *object, const char *motion)
{
    static_cast<SubSceneModelObject *>(object)->bindMotion(motion);
    demoSwitchOn(object, true);
    object->setTreeShown(true);
}

// FS2 0x001b9680
// callcheck: readNameArg, readStringArg, FUN_001b9ab0, FUN_001b36b0, FUN_001b9aa0, FUN_001b9a80, FUN_001b9a30, FUN_001b9a70, FUN_001b9a60, FUN_001b9a50, strcmp, FUN_001b9a20, hasCharacter, placeCharacter, find8, hasItem, placeItem, find8, operator_new, SubSceneModelObject, FUN_001a4c20, attach, FUN_001a4c40, demoSwitchOn, bindMotion (small getters and the list walk, inline here; the ROM's bind sequences are demoBindObject)

static const int *nativeDemoBind(ScriptRunner *self, int *, const int *code)
{
    const char *name = nullptr;
    const char *motion = nullptr;
    code = self->readNameArg(&name, code);
    code = self->readStringArg(&motion, code);
    StageScene *scene = gStageScene;
    if (scene->camerasReady)
        return code;

    StageObjArray *group = &scene->objects->m_lists[StageObjGroup_8];
    for (StageObject **slot = group->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *object = *slot;
        if (strcmp(object->m_name, name) == 0) {
            demoBindObject(object, motion);
            return code;
        }
    }

    StageFiles *files = scene->files;
    if (files->hasCharacter(name))
        files->placeCharacter(name, nullptr, nullptr);
    else if (files->hasItem(name))
        files->placeItem(name, nullptr, nullptr);
    else {
        SubSceneModelObject *actor = new SubSceneModelObject(SubSceneRole_Actor);
        StringMap<Tim2Texture *> textures;
        actor->attach(name, nullptr, &textures);
        actor->bindMotion(motion);
        demoSwitchOn(actor, true);
        return code;
    }
    if (StageObject *object = scene->objects->find8(name, 0))
        demoBindObject(object, motion);
    return code;
}

// 0x163 demoFade(frames, colour, _, _): back from a colour; frames below 0 at
// once. Of the last three the ROM keeps only the first: it overwrites what it
// has shifted, so the two others count for nothing (asm 0x1b7070..0x1b7080).
// FS2 0x001b6fe0
// callcheck: FUN_00151380, start, start

static const int *nativeDemoFade(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(4);
    int frames = args[0];
    StageFade *fade = gStageScene->fade;
    fade->setColour((u_int)args[1]);
    if (frames < 0)
        fade->start(0.0f, 0.0f, 0.0f);
    else
        fade->start((float)frames / kScriptFramesPerSecond, 0.0f, 1.0f);
    return code;
}

// 0x164 demoFadeOut(frames, colour, _, _): to a colour, as 0x163 back from one.
// FS2 0x001b6e50
// callcheck: FUN_00151380, start, start

static const int *nativeDemoFadeOut(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(4);
    int frames = args[0];
    StageFade *fade = gStageScene->fade;
    fade->setColour((u_int)args[1]);
    if (frames < 0)
        fade->start(0.0f, 0.0f, 0.0f);
    else
        fade->start((float)frames / kScriptFramesPerSecond, 1.0f, 0.0f);
    return code;
}

// 0x148 demoMotion(name): the target plays the pack's motion; on the demo
// camera, a cut. This is what reaches DemoCamera::playCutNamed.
// FS2 0x001b92f0
// callcheck: readStringArg

static const int *nativeDemoMotion(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name = nullptr;
    code = self->readStringArg(&name, code);
    target->scriptPlayMotion(name);
    return code;
}

// 0x159 demoCut(name): the next cut of an event, a motion of its pack on the
// demo camera. The runner's first one waits a frame.
// The ROM then compares a null pointer with "SJN" and "SJE" for the overlay
// (data 0x304b58, 0x304b60: 1, 2); nothing there matches, so it is 0.
// PC: the kind test: the ROM calls the slot on whatever is on top.
// FS2 0x001b7e70
// callcheck: readStringArg, scriptYield, getTopView, playCutNamed, strcmp, strcmp, setOverlay

static const int *nativeDemoCut(ScriptRunner *self, int *, const int *code)
{
    enum { Native = 0x147 };    // what the ROM yields under; 0x147 waits the same way
    const char *name = nullptr;
    const int *next = self->readStringArg(&name, code);
    takeScriptYield();
    if (self->m_cutWaited == 0) {
        self->m_cutWaited = 1;
        scriptYield(Native, 0);
        return next;
    }
    StageObject *top = gStageScene->getTopView();
    if (top != nullptr && top->getKind() == StageObject_Kind13) {
        DemoCamera *camera = static_cast<DemoCamera *>(top);
        camera->playCutNamed(name);
        camera->setOverlay(0);
        camera->m_demoCut++;
    }
    return next;
}

// 0x161 simpleEvent(name): half a second of fade
// FS2 0x001b7390
// callcheck: readNameArg, startSimpleEvent, scriptYield

static const int *nativeSimpleEvent(ScriptRunner *self, int *, const int *code)
{
    const char *name = nullptr;
    const int *next = self->readNameArg(&name, code);
    waitSimpleEvent(0.5f, name);
    return next;
}

// 0x162 simpleEventDelayed(delay; name)
// FS2 0x001b7170
// callcheck: readNameArg, startSimpleEvent, scriptYield

static const int *nativeSimpleEventDelayed(ScriptRunner *self, int *, const int *code)
{
    int delay = self->popped(1)[0];
    const char *name = nullptr;
    const int *next = self->readNameArg(&name, code);
    waitSimpleEvent((float)delay / kScriptFramesPerSecond, name);
    return next;
}

// 0x155 simpleEventNow(name): black at once, in the stage's menu mode 1 only
// FS2 0x001b8460
// callcheck: readNameArg, getMenuMode, start, startSimpleEvent

static const int *nativeSimpleEventNow(ScriptRunner *self, int *, const int *code)
{
    const char *name = nullptr;
    const int *next = self->readNameArg(&name, code);
    if (gStageScene->getMenuMode() == 1) {
        gStageScene->fade->start(0.0f, 1.0f, 0.0f);
        gStageScene->startSimpleEvent(0.0f, name);
    }
    return next;
}

// 0x1f2 pendingEvent(name): black at once; the event starts after the scripts
// FS2 0x001ac0b0
// callcheck: readNameArg, getMenuMode, start, currentMission, setPendingEvent

static const int *nativePendingEvent(ScriptRunner *self, int *, const int *code)
{
    const char *name = nullptr;
    const int *next = self->readNameArg(&name, code);
    if (gStageScene->getMenuMode() == 1) {
        gStageScene->fade->start(0.0f, 1.0f, 0.0f);
        gStageScene->currentMission()->setPendingEvent(name);
    }
    return next;
}

// 0x70 setPlayer(name): the character called name becomes the player and the
// cameras follow it; no such character clears the player. Stage 1's Root does
// this for F_PC01 in its menu mode 1 block.
// FS2 0x001c81a0
// callcheck: readNameArg, findCharacter, setPlayer

static const int *nativeSetPlayer(ScriptRunner *self, int *, const int *code)
{
    const char *name = nullptr;
    const int *next = self->readNameArg(&name, code);
    // PC: the player or partner label with nobody behind it leaves the ROM's
    // name uninitialised.
    if (name == nullptr)
        return next;
    StageObject *who = gStageScene->objects->findCharacter(name, 0);
    gStageScene->setPlayer(static_cast<StagePlayer *>(who));
    return next;
}

static const char kViewNone[]  = "NULL";        /* data 0x304ba8 */
static const char kViewLight[] = "scr_demo_pl"; /* data 0x304bd0 */
constexpr int kViewPriorityScript = 3;          // between the main camera 0 and the demo camera 4
constexpr u_char kViewFlagScript  = 1;

// FS2 0x001c8f40

static bool isNullName(const char *name)
{
    return strcmp(name, kViewNone) == 0;
}

// FS2 0x001d1260
// callcheck: find0

static StageObject *findCameraObject(const char *name)
{
    return gStageScene->objects->find0(name, 0);
}

// FS2 0x001b56b0
// callcheck: find5

static StageObject *findLightObject(const char *name)
{
    return gStageScene->objects->find5(name, 0);
}

// 0x64 scriptView(name): the object called name takes the view above the main
// camera; "NULL" gives it back. During an event the light scr_demo_pl hangs
// from that view while it lasts.
// FS2 0x001c8dc0
// callcheck: readNameArg, isNullName, removeViewPriority, isEventActive, findLightObject, setParent, demoSwitchOn, findCameraObject, getTopView, pushView, isEventActive, findLightObject, setParent, demoSwitchOn

static const int *nativeScriptView(ScriptRunner *self, int *, const int *code)
{
    const char *name = nullptr;
    const int *next = self->readNameArg(&name, code);
    // PC: as nativeSetPlayer.
    if (name == nullptr)
        return next;

    if (isNullName(name)) {
        gStageScene->removeViewPriority(kViewPriorityScript);
        if (gStageScene->isEventActive()) {
            StageObject *light = findLightObject(kViewLight);
            light->setParent(nullptr);
            demoSwitchOn(light, false);
        }
        return next;
    }

    StageObject *view = findCameraObject(name);
    if (view != nullptr && gStageScene->getTopView() != view) {
        gStageScene->pushView(view, kViewPriorityScript, kViewFlagScript);
        if (gStageScene->isEventActive()) {
            StageObject *light = findLightObject(kViewLight);
            light->setParent(view);
            demoSwitchOn(light, true);
        }
    }
    return next;
}

// 0x53 menuMode(): acc = StageScene::getMenuMode. Stage 1's Root tests it for 1
// first of all: everything it does sits behind this.
// FS2 0x001ca5a0
// callcheck: getMenuMode

static const int *nativeMenuMode(ScriptRunner *, int *acc, const int *code)
{
    *acc = gStageScene->getMenuMode();
    return code;
}

// 0x4e fadeIn(frames): frames in 30ths, from black
// FS2 0x001cab60
// callcheck: start

static const int *nativeFadeIn(ScriptRunner *self, int *, const int *code)
{
    int frames = self->popped(1)[0];
    gStageScene->fade->start((float)frames / kScriptFramesPerSecond, 0.0f, 1.0f);
    return code;
}

// 0x138 missionValue(value)
// FS2 0x001ba660
// callcheck: setMissionValue

static const int *nativeMissionValue(ScriptRunner *self, int *, const int *code)
{
    gStageScene->setMissionValue((float)self->popped(1)[0]);
    return code;
}

// 0x139 moveScale(action, percent)
// FS2 0x001ba590
// callcheck: setMoveScale

static const int *nativeMoveScale(ScriptRunner *self, int *, const int *code)
{
    constexpr int kMoveActions = 6;
    const int *args = self->popped(2);
    if (args[0] >= 0 && args[0] < kMoveActions)
        gStageScene->setMoveScale((float)args[1] / 100.0f, args[0]);
    return code;
}

enum { kFirstNative = 0x19, kLastNative = 0x235, kNativeCount = kLastNative - kFirstNative + 1 };

// What the VM does for a native nobody wrote yet.
// layout: host
constexpr int kScriptUseKindLast = 0x7531;    // natives 0x79, 0xf6
constexpr int kNoticeForever = 0x7fffffff;    // native 0x14f
constexpr int kNoticeLeastLevel = 5;          // sdata 0x31a99c, 0x31a9a0
constexpr u_int kNativeMotionFlags = 0x50;     // native 0xb1
constexpr int kDefaultNoticeFrames = 0x96;     // natives 0x46, 0x6a: 5 seconds
static const char kNoObjectName[] = "NULL";   // native 0x6b: data 0x304ba8

// layout: host
struct ScriptNativeInfo
{
    u_char pops;
    u_char words;
    u_int  address;
};
static const ScriptNativeInfo kNativeInfo[kNativeCount] = {
#include "game/scriptnatives.inc"
};

// layout: host
// layout: host
struct ScriptNativeBinding
{
    int          id;
    ScriptNative native;
};
// 0xf7 setUseKind(kind, priority, label), 0xf8 enableUseKind(kind, on): the
// built-in kinds of the Use button's list.
// FS2 0x001be680, 0x001be5b0
// callcheck: readCellArg, FUN_0023eff0, FUN_0023efb0

static const int *nativeEnableUseKind(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    gStageScene->itemList->enableKind(args[0], args[1] != 0);
    return code;
}

static const int *nativeSetUseKind(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    void *label;
    const int *next = self->readCellArg(&label, code);
    gStageScene->itemList->setKind(args[0], label, args[1]);
    return next;
}

// A script's door: what it asks of the character it targets (game/stagedoor.h).
// 0x79 offerUse(kind, label), 0xf6 offerUseIf(kind, enabled, label): the Use
// button's label, for the target character.
// FS2 0x001c79b0, 0x001be770
// callcheck: readCellArg, FUN_0023ef40, readCellArg, FUN_0023ef40 (the slot is getScriptCharacter)

static int scriptUseKind(int value)
{
    const int kind = value + 1;
    return kind < 1 ? 1 : kind > kScriptUseKindLast ? kScriptUseKindLast : kind;
}

static const int *nativeOfferUse(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int kind = self->popped(1)[0];
    void *label;
    const int *next = self->readCellArg(&label, code);
    gStageScene->itemList->post(target->getScriptCharacter(), scriptUseKind(kind), label, 0, true);
    *acc = 0;
    return next;
}

static const int *nativeOfferUseIf(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(2);
    const int kind = args[0], enabled = args[1];
    void *label;
    const int *next = self->readCellArg(&label, code);
    gStageScene->itemList->post(target->getScriptCharacter(), scriptUseKind(kind), label, 0, enabled != 0);
    *acc = 0;
    return next;
}

// 0x44 isFacedBy(object).
// FS2 0x001cb530
// callcheck: readObjectArg

static const int *nativeIsFacedBy(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    // PC: with no player yet the ROM reads low memory and answers no.
    StageObject *object = (StageObject *)scriptLabelCell(label);
    *acc = object != nullptr && target->scriptIsFacing(object);
    return next;
}

// 0x45 isMenuPick(text): the menu's pending answer is text.
// FS2 0x001cb3e0
// callcheck: readCellArg, wstrcmp

static const int *nativeIsMenuPick(ScriptRunner *self, int *acc, const int *code)
{
    void *text;
    const int *next = self->readCellArg(&text, code);
    StageMenu *menu = gStageScene->menu;
    *acc = menu->pendingOn != 0 && menu->pending != nullptr
        && wstrcmp(menu->pending, (const u_short *)text) == 0;
    return next;
}

// 0x87 openDoor(), 0x88 closeDoor(), 0x89 useDoorKey(lock): on the character.
// FS2 0x001c6f90, 0x001c6f00, 0x001c6e20

static const int *nativeOpenDoor(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptOpenDoor();
    return code;
}

static const int *nativeCloseDoor(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptCloseDoor();
    return code;
}

static const int *nativeUseDoorKey(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    *acc = target->scriptUseDoorKey(self->popped(1)[0]);
    return code;
}

// 0x11b canOpenDoor(), 0x11c canCloseDoor(), 0x11d canLockDoor(),
// 0x12c canUnlockDoor(): the character's doorActions.
// FS2 0x001bc620, 0x001bc560, 0x001bc4a0, 0x001bb2b0
// callcheck: FUN_00219b30

static const int *answerDoorAction(ScriptRunner *self, int *acc, const int *code, int action)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && (who->doorActions() & action) != 0;
    return code;
}

static const int *nativeCanOpenDoor(ScriptRunner *self, int *acc, const int *code)
{
    return answerDoorAction(self, acc, code, CharacterDoor_Open);
}

static const int *nativeCanCloseDoor(ScriptRunner *self, int *acc, const int *code)
{
    return answerDoorAction(self, acc, code, CharacterDoor_Close);
}

static const int *nativeCanLockDoor(ScriptRunner *self, int *acc, const int *code)
{
    return answerDoorAction(self, acc, code, CharacterDoor_Lock);
}

static const int *nativeCanUnlockDoor(ScriptRunner *self, int *acc, const int *code)
{
    return answerDoorAction(self, acc, code, CharacterDoor_Unlock);
}

// 0x136 canUse(): no notice stands in the way of the Use button's answer.
// FS2 0x001ba7e0
// callcheck: isEventActive, FUN_00207e20, FUN_00207e20, FUN_00207e20

static const int *nativeCanUse(ScriptRunner *, int *acc, const int *code)
{
    PauseMenuState *state = gStageScene->pauseState;
    const bool busy = gStageScene->isEventActive() ? state->hasNotice(3)
                                                   : state->hasNotice(1) || state->hasNotice(0);
    *acc = !busy;
    return code;
}

// 0x14f notice(frames, arg, level, text): frames 0 stands for ever.
// FS2 0x001b8b00
// callcheck: readCellArg, addNotice

static const int *nativeNotice(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(3);
    const int frames = args[0], arg = args[1], level = args[2];
    void *text;
    const int *next = self->readCellArg(&text, code);
    gStageScene->pauseState->addNotice((const u_short *)text, frames == 0 ? kNoticeForever : frames, arg,
                                       level < kNoticeLeastLevel ? kNoticeLeastLevel : level);
    return next;
}

// 0x15b doorUsed(): the running door's state for the S2D.
// FS2 0x001b7c90
// callcheck: FUN_00172530

static const int *nativeDoorUsed(ScriptRunner *self, int *, const int *code)
{
    self->getTarget()->getScriptDoor()->noteUse(true);
    return code;
}

// 0x231 keyWorksFrom(object).
// FS2 0x001a7ea0
// callcheck: readObjectArg, FUN_001739f0

static const int *nativeKeyWorksFrom(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    // PC: the same as nativeIsFacedBy.
    *acc = object != nullptr && target->getScriptDoor()->keyWorksFrom(object->m_xform.position);
    return next;
}

// The doors' (game/stagedoor.h).
static const ScriptNativeBinding kDoorNatives[] = {
    { 0x044, nativeIsFacedBy },
    { 0x045, nativeIsMenuPick },
    { 0x079, nativeOfferUse },
    { 0x087, nativeOpenDoor },
    { 0x088, nativeCloseDoor },
    { 0x089, nativeUseDoorKey },
    { 0x0f6, nativeOfferUseIf },
    { 0x11b, nativeCanOpenDoor },
    { 0x11c, nativeCanCloseDoor },
    { 0x11d, nativeCanLockDoor },
    { 0x12c, nativeCanUnlockDoor },
    { 0x136, nativeCanUse },
    { 0x14f, nativeNotice },
    { 0x15b, nativeDoorUsed },
    { 0x231, nativeKeyWorksFrom },
};

// 0x1a7 specialAllowed(name), and the same with its name in the ROM: may the
// target's character start that special action now.
// PC: a target with no character says no.
// FS2 0x001b2280
// callcheck: readStringArg, isSpecialAllowed

static const int *nativeSpecialAllowed(ScriptRunner *self, int *acc, const int *code)
{
    const char *name = nullptr;
    code = self->readStringArg(&name, code);
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->isSpecialAllowed(name) ? 1 : 0;
    return code;
}

template <const char *const &kName>
static const int *nativeSpecialAllowedOf(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->isSpecialAllowed(kName) ? 1 : 0;
    return code;
}

// FS2 0x001b2810
extern const char *const kSpecialElevatorB1 = "ElevatorB1";
// FS2 0x001b2750
extern const char *const kSpecialElevatorB3 = "ElevatorB3";
// FS2 0x001b2690
extern const char *const kSpecialElevatorBxB3 = "ElevatorBxB3";
// FS2 0x001b25d0
extern const char *const kSpecialElevatorBxB1 = "ElevatorBxB1";


// ──────────────────────────────────────────────────────────────────────
// Natives: what F_PC01 and Root ask on stage 1.

// 0x3a seenEnemy(): the number of the nearest enemy the target sees.
// FS2 0x001cbf20
// callcheck: getSeenEnemyId

static const int *nativeSeenEnemy(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr ? who->getSeenEnemyId() : 0;
    return code;
}

// 0x3e isBitten(): an enemy holds the target. Runner slot +0xec.
// FS2 0x001cbc10
// callcheck: hasBiter

static const int *nativeIsBitten(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->hasBiter();
    return code;
}

// 0x3f distanceTo(name): to the object, or to the spawn point P_SPWnn; 0
// with an unset one. Runner slot +0xf0, the object's +0x228.
// FS2 0x001cba40
// callcheck: readObjectArg, parseSpawnId, currentMission, distanceToPoint

static const int *nativeDistanceTo(ScriptRunner *self, int *acc, const int *code)
{
    StageObject *me = self->getTarget()->getScriptObject();
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *other = (StageObject *)scriptLabelCell(label);
    int distance = 0;
    if (other != nullptr) {
        if (me != nullptr)
            distance = (int)me->distanceTo(other);
    } else {
        const int id = parseSpawnId(label);
        StageMission *mission = gStageScene->currentMission();
        if (id >= 0 && me != nullptr && mission->spawnPoints[id].state != 0)
            distance = (int)me->distanceToPoint(mission->spawnPoints[id].position);
    }
    *acc = distance;
    return next;
}

// 0x49 carries(name): the target has the item. Runner slot +0x108.
// FS2 0x001cb010
// callcheck: readNameArg, FUN_0021bee0

static const int *nativeCarries(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    *acc = who != nullptr && who->findItem(name) != nullptr;
    return next;
}

// The sound of natives 0x6b and 0xb6: "NULL" plays it flat, an object at its
// place (kinds 1..6 with a model hang it on bn_top), and what can hear there
// reports it. Runner slot +0x148.
// PC: a name that is no object comes from the effect definitions' places
// (0x190650, unwritten): it stays silent here.
static void playSoundFrom(ScriptRunner *target, const char *sound, const char *label, float volume)
{
    static const char kTopBone[] = "bn_top";    /* data 0x304b50 */
    static const sceVu0FVECTOR kOrigin = { 0.0f, 0.0f, 0.0f, 1.0f };    /* data 0x2f7dc0 */
    constexpr int kModelKindFirst = 1;
    constexpr int kModelKindLast  = 6;
    if (strcmp(label, kNoObjectName) == 0) {
        gStageScene->sound->playNamed(sound, volume);
        return;
    }
    StageObject *me = target->getScriptObject();
    StageObject *object = (StageObject *)scriptLabelCell(label);
    if (me == nullptr)
        return;
    if (object == nullptr) {
        // No object by that name: an effect's definition may give the place.
        const float *offset = gStageScene->nodeTree->effectOffset(label);
        if (offset != nullptr) {
            const sceVu0FVECTOR place = {offset[0], offset[1], offset[2], kOrigin[3]};
            me->playNamedHeard(sound, place, nullptr);
        }
        return;
    }
    const int kind = object->getKind();
    if (kind >= kModelKindFirst && kind <= kModelKindLast && object->m_model != nullptr) {
        BoneMap::Node *node = object->m_model->boneMap.find(kTopBone);
        if (node != nullptr) {
            me->playNamedHeard(sound, kOrigin, node->value);
            return;
        }
    }
    me->playNamedHeard(sound, object->getWorldMatrix()[3], nullptr);
}

// 0x6b playSoundAt(sound, object).
// FS2 0x001c84b0
// callcheck: readNameArg, readObjectArg, strcmp, playNamed, getKind, getKind, find, FUN_00190650, getMatrix

static const int *nativePlaySoundAt(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *sound;
    const char *label;
    const int *next = self->readNameArg(&sound, code);
    next = self->readObjectArg(&label, next);
    playSoundFrom(target, sound, label, 1.0f);
    return next;
}

// 0x94 pointAt(id): the target's script point (+0x720) becomes the place of
// the character numbered id; 0 for a number with nobody. The object's
// slot +0x2d8.
// FS2 0x001c62f0
// callcheck: currentMission, FUN_00179ad0

static const int *nativePointAt(ScriptRunner *self, int *acc, const int *code)
{
    const int id = self->popped(1)[0];
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageMission *mission = gStageScene->currentMission();
    StageObject *other = id > 0 && id <= mission->characterCount ? mission->characters[id - 1] : nullptr;
    if (who == nullptr || other == nullptr) {
        *acc = 0;
        return code;
    }
    who->setScriptPoint(other->m_xform.position);
    *acc = 1;
    return code;
}

// 0xac playMusic(name)
// FS2 0x001c40f0
// callcheck: readNameArg, playMusic

static const int *nativePlayMusic(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    gStageScene->sound->playMusic(0, name, 0);
    return next;
}

// 0xb1 playModelMotion(object, name): on the object's model.
// FS2 0x001c3c30
// callcheck: readObjectArg, readNameArg, playNamed

static const int *nativePlayModelMotion(ScriptRunner *self, int *, const int *code)
{
    const char *label;
    const char *name;
    const int *next = self->readObjectArg(&label, code);
    next = self->readNameArg(&name, next);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    object->m_model->playNamed(0, name, true, false, kNativeMotionFlags, 0);
    return next;
}

// 0xb5 isRunningMotion(): runner slot +0x128.
// FS2 0x001c38a0
// callcheck: isRunningMotion

static const int *nativeIsRunningMotion(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->isRunningMotion();
    return code;
}

// 0xcf hasPartner()
// FS2 0x001c1530

static const int *nativeHasPartner(ScriptRunner *, int *acc, const int *code)
{
    *acc = gStageScene->partner != nullptr;
    return code;
}

// 0x129 setMoveScale(percent): negative leaves it.
// FS2 0x001bb6c0
// callcheck: setDefaultMoveScale

static const int *nativeSetMoveScale(ScriptRunner *self, int *, const int *code)
{
    const int percent = self->popped(1)[0];
    if (percent >= 0)
        gStageScene->setDefaultMoveScale((float)percent / 100.0f);
    return code;
}

// Every active object of the group called name stops and hides; answers
// how many.
// FS2 0x001d1150
// callcheck: find, setActive, setShown

static int hideActiveNamed(int group, const char *name)
{
    StageNameNode *node = StageNameNode::find(gStageScene->objects->m_names[group].m_root, name);
    int hidden = 0;
    if (node == nullptr || node->value == nullptr)
        return hidden;
    for (StageObject **slot = node->value->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *object = *slot;
        if (object->m_active == 0)
            continue;
        object->m_tree->setActive(false);
        object->m_tree->setShown(false);
        hidden++;
    }
    return hidden;
}

// 0x133 hide(name): group 1's.
// FS2 0x001babb0
// callcheck: readNameArg, FUN_001d1150

static const int *nativeHideNamed(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    hideActiveNamed(StageObjGroup_1, name);
    return next;
}

// 0x1c7 setNotice(index, frames, arg, level, text): what showNotice posts.
// FS2 0x001af120
// callcheck: readCellArg, currentMission, setNotice

static const int *nativeSetNotice(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(4);
    void *text;
    const int *next = self->readCellArg(&text, code);
    gStageScene->currentMission()->setNotice(args[0], (const u_short *)text, args[1], args[2], args[3]);
    return next;
}

// 0x1fa setBlockRange(range)
// FS2 0x001abb60
// callcheck: currentMission

static const int *nativeSetBlockRange(ScriptRunner *self, int *, const int *code)
{
    const int range = self->popped(1)[0];
    gStageScene->currentMission()->blockRange = (float)range;
    return code;
}

// The notices a script posts; the plain ones show for kDefaultNoticeFrames.

// 0x46 message(text)
// FS2 0x001cb330
// callcheck: readCellArg, FUN_00207bc0

static const int *nativeMessage(ScriptRunner *self, int *, const int *code)
{
    void *text;
    const int *next = self->readCellArg(&text, code);
    gStageScene->pauseState->addMessage((const u_short *)text, kDefaultNoticeFrames, 0);
    return next;
}

// 0x7d messageFor(frames, text)
// FS2 0x001c76b0
// callcheck: readCellArg, FUN_00207bc0

static const int *nativeMessageFor(ScriptRunner *self, int *, const int *code)
{
    const int frames = self->popped(1)[0];
    void *text;
    const int *next = self->readCellArg(&text, code);
    gStageScene->pauseState->addMessage((const u_short *)text, frames, 0);
    return next;
}

// 0x124 messageAfter(frames, delay, text)
// FS2 0x001bbb50
// callcheck: readCellArg, FUN_00207bc0

static const int *nativeMessageAfter(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    void *text;
    const int *next = self->readCellArg(&text, code);
    gStageScene->pauseState->addMessage((const u_short *)text, args[0], args[1]);
    return next;
}

// 0x6a hint(text)
// FS2 0x001c87f0
// callcheck: readCellArg, FUN_00207cf0

static const int *nativeHint(ScriptRunner *self, int *, const int *code)
{
    void *text;
    const int *next = self->readCellArg(&text, code);
    gStageScene->pauseState->addHint((const u_short *)text, kDefaultNoticeFrames, 0);
    return next;
}

// 0x116 hintFor(frames, text)
// FS2 0x001bc9e0
// callcheck: readCellArg, FUN_00207cf0

static const int *nativeHintFor(ScriptRunner *self, int *, const int *code)
{
    const int frames = self->popped(1)[0];
    void *text;
    const int *next = self->readCellArg(&text, code);
    gStageScene->pauseState->addHint((const u_short *)text, frames, 0);
    return next;
}

// 0x125 hintAfter(frames, delay, text)
// FS2 0x001bba50
// callcheck: readCellArg, FUN_00207cf0

static const int *nativeHintAfter(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    void *text;
    const int *next = self->readCellArg(&text, code);
    gStageScene->pauseState->addHint((const u_short *)text, args[0], args[1]);
    return next;
}

// 0x1aa clearPinned()
// FS2 0x001b2010
// callcheck: FUN_00207490

static const int *nativeClearPinned(ScriptRunner *, int *acc, const int *code)
{
    *acc = gStageScene->pauseState->clearPinned();
    return code;
}

static const ScriptNativeBinding kNatives[] = {
    { 0x046, nativeMessage },
    { 0x07d, nativeMessageFor },
    { 0x124, nativeMessageAfter },
    { 0x06a, nativeHint },
    { 0x116, nativeHintFor },
    { 0x125, nativeHintAfter },
    { 0x1aa, nativeClearPinned },
    { 0x03a, nativeSeenEnemy },
    { 0x03e, nativeIsBitten },
    { 0x03f, nativeDistanceTo },
    { 0x049, nativeCarries },
    { 0x06b, nativePlaySoundAt },
    { 0x094, nativePointAt },
    { 0x0ac, nativePlayMusic },
    { 0x0b1, nativePlayModelMotion },
    { 0x0b5, nativeIsRunningMotion },
    { 0x0cf, nativeHasPartner },
    { 0x129, nativeSetMoveScale },
    { 0x133, nativeHideNamed },
    { 0x1c7, nativeSetNotice },
    { 0x1fa, nativeSetBlockRange },

    { 0x19f, nativeSpecialAllowedOf<kSpecialElevatorB1> },
    { 0x1a0, nativeSpecialAllowedOf<kSpecialElevatorB3> },
    { 0x1a1, nativeSpecialAllowedOf<kSpecialElevatorBxB3> },
    { 0x1a2, nativeSpecialAllowedOf<kSpecialElevatorBxB1> },
    { 0x1a7, nativeSpecialAllowed },
    { 0x0f7, nativeSetUseKind },
    { 0x0f8, nativeEnableUseKind },
    { 0x04e, nativeFadeIn },
    { 0x053, nativeMenuMode },
    { 0x064, nativeScriptView },
    { 0x070, nativeSetPlayer },
    { 0x138, nativeMissionValue },
    { 0x139, nativeMoveScale },
    { 0x155, nativeSimpleEventNow },
    { 0x144, nativeDemoPosition },
    { 0x145, nativeDemoBind },
    { 0x148, nativeDemoMotion },
    { 0x159, nativeDemoCut },
    { 0x163, nativeDemoFade },
    { 0x164, nativeDemoFadeOut },
    { 0x15f, nativeFileEvent },
    { 0x160, nativeFileEventDelayed },
    { 0x161, nativeSimpleEvent },
    { 0x162, nativeSimpleEventDelayed },
    { 0x1f2, nativePendingEvent },
    { 0x026, nativeSetPosition },
    { 0x027, nativeSetRotation },
    { 0x02a, nativeLightColor },
    { 0x02b, nativeLightRadiusSquared },
    { 0x02c, nativeLightConeAngle },
    { 0x1b3, nativeLightFlag240 },
    { 0x1b4, nativeLightFlag248 },
    { 0x1b6, nativeClearColor },
    { 0x1b7, nativeAmbient },
    { 0x1b8, nativeLight },
    { 0x1b9, nativeModelAmbient },
    { 0x1ba, nativeModelLight },
    { 0x1bb, nativeFog },
    { 0x1bc, nativeFarFog0 },
    { 0x1bd, nativeTransitionRange },
    { 0x1be, nativeTransitionFrames },
    { 0x1c2, nativeAddSetKeys },
    { 0x1c3, nativeSetSetLoop },
    { 0x1d4, nativeIsActive },
    { 0x1df, nativeModelAmbientSb },
    { 0x1e0, nativeModelColorSb },
    { 0x1e1, nativeTransitionSbFrames },
    { 0x1e2, nativeLightFadeFrames },
    { 0x1e3, nativeLightValue244 },
    { 0x1e4, nativeLightModelColors },
    { 0x1e5, nativeLightCastsShadow },
    { 0x203, nativeLightRadius },
    { 0x20c, nativeLightEnabled },
    { 0x217, nativeFarFog },
    { 0x229, nativeLightValue2d8 },
};

// ──────────────────────────────────────────────────────────────────────
// The rest of stage 1's object script (work/dump/script0.bin), then what sits
// beside them.

// The script's labels hold item texts from file version 0.4 on: the three
// scripts of a stage 1 run have 0.4.
// FS2 0x0029eff0
// callcheck: checkedBlock

static bool scriptHasLabelTexts()
{
    if (gStageScene->scripts[0] == nullptr)
        return false;
    const ScriptBlock *block = ((const ScriptEntry *)gStageScene->scripts[0]->entry(0))->checkedBlock();
    return block != nullptr && block->versionMajor == 0 && block->versionMinor >= 4;
}

// 0x47 placeAt(object): the target takes the object's turn and place.
// FS2 0x001cb1f0
// callcheck: readObjectArg

static const int *nativePlaceAt(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    // PC: the ROM reads low memory with nobody behind the label.
    if (object == nullptr)
        return next;
    target->scriptSetRotation(object->m_xform.rotation);
    target->scriptSetPosition(object->m_xform.position);
    return next;
}

// 0x4c takeItem(name), 0x4d takeItems(count, name): the target character loses them.
// FS2 0x001cad40, 0x001cac50
// callcheck: readNameArg, takeNamedItems, readNameArg, takeNamedItems

static const int *nativeTakeItem(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    // PC: the ROM has no test: a null character reads low memory.
    StageCharacter *character = target->getScriptCharacter();
    if (character != nullptr)
        character->takeNamedItems(name, 1, 0);
    return next;
}

static const int *nativeTakeItems(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int count = self->popped(1)[0];
    const char *name;
    const int *next = self->readNameArg(&name, code);
    // PC: as nativeTakeItem.
    StageCharacter *character = target->getScriptCharacter();
    if (character != nullptr)
        character->takeNamedItems(name, count, 0);
    return next;
}

// 0x4f fadeOut(frames): to black, once the scene takes input; waits till then.
// FS2 0x001ca9a0
// callcheck: isInputBlocked, start, scriptYield

static const int *nativeFadeOut(ScriptRunner *self, int *, const int *code)
{
    constexpr int Native = 0x4f;
    const int frames = self->popped(1)[0];
    takeScriptYield();
    if (gStageScene->isInputBlocked())
        scriptYield(Native, 0);
    else
        gStageScene->fade->start((float)frames / kScriptFramesPerSecond, 1.0f, 0.0f);
    return code;
}

// 0x52 clearStage(): the stage's end, once the scene is free.
// FS2 0x001ca610
// callcheck: isBusy, startClear, scriptYield

static const int *nativeClearStage(ScriptRunner *, int *, const int *code)
{
    constexpr int Native = 0x52;
    takeScriptYield();
    if (gStageScene->isBusy())
        scriptYield(Native, 0);
    else
        gStageScene->startClear();
    return code;
}

// 0x5b spawnEffect(name): its handle.
// FS2 0x001c9e50
// callcheck: readNameArg, spawn

static const int *nativeSpawnEffect(ScriptRunner *self, int *acc, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    *acc = gStageScene->effects->spawn(name, nullptr, nullptr, nullptr);
    return next;
}

// 0x5f stopEffect(handle): false when it was gone.
// FS2 0x001c92d0
// callcheck: stop

static const int *nativeStopEffect(ScriptRunner *self, int *acc, const int *code)
{
    const int handle = self->popped(1)[0];
    *acc = handle >= 0 && gStageScene->effects->stop(handle);
    return code;
}

// 0x60 stopEffectIn(handle, frames): at least one frame.
// FS2 0x001c9190
// callcheck: find, setStopFrames

static const int *nativeStopEffectIn(ScriptRunner *self, int *acc, const int *code)
{
    constexpr int kLeastFrames = 1;     // sdata 0x31a9a4
    const int *args = self->popped(2);
    StageParticle *effect = gStageScene->effects->find(args[0]);
    if (effect != nullptr)
        effect->setStopFrames(args[1] > kLeastFrames ? args[1] : kLeastFrames);
    *acc = 0;
    return code;
}

// 0x6f isCrouching().
// FS2 0x001c8290

static const int *nativeIsCrouching(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptIsCrouching();
    return code;
}

// 0x71 setPartner(name): nobody by that name clears it.
// FS2 0x001c80b0
// callcheck: readNameArg, findCharacter

static const int *nativeSetPartner(ScriptRunner *self, int *, const int *code)
{
    const char *name = nullptr;
    const int *next = self->readNameArg(&name, code);
    // PC: as nativeSetPlayer.
    if (name == nullptr)
        return next;
    gStageScene->partner = gStageScene->objects->findCharacter(name, 0);
    return next;
}

// 0x7a addMenuLine(kind, keep, text): text a cell. Below 0 the kind is the
// target's own (StageObject::scriptAddMenuLine).
// FS2 0x001c78c0
// callcheck: readCellArg

static const int *nativeAddMenuLine(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(2);
    void *text;
    const int *next = self->readCellArg(&text, code);
    target->scriptAddMenuLine((const u_short *)text, args[0], args[1] == 0);
    return next;
}

// 0x83 isInReach(object).
// FS2 0x001c7300
// callcheck: readObjectArg, isInReachOf

static const int *nativeIsInReach(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    StageCharacter *character = target->getScriptCharacter();
    // PC: both null reads in the ROM.
    *acc = object != nullptr && character != nullptr && character->isInReachOf(object);
    return next;
}

// 0xa8 hideLoose(name): group 3's that hang from nothing.
// FS2 0x001c48b0
// callcheck: readNameArg, findAll3, setActive, setShown

static const int *nativeHideLoose(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageObjArray *list = gStageScene->objects->findAll3(name);
    for (StageObject **slot = list->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *object = *slot;
        if (object->m_parent == nullptr && object->m_active != 0) {
            object->m_tree->setActive(false);
            object->m_tree->setShown(false);
        }
    }
    return next;
}

// 0xad playVoice(name).
// FS2 0x001c4050
// callcheck: readNameArg, playVoice

static const int *nativePlayVoice(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    // PC: as nativeTakeItem.
    StageCharacter *character = target->getScriptCharacter();
    if (character != nullptr)
        character->playVoice(name);
    return next;
}

// 0xaf noiseFrom(loudness, object): the object is heard.
// FS2 0x001c3e70
// callcheck: readObjectArg

static const int *nativeNoiseFrom(ScriptRunner *self, int *, const int *code)
{
    const int loudness = self->popped(1)[0];
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    // PC: as nativePlaceAt.
    if (object != nullptr)
        object->makeNoise((float)loudness);
    return next;
}

// 0xb4 shout(loudness): 0 without a character.
// FS2 0x001c3930
// callcheck: tryShout

static const int *nativeShout(ScriptRunner *self, int *acc, const int *code)
{
    const int loudness = self->popped(1)[0];
    StageCharacter *character = self->getTarget()->getScriptCharacter();
    *acc = character != nullptr && character->tryShout((float)loudness);
    return code;
}

// 0x126 purpose(frames, delay, label): the label's item text, or the label
// itself as the text.
// FS2 0x001bb910
// callcheck: readLabelArg, scriptHasLabelTexts, addPurpose

static const int *nativePurpose(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    const char *label;
    const int *next = self->readLabelArg(&label, code);
    const void *text = label;
    if (scriptHasLabelTexts() && (label[0] == '_' || label[1] == '@'))
        text = scriptLabelCell(label);
    gStageScene->pauseState->addPurpose((const u_short *)text, args[0], args[1]);
    return next;
}

// 0x13c jackRange(units), 0x13d jackAngle(degrees): the sight-jack's.
// FS2 0x001ba300, 0x001ba230

static const int *nativeJackRange(ScriptRunner *self, int *, const int *code)
{
    gStageScene->party->setJackRange((float)self->popped(1)[0]);
    return code;
}

static const int *nativeJackAngle(ScriptRunner *self, int *, const int *code)
{
    constexpr float kRadiansPerDegree = 0.017453292f;
    gStageScene->party->setJackAngle((float)self->popped(1)[0] * kRadiansPerDegree);
    return code;
}

// 0x20f crittersAt(kind, count, range, object).
// FS2 0x001aa3a0
// callcheck: readObjectArg, spawnCritters

static const int *nativeCrittersAt(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(3);
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    // PC: as nativePlaceAt.
    if (object != nullptr)
        gStageScene->nodeTree->spawnCritters(object->m_xform.position, (float)args[2], args[0], args[1]);
    return next;
}

// The most used of the rest, by sssall.py's count over the 434 scripts on the
// disc. 0x2f is what gives a brain its scripts.

// 0x2f setBrainEntry(kind, entry): entry a word, into the second script's
// table. On the running script, not its target.
// FS2 0x001ccdf0

static const int *nativeSetBrainEntry(ScriptRunner *self, int *, const int *code)
{
    const int kind = self->popped(1)[0];
    self->scriptSetBrainEntry(kind, (const ScriptEntry *)gStageScene->scripts[1]->entry(*code));
    return code + 1;
}

// 0x31 heardKind().
// FS2 0x001ccc50

static const int *nativeHeardKind(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptHeardKind();
    return code;
}

// 0x3c shoutNow(loudness): as 0xb4; the ROM has no null test here.
// FS2 0x001cbd90
// callcheck: tryShout

static const int *nativeShoutNow(ScriptRunner *self, int *acc, const int *code)
{
    return nativeShout(self, acc, code);
}

// 0xb9 noPathTo(x, y, z): true when the route has no way there.
// FS2 0x001c3110
// callcheck: measurePath

static const int *nativeNoPathTo(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(3);
    sceVu0FVECTOR point = { (float)args[0], (float)args[1], (float)args[2], 1.0f };
    StageCharacter *character = self->getTarget()->getScriptCharacter();
    *acc = !(character != nullptr && character->measurePath(character->m_xform.position, point) >= 0.0f);
    return code;
}

// 0xbb stopMoving(): 0 without a character.
// FS2 0x001c2fd0
// callcheck: stopMoving

static const int *nativeStopMoving(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *character = self->getTarget()->getScriptCharacter();
    *acc = character != nullptr && character->stopMoving();
    return code;
}

// 0xbc setBiteCandidateOf(number): a character by the mission's number; 0xbd
// setBiteCandidate(name): by name, "NULL" clears it, nobody by that name
// leaves it.
// FS2 0x001c2e50, 0x001c2cf0
// callcheck: currentMission, readNameArg, strcmp, findCharacter

static const int *nativeSetBiteCandidateOf(ScriptRunner *self, int *, const int *code)
{
    const int number = self->popped(1)[0];
    StageCharacter *character = self->getTarget()->getScriptCharacter();
    if (character == nullptr)
        return code;
    StageMission *mission = gStageScene->currentMission();
    if (number >= 1 && number <= mission->characterCount && mission->characters[number - 1] != nullptr)
        character->m_biteCandidate = mission->characters[number - 1];
    return code;
}

static const int *nativeSetBiteCandidate(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *character = target->getScriptCharacter();
    if (character == nullptr)
        return next;
    if (strcmp(name, kNoObjectName) == 0) {
        character->m_biteCandidate = nullptr;
    } else {
        StageObject *who = gStageScene->objects->findCharacter(name, 0);
        if (who != nullptr)
            character->m_biteCandidate = who;
    }
    return next;
}

// 0xc3 goToHeard(): the script's point becomes where the noise came from.
// FS2 0x001c2500
// callcheck: set

static const int *nativeGoToHeard(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *character = self->getTarget()->getScriptCharacter();
    int heard = 0;
    if (character != nullptr && character->m_point6e0.isSet != 0) {
        character->m_scriptPoint.set(character->m_point6e0.point);
        heard = 1;
    }
    *acc = heard;
    return code;
}

// 0xca pathLength(): of the last path measured.
// FS2 0x001c1b10

static const int *nativePathLength(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *character = self->getTarget()->getScriptCharacter();
    *acc = character != nullptr ? (int)character->m_pathLength : 0;
    return code;
}

// 0xde modulo(a, b).
// FS2 0x001c0600

static const int *nativeModulo(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(2);
    // PC: the EE's div leaves junk; the host would trap.
    *acc = args[1] != 0 ? args[0] % args[1] : 0;
    return code;
}

// 0xf4 distanceToXyz(x, y, z).
// FS2 0x001be9b0

static const int *nativeDistanceToXyz(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(3);
    StageObject *me = self->getTarget()->getScriptObject();
    // PC: the ROM reads the runner's object without asking; a bare runner has none.
    if (me == nullptr) {
        *acc = 0;
        return code;
    }
    const float dx = (float)args[0] - me->m_xform.position[0];
    const float dy = (float)args[1] - me->m_xform.position[1];
    const float dz = (float)args[2] - me->m_xform.position[2];
    *acc = (int)sqrtf(dz * dz + dx * dx + dy * dy);
    return code;
}

// 0xf5 holdsWeapon(name): the long weapon's name.
// FS2 0x001be8e0
// callcheck: readNameArg, holdsWeapon

static const int *nativeHoldsWeapon(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *character = target->getScriptCharacter();
    *acc = character != nullptr && character->holdsWeapon(name);
    return next;
}

// 0x15d overlay(entry): the second script's entry over what the brain runs.
// On the running script, not its target.
// FS2 0x001b7b10
// callcheck: startOverlay

static const int *nativeOverlay(ScriptRunner *self, int *acc, const int *code)
{
    constexpr int kScriptOverlay = 1;
    StageCharacter *character = self->getScriptCharacter();
    // PC: the ROM has no test.
    *acc = character != nullptr && character->m_brain != nullptr
        && character->m_brain->startOverlay(kScriptOverlay,
                                            (const ScriptEntry *)gStageScene->scripts[1]->entry(*code));
    return code + 1;
}

// 0x16a isPlayerDriving(): the player's two car modes.
// FS2 0x001b65b0
// callcheck: isMode1, isMode2

static const int *nativeIsPlayerDriving(ScriptRunner *, int *acc, const int *code)
{
    StageCharacter *player = gStageScene->player;
    // PC: with no player the ROM reads low memory.
    *acc = player != nullptr && (player->isMode1() || player->isMode2());
    return code;
}

// Stage 1's object script, the effect natives beside 0x5b, and the most used.
static const ScriptNativeBinding kStageNatives[] = {
    { 0x047, nativePlaceAt },
    { 0x04c, nativeTakeItem },
    { 0x04d, nativeTakeItems },
    { 0x04f, nativeFadeOut },
    { 0x052, nativeClearStage },
    { 0x05b, nativeSpawnEffect },
    { 0x05f, nativeStopEffect },
    { 0x060, nativeStopEffectIn },
    { 0x06f, nativeIsCrouching },
    { 0x071, nativeSetPartner },
    { 0x07a, nativeAddMenuLine },
    { 0x083, nativeIsInReach },
    { 0x0a8, nativeHideLoose },
    { 0x0ad, nativePlayVoice },
    { 0x0af, nativeNoiseFrom },
    { 0x0b4, nativeShout },
    { 0x126, nativePurpose },
    { 0x13c, nativeJackRange },
    { 0x13d, nativeJackAngle },
    { 0x20f, nativeCrittersAt },
    { 0x02f, nativeSetBrainEntry },
    { 0x031, nativeHeardKind },
    { 0x03c, nativeShoutNow },
    { 0x0b9, nativeNoPathTo },
    { 0x0bb, nativeStopMoving },
    { 0x0bc, nativeSetBiteCandidateOf },
    { 0x0bd, nativeSetBiteCandidate },
    { 0x0c3, nativeGoToHeard },
    { 0x0ca, nativePathLength },
    { 0x0de, nativeModulo },
    { 0x0f4, nativeDistanceToXyz },
    { 0x0f5, nativeHoldsWeapon },
    { 0x15d, nativeOverlay },
    { 0x16a, nativeIsPlayerDriving },
};

// ──────────────────────────────────────────────────────────────────────
// Natives: what the stages' scripts ask most. A number is 1.. in the
// mission's character list; the ROM repeats that test in each.

// PC: the ROM reads the answer of a native on a bare runner off low memory;
// here each answers 0.
static StageCharacter *scriptCharacterAt(int number)
{
    StageMission *mission = gStageScene->currentMission();
    if (number < 1 || number > mission->characterCount)
        return nullptr;
    return static_cast<StageCharacter *>(mission->characters[number - 1]);
}

// Adds to a foe's weight, never below 0. An inline in the ROM's natives,
// whose clamp reads a zero from data (0x31a9a8, 0x31a9ac, 0x31a9b0).
static void addFoeWeightTo(StageCharacter *who, StageCharacter *foe, int amount)
{
    const int weight = who->getFoeWeight(foe) + amount;
    who->setFoeWeight(foe, weight > 0 ? weight : 0);
}

// A bit of the frame's hearing (stepStatus).
static int heardBit(ScriptRunner *self, int bit)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    return who != nullptr && (who->m_scriptValues[kScriptValueHeard] & bit) != 0;
}

// 0x22 spawnNamed(name).
// FS2 0x001ce0e0
// callcheck: readNameArg, spawnNamed

static const int *nativeSpawnNamed(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    gStageScene->nodeTree->spawnNamed(name, 0, 0);
    return next;
}

// 0x23, 0x24, 0x25: where the target stands, one axis each.
// FS2 0x001ce050

static const int *nativePosX(ScriptRunner *self, int *acc, const int *code)
{
    *acc = (int)self->getTarget()->scriptGetPosX();
    return code;
}

// FS2 0x001cdfc0
static const int *nativePosY(ScriptRunner *self, int *acc, const int *code)
{
    *acc = (int)self->getTarget()->scriptGetPosY();
    return code;
}

// FS2 0x001cdf30
static const int *nativePosZ(ScriptRunner *self, int *acc, const int *code)
{
    *acc = (int)self->getTarget()->scriptGetPosZ();
    return code;
}

// 0x40 heightTo(number), 0x41 heightTo(name). Runner slot +0xf4.
// FS2 0x001cb8d0
// callcheck: readObjectArg, heightDifference, currentMission (scriptCharacterAt)

static const int *nativeHeightToNumber(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *other = scriptCharacterAt(self->popped(1)[0]);
    StageObject *me = self->getTarget()->getScriptObject();
    *acc = me != nullptr && other != nullptr ? (int)me->heightDifference(other) : 0;
    return code;
}

// FS2 0x001cb7b0
static const int *nativeHeightToName(ScriptRunner *self, int *acc, const int *code)
{
    StageObject *me = self->getTarget()->getScriptObject();
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *other = (StageObject *)scriptLabelCell(label);
    *acc = me != nullptr && other != nullptr ? (int)me->heightDifference(other) : 0;
    return next;
}

// 0x4a reload(rounds). Runner slot +0x10c.
// FS2 0x001caf30

static const int *nativeReload(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptReload(self->popped(1)[0]);
    return code;
}

// 0x7f: drops its three arguments and its string; nothing else in retail.
// FS2 0x001c7590
// callcheck: readStringArg

static const int *nativeNothing7f(ScriptRunner *self, int *, const int *code)
{
    self->dropArgs(3);
    const char *text;
    return self->readStringArg(&text, code);
}

// 0x80 putWeaponAway(). Runner slot +0x158.
// FS2 0x001c7530

static const int *nativePutWeaponAway(ScriptRunner *self, int *, const int *code)
{
    self->getTarget()->scriptPutWeaponAway();
    return code;
}

// 0x8e distanceToAlly(): to the nearest of its side up, -1 none.
// FS2 0x001c6a20
// callcheck: findNearestFoe, distanceTo

static const int *nativeNearestAllyDistance(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageObject *ally = who != nullptr ? who->findNearestFoe(false, false) : nullptr;
    *acc = ally != nullptr ? (int)who->distanceTo(ally) : -1;
    return code;
}

// 0x8f unnoticedFoe(): the nearest foe it sees and gives no weight yet.
// FS2 0x001c6940
// callcheck: getUnnoticedFoe

static const int *nativeUnnoticedFoe(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageObject *foe = who != nullptr ? who->getUnnoticedFoe() : nullptr;
    *acc = foe != nullptr ? foe->m_id : 0;
    return code;
}

// 0x91 nearestFoeDistance(), -1 none. 0x92 nearestFoe(): its number.
// FS2 0x001c6670
// callcheck: getNearestFoeDistance, getNearestFoe

static const int *nativeNearestFoeDistance(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr ? who->getNearestFoeDistance() : -1;
    return code;
}

// FS2 0x001c6590
static const int *nativeNearestFoe(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageObject *foe = who != nullptr ? who->getNearestFoe() : nullptr;
    *acc = foe != nullptr ? foe->m_id : 0;
    return code;
}

// 0x93 heardAlly(), 0xa9 heardFoe(), 0xe2 heardAny().
// FS2 0x001c64a0

static const int *nativeHeardAlly(ScriptRunner *self, int *acc, const int *code)
{
    *acc = heardBit(self, kHeardAlly);
    return code;
}

// FS2 0x001c47c0
static const int *nativeHeardFoe(ScriptRunner *self, int *acc, const int *code)
{
    *acc = heardBit(self, kHeardFoe);
    return code;
}

// FS2 0x001c0240
static const int *nativeHeardAny(ScriptRunner *self, int *acc, const int *code)
{
    *acc = heardBit(self, kHeardAny);
    return code;
}

// 0x95 takeCorpse(). 0x96 lightReach().
// FS2 0x001c6250
// callcheck: takeCorpse, getLightReach

static const int *nativeTakeCorpse(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->takeCorpse();
    return code;
}

// FS2 0x001c61b0
static const int *nativeLightReach(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr ? who->getLightReach() : 0;
    return code;
}

// 0x97 addFoeWeight(amount, name): a character by name, NEAR the nearest
// foe, ALL every enemy up.
// FS2 0x001c5e20
// callcheck: readNameArg, strcmp, findNearestFoe, strcmp, currentMission, getLifeState, isEnemyOf, addFoeWeightTo, findCharacter, isEnemyOf, addFoeWeightTo, FUN_00217be0, FUN_00217c60 (addFoeWeightTo: getFoeWeight, setFoeWeight)

static const int *nativeAddFoeWeightNamed(ScriptRunner *self, int *, const int *code)
{
    static const char kNearest[] = "NEAR";      /* data 0x304bc0 */
    static const char kAll[]     = "ALL";       /* data 0x304bc8 */
    ScriptRunner *target = self->getTarget();
    const int amount = self->popped(1)[0];
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = target->getScriptCharacter();
    if (who == nullptr)
        return next;
    StageObject *foe;
    if (strcmp(kNearest, name) == 0) {
        foe = who->findNearestFoe(true, false);
    } else if (strcmp(kAll, name) == 0) {
        StageMission *mission = gStageScene->currentMission();
        for (int i = 0; i < mission->characterCount; i++) {
            StageCharacter *other = static_cast<StageCharacter *>(mission->characters[i]);
            if (other->m_active != 0 && other->getLifeState() == LifeState_Up && who->isEnemyOf(other))
                addFoeWeightTo(who, other, amount);
        }
        return next;
    } else {
        foe = gStageScene->objects->findCharacter(name, 0);
    }
    if (foe != nullptr && who->isEnemyOf(foe))
        addFoeWeightTo(who, static_cast<StageCharacter *>(foe), amount);
    return next;
}

// 0x98 addFoeWeightInView(amount, limit), 0x99 the ones out of view.
// FS2 0x001c5d40
// callcheck: addFoeWeight

static const int *nativeAddFoeWeightInView(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    if (who != nullptr)
        who->addFoeWeight(args[0], args[1], true);
    return code;
}

// FS2 0x001c5c60
static const int *nativeAddFoeWeightOutOfView(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    if (who != nullptr)
        who->addFoeWeight(args[0], args[1], false);
    return code;
}

// 0x9a bestFoe(): its number.
// FS2 0x001c5b70
// callcheck: getBestFoe, getBestFoe

static const int *nativeBestFoe(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageObject *foe = who != nullptr ? who->getBestFoe() : nullptr;
    *acc = foe != nullptr ? foe->m_id : 0;
    return code;
}

// 0x9b foeWeight(name), 0x9c foeWeight(number).
// FS2 0x001c5a40
// callcheck: readNameArg, findCharacter, getFoeWeight

static const int *nativeFoeWeightNamed(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageObject *foe = gStageScene->objects->findCharacter(name, 0);
    StageCharacter *who = target->getScriptCharacter();
    *acc = who != nullptr && foe != nullptr ? who->getFoeWeight(static_cast<StageCharacter *>(foe)) : 0;
    return next;
}

// FS2 0x001c58b0
// callcheck: currentMission (scriptCharacterAt)
static const int *nativeFoeWeight(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *foe = scriptCharacterAt(self->popped(1)[0]);
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && foe != nullptr ? who->getFoeWeight(foe) : 0;
    return code;
}

// 0xa3 nearestCorpse(): its number. 0xa4 heardWithin(range).
// FS2 0x001c4fe0
// callcheck: findNearestCorpse

static const int *nativeNearestCorpse(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageCharacter *corpse = who != nullptr ? who->findNearestCorpse() : nullptr;
    *acc = corpse != nullptr ? corpse->m_id : 0;
    return code;
}

// FS2 0x001c4ee0
// callcheck: FUN_00219980 (inline here: m_lastNoiseRange)
static const int *nativeHeardWithin(ScriptRunner *self, int *acc, const int *code)
{
    const float range = (float)self->popped(1)[0];
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->m_lastNoiseRange <= range;
    return code;
}

// 0xbf isBitten(number), 0xc0 isBitten(name).
// FS2 0x001c2aa0
// callcheck: isBitten, readObjectArg, isBitten, currentMission (scriptCharacterAt)

static const int *nativeIsBittenNumber(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = scriptCharacterAt(self->popped(1)[0]);
    *acc = who != nullptr && who->isBitten();
    return code;
}

// FS2 0x001c2990
static const int *nativeIsBittenName(ScriptRunner *self, int *acc, const int *code)
{
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageCharacter *who = (StageCharacter *)scriptLabelCell(label);
    *acc = who != nullptr && who->isBitten();
    return next;
}

// 0xc1 forgetFoe(number): 0 every other character up.
// FS2 0x001c2720
// callcheck: getScriptCharacter, currentMission, getLifeState, setFoeWeight

static const int *nativeForgetFoe(ScriptRunner *self, int *, const int *code)
{
    const int number = self->popped(1)[0];
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    if (who == nullptr)
        return code;
    if (number != 0) {
        StageCharacter *foe = scriptCharacterAt(number);
        if (foe != nullptr)
            who->setFoeWeight(foe, 0);
        return code;
    }
    StageMission *mission = gStageScene->currentMission();
    for (int i = 0; i < mission->characterCount; i++) {
        StageCharacter *other = static_cast<StageCharacter *>(mission->characters[i]);
        if (other->m_active != 0 && other->getLifeState() == LifeState_Up && other != who)
            who->setFoeWeight(other, 0);
    }
    return code;
}

// 0xc7 alertOn(name), 0xc8 alertOn(number).
// FS2 0x001c2030
// callcheck: readNameArg, findCharacter, show

static const int *nativeAlertOnName(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageObject *who = gStageScene->objects->findCharacter(name, 0);
    if (who != nullptr)
        gStageScene->alert->show(who);
    return next;
}

// FS2 0x001c1ee0
static const int *nativeAlertOnNumber(ScriptRunner *self, int *, const int *code)
{
    StageCharacter *who = scriptCharacterAt(self->popped(1)[0]);
    if (who != nullptr)
        gStageScene->alert->show(who);
    return code;
}

// 0xdc percentOf(a, b): rounded.
// FS2 0x001c0760

static const int *nativePercentOf(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(2);
    *acc = (args[0] * args[1] + 50) / 100;
    return code;
}

// 0xe1 clamp(value, low, high): what the two std::min / max inlines make.
// FS2 0x001c0330

static const int *nativeClamp(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(3);
    const int low = args[0] < args[1] ? args[1] : args[0];
    *acc = args[2] <= low ? args[2] : low;
    return code;
}

// 0xe4 attackBite(). Runner slot +0xcc.
// FS2 0x001c00a0

static const int *nativeAttackBite(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptAttackBite();
    return code;
}

// 0xe5 distanceTo(number). Runner slot +0xf0.
// FS2 0x001bff30
// callcheck: distanceTo, currentMission (scriptCharacterAt)

static const int *nativeDistanceToNumber(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *other = scriptCharacterAt(self->popped(1)[0]);
    StageObject *me = self->getTarget()->getScriptObject();
    *acc = me != nullptr && other != nullptr ? (int)me->distanceTo(other) : 0;
    return code;
}

// 0xe6 pathToXyz(x, y, z): 1 a way there, 2 none.
// FS2 0x001bfda0
// callcheck: measurePath

static const int *nativePathToXyz(ScriptRunner *self, int *acc, const int *code)
{
    enum { kPathFound = 1, kPathNone = 2 };
    const int *args = self->popped(3);
    sceVu0FVECTOR place = { (float)args[0], (float)args[1], (float)args[2], 1.0f };
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->measurePath(who->m_xform.position, place) >= 0.0f ? kPathFound : kPathNone;
    return code;
}

// 0xea hasLineTo(number). Runner slot +0x114.
// FS2 0x001bf820
// callcheck: currentMission (scriptCharacterAt)

static const int *nativeHasLineTo(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *other = scriptCharacterAt(self->popped(1)[0]);
    // PC: a null other has no head to ask; the base slot's answer.
    *acc = other != nullptr && self->getTarget()->scriptHasLineTo(other);
    return code;
}

// 0xf2 askLift(number): the number is not read past its test.
// FS2 0x001bed00
// callcheck: askLift, currentMission (scriptCharacterAt)

static const int *nativeAskLift(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *other = scriptCharacterAt(self->popped(1)[0]);
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && other != nullptr && who->askLift();
    return code;
}

// 0xf3 addFoeWeight(number, amount).
// FS2 0x001beb20
// callcheck: isEnemyOf, addFoeWeightTo, currentMission, FUN_00217be0, FUN_00217c60 (scriptCharacterAt; addFoeWeightTo)

static const int *nativeAddFoeWeight(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    const int amount = args[1];
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageCharacter *foe = scriptCharacterAt(args[0]);
    if (who != nullptr && foe != nullptr && who->isEnemyOf(foe))
        addFoeWeightTo(who, foe, amount);
    return code;
}

// 0x104 useItem(name), 0x105 useSmoke(), 0x106 putItem(). Slots +0x170,
// +0x16c, +0x17c, void in the ROM: their acc is what v0 held.
// FS2 0x001bd860

static const int *nativeUseItem(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    target->scriptUseNamedItem(name);
    *acc = 0;
    return next;
}

// FS2 0x001bd7d0
static const int *nativeUseSmoke(ScriptRunner *self, int *acc, const int *code)
{
    self->getTarget()->scriptUseSmoke();
    *acc = 0;
    return code;
}

// FS2 0x001bd740
static const int *nativePutItem(ScriptRunner *self, int *acc, const int *code)
{
    self->getTarget()->scriptPutItem();
    *acc = 0;
    return code;
}

// 0x107 seesFlyingItem().
// FS2 0x001bd660

static const int *nativeSeesFlyingItem(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->m_seenFlying != nullptr;
    return code;
}

// 0x108, 0x109, 0x10b: the script's point goes to one of beforeScripts'
// three: the throw's, the landing, the item lying still.
// FS2 0x001bd560
// callcheck: set

static const int *goToWatchedPoint(ScriptRunner *self, int *acc, const int *code,
                                   ScriptPoint StageCharacter::*member)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    int found = 0;
    if (who != nullptr && (who->*member).isSet != 0) {
        who->m_scriptPoint.set((who->*member).point);
        found = 1;
    }
    *acc = found;
    return code;
}

static const int *nativeGoToThrow(ScriptRunner *self, int *acc, const int *code)
{
    return goToWatchedPoint(self, acc, code, &StageCharacter::m_point740);
}

// FS2 0x001bd420
static const int *nativeGoToLanding(ScriptRunner *self, int *acc, const int *code)
{
    return goToWatchedPoint(self, acc, code, &StageCharacter::m_point760);
}

// FS2 0x001bd280
static const int *nativeGoToLyingItem(ScriptRunner *self, int *acc, const int *code)
{
    return goToWatchedPoint(self, acc, code, &StageCharacter::m_point780);
}

// 0x10a seesBurner(). 0x10e seesOpenTrap().
// FS2 0x001bd380
// callcheck: seesBurner, seesOpenTrap

static const int *nativeSeesBurner(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->seesBurner();
    return code;
}

// FS2 0x001bcf50
static const int *nativeSeesOpenTrap(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->seesOpenTrap();
    return code;
}

// 0x10c burnerIn(volume): a burner in use stands in the volume called name.
// FS2 0x001bd080
// callcheck: readNameArg, find7, currentMission, currentMission, getMatrix, contains

static const int *nativeBurnerIn(ScriptRunner *self, int *acc, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageVolume *volume = static_cast<StageVolume *>(gStageScene->objects->find7(name, 0));
    *acc = 0;
    if (volume == nullptr)
        return next;
    const MissionObjectList &burners = gStageScene->currentMission()->burners;
    for (int i = 0; i < burners.count; i++) {
        StageItem *burner = static_cast<StageItem *>(burners.objects[i]);
        if (burner->m_active != 0 && burner->m_parent == nullptr && burner->m_inUse != 0
            && volume->contains(burner->getMatrix()[3])) {
            *acc = 1;
            break;
        }
    }
    return next;
}

// 0x10d setTrap(), 0x110 takeUpTrap(): slots +0x178, +0x180.
// 0x10f canTakeUpTrap(): the allows.
// FS2 0x001bcff0

static const int *nativeSetTrap(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptSetTrap();
    return code;
}

// FS2 0x001bce20
static const int *nativeTakeUpTrap(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptTakeUpTrap();
    return code;
}

// FS2 0x001bceb0
static const int *nativeCanTakeUpTrap(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->m_allows.trapCancel != 0;
    return code;
}

// 0x121 dropLongWeapon(). 0x122 pickUp(): slot +0x190.
// FS2 0x001bbe50
// callcheck: dropLongWeapon

static const int *nativeDropLongWeapon(ScriptRunner *self, int *, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    if (who != nullptr)
        who->dropLongWeapon();
    return code;
}

// FS2 0x001bbdc0
static const int *nativePickUp(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptTakeItem() != nullptr;
    return code;
}

// 0x13e alertCry().
// FS2 0x001ba190
// callcheck: cry

static const int *nativeAlertCry(ScriptRunner *self, int *, const int *code)
{
    gStageScene->alert->cry(self->getTarget()->getScriptCharacter());
    return code;
}

// 0x1bf wasHit(): this frame. 0x1de darkness(): percent of the area's.
// FS2 0x001b0040

static const int *nativeWasHit(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr ? who->m_hitThisFrame : 0;
    return code;
}

// FS2 0x001adaa0
static const int *nativeDarkness(ScriptRunner *self, int *acc, const int *code)
{
    constexpr float kPercent = 100.0f;
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr ? (int)(who->m_areaDark * kPercent) : 0;
    return code;
}

// 0x1dd: 0, whatever is asked.
// FS2 0x001adb70

static const int *nativeZero(ScriptRunner *, int *acc, const int *code)
{
    *acc = 0;
    return code;
}

// 0x1f4 isNearViewTarget(): of the running script, not its target.
// FS2 0x001abfc0
// callcheck: isNearViewTarget

static const int *nativeIsNearViewTarget(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getScriptCharacter();
    *acc = who != nullptr && who->isNearViewTarget();
    return code;
}

// 0x206 canUseSevenKey().
// FS2 0x001aaed0
// callcheck: isInputBlocked, isSpecialAllowed

static const int *nativeCanUseSevenKey(ScriptRunner *, int *acc, const int *code)
{
    static const char kSevenKey[] = "SevenKey"; /* data 0x304ac8 */
    StageCharacter *player = static_cast<StageCharacter *>(scenePlayer());
    *acc = !gStageScene->isInputBlocked() && player->isSpecialAllowed(kSevenKey);
    return code;
}

// 0x207: the number of what m_628 holds.
// FS2 0x001aadb0

static const int *nativeRouteObject(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->m_628 != nullptr ? who->m_628->m_id : 0;
    return code;
}

// 0x20b isStanding(): the allows.
// FS2 0x001aa920

static const int *nativeIsStanding(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr ? who->m_allows.standing : 0;
    return code;
}

// 0x213 isFatal(): the life state says so.
// FS2 0x001aa000
// callcheck: getLifeState

static const int *nativeIsFatal(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->getLifeState() == LifeState_Fatal;
    return code;
}

// Natives 0x2d .. 0x214, by how many scripts call them: sssall.py's order.

// 0x2d switchOn(on): the slot +0x64: a light's tree, a character's lamp.
// FS2 0x001ccf90

static const int *nativeSwitchOn(ScriptRunner *self, int *, const int *code)
{
    const int on = self->popped(1)[0];
    self->getTarget()->cmdSetTreeSwitchedOn(on != 0);
    return code;
}

// What natives 0x32, 0x33 ask of a character: 60 degrees of cone.
constexpr float kScriptViewCone = 1.0471975f;

// Who sees target: the actor through the frame's sight table, a runner with
// none through its own slot.
static bool scriptSeesCharacter(ScriptRunner *target, StageCharacter *other)
{
    StageCharacter *actor = target->getScriptActor();
    if (actor == nullptr)
        return target->scriptCanSee(kScriptViewCone, other);
    float distance = 0.0f;
    return actor->canSeeThisFrame(other, &distance);
}

// 0x32 canSee(number).
// FS2 0x001cca70
// callcheck: canSeeThisFrame, currentMission (scriptCharacterAt)

static const int *nativeCanSeeNumber(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    StageCharacter *other = scriptCharacterAt(self->popped(1)[0]);
    *acc = other != nullptr && scriptSeesCharacter(target, other);
    return code;
}

// 0x33 canSee(name): a character by the sight table, anything else or a
// spawn point (a label with no object: S01..) by the cone at full range.
// FS2 0x001cc710
// callcheck: readObjectArg, canSeeThisFrame, isPointInView, parseSpawnId, currentMission, isPointInView

static const int *nativeCanSeeName(ScriptRunner *self, int *acc, const int *code)
{
    constexpr float kFullRange = 1.0f;
    ScriptRunner *target = self->getTarget();
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    float distance;
    if (object == nullptr) {
        const int id = parseSpawnId(label);
        StageMission *mission = gStageScene->currentMission();
        StageCharacter *who = id >= 0 && mission->spawnPoints[id].state != 0 ? target->getScriptCharacter() : nullptr;
        *acc = who != nullptr
            && who->isPointInView(kScriptViewCone, kFullRange, mission->spawnPoints[id].position, &distance, nullptr);
    } else if (object->getKind() == StageObject_Character) {
        *acc = scriptSeesCharacter(target, static_cast<StageCharacter *>(object));
    } else {
        StageCharacter *who = target->getScriptCharacter();
        *acc = who != nullptr
            && who->isPointInView(kScriptViewCone, kFullRange, object->m_xform.position, &distance, nullptr);
    }
    return next;
}

// 0x50, 0x51 blockPath(name), allowPath(name): every character's route.
// FS2 0x001ca870
// callcheck: readNameArg, currentMission, currentMission, FUN_00218110 (allowPath)

static const int *nativeBlockPath(ScriptRunner *self, int *acc, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageMission *mission = gStageScene->currentMission();
    for (StageObject **it = mission->characters; it != mission->charactersEnd(); it++)
        static_cast<StageCharacter *>(*it)->allowPath(name, false);
    *acc = 1;
    return next;
}

// FS2 0x001ca740
// callcheck: readNameArg, currentMission, currentMission, FUN_00218110 (allowPath)
static const int *nativeAllowPath(ScriptRunner *self, int *acc, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageMission *mission = gStageScene->currentMission();
    for (StageObject **it = mission->characters; it != mission->charactersEnd(); it++)
        static_cast<StageCharacter *>(*it)->allowPath(name, true);
    *acc = 1;
    return next;
}

// 0x54 hasLineTo(name), 0xea's form with a name.
// FS2 0x001ca480
// callcheck: readNameArg, findCharacter

static const int *nativeHasLineToName(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *other = static_cast<StageCharacter *>(gStageScene->objects->findCharacter(name, 0));
    // PC: a null other has no head to ask.
    *acc = other != nullptr && target->scriptHasLineTo(other);
    return next;
}

// 0x57 isCrouchingName(name).
// FS2 0x001ca150
// callcheck: readNameArg, findCharacter

static const int *nativeIsCrouchingName(ScriptRunner *self, int *acc, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = static_cast<StageCharacter *>(gStageScene->objects->findCharacter(name, 0));
    *acc = who != nullptr && who->scriptIsCrouching();
    return next;
}

// 0x59, 0x5a: what the route asks now.
// FS2 0x001c9fb0

static const int *nativeRouteAction12c(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptRouteAction12c();
    return code;
}

// FS2 0x001c9f20
static const int *nativeRouteAction130(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptRouteAction130();
    return code;
}

// An effect hung on one of object's bones; -1 without the bone. The ROM has
// this body twice over: StageCharacter 0x21aa90 and SubSceneModelObject
// 0x2689a0.
// FS2 0x0021aa90
// FS2 0x002689a0
// callcheck: find, spawn

static int spawnEffectOnBone(StageObject *object, const char *name, const char *bone,
                             const float *place = nullptr, const float *turn = nullptr)
{
    BoneMap::Node *node = object->m_model->boneMap.find(bone);
    if (node == nullptr || node->value == nullptr)
        return -1;
    return gStageScene->effects->spawn(name, node->value, place, turn);
}

// 0x5d spawnEffectOn(effect, object, bone): its handle. Bone "NULL" hangs it
// on the object itself, object "NULL" too on the top view.
// FS2 0x001c9800
// callcheck: readNameArg, readObjectArg, readNameArg, strcmp, strcmp, getTopView, spawn, FUN_0021aa90, find, spawn (spawnEffectOnBone)

static const int *nativeSpawnEffectOn(ScriptRunner *self, int *acc, const int *code)
{
    static const char kNone[] = "NULL";     /* data 0x304ba8 */
    const char *name;
    const char *label;
    const char *bone;
    const int *next = self->readNameArg(&name, code);
    next = self->readObjectArg(&label, next);
    next = self->readNameArg(&bone, next);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    if (strcmp(bone, kNone) == 0) {
        StageObject *parent = strcmp(label, kNone) == 0 ? gStageScene->getTopView() : object;
        *acc = gStageScene->effects->spawn(name, parent, nullptr, nullptr);
    } else {
        // PC: the ROM has no test of the cell.
        *acc = object != nullptr ? spawnEffectOnBone(object, name, bone) : -1;
    }
    return next;
}

// 0x82 wakeBrain().
// FS2 0x001c7420
// callcheck: FUN_00219cb0 (wakeBrain)

static const int *nativeWakeBrain(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->wakeBrain();
    return code;
}

// 0x85 climbPoint(name): 0, 1 or 2. 0x86 askLift(name): the name is only tested.
// FS2 0x001c7160
// callcheck: readNameArg, findCharacter, FUN_00219040 (getClimbPoint)

static const int *nativeClimbPoint(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = target->getScriptCharacter();
    StageObject *other = gStageScene->objects->findCharacter(name, 0);
    *acc = who != nullptr && other != nullptr ? who->getClimbPoint(other) : 0;
    return next;
}

// FS2 0x001c7020
// callcheck: readNameArg, findCharacter, askLift
static const int *nativeAskLiftName(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = target->getScriptCharacter();
    StageObject *other = gStageScene->objects->findCharacter(name, 0);
    *acc = who != nullptr && other != nullptr && who->askLift();
    return next;
}

// 0x90 lastHitBy(): a character's this frame's hitter, a map object's or a
// door's toucher: their number, 0 none.
// FS2 0x001c6710

static const int *nativeLastHitBy(ScriptRunner *self, int *acc, const int *code)
{
    constexpr int kMapKindLast = 6;
    StageObject *object = self->getTarget()->getScriptObject();
    int number = 0;
    const int kind = object->getKind();
    if (kind == StageObject_Character) {
        StageObject *by = static_cast<StageCharacter *>(object)->m_hitThisFrameBy;
        number = by != nullptr ? by->m_id : 0;
    } else if (kind > 0 && kind <= kMapKindLast) {
        StageObject *by = static_cast<StageMapObject *>(object)->getTouchedBy();
        number = by != nullptr ? by->m_id : 0;
    }
    *acc = number;
    return code;
}

// The runner's actor or object: where a spawn is measured from.
// FS2 0x001c5800 (the ROM's 0x1c5460 has the same body inline)
// callcheck: missionPickSpawnPoint, missionSpawnPointTaken, missionSpawnAt

static bool spawnNear(ScriptRunner *runner, int strong, int mode)
{
    StageObject *object = runner->getScriptActor();
    if (object == nullptr)
        object = runner->getScriptObject();
    int point = missionPickSpawnPoint(object->m_xform.position, mode);
    return !missionSpawnPointTaken(&point) && missionSpawnAt(&point, strong);
}

// 0x9d spawnAt(name, strong, mode): name RANDOM any point, a spawn point's
// name around it, an object's around it.
// FS2 0x001c55c0
// callcheck: readObjectArg, strcmp, FUN_001c5800, parseSpawnId, FUN_001afd00, FUN_001c5800, currentMission, FUN_001afd10, missionPickSpawnPoint, missionSpawnPointTaken, missionSpawnAt (spawnNear; scriptLabelCell; spawnPoints)

static const int *nativeSpawnAt(ScriptRunner *self, int *acc, const int *code)
{
    static const char kRandom[] = "RANDOM"; /* data 0x304b28 */
    constexpr int kAnyPoint = -1;
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(2);
    const int strong = args[0];
    const int mode = args[1];
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    bool spawned = false;
    if (strcmp(label, kRandom) == 0) {
        StageCharacter *actor = target->getScriptActor();
        spawned = spawnNear(actor != nullptr ? static_cast<ScriptRunner *>(actor) : target, strong, kAnyPoint);
    } else {
        const int id = parseSpawnId(label);
        if (id < 0) {
            StageObject *object = (StageObject *)scriptLabelCell(label);
            if (object != nullptr)
                spawned = spawnNear(object, strong, mode);
        } else {
            int point = missionPickSpawnPoint(gStageScene->currentMission()->spawnPoints[id].position, mode);
            spawned = !missionSpawnPointTaken(&point) && missionSpawnAt(&point, strong);
        }
    }
    *acc = spawned;
    return next;
}

// 0x9e spawnNear(strong, mode).
// FS2 0x001c5460
// callcheck: missionPickSpawnPoint, missionSpawnPointTaken, missionSpawnAt (spawnNear)

static const int *nativeSpawnNear(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(2);
    *acc = spawnNear(self->getTarget(), args[0], args[1]);
    return code;
}

// 0xa5 loseItem(name): one of them goes off stage.
// FS2 0x001c4dd0
// callcheck: readNameArg, findItem, dropItem, setActive, setShown

static const int *nativeLoseItem(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = target->getScriptCharacter();
    StageObject *item = who != nullptr ? who->findItem(name) : nullptr;
    if (item != nullptr) {
        who->dropItem(item);
        item->m_tree->setActive(false);
        item->m_tree->setShown(false);
    }
    return next;
}

// 0xa7 showItems(name): those called so, off stage and on no one, come on.
// FS2 0x001c4a90
// callcheck: readNameArg, findAll3, setActive, setShown

static const int *nativeShowItems(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageObjArray *items = gStageScene->objects->findAll3(name);
    // PC: the ROM has no test of the list.
    if (items == nullptr)
        return next;
    for (StageObject **slot = items->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *item = *slot;
        if (item->m_parent != nullptr || item->m_active != 0)
            continue;
        item->m_tree->setActive(true);
        item->m_tree->setShown(true);
    }
    return next;
}

// 0xb0 playObjectMotion(object, motion): on group 1's object of that name.
// FS2 0x001c3d50
// callcheck: readNameArg, readNameArg, find1, playNamed

static const int *nativePlayObjectMotion(ScriptRunner *self, int *, const int *code)
{
    constexpr u_int kFlags = 0x50;
    const char *object;
    const char *motion;
    const int *next = self->readNameArg(&object, code);
    next = self->readNameArg(&motion, next);
    StageObject *found = gStageScene->objects->find1(object, 0);
    // PC: the ROM has no test.
    if (found != nullptr)
        found->m_model->playNamed(0, motion, false, false, kFlags, 0);
    return next;
}

// 0xc4 searchPoint(): the handle of the search group at hand. 0xc5
// searchPointLast(): its highest number.
// FS2 0x001c2400
// callcheck: FUN_0021c010, getHandle (getNearSearchPoint)

static const int *nativeSearchPoint(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageObject *point = who != nullptr ? who->getNearSearchPoint() : nullptr;
    *acc = point != nullptr ? (int)point->getHandle() : 0;
    return code;
}

// FS2 0x001c2310
// callcheck: FUN_0021c010, FUN_0017ed30 (getNearSearchPoint; getSearchLastIndex)
static const int *nativeSearchPointLast(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageObject *point = who != nullptr ? who->getNearSearchPoint() : nullptr;
    *acc = point != nullptr ? point->getSearchLastIndex() : 0;
    return code;
}

// 0xdd abs(value).
// FS2 0x001c06b0
// callcheck: abs

static const int *nativeAbs(ScriptRunner *self, int *acc, const int *code)
{
    *acc = abs(self->popped(1)[0]);
    return code;
}

// 0xe9 inReach(number): slot +0xfc, a character's weapon reach.
// FS2 0x001bf980
// callcheck: currentMission (scriptCharacterAt)

static const int *nativeInReach(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    StageCharacter *other = scriptCharacterAt(self->popped(1)[0]);
    // PC: a null other has no place to measure.
    *acc = other != nullptr && target->scriptIsFacing(other);
    return code;
}

// 0xff searchPointAt(x, y, z, rank): the handle of the rank-th nearest search
// group; the script point goes there.
// FS2 0x001bde10
// callcheck: FUN_00219210, set, getHandle (pickSearchPoint)

static const int *nativeSearchPointAt(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(4);
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = 0;
    if (who != nullptr) {
        const sceVu0FVECTOR place = { (float)args[0], (float)args[1], (float)args[2], 1.0f };
        StageObject *point = who->pickSearchPoint(place, args[3]);
        if (point != nullptr) {
            who->m_scriptPoint.set(point->m_xform.position);
            *acc = (int)point->getHandle();
        }
    }
    return code;
}

// 0x100 searchPointNext(number): the group's point of that number, for the
// script point; number, 0 none.
// FS2 0x001bdc90
// callcheck: FUN_0021c010, FUN_0017ee30, set (getNearSearchPoint; findSearchPoint)

static const int *nativeSearchPointNext(ScriptRunner *self, int *acc, const int *code)
{
    const int number = self->popped(1)[0];
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = 0;
    if (who != nullptr) {
        StageObject *group = who->getNearSearchPoint();
        StageObject *point = group != nullptr ? group->findSearchPoint(number) : nullptr;
        if (point != nullptr) {
            who->m_scriptPoint.set(point->m_xform.position);
            *acc = number;
        }
    }
    return code;
}

// 0x123 nearWeapon(range): the handle of the nearest weapon it can wield;
// the script point goes there.
// FS2 0x001bbc50
// callcheck: FUN_0021b690, set, getHandle (findLooseWeapon)

static const int *nativeNearWeapon(ScriptRunner *self, int *acc, const int *code)
{
    constexpr u_int kAnyClass = 0xffff;
    const int range = self->popped(1)[0];
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageObject *weapon = who != nullptr ? who->findLooseWeapon((float)range, kAnyClass) : nullptr;
    *acc = 0;
    if (weapon != nullptr) {
        who->m_scriptPoint.set(weapon->m_xform.position);
        *acc = (int)weapon->getHandle();
    }
    return code;
}

// 0x12e swing(): the brain swings at nothing named.
// FS2 0x001bb160

static const int *nativeSwing(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptSwingAt(nullptr);
    return code;
}

// 0x132 showGroup1(name): group 1's objects of that name, those off, come on.
// FS2 0x001bac20
// callcheck: readNameArg, find, setActive, setShown

static const int *nativeShowGroup1(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageNameNode *node = StageNameNode::find(gStageScene->objects->m_names[StageObjGroup_1].m_root, name);
    if (node == nullptr || node->value == nullptr)
        return next;
    for (StageObject **slot = node->value->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *object = *slot;
        if (object->m_active != 0)
            continue;
        object->m_tree->setActive(true);
        object->m_tree->setShown(true);
    }
    return next;
}

// 0x13b leaveQuiet(), 0x17e leave(): the character is done with the stage.
// FS2 0x001ba3c0
// callcheck: FUN_00219420 (leaveQuiet)

static const int *nativeLeaveQuiet(ScriptRunner *self, int *, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    if (who != nullptr)
        who->leaveQuiet();
    return code;
}

// FS2 0x001b5200
// callcheck: FUN_00219510 (leaveLoud)
static const int *nativeLeave(ScriptRunner *self, int *, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    if (who != nullptr)
        who->leaveLoud();
    return code;
}

// 0x13f bulletFlying(): its own shot is in the air.
// FS2 0x001ba0e0

static const int *nativeBulletFlying(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->m_bulletFlying != 0;
    return code;
}

// 0x143 isCorpse().
// FS2 0x001b9bf0
// callcheck: getLifeState

static const int *nativeIsCorpse(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->getLifeState() == LifeState_Corpse;
    return code;
}

// 0x166 isWalking(): moving, not at a run.
// FS2 0x001b6820
// callcheck: FUN_001b6d50, FUN_001b6cc0, FUN_001b6d50, FUN_001b6910 (m_motion, isRunMotion, isCrouchRunMotion, isMovingMotion)

static const int *nativeIsWalking(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    // PC: the ROM has no test.
    const int id = who != nullptr ? who->m_motion->motionId : 0;
    *acc = !isRunMotion(id) && !isCrouchRunMotion(id) && isMovingMotion(id);
    return code;
}

// 0x171 hasOverlay(): the brain runs an overlay slot.
// FS2 0x001b60b0
// callcheck: overlayMark

static const int *nativeHasOverlay(ScriptRunner *self, int *acc, const int *code)
{
    constexpr int kNoOverlay = -1;
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->m_brain != nullptr && who->m_brain->overlayMark() != kNoOverlay;
    return code;
}

// 0x18c angleTo(x, y): degrees, the facing of the object's world place.
// FS2 0x001b42f0
// callcheck: atan2Octant, addAngle

static const int *nativeAngleTo(ScriptRunner *self, int *acc, const int *code)
{
    constexpr float kPi = 3.14159274f;
    constexpr float kDegrees = 57.295776f;
    const int *args = self->popped(2);
    const sceVu0FVECTOR *world = self->getTarget()->getScriptObject()->getWorldMatrix();
    const float angle = atan2Octant((float)args[0] - world[3][0], (float)args[1] - world[3][1]);
    *acc = (int)(addAngle(-angle, kPi) * kDegrees);
    return code;
}

// 0x192 eventEffect(effect, object): on bn_top of a sub-scene's object.
// FS2 0x001b39a0
// callcheck: readStringArg, readStringArg, find8, FUN_002689a0 (spawnEffectOnBone)

static const int *nativeEventEffect(ScriptRunner *self, int *acc, const int *code)
{
    static const char kTop[] = "bn_top";    /* data 0x304b50 */
    const char *name;
    const char *objectName;
    const int *next = self->readStringArg(&name, code);
    next = self->readStringArg(&objectName, next);
    StageObject *object = gStageScene->objects->find8(objectName, 0);
    *acc = object != nullptr ? spawnEffectOnBone(object, name, kTop) : 0;
    return next;
}

// 0x19b isMotionEnding(object).
// FS2 0x001b2ba0
// callcheck: readObjectArg, isBoneEnding

static const int *nativeIsMotionEnding(ScriptRunner *self, int *acc, const int *code)
{
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    // PC: an empty cell reads rubbish on the EE.
    *acc = object != nullptr && object->m_model->pose->isBoneEnding(nullptr);
    return next;
}

// 0x1ef nearNamed(name, rank): group 6's rank-th nearest, for the script point.
// FS2 0x001ac5f0
// callcheck: readNameArg, FUN_0029f5c0, FUN_00179ad0 (scriptPickNamed; setScriptPoint)

static const int *nativeNearNamed(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int rank = self->popped(1)[0];
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = target->getScriptCharacter();
    // PC: the ROM has no test.
    StageObject *found = who != nullptr ? scriptPickNamed(name, who, rank, true, false) : nullptr;
    if (found != nullptr)
        who->setScriptPoint(found->m_xform.position);
    *acc = found != nullptr;
    return next;
}

// 0x1f9 smokeNear(): a burner in its area within the mission's block range.
// FS2 0x001abc00
// callcheck: currentMission, currentMission, currentMission, isInSmokeArea

static const int *nativeSmokeNear(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    StageMission *mission = gStageScene->currentMission();
    const float range = mission->blockRange;
    StageObject **it = mission->burners.objects;
    StageObject **end = it + mission->burners.count;
    *acc = 0;
    for (; it != end; it++) {
        StageThrownItem *item = static_cast<StageThrownItem *>(*it);
        if (item->m_active != 0 && static_cast<StageObject *>(item)->m_parent == nullptr && item->isInSmokeArea()
            && target->scriptDistanceTo(item) <= range) {
            *acc = 1;
            break;
        }
    }
    return code;
}

// 0x214 fireAt(name, flags, frames): the brain's shot, at a character or a
// group 6 object.
// FS2 0x001a9e40
// callcheck: readNameArg, readStringArg, findCharacter, find6

static const int *nativeFireAt(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int frames30 = self->popped(1)[0];
    const char *name;
    const char *flags;
    const int *next = self->readNameArg(&name, code);
    next = self->readStringArg(&flags, next);
    StageObject *object = gStageScene->objects->findCharacter(name, 0);
    if (object == nullptr)
        object = gStageScene->objects->find6(name, 0);
    *acc = target->scriptFireAt(object, flags, frames30);
    return next;
}

// 0x35 markPlace(): slot +0xb8, a character keeps where it stands, once.
// FS2 0x001cc2d0

static const int *nativeMarkPlace(ScriptRunner *self, int *, const int *code)
{
    self->getTarget()->scriptMarkPlace();
    return code;
}

// 0x3d heardAlly2(): the bit 0x93 asks too; the ROM's has no test of the character.
// FS2 0x001cbca0

static const int *nativeHeardAllyAgain(ScriptRunner *self, int *acc, const int *code)
{
    *acc = heardBit(self, kHeardAlly);
    return code;
}

// 0x9f leaveNoticed(), 0x17f leaveAtOnce().
// FS2 0x001c53e0
// callcheck: FUN_002196f0 (leaveNoticed)

static const int *nativeLeaveNoticed(ScriptRunner *self, int *, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    if (who != nullptr)
        who->leaveNoticed();
    return code;
}

// FS2 0x001b5180
// callcheck: FUN_00219310 (leaveAtOnce)
static const int *nativeLeaveAtOnce(ScriptRunner *self, int *, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    if (who != nullptr)
        who->leaveAtOnce();
    return code;
}

static const char kSpawnPointName[] = "P_SPW%02d";  /* data 0x304bb0, 0x309d80 */

// What P_SPW<id> may be: missionPickSpawnPoint takes only the kept ones.
enum { kSpawnPointOff = 1, kSpawnPointKept = 2 };

// The number in a P_SPW<id> name; -1 for any other.
// FS2 0x0029fb30
// callcheck: sscanf

int parseSpawnId(const char *name)
{
    int id = -1;
    sscanf(name, kSpawnPointName, &id);
    return id;
}

// A named spawn point's state, when the map has it.
static void setNamedSpawnPoint(const char *label, int state)
{
    int id;
    if (sscanf(label, kSpawnPointName, &id) != 1)
        return;
    MissionSpawnPoint &point = gStageScene->currentMission()->spawnPoints[id];
    if (point.state != 0)
        point.state = state;
}

// 0xa0 keepSpawnPoint(name), 0xa1 dropSpawnPoint(name). 0x174 dropSpawnPoints().
// FS2 0x001c52d0
// callcheck: readObjectArg, sscanf, currentMission

static const int *nativeKeepSpawnPoint(ScriptRunner *self, int *acc, const int *code)
{
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    setNamedSpawnPoint(label, kSpawnPointKept);
    *acc = 0;
    return next;
}

// FS2 0x001c51c0
// callcheck: readObjectArg, sscanf, currentMission
static const int *nativeDropSpawnPoint(ScriptRunner *self, int *acc, const int *code)
{
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    setNamedSpawnPoint(label, kSpawnPointOff);
    *acc = 0;
    return next;
}

// FS2 0x001b5d80
// callcheck: currentMission
static const int *nativeDropSpawnPoints(ScriptRunner *, int *acc, const int *code)
{
    StageMission *mission = gStageScene->currentMission();
    for (MissionSpawnPoint &point : mission->spawnPoints) {
        if (point.state != 0)
            point.state = kSpawnPointOff;
    }
    *acc = 0;
    return code;
}

// 0xb3 doorState(state): what the map shows of the door, in the script's
// numbers.
// FS2 0x001c3a30
// callcheck: FUN_001725d0, FUN_001725d0, FUN_001725d0, FUN_001725d0

static const int *nativeDoorState(ScriptRunner *self, int *, const int *code)
{
    const int state = self->popped(1)[0];
    StageDoor *door = self->getTarget()->getScriptDoor();
    // PC: the ROM has no test.
    if (door == nullptr)
        return code;
    switch (state) {
    case 3: door->m_doorState = StageDoor_None; break;
    case 2: door->m_doorState = StageDoor_Locked; break;
    case 1: door->m_doorState = StageDoor_Open2; break;
    case 0: door->m_doorState = StageDoor_Unknown; break;
    }
    return code;
}

// 0xce isOnStage(number).
// FS2 0x001c1590
// callcheck: currentMission (scriptCharacterAt)

static const int *nativeIsOnStage(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = scriptCharacterAt(self->popped(1)[0]);
    *acc = who != nullptr && who->m_active != 0;
    return code;
}

// 0x11f takeItem(name): one, mode 1.
// FS2 0x001bbff0
// callcheck: readNameArg, takeNamedItems

static const int *nativeTakeNamedItem(ScriptRunner *self, int *, const int *code)
{
    constexpr u_int kModeHide = 1;
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = target->getScriptCharacter();
    if (who != nullptr)
        who->takeNamedItems(name, 1, kModeHide);
    return next;
}

// 0x128 setVisibility(name, percent); below 0 nothing.
// FS2 0x001bb770
// callcheck: readNameArg, currentMission, setVisibilityRange

static const int *nativeSetVisibility(ScriptRunner *self, int *, const int *code)
{
    const int percent = self->popped(1)[0];
    const char *name;
    const int *next = self->readNameArg(&name, code);
    if (percent >= 0)
        gStageScene->currentMission()->setVisibilityRange((float)percent / 100.0f, name);
    return next;
}

// 0x12a spawnEffectAt(name, x, y, z, rx, ry, rz): degrees; its handle.
// FS2 0x001bb4c0
// callcheck: readNameArg, spawn

static const int *nativeSpawnEffectAt(ScriptRunner *self, int *acc, const int *code)
{
    constexpr float kRadians = 0.017453292f;
    const int *args = self->popped(6);
    const char *name;
    const int *next = self->readNameArg(&name, code);
    const sceVu0FVECTOR place = { (float)args[0], (float)args[1], (float)args[2], 1.0f };
    const sceVu0FVECTOR turn = { (float)args[3] * kRadians, (float)args[4] * kRadians, (float)args[5] * kRadians, 1.0f };
    *acc = gStageScene->effects->spawn(name, nullptr, place, turn);
    return next;
}

// 0x137 springTraps(name): the free items of that name read as sprung.
// FS2 0x001ba6f0
// callcheck: readNameArg, FUN_0021eb50, FUN_0021ead0, strcmp

static const int *nativeSpringTraps(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageObjArray &items = gStageScene->objects->m_lists[StageObjGroup_3];
    for (StageObject **slot = items.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *item = *slot;
        if (item->m_active != 0 && item->m_parent == nullptr && strcmp(item->m_name, name) == 0)
            static_cast<StageItem *>(item)->m_sprung = 1;
    }
    return next;
}

// 0x1c5 spawnNearLater(strong, mode, frames): now, or a timed record at its place.
// FS2 0x001af290
// callcheck: missionPickSpawnPoint, missionSpawnPointTaken, missionSpawnAt, currentMission, addTimedRecord

static const int *nativeSpawnNearLater(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(3);
    const int strong = args[0];
    const int mode = args[1];
    const int frames = args[2];
    if (frames < 1) {
        *acc = spawnNear(target, strong, mode);
    } else {
        const int x = (int)target->scriptGetPosX();
        const int y = (int)target->scriptGetPosY();
        const int z = (int)target->scriptGetPosZ();
        *acc = gStageScene->currentMission()->addTimedRecord(strong, x, y, z, mode, frames) != 0;
    }
    return code;
}

// 0x1c6 timedRecords(): how many wait.
// FS2 0x001af220
// callcheck: currentMission

static const int *nativeTimedRecords(ScriptRunner *, int *acc, const int *code)
{
    *acc = gStageScene->currentMission()->timedCount;
    return code;
}

// 0x1f6 letterbox(on): the mission's +0xda.
// FS2 0x001abeb0
// callcheck: currentMission

static const int *nativeLetterbox(ScriptRunner *self, int *, const int *code)
{
    const int on = self->popped(1)[0];
    gStageScene->currentMission()->letterbox = on != 0;
    return code;
}

// The spawn system of the stage's waiting characters, beside scriptPrepare in
// the ROM; what the natives above end in.

// The object is near the top view and in it.
// FS2 0x0029f200
// callcheck: getTopView, getMissionValue, getWorldMatrix, vecDistance, seesPoint

bool missionObjectInSight(StageObject *object)
{
    StageCamera *view = static_cast<StageCamera *>(gStageScene->getTopView());
    const float reach = gStageScene->getMissionValue();
    return vecDistance(view->getWorldMatrix()[3], object->m_xform.position) <= reach
        && view->seesPoint(object->m_xform.position);
}

// FS2 0x0029fd10

bool missionSpawnPointTaken(int *point)
{
    return *point == -1;
}

// FS2 0x0029f810
// callcheck: getMissionValue, getTopView, currentMission, getWorldMatrix, vecDistance, seesPoint, vecDistance, randNext2, FUN_0029fdb0 (std::stable_sort)

int missionPickSpawnPoint(const float *place, int mode)
{
    // layout: host -- the ROM keeps these on its stack.
    struct Candidate
    {
        int   index;
        float distance;
        bool  inView;
    };
    constexpr u_int kMost = 0x100;
    constexpr int   kKept = 2;
    const float reach = gStageScene->getMissionValue();
    StageCamera *view = static_cast<StageCamera *>(gStageScene->getTopView());
    StageMission *mission = gStageScene->currentMission();
    std::vector<Candidate> found;
    int hidden = 0;
    for (int i = 0; i < mission->spawnCount && found.size() != kMost; i++) {
        const MissionSpawnPoint &point = mission->spawnPoints[i];
        if (point.state != kKept)
            continue;
        const bool inView = vecDistance(view->getWorldMatrix()[3], point.position) <= reach
            && view->seesPoint(point.position);
        if (!inView)
            hidden++;
        found.push_back({ i, vecDistance(place, point.position), inView });
    }
    if (hidden == 0)
        return -1;
    const int count = (int)found.size();
    int from;
    if (mode < 0) {
        from = (int)(randNext2() % (u_int)count);
    } else {
        // The ROM's sort is unstable; ties are the file's order here.
        std::stable_sort(found.begin(), found.end(),
                         [](const Candidate &a, const Candidate &b) { return a.distance < b.distance; });
        from = mode;
    }
    for (int i = from; i < count; i++) {
        if (!found[i].inView)
            return found[i].index;
    }
    for (int i = (from < count ? from : count) - 1; i >= 0; i--) {
        if (!found[i].inView)
            return found[i].index;
    }
    return -1;
}

// FS2 0x0029fb60
// callcheck: currentMission, currentMission, getStageSelect, canSpawn, FUN_002115e0, FUN_002187f0 (isSpawnable; setSpawnPoint)

bool missionSpawnAt(int *point, int strong)
{
    constexpr int kKindWeak   = 3;
    constexpr int kKindStrong = 7;
    const int kind = strong != 0 ? kKindStrong : kKindWeak;
    StageMission *mission = gStageScene->currentMission();
    for (StageObject **it = mission->characters; it != mission->charactersEnd(); it++) {
        StageCharacter *who = static_cast<StageCharacter *>(*it);
        if (who->m_active == 0 && stageObjRecord(who->m_itemDef)->kind == kind
            && !who->canSpawn() && who->isSpawnable()) {
            who->setSpawnPoint(point);
            return true;
        }
    }
    return false;
}

// FS2 0x0029f5c0
// callcheck: findAll6, pathLength, FUN_002a0040 (std::stable_sort)

StageObject *scriptPickNamed(const char *name, StageCharacter *who, u_int rank, bool straight, bool lastInstead)
{
    // layout: host -- the ROM keeps these on its stack.
    struct Candidate
    {
        StageObject *object;
        float        distance;
    };
    StageObjArray *list = gStageScene->objects->findAll6(name);
    if (list == nullptr)
        return nullptr;
    if (list->m_count == 1) {
        if (!lastInstead && rank != 0)
            return nullptr;
        for (StageObject **slot = list->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
            if ((*slot)->m_active != 0)
                return *slot;
        }
        // PC: the ROM reads through a null here.
        return nullptr;
    }
    std::vector<Candidate> found;
    for (StageObject **slot = list->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *object = *slot;
        if (object->m_active == 0)
            continue;
        float distance;
        if (straight) {
            distance = who->distanceTo(object);
        } else {
            // PC: the route's filter is a stub's null until game/characterroute.cpp has it.
            CharacterRoute *route = who->m_brain != nullptr ? who->m_brain->getRoute() : nullptr;
            distance = route != nullptr && route->filter != nullptr
                ? route->filter->pathLength(who->m_xform.position, object->m_xform.position)
                : who->distanceTo(object);
        }
        found.push_back({ object, distance });
    }
    if (found.empty())
        return nullptr;
    if (found.size() == 1)
        return rank == 0 ? found[0].object : nullptr;
    std::stable_sort(found.begin(), found.end(),
                     [](const Candidate &a, const Candidate &b) { return a.distance < b.distance; });
    if (rank < found.size())
        return found[rank].object;
    return lastInstead ? found.back().object : nullptr;
}

static const char kSpawnRandom[] = "RANDOM";    /* data 0x304b28 */

// 0x1d1 hurtBy(number, amount, reaction), 0x1d2 hurtByName(amount, reaction,
// name): the target takes a blow of that character's.
// FS2 0x001ae610
// callcheck: currentMission, FUN_00217d60 (scriptCharacterAt; scriptHurt)

static const int *nativeHurtBy(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(3);
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageCharacter *from = scriptCharacterAt(args[0]);
    // PC: the ROM has no test of either.
    if (who != nullptr && from != nullptr)
        who->scriptHurt(from, args[1], args[2]);
    return code;
}

// FS2 0x001ae4c0
// callcheck: readNameArg, findCharacter, FUN_00217d60 (scriptHurt)

static const int *nativeHurtByName(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(2);
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = target->getScriptCharacter();
    StageObject *from = gStageScene->objects->findCharacter(name, 0);
    // PC: the ROM has no test of either.
    if (who != nullptr && from != nullptr)
        who->scriptHurt(from, args[0], args[1]);
    return next;
}

// 0x61 crouch(loudness): the target goes down, heard that far.
// FS2 0x001c90b0

static const int *nativeCrouch(ScriptRunner *self, int *acc, const int *code)
{
    const int loudness = self->popped(1)[0];
    *acc = self->getTarget()->scriptCrouch((float)loudness);
    return code;
}

// FS2 0x001afcf0
// FS2 0x001afd00
// FS2 0x001afd10
// The three small accessors of 0x1af9e0 are m_xform.position, the label's
// cell and spawnPoints[i].

// What native 0x1c0 does at once: as native 0x9d, a name RANDOM anywhere.
// FS2 0x001afe60
// callcheck: strcmp, parseSpawnId, missionPickSpawnPoint, missionSpawnPointTaken, missionSpawnAt, missionPickSpawnPoint, missionSpawnPointTaken, missionSpawnAt, currentMission, missionPickSpawnPoint, missionSpawnPointTaken, missionSpawnAt (spawnNear)

static bool spawnAtLabel(ScriptRunner *target, int strong, const char *label, int mode)
{
    constexpr int kAnyPoint = -1;
    if (strcmp(label, kSpawnRandom) == 0) {
        StageCharacter *actor = target->getScriptActor();
        return spawnNear(actor != nullptr ? static_cast<ScriptRunner *>(actor) : target, strong, kAnyPoint);
    }
    const int id = parseSpawnId(label);
    if (id < 0) {
        StageObject *object = (StageObject *)scriptLabelCell(label);
        return object != nullptr && spawnNear(object, strong, mode);
    }
    int point = missionPickSpawnPoint(gStageScene->currentMission()->spawnPoints[id].position, mode);
    return !missionSpawnPointTaken(&point) && missionSpawnAt(&point, strong);
}

// A timed record at a place, to spawn from when its frames run out.
static bool addTimedSpawn(const float *place, int strong, int mode, int frames)
{
    return gStageScene->currentMission()->addTimedRecord(strong, (int)place[0], (int)place[1], (int)place[2],
                                                         mode, frames) != 0;
}

// 0x1c0 spawnAtLater(name, strong, mode, frames): native 0x9d, later.
// FS2 0x001af9e0
// callcheck: readObjectArg, FUN_001afe60, strcmp, FUN_001afd20, parseSpawnId, FUN_001afd00, FUN_001afcf0, currentMission, addTimedRecord, currentMission, FUN_001afd10, missionPickSpawnPoint, missionSpawnPointTaken, currentMission, FUN_001afd10, currentMission, addTimedRecord

static const int *nativeSpawnAtLater(ScriptRunner *self, int *acc, const int *code)
{
    constexpr int kAnyPoint = -1;
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(3);
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    const int strong = args[0];
    const int mode = args[1];
    const int frames = args[2];
    if (frames < 1) {
        *acc = spawnAtLabel(target, strong, label, mode);
        return next;
    }
    if (strcmp(label, kSpawnRandom) == 0) {
        StageCharacter *actor = target->getScriptActor();
        ScriptRunner *from = actor != nullptr ? static_cast<ScriptRunner *>(actor) : target;
        const float place[3] = { from->scriptGetPosX(), from->scriptGetPosY(), from->scriptGetPosZ() };
        *acc = addTimedSpawn(place, strong, kAnyPoint, frames);
        return next;
    }
    const int id = parseSpawnId(label);
    if (id < 0) {
        StageObject *object = (StageObject *)scriptLabelCell(label);
        *acc = object != nullptr && addTimedSpawn(object->m_xform.position, strong, mode, frames);
        return next;
    }
    StageMission *mission = gStageScene->currentMission();
    int point = missionPickSpawnPoint(mission->spawnPoints[id].position, mode);
    *acc = !missionSpawnPointTaken(&point) && addTimedSpawn(mission->spawnPoints[point].position, strong, mode, frames);
    return next;
}

// 0x20a isBroken(): a light's m_broken, a character that lies a corpse.
// FS2 0x001aa9e0
// callcheck: getLifeState

static const int *nativeIsBroken(ScriptRunner *self, int *acc, const int *code)
{
    StageObject *object = self->getTarget()->getScriptObject();
    constexpr int kLightKindFirst = StageObject_Light;
    constexpr int kLightKindLast  = 0x12;
    const int kind = object->getKind();
    if (kind >= kLightKindFirst && kind <= kLightKindLast)
        *acc = static_cast<StageLight *>(object)->isBroken();
    else if (kind == StageObject_Character)
        *acc = static_cast<StageCharacter *>(object)->getLifeState() == LifeState_Corpse;
    else
        *acc = 0;
    return code;
}

// 0xb2 lightOn(name), 0x127 lightOff(name): the object's m_lightOn.
// FS2 0x001c3b70
// callcheck: readObjectArg

static const int *nativeObjectLightOn(ScriptRunner *self, int *, const int *code)
{
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    // PC: the ROM has no test of the cell.
    if (StageObject *object = (StageObject *)scriptLabelCell(label))
        object->m_lightOn = 1;
    return next;
}

// FS2 0x001bb850
// callcheck: readObjectArg

static const int *nativeObjectLightOff(ScriptRunner *self, int *, const int *code)
{
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    // PC: the ROM has no test of the cell.
    if (StageObject *object = (StageObject *)scriptLabelCell(label))
        object->m_lightOn = 0;
    return next;
}

// 0x208 isInSight(number): the character numbered, seen from the top view.
// FS2 0x001aac70
// callcheck: currentMission, FUN_0029f200 (scriptCharacterAt)

static const int *nativeIsInSight(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *other = scriptCharacterAt(self->popped(1)[0]);
    // PC: the ROM reads the place of a null.
    *acc = other != nullptr && missionObjectInSight(other);
    return code;
}

// 0xab stopSound(name).
// FS2 0x001c4180
// callcheck: readNameArg, stopNamed

static const int *nativeStopSound(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    gStageScene->sound->stopNamed(name);
    return next;
}

// 0x21a setMotionLabel(name).
// FS2 0x001a9850
// callcheck: readNameArg, FUN_00217c80 (setMotionByLabel)

static const int *nativeSetMotionLabel(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    if (StageCharacter *who = target->getScriptCharacter())
        who->setMotionByLabel(name);
    return next;
}

// 0x68 playMotion(name).
// FS2 0x001c8980
// callcheck: readNameArg

static const int *nativePlayMotion(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    target->scriptPlayMotion(name);
    return next;
}

// 0xb6 playSoundAtVolume(sound, object, percent): native 0x6b with a volume
// for the flat case (the heard ones pass it in f12, and nothing reads it).
// FS2 0x001c34f0
// callcheck: readNameArg, readObjectArg, strcmp, playNamed, getKind, getKind, find, FUN_00190650, getMatrix (playSoundFrom)

static const int *nativePlaySoundAtVolume(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int percent = self->popped(1)[0];
    const char *sound;
    const char *label;
    const int *next = self->readNameArg(&sound, code);
    next = self->readObjectArg(&label, next);
    playSoundFrom(target, sound, label, (float)percent / 100.0f);
    return next;
}

// 0x5e spawnEffectAt(x, y, z, rx, ry, rz, effect, object, bone): native 0x5d
// with a place and a turn, in degrees; its handle, -1 none.
// FS2 0x001c93c0
// callcheck: readNameArg, readObjectArg, readNameArg, isNullName, isNullName, getTopView, FUN_001c97f0, FUN_001c97e0, spawn, FUN_001c97f0, FUN_001c9780, FUN_001b56a0, FUN_001b5690, FUN_001b5680, FUN_001c97e0, spawn, spawnEffectOnBone, FUN_001c9770

static const int *nativeSpawnEffectPlaced(ScriptRunner *self, int *acc, const int *code)
{
    constexpr float kRadians = 0.017453292f;
    const int *args = self->popped(6);
    const char *name;
    const char *label;
    const char *bone;
    const int *next = self->readNameArg(&name, code);
    next = self->readObjectArg(&label, next);
    next = self->readNameArg(&bone, next);
    const sceVu0FVECTOR place = { (float)args[0], (float)args[1], (float)args[2], 1.0f };
    const sceVu0FVECTOR turn = { (float)args[3] * kRadians, (float)args[4] * kRadians, (float)args[5] * kRadians, 1.0f };
    StageObject *object = (StageObject *)scriptLabelCell(label);
    int handle = -1;
    if (isNullName(bone)) {
        StageObject *parent = isNullName(label) ? gStageScene->getTopView() : object;
        handle = gStageScene->effects->spawn(name, parent, place, turn);
    } else if (object != nullptr && object->m_model != nullptr) {
        // The ROM's two ways, a character's and the rest's, are one lookup.
        BoneMap::Node *node = object->m_model->boneMap.find(bone);
        if (node != nullptr && node->value != nullptr)
            handle = gStageScene->effects->spawn(name, node->value, place, turn);
    }
    *acc = handle;
    return next;
}

// 0x34 canSeeWithin(degrees, name): native 0x33 with the cone given; a
// character at 60 degrees goes by the sight table.
// FS2 0x001cc330
// callcheck: readObjectArg, canSeeThisFrame, isPointInView, parseSpawnId, currentMission, isPointInView

static const int *nativeCanSeeWithin(ScriptRunner *self, int *acc, const int *code)
{
    constexpr float kRadians = 0.017453292f;
    constexpr float kFullRange = 1.0f;
    constexpr int kTableDegrees = 60;
    ScriptRunner *target = self->getTarget();
    const int degrees = self->popped(1)[0];
    const float cone = (float)degrees * kRadians;
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    float distance;
    if (object == nullptr) {
        const int id = parseSpawnId(label);
        StageMission *mission = gStageScene->currentMission();
        StageCharacter *who = id >= 0 && mission->spawnPoints[id].state != 0 ? target->getScriptCharacter() : nullptr;
        *acc = who != nullptr && who->isPointInView(cone, kFullRange, mission->spawnPoints[id].position, &distance, nullptr);
    } else if (object->getKind() == StageObject_Character) {
        StageCharacter *actor = target->getScriptActor();
        if (actor == nullptr || degrees != kTableDegrees) {
            *acc = target->scriptCanSee(cone, object);
        } else {
            distance = 0.0f;
            *acc = actor->canSeeThisFrame(static_cast<StageCharacter *>(object), &distance);
        }
    } else {
        StageCharacter *who = target->getScriptCharacter();
        *acc = who != nullptr && who->isPointInView(cone, kFullRange, object->m_xform.position, &distance, nullptr);
    }
    return next;
}

// 0x1fb markWeapons(name): the free weapons of that name; findLooseWeapon
// leaves them.
// FS2 0x001ab870
// callcheck: readNameArg, findAll3

static const int *nativeMarkWeapons(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageObjArray *items = gStageScene->objects->findAll3(name);
    if (items == nullptr)
        return next;
    for (StageObject **slot = items->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *item = *slot;
        if (item->m_active != 0 && item->getKind() == StageObject_WeaponItem)
            static_cast<StageWeaponItem *>(item)->m_45c = 1;
    }
    return next;
}

// 0x15e overlayKind(kind, entry): native 0x15d with the overlay's number given.
// On the running script, not its target.
// FS2 0x001b79d0
// callcheck: startOverlay

static const int *nativeOverlayKind(ScriptRunner *self, int *acc, const int *code)
{
    const int kind = self->popped(1)[0];
    StageCharacter *character = self->getScriptCharacter();
    // PC: the ROM has no test.
    *acc = character != nullptr && character->m_brain != nullptr
        && character->m_brain->startOverlay(kind, (const ScriptEntry *)gStageScene->scripts[1]->entry(*code));
    return code + 1;
}

// A list of spawn points like "1, 3-7, 12" gets value (the rest other, when
// other is 0 or more); 0 turns a point off, the rest keep it. How many
// points changed, 0 at a word that is no number.
// FS2 0x001d0b50
// callcheck: strtol, strtol, currentMission

static int applySpawnPointList(const char *text, int value, int other)
{
    constexpr int kStateOff = 1;
    constexpr int kStateKept = 2;
    constexpr int kNotListed = -1;
    const auto isSpace = [](char c) {
        return c == 0x1a || c == '\f' || c == '\r' || c == '\n' || c == '\t' || c == ' ';
    };
    int list[kMissionSpawnPoints];
    std::fill(list, list + kMissionSpawnPoints, other >= 0 ? other : kNotListed);

    char *end;
    for (;;) {
        while (*text != '\0' && isSpace(*text))
            text++;
        if (*text == '\0')
            break;
        const u_int first = (u_int)strtol(text, &end, 10);
        if (end == text)
            return 0;
        text = end;
        while (*text != '\0' && isSpace(*text))
            text++;
        if (*text == '-') {
            text++;
            while (*text != '\0' && isSpace(*text))
                text++;
            const u_int last = (u_int)strtol(text, &end, 10);
            if (end == text)
                return 0;
            if (first < kMissionSpawnPoints && last < kMissionSpawnPoints) {
                for (u_int i = first; i <= last; i++)
                    list[i] = value;
            }
            text = end;
            while (*text != '\0' && isSpace(*text))
                text++;
        } else if (first < kMissionSpawnPoints) {
            list[first] = value;
        }
        if (*text == ',')
            text++;
    }

    StageMission *mission = gStageScene->currentMission();
    int changed = 0;
    for (int i = 0; i < kMissionSpawnPoints; i++) {
        if (list[i] < 0 || mission->spawnPoints[i].state == 0)
            continue;
        mission->spawnPoints[i].state = list[i] == 0 ? kStateOff : kStateKept;
        changed++;
    }
    return changed;
}

// 0x178 keepSpawnPointList(list, value, other).
// FS2 0x001b5890
// callcheck: readStringArg, FUN_001d0b50 (applySpawnPointList)

static const int *nativeKeepSpawnPointList(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(2);
    const char *text;
    const int *next = self->readStringArg(&text, code);
    *acc = applySpawnPointList(text, args[0], args[1]);
    return next;
}

// 0x173 keepAllSpawnPoints().
// FS2 0x001b5e90
// callcheck: currentMission

static const int *nativeKeepAllSpawnPoints(ScriptRunner *, int *acc, const int *code)
{
    constexpr int kStateKept = 2;
    StageMission *mission = gStageScene->currentMission();
    for (int i = 0; i < kMissionSpawnPoints; i++) {
        if (mission->spawnPoints[i].state != 0)
            mission->spawnPoints[i].state = kStateKept;
    }
    *acc = 0;
    return code;
}

// 0x175 keepSpawnPoints(first, last): how many there were.
// FS2 0x001b5bf0
// callcheck: currentMission

static const int *nativeKeepSpawnPoints(ScriptRunner *self, int *acc, const int *code)
{
    constexpr int kStateKept = 2;
    const int *args = self->popped(2);
    StageMission *mission = gStageScene->currentMission();
    int kept = 0;
    for (int i = args[0]; i <= args[1]; i++) {
        if (mission->spawnPoints[i].state != 0) {
            mission->spawnPoints[i].state = kStateKept;
            kept++;
        }
    }
    *acc = kept;
    return code;
}

// 0x227 blowNear(name): the target's blow with that weapon reaches under 140
// of its own height.
// FS2 0x001a8ac0
// callcheck: readNameArg, isBlowActive, strcmp

static const int *nativeBlowNear(ScriptRunner *self, int *acc, const int *code)
{
    constexpr float kReach = 140.0f;
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = target->getScriptCharacter();
    *acc = 0;
    if (who != nullptr && who->m_290 != nullptr && who->isBlowActive() && who->m_weapons[0] != nullptr
        && strcmp(who->m_290->m_name, name) == 0) {
        const sceVu0FVECTOR *matrix = who->m_290->getMatrix();
        const sceVu0FVECTOR point = { matrix[3][0], matrix[3][1], who->m_xform.position[2], matrix[3][3] };
        *acc = who->distanceToPoint(point) < kReach;
    }
    return next;
}

// 0xbe biteTargetNumber().
// FS2 0x001c2c00
// callcheck: getBiteTarget, getBiteTarget

static const int *nativeBiteTargetNumber(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    StageObject *bitten = who != nullptr ? who->getBiteTarget() : nullptr;
    *acc = bitten != nullptr ? bitten->m_id : 0;
    return code;
}

// 0x6d isCompanion().
// FS2 0x001c83c0

static const int *nativeIsCompanion(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptIsCompanion();
    return code;
}

// 0x179 releaseAction().
// FS2 0x001b5800
// callcheck: releaseAction

static const int *nativeReleaseAction(ScriptRunner *self, int *, const int *code)
{
    if (StageCharacter *who = self->getTarget()->getScriptCharacter())
        who->m_motion->releaseAction();
    return code;
}

// 0x185 shift(value, count): left by a positive count, right (unsigned) by a
// negative one.
// FS2 0x001b4cf0

static const int *nativeShift(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(2);
    u_int value = (u_int)args[0];
    const int count = args[1];
    if (count > 0)
        value <<= count & 0x1f;
    else if (count < 0)
        value >>= -count & 0x1f;
    *acc = (int)value;
    return code;
}

// 0xf9 pickedItem(): the index the item list just picked, 0 none.
// FS2 0x001be520

static const int *nativePickedItem(ScriptRunner *, int *acc, const int *code)
{
    const int picked = gStageScene->itemList->pendingCount;
    *acc = picked > 0 ? picked - 1 : picked;
    return code;
}

// 0x234 playMusicOn(channel, frames, name), 0x235 stopMusicOn(channel, frames):
// channels from 1.
// FS2 0x001a7b40
// callcheck: readNameArg, playMusic

static const int *nativePlayMusicOn(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    const char *name;
    const int *next = self->readNameArg(&name, code);
    gStageScene->sound->playMusic(args[0] - 1, name, args[1]);
    return next;
}

// FS2 0x001a7a80
// callcheck: stopMusic

static const int *nativeStopMusicOn(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    gStageScene->sound->stopMusic(args[0] - 1, args[1]);
    return code;
}

// 0x1a9 showNotice(text): held on screen, at the highest level.
// FS2 0x001b2080
// callcheck: readCellArg, addNotice

static const int *nativeShowNotice(ScriptRunner *self, int *, const int *code)
{
    constexpr int kForever = 0x7fffffff;
    constexpr int kNoticeArg = 0;
    void *text;
    const int *next = self->readCellArg(&text, code);
    gStageScene->pauseState->addNotice((const u_short *)text, kForever, kNoticeArg, kForever);
    return next;
}

// 0xa6 loseItems(count, name): drops that many, and they leave the stage.
// FS2 0x001c4c70
// callcheck: readNameArg, findItem, dropItem, setActive, setShown

static const int *nativeLoseItems(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int count = self->popped(1)[0];
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = target->getScriptCharacter();
    if (who == nullptr)
        return next;
    for (int i = 0; i < count; i++) {
        StageObject *item = who->findItem(name);
        if (item == nullptr)
            break;
        who->dropItem(item);
        item->m_tree->setActive(false);
        item->m_tree->setShown(false);
    }
    return next;
}

// 0x200 takeOverState(): 0 no alternate action, 1 on the player, 2 on another.
// FS2 0x001ab510
// callcheck: getPlayerAltFlag

static const int *nativeTakeOverState(ScriptRunner *, int *acc, const int *code)
{
    constexpr int kOnPlayer = 1;
    constexpr int kOnOther = 2;
    StageScene *scene = gStageScene;
    *acc = 0;
    if (scene->getPlayerAltFlag() != 0)
        *acc = scene->controlled == nullptr || scene->controlled == scene->player ? kOnPlayer : kOnOther;
    return code;
}

// 0x172 hasOverlayKind(kind).
// FS2 0x001b5fa0
// callcheck: hasOverlay

static const int *nativeHasOverlayKind(ScriptRunner *self, int *acc, const int *code)
{
    const int kind = self->popped(1)[0];
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    // PC: the ROM has no test.
    *acc = who != nullptr && who->m_brain != nullptr && who->m_brain->hasOverlay(kind);
    return code;
}

// 0x16f carLights(on).
// FS2 0x001b6260
// callcheck: setLights

static const int *nativeCarLights(ScriptRunner *self, int *, const int *code)
{
    const int on = self->popped(1)[0];
    gStageScene->car->setLights(on != 0);
    return code;
}

// 0xd0 isUp(name).
// FS2 0x001c1430
// callcheck: readNameArg, findCharacter, getLifeState

static const int *nativeIsUp(ScriptRunner *self, int *acc, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageObject *found = gStageScene->objects->findCharacter(name, 0);
    *acc = found != nullptr && static_cast<StageCharacter *>(found)->getLifeState() == LifeState_Up;
    return next;
}

static const char kSpecialGetOn[]  = "GettingOnCar";    /* data 0x304b88 */
static const char kSpecialGetOff[] = "GettingOffCar";   /* data 0x304b78 */
static const char kSpecialDrag[]   = "DragOutCar";      /* data 0x304b68 */

// 0x168 playerGetOnCar(), 0x169 playerGetOffCar().
// FS2 0x001b66d0
// callcheck: setSpecial

static const int *nativePlayerGetOnCar(ScriptRunner *, int *, const int *code)
{
    gStageScene->player->setSpecial(true, kSpecialGetOn);
    return code;
}

// FS2 0x001b6660
// callcheck: setSpecial

static const int *nativePlayerGetOffCar(ScriptRunner *, int *, const int *code)
{
    gStageScene->player->setSpecial(true, kSpecialGetOff);
    return code;
}

// 0x167 canGetOnCar(), 0x16b canGetOffCar(): the player may, and no event runs.
// FS2 0x001b6740
// callcheck: isSpecialAllowed

static const int *nativeCanGetOnCar(ScriptRunner *, int *acc, const int *code)
{
    *acc = gStageScene->player->isSpecialAllowed(kSpecialGetOn) && gStageScene->event == nullptr;
    return code;
}

// FS2 0x001b64d0
// callcheck: isSpecialAllowed

static const int *nativeCanGetOffCar(ScriptRunner *, int *acc, const int *code)
{
    *acc = gStageScene->player->isSpecialAllowed(kSpecialGetOff) && gStageScene->event == nullptr;
    return code;
}

// 0x16c canDragOutCar(), 0x16d dragOutCar().
// FS2 0x001b6420
// callcheck: isSpecialAllowed

static const int *nativeCanDragOutCar(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->isSpecialAllowed(kSpecialDrag);
    return code;
}

// FS2 0x001b63c0

static const int *nativeDragOutCar(ScriptRunner *self, int *, const int *code)
{
    self->getTarget()->scriptDragOutCar();
    return code;
}

// 0x212 breakWindow(): a door's window, with no effect.
// FS2 0x001aa0b0
// callcheck: breakPart

static const int *nativeBreakWindow(ScriptRunner *self, int *, const int *code)
{
    StageObject *object = self->getTarget()->getScriptObject();
    if (object != nullptr && object->getKind() == StageObject_Door
        && static_cast<StageDoor *>(object)->m_window != 0)
        object->breakPart(0, false);
    return code;
}

// 0x20e resetTraps(name): the free items of that name, unsprung (0x137's undo).
// FS2 0x001aa550
// callcheck: readNameArg, FUN_0021eb50, FUN_0021ead0, strcmp

static const int *nativeResetTraps(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageObjArray &items = gStageScene->objects->m_lists[StageObjGroup_3];
    for (StageObject **slot = items.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *item = *slot;
        if (item->m_active != 0 && item->m_parent == nullptr && strcmp(item->m_name, name) == 0)
            static_cast<StageItem *>(item)->m_sprung = 0;
    }
    return next;
}

// 0x1b5 addGroupColorKey(frame, r, g, b, a), 0x1c4 setGroupColorLoop(loop):
// every object sharing the target's name.
// FS2 0x001b13f0
// callcheck: colorToFloatGs, findAll5

static const int *nativeAddGroupColorKey(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(5);
    float color[4];
    colorToFloatGs(color, &args[1]);
    const char *name = target->getName();
    StageObjArray *list = name != nullptr ? gStageScene->objects->findAll5(name) : nullptr;
    if (list == nullptr)
        return code;
    for (StageObject **slot = list->begin(); slot != nullptr; slot = StageObjArray::next(slot))
        (*slot)->cmdAddColorKey(args[0], color);
    return code;
}

// FS2 0x001af4a0
// callcheck: findAll5

static const int *nativeSetGroupColorLoop(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int loop = self->popped(1)[0];
    const char *name = target->getName();
    StageObjArray *list = name != nullptr ? gStageScene->objects->findAll5(name) : nullptr;
    if (list == nullptr)
        return code;
    for (StageObject **slot = list->begin(); slot != nullptr; slot = StageObjArray::next(slot))
        (*slot)->cmdSetColorLoop((u_char)loop);
    return code;
}

// 0x18b angleToName(object): degrees from the target to it, 0 the +y axis
// turned half a circle.
// FS2 0x001b44a0
// callcheck: readObjectArg, atan2Octant, addAngle

static const int *nativeAngleToName(ScriptRunner *self, int *acc, const int *code)
{
    constexpr float kPi = 3.1415927f;
    constexpr float kDegrees = 57.295776f;
    ScriptRunner *target = self->getTarget();
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    StageObject *me = target->getScriptObject();
    // PC: the ROM has no test of either.
    if (object == nullptr || me == nullptr) {
        *acc = 0;
        return next;
    }
    const sceVu0FVECTOR *mine = me->getWorldMatrix();
    const sceVu0FVECTOR *theirs = object->getWorldMatrix();
    const float angle = atan2Octant(theirs[3][0] - mine[3][0], theirs[3][1] - mine[3][1]);
    *acc = (int)(addAngle(-angle, kPi) * kDegrees);
    return next;
}

// Natives 0xaa, 0x1e8: the gadget of the one on the pad takes the text, and
// the special turns on. Inline in 0x1acb00. True: no such gadget.
static bool armPadGadgetText(const char *special, const char *text)
{
    StageCharacter *onPad = static_cast<StageCharacter *>(scenePlayer());
    const bool armed = onPad->m_motion->armGadgetSpecial(special, text);
    if (armed)
        onPad->setSpecial(true, special);
    return !armed;
}

// The same, the text an object's name.
// FS2 0x001c46d0
// callcheck: FUN_001d1280, setText, setSpecial

static bool armPadGadget(const char *special, StageObject *object)
{
    // PC: the ROM reads +0x20 of a null object: the zero page, an empty text.
    return armPadGadgetText(special, object != nullptr ? object->m_name : "");
}

// 0xaa useLockGadget(kind, value, ?, ?): the seven keys (kind 3, value the
// object) or the number lock (kind 2): waits for the input, arms the
// gadget, and when the owned screen comes up saves the flags and fades,
// hides the objects until it ends, and restores them.
// FS2 0x001c4200
// callcheck: FUN_001c47a0, FUN_001c4790, FUN_001c4780, isOwned2Busy, isOwned1Busy, restoreObjectFlags, scriptYield, FUN_001c46b0, isFullInMode2, scriptYield, FUN_001c46a0, stageHideObjects, currentMission, FUN_001c4680, currentMission, FUN_001c4680, FUN_0018bf10, scriptYield, scriptObjectOfValue, isOwned2Busy, FUN_001c46d0, saveObjectFlags, findNumLockTarget, isOwned1Busy, FUN_001c46d0, saveObjectFlags, scriptYield, scriptYield, isInputBlocked, FUN_001c46c0, scriptObjectOfValue, FUN_001c46d0, scriptYield, FUN_001c4760, findNumLockTarget, FUN_001c46d0, scriptYield, scriptYield

static const int *nativeUseLockGadget(ScriptRunner *self, int *, const int *code)
{
    enum { Native = 0xaa, Start = 0, Armed = 1, Fading = 2, Waiting = 3 };
    constexpr int kKindNumLock = 2;
    constexpr int kKindSevenKey = 3;
    static const char kSevenKey[] = "SevenKey";  /* data 0x304ac8 */
    static const char kNumLock[]  = "NumLock";   /* data 0x304ad8 */

    const int *args = self->popped(4);
    const int kind = args[0];
    const int value = args[1];
    StageScene *scene = gStageScene;
    switch (takeScriptYield()) {
    case Start:
        if (scene->isInputBlocked()) {
            scriptYield(Native, Start);
        } else if (kind == kKindSevenKey) {
            armPadGadget(kSevenKey, scriptObjectOfValue(value));
            scriptYield(Native, Armed);
        } else if (kind == kKindNumLock) {
            if (StageObject *lock = findNumLockTarget()) {
                armPadGadget(kNumLock, lock);
                scriptYield(Native, Armed);
            }
        }
        break;
    case Armed: {
        bool busy = false;
        if (kind == kKindSevenKey) {
            StageObject *object = scriptObjectOfValue(value);
            busy = scene->isOwned2Busy();
            if (busy)
                scene->saveObjectFlags((void *)(intptr_t)kKindSevenKey, object);
            else
                armPadGadget(kSevenKey, object);
        } else if (kind == kKindNumLock) {
            StageObject *lock = findNumLockTarget();
            busy = scene->isOwned1Busy();
            if (busy)
                scene->saveObjectFlags((void *)(intptr_t)kKindNumLock, lock);
            else
                armPadGadget(kNumLock, lock);
        }
        scriptYield(Native, busy ? Fading : Armed);
        break;
    }
    case Fading:
        if (!scene->fade->isFullInMode2()) {
            scriptYield(Native, Fading);
        } else {
            stageHideObjects();
            StageObject *kept = scene->currentMission()->keptObject;
            if (kept != nullptr)
                kept->m_viewActive = 1;
            scriptYield(Native, Waiting);
        }
        break;
    case Waiting: {
        bool done = false;
        if (kind == kKindSevenKey)
            done = !scene->isOwned2Busy();
        else if (kind == kKindNumLock)
            done = !scene->isOwned1Busy();
        if (done)
            scene->restoreObjectFlags();
        else
            scriptYield(Native, Waiting);
        break;
    }
    default:
        break;
    }
    return code;
}

static const char kSpecialSigLamp[] = "SigLamp";       /* data 0x304ae0 */

// 0x1e7 canUseSigLamp(): the one on the pad may, and the input is free.
// FS2 0x001acd00
// callcheck: isInputBlocked, isSpecialAllowed

static const int *nativeCanUseSigLamp(ScriptRunner *, int *acc, const int *code)
{
    StageCharacter *onPad = static_cast<StageCharacter *>(scenePlayer());
    *acc = !gStageScene->isInputBlocked() && onPad->isSpecialAllowed(kSpecialSigLamp);
    return code;
}

// 0x1e8 useSigLamp(text): the same test, then its gadget takes the text.
// FS2 0x001acb00
// callcheck: readNameArg, isInputBlocked, isSpecialAllowed, FUN_001d1280, setText, setSpecial (armPadGadgetText)

static const int *nativeUseSigLamp(ScriptRunner *self, int *acc, const int *code)
{
    const char *text;
    const int *next = self->readNameArg(&text, code);
    StageCharacter *onPad = static_cast<StageCharacter *>(scenePlayer());
    *acc = 0;
    if (!gStageScene->isInputBlocked() && onPad->isSpecialAllowed(kSpecialSigLamp)) {
        armPadGadgetText(kSpecialSigLamp, text);
        *acc = 1;
    }
    return next;
}

// 0x1e9 sigLampHasTarget().
// FS2 0x001aca90
// callcheck: hasTarget (slot +0x40)

static const int *nativeSigLampHasTarget(ScriptRunner *, int *acc, const int *code)
{
    constexpr int kOwnedSigLamp = 0;    // StageScene +0x8c
    *acc = gStageScene->owned[kOwnedSigLamp]->hasTarget();
    return code;
}

// 0x176 dropSpawnPointRange(first, last): how many there were.
// FS2 0x001b5a60
// callcheck: currentMission

static const int *nativeDropSpawnPointRange(ScriptRunner *self, int *acc, const int *code)
{
    constexpr int kStateOff = 1;
    const int *args = self->popped(2);
    StageMission *mission = gStageScene->currentMission();
    int dropped = 0;
    for (int i = args[0]; i <= args[1]; i++) {
        if (mission->spawnPoints[i].state != 0) {
            mission->spawnPoints[i].state = kStateOff;
            dropped++;
        }
    }
    *acc = dropped;
    return code;
}

// 0x6e isBlowActive().
// FS2 0x001c8320
// callcheck: isBlowActive

static const int *nativeIsBlowActive(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    *acc = who != nullptr && who->isBlowActive();
    return code;
}

// 0x209 isObjectInSight(object).
// FS2 0x001aab80
// callcheck: readObjectArg, FUN_0029f200

static const int *nativeIsObjectInSight(ScriptRunner *self, int *acc, const int *code)
{
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    // PC: the ROM reads the place of a null.
    *acc = object != nullptr && missionObjectInSight(object);
    return next;
}

// 0x5c spawnEffectOnObject(effect, object): on it, or on the top view for
// "NULL". The ROM keeps native 0x5d's whole body and tests its bone word
// against itself, so the other way is dead code.
// FS2 0x001c9b30
// callcheck: find, spawnEffectOnBone (the dead way)

static const int *nativeSpawnEffectOnObject(ScriptRunner *self, int *acc, const int *code)
{
    const char *name;
    const char *label;
    const int *next = self->readNameArg(&name, code);
    next = self->readObjectArg(&label, next);
    StageObject *parent = isNullName(label) ? gStageScene->getTopView() : (StageObject *)scriptLabelCell(label);
    *acc = gStageScene->effects->spawn(name, parent, nullptr, nullptr);
    return next;
}

// The sixth run's natives, by use on the disc (sssall.py). A car, the elevator
// and the number lock have no host class yet (createMapObject makes plain
// objects of them): what these call of them are stubs.

// 0x3b swingAtBite().
// FS2 0x001cbe90
// callcheck: scriptSwingAtBite

static const int *nativeSwingAtBite(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptSwingAtBite();
    return code;
}

// 0x43 addPlainMenuLine(kind, text): text a cell.
// FS2 0x001cb640
// callcheck: readCellArg, scriptAddMenuLine

static const int *nativeAddPlainMenuLine(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int kind = self->popped(1)[0];
    void *text;
    const int *next = self->readCellArg(&text, code);
    target->scriptAddMenuLine((const u_short *)text, kind, true);
    return next;
}

// 0x55 isCompanionName(name).
// FS2 0x001ca370
// callcheck: readNameArg, findCharacter, scriptIsCompanion

static const int *nativeIsCompanionName(ScriptRunner *self, int *acc, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = static_cast<StageCharacter *>(gStageScene->objects->findCharacter(name, 0));
    // PC: the ROM has no test.
    *acc = who != nullptr && who->scriptIsCompanion();
    return next;
}

// 0x56 isBlowActiveOf(object).
// FS2 0x001ca260
// callcheck: readObjectArg, isBlowActive

static const int *nativeIsBlowActiveOf(ScriptRunner *self, int *acc, const int *code)
{
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageCharacter *who = static_cast<StageCharacter *>((StageObject *)scriptLabelCell(label));
    *acc = who != nullptr && who->isBlowActive();
    return next;
}

// 0x58 isRunningName(name).
// FS2 0x001ca040
// callcheck: readNameArg, findCharacter, isRunningMotion

static const int *nativeIsRunningName(ScriptRunner *self, int *acc, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = static_cast<StageCharacter *>(gStageScene->objects->findCharacter(name, 0));
    // PC: the ROM has no test.
    *acc = who != nullptr && who->isRunningMotion();
    return next;
}

// 0x62 callOut(loudness).
// FS2 0x001c8fd0
// callcheck: scriptCallOut

static const int *nativeCallOut(ScriptRunner *self, int *acc, const int *code)
{
    const int loudness = self->popped(1)[0];
    *acc = self->getTarget()->scriptCallOut((float)loudness);
    return code;
}

// 0x81 pickableItem(): its handle, 0 none.
// FS2 0x001c74c0
// callcheck: scriptPickableHandle

static const int *nativePickableItem(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptPickableHandle();
    return code;
}

// 0xc2 clearFoeWeight(name).
// FS2 0x001c2600
// callcheck: readNameArg, getScriptCharacter, findCharacter, setFoeWeight

static const int *nativeClearFoeWeight(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageCharacter *who = target->getScriptCharacter();
    StageCharacter *other = static_cast<StageCharacter *>(gStageScene->objects->findCharacter(name, 0));
    if (who != nullptr && other != nullptr)
        who->setFoeWeight(other, 0);
    return next;
}

// 0xd3 isPlayer(): the target is the scene's player.
// FS2 0x001c1160

static const int *nativeIsPlayer(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *player = gStageScene->player;
    *acc = player == self->getTarget();
    return code;
}

// 0xed isCrouchingNumber(number).
// FS2 0x001bf400
// callcheck: currentMission, scriptIsCrouching (scriptCharacterAt)

static const int *nativeIsCrouchingNumber(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = scriptCharacterAt(self->popped(1)[0]);
    *acc = who != nullptr && who->scriptIsCrouching();
    return code;
}

// 0x12b spawnNearPlace(strong, x, y, z, mode).
// FS2 0x001bb370
// callcheck: missionPickSpawnPoint, missionSpawnPointTaken, missionSpawnAt

static const int *nativeSpawnNearPlace(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(5);
    const int strong = args[0];
    const sceVu0FVECTOR place = { (float)args[1], (float)args[2], (float)args[3], 1.0f };
    int point = missionPickSpawnPoint(place, args[4]);
    if (!missionSpawnPointTaken(&point))
        missionSpawnAt(&point, strong);
    *acc = 0;
    return code;
}

// 0x130 fireAtBite(frames, flags).
// FS2 0x001bafc0
// callcheck: readStringArg, scriptFireAtBite

static const int *nativeFireAtBite(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int frames30 = self->popped(1)[0];
    const char *flags;
    const int *next = self->readStringArg(&flags, code);
    *acc = target->scriptFireAtBite(flags, frames30);
    return next;
}

// 0x131 fireAtName(name): a character, or an object of group 6.
// FS2 0x001bae60
// callcheck: readNameArg, findCharacter, find6, scriptFireAt

static const int *nativeFireAtName(ScriptRunner *self, int *acc, const int *code)
{
    static const char kNoFlags[] = "";  /* data 0x304ba0 */
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageObject *object = gStageScene->objects->findCharacter(name, 0);
    if (object == nullptr)
        object = gStageScene->objects->find6(name, 0);
    *acc = target->scriptFireAt(object, kNoFlags, 0);
    return next;
}

// 0x135 hideEffect(name): group 5's.
// FS2 0x001ba900
// callcheck: readNameArg, FUN_001d1150

static const int *nativeHideEffect(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    hideActiveNamed(StageObjGroup_5, name);
    return next;
}

// 0x140 bulletFlyingNumber(number).
// FS2 0x001b9f80
// callcheck: currentMission (scriptCharacterAt)

static const int *nativeBulletFlyingNumber(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = scriptCharacterAt(self->popped(1)[0]);
    *acc = who != nullptr && who->m_bulletFlying != 0;
    return code;
}

// A notice for so many frames (0 forever), at the one level the data allows:
// the ROM clamps 5 to sdata 0x31a99c, 0x31a9a0, both 5.
static void addScriptNotice(const void *text, int frames, int arg)
{
    constexpr int kForever = 0x7fffffff;
    constexpr int kNoticeLevel = 5;
    gStageScene->pauseState->addNotice((const u_short *)text, frames == 0 ? kForever : frames, arg, kNoticeLevel);
}

// 0x14d noticeFor(frames, text).
// FS2 0x001b8e40
// callcheck: readCellArg, addNotice

static const int *nativeNoticeFor(ScriptRunner *self, int *, const int *code)
{
    const int frames = self->popped(1)[0];
    void *text;
    const int *next = self->readCellArg(&text, code);
    addScriptNotice(text, frames, 0);
    return next;
}

// 0x14e noticeForArg(frames, arg, text).
// FS2 0x001b8ca0
// callcheck: readCellArg, addNotice

static const int *nativeNoticeForArg(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    void *text;
    const int *next = self->readCellArg(&text, code);
    addScriptNotice(text, args[0], args[1]);
    return next;
}

// 0x154 carFlag3b9().
// FS2 0x001b8520

static const int *nativeCarFlag3b9(ScriptRunner *, int *acc, const int *code)
{
    StageCar *car = gStageScene->car;
    *acc = car != nullptr ? car->m_honked : 0;
    return code;
}

// 0x15a useItemAt(percent y, percent z, name).
// FS2 0x001b7d00
// callcheck: readNameArg, scriptUseItemAt

static const int *nativeUseItemAt(ScriptRunner *self, int *acc, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(2);
    const char *name;
    const int *next = self->readNameArg(&name, code);
    const sceVu0FVECTOR point = { 0.0f, (float)args[0] / 100.0f, (float)args[1] / 100.0f, 0.0f };
    target->scriptUseItemAt(name, point);
    *acc = 0;
    return next;
}

// 0x15c doorNotUsed().
// FS2 0x001b7c20
// callcheck: getScriptDoor, noteUse

static const int *nativeDoorNotUsed(ScriptRunner *self, int *, const int *code)
{
    StageDoor *door = self->getTarget()->getScriptDoor();
    // PC: the ROM has no test.
    if (door != nullptr)
        door->noteUse(false);
    return code;
}

// 0x170 carLightsOn().
// FS2 0x001b6190
// callcheck: isLit

static const int *nativeCarLightsOn(ScriptRunner *, int *acc, const int *code)
{
    StageCar *car = gStageScene->car;
    *acc = car != nullptr && car->m_lights->isLit();
    return code;
}

// 0x177 keepSpawnPointListAll(value, text): as 0x176, the others untouched.
// FS2 0x001b5980
// callcheck: readStringArg, FUN_001d0b50 (applySpawnPointList)

static const int *nativeKeepSpawnPointListAll(ScriptRunner *self, int *acc, const int *code)
{
    constexpr int kOthersUntouched = -1;
    const int value = self->popped(1)[0];
    const char *text;
    const int *next = self->readStringArg(&text, code);
    *acc = applySpawnPointList(text, value, kOthersUntouched);
    return next;
}

// 0x17a setHoldMotion(number): the script's numbers start at motion 0xf4.
// FS2 0x001b56f0
// callcheck: getScriptCharacter, FUN_001ded30

static const int *nativeSetHoldMotion(ScriptRunner *self, int *acc, const int *code)
{
    constexpr int kFirstScriptMotion = 0xf4;
    const int number = self->popped(1)[0];
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    // PC: the ROM has no test.
    *acc = who != nullptr && who->m_motion->setHold(number + kFirstScriptMotion);
    return code;
}

// What can be named for an effect: "bn_" in front and the digits of
// "ENV_xLn" are forgiven, whichever comes first. The ROM spells the walk
// out twice (0x1b3240 and 0x1b3440) with its own sprintf buffers.
static bool normalizeEnvName(const char *name, char *out)
{
    char letter;
    int number;
    if (name[0] != 'E' || sscanf(name, "ENV_%cL%2d", &letter, &number) != 2) /* data 0x304b30 */
        return false;
    sprintf(out, "ENV_%cL%02d", letter, number);    /* data 0x304b40 */
    return strcmp(name, out) != 0;
}

// FS2 0x001b3440
// callcheck: findAll5, FUN_001b36b0, findAll5, FUN_001b3440, sscanf, sprintf, strcmp, FUN_001b3440

static StageObjArray *findEffectGroup(const char *name)
{
    StageObjArray *group = gStageScene->objects->findAll5(name);
    if (group != nullptr)
        return group;
    if (name[0] == 'b' && name[1] == 'n' && name[2] == '_')
        return findEffectGroup(name + 3);
    char fixed[0x20];
    if (normalizeEnvName(name, fixed))
        return findEffectGroup(fixed);
    return nullptr;
}

// The effects of that name take the flag; another name is a sub-scene
// object's lamp.
// FS2 0x001b3240
// callcheck: FUN_001b36b0, findAll5, FUN_001b3440, sscanf, sprintf, strcmp, FUN_001b3440, find8, setTreeScriptFlag

static void setEffectsOn(const char *name, bool on)
{
    StageObjArray *group = name[0] == 'b' || name[0] == 'E' ? findEffectGroup(name) : nullptr;
    if (group == nullptr) {
        static_cast<SubSceneModelObject *>(gStageScene->objects->find8(name, 0))->setLampOn(on);
        return;
    }
    for (StageObject **slot = group->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        if ((*slot)->m_active != 0)
            (*slot)->setTreeScriptFlag(on);
    }
}

// 0x17c, 0x194 effectsOn(name), 0x195 effectsOff(name).
// FS2 0x001b52e0
// callcheck: readStringArg, FUN_001b3240

static const int *nativeEffectsOn(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readStringArg(&name, code);
    setEffectsOn(name, true);
    return next;
}

// FS2 0x001b31e0
// callcheck: readStringArg, FUN_001b3240

static const int *nativeEffectsOff(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readStringArg(&name, code);
    setEffectsOn(name, false);
    return next;
}

// 0x18d rideCar(on).
// FS2 0x001b41f0
// callcheck: getScriptCharacter, FUN_0017af50

static const int *nativeRideCar(ScriptRunner *self, int *, const int *code)
{
    const int on = self->popped(1)[0];
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    // PC: the ROM has no test.
    if (who != nullptr)
        who->rideCar(on == 0, 0);
    return code;
}

// 0x18e setPlayerCarFlag(on), 0x18f playerCarFlag().
// FS2 0x001b4110
// callcheck: FUN_001dee90

static const int *nativeSetPlayerCarFlag(ScriptRunner *self, int *, const int *code)
{
    const int on = self->popped(1)[0];
    gStageScene->player->m_motion->setCarSpecialFlag(on != 0);
    return code;
}

// FS2 0x001b4060
// callcheck: FUN_001dee50

static const int *nativePlayerCarFlag(ScriptRunner *, int *acc, const int *code)
{
    *acc = gStageScene->player->m_motion->getCarSpecialFlag();
    return code;
}

// 0x193 attachEffect(effect, object): under the object's bn_top when it has
// one, else under the object; on and shown.
// FS2 0x001b3720
// callcheck: readStringArg, readStringArg, attachEffectToObject, find8, find5, find, erase, pushBack, setActive, setShown

static const int *nativeAttachEffect(ScriptRunner *self, int *, const int *code)
{
    static const char kTop[] = "bn_top";    /* data 0x304b50 */
    const char *effectName;
    const char *objectName;
    const int *next = self->readStringArg(&effectName, code);
    next = self->readStringArg(&objectName, next);
    StageObject *object = gStageScene->objects->find8(objectName, 0);
    StageObject *effect = gStageScene->objects->find5(effectName, 0);
    BoneMap::Node *top = object->m_model->boneMap.find(kTop);
    StageNodeBase *parent = top != nullptr ? (StageNodeBase *)top->value : (StageNodeBase *)object;
    effect->setParent(parent);
    effect->m_tree->setActive(true);
    effect->m_tree->setShown(true);
    return next;
}

// The script's motion natives work on any map object: slot +0x9c answers the
// object itself on each of them (ScriptRunner::getScriptMapObject), not only
// on a car. PC: its pose can be missing where the ROM's never is: the host
// makes a plain object of the elevator, and that one takes no motion list.
static StageObject *scriptMotionObject(ScriptRunner *target)
{
    StageObject *object = target->getScriptMapObject();
    return object != nullptr && object->m_model != nullptr && object->m_model->pose != nullptr ? object
                                                                                               : nullptr;
}

// The car behind the same slot. The ROM writes through whatever answers it, and
// only cars are ever asked. PC: null for anything else.
static StageCar *scriptCar(ScriptRunner *target)
{
    StageObject *object = target->getScriptMapObject();
    return object != nullptr && object->getKind() == StageObject_Car ? static_cast<StageCar *>(object) : nullptr;
}

// 0x198 carPlayMotion(name).
// FS2 0x001b2e80
// callcheck: readNameArg, getScriptMapObject, playNamed

static const int *nativeCarPlayMotion(ScriptRunner *self, int *, const int *code)
{
    constexpr u_int kCarMotionFlags = 0x50;
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    if (StageObject *object = scriptMotionObject(target))
        object->m_model->playNamed(0, name, false, false, kCarMotionFlags, 0);
    return next;
}

// 0x19a carMotionEnding().
// FS2 0x001b2cd0
// callcheck: getScriptMapObject, isBoneEnding

static const int *nativeCarMotionEnding(ScriptRunner *self, int *acc, const int *code)
{
    StageObject *object = scriptMotionObject(self->getTarget());
    *acc = object != nullptr && object->m_model->pose->isBoneEnding(nullptr);
    return code;
}

// 0x19e setCarValue394(thousandths).
// FS2 0x001b28d0

static const int *nativeSetCarValue394(ScriptRunner *self, int *, const int *code)
{
    const int thousandths = self->popped(1)[0];
    if (gStageScene->car != nullptr)
        gStageScene->car->m_steerRate = (float)thousandths / 1000.0f;
    return code;
}

// 0x1ac setCarFlag3a0(on).
// FS2 0x001b1e60
// callcheck: getScriptMapObject

static const int *nativeSetCarFlag3a0(ScriptRunner *self, int *, const int *code)
{
    const int on = self->popped(1)[0];
    if (StageCar *car = scriptCar(self->getTarget()))
        car->m_onGround = on != 0;
    return code;
}

// 0x1c8 setCarMode(mode).
// FS2 0x001af050
// callcheck: getScriptMapObject

static const int *nativeSetCarMode(ScriptRunner *self, int *, const int *code)
{
    const int mode = self->popped(1)[0];
    if (StageCar *car = scriptCar(self->getTarget()))
        car->m_mode = mode;
    return code;
}

// The floors the scripts number: B1 1, B3 3.
enum ElevatorFloor
{
    ElevatorFloor_None = 0,
    ElevatorFloor_B1   = 1,
    ElevatorFloor_B3   = 3,
};

// What a character can start with the elevator: on it, the side it stands
// at; off it, the call of either floor. The ROM spells this out in natives
// 0x1ca and 0x1cb. 0 none.
enum ElevatorAction
{
    ElevatorAction_None     = 0,
    ElevatorAction_CallB1   = 1,
    ElevatorAction_CallB3   = 2,
    ElevatorAction_RideToB1 = 3,
    ElevatorAction_RideToB3 = 4,
};

static const char kElevatorB1[]   = "ElevatorB1";   /* data 0x304ae8 */
static const char kElevatorB3[]   = "ElevatorB3";   /* data 0x304af8 */
static const char kElevatorBxB3[] = "ElevatorBxB3"; /* data 0x304b08 */
static const char kElevatorBxB1[] = "ElevatorBxB1"; /* data 0x304b18 */

// callcheck: getZs9901, isOnElevator, isSpecialAllowed, isSpecialAllowed, elevatorIsAtB1, elevatorIsAtB3, isSpecialAllowed, isSpecialAllowed

static ElevatorAction elevatorActionOf(StageCharacter *who)
{
    StageObject *elevator = gStageScene->getZs9901();
    if (!who->isOnElevator()) {
        if (who->isSpecialAllowed(kElevatorB1))
            return ElevatorAction_CallB1;
        if (who->isSpecialAllowed(kElevatorB3))
            return ElevatorAction_CallB3;
        return ElevatorAction_None;
    }
    if (elevatorIsAtB1(elevator))
        return who->isSpecialAllowed(kElevatorBxB3) ? ElevatorAction_RideToB3 : ElevatorAction_None;
    if (elevatorIsAtB3(elevator) && who->isSpecialAllowed(kElevatorBxB1))
        return ElevatorAction_RideToB1;
    return ElevatorAction_None;
}

// 0x1ca elevatorAction(): of the script's own runner, not its target.
// FS2 0x001aee20
// callcheck: getScriptCharacter, getZs9901, isOnElevator, isSpecialAllowed, isSpecialAllowed, elevatorIsAtB1, elevatorIsAtB3, isSpecialAllowed (elevatorActionOf)

static const int *nativeElevatorAction(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getScriptCharacter();
    // PC: the ROM has no test.
    *acc = who != nullptr ? elevatorActionOf(who) : ElevatorAction_None;
    return code;
}

// 0x1cb useElevator(): starts that special.
// FS2 0x001aeb40
// callcheck: getScriptCharacter, getZs9901, isOnElevator, isSpecialAllowed, isSpecialAllowed, elevatorIsAtB1, elevatorIsAtB3, isSpecialAllowed, scriptUseSpecial (elevatorActionOf)

static const int *nativeUseElevator(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getScriptCharacter();
    const char *special = nullptr;
    // PC: the ROM has no test.
    switch (who != nullptr ? elevatorActionOf(who) : ElevatorAction_None) {
    case ElevatorAction_RideToB3: special = kElevatorBxB3; break;
    case ElevatorAction_RideToB1: special = kElevatorBxB1; break;
    case ElevatorAction_CallB3:   special = kElevatorB3;   break;
    case ElevatorAction_CallB1:   special = kElevatorB1;   break;
    default:                      break;
    }
    *acc = special != nullptr && self->scriptUseSpecial(special);
    return code;
}

// 0x1cc elevatorMove(floor, kind): floor 1 or 3, kind 4 or 5. The ROM leaves
// its registers as they were for any other value; PC: false.
// FS2 0x001ae9f0
// callcheck: getZs9901, FUN_00294f90

static const int *nativeElevatorMove(ScriptRunner *self, int *, const int *code)
{
    constexpr int kKindSecond = 5;
    const int *args = self->popped(2);
    elevatorMove(gStageScene->getZs9901(), args[0] == ElevatorFloor_B3, args[1] == kKindSecond);
    return code;
}

// 0x1cd elevatorRelease().
// FS2 0x001ae9a0
// callcheck: getZs9901, FUN_00294280

static const int *nativeElevatorRelease(ScriptRunner *, int *, const int *code)
{
    constexpr int kFloorRelease = 5;
    elevatorCall(gStageScene->getZs9901(), kFloorRelease);
    return code;
}

// 0x1ce elevatorFloor().
// FS2 0x001ae900
// callcheck: getZs9901, FUN_00294c70, FUN_00294c30

static const int *nativeElevatorFloor(ScriptRunner *, int *acc, const int *code)
{
    StageObject *elevator = gStageScene->getZs9901();
    if (elevatorIsAtB1(elevator))
        *acc = ElevatorFloor_B1;
    else if (elevatorIsAtB3(elevator))
        *acc = ElevatorFloor_B3;
    else
        *acc = ElevatorFloor_None;
    return code;
}

// 0x1cf elevatorPhase(): 7, 6, 5, 4 for the pose's phases 2, 4, 1, 3; 0 none.
// FS2 0x001ae820
// callcheck: getZs9901, FUN_00294b00, FUN_00294ad0, FUN_00294b60, FUN_00294b30

static const int *nativeElevatorPhase(ScriptRunner *, int *acc, const int *code)
{
    StageObject *elevator = gStageScene->getZs9901();
    if (elevatorIsPhase2(elevator))
        *acc = 7;
    else if (elevatorIsPhase4(elevator))
        *acc = 6;
    else if (elevatorIsPhase1(elevator))
        *acc = 5;
    else if (elevatorIsPhase3(elevator))
        *acc = 4;
    else
        *acc = 0;
    return code;
}

// 0x21d elevatorFloorFree(floor): not called yet.
// FS2 0x001a93c0
// callcheck: getZs9901

static const int *nativeElevatorFloorFree(ScriptRunner *self, int *acc, const int *code)
{
    const u_int floor = (u_int)self->popped(1)[0];
    *acc = (elevatorCalledFloors(gStageScene->getZs9901()) & (1u << (floor & 0x1f))) == 0;
    return code;
}

// 0x1d0 inCompanionMode().
// FS2 0x001ae780
// callcheck: getPlayerAltFlag, FUN_001fe9f0

static const int *nativeInCompanionMode(ScriptRunner *, int *acc, const int *code)
{
    *acc = gStageScene->getPlayerAltFlag() != 0 && gStageScene->party->isCompanionMode();
    return code;
}

// 0x1d3 setNoticeEnabled(index, on).
// FS2 0x001ae400
// callcheck: currentMission, setNoticeEnabled

static const int *nativeSetNoticeEnabled(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(2);
    gStageScene->currentMission()->setNoticeEnabled(args[0], args[1] != 0);
    return code;
}

// Whom natives 0x1d5, 0x1d7 and 0x1ed mean: who plays now, 0x1d5 and 0x1d7
// the partner off a draining stage.
static StageCharacter *playingCharacter()
{
    StageObject *who = gStageScene->controlled != nullptr ? gStageScene->controlled : gStageScene->player;
    return static_cast<StageCharacter *>(who);
}

// callcheck: isStageDrained

static StageCharacter *drainedCharacter()
{
    StageCharacter *who = playingCharacter();
    return who->isStageDrained() ? who : static_cast<StageCharacter *>(gStageScene->partner);
}

// 0x1d5 startDrain(frames, rate).
// FS2 0x001ae1d0
// callcheck: isStageDrained, FUN_0020d2c0

static const int *nativeStartDrain(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(2);
    drainedCharacter()->startDrainSlow(args[0], args[1]);
    *acc = 1;
    return code;
}

// 0x1d7 isDraining().
// FS2 0x001ae010
// callcheck: isStageDrained, isMotionSlowed

static const int *nativeIsDraining(ScriptRunner *, int *acc, const int *code)
{
    *acc = drainedCharacter()->isMotionSlowed();
    return code;
}

// 0x1d8 setSlowMotion(from, frames, to).
// FS2 0x001adf00
// callcheck: getScriptCharacter, FUN_0020d210

static const int *nativeSetSlowMotion(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(3);
    // PC: the ROM has no test.
    if (StageCharacter *who = self->getTarget()->getScriptCharacter())
        who->setSlowChange(args[0], args[1], args[2]);
    *acc = 1;
    return code;
}

// 0x1d9 clearSlowMotion().
// FS2 0x001ade60
// callcheck: getScriptCharacter, FUN_0020d1c0

static const int *nativeClearSlowMotion(ScriptRunner *self, int *acc, const int *code)
{
    // PC: the ROM has no test.
    if (StageCharacter *who = self->getTarget()->getScriptCharacter())
        who->clearSlowChange();
    *acc = 1;
    return code;
}

// The scene's three gadgets (StageScene::owned).
enum ScriptGadget
{
    ScriptGadget_SigLamp  = 0,
    ScriptGadget_NumLock  = 1,
    ScriptGadget_SevenKey = 2,
};

// 0x1e6 sigLampSlot2c().
// FS2 0x001acdf0

static const int *nativeSigLampSlot2c(ScriptRunner *, int *acc, const int *code)
{
    SceneOwnedObject *lamp = gStageScene->owned[ScriptGadget_SigLamp];
    *acc = lamp != nullptr && lamp->isSlot2c();
    return code;
}

// 0x1eb sigLampSlot44().
// FS2 0x001ac9b0

static const int *nativeSigLampSlot44(ScriptRunner *, int *acc, const int *code)
{
    *acc = gStageScene->owned[ScriptGadget_SigLamp]->isSlot44();
    return code;
}

// 0x1ee, 0x1ec, 0x204 the gadget's two names.
static const int *setGadgetNames(ScriptRunner *self, int gadget, const int *code)
{
    const char *first;
    const char *second;
    const int *next = self->readNameArg(&first, code);
    next = self->readNameArg(&second, next);
    gStageScene->owned[gadget]->setNames(first, second);
    return next;
}

// FS2 0x001ac750
// callcheck: readNameArg, readNameArg

static const int *nativeSigLampNames(ScriptRunner *self, int *, const int *code)
{
    return setGadgetNames(self, ScriptGadget_SigLamp, code);
}

// FS2 0x001ac900
// callcheck: readNameArg, readNameArg

static const int *nativeNumLockNames(ScriptRunner *self, int *, const int *code)
{
    return setGadgetNames(self, ScriptGadget_NumLock, code);
}

// FS2 0x001ab0d0
// callcheck: readNameArg, readNameArg

static const int *nativeSevenKeyNames(ScriptRunner *self, int *, const int *code)
{
    return setGadgetNames(self, ScriptGadget_SevenKey, code);
}

// 0x1ed canUseNumLock().
// FS2 0x001ac800
// callcheck: isInputBlocked, isSpecialAllowed

static const int *nativeCanUseNumLock(ScriptRunner *, int *acc, const int *code)
{
    static const char kNumLock[] = "NumLock";   /* data 0x304ad8 */
    *acc = !gStageScene->isInputBlocked() && playingCharacter()->isSpecialAllowed(kNumLock);
    return code;
}

// 0x1f0 placeRespawn(strong, x, y, z, degrees): the first character off stage
// of that sheet class who may come back returns there.
// FS2 0x001ac200
// callcheck: FUN_0021ef20, getStageSelect, canSpawn, isSpawnable, FUN_00218610

static const int *nativePlaceRespawn(ScriptRunner *self, int *acc, const int *code)
{
    constexpr int kClassWeak = 3;
    constexpr int kClassStrong = 7;
    constexpr float kRadians = 0.017453292f;
    const int *args = self->popped(5);
    const int wanted = args[0] == 0 ? kClassWeak : kClassStrong;
    const sceVu0FVECTOR position = { (float)args[1], (float)args[2], (float)args[3], 1.0f };
    const sceVu0FVECTOR rotation = { 0.0f, 0.0f, (float)args[4] * kRadians, 1.0f };
    StageObjArray *characters = &gStageScene->objects->m_lists[StageObjGroup_Characters];
    for (StageObject **slot = characters->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageCharacter *who = static_cast<StageCharacter *>(*slot);
        if (who->m_active != 0 || stageObjRecord(who->m_itemDef)->kind != wanted)
            continue;
        if (!who->canSpawn() && who->isSpawnable()) {
            who->setRespawnPlace(position, rotation);
            *acc = 1;
            return code;
        }
    }
    *acc = 0;
    return code;
}

// 0x1f3 resetCar().
// FS2 0x001ac050
// callcheck: FUN_0028dac0

static const int *nativeResetCar(ScriptRunner *, int *, const int *code)
{
    if (gStageScene->car != nullptr)
        gStageScene->car->resetPlace();
    return code;
}

// 0x1fc askNumLockOpen(), 0x1fd askNumLockClose().
// FS2 0x001ab800
// callcheck: scriptAskNumLock

static const int *nativeAskNumLockOpen(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptAskNumLock(0);
    return code;
}

// FS2 0x001ab790
// callcheck: scriptAskNumLock

static const int *nativeAskNumLockClose(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->getTarget()->scriptAskNumLock(1);
    return code;
}

// 0x1fe visionOf(name).
// FS2 0x001ab6c0
// callcheck: readNameArg, findCharacter, FUN_001fe7a0

static const int *nativeVisionOf(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    gStageScene->party->setVisionTarget(gStageScene->objects->findCharacter(name, 0));
    return next;
}

// 0x1ff visionAddA(name), 0x21e visionAddB(name).
// FS2 0x001ab5d0
// callcheck: readNameArg, findAny, FUN_001fe770

static const int *nativeVisionAddA(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    gStageScene->party->addVisionA(gStageScene->objects->findAny(name, 0));
    return next;
}

// FS2 0x001a92d0
// callcheck: readNameArg, findAny, FUN_001fe740

static const int *nativeVisionAddB(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    gStageScene->party->addVisionB(gStageScene->objects->findAny(name, 0));
    return next;
}

// 0x20d visionFrames(frames): the script counts 60 a second, the record 50.
// FS2 0x001aa640
// callcheck: FUN_001fe7b0

static const int *nativeVisionFrames(ScriptRunner *self, int *, const int *code)
{
    const int frames = self->popped(1)[0];
    gStageScene->party->setVisionFrames(frames * 5 / 6);
    return code;
}

// 0x201 numLockPush(a, b, c, d), 0x202 numLockPop(), 0x22c numLockScramble().
// FS2 0x001ab400
// callcheck: findNumLockTarget, FUN_002a6cd0

static const int *nativeNumLockPush(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(4);
    *acc = numLockPushDials(findNumLockTarget(), args[0], args[1], args[2], args[3]);
    return code;
}

// FS2 0x001ab380
// callcheck: findNumLockTarget, FUN_002a6c40

static const int *nativeNumLockPop(ScriptRunner *, int *acc, const int *code)
{
    *acc = numLockPopDials(findNumLockTarget());
    return code;
}

// FS2 0x001a8360
// callcheck: findNumLockTarget, FUN_002a6b70

static const int *nativeNumLockScramble(ScriptRunner *, int *, const int *code)
{
    numLockScramble(findNumLockTarget());
    return code;
}

// 0x211 showHidePart(index).
// FS2 0x001aa180
// callcheck: getScriptCharacter, show

static const int *nativeShowHidePart(ScriptRunner *self, int *, const int *code)
{
    const int index = self->popped(1)[0];
    // PC: the ROM has no test.
    if (StageCharacter *who = self->getTarget()->getScriptCharacter())
        who->m_hidePart->m_switch->show(index);
    return code;
}

// 0x218 setBroken(on): a light's and all under it.
// FS2 0x001a9990
// callcheck: cmdSetTreeBroken

static const int *nativeSetBroken(ScriptRunner *self, int *, const int *code)
{
    const int on = self->popped(1)[0];
    self->getTarget()->cmdSetTreeBroken(on != 0);
    return code;
}

// 0x21b playVoice(name).
// FS2 0x001a97b0
// callcheck: readNameArg, getScriptCharacter, FUN_002a65c0

static const int *nativePlayVoiceHeard(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const char *name;
    const int *next = self->readNameArg(&name, code);
    // PC: the ROM has no test.
    if (StageCharacter *who = target->getScriptCharacter())
        who->playVoiceHeard(name);
    return next;
}

// 0x21c unmarkWeapons(name): 0x1fb's mark comes off the active ones.
// FS2 0x001a94c0
// callcheck: readNameArg, findAll3, getKind

static const int *nativeUnmarkWeapons(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readNameArg(&name, code);
    StageObjArray *group = gStageScene->objects->findAll3(name);
    if (group == nullptr)
        return next;
    for (StageObject **slot = group->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *object = *slot;
        if (object->m_active != 0 && object->getKind() == StageObject_WeaponItem)
            static_cast<StageWeaponItem *>(object)->m_45c = 0;
    }
    return next;
}

// 0x21f nearLooseWeapon(range, mask): the script point goes to it; its handle.
// FS2 0x001a9160
// callcheck: getScriptCharacter, findLooseWeapon, set, getHandle

static const int *nativeNearLooseWeapon(ScriptRunner *self, int *acc, const int *code)
{
    const int *args = self->popped(2);
    *acc = 0;
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    if (who == nullptr)
        return code;
    StageObject *weapon = who->findLooseWeapon((float)args[0], (u_int)args[1]);
    if (weapon != nullptr) {
        who->m_scriptPoint.set(weapon->m_xform.position);
        *acc = (int)weapon->getHandle();
    }
    return code;
}

// 0x220 numLockFlag().
// FS2 0x001a90d0
// callcheck: FUN_002a8920

static const int *nativeNumLockFlag(ScriptRunner *, int *acc, const int *code)
{
    SceneOwnedObject *lock = gStageScene->owned[ScriptGadget_NumLock];
    *acc = lock != nullptr && lock->getRecordFlag31();
    return code;
}

// 0x228 isPartShown0(index): a map object's part shows part 0.
// FS2 0x001a88b0
// callcheck: getScriptObject, getKind, getKind

static const int *nativeIsPartAtZero(ScriptRunner *self, int *acc, const int *code)
{
    constexpr int kModelKindFirst = 1;
    constexpr int kModelKindLast = 6;
    const int index = self->popped(1)[0];
    *acc = 0;
    StageObject *object = self->getTarget()->getScriptObject();
    if (object == nullptr)
        return code;
    const int kind = object->getKind();
    if (kind < kModelKindFirst || kind > kModelKindLast)
        return code;
    MapObjectParts *parts = object->m_mapParts;
    // PC: the ROM has no test for the record.
    if (parts != nullptr && parts->switches != nullptr && index < parts->count)
        *acc = parts->switches[index].shown == 0;
    return code;
}

// 0x22a missionFlagD9().
// FS2 0x001a8640
// callcheck: currentMission

static const int *nativeMissionFlagD9(ScriptRunner *, int *acc, const int *code)
{
    *acc = gStageScene->currentMission()->_d9;
    return code;
}

// 0x39 hasAllyWithin(range).
// FS2 0x001cbfc0
// callcheck: scriptHasAllyWithin

static const int *nativeHasAllyWithin(ScriptRunner *self, int *acc, const int *code)
{
    const int range = self->popped(1)[0];
    *acc = self->getTarget()->scriptHasAllyWithin((float)range);
    return code;
}

// 0xae setMusicVolume(percent).
// FS2 0x001c3fa0
// callcheck: setMusicVolume

static const int *nativeSetMusicVolume(ScriptRunner *self, int *, const int *code)
{
    gStageScene->sound->setMusicVolume(self->popped(1)[0]);
    return code;
}

// 0xd2 isUp(): the target's own (0xd0 takes a name).
// FS2 0x001c1200
// callcheck: getScriptCharacter, getLifeState

static const int *nativeIsUpSelf(ScriptRunner *self, int *acc, const int *code)
{
    StageCharacter *who = self->getTarget()->getScriptCharacter();
    // PC: the ROM has no test.
    *acc = who != nullptr && who->getLifeState() == LifeState_Up;
    return code;
}

// 0x142 gameOver(): once input is free; the target's actor falls, by whom
// last hit it.
// FS2 0x001b9cc0
// callcheck: isInputBlocked, scriptYield, getScriptActor, startGameOver, startGameOver

static const int *nativeGameOver(ScriptRunner *self, int *, const int *code)
{
    enum { Native = 0x142 };
    ScriptRunner *target = self->getTarget();
    takeScriptYield();
    if (gStageScene->isInputBlocked()) {
        scriptYield(Native, 0);
        return code;
    }
    StageCharacter *actor = target->getScriptActor();
    gStageScene->startGameOver(actor, actor != nullptr ? actor->m_lastHitBy : nullptr);
    return code;
}

// 0x17d effectsOff(name): 0x195's code.
// FS2 0x001b5280
// callcheck: readStringArg, setEffectsOn

static const int *nativeEffectsOff2(ScriptRunner *self, int *, const int *code)
{
    const char *name;
    const int *next = self->readStringArg(&name, code);
    setEffectsOn(name, false);
    return next;
}

// 0x191 demoCutOverlay(name, overlay): 0x159 with the overlay SJN or SJE in
// its own argument, where 0x159's is a null pointer.
// PC: the kind test: the ROM calls the slot on whatever is on top.
// FS2 0x001b3ae0
// callcheck: readStringArg, readStringArg, scriptYield, getTopView, playCutNamed, strcmp, strcmp, setOverlay

static const int *nativeDemoCutOverlay(ScriptRunner *self, int *, const int *code)
{
    enum { Native = 0x147, OverlayNone = 0, OverlaySjn = 1, OverlaySje = 2 };
    const char *name = nullptr;
    const char *overlayName = nullptr;
    const int *next = self->readStringArg(&name, code);
    next = self->readStringArg(&overlayName, next);
    takeScriptYield();
    if (self->m_cutWaited == 0) {
        self->m_cutWaited = 1;
        scriptYield(Native, 0);
        return next;
    }
    int overlay = OverlayNone;
    if (overlayName != nullptr) {
        if (strcmp(overlayName, "SJN") == 0)        /* data 0x304b58 */
            overlay = OverlaySjn;
        else if (strcmp(overlayName, "SJE") == 0)   /* data 0x304b60 */
            overlay = OverlaySje;
    }
    StageObject *top = gStageScene->getTopView();
    if (top != nullptr && top->getKind() == StageObject_Kind13) {
        DemoCamera *camera = static_cast<DemoCamera *>(top);
        camera->playCutNamed(name);
        camera->setOverlay(overlay);
        camera->m_demoCut++;
    }
    return next;
}

// 0x1dc askRouteKind11(): of the script's own runner, not its target.
// FS2 0x001adbc0
// callcheck: scriptAskRouteKind11

static const int *nativeAskRouteKind11(ScriptRunner *self, int *acc, const int *code)
{
    *acc = self->scriptAskRouteKind11();
    return code;
}

// 0x1f5 missionFlagD8().
// FS2 0x001abf50
// callcheck: currentMission

static const int *nativeMissionFlagD8(ScriptRunner *, int *acc, const int *code)
{
    *acc = gStageScene->currentMission()->_d8;
    return code;
}

// 0x205 sevenKeyNumbers(a, b, c, d, name).
// FS2 0x001aafd0
// callcheck: readNameArg, FUN_002b2b70

static const int *nativeSevenKeyNumbers(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(4);
    const char *name;
    const int *next = self->readNameArg(&name, code);
    gStageScene->owned[ScriptGadget_SevenKey]->setSevenKeyNumbers(name, args[0], args[1], args[2], args[3]);
    return next;
}

// A clock of the result (StageResult::clock, clock2): state 1 with a target
// starts it (0x222, 0x224); 0x225, 0x226 ask whether it is off or has both
// of the bits it ends with.
static bool isClockFree(const StageClock &clock)
{
    constexpr u_int kClockEnded = 0x14;
    return clock.state == 0 || (clock.state & kClockEnded) == kClockEnded;
}

// 0x222 startClock(frames30).
// FS2 0x001a8f40
// callcheck: scriptFramesFrom30, getStageResult

static const int *nativeStartClock(ScriptRunner *self, int *, const int *code)
{
    const int frames = scriptFramesFrom30(self->popped(1)[0]);
    StageResult *result = getStageResult();
    result->clock.state = 1;
    result->clock.target = frames;
    return code;
}

// 0x223 addClock2(amount).
// FS2 0x001a8ea0
// callcheck: getStageResult

static const int *nativeAddClock2(ScriptRunner *self, int *, const int *code)
{
    const int amount = self->popped(1)[0];
    getStageResult()->clock2.value += amount;
    return code;
}

// 0x224 startClock2(target).
// FS2 0x001a8e00
// callcheck: getStageResult

static const int *nativeStartClock2(ScriptRunner *self, int *, const int *code)
{
    const int target = self->popped(1)[0];
    StageResult *result = getStageResult();
    result->clock2.state = 1;
    result->clock2.target = target;
    return code;
}

// 0x225 clockFree(), 0x226 clock2Free().
// FS2 0x001a8d50
// callcheck: getStageResult

static const int *nativeClockFree(ScriptRunner *, int *acc, const int *code)
{
    *acc = isClockFree(getStageResult()->clock);
    return code;
}

// FS2 0x001a8ca0
// callcheck: getStageResult

static const int *nativeClock2Free(ScriptRunner *, int *acc, const int *code)
{
    *acc = isClockFree(getStageResult()->clock2);
    return code;
}

// 0x22b spawnEffectOnNumber(number, x, y, z, rx, ry, rz, effect, bone).
// FS2 0x001a83a0
// callcheck: readNameArg, readNameArg, currentMission, spawnEffectOnBone

static const int *nativeSpawnEffectOnNumber(ScriptRunner *self, int *acc, const int *code)
{
    constexpr float kRadians = 0.017453292f;
    const int *args = self->popped(7);
    const char *name;
    const char *bone;
    const int *next = self->readNameArg(&name, code);
    next = self->readNameArg(&bone, next);
    const sceVu0FVECTOR place = { (float)args[1], (float)args[2], (float)args[3], 1.0f };
    const sceVu0FVECTOR turn = { (float)args[4] * kRadians, (float)args[5] * kRadians, (float)args[6] * kRadians, 1.0f };
    StageCharacter *who = scriptCharacterAt(args[0]);
    // PC: the ROM has no test.
    *acc = who != nullptr ? spawnEffectOnBone(who, name, bone, place, turn) : -1;
    return next;
}

// 0x230 playSoundAtPlace(x, y, z, sound).
// FS2 0x001a7fe0
// callcheck: readNameArg, getScriptObject, playNamedHeard

static const int *nativePlaySoundAtPlace(ScriptRunner *self, int *, const int *code)
{
    ScriptRunner *target = self->getTarget();
    const int *args = self->popped(3);
    const char *sound;
    const int *next = self->readNameArg(&sound, code);
    const sceVu0FVECTOR place = { (float)args[0], (float)args[1], (float)args[2], 1.0f };
    if (StageObject *me = target->getScriptObject())
        me->playNamedHeard(sound, place, nullptr);
    return next;
}

// By how many scripts call them (sssall.py); the ROM's numbers.
static const ScriptNativeBinding kAskedNatives[] = {
    { 0x039, nativeHasAllyWithin },
    { 0x0ae, nativeSetMusicVolume },
    { 0x0d2, nativeIsUpSelf },
    { 0x142, nativeGameOver },
    { 0x17d, nativeEffectsOff2 },
    { 0x191, nativeDemoCutOverlay },
    { 0x1dc, nativeAskRouteKind11 },
    { 0x1f5, nativeMissionFlagD8 },
    { 0x205, nativeSevenKeyNumbers },
    { 0x222, nativeStartClock },
    { 0x223, nativeAddClock2 },
    { 0x224, nativeStartClock2 },
    { 0x225, nativeClockFree },
    { 0x226, nativeClock2Free },
    { 0x22b, nativeSpawnEffectOnNumber },
    { 0x230, nativePlaySoundAtPlace },
    { 0x03b, nativeSwingAtBite },
    { 0x043, nativeAddPlainMenuLine },
    { 0x055, nativeIsCompanionName },
    { 0x056, nativeIsBlowActiveOf },
    { 0x058, nativeIsRunningName },
    { 0x062, nativeCallOut },
    { 0x081, nativePickableItem },
    { 0x0c2, nativeClearFoeWeight },
    { 0x0d3, nativeIsPlayer },
    { 0x0ed, nativeIsCrouchingNumber },
    { 0x12b, nativeSpawnNearPlace },
    { 0x130, nativeFireAtBite },
    { 0x131, nativeFireAtName },
    { 0x135, nativeHideEffect },
    { 0x140, nativeBulletFlyingNumber },
    { 0x14d, nativeNoticeFor },
    { 0x14e, nativeNoticeForArg },
    { 0x154, nativeCarFlag3b9 },
    { 0x15a, nativeUseItemAt },
    { 0x15c, nativeDoorNotUsed },
    { 0x170, nativeCarLightsOn },
    { 0x177, nativeKeepSpawnPointListAll },
    { 0x17a, nativeSetHoldMotion },
    { 0x17c, nativeEffectsOn },
    { 0x18d, nativeRideCar },
    { 0x18e, nativeSetPlayerCarFlag },
    { 0x18f, nativePlayerCarFlag },
    { 0x193, nativeAttachEffect },
    { 0x194, nativeEffectsOn },
    { 0x195, nativeEffectsOff },
    { 0x198, nativeCarPlayMotion },
    { 0x19a, nativeCarMotionEnding },
    { 0x19e, nativeSetCarValue394 },
    { 0x1ac, nativeSetCarFlag3a0 },
    { 0x1c8, nativeSetCarMode },
    { 0x1ca, nativeElevatorAction },
    { 0x1cb, nativeUseElevator },
    { 0x1cc, nativeElevatorMove },
    { 0x1cd, nativeElevatorRelease },
    { 0x1ce, nativeElevatorFloor },
    { 0x1cf, nativeElevatorPhase },
    { 0x1d0, nativeInCompanionMode },
    { 0x1d3, nativeSetNoticeEnabled },
    { 0x1d5, nativeStartDrain },
    { 0x1d7, nativeIsDraining },
    { 0x1d8, nativeSetSlowMotion },
    { 0x1d9, nativeClearSlowMotion },
    { 0x1e6, nativeSigLampSlot2c },
    { 0x1eb, nativeSigLampSlot44 },
    { 0x1ec, nativeNumLockNames },
    { 0x1ed, nativeCanUseNumLock },
    { 0x1ee, nativeSigLampNames },
    { 0x1f0, nativePlaceRespawn },
    { 0x1f3, nativeResetCar },
    { 0x1fc, nativeAskNumLockOpen },
    { 0x1fd, nativeAskNumLockClose },
    { 0x1fe, nativeVisionOf },
    { 0x1ff, nativeVisionAddA },
    { 0x201, nativeNumLockPush },
    { 0x202, nativeNumLockPop },
    { 0x204, nativeSevenKeyNames },
    { 0x20d, nativeVisionFrames },
    { 0x211, nativeShowHidePart },
    { 0x218, nativeSetBroken },
    { 0x21b, nativePlayVoiceHeard },
    { 0x21c, nativeUnmarkWeapons },
    { 0x21d, nativeElevatorFloorFree },
    { 0x21e, nativeVisionAddB },
    { 0x21f, nativeNearLooseWeapon },
    { 0x220, nativeNumLockFlag },
    { 0x228, nativeIsPartAtZero },
    { 0x22a, nativeMissionFlagD9 },
    { 0x22c, nativeNumLockScramble },
    { 0x022, nativeSpawnNamed },
    { 0x023, nativePosX },
    { 0x024, nativePosY },
    { 0x025, nativePosZ },
    { 0x02d, nativeSwitchOn },
    { 0x032, nativeCanSeeNumber },
    { 0x033, nativeCanSeeName },
    { 0x034, nativeCanSeeWithin },
    { 0x035, nativeMarkPlace },
    { 0x03d, nativeHeardAllyAgain },
    { 0x040, nativeHeightToNumber },
    { 0x041, nativeHeightToName },
    { 0x04a, nativeReload },
    { 0x050, nativeBlockPath },
    { 0x051, nativeAllowPath },
    { 0x054, nativeHasLineToName },
    { 0x057, nativeIsCrouchingName },
    { 0x059, nativeRouteAction12c },
    { 0x05a, nativeRouteAction130 },
    { 0x05c, nativeSpawnEffectOnObject },
    { 0x05d, nativeSpawnEffectOn },
    { 0x05e, nativeSpawnEffectPlaced },
    { 0x061, nativeCrouch },
    { 0x068, nativePlayMotion },
    { 0x06d, nativeIsCompanion },
    { 0x06e, nativeIsBlowActive },
    { 0x07f, nativeNothing7f },
    { 0x080, nativePutWeaponAway },
    { 0x082, nativeWakeBrain },
    { 0x085, nativeClimbPoint },
    { 0x086, nativeAskLiftName },
    { 0x08e, nativeNearestAllyDistance },
    { 0x08f, nativeUnnoticedFoe },
    { 0x090, nativeLastHitBy },
    { 0x091, nativeNearestFoeDistance },
    { 0x092, nativeNearestFoe },
    { 0x093, nativeHeardAlly },
    { 0x095, nativeTakeCorpse },
    { 0x096, nativeLightReach },
    { 0x097, nativeAddFoeWeightNamed },
    { 0x098, nativeAddFoeWeightInView },
    { 0x099, nativeAddFoeWeightOutOfView },
    { 0x09a, nativeBestFoe },
    { 0x09b, nativeFoeWeightNamed },
    { 0x09c, nativeFoeWeight },
    { 0x09d, nativeSpawnAt },
    { 0x09e, nativeSpawnNear },
    { 0x09f, nativeLeaveNoticed },
    { 0x0a0, nativeKeepSpawnPoint },
    { 0x0a1, nativeDropSpawnPoint },
    { 0x0a3, nativeNearestCorpse },
    { 0x0a4, nativeHeardWithin },
    { 0x0a5, nativeLoseItem },
    { 0x0a6, nativeLoseItems },
    { 0x0a7, nativeShowItems },
    { 0x0a9, nativeHeardFoe },
    { 0x0aa, nativeUseLockGadget },
    { 0x0ab, nativeStopSound },
    { 0x0b0, nativePlayObjectMotion },
    { 0x0b2, nativeObjectLightOn },
    { 0x0b3, nativeDoorState },
    { 0x0b6, nativePlaySoundAtVolume },
    { 0x0be, nativeBiteTargetNumber },
    { 0x0bf, nativeIsBittenNumber },
    { 0x0c0, nativeIsBittenName },
    { 0x0c1, nativeForgetFoe },
    { 0x0c4, nativeSearchPoint },
    { 0x0c5, nativeSearchPointLast },
    { 0x0c7, nativeAlertOnName },
    { 0x0c8, nativeAlertOnNumber },
    { 0x0ce, nativeIsOnStage },
    { 0x0d0, nativeIsUp },
    { 0x0dc, nativePercentOf },
    { 0x0dd, nativeAbs },
    { 0x0e1, nativeClamp },
    { 0x0e2, nativeHeardAny },
    { 0x0e4, nativeAttackBite },
    { 0x0e5, nativeDistanceToNumber },
    { 0x0e6, nativePathToXyz },
    { 0x0e9, nativeInReach },
    { 0x0ea, nativeHasLineTo },
    { 0x0f2, nativeAskLift },
    { 0x0f3, nativeAddFoeWeight },
    { 0x0f9, nativePickedItem },
    { 0x0ff, nativeSearchPointAt },
    { 0x100, nativeSearchPointNext },
    { 0x104, nativeUseItem },
    { 0x105, nativeUseSmoke },
    { 0x106, nativePutItem },
    { 0x107, nativeSeesFlyingItem },
    { 0x108, nativeGoToThrow },
    { 0x109, nativeGoToLanding },
    { 0x10a, nativeSeesBurner },
    { 0x10b, nativeGoToLyingItem },
    { 0x10c, nativeBurnerIn },
    { 0x10d, nativeSetTrap },
    { 0x10e, nativeSeesOpenTrap },
    { 0x10f, nativeCanTakeUpTrap },
    { 0x110, nativeTakeUpTrap },
    { 0x11f, nativeTakeNamedItem },
    { 0x121, nativeDropLongWeapon },
    { 0x122, nativePickUp },
    { 0x123, nativeNearWeapon },
    { 0x127, nativeObjectLightOff },
    { 0x128, nativeSetVisibility },
    { 0x12a, nativeSpawnEffectAt },
    { 0x12e, nativeSwing },
    { 0x132, nativeShowGroup1 },
    { 0x137, nativeSpringTraps },
    { 0x13b, nativeLeaveQuiet },
    { 0x13e, nativeAlertCry },
    { 0x13f, nativeBulletFlying },
    { 0x143, nativeIsCorpse },
    { 0x15e, nativeOverlayKind },
    { 0x166, nativeIsWalking },
    { 0x167, nativeCanGetOnCar },
    { 0x168, nativePlayerGetOnCar },
    { 0x169, nativePlayerGetOffCar },
    { 0x16b, nativeCanGetOffCar },
    { 0x16c, nativeCanDragOutCar },
    { 0x16d, nativeDragOutCar },
    { 0x16f, nativeCarLights },
    { 0x171, nativeHasOverlay },
    { 0x172, nativeHasOverlayKind },
    { 0x173, nativeKeepAllSpawnPoints },
    { 0x174, nativeDropSpawnPoints },
    { 0x175, nativeKeepSpawnPoints },
    { 0x176, nativeDropSpawnPointRange },
    { 0x178, nativeKeepSpawnPointList },
    { 0x179, nativeReleaseAction },
    { 0x17e, nativeLeave },
    { 0x17f, nativeLeaveAtOnce },
    { 0x185, nativeShift },
    { 0x18b, nativeAngleToName },
    { 0x18c, nativeAngleTo },
    { 0x192, nativeEventEffect },
    { 0x19b, nativeIsMotionEnding },
    { 0x1a9, nativeShowNotice },
    { 0x1b5, nativeAddGroupColorKey },
    { 0x1bf, nativeWasHit },
    { 0x1c0, nativeSpawnAtLater },
    { 0x1c4, nativeSetGroupColorLoop },
    { 0x1c5, nativeSpawnNearLater },
    { 0x1c6, nativeTimedRecords },
    { 0x1d1, nativeHurtBy },
    { 0x1d2, nativeHurtByName },
    { 0x1dd, nativeZero },
    { 0x1de, nativeDarkness },
    { 0x1e7, nativeCanUseSigLamp },
    { 0x1e8, nativeUseSigLamp },
    { 0x1e9, nativeSigLampHasTarget },
    { 0x1ef, nativeNearNamed },
    { 0x1f4, nativeIsNearViewTarget },
    { 0x1f6, nativeLetterbox },
    { 0x1f9, nativeSmokeNear },
    { 0x1fb, nativeMarkWeapons },
    { 0x200, nativeTakeOverState },
    { 0x206, nativeCanUseSevenKey },
    { 0x207, nativeRouteObject },
    { 0x208, nativeIsInSight },
    { 0x209, nativeIsObjectInSight },
    { 0x20a, nativeIsBroken },
    { 0x20b, nativeIsStanding },
    { 0x20e, nativeResetTraps },
    { 0x212, nativeBreakWindow },
    { 0x213, nativeIsFatal },
    { 0x214, nativeFireAt },
    { 0x21a, nativeSetMotionLabel },
    { 0x227, nativeBlowNear },
    { 0x234, nativePlayMusicOn },
    { 0x235, nativeStopMusicOn },
};

// The ROM indexes table 0x304230 by id - 0x19.
// FS2 0x001a7a20

static const int *callNative(ScriptRunner *self, int id, const int *code, int *acc)
{
    static ScriptNative table[kNativeCount];
    static bool built;
    static bool reported[kNativeCount];
    if (!built) {
        built = true;
        for (const ScriptNativeBinding &binding : kNatives)
            table[binding.id - kFirstNative] = binding.native;
        for (const ScriptNativeBinding &binding : kDoorNatives)
            table[binding.id - kFirstNative] = binding.native;
        for (const ScriptNativeBinding &binding : kStageNatives)
            table[binding.id - kFirstNative] = binding.native;
        for (const ScriptNativeBinding &binding : kAskedNatives)
            table[binding.id - kFirstNative] = binding.native;
    }

    // PC: an id off the table would be a wild jump in the ROM.
    if (id < kFirstNative || id > kLastNative)
        return code;
    int index = id - kFirstNative;
    if (table[index] != nullptr)
        return table[index](self, acc, code);

    // PC: not written yet: pop and skip as the ROM's would, and say so.
    const ScriptNativeInfo &info = kNativeInfo[index];
    if (!reported[index]) {
        reported[index] = true;
        printf("[stub] scriptNative_%x (FS2 0x%08x) reached\n", id, info.address);
    }
    self->dropArgs(info.pops);
    return code + info.words;
}
