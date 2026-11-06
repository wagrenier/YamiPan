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
#include "game/playermotion.h"
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

// Saved over a stage's restart: the bindings' character ids.
int sSavedBinds[kJackBinds];                // bss 0x598b20

// The film's rate and sizes, by stage. layout: host
struct JackTuning
{
    u_int rate;
    float _68;
    float _6c;
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

// in = in * R(angles). Inline here; FS2 0x00135990.
// callcheck: FUN_00135a90 (rotationMatrixXyz)
void rotateByAngles(sceVu0FMATRIX out, const sceVu0FVECTOR *in, const sceVu0FVECTOR angles)
{
    sceVu0FMATRIX turn;
    rotationMatrixXyz(turn, angles);
    multiplyMatrix(out, in, turn);
}

} // namespace

// ──────────────────────────────────────────────────────────────────────
// The camera and the fill.

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
    m_film.rate = tuning.rate;
    m_film._68 = tuning._68;
    m_film._6c = tuning._6c;
    // PC: stage 0x1b also builds an object at +0xb4 (0x2d31d0, 15 by 15). It
    // is the draw's, which is not written.
}

// FS2 0x00203be0
// callcheck: operator_delete, FUN_00206f00, FUN_00206f00, operator_delete, operator_delete, operator_delete

SightJack::~SightJack()
{
    if (m_fill != nullptr)
        m_fill->destroy();
    delete m_fleckTexture;
    if (m_flecks != nullptr)
        demoFleckDestroy(m_flecks);
    for (Tim2Texture *frame : m_film.frames)
        delete frame;
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
            gStageScene->model->setPlace(kNowhere);
        } else {
            gStageScene->model->setPlace(m_camera->getWorldMatrix()[3]);
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

// callcheck: demoFilmDraw, demoFleckStep, demoFleckDraw

void SightJack::drawFilm(float filmLevel, sceVif1Packet *packet)
{
    demoFilmDraw(filmLevel, &m_film, packet);
    if (m_flecks != nullptr) {
        demoFleckStep(m_flecks);
        demoFleckDraw(m_fill != nullptr ? m_fill->m_level : 0.0f, m_flecks, packet);
    }
}

// FS2 0x00201a40
// callcheck: openList, gsFillRect, gfxGetTexArea, getList, demoFilmDraw, demoFleckStep, demoFleckDraw, FUN_001ffcd0, closeList (FUN_001ffcd0, the bind's arrow, is not drawn)

void SightJack::drawNobody(float filmLevel)
{
    GamePacketManager *packets = &gGameSystem->m_packets;
    sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
    fillBlack(packet);
    m_fill->m_level = 1.0f;
    m_fill->draw(packet, scene2dTexList());
    drawFilm(filmLevel, packet);
    packets->closeList();
}

// FS2 0x002016d0
// callcheck: openList, gsFillRect, gfxGetTexArea, getList, demoFilmDraw, demoFleckStep, demoFleckDraw, FUN_001ffcd0, closeList (FUN_001ffcd0, the bind's arrow, is not drawn)

void SightJack::drawCorpse(float filmLevel)
{
    GamePacketManager *packets = &gGameSystem->m_packets;
    sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
    fillBlack(packet);
    m_fill->draw(packet, scene2dTexList());
    drawFilm(filmLevel, packet);
    packets->closeList();
}

// FS2 0x00201bc0
// PC: the film, the grain and the flecks over the view. The markers 0x205f50,
// 0x201520, the bind's arrow 0x1ffcd0 and all of list 8 (the frame 0x2cfb20,
// 0x1ff010, the signal 0x2cf880 and 0x2d10f0) are not drawn yet.
// callcheck: openList, demoFilmDraw, gfxGetTexArea, getList, demoFleckStep, demoFleckDraw, closeList (the unwritten ones above)

void SightJack::drawLive(float filmLevel)
{
    GamePacketManager *packets = &gGameSystem->m_packets;
    sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
    demoFilmDraw(filmLevel, &m_film, packet);
    m_fill->draw(packet, scene2dTexList());
    if (m_flecks != nullptr) {
        demoFleckStep(m_flecks);
        demoFleckDraw(m_fill->m_level, m_flecks, packet);
    }
    packets->closeList();
}

// FS2 0x002024c0
// The frame's gate. PC: the warning's pass 0x201840, the markers 0x201380 and
// 0x205f50 and the frame 0x2cfb20 are not drawn yet, so a warning and a game
// over show their view alone (drawLive has the rest of the list).
// callcheck: vecDistance, openList, demoFilmDraw, demoFleckStep, demoFleckDraw, closeList, openList, gfxGetTexArea, getList, FUN_001518a0, gfxGetTexArea, getList, FUN_001518a0, closeList, getAltSoundFlag, getLifeState, FUN_00215370, FUN_00201bc0, FUN_00201a40, FUN_002016d0 (and those not drawn: FUN_002cfb20, getTopView, FUN_00205f50, canBeJacked, FUN_00201840, FUN_001ffcd0, FUN_001ff010, FUN_00201380)

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
        return;
    }
    if (m_warner != nullptr)
        return;
    if (m_visionFade > 0) {
        sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
        const u_int alpha = (u_int)(m_visionFade * kVisionFadeScale / 8) << 24;
        m_fill->drawGrain(packet, scene2dTexList(), alpha | kVisionTintA);
        m_fill->drawGrain(packet, scene2dTexList(), alpha | kVisionTintB);
        packets->closeList();
        m_visionFade--;
        return;
    }
    if (!m_running)
        return;

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
        packets->closeList();
    } else {
        m_visionFrames--;
        sceVif1Packet *packet = packets->openList(PacketList_Scene2d);
        fillBlack(packet);
        m_fill->drawGrain(packet, scene2dTexList(), kVisionWaitTintA);
        m_fill->drawGrain(packet, scene2dTexList(), kVisionWaitTintB);
        drawFilm(filmLevel, packet);
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
// A jump to its body, FUN_001fed30.
// callcheck: FUN_001fed30

void StageParty::restore()
{
    for (int i = 0; i < kJackBinds; i++) {
        if (sSavedBinds[i] == -1) {
            m_jack->m_binds[i] = nullptr;
            continue;
        }
        StageObjArray &list = gStageScene->objects->m_lists[StageObjGroup_Characters];
        for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
            if (*slot != nullptr && sSavedBinds[i] == (*slot)->m_id)
                m_jack->m_binds[i] = *slot;
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

// A jump to its body, FUN_001fef50.
// FS2 0x001fe7a0
// callcheck: FUN_001fef50

void StageParty::setVisionTarget(StageObject *object)
{
    SightJack *jack = m_jack;
    if (jack->m_tuned == jack->m_vision)
        jack->m_tuned = nullptr;
    if (jack->m_kept == jack->m_vision)
        jack->m_kept = nullptr;
    for (int i = 0; i < kJackBinds; i++) {
        if (jack->m_binds[i] == jack->m_vision)
            jack->m_binds[i] = nullptr;
    }
    if (object == nullptr) {
        jack->m_visionShowCount = 0;
        jack->m_visionHideCount = 0;
    }
    jack->m_vision = object;
    if (object != nullptr)
        jack->m_visionFade = 8;
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
// callcheck: operator_new, Tim2Texture, operator_new, FUN_002a1240

void StageParty::setImage(const void *image)
{
    SightJack *jack = m_jack;
    if (jack->m_flecks != nullptr)
        demoFleckDestroy(jack->m_flecks);
    delete jack->m_fleckTexture;
    jack->m_fleckTexture = new Tim2Texture(image);
    jack->m_flecks = demoFleckCreate(jack->m_fleckTexture);
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

// FS2 0x001fea40 (the film), 0x001fea30 (the flecks): inline in the event's code.
void demoFilmTake(StageParty *party, void **film, void **flecks)
{
    *film = &party->m_jack->m_film;
    *flecks = party->m_jack->m_flecks;
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
    if (gStageScene->keepList != nullptr)
        gStageScene->keepList->addNamed(kSoundPetit, 0);
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
