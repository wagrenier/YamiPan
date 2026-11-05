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
#include "game/wstring.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "common/yamipan_stub.h"
#include "game/characterbrain.h"
#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/mathutil.h"
#include "game/scenerender.h"
#include "game/stagelight.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
#include "game/subsceneobject.h"
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
// PC: the pick 0x21a040 / 0x21a1c0 reads the sight table +0x468 the brain
// fills, which is not written, so nobody is seen.
// FS2 0x001cbf20
// callcheck: FUN_0021a040

static const int *nativeSeenEnemy(ScriptRunner *, int *acc, const int *code)
{
    *acc = 0;
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

// 0x6b playSoundAt(sound, object): "NULL" plays it flat, an object at its
// place, and what can hear there reports it. Runner slot +0x148.
// PC: the sound comes out at the object's own place: the ROM takes bn_top
// for an object of kinds 1..6 with a model, and a name that is no object
// from the map's points (0x190650, unwritten: it stays silent here).
// FS2 0x001c84b0
// callcheck: readNameArg, readObjectArg, strcmp, playNamed, getKind, getKind, find, FUN_00190650, getMatrix

static const int *nativePlaySoundAt(ScriptRunner *self, int *, const int *code)
{
    StageObject *me = self->getTarget()->getScriptObject();
    const char *sound;
    const char *label;
    const int *next = self->readNameArg(&sound, code);
    next = self->readObjectArg(&label, next);
    if (strcmp(label, kNoObjectName) == 0) {
        gStageScene->sound->playNamed(sound, 1.0f);
        return next;
    }
    StageObject *object = (StageObject *)scriptLabelCell(label);
    if (object != nullptr && me != nullptr)
        me->playNamedHeard(sound, object->getMatrix()[3], nullptr);
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

// 0x20f motesAt(kind, count, range, object).
// FS2 0x001aa3a0
// callcheck: readObjectArg, spawnMotes

static const int *nativeMotesAt(ScriptRunner *self, int *, const int *code)
{
    const int *args = self->popped(3);
    const char *label;
    const int *next = self->readObjectArg(&label, code);
    StageObject *object = (StageObject *)scriptLabelCell(label);
    // PC: as nativePlaceAt.
    if (object != nullptr)
        gStageScene->nodeTree->spawnMotes(object->m_xform.position, (float)args[2], args[0], args[1]);
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
    { 0x20f, nativeMotesAt },
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
