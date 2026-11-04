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

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "common/yamipan_stub.h"
#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/mathutil.h"
#include "game/scenerender.h"
#include "game/stagelight.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
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

// The player, or what acts for it. PC: StagePlayer is a stand-in for the
// character class, a StageObject in the ROM.
static StageObject *scenePlayer()
{
    StageObject *object = gStageScene->controlled;
    return object != nullptr ? object : reinterpret_cast<StageObject *>(gStageScene->player);
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

enum { kFirstNative = 0x19, kLastNative = 0x235, kNativeCount = kLastNative - kFirstNative + 1 };

// What the VM does for a native nobody wrote yet.
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
static const ScriptNativeBinding kNatives[] = {
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
