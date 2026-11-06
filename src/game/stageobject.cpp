/* ==========================================================================
 *  game/stageobject.cpp
 *
 *  The base of the stage's objects (StageObject, gamehooks.h: doors, items,
 *  cameras, lights, characters) and the tree of objects (StageObjectTree),
 *  0x18ae70..0x18c050, with the three script-facing slots before it
 *  (0x189ce0..0x18ae70: scriptAddMenuLine, setProperty, getProperty). The
 *  node base's dtor 0x1865c0 is here too, since a StageObject is one. The file
 *  name is the port's (no source string).
 * ======================================================================== */
#include "game/clock.h"
#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/objectproperty.h"
#include "game/stagescene.h"
#include "game/sysdata.h"
#include "game/uiwidget.h"      /* UiShowFlags */
#include "game/vu0math.h"

constexpr float kDegreesToRadians  = 0.017453292f;
constexpr float kRadiansToDegrees  = 57.295776f;
constexpr int   kFramesPerSecond30 = 30;
constexpr int   kFrameStepUnit     = 300;       // Prop_FrameStep: of these, a frame's
constexpr int   kObjectKindItemFirst = 0x13;    // the items and weapons (StageItem), as Prop_ObjectType tests
constexpr int   kObjectKindItemLast  = 0x17;

// The serial's counter.
static u_int sObjectSerial;     /* sdata 0x31ae08 (gp-0x7a68) */

// ──────────────────────────────────────────────────────────────────────
// The node base's dtor (the ROM's two inlined bases included): frees its two
// blocks, leaves the parent, orphans the children.
// FS2 0x001865c0
// callcheck: free, erase, FUN_00186b80, operator_delete (detach is inline, the list a member)

StageNodeBase::~StageNodeBase()
{
    if (m_lastPosition != nullptr)
        gGlobalHeap.free(m_lastPosition);
    if (m_inverse != nullptr)
        gGlobalHeap.free(m_inverse);
    detach();
    for (StageNodeList::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->m_parent = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Records it; both unlock flags set then mark the save, as a clear does.
static void markStageCleared(int stage, int variant)
{
    SaveData *save = getSaveData();
    int bit = save->clearBit(getStageSelect()->kind, variant);
    u_char *flags = &save->stageFlags[stage];
    if ((bit & ~*flags) == 0)
        return;
    *flags |= (u_char)bit;
    if (save->stageFlags[kStageFlagUnlockA] != 0 && save->stageFlags[kStageFlagUnlockB] != 0)
        save->markUnlock468();
}

// Stage 1's Root is what sets Prop_ShowFlag: run and the menus off, ForceView
// on. Every other id, the character's own among them, does nothing here; 0x5d
// asks for the mission and drops it.
// FS2 0x00189f40
// callcheck: getKind, setTreeShown, setYaw, currentMission, setScoreClock, currentMission, setClockFrames, currentMission, setResultHold, currentMission, setResultPaused, currentMission, findCharacterById, findCharacterById, setPlayer, setScriptVar3f, setScriptVar3e, setResultValue, getSaveData, pickVariant3ac, getSaveData, getSaveData, clearBit, markUnlock468, getSaveData, clearBit, markUnlock468 (markStageCleared holds the last; cmdSetTreeSwitchedOn is the runner's slot +0x64)

void StageObject::setProperty(int id, int sub, int value)
{
    StageScene *scene = gStageScene;
    switch (id) {
    case Prop_ViewActive:
        if (getKind() == StageObject_Car)
            m_viewActive = value != 0;
        break;
    case Prop_Shown:
        setTreeShown(value != 0);
        break;
    case Prop_SwitchedOn:
        cmdSetTreeSwitchedOn(value != 0);
        break;
    case Prop_YawDegrees:
        setYaw((float)value * kDegreesToRadians);
        break;
    case Prop_ScoreClock:
        scene->currentMission()->setScoreClock(value);
        break;
    case Prop_ClockFrames:
        scene->currentMission()->setClockFrames(value);
        break;
    case Prop_ResultHold:
        scene->currentMission()->setResultHold(value != 0);
        break;
    case Prop_ResultPaused:
        scene->currentMission()->setResultPaused(value != 0);
        break;
    case Prop_ShowFlag:
        if (sub >= 0 && sub < kShowFlagCount) {
            bool hidden = value != 1;
            scene->showFlags->flag[(int)kShowFlagByte[sub]] = sub == kShowFlagInverted ? !hidden : hidden;
        }
        break;
    case Prop_PartnerId:
        if (value == 0)
            scene->partner = nullptr;
        else if (StageObject *who = scene->objects->findCharacterById(value))
            scene->partner = who;
        break;
    case Prop_PlayerId:
        if (StageObject *who = scene->objects->findCharacterById(value))
            scene->setPlayer(static_cast<StagePlayer *>(who));
        break;
    case Prop_ScriptVar3f:
        scene->setScriptVar3f(value);
        break;
    case Prop_ScriptVar3e:
        scene->setScriptVar3e(value);
        break;
    case Prop_Result:
        scene->setResultValue(sub, value);
        break;
    case Prop_Archive: {
        int archive = sub + 1;
        if (archive <= 0 || archive > kArchiveCount || value < 0 || value >= kArchiveStates)
            break;
        SaveData *save = getSaveData();
        if (archive == kArchiveVariantUnlock) {
            if (value == kArchiveWanted)
                save->pickVariant3ac(-1);
            else if (value == kArchiveHeld && save->archive[archive] != kArchiveHeld
                     && save->variant3ac == 0)
                save->_3b0 = 1;
        }
        save->archive[archive] = (u_char)value;
        break;
    }
    case Prop_StageCleared: {
        int stage = sub + 1;
        if (stage < 0 || stage >= kStageCount)
            break;
        if (value == 2)
            markStageCleared(stage, 1);
        else if (value == 1)
            markStageCleared(stage, 0);
        break;
    }
    default:
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// A stage's clear state: 0 not, 1 in variant 0, 2 in both.
static int stageClearedState(int stage)
{
    int kind = getStageSelect()->kind;
    SaveData *save = getSaveData();
    if (!save->testStageFlags(stage, save->clearBit(kind, 0)))
        return 0;
    if (!save->testStageFlags(stage, save->clearBit(kind, 1)))
        return 1;
    return 2;
}

// 0x269160 and the map object's 0x174b00 are copies; the second adds id 0x6b
// for a kind 6 object, not built yet.
// FS2 0x0018a5b0
// callcheck: getStageSelect, isShown, getHandle, getSeconds, currentMission, getScoreClock, getClockFrames, getResultHold, getResultPaused, onRestore, getDeltaSeconds, getScriptVar3f, getScriptVar3e, getResultValue, stageFps, getMissionFrames, random, getSaveData, clearBit, testStageFlags

int StageObject::getProperty(int id, int sub)
{
    StageScene *scene = gStageScene;
    switch (id) {
    case Prop_ViewActive:
        return m_viewActive;
    case Prop_Shown:
        return isShown();
    case Prop_Handle:
        return (int)getHandle();
    case Prop_Group:
        return 4;
    case Prop_Frames30: {
        Clock *clock = m_clock != nullptr ? m_clock : scene->clock;
        return (int)(clock->getSeconds() * (float)kFramesPerSecond30 + 0.5f);
    }
    case Prop_YawDegrees:
        return (int)(m_xform.rotation[2] * kRadiansToDegrees);
    case Prop_ScoreClock:
        return scene->currentMission()->getScoreClock();
    case Prop_ClockFrames:
        return scene->currentMission()->getClockFrames();
    case Prop_ResultHold:
        return scene->currentMission()->getResultHold();
    case Prop_ResultPaused:
        return scene->currentMission()->getResultPaused();
    case Prop_ShowFlag: {
        if (sub < 0 || sub >= kShowFlagCount)
            return 0;
        UiShowFlags *flags = scene->showFlags;
        int value = flags->flag[(int)kShowFlagByte[sub]];
        if (sub == kShowFlagInverted)
            return value;
        if (sub == kShowFlagWithFirst)
            value = value != 0 && flags->flag[0] != 0;
        return value == 0;
    }
    case Prop_Restore:
        return scene->currentMission()->onRestore();
    case Prop_ActorId: {
        StageObject *actor = scene->controlled != nullptr ? scene->controlled : (StageObject *)scene->player;
        return actor != nullptr ? actor->m_id : 0;
    }
    case Prop_PartnerId:
        return scene->partner != nullptr ? scene->partner->m_id : 0;
    case Prop_PlayerId:
        return scene->player != nullptr ? ((StageObject *)scene->player)->m_id : 0;
    case Prop_FrameStep:
        return kFrameStepUnit / (int)(1.0f / scene->clock->getDeltaSeconds());
    case Prop_ScriptVar3f:
        return scene->getScriptVar3f();
    case Prop_ScriptVar3e:
        return scene->getScriptVar3e();
    case Prop_Result:
        return scene->getResultValue(sub);
    case Prop_Fps:
        return stageFps();
    case Prop_MissionFrames:
        return scene->getMissionFrames();
    case Prop_Random100:
        return scene->random(100);
    case Prop_Archive: {
        int archive = sub + 1;
        if (archive <= 0 || archive > kArchiveCount)
            return 0;
        SaveData *save = getSaveData();
        if (archive == kArchiveVariantUnlock && save->_3b0 != 0)
            return kArchiveReported;
        return save->archive[archive];
    }
    case Prop_StageCleared: {
        int stage = sub + 1;
        if (stage < 0 || stage >= kStageCount)
            return 0;
        return stageClearedState(stage);
    }
    case Prop_SwitchedOn:
        return cmdIsLit();
    case Prop_ObjectType: {
        const int kind = getKind();
        if (kind == StageObject_Character)
            return stageObjRecord(m_itemDef)->kind;
        if (kind >= kObjectKindItemFirst && kind <= kObjectKindItemLast)
            return gItemDefs[m_itemDef < 0 || m_itemDef >= kItemDefCount ? 0 : m_itemDef].kind;
        return 0;
    }
    default:                    // 0x3d, 0x5d (which asks for the mission and drops it): 0
        return 0;
    }
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
    gStageScene->noises->addNoise(loudness, this, SceneNoise_Sound);
}

// ──────────────────────────────────────────────────────────────────────
// Plays the named sound and reports it, when it has a loudness.
// FS2 0x0018aea0
// callcheck: FUN_00242fc0

void StageObject::playNamedHeard(const char *name, const float *pos, StageNodeBase *parent)
{
    StageSoundSource *source = gStageScene->sound->addSourceNamed(name, pos, parent);
    if (source != nullptr && source->m_entry->loudness > 0.0f)
        gStageScene->noises->addSourceNoise(this, source);
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
        gStageScene->noises->addSourceNoise(this, source);
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
        gStageScene->noises->addSourceNoise(this, source);
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

    ModelInstance *data = m_model->data;
    if (data != nullptr) {
        // PC: the ROM writes through the vtable's matrix; it is m_xform.world.
        copyMatrix(m_xform.world, data->node->matrix.get());
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
// callcheck: ~StageModel, ~Clock, erase, FUN_0018bf70, operator_delete, ~ScriptRunner, free, erase, FUN_00186b80 (delete of the members, the bases' dtors)

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
    // PC: the ROM leaves these to the subclasses' ctors. On the host they are
    // one flat block (gamehooks.h) that each ctor fills only as far as its
    // TU has read, and CarPose, ElevatorPose and the car's own are stubs: zero,
    // not heap garbage (a camera's m_linked, read by StageSound::update).
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
    m_modelLights[0] = m_modelLights[1] = m_modelLights[2] = nullptr;
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

// A line of the stage menu, from this object. Below 0 the kind comes from what
// the object is; other objects offer none. Who controls the player matters:
// a player of record kind 0xb or 0xd has no menu.
// FS2 0x00189ce0: the slot's thunk is 0x00177450.
// callcheck: getKind, getStageSelect, getStageSelect, addLine

void StageObject::scriptAddMenuLine(const u_short *text, int kind, bool plain)
{
    constexpr int kLineItem = 0x40, kLinePlayer = 0x30, kLinePartner = 0x20, kLineCar = 0x10, kLineDoor = 0;
    constexpr int kRecordKind11 = 0xb, kRecordKind13 = 0xd;

    if (kind < 0) {
        switch (getKind()) {
        case StageObject_Item:
        case StageObject_ItemE:
        case StageObject_WeaponItem:
            kind = kLineItem;
            break;
        case StageObject_Character:
            if (gStageScene->partner == this)
                kind = kLinePartner;
            else if (gStageScene->player == this)
                kind = kLinePlayer;
            else
                return;
            break;
        case StageObject_Car:
            kind = kLineCar;
            break;
        case StageObject_Door:
            kind = kLineDoor;
            break;
        default:
            return;
        }
    }

    // PC: with no player the ROM reads low memory.
    if (gStageScene->player == nullptr)
        return;
    const StageObjRecord *record = stageObjRecord(gStageScene->player->m_itemDef);
    if (record != nullptr && record->kind != kRecordKind11 && record->kind != kRecordKind13)
        gStageScene->menu->addLine(text, kind, plain);
}
