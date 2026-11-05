/* ==========================================================================
 *  game/stagecharacter.cpp
 *
 *  A character of the stage being built: StageCharacter, vtable 0x312240 (the
 *  side base's 0x312264, the runner's 0x31227c), the F_ and player points of
 *  the map, the player and the partner among them. The run 0x17b3e0..0x17df60
 *  of unassigned@00171300; no source-path string, the file name is the
 *  port's. The character's sheet is stageObjRecord(m_itemDef), which the
 *  ROM inlines at every use. Its parts live elsewhere: CharacterInput
 *  0x239c60, CharacterBrain 0x1dd680, PlayerMotion 0x1ed410, CharacterGround
 *  0x1f3d20. What it does each frame is the code at 0x20a000..0x21d000.
 * ======================================================================== */
#include <stdlib.h>
#include <string.h>

#include "game/characterground.h"
#include "game/characterinput.h"
#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/modelpose.h"
#include "game/packtables.h"
#include "game/playermotion.h"
#include "game/scenepairs.h"
#include "game/scriptrunner.h"
#include "game/stagenode.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
#include "game/sysdata.h"

// The heap vector the node keeps its world position in.
constexpr u_int kPositionVectorSize = 0x10;

static const char kBonePelvis[]     = "bn_pelvis";      /* data 0x303200 */
static const char kBoneHead[]       = "bn_head";        /* data 0x303400 */
static const char kBoneRightThigh[] = "bn_r_thigh";     /* data 0x303428 */
static const char kBoneRightCalf[]  = "bn_r_calf";      /* data 0x303438 */
static const char kBoneSpineBase[]  = "bn_spine_base";  /* data 0x303388 */
static const char kBoneLeftFoot[]   = "bn_l_foot";      /* data 0x303330 */
static const char kBoneRightFoot[]  = "bn_r_foot";      /* data 0x303340 */

static const char kPartHead[]       = "head";           /* data 0x3032f8 */
static const char kPartHide[]       = "hide";           /* data 0x303360 */

static const char kEffectYamiBlood[]  = "yamiblood";    /* data 0x303368 */
static const char kEffectYamiSmoke1[] = "yamismoke1";   /* data 0x303378 */
static const char kEffectYamiSmoke2[] = "yamismoke2";   /* data 0x3033a0 */
static const char kEffectOtBlood[]    = "otblood";      /* data 0x303300 */
static const char kEffectBlood[]      = "BLOOD";        /* data 0x303308 */
static const char kEffectGlow[]       = "koh";          /* data 0x303398 */
static const char kEffectBathSplash[] = "bthamon";      /* data 0x303310 */
static const char kEffectSplash[]     = "L_SPLASH";     /* data 0x303318 */
static const char kEffectFootRipple[] = "F_HAMON";      /* data 0x303328 */
static const char kEffectBodyHead[]   = "shiryotai";    /* data 0x303408 */
static const char kEffectBody[]       = "shiryotai2";   /* data 0x303418 */

static const char kCharacterSrt[] = "F_SRT";            /* data 0x303350 */
static const char kCharacterYrt[] = "F_YRT";            /* data 0x303358 */

static const char kRouteAllDoors[] = "ALLDOOR";         /* data 0x3033b0 */

// The route's step filters, by the sheet's 段差Lv: a character opens the ones
// from its own level up.
static const char *const kRouteSteps[] = {              /* data 0x2f7bf0 */
    "STEP dummy", "STEP60", "STEP90", "STEP120", "STEP180", "STEP200",
};
constexpr u_int kRouteStepCount = sizeof(kRouteSteps) / sizeof(kRouteSteps[0]);

// What part of the pose's name says the body is: cameraBodyHeight's sizes.
// layout: host -- EE 8.
struct BodySizeName
{
    const char *name;           // FS2 +0x0
    int         size;           // FS2 +0x4
};
static const BodySizeName kBodySizes[] = {              /* data 0x2f7ba0 */
    { "_lh_", 0 }, { "_th_", 0 }, { "_mh_", 1 }, { "_sh_", 2 },
    { "_ch_", 3 }, { "_ms_", 1 }, { "_ss_", 2 }, { nullptr, 0 },
};
constexpr int kBodySizeDefault = 1;

// The glow's rotation.
static const sceVu0FVECTOR kGlowRotation = { 0.0f, -1.5707964f, 0.0f, 1.0f }; /* data 0x2f7be0 */

// The stages that have the second yami smoke.
constexpr int kSmokeStageFirst = 0x11;
constexpr int kSmokeStageLast  = 0x15;

constexpr float kIdleDelayRange = 10.0f;
constexpr float kIdleDelayFirst = 10.0f;
constexpr float kRandRange      = 2.1474836e+09f;

constexpr float kNoValue = -1.0f;       // reset's +0x458

constexpr u_char kGroundEnabled = 1;

// The named bone; the ROM takes it with no test.
// PC: a model with no such bone would read address 0 on the EE and carry on.
static StageNodeBase *characterBone(StageObject *object, const char *name)
{
    BoneMap::Node *bone = object->m_model->boneMap.find(name);
    return bone != nullptr ? bone->value : nullptr;
}

// A kept effect waits under the character until something starts it.
static void holdEffect(StageParticle *effect, StageObject *owner)
{
    effect->setRunning(false);
    effect->m_tree->setParent(owner->m_tree);
}

// ──────────────────────────────────────────────────────────────────────
// The ladders of the ROM, one clear per line; the stage kind's sheet is read
// again for each block.
// FS2 0x0017b3e0
// callcheck: memset, getStageSelect (stageObjRecord, inline at each block)

void StageCharacter::initAbilities()
{
    CharacterAbilities *a = &m_abilities;
    memset(a, 1, sizeof(*a));

    const StageObjRecord *record = stageObjRecord(m_itemDef);
    if (record->drivingSkill == 2)
        a->driving = 2;
    else if (record->drivingSkill == 0)
        a->driving = 0;
    else
        a->driving = 1;

    switch (record->stepLevel) {
    case 0: a->stepLevel = 0; break;
    case 1: a->stepLevel = 1; break;
    case 2: a->stepLevel = 2; break;
    case 3: a->stepLevel = 3; break;
    case 4: a->stepLevel = 4; break;
    case 5: a->stepLevel = 4; break;
    }
    if (record->ladder == 0)
        a->ladder = 0;
    if (record->jumpLevel == 0)
        a->jump = 0;

    switch (record->kind) {
    case CharacterKind_0:
    case CharacterKind_1:
    case CharacterKind_9:
    case CharacterKind_E:
        a->flag3be = 0;
        a->flag3c5 = 0;
        a->flag3b8 = 0;
        a->flag3cc = 0;
        if (a->stepLevel < 4)
            a->flag3c0 = 0;
        else
            a->flag3c1 = 0;
        break;

    case CharacterKind_2:
    case CharacterKind_4:
    case CharacterKind_8:
    case CharacterKind_A:
        a->quickTurn = 0;
        a->moveCrouched = 0;
        a->strafe = 0;
        a->runStart = 0;
        a->flag3ac = 0;
        a->flag3bf = 0;
        a->flag3c0 = 0;
        a->flag3c1 = 0;
        a->flag3c3 = 0;
        a->flag3c6 = 0;
        a->flag3ce = 0;
        if (record->kind == CharacterKind_A) {
            a->flag3b6 = 0;
            a->flag3c4 = 0;
        }
        else if (record->kind == CharacterKind_8 || record->kind == CharacterKind_2) {
            a->flag3c4 = 0;
        }
        break;

    case CharacterKind_Srt:
    case CharacterKind_5:
    case CharacterKind_6:
    case CharacterKind_Yrt:
    case CharacterKind_NoShadow:
    case CharacterKind_C:
    case CharacterKind_HideD:
    case CharacterKind_F:
        a->quickTurn = 0;
        a->flag3a5 = 0;
        a->flag3a6 = 0;
        a->moveCrouched = 0;
        a->runStart = 0;
        a->flag3ac = 0;
        a->flag3ad = 0;
        a->flag3b9 = 0;
        a->flag3ba = 0;
        a->flag3bb = 0;
        a->flag3bf = 0;
        a->flag3c0 = 0;
        a->flag3c1 = 0;
        a->flag3c3 = 0;
        a->flag3ce = 0;
        if (record->kind == CharacterKind_NoShadow) {
            a->strafe = 0;
            a->walkBack = 0;
            a->stairs = 0;
            a->flag3b6 = 0;
            a->flag3b7 = 0;
            a->flag3b8 = 0;
            a->flag3bc = 0;
            a->flag3bd = 0;
            a->flag3be = 0;
            a->flag3c2 = 0;
            a->flag3c3 = 0;
            a->flag3cc = 0;
            a->flag3cd = 0;
        }
        else if (record->kind == CharacterKind_HideD) {
            a->strafe = 0;
            a->walkBack = 0;
            a->stairs = 0;
            a->flag3b6 = 0;
            a->flag3b7 = 0;
            a->flag3b8 = 0;
            a->flag3bc = 0;
            a->flag3bd = 0;
            a->flag3be = 0;
            a->flag3cc = 0;
            a->flag3cd = 0;
        }
        else if (record->kind == CharacterKind_Srt) {
            a->strafe = 0;
            a->walkBack = 0;
            a->stairs = 0;
            a->flag3b6 = 0;
            a->flag3b7 = 0;
            a->flag3b8 = 0;
            a->flag3bc = 0;
            a->flag3bd = 0;
        }
        else if (record->kind == CharacterKind_Yrt) {
            a->strafe = 0;
            a->walkBack = 0;
            a->flag3b7 = 0;
            a->flag3b8 = 0;
            a->flag3bd = 0;
            a->flag3be = 0;
            a->flag3c5 = 0;
        }
        else if (record->kind == CharacterKind_F) {
            a->strafe = 0;
            a->stairs = 0;
            a->flag3b6 = 0;
            a->flag3b7 = 0;
            a->flag3b8 = 0;
            a->flag3bc = 0;
            a->flag3bd = 0;
            a->flag3be = 0;
            a->flag3c5 = 0;
        }
        else if (record->kind == CharacterKind_6) {
            a->strafe = 0;
            a->flag3b6 = 0;
            a->flag3b9 = 0;
            a->flag3bd = 0;
        }
        else if (record->kind == CharacterKind_C || record->kind == CharacterKind_5) {
            a->flag3b6 = 0;
            a->flag3b7 = 0;
            a->flag3b8 = 0;
        }
        break;

    case CharacterKind_None:
        memset(a, 0, sizeof(*a));
        break;
    }
}

// FS2 0x0017bb30
// callcheck: randNext

void StageCharacter::rollIdleDelay()
{
    m_idleDelay = ((float)randNext() * kIdleDelayRange) / kRandRange + kIdleDelayFirst;
}

// FS2 0x0017bc60
// callcheck: stopChain

void StageCharacter::stopBodyEffects()
{
    if (m_smoke[0] != nullptr)
        m_smoke[0]->stopChain();
    if (m_smoke[1] != nullptr)
        m_smoke[1]->stopChain();
    if (m_glow != nullptr)
        m_glow->stopChain();
    if (m_bodyEffects != nullptr) {
        for (int i = 0; i < kBodyEffectCount; i++) {
            if (m_bodyEffects[i] != nullptr)
                m_bodyEffects[i]->stopChain();
        }
    }
}

// The ROM clamps each value to 0..the sheet's (gp-0x7fa4..-0x7fb4 hold 0)
// with the sheet's value on both sides, which leaves it as it is.
// FS2 0x0017bd10
// callcheck: getStageSelect, setEndurance, setRightArm, setFlinch, randNext (stageObjRecord, inline at each field; rollIdleDelay inline)

void StageCharacter::initStatus()
{
    const StageObjRecord *record = stageObjRecord(m_itemDef);
    setEndurance(record->endurance);
    m_leftArm = record->leftArm;
    setRightArm(record->rightArm);
    m_rightLeg = record->rightLeg;
    m_leftLeg = record->leftLeg;
    m_health = (float)record->health;
    setFlinch(0);
    m_unarmedMsp = record->unarmedMsp;
    m_3e4 = 0;
    m_3e8 = 0;
    m_3ec = 0;
    m_3f0 = 0;
    m_winded = 0;
    m_idleTime = 0.0f;
    rollIdleDelay();
    m_450 = 0;
    m_altAction = 0;
    if (record->kind == CharacterKind_Yrt && m_ground->pushBody != nullptr)
        m_ground->pushBody->on = 1;
}

// FS2 0x002176d0
// callcheck: viewLinkedObject

bool StageCharacter::isViewLinked()
{
    return viewLinkedObject() == this;
}

// FS2 0x0017c620
// callcheck: FUN_0021a8c0, updateMatrix, initStatus, reset, reset, isViewLinked, getTopView, onViewTop, reset, reset, resetActionState, stopSounds

void StageCharacter::reset(int arg)
{
    m_block7c8.clear();
    updateMatrix();
    initStatus();
    m_motion->reset(arg);
    m_ground->reset();
    m_ground->m_enabled = kGroundEnabled;
    if (isViewLinked())
        gStageScene->getTopView()->onViewTop();
    if (m_input != nullptr)
        m_input->reset();
    if (m_brain != nullptr)
        m_brain->reset();
    resetActionState();
    stopSounds();
    m_slowStart = kNoValue;
}

// The items of the sheet's column, last placed first, until one takes.
// Inline in the ROM. PC: findAll3 answers a list that ends in a 0 and starts
// after a 0 (StageObjArray), which is where the walk stops.
static void giveItemsNamed(StageCharacter *character, const char *name)
{
    StageObjArray *items = gStageScene->objects->findAll3(name);
    if (items == nullptr)
        return;
    StageObject **slot = items->m_count == 0 ? nullptr : &items->m_data[items->m_count];
    while (slot != nullptr && !character->giveItem(*slot)) {
        slot--;
        if (*slot == nullptr)
            slot = nullptr;
    }
}

// FS2 0x0017c700
// callcheck: setName, addCharacter, add, findEntry, setEntry, setModel, getStageSelect, operator_new, operator_new, PartSwitch, init, initAbilities, createBlock98, operator_new, PlayerMotion, loadMotions, setupBody, strstr, operator_new, CharacterGround, findAll3, giveItem, modelFindPart, getCharacterClass, find, spawn, setRunning, FUN_0017e260, FUN_0017e1b0, memset, initSlots14, initSlots18, strcmp, setActive, setShown, allow, reset (stageObjRecord, applyCharacterModelFlags, CharacterHidePart and the object tree's setParent are inline there)

void StageCharacter::setup(const char *name, ModelInstance *instance, StringMap<Tim2Texture *> *textures,
                           StageList<void *> *motions, StageList<void *> *motions2, void *mdl)
{
    StageScene *scene = gStageScene;
    setName(name);
    scene->objects->addCharacter(this);
    scene->pairs->add(this, ScenePairList_Characters);
    setEntry(scene->scripts[0]->findEntry(m_name));
    setModel(instance, textures);

    const StageObjRecord *record = stageObjRecord(m_itemDef);
    applyCharacterModelFlags(record->kind);
    m_hidePart = new CharacterHidePart(this, record->kind);
    m_model->m_loop = 0;
    initAbilities();
    scene->createBlock98(this);

    m_motion = new PlayerMotion(this);
    m_motion->loadMotions(motions, motions2, mdl);
    m_bodyEffects = setupBody(true);

    m_bodySize = kBodySizeDefault;
    for (const BodySizeName *entry = kBodySizes; entry->name != nullptr; entry++) {
        if (strstr(m_model->pose->name, entry->name) != nullptr) {
            m_bodySize = entry->size;
            break;
        }
    }

    m_ground = new CharacterGround(this);

    giveItemsNamed(this, record->items[0]);
    giveItemsNamed(this, record->items[1]);

    ModelInstance *data = m_model->data;
    if (data != nullptr && data->node->meshCount != 0) {
        m_parts[0] = modelFindPart(data, kPartHead, sizeof(kPartHead) - 1, 0);
        m_parts[1] = data->partRecs;
    }

    if ((getCharacterClass() & 8) == 0) {
        m_bloodEffect = record->kind == CharacterKind_HideD ? kEffectOtBlood : kEffectBlood;
        m_smoke[0] = nullptr;
        m_smoke[1] = nullptr;
        m_glow = nullptr;
    }
    else {
        m_bloodEffect = kEffectYamiBlood;
        m_smoke[0] = scene->nodeTree->spawn(kEffectYamiSmoke1, characterBone(this, kBonePelvis),
                                            nullptr, nullptr, 0, 0);
        if (m_smoke[0] != nullptr)
            holdEffect(m_smoke[0], this);

        int stage = getStageSelect()->stage;
        if (stage >= kSmokeStageFirst && stage <= kSmokeStageLast) {
            m_smoke[1] = scene->nodeTree->spawn(kEffectYamiSmoke2, characterBone(this, kBonePelvis),
                                                nullptr, nullptr, 0, 0);
            if (m_smoke[1] != nullptr)
                holdEffect(m_smoke[1], this);
        }
        else {
            m_smoke[1] = nullptr;
        }

        m_glow = nullptr;
        if (record->kind == CharacterKind_C || record->kind == CharacterKind_5) {
            sceVu0FVECTOR rotation;
            memcpy(rotation, kGlowRotation, sizeof(rotation));
            m_glow = scene->nodeTree->spawn(kEffectGlow, characterBone(this, kBoneSpineBase),
                                            nullptr, rotation, 0, 0);
            if (m_glow != nullptr)
                m_glow->m_tree->setParent(m_tree);
        }
    }

    const char *splash = record->kind == CharacterKind_NoShadow ? kEffectBathSplash : kEffectSplash;
    m_splash = scene->nodeTree->spawn(splash, nullptr, nullptr, nullptr, 0, 0);
    if (m_splash != nullptr)
        holdEffect(m_splash, this);

    m_footEffects[0] = nullptr;
    m_footNodes[0] = nullptr;
    m_footEffects[1] = nullptr;
    m_footNodes[1] = nullptr;
    switch (record->kind) {
    case CharacterKind_0:
    case CharacterKind_1:
    case CharacterKind_2:
    case CharacterKind_4:
    case CharacterKind_8:
    case CharacterKind_9:
    case CharacterKind_A:
    case CharacterKind_E:
        for (int i = 0; i < kCharacterFeet; i++) {
            m_footEffects[i] = scene->nodeTree->spawn(kEffectFootRipple, nullptr, nullptr, nullptr, 0, 0);
            m_footNodes[i] = characterBone(this, i == 0 ? kBoneLeftFoot : kBoneRightFoot);
        }
        break;
    case CharacterKind_5:
    case CharacterKind_6:
    case CharacterKind_C:
    case CharacterKind_F:
        m_footEffects[0] = scene->nodeTree->spawn(kEffectFootRipple, nullptr, nullptr, nullptr, 0, 0);
        m_footNodes[0] = this;
        break;
    default:
        break;
    }
    for (int i = 0; i < kCharacterFeet; i++) {
        if (m_footEffects[i] != nullptr)
            holdEffect(m_footEffects[i], this);
    }

    memset(&m_allows, 0, sizeof(m_allows));
    m_door = nullptr;
    if (m_motion != nullptr)
        m_motion->initSlots14();
    memset(&m_wants, 0, sizeof(m_wants));
    if (m_motion != nullptr)
        m_motion->initSlots18();

    // F_YRT and F_SRT stand for many records (their name and the stage's):
    // the plain names stay out.
    if (record->kind == CharacterKind_Yrt || record->kind == CharacterKind_Srt) {
        const char *plain = record->kind == CharacterKind_Yrt ? kCharacterYrt : kCharacterSrt;
        bool stays = strcmp(m_name, plain) != 0;
        m_tree->setActive(stays);
        m_tree->setShown(stays);
    }

    if (m_brain != nullptr) {
        // PC: null until game/characterbrain.cpp fills the route.
        RouteFilter *filter = m_brain->route != nullptr ? m_brain->route->filter : nullptr;
        for (u_int step = (u_int)record->stepLevel; step < kRouteStepCount; step++)
            filter->allow(kRouteSteps[step], true);
        if (record->kind == CharacterKind_C || record->kind == CharacterKind_5
            || record->kind == CharacterKind_HideD)
            filter->allow(kRouteAllDoors, true);
    }

    reset(0);
}

// The record's size and centre of what the model's name says, and the two
// parts of a body with no mesh (the ROM's "shiryotai", 死霊体: a spirit).
// FS2 0x0017d970
// callcheck: find, strstr, operator_new[], find, spawn, find, spawn, find, spawn, find, spawn, FUN_0017e260, FUN_0017e1b0

StageParticle **StageObject::setupBody(bool keep)
{
    m_model->m_lightProxy = characterBone(this, kBonePelvis);

    // PC: an event's character whose motion file was not found has no pose.
    const MotionModel *body = nullptr;
    if (m_model->pose != nullptr) {
        for (int i = 0; i < kMotionModelCount; i++) {
            const MotionModel *entry = &gMotionModels[i];
            if (entry->tag != nullptr && strstr(m_model->pose->name, entry->tag) != nullptr) {
                body = entry;
                break;
            }
        }
    }
    if (body != nullptr) {
        memcpy(m_model->shadowSize, body->c, sizeof(m_model->shadowSize));
        memcpy(m_model->shadowOffset, body->d, sizeof(m_model->shadowOffset));
    }

    if (m_model->data != nullptr)
        return nullptr;

    StageParticle *onStack[kBodyEffectCount];
    StageParticle **effects = keep ? new StageParticle *[kBodyEffectCount] : onStack;
    StageNodeTree *tree = gStageScene->nodeTree;
    effects[0] = tree->spawn(kEffectBodyHead, characterBone(this, kBoneHead), nullptr, nullptr, 0, 0);
    effects[1] = tree->spawn(kEffectBody, characterBone(this, kBonePelvis), nullptr, nullptr, 0, 0);
    effects[2] = tree->spawn(kEffectBody, characterBone(this, kBoneRightThigh), nullptr, nullptr, 0, 0);
    effects[3] = tree->spawn(kEffectBody, characterBone(this, kBoneRightCalf), nullptr, nullptr, 0, 0);
    for (int i = 0; i < kBodyEffectCount; i++) {
        if (effects[i] != nullptr)
            effects[i]->m_tree->setParent(m_tree);
    }
    return keep ? effects : nullptr;
}

// The character's parts are gone before the object's own destructor runs.
// FS2 0x0017dd10
// callcheck: ~CharacterInput, ~PlayerMotion, ~CharacterGround, ~PartSwitch, operator_delete, operator_delete[], FUN_0017e150, ~StageObject (the brain's dtor through its vptr at +0xb4; the hide part inline: CharacterHidePart)

StageCharacter::~StageCharacter()
{
    delete m_input;
    delete m_brain;
    delete m_motion;
    delete m_ground;
    delete m_hidePart;
    delete[] m_bodyEffects;
}

// FS2 0x0017de30
// callcheck: StageObject, FUN_0017e140, setClock, operator_new, CharacterInput, alloc, operator_new, CharacterBrain, initSounds

StageCharacter::StageCharacter()
{
    m_itemDef = -1;
    setClock(gStageScene->clock);
    m_input = new CharacterInput(this);
    m_lastPosition = static_cast<float *>(gGlobalHeap.alloc(kPositionVectorSize, kPositionVectorSize));
    m_lightOn = 1;
    m_brain = nullptr;
    if (gStageScene->route != nullptr) {
        m_brain = new CharacterBrain(this);
        m_brain->runner = this;
    }
    m_lampItem = nullptr;
    m_weapons[0] = nullptr;
    m_weapons[1] = nullptr;
    m_parts[0] = nullptr;
    m_parts[1] = nullptr;
    m_290 = nullptr;
    m_294 = 0;
    m_lookAt = nullptr;
    m_shadowLight = nullptr;
    m_xform.rotationXyz = 1;
    m_gear = nullptr;
    m_42c = 0;
    initSounds();
}

// ──────────────────────────────────────────────────────────────────────
// PC: the terminate hook 0x179d30 is not written. Its first calls are the
// ground's end of the frame and the motion's 0x1e06f0, whose 0x1e18c0 opens
// with the copy of the motion's wall flag; the rest (the character's
// effects) waits for its TU.

void StageCharacter::sideHook3()
{
    m_ground->endFrame();
    m_motion->m_wallCheckNow = m_motion->m_wallCheck;
}

// FS2 0x00215ef0

bool StageObject::isPlayer()
{
    return gStageScene->player == this;
}

// FS2 0x00215ed0

bool StageObject::isPartner()
{
    return gStageScene->partner == this;
}
