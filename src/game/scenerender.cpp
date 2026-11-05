/* ==========================================================================
 *  game/scenerender.cpp
 *
 *  SceneRender, the stage's lighting, 0x1f6ac0..0x1fa900 (game/scenerender.h);
 *  rena's light records 0x2e04e0..0x2e0840 are written here too. The file name is the port's (no source string).
 *
 *  Left out: the three DrawSysFog setters inside the range are drawsys.h's
 *  inlines. rena/rena_draw.cpp reads the point and spot records for the draw
 *  module (VU1 0x545, 0x2eb).
 * ======================================================================== */
#include "game/playermotion.h"
#include "game/scenerender.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/areagrid.h"
#include "game/drawsys.h"
#include "game/fileloader.h"
#include "game/gfxtarget.h"
#include "game/gsblockmap.h"
#include "game/gsutil.h"
#include "game/mathutil.h"
#include "game/stageitem.h"
#include "game/stagelight.h"
#include "game/stageloop.h"
#include "game/stageobjlist.h"
#include "game/stagescene.h"
#include "game/vu0math.h"

// PC: the ROM's libc strcasecmp (0x127ea0); MSVC has no such name.
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

static constexpr float kDegToRad = 0.017453292f;

// The transition key every set gets from addSetKeys: the cameras' range and
// 30 frames of fade.
static constexpr float kDefaultCameraRange = 1600.0f;
static constexpr float kDefaultFadeFrames = 30.0f;

// update's aim from the lamp: 200 ahead, kept 10 off the wall it hits.
static constexpr float kLampReach = 200.0f;
static constexpr float kLampWallGap = -10.0f;
static constexpr u_int kLampRayMask = 0xfc2600f;
static constexpr u_int kLampRayFlags = 0xc0000;
// The player's lamp eases to its place (StageItemE::getLightMatrix).
static constexpr float kLampEaseRate = 0.005f;
static constexpr float kLampEaseStep = 20.0f;
// updateLights' test between two characters' heads.
static constexpr u_int kHeadRayMask = 0xe42600f;
static constexpr u_int kHeadRayFlags = 0x40800;
// What a light's priority is when update pulls it towards the view.
static constexpr int kHeldLightPriority = 4;
// A shadow light weaker than this counts as none.
static constexpr float kMinShadowWeight = 0.3f;
// A model's shadow direction: how much of the new one a frame takes, and
// the limits on a stored one's size. The ROM's default is (0.1, 0, 1, 1).
static constexpr float kShadowDirectionEase = 0.7f;
static constexpr float kShadowDirectionMin = 0.1f;
static constexpr float kShadowDirectionMax = 1e6f;

// setup's own lights.
static const char kDemoPlayerLight[] = "scr_demo_pl";   /* data 0x305af8 */
static const char kPlayerLight[] = "yami_pl";           /* data 0x305b08 */
static const char kDemoLight[] = "demo_pl";             /* data 0x305b10 */
static const char kBonePrefix[] = "bn_%s";              /* data 0x305b18 */
static const char kNullEffectLight[] = "ENV_NULL";      /* data 0x305b20 */
static const char kSpotTextureExt[] = ".tm2";           /* data 0x305b30 */
static const char kHeadBone[] = "bn_head";              /* data 0x303178 */
static constexpr int kDemoPlayerPriority = 5;
static constexpr int kPlayerLightPriority = 4;

// The texture area blocks of the spotlight (setSpotlightTexture).
static constexpr int kSpotlightBlocks = 0xa0;

// The ROM's array walks over the active objects (the list's inline).
static StageObject **nextActive(StageObject **slot)
{
    while (slot != nullptr && !(*slot)->m_active)
        slot = StageObjArray::next(slot);
    return slot;
}

template <class Fn> void SceneLightSet::forEachPlayed(Fn fn)
{
    for (int i = 0; i < kSceneLights; i++) {
        fn(direction[i]);
        fn(color[i]);
    }
    fn(ambient);
    for (int i = 0; i < kSceneLights; i++) {
        fn(modelDirection[i]);
        fn(modelColor[i]);
        fn(modelColorSb[i]);
    }
    fn(modelAmbient);
    fn(modelAmbientSb);
    fn(fog);
    fn(fogColor);
    for (int i = 0; i < kSceneFarFogs; i++) {
        fn(farFog[i]);
        fn(farFogColor[i]);
    }
    fn(clearColor);
    fn(transition);
}

template <class Fn> void SceneLightSet::forEachTrack(Fn fn)
{
    forEachPlayed(fn);
    for (int i = 0; i < kSceneLights; i++) {
        fn(angles[i]);
        fn(modelAngles[i]);
    }
}

// A fade starts from where it is; at once when snap.
static void startFade(VectorFade &fade, int frames, bool snap)
{
    fade.setFrom(fade.value);
    fade.frames = frames;
    fade.frame = snap ? frames : 0;
}

// The rena records. Their other slots (0x2e00c0.., 0x2e05f0..) upload them
// to VU1 and wait for the spotlight.
// FS2 0x002e0730, 0x002e04e0: the shared lines
// callcheck: sceVu0UnitMatrix

RenaLight::RenaLight()
    : active(0), color{1.0f, 1.0f, 1.0f, 0.0f}, shade{}
{
    sceVu0UnitMatrix(matrix);
}

// FS2 0x002e0810
// callcheck: FUN_0010b9e0 (the matrix copy, memcpy here)

void RenaLight::setMatrix(const sceVu0FVECTOR *from)
{
    memcpy(matrix, from, sizeof(matrix));
}

// FS2 0x002e07c0
void RenaLight::setColor(const sceVu0FVECTOR from)
{
    memcpy(color, from, sizeof(color));
}

// FS2 0x002e07b0
void RenaLight::setShade(const sceVu0FVECTOR from)
{
    memcpy(shade, from, sizeof(shade));
}

// FS2 0x002e0730
// callcheck: sceVu0UnitMatrix (in the base)

RenaPointLight::RenaPointLight()
    : radius(1200.0f), weight(4.0f)
{
}

// FS2 0x002e04e0
// callcheck: sceVu0UnitMatrix (in the base)

RenaSpotLight::RenaSpotLight()
    : radius(1200.0f), reach(1200.0f), tangent(0.5f), angle(0.25268f), intensity(1.0f),
      _90{0.8f, 3.0f, 4.0f, -0.13f}
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001f6ac0
// callcheck: addBox

void SceneRender::addSbBox(AreaBox *box)
{
    m_sbGrid->addBox(box);
}

// FS2 0x001f6ad0
// callcheck: addBox

void SceneRender::addCbBox(AreaBox *box)
{
    m_cbGrid->addBox(box);
}

// FS2 0x001f6ae0
// callcheck: none

void SceneRender::setSetLoop(int set, bool loop)
{
    m_sets[set].forEachTrack([loop](VectorTrack &track) { track.loop = loop; });
}

// FS2 0x001f6c30
// callcheck: operator_new, VectorKey, FUN_0029a700, addKey (one for each track; forEachTrack holds the repeats)

void SceneRender::addSetKeys(int set, int time)
{
    SceneLightSet &tracks = m_sets[set];
    VectorTrack *transition = &tracks.transition;
    tracks.forEachTrack([&](VectorTrack &track) {
        if (&track == transition) {
            const float defaults[4] = { kDefaultCameraRange, kDefaultFadeFrames, kDefaultFadeFrames, 0.0f };
            track.addKey(new VectorKey(defaults, 0), time, true);
        } else {
            track.addKey(new VectorKey(0), time, true);
        }
    });
}

// A direction key is (0, 1, 0) turned by the angles' key, in degrees.
static void directionFromAngles(VectorTrack &angles, VectorTrack &direction, int key)
{
    const float *turn = angles.keyAt(key)->value;
    const float x = normalizeAngle(turn[0] * kDegToRad);
    const float y = normalizeAngle(turn[1] * kDegToRad);
    const float z = normalizeAngle(turn[2] * kDegToRad);

    sceVu0FMATRIX matrix;
    identityMatrix(matrix);
    rotateMatrixY(y, matrix, matrix);
    rotateMatrixX(x, matrix, matrix);
    rotateMatrixZ(z, matrix, matrix);

    float *out = direction.keyAt(key)->value;
    out[0] = 0.0f;
    out[1] = 1.0f;
    out[2] = 0.0f;
    out[3] = 0.0f;
    applyMatrix(out, matrix, out);
}

// FS2 0x001f7020
// callcheck: keyAt, normalizeAngle, keyAt, normalizeAngle, keyAt, normalizeAngle, identityMatrix, rotateMatrixY, rotateMatrixX, rotateMatrixZ, keyAt, keyAt, keyAt, keyAt, keyAt, keyAt, applyMatrix (directionFromAngles)

void SceneRender::updateModelDirection(int set, int light, int key)
{
    directionFromAngles(m_sets[set].modelAngles[light], m_sets[set].modelDirection[light], key);
}

// FS2 0x001f71d0
// callcheck: keyAt, normalizeAngle, keyAt, normalizeAngle, keyAt, normalizeAngle, identityMatrix, rotateMatrixY, rotateMatrixX, rotateMatrixZ, keyAt, keyAt, keyAt, keyAt, keyAt, keyAt, applyMatrix (directionFromAngles)

void SceneRender::updateDirection(int set, int light, int key)
{
    directionFromAngles(m_sets[set].angles[light], m_sets[set].direction[light], key);
}

// FS2 0x001f7380
// callcheck: gfxSetClearColor

void SceneRender::applyClearColor()
{
    gfxSetClearColor(m_clearEnabled, (int)m_clearColor[0] & 0xff, (int)m_clearColor[1] & 0xff,
                     (int)m_clearColor[2] & 0xff, 0);
}

// The point light's record.
void SceneRender::updatePointLight(StageLight *point, bool wasActive)
{
    sceVu0FVECTOR shadeColor;

    setColor(point->m_color);
    scaleVec3(m_shadeScale, shadeColor, point->m_color);
    setShade(shadeColor);
    radius = point->m_radius;
    weight = point->getWeight();

    if (point->m_mode == StageLight_Hard && point->m_shadowOwner != nullptr) {
        sceVu0FMATRIX lamp;
        point->m_shadowOwner->m_lampItem->getLightMatrix(lamp);
        if (!m_snap && point == m_lastPoint && wasActive)
            approachVec3(kLampEaseRate, kLampEaseStep, lamp[3], getMatrix()[3], lamp[3]);
        setMatrix(lamp);
    } else {
        setMatrix(point->getWorldMatrix());
    }
}

// The spot's record.
void SceneRender::updateSpotLight(SpotLight *spot)
{
    m_spot.radius = spot->m_radius;
    m_spot.reach = spot->m_reach;
    m_spot.angle = (float)(spot->m_coneDegrees * 2) * kDegToRad;
    m_spot.tangent = tanf(m_spot.angle);
    m_spot.setMatrix(spot->getWorldMatrix());
    drawSysSetSpotlightTexture(spot->m_texture != nullptr ? spot->m_texture : m_spotlightTexture,
                               m_spotlightBlock);
    m_spot.intensity = (float)spot->m_intensityPercent / 100.0f;
}

// FS2 0x001f73c0
// callcheck: step, viewLinkedObject, getLampMatrix, applyMatrix, raycast, vecDirection, scaleVec4, contains, getTopView, setFrom, setTo, isMode1, setGearLightOrder, setValue2b0, setActive, setShown, getCharacterClass, erase, pushBack, isLit, vecDistance, getWeight, getLightMatrix, setMatrix, setColor, setShade, scaleVec3, approachVec3, tanf, drawSysSetSpotlightTexture, drawSysCurrent, drawSysGetFog, setRange (updatePointLight and updateSpotLight hold the records; setParent is the erases and the push)

void SceneRender::update()
{
    for (int set = 0; set < kSceneLightSets; set++)
        m_sets[set].forEachPlayed([](VectorTrack &track) { track.step(); });

    StageScene *scene = gStageScene;
    StageObject *linked = viewLinkedObject();

    if (m_cbGrid->_238) {
        int set = 0;
        if (m_forcedSet < 0) {
            AreaBox *box;
            if (linked == nullptr) {
                box = m_cbGrid->contains(scene->getTopView()->getWorldMatrix()[3]);
            } else {
                // Where the player's lamp points, kept off the wall.
                sceVu0FMATRIX lamp;
                sceVu0FVECTOR hit;
                sceVu0FVECTOR aim = { 0.0f, kLampReach, 0.0f, 1.0f };
                linked->m_motion->getLampMatrix(lamp);
                applyMatrix(aim, lamp, aim);
                if (scene->raycast(hit, lamp[3], aim, 0, kLampRayMask, kLampRayFlags, nullptr) != nullptr) {
                    sceVu0FVECTOR back;
                    vecDirection(back, lamp[3], hit);
                    scaleVec4(kLampWallGap, back, back);
                    for (int lane = 0; lane < 4; lane++)
                        aim[lane] = hit[lane] - back[lane];
                }
                box = m_cbGrid->contains(aim);
            }
            if (box != nullptr)
                set = static_cast<CbAreaBox *>(box)->m_number;
        } else {
            set = m_forcedSet;
        }

        if (m_snap || set != m_set) {
            const bool snap = m_snap || m_set < 0;
            const int frames = (int)m_sets[set].transition.value[SceneTransition_Frames];
            for (int i = 0; i < kSceneLights; i++) {
                startFade(m_fadeDirection[i], frames, snap);
                startFade(m_fadeColor[i], frames, snap);
            }
            startFade(m_fadeAmbient, frames, snap);
            startFade(m_fadeFog, frames, snap);
            startFade(m_fadeFogColor, frames, snap);
            for (int i = 0; i < kSceneFarFogs; i++) {
                startFade(m_fadeFarFog[i], frames, snap);
                startFade(m_fadeFarFogColor[i], frames, snap);
            }
            startFade(m_fadeClearColor, frames, snap);
            startFade(m_fadeTransition, frames, snap);
            m_set = set;
        }

        SceneLightSet &tracks = m_sets[m_set];
        for (int i = 0; i < kSceneLights; i++) {
            m_fadeDirection[i].setTo(tracks.direction[i].value);
            m_fadeColor[i].setTo(tracks.color[i].value);
        }
        m_fadeAmbient.setTo(tracks.ambient.value);
        m_fadeFog.setTo(tracks.fog.value);
        m_fadeFogColor.setTo(tracks.fogColor.value);
        for (int i = 0; i < kSceneFarFogs; i++) {
            m_fadeFarFog[i].setTo(tracks.farFog[i].value);
            m_fadeFarFogColor[i].setTo(tracks.farFogColor[i].value);
        }
        m_fadeClearColor.setTo(tracks.clearColor.value);
        m_fadeTransition.setTo(tracks.transition.value);

        for (int i = 0; i < kSceneLights; i++) {
            m_fadeDirection[i].step();
            m_fadeColor[i].step();
        }
        m_fadeAmbient.step();
        m_fadeFog.step();
        m_fadeFogColor.step();
        for (int i = 0; i < kSceneFarFogs; i++) {
            m_fadeFarFog[i].step();
            m_fadeFarFogColor[i].step();
        }
        m_fadeClearColor.step();
        m_fadeTransition.step();

        for (int i = 0; i < kSceneLights; i++) {
            memcpy(m_direction[i], m_fadeDirection[i].value, sizeof(sceVu0FVECTOR));
            memcpy(m_lightColor[i], m_fadeColor[i].value, sizeof(sceVu0FVECTOR));
        }
        memcpy(m_ambient, m_fadeAmbient.value, sizeof(sceVu0FVECTOR));
        memcpy(m_fog, m_fadeFog.value, sizeof(sceVu0FVECTOR));
        memcpy(m_fogColor, m_fadeFogColor.value, sizeof(sceVu0FVECTOR));
        for (int i = 0; i < kSceneFarFogs; i++) {
            memcpy(m_farFog[i], m_fadeFarFog[i].value, sizeof(sceVu0FVECTOR));
            memcpy(m_farFogColor[i], m_fadeFarFogColor[i].value, sizeof(sceVu0FVECTOR));
        }
        memcpy(m_clearColor, m_fadeClearColor.value, sizeof(sceVu0FVECTOR));
        memcpy(m_transition, m_fadeTransition.value, sizeof(sceVu0FVECTOR));
    }

    // What the player holds outranks the map's lights.
    StageObject *view = viewLinkedObject();
    if (view != nullptr && scene->getTopView()->getKind() != StageObject_Kind9) {
        StagePlayer *player = static_cast<StagePlayer *>(view);
        if (player->isMode1()) {
            player->setGearLightOrder(kHeldLightPriority);
        } else if (view->m_lampItem != nullptr) {
            view->m_lampItem->m_lamp->setValue2b0(kHeldLightPriority);
        }
    }

    // yami_pl hangs under a character of class 8, while one is in view.
    if (m_playerLight != nullptr) {
        StageObject *owner = viewLinkedObject();
        m_playerLight->m_tree->setActive(false);
        m_playerLight->m_tree->setShown(false);
        if (owner != nullptr && scene->getTopView()->getKind() != StageObject_Kind9
            && (owner->getCharacterClass() & 8) != 0) {
            m_playerLight->setParent(owner);
            m_playerLight->m_tree->setActive(true);
            m_playerLight->m_tree->setShown(true);
        }
    }

    // The nearest of the highest priority of each kind; the first of a
    // priority is measured from the view, the others from the character.
    StageLight *point = nullptr;
    SpotLight *spot = nullptr;
    float pointDistance = 0.0f;
    float spotDistance = 0.0f;
    const float *topPosition = nullptr;
    StageObjArray &lights = scene->objects->m_lists[StageObjGroup_5];
    for (StageObject **slot = nextActive(lights.begin()); slot != nullptr;
         slot = nextActive(StageObjArray::next(slot))) {
        StageLight *light = static_cast<StageLight *>(*slot);
        if (light->m_priority == -1 || !light->isLit())
            continue;
        topPosition = scene->getTopView()->getWorldMatrix()[3];
        const float *from = linked != nullptr ? linked->getWorldMatrix()[3] : topPosition;
        const float *lightPosition = light->getWorldMatrix()[3];

        if (light->getKind() == StageObject_PointLight) {
            if (point == nullptr || point->m_priority < light->m_priority) {
                pointDistance = vecDistance(lightPosition, topPosition);
                point = light;
            } else if (point->m_priority == light->m_priority) {
                const float distance = vecDistance(lightPosition, from);
                if (distance < pointDistance) {
                    pointDistance = distance;
                    point = light;
                }
            }
        }
        if (light->getKind() == StageObject_SpotLight) {
            if (spot == nullptr || spot->m_priority < light->m_priority) {
                spotDistance = vecDistance(lightPosition, topPosition);
                spot = static_cast<SpotLight *>(light);
            } else if (spot->m_priority == light->m_priority) {
                const float distance = vecDistance(lightPosition, from);
                if (distance < spotDistance) {
                    spotDistance = distance;
                    spot = static_cast<SpotLight *>(light);
                }
            }
        }
    }

    if (point == nullptr) {
        setActive(false);
    } else {
        const bool wasActive = isActive();
        setActive(point->m_active && point->isLit());
        if (isActive())
            updatePointLight(point, wasActive);
    }
    m_lastPoint = point;

    if (spot == nullptr) {
        m_spot.setActive(false);
    } else {
        m_spot.setActive(spot->m_active && spot->isLit());
        updateSpotLight(spot);
    }

    DrawSysLights *record = drawSysCurrent()->lights;
    for (int i = 0; i < kSceneLights; i++) {
        record->getLight(i)->setDirection(m_direction[i]);
        record->getLight(i)->setColor(m_lightColor[i]);
    }
    record->getAmbient()->setColor(m_ambient);

    DrawSysFog *fog = record->getFog();
    fog->setEnabled(m_fogEnabled);
    fog->setNear(m_fog[0]);
    fog->setFar(m_fog[1] + scene->getTopView()->m_range2);
    fog->setNearValue(m_fog[2]);
    fog->setFarValue(m_fog[3]);
    fog->setColor(m_fogColor);

    for (int i = 0; i < kSceneFarFogs; i++) {
        DrawSysFog *far = drawSysGetFog(i);
        far->setEnabled(m_fogEnabled);
        far->setNear(m_farFog[i][0]);
        far->setFar(m_farFog[i][1]);
        far->setNearValue(m_farFog[i][2]);
        far->setFarValue(m_farFog[i][3]);
        far->setColor(m_farFogColor[i]);
    }

    StageObjArray &cameras = scene->objects->m_lists[StageObjGroup_0];
    for (StageObject **slot = cameras.begin(); slot != nullptr; slot = StageObjArray::next(slot))
        static_cast<StageCamera *>(*slot)->setRange(m_transition[SceneTransition_CameraRange]);
}

// Which light a model gets, by weight.
// layout: host (locals of the ROM's frame)
struct ModelLightPick
{
    float       weight;
    StageLight *light;
};

// One object: its set and box, the lights around it, what it throws a
// shadow from. In the ROM all of it is updateLights' loop body.
void SceneRender::lightObject(StageObject *object)
{
    StageModel *model = object->m_model;
    if (model == nullptr || !object->getShownState() || !model->m_lit)
        return;

    StageNodeBase *place = model->m_lightProxy != nullptr ? model->m_lightProxy : static_cast<StageNodeBase *>(object);
    sceVu0FVECTOR pos;
    memcpy(pos, place->getWorldMatrix()[3], sizeof(pos));

    if (model->data != nullptr && m_cbGrid->_238) {
        int set = 0;
        if (m_forcedSet < 0) {
            AreaBox *box = m_cbGrid->contains(pos);
            if (box != nullptr)
                set = static_cast<CbAreaBox *>(box)->m_number;
        } else {
            set = m_forcedSet;
        }
        const bool inSb = m_sbGrid->contains(pos) != nullptr;

        if (m_snap || set != model->m_lightSet || inSb != (model->m_inSbBox != 0)) {
            const bool snap = m_snap || model->m_lightSet < 0;
            const float *transition = m_sets[set].transition.value;
            const int frames = (int)transition[inSb ? SceneTransition_SbFrames : SceneTransition_Frames];
            for (int i = 0; i < kSceneLights; i++) {
                if (snap)
                    object->m_modelLights[i] = nullptr;
                startFade(model->m_lightFades[i], frames, snap);
                startFade(model->m_lightFades[kModelFadeColor + i], frames, snap);
            }
            startFade(model->m_lightFades[kModelFadeAmbient], frames, snap);
            model->m_lightSet = set;
            model->m_inSbBox = inSb;
        }
    }
    if (model->data != nullptr)
        model->data->drawMode = 0;

    // [0] what light 0 will be, [1] the strongest ENV_ light, [2] the
    // strongest other.
    ModelLightPick picks[kSceneLights] = {};
    StageLight *anyLight = nullptr;     // the ROM's weight here is never stored: the last that reaches
    ModelLightPick shadow = {};

    StageObjArray &lights = gStageScene->objects->m_lists[StageObjGroup_5];
    for (StageObject **slot = nextActive(lights.begin()); slot != nullptr;
         slot = nextActive(StageObjArray::next(slot))) {
        StageLight *light = static_cast<StageLight *>(*slot);
        if (!light->m_usable)
            continue;
        if ((light->m_envLight || light->m_yamLight)
            && !(boxContains(light->m_bounds[0], light->m_bounds[1], pos) && light->inBoxes(pos)))
            continue;

        if (!light->m_yamLight) {
            const float weight = light->lightAt(pos);
            ModelLightPick &pick = picks[light->m_envLight ? 1 : 2];
            if (0.0f < weight && pick.weight < weight) {
                pick.weight = weight;
                pick.light = light;
            }
            if (0.0f < weight)
                anyLight = light;
        }

        if (object->getKind() != StageObject_Character)
            continue;
        const float weight = light->shadowAt(pos);
        if (!(0.0f < weight && shadow.weight < weight))
            continue;
        StageObject *owner = light->m_shadowOwner;
        if (owner == nullptr) {
            shadow.weight = weight;
            shadow.light = light;
        } else if ((object->followsPlayerLight() || object->isYriOrKne()) && owner->isPlayer()) {
            const sceVu0FVECTOR *ownerHead = owner->m_model->boneMap.find(kHeadBone)->value->getMatrix();
            const sceVu0FVECTOR *head = object->m_model->boneMap.find(kHeadBone)->value->getMatrix();
            if (!gStageScene->segmentHitsAny(head[3], ownerHead[3], 0, kHeadRayMask, kHeadRayFlags, owner->m_gear)) {
                shadow.weight = weight;
                shadow.light = light;
            }
        }
    }

    if (object->getKind() == StageObject_Character) {
        if (picks[1].light != nullptr && picks[1].light->m_lightsModels)
            picks[0] = picks[1];
    } else if (picks[2].light != nullptr && picks[2].light->m_lightsModels) {
        picks[0] = picks[2];
    } else if (picks[1].light != nullptr && picks[1].light->m_lightsModels) {
        picks[0] = picks[1];
    }

    for (int i = 0; i < kSceneLights; i++) {
        const float weight = picks[i].weight;
        StageLight *light = picks[i].light;
        StageLight *last = object->m_modelLights[i];

        if (model->data != nullptr) {
            VectorFade &direction = model->m_lightFades[i];
            VectorFade &color = model->m_lightFades[kModelFadeColor + i];
            VectorFade &ambient = model->m_lightFades[kModelFadeAmbient];
            sceVu0FVECTOR ambientTo;
            const SceneLightSet &tracks = m_sets[model->m_lightSet];

            // Light gone or changed: it fades, when it says so.
            const StageLight *fader = light == nullptr ? last : (light != last ? light : nullptr);
            if (fader != nullptr && !m_snap && fader->m_modelFade) {
                const int frames = fader->m_modelFadeFrames;
                direction.setFrom(direction.value);
                direction.frames = frames;
                direction.frame = 0;
                color.setFrom(color.value);
                color.frames = frames;
                color.frame = 0;
                if (i == 0) {
                    ambient.setFrom(ambient.value);
                    ambient.frames = frames;
                    ambient.frame = 0;
                }
            }

            if (light == nullptr) {
                direction.setTo(tracks.modelDirection[i].value);
                color.setTo(model->m_inSbBox ? tracks.modelColorSb[i].value : tracks.modelColor[i].value);
                if (i == 0) {
                    getModelAmbient(object, ambientTo, nullptr);
                    ambient.setTo(ambientTo);
                }
            } else {
                sceVu0FVECTOR to, lit;
                const float *lightPosition = light->getWorldMatrix()[3];
                for (int lane = 0; lane < 4; lane++)
                    to[lane] = lightPosition[lane] - pos[lane];
                vecNormalize(to, to);
                if (i == 0) {
                    sceVu0FVECTOR extra;
                    to[2] = -to[2];
                    scaleVec4(weight, lit, light->m_modelColor);
                    scaleVec4(weight, extra, light->m_modelAmbient);
                    getModelAmbient(object, ambientTo, extra);
                    ambient.setTo(ambientTo);
                } else {
                    scaleVec4(weight, lit, light->m_color);
                }
                direction.setTo(to);
                color.setTo(lit);
            }

            direction.step();
            color.step();
            if (i == 0)
                ambient.step();

            memcpy(model->m_lightDirs[i], direction.value, sizeof(sceVu0FVECTOR));
            memcpy(model->m_lightColors[i], color.value, sizeof(sceVu0FVECTOR));
            if (i == 0)
                memcpy(model->m_ambient, ambient.value, sizeof(sceVu0FVECTOR));

            model->data->setLight(i, model->m_lightDirs[i], model->m_lightColors[i]);
            if (model->m_ownAmbient)
                model->data->setAmbient(model->m_ambient);
        }
        object->m_modelLights[i] = light;
    }

    sceVu0FVECTOR shadowDirection;
    if (anyLight == nullptr) {
        const float size = fabsf(model->shadowDirection[2]) + fabsf(model->shadowDirection[0])
                         + fabsf(model->shadowDirection[1]);
        if (size < kShadowDirectionMin || kShadowDirectionMax < size) {
            shadowDirection[0] = 0.1f;
            shadowDirection[1] = 0.0f;
            shadowDirection[2] = 1.0f;
            shadowDirection[3] = 1.0f;
        } else {
            memcpy(shadowDirection, model->shadowDirection, sizeof(shadowDirection));
        }
    } else {
        vecDirection(shadowDirection, anyLight->getWorldMatrix()[3], pos);
    }
    blendVec4(kShadowDirectionEase, shadowDirection, shadowDirection, model->shadowDirection);
    vecNormalize(model->shadowDirection, shadowDirection);

    if (object->getKind() == StageObject_Character) {
        if (shadow.light != nullptr && shadow.weight < kMinShadowWeight) {
            shadow.light = nullptr;
            shadow.weight = 0.0f;
        }
        object->m_shadowLight = shadow.light;
        object->m_shadowWeight = shadow.weight;
    }
}

// FS2 0x001f8620
// callcheck: sceneGroupIsLit, getShownState, contains, setFrom, boxContains, inBoxes, isYriOrKne, isPlayer, find, segmentHitsAny, setTo, getModelAmbient, step, vecNormalize, scaleVec4, setLight, setAmbient, vecDirection, lerpVec4b (lightObject holds them; lerpVec4b is blendVec4)

void SceneRender::updateLights()
{
    for (int group = 0; group < StageObjGroup_Count; group++) {
        if (!sceneGroupIsLit(group))
            continue;
        StageObjArray &list = gStageScene->objects->m_lists[group];
        for (StageObject **slot = nextActive(list.begin()); slot != nullptr;
             slot = nextActive(StageObjArray::next(slot)))
            lightObject(*slot);
    }
}

// The groups whose objects have lit models: 1, 2, 3 and 8.
// FS2 0x001f9490
// callcheck: none

bool sceneGroupIsLit(int group)
{
    constexpr u_int kLitGroups = 0x87;
    return (u_int)(group - 1) < 8 && ((1u << (group - 1)) & kLitGroups) != 0;
}

// FS2 0x001f94c0
// callcheck: operator_new, PointLight, addToScene, setActive, setShown, build, sprintf, attachVcm, strlen, modelFindPart, bindParts, strncpy, find5, FUN_0017e260, FUN_0017e1b0, getKind, strcasecmp, spawn, getStageFiles, strcpy, strcat, heapAllocAligned, Tim2Texture, start (setParent is the erases and the push; heapAllocAligned is Tim2Texture new)

void SceneRender::setup()
{
    StageScene *scene = gStageScene;
    StageObjectList *objects = scene->objects;

    if (!scene->isSubScene) {
        PointLight *demo = new PointLight;
        demo->addToScene(kDemoPlayerLight, true);
        demo->m_castsShadow = 0;
        demo->m_tree->setActive(false);
        demo->m_tree->setShown(false);
        demo->m_priority = kDemoPlayerPriority;
        demo->_273 = 1;

        m_playerLight = new PointLight;
        m_playerLight->addToScene(kPlayerLight, true);
        m_playerLight->m_castsShadow = 0;
        m_playerLight->m_tree->setActive(false);
        m_playerLight->m_tree->setShown(false);
        m_playerLight->m_priority = kPlayerLightPriority;
        m_playerLight->_273 = 1;
    } else {
        PointLight *demo = new PointLight;
        demo->m_mode = StageLight_Keyed;
        demo->addToScene(kDemoLight, true);
        demo->m_castsShadow = 0;
        demo->m_tree->setActive(false);
        demo->m_tree->setShown(false);
        demo->m_priority = kDemoPlayerPriority;
        demo->_273 = 1;
    }

    for (int group = 0; group < StageObjGroup_Count; group++) {
        if (!sceneGroupIsLit(group))
            continue;
        StageObjArray &list = objects->m_lists[group];
        for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
            StageModel *model = (*slot)->m_model;
            if (model != nullptr && model->data != nullptr)
                model->data->spot = &m_spot;
        }
    }

    m_cbGrid->build();
    m_sbGrid->build();

    // A map light's model parts and vertex layers.
    StageObjArray &lights = objects->m_lists[StageObjGroup_5];
    for (StageObject **slot = lights.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageLight *light = static_cast<StageLight *>(*slot);
        if (!light->m_envLight)
            continue;
        char boneName[0x8c];
        sprintf(boneName, kBonePrefix, light->m_name);
        for (int group = 0; group < StageObjGroup_Count; group++) {
            if (!sceneGroupIsLit(group))
                continue;
            StageObjArray &list = objects->m_lists[group];
            for (StageObject **it = list.begin(); it != nullptr; it = StageObjArray::next(it)) {
                StageModel *model = (*it)->m_model;
                if (model == nullptr)
                    continue;
                model->attachVcm(light);
                if (model->data != nullptr
                    && modelFindPart(model->data, boneName, strlen(boneName), 0) != nullptr)
                    light->bindParts(model->data);
            }
        }
    }

    // ENV_xxnnnnc follows ENV_xxnnnn: the same name up to the 'c'.
    for (StageObject **slot = lights.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageLight *light = static_cast<StageLight *>(*slot);
        if (!light->m_envLight || light->m_name[8] != 'c')
            continue;
        char parentName[0x10] = {};     // PC: the ROM leaves the rest of the buffer
        strncpy(parentName, light->m_name, 8);
        StageObject *parent = objects->find5(parentName, 0);
        if (parent != nullptr)
            light->m_tree->setParent(parent->m_tree);
    }

    // A door knows its light.
    StageObjArray &roots = objects->m_lists[StageObjGroup_1];
    for (StageObject **slot = roots.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        StageObject *object = *slot;
        if (object->getKind() != StageObject_Door)
            continue;
        const int index = object->m_doorRecord;
        if (index < 0 || index >= kDoorRecords)
            continue;   // PC: the ROM reads from address 0x20
        if (strlen(gDoorRecords[index].light) != 0) {
            StageObject *light = objects->find5(gDoorRecords[index].light, 0);
            if (light != nullptr)
                object->m_doorLight = light;
        }
    }

    if (scene->lpt != nullptr) {
        const LptRecord *records = (const LptRecord *)scene->lpt->data.get();
        for (int i = 0; i < scene->lpt->_04; i++) {
            if (strcasecmp(records[i].name, kNullEffectLight) == 0)
                scene->nodeTree->spawn(records[i].effect, nullptr, records[i].pos, records[i].rot, 0, 0);
        }
    }

    // A spot may have a texture of its own: <name>.tm2.
    StageFiles *files = getStageFiles();
    if (files != nullptr && files->loader != nullptr) {
        u_int count = 0;
        for (StageObject **slot = lights.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
            if ((*slot)->getKind() != StageObject_SpotLight)
                continue;
            SpotLight *spot = static_cast<SpotLight *>(*slot);
            char fileName[0x80];
            strcpy(fileName, spot->m_name);
            strcat(fileName, kSpotTextureExt);
            void *image = files->loader->getData(fileName);
            if (image == nullptr)
                continue;
            m_spotTextures[count] = new Tim2Texture(image);
            spot->m_texture = m_spotTextures[count];
            count++;
            if (count >= kSceneSpotTextures)
                break;
        }
    }

    start();
}

// FS2 0x001f9c70
// callcheck: none

int sceneRenderGroup(int mode)
{
    if (mode == 2)
        return 0x12;
    if (mode == 1)
        return 0x13;
    return -1;
}

// FS2 0x001f9ca0
// callcheck: scaleVec4, scaleVec4

void SceneRender::getModelAmbient(StageObject *object, sceVu0FVECTOR out, const float *extra)
{
    constexpr float kFreeItemScale = 1.5f;
    constexpr float kFreeItemDarkScale = 5.0f;

    const StageModel *model = object->m_model;
    const SceneLightSet &tracks = m_sets[model->m_lightSet];
    memcpy(out, model->m_inSbBox ? tracks.modelAmbientSb.value : tracks.modelAmbient.value,
           sizeof(sceVu0FVECTOR));

    bool lyingFree = false;
    const int kind = object->getKind();
    if (kind >= StageObject_Item && kind <= StageObject_ThrownItem) {
        const StageItem *item = static_cast<StageItem *>(object);
        lyingFree = object->m_parent == nullptr && item->m_holder == nullptr && object->m_xform.dirty
                 && !item->m_inUse && !item->m_sprung;
    }

    if (lyingFree) {
        // A dark place raises it more, so that it can be seen.
        float scale = kFreeItemScale;
        const float mean = (out[2] + out[0] + out[1]) / 3.0f;
        if (mean < 1.0f) {
            const float dark = 1.0f - mean;
            scale = dark * dark * kFreeItemDarkScale + kFreeItemScale;
        }
        if (extra != nullptr) {
            sceVu0FVECTOR scaled;
            scaleVec4(scale, scaled, extra);
            for (int lane = 0; lane < 4; lane++)
                out[lane] += scaled[lane];
        }
        scaleVec4(scale, out, out);
    } else if (extra != nullptr) {
        for (int lane = 0; lane < 4; lane++)
            out[lane] += extra[lane];
    }
}

// FS2 0x001f9ee0
// callcheck: reset (the played tracks), drawSysSetSceneRender, find5, setActive, setShown

void SceneRender::start()
{
    constexpr float kE10Lane = 160.0f / 255.0f;     // 0x3f20a0a1

    m_set = -1;
    for (int set = 0; set < kSceneLightSets; set++)
        m_sets[set].forEachPlayed([](VectorTrack &track) { track.reset(); });

    memset(m_direction, 0, sizeof(m_direction));
    memset(m_lightColor, 0, sizeof(m_lightColor));
    memset(m_ambient, 0, sizeof(m_ambient));
    memset(m_fog, 0, sizeof(m_fog));
    memset(m_fogColor, 0, sizeof(m_fogColor));
    memset(m_farFog, 0, sizeof(m_farFog));
    memset(m_farFogColor, 0, sizeof(m_farFogColor));
    memset(m_clearColor, 0, sizeof(m_clearColor));

    drawSysSetSceneRender(this);

    m_fogEnabled = 1;
    m_clearEnabled = 1;
    m_e10[0] = m_e10[1] = m_e10[2] = kE10Lane;
    m_e10[3] = 0.0f;
    m_shadeScale = 0.3f;

    if (gStageScene->isSubScene) {
        StageObject *demo = gStageScene->objects->find5(kDemoLight, 0);
        demo->m_tree->setActive(false);
        demo->m_tree->setShown(false);
    }
}

// FS2 0x001fa150
// callcheck: getVramBlocks, gfxGetTexArea, alloc, drawSysSetShadowTexture

void SceneRender::setShadowTexture(Tim2Texture *texture)
{
    m_shadowTexture = texture;
    m_shadowBlocks = texture->getVramBlocks();
    m_shadowBlock = gfxGetTexArea()->alloc(m_shadowBlocks, 0, 0);
    drawSysSetShadowTexture(texture, m_shadowBlock);
}

// FS2 0x001fa1c0
// callcheck: gfxGetTexArea, alloc, drawSysSetSpotlightTexture

void SceneRender::setSpotlightTexture(Tim2Texture *texture)
{
    m_spotlightTexture = texture;
    m_spotlightBlocks = kSpotlightBlocks;
    m_spotlightBlock = gfxGetTexArea()->alloc(m_spotlightBlocks, 0, 0);
    drawSysSetSpotlightTexture(texture, m_spotlightBlock);
}

// FS2 0x001fa230
// callcheck: gfxGetTexArea, drawSysSetSceneRender, drawSysSetSpotlightTexture, drawSysSetShadowTexture, ~AreaGrid, __destroy_new_array, operator_delete (the fades are members)

SceneRender::~SceneRender()
{
    gfxGetTexArea()->free(m_spotlightBlock, m_spotlightBlocks);
    gfxGetTexArea()->free(m_shadowBlock, m_shadowBlocks);
    drawSysSetSceneRender(nullptr);
    drawSysSetSpotlightTexture(nullptr, 0);
    drawSysSetShadowTexture(nullptr, 0);

    // A sub-scene gives the parent's textures back.
    if (gStageScene->parent != nullptr) {
        SceneRender *parent = gStageScene->parent->render;
        drawSysSetSceneRender(parent);
        drawSysSetSpotlightTexture(parent->m_spotlightTexture, parent->m_spotlightBlock);
        drawSysSetShadowTexture(parent->m_shadowTexture, parent->m_shadowBlock);
    }

    delete m_cbGrid;
    delete m_sbGrid;
    delete[] m_sets;
    for (int i = 0; i < kSceneSpotTextures; i++)
        delete m_spotTextures[i];
}

// FS2 0x001fa4a0
// callcheck: RenaPointLight, RenaSpotLight, operator_new[], FUN_00104d90, operator_new, VectorFade, operator_new, AreaGrid, operator_new, AreaGrid, memset (the fades are members)

SceneRender::SceneRender()
    : m_snap(1), m_forcedSet(-1), m_playerLight(nullptr), m_lastPoint(nullptr),
      m_fogEnabled(0), m_clearEnabled(0), m_set(-1),
      m_direction{}, m_lightColor{}, m_ambient{}, m_fog{}, m_fogColor{}, m_farFog{}, m_farFogColor{},
      m_clearColor{}, m_transition{}, m_e10{}, m_shadeScale(0.0f), m_spotlightTexture(nullptr),
      m_spotlightBlock(0), m_spotlightBlocks(0), m_shadowTexture(nullptr), m_shadowBlock(0),
      m_shadowBlocks(0)
{
    // PC: the ROM leaves the tracks' values as the heap had them, until
    // scripts have keyed them.
    m_sets = new SceneLightSet[kSceneLightSets];
    for (int set = 0; set < kSceneLightSets; set++) {
        m_sets[set].forEachTrack([](VectorTrack &track) {
            memset(track.value, 0, sizeof(track.value));
            track.linear = 1;
        });
    }

    m_cbGrid = new AreaGrid;
    m_sbGrid = new AreaGrid;
    memset(m_spotTextures, 0, sizeof(m_spotTextures));
}
