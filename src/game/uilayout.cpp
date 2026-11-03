// game/uilayout.cpp -- the 2D layout player: UiLayout, its script
// interpreter and the nested layout it can hold (game/uilayout.h).

#include "game/uilayout.h"

#include <stdio.h>
#include <string.h>

#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/heap.h"
#include "game/pad.h"
#include "game/sysdata.h"
#include "game/thread.h"
#include "sdk/scetypes.h"

// The sequence array grows by this many entries.
constexpr int kSeqArrayGrow = 16;

// The 16-bit halves of a packed command argument (signed).
static inline int argLo(int arg)
{
    return (short)arg;
}

static inline int argHi(int arg)
{
    return (short)(arg >> 16);
}

// An anim-set index packed as lo(arg0), plus the value of the variable
// hi(arg1) names when it is not negative (runCommands' ops 25..30).
static inline int packedAnimSet(S2dData *data, int arg0, int arg1)
{
    int index = argLo(arg0);
    if (argHi(arg1) >= 0)
        index += data->getVar(argHi(arg1))->value;
    return index;
}

// ──────────────────────────────────────────────────────────────────────
// Opens a nested layout: path under dir. "@name" inside a stage is the
// stage pack's dir + name, opened at once; anywhere else the file is
// loaded by a job and opened by advance once it is in.
// FS2 0x00243390
// callcheck: operator_new (the new-expression)

void UiSubLayout::load(const char *dir, const char *path, int arg)
{
    close();
    m_arg = arg;
    if (path == nullptr)
        return;
    if (path[0] == '@') {
        if (inStage()) {
            char name[32];
            sprintf(name, "%s%s", dir, path + 1);
            open(getStagePackData(name));
            return;
        }
        path++;
    }
    m_job = new FileJob(path, 0);
    m_job->m_dir = dir;
    m_job->start();
    m_state = SubLayout_Loading;
}

// ──────────────────────────────────────────────────────────────────────
// The child's update; it closes once it has a result.
// FS2 0x002434c0

void UiSubLayout::update()
{
    if (m_state == SubLayout_Open) {
        m_child->update();
        if (m_child->m_result >= 0)
            close();
    }
}

// ──────────────────────────────────────────────────────────────────────
// The child's advance, or the load's end: opened when the job is done,
// closed when it failed.
// FS2 0x00243520
// callcheck: advance (UiLayout::advance, inline in the ROM: the sequences' and the base's)

void UiSubLayout::advance()
{
    if (m_state == SubLayout_Closing) {
        close();
    } else if (m_state == SubLayout_Open) {
        m_child->advance();
    } else if (m_state == SubLayout_Loading) {
        if (m_job->m_state == FileJob_Done)
            open(m_job->m_data);
        else if (m_job->m_state & FileJob_Failed)
            close();
    }
}

// ──────────────────────────────────────────────────────────────────────
// A child layout on an S2D file's data; null, or data setData refuses,
// closes instead. The child gets its first advance here.
// FS2 0x00243630
// callcheck: operator_new, UiLayout (the new-expression)
// callcheck: operator_delete[], strlen, operator_new[], strcpy (setBaseDir, inline in the ROM)
// callcheck: advance (UiLayout::advance, inline in the ROM: the sequences' and the base's)

bool UiSubLayout::open(void *data)
{
    if (data == nullptr) {
        close();
        return false;
    }
    m_child = new UiLayout;
    m_child->setBaseDir(m_childDir);
    if (!m_child->setData(data, m_arg)) {
        close();
        return false;
    }
    m_state = SubLayout_Open;
    m_child->advance();
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// Ends the child (keeping its result) and the load.
// FS2 0x00243770
// callcheck: stopSound, removeSequence, set, operator_delete[], ~UiDrawer, close, operator_delete (the delete-expression, ~UiLayout inline)
// callcheck: ~FileJob (the delete-expression)

void UiSubLayout::close()
{
    if (m_child != nullptr) {
        m_lastResult = m_child->m_result;
        delete m_child;
    }
    m_child = nullptr;
    m_state = SubLayout_None;
    if (m_job != nullptr) {
        delete m_job;
        m_job = nullptr;
    }
    m_childDir = nullptr;
    m_18 = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00243890

void UiVec4::set(float x_, float y_, float z_, float w_)
{
    x = x_;
    y = y_;
    z = z_;
    w = w_;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002438b0
// callcheck: stopSound, removeSequence, set, operator_delete[], ~UiDrawer, close, operator_delete, ~FileJob (close, inline in the ROM)

UiSubLayout::~UiSubLayout()
{
    close();
}

// ──────────────────────────────────────────────────────────────────────
// Draws the sequences in priority order through the drawer, then the
// child on top; answers the sequence count.
// FS2 0x002439f0

int UiLayout::draw(sceVif1Packet *packet, void *texList)
{
    m_drawer->begin(packet, texList);
    m_drawer->setMode(m_drawMode ? 1 : 0);
    sortSequences();
    for (int i = 0; i < m_seqCount; i++) {
        UiSequence *seq = m_seqs[i];
        if (seq != nullptr)
            seq->draw(m_drawer);
    }
    m_drawer->end();
    if (m_state == SubLayout_Open)
        m_child->draw(packet, texList);
    return m_seqCount;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00243b90

void UiLayout::advance()
{
    for (int i = 0; i < m_seqCount; i++) {
        if (m_seqs[i] != nullptr)
            m_seqs[i]->advance();
    }
    UiSubLayout::advance();
}

// ──────────────────────────────────────────────────────────────────────
// Each sequence's frame commands once, in priority order; a pass that
// starts or stops a sequence (or yields with a positive id) begins again
// with the ones not run yet. Then the child.
// FS2 0x00243c10

void UiLayout::update()
{
    sortSequences();
    for (int i = 0; i < m_seqCount; i++) {
        if (m_seqs[i] != nullptr)
            m_seqs[i]->m_pending = 1;
    }
    int changed;
    do {
        changed = 0;
        for (int i = 0; i < m_seqCount; i++) {
            UiSequence *seq = m_seqs[i];
            if (seq != nullptr && seq->m_pending == 1) {
                seq->m_pending = 0;
                changed = runCommands(m_seqs[i], S2dTrigger_Frame);
                if (changed > 0)
                    break;
            }
        }
    } while (changed > 0);
    UiSubLayout::update();
}

// ──────────────────────────────────────────────────────────────────────
// Runs a sequence's stop commands and removes it; -1 when it is not
// running.
// FS2 0x00243dc0
// callcheck: findSequence (inline in the ROM)

int UiLayout::stopSequence(int id)
{
    if (id < 0)
        return -1;
    UiSequence *seq = findSequence(id);
    if (seq == nullptr)
        return -1;
    runCommands(seq, S2dTrigger_Stop);
    removeSequence(id);
    sortSequences();
    return id;
}

// ──────────────────────────────────────────────────────────────────────
// The script interpreter: from the sequence's cursor, every command whose
// trigger matches and whose condition holds runs; the others skip
// themselves and their skip count. The cursor goes back to the first
// command at the end of the pass (not after StopSelf, Yield or Restart).
// Answers the sequences started and stopped, or the sequence's id when a
// Yield / Restart ends the pass.
// FS2 0x00243f40
// callcheck: findSequence (inline in the ROM)

int UiLayout::runCommands(UiSequence *seq, int trigger)
{
    int changes = 0;
    if (seq == nullptr)
        return -1;

    S2dCmd *cmd = seq->m_cursor;
    if (cmd != nullptr && cmd >= seq->m_cmdEnd)
        cmd = nullptr;
    while (cmd != nullptr) {
        bool run = false;
        if (cmd->trigger == S2dTrigger_Start) {
            if (trigger == S2dTrigger_Start)
                run = true;
        } else if (cmd->trigger == S2dTrigger_Stop) {
            if (trigger == S2dTrigger_Stop)
                run = true;
        } else if (trigger == S2dTrigger_Frame) {
            run = true;
        }
        if (run && !testCondition(seq, &cmd->cond))
            run = false;
        if (!run) {
            cmd = seq->step(cmd->skip + 1);
            continue;
        }

        switch (cmd->op) {
        case S2dOp_AddAnimSet: {
            int index = cmd->arg0;
            if (cmd->arg1 != 0)
                index += m_data->findVar(m_data->getString(cmd->arg1))->value;
            seq->addAnimSet(m_data->animSets[index]);
            break;
        }
        case S2dOp_SetVar:
            m_data->vars[cmd->arg0].value = cmd->arg1;
            break;
        case S2dOp_AddVar:
            m_data->vars[cmd->arg0].add(cmd->arg1);
            break;
        case S2dOp_StopSelf:
            stopSequence(seq->m_id);
            return changes + 1;
        case S2dOp_StopSequence:
            stopSequence(cmd->arg0);
            changes++;
            break;
        case S2dOp_StartSequence:
            startSequence(cmd->arg0, cmd->arg1);
            changes++;
            break;
        case S2dOp_Yield:
            if (cmd->cond.kind == S2dCond_Always) {
                if (seq->step(1) == nullptr)
                    seq->m_cursor = seq->m_cmdBegin;
            }
            return seq->m_id;
        case S2dOp_Restart:
            seq->m_cursor = seq->m_cmdBegin;
            return seq->m_id;
        case S2dOp_CreateWidget:
            UiWidget::create(seq, &m_data->widgetDefs[cmd->arg0], this, cmd->arg1);
            break;
        case S2dOp_RemoveWidget: {
            S2dWidgetDef *def = &m_data->widgetDefs[cmd->arg0];
            if (def != nullptr)
                seq->removeWidget(def->id);
            break;
        }
        case S2dOp_CopyVar:
            m_data->vars[cmd->arg0].value = m_data->vars[cmd->arg1].value;
            break;
        case S2dOp_RemoveAnimSet: {
            int index = cmd->arg0;
            if (cmd->arg1 != 0) {
                S2dVar *var = m_data->findVar(m_data->getString(cmd->arg1));
                if (var != nullptr)
                    index += var->value;
            }
            S2dAnimSet *set = m_data->animSets[index];
            if (set != nullptr) {
                for (u_int i = 0; i < set->count; i++) {
                    if (set->entries[i].anim == -1)
                        seq->removeActor(set->entries[i].actorId);
                }
            }
            break;
        }
        case S2dOp_AddAnimSetTo: {
            UiSequence *target = findSequence(cmd->arg1);
            if (target != nullptr)
                target->addAnimSet(m_data->animSets[cmd->arg0]);
            break;
        }
        case S2dOp_RemoveAnimSetFrom: {
            UiSequence *target = findSequence(cmd->arg1);
            S2dAnimSet *set;
            if (target != nullptr && (set = m_data->animSets[cmd->arg0]) != nullptr) {
                for (u_int i = 0; i < set->count; i++) {
                    if (set->entries[i].anim == -1)
                        target->removeActor(set->entries[i].actorId);
                }
            }
            break;
        }
        case S2dOp_End:
            if (cmd->arg0 < 0)
                m_result = cmd->arg1;
            else
                m_result = m_data->getVar(cmd->arg0)->value;
            m_done = 1;
            break;
        case S2dOp_SetSeqValue: {
            UiSequence *target = cmd->arg0 >= 0 ? findSequence(cmd->arg0) : seq;
            if (target != nullptr)
                target->m_value = cmd->arg1;
            break;
        }
        case S2dOp_SetSeqValueVar: {
            UiSequence *target = cmd->arg0 >= 0 ? findSequence(cmd->arg0) : seq;
            if (target != nullptr)
                target->m_value = m_data->getVar(cmd->arg1)->value;
            break;
        }
        case S2dOp_Jump:
            if (seq->m_cursor != nullptr)
                seq->m_cursor += cmd->arg1;
            break;
        case S2dOp_PlaySound:
            playSound(cmd->arg1, m_data->getString(cmd->arg0));
            break;
        case S2dOp_StopSound:
            stopSound(cmd->arg1);
            break;
        case S2dOp_OpenLayout:
            load(m_baseDir, m_data->getString(cmd->arg0), cmd->arg1);
            break;
        case S2dOp_WriteGameValue:
            writeGameValue(cmd->arg0, cmd->arg1);
            break;
        case S2dOp_ReadGameValue:
            readGameValue(cmd->arg0, cmd->arg1);
            break;
        case S2dOp_AddVarVar:
            m_data->vars[cmd->arg0].add(m_data->vars[cmd->arg1].value);
            break;
        case S2dOp_SetFrame: {
            int index = packedAnimSet(m_data, cmd->arg0, cmd->arg1);
            seq->setAnimSetFrame(m_data->animSets[index], argLo(cmd->arg1));
            break;
        }
        case S2dOp_SetFrameVar: {
            int frame = m_data->getVar(argLo(cmd->arg1))->value;
            int index = packedAnimSet(m_data, cmd->arg0, cmd->arg1);
            seq->setAnimSetFrame(m_data->animSets[index], frame);
            break;
        }
        case S2dOp_SetPlay: {
            int index = packedAnimSet(m_data, cmd->arg0, cmd->arg1);
            seq->setAnimSetPlay(m_data->animSets[index], (u_short)cmd->arg1);
            break;
        }
        case S2dOp_SetFrameIn: {
            UiSequence *target = findSequence(argHi(cmd->arg0));
            if (target != nullptr) {
                int index = packedAnimSet(m_data, cmd->arg0, cmd->arg1);
                target->setAnimSetFrame(m_data->animSets[index], argLo(cmd->arg1));
            }
            break;
        }
        case S2dOp_SetFrameVarIn: {
            UiSequence *target = findSequence(argHi(cmd->arg0));
            if (target != nullptr) {
                int frame = m_data->getVar(argLo(cmd->arg1))->value;
                int index = packedAnimSet(m_data, cmd->arg0, cmd->arg1);
                target->setAnimSetFrame(m_data->animSets[index], frame);
            }
            break;
        }
        case S2dOp_SetPlayIn: {
            int index = packedAnimSet(m_data, cmd->arg0, cmd->arg1);
            UiSequence *target = findSequence(argHi(cmd->arg0));
            if (target != nullptr)
                target->setAnimSetPlay(m_data->animSets[index], (u_short)cmd->arg1);
            break;
        }
        default:
            break;
        }
        cmd = seq->step(1);
    }
    seq->m_cursor = seq->m_cmdBegin;
    return changes;
}

// ──────────────────────────────────────────────────────────────────────
// Starts sequence id with a value (its start commands run now); a running
// one is left alone. Answers id, or -1.
// FS2 0x00244d90
// callcheck: findSequence (inline in the ROM)
// callcheck: operator_new (the new-expression)

int UiLayout::startSequence(int id, int value)
{
    if (id < 0)
        return -1;
    if (findSequence(id) != nullptr)
        return id;

    if (m_seqCapacity <= m_seqCount) {
        if (m_seqs == nullptr)
            m_seqs = (UiSequence **)gGlobalHeap.alloc(kSeqArrayGrow * sizeof(UiSequence *), 16);
        else
            m_seqs = (UiSequence **)gGlobalHeap.realloc(m_seqs, (m_seqCapacity + kSeqArrayGrow) * sizeof(UiSequence *));
        for (int i = m_seqCapacity; i < m_seqCapacity + kSeqArrayGrow; i++)
            m_seqs[i] = nullptr;
        m_seqCapacity += kSeqArrayGrow;
    }
    UiSequence *seq = new UiSequence(m_data, this, 0);
    m_seqs[m_seqCount] = seq;
    m_seqCount++;
    seq->m_value = value;

    S2dSeqDef *def = m_data->seqs[id];
    seq->m_def = def;
    seq->m_id = id;
    seq->m_priority = def->priority;
    seq->m_cmdBegin = def->cmds;
    seq->m_cursor = seq->m_cmdBegin;
    seq->m_cmdEnd = def->cmds + def->cmdCount;
    runCommands(seq, S2dTrigger_Start);
    sortSequences();
    return id;
}

// ──────────────────────────────────────────────────────────────────────
// A command's condition, negated when its flag says so; unknown kinds are
// false (and not negated).
// FS2 0x00245060
// callcheck: findSequence (inline in the ROM)

bool UiLayout::testCondition(UiSequence *seq, const S2dCond *cond)
{
    const Pad &pad = gGameSystem->m_pad0;
    bool ok = true;

    switch (cond->kind) {
    case S2dCond_Always:
        return true;

    case S2dCond_KeyPressed:
    case S2dCond_KeyRepeat:
    case S2dCond_KeyHeld: {
        u_int want = cond->b;
        u_int word;
        if (cond->kind == S2dCond_KeyRepeat)
            word = pad.m_repeat;
        else if (cond->kind == S2dCond_KeyHeld)
            word = pad.m_held;
        else
            word = pad.m_pressed;
        u_int keys = word;
        if (word & bitMask(ButtonMap::defaultButton(0)))
            keys |= UiKey_Decide;
        if (word & bitMask(ButtonMap::defaultButton(1)))
            keys |= UiKey_Cancel;
        if (pad.m_repeat & (1u << PadBit_Up))
            keys |= UiKey_RepeatUp;
        if (pad.m_repeat & (1u << PadBit_Down))
            keys |= UiKey_RepeatDown;
        if (pad.m_repeat & (1u << PadBit_Left))
            keys |= UiKey_RepeatLeft;
        if (pad.m_repeat & (1u << PadBit_Right))
            keys |= UiKey_RepeatRight;

        if (want & UiKey_Any) {
            if (keys == 0)
                ok = false;
        } else if (cond->flags & S2dCondFlag_AllKeys) {
            if ((keys & want) != want)
                ok = false;
        } else if ((keys & want) == 0) {
            ok = false;
        }
        break;
    }

    case S2dCond_AnimSetDone:
        if (!seq->isAnimSetDone(m_data->animSets[cond->a]))
            ok = false;
        break;

    case S2dCond_ActorDone: {
        UiActor *actor = seq->findActor(cond->a);
        if (actor == nullptr || !actor->isDone())
            ok = false;
        break;
    }

    case S2dCond_Var:
        ok = compare(m_data->vars[cond->a].value, cond->b, cond);
        break;

    case S2dCond_SequenceRunning:
        if (findSequence(cond->a) == nullptr)
            ok = false;
        break;

    case S2dCond_GameState:
        ok = testGameState(m_data->getString(cond->a), m_data->getString(cond->b), seq);
        break;

    case S2dCond_VarVar:
        ok = compare(m_data->vars[cond->a].value, m_data->vars[cond->b].value, cond);
        break;

    case S2dCond_Arg:
        ok = compare(m_arg, cond->b, cond);
        break;

    case S2dCond_SeqValue: {
        UiSequence *target = cond->a >= 0 ? findSequence(cond->a) : seq;
        if (target != nullptr)
            ok = compare(target->m_value, cond->b, cond);
        break;
    }

    case S2dCond_SeqValueVar: {
        UiSequence *target = cond->a >= 0 ? findSequence(cond->a) : seq;
        if (target != nullptr)
            ok = compare(target->m_value, m_data->getVar(cond->b)->value, cond);
        break;
    }

    case S2dCond_SubLayout:
        if (cond->b == 0)
            ok = m_state == SubLayout_None;
        else
            ok = m_state == SubLayout_Loading;
        break;

    case S2dCond_SubResult: {
        int value = cond->a < 0 ? cond->b : m_data->getVar(cond->a)->value;
        ok = compare(m_lastResult, value, cond);
        break;
    }

    case S2dCond_GameValue:
        ok = testGameValue(cond->a, cond->b);
        break;

    case S2dCond_SeqAnimSetDone: {
        // PC: with a < 0 the ROM tests whatever sequence an uninitialised
        // register points at; here it is none (true).
        UiSequence *target = cond->a >= 0 ? findSequence(cond->b) : nullptr;
        if (target != nullptr && !target->isAnimSetDone(m_data->animSets[cond->a]))
            ok = false;
        break;
    }

    default:
        return false;
    }

    if (cond->flags & S2dCondFlag_Negate)
        ok = !ok;
    return ok;
}

// ──────────────────────────────────────────────────────────────────────
// a against b with the condition's operator; an unknown one is false.
// FS2 0x00245c00

bool UiLayout::compare(int a, int b, const S2dCond *cond)
{
    switch (cond->flags & S2dCondFlag_OpMask) {
    case S2dCompare_AnyBit:
        return (a & b) != 0;
    case S2dCompare_LessEqual:
        return a <= b;
    case S2dCompare_Less:
        return a < b;
    case S2dCompare_GreaterEqual:
        return a >= b;
    case S2dCompare_Greater:
        return a > b;
    case S2dCompare_Equal:
        return a == b;
    default:
        return false;
    }
}

// ──────────────────────────────────────────────────────────────────────
// -1: every sequence and the array (answers 0). Else the sequence with
// that id; the last one takes its place. Answers its index, or -1.
// FS2 0x00245c80
// callcheck: removeActor, removeWidget, operator_delete (the delete-expressions, ~UiSequence inline)

int UiLayout::removeSequence(int id)
{
    if (m_seqs == nullptr)
        return 0;

    if (id < 0) {
        for (int i = 0; i < m_seqCount; i++) {
            if (m_seqs[i] != nullptr)
                delete m_seqs[i];
        }
        gGlobalHeap.free(m_seqs);
        m_seqCapacity = 0;
        m_seqCount = 0;
        m_seqs = nullptr;
        return 0;
    }

    for (int i = 0; i < m_seqCount; i++) {
        if (m_seqs[i]->m_id == id) {
            delete m_seqs[i];
            m_seqs[i] = nullptr;
            UiSequence *last = m_seqs[m_seqCount - 1];
            m_seqs[m_seqCount - 1] = nullptr;
            m_seqs[i] = last;
            m_seqCount--;
            return i;
        }
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00245e00

UiSequence *UiLayout::findSequence(int id)
{
    if (m_seqs != nullptr && id >= 0) {
        for (int i = 0; i < m_seqCount; i++) {
            if (m_seqs[i] != nullptr && m_seqs[i]->m_id == id)
                return m_seqs[i];
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Plays an S2D file (relocated in place on first use) and starts its first
// sequence; false (no data kept) when it is not an S2D file.
// FS2 0x00245e80

bool UiLayout::setData(void *data, int arg)
{
    removeSequence(-1);
    m_arg = arg;
    m_data = (S2dData *)data;
    bool ok = m_data->relocate();
    if (!ok) {
        m_data = nullptr;
    } else if (m_data->startSeq >= 0) {
        startSequence(m_data->startSeq, m_arg);
    }
    return ok;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00245f10
// callcheck: operator_delete[], operator_new[] (the delete[] / new[] expressions)

void UiLayout::setBaseDir(const char *dir)
{
    if (m_baseDir != nullptr)
        delete[] m_baseDir;
    m_baseDir = nullptr;
    if (dir != nullptr) {
        m_baseDir = new char[strlen(dir) + 1];
        strcpy(m_baseDir, dir);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Back to no data: sequences gone, colour white, offset 0. The sounds are
// meant to stop too, but stopSound(-1) does nothing (as in the ROM).
// FS2 0x00245f80
// callcheck: operator_delete[] (setBaseDir, inline in the ROM)
// callcheck: set (inline in the ROM: plain stores)

void UiLayout::clear()
{
    stopSound(-1);
    removeSequence(-1);
    m_seqCapacity = 0;
    m_seqCount = 0;
    m_seqs = nullptr;
    m_result = -1;
    m_arg = -1;
    m_drawMode = 0;
    m_color.set(1.0f, 1.0f, 1.0f, 1.0f);
    m_offset.set(0.0f, 0.0f, 0.0f, 0.0f);
    m_data = nullptr;
    setBaseDir(nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00246010
// callcheck: stopSound, removeSequence, operator_delete[] (clear, inline in the ROM)
// callcheck: ~UiDrawer (the delete-expression)
// callcheck: clear, ~UiDrawer, ~UiSubLayout, operator_delete, ~FileJob (the base destructor's close, inline: the child's and the job's delete-expressions)
// callcheck: operator_delete (the deleting variant)

UiLayout::~UiLayout()
{
    clear();
    delete m_drawer;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00246160
// callcheck: operator_new, UiDrawer (the new-expression)
// callcheck: set (inline in the ROM: plain stores)

UiLayout::UiLayout()
    : m_data(nullptr), m_seqCapacity(0), m_seqCount(0), m_seqs(nullptr), m_drawMode(0),
      m_arg(-1), m_result(-1), m_done(0), m_baseDir(nullptr)
{
    m_drawer = new UiDrawer;
    m_color.set(1.0f, 1.0f, 1.0f, 1.0f);
    m_offset.set(0.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < kUiLayoutSoundSlots; i++) {
        m_soundName[i] = nullptr;
        m_soundHandle[i] = -1;
    }
}
