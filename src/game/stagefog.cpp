// game/stagefog.cpp -- the fog that goes with a character's head
// (game/stagefog.h).

#include "game/stagefog.h"

#include <string.h>

#include "game/characterbrain.h"
#include "game/gamehooks.h"
#include "game/scenerender.h"
#include "game/stageitem.h"
#include "game/stageobjlist.h"
#include "game/stageparticle.h"
#include "game/sysdata.h"
#include "game/vu0math.h"

static const char kHatsuenItem[] = "I_HATUEN";  /* data 0x3038a8 */
static const char kHatsuFog[]    = "hatsu_fog"; /* data 0x30be58 */
static const char kKohFog[]      = "koh_fog";   /* data 0x30be68 */
static const char kHeadBone[]    = "bn_head";   /* data 0x30be70 */

// Both fogs spawn here, then go to a head.
static const float kSpawnPlace[4]    = { 0.0f, 0.0f, 0.0f, 1.0f };  /* rodata 0x2fc390 */
static const float kSpawnRotation[4] = { 0.0f, 0.0f, 0.0f, 1.0f };  /* rodata 0x2fc3a0 */

constexpr float kGlowFogReach = 500.0f;

// sceneRenderGroup's modes: the light set the fog puts the frame in.
constexpr int kRenderGroupGlow  = 1;
constexpr int kRenderGroupSmoke = 2;

// The kinds that glow (StageCharacter::m_glow).
static bool isGlowKind(int kind)
{
    return kind == CharacterKind_C || kind == CharacterKind_5;
}

// ──────────────────────────────────────────────────────────────────────
// Each fog whose owner became or stopped being the view's character, against
// what storeActive kept last frame.
// FS2 0x002ce0c0
// callcheck: setVisible, setVisible

void SmokeFog::setActiveFor(StageObject *linked, SceneRender *render)
{
    for (int i = 0; i < kSmokeFogs; i++) {
        CharacterFog *fog = m_fogs[i];
        StageObject *owner = fog->m_owner;
        if (owner == nullptr)
            continue;
        const bool on = owner == linked;
        if (on == (fog->m_shown != 0))
            continue;
        render->m_snap = 1;
        if (on) {
            if (fog->m_effect != nullptr)
                fog->m_effect->setVisible(true);
        } else {
            if (fog->m_effect != nullptr)
                fog->m_effect->setVisible(false);
        }
    }
}

// FS2 0x002ce180

void SmokeFog::storeActive(StageObject *linked)
{
    for (int i = 0; i < kSmokeFogs; i++) {
        CharacterFog *fog = m_fogs[i];
        if (fog->m_owner != nullptr)
            fog->m_shown = fog->m_owner == linked;
    }
}

// ──────────────────────────────────────────────────────────────────────
// show is the ROM's: not reset between fogs, so the fogs after the view's
// are shown too, whoever owns them.
// FS2 0x002ce1c0
// callcheck: vecDistance, setRunning, setVisible, sceneRenderGroup

void SmokeFog::updateRange(float range, StageObject *linked, SceneRender *render)
{
    bool show = false;
    for (int i = 0; i < kSmokeFogs; i++) {
        CharacterFog *fog = m_fogs[i];
        StageObject *owner = fog->m_owner;
        if (owner == nullptr)
            continue;

        bool inSmoke = false;
        for (int j = 0; j < m_sourceCount; j++) {
            if (vecDistance(m_sources[j]->m_xform.position, owner->m_xform.position) < range) {
                inSmoke = true;
                break;
            }
        }
        if (fog->m_effect != nullptr)
            fog->m_effect->setRunning(inSmoke);
        if (inSmoke && owner == linked)
            show = true;
        if (fog->m_effect != nullptr)
            fog->m_effect->setVisible(show);
    }
    if (show)
        render->m_forcedSet = sceneRenderGroup(kRenderGroupSmoke);
}

// FS2 0x002ce360
// callcheck: isSmoking, isInSmokeArea

void SmokeFog::addObject(StageThrownItem *item)
{
    if (m_sourceCount < kSmokeSources && item->isSmoking() && item->isInSmokeArea())
        m_sources[m_sourceCount++] = item;
}

// ──────────────────────────────────────────────────────────────────────
// Nothing is done once the first fog has its owner. The list is walked to
// its end although only eight can have one; F_SRT, F_YRT and kind 0xe go
// without.
// FS2 0x002ce3e0
// callcheck: attachTo, getStageSelect (stageObjRecord)

void SmokeFog::attachCharacters(StageObjectList *objects)
{
    if (m_fogs[0]->m_owner != nullptr)
        return;

    StageObjArray &characters = objects->m_lists[StageObjGroup_Characters];
    int used = 0;
    StageObject **slot = characters.begin();
    while (slot != nullptr && (*slot)->m_active == 0)
        slot = StageObjArray::next(slot);

    while (slot != nullptr) {
        StageObject *character = *slot;
        const int kind = stageObjRecord(character->m_itemDef)->kind;
        if (kind != CharacterKind_E && kind != CharacterKind_Yrt && kind != CharacterKind_Srt
            && used < kSmokeFogs) {
            m_fogs[used++]->attachTo(character);
        }
        slot = StageObjArray::next(slot);
        while (slot != nullptr && (*slot)->m_active == 0)
            slot = StageObjArray::next(slot);
    }
}

// FS2 0x002ce5a0
// callcheck: operator_delete, operator_delete

SmokeFog::~SmokeFog()
{
    for (int i = 0; i < kSmokeFogs; i++)
        delete m_fogs[i];
}

// ──────────────────────────────────────────────────────────────────────
// Every smoke candle of a stage asks; the first makes the set.
// FS2 0x002ce620
// callcheck: strcmp, operator_new, operator_new, spawn, setRunning

SmokeFog *SmokeFog::createFor(StageObject *owner, StageNodeTree *tree)
{
    if (strcmp(owner->m_name, kHatsuenItem) != 0)
        return nullptr;

    SmokeFog *set = new SmokeFog;
    for (int i = 0; i < kSmokeFogs; i++) {
        CharacterFog *fog = new CharacterFog;
        fog->m_effect = tree->spawn(kHatsuFog, nullptr, kSpawnPlace, kSpawnRotation, 0, 0);
        if (fog->m_effect != nullptr)
            fog->m_effect->setRunning(false);
        set->m_fogs[i] = fog;
    }
    set->m_sourceCount = 0;
    return set;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ce790
// callcheck: setRunning, sceneRenderGroup

void GlowFog::updateVisible(SceneRender *render)
{
    bool on = false;
    if (m_effect != nullptr) {
        if (m_unmeasured == 0 && m_nearest < kGlowFogReach)
            on = true;
        m_effect->setRunning(on);
    }
    if (on)
        render->m_forcedSet = sceneRenderGroup(kRenderGroupGlow);
}

// ──────────────────────────────────────────────────────────────────────
// A character in BrainState_Normal neither burns nor counts, and its glow is
// left as it was.
// FS2 0x002ce820
// callcheck: setRunning, vecDistance, getStageSelect (stageObjRecord), getBrainPace (slot +0x278)

void GlowFog::updateFor(StageCharacter *who, StageCharacter *player)
{
    if (!isGlowKind(stageObjRecord(who->m_itemDef)->kind))
        return;
    const int pace = who->getBrainPace();
    if (pace != BrainState_Slot4c && pace != BrainState_Slot4b && pace != BrainState_Slot4a
        && pace != BrainState_Foe) {
        return;
    }

    const int endurance = who->m_endurance;
    if (who->m_glow != nullptr)
        who->m_glow->setRunning(endurance > 0);
    if (endurance > 0 && player != nullptr) {
        const float distance = vecDistance(who->m_xform.position, player->m_xform.position);
        if (m_unmeasured != 0) {
            m_nearest = distance;
            m_unmeasured = 0;
        } else if (distance < m_nearest) {
            m_nearest = distance;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// Every character of the stage asks; the first of a glowing kind makes it.
// FS2 0x002ce9d0
// callcheck: operator_new, spawn, setRunning, getStageSelect (stageObjRecord)

GlowFog *GlowFog::createFor(StageObject *owner, StageNodeTree *tree)
{
    if (!isGlowKind(stageObjRecord(owner->m_itemDef)->kind))
        return nullptr;

    GlowFog *fog = new GlowFog;
    fog->m_effect = tree->spawn(kKohFog, nullptr, kSpawnPlace, kSpawnRotation, 0, 0);
    if (fog->m_effect != nullptr)
        fog->m_effect->setRunning(false);
    fog->m_unmeasured = 1;
    return fog;
}

// FS2 0x002ceb70
// callcheck: setVisible, setVisible

void GlowFog::setActive(bool on, SceneRender *render)
{
    if (on == (m_shown != 0))
        return;
    render->m_snap = 1;
    if (on) {
        if (m_effect != nullptr)
            m_effect->setVisible(true);
    } else {
        if (m_effect != nullptr)
            m_effect->setVisible(false);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The ROM detaches twice, once before and once inside each setParent.
// FS2 0x002cebd0
// callcheck: find, erase, erase, pushBack, erase, erase, pushBack

void CharacterFog::attachTo(StageObject *who)
{
    if (m_effect != nullptr) {
        StageNodeBase *head = nullptr;
        if (who != nullptr && who->m_model != nullptr) {
            auto *bone = who->m_model->boneMap.find(kHeadBone);
            if (bone != nullptr)
                head = bone->value;
        }
        m_effect->setParent(head);
        if (head != nullptr)
            m_effect->m_tree->setParent(who->m_tree);
    }
    m_owner = who;
}
