#include "game/gadgetview.h"

#include <math.h>
#include <string.h>

#include "game/characterinput.h"
#include "game/gamepacket.h"
#include "game/gamesystem.h"
#include "game/mathutil.h"
#include "game/numlock.h"
#include "game/pad.h"
#include "game/stagelight.h"
#include "game/stageobjlist.h"
#include "game/stageparticle.h"
#include "game/stagescene.h"
#include "game/vu0math.h"
#include "rena/rena_2d_texture_tim2.h"
#include "sdk/libgraph.h"
#include "yamipan/rendering/yamipan_drawui.h"

namespace {

// The scene's fade, in seconds and levels: to black and back.
constexpr float kFadeSeconds = 0.5f;
constexpr float kFadeBlack = 1.0f;
constexpr float kFadeClear = 0.0f;
constexpr float kFadeFromHere = -1.0f;

// StageScene::pushView: the gadget's camera over the others.
constexpr int    kGadgetViewPriority = 4;
constexpr u_char kGadgetViewFlag = 1;

// What a gadget's sounds are heard at (StageObject::playNamedHeard2's f12,
// which the host's does not take: game/charactersound.cpp's note).
// constexpr float kGadgetSoundRange = 800.0f;

// The gadget camera's projection distance while in view.
constexpr float kGadgetScrZ = 540.0f;

// -- SigLamp.
const char kSigLampSound[] = "SIGLAMP";         /* data 0x30a690 */
const char kSigLampSpotName[] = "SigLamp_sl";   /* data 0x30a698 */
const char kSigLampPointName[] = "SigLamp_pl";  /* data 0x30a6a8 */

constexpr float kSigLampSpotRadiusSq = 12000000.0f;  // 0x4b371b00
constexpr float kSigLampPointRadiusSq = 174243.0f;   // 0x482a28c0
const float kSigLampPointColor[3] = { 0.78431374f, 0.76862746f, 0.7058824f }; // 200, 196, 180 / 255
constexpr float kSigLampLightX = 5.0f;              // both lights' place on the camera
constexpr int   kSigLampLightPriority = 3;
constexpr int   kSigLampLightMode = 2;              // StageLightMode: unnamed (neither Hard nor Keyed)
// The stick turns the camera from where it stood: up / down 50 degrees about
// x, left / right 60 about z.
constexpr float kSigLampPitch = 0.8726647f;
constexpr float kSigLampYaw = 1.0471976f;
// The beam's ray.
constexpr int   kSigLampRayArg = 1;
constexpr u_int kSigLampRayMask = 5;
constexpr u_int kSigLampRayFlags = 0x2040000;

// The mask: the texture is one quarter of it, mirrored into the other three.
constexpr u_long kSigLampTex1 = 0x60;           // TEX1_1: bilinear both ways
constexpr u_long kSigLampTest = 0x33001;        // TEST_1: alpha never passes, RGB still written; depth always
constexpr u_long kSigLampClamp = 0x5;           // CLAMP_1: clamp u and v
constexpr u_long kSigLampAlpha = 0x44;          // ALPHA_1: (Cs - Cd) * As + Cd
constexpr u_long kSigLampPrim = 0x156;          // PRIM: sprite, textured, blended, UV
constexpr int    kSigLampQuarters = 4;
constexpr int    kSigLampOrigin = 0x7000;       // the frame's corner, 12.4
constexpr int    kSigLampQuarter = 0x1000;      // 256 pixels
constexpr uint32_t kSigLampZFirst = 0xff0fffff;
constexpr uint32_t kSigLampZLast = 0xffffffff;
const uint8_t kSigLampRgbaq[4] = { 0x80, 0x80, 0x80, 0x80 };

// -- The number lock.
const char kNumLockOpenSound[] = "NLOCK_UL";    /* data 0x30a6b8 */
const char kNumLockKeyBone[] = "bn_key";        /* data 0x30a6c8 */
const char kNumLockTurnSound[] = "NLOCK_REV";   /* data 0x30a6d0 */
const char kNumLockEffect[] = "numlock";        /* data 0x30a6e0 */

// The sparkle's place on bn_key: x moves 0.8 a dial (data 0x2fb730, 0x2fb750).
const float kNumLockSparklePos[4] = { -1.2f, 2.5f, 3.0f, 1.0f };
const float kNumLockSparkleRot[4] = { 0.0f, 0.0f, 0.0f, 1.0f };   /* data 0x2fb740 */
constexpr float kNumLockDialStep = 0.8f;
constexpr int   kNumLockLights = 3;

} // namespace

// ──────────────────────────────────────────────────────────────────────
// The body's base.

// FS2 0x002a9940
// callcheck: operator_new[], operator_new[]

GadgetView::GadgetView(int capacity)
    : m_capacity(capacity)
{
    m_objects = new StageObject *[m_capacity];
    m_cameras = new StageCamera *[m_capacity];
    for (int i = 0; i < m_capacity; i++) {
        m_objects[i] = nullptr;
        m_cameras[i] = nullptr;
    }
}

// FS2 0x002a98c0
// callcheck: operator_delete[], operator_delete[], operator_delete

GadgetView::~GadgetView()
{
    delete[] m_objects;
    delete[] m_cameras;
}

int GadgetView::findPair(const char *objectName) const
{
    int i = 0;
    for (; i < m_capacity; i++) {
        if (m_objects[i] != nullptr && strcmp(objectName, m_objects[i]->m_name) == 0)
            break;
    }
    return i;
}

// FS2 0x002a9810
// callcheck: strcmp

StageObject *GadgetView::findObject(const char *objectName)
{
    const int i = findPair(objectName);
    return i < m_capacity ? m_objects[i] : nullptr;
}

// FS2 0x002a9700
// callcheck: find1, find0

bool GadgetView::addPair(const char *objectName, const char *cameraName)
{
    StageObject *object = gStageScene->objects->find1(objectName, 0);
    if (object == nullptr)
        return false;
    StageObject *camera = gStageScene->objects->find0(cameraName, 0);
    if (camera == nullptr)
        return false;
    if (hasObject(objectName) || m_count == m_capacity || m_state != GadgetState_Idle)
        return false;
    int i = 0;
    while (i < m_capacity && m_objects[i] != nullptr)
        i++;
    m_objects[i] = object;
    m_cameras[i] = static_cast<StageCamera *>(camera);
    m_count++;
    return true;
}

// FS2 0x002a9610
// callcheck: strcmp

bool GadgetView::removePair(const char *objectName)
{
    if (m_state != GadgetState_Idle)
        return false;
    const int i = findPair(objectName);
    if (i == m_capacity)
        return false;
    m_objects[i] = nullptr;
    m_cameras[i] = nullptr;
    m_count--;
    return true;
}

// FS2 0x002a9570
// callcheck: strcmp

bool GadgetView::hasObject(const char *objectName)
{
    return findPair(objectName) != m_capacity;
}

// FS2 0x002a94c0
// callcheck: isNotRunning

bool GadgetView::canUse()
{
    if (isActive() || m_count == 0)
        return false;
    int i = 0;
    while (i < m_capacity && !(m_objects[i] != nullptr && m_objects[i]->m_active != 0))
        i++;
    if (i == m_capacity)
        return false;
    return gStageScene->fade->isNotRunning();
}

// FS2 0x002a93d0
// callcheck: strcmp

bool GadgetView::takeMotionText(const char *objectName)
{
    if (!canUse())
        return false;
    const int i = findPair(objectName);
    if (i == m_capacity)
        return false;
    m_object = m_objects[i];
    m_camera = m_cameras[i];
    m_state = GadgetState_Selected;
    return true;
}

// FS2 0x002a93a0

bool GadgetView::takeMotionUser(StageObject *user)
{
    const bool selected = m_state == GadgetState_Selected;
    if (selected) {
        m_userSet = 1;
        m_user = user;
    }
    return selected;
}

// FS2 0x002a9380

bool GadgetView::cancel()
{
    const bool active = m_state != GadgetState_Idle;
    if (active)
        m_cancel = 1;
    return active;
}

void GadgetView::reset()
{
    m_user = nullptr;
    m_object = nullptr;
    m_camera = nullptr;
    m_state = GadgetState_Idle;
}

// FS2 0x002a9050
// callcheck: isFullInMode2, start, isNotRunning, start, start, isFullInMode2, start, start, start

void GadgetView::update()
{
    StageFade *fade = gStageScene->fade;
    switch (m_state) {
    case GadgetState_FadeBack:
        m_cancel = 0;
        if (fade->isFullInMode2()) {
            fade->start(kFadeSeconds, kFadeClear, kFadeFromHere);
            leaveView();
        } else if (fade->isNotRunning()) {
            reset();
        }
        break;
    case GadgetState_Looking:
        if (m_cancel == 0)
            frame();
        if (m_cancel != 0) {
            fade->start(kFadeSeconds, kFadeBlack, kFadeFromHere);
            m_state = GadgetState_FadeBack;
            m_cancel = 0;
        }
        break;
    case GadgetState_FadeOut:
        if (m_cancel == 0) {
            if (fade->isFullInMode2()) {
                fade->start(kFadeSeconds, kFadeClear, kFadeFromHere);
                m_state = GadgetState_Looking;
                enterView();
            }
        } else {
            fade->start(kFadeSeconds, kFadeClear, kFadeFromHere);
            m_state = GadgetState_FadeBack;
            m_cancel = 0;
        }
        break;
    case GadgetState_Selected:
        if (m_cancel == 0) {
            if (m_userSet != 0) {
                fade->start(kFadeSeconds, kFadeBlack, kFadeFromHere);
                m_state = GadgetState_FadeOut;
                m_userSet = 0;
            }
        } else {
            reset();
            m_userSet = 0;
            m_cancel = 0;
        }
        break;
    default:
        break;
    }
}

// FS2 0x002a9300
// callcheck: pushView, setChildrenShown, setChildrenActive

void GadgetView::pushCamera()
{
    gStageScene->pushView(m_camera, kGadgetViewPriority, kGadgetViewFlag);
    m_object->m_model->data->flags &= ~ModelFlag_CastsShadow;
    m_object->m_tree->setChildrenShown(false);
    m_object->m_tree->setChildrenActive(false);
    m_inView = 1;
}

// FS2 0x002a9290
// callcheck: removeView, setChildrenShown, setChildrenActive

void GadgetView::popCamera()
{
    gStageScene->removeView(m_camera);
    m_object->m_tree->setChildrenShown(true);
    m_object->m_tree->setChildrenActive(true);
    m_object->m_model->data->flags |= ModelFlag_CastsShadow;
    m_inView = 0;
}

// ──────────────────────────────────────────────────────────────────────
// The signal lamp.

// Inline in SigLamp's ctor 0x2a7bf0.
// callcheck: heapAllocAligned, GadgetView

SigLampView::SigLampView(Tim2Texture *texture)
    : GadgetView(2), m_texture(texture)
{
}

// FS2 0x002a7b10
// callcheck: ~GadgetView, operator_delete

SigLampView::~SigLampView()
{
}

// The two lights hang under the camera while in view; out of it they hang
// nowhere.
// FS2 0x002a8650
// callcheck: operator_new, SpotLight, setVector, setRadiusSquared, setConeAngle, setPositionXyz, setRotation, addToScene, erase, setTreeScriptFlag, operator_new, PointLight, setVector, setRadiusSquared, setPositionXyz, setRotation, addToScene, erase, setTreeScriptFlag

void SigLampView::prepare()
{
    m_spot = new SpotLight();
    m_spot->setVector(1.0f, 1.0f, 1.0f);
    m_spot->setRadiusSquared(kSigLampSpotRadiusSq);
    m_spot->setConeAngle(0);
    m_spot->m_xform.setPositionXyz(kSigLampLightX, 0.0f, 0.0f);
    m_spot->m_xform.setRotation(0.0f, 0.0f, 0.0f);
    m_spot->addToScene(kSigLampSpotName, true);
    m_spot->detach();
    m_spot->m_limitByMap = 0;
    m_spot->setTreeScriptFlag(false);
    m_spot->m_priority = kSigLampLightPriority;
    m_spot->m_mode = kSigLampLightMode;

    m_point = new PointLight();
    m_point->setVector(kSigLampPointColor[0], kSigLampPointColor[1], kSigLampPointColor[2]);
    m_point->setRadiusSquared(kSigLampPointRadiusSq);
    m_point->m_castsShadow = 0;
    m_point->m_xform.setPositionXyz(kSigLampLightX, 0.0f, 0.0f);
    m_point->m_xform.setRotation(0.0f, 0.0f, 0.0f);
    m_point->addToScene(kSigLampPointName, true);
    m_point->detach();
    m_point->setTreeScriptFlag(false);
    m_point->m_priority = kSigLampLightPriority;
    m_point->m_mode = kSigLampLightMode;
}

// FS2 0x002a8180
// callcheck: pushCamera, setScrZ, erase, erase, pushBack, erase, erase, pushBack, setChildrenActive

void SigLampView::enterView()
{
    pushCamera();
    m_beamLength = m_point->m_xform.position[1];
    memcpy(m_cameraRotation, m_camera->m_xform.rotation, sizeof(m_cameraRotation));
    m_cameraScrZ = m_camera->m_scrZ;
    if (m_cameraScrZ != kGadgetScrZ)
        m_camera->setScrZ(kGadgetScrZ);
    m_spot->setParent(m_camera);
    m_point->setParent(m_camera);
    m_object->m_model->data->shown = 0;
    m_object->m_tree->setChildrenActive(false);
}

// FS2 0x002a8050
// callcheck: popCamera, setScrZ, setTreeScriptFlag, setTreeScriptFlag, erase, erase, setChildrenActive

void SigLampView::leaveView()
{
    popCamera();
    m_point->m_xform.position[1] = m_beamLength;
    memcpy(m_camera->m_xform.rotation, m_cameraRotation, sizeof(m_cameraRotation));
    if (m_cameraScrZ != m_camera->m_scrZ)
        m_camera->setScrZ(m_cameraScrZ);
    m_spot->setTreeScriptFlag(false);
    m_point->setTreeScriptFlag(false);
    m_spot->detach();
    m_point->detach();
    m_object->m_model->data->shown = 1;
    m_object->m_tree->setChildrenActive(true);
}

// The look ends when the user is down, the partner is, the stage is over or
// Squat is pressed. Otherwise Lamp held lights both lights (SIGLAMP as it
// changes), the stick turns the camera from where it stood, and the point
// light goes to where the beam hits, at most as far as it stood.
// FS2 0x002a8380
// callcheck: isGameOver, isCleared, bitMask, bitMask, playNamedHeard2, setTreeScriptFlag, setTreeScriptFlag, addAngle, addAngle, applyMatrix, applyMatrix, raycast

void SigLampView::frame()
{
    StageCharacter *user = static_cast<StageCharacter *>(m_user);
    if (user->m_input == nullptr)
        return;
    CharacterPad *pad = user->m_input->pad();

    bool over = true;
    if (user->m_endurance > 0) {
        StageCharacter *partner = static_cast<StageCharacter *>(gStageScene->partner);
        if (!(partner != nullptr && partner->m_endurance < 1))
            over = false;
    }
    over = over || gStageScene->isGameOver();
    over = over || gStageScene->isCleared();
    if (over || pad->actionPressed(PadAction_Squat)) {
        cancel();
        return;
    }

    const bool lit = pad->actionHeld(PadAction_Lamp);
    if (lit != (m_spot->m_switchedOn != 0))
        m_object->playNamedHeard2(kSigLampSound, m_camera->m_xform.position, nullptr);
    m_spot->setTreeScriptFlag(lit);
    m_point->setTreeScriptFlag(lit);
    m_camera->m_xform.rotation[0] = addAngle(m_cameraRotation[0], pad->m_stick[3] * kSigLampPitch);
    m_camera->m_xform.rotation[2] = addAngle(m_cameraRotation[2], -pad->m_stick[2] * kSigLampYaw);

    sceVu0FVECTOR from = { m_point->m_xform.position[0], 0.0f, m_point->m_xform.position[2], 1.0f };
    sceVu0FVECTOR to = { from[0], m_beamLength, from[2], 1.0f };
    applyMatrix(from, m_camera->getWorldMatrix(), from);
    applyMatrix(to, m_camera->getWorldMatrix(), to);
    sceVu0FVECTOR hit;
    if (gStageScene->raycast(hit, from, to, kSigLampRayArg, kSigLampRayMask, kSigLampRayFlags, nullptr) == nullptr) {
        m_point->m_xform.position[1] = m_beamLength;
        return;
    }
    const float dx = hit[0] - from[0], dy = hit[1] - from[1], dz = hit[2] - from[2];
    m_point->m_xform.position[1] = sqrtf(dz * dz + dx * dx + dy * dy);
}

// The mask over the frame while in view: four sprites of the texture, each a
// quarter of the 512 x 512 window, mirrored about its centre.
// PC: the ROM uploads the texture into its texture area and packs the GIF
// tags; the host binds the texture and hands the same registers and
// vertices to the 2D module, which stretches the window over any aspect.
// FS2 0x002a7ce0
// callcheck: openList, gfxGetTexArea, getList, place, upload, putTex0, sceVif1PkCnt, sceVif1PkAddCode, sceVif1PkOpenDirectCode, sceVif1PkOpenGifTag, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkCloseGifTag, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag

void SigLampView::draw()
{
    if (m_inView == 0)
        return;
    sceVif1Packet *packet = gGameSystem->m_packets.openList(PacketList_Scene2d);
    YamiPan_DrawUiTexture(packet, m_texture);
    YamiPan_DrawUiReg(packet, SCE_GS_TEX1_1, kSigLampTex1);
    const int width = m_texture->m_width * 0x10;
    const int height = m_texture->m_height * 0x10;
    for (int i = 0; i < kSigLampQuarters; i++) {
        YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kSigLampTest);
        YamiPan_DrawUiReg(packet, SCE_GS_CLAMP_1, kSigLampClamp);
        YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kSigLampAlpha);
        const int column = i % 2;
        const int row = i / 2;
        const YamiPanUiVertex corners[2] = {
            { {}, (uint16_t)(column * width), (uint16_t)(row * height),
              (uint16_t)(column * kSigLampQuarter + kSigLampOrigin),
              (uint16_t)(row * kSigLampQuarter + kSigLampOrigin), kSigLampZFirst },
            { {}, (uint16_t)((column == 0) * width), (uint16_t)((row == 0) * height),
              (uint16_t)(column * kSigLampQuarter + kSigLampOrigin + kSigLampQuarter),
              (uint16_t)(row * kSigLampQuarter + kSigLampOrigin + kSigLampQuarter), kSigLampZLast },
        };
        YamiPan_DrawUiPrim(packet, kSigLampPrim, kSigLampRgbaq);
        YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_UV);
        for (const YamiPanUiVertex &corner : corners)
            YamiPan_DrawUiVertex(packet, &corner);
    }
}

// FS2 0x002a7bf0
// callcheck: heapAllocAligned, GadgetView

SigLamp::SigLamp(Tim2Texture *texture)
    : SceneOwnedObject(new SigLampView(texture))
{
}

// ──────────────────────────────────────────────────────────────────────
// The number lock.

// FS2 0x002a88b0
// callcheck: ~GadgetView, operator_delete

NumLockView::~NumLockView()
{
}

// FS2 0x002a8f80
// callcheck: spawn, setEmitting

void NumLockView::prepare()
{
    m_dial = 0;
    m_sparkle = gStageScene->nodeTree->spawn(kNumLockEffect, nullptr, kNumLockSparklePos,
                                             kNumLockSparkleRot, 0, 0);
    if (m_sparkle != nullptr)
        m_sparkle->setEmitting(false);
    m_tried = 0;
    m_recordFlag = 0;
}

// The lock's own light while in view, the rain stopped, the sparkle on its key.
// PC: NumLock::setInView takes the camera in the ROM, and does nothing.
// FS2 0x002a8b60
// callcheck: pushCamera, setInView, setEmitting, find, erase, erase, pushBack, setRainEmitting, setLight, setAmbient

void NumLockView::enterView()
{
    m_tried = 0;
    pushCamera();
    if (m_object->getKind() != StageObject_NumLock)
        return;
    NumLock *lock = static_cast<NumLock *>(m_object);
    lock->setInView(true);
    if (m_sparkle != nullptr) {
        m_sparkle->setEmitting(true);
        m_sparkle->setParent(lock->m_model->boneMap.find(kNumLockKeyBone)->value);
    }
    m_dial = 0;
    gStageScene->effects->setRainEmitting(false);
    for (int i = 0; i < kNumLockLights; i++)
        lock->m_model->data->setLight(i, lock->m_lightDirection, lock->m_lightColor);
    lock->m_model->data->setAmbient(lock->m_lightColor);
}

// Out of the view: a lock set right opens (NLOCK_UL) and goes.
// FS2 0x002a8a30
// callcheck: popCamera, setInView, dialsRight, playNamedHeard2, setActive, setShown, setEmitting, erase, setRainEmitting

void NumLockView::leaveView()
{
    popCamera();
    if (m_object->getKind() != StageObject_NumLock)
        return;
    NumLock *lock = static_cast<NumLock *>(m_object);
    lock->setInView(false);
    if (lock->dialsRight()) {
        lock->playNamedHeard2(kNumLockOpenSound, lock->m_xform.position, nullptr);
        lock->m_tree->setActive(false);
        lock->m_tree->setShown(false);
    }
    if (m_sparkle != nullptr) {
        m_sparkle->setEmitting(false);
        m_sparkle->detach();
    }
    gStageScene->effects->setRainEmitting(true);
    lock->m_model->data->drawMode = 0;
}

// While no dial turns: Squat leaves, Use leaves as a try. Left / right pick
// a dial (0..3), up / down turn it (NLOCK_REV); the sparkle follows the dial.
// FS2 0x002a8d00
// callcheck: dialsTurning, bitMask, bitMask, playNamedHeard2, dialUp, playNamedHeard2, dialDown, setPosition

void NumLockView::frame()
{
    StageCharacter *user = static_cast<StageCharacter *>(m_user);
    if (user->m_input == nullptr)
        return;
    CharacterPad *pad = user->m_input->pad();
    if (m_object->getKind() != StageObject_NumLock) {
        cancel();
        return;
    }
    NumLock *lock = static_cast<NumLock *>(m_object);
    if (!lock->dialsTurning()) {
        if (pad->actionPressed(PadAction_Squat)) {
            cancel();
            return;
        }
        if (pad->actionPressed(PadAction_Use)) {
            cancel();
            m_tried = 1;
            return;
        }
    }
    if ((pad->m_pressed & SCE_PADLleft) != 0) {
        lock->playNamedHeard2(kNumLockTurnSound, lock->m_xform.position, nullptr);
        lock->dialUp(m_dial);
    }
    if ((pad->m_pressed & SCE_PADLright) != 0) {
        lock->playNamedHeard2(kNumLockTurnSound, lock->m_xform.position, nullptr);
        lock->dialDown(m_dial);
    }
    if ((pad->m_pressed & SCE_PADLup) != 0 && m_dial > 0)
        m_dial--;
    if ((pad->m_pressed & SCE_PADLdown) != 0 && m_dial < kNumLockDials - 1)
        m_dial++;
    if (m_sparkle != nullptr) {
        const float pos[3] = {
            (kNumLockSparklePos[0] + 0.0f) - (float)m_dial * kNumLockDialStep,
            kNumLockSparklePos[1], kNumLockSparklePos[2],
        };
        m_sparkle->m_xform.setPosition(pos);
    }
}

// FS2 0x002a89b0
// callcheck: heapAllocAligned, GadgetView

NumLockGadget::NumLockGadget()
    : SceneOwnedObject(new NumLockView())
{
}

// ──────────────────────────────────────────────────────────────────────
// The handle.

// FS2 0x002a9be0
// callcheck: operator_delete (the deleting dtor's own delete of the handle: the compiler's here; the body goes through its slot 0)

SceneOwnedObject::~SceneOwnedObject()
{
    delete m_view;
}

// FS2 0x002a8920

bool SceneOwnedObject::getRecordFlag31()
{
    return static_cast<NumLockView *>(m_view)->m_recordFlag != 0;
}

// FS2 0x002a8910

void SceneOwnedObject::setRecordFlag31(bool on)
{
    static_cast<NumLockView *>(m_view)->m_recordFlag = on;
}

// FS2 0x002a8930

bool SceneOwnedObject::wasTried()
{
    return static_cast<NumLockView *>(m_view)->m_tried != 0;
}

// FS2 0x002a8940
// callcheck: findObject, dialsRight

bool SceneOwnedObject::isLockOpen(const char *objectName)
{
    StageObject *object = m_view->findObject(objectName);
    if (object == nullptr || object->getKind() != StageObject_NumLock)
        return false;
    return static_cast<NumLock *>(object)->dialsRight();
}
