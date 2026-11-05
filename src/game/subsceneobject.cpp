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
#include "game/stagescene.h"
#include "game/sysdata.h"
#include "game/uiwidget.h"      /* UiShowFlags */

namespace {

constexpr char kBoneTop[]        = "bn_top";    // data 0x308950, 0x30b440
constexpr char kRootObjectName[] = "Root";      // data 0x30b448
constexpr char kHidePartName[]   = "hide";      // data 0x303360
constexpr int  kHidePartCount    = 2;
// StageObjRecord::kind: the two with a hide part; 0xb also casts no shadow.
constexpr int  kCharacterKindNoShadow = 0xb;
constexpr int  kCharacterKindHideD    = 0xd;

constexpr float kObjectFrameStep = 1.2f;        // what sideHook2 hands StageModel::advance
constexpr u_int kNamedMotionFlags = 0x50;       // DemoCamera's cuts pass the same
constexpr float kLightTurn = -1.5707964f;       // a light's axis along the bone

constexpr int   kFramesPerSecond30 = 30;
constexpr float kDegreesToRadians  = 0.017453292f;
constexpr float kRadiansToDegrees  = 57.295776f;
constexpr int   kFrameStepUnit     = 300;       // Prop_FrameStep: of these, a frame's worth

// What getProperty and setProperty take. The numbers are the scripts'; the
// names are read from these two functions (ScriptRunner::getProperty's own
// list is unread).
enum SubSceneProperty
{
    Prop_StageCleared   = 0x00,     // sub: stage - 1; 0 no, 1 variant 0, 2 both
    Prop_Archive        = 0x01,     // sub: id - 1
    Prop_Random100      = 0x02,
    Prop_YawDegrees     = 0x03,
    Prop_MissionFrames  = 0x39,
    Prop_Frames30       = 0x3a,     // the object's clock in 30ths
    Prop_Fps            = 0x3b,
    Prop_Result         = 0x3c,
    Prop_ScriptVar3e    = 0x3e,
    Prop_ScriptVar3f    = 0x3f,
    Prop_Group          = 0x40,     // always 4
    Prop_FrameStep      = 0x4d,
    Prop_SwitchedOn     = 0x4f,
    Prop_PlayerId       = 0x50,
    Prop_PartnerId      = 0x51,
    Prop_ActorId        = 0x52,     // who acts for the player, or the player
    Prop_Handle         = 0x54,
    Prop_Restore        = 0x55,
    Prop_ShowFlag       = 0x56,     // sub: an index of kShowFlagByte
    Prop_Shown          = 0x59,
    Prop_ResultPaused   = 0x6d,
    Prop_ResultHold     = 0x6e,
    Prop_ClockFrames    = 0x6f,
    Prop_ScoreClock     = 0x70,
    Prop_ViewActive     = 0x72,
};

// Prop_ShowFlag's numbers: the byte of UiShowFlags each one is. The set
// stores "value != 1" there and the get answers the byte's inverse, so 1
// means shown, except 7, which is the other way round.
constexpr int kShowFlagCount = 17;
constexpr signed char kShowFlagByte[kShowFlagCount] = {
    0, 1, 2, 3, 4, 9, 5, 10, 6, 7, 13, 12, 11, 14, 8, 15, 16,
};
constexpr int kShowFlagInverted  = 7;   // the byte holds the value as it is
constexpr int kShowFlagWithFirst = 6;   // read: the byte and'ed with byte 0

constexpr int kArchiveVariantUnlock = 0x40;     // the one id with rules of its own
constexpr int kArchiveWanted   = 1;
constexpr int kArchiveHeld     = 2;
constexpr int kArchiveReported = 3;             // read only
constexpr int kArchiveStates   = 3;

StageObjectList *parentObjects()
{
    return gStageScene->parent->objects;
}

void hangLight(StageLight *light, StageMatrixNode *parent)
{
    light->m_xform.setRotation(0.0f, 0.0f, kLightTurn);
    light->setParent(parent);
}

// A stage's clear state: 0 not, 1 in variant 0, 2 in both.
int stageClearedState(int stage)
{
    int kind = getStageSelect()->kind;
    SaveData *save = getSaveData();
    if (!save->testStageFlags(stage, save->clearBit(kind, 0)))
        return 0;
    if (!save->testStageFlags(stage, save->clearBit(kind, 1)))
        return 1;
    return 2;
}

// Records it; both unlock flags set then mark the save, as a clear does.
void markStageCleared(int stage, int variant)
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
// FS2 0x00268ae0 (0x1a6890 the runner's thunk)
// callcheck: getKind, setTreeShown, setYaw, currentMission, setScoreClock, setClockFrames, setResultHold, setResultPaused, findCharacterById, setPlayer, setScriptVar3f, setScriptVar3e, setResultValue, getSaveData, pickVariant3ac, clearBit, markUnlock468

void SubSceneModelObject::setProperty(int id, int sub, int value)
{
    StageScene *scene = gStageScene;
    switch (id) {
    case Prop_ViewActive:
        if (getKind() == StageObject_Kind3)
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
            scene->setPlayer((StagePlayer *)who);
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

// FS2 0x00269160 (0x1a68a0 the runner's thunk)
// callcheck: getStageSelect, isShown, getHandle, getSeconds, currentMission, getScoreClock, getClockFrames, getResultHold, getResultPaused, onRestore, getDeltaSeconds, getScriptVar3f, getScriptVar3e, getResultValue, stageFps, getMissionFrames, random, getSaveData, clearBit, testStageFlags (property 0x6c tests the kind against 0 and 0x13..0x17, then two tables: no class of this family is of those kinds, so it answers 0)

int SubSceneModelObject::getProperty(int id, int sub)
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
    default:                    // 0x3d, 0x4f (the runner's 0x172050), 0x5d, 0x6c: 0
        return 0;
    }
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

// PC: the stageSelf test: StageCharacter's ctor and setup are stubs, so a stage
// has no characters and the pack's file stands in. Delete with StageCharacter.
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
// from its +0x200; StageCharacter does not exist yet, so a character with no
// stage self counts as kind 0 and hides nothing. Delete with StageCharacter.
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
        m_model->data->flags |= kind == kCharacterKindNoShadow
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
    if (kind == kCharacterKindHideD || kind == kCharacterKindNoShadow) {
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
