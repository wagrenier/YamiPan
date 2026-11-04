/* ==========================================================================
 *  game/stageobject.cpp
 *
 *  The base of the stage's objects (StageObject, gamehooks.h: doors, items,
 *  cameras, lights, characters) and the tree of objects (StageObjectTree),
 *  0x18ae70..0x18c050, with the three script-facing slots before it
 *  (0x189ce0..0x18ae70, not written yet). The node base's dtor 0x1865c0 is
 *  here too, since a StageObject is one. The file name is the port's (no
 *  source string).
 * ======================================================================== */
#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/stagescene.h"
#include "game/vu0math.h"

// The serial's counter.
static u_int sObjectSerial;     /* sdata 0x31ae08 (gp-0x7a68) */

// ──────────────────────────────────────────────────────────────────────
// The node base's dtor (the ROM's two inlined bases included): frees its two
// blocks, leaves the parent, orphans the children.
// FS2 0x001865c0
// callcheck: free, erase, FUN_00186b80, operator_delete (detach is inline, the list a member)

StageNodeBase::~StageNodeBase()
{
    if (_10 != nullptr)
        gGlobalHeap.free(_10);
    if (m_inverse != nullptr)
        gGlobalHeap.free(m_inverse);
    detach();
    for (StageNodeList::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->m_parent = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Empty in the base.
// FS2 0x0018ae70

void StageObject::noiseHook()
{
}

// ──────────────────────────────────────────────────────────────────────
// A noise of the object: what listens for it finds it in the scene's list.
// FS2 0x0018ae80
// callcheck: FUN_002430a0

void StageObject::makeNoise(float loudness)
{
    gStageScene->uiState->addNoise(loudness, this, nullptr);
}

// ──────────────────────────────────────────────────────────────────────
// Plays the named sound and reports it, when it has a loudness.
// FS2 0x0018aea0
// callcheck: FUN_00242fc0

void StageObject::playNamedHeard(const char *name, const float *pos, StageNodeBase *parent)
{
    StageSoundSource *source = gStageScene->sound->addSourceNamed(name, pos, parent);
    if (source != nullptr && source->m_entry->loudness > 0.0f)
        gStageScene->uiState->addSourceNoise(this, source);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018af00

float StageObject::distanceToPoint(const sceVu0FVECTOR point)
{
    return vecDistance(m_xform.position, point);
}

// ──────────────────────────────────────────────────────────────────────
// What a script holds for the object: a character its odd serial, others
// the pointer. The ROM asks for the kind twice (above -1, below 1).
// FS2 0x0018af10
// callcheck: getKind

intptr_t StageObject::getHandle()
{
    if (getKind() == StageObject_Character)
        return (intptr_t)m_serial;
    return (intptr_t)this;
}

// ──────────────────────────────────────────────────────────────────────
// The next serial that is not 0 in its low 31 bits. The ROM unrolls the
// recursion eight levels.
// FS2 0x0018afc0

static u_int StageObject_nextSerial()
{
    sObjectSerial++;
    u_int serial = sObjectSerial & 0x7fffffff;
    return serial != 0 ? serial : StageObject_nextSerial();
}

// ──────────────────────────────────────────────────────────────────────
// The ROM increments here and calls 0x18afc0 only when that wraps to 0.
// FS2 0x0018af70

void StageObject::assignSerial()
{
    m_serial = StageObject_nextSerial() << 1 | 1;
}

// ──────────────────────────────────────────────────────────────────────
// Sets the flag here and goes down the children through their slot.
// FS2 0x0018b0d0
// callcheck: setDirtyTree (the children's virtual)

void StageObject::setDirtyTree(bool dirty)
{
    m_xform.dirty = dirty;
    for (StageNodeList::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->setDirtyTree(dirty);
}

// ──────────────────────────────────────────────────────────────────────
// Stops a source that still holds a slot.
// FS2 0x0018b130

void StageObject::stopSound(StageSoundSource *source)
{
    if (gStageScene->sound->findSource(source) != nullptr)
        gStageScene->sound->stopSource(source);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018b170

StageSoundSource *StageObject::playNamed(const char *name, const float *pos, StageNodeBase *parent)
{
    return gStageScene->sound->addSourceNamed(name, pos, parent);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018b180

StageSoundSource *StageObject::playEntry(const SeEntry *entry, const float *pos, StageNodeBase *parent)
{
    return gStageScene->sound->addSource(entry, pos, parent);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018b190
// callcheck: FUN_00242fc0

StageSoundSource *StageObject::playEntryHeard(const SeEntry *entry, const float *pos, StageNodeBase *parent)
{
    StageSoundSource *source = gStageScene->sound->addSource(entry, pos, parent);
    if (source == nullptr)
        return nullptr;
    if (source->m_entry->loudness > 0.0f)
        gStageScene->uiState->addSourceNoise(this, source);
    return source;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018b210
// callcheck: FUN_00242fc0

StageSoundSource *StageObject::playNamedHeard2(const char *name, const float *pos, StageNodeBase *parent)
{
    StageSoundSource *source = gStageScene->sound->addSourceNamed(name, pos, parent);
    if (source == nullptr)
        return nullptr;
    if (source->m_entry->loudness > 0.0f)
        gStageScene->uiState->addSourceNoise(this, source);
    return source;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018b290

bool StageObject::getShownState()
{
    if (m_model == nullptr || m_model->data == nullptr)
        return m_shown != 0;
    return m_model->data->shown != 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018b2d0

void StageObject::setShownState(bool on)
{
    if (m_model == nullptr || m_model->data == nullptr)
        m_shown = on;
    else if (on != (m_model->data->shown != 0))
        m_model->data->shown = on;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018b320

void StageObject::setActive(bool on)
{
    m_active = on;
    m_viewActive = on;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018b330
// callcheck: ~Clock

void StageObject::setClock(Clock *parent)
{
    if (m_clock != nullptr)
        delete m_clock;
    m_clock = Clock::create(parent);
}

// ──────────────────────────────────────────────────────────────────────
// Replaces the model. With a model file the object's matrix becomes the
// root's, and the object keeps that matrix's inverse (a block of the global
// heap; the ROM drops an older block here); the model's shown flag starts
// from m_active.
// FS2 0x0018b380
// callcheck: ~StageModel, operator_new, FUN_0018bf40, __construct_array, init, FUN_00136430, alloc, FUN_00136400 (new StageModel: the ROM's ctor is inline; the matrix helpers), getWorldMatrix (virtual)

void StageObject::setModel(ModelInstance *instance, StringMap<Tim2Texture *> *textures)
{
    if (m_model != nullptr)
        delete m_model;
    m_model = new StageModel;
    m_model->init(instance, textures, getWorldMatrix());

    StageModelData *data = m_model->data;
    if (data != nullptr) {
        // PC: the ROM writes through the vtable's matrix; it is m_xform.world.
        copyMatrix(m_xform.world, *data->rootMatrix);
        m_inverse = (sceVu0FVECTOR *)gGlobalHeap.alloc(sizeof(sceVu0FMATRIX), sizeof(sceVu0FMATRIX));
        identityMatrix(m_inverse);
        invertRigidMatrix(m_inverse, getWorldMatrix());
        data->shown = m_active != 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Clears the name, then copies at most 15 characters.
// FS2 0x0018b4d0

void StageObject::setName(const char *name)
{
    memset(m_name, 0, sizeof(m_name));
    strncpy(m_name, name, sizeof(m_name) - 1);
}

// ──────────────────────────────────────────────────────────────────────
// Frees the model, the clock and the tree node; the ROM's ~ScriptRunner and
// the node's dtor follow as the bases'.
// FS2 0x0018b4e0
// callcheck: ~StageModel, ~Clock, FUN_0017e260, FUN_0018bf70, operator_delete, ~ScriptRunner, free, erase, FUN_00186b80 (delete of the members, the bases' dtors)

StageObject::~StageObject()
{
    if (m_model != nullptr)
        delete m_model;
    if (m_clock != nullptr)
        delete m_clock;
    delete m_tree;
}

// ──────────────────────────────────────────────────────────────────────
// Inline in ~StageObject (0x18b4e0): leaves the parent's list, orphans the
// children.

StageObjectTree::~StageObjectTree()
{
    if (m_parent != nullptr) {
        for (StageList<StageObjectTree *>::Node *link = m_parent->m_children.head;
             link != nullptr; link = link->next) {
            if (link->value == this) {
                m_parent->m_children.erase(link);
                m_parent = nullptr;
                break;
            }
        }
    }
    for (StageList<StageObjectTree *>::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->m_parent = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The bases, their vtables, then the object's state: active and shown, no
// name, no model; its serial and its tree node.
// FS2 0x0018b700
// callcheck: StageNodeBase, ScriptRunner, NodeTransform, FUN_0018af70, operator_new, FUN_0018bf60 (the bases, the members' ctors, new StageObjectTree)

StageObject::StageObject()
{
    // PC: the fields past the transform belong to subclasses whose ctors are
    // still stubs (a camera's m_linked, read by StageSound::update): zero
    // until those are written, not heap garbage.
    memset(&m_itemDef, 0, (char *)(&m_ammoA + 1) - (char *)&m_itemDef);
    memset(m_name, 0, sizeof(m_name));
    m_model = nullptr;
    m_clock = nullptr;
    m_dead = 0;
    m_active = 1;
    m_shown = 1;
    m_lightOn = 0;
    assignSerial();
    m_tree = new StageObjectTree(this);
    _0fc = nullptr;
    _100 = nullptr;
    _104 = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The ROM inlines four levels of this walk, and goes through the small
// accessors 0x18b9a0..0x18b9d0 from there down.
// FS2 0x0018b7f0
// callcheck: FUN_0018b9a0, FUN_0018b9b0, FUN_0018b9c0, FUN_0018b9d0 (list accessors), setScriptFlag2 (the object's virtual)

void StageObjectTree::setScriptFlag2(bool on)
{
    m_object->setScriptFlag2(on);
    for (StageList<StageObjectTree *>::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->setScriptFlag2(on);
}

// ──────────────────────────────────────────────────────────────────────
// The list's accessors, instances the walks inline from their fifth level
// on:
//   FS2 0x0018b9a0  iterator next
//   FS2 0x0018b9b0  list head
//   FS2 0x0018b9c0  the node's object
//   FS2 0x0018b9d0  the link's value

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018b9e0
// callcheck: FUN_0018b9a0, FUN_0018b9b0, FUN_0018b9c0, FUN_0018b9d0 (list accessors), setScriptFlag (the object's virtual)

void StageObjectTree::setScriptFlag(bool on)
{
    m_object->setScriptFlag(on);
    for (StageList<StageObjectTree *>::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->setScriptFlag(on);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018bb90

void StageObjectTree::setChildrenShown(bool on)
{
    for (StageList<StageObjectTree *>::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->setShown(on);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018bbe0
// callcheck: FUN_0018b9a0, FUN_0018b9b0, FUN_0018b9c0, FUN_0018b9d0 (list accessors)

void StageObjectTree::setShown(bool on)
{
    m_object->setShown(on);
    for (StageList<StageObjectTree *>::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->setShown(on);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018bd70

void StageObjectTree::setChildrenActive(bool on)
{
    for (StageList<StageObjectTree *>::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->setActive(on);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0018bdc0
// callcheck: FUN_0018b9a0, FUN_0018b9b0, FUN_0018b9c0, FUN_0018b9d0, FUN_0018bf10, FUN_0018b320 (list accessors, the view flag's setter, the object's setActive inlined at the first levels)

void StageObjectTree::setActive(bool on)
{
    m_object->setActive(on);
    for (StageList<StageObjectTree *>::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->setActive(on);
}

// ──────────────────────────────────────────────────────────────────────
// More instances:
//   FS2 0x0018bf10  the view flag's setter (a one-byte class)
//   FS2 0x0018bf20  ctor of the model's five 0x10 records
//   FS2 0x0018bf40  ctor of the model's +0x14 record
//   FS2 0x0018bf60  the child list's ctor (StageList)
//   FS2 0x0018bf70  the child list's dtor (StageList)
//   FS2 0x0018bfd0  the child list's clear (StageList)
//   FS2 0x0018c040  the runner base's thunk of 0x18b4e0

// ──────────────────────────────────────────────────────────────────────
// Slot +0x248: shows the object and all under it.
// FS2 0x00172260

void StageObject::setTreeShown(bool on)
{
    m_tree->setShown(on);
}

// ──────────────────────────────────────────────────────────────────────
// The tail of 0x185fc0: the object's tree walks.
// FS2 0x00185fc0

void StageObject::setTreeScriptFlag(bool on)
{
    m_tree->setScriptFlag(on);
}
