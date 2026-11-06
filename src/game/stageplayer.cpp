/* ==========================================================================
 *  game/stageplayer.cpp
 *
 *  What a character does around the frame's scripts (StageScene::
 *  runStageScripts), and what it needs for it:
 *
 *    0x21c310..0x21ca70    beforeScripts, afterScripts, stepStatus
 *    0x20d8c0, 0x210940, 0x210b30, 0x217a80  how visible it is, what it sees
 *    0x214cf0..0x216270    the small answers about a character
 *    0x242e80..0x243340    SceneNoiseList, the frame's noises
 *    0x17e950..0x17f040    the scripted volumes and the markers
 *
 *  The roadmap called 0x20d8c0 "the move" for its getMoveScale calls. It
 *  moves nothing: it works out how hard the character is to see, and
 *  moveScale is concealment by posture (StageMission::kMoveScales: standing
 *  1.1, walking 1.0, running 0.8, crouched 1.25, 1.15, 0.9).
 *
 *  Doubts: the file name and its extent are the port's; the names of the
 *  noise kinds are read from who may hear them, not from the game.
 * ======================================================================== */
#include <float.h>
#include <math.h>
#include <string.h>

#include "game/characterbrain.h"
#include "game/characterground.h"
#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/modelpose.h"
#include "game/playermotion.h"
#include "game/seplayer.h"
#include "game/stagecollision.h"
#include "game/stagedoor.h"
#include "game/stageitem.h"
#include "game/stagelight.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
#include "game/stagesound.h"
#include "game/sysdata.h"
#include "game/viewcamera.h"
#include "game/vu0math.h"

// The head a character looks from and is seen by. data 0x303178
static const char kHeadBone[] = "bn_head";

// The sheet's ctype: a body of getCharacterClass's bit 8 sees whatever the
// target's visibility.
constexpr u_int kClassIgnoresConcealment = 8;

// isPointInView: at this distance a half circle counts as the view.
constexpr float kCloseViewRange = 150.0f;
constexpr float kCloseViewAngle = 3.1415927f;
constexpr u_int kViewRayMask  = 0x1e42600f;
constexpr u_int kViewRayFlags = 0x800;

// stepStatus: rays from the head to a noise. A wall takes half of its reach,
// two take all of it. The second mask is the first without bit 0x10000000.
constexpr u_int kNoiseRayMask     = 0xe42600f;
constexpr u_int kNoiseRayFlags    = 0x200;
constexpr u_int kNoiseRayNextFlags = 0x600;
constexpr u_int kFaceThin         = 0x400;  // StageCollisionFace::flags: a thin wall: only half
constexpr int   kNoiseMuffleWhole = 0x400;
constexpr int   kNoiseMuffleHalf  = 0x200;

// beforeScripts: how wide a thrown item is seen. 0x3f860a92: 60 degrees.
constexpr float kThrownViewAngle = 0x1.0c1524p+0f;

// updateVisibility: the indices of StageMission::moveScale.
enum MoveScaleIndex
{
    MoveScale_Stand       = 0,
    MoveScale_Walk        = 1,
    MoveScale_Run         = 2,
    MoveScale_Crouch      = 3,
    MoveScale_CrouchWalk  = 4,
    MoveScale_CrouchRun   = 5,
};
// A lamp in its hand leaves a quarter of the concealment.
constexpr float kLampConcealment = 0.25f;
// Name positions of a VISIBILITY_ volume: "VISIBILITY_MASK", "VISIBILITY_01".
constexpr int kVolumeNameMask  = 11;
constexpr int kVolumeNameLevel = 12;

// A lit seer reaches at least this far into the dark, whatever the target.
constexpr float kLampMinVisibility = 0.5f;

// isScenarioLead: the stages it asks about (StageSelect::stage).
constexpr int kLeadStageA = 0x18;
constexpr int kLeadStageB = 0x15;
constexpr int kLeadStageC = 0xc;
constexpr int kLeadStageD = 2;
constexpr int kLeadStageE = 6;
constexpr int kLeadStageF = 1;
constexpr int kKindLeadShared = 9;  // StageObjRecord::kind, told apart by the name
constexpr int kNameLeadDigit  = 10; // its name's character

// getActionState's motion ids of a grab.
constexpr int kMotionHeldA = 0xa0;
constexpr int kMotionHeldB = 0xa1;
// The scope's view (ViewCamera). data 0x306f60
static const char kZoomView[] = "camview_zoom";
// Model names. data 0x306f80, 0x306f88
static const char kModelYri[] = "yri";
static const char kModelKne[] = "kne";

static const sceVu0FVECTOR *headMatrix(StageObject *object)
{
    BoneMap::Node *bone = object->m_model->boneMap.find(kHeadBone);
    return bone != nullptr ? bone->value->getMatrix() : object->getMatrix();
}

static bool isLitByHardLight(StageObject *object)
{
    return object->m_shadowLight != nullptr && object->m_shadowLight->getMode() == StageLight_Hard;
}

// The volume of list holding point; null none. Walks the active objects
// only (StageObjIterator 0x21ec30, inline here).
static StageVolume *findVolume(StageObjArray *list, const float *point)
{
    for (StageObject **slot = list->begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        if (!(*slot)->m_active)
            continue;
        StageVolume *volume = static_cast<StageVolume *>(*slot);
        if (volume->contains(point))
            return volume;
    }
    return nullptr;
}

static bool isFlying(const StageThrownItem *item)
{
    return item->m_inUse && item->m_moving;
}

// ---------------------------------------------------------------- the frame

// FS2 0x0021c7c0
// callcheck: stepStatus, ageSightTable, updateVisibility, currentMission, canSee, set (the last two in watchThrownItems)

void StageCharacter::beforeScripts()
{
    stepStatus();
    ageSightTable();
    updateVisibility();
    if (gStageScene->currentMission()->thrownItems.count >= 1)
        watchThrownItems();
    for (int i = 0; i < FoeCache_Count; i++)
        m_foeCache[i] = foeNotAsked();
    m_nearestFoeDistance = -1;
    m_placedThisFrame = 0;
}

// The middle of 0x21c7c0.
// callcheck: set, canSee, canSee, currentMission, currentMission, currentMission, canSee, canSee, set, set

void StageCharacter::watchThrownItems()
{
    StageThrownItem *flying = m_seenFlying;
    StageThrownItem *lying  = m_seenThrown;
    float distance;

    if (m_followedFlying != nullptr) {
        if (!m_followedFlying->m_active) {
            m_followedFlying = nullptr;
        } else if (!isFlying(m_followedFlying)) {
            m_point760.set(m_followedFlying->m_xform.position);
            m_followedFlying = nullptr;
        }
    }
    if (lying != nullptr) {
        if (!lying->m_active) {
            lying = nullptr;
        } else {
            distance = 0.0f;
            if (!canSee(kThrownViewAngle, lying, &distance))
                lying = nullptr;
        }
    }
    if (flying != nullptr) {
        bool keep = false;
        if (flying->m_active && isFlying(flying)) {
            distance = 0.0f;
            keep = canSee(kThrownViewAngle, flying, &distance);
        }
        if (!keep)
            flying = nullptr;
    }

    if (flying == nullptr || lying == nullptr) {
        StageMission *mission = gStageScene->currentMission();
        for (int i = 0; i < mission->thrownItems.count; i++) {
            StageThrownItem *item = static_cast<StageThrownItem *>(mission->thrownItems.objects[i]);
            if (!item->m_active || static_cast<StageObject *>(item)->m_parent != nullptr || !item->m_inUse)
                continue;
            distance = 0.0f;
            if (isFlying(item)) {
                if (flying == nullptr && canSee(kThrownViewAngle, item, &distance)) {
                    flying = item;
                    if (lying != nullptr)
                        break;
                }
            } else if (lying == nullptr && canSee(kThrownViewAngle, item, &distance)) {
                lying = item;
                if (flying != nullptr)
                    break;
            }
        }
    }

    if (flying != nullptr && m_followedFlying != flying) {
        m_point740.set(flying->getThrowVector());
        m_point760.isSet = 0;
        m_followedFlying = flying;
    }
    m_seenFlying = flying;
    if (lying != nullptr && m_seenThrown != lying) {
        m_seenThrown = lying;
        m_point780.set(lying->m_xform.position);
    }
}

// FS2 0x0021c310
// callcheck: getBrainPace, getBrainPace, setShown, setChildrenActive

void StageCharacter::afterScripts()
{
    StageDoor *door = m_door;
    if (door == nullptr || door->m_user != this) {
        m_doorInUse = nullptr;
    } else {
        m_doorInUse = door;
        m_lastDoorMode = door->m_doorMode;
    }
    m_hitThisFrameBy = nullptr;
    m_628 = 0;
    m_610 = 0;
    m_670 = getBrainPace();
    m_bulletFlying = 0;
    m_hitThisFrame = 0;
    if (getBrainPace() == BrainState_Off) {
        m_tree->setShown(false);
        m_viewActive = 0;
        m_ground->m_enabled = 0;
        m_tree->setChildrenActive(false);
    }
}

// FS2 0x0021c3d0
// callcheck: find, isFromAlly, place, distanceToPoint, raycast, raycast, randNext, place, set, clearAllSlots, setActive, setShown

void StageCharacter::stepStatus()
{
    const SceneNoise *heard = nullptr;
    float quietest = FLT_MAX;
    int   bits = 0;

    m_heardFrom = nullptr;
    m_lastNoiseRange = m_noiseRange;
    m_noiseRange = FLT_MAX;

    const float hearing = m_hearing;
    const sceVu0FVECTOR *head = headMatrix(this);
    const SceneNoiseList::List &list = gStageScene->noises->front();

    for (u_int i = 0; i < list.count; i++) {
        const SceneNoise *noise = &list.noises[i];
        if (noise->source == this)
            continue;
        if (noise->kind != SceneNoise_Any && (noise->kind == SceneNoise_Ally) != noise->isFromAlly(this))
            continue;

        const float *place = noise->place();
        const float distance = distanceToPoint(place);
        const float loudness = noise->loudness;
        if (loudness < distance)
            continue;

        int muffle = 0;
        sceVu0FVECTOR from, hit;
        memcpy(from, head[3], sizeof(from));
        StageCollisionFace *face = gStageScene->raycast(hit, from, place, 0, kNoiseRayMask, kNoiseRayFlags, nullptr);
        if (face != nullptr) {
            if ((face->flags & kFaceThin) == 0) {
                muffle = kNoiseMuffleWhole;
            } else {
                muffle = kNoiseMuffleHalf;
                memcpy(from, hit, sizeof(from));
                if (gStageScene->raycast(hit, from, place, 0, kNoiseRayMask, kNoiseRayNextFlags, nullptr) != nullptr)
                    muffle = kNoiseMuffleWhole;
            }
        }
        if (muffle != 0 && distance > loudness * ((float)(0x400 - muffle) / 1024.0f))
            continue;

        switch (noise->kind) {
        case SceneNoise_Sound: {
            int chance = 0;
            if (loudness != 0.0f)
                chance = (int)((hearing * (loudness - distance) * 100.0f) / loudness);
            if (randNext() % 100 < chance) {
                bits |= kHeardSound;
                if (loudness < quietest) {
                    quietest = loudness;
                    heard = noise;
                }
            }
            break;
        }
        case SceneNoise_Any:  bits |= kHeardAny;  break;
        case SceneNoise_Ally: bits |= kHeardAlly; break;
        case SceneNoise_Foe:  bits |= kHeardFoe;  break;
        }
    }
    m_scriptValues[kScriptValueHeard] = bits;

    if (heard != nullptr) {
        m_point6e0.set(heard->place());
        m_heardFrom = heard->source;
    }

    if (m_68c > 0)
        m_68c--;
    if (m_lifeOverride >= 0) {
        if (m_lifeOverride == 0) {
            m_lifeOverride = -1;
            m_brain->clearAllSlots();
            m_tree->setActive(false);
            m_tree->setShown(false);
        } else {
            m_lifeOverride--;
        }
    }
}

// FS2 0x00217a80
// callcheck: currentMission, currentMission, currentMission, getLifeState, getLifeState, getLifeState, isEnemyOf, currentMission

void StageCharacter::ageSightTable()
{
    StageMission *mission = gStageScene->currentMission();
    for (int i = 0; i < mission->characterCount; i++) {
        StageCharacter *other = static_cast<StageCharacter *>(mission->characters[i]);
        if (other == nullptr)
            return;
        SightEntry *line = &m_sightTable[i];
        const int state = other->getLifeState();
        if (state > 1 && state < 6) {
            if (state != 2 || !m_sightOwner->isEnemyOf(other))
                line->seen = 0;
        } else {
            line->seen = 0;
        }
        line->frame = -1;
    }
}

// How visible it is: 1 less the concealment of its posture and of the dark
// of the volume it stands in. The volume it stood in is asked first, and a
// VISIBILITY_MASK overrides any other that is not a mask.
// FS2 0x0020d8c0
// callcheck: currentMission, contains, contains, contains, getDefaultMoveScale, getMoveScale, isLampOn, StageObjIterator (inline, findVolume)

float StageCharacter::updateVisibility()
{
    if (isLitByHardLight(this)) {
        m_visibility = 1.0f;
        m_areaDark = 1.0f;
        return m_visibility;
    }

    StageMission *mission = gStageScene->currentMission();
    const float *position = m_xform.position;
    StageVolume *last = m_area;
    StageVolume *found = last;
    int level = -1;

    if (last != nullptr) {
        if (last->m_name[kVolumeNameMask] == 'M') {
            if (!last->contains(position))
                found = nullptr;
        } else {
            level = 0;
            found = mission->visibility[0].objects != nullptr
                ? findVolume(mission->visibility[0].objects, position) : nullptr;
            if (found == nullptr && last->contains(position)) {
                level = -1;
                found = last;
            }
        }
    }
    while (found == nullptr && level <= 2) {
        level++;
        if (mission->visibility[level].objects != nullptr)
            found = findVolume(mission->visibility[level].objects, position);
    }

    float dark = -1.0f;
    if (found != nullptr) {
        if (level == -1) {
            switch (found->m_name[kVolumeNameLevel]) {
            case '3': level = 3; break;
            case '2': level = 2; break;
            case '1': level = 1; break;
            default:  level = 0; break;
            }
        }
        dark = mission->visibility[level].range;
    }
    if (dark < 0.0f)
        dark = gStageScene->getDefaultMoveScale();
    m_area = found;

    // The light falling on the player or the partner.
    bool  plain  = true;
    float weight = 0.0f;
    if ((gStageScene->player == this || gStageScene->partner == this) && m_shadowLight != nullptr) {
        weight = m_shadowWeight;
        plain  = m_shadowLight->m_yamLight == 0;
    }

    const int motion = m_motion->motionId;
    MoveScaleIndex posture;
    if (isCrouchMotion(motion)) {
        if (isCrouchRunMotion(motion))
            posture = MoveScale_CrouchRun;
        else
            posture = isMovingMotion(motion) ? MoveScale_CrouchWalk : MoveScale_Crouch;
    } else if (isRunMotion(motion) || isCrouchRunMotion(motion)) {
        posture = MoveScale_Run;
    } else {
        posture = isMovingMotion(motion) ? MoveScale_Walk : MoveScale_Stand;
    }
    const float postureScale = gStageScene->getMoveScale(posture);

    const float lamp = isLampOn() ? kLampConcealment : 1.0f;
    const float lit  = (1.0f - dark) * weight + dark;
    const float area = plain ? dark * (1.0f - weight) : lit;
    float concealment = lamp * postureScale * area;
    if (concealment > 1.0f)
        concealment = 1.0f;
    if (concealment < 0.0f)
        concealment = 0.0f;

    m_visibility = 1.0f - concealment;
    m_areaDark = plain ? dark : lit;
    return m_visibility;
}

// FS2 0x00210940
// callcheck: getKind, getKind, isLampOn, find, isPointInView, find, isPointInView, getMatrix, isPointInView

bool StageCharacter::canSee(float viewAngle, StageObject *target, float *distance)
{
    if (target->getKind() != StageObject_Character)
        return isPointInView(viewAngle, 1.0f, target->getMatrix()[3], distance, nullptr);

    StageObject *skip = m_gear != nullptr ? m_gear : target->m_gear;
    float scale = static_cast<StageCharacter *>(target)->m_visibility;
    if (isLampOn() && !isLitByHardLight(target)) {
        scale *= 2.0f;
        if (scale <= kLampMinVisibility)
            scale = kLampMinVisibility;
    }
    return isPointInView(viewAngle, scale, headMatrix(target)[3], distance, skip);
}

// FS2 0x00210b30
// callcheck: getStageSelect, getLampMatrix, applyMatrix, vecDirection, vecNormalize, vecDot, acosf, segmentHitsAny

bool StageCharacter::isPointInView(float viewAngle, float scale, const float *point, float *distance, StageObject *skip)
{
    if (getCharacterClass() == kClassIgnoresConcealment)
        scale = 1.0f;

    sceVu0FMATRIX lamp;
    m_motion->getLampMatrix(lamp);
    sceVu0FVECTOR eye;
    memcpy(eye, lamp[3], sizeof(eye));

    sceVu0FVECTOR toPoint = { point[0] - eye[0], point[1] - eye[1], point[2] - eye[2], point[3] - eye[3] };
    const float square = toPoint[0] * toPoint[0] + toPoint[1] * toPoint[1] + toPoint[2] * toPoint[2];
    if (m_sight * m_sight < square)
        return false;
    *distance = sqrtf(square);
    if (scale * m_sight <= *distance)
        return false;

    sceVu0FVECTOR forward = { 0.0f, 1.0f, 0.0f, 1.0f };     // data 0x2fa7d0
    applyMatrix(forward, lamp, forward);
    vecDirection(forward, forward, eye);
    vecNormalize(toPoint, toPoint);
    const float angle = acosf(vecDot(toPoint, forward)) * 2.0f;
    if (angle > viewAngle && (angle > kCloseViewAngle || *distance > kCloseViewRange))
        return false;
    return !gStageScene->segmentHitsAny(eye, point, 0, kViewRayMask, kViewRayFlags, skip);
}

// ------------------------------------------------- the answers about one

// FS2 0x00214cf0
// callcheck: isEnemyOf, isEnemyOf

bool StageCharacter::isAlly(StageObject *other)
{
    return !isEnemyOf(other) && !static_cast<StageCharacter *>(other)->isEnemyOf(this);
}

// FS2 0x00214f60
// callcheck: isRunning

u_char StageObject::getAltSoundFlag()
{
    if (gStageScene->player == this)
        return gStageScene->party->isRunning();
    return static_cast<StageCharacter *>(this)->isHoldingAction();
}

// FS2 0x00214fc0
// callcheck: isMode1 (inline), getTopView, getKind, strcmp

int StageObject::getActionState()
{
    if (m_gear != nullptr && m_abilities.driving == 1)
        return CharacterAction_Driving;

    StageObject *view = gStageScene->getTopView();
    if (view->getKind() == StageObject_Kind10 && strcmp(view->m_name, kZoomView) == 0)
        return CharacterAction_Scope;

    const int motion = m_motion->motionId;
    if (isLadderMotion(motion))
        return CharacterAction_Ladder;
    if (motion == kMotionHeldA || motion == kMotionHeldB)
        return CharacterAction_Held;
    if ((m_abilities.flag3c3 && static_cast<StageCharacter *>(this)->m_allows.fire) || isAttackMotion(motion))
        return CharacterAction_Attack;
    return CharacterAction_None;
}

// FS2 0x002152a0
bool StageObject::isMode2()
{
    return m_gear != nullptr && m_abilities.driving == 2;
}

// FS2 0x002152d0
bool StageCharacter::isMode1()
{
    return m_gear != nullptr && m_abilities.driving == 1;
}

// FS2 0x00215310
bool StageCharacter::isDriver()
{
    return m_abilities.driving == 1;
}

// FS2 0x00215620
// callcheck: getSaveData (getStageSelect, the same word), getStageSelect (stageObjRecord, inline at each branch)

bool StageObject::isScenarioLead()
{
    const int stage = getStageSelect()->stage;
    const int kind = stageObjRecord(m_itemDef)->kind;
    switch (stage) {
    case kLeadStageA:
        return kind == kKindLeadShared && m_name[kNameLeadDigit] == '2';
    case kLeadStageB:
        return kind == kKindLeadShared && m_name[kNameLeadDigit] == '1';
    case kLeadStageC:
    case kLeadStageD:
        return kind == 1;
    case kLeadStageE:
    case kLeadStageF:
        return kind == 0;
    }
    return false;
}

// FS2 0x00215e70
// callcheck: strstr, strstr

bool StageObject::isYriOrKne()
{
    const char *name = m_model->pose->name;
    return strstr(name, kModelYri) != nullptr || strstr(name, kModelKne) != nullptr;
}

// FS2 0x00216140
// callcheck: getStageSelect (stageObjRecord, inline)

u_int StageObject::getCharacterClass()
{
    switch (stageObjRecord(m_itemDef)->kind) {
    case 0:
        return 1;
    case 1: case 0xf:
        return 2;
    case 2: case 3: case 8: case 10: case 0xd:
        return 4;
    case 4: case 5: case 6: case 7: case 0xb: case 0xc:
        return 8;
    }
    return 0;
}

// ---------------------------------------------------------- the frame's noises

// FS2 0x00243150

SceneNoiseList::SceneNoiseList()
{
    m_front = 0;
    m_lists[0].count = 0;
    m_lists[1].count = 0;
    for (u_int i = 0; i < kSceneSoundWatch; i++) {
        m_watches[i].object = nullptr;
        m_watches[i].source = nullptr;
        m_watches[i].delay  = kSceneWatchDelay;
    }
}

// FS2 0x00243100
// callcheck: operator_delete (the compiler's)
SceneNoiseList::~SceneNoiseList()
{
}

// FS2 0x00242e80
// callcheck: findSource

void SceneNoiseList::pollSoundSources()
{
    for (u_int i = 0; i < kSceneSoundWatch; i++) {
        SceneSoundWatch *watch = &m_watches[i];
        if (watch->object == nullptr)
            continue;
        const int delay = watch->delay;
        watch->delay = delay - 1;
        if (delay >= 1)
            continue;

        watch->source = gStageScene->sound->findSource(watch->source);
        if (watch->source == nullptr) {
            watch->object = nullptr;
            watch->delay  = kSceneWatchDelay;
        } else {
            addNoise(watch->source->m_entry->loudness, watch->object, SceneNoise_Sound);
            watch->delay = 0;
        }
    }
    m_front = (m_front + 1) & 1;
    back().count = 0;
}

// FS2 0x00242fc0

void SceneNoiseList::addSourceNoise(StageObject *object, StageSoundSource *source)
{
    if (source == nullptr || !(source->m_entry->loudness > 0.0f))
        return;
    addNoise(source->m_entry->loudness, object, SceneNoise_Sound);
    if (!source->m_entry->resumable)
        return;
    for (u_int i = 0; i < kSceneSoundWatch; i++) {
        if (m_watches[i].object == nullptr) {
            m_watches[i].object = object;
            m_watches[i].source = source;
            m_watches[i].delay  = kSceneWatchDelay;
            return;
        }
    }
}

// FS2 0x002430a0

void SceneNoiseList::addNoise(float loudness, StageObject *object, int kind)
{
    List &list = back();
    if (list.count < kSceneNoises) {
        SceneNoise *noise = &list.noises[list.count++];
        noise->source   = object;
        noise->loudness = loudness;
        noise->kind     = kind;
    }
}

// FS2 0x002431e0
// callcheck: getKind, getKind, find

const float *SceneNoise::place() const
{
    if (source->getKind() == StageObject_Character && (u_int)(kind - 1) < 3)
        return headMatrix(source)[3];
    return source->m_xform.position;
}

// FS2 0x002432a0
// callcheck: getKind, getKind, isAlly

bool SceneNoise::isFromAlly(StageObject *hearer) const
{
    return source->getKind() == StageObject_Character
        && static_cast<StageCharacter *>(hearer)->isAlly(source);
}

// ------------------------------------------------------------- the volumes

// A map object list's entry: name, group, script, then the matrix. The same
// for a marker (0x17ef10) and a volume (0x17ecb0).
// FS2 0x0017ecb0
// callcheck: setName, add7, findEntry, setEntry

void StageVolume::addToScene(const char *name)
{
    setName(name);
    gStageScene->objects->add7(this);
    setEntry(gStageScene->scripts[0]->findEntry(m_name));
    rebuildMatrix();
    setDirty(false);
}

// FS2 0x0017ef10
// callcheck: setName, add6, findEntry, setEntry

void StageMarker::addToScene(const char *name)
{
    setName(name);
    gStageScene->objects->add6(this);
    setEntry(gStageScene->scripts[0]->findEntry(m_name));
    rebuildMatrix();
    setDirty(false);
}

// FS2 0x0017e950
// callcheck: getWorldMatrix

bool StageVolume::containsObject(StageObject *object)
{
    return contains(object->getWorldMatrix()[3]);
}

// FS2 0x0017ea90
// callcheck: StageObject (the compiler's)
BoxVolume::BoxVolume()
{
}

// FS2 0x0017e860
// callcheck: operator_delete, ~StageObject (the compiler's)
BoxVolume::~BoxVolume()
{
}

// x and y (the map's width and depth) centred, height from the floor up.
// FS2 0x0017ea50

void BoxVolume::setSize(float width, float height, float depth)
{
    m_min[0] = -width / 2.0f;
    m_min[1] = -depth / 2.0f;
    m_min[2] = 0.0f;
    m_max[0] = width / 2.0f;
    m_max[1] = depth / 2.0f;
    m_max[2] = height;
}

// FS2 0x0017e9a0
// callcheck: getMatrix, invertRigidMatrix, applyMatrix

bool BoxVolume::contains(const float *point)
{
    sceVu0FMATRIX inverse;
    invertRigidMatrix(inverse, getMatrix());
    sceVu0FVECTOR local = { point[0], point[1], point[2], 1.0f };
    applyMatrix(local, inverse, local);
    for (int i = 0; i < 3; i++) {
        if (local[i] < m_min[i] || m_max[i] < local[i])
            return false;
    }
    return true;
}

// FS2 0x0017ec30
// callcheck: StageObject (the compiler's)
SphereVolume::SphereVolume()
{
}

// FS2 0x0017efa0
// callcheck: operator_delete, ~StageObject (the compiler's)
SphereVolume::~SphereVolume()
{
}

// FS2 0x0017ec20
void SphereVolume::setRadius(float radius)
{
    m_radius = radius;
}

// FS2 0x0017eba0
// callcheck: getMatrix

bool SphereVolume::contains(const float *point)
{
    const sceVu0FVECTOR *matrix = getMatrix();
    const float x = point[0] - matrix[3][0];
    const float y = point[1] - matrix[3][1];
    const float z = point[2] - matrix[3][2];
    return sqrtf(z * z + x * x + y * y) <= m_radius;
}
