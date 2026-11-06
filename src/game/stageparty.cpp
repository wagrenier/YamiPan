/* ==========================================================================
 *  game/stageparty.cpp
 *
 *  The sight-jack, game/stageparty.h. The file name and every name in it are
 *  the port's. The draw is the frame's gate and its host passes are still to
 *  come: see SightJack::draw.
 * ======================================================================== */

#include "game/stageparty.h"

#include <math.h>
#include <string.h>

#include "common/yamipan_stub.h"
#include "game/characterbrain.h"
#include "game/characterinput.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/packtables.h"
#include "game/playermotion.h"
#include "game/ripplesheet.h"
#include "game/rumblelist.h"
#include "game/scenerender.h"
#include "game/stageobjlist.h"
#include "game/stageresult.h"
#include "game/stagescene.h"
#include "game/stagesound.h"
#include "game/sysdata.h"
#include "game/vu0math.h"
#include "sdk/libgraph.h"
#include "yamipan/rendering/yamipan_drawui.h"
#include "game/gamepacket.h"
#include "game/gsutil.h"
#include "game/mathutil.h"

namespace {

// The camera, StageCamera's view-screen distance, size and far.
constexpr float kCameraScrZ   = 450.0f;
constexpr float kCameraWidth  = 512.0f;
constexpr float kCameraHeight = 512.0f;
constexpr float kCameraFar    = 1600.0f;

// The fill's corner and size, in GS units. Its colour.
constexpr u_int kFillRgba = 0x00745759;
constexpr float kFillLevelRange = 108.0f;   // JackFill::draw: alpha at level 1,
constexpr float kFillLevelBase  = 12.0f;    // over what shows at level 0
constexpr int   kFillX = 0x700, kFillY = 0x700, kFillW = 0x200, kFillH = 0x200;

constexpr float kStickRest      = 40.0f;   // a stick's bytes from centre
constexpr int   kStickCentre    = 0x80;
constexpr float kNoiseOn        = 0.5f;    // m_fill's level
constexpr float kNoiseOff       = 0.3f;
constexpr float kWarnSeconds    = 0.3f;
constexpr int   kBindFlashFrames = 0x10;
constexpr int   kFlashFrames    = 5;
constexpr int   kHoldFrames     = 10;      // a tap ends before this
constexpr float kFlyerEyeLift   = 60.0f;   // above a kind 0xf's place
constexpr float kFlyerTurnRate  = 0.3f;
constexpr float kFlyerTurnMax   = 0.05235988f; // 3 degrees
constexpr float kPi             = 3.1415927f;

// Motions a viewed character cannot be seen through while m_bfc is set.
constexpr int kMotionHiddenA = 0x42, kMotionHiddenB = 0x94, kMotionHiddenC = 0x95;
constexpr int kStageWithB4 = 0x1b;

// Sounds.
const char kSoundNoise[]  = "JAK_NOISE";    // data 0x306808
const char kSoundTuning[] = "JAK_TUNING";   // data 0x306818
const char kSoundIn[]     = "JAK_IN";       // data 0x306828
const char kSoundMode[]   = "JAK_MODE";     // data 0x306830
const char kSoundPetit[]  = "PETITJACK";    // data 0x306790
const char kSoundVision[] = "KAKOSI";       // data 0x3067a8

// Saved over a stage's restart: the bindings' character ids.
int sSavedBinds[kJackBinds];                // bss 0x598b20

// The film's band and levels, by stage. layout: host
struct JackTuning
{
    u_int band;
    float levelLow;
    float levelHigh;
    float filmNear;
    float filmFar;
};
const JackTuning kTuningDark  = { 0x100, 0.3f, 1.1f,  400.0f, 5500.0f };    // data 0x305b40
const JackTuning kTuningLight = { 0x100, 0.3f, 0.95f, 400.0f, 5500.0f };    // data 0x305b54

// The ROM's test is one long chain of compares.
bool stageIsLight(int stage)
{
    if (stage == 3 || stage == 0x15 || stage == 0x16)
        return false;
    if (stage >= 0x11 && stage <= 0x14)
        return false;
    return stage == 0x50 || (stage >= 0x17 && stage <= 0x1d) || (stage >= 4 && stage <= 0x10)
        || stage == 2 || stage == 1;
}

// To -pi..pi, from within one turn of it.
float wrapPi(float angle)
{
    constexpr float kTurn = 6.2831855f;
    if (angle < -3.1415927f)
        angle += kTurn;
    if (angle > 3.1415927f)
        angle -= kTurn;
    return angle;
}

const sceVu0FVECTOR kNowhere = { -1e7f, -1e7f, -1e7f, 1.0f };               // data 0x306780

} // namespace

// ──────────────────────────────────────────────────────────────────────
// The camera and the fill.

constexpr int    kStageBoss = 0x1b;
constexpr u_int  kRippleColumns = 15;       // the boss's water, ripplesheet.h
constexpr u_int  kRippleRows = 15;
constexpr u_int  kRippleWaves = 1;
constexpr float  kRippleSpeedMin = 0.3f;
constexpr float  kRippleSpeedMax = 3.0f;
constexpr float  kRipplePeak = 8.0f;
constexpr float  kRippleCameraAbove = 195.0f;
constexpr u_int  kRippleTint = 0x1e004050;  // the frame is pushed away from this brown

// callcheck: StageCamera (createCamera's inline ctor)
JackCamera::JackCamera() : StageCamera(kCameraScrZ, kCameraWidth, kCameraHeight, kCameraFar)
{
    m_linked = nullptr;
}

// FS2 0x00178030
// callcheck: onViewTop, getStageSelect

void JackCamera::onViewTop()
{
    gStageScene->render->m_snap = 1;    // StageCamera::onViewTop 0x177d00
    if (m_linked != nullptr && stageObjRecord(m_linked->m_itemDef)->kind == CharacterKind_F)
        memcpy(m_xform.rotation, m_linked->m_xform.rotation, sizeof(m_xform.rotation));
}

// FS2 0x002082e0
// callcheck: ScreenFill

JackFill::JackFill(u_int rgba, int x, int y, int w, int h)
    : ScreenFill(rgba, x, y, w, h), m_rgba(rgba)
{
}

// FS2 0x002081b0
// callcheck: FUN_001518a0, FUN_001518a0

void JackFill::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    const u_int alpha = (u_int)(m_level * kFillLevelRange + kFillLevelBase) << 24;
    drawGrain(packet, texList, alpha | (m_rgba & 0xffffff));
    drawGrain(packet, texList, alpha);
}

// ──────────────────────────────────────────────────────────────────────
// The record.

// FS2 0x00203d70
// callcheck: operator_new, FUN_002082e0, getStageSelect, getStageSelect, operator_new, FUN_002d31d0, FUN_002d2c20, FUN_002d2f50

SightJack::SightJack()
{
    m_fill = new JackFill(kFillRgba, kFillX, kFillY, kFillW, kFillH);
    const int stage = getStageSelect()->stage;
    const JackTuning &tuning = stageIsLight(stage) ? kTuningLight : kTuningDark;
    m_filmNear = tuning.filmNear;
    m_filmFar = tuning.filmFar;
    m_film.band = tuning.band;
    m_film.levelLow = tuning.levelLow;
    m_film.levelHigh = tuning.levelHigh;
    if (stage == kStageBoss) {
        m_ripples = new RippleSheet(kRippleColumns, kRippleRows, kRippleWaves);
        RippleSpec &spec = m_ripples->spec();
        spec.speedMin = kRippleSpeedMin;
        spec.speedMax = kRippleSpeedMax;
        spec.setPeak(kRipplePeak);
        m_ripples->restart();
    }
}

// FS2 0x00203be0
// callcheck: operator_delete, FUN_00206f00, FUN_00206f00, operator_delete, operator_delete, operator_delete

SightJack::~SightJack()
{
    if (m_fill != nullptr)
        m_fill->destroy();
    delete m_fleckTexture;
    delete m_flecks;
    delete m_ripples;
}

// FS2 0x00203ae0
// callcheck: setActive, setShown, setActive, setShown, setBrainState, restoreFlags

void SightJack::reset()
{
    // PC: the ROM clears m_viewer first and then ends a vision through it, a
    // read near address 0. Ending it first gives what that code means.
    endVision();
    m_warnPushed = 0;
    m_mode = JackMode_None;
    for (int i = 0; i < kJackBinds; i++)
        m_binds[i] = nullptr;
    m_viewer = nullptr;
    m_kept = nullptr;
    m_tuned = nullptr;
    m_holdState = JackHold_None;
    m_bindFlash = 0;
    m_flashFrames = 0;
    m_visionFade = 0;
    m_visionFrames = 0;
    m_markerFrames = 0;
    m_viewChanged = 0;
    m_vision = nullptr;
    m_visionShowCount = 0;
    m_visionHideCount = 0;
}

// FS2 0x001fea50

StageObject *SightJack::viewed() const
{
    switch (m_mode) {
    case JackMode_KeptTurn:
    case JackMode_Kept:
        return m_kept;
    case JackMode_Tuned:
        return m_tuned;
    default:
        return nullptr;
    }
}

bool SightJack::stickAtRest() const
{
    if (m_viewer == nullptr || m_viewer->m_input == nullptr)
        return true;
    const CharacterPad *pad = m_viewer->m_input->pad();
    const float x = (float)(pad->m_analog[2] - kStickCentre);
    const float y = (float)(pad->m_analog[3] - kStickCentre);
    return sqrtf(x * x + y * y) < kStickRest;
}

// FS2 0x00200150
// callcheck: copyMatrix, getStageSelect, approachAnglesXZ, identityMatrix, rotateByAngles, rotateByAngles, rotateMatrixZ, find, getEyeMatrix, copyMatrix

void SightJack::aimAt(StageObject *object)
{
    if (object == nullptr) {
        m_camera->m_linked = nullptr;
        m_camera->onLinkChanged();
        return;
    }
    if (object->getKind() != StageObject_Character) {
        copyMatrix(m_camera->m_xform.world, object->getMatrix());
        return;
    }
    if (stageObjRecord(object->m_itemDef)->kind == CharacterKind_F) {
        sceVu0FVECTOR *world = m_camera->m_xform.world;
        approachAnglesXZ(kFlyerTurnRate, kFlyerTurnMax, m_camera->m_xform.rotation,
                         m_camera->m_xform.rotation, object->m_xform.rotation);
        identityMatrix(world);
        rotateByAngles(world, world, object->m_motion->m_lampAngles);
        rotateByAngles(world, world, m_camera->m_xform.rotation);
        rotateMatrixZ(kPi, world, world);
        world[3][0] = object->m_xform.position[0];
        world[3][1] = object->m_xform.position[1];
        world[3][2] = object->m_xform.position[2] + kFlyerEyeLift;
    } else if (object->m_model->boneMap.find("bn_head")->value != nullptr) {
        object->m_motion->getEyeMatrix(m_camera->m_xform.world);
    } else {
        copyMatrix(m_camera->m_xform.world, object->getMatrix());
    }
    m_camera->m_linked = object;
    m_camera->onLinkChanged();
}

// FS2 0x00200400
// callcheck: bitMask, bitMask, bitMask

bool SightJack::anyShoulderHeld() const
{
    const CharacterPad *pad = m_viewer->m_input->pad();
    return pad->actionHeld(PadAction_Strafe) || pad->actionHeld(PadAction_Ready) || pad->actionHeld(PadAction_Zoom);
}

// FS2 0x002004c0
// callcheck: FUN_002067d0

bool SightJack::keep(StageObject *object)
{
    if (object == nullptr)
        return false;
    const int bind = m_viewer->m_input->pad()->pressedBind();
    if (bind == kJackNoBind)
        return false;
    m_kept = object;
    if (m_fill != nullptr)
        m_fill->m_level = 0.0f;
    for (int i = 0; i < kJackBinds; i++) {
        if (m_binds[i] == object)
            m_binds[i] = nullptr;
    }
    m_binds[bind] = object;
    m_bindShown = bind;
    m_bindFlash = kBindFlashFrames;
    m_flashFrames = kFlashFrames;
    return true;
}

// FS2 0x00200d10
// callcheck: vecDistance, isPlayer, FUN_00215480, getStageSelect (stageObjRecord)

bool SightJack::canView(StageObject *object) const
{
    if (object == nullptr)
        return false;
    if (m_range < vecDistance(object->m_xform.position, m_viewer->m_xform.position))
        return false;
    if (object->getKind() != StageObject_Character)
        return true;
    if (m_viewer->m_id == object->m_id)
        return false;
    StageCharacter *character = static_cast<StageCharacter *>(object);
    if (character->getBrainPace() == 0 && object != m_vision)
        return false;
    const int kind = stageObjRecord(object->m_itemDef)->kind;
    if (kind == CharacterKind_Srt || kind == CharacterKind_Yrt)
        return false;
    const PlayerMotion *motion = object->m_motion;
    if (motion->m_bfc != 0
        && (motion->motionId == kMotionHiddenA || motion->motionId == kMotionHiddenB
            || motion->motionId == kMotionHiddenC))
        return false;
    if (object == m_vision) {
        if (!m_viewer->isPlayer() || !static_cast<StageCharacter *>(m_viewer)->canSeeVisions())
            return false;
    }
    return true;
}

// FS2 0x00200fe0
// callcheck: FUN_002067d0, FUN_00200d10

bool SightJack::recall()
{
    const int bind = m_viewer->m_input->pad()->pressedBind();
    if (bind == kJackNoBind || m_binds[bind] == nullptr)
        return false;
    m_kept = canView(m_binds[bind]) ? m_binds[bind] : nullptr;
    m_bindShown = bind;
    m_bindFlash = kBindFlashFrames;
    m_flashFrames = kFlashFrames;
    m_viewChanged = 1;
    return true;
}

// callcheck: FUN_00206890

std::vector<SightJack::Bearing> SightJack::gatherBearings() const
{
    std::vector<Bearing> bearings;
    StageObjArray &list = gStageScene->objects->m_lists[StageObjGroup_Characters];
    for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *object = *slot;
        if (!canView(object))
            continue;
        const float dx = object->m_xform.position[0] - m_viewer->m_xform.position[0];
        const float dy = object->m_xform.position[1] - m_viewer->m_xform.position[1];
        bearings.push_back({ atan2f(dy, dx), sqrtf(dx * dx + dy * dy), object });
    }
    return bearings;
}

// FS2 0x001fff00
// callcheck: atan2f, atan2f

SightJack::Bearing SightJack::pickBearing(const std::vector<Bearing> &bearings) const
{
    const CharacterPad *pad = m_viewer->m_input->pad();
    const float stick = atan2f((float)-(pad->m_analog[3] - kStickCentre), (float)(pad->m_analog[2] - kStickCentre));
    const sceVu0FVECTOR *matrix = m_viewer->getMatrix();
    const float facing = atan2f(-matrix[0][1], -matrix[0][0]);
    const float line = wrapPi(stick + facing);

    Bearing best = { 0.0f, 0.0f, nullptr };
    float nearest = kPi;
    for (const Bearing &bearing : bearings) {
        const float off = fabsf(wrapPi(bearing.angle - line));
        if (off <= m_angle && off <= nearest) {
            best = { off, bearing.distance, bearing.object };
            nearest = off;
        }
    }
    return best;
}

// FS2 0x00200a50
// callcheck: FUN_00206f70, FUN_00200d10, FUN_00206890, isEnemyOf, getLifeState, FUN_00206f80, atan2f, canView (gatherBearings)

StageObject *SightJack::keepNearestFoe(int buttons)
{
    if (!(buttons & JackButton_NearestFoe))
        return nullptr;
    const Bearing *best = nullptr;
    const std::vector<Bearing> bearings = gatherBearings();
    for (const Bearing &bearing : bearings) {
        if (bearing.object->getKind() != StageObject_Character)
            continue;
        StageCharacter *character = static_cast<StageCharacter *>(bearing.object);
        if (character->isEnemyOf(m_viewer) && character->getLifeState() != LifeState_Corpse            && (best == nullptr || bearing.distance < best->distance))
            best = &bearing;
    }
    if (best == nullptr)
        return nullptr;
    m_kept = best->object;
    return m_kept;
}

// FS2 0x00200580
// callcheck: FUN_00206d60, FUN_00206f70, FUN_00200d10, FUN_00206890, vecDistance, FUN_00200d10, isEnemyOf, FUN_00206f80, atan2f (gatherBearings)

StageObject *SightJack::stepAlertedFoe(int buttons)
{
    constexpr int kStepFarther = 1, kStepNearer = 2;
    int step = (buttons & JackButton_Farther) ? kStepFarther : 0;
    if (buttons & JackButton_Nearer)
        step = kStepNearer;
    if (step == 0)
        return nullptr;
    const std::vector<Bearing> bearings = gatherBearings();
    const float keptDistance = m_kept == nullptr
        ? 0.0f : vecDistance(m_kept->m_xform.position, m_viewer->m_xform.position);
    const Bearing *nearest = nullptr, *farthest = nullptr, *justNearer = nullptr, *justFarther = nullptr;
    for (const Bearing &bearing : bearings) {
        if (bearing.object->getKind() != StageObject_Character || !canView(bearing.object))
            continue;
        StageCharacter *character = static_cast<StageCharacter *>(bearing.object);
        if (!character->isEnemyOf(m_viewer))
            continue;
        const int state = character->m_brain->getState();
        if (state < BrainState_Slot4a || state > BrainState_Foe)
            continue;
        if (nearest == nullptr || bearing.distance < nearest->distance)
            nearest = &bearing;
        if (farthest == nullptr || farthest->distance < bearing.distance)
            farthest = &bearing;
        if (m_kept != nullptr) {
            if (bearing.distance < keptDistance && (justNearer == nullptr || justNearer->distance < bearing.distance))
                justNearer = &bearing;
            if (keptDistance < bearing.distance && (justFarther == nullptr || bearing.distance < justFarther->distance))
                justFarther = &bearing;
        }
    }
    if (nearest == nullptr)
        return nullptr;
    if (step == kStepFarther)
        m_kept = (justFarther != nullptr ? justFarther : nearest)->object;
    else
        m_kept = (justNearer != nullptr ? justNearer : farthest)->object;
    return m_kept;
}

// FS2 0x00201080
// callcheck: FUN_00206f70, FUN_00200d10, FUN_00206890, FUN_001fff00, FUN_00206f80, atan2f, canView (gatherBearings)

bool SightJack::tune()
{
    if (stickAtRest())
        return false;
    StageObject *before = m_tuned;
    const std::vector<Bearing> bearings = gatherBearings();
    const Bearing best = pickBearing(bearings);
    m_tuned = best.object;
    if (m_tuned == nullptr) {
        if (m_fill != nullptr)
            m_fill->m_level = 1.0f;
        return true;
    }
    if (m_tuned != before) {
        m_viewChanged = 1;
        if (before != nullptr)
            m_flashFrames = kFlashFrames;
        for (int i = 0; i < kJackBinds; i++) {
            if (m_binds[i] == m_tuned) {
                m_bindShown = i;
                m_bindFlash = kBindFlashFrames;
                m_flashFrames = kFlashFrames;
                break;
            }
        }
    }
    if (m_fill != nullptr)
        m_fill->m_level = best.angle / m_angle;
    return true;
}

// FS2 0x00202c70
// callcheck: FUN_00200400, FUN_002004c0, FUN_00200400, FUN_00200400, FUN_00200a50, FUN_00200580, FUN_00200400, bitMask, bitMask, bitMask

bool SightJack::pollButtons()
{
    if (anyShoulderHeld() && keep(viewed())) {
        m_mode = stickAtRest() ? JackMode_Kept : JackMode_KeptTurn;
        m_holdState = JackHold_Held;
    }
    switch (m_holdState) {
    case JackHold_Held:
        if (!anyShoulderHeld())
            m_holdState = JackHold_None;
        break;
    case JackHold_Counting:
        if (m_holdFrames++ < kHoldFrames) {
            if (!anyShoulderHeld()) {
                if (keepNearestFoe(m_heldButtons) != nullptr || stepAlertedFoe(m_heldButtons) != nullptr) {
                    m_viewChanged = 1;
                    m_mode = JackMode_Kept;
                }
                m_holdState = JackHold_None;
            }
        } else {
            m_holdState = JackHold_Held;
        }
        break;
    case JackHold_None:
        m_holdFrames = 0;
        if (anyShoulderHeld())
            m_holdState = JackHold_Counting;
        break;
    default:
        break;
    }
    const CharacterPad *pad = m_viewer->m_input->pad();
    m_heldButtons = (pad->actionHeld(PadAction_Zoom) ? JackButton_Nearer : 0)
                  | (pad->actionHeld(PadAction_Strafe) ? JackButton_NearestFoe : 0)
                  | (pad->actionHeld(PadAction_Ready) ? JackButton_Farther : 0);
    return m_holdState != JackHold_None;
}

// FS2 0x00202f50
// callcheck: FUN_00200d10, FUN_00200d10, FUN_00201080, FUN_00200fe0, FUN_00201080, FUN_002004c0, FUN_00201080, FUN_00200fe0

void SightJack::stepMode()
{
    switch (m_mode) {
    case JackMode_KeptLost:
        if (m_fill != nullptr)
            m_fill->m_level = 1.0f;
        if (stickAtRest()) {
            m_mode = JackMode_None;
            m_kept = nullptr;
            m_holdState = JackHold_None;
        }
        break;
    case JackMode_KeptTurn:
        if (m_fill != nullptr)
            m_fill->m_level = 0.0f;
        if (!canView(m_kept)) {
            m_mode = JackMode_KeptLost;
            m_holdState = JackHold_None;
        }
        if (stickAtRest()) {
            m_mode = JackMode_Kept;
            m_holdState = JackHold_None;
        }
        break;
    case JackMode_Kept:
        if (m_fill != nullptr)
            m_fill->m_level = 0.0f;
        if (!canView(m_kept)) {
            m_mode = JackMode_None;
            m_kept = nullptr;
            m_holdState = JackHold_None;
        }
        if (tune()) {
            m_mode = JackMode_Tuned;
            m_kept = nullptr;
            m_holdState = JackHold_None;
        }
        if (recall()) {
            m_mode = JackMode_Kept;
            m_holdState = JackHold_None;
        }
        break;
    case JackMode_Tuned:
        if (!tune()) {
            m_mode = JackMode_None;
            m_kept = nullptr;
            m_holdState = JackHold_None;
        }
        if (keep(m_tuned)) {
            m_mode = JackMode_KeptTurn;
            m_holdState = JackHold_None;
        }
        break;
    default:
        if (m_mode != JackMode_None)
            m_mode = JackMode_None;
        m_tuned = nullptr;
        if (tune()) {
            m_mode = JackMode_Tuned;
            m_kept = nullptr;
            m_holdState = JackHold_None;
        }
        if (recall()) {
            m_mode = JackMode_Kept;
            m_holdState = JackHold_None;
        }
        break;
    }
}

// FS2 0x001fede0
// callcheck: saveFlags, stageHideObjects, setActive, setShown, setActive, setShown, setActive, setShown, setActive, setShown, setShown, setChildrenActive, setBrainState

void SightJack::beginVision()
{
    if (m_vision == nullptr || m_visionOn)
        return;
    gStageScene->arrays->saveFlags();
    stageHideObjects();
    for (int i = 0; i < m_visionShowCount; i++) {
        m_visionShow[i]->m_tree->setActive(true);
        m_visionShow[i]->m_tree->setShown(true);
    }
    for (int i = 0; i < m_visionHideCount; i++) {
        m_visionHide[i]->m_tree->setActive(false);
        m_visionHide[i]->m_tree->setShown(false);
    }
    m_viewer->m_tree->setActive(true);
    m_viewer->m_tree->setShown(true);
    m_vision->m_tree->setActive(true);
    m_vision->m_tree->setShown(true);
    m_viewer->m_tree->setShown(false);
    m_viewer->m_tree->setChildrenActive(false);
    static_cast<StageCharacter *>(m_vision)->setBrainState(1, 0);
    m_visionOn = 1;
}

// callcheck: setActive, setShown, setActive, setShown, setBrainState, restoreFlags

void SightJack::endVision()
{
    if (m_vision == nullptr || !m_visionOn)
        return;
    m_viewer->m_tree->setActive(false);
    m_viewer->m_tree->setShown(false);
    m_vision->m_tree->setActive(false);
    m_vision->m_tree->setShown(false);
    static_cast<StageCharacter *>(m_vision)->setBrainState(0, 0);
    gStageScene->arrays->restoreFlags();
    m_visionOn = 0;
}

// FS2 0x002039b0
// callcheck: FUN_00203ae0, FUN_00200150, FUN_00200d10, FUN_00203240, pushView, playNamed, getSystemData

void SightJack::start(StageObject *viewer)
{
    m_running = 1;
    if (m_viewer != nullptr && m_viewer != viewer)
        reset();
    m_viewer = viewer;
    aimAt(viewer);
    m_gameOverOn = nullptr;
    if (m_kept != nullptr && !canView(m_kept))
        m_kept = nullptr;
    if ((m_mode == JackMode_KeptTurn || m_mode == JackMode_Kept) && m_kept != nullptr) {
        m_bindFlash = kBindFlashFrames;
        m_flashFrames = kFlashFrames;
    }
    update();
    if (m_warner == nullptr)
        gStageScene->pushView(m_camera, 2, 1);
    gStageScene->sound->playNamed(kSoundIn, 1.0f);
    m_fill->m_level = 0.0f;
    m_noisePlaying = 0;
    m_startTime = getSystemData()->save.playTime;
}

// FS2 0x00203840
// callcheck: getSystemData, getStageResult, setActive, setShown, setActive, setShown, setBrainState, restoreFlags, removeView, stopNamed, stopNamed, stopNamed, stopNamed

void SightJack::stop()
{
    if (m_gameOverOn == nullptr && m_startTime < getSystemData()->save.playTime)
        getStageResult()->counter[3]++;
    m_running = 0;
    endVision();
    if (!m_warnPushed) {
        gStageScene->removeView(m_camera);
        m_camera->m_linked = nullptr;
        m_camera->onLinkChanged();
    }
    gStageScene->sound->stopNamed(kSoundIn);
    gStageScene->sound->stopNamed(kSoundMode);
    gStageScene->sound->stopNamed(kSoundTuning);
    gStageScene->sound->stopNamed(kSoundNoise);
    gStageScene->render->m_snap = 1;
}

// FS2 0x00203240
// callcheck: FUN_00200150, setActive, setShown, setActive, setShown, setBrainState, restoreFlags, getSeconds, removeView, FUN_00200150, FUN_00200150, FUN_00202c70, FUN_00202f50, FUN_00200150, FUN_001fede0, playNamed, stopNamed, stopNamed, playNamed, FUN_002d0750, FUN_002d0750, FUN_00200150

void SightJack::update()
{
    m_viewer = gStageScene->player;
    if (m_gameOverOn != nullptr) {
        aimAt(m_gameOverOn);
        endVision();
        return;
    }
    if (m_warner != nullptr) {
        if (m_warner->m_clock->getSeconds() >= m_warnEnd) {
            if (!m_running && m_warnPushed) {
                gStageScene->removeView(m_camera);
                m_camera->m_linked = nullptr;
                m_camera->onLinkChanged();
            }
            m_warner = nullptr;
            m_warnPushed = 0;
            return;
        }
        aimAt(m_warnPushed ? m_warner : viewed());
        endVision();
        return;
    }
    if (!m_running)
        return;

    m_viewChanged = 0;
    if (m_viewer->m_input == nullptr || m_viewer->m_input->pad() == nullptr)
        return;
    if (m_viewer->m_altAction == 0) {
        if (!pollButtons())
            stepMode();
    } else if (m_fill != nullptr) {
        m_fill->m_level = 0.0f;
    }

    aimAt(viewed());
    StageObject *target = viewed();
    if (target == nullptr || target != m_vision)
        endVision();
    else
        beginVision();

    if (m_mode == JackMode_None && m_fill != nullptr)
        m_fill->m_level = 1.0f;

    StageSound *sound = gStageScene->sound;
    if (!m_noisePlaying) {
        if (m_fill->m_level > kNoiseOn) {
            sound->playNamed(kSoundNoise, 1.0f);
            m_noisePlaying = 1;
        }
    } else if (m_fill->m_level < kNoiseOff) {
        sound->stopNamed(kSoundNoise);
        sound->stopNamed(kSoundTuning);
        if (m_flashFrames < 1)
            sound->playNamed(kSoundTuning, 1.0f);
        m_noisePlaying = 0;
    }

    if (m_viewChanged) {
        if (viewed() == nullptr) {
            gStageScene->daylight->setPlace(kNowhere);
        } else {
            gStageScene->daylight->setPlace(m_camera->getWorldMatrix()[3]);
            m_camera->onViewTop();
            aimAt(viewed());
        }
    }
}

// The frame's passes share these.

constexpr u_long kJackAlpha = 0x44;         // ALPHA_1: over the frame
constexpr u_long kJackBlackTest = 0x30000;  // TEST_1: past any depth
constexpr int    kJackFrame = 0x200;
constexpr u_int  kJackBlack = 0x80000000;
constexpr int    kMarkerFrames = 0x15e;     // m_markerFrames each running frame
constexpr int    kVisionFadeScale = 0x20;   // m_visionFade to alpha, over 8
constexpr u_int  kVisionTintA = 0x00b4d9f8; // a vision's fade: the two tints
constexpr u_int  kVisionTintB = 0x00001c33;
constexpr u_int  kVisionWaitTintA = 0x804688c0; // while it waits to start
constexpr u_int  kVisionWaitTintB = 0x80001c33;
constexpr float  kFlashFramesScale = 5.0f;  // m_flashFrames to the fill's level
constexpr u_long kJackAdd = 0x48;           // ALPHA_1: added to the frame
constexpr u_long kMarkerTest = 0x33001;     // TEST_1: past any depth, the frame's alpha kept
constexpr float  kGsOne = 128.0f;           // RGBAQ's and FIX's 1.0
constexpr int    kFrameScaleOne = 0xff;     // gsFrameScale: past it the frame is added on
constexpr int    kFrameScaleBoost = 0x80;
// The GS window, 12.4: what the frame covers.
constexpr int    kWindowMin = 0x7000;
constexpr int    kWindowCentre = 0x8000;
constexpr int    kWindowMax = 0x9000;
constexpr u_long kMarkerLines = 0x49;       // PRIM: LINE, Gouraud, blended
constexpr u_long kMarkerGlow = 0x4d;        // PRIM: TRIFAN, Gouraud, blended
constexpr u_long kBindFlashStrip = 0x4c;    // PRIM: TRISTRIP, Gouraud, blended
constexpr u_int  kMarkerWhite = 0x00ffffff;
constexpr u_int  kMarkerRgb = 0x00ffffff;
constexpr u_int  kMarkerViewer = 0x80e03050;
constexpr u_int  kMarkerPartner = 0x8030e050;
constexpr u_int  kMarkerFoe = 0x805030e0;
constexpr float  kMarkerNear = 8.0f;        // of a projected w
constexpr float  kMarkerFar = 8388592.0f;
constexpr float  kMarkerFade = 2000.0f;     // data 0x306840: the rays are longest at 0
constexpr float  kMarkerRaySpan = 192.0f;
constexpr float  kMarkerRayMin = 48.0f;
constexpr float  kMarkerGlowDepth = 1000.0f;
constexpr float  kMarkerGlowSpan = 11.0f;
constexpr int    kBindFlashAlpha = 0xc0;    // m_bindFlash to alpha, over 16
constexpr u_int  kBindFlashGrey = 0x00808080;
constexpr u_int  kWarningWash = 0x20000080;
constexpr u_int  kAltActionWash = 0x80200000;
constexpr u_int  kTintPartner = 0x1e0a1e14;
constexpr u_int  kTintTakenOver = 0x1e004050;
constexpr u_int  kTintFoe = 0x1e505000;     // has found a foe
constexpr u_int  kTintSearching = 0x16503200; // brain states 2..4
// The stages with a view of their own (StageSelect::stage).
constexpr int    kStagePartnerPalette = 6;
constexpr int    kStageE21 = 0x15;
constexpr int    kStagePaletteA = 0x18;

// The frame goes black, depth untouched.
static void fillBlack(sceVif1Packet *packet)
{
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kJackAlpha);
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kJackBlackTest);
    gsFillRect(packet, 0, 0, kJackFrame, kJackFrame, kJackBlack);
}

static TexAreaList *scene2dTexList()
{
    return gfxGetTexArea()->getList(PacketList_Scene2d);
}

// What SightJack::drawPaletteView maps the frame's brightness through, in
// the order the ROM has them: a GS CLUT's, with entries 8..f and 0x10..0x17
// of each 0x20 swapped. Each entry's top byte is its index; nothing reads it.
// data 0x305b80: a vision of the past.
const u_int kRampSepia[256] = {
    0x00001c33, 0x01001c35, 0x02001d36, 0x03001e37, 0x04001e38, 0x05001f39, 0x0600203a, 0x0700203b,
    0x10022645, 0x11022746, 0x12022848, 0x13022849, 0x1402294a, 0x15032a4b, 0x16032a4c, 0x17032b4d,
    0x0800213c, 0x0901223e, 0x0a01223f, 0x0b012340, 0x0c012441, 0x0d012442, 0x0e012543, 0x0f022644,
    0x18032c4e, 0x19042c4f, 0x1a042d50, 0x1b042e51, 0x1c042e52, 0x1d042f53, 0x1e053054, 0x1f053156,
    0x20053157, 0x21053258, 0x22063359, 0x2306335a, 0x2406345b, 0x2507355c, 0x2607355d, 0x2707365e,
    0x300a3c67, 0x310a3d68, 0x320b3e69, 0x330b3e6a, 0x340b3f6b, 0x350c406c, 0x360c406d, 0x370c416e,
    0x2807375f, 0x29083760, 0x2a083861, 0x2b083962, 0x2c093a63, 0x2d093a64, 0x2e093b65, 0x2f0a3c66,
    0x380d426f, 0x390d4370, 0x3a0d4371, 0x3b0e4472, 0x3c0e4573, 0x3d0f4574, 0x3e0f4675, 0x3f0f4776,
    0x40104777, 0x41104878, 0x42114979, 0x43114a7a, 0x44114a7b, 0x45124b7b, 0x46124c7c, 0x47134c7d,
    0x50175386, 0x51175487, 0x52185487, 0x53185588, 0x54195689, 0x5519568a, 0x561a578b, 0x571a588c,
    0x48134d7e, 0x49144e7f, 0x4a144f80, 0x4b154f81, 0x4c155082, 0x4d155183, 0x4e165184, 0x4f165285,
    0x581b598d, 0x591b598e, 0x5a1c5a8f, 0x5b1c5b8f, 0x5c1d5b90, 0x5d1e5c91, 0x5e1e5d92, 0x5f1f5e93,
    0x601f5e94, 0x61205f95, 0x62206096, 0x63216096, 0x64216197, 0x65226298, 0x66236399, 0x6723639a,
    0x70296aa1, 0x71296ba2, 0x722a6ba3, 0x732b6ca4, 0x742b6da4, 0x752c6ea5, 0x762d6ea6, 0x772d6fa7,
    0x6824649b, 0x6924659b, 0x6a25669c, 0x6b26669d, 0x6c26679e, 0x6d27689f, 0x6e2769a0, 0x6f2869a0,
    0x782e70a8, 0x792f71a8, 0x7a2f71a9, 0x7b3072aa, 0x7c3173ab, 0x7d3174ac, 0x7e3274ac, 0x7f3375ad,
    0x803376ae, 0x813476af, 0x823577af, 0x833678b0, 0x843679b1, 0x853779b2, 0x86387ab2, 0x87397bb3,
    0x903f82ba, 0x914082bb, 0x924183bb, 0x934284bc, 0x944385bd, 0x954385bd, 0x964486be, 0x974587bf,
    0x88397cb4, 0x893a7cb5, 0x8a3b7db5, 0x8b3c7eb6, 0x8c3c7fb7, 0x8d3d7fb8, 0x8e3e80b8, 0x8f3f81b9,
    0x984688c0, 0x994788c0, 0x9a4889c1, 0x9b488ac2, 0x9c498bc2, 0x9d4a8cc3, 0x9e4b8cc4, 0x9f4c8dc4,
    0xa04d8ec5, 0xa14e8fc6, 0xa24e8fc6, 0xa34f90c7, 0xa45091c8, 0xa55192c8, 0xa65292c9, 0xa75393ca,
    0xb05b9acf, 0xb15c9bd0, 0xb25d9cd1, 0xb35e9cd1, 0xb45f9dd2, 0xb5609ed2, 0xb6619fd3, 0xb7629fd4,
    0xa85494ca, 0xa95595cb, 0xaa5695cc, 0xab5696cc, 0xac5797cd, 0xad5898ce, 0xae5998ce, 0xaf5a99cf,
    0xb863a0d4, 0xb964a1d5, 0xba65a2d6, 0xbb66a3d6, 0xbc67a3d7, 0xbd68a4d7, 0xbe69a5d8, 0xbf6aa6d8,
    0xc06ba6d9, 0xc16ca7da, 0xc26da8da, 0xc36ea9db, 0xc46faadb, 0xc570aadc, 0xc671abdc, 0xc772acdd,
    0xd07cb3e2, 0xd17db4e2, 0xd27eb5e3, 0xd37fb5e4, 0xd480b6e4, 0xd581b7e5, 0xd682b8e5, 0xd783b8e6,
    0xc873adde, 0xc974adde, 0xca75aedf, 0xcb76afdf, 0xcc77b0e0, 0xcd78b1e0, 0xce79b1e1, 0xcf7ab2e1,
    0xd884b9e6, 0xd986bae7, 0xda87bbe7, 0xdb88bce8, 0xdc89bce8, 0xdd8abde9, 0xde8bbee9, 0xdf8cbfea,
    0xe08ec0ea, 0xe18fc0eb, 0xe290c1eb, 0xe391c2eb, 0xe492c3ec, 0xe594c4ec, 0xe695c4ed, 0xe796c5ed,
    0xf0a1ccf1, 0xf1a2cdf2, 0xf2a3cef2, 0xf3a5cff3, 0xf4a6d0f3, 0xf5a7d1f4, 0xf6a8d1f4, 0xf7aad2f4,
    0xe897c6ee, 0xe998c7ee, 0xea9ac8ef, 0xeb9bc8ef, 0xec9cc9f0, 0xed9dcaf0, 0xee9ecbf1, 0xefa0ccf1,
    0xf8abd3f5, 0xf9acd4f5, 0xfaaed5f6, 0xfbafd5f6, 0xfcb0d6f6, 0xfdb1d7f7, 0xfeb3d8f7, 0xffb4d9f8,
};
// data 0x305f80: the plain brightness, black to white.
const u_int kRampGrey[256] = {
    0x00000000, 0x01000000, 0x02010101, 0x03020202, 0x04030303, 0x05040404, 0x06050505, 0x07060606,
    0x100f0f0f, 0x11101010, 0x12111111, 0x13121212, 0x14131313, 0x15141414, 0x16151515, 0x17161616,
    0x08070707, 0x09080808, 0x0a090909, 0x0b0a0a0a, 0x0c0b0b0b, 0x0d0c0c0c, 0x0e0d0d0d, 0x0f0e0e0e,
    0x18171717, 0x19181818, 0x1a191919, 0x1b1a1a1a, 0x1c1b1b1b, 0x1d1c1c1c, 0x1e1d1d1d, 0x1f1e1e1e,
    0x201f1f1f, 0x21202020, 0x22212121, 0x23222222, 0x24232323, 0x25242424, 0x26252525, 0x27262626,
    0x302f2f2f, 0x31303030, 0x32313131, 0x33323232, 0x34333333, 0x35343434, 0x36353535, 0x37363636,
    0x28272727, 0x29282828, 0x2a292929, 0x2b2a2a2a, 0x2c2b2b2b, 0x2d2c2c2c, 0x2e2d2d2d, 0x2f2e2e2e,
    0x38373737, 0x39383838, 0x3a393939, 0x3b3a3a3a, 0x3c3b3b3b, 0x3d3c3c3c, 0x3e3d3d3d, 0x3f3e3e3e,
    0x403f3f3f, 0x41404040, 0x42414141, 0x43424242, 0x44434343, 0x45444444, 0x46454545, 0x47464646,
    0x504f4f4f, 0x51505050, 0x52515151, 0x53525252, 0x54535353, 0x55545454, 0x56555555, 0x57565656,
    0x48474747, 0x49484848, 0x4a494949, 0x4b4a4a4a, 0x4c4b4b4b, 0x4d4c4c4c, 0x4e4d4d4d, 0x4f4e4e4e,
    0x58575757, 0x59585858, 0x5a595959, 0x5b5a5a5a, 0x5c5b5b5b, 0x5d5c5c5c, 0x5e5d5d5d, 0x5f5e5e5e,
    0x605f5f5f, 0x61606060, 0x62616161, 0x63626262, 0x64636363, 0x65646464, 0x66656565, 0x67666666,
    0x706f6f6f, 0x71707070, 0x72717171, 0x73727272, 0x74737373, 0x75747474, 0x76757575, 0x77767676,
    0x68676767, 0x69686868, 0x6a696969, 0x6b6a6a6a, 0x6c6b6b6b, 0x6d6c6c6c, 0x6e6d6d6d, 0x6f6e6e6e,
    0x78777777, 0x79787878, 0x7a797979, 0x7b7a7a7a, 0x7c7b7b7b, 0x7d7c7c7c, 0x7e7d7d7d, 0x7f7e7e7e,
    0x807f7f7f, 0x81808080, 0x82818181, 0x83828282, 0x84838383, 0x85848484, 0x86858585, 0x87868686,
    0x908f8f8f, 0x91909090, 0x92919191, 0x93929292, 0x94939393, 0x95949494, 0x96959595, 0x97969696,
    0x88878787, 0x89888888, 0x8a898989, 0x8b8a8a8a, 0x8c8b8b8b, 0x8d8c8c8c, 0x8e8d8d8d, 0x8f8e8e8e,
    0x98979797, 0x99989898, 0x9a999999, 0x9b9a9a9a, 0x9c9b9b9b, 0x9d9c9c9c, 0x9e9d9d9d, 0x9f9e9e9e,
    0xa09f9f9f, 0xa1a0a0a0, 0xa2a1a1a1, 0xa3a2a2a2, 0xa4a3a3a3, 0xa5a4a4a4, 0xa6a5a5a5, 0xa7a6a6a6,
    0xb0afafaf, 0xb1b0b0b0, 0xb2b1b1b1, 0xb3b2b2b2, 0xb4b3b3b3, 0xb5b4b4b4, 0xb6b5b5b5, 0xb7b6b6b6,
    0xa8a7a7a7, 0xa9a8a8a8, 0xaaa9a9a9, 0xabaaaaaa, 0xacababab, 0xadacacac, 0xaeadadad, 0xafaeaeae,
    0xb8b7b7b7, 0xb9b8b8b8, 0xbab9b9b9, 0xbbbababa, 0xbcbbbbbb, 0xbdbcbcbc, 0xbebdbdbd, 0xbfbebebe,
    0xc0bfbfbf, 0xc1c0c0c0, 0xc2c1c1c1, 0xc3c2c2c2, 0xc4c3c3c3, 0xc5c4c4c4, 0xc6c5c5c5, 0xc7c6c6c6,
    0xd0cfcfcf, 0xd1d0d0d0, 0xd2d1d1d1, 0xd3d2d2d2, 0xd4d3d3d3, 0xd5d4d4d4, 0xd6d5d5d5, 0xd7d6d6d6,
    0xc8c7c7c7, 0xc9c8c8c8, 0xcac9c9c9, 0xcbcacaca, 0xcccbcbcb, 0xcdcccccc, 0xcecdcdcd, 0xcfcecece,
    0xd8d7d7d7, 0xd9d8d8d8, 0xdad9d9d9, 0xdbdadada, 0xdcdbdbdb, 0xdddcdcdc, 0xdedddddd, 0xdfdedede,
    0xe0dfdfdf, 0xe1e0e0e0, 0xe2e1e1e1, 0xe3e2e2e2, 0xe4e3e3e3, 0xe5e4e4e4, 0xe6e5e5e5, 0xe7e6e6e6,
    0xf0efefef, 0xf1f0f0f0, 0xf2f1f1f1, 0xf3f2f2f2, 0xf4f3f3f3, 0xf5f4f4f4, 0xf6f5f5f5, 0xf7f6f6f6,
    0xe8e7e7e7, 0xe9e8e8e8, 0xeae9e9e9, 0xebeaeaea, 0xecebebeb, 0xedececec, 0xeeededed, 0xefeeeeee,
    0xf8f7f7f7, 0xf9f8f8f8, 0xfaf9f9f9, 0xfbfafafa, 0xfcfbfbfb, 0xfdfcfcfc, 0xfefdfdfd, 0xfffefefe,
};
// data 0x306380: the picture negative: dark goes white, through pale blue, bright goes dark red.
const u_int kRampNegative[256] = {
    0x00ffffff, 0x01fdfdfd, 0x02fbfbfb, 0x03f9f9f9, 0x04f7f7f7, 0x05f5f5f5, 0x06f3f3f3, 0x07f1f1f1,
    0x10fddede, 0x11fbdcdc, 0x12f9dada, 0x13f7d8d8, 0x14f5d6d6, 0x15f3d4d4, 0x16f1d2d2, 0x17f0d0d0,
    0x08eeeeee, 0x09ececec, 0x0aeaeaea, 0x0be8e8e8, 0x0ce6e6e6, 0x0de4e4e4, 0x0ee2e2e2, 0x0fe0e0e0,
    0x18eecece, 0x19eccccc, 0x1aeacaca, 0x1be8c8c8, 0x1ce6c6c6, 0x1de4c4c4, 0x1ee2c2c2, 0x1fe0c0c0,
    0x20dfbfbf, 0x21debebe, 0x22ddbdbd, 0x23dcbcbc, 0x24dbbbbb, 0x25dababa, 0x26d9b9b9, 0x27d8b8b8,
    0x30cfafaf, 0x31ceaeae, 0x32cdadad, 0x33ccacac, 0x34cbabab, 0x35caaaaa, 0x36c9a9a9, 0x37c8a8a8,
    0x28d7b7b7, 0x29d6b6b6, 0x2ad5b5b5, 0x2bd4b4b4, 0x2cd3b3b3, 0x2dd2b2b2, 0x2ed1b1b1, 0x2fd0b0b0,
    0x38c7a7a7, 0x39c6a6a6, 0x3ac5a5a5, 0x3bc4a4a4, 0x3cc3a3a3, 0x3dc2a2a2, 0x3ec1a1a1, 0x3fc0a0a0,
    0x40c0a0a0, 0x41bf9f9f, 0x42bf9f9f, 0x43be9e9e, 0x44be9e9e, 0x45bd9d9d, 0x46bd9d9d, 0x47bc9c9c,
    0x50b89898, 0x51b79797, 0x52b79797, 0x53b69696, 0x54b69696, 0x55b59595, 0x56b59595, 0x57b49494,
    0x48bc9c9c, 0x49bb9b9b, 0x4abb9b9b, 0x4bba9a9a, 0x4cba9a9a, 0x4db99999, 0x4eb99999, 0x4fb89898,
    0x58b49494, 0x59b39393, 0x5ab39393, 0x5bb29292, 0x5cb29292, 0x5db19191, 0x5eb19191, 0x5fb09090,
    0x60b09090, 0x61af8f8f, 0x62af8f8f, 0x63ae8e8e, 0x64ae8e8e, 0x65ad8d8d, 0x66ad8d8d, 0x67ac8c8c,
    0x70a88888, 0x71a78787, 0x72a78787, 0x73a68686, 0x74a68686, 0x75a58585, 0x76a58585, 0x77a48484,
    0x68ac8c8c, 0x69ab8b8b, 0x6aab8b8b, 0x6baa8a8a, 0x6caa8a8a, 0x6da98989, 0x6ea98989, 0x6fa88888,
    0x78a48484, 0x79a38383, 0x7aa38383, 0x7ba28282, 0x7ca28282, 0x7da18181, 0x7ea18181, 0x7fa08080,
    0x809f7f7f, 0x819e7e7f, 0x829c7d7e, 0x839b7c7e, 0x849a7b7d, 0x85997a7c, 0x8697797c, 0x8796787b,
    0x908b6f75, 0x918a6e75, 0x92886d74, 0x93876c74, 0x94866b73, 0x95856a72, 0x96836972, 0x97826871,
    0x8895777a, 0x8994767a, 0x8a927579, 0x8b917479, 0x8c907378, 0x8d8f7277, 0x8e8d7177, 0x8f8c7076,
    0x98816770, 0x99806670, 0x9a7e656f, 0x9b7d646f, 0x9c7c636e, 0x9d7b626d, 0x9e79616d, 0x9f78606c,
    0xa0775f6b, 0xa1765e6b, 0xa2745d6a, 0xa3735c6a, 0xa4725b69, 0xa5715a68, 0xa66f5968, 0xa76e5867,
    0xb0634f61, 0xb1624e61, 0xb2604d60, 0xb35f4c60, 0xb45e4b5f, 0xb55d4a5e, 0xb65b495e, 0xb75a485d,
    0xa86d5766, 0xa96c5666, 0xaa6a5565, 0xab695465, 0xac685364, 0xad675263, 0xae655163, 0xaf645062,
    0xb859475c, 0xb958465c, 0xba56455b, 0xbb55445b, 0xbc54435a, 0xbd534259, 0xbe514159, 0xbf504058,
    0xc04f3f57, 0xc14e3e57, 0xc24c3d56, 0xc34b3c56, 0xc44a3b55, 0xc5493a54, 0xc6473954, 0xc7463853,
    0xd03b2f4d, 0xd13a2e4d, 0xd2382d4c, 0xd3372c4c, 0xd4362b4b, 0xd5352a4a, 0xd633294a, 0xd7322849,
    0xc8453752, 0xc9443652, 0xca423551, 0xcb413451, 0xcc403350, 0xcd3f324f, 0xce3d314f, 0xcf3c304e,
    0xd8312748, 0xd9302648, 0xda2e2547, 0xdb2d2447, 0xdc2c2346, 0xdd2b2245, 0xde292145, 0xdf282044,
    0xe0271f43, 0xe1261e43, 0xe2241d42, 0xe3231c42, 0xe4221b41, 0xe5211a40, 0xe61f1940, 0xe71e183f,
    0xf0130f39, 0xf1120e39, 0xf2100d38, 0xf30f0c38, 0xf40e0b37, 0xf50d0a36, 0xf60b0936, 0xf70a0835,
    0xe81d173e, 0xe91c163e, 0xea1a153d, 0xeb19143d, 0xec18133c, 0xed17123b, 0xee15113b, 0xef14103a,
    0xf8090734, 0xf9080634, 0xfa060533, 0xfb050433, 0xfc040332, 0xfd030231, 0xfe010131, 0xff000030,
};

// ──────────────────────────────────────────────────────────────────────
// What lies over the frame.

// FS2 0x002cfb20
// The whole frame times level / 128; past 0xff, level - 0x80 on top of the
// frame as it stands. PC: a host pass in place of eight sprites of Cd * FIX.
// callcheck: gfxGetDrawEnv (PC: YamiPan_DrawUiFrameScale)

void gsFrameScale(sceVif1Packet *packet, int level)
{
    if (level > kFrameScaleOne) {
        level -= kFrameScaleBoost;
        YamiPan_DrawUiFrameScale(packet, 1.0f + (float)(level > kFrameScaleOne ? kFrameScaleOne : level) / kGsOne);
    } else {
        YamiPan_DrawUiFrameScale(packet, (float)level / kGsOne);
    }
}

// FS2 0x002cf880
// The frame pushed away from rgba, as far as its alpha says. PC: a host pass.
// callcheck: gfxGetDrawEnv (PC: YamiPan_DrawUiFrameContrast)

void gsFrameContrast(sceVif1Packet *packet, u_int rgba) { YamiPan_DrawUiFrameContrast(packet, rgba); }

// FS2 0x001ff010
// FS2 0x001ff460 gsShrinkFrame, FS2 0x002b6b90 gsLuminance and
// FS2 0x001ffb30 gsUploadClut256 are inside the host's pass.
// The frame goes through a ramp: halved with a bilinear filter, its brightness
// (R * 0x26 + G * 0x4b + B * 0xe, each over 128) picks a colour of ramp, and
// that is stretched back over the frame, with no blend. The ROM's a2 is how
// many times to halve; all three calls pass 1, which is what the host does.
// PC: a host pass in place of the buffers past the frame, with no GS memory.
// callcheck: gfxGetDrawEnv, FUN_001ff460, FUN_001ff460, FUN_002b6b90, FUN_001ffb30, gfxGetDrawEnv (PC: YamiPan_DrawUiFramePalette)

void SightJack::drawPaletteView(sceVif1Packet *packet, const u_int *ramp)
{
    YamiPan_DrawUiFramePalette(packet, ramp);
}

// FS2 0x002058e0
// rgba added over the frame. The ROM's eight sprites are one fill here.
// callcheck: gfxGetDrawEnv (PC: gsFillRect)

void gsFrameAdd(sceVif1Packet *packet, u_int rgba)
{
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kJackAdd);
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kJackBlackTest);
    gsFillRect(packet, 0, 0, kJackFrame, kJackFrame, rgba);
}

static void markerVertex(sceVif1Packet *packet, int x, int y, u_int rgba)
{
    YamiPanUiVertex vertex = {};
    vertex.rgba[0] = (u_char)rgba;
    vertex.rgba[1] = (u_char)(rgba >> 8);
    vertex.rgba[2] = (u_char)(rgba >> 16);
    vertex.rgba[3] = (u_char)(rgba >> 24);
    vertex.x = (u_short)x;
    vertex.y = (u_short)y;
    YamiPan_DrawUiVertex(packet, &vertex);
}

static void markerRay(sceVif1Packet *packet, int x, int y, int endX, int endY, u_int rgb, u_int centre)
{
    markerVertex(packet, x, y, centre);
    markerVertex(packet, endX, endY, rgb);
}

// FS2 0x00205f50
// A cross over object's head as view sees it: two white lines across the
// frame, brightest on the side it is on, four rays of rgba and, within
// kMarkerGlowDepth, a glow. The ROM patches vertices into two static packets
// (0x2fa450, 0x2fa560); a row out of the window goes to 0 there and is left
// out here.
// PC: the 3D widens with the display, so what sits on a projected point is
// pulled in by as much (YamiPan_DrawUiSqueeze); the line across stays whole.
// callcheck: find, getMatrix, getMatrix, applyMatrix, applyMatrix, sceVif1PkCnt, sceVif1PkAddCode, sceVif1PkAddCode, FUN_0010b1c8, sceVif1PkCloseDmaTag, sceVif1PkCnt, sceVif1PkAddCode, sceVif1PkAddCode, FUN_0010b1c8, sceVif1PkCloseDmaTag

void jackMarker(sceVif1Packet *packet, StageObject *object, StageObject *view, u_int rgba)
{
    if (object == nullptr)
        return;

    sceVu0FVECTOR head;
    if (auto *bone = object->m_model->boneMap.find("bn_head")->value) {
        const int family = object->m_motion->m_modelFamily;
        // The ROM reads address 0x20 for a family out of range.
        static const sceVu0FVECTOR kNoOffset = { 0.0f, 0.0f, 0.0f, 1.0f };
        applyMatrix(head, bone->getMatrix(),
                    family < 0 || family >= kMotionModelCount ? kNoOffset : gMotionModels[family].b);
    } else {
        const sceVu0FVECTOR *matrix = object->getMatrix();
        head[0] = matrix[3][0];
        head[1] = matrix[3][1];
        head[2] = matrix[3][2];
        head[3] = matrix[3][3];
    }

    sceVu0FVECTOR screen;
    applyMatrix(screen, static_cast<StageCamera *>(view)->m_viewScreen, head);
    const float depth = screen[3];
    if (depth < kMarkerNear || depth > kMarkerFar)
        return;

    const float strength = (float)(rgba >> 24) / kGsOne;
    const int x = (int)(screen[0] / depth * 16.0f);
    const int y = (int)(screen[1] / depth * 16.0f);
    const float closeness = 1.0f - (depth > kMarkerFade ? kMarkerFade : depth) / kMarkerFade;

    const u_int lineOn = ((u_int)(int)(strength * 16.0f) << 24) | kMarkerWhite;
    const int rayX = (int)((closeness * kMarkerRaySpan + kMarkerRayMin) * 16.0f);
    const int rayY = rayX * 4 / 3;
    const bool inReach = x > kWindowMin - rayX && x < kWindowMax + rayX
                      && y > kWindowMin - rayY && y < kWindowMax + rayY;
    const u_int rgb = rgba & kMarkerRgb;

    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kJackAlpha);
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kMarkerTest);
    YamiPan_DrawUiPrim(packet, kMarkerLines, nullptr);
    YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_RGBAQ);
    if (y > kWindowMin && y < kWindowMax) {
        markerVertex(packet, kWindowMin, y, x < kWindowCentre ? lineOn : kMarkerWhite);
        markerVertex(packet, kWindowMax, y, x < kWindowCentre ? kMarkerWhite : lineOn);
    }
    YamiPan_DrawUiSqueeze(packet, 1);
    if (x > kWindowMin && x < kWindowMax) {
        markerVertex(packet, x, kWindowMin, y < kWindowCentre ? lineOn : kMarkerWhite);
        markerVertex(packet, x, kWindowMax, y < kWindowCentre ? kMarkerWhite : lineOn);
    }
    if (inReach) {
        const u_int centre = ((u_int)(int)(strength * kGsOne) << 24) | rgb;
        markerRay(packet, x, y, x, y - rayY, rgb, centre);
        markerRay(packet, x, y, x, y + rayY, rgb, centre);
        markerRay(packet, x, y, x + rayX, y, rgb, centre);
        markerRay(packet, x, y, x - rayX, y, rgb, centre);
    }
    if (depth <= kMarkerGlowDepth && inReach) {
        const int reach = (int)((closeness * kMarkerGlowSpan + 1.0f) * 16.0f);
        YamiPan_DrawUiPrim(packet, kMarkerGlow, nullptr);
        YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_RGBAQ);
        markerVertex(packet, x, y, ((u_int)(int)(strength * 32.0f) << 24) | rgb);
        markerVertex(packet, x, y - rayY, rgb);
        markerVertex(packet, x + reach, y - reach, rgb);
        markerVertex(packet, x + rayX, y, rgb);
        markerVertex(packet, x + reach, y + reach, rgb);
        markerVertex(packet, x, y + rayY, rgb);
        markerVertex(packet, x - reach, y + reach, rgb);
        markerVertex(packet, x - rayX, y, rgb);
        markerVertex(packet, x - reach, y - reach, rgb);
        markerVertex(packet, x, y - rayY, rgb);
    }
    YamiPan_DrawUiSqueeze(packet, 0);
}

// FS2 0x001ffcd0
// A bound button's side of the frame lights and fades to the middle. The
// ROM patches the static packet 0x2fa650.
// callcheck: sceVif1PkCnt, sceVif1PkAddCode, sceVif1PkAddCode, FUN_0010b1c8, sceVif1PkCloseDmaTag

void SightJack::drawBindFlash(sceVif1Packet *packet) const
{
    if (m_bindFlash <= 0 || m_bindShown == kJackNoBind)
        return;
    // layout: host -- the four corners, read from the ROM's patches.
    struct Side { int x0, y0, x1, y1, x2, y2, x3, y3; };
    static const Side kSides[kJackBinds] = {
        { kWindowMin, kWindowMin, kWindowMax, kWindowMin, kWindowMin, kWindowCentre, kWindowMax, kWindowCentre },
        { kWindowMax, kWindowMin, kWindowMax, kWindowMax, kWindowCentre, kWindowMin, kWindowCentre, kWindowMax },
        { kWindowMin, kWindowMax, kWindowMax, kWindowMax, kWindowMin, kWindowCentre, kWindowMax, kWindowCentre },
        { kWindowMin, kWindowMin, kWindowMin, kWindowMax, kWindowCentre, kWindowMin, kWindowCentre, kWindowMax },
    };
    const Side &side = kSides[m_bindShown];
    const u_int lit = ((u_int)(m_bindFlash * kBindFlashAlpha >> 4) << 24) | kBindFlashGrey;

    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kJackAdd);
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kMarkerTest);
    YamiPan_DrawUiPrim(packet, kBindFlashStrip, nullptr);
    YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_RGBAQ);
    markerVertex(packet, side.x0, side.y0, lit);
    markerVertex(packet, side.x1, side.y1, lit);
    markerVertex(packet, side.x2, side.y2, kBindFlashGrey);
    markerVertex(packet, side.x3, side.y3, kBindFlashGrey);
}

// Inline three times in the ROM: the viewer's cross, then the partner's
// unless it is the one looked through.
// callcheck: getTopView, jackMarker, canBeJacked, jackMarker

void SightJack::drawOwnMarkers(sceVif1Packet *packet)
{
    StageObject *view = gStageScene->getTopView();
    jackMarker(packet, m_viewer, view, kMarkerViewer);
    StageObject *partner = gStageScene->partner;
    if (partner != nullptr && static_cast<StageCharacter *>(partner)->canBeJacked() && viewed() != partner)
        jackMarker(packet, partner, view, kMarkerPartner);
}

// FS2 0x00201520
// Everyone viewable who has found a foe.
// callcheck: getTopView, FUN_00200d10, jackMarker

void SightJack::drawFoeMarkers(sceVif1Packet *packet)
{
    StageObject *view = gStageScene->getTopView();
    StageObjArray &list = gStageScene->objects->m_lists[StageObjGroup_Characters];
    for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *object = *slot;
        if (canView(object) && object->m_brain != nullptr && static_cast<StageCharacter *>(object)->getBrainPace() == BrainState_Foe
            && object != m_viewer && object != gStageScene->partner && object != viewed())
            jackMarker(packet, object, view, kMarkerFoe);
    }
}

// FS2 0x00201380
// After the jack: the kept one's cross, while it has found a foe, fades.
// callcheck: FUN_00200d10, getTopView, openList, jackMarker, closeList

void SightJack::drawFadingMarker()
{
    if (m_markerFrames <= 0 || (m_mode != JackMode_KeptTurn && m_mode != JackMode_Kept))
        return;
    StageObject *kept = m_kept;
    if (!canView(kept))
        return;
    StageObject *view = gStageScene->getTopView();
    GamePacketManager *packets = &gGameSystem->m_packets;
    sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
    if (kept != gStageScene->partner && kept->m_brain != nullptr && static_cast<StageCharacter *>(kept)->getBrainPace() == BrainState_Foe
        && kept != m_viewer) {
        const u_int alpha = (u_int)((m_markerFrames << 7) / kMarkerFrames) << 24;
        jackMarker(packet, kept, view, alpha | (kMarkerFoe & kMarkerRgb));
    }
    packets->closeList();
    m_markerFrames--;
}

// FS2 0x00201840
// While a foe attacks: the own crosses, the film, a red wash, the bright frame.
// callcheck: openList, getTopView, jackMarker, canBeJacked, jackMarker, step, draw, demoFilmDraw, gsFillRect, closeList, openList, gsFrameScale, closeList

void SightJack::drawWarning(float filmLevel)
{
    GamePacketManager *packets = &gGameSystem->m_packets;
    sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
    drawOwnMarkers(packet);
    if (m_flecks != nullptr) {
        m_flecks->step();
        m_flecks->draw(packet);
    }
    m_film.draw(filmLevel, packet);
    // PC: the ROM leaves ALPHA_1 as the film set it.
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kJackAlpha);
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kJackBlackTest);
    gsFillRect(packet, 0, 0, kJackFrame, kJackFrame, kWarningWash);
    packets->closeList();

    packet = packets->openList(PacketList_SceneModel);
    gsFrameScale(packet, m_screenLevel);
    packets->closeList();
}

// callcheck: demoFilmDraw, step, draw

void SightJack::drawFilm(float filmLevel, sceVif1Packet *packet)
{
    m_film.draw(filmLevel, packet);
    if (m_flecks != nullptr) {
        m_flecks->step();
        m_flecks->draw(packet);
    }
}

// FS2 0x00201a40
// callcheck: openList, gsFillRect, gfxGetTexArea, getList, demoFilmDraw, step, draw, drawBindFlash, closeList

void SightJack::drawNobody(float filmLevel)
{
    GamePacketManager *packets = &gGameSystem->m_packets;
    sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
    fillBlack(packet);
    m_fill->m_level = 1.0f;
    m_fill->draw(packet, scene2dTexList());
    drawFilm(filmLevel, packet);
    drawBindFlash(packet);
    packets->closeList();
}

// FS2 0x002016d0
// callcheck: openList, gsFillRect, gfxGetTexArea, getList, demoFilmDraw, step, draw, drawBindFlash, closeList

void SightJack::drawCorpse(float filmLevel)
{
    GamePacketManager *packets = &gGameSystem->m_packets;
    sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
    fillBlack(packet);
    m_fill->draw(packet, scene2dTexList());
    drawFilm(filmLevel, packet);
    drawBindFlash(packet);
    packets->closeList();
}

// FS2 0x00201bc0
// The film and the grain over the eyes' picture, the crosses; under them on
// list 8 the bright frame and the tint of what the character is.
// Through the boss with the camera above 195, last of all the water
// (ripplesheet.h).
// callcheck: openList, demoFilmDraw, gfxGetTexArea, getList, getTopView, jackMarker, canBeJacked, jackMarker, step, draw, drawBindFlash, closeList, openList, gsFrameScale, getStageSelect, strcmp, strcmp, getStageSelect, gsFrameAdd, getStageSelect, strcmp, strcmp, gsFrameContrast, getStageSelect, drawFoeMarkers, strcmp, strcmp, strcmp, getStageSelect, strcmp, strcmp, strcmp, getStageSelect, isTakenOver, strcmp, strcmp, closeList, draw, advance, getDeltaSeconds (PC: gfxGetDrawEnv, gsFrameContrast and gsFillRect are in RippleSheet::draw's host pass)

void SightJack::drawLive(float filmLevel)
{
    GamePacketManager *packets = &gGameSystem->m_packets;
    sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
    m_film.draw(filmLevel, packet);
    m_fill->draw(packet, scene2dTexList());
    drawOwnMarkers(packet);
    if (m_mode == JackMode_KeptTurn || m_mode == JackMode_Kept)
        drawFoeMarkers(packet);
    if (m_flecks != nullptr) {
        m_flecks->step();
        m_flecks->draw(packet);
    }
    drawBindFlash(packet);
    packets->closeList();

    packet = packets->openList(PacketList_SceneModel);
    gsFrameScale(packet, m_screenLevel);

    StageObject *target = viewed();
    const int stage = getStageSelect()->stage;
    const bool isPartner = target == gStageScene->partner;
    // The ROM also compares the name with F_E08_YPR here and drops the answer.
    const bool paletteA = (isPartner && stage == kStagePartnerPalette)
                       || (stage == kStagePaletteA && strcmp(target->m_name, "F_E24_SNPC1") == 0);
    const bool paletteB = (stage == kStageE21 && strcmp(target->m_name, "F_E21_SNPC1") == 0)
                       || (stage == kStagePaletteA && strcmp(target->m_name, "F_E24_SNPC2") == 0)
                       || (stage == kStageBoss && strcmp(target->m_name, "F_SNPC2") == 0);
    const bool boss = stage == kStageBoss && strcmp(target->m_name, "F_E27_BOSS") == 0;

    if (paletteA)
        drawPaletteView(packet, kRampGrey);
    else if (paletteB)
        drawPaletteView(packet, kRampNegative);

    if (m_viewer->m_altAction != 0) {
        gsFrameAdd(packet, kAltActionWash);
    } else if (!paletteA && !paletteB && !boss) {
        u_int tint = 0;
        if (isPartner) {
            tint = kTintPartner;
        } else if (static_cast<StageCharacter *>(target)->isTakenOver()) {
            tint = kTintTakenOver;
        } else if (target->m_brain != nullptr) {
            const int state = static_cast<StageCharacter *>(target)->getBrainPace();
            if (state == BrainState_Foe)
                tint = kTintFoe;
            else if (state == BrainState_Slot4a || state == BrainState_Slot4b || state == BrainState_Slot4c)
                tint = kTintSearching;
        }
        if (tint != 0)
            gsFrameContrast(packet, tint);
    }

    if (boss && m_camera->getMatrix()[3][2] > kRippleCameraAbove) {
        m_ripples->draw(packet, kRippleTint);
        m_ripples->advance(gStageScene->clock->getDeltaSeconds());
    }
    packets->closeList();
}

// FS2 0x002024c0
// The frame's gate.
// callcheck: vecDistance, openList, demoFilmDraw, step, draw, closeList, openList, gsFrameScale, closeList, openList, getTopView, jackMarker, canBeJacked, jackMarker, closeList, openList, gsFillRect, gfxGetTexArea, getList, FUN_001518a0, gfxGetTexArea, getList, FUN_001518a0, demoFilmDraw, step, draw, drawBindFlash, closeList, drawWarning, openList, gfxGetTexArea, getList, FUN_001518a0, gfxGetTexArea, getList, FUN_001518a0, closeList, drawFadingMarker, openList, gfxGetTexArea, getList, demoFilmDraw, step, draw, drawBindFlash, closeList, openList, gsFrameScale, closeList, getAltSoundFlag, getLifeState, FUN_00215370, FUN_00201bc0, FUN_00201a40, FUN_002016d0

void SightJack::draw()
{
    m_viewer = gStageScene->player;
    GamePacketManager *packets = &gGameSystem->m_packets;

    StageObject *target = viewed();
    float filmLevel = 0.0f;
    if (target != nullptr) {
        const float distance = vecDistance(m_viewer->m_xform.position, target->m_xform.position);
        if (distance > m_filmNear)
            filmLevel = distance < m_filmFar ? (distance - m_filmNear) / (m_filmFar - m_filmNear) : 1.0f;
    }

    if (m_gameOverOn != nullptr) {
        sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
        drawFilm(filmLevel, packet);
        packets->closeList();
        packet = packets->openList(PacketList_SceneModel);
        gsFrameScale(packet, m_screenLevel);
        packets->closeList();
        packet = packets->openList(PacketList_Screen);
        drawOwnMarkers(packet);
        packets->closeList();
        return;
    }
    if (m_warner != nullptr) {
        drawWarning(filmLevel);
        return;
    }
    if (m_visionFade > 0) {
        sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
        const u_int alpha = (u_int)(m_visionFade * kVisionFadeScale / 8) << 24;
        m_fill->drawGrain(packet, scene2dTexList(), alpha | kVisionTintA);
        m_fill->drawGrain(packet, scene2dTexList(), alpha | kVisionTintB);
        packets->closeList();
        m_visionFade--;
        return;
    }
    if (!m_running) {
        drawFadingMarker();
        return;
    }

    m_markerFrames = kMarkerFrames;
    if (m_flashFrames >= 0) {
        m_fill->m_level = (float)m_flashFrames / kFlashFramesScale;
        m_flashFrames--;
    }

    if (!m_visionOn) {
        if (target == nullptr || target->getAltSoundFlag() != 0)
            drawNobody(filmLevel);
        else if (static_cast<StageCharacter *>(target)->getLifeState() == LifeState_Corpse
                 || static_cast<StageCharacter *>(target)->isJackBlank())
            drawCorpse(filmLevel);
        else
            drawLive(filmLevel);
    } else if (m_visionFrames < 1) {
        sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
        m_fill->draw(packet, scene2dTexList());
        drawFilm(filmLevel, packet);
        drawBindFlash(packet);
        packets->closeList();
        packet = packets->openList(PacketList_SceneModel);
        gsFrameScale(packet, m_screenLevel);
        drawPaletteView(packet, kRampSepia);
        packets->closeList();
    } else {
        m_visionFrames--;
        sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
        fillBlack(packet);
        m_fill->drawGrain(packet, scene2dTexList(), kVisionWaitTintA);
        m_fill->drawGrain(packet, scene2dTexList(), kVisionWaitTintB);
        drawFilm(filmLevel, packet);
        drawBindFlash(packet);
        packets->closeList();
    }
    if (m_bindFlash > 0)
        m_bindFlash--;
}

// ──────────────────────────────────────────────────────────────────────
// StageParty: +0 is the record.

// FS2 0x001fecf0
// callcheck: operator_new, FUN_00203d70, SightJack

StageParty::StageParty() : m_jack(new SightJack) {}

// FS2 0x001fec90
// callcheck: FUN_00203be0, operator_delete, ~SightJack

StageParty::~StageParty() { delete m_jack; }

// FS2 0x001fe6b0
bool StageParty::isRecordBusy() { return m_jack->m_warner != nullptr; }

// FS2 0x001fe6c0
// callcheck: FUN_001fed30

void StageParty::restore() { m_jack->restoreBinds(); }

// FS2 0x001fed30

void SightJack::restoreBinds()
{
    for (int i = 0; i < kJackBinds; i++) {
        if (sSavedBinds[i] == -1) {
            m_binds[i] = nullptr;
            continue;
        }
        StageObjArray &list = gStageScene->objects->m_lists[StageObjGroup_Characters];
        for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
            if (*slot != nullptr && sSavedBinds[i] == (*slot)->m_id)
                m_binds[i] = *slot;
        }
    }
}

// FS2 0x001fe6d0

void StageParty::save()
{
    for (int i = 0; i < kJackBinds; i++)
        sSavedBinds[i] = m_jack->m_binds[i] == nullptr ? -1 : m_jack->m_binds[i]->m_id;
}

// FS2 0x001fe720
// callcheck: memset

void StageParty::resetSaved() { memset(sSavedBinds, 0xff, sizeof(sSavedBinds)); }

// FS2 0x001fe740

void StageParty::addVisionB(StageObject *object)
{
    m_jack->m_visionHide[m_jack->m_visionHideCount++] = object;
}

// FS2 0x001fe770

void StageParty::addVisionA(StageObject *object)
{
    m_jack->m_visionShow[m_jack->m_visionShowCount++] = object;
}

// FS2 0x001fe7a0
// callcheck: FUN_001fef50

void StageParty::setVisionTarget(StageObject *object) { m_jack->setVisionTarget(object); }

// FS2 0x001fef50
// callcheck: addNamed

void SightJack::setVisionTarget(StageObject *object)
{
    if (m_tuned == m_vision)
        m_tuned = nullptr;
    if (m_kept == m_vision)
        m_kept = nullptr;
    for (int i = 0; i < kJackBinds; i++) {
        if (m_binds[i] == m_vision)
            m_binds[i] = nullptr;
    }
    if (object == nullptr) {
        m_visionShowCount = 0;
        m_visionHideCount = 0;
    }
    m_vision = object;
    if (object != nullptr) {
        m_visionFade = 8;
        if (gStageScene->rumble != nullptr)
            gStageScene->rumble->addNamed(kSoundVision, nullptr);
    }
}

// FS2 0x001fe7b0

void StageParty::setVisionFrames(int frames) { m_jack->m_visionFrames = frames; }

// FS2 0x001fe7c0

void StageParty::startGameOverView(StageObject *who)
{
    SightJack *jack = m_jack;
    jack->m_running = 1;
    jack->m_gameOverOn = who;
    jack->m_mode = JackMode_Kept;
    jack->m_viewer = gStageScene->player;
    jack->m_fill->m_level = 0.0f;
    jack->m_bindShown = JackMode_Kept;
    jack->m_flashFrames = 0;
    jack->m_noisePlaying = 0;
}

// FS2 0x001fe800
bool StageParty::isRunning() { return m_jack->m_running != 0; }

// FS2 0x001fe810
void StageParty::setJackAngle(float angle) { m_jack->m_angle = angle > 0.0f ? angle : 0.0f; }

// FS2 0x001fe850
void StageParty::setJackRange(float range) { m_jack->m_range = range > 0.0f ? range : 0.0f; }

// FS2 0x001fe890
// callcheck: operator_new, Tim2Texture, FUN_001928f0

void StageParty::addImage(const void *image)
{
    m_jack->m_film.frames.push_back(new Tim2Texture(image));
}

// FS2 0x001fe900
// callcheck: operator_new, Tim2Texture, operator_new, DemoFleck

void StageParty::setImage(const void *image)
{
    SightJack *jack = m_jack;
    delete jack->m_flecks;
    delete jack->m_fleckTexture;
    jack->m_fleckTexture = new Tim2Texture(image);
    jack->m_flecks = new DemoFleck(jack->m_fleckTexture);
}

// FS2 0x001fe9b0
// callcheck: setImage

void StageParty::setFillImage(const void *image) { m_jack->m_fill->setImage(image); }

// FS2 0x001fe9c0

void StageParty::setCompanion(StageObject *object)
{
    SightJack *jack = m_jack;
    jack->m_mode = JackMode_Kept;
    jack->m_kept = object;
    if (jack->m_fill != nullptr)
        jack->m_fill->m_level = 0.0f;
}

// FS2 0x001fe9f0

bool StageParty::isCompanionMode()
{
    return m_jack->m_mode == JackMode_KeptTurn || m_jack->m_mode == JackMode_Kept;
}

// FS2 0x001fea50
StageObject *StageParty::getCompanion() { return m_jack->viewed(); }

// FS2 0x001fea30
DemoFleck *StageParty::getFlecks() { return m_jack->m_flecks; }
// FS2 0x001fea40
JackFilm *StageParty::getFilm() { return &m_jack->m_film; }

// FS2 0x00204250
// A copy of the film: its own textures of the same files, the same settings.
// callcheck: operator_new, operator_new, Tim2Texture, FUN_001928f0

JackFilm *JackFilm::clone() const
{
    JackFilm *copy = new JackFilm;
    for (const Tim2Texture *frame : frames)
        copy->frames.push_back(new Tim2Texture(frame->m_image));
    copy->band = band;
    copy->color = color;
    copy->levelLow = levelLow;
    copy->levelHigh = levelHigh;
    return copy;
}

// FS2 0x002052d0
// The ROM's second argument says if the record goes too; here delete does.
// callcheck: operator_delete

JackFilm::~JackFilm()
{
    for (Tim2Texture *frame : frames)
        delete frame;
}

// FS2 0x00204550
// FS2 0x00205070 drawCorner, FS2 0x002053a0 gsGreenToAlpha, FS2 0x00205670
// gsUploadClutOrdered and FS2 0x00205b80 gsStripSprite are the GS's side,
// inside the host's pass.
// Each corner steps, then the ROM composes the frames at twice their size
// (right and bottom corners mirrored), each corner its shown frame with the
// leaving one at 1 - progress over it, turns the composition's green into an
// 8-bit image and draws that over the whole frame through a table of 256:
// clear up to the band, solid colour past it, alpha rising across it.
// PC: the table's numbers go to the host's pass; no GS memory, no scratchpad.
// callcheck: demoFleckRandNext, demoFleckRandNext, demoFleckRandNext, gfxGetVramBlock, FUN_00205670, gfxGetDrawEnv, gfxGetVramBlock, getVramBlocks, FUN_00205070, FUN_00205070, FUN_00205070, FUN_00205070, gfxGetVramBlock, getVramBlocks, FUN_002053a0, gfxGetVramBlock, getVramBlocks, FUN_00205b80, __fixunssfdi (PC: YamiPan_DrawUiFilm)

void JackFilm::draw(float level, sceVif1Packet *packet)
{
    constexpr float kOne = 1.0f;
    constexpr u_int kRandShift = 0x10;
    constexpr u_int kRandMask = 0x7fff;
    constexpr u_int kRandBits = 15;
    constexpr u_int kStepShift = 12;    // the draw's top three bits
    constexpr float kStepFrames = 16.0f; // 16..23 frames to a fade
    constexpr float kBrightest = 255.0f;
    constexpr int   kTableSize = 0x100;
    constexpr float kGsOne = 128.0f;

    if (frames.empty())
        return;

    const u_int count = (u_int)frames.size();
    const auto nextDraw = [this]() {
        seed = demoFleckRandNext(seed);
        return seed >> kRandShift & kRandMask;
    };

    YamiPanFilm film{};
    for (int i = 0; i < kJackFilmCorners; i++) {
        Corner &corner = corners[i];
        corner.progress += corner.step;
        if (corner.progress >= kOne) {
            corner.step = kOne / ((float)(nextDraw() >> kStepShift) + kStepFrames);
            corner.progress = 0.0f;
            int leaving = corner.shown;
            if (leaving == -1)
                leaving = (int)(count * nextDraw() >> kRandBits);
            corner.leaving = leaving;
            corner.shown = (int)(count * nextDraw() >> kRandBits);
            if (corner.shown == corner.leaving && (u_int)++corner.shown >= count)
                corner.shown = 0;
        }
        film.corners[i].shown = frames[corner.shown];
        film.corners[i].leaving = frames[corner.leaving];
        film.corners[i].leavingAlpha = (float)(int)((kOne - corner.progress) * kGsOne) / kGsOne;
    }

    const int centre = (int)(kBrightest - (level * (levelHigh - levelLow) + levelLow) * kBrightest);
    const int halfBand = (int)band / 2;
    const auto inTable = [](int entry) { return entry > kTableSize ? kTableSize : entry < 0 ? 0 : entry; };
    film.bandStart = centre - halfBand;
    film.bandLength = (int)band;
    film.clearBelow = inTable(centre - halfBand);
    film.solidAbove = inTable(centre + halfBand);
    film.color = color;
    YamiPan_DrawUiFilm(packet, &film);
}

// FS2 0x001feab0
// callcheck: ownedHasTarget, pushView, getSeconds, FUN_002bc3c0

void StageParty::warnAttacker(StageCharacter *attacker)
{
    SightJack *jack = m_jack;
    if (!jack->m_running && jack->m_warner == nullptr && !gStageScene->ownedHasTarget()) {
        gStageScene->pushView(jack->m_camera, 2, 1);
        jack->m_warnPushed = 1;
    }
    jack->m_warner = attacker;
    jack->m_warnEnd = attacker->m_clock->getSeconds() + kWarnSeconds;
    if (gStageScene->rumble != nullptr)
        gStageScene->rumble->addNamed(kSoundPetit, nullptr);
}

// FS2 0x001feb70
void StageParty::draw() { m_jack->draw(); }
// FS2 0x001feb80
void StageParty::update() { m_jack->update(); }
// FS2 0x001feb90
void StageParty::stop() { m_jack->stop(); }
// FS2 0x001feba0
void StageParty::start(StageObject *object) { m_jack->start(object); }

// FS2 0x001febb0
// callcheck: operator_new, StageCamera, addToScene, setDirtyTree, FUN_00203ae0

void StageParty::createCamera()
{
    SightJack *jack = m_jack;
    jack->m_camera = new JackCamera;
    jack->m_camera->addToScene("camjack");
    jack->m_camera->setDirtyTree(false);
    jack->reset();
}

// ──────────────────────────────────────────────────────────────────────
// StageDaylight.

// ALPHA_1 by DaylightGradient::blend (0x2fd9b0): the table runs into other
// data after these, and every row here says 0.
constexpr u_long kDaylightBlend[] = { 0x48, 0x49 };
constexpr u_int  kDaylightPrim = 0x14c;     // TRISTRIP, IIP, ABE, FST
constexpr u_int  kDaylightOrigin = 0x700;   // gsFillRect's: the window's corner
constexpr u_int  kDaylightZ = 0xff0fffff;
constexpr u_int  kRgbMask = 0xffffff;       // RGBAQ with no alpha
constexpr float  kFilmBrightest = 255.0f;   // a band's size as a level

enum DaylightKind
{
    DaylightKind_None   = -1,
    DaylightKind_Pale   = 0,
    DaylightKind_Blue   = 1,
    DaylightKind_Bright = 2,
};

// FS2 0x0030f930
static const DaylightGradient kDaylightGradients[] = {
    { 0x40696e80, 0x18787880, 0x20696e80, 0x00787880, 0, 0.6f, 3.0f,   2.0f },
    { 0x46647d96, 0x32646e7d, 0x14000000, 0x00000000, 0, 0.5f, 1.57f,  2.0f },
    { 0x96696e96, 0x1423285a, 0x285f6478, 0x0a787880, 0, 0.6f, 5.034f, 3.0f },
};

// FS2 0x0030f990
static const DaylightFilmSettings kDaylightFilms[] = {
    { 0.3f, -0.5f, 700, 0.6f, 2.0f, 2.0f },
    { 0.3f, -0.5f, 400, 0.6f, 2.0f, 2.0f },
    { 0.3f, -0.5f, 400, 0.6f, 2.0f, 2.0f },
};

static inline float daylightLerp(float from, float to, float t) { return t * (to - from) + from + 0.0f; }

// t of the way from a to b, RGBA byte by byte.
// FS2 0x002d0370
static u_int daylightMix(float t, u_int a, u_int b)
{
    const auto channel = [t, a, b](int shift) {
        const int from = (int)(a >> shift & 0xff);
        const int to = (int)(b >> shift & 0xff);
        const int value = (int)(t * (float)(to - from) + (float)from + 0.0f);
        return (u_int)(value < 0 ? 0 : value > 0xff ? 0xff : value);
    };
    return channel(24) << 24 | channel(16) << 16 | channel(8) << 8 | channel(0);
}

static void daylightVertex(sceVif1Packet *packet, int x, int y, u_int rgba)
{
    YamiPanUiVertex vertex = {};
    vertex.rgba[0] = (u_char)rgba;
    vertex.rgba[1] = (u_char)(rgba >> 8);
    vertex.rgba[2] = (u_char)(rgba >> 16);
    vertex.rgba[3] = (u_char)(rgba >> 24);
    vertex.x = (u_short)((x + kDaylightOrigin) * 0x10);
    vertex.y = (u_short)((y + kDaylightOrigin) * 0x10);
    vertex.z = kDaylightZ;
    YamiPan_DrawUiVertex(packet, &vertex);
}

// FS2 0x002d0170
// The ROM's eight strips across are one here: only y shades.
// callcheck: gfxGetDrawEnv (PC: YamiPan_DrawUiPrim, YamiPan_DrawUiVertex)

void DaylightGradient::fill(sceVif1Packet *packet, u_int topRgba, u_int bottomRgba) const
{
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kDaylightBlend[blend]);
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kJackBlackTest);
    YamiPan_DrawUiPrim(packet, kDaylightPrim, nullptr);
    YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_RGBAQ);
    daylightVertex(packet, 0, 0, topRgba);
    daylightVertex(packet, kJackFrame, 0, topRgba);
    daylightVertex(packet, 0, kJackFrame, bottomRgba);
    daylightVertex(packet, kJackFrame, kJackFrame, bottomRgba);
}

// FS2 0x002cff90
// callcheck: daylightMix, daylightMix, DaylightGradient::fill

void DaylightGradient::drawFade(float t, sceVif1Packet *packet) const
{
    if (fadeSeconds > 0.0f && t < fadeSeconds) {
        const float done = t / fadeSeconds;
        u_int topRgba = daylightMix(done, topSettled, topSettled & kRgbMask);
        u_int bottomRgba = daylightMix(done, bottomSettled, bottomSettled & kRgbMask);
        fill(packet, topRgba, bottomRgba);
    }
}

// FS2 0x002d0040
// callcheck: daylightMix, daylightMix, daylightMix, daylightMix, fill

void DaylightGradient::drawRise(float t, sceVif1Packet *packet) const
{
    u_int topRgba, bottomRgba;
    if (t <= peakAt) {
        const float done = peakAt <= 0.0f ? 1.0f : t / peakAt;
        topRgba = daylightMix(done, top & kRgbMask, top);
        bottomRgba = daylightMix(done, bottom & kRgbMask, bottom);
    } else if (t <= settledAt) {
        const float span = settledAt - peakAt;
        const float done = span <= 0.0f ? 0.0f : (t - peakAt) / span;
        topRgba = daylightMix(done, top, topSettled);
        bottomRgba = daylightMix(done, bottom, bottomSettled);
    } else {
        topRgba = topSettled;
        bottomRgba = bottomSettled;
    }
    fill(packet, topRgba, bottomRgba);
}

// FS2 0x002cfe20
// callcheck: draw

void DaylightFilm::draw(float t, sceVif1Packet *packet)
{
    if (film == nullptr)
        return;
    const float clear = (float)-(int)film->band / kFilmBrightest;
    float level;
    if (t <= settings.peakAt) {
        const float done = settings.peakAt <= 0.0f ? 1.0f : t / settings.peakAt;
        level = daylightLerp(clear, settings.peak, done);
    } else if (t <= settings.settledAt) {
        const float span = settings.settledAt - settings.peakAt;
        const float done = span <= 0.0f ? 0.0f : (t - settings.peakAt) / span;
        level = daylightLerp(settings.peak, settings.settled, done);
    } else {
        level = settings.settled;
    }
    film->band = (u_int)settings.band;
    film->draw(level, packet);
}

// FS2 0x002cff30
// callcheck: ~JackFilm, operator_delete

DaylightFilm::~DaylightFilm()
{
    delete film;
}

// FS2 0x002d07a0
// callcheck: getFilm, clone

StageDaylight::StageDaylight(StageNodeTree *tree, int stage)
{
    constexpr u_int kFilmBand = 0x100;
    constexpr float kFilmHighest = 1.0f;

    JackFilm *jackFilm = gStageScene != nullptr && gStageScene->party != nullptr
        ? gStageScene->party->getFilm() : nullptr;
    if (jackFilm != nullptr) {
        m_film.film = jackFilm->clone();
        m_film.film->band = kFilmBand;
        m_film.film->levelLow = 0.0f;
        m_film.film->levelHigh = kFilmHighest;
    }
    m_tree = tree;

    int kind;
    switch (stage) {
    case 0x1b:
        kind = DaylightKind_Bright;
        break;
    case 0x5d:
    case 0x19:
        kind = DaylightKind_Blue;
        break;
    case 0x16:
    case 0x14:
    case 0x12:
    case 0x11:
    case 3:
        kind = DaylightKind_Pale;
        break;
    default:
        kind = DaylightKind_None;
        break;
    }
    m_active = kind != DaylightKind_None;
    if (kind == DaylightKind_None)
        kind = DaylightKind_Pale;
    m_gradient = kDaylightGradients[kind];
    m_film.settings = kDaylightFilms[kind];
}

// FS2 0x002d0750
// callcheck: contains

void StageDaylight::setPlace(const float *position)
{
    m_inside = m_tree != nullptr && m_tree->contains(position);
    m_insideSeconds = m_film.settings.settledAt;
    m_outsideSeconds = m_gradient.settledAt;
}

// FS2 0x002d0550
// The ROM holds setPlace's body twice here, inline.
// callcheck: contains, contains, contains, draw, drawRise, drawFade, draw

void StageDaylight::draw(float deltaSeconds, sceVif1Packet *packet, StageObject *camera)
{
    if (m_tree == nullptr || camera == nullptr || !m_active)
        return;
    const float *eye = camera->getMatrix()[3];
    if (!m_placed) {
        m_placed = true;
        setPlace(eye);
    }
    if (camera != m_camera)
        setPlace(eye);
    m_camera = camera;

    bool inside = m_tree->contains(eye);
    if (inside != m_inside) {
        m_insideSeconds = 0.0f;
        m_outsideSeconds = 0.0f;
    }
    m_inside = inside;

    if (m_inside) {
        m_insideSeconds += deltaSeconds;
        m_gradient.drawFade(m_insideSeconds, packet);
        m_film.draw(m_insideSeconds, packet);
        return;
    }
    m_outsideSeconds += deltaSeconds;
    JackFilm *film = m_film.film;
    const float fade = m_film.settings.fadeSeconds;
    if (film != nullptr && fade > 0.0f && m_outsideSeconds < fade) {
        const float clear = (float)-(int)film->band / kFilmBrightest;
        film->draw(daylightLerp(m_film.settings.settled, clear, m_outsideSeconds / fade), packet);
    }
    m_gradient.drawRise(m_outsideSeconds, packet);
}
