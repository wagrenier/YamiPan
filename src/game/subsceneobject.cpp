/* ==========================================================================
 *  game/subsceneobject.cpp
 *
 *  An event's objects (game/subsceneobject.h): 0x268310..0x269da0 the base,
 *  0x2bb600..0x2bc100 the classes on it. The file name is the port's.
 *
 *  Their frame is ScenePairTable's third step (sideHook2).
 *
 *  Left out: 0x2689a0 (a sound on a named bone, through the scene's +0xdc):
 *  no vtable holds it and nothing written calls it.
 * ======================================================================== */
#include "game/subsceneobject.h"

#include <stdio.h>
#include <string.h>

#include "game/clock.h"
#include "game/modelpose.h"
#include "game/stageitem.h"
#include "game/stagelight.h"
#include "game/stageobjlist.h"
#include "game/objectproperty.h"
#include "game/stagescene.h"
#include "game/sysdata.h"
#include "game/uiwidget.h"      /* UiShowFlags */

namespace {

constexpr char kBoneTop[]        = "bn_top";    // data 0x308950, 0x30b440
constexpr char kRootObjectName[] = "Root";      // data 0x30b448
constexpr char kHidePartName[]   = "hide";      // data 0x303360
constexpr int  kHidePartCount    = 2;
// StageObjRecord::kind: the two with a hide part; 0xb also casts no shadow.

constexpr float kObjectFrameStep = 1.2f;        // what sideHook2 hands StageModel::advance
constexpr u_int kNamedMotionFlags = 0x50;       // DemoCamera's cuts pass the same
constexpr float kLightTurn = -1.5707964f;       // a light's axis along the bone


StageObjectList *parentObjects()
{
    return gStageScene->parent->objects;
}

void hangLight(StageLight *light, StageMatrixNode *parent)
{
    light->m_xform.setRotation(0.0f, 0.0f, kLightTurn);
    light->setParent(parent);
}

} // namespace

// ──────────────────────────────────────────────────────────────────────
// The lamp. The ROM repeats the parent link four times, inline.
// FS2 0x00268310
// callcheck: sprintf, operator_new, SpotLight, addToScene, setRotation, erase, pushBack, PointLight (the parent links inline)

void SubSceneLamp::build()
{
    char name[0x10];

    sprintf(name, "%s_sl", m_owner->m_name);
    m_spot = new SpotLight;
    m_spot->addToScene(name, true);
    hangLight(m_spot, this);
    m_spot->m_limitByMap = 1;
    m_spot->m_priority = 0;

    sprintf(name, "%s_sl2", m_owner->m_name);
    m_wideSpot = new SpotLight;
    m_wideSpot->addToScene(name, false);
    hangLight(m_wideSpot, this);

    sprintf(name, "%s_pl", m_owner->m_name);
    m_point = new PointLight;
    m_point->addToScene(name, true);
    hangLight(m_point, this);
    m_point->m_castsShadow = 0;

    sprintf(name, "%s_pl2", m_owner->m_name);
    m_point2 = new PointLight;
    m_point2->addToScene(name, true);
    hangLight(m_point2, this);
    m_point2->m_castsShadow = 0;
    m_point2->m_usable = 0;
    m_point2->m_priority = 0;
}

SubSceneLamp::SubSceneLamp(const sceVu0FVECTOR *matrix, StageObject *owner)
    : m_owner(owner)
{
    m_matrix = matrix;
    build();
}

void SubSceneLamp::setOn(bool on)
{
    m_spot->setTreeScriptFlag(on);
    m_point->setTreeScriptFlag(on);
    m_spot->setDirtyTree(on);
    m_point->setDirtyTree(on);
    m_on = on;
}

void SubSceneLamp::updateLights()
{
    m_spot->rebuildMatrix();
    m_point->rebuildMatrix();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00268830
// callcheck: findBone, operator_new, StageNodeBase, build, setTreeScriptFlag, setTreeScriptFlag, setTreeScriptFlag, setTreeScriptFlag (build through SubSceneLamp's ctor, the rest in setOn)

void SubSceneModelObject::setLampOn(bool on)
{
    if (m_lamp == nullptr) {
        if (!on)
            return;
        ModelPoseBone *bone = m_model->pose->findBone(kBoneTop);
        m_lamp = new SubSceneLamp(bone->matrix, this);
    }
    if (m_lamp->m_on != on)
        m_lamp->setOn(on);
}

// FS2 0x00268a20 (0x1a6860 the runner's thunk)
// callcheck: getSubSceneBlock, findMotion, playMotionFile

void SubSceneModelObject::playNamedMotion(const char *name)
{
    void *file = gStageScene->getSubSceneBlock()->findMotion(name);
    if (file != nullptr)
        m_model->playMotionFile(file, false, false, kNamedMotionFlags, 0);
}

// FS2 0x00268a90 (0x1a6870 the runner's thunk)
// callcheck: setPosition, applyMotion

void SubSceneModelObject::setPosition(const float *pos)
{
    m_xform.setPosition(pos);
    if (m_role == SubSceneRole_Character)
        m_model->applyMotion(m_xform.position, m_xform.rotation);
}

// ──────────────────────────────────────────────────────────────────────
// The ROM's copy of StageObject::setProperty, less an id with no effect.
// FS2 0x00268ae0 (0x1a6890 the runner's thunk)
// callcheck: getKind, setTreeShown, setYaw, currentMission, setScoreClock, setClockFrames, setResultHold, setResultPaused, findCharacterById, setPlayer, setScriptVar3f, setScriptVar3e, setResultValue, getSaveData, pickVariant3ac, clearBit, markUnlock468

void SubSceneModelObject::setProperty(int id, int sub, int value)
{
    StageObject::setProperty(id, sub, value);
}

// The ROM's copy of StageObject::getProperty.
// FS2 0x00269160 (0x1a68a0 the runner's thunk)
// callcheck: getStageSelect, isShown, getHandle, getSeconds, currentMission, getScoreClock, getClockFrames, getResultHold, getResultPaused, onRestore, getDeltaSeconds, getScriptVar3f, getScriptVar3e, getResultValue, stageFps, getMissionFrames, random, getSaveData, clearBit, testStageFlags

int SubSceneModelObject::getProperty(int id, int sub)
{
    return StageObject::getProperty(id, sub);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002699f0 (0x1a6830 the copy, in the side base's table)
// callcheck: advance

void SubSceneModelObject::sideHook2()
{
    m_model->advance(kObjectFrameStep, m_xform.position, m_xform.rotation, 0);
    if (m_lamp != nullptr && m_lamp->m_on)
        m_lamp->updateLights();
}

// PC: the ROM takes stageSelf with no test. Kept: an effect or part the stage
// has not placed yet would answer null here; the pack's file stands in.
// FS2 0x00269a90
// callcheck: setRotation, findCharacter, getSubSceneBlock, findModel, setBaseMotion, bindBones, placeBones, add, setDirtyTree

void SubSceneModelObject::bindMotion(const char *name)
{
    void *file = nullptr;
    m_xform.setRotation(0.0f, 0.0f, 0.0f);
    if (m_role == SubSceneRole_Character) {
        StageObject *stageSelf = parentObjects()->findCharacter(m_name, 0);
        if (stageSelf != nullptr && stageSelf->m_model != nullptr)
            file = stageSelf->m_model->m_motionFile;
    }
    if (file == nullptr)
        file = gStageScene->getSubSceneBlock()->findModel(name);
    if (file != nullptr) {
        m_model->setBaseMotion(file, 0);
        m_model->bindBones();
        m_model->placeBones(this);
        gStageScene->pairs->add(this, ScenePairList_Objects);
        setDirtyTree(true);
    }
}

// FS2 0x00269bd0
// callcheck: setName, add8, setModel

void SubSceneModelObject::attach(const char *name, ModelInstance *instance,
                                 StringMap<Tim2Texture *> *textures)
{
    setName(name);
    gStageScene->objects->add8(this);
    setModel(instance, textures);
}

// FS2 0x00269c30 (0x269da0 the thunk)
// callcheck: ~StageObject, operator_delete

SubSceneModelObject::~SubSceneModelObject()
{
    delete m_lamp;
    delete m_extraNode;
}

// FS2 0x00269d00
// callcheck: StageObject, setDirtyTree, setActive, setShown

SubSceneModelObject::SubSceneModelObject(int role)
    : m_role(role)
{
    setDirtyTree(false);
    bool startsOn = m_role == SubSceneRole_Map;
    m_tree->setActive(startsOn);
    m_tree->setShown(startsOn);
}

// ──────────────────────────────────────────────────────────────────────
// The item with a lamp.
// FS2 0x002bb610 (0x2bc0e0 the thunk)
// callcheck: setDark

void SubSceneItem::setScriptFlag(bool on)
{
    m_itemLamp->setDark(on);
}

// FS2 0x002bb620
// callcheck: setSwitchedOn

void SubSceneItem::setLampOn(bool on)
{
    m_itemLamp->setSwitchedOn(on);
}

// FS2 0x002bb630
// callcheck: bindMotion, find, attachTo

void SubSceneItem::bindMotion(const char *name)
{
    SubSceneModelObject::bindMotion(name);
    m_itemLamp->attachTo(m_model->boneMap.find(kBoneTop)->value);
}

// FS2 0x002bb680
// callcheck: attach, markModelAsItem, operator_new, ItemLamp, setSwitchedOn

void SubSceneItem::setup(const char *name, ModelInstance *instance,
                         StringMap<Tim2Texture *> *textures)
{
    attach(name, instance, textures);
    markModelAsItem();
    m_itemLamp = new ItemLamp(this);
    m_itemLamp->setSwitchedOn(false);
}

// FS2 0x002bb6e0 (0x2bc0f0 the thunk)
// callcheck: ~ItemLamp, ~SubSceneModelObject, operator_delete

SubSceneItem::~SubSceneItem()
{
    delete m_itemLamp;
}

// FS2 0x002bb7a0
// callcheck: SubSceneModelObject

SubSceneItem::SubSceneItem(int role)
    : SubSceneModelObject(role)
{
}

// FS2 0x002bb810
// callcheck: attach, markModelAsItem

void SubSceneProp::setup(const char *name, ModelInstance *instance,
                         StringMap<Tim2Texture *> *textures)
{
    attach(name, instance, textures);
    markModelAsItem();
}

// ──────────────────────────────────────────────────────────────────────
// Characters.
// FS2 0x002bb840
// callcheck: bindMotion, setupBody

void SubSceneCharacter::bindMotion(const char *name)
{
    SubSceneModelObject::bindMotion(name);
    setupBody(false);
}

// PC: the ROM takes the stage's character with no test, and the record's kind
// from its +0x200. Kept for the same reason as bindMotion's: a character with
// no stage self counts as kind 0 and hides nothing.
// FS2 0x002bb870
// callcheck: attach, findCharacter, operator_new, getStageSelect, CharacterHidePart, copyFrom, getStageSelect, applyCharacterModelFlags (CharacterHidePart; the record's table inline: stageObjRecord)

void SubSceneCharacter::setup(const char *name, ModelInstance *instance,
                              StringMap<Tim2Texture *> *textures)
{
    attach(name, instance, textures);
    StageObject *stageSelf = parentObjects()->findCharacter(name, 0);
    int kind = stageSelf != nullptr ? stageObjRecord(stageSelf->m_itemDef)->kind : 0;
    m_hidePart = new CharacterHidePart(this, kind);
    if (stageSelf != nullptr)
        m_hidePart->copyFrom(stageSelf->m_hidePart);
    applyCharacterModelFlags(kind);
}

// FS2 0x002bba20 (0x2bc0d0 the thunk)
// callcheck: ~CharacterHidePart, ~SubSceneModelObject, operator_delete

SubSceneCharacter::~SubSceneCharacter()
{
    delete m_hidePart;
}

// FS2 0x002bbab0
// callcheck: SubSceneModelObject

SubSceneCharacter::SubSceneCharacter(int role)
    : SubSceneModelObject(role)
{
}

// ──────────────────────────────────────────────────────────────────────
// The gate.
// FS2 0x002bbb00
// callcheck: setEffects

void SubSceneGateObject::setLampOn(bool on)
{
    m_gate->setEffects(on);
}

// PC: the stageSelf and view target tests (the same stand-ins as the map's).
// FS2 0x002bbb10
// callcheck: bindMotion, find1, operator_new, getKind, MapObjectParts, copyShown, operator_new, GateParts, copyFrom

void SubSceneGateObject::bindMotion(const char *name)
{
    SubSceneModelObject::bindMotion(name);
    StageObject *stageSelf = parentObjects()->find1(m_name, 0);
    m_parts = new MapObjectParts(this, m_collisionRecord, getKind() == StageObject_SubSceneMap);
    m_parts->copyShown(stageSelf != nullptr ? stageSelf->m_mapParts : nullptr);
    m_gate = new GateParts(this, m_parts->switches, m_parts->ids, m_parts->count, nullptr);
    StageViewTarget *target = gStageScene->parent->viewTarget;
    m_gate->copyFrom(target != nullptr ? target->gateParts : nullptr);
}

// FS2 0x002bbc10
// callcheck: attach, strcmp, collisionFindRecord, applyMapModelFlags, find1, operator_new, MapObjectParts, copyShown (a copy of SubSceneMapObject::setup in the ROM)

void SubSceneGateObject::setup(const char *name, ModelInstance *instance,
                               StringMap<Tim2Texture *> *textures, void *collision)
{
    SubSceneMapObject::setup(name, instance, textures, collision);
}

// FS2 0x002bbcf0 (0x2bc0c0 the thunk)
// callcheck: ~GateParts, ~MapObjectParts, ~SubSceneModelObject, operator_delete (the two bases inline)

SubSceneGateObject::~SubSceneGateObject()
{
    delete m_gate;
}

// FS2 0x002bbdb0
// callcheck: SubSceneModelObject

SubSceneGateObject::SubSceneGateObject(int role)
    : SubSceneMapObject(role)
{
}

// ──────────────────────────────────────────────────────────────────────
// The map.
// PC: the stageSelf test: a sub-scene's map may name what the stage has not.
// FS2 0x002bbe30
// callcheck: bindMotion, find1, operator_new, getKind, MapObjectParts, copyShown

void SubSceneMapObject::bindMotion(const char *name)
{
    SubSceneModelObject::bindMotion(name);
    StageObject *stageSelf = parentObjects()->find1(m_name, 0);
    m_parts = new MapObjectParts(this, m_collisionRecord, getKind() == StageObject_SubSceneMap);
    m_parts->copyShown(stageSelf != nullptr ? stageSelf->m_mapParts : nullptr);
}

// FS2 0x002bbed0
// callcheck: attach, strcmp, collisionFindRecord, applyMapModelFlags, find1, operator_new, MapObjectParts, copyShown

void SubSceneMapObject::setup(const char *name, ModelInstance *instance,
                              StringMap<Tim2Texture *> *textures, void *collision)
{
    attach(name, instance, textures);
    bool isRoot = strcmp(name, kRootObjectName) == 0;
    m_collisionRecord = collisionFindRecord(collision, name);
    applyMapModelFlags();
    if (isRoot) {
        StageObject *stageSelf = parentObjects()->find1(m_name, 0);
        m_parts = new MapObjectParts(this, m_collisionRecord, true);
        m_parts->copyShown(stageSelf != nullptr ? stageSelf->m_mapParts : nullptr);
    }
}

// FS2 0x002bbfb0 (0x2bc0b0 the thunk)
// callcheck: ~MapObjectParts, ~SubSceneModelObject, operator_delete

SubSceneMapObject::~SubSceneMapObject()
{
    delete m_parts;
}

// FS2 0x002bc040
// callcheck: SubSceneModelObject

SubSceneMapObject::SubSceneMapObject(int role)
    : SubSceneModelObject(role)
{
}

// ──────────────────────────────────────────────────────────────────────
// A character's model, shared with StageCharacter (its TU takes them).
// FS2 0x0017dcb0

void StageObject::applyCharacterModelFlags(int kind)
{
    if (m_model->data != nullptr)
        m_model->data->flags |= kind == CharacterKind_NoShadow
                                    ? ModelFlag_Item
                                    : ModelFlag_Item | ModelFlag_CastsShadow;
    m_model->m_animateSurface = m_model->m_kind1Surface != nullptr;
    m_model->m_ownAmbient = 1;
    m_model->m_lit = 1;
}

// FS2 0x0017e010
// callcheck: show

void CharacterHidePart::copyFrom(const CharacterHidePart *other)
{
    if (m_switch != nullptr && other->m_switch != nullptr)
        m_switch->show(other->m_switch->shown);
}

// FS2 0x0017e050
// callcheck: ~PartSwitch, operator_delete

CharacterHidePart::~CharacterHidePart()
{
    delete m_switch;
}

// FS2 0x0017e0b0
// callcheck: operator_new, PartSwitch, init

CharacterHidePart::CharacterHidePart(StageObject *owner, int kind)
    : m_switch(nullptr)
{
    if (kind == CharacterKind_HideD || kind == CharacterKind_NoShadow) {
        m_switch = new PartSwitch;
        m_switch->init(owner->m_model->data, kHidePartName, kHidePartCount, 0);
    }
}

/* What has no body of its own here, for the ledger:
 *
 * The slots the host's compiler writes (game/subsceneobject.h has them):
 *   FS2 0x002687c0 SubSceneLamp::~SubSceneLamp
 *   FS2 0x00269150 SubSceneModelObject::getKind
 *   FS2 0x002bb600 SubSceneItem::getKind
 *   FS2 0x002bbc00 SubSceneMapObject::getKind
 *   FS2 0x002bc090 SubSceneCharacter::getKind
 *   FS2 0x002bc0a0 SubSceneGateObject::getKind
 *
 * Empty in the ROM, the StageObject defaults on the host:
 *   FS2 0x00269a80 SubSceneModelObject::sideHook0
 *   FS2 0x00269a70 SubSceneModelObject::sideHook1
 *   FS2 0x002699e0 SubSceneModelObject::sideHook3
 *
 * The ROM's this-adjusting thunks (-0x40), which the host has no need of:
 *   FS2 0x002bc0b0 to SubSceneMapObject::~SubSceneMapObject
 *   FS2 0x002bc0c0 to SubSceneGateObject::~SubSceneGateObject
 *   FS2 0x002bc0d0 to SubSceneCharacter::~SubSceneCharacter
 *   FS2 0x002bc0e0 to SubSceneItem::setScriptFlag
 *   FS2 0x002bc0f0 to SubSceneItem::~SubSceneItem
 */
